#define PETSCMAT_DLL


/*
    Factorization code for BAIJ format. 
*/

#include "../src/mat/impls/baij/seq/baij.h"
#include "../src/mat/blockinvert.h"
#include "petscbt.h"
#include "../src/mat/utils/freespace.h"

#undef __FUNCT__  
#define __FUNCT__ "MatSolveTranspose_SeqBAIJ_1_NaturalOrdering"
PetscErrorCode MatSolveTranspose_SeqBAIJ_1_NaturalOrdering(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ *)A->data;
  PetscErrorCode ierr;
  PetscInt       i,n=a->mbs,*vi,*ai=a->i,*aj=a->j,nz;
  PetscInt       *diag = a->diag;
  MatScalar      *aa=a->a,*v;
  PetscScalar    s1,*x,*b;

  PetscFunctionBegin;
  ierr = VecCopy(bb,xx);CHKERRQ(ierr);
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  
  /* forward solve the U^T */
  for (i=0; i<n; i++) {

    v     = aa + diag[i];
    /* multiply by the inverse of the block diagonal */
    s1    = (*v++)*x[i];
    vi    = aj + diag[i] + 1;
    nz    = ai[i+1] - diag[i] - 1;
    while (nz--) {
      x[*vi++]  -= (*v++)*s1;
    }
    x[i]   = s1;
  }
  /* backward solve the L^T */
  for (i=n-1; i>=0; i--){
    v    = aa + diag[i] - 1;
    vi   = aj + diag[i] - 1;
    nz   = diag[i] - ai[i];
    s1   = x[i];
    while (nz--) {
      x[*vi--]   -=  (*v--)*s1;
    }
  }
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*(a->nz) - A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolveTranspose_SeqBAIJ_2_NaturalOrdering"
PetscErrorCode MatSolveTranspose_SeqBAIJ_2_NaturalOrdering(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ *)A->data;
  PetscErrorCode ierr;
  PetscInt       i,n=a->mbs,*vi,*ai=a->i,*aj=a->j,nz,idx,idt;
  PetscInt       *diag = a->diag,oidx;
  MatScalar      *aa=a->a,*v;
  PetscScalar    s1,s2,x1,x2;
  PetscScalar    *x,*b;

  PetscFunctionBegin;
  ierr = VecCopy(bb,xx);CHKERRQ(ierr);
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);

  /* forward solve the U^T */
  idx = 0;
  for (i=0; i<n; i++) {

    v     = aa + 4*diag[i];
    /* multiply by the inverse of the block diagonal */
    x1 = x[idx];   x2 = x[1+idx];
    s1 = v[0]*x1  +  v[1]*x2;
    s2 = v[2]*x1  +  v[3]*x2;
    v += 4;

    vi    = aj + diag[i] + 1;
    nz    = ai[i+1] - diag[i] - 1;
    while (nz--) {
      oidx = 2*(*vi++);
      x[oidx]   -= v[0]*s1  +  v[1]*s2;
      x[oidx+1] -= v[2]*s1  +  v[3]*s2;
      v  += 4;
    }
    x[idx]   = s1;x[1+idx] = s2;
    idx += 2;
  }
  /* backward solve the L^T */
  for (i=n-1; i>=0; i--){
    v    = aa + 4*diag[i] - 4;
    vi   = aj + diag[i] - 1;
    nz   = diag[i] - ai[i];
    idt  = 2*i;
    s1   = x[idt];  s2 = x[1+idt];
    while (nz--) {
      idx   = 2*(*vi--);
      x[idx]   -=  v[0]*s1 +  v[1]*s2;
      x[idx+1] -=  v[2]*s1 +  v[3]*s2;
      v -= 4;
    }
  }
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*4.0*(a->nz) - 2.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolveTranspose_SeqBAIJ_2_NaturalOrdering_newdatastruct"
PetscErrorCode MatSolveTranspose_SeqBAIJ_2_NaturalOrdering_newdatastruct(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ *)A->data;
  PetscErrorCode ierr;
  PetscInt       n=a->mbs,*vi,*ai=a->i,*aj=a->j,*diag=a->diag;
  PetscInt       nz,idx,idt,j,i,oidx;
  PetscInt       bs=A->rmap->bs,bs2=a->bs2;
  MatScalar      *aa=a->a,*v;
  PetscScalar    s1,s2,x1,x2;
  PetscScalar    *x,*b;

  PetscFunctionBegin;
  ierr = VecCopy(bb,xx);CHKERRQ(ierr);
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);

  /* forward solve the U^T */
  idx = 0;
  for (i=0; i<n; i++) {
    v     = aa + bs2*diag[i];
    /* multiply by the inverse of the block diagonal */
    x1 = x[idx];   x2 = x[1+idx];
    s1 = v[0]*x1  +  v[1]*x2;
    s2 = v[2]*x1  +  v[3]*x2;
    v -= bs2;

    vi    = aj + diag[i] - 1;
    nz    = diag[i] - diag[i+1] - 1;
    for(j=0;j>-nz;j--){
      oidx = bs*vi[j];
      x[oidx]   -= v[0]*s1  +  v[1]*s2;
      x[oidx+1] -= v[2]*s1  +  v[3]*s2;
      v  -= bs2;
    }
    x[idx]   = s1;x[1+idx] = s2;
    idx += bs;
  }
  /* backward solve the L^T */
  for (i=n-1; i>=0; i--){
    v    = aa + bs2*ai[i];
    vi   = aj + ai[i];
    nz   = ai[i+1] - ai[i];
    idt  = bs*i;
    s1   = x[idt];  s2 = x[1+idt];
    for(j=0;j<nz;j++){
      idx   = bs*vi[j];
      x[idx]   -=  v[0]*s1 +  v[1]*s2;
      x[idx+1] -=  v[2]*s1 +  v[3]*s2;
      v += bs2;
    }
  }
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*bs2*(a->nz) - bs*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolveTranspose_SeqBAIJ_3_NaturalOrdering"
PetscErrorCode MatSolveTranspose_SeqBAIJ_3_NaturalOrdering(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ *)A->data;
  PetscErrorCode ierr;
  PetscInt       i,n=a->mbs,*vi,*ai=a->i,*aj=a->j,nz,idx,idt;
  PetscInt       *diag = a->diag,oidx;
  MatScalar      *aa=a->a,*v;
  PetscScalar    s1,s2,s3,x1,x2,x3;
  PetscScalar    *x,*b;

  PetscFunctionBegin;
  ierr = VecCopy(bb,xx);CHKERRQ(ierr);
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);

  /* forward solve the U^T */
  idx = 0;
  for (i=0; i<n; i++) {

    v     = aa + 9*diag[i];
    /* multiply by the inverse of the block diagonal */
    x1 = x[idx];   x2 = x[1+idx]; x3    = x[2+idx];
    s1 = v[0]*x1  +  v[1]*x2 +  v[2]*x3;
    s2 = v[3]*x1  +  v[4]*x2 +  v[5]*x3;
    s3 = v[6]*x1  +  v[7]*x2 + v[8]*x3;
    v += 9;

    vi    = aj + diag[i] + 1;
    nz    = ai[i+1] - diag[i] - 1;
    while (nz--) {
      oidx = 3*(*vi++);
      x[oidx]   -= v[0]*s1  +  v[1]*s2 +  v[2]*s3;
      x[oidx+1] -= v[3]*s1  +  v[4]*s2 +  v[5]*s3;
      x[oidx+2] -= v[6]*s1 + v[7]*s2 + v[8]*s3;
      v  += 9;
    }
    x[idx]   = s1;x[1+idx] = s2; x[2+idx] = s3;
    idx += 3;
  }
  /* backward solve the L^T */
  for (i=n-1; i>=0; i--){
    v    = aa + 9*diag[i] - 9;
    vi   = aj + diag[i] - 1;
    nz   = diag[i] - ai[i];
    idt  = 3*i;
    s1 = x[idt];  s2 = x[1+idt]; s3 = x[2+idt];
    while (nz--) {
      idx   = 3*(*vi--);
      x[idx]   -=  v[0]*s1 +  v[1]*s2 +  v[2]*s3;
      x[idx+1] -=  v[3]*s1 +  v[4]*s2 +  v[5]*s3;
      x[idx+2] -= v[6]*s1 + v[7]*s2 + v[8]*s3;
      v -= 9;
    }
  }
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*9.0*(a->nz) - 3.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolveTranspose_SeqBAIJ_3_NaturalOrdering_newdatastruct"
PetscErrorCode MatSolveTranspose_SeqBAIJ_3_NaturalOrdering_newdatastruct(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ *)A->data;
  PetscErrorCode ierr;
  PetscInt       n=a->mbs,*vi,*ai=a->i,*aj=a->j,*diag=a->diag;
  PetscInt       nz,idx,idt,j,i,oidx;
  PetscInt       bs=A->rmap->bs,bs2=a->bs2;
  MatScalar      *aa=a->a,*v;
  PetscScalar    s1,s2,s3,x1,x2,x3;
  PetscScalar    *x,*b;

  PetscFunctionBegin;
  ierr = VecCopy(bb,xx);CHKERRQ(ierr);
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);

  /* forward solve the U^T */
  idx = 0;
  for (i=0; i<n; i++) {
    v     = aa + bs2*diag[i];
    /* multiply by the inverse of the block diagonal */
    x1 = x[idx];   x2 = x[1+idx];  x3 = x[2+idx];
    s1 = v[0]*x1  +  v[1]*x2  + v[2]*x3;
    s2 = v[3]*x1  +  v[4]*x2  + v[5]*x3;
    s3 = v[6]*x1  +  v[7]*x2  + v[8]*x3;
    v -= bs2;

    vi    = aj + diag[i] - 1;
    nz    = diag[i] - diag[i+1] - 1;
    for(j=0;j>-nz;j--){
      oidx = bs*vi[j];
      x[oidx]   -= v[0]*s1  +  v[1]*s2  + v[2]*s3;
      x[oidx+1] -= v[3]*s1  +  v[4]*s2  + v[5]*s3;
      x[oidx+2] -= v[6]*s1  +  v[7]*s2  + v[8]*s3;
      v  -= bs2;
    }
    x[idx]   = s1;x[1+idx] = s2;  x[2+idx] = s3;
    idx += bs;
  }
  /* backward solve the L^T */
  for (i=n-1; i>=0; i--){
    v    = aa + bs2*ai[i];
    vi   = aj + ai[i];
    nz   = ai[i+1] - ai[i];
    idt  = bs*i;
    s1   = x[idt];  s2 = x[1+idt];  s3 = x[2+idt];
    for(j=0;j<nz;j++){
      idx   = bs*vi[j];
      x[idx]   -= v[0]*s1  +  v[1]*s2  + v[2]*s3;
      x[idx+1] -= v[3]*s1  +  v[4]*s2  + v[5]*s3;
      x[idx+2] -= v[6]*s1  +  v[7]*s2  + v[8]*s3;
      v += bs2;
    }
  }
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*bs2*(a->nz) - bs*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolveTranspose_SeqBAIJ_4_NaturalOrdering"
PetscErrorCode MatSolveTranspose_SeqBAIJ_4_NaturalOrdering(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ *)A->data;
  PetscErrorCode ierr;
  PetscInt       i,n=a->mbs,*vi,*ai=a->i,*aj=a->j,nz,idx,idt;
  PetscInt       *diag = a->diag,oidx;
  MatScalar      *aa=a->a,*v;
  PetscScalar    s1,s2,s3,s4,x1,x2,x3,x4;
  PetscScalar    *x,*b;

  PetscFunctionBegin;
  ierr = VecCopy(bb,xx);CHKERRQ(ierr);
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);

  /* forward solve the U^T */
  idx = 0;
  for (i=0; i<n; i++) {

    v     = aa + 16*diag[i];
    /* multiply by the inverse of the block diagonal */
    x1    = x[idx];   x2 = x[1+idx]; x3    = x[2+idx]; x4 = x[3+idx];
    s1 = v[0]*x1  +  v[1]*x2 +  v[2]*x3 +  v[3]*x4;
    s2 = v[4]*x1  +  v[5]*x2 +  v[6]*x3 +  v[7]*x4;
    s3 = v[8]*x1  +  v[9]*x2 + v[10]*x3 + v[11]*x4;
    s4 = v[12]*x1 + v[13]*x2 + v[14]*x3 + v[15]*x4;
    v += 16;

    vi    = aj + diag[i] + 1;
    nz    = ai[i+1] - diag[i] - 1;
    while (nz--) {
      oidx = 4*(*vi++);
      x[oidx]   -= v[0]*s1  +  v[1]*s2 +  v[2]*s3 +  v[3]*s4;
      x[oidx+1] -= v[4]*s1  +  v[5]*s2 +  v[6]*s3 +  v[7]*s4;
      x[oidx+2] -= v[8]*s1 + v[9]*s2 + v[10]*s3 + v[11]*s4;
      x[oidx+3] -= v[12]*s1 + v[13]*s2 + v[14]*s3 + v[15]*s4;
      v  += 16;
    }
    x[idx]   = s1;x[1+idx] = s2; x[2+idx] = s3;x[3+idx] = s4;
    idx += 4;
  }
  /* backward solve the L^T */
  for (i=n-1; i>=0; i--){
    v    = aa + 16*diag[i] - 16;
    vi   = aj + diag[i] - 1;
    nz   = diag[i] - ai[i];
    idt  = 4*i;
    s1 = x[idt];  s2 = x[1+idt]; s3 = x[2+idt];s4 = x[3+idt];
    while (nz--) {
      idx   = 4*(*vi--);
      x[idx]   -=  v[0]*s1 +  v[1]*s2 +  v[2]*s3 +  v[3]*s4;
      x[idx+1] -=  v[4]*s1 +  v[5]*s2 +  v[6]*s3 +  v[7]*s4;
      x[idx+2] -= v[8]*s1 + v[9]*s2 + v[10]*s3 + v[11]*s4;
      x[idx+3] -= v[12]*s1 + v[13]*s2 + v[14]*s3 + v[15]*s4;
      v -= 16;
    }
  }
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*16*(a->nz) - 4.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolveTranspose_SeqBAIJ_4_NaturalOrdering_newdatastruct"
PetscErrorCode MatSolveTranspose_SeqBAIJ_4_NaturalOrdering_newdatastruct(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ *)A->data;
  PetscErrorCode ierr;
  PetscInt       n=a->mbs,*vi,*ai=a->i,*aj=a->j,*diag=a->diag;
  PetscInt       nz,idx,idt,j,i,oidx;
  PetscInt       bs=A->rmap->bs,bs2=a->bs2;
  MatScalar      *aa=a->a,*v;
  PetscScalar    s1,s2,s3,s4,x1,x2,x3,x4;
  PetscScalar    *x,*b;

  PetscFunctionBegin;
  ierr = VecCopy(bb,xx);CHKERRQ(ierr);
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);

  /* forward solve the U^T */
  idx = 0;
  for (i=0; i<n; i++) {
    v     = aa + bs2*diag[i];
    /* multiply by the inverse of the block diagonal */
    x1 = x[idx];   x2 = x[1+idx];  x3 = x[2+idx];  x4 = x[3+idx];
    s1 =  v[0]*x1  +  v[1]*x2  + v[2]*x3  + v[3]*x4;
    s2 =  v[4]*x1  +  v[5]*x2  + v[6]*x3  + v[7]*x4;
    s3 =  v[8]*x1  +  v[9]*x2  + v[10]*x3 + v[11]*x4;
    s4 =  v[12]*x1 +  v[13]*x2 + v[14]*x3 + v[15]*x4;
    v -= bs2;

    vi    = aj + diag[i] - 1;
    nz    = diag[i] - diag[i+1] - 1;
    for(j=0;j>-nz;j--){
      oidx = bs*vi[j];
      x[oidx]   -=  v[0]*s1  +  v[1]*s2  + v[2]*s3  + v[3]*s4;
      x[oidx+1] -=  v[4]*s1  +  v[5]*s2  + v[6]*s3  + v[7]*s4;
      x[oidx+2] -=  v[8]*s1  +  v[9]*s2  + v[10]*s3 + v[11]*s4;
      x[oidx+3] -=  v[12]*s1 +  v[13]*s2 + v[14]*s3 + v[15]*s4;
      v  -= bs2;
    }
    x[idx]   = s1;x[1+idx] = s2;  x[2+idx] = s3;  x[3+idx] = s4;
    idx += bs;
  }
  /* backward solve the L^T */
  for (i=n-1; i>=0; i--){
    v    = aa + bs2*ai[i];
    vi   = aj + ai[i];
    nz   = ai[i+1] - ai[i];
    idt  = bs*i;
    s1   = x[idt];  s2 = x[1+idt];  s3 = x[2+idt];  s4 = x[3+idt];
    for(j=0;j<nz;j++){
      idx   = bs*vi[j];
      x[idx]   -=  v[0]*s1  +  v[1]*s2  + v[2]*s3  + v[3]*s4;
      x[idx+1] -=  v[4]*s1  +  v[5]*s2  + v[6]*s3  + v[7]*s4;
      x[idx+2] -=  v[8]*s1  +  v[9]*s2  + v[10]*s3 + v[11]*s4;
      x[idx+3] -=  v[12]*s1 +  v[13]*s2 + v[14]*s3 + v[15]*s4;
      v += bs2;
    }
  }
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*bs2*(a->nz) - bs*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolveTranspose_SeqBAIJ_5_NaturalOrdering"
PetscErrorCode MatSolveTranspose_SeqBAIJ_5_NaturalOrdering(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ *)A->data;
  PetscErrorCode ierr;
  PetscInt       i,n=a->mbs,*vi,*ai=a->i,*aj=a->j,nz,idx,idt;
  PetscInt       *diag = a->diag,oidx;
  MatScalar      *aa=a->a,*v;
  PetscScalar    s1,s2,s3,s4,s5,x1,x2,x3,x4,x5;
  PetscScalar    *x,*b;

  PetscFunctionBegin;
  ierr = VecCopy(bb,xx);CHKERRQ(ierr);
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);

  /* forward solve the U^T */
  idx = 0;
  for (i=0; i<n; i++) {

    v     = aa + 25*diag[i];
    /* multiply by the inverse of the block diagonal */
    x1    = x[idx];   x2 = x[1+idx]; x3    = x[2+idx]; x4 = x[3+idx]; x5 = x[4+idx];
    s1 = v[0]*x1  +  v[1]*x2 +  v[2]*x3 +  v[3]*x4 +  v[4]*x5;
    s2 = v[5]*x1  +  v[6]*x2 +  v[7]*x3 +  v[8]*x4 +  v[9]*x5;
    s3 = v[10]*x1 + v[11]*x2 + v[12]*x3 + v[13]*x4 + v[14]*x5;
    s4 = v[15]*x1 + v[16]*x2 + v[17]*x3 + v[18]*x4 + v[19]*x5;
    s5 = v[20]*x1 + v[21]*x2 + v[22]*x3 + v[23]*x4 + v[24]*x5;
    v += 25;

    vi    = aj + diag[i] + 1;
    nz    = ai[i+1] - diag[i] - 1;
    while (nz--) {
      oidx = 5*(*vi++);
      x[oidx]   -= v[0]*s1  +  v[1]*s2 +  v[2]*s3 +  v[3]*s4 +  v[4]*s5;
      x[oidx+1] -= v[5]*s1  +  v[6]*s2 +  v[7]*s3 +  v[8]*s4 +  v[9]*s5;
      x[oidx+2] -= v[10]*s1 + v[11]*s2 + v[12]*s3 + v[13]*s4 + v[14]*s5;
      x[oidx+3] -= v[15]*s1 + v[16]*s2 + v[17]*s3 + v[18]*s4 + v[19]*s5;
      x[oidx+4] -= v[20]*s1 + v[21]*s2 + v[22]*s3 + v[23]*s4 + v[24]*s5;
      v  += 25;
    }
    x[idx]   = s1;x[1+idx] = s2; x[2+idx] = s3;x[3+idx] = s4; x[4+idx] = s5;
    idx += 5;
  }
  /* backward solve the L^T */
  for (i=n-1; i>=0; i--){
    v    = aa + 25*diag[i] - 25;
    vi   = aj + diag[i] - 1;
    nz   = diag[i] - ai[i];
    idt  = 5*i;
    s1 = x[idt];  s2 = x[1+idt]; s3 = x[2+idt];s4 = x[3+idt]; s5 = x[4+idt];
    while (nz--) {
      idx   = 5*(*vi--);
      x[idx]   -=  v[0]*s1 +  v[1]*s2 +  v[2]*s3 +  v[3]*s4 +  v[4]*s5;
      x[idx+1] -=  v[5]*s1 +  v[6]*s2 +  v[7]*s3 +  v[8]*s4 +  v[9]*s5;
      x[idx+2] -= v[10]*s1 + v[11]*s2 + v[12]*s3 + v[13]*s4 + v[14]*s5;
      x[idx+3] -= v[15]*s1 + v[16]*s2 + v[17]*s3 + v[18]*s4 + v[19]*s5;
      x[idx+4] -= v[20]*s1 + v[21]*s2 + v[22]*s3 + v[23]*s4 + v[24]*s5;
      v -= 25;
    }
  }
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*25*(a->nz) - 5.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolveTranspose_SeqBAIJ_5_NaturalOrdering_newdatastruct"
PetscErrorCode MatSolveTranspose_SeqBAIJ_5_NaturalOrdering_newdatastruct(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ *)A->data;
  PetscErrorCode ierr;
  PetscInt       n=a->mbs,*vi,*ai=a->i,*aj=a->j,*diag=a->diag;
  PetscInt       nz,idx,idt,j,i,oidx;
  PetscInt       bs=A->rmap->bs,bs2=a->bs2;
  MatScalar      *aa=a->a,*v;
  PetscScalar    s1,s2,s3,s4,s5,x1,x2,x3,x4,x5;
  PetscScalar    *x,*b;

  PetscFunctionBegin;
  ierr = VecCopy(bb,xx);CHKERRQ(ierr);
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);

  /* forward solve the U^T */
  idx = 0;
  for (i=0; i<n; i++) {
    v     = aa + bs2*diag[i];
    /* multiply by the inverse of the block diagonal */
    x1 = x[idx];   x2 = x[1+idx];  x3 = x[2+idx];  x4 = x[3+idx];
    x5 = x[4+idx];
    s1 =  v[0]*x1   +  v[1]*x2   + v[2]*x3   + v[3]*x4   + v[4]*x5;
    s2 =  v[5]*x1   +  v[6]*x2   + v[7]*x3   + v[8]*x4   + v[9]*x5;
    s3 =  v[10]*x1  +  v[11]*x2  + v[12]*x3  + v[13]*x4  + v[14]*x5;
    s4 =  v[15]*x1  +  v[16]*x2  + v[17]*x3  + v[18]*x4  + v[19]*x5;
    s5 =  v[20]*x1  +  v[21]*x2  + v[22]*x3  + v[23]*x4   + v[24]*x5;
    v -= bs2;

    vi    = aj + diag[i] - 1;
    nz    = diag[i] - diag[i+1] - 1;
    for(j=0;j>-nz;j--){
      oidx = bs*vi[j];
      x[oidx]   -=  v[0]*s1   +  v[1]*s2   + v[2]*s3   + v[3]*s4   + v[4]*s5;
      x[oidx+1] -=  v[5]*s1   +  v[6]*s2   + v[7]*s3   + v[8]*s4   + v[9]*s5;
      x[oidx+2] -=  v[10]*s1  +  v[11]*s2  + v[12]*s3  + v[13]*s4  + v[14]*s5;
      x[oidx+3] -=  v[15]*s1  +  v[16]*s2  + v[17]*s3  + v[18]*s4  + v[19]*s5;
      x[oidx+4] -=  v[20]*s1  +  v[21]*s2  + v[22]*s3  + v[23]*s4   + v[24]*s5;
      v  -= bs2;
    }
    x[idx]   = s1;x[1+idx] = s2;  x[2+idx] = s3;  x[3+idx] = s4; x[4+idx] = s5;
    idx += bs;
  }
  /* backward solve the L^T */
  for (i=n-1; i>=0; i--){
    v    = aa + bs2*ai[i];
    vi   = aj + ai[i];
    nz   = ai[i+1] - ai[i];
    idt  = bs*i;
    s1   = x[idt];  s2 = x[1+idt];  s3 = x[2+idt];  s4 = x[3+idt];  s5 = x[4+idt];
    for(j=0;j<nz;j++){
      idx   = bs*vi[j];
      x[idx]   -=  v[0]*s1   +  v[1]*s2   + v[2]*s3   + v[3]*s4   + v[4]*s5;
      x[idx+1] -=  v[5]*s1   +  v[6]*s2   + v[7]*s3   + v[8]*s4   + v[9]*s5;
      x[idx+2] -=  v[10]*s1  +  v[11]*s2  + v[12]*s3  + v[13]*s4  + v[14]*s5;
      x[idx+3] -=  v[15]*s1  +  v[16]*s2  + v[17]*s3  + v[18]*s4  + v[19]*s5;
      x[idx+4] -=  v[20]*s1  +  v[21]*s2  + v[22]*s3  + v[23]*s4   + v[24]*s5;
      v += bs2;
    }
  }
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*bs2*(a->nz) - bs*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolveTranspose_SeqBAIJ_6_NaturalOrdering"
PetscErrorCode MatSolveTranspose_SeqBAIJ_6_NaturalOrdering(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ *)A->data;
  PetscErrorCode ierr;
  PetscInt       i,n=a->mbs,*vi,*ai=a->i,*aj=a->j,nz,idx,idt;
  PetscInt       *diag = a->diag,oidx;
  MatScalar      *aa=a->a,*v;
  PetscScalar    s1,s2,s3,s4,s5,s6,x1,x2,x3,x4,x5,x6;
  PetscScalar    *x,*b;

  PetscFunctionBegin;
  ierr = VecCopy(bb,xx);CHKERRQ(ierr);
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);

  /* forward solve the U^T */
  idx = 0;
  for (i=0; i<n; i++) {

    v     = aa + 36*diag[i];
    /* multiply by the inverse of the block diagonal */
    x1    = x[idx];   x2 = x[1+idx]; x3    = x[2+idx]; x4 = x[3+idx]; x5 = x[4+idx];
    x6    = x[5+idx]; 
    s1 = v[0]*x1  +  v[1]*x2 +  v[2]*x3 +  v[3]*x4 +  v[4]*x5 +  v[5]*x6;
    s2 = v[6]*x1  +  v[7]*x2 +  v[8]*x3 +  v[9]*x4 + v[10]*x5 + v[11]*x6;
    s3 = v[12]*x1 + v[13]*x2 + v[14]*x3 + v[15]*x4 + v[16]*x5 + v[17]*x6;
    s4 = v[18]*x1 + v[19]*x2 + v[20]*x3 + v[21]*x4 + v[22]*x5 + v[23]*x6;
    s5 = v[24]*x1 + v[25]*x2 + v[26]*x3 + v[27]*x4 + v[28]*x5 + v[29]*x6;
    s6 = v[30]*x1 + v[31]*x2 + v[32]*x3 + v[33]*x4 + v[34]*x5 + v[35]*x6;
    v += 36;

    vi    = aj + diag[i] + 1;
    nz    = ai[i+1] - diag[i] - 1;
    while (nz--) {
      oidx = 6*(*vi++);
      x[oidx]   -= v[0]*s1  +  v[1]*s2 +  v[2]*s3 +  v[3]*s4 +  v[4]*s5 +  v[5]*s6;
      x[oidx+1] -= v[6]*s1  +  v[7]*s2 +  v[8]*s3 +  v[9]*s4 + v[10]*s5 + v[11]*s6;
      x[oidx+2] -= v[12]*s1 + v[13]*s2 + v[14]*s3 + v[15]*s4 + v[16]*s5 + v[17]*s6;
      x[oidx+3] -= v[18]*s1 + v[19]*s2 + v[20]*s3 + v[21]*s4 + v[22]*s5 + v[23]*s6;
      x[oidx+4] -= v[24]*s1 + v[25]*s2 + v[26]*s3 + v[27]*s4 + v[28]*s5 + v[29]*s6;
      x[oidx+5] -= v[30]*s1 + v[31]*s2 + v[32]*s3 + v[33]*s4 + v[34]*s5 + v[35]*s6;
      v  += 36;
    }
    x[idx]   = s1;x[1+idx] = s2; x[2+idx] = s3;x[3+idx] = s4; x[4+idx] = s5;
    x[5+idx] = s6;
    idx += 6;
  }
  /* backward solve the L^T */
  for (i=n-1; i>=0; i--){
    v    = aa + 36*diag[i] - 36;
    vi   = aj + diag[i] - 1;
    nz   = diag[i] - ai[i];
    idt  = 6*i;
    s1 = x[idt];  s2 = x[1+idt]; s3 = x[2+idt];s4 = x[3+idt]; s5 = x[4+idt];
    s6 = x[5+idt];
    while (nz--) {
      idx   = 6*(*vi--);
      x[idx]   -=  v[0]*s1 +  v[1]*s2 +  v[2]*s3 +  v[3]*s4 +  v[4]*s5 +  v[5]*s6;
      x[idx+1] -=  v[6]*s1 +  v[7]*s2 +  v[8]*s3 +  v[9]*s4 + v[10]*s5 + v[11]*s6;
      x[idx+2] -= v[12]*s1 + v[13]*s2 + v[14]*s3 + v[15]*s4 + v[16]*s5 + v[17]*s6;
      x[idx+3] -= v[18]*s1 + v[19]*s2 + v[20]*s3 + v[21]*s4 + v[22]*s5 + v[23]*s6;
      x[idx+4] -= v[24]*s1 + v[25]*s2 + v[26]*s3 + v[27]*s4 + v[28]*s5 + v[29]*s6;
      x[idx+5] -= v[30]*s1 + v[31]*s2 + v[32]*s3 + v[33]*s4 + v[34]*s5 + v[35]*s6;
      v -= 36;
    }
  }
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*36*(a->nz) - 6.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolveTranspose_SeqBAIJ_6_NaturalOrdering_newdatastruct"
PetscErrorCode MatSolveTranspose_SeqBAIJ_6_NaturalOrdering_newdatastruct(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ *)A->data;
  PetscErrorCode ierr;
  PetscInt       n=a->mbs,*vi,*ai=a->i,*aj=a->j,*diag=a->diag;
  PetscInt       nz,idx,idt,j,i,oidx;
  PetscInt       bs=A->rmap->bs,bs2=a->bs2;
  MatScalar      *aa=a->a,*v;
  PetscScalar    s1,s2,s3,s4,s5,s6,x1,x2,x3,x4,x5,x6;
  PetscScalar    *x,*b;

  PetscFunctionBegin;
  ierr = VecCopy(bb,xx);CHKERRQ(ierr);
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);

  /* forward solve the U^T */
  idx = 0;
  for (i=0; i<n; i++) {
    v     = aa + bs2*diag[i];
    /* multiply by the inverse of the block diagonal */
    x1 = x[idx];   x2 = x[1+idx];  x3 = x[2+idx];  x4 = x[3+idx];
    x5 = x[4+idx]; x6 = x[5+idx];
    s1 = v[0]*x1  +  v[1]*x2 +  v[2]*x3 +  v[3]*x4 +  v[4]*x5 +  v[5]*x6;
    s2 = v[6]*x1  +  v[7]*x2 +  v[8]*x3 +  v[9]*x4 + v[10]*x5 + v[11]*x6;
    s3 = v[12]*x1 + v[13]*x2 + v[14]*x3 + v[15]*x4 + v[16]*x5 + v[17]*x6;
    s4 = v[18]*x1 + v[19]*x2 + v[20]*x3 + v[21]*x4 + v[22]*x5 + v[23]*x6;
    s5 = v[24]*x1 + v[25]*x2 + v[26]*x3 + v[27]*x4 + v[28]*x5 + v[29]*x6;
    s6 = v[30]*x1 + v[31]*x2 + v[32]*x3 + v[33]*x4 + v[34]*x5 + v[35]*x6;
    v -= bs2;

    vi    = aj + diag[i] - 1;
    nz    = diag[i] - diag[i+1] - 1;
    for(j=0;j>-nz;j--){
      oidx = bs*vi[j];
      x[oidx]   -= v[0]*s1  +  v[1]*s2 +  v[2]*s3 +  v[3]*s4 +  v[4]*s5 +  v[5]*s6;
      x[oidx+1] -= v[6]*s1  +  v[7]*s2 +  v[8]*s3 +  v[9]*s4 + v[10]*s5 + v[11]*s6;
      x[oidx+2] -= v[12]*s1 + v[13]*s2 + v[14]*s3 + v[15]*s4 + v[16]*s5 + v[17]*s6;
      x[oidx+3] -= v[18]*s1 + v[19]*s2 + v[20]*s3 + v[21]*s4 + v[22]*s5 + v[23]*s6;
      x[oidx+4] -= v[24]*s1 + v[25]*s2 + v[26]*s3 + v[27]*s4 + v[28]*s5 + v[29]*s6;
      x[oidx+5] -= v[30]*s1 + v[31]*s2 + v[32]*s3 + v[33]*s4 + v[34]*s5 + v[35]*s6;
      v  -= bs2;
    }
    x[idx]   = s1;x[1+idx] = s2;  x[2+idx] = s3;  x[3+idx] = s4; x[4+idx] = s5;
    x[5+idx] = s6;
    idx += bs;
  }
  /* backward solve the L^T */
  for (i=n-1; i>=0; i--){
    v    = aa + bs2*ai[i];
    vi   = aj + ai[i];
    nz   = ai[i+1] - ai[i];
    idt  = bs*i;
    s1   = x[idt];  s2 = x[1+idt];  s3 = x[2+idt];  s4 = x[3+idt];  s5 = x[4+idt];
    s6   = x[5+idt];
    for(j=0;j<nz;j++){
      idx   = bs*vi[j];
      x[idx]   -= v[0]*s1  +  v[1]*s2 +  v[2]*s3 +  v[3]*s4 +  v[4]*s5 +  v[5]*s6;
      x[idx+1] -= v[6]*s1  +  v[7]*s2 +  v[8]*s3 +  v[9]*s4 + v[10]*s5 + v[11]*s6;
      x[idx+2] -= v[12]*s1 + v[13]*s2 + v[14]*s3 + v[15]*s4 + v[16]*s5 + v[17]*s6;
      x[idx+3] -= v[18]*s1 + v[19]*s2 + v[20]*s3 + v[21]*s4 + v[22]*s5 + v[23]*s6;
      x[idx+4] -= v[24]*s1 + v[25]*s2 + v[26]*s3 + v[27]*s4 + v[28]*s5 + v[29]*s6;
      x[idx+5] -= v[30]*s1 + v[31]*s2 + v[32]*s3 + v[33]*s4 + v[34]*s5 + v[35]*s6;
      v += bs2;
    }
  }
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*bs2*(a->nz) - bs*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolveTranspose_SeqBAIJ_7_NaturalOrdering"
PetscErrorCode MatSolveTranspose_SeqBAIJ_7_NaturalOrdering(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ *)A->data;
  PetscErrorCode ierr;
  PetscInt       i,n=a->mbs,*vi,*ai=a->i,*aj=a->j,nz,idx,idt;
  PetscInt       *diag = a->diag,oidx;
  MatScalar      *aa=a->a,*v;
  PetscScalar    s1,s2,s3,s4,s5,s6,s7,x1,x2,x3,x4,x5,x6,x7;
  PetscScalar    *x,*b;

  PetscFunctionBegin;
  ierr = VecCopy(bb,xx);CHKERRQ(ierr);
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);

  /* forward solve the U^T */
  idx = 0;
  for (i=0; i<n; i++) {

    v     = aa + 49*diag[i];
    /* multiply by the inverse of the block diagonal */
    x1    = x[idx];   x2 = x[1+idx]; x3    = x[2+idx]; x4 = x[3+idx]; x5 = x[4+idx];
    x6    = x[5+idx]; x7 = x[6+idx];
    s1 = v[0]*x1  +  v[1]*x2 +  v[2]*x3 +  v[3]*x4 +  v[4]*x5 +  v[5]*x6 +  v[6]*x7;
    s2 = v[7]*x1  +  v[8]*x2 +  v[9]*x3 + v[10]*x4 + v[11]*x5 + v[12]*x6 + v[13]*x7;
    s3 = v[14]*x1 + v[15]*x2 + v[16]*x3 + v[17]*x4 + v[18]*x5 + v[19]*x6 + v[20]*x7;
    s4 = v[21]*x1 + v[22]*x2 + v[23]*x3 + v[24]*x4 + v[25]*x5 + v[26]*x6 + v[27]*x7;
    s5 = v[28]*x1 + v[29]*x2 + v[30]*x3 + v[31]*x4 + v[32]*x5 + v[33]*x6 + v[34]*x7;
    s6 = v[35]*x1 + v[36]*x2 + v[37]*x3 + v[38]*x4 + v[39]*x5 + v[40]*x6 + v[41]*x7;
    s7 = v[42]*x1 + v[43]*x2 + v[44]*x3 + v[45]*x4 + v[46]*x5 + v[47]*x6 + v[48]*x7;
    v += 49;

    vi    = aj + diag[i] + 1;
    nz    = ai[i+1] - diag[i] - 1;
    while (nz--) {
      oidx = 7*(*vi++);
      x[oidx]   -= v[0]*s1  +  v[1]*s2 +  v[2]*s3 +  v[3]*s4 +  v[4]*s5 +  v[5]*s6 +  v[6]*s7;
      x[oidx+1] -= v[7]*s1  +  v[8]*s2 +  v[9]*s3 + v[10]*s4 + v[11]*s5 + v[12]*s6 + v[13]*s7;
      x[oidx+2] -= v[14]*s1 + v[15]*s2 + v[16]*s3 + v[17]*s4 + v[18]*s5 + v[19]*s6 + v[20]*s7;
      x[oidx+3] -= v[21]*s1 + v[22]*s2 + v[23]*s3 + v[24]*s4 + v[25]*s5 + v[26]*s6 + v[27]*s7;
      x[oidx+4] -= v[28]*s1 + v[29]*s2 + v[30]*s3 + v[31]*s4 + v[32]*s5 + v[33]*s6 + v[34]*s7;
      x[oidx+5] -= v[35]*s1 + v[36]*s2 + v[37]*s3 + v[38]*s4 + v[39]*s5 + v[40]*s6 + v[41]*s7;
      x[oidx+6] -= v[42]*s1 + v[43]*s2 + v[44]*s3 + v[45]*s4 + v[46]*s5 + v[47]*s6 + v[48]*s7;
      v  += 49;
    }
    x[idx]   = s1;x[1+idx] = s2; x[2+idx] = s3;x[3+idx] = s4; x[4+idx] = s5;
    x[5+idx] = s6;x[6+idx] = s7;
    idx += 7;
  }
  /* backward solve the L^T */
  for (i=n-1; i>=0; i--){
    v    = aa + 49*diag[i] - 49;
    vi   = aj + diag[i] - 1;
    nz   = diag[i] - ai[i];
    idt  = 7*i;
    s1 = x[idt];  s2 = x[1+idt]; s3 = x[2+idt];s4 = x[3+idt]; s5 = x[4+idt];
    s6 = x[5+idt];s7 = x[6+idt]; 
    while (nz--) {
      idx   = 7*(*vi--);
      x[idx]   -=  v[0]*s1 +  v[1]*s2 +  v[2]*s3 +  v[3]*s4 +  v[4]*s5 +  v[5]*s6 +  v[6]*s7;
      x[idx+1] -=  v[7]*s1 +  v[8]*s2 +  v[9]*s3 + v[10]*s4 + v[11]*s5 + v[12]*s6 + v[13]*s7;
      x[idx+2] -= v[14]*s1 + v[15]*s2 + v[16]*s3 + v[17]*s4 + v[18]*s5 + v[19]*s6 + v[20]*s7;
      x[idx+3] -= v[21]*s1 + v[22]*s2 + v[23]*s3 + v[24]*s4 + v[25]*s5 + v[26]*s6 + v[27]*s7;
      x[idx+4] -= v[28]*s1 + v[29]*s2 + v[30]*s3 + v[31]*s4 + v[32]*s5 + v[33]*s6 + v[34]*s7;
      x[idx+5] -= v[35]*s1 + v[36]*s2 + v[37]*s3 + v[38]*s4 + v[39]*s5 + v[40]*s6 + v[41]*s7;
      x[idx+6] -= v[42]*s1 + v[43]*s2 + v[44]*s3 + v[45]*s4 + v[46]*s5 + v[47]*s6 + v[48]*s7;
      v -= 49;
    }
  }
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*49*(a->nz) - 7.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
#undef __FUNCT__  
#define __FUNCT__ "MatSolveTranspose_SeqBAIJ_7_NaturalOrdering_newdatastruct"
PetscErrorCode MatSolveTranspose_SeqBAIJ_7_NaturalOrdering_newdatastruct(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ *)A->data;
  PetscErrorCode ierr;
  PetscInt       n=a->mbs,*vi,*ai=a->i,*aj=a->j,*diag=a->diag;
  PetscInt       nz,idx,idt,j,i,oidx;
  PetscInt       bs=A->rmap->bs,bs2=a->bs2;
  MatScalar      *aa=a->a,*v;
  PetscScalar    s1,s2,s3,s4,s5,s6,s7,x1,x2,x3,x4,x5,x6,x7;
  PetscScalar    *x,*b;

  PetscFunctionBegin;
  ierr = VecCopy(bb,xx);CHKERRQ(ierr);
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);

  /* forward solve the U^T */
  idx = 0;
  for (i=0; i<n; i++) {
    v     = aa + bs2*diag[i];
    /* multiply by the inverse of the block diagonal */
    x1 = x[idx];   x2 = x[1+idx];  x3 = x[2+idx];  x4 = x[3+idx];
    x5 = x[4+idx]; x6 = x[5+idx];  x7 = x[6+idx];
    s1 = v[0]*x1  +  v[1]*x2 +  v[2]*x3 +  v[3]*x4 +  v[4]*x5 +  v[5]*x6 +  v[6]*x7;
    s2 = v[7]*x1  +  v[8]*x2 +  v[9]*x3 + v[10]*x4 + v[11]*x5 + v[12]*x6 + v[13]*x7;
    s3 = v[14]*x1 + v[15]*x2 + v[16]*x3 + v[17]*x4 + v[18]*x5 + v[19]*x6 + v[20]*x7;
    s4 = v[21]*x1 + v[22]*x2 + v[23]*x3 + v[24]*x4 + v[25]*x5 + v[26]*x6 + v[27]*x7;
    s5 = v[28]*x1 + v[29]*x2 + v[30]*x3 + v[31]*x4 + v[32]*x5 + v[33]*x6 + v[34]*x7;
    s6 = v[35]*x1 + v[36]*x2 + v[37]*x3 + v[38]*x4 + v[39]*x5 + v[40]*x6 + v[41]*x7;
    s7 = v[42]*x1 + v[43]*x2 + v[44]*x3 + v[45]*x4 + v[46]*x5 + v[47]*x6 + v[48]*x7;
    v -= bs2;
    vi    = aj + diag[i] - 1;
    nz    = diag[i] - diag[i+1] - 1;
    for(j=0;j>-nz;j--){
      oidx = bs*vi[j];
      x[oidx]   -= v[0]*s1  +  v[1]*s2 +  v[2]*s3 +  v[3]*s4 +  v[4]*s5 +  v[5]*s6 +  v[6]*s7;
      x[oidx+1] -= v[7]*s1  +  v[8]*s2 +  v[9]*s3 + v[10]*s4 + v[11]*s5 + v[12]*s6 + v[13]*s7;
      x[oidx+2] -= v[14]*s1 + v[15]*s2 + v[16]*s3 + v[17]*s4 + v[18]*s5 + v[19]*s6 + v[20]*s7;
      x[oidx+3] -= v[21]*s1 + v[22]*s2 + v[23]*s3 + v[24]*s4 + v[25]*s5 + v[26]*s6 + v[27]*s7;
      x[oidx+4] -= v[28]*s1 + v[29]*s2 + v[30]*s3 + v[31]*s4 + v[32]*s5 + v[33]*s6 + v[34]*s7;
      x[oidx+5] -= v[35]*s1 + v[36]*s2 + v[37]*s3 + v[38]*s4 + v[39]*s5 + v[40]*s6 + v[41]*s7;
      x[oidx+6] -= v[42]*s1 + v[43]*s2 + v[44]*s3 + v[45]*s4 + v[46]*s5 + v[47]*s6 + v[48]*s7;
      v  -= bs2;
    }
    x[idx]   = s1;  x[1+idx] = s2;  x[2+idx] = s3;  x[3+idx] = s4; x[4+idx] = s5;
    x[5+idx] = s6;  x[6+idx] = s7;
    idx += bs;
  }
  /* backward solve the L^T */
  for (i=n-1; i>=0; i--){
    v    = aa + bs2*ai[i];
    vi   = aj + ai[i];
    nz   = ai[i+1] - ai[i];
    idt  = bs*i;
    s1   = x[idt];    s2 = x[1+idt];  s3 = x[2+idt];  s4 = x[3+idt];  s5 = x[4+idt];
    s6   = x[5+idt];  s7 = x[6+idt];
    for(j=0;j<nz;j++){
      idx   = bs*vi[j];
      x[idx]   -= v[0]*s1  +  v[1]*s2 +  v[2]*s3 +  v[3]*s4 +  v[4]*s5 +  v[5]*s6 +  v[6]*s7;
      x[idx+1] -= v[7]*s1  +  v[8]*s2 +  v[9]*s3 + v[10]*s4 + v[11]*s5 + v[12]*s6 + v[13]*s7;
      x[idx+2] -= v[14]*s1 + v[15]*s2 + v[16]*s3 + v[17]*s4 + v[18]*s5 + v[19]*s6 + v[20]*s7;
      x[idx+3] -= v[21]*s1 + v[22]*s2 + v[23]*s3 + v[24]*s4 + v[25]*s5 + v[26]*s6 + v[27]*s7;
      x[idx+4] -= v[28]*s1 + v[29]*s2 + v[30]*s3 + v[31]*s4 + v[32]*s5 + v[33]*s6 + v[34]*s7;
      x[idx+5] -= v[35]*s1 + v[36]*s2 + v[37]*s3 + v[38]*s4 + v[39]*s5 + v[40]*s6 + v[41]*s7;
      x[idx+6] -= v[42]*s1 + v[43]*s2 + v[44]*s3 + v[45]*s4 + v[46]*s5 + v[47]*s6 + v[48]*s7;
      v += bs2;
    }
  }
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*bs2*(a->nz) - bs*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*---------------------------------------------------------------------------------------------*/
#undef __FUNCT__  
#define __FUNCT__ "MatSolveTranspose_SeqBAIJ_1"
PetscErrorCode MatSolveTranspose_SeqBAIJ_1(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ *)A->data;
  IS             iscol=a->col,isrow=a->row;
  PetscErrorCode ierr;
  const PetscInt *r,*c,*rout,*cout;
  PetscInt       i,n=a->mbs,*vi,*ai=a->i,*aj=a->j,nz;
  PetscInt       *diag = a->diag;
  MatScalar      *aa=a->a,*v;
  PetscScalar    s1,*x,*b,*t;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  t  = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout;

  /* copy the b into temp work space according to permutation */
  for (i=0; i<n; i++) {
    t[i] = b[c[i]];
  } 

  /* forward solve the U^T */
  for (i=0; i<n; i++) {

    v     = aa + diag[i];
    /* multiply by the inverse of the block diagonal */
    s1    = (*v++)*t[i];
    vi    = aj + diag[i] + 1;
    nz    = ai[i+1] - diag[i] - 1;
    while (nz--) {
      t[*vi++]  -= (*v++)*s1;
    }
    t[i]   = s1;
  }
  /* backward solve the L^T */
  for (i=n-1; i>=0; i--){
    v    = aa + diag[i] - 1;
    vi   = aj + diag[i] - 1;
    nz   = diag[i] - ai[i];
    s1   = t[i];
    while (nz--) {
      t[*vi--]   -=  (*v--)*s1;
    }
  }

  /* copy t into x according to permutation */
  for (i=0; i<n; i++) {
    x[r[i]]   = t[i];
  } 

  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*(a->nz) - A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolveTranspose_SeqBAIJ_2"
PetscErrorCode MatSolveTranspose_SeqBAIJ_2(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ *)A->data;
  IS             iscol=a->col,isrow=a->row;
  PetscErrorCode ierr;
  const PetscInt *r,*c,*rout,*cout;
  PetscInt       i,n=a->mbs,*vi,*ai=a->i,*aj=a->j,nz,idx,idt;
  PetscInt       *diag = a->diag,ii,ic,ir,oidx;
  MatScalar      *aa=a->a,*v;
  PetscScalar    s1,s2,x1,x2;
  PetscScalar    *x,*b,*t;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  t  = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout;

  /* copy the b into temp work space according to permutation */
  ii = 0;
  for (i=0; i<n; i++) {
    ic      = 2*c[i];
    t[ii]   = b[ic];
    t[ii+1] = b[ic+1];
    ii += 2;
  } 

  /* forward solve the U^T */
  idx = 0;
  for (i=0; i<n; i++) {

    v     = aa + 4*diag[i];
    /* multiply by the inverse of the block diagonal */
    x1    = t[idx];   x2 = t[1+idx];
    s1 = v[0]*x1  +  v[1]*x2;
    s2 = v[2]*x1  +  v[3]*x2;
    v += 4;

    vi    = aj + diag[i] + 1;
    nz    = ai[i+1] - diag[i] - 1;
    while (nz--) {
      oidx = 2*(*vi++);
      t[oidx]   -= v[0]*s1  +  v[1]*s2;
      t[oidx+1] -= v[2]*s1  +  v[3]*s2;
      v  += 4;
    }
    t[idx]   = s1;t[1+idx] = s2;
    idx += 2;
  }
  /* backward solve the L^T */
  for (i=n-1; i>=0; i--){
    v    = aa + 4*diag[i] - 4;
    vi   = aj + diag[i] - 1;
    nz   = diag[i] - ai[i];
    idt  = 2*i;
    s1 = t[idt];  s2 = t[1+idt];
    while (nz--) {
      idx   = 2*(*vi--);
      t[idx]   -=  v[0]*s1 +  v[1]*s2;
      t[idx+1] -=  v[2]*s1 +  v[3]*s2;
      v -= 4;
    }
  }

  /* copy t into x according to permutation */
  ii = 0;
  for (i=0; i<n; i++) {
    ir      = 2*r[i];
    x[ir]   = t[ii];
    x[ir+1] = t[ii+1];
    ii += 2;
  } 

  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*4*(a->nz) - 2.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolveTranspose_SeqBAIJ_2_newdatastruct"
PetscErrorCode MatSolveTranspose_SeqBAIJ_2_newdatastruct(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ *)A->data;
  PetscErrorCode ierr;
  IS             iscol=a->col,isrow=a->row;
  PetscInt       n=a->mbs,*vi,*ai=a->i,*aj=a->j,*diag=a->diag;
  const PetscInt *r,*c,*rout,*cout;
  PetscInt       nz,idx,idt,j,i,oidx,ii,ic,ir;
  PetscInt       bs=A->rmap->bs,bs2=a->bs2;
  MatScalar      *aa=a->a,*v;
  PetscScalar    s1,s2,x1,x2;
  PetscScalar    *x,*b,*t;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  t = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout;

  /* copy b into temp work space according to permutation */
  for(i=0;i<n;i++){
    ii = bs*i; ic = bs*c[i];
    t[ii] = b[ic]; t[ii+1] = b[ic+1];
  }

  /* forward solve the U^T */
  idx = 0;
  for (i=0; i<n; i++) {
    v     = aa + bs2*diag[i];
    /* multiply by the inverse of the block diagonal */
    x1 = t[idx];   x2 = t[1+idx];
    s1 = v[0]*x1  +  v[1]*x2;
    s2 = v[2]*x1  +  v[3]*x2;
    v -= bs2;

    vi    = aj + diag[i] - 1;
    nz    = diag[i] - diag[i+1] - 1;
    for(j=0;j>-nz;j--){
      oidx = bs*vi[j];
      t[oidx]   -= v[0]*s1  +  v[1]*s2;
      t[oidx+1] -= v[2]*s1  +  v[3]*s2;
      v  -= bs2;
    }
    t[idx]   = s1;t[1+idx] = s2;
    idx += bs;
  }
  /* backward solve the L^T */
  for (i=n-1; i>=0; i--){
    v    = aa + bs2*ai[i];
    vi   = aj + ai[i];
    nz   = ai[i+1] - ai[i];
    idt  = bs*i;
    s1   = t[idt];  s2 = t[1+idt];
    for(j=0;j<nz;j++){
      idx   = bs*vi[j];
      t[idx]   -=  v[0]*s1 +  v[1]*s2;
      t[idx+1] -=  v[2]*s1 +  v[3]*s2;
      v += bs2;
    }
  }

  /* copy t into x according to permutation */
  for(i=0;i<n;i++){
    ii = bs*i;  ir = bs*r[i];
    x[ir] = t[ii];  x[ir+1] = t[ii+1];
  }

  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*bs2*(a->nz) - bs*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolveTranspose_SeqBAIJ_3"
PetscErrorCode MatSolveTranspose_SeqBAIJ_3(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ *)A->data;
  IS             iscol=a->col,isrow=a->row;
  PetscErrorCode ierr;
  const PetscInt *r,*c,*rout,*cout;
  PetscInt       i,n=a->mbs,*vi,*ai=a->i,*aj=a->j,nz,idx,idt;
  PetscInt       *diag = a->diag,ii,ic,ir,oidx;
  MatScalar      *aa=a->a,*v;
  PetscScalar    s1,s2,s3,x1,x2,x3;
  PetscScalar    *x,*b,*t;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  t  = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout;

  /* copy the b into temp work space according to permutation */
  ii = 0;
  for (i=0; i<n; i++) {
    ic      = 3*c[i];
    t[ii]   = b[ic];
    t[ii+1] = b[ic+1];
    t[ii+2] = b[ic+2];
    ii += 3;
  } 

  /* forward solve the U^T */
  idx = 0;
  for (i=0; i<n; i++) {

    v     = aa + 9*diag[i];
    /* multiply by the inverse of the block diagonal */
    x1    = t[idx];   x2 = t[1+idx]; x3    = t[2+idx];
    s1 = v[0]*x1  +  v[1]*x2 +  v[2]*x3;
    s2 = v[3]*x1  +  v[4]*x2 +  v[5]*x3;
    s3 = v[6]*x1  +  v[7]*x2 + v[8]*x3;
    v += 9;

    vi    = aj + diag[i] + 1;
    nz    = ai[i+1] - diag[i] - 1;
    while (nz--) {
      oidx = 3*(*vi++);
      t[oidx]   -= v[0]*s1  +  v[1]*s2 +  v[2]*s3;
      t[oidx+1] -= v[3]*s1  +  v[4]*s2 +  v[5]*s3;
      t[oidx+2] -= v[6]*s1 + v[7]*s2 + v[8]*s3;
      v  += 9;
    }
    t[idx]   = s1;t[1+idx] = s2; t[2+idx] = s3;
    idx += 3;
  }
  /* backward solve the L^T */
  for (i=n-1; i>=0; i--){
    v    = aa + 9*diag[i] - 9;
    vi   = aj + diag[i] - 1;
    nz   = diag[i] - ai[i];
    idt  = 3*i;
    s1 = t[idt];  s2 = t[1+idt]; s3 = t[2+idt];
    while (nz--) {
      idx   = 3*(*vi--);
      t[idx]   -=  v[0]*s1 +  v[1]*s2 +  v[2]*s3;
      t[idx+1] -=  v[3]*s1 +  v[4]*s2 +  v[5]*s3;
      t[idx+2] -= v[6]*s1 + v[7]*s2 + v[8]*s3;
      v -= 9;
    }
  }

  /* copy t into x according to permutation */
  ii = 0;
  for (i=0; i<n; i++) {
    ir      = 3*r[i];
    x[ir]   = t[ii];
    x[ir+1] = t[ii+1];
    x[ir+2] = t[ii+2];
    ii += 3;
  } 

  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*9*(a->nz) - 3.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolveTranspose_SeqBAIJ_3_newdatastruct"
PetscErrorCode MatSolveTranspose_SeqBAIJ_3_newdatastruct(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ *)A->data;
  PetscErrorCode ierr;
  IS             iscol=a->col,isrow=a->row;
  PetscInt       n=a->mbs,*vi,*ai=a->i,*aj=a->j,*diag=a->diag;
  const PetscInt *r,*c,*rout,*cout;
  PetscInt       nz,idx,idt,j,i,oidx,ii,ic,ir;
  PetscInt       bs=A->rmap->bs,bs2=a->bs2;
  MatScalar      *aa=a->a,*v;
  PetscScalar    s1,s2,s3,x1,x2,x3;
  PetscScalar    *x,*b,*t;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  t = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout;

  /* copy b into temp work space according to permutation */
  for(i=0;i<n;i++){
    ii = bs*i; ic = bs*c[i];
    t[ii] = b[ic]; t[ii+1] = b[ic+1]; t[ii+2] = b[ic+2];
  }

  /* forward solve the U^T */
  idx = 0;
  for (i=0; i<n; i++) {
    v     = aa + bs2*diag[i];
    /* multiply by the inverse of the block diagonal */
    x1    = t[idx];   x2 = t[1+idx]; x3    = t[2+idx];
    s1 = v[0]*x1  +  v[1]*x2 +  v[2]*x3;
    s2 = v[3]*x1  +  v[4]*x2 +  v[5]*x3;
    s3 = v[6]*x1  +  v[7]*x2 + v[8]*x3;
    v -= bs2;

    vi    = aj + diag[i] - 1;
    nz    = diag[i] - diag[i+1] - 1;
    for(j=0;j>-nz;j--){
      oidx = bs*vi[j];
      t[oidx]   -= v[0]*s1  +  v[1]*s2 +  v[2]*s3;
      t[oidx+1] -= v[3]*s1  +  v[4]*s2 +  v[5]*s3;
      t[oidx+2] -= v[6]*s1 + v[7]*s2 + v[8]*s3;
      v  -= bs2;
    }
    t[idx]   = s1;t[1+idx] = s2;  t[2+idx] = s3;
    idx += bs;
  }
  /* backward solve the L^T */
  for (i=n-1; i>=0; i--){
    v    = aa + bs2*ai[i];
    vi   = aj + ai[i];
    nz   = ai[i+1] - ai[i];
    idt  = bs*i;
    s1   = t[idt];  s2 = t[1+idt];  s3 = t[2+idt];
    for(j=0;j<nz;j++){
      idx   = bs*vi[j];
      t[idx]   -= v[0]*s1  +  v[1]*s2 +  v[2]*s3;
      t[idx+1] -= v[3]*s1  +  v[4]*s2 +  v[5]*s3;
      t[idx+2] -= v[6]*s1 + v[7]*s2 + v[8]*s3;
      v += bs2;
    }
  }

  /* copy t into x according to permutation */
  for(i=0;i<n;i++){
    ii = bs*i;  ir = bs*r[i];
    x[ir] = t[ii];  x[ir+1] = t[ii+1]; x[ir+2] = t[ii+2];
  }

  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*bs2*(a->nz) - bs*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolveTranspose_SeqBAIJ_4"
PetscErrorCode MatSolveTranspose_SeqBAIJ_4(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ *)A->data;
  IS             iscol=a->col,isrow=a->row;
  PetscErrorCode ierr;
  const PetscInt *r,*c,*rout,*cout;
  PetscInt       i,n=a->mbs,*vi,*ai=a->i,*aj=a->j,nz,idx,idt;
  PetscInt       *diag = a->diag,ii,ic,ir,oidx;
  MatScalar      *aa=a->a,*v;
  PetscScalar    s1,s2,s3,s4,x1,x2,x3,x4;
  PetscScalar    *x,*b,*t;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  t  = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout;

  /* copy the b into temp work space according to permutation */
  ii = 0;
  for (i=0; i<n; i++) {
    ic      = 4*c[i];
    t[ii]   = b[ic];
    t[ii+1] = b[ic+1];
    t[ii+2] = b[ic+2];
    t[ii+3] = b[ic+3];
    ii += 4;
  } 

  /* forward solve the U^T */
  idx = 0;
  for (i=0; i<n; i++) {

    v     = aa + 16*diag[i];
    /* multiply by the inverse of the block diagonal */
    x1    = t[idx];   x2 = t[1+idx]; x3    = t[2+idx]; x4 = t[3+idx];
    s1 = v[0]*x1  +  v[1]*x2 +  v[2]*x3 +  v[3]*x4;
    s2 = v[4]*x1  +  v[5]*x2 +  v[6]*x3 +  v[7]*x4;
    s3 = v[8]*x1  +  v[9]*x2 + v[10]*x3 + v[11]*x4;
    s4 = v[12]*x1 + v[13]*x2 + v[14]*x3 + v[15]*x4;
    v += 16;

    vi    = aj + diag[i] + 1;
    nz    = ai[i+1] - diag[i] - 1;
    while (nz--) {
      oidx = 4*(*vi++);
      t[oidx]   -= v[0]*s1  +  v[1]*s2 +  v[2]*s3 +  v[3]*s4;
      t[oidx+1] -= v[4]*s1  +  v[5]*s2 +  v[6]*s3 +  v[7]*s4;
      t[oidx+2] -= v[8]*s1 + v[9]*s2 + v[10]*s3 + v[11]*s4;
      t[oidx+3] -= v[12]*s1 + v[13]*s2 + v[14]*s3 + v[15]*s4;
      v  += 16;
    }
    t[idx]   = s1;t[1+idx] = s2; t[2+idx] = s3;t[3+idx] = s4;
    idx += 4;
  }
  /* backward solve the L^T */
  for (i=n-1; i>=0; i--){
    v    = aa + 16*diag[i] - 16;
    vi   = aj + diag[i] - 1;
    nz   = diag[i] - ai[i];
    idt  = 4*i;
    s1 = t[idt];  s2 = t[1+idt]; s3 = t[2+idt];s4 = t[3+idt];
    while (nz--) {
      idx   = 4*(*vi--);
      t[idx]   -=  v[0]*s1 +  v[1]*s2 +  v[2]*s3 +  v[3]*s4;
      t[idx+1] -=  v[4]*s1 +  v[5]*s2 +  v[6]*s3 +  v[7]*s4;
      t[idx+2] -= v[8]*s1 + v[9]*s2 + v[10]*s3 + v[11]*s4;
      t[idx+3] -= v[12]*s1 + v[13]*s2 + v[14]*s3 + v[15]*s4;
      v -= 16;
    }
  }

  /* copy t into x according to permutation */
  ii = 0;
  for (i=0; i<n; i++) {
    ir      = 4*r[i];
    x[ir]   = t[ii];
    x[ir+1] = t[ii+1];
    x[ir+2] = t[ii+2];
    x[ir+3] = t[ii+3];
    ii += 4;
  } 

  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*16*(a->nz) - 4.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolveTranspose_SeqBAIJ_4_newdatastruct"
PetscErrorCode MatSolveTranspose_SeqBAIJ_4_newdatastruct(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ *)A->data;
  PetscErrorCode ierr;
  IS             iscol=a->col,isrow=a->row;
  PetscInt       n=a->mbs,*vi,*ai=a->i,*aj=a->j,*diag=a->diag;
  const PetscInt *r,*c,*rout,*cout;
  PetscInt       nz,idx,idt,j,i,oidx,ii,ic,ir;
  PetscInt       bs=A->rmap->bs,bs2=a->bs2;
  MatScalar      *aa=a->a,*v;
  PetscScalar    s1,s2,s3,s4,x1,x2,x3,x4;
  PetscScalar    *x,*b,*t;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  t = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout;

  /* copy b into temp work space according to permutation */
  for(i=0;i<n;i++){
    ii = bs*i; ic = bs*c[i];
    t[ii] = b[ic]; t[ii+1] = b[ic+1]; t[ii+2] = b[ic+2]; t[ii+3] = b[ic+3];
  }

  /* forward solve the U^T */
  idx = 0;
  for (i=0; i<n; i++) {
    v     = aa + bs2*diag[i];
    /* multiply by the inverse of the block diagonal */
    x1    = t[idx];   x2 = t[1+idx]; x3    = t[2+idx];  x4 = t[3+idx];
    s1 = v[0]*x1  +  v[1]*x2 +  v[2]*x3 +  v[3]*x4;
    s2 = v[4]*x1  +  v[5]*x2 +  v[6]*x3 +  v[7]*x4;
    s3 = v[8]*x1  +  v[9]*x2 + v[10]*x3 + v[11]*x4;
    s4 = v[12]*x1 + v[13]*x2 + v[14]*x3 + v[15]*x4;
    v -= bs2;

    vi    = aj + diag[i] - 1;
    nz    = diag[i] - diag[i+1] - 1;
    for(j=0;j>-nz;j--){
      oidx = bs*vi[j];
      t[oidx]   -= v[0]*s1  +  v[1]*s2 +  v[2]*s3 +  v[3]*s4;
      t[oidx+1] -= v[4]*s1  +  v[5]*s2 +  v[6]*s3 +  v[7]*s4;
      t[oidx+2] -= v[8]*s1 + v[9]*s2 + v[10]*s3 + v[11]*s4;
      t[oidx+3] -= v[12]*s1 + v[13]*s2 + v[14]*s3 + v[15]*s4;
      v  -= bs2;
    }
    t[idx]   = s1;t[1+idx] = s2;  t[2+idx] = s3;  t[3+idx] = s4;
    idx += bs;
  }
  /* backward solve the L^T */
  for (i=n-1; i>=0; i--){
    v    = aa + bs2*ai[i];
    vi   = aj + ai[i];
    nz   = ai[i+1] - ai[i];
    idt  = bs*i;
    s1   = t[idt];  s2 = t[1+idt];  s3 = t[2+idt];  s4 = t[3+idt];
    for(j=0;j<nz;j++){
      idx   = bs*vi[j];
      t[idx]   -=  v[0]*s1 +  v[1]*s2 +  v[2]*s3  +  v[3]*s4;
      t[idx+1] -=  v[4]*s1 +  v[5]*s2 +  v[6]*s3  +  v[7]*s4;
      t[idx+2] -=  v[8]*s1 +  v[9]*s2 +  v[10]*s3 + v[11]*s4;
      t[idx+3] -= v[12]*s1 +  v[13]*s2 + v[14]*s3 + v[15]*s4;
      v += bs2;
    }
  }

  /* copy t into x according to permutation */
  for(i=0;i<n;i++){
    ii = bs*i;  ir = bs*r[i];
    x[ir] = t[ii];  x[ir+1] = t[ii+1]; x[ir+2] = t[ii+2];  x[ir+3] = t[ii+3];
  }

  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*bs2*(a->nz) - bs*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolveTranspose_SeqBAIJ_5"
PetscErrorCode MatSolveTranspose_SeqBAIJ_5(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ *)A->data;
  IS             iscol=a->col,isrow=a->row;
  PetscErrorCode ierr;
  const PetscInt *r,*c,*rout,*cout;
  PetscInt       i,n=a->mbs,*vi,*ai=a->i,*aj=a->j,nz,idx,idt;
  PetscInt       *diag = a->diag,ii,ic,ir,oidx;
  MatScalar      *aa=a->a,*v;
  PetscScalar    s1,s2,s3,s4,s5,x1,x2,x3,x4,x5;
  PetscScalar    *x,*b,*t;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  t  = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout;

  /* copy the b into temp work space according to permutation */
  ii = 0;
  for (i=0; i<n; i++) {
    ic      = 5*c[i];
    t[ii]   = b[ic];
    t[ii+1] = b[ic+1];
    t[ii+2] = b[ic+2];
    t[ii+3] = b[ic+3];
    t[ii+4] = b[ic+4];
    ii += 5;
  } 

  /* forward solve the U^T */
  idx = 0;
  for (i=0; i<n; i++) {

    v     = aa + 25*diag[i];
    /* multiply by the inverse of the block diagonal */
    x1    = t[idx];   x2 = t[1+idx]; x3    = t[2+idx]; x4 = t[3+idx]; x5 = t[4+idx];
    s1 = v[0]*x1  +  v[1]*x2 +  v[2]*x3 +  v[3]*x4 +  v[4]*x5;
    s2 = v[5]*x1  +  v[6]*x2 +  v[7]*x3 +  v[8]*x4 +  v[9]*x5;
    s3 = v[10]*x1 + v[11]*x2 + v[12]*x3 + v[13]*x4 + v[14]*x5;
    s4 = v[15]*x1 + v[16]*x2 + v[17]*x3 + v[18]*x4 + v[19]*x5;
    s5 = v[20]*x1 + v[21]*x2 + v[22]*x3 + v[23]*x4 + v[24]*x5;
    v += 25;

    vi    = aj + diag[i] + 1;
    nz    = ai[i+1] - diag[i] - 1;
    while (nz--) {
      oidx = 5*(*vi++);
      t[oidx]   -= v[0]*s1  +  v[1]*s2 +  v[2]*s3 +  v[3]*s4 +  v[4]*s5;
      t[oidx+1] -= v[5]*s1  +  v[6]*s2 +  v[7]*s3 +  v[8]*s4 +  v[9]*s5;
      t[oidx+2] -= v[10]*s1 + v[11]*s2 + v[12]*s3 + v[13]*s4 + v[14]*s5;
      t[oidx+3] -= v[15]*s1 + v[16]*s2 + v[17]*s3 + v[18]*s4 + v[19]*s5;
      t[oidx+4] -= v[20]*s1 + v[21]*s2 + v[22]*s3 + v[23]*s4 + v[24]*s5;
      v  += 25;
    }
    t[idx]   = s1;t[1+idx] = s2; t[2+idx] = s3;t[3+idx] = s4; t[4+idx] = s5;
    idx += 5;
  }
  /* backward solve the L^T */
  for (i=n-1; i>=0; i--){
    v    = aa + 25*diag[i] - 25;
    vi   = aj + diag[i] - 1;
    nz   = diag[i] - ai[i];
    idt  = 5*i;
    s1 = t[idt];  s2 = t[1+idt]; s3 = t[2+idt];s4 = t[3+idt]; s5 = t[4+idt];
    while (nz--) {
      idx   = 5*(*vi--);
      t[idx]   -=  v[0]*s1 +  v[1]*s2 +  v[2]*s3 +  v[3]*s4 +  v[4]*s5;
      t[idx+1] -=  v[5]*s1 +  v[6]*s2 +  v[7]*s3 +  v[8]*s4 +  v[9]*s5;
      t[idx+2] -= v[10]*s1 + v[11]*s2 + v[12]*s3 + v[13]*s4 + v[14]*s5;
      t[idx+3] -= v[15]*s1 + v[16]*s2 + v[17]*s3 + v[18]*s4 + v[19]*s5;
      t[idx+4] -= v[20]*s1 + v[21]*s2 + v[22]*s3 + v[23]*s4 + v[24]*s5;
      v -= 25;
    }
  }

  /* copy t into x according to permutation */
  ii = 0;
  for (i=0; i<n; i++) {
    ir      = 5*r[i];
    x[ir]   = t[ii];
    x[ir+1] = t[ii+1];
    x[ir+2] = t[ii+2];
    x[ir+3] = t[ii+3];
    x[ir+4] = t[ii+4];
    ii += 5;
  } 

  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*25*(a->nz) - 5.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolveTranspose_SeqBAIJ_5_newdatastruct"
PetscErrorCode MatSolveTranspose_SeqBAIJ_5_newdatastruct(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ *)A->data;
  PetscErrorCode ierr;
  IS             iscol=a->col,isrow=a->row;
  PetscInt       n=a->mbs,*vi,*ai=a->i,*aj=a->j,*diag=a->diag;
  const PetscInt *r,*c,*rout,*cout;
  PetscInt       nz,idx,idt,j,i,oidx,ii,ic,ir;
  PetscInt       bs=A->rmap->bs,bs2=a->bs2;
  MatScalar      *aa=a->a,*v;
  PetscScalar    s1,s2,s3,s4,s5,x1,x2,x3,x4,x5;
  PetscScalar    *x,*b,*t;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  t = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout;

  /* copy b into temp work space according to permutation */
  for(i=0;i<n;i++){
    ii = bs*i; ic = bs*c[i];
    t[ii] = b[ic]; t[ii+1] = b[ic+1]; t[ii+2] = b[ic+2]; t[ii+3] = b[ic+3];
    t[ii+4] = b[ic+4];
  }

  /* forward solve the U^T */
  idx = 0;
  for (i=0; i<n; i++) {
    v     = aa + bs2*diag[i];
    /* multiply by the inverse of the block diagonal */
    x1    = t[idx];   x2 = t[1+idx]; x3    = t[2+idx]; x4 = t[3+idx]; x5 = t[4+idx];
    s1 = v[0]*x1  +  v[1]*x2 +  v[2]*x3 +  v[3]*x4 +  v[4]*x5;
    s2 = v[5]*x1  +  v[6]*x2 +  v[7]*x3 +  v[8]*x4 +  v[9]*x5;
    s3 = v[10]*x1 + v[11]*x2 + v[12]*x3 + v[13]*x4 + v[14]*x5;
    s4 = v[15]*x1 + v[16]*x2 + v[17]*x3 + v[18]*x4 + v[19]*x5;
    s5 = v[20]*x1 + v[21]*x2 + v[22]*x3 + v[23]*x4 + v[24]*x5;
    v -= bs2;

    vi    = aj + diag[i] - 1;
    nz    = diag[i] - diag[i+1] - 1;
    for(j=0;j>-nz;j--){
      oidx = bs*vi[j];
      t[oidx]   -= v[0]*s1  +  v[1]*s2 +  v[2]*s3 +  v[3]*s4 +  v[4]*s5;
      t[oidx+1] -= v[5]*s1  +  v[6]*s2 +  v[7]*s3 +  v[8]*s4 +  v[9]*s5;
      t[oidx+2] -= v[10]*s1 + v[11]*s2 + v[12]*s3 + v[13]*s4 + v[14]*s5;
      t[oidx+3] -= v[15]*s1 + v[16]*s2 + v[17]*s3 + v[18]*s4 + v[19]*s5;
      t[oidx+4] -= v[20]*s1 + v[21]*s2 + v[22]*s3 + v[23]*s4 + v[24]*s5;
      v  -= bs2;
    }
    t[idx]   = s1;t[1+idx] = s2;  t[2+idx] = s3;  t[3+idx] = s4; t[4+idx] =s5;
    idx += bs;
  }
  /* backward solve the L^T */
  for (i=n-1; i>=0; i--){
    v    = aa + bs2*ai[i];
    vi   = aj + ai[i];
    nz   = ai[i+1] - ai[i];
    idt  = bs*i;
    s1   = t[idt];  s2 = t[1+idt];  s3 = t[2+idt];  s4 = t[3+idt]; s5 = t[4+idt];
    for(j=0;j<nz;j++){
      idx   = bs*vi[j];
      t[idx]   -= v[0]*s1  +  v[1]*s2 +  v[2]*s3 +  v[3]*s4 +  v[4]*s5;
      t[idx+1] -= v[5]*s1  +  v[6]*s2 +  v[7]*s3 +  v[8]*s4 +  v[9]*s5;
      t[idx+2] -= v[10]*s1 + v[11]*s2 + v[12]*s3 + v[13]*s4 + v[14]*s5;
      t[idx+3] -= v[15]*s1 + v[16]*s2 + v[17]*s3 + v[18]*s4 + v[19]*s5;
      t[idx+4] -= v[20]*s1 + v[21]*s2 + v[22]*s3 + v[23]*s4 + v[24]*s5;
      v += bs2;
    }
  }

  /* copy t into x according to permutation */
  for(i=0;i<n;i++){
    ii = bs*i;  ir = bs*r[i];
    x[ir] = t[ii];  x[ir+1] = t[ii+1]; x[ir+2] = t[ii+2];  x[ir+3] = t[ii+3];
    x[ir+4] = t[ii+4];
  }

  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*bs2*(a->nz) - bs*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolveTranspose_SeqBAIJ_6"
PetscErrorCode MatSolveTranspose_SeqBAIJ_6(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ *)A->data;
  IS             iscol=a->col,isrow=a->row;
  PetscErrorCode ierr;
  const PetscInt *r,*c,*rout,*cout;
  PetscInt       i,n=a->mbs,*vi,*ai=a->i,*aj=a->j,nz,idx,idt;
  PetscInt       *diag = a->diag,ii,ic,ir,oidx;
  MatScalar      *aa=a->a,*v;
  PetscScalar    s1,s2,s3,s4,s5,s6,x1,x2,x3,x4,x5,x6;
  PetscScalar    *x,*b,*t;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  t  = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout;

  /* copy the b into temp work space according to permutation */
  ii = 0;
  for (i=0; i<n; i++) {
    ic      = 6*c[i];
    t[ii]   = b[ic];
    t[ii+1] = b[ic+1];
    t[ii+2] = b[ic+2];
    t[ii+3] = b[ic+3];
    t[ii+4] = b[ic+4];
    t[ii+5] = b[ic+5];
    ii += 6;
  } 

  /* forward solve the U^T */
  idx = 0;
  for (i=0; i<n; i++) {

    v     = aa + 36*diag[i];
    /* multiply by the inverse of the block diagonal */
    x1    = t[idx];   x2 = t[1+idx]; x3    = t[2+idx]; x4 = t[3+idx]; x5 = t[4+idx];
    x6    = t[5+idx]; 
    s1 = v[0]*x1  +  v[1]*x2 +  v[2]*x3 +  v[3]*x4 +  v[4]*x5 +  v[5]*x6;
    s2 = v[6]*x1  +  v[7]*x2 +  v[8]*x3 +  v[9]*x4 + v[10]*x5 + v[11]*x6;
    s3 = v[12]*x1 + v[13]*x2 + v[14]*x3 + v[15]*x4 + v[16]*x5 + v[17]*x6;
    s4 = v[18]*x1 + v[19]*x2 + v[20]*x3 + v[21]*x4 + v[22]*x5 + v[23]*x6;
    s5 = v[24]*x1 + v[25]*x2 + v[26]*x3 + v[27]*x4 + v[28]*x5 + v[29]*x6;
    s6 = v[30]*x1 + v[31]*x2 + v[32]*x3 + v[33]*x4 + v[34]*x5 + v[35]*x6;
    v += 36;

    vi    = aj + diag[i] + 1;
    nz    = ai[i+1] - diag[i] - 1;
    while (nz--) {
      oidx = 6*(*vi++);
      t[oidx]   -= v[0]*s1  +  v[1]*s2 +  v[2]*s3 +  v[3]*s4 +  v[4]*s5 +  v[5]*s6;
      t[oidx+1] -= v[6]*s1  +  v[7]*s2 +  v[8]*s3 +  v[9]*s4 + v[10]*s5 + v[11]*s6;
      t[oidx+2] -= v[12]*s1 + v[13]*s2 + v[14]*s3 + v[15]*s4 + v[16]*s5 + v[17]*s6;
      t[oidx+3] -= v[18]*s1 + v[19]*s2 + v[20]*s3 + v[21]*s4 + v[22]*s5 + v[23]*s6;
      t[oidx+4] -= v[24]*s1 + v[25]*s2 + v[26]*s3 + v[27]*s4 + v[28]*s5 + v[29]*s6;
      t[oidx+5] -= v[30]*s1 + v[31]*s2 + v[32]*s3 + v[33]*s4 + v[34]*s5 + v[35]*s6;
      v  += 36;
    }
    t[idx]   = s1;t[1+idx] = s2; t[2+idx] = s3;t[3+idx] = s4; t[4+idx] = s5;
    t[5+idx] = s6;
    idx += 6;
  }
  /* backward solve the L^T */
  for (i=n-1; i>=0; i--){
    v    = aa + 36*diag[i] - 36;
    vi   = aj + diag[i] - 1;
    nz   = diag[i] - ai[i];
    idt  = 6*i;
    s1 = t[idt];  s2 = t[1+idt]; s3 = t[2+idt];s4 = t[3+idt]; s5 = t[4+idt];
    s6 = t[5+idt];
    while (nz--) {
      idx   = 6*(*vi--);
      t[idx]   -=  v[0]*s1 +  v[1]*s2 +  v[2]*s3 +  v[3]*s4 +  v[4]*s5 +  v[5]*s6;
      t[idx+1] -=  v[6]*s1 +  v[7]*s2 +  v[8]*s3 +  v[9]*s4 + v[10]*s5 + v[11]*s6;
      t[idx+2] -= v[12]*s1 + v[13]*s2 + v[14]*s3 + v[15]*s4 + v[16]*s5 + v[17]*s6;
      t[idx+3] -= v[18]*s1 + v[19]*s2 + v[20]*s3 + v[21]*s4 + v[22]*s5 + v[23]*s6;
      t[idx+4] -= v[24]*s1 + v[25]*s2 + v[26]*s3 + v[27]*s4 + v[28]*s5 + v[29]*s6;
      t[idx+5] -= v[30]*s1 + v[31]*s2 + v[32]*s3 + v[33]*s4 + v[34]*s5 + v[35]*s6;
      v -= 36;
    }
  }

  /* copy t into x according to permutation */
  ii = 0;
  for (i=0; i<n; i++) {
    ir      = 6*r[i];
    x[ir]   = t[ii];
    x[ir+1] = t[ii+1];
    x[ir+2] = t[ii+2];
    x[ir+3] = t[ii+3];
    x[ir+4] = t[ii+4];
    x[ir+5] = t[ii+5];
    ii += 6;
  } 

  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*36*(a->nz) - 6.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolveTranspose_SeqBAIJ_6_newdatastruct"
PetscErrorCode MatSolveTranspose_SeqBAIJ_6_newdatastruct(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ *)A->data;
  PetscErrorCode ierr;
  IS             iscol=a->col,isrow=a->row;
  PetscInt       n=a->mbs,*vi,*ai=a->i,*aj=a->j,*diag=a->diag;
  const PetscInt *r,*c,*rout,*cout;
  PetscInt       nz,idx,idt,j,i,oidx,ii,ic,ir;
  PetscInt       bs=A->rmap->bs,bs2=a->bs2;
  MatScalar      *aa=a->a,*v;
  PetscScalar    s1,s2,s3,s4,s5,s6,x1,x2,x3,x4,x5,x6;
  PetscScalar    *x,*b,*t;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  t = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout;

  /* copy b into temp work space according to permutation */
  for(i=0;i<n;i++){
    ii = bs*i; ic = bs*c[i];
    t[ii] = b[ic]; t[ii+1] = b[ic+1]; t[ii+2] = b[ic+2]; t[ii+3] = b[ic+3];
    t[ii+4] = b[ic+4];  t[ii+5] = b[ic+5];
  }

  /* forward solve the U^T */
  idx = 0;
  for (i=0; i<n; i++) {
    v     = aa + bs2*diag[i];
    /* multiply by the inverse of the block diagonal */
    x1    = t[idx];   x2 = t[1+idx]; x3    = t[2+idx]; x4 = t[3+idx]; x5 = t[4+idx];
    x6    = t[5+idx]; 
    s1 = v[0]*x1  +  v[1]*x2 +  v[2]*x3 +  v[3]*x4 +  v[4]*x5 +  v[5]*x6;
    s2 = v[6]*x1  +  v[7]*x2 +  v[8]*x3 +  v[9]*x4 + v[10]*x5 + v[11]*x6;
    s3 = v[12]*x1 + v[13]*x2 + v[14]*x3 + v[15]*x4 + v[16]*x5 + v[17]*x6;
    s4 = v[18]*x1 + v[19]*x2 + v[20]*x3 + v[21]*x4 + v[22]*x5 + v[23]*x6;
    s5 = v[24]*x1 + v[25]*x2 + v[26]*x3 + v[27]*x4 + v[28]*x5 + v[29]*x6;
    s6 = v[30]*x1 + v[31]*x2 + v[32]*x3 + v[33]*x4 + v[34]*x5 + v[35]*x6;
    v -= bs2;

    vi    = aj + diag[i] - 1;
    nz    = diag[i] - diag[i+1] - 1;
    for(j=0;j>-nz;j--){
      oidx = bs*vi[j];
      t[oidx]   -= v[0]*s1  +  v[1]*s2 +  v[2]*s3 +  v[3]*s4 +  v[4]*s5 +  v[5]*s6;
      t[oidx+1] -= v[6]*s1  +  v[7]*s2 +  v[8]*s3 +  v[9]*s4 + v[10]*s5 + v[11]*s6;
      t[oidx+2] -= v[12]*s1 + v[13]*s2 + v[14]*s3 + v[15]*s4 + v[16]*s5 + v[17]*s6;
      t[oidx+3] -= v[18]*s1 + v[19]*s2 + v[20]*s3 + v[21]*s4 + v[22]*s5 + v[23]*s6;
      t[oidx+4] -= v[24]*s1 + v[25]*s2 + v[26]*s3 + v[27]*s4 + v[28]*s5 + v[29]*s6;
      t[oidx+5] -= v[30]*s1 + v[31]*s2 + v[32]*s3 + v[33]*s4 + v[34]*s5 + v[35]*s6;
      v  -= bs2;
    }
    t[idx]   = s1;t[1+idx] = s2;  t[2+idx] = s3;  t[3+idx] = s4; t[4+idx] =s5;
    t[5+idx] = s6;
    idx += bs;
  }
  /* backward solve the L^T */
  for (i=n-1; i>=0; i--){
    v    = aa + bs2*ai[i];
    vi   = aj + ai[i];
    nz   = ai[i+1] - ai[i];
    idt  = bs*i;
    s1   = t[idt];  s2 = t[1+idt];  s3 = t[2+idt];  s4 = t[3+idt]; s5 = t[4+idt];
    s6   = t[5+idt];
   for(j=0;j<nz;j++){
      idx   = bs*vi[j];
      t[idx]   -= v[0]*s1  +  v[1]*s2 +  v[2]*s3 +  v[3]*s4 +  v[4]*s5 +  v[5]*s6;
      t[idx+1] -= v[6]*s1  +  v[7]*s2 +  v[8]*s3 +  v[9]*s4 + v[10]*s5 + v[11]*s6;
      t[idx+2] -= v[12]*s1 + v[13]*s2 + v[14]*s3 + v[15]*s4 + v[16]*s5 + v[17]*s6;
      t[idx+3] -= v[18]*s1 + v[19]*s2 + v[20]*s3 + v[21]*s4 + v[22]*s5 + v[23]*s6;
      t[idx+4] -= v[24]*s1 + v[25]*s2 + v[26]*s3 + v[27]*s4 + v[28]*s5 + v[29]*s6;
      t[idx+5] -= v[30]*s1 + v[31]*s2 + v[32]*s3 + v[33]*s4 + v[34]*s5 + v[35]*s6;
      v += bs2;
    }
  }

  /* copy t into x according to permutation */
  for(i=0;i<n;i++){
    ii = bs*i;  ir = bs*r[i];
    x[ir] = t[ii];  x[ir+1] = t[ii+1]; x[ir+2] = t[ii+2];  x[ir+3] = t[ii+3];
    x[ir+4] = t[ii+4];  x[ir+5] = t[ii+5];
  }

  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*bs2*(a->nz) - bs*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolveTranspose_SeqBAIJ_7"
PetscErrorCode MatSolveTranspose_SeqBAIJ_7(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ *)A->data;
  IS             iscol=a->col,isrow=a->row;
  PetscErrorCode ierr;
  const PetscInt *r,*c,*rout,*cout;
  PetscInt       i,n=a->mbs,*vi,*ai=a->i,*aj=a->j,nz,idx,idt;
  PetscInt       *diag = a->diag,ii,ic,ir,oidx;
  MatScalar      *aa=a->a,*v;
  PetscScalar    s1,s2,s3,s4,s5,s6,s7,x1,x2,x3,x4,x5,x6,x7;
  PetscScalar    *x,*b,*t;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  t  = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout;

  /* copy the b into temp work space according to permutation */
  ii = 0;
  for (i=0; i<n; i++) {
    ic      = 7*c[i];
    t[ii]   = b[ic];
    t[ii+1] = b[ic+1];
    t[ii+2] = b[ic+2];
    t[ii+3] = b[ic+3];
    t[ii+4] = b[ic+4];
    t[ii+5] = b[ic+5];
    t[ii+6] = b[ic+6];
    ii += 7;
  } 

  /* forward solve the U^T */
  idx = 0;
  for (i=0; i<n; i++) {

    v     = aa + 49*diag[i];
    /* multiply by the inverse of the block diagonal */
    x1    = t[idx];   x2 = t[1+idx]; x3    = t[2+idx]; x4 = t[3+idx]; x5 = t[4+idx];
    x6    = t[5+idx]; x7 = t[6+idx];
    s1 = v[0]*x1  +  v[1]*x2 +  v[2]*x3 +  v[3]*x4 +  v[4]*x5 +  v[5]*x6 +  v[6]*x7;
    s2 = v[7]*x1  +  v[8]*x2 +  v[9]*x3 + v[10]*x4 + v[11]*x5 + v[12]*x6 + v[13]*x7;
    s3 = v[14]*x1 + v[15]*x2 + v[16]*x3 + v[17]*x4 + v[18]*x5 + v[19]*x6 + v[20]*x7;
    s4 = v[21]*x1 + v[22]*x2 + v[23]*x3 + v[24]*x4 + v[25]*x5 + v[26]*x6 + v[27]*x7;
    s5 = v[28]*x1 + v[29]*x2 + v[30]*x3 + v[31]*x4 + v[32]*x5 + v[33]*x6 + v[34]*x7;
    s6 = v[35]*x1 + v[36]*x2 + v[37]*x3 + v[38]*x4 + v[39]*x5 + v[40]*x6 + v[41]*x7;
    s7 = v[42]*x1 + v[43]*x2 + v[44]*x3 + v[45]*x4 + v[46]*x5 + v[47]*x6 + v[48]*x7;
    v += 49;

    vi    = aj + diag[i] + 1;
    nz    = ai[i+1] - diag[i] - 1;
    while (nz--) {
      oidx = 7*(*vi++);
      t[oidx]   -= v[0]*s1  +  v[1]*s2 +  v[2]*s3 +  v[3]*s4 +  v[4]*s5 +  v[5]*s6 +  v[6]*s7;
      t[oidx+1] -= v[7]*s1  +  v[8]*s2 +  v[9]*s3 + v[10]*s4 + v[11]*s5 + v[12]*s6 + v[13]*s7;
      t[oidx+2] -= v[14]*s1 + v[15]*s2 + v[16]*s3 + v[17]*s4 + v[18]*s5 + v[19]*s6 + v[20]*s7;
      t[oidx+3] -= v[21]*s1 + v[22]*s2 + v[23]*s3 + v[24]*s4 + v[25]*s5 + v[26]*s6 + v[27]*s7;
      t[oidx+4] -= v[28]*s1 + v[29]*s2 + v[30]*s3 + v[31]*s4 + v[32]*s5 + v[33]*s6 + v[34]*s7;
      t[oidx+5] -= v[35]*s1 + v[36]*s2 + v[37]*s3 + v[38]*s4 + v[39]*s5 + v[40]*s6 + v[41]*s7;
      t[oidx+6] -= v[42]*s1 + v[43]*s2 + v[44]*s3 + v[45]*s4 + v[46]*s5 + v[47]*s6 + v[48]*s7;
      v  += 49;
    }
    t[idx]   = s1;t[1+idx] = s2; t[2+idx] = s3;t[3+idx] = s4; t[4+idx] = s5;
    t[5+idx] = s6;t[6+idx] = s7;
    idx += 7;
  }
  /* backward solve the L^T */
  for (i=n-1; i>=0; i--){
    v    = aa + 49*diag[i] - 49;
    vi   = aj + diag[i] - 1;
    nz   = diag[i] - ai[i];
    idt  = 7*i;
    s1 = t[idt];  s2 = t[1+idt]; s3 = t[2+idt];s4 = t[3+idt]; s5 = t[4+idt];
    s6 = t[5+idt];s7 = t[6+idt]; 
    while (nz--) {
      idx   = 7*(*vi--);
      t[idx]   -=  v[0]*s1 +  v[1]*s2 +  v[2]*s3 +  v[3]*s4 +  v[4]*s5 +  v[5]*s6 +  v[6]*s7;
      t[idx+1] -=  v[7]*s1 +  v[8]*s2 +  v[9]*s3 + v[10]*s4 + v[11]*s5 + v[12]*s6 + v[13]*s7;
      t[idx+2] -= v[14]*s1 + v[15]*s2 + v[16]*s3 + v[17]*s4 + v[18]*s5 + v[19]*s6 + v[20]*s7;
      t[idx+3] -= v[21]*s1 + v[22]*s2 + v[23]*s3 + v[24]*s4 + v[25]*s5 + v[26]*s6 + v[27]*s7;
      t[idx+4] -= v[28]*s1 + v[29]*s2 + v[30]*s3 + v[31]*s4 + v[32]*s5 + v[33]*s6 + v[34]*s7;
      t[idx+5] -= v[35]*s1 + v[36]*s2 + v[37]*s3 + v[38]*s4 + v[39]*s5 + v[40]*s6 + v[41]*s7;
      t[idx+6] -= v[42]*s1 + v[43]*s2 + v[44]*s3 + v[45]*s4 + v[46]*s5 + v[47]*s6 + v[48]*s7;
      v -= 49;
    }
  }

  /* copy t into x according to permutation */
  ii = 0;
  for (i=0; i<n; i++) {
    ir      = 7*r[i];
    x[ir]   = t[ii];
    x[ir+1] = t[ii+1];
    x[ir+2] = t[ii+2];
    x[ir+3] = t[ii+3];
    x[ir+4] = t[ii+4];
    x[ir+5] = t[ii+5];
    x[ir+6] = t[ii+6];
    ii += 7;
  } 

  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*49*(a->nz) - 7.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
#undef __FUNCT__  
#define __FUNCT__ "MatSolveTranspose_SeqBAIJ_7_newdatastruct"
PetscErrorCode MatSolveTranspose_SeqBAIJ_7_newdatastruct(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ *)A->data;
  PetscErrorCode ierr;
  IS             iscol=a->col,isrow=a->row;
  PetscInt       n=a->mbs,*vi,*ai=a->i,*aj=a->j,*diag=a->diag;
  const PetscInt *r,*c,*rout,*cout;
  PetscInt       nz,idx,idt,j,i,oidx,ii,ic,ir;
  PetscInt       bs=A->rmap->bs,bs2=a->bs2;
  MatScalar      *aa=a->a,*v;
  PetscScalar    s1,s2,s3,s4,s5,s6,s7,x1,x2,x3,x4,x5,x6,x7;
  PetscScalar    *x,*b,*t;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  t = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout;

  /* copy b into temp work space according to permutation */
  for(i=0;i<n;i++){
    ii = bs*i; ic = bs*c[i];
    t[ii] = b[ic]; t[ii+1] = b[ic+1]; t[ii+2] = b[ic+2]; t[ii+3] = b[ic+3];
    t[ii+4] = b[ic+4];  t[ii+5] = b[ic+5];  t[ii+6] = b[ic+6];
  }

  /* forward solve the U^T */
  idx = 0;
  for (i=0; i<n; i++) {
    v     = aa + bs2*diag[i];
    /* multiply by the inverse of the block diagonal */
    x1    = t[idx];   x2 = t[1+idx]; x3    = t[2+idx]; x4 = t[3+idx]; x5 = t[4+idx];
    x6    = t[5+idx]; x7 = t[6+idx];
    s1 = v[0]*x1  +  v[1]*x2 +  v[2]*x3 +  v[3]*x4 +  v[4]*x5 +  v[5]*x6 +  v[6]*x7;
    s2 = v[7]*x1  +  v[8]*x2 +  v[9]*x3 + v[10]*x4 + v[11]*x5 + v[12]*x6 + v[13]*x7;
    s3 = v[14]*x1 + v[15]*x2 + v[16]*x3 + v[17]*x4 + v[18]*x5 + v[19]*x6 + v[20]*x7;
    s4 = v[21]*x1 + v[22]*x2 + v[23]*x3 + v[24]*x4 + v[25]*x5 + v[26]*x6 + v[27]*x7;
    s5 = v[28]*x1 + v[29]*x2 + v[30]*x3 + v[31]*x4 + v[32]*x5 + v[33]*x6 + v[34]*x7;
    s6 = v[35]*x1 + v[36]*x2 + v[37]*x3 + v[38]*x4 + v[39]*x5 + v[40]*x6 + v[41]*x7;
    s7 = v[42]*x1 + v[43]*x2 + v[44]*x3 + v[45]*x4 + v[46]*x5 + v[47]*x6 + v[48]*x7;
    v -= bs2;

    vi    = aj + diag[i] - 1;
    nz    = diag[i] - diag[i+1] - 1;
    for(j=0;j>-nz;j--){
      oidx = bs*vi[j];
      t[oidx]   -= v[0]*s1  +  v[1]*s2 +  v[2]*s3 +  v[3]*s4 +  v[4]*s5 +  v[5]*s6 +  v[6]*s7;
      t[oidx+1] -= v[7]*s1  +  v[8]*s2 +  v[9]*s3 + v[10]*s4 + v[11]*s5 + v[12]*s6 + v[13]*s7;
      t[oidx+2] -= v[14]*s1 + v[15]*s2 + v[16]*s3 + v[17]*s4 + v[18]*s5 + v[19]*s6 + v[20]*s7;
      t[oidx+3] -= v[21]*s1 + v[22]*s2 + v[23]*s3 + v[24]*s4 + v[25]*s5 + v[26]*s6 + v[27]*s7;
      t[oidx+4] -= v[28]*s1 + v[29]*s2 + v[30]*s3 + v[31]*s4 + v[32]*s5 + v[33]*s6 + v[34]*s7;
      t[oidx+5] -= v[35]*s1 + v[36]*s2 + v[37]*s3 + v[38]*s4 + v[39]*s5 + v[40]*s6 + v[41]*s7;
      t[oidx+6] -= v[42]*s1 + v[43]*s2 + v[44]*s3 + v[45]*s4 + v[46]*s5 + v[47]*s6 + v[48]*s7;
      v  -= bs2;
    }
    t[idx]   = s1;t[1+idx] = s2;  t[2+idx] = s3;  t[3+idx] = s4; t[4+idx] =s5;
    t[5+idx] = s6;  t[6+idx] = s7;
    idx += bs;
  }
  /* backward solve the L^T */
  for (i=n-1; i>=0; i--){
    v    = aa + bs2*ai[i];
    vi   = aj + ai[i];
    nz   = ai[i+1] - ai[i];
    idt  = bs*i;
    s1   = t[idt];  s2 = t[1+idt];  s3 = t[2+idt];  s4 = t[3+idt]; s5 = t[4+idt];
    s6   = t[5+idt];  s7 = t[6+idt];
   for(j=0;j<nz;j++){
      idx   = bs*vi[j];
      t[idx]   -=  v[0]*s1 +  v[1]*s2 +  v[2]*s3 +  v[3]*s4 +  v[4]*s5 +  v[5]*s6 +  v[6]*s7;
      t[idx+1] -=  v[7]*s1 +  v[8]*s2 +  v[9]*s3 + v[10]*s4 + v[11]*s5 + v[12]*s6 + v[13]*s7;
      t[idx+2] -= v[14]*s1 + v[15]*s2 + v[16]*s3 + v[17]*s4 + v[18]*s5 + v[19]*s6 + v[20]*s7;
      t[idx+3] -= v[21]*s1 + v[22]*s2 + v[23]*s3 + v[24]*s4 + v[25]*s5 + v[26]*s6 + v[27]*s7;
      t[idx+4] -= v[28]*s1 + v[29]*s2 + v[30]*s3 + v[31]*s4 + v[32]*s5 + v[33]*s6 + v[34]*s7;
      t[idx+5] -= v[35]*s1 + v[36]*s2 + v[37]*s3 + v[38]*s4 + v[39]*s5 + v[40]*s6 + v[41]*s7;
      t[idx+6] -= v[42]*s1 + v[43]*s2 + v[44]*s3 + v[45]*s4 + v[46]*s5 + v[47]*s6 + v[48]*s7;
      v += bs2;
    }
  }

  /* copy t into x according to permutation */
  for(i=0;i<n;i++){
    ii = bs*i;  ir = bs*r[i];
    x[ir] = t[ii];  x[ir+1] = t[ii+1]; x[ir+2] = t[ii+2];  x[ir+3] = t[ii+3];
    x[ir+4] = t[ii+4];  x[ir+5] = t[ii+5];  x[ir+6] = t[ii+6];
  }

  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*bs2*(a->nz) - bs*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* ----------------------------------------------------------- */
#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqBAIJ_N"
PetscErrorCode MatSolve_SeqBAIJ_N(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ *)A->data;
  IS             iscol=a->col,isrow=a->row;
  PetscErrorCode ierr;
  const PetscInt *r,*c,*rout,*cout,*ai=a->i,*aj=a->j,*vi;
  PetscInt       i,n=a->mbs;
  PetscInt       nz,bs=A->rmap->bs,bs2=a->bs2;
  MatScalar      *aa=a->a,*v;
  PetscScalar    *x,*b,*s,*t,*ls;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr); 
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  t  = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout + (n-1);

  /* forward solve the lower triangular */
  ierr = PetscMemcpy(t,b+bs*(*r++),bs*sizeof(PetscScalar));CHKERRQ(ierr);
  for (i=1; i<n; i++) {
    v   = aa + bs2*ai[i];
    vi  = aj + ai[i];
    nz  = a->diag[i] - ai[i];
    s = t + bs*i;
    ierr = PetscMemcpy(s,b+bs*(*r++),bs*sizeof(PetscScalar));CHKERRQ(ierr);
    while (nz--) {
      Kernel_v_gets_v_minus_A_times_w(bs,s,v,t+bs*(*vi++));
      v += bs2;
    }
  }
  /* backward solve the upper triangular */
  ls = a->solve_work + A->cmap->n;
  for (i=n-1; i>=0; i--){
    v   = aa + bs2*(a->diag[i] + 1);
    vi  = aj + a->diag[i] + 1;
    nz  = ai[i+1] - a->diag[i] - 1;
    ierr = PetscMemcpy(ls,t+i*bs,bs*sizeof(PetscScalar));CHKERRQ(ierr);
    while (nz--) {
      Kernel_v_gets_v_minus_A_times_w(bs,ls,v,t+bs*(*vi++));
      v += bs2;
    }
    Kernel_w_gets_A_times_v(bs,ls,aa+bs2*a->diag[i],t+i*bs);
    ierr = PetscMemcpy(x + bs*(*c--),t+i*bs,bs*sizeof(PetscScalar));CHKERRQ(ierr);
  }

  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*(a->bs2)*(a->nz) - A->rmap->bs*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* ----------------------------------------------------------- */
#undef __FUNCT__  
#define __FUNCT__ "MatSolveTranspose_SeqBAIJ_N"
PetscErrorCode MatSolveTranspose_SeqBAIJ_N(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ       *a=(Mat_SeqBAIJ *)A->data;
  IS                iscol=a->col,isrow=a->row;
  PetscErrorCode    ierr;
  const PetscInt    *r,*c,*rout,*cout,*ai=a->i,*aj=a->j,*vi;
  PetscInt          i,n=a->mbs,j;
  PetscInt          nz,bs=A->rmap->bs,bs2=a->bs2;
  const MatScalar   *aa=a->a,*v;
  PetscScalar       *x,*t,*ls;
  const PetscScalar *b;
  PetscFunctionBegin;
  ierr = VecGetArray(bb,(PetscScalar**)&b);CHKERRQ(ierr); 
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  t    = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout;

  /* copy the b into temp work space according to permutation */
  for (i=0; i<n; i++) {
    for (j=0; j<bs; j++) {
      t[i*bs+j] = b[c[i]*bs+j];
    }
  } 


  /* forward solve the upper triangular transpose */
  ls = a->solve_work + A->cmap->n;
  for (i=0; i<n; i++){
    ierr = PetscMemcpy(ls,t+i*bs,bs*sizeof(PetscScalar));CHKERRQ(ierr);
    Kernel_w_gets_transA_times_v(bs,ls,aa+bs2*a->diag[i],t+i*bs);
    v   = aa + bs2*(a->diag[i] + 1);
    vi  = aj + a->diag[i] + 1;
    nz  = ai[i+1] - a->diag[i] - 1;
    while (nz--) {
      Kernel_v_gets_v_minus_transA_times_w(bs,t+bs*(*vi++),v,t+i*bs);
      v += bs2;
    }
  }

  /* backward solve the lower triangular transpose */
  for (i=n-1; i>=0; i--) {
    v   = aa + bs2*ai[i];
    vi  = aj + ai[i];
    nz  = a->diag[i] - ai[i];
    while (nz--) {
      Kernel_v_gets_v_minus_transA_times_w(bs,t+bs*(*vi++),v,t+i*bs);
      v += bs2;
    }
  }

  /* copy t into x according to permutation */
  for (i=0; i<n; i++) {
    for (j=0; j<bs; j++) {
      x[bs*r[i]+j]   = t[bs*i+j];
    }
  } 

  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*(a->bs2)*(a->nz) - A->rmap->bs*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolveTranspose_SeqBAIJ_N_newdatastruct"
PetscErrorCode MatSolveTranspose_SeqBAIJ_N_newdatastruct(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ       *a=(Mat_SeqBAIJ *)A->data;
  IS                iscol=a->col,isrow=a->row;
  PetscErrorCode    ierr;
  const PetscInt    *r,*c,*rout,*cout,*ai=a->i,*aj=a->j,*vi,*diag=a->diag;
  PetscInt          i,n=a->mbs,j;
  PetscInt          nz,bs=A->rmap->bs,bs2=a->bs2;
  const MatScalar   *aa=a->a,*v;
  PetscScalar       *x,*t,*ls;
  const PetscScalar *b;
  PetscFunctionBegin;
  ierr = VecGetArray(bb,(PetscScalar**)&b);CHKERRQ(ierr); 
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  t    = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout;

  /* copy the b into temp work space according to permutation */
  for (i=0; i<n; i++) {
    for (j=0; j<bs; j++) {
      t[i*bs+j] = b[c[i]*bs+j];
    }
  } 


  /* forward solve the upper triangular transpose */
  ls = a->solve_work + A->cmap->n;
  for (i=0; i<n; i++){
    ierr = PetscMemcpy(ls,t+i*bs,bs*sizeof(PetscScalar));CHKERRQ(ierr);
    Kernel_w_gets_transA_times_v(bs,ls,aa+bs2*diag[i],t+i*bs);
    v   = aa + bs2*(diag[i] - 1);
    vi  = aj + diag[i] - 1;
    nz  = diag[i] - diag[i+1] - 1;
    for(j=0;j>-nz;j--){
      Kernel_v_gets_v_minus_transA_times_w(bs,t+bs*(vi[j]),v,t+i*bs);
      v -= bs2;
    }
  }

  /* backward solve the lower triangular transpose */
  for (i=n-1; i>=0; i--) {
    v   = aa + bs2*ai[i];
    vi  = aj + ai[i];
    nz  = ai[i+1] - ai[i];
    for(j=0;j<nz;j++){
      Kernel_v_gets_v_minus_transA_times_w(bs,t+bs*(vi[j]),v,t+i*bs);
      v += bs2;
    }
  }

  /* copy t into x according to permutation */
  for (i=0; i<n; i++) {
    for (j=0; j<bs; j++) {
      x[bs*r[i]+j]   = t[bs*i+j];
    }
  } 

  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*(a->bs2)*(a->nz) - A->rmap->bs*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqBAIJ_7"
PetscErrorCode MatSolve_SeqBAIJ_7(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ *)A->data;
  IS             iscol=a->col,isrow=a->row;
  PetscErrorCode ierr;
  const PetscInt *r,*c,*ai=a->i,*aj=a->j,*rout,*cout,*diag = a->diag,*vi;
  PetscInt       i,n=a->mbs,nz,idx,idt,idc;
  MatScalar      *aa=a->a,*v;
  PetscScalar    s1,s2,s3,s4,s5,s6,s7,x1,x2,x3,x4,x5,x6,x7;
  PetscScalar    *x,*b,*t;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  t  = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout + (n-1);

  /* forward solve the lower triangular */
  idx    = 7*(*r++); 
  t[0] = b[idx];   t[1] = b[1+idx]; 
  t[2] = b[2+idx]; t[3] = b[3+idx]; t[4] = b[4+idx];
  t[5] = b[5+idx]; t[6] = b[6+idx]; 

  for (i=1; i<n; i++) {
    v     = aa + 49*ai[i];
    vi    = aj + ai[i];
    nz    = diag[i] - ai[i];
    idx   = 7*(*r++); 
    s1  = b[idx];s2 = b[1+idx];s3 = b[2+idx];s4 = b[3+idx];
    s5  = b[4+idx];s6 = b[5+idx];s7 = b[6+idx];
    while (nz--) {
      idx   = 7*(*vi++);
      x1    = t[idx];  x2 = t[1+idx];x3 = t[2+idx];
      x4    = t[3+idx];x5 = t[4+idx];
      x6    = t[5+idx];x7 = t[6+idx];
      s1 -= v[0]*x1 + v[7]*x2  + v[14]*x3 + v[21]*x4 + v[28]*x5 + v[35]*x6 + v[42]*x7;
      s2 -= v[1]*x1 + v[8]*x2  + v[15]*x3 + v[22]*x4 + v[29]*x5 + v[36]*x6 + v[43]*x7;
      s3 -= v[2]*x1 + v[9]*x2  + v[16]*x3 + v[23]*x4 + v[30]*x5 + v[37]*x6 + v[44]*x7;
      s4 -= v[3]*x1 + v[10]*x2 + v[17]*x3 + v[24]*x4 + v[31]*x5 + v[38]*x6 + v[45]*x7;
      s5 -= v[4]*x1 + v[11]*x2 + v[18]*x3 + v[25]*x4 + v[32]*x5 + v[39]*x6 + v[46]*x7;
      s6 -= v[5]*x1 + v[12]*x2 + v[19]*x3 + v[26]*x4 + v[33]*x5 + v[40]*x6 + v[47]*x7;
      s7 -= v[6]*x1 + v[13]*x2 + v[20]*x3 + v[27]*x4 + v[34]*x5 + v[41]*x6 + v[48]*x7;
      v += 49;
    }
    idx = 7*i;
    t[idx]   = s1;t[1+idx] = s2;
    t[2+idx] = s3;t[3+idx] = s4; t[4+idx] = s5;
    t[5+idx] = s6;t[6+idx] = s7;
  }
  /* backward solve the upper triangular */
  for (i=n-1; i>=0; i--){
    v    = aa + 49*diag[i] + 49;
    vi   = aj + diag[i] + 1;
    nz   = ai[i+1] - diag[i] - 1;
    idt  = 7*i;
    s1 = t[idt];  s2 = t[1+idt]; 
    s3 = t[2+idt];s4 = t[3+idt]; s5 = t[4+idt];
    s6 = t[5+idt];s7 = t[6+idt]; 
    while (nz--) {
      idx   = 7*(*vi++);
      x1    = t[idx];   x2 = t[1+idx];
      x3    = t[2+idx]; x4 = t[3+idx]; x5 = t[4+idx];
      x6    = t[5+idx]; x7 = t[6+idx];
      s1 -= v[0]*x1 + v[7]*x2  + v[14]*x3 + v[21]*x4 + v[28]*x5 + v[35]*x6 + v[42]*x7;
      s2 -= v[1]*x1 + v[8]*x2  + v[15]*x3 + v[22]*x4 + v[29]*x5 + v[36]*x6 + v[43]*x7;
      s3 -= v[2]*x1 + v[9]*x2  + v[16]*x3 + v[23]*x4 + v[30]*x5 + v[37]*x6 + v[44]*x7;
      s4 -= v[3]*x1 + v[10]*x2 + v[17]*x3 + v[24]*x4 + v[31]*x5 + v[38]*x6 + v[45]*x7;
      s5 -= v[4]*x1 + v[11]*x2 + v[18]*x3 + v[25]*x4 + v[32]*x5 + v[39]*x6 + v[46]*x7;
      s6 -= v[5]*x1 + v[12]*x2 + v[19]*x3 + v[26]*x4 + v[33]*x5 + v[40]*x6 + v[47]*x7;
      s7 -= v[6]*x1 + v[13]*x2 + v[20]*x3 + v[27]*x4 + v[34]*x5 + v[41]*x6 + v[48]*x7;
      v += 49;
    }
    idc = 7*(*c--);
    v   = aa + 49*diag[i];
    x[idc]   = t[idt]   = v[0]*s1+v[7]*s2+v[14]*s3+
                                 v[21]*s4+v[28]*s5+v[35]*s6+v[42]*s7;
    x[1+idc] = t[1+idt] = v[1]*s1+v[8]*s2+v[15]*s3+
                                 v[22]*s4+v[29]*s5+v[36]*s6+v[43]*s7;
    x[2+idc] = t[2+idt] = v[2]*s1+v[9]*s2+v[16]*s3+
                                 v[23]*s4+v[30]*s5+v[37]*s6+v[44]*s7;
    x[3+idc] = t[3+idt] = v[3]*s1+v[10]*s2+v[17]*s3+
                                 v[24]*s4+v[31]*s5+v[38]*s6+v[45]*s7;
    x[4+idc] = t[4+idt] = v[4]*s1+v[11]*s2+v[18]*s3+
                                 v[25]*s4+v[32]*s5+v[39]*s6+v[46]*s7;
    x[5+idc] = t[5+idt] = v[5]*s1+v[12]*s2+v[19]*s3+
                                 v[26]*s4+v[33]*s5+v[40]*s6+v[47]*s7;
    x[6+idc] = t[6+idt] = v[6]*s1+v[13]*s2+v[20]*s3+
                                 v[27]*s4+v[34]*s5+v[41]*s6+v[48]*s7;
  }

  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*49*(a->nz) - 7.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqBAIJ_7_newdatastruct"
PetscErrorCode MatSolve_SeqBAIJ_7_newdatastruct(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ *)A->data;
  IS             iscol=a->col,isrow=a->row;
  PetscErrorCode ierr;
  const PetscInt *r,*c,*ai=a->i,*aj=a->j,*adiag=a->diag,*rout,*cout,*vi;
  PetscInt       i,n=a->mbs,nz,idx,idt,idc,m;
  MatScalar      *aa=a->a,*v;
  PetscScalar    s1,s2,s3,s4,s5,s6,s7,x1,x2,x3,x4,x5,x6,x7;
  PetscScalar    *x,*b,*t;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  t  = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout;

  /* forward solve the lower triangular */
  idx    = 7*r[0]; 
  t[0] = b[idx];   t[1] = b[1+idx]; 
  t[2] = b[2+idx]; t[3] = b[3+idx]; t[4] = b[4+idx];
  t[5] = b[5+idx]; t[6] = b[6+idx]; 

  for (i=1; i<n; i++) {
    v     = aa + 49*ai[i];
    vi    = aj + ai[i];
    nz    = ai[i+1] - ai[i];
    idx   = 7*r[i]; 
    s1  = b[idx];s2 = b[1+idx];s3 = b[2+idx];s4 = b[3+idx];
    s5  = b[4+idx];s6 = b[5+idx];s7 = b[6+idx];
    for(m=0;m<nz;m++){
      idx   = 7*vi[m];
      x1    = t[idx];  x2 = t[1+idx];x3 = t[2+idx];
      x4    = t[3+idx];x5 = t[4+idx];
      x6    = t[5+idx];x7 = t[6+idx];
      s1 -= v[0]*x1 + v[7]*x2  + v[14]*x3 + v[21]*x4 + v[28]*x5 + v[35]*x6 + v[42]*x7;
      s2 -= v[1]*x1 + v[8]*x2  + v[15]*x3 + v[22]*x4 + v[29]*x5 + v[36]*x6 + v[43]*x7;
      s3 -= v[2]*x1 + v[9]*x2  + v[16]*x3 + v[23]*x4 + v[30]*x5 + v[37]*x6 + v[44]*x7;
      s4 -= v[3]*x1 + v[10]*x2 + v[17]*x3 + v[24]*x4 + v[31]*x5 + v[38]*x6 + v[45]*x7;
      s5 -= v[4]*x1 + v[11]*x2 + v[18]*x3 + v[25]*x4 + v[32]*x5 + v[39]*x6 + v[46]*x7;
      s6 -= v[5]*x1 + v[12]*x2 + v[19]*x3 + v[26]*x4 + v[33]*x5 + v[40]*x6 + v[47]*x7;
      s7 -= v[6]*x1 + v[13]*x2 + v[20]*x3 + v[27]*x4 + v[34]*x5 + v[41]*x6 + v[48]*x7;
      v += 49;
    }
    idx = 7*i;
    t[idx]   = s1;t[1+idx] = s2;
    t[2+idx] = s3;t[3+idx] = s4; t[4+idx] = s5;
    t[5+idx] = s6;t[6+idx] = s7;
  }
  /* backward solve the upper triangular */
  for (i=n-1; i>=0; i--){
    v    = aa + 49*(adiag[i+1]+1);
    vi   = aj + adiag[i+1]+1;
    nz   = adiag[i] - adiag[i+1] - 1;
    idt  = 7*i;
    s1 = t[idt];  s2 = t[1+idt]; 
    s3 = t[2+idt];s4 = t[3+idt]; s5 = t[4+idt];
    s6 = t[5+idt];s7 = t[6+idt]; 
    for(m=0;m<nz;m++){
      idx   = 7*vi[m];
      x1    = t[idx];   x2 = t[1+idx];
      x3    = t[2+idx]; x4 = t[3+idx]; x5 = t[4+idx];
      x6    = t[5+idx]; x7 = t[6+idx];
      s1 -= v[0]*x1 + v[7]*x2  + v[14]*x3 + v[21]*x4 + v[28]*x5 + v[35]*x6 + v[42]*x7;
      s2 -= v[1]*x1 + v[8]*x2  + v[15]*x3 + v[22]*x4 + v[29]*x5 + v[36]*x6 + v[43]*x7;
      s3 -= v[2]*x1 + v[9]*x2  + v[16]*x3 + v[23]*x4 + v[30]*x5 + v[37]*x6 + v[44]*x7;
      s4 -= v[3]*x1 + v[10]*x2 + v[17]*x3 + v[24]*x4 + v[31]*x5 + v[38]*x6 + v[45]*x7;
      s5 -= v[4]*x1 + v[11]*x2 + v[18]*x3 + v[25]*x4 + v[32]*x5 + v[39]*x6 + v[46]*x7;
      s6 -= v[5]*x1 + v[12]*x2 + v[19]*x3 + v[26]*x4 + v[33]*x5 + v[40]*x6 + v[47]*x7;
      s7 -= v[6]*x1 + v[13]*x2 + v[20]*x3 + v[27]*x4 + v[34]*x5 + v[41]*x6 + v[48]*x7;
      v += 49;
    }
    idc = 7*c[i];
    x[idc]   = t[idt]   = v[0]*s1+v[7]*s2+v[14]*s3+
                                 v[21]*s4+v[28]*s5+v[35]*s6+v[42]*s7;
    x[1+idc] = t[1+idt] = v[1]*s1+v[8]*s2+v[15]*s3+
                                 v[22]*s4+v[29]*s5+v[36]*s6+v[43]*s7;
    x[2+idc] = t[2+idt] = v[2]*s1+v[9]*s2+v[16]*s3+
                                 v[23]*s4+v[30]*s5+v[37]*s6+v[44]*s7;
    x[3+idc] = t[3+idt] = v[3]*s1+v[10]*s2+v[17]*s3+
                                 v[24]*s4+v[31]*s5+v[38]*s6+v[45]*s7;
    x[4+idc] = t[4+idt] = v[4]*s1+v[11]*s2+v[18]*s3+
                                 v[25]*s4+v[32]*s5+v[39]*s6+v[46]*s7;
    x[5+idc] = t[5+idt] = v[5]*s1+v[12]*s2+v[19]*s3+
                                 v[26]*s4+v[33]*s5+v[40]*s6+v[47]*s7;
    x[6+idc] = t[6+idt] = v[6]*s1+v[13]*s2+v[20]*s3+
                                 v[27]*s4+v[34]*s5+v[41]*s6+v[48]*s7;
  }

  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*49*(a->nz) - 7.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqBAIJ_7_NaturalOrdering"
PetscErrorCode MatSolve_SeqBAIJ_7_NaturalOrdering(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ       *a = (Mat_SeqBAIJ *)A->data;
  PetscInt          i,n=a->mbs,*vi,*ai=a->i,*aj=a->j,nz,idx,idt;
  PetscErrorCode    ierr;
  PetscInt          *diag = a->diag,jdx;
  const MatScalar   *aa=a->a,*v;
  PetscScalar       *x,s1,s2,s3,s4,s5,s6,s7,x1,x2,x3,x4,x5,x6,x7;
  const PetscScalar *b;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,(PetscScalar**)&b);CHKERRQ(ierr); 
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr); 
  /* forward solve the lower triangular */
  idx    = 0;
  x[0] = b[idx];   x[1] = b[1+idx]; x[2] = b[2+idx]; 
  x[3] = b[3+idx]; x[4] = b[4+idx]; x[5] = b[5+idx];
  x[6] = b[6+idx]; 
  for (i=1; i<n; i++) {
    v     =  aa + 49*ai[i];
    vi    =  aj + ai[i];
    nz    =  diag[i] - ai[i];
    idx   =  7*i;
    s1  =  b[idx];   s2 = b[1+idx]; s3 = b[2+idx];
    s4  =  b[3+idx]; s5 = b[4+idx]; s6 = b[5+idx];
    s7  =  b[6+idx];
    while (nz--) {
      jdx   = 7*(*vi++);
      x1    = x[jdx];   x2 = x[1+jdx]; x3 = x[2+jdx];
      x4    = x[3+jdx]; x5 = x[4+jdx]; x6 = x[5+jdx];
      x7    = x[6+jdx];
      s1 -= v[0]*x1 + v[7]*x2  + v[14]*x3 + v[21]*x4 + v[28]*x5 + v[35]*x6 + v[42]*x7;
      s2 -= v[1]*x1 + v[8]*x2  + v[15]*x3 + v[22]*x4 + v[29]*x5 + v[36]*x6 + v[43]*x7;
      s3 -= v[2]*x1 + v[9]*x2  + v[16]*x3 + v[23]*x4 + v[30]*x5 + v[37]*x6 + v[44]*x7;
      s4 -= v[3]*x1 + v[10]*x2 + v[17]*x3 + v[24]*x4 + v[31]*x5 + v[38]*x6 + v[45]*x7;
      s5 -= v[4]*x1 + v[11]*x2 + v[18]*x3 + v[25]*x4 + v[32]*x5 + v[39]*x6 + v[46]*x7;
      s6 -= v[5]*x1 + v[12]*x2 + v[19]*x3 + v[26]*x4 + v[33]*x5 + v[40]*x6 + v[47]*x7;
      s7 -= v[6]*x1 + v[13]*x2 + v[20]*x3 + v[27]*x4 + v[34]*x5 + v[41]*x6 + v[48]*x7;
      v += 49;
     }
    x[idx]   = s1;
    x[1+idx] = s2;
    x[2+idx] = s3;
    x[3+idx] = s4;
    x[4+idx] = s5;
    x[5+idx] = s6;
    x[6+idx] = s7;
  }
  /* backward solve the upper triangular */
  for (i=n-1; i>=0; i--){
    v    = aa + 49*diag[i] + 49;
    vi   = aj + diag[i] + 1;
    nz   = ai[i+1] - diag[i] - 1;
    idt  = 7*i;
    s1 = x[idt];   s2 = x[1+idt]; 
    s3 = x[2+idt]; s4 = x[3+idt]; 
    s5 = x[4+idt]; s6 = x[5+idt];
    s7 = x[6+idt];
    while (nz--) {
      idx   = 7*(*vi++);
      x1    = x[idx];   x2 = x[1+idx]; x3 = x[2+idx]; 
      x4    = x[3+idx]; x5 = x[4+idx]; x6 = x[5+idx]; 
      x7    = x[6+idx];  
      s1 -= v[0]*x1 + v[7]*x2  + v[14]*x3 + v[21]*x4 + v[28]*x5 + v[35]*x6 + v[42]*x7;
      s2 -= v[1]*x1 + v[8]*x2  + v[15]*x3 + v[22]*x4 + v[29]*x5 + v[36]*x6 + v[43]*x7;
      s3 -= v[2]*x1 + v[9]*x2  + v[16]*x3 + v[23]*x4 + v[30]*x5 + v[37]*x6 + v[44]*x7;
      s4 -= v[3]*x1 + v[10]*x2 + v[17]*x3 + v[24]*x4 + v[31]*x5 + v[38]*x6 + v[45]*x7;
      s5 -= v[4]*x1 + v[11]*x2 + v[18]*x3 + v[25]*x4 + v[32]*x5 + v[39]*x6 + v[46]*x7;
      s6 -= v[5]*x1 + v[12]*x2 + v[19]*x3 + v[26]*x4 + v[33]*x5 + v[40]*x6 + v[47]*x7;
      s7 -= v[6]*x1 + v[13]*x2 + v[20]*x3 + v[27]*x4 + v[34]*x5 + v[41]*x6 + v[48]*x7;
      v += 49;
    }
    v        = aa + 49*diag[i];
    x[idt]   = v[0]*s1 + v[7]*s2  + v[14]*s3 + v[21]*s4 
                         + v[28]*s5 + v[35]*s6 + v[42]*s7;
    x[1+idt] = v[1]*s1 + v[8]*s2  + v[15]*s3 + v[22]*s4 
                         + v[29]*s5 + v[36]*s6 + v[43]*s7;
    x[2+idt] = v[2]*s1 + v[9]*s2  + v[16]*s3 + v[23]*s4 
                         + v[30]*s5 + v[37]*s6 + v[44]*s7;
    x[3+idt] = v[3]*s1 + v[10]*s2  + v[17]*s3 + v[24]*s4 
                         + v[31]*s5 + v[38]*s6 + v[45]*s7;
    x[4+idt] = v[4]*s1 + v[11]*s2  + v[18]*s3 + v[25]*s4 
                         + v[32]*s5 + v[39]*s6 + v[46]*s7;
    x[5+idt] = v[5]*s1 + v[12]*s2  + v[19]*s3 + v[26]*s4 
                         + v[33]*s5 + v[40]*s6 + v[47]*s7;
    x[6+idt] = v[6]*s1 + v[13]*s2  + v[20]*s3 + v[27]*s4 
                         + v[34]*s5 + v[41]*s6 + v[48]*s7;
  }

  ierr = VecRestoreArray(bb,(PetscScalar**)&b);CHKERRQ(ierr); 
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr); 
  ierr = PetscLogFlops(2.0*36*(a->nz) - 6.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqBAIJ_7_NaturalOrdering_newdatastruct"
PetscErrorCode MatSolve_SeqBAIJ_7_NaturalOrdering_newdatastruct(Mat A,Vec bb,Vec xx)
{
    Mat_SeqBAIJ       *a = (Mat_SeqBAIJ *)A->data;
    PetscInt          i,k,n=a->mbs,*vi,*ai=a->i,*aj=a->j,*adiag=a->diag,nz;
    PetscErrorCode    ierr;
    PetscInt          idx,jdx,idt;
    PetscInt          bs = A->rmap->bs,bs2 = a->bs2;
    const MatScalar   *aa=a->a,*v;
    PetscScalar       *x;
    const PetscScalar *b;
    PetscScalar        s1,s2,s3,s4,s5,s6,s7,x1,x2,x3,x4,x5,x6,x7; 

    PetscFunctionBegin;
    ierr = VecGetArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
    ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
    /* forward solve the lower triangular */
    idx    = 0;
    x[0] = b[idx]; x[1] = b[1+idx];x[2] = b[2+idx];x[3] = b[3+idx];
    x[4] = b[4+idx];x[5] = b[5+idx];x[6] = b[6+idx];
    for (i=1; i<n; i++) {
       v    = aa + bs2*ai[i];
       vi   = aj + ai[i];
       nz   = ai[i+1] - ai[i];
      idx   = bs*i;
       s1   = b[idx];s2 = b[1+idx];s3 = b[2+idx];s4 = b[3+idx];
       s5   = b[4+idx];s6 = b[5+idx];s7 = b[6+idx];
       for(k=0;k<nz;k++) {
          jdx   = bs*vi[k];
          x1    = x[jdx];x2 = x[1+jdx]; x3 =x[2+jdx];x4 =x[3+jdx];
	  x5    = x[4+jdx]; x6 = x[5+jdx];x7 = x[6+jdx];
          s1   -= v[0]*x1 + v[7]*x2 + v[14]*x3 + v[21]*x4  + v[28]*x5 + v[35]*x6 + v[42]*x7;
          s2   -= v[1]*x1 + v[8]*x2 + v[15]*x3 + v[22]*x4  + v[29]*x5 + v[36]*x6 + v[43]*x7;
          s3   -= v[2]*x1 + v[9]*x2 + v[16]*x3 + v[23]*x4  + v[30]*x5 + v[37]*x6 + v[44]*x7;
	  s4   -= v[3]*x1 + v[10]*x2 + v[17]*x3 + v[24]*x4  + v[31]*x5 + v[38]*x6 + v[45]*x7;
          s5   -= v[4]*x1 + v[11]*x2 + v[18]*x3 + v[25]*x4  + v[32]*x5 + v[39]*x6 + v[46]*x7;
	  s6   -= v[5]*x1 + v[12]*x2 + v[19]*x3 + v[26]*x4  + v[33]*x5 + v[40]*x6 + v[47]*x7;
	  s7   -= v[6]*x1 + v[13]*x2 + v[20]*x3 + v[27]*x4  + v[34]*x5 + v[41]*x6 + v[48]*x7;
          v   +=  bs2;
        }

       x[idx]   = s1;
       x[1+idx] = s2;
       x[2+idx] = s3;
       x[3+idx] = s4;
       x[4+idx] = s5;
       x[5+idx] = s6;			
       x[6+idx] = s7;
    }
 
   /* backward solve the upper triangular */
  for (i=n-1; i>=0; i--){
    v   = aa + bs2*(adiag[i+1]+1);
     vi  = aj + adiag[i+1]+1;
     nz  = adiag[i] - adiag[i+1]-1;
     idt = bs*i;
     s1 = x[idt];  s2 = x[1+idt];s3 = x[2+idt];s4 = x[3+idt];
     s5 = x[4+idt];s6 = x[5+idt];s7 = x[6+idt];	
    for(k=0;k<nz;k++) {
      idx   = bs*vi[k];
       x1    = x[idx];   x2 = x[1+idx]; x3 = x[2+idx];x4 = x[3+idx];
       x5    = x[4+idx];x6 = x[5+idx];x7 = x[6+idx];
       s1   -= v[0]*x1 + v[7]*x2 + v[14]*x3 + v[21]*x4  + v[28]*x5 + v[35]*x6 + v[42]*x7;
       s2   -= v[1]*x1 + v[8]*x2 + v[15]*x3 + v[22]*x4  + v[29]*x5 + v[36]*x6 + v[43]*x7;
       s3   -= v[2]*x1 + v[9]*x2 + v[16]*x3 + v[23]*x4  + v[30]*x5 + v[37]*x6 + v[44]*x7;
       s4   -= v[3]*x1 + v[10]*x2 + v[17]*x3 + v[24]*x4  + v[31]*x5 + v[38]*x6 + v[45]*x7;
       s5   -= v[4]*x1 + v[11]*x2 + v[18]*x3 + v[25]*x4  + v[32]*x5 + v[39]*x6 + v[46]*x7;
       s6   -= v[5]*x1 + v[12]*x2 + v[19]*x3 + v[26]*x4  + v[33]*x5 + v[40]*x6 + v[47]*x7;
       s7   -= v[6]*x1 + v[13]*x2 + v[20]*x3 + v[27]*x4  + v[34]*x5 + v[41]*x6 + v[48]*x7;
        v   +=  bs2;
    }
    /* x = inv_diagonal*x */
    x[idt]   = v[0]*s1 + v[7]*s2 + v[14]*s3 + v[21]*s4  + v[28]*s5 + v[35]*s6 + v[42]*s7;
    x[1+idt] = v[1]*s1 + v[8]*s2 + v[15]*s3 + v[22]*s4  + v[29]*s5 + v[36]*s6 + v[43]*s7;
    x[2+idt] = v[2]*s1 + v[9]*s2 + v[16]*s3 + v[23]*s4  + v[30]*s5 + v[37]*s6 + v[44]*s7;
    x[3+idt] = v[3]*s1 + v[10]*s2 + v[17]*s3 + v[24]*s4  + v[31]*s5 + v[38]*s6 + v[45]*s7;
    x[4+idt] = v[4]*s1 + v[11]*s2 + v[18]*s3 + v[25]*s4  + v[32]*s5 + v[39]*s6 + v[46]*s7;
    x[5+idt] = v[5]*s1 + v[12]*s2 + v[19]*s3 + v[26]*s4  + v[33]*s5 + v[40]*s6 + v[47]*s7;
    x[6+idt] = v[6]*s1 + v[13]*s2 + v[20]*s3 + v[27]*s4  + v[34]*s5 + v[41]*s6 + v[48]*s7;
  } 

  ierr = VecRestoreArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*bs2*(a->nz) - bs*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqBAIJ_6"
PetscErrorCode MatSolve_SeqBAIJ_6(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ       *a=(Mat_SeqBAIJ *)A->data;
  IS                iscol=a->col,isrow=a->row;
  PetscErrorCode    ierr;
  const PetscInt    *r,*c,*rout,*cout;
  PetscInt          *diag = a->diag,i,n=a->mbs,*vi,*ai=a->i,*aj=a->j,nz,idx,idt,idc;
  const MatScalar   *aa=a->a,*v;
  PetscScalar       *x,s1,s2,s3,s4,s5,s6,x1,x2,x3,x4,x5,x6,*t;
  const PetscScalar *b;
  PetscFunctionBegin;
  ierr = VecGetArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  t  = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout + (n-1);

  /* forward solve the lower triangular */
  idx    = 6*(*r++); 
  t[0] = b[idx];   t[1] = b[1+idx]; 
  t[2] = b[2+idx]; t[3] = b[3+idx]; 
  t[4] = b[4+idx]; t[5] = b[5+idx];
  for (i=1; i<n; i++) {
    v     = aa + 36*ai[i];
    vi    = aj + ai[i];
    nz    = diag[i] - ai[i];
    idx   = 6*(*r++); 
    s1  = b[idx];s2 = b[1+idx];s3 = b[2+idx];s4 = b[3+idx];
    s5  = b[4+idx]; s6 = b[5+idx];
    while (nz--) {
      idx   = 6*(*vi++);
      x1    = t[idx];   x2 = t[1+idx]; x3 = t[2+idx];
      x4    = t[3+idx]; x5 = t[4+idx]; x6 = t[5+idx];
      s1 -= v[0]*x1 + v[6]*x2  + v[12]*x3 + v[18]*x4 + v[24]*x5 + v[30]*x6;
      s2 -= v[1]*x1 + v[7]*x2  + v[13]*x3 + v[19]*x4 + v[25]*x5 + v[31]*x6;
      s3 -= v[2]*x1 + v[8]*x2  + v[14]*x3 + v[20]*x4 + v[26]*x5 + v[32]*x6;
      s4 -= v[3]*x1 + v[9]*x2  + v[15]*x3 + v[21]*x4 + v[27]*x5 + v[33]*x6;
      s5 -= v[4]*x1 + v[10]*x2 + v[16]*x3 + v[22]*x4 + v[28]*x5 + v[34]*x6;
      s6 -= v[5]*x1 + v[11]*x2 + v[17]*x3 + v[23]*x4 + v[29]*x5 + v[35]*x6;
      v += 36;
    }
    idx = 6*i;
    t[idx]   = s1;t[1+idx] = s2;
    t[2+idx] = s3;t[3+idx] = s4; 
    t[4+idx] = s5;t[5+idx] = s6;
  }
  /* backward solve the upper triangular */
  for (i=n-1; i>=0; i--){
    v    = aa + 36*diag[i] + 36;
    vi   = aj + diag[i] + 1;
    nz   = ai[i+1] - diag[i] - 1;
    idt  = 6*i;
    s1 = t[idt];  s2 = t[1+idt]; 
    s3 = t[2+idt];s4 = t[3+idt]; 
    s5 = t[4+idt];s6 = t[5+idt];
    while (nz--) {
      idx   = 6*(*vi++);
      x1    = t[idx];   x2 = t[1+idx];
      x3    = t[2+idx]; x4 = t[3+idx]; 
      x5    = t[4+idx]; x6 = t[5+idx];
      s1 -= v[0]*x1 + v[6]*x2  + v[12]*x3 + v[18]*x4 + v[24]*x5 + v[30]*x6;
      s2 -= v[1]*x1 + v[7]*x2  + v[13]*x3 + v[19]*x4 + v[25]*x5 + v[31]*x6;
      s3 -= v[2]*x1 + v[8]*x2  + v[14]*x3 + v[20]*x4 + v[26]*x5 + v[32]*x6;
      s4 -= v[3]*x1 + v[9]*x2  + v[15]*x3 + v[21]*x4 + v[27]*x5 + v[33]*x6;
      s5 -= v[4]*x1 + v[10]*x2 + v[16]*x3 + v[22]*x4 + v[28]*x5 + v[34]*x6;
      s6 -= v[5]*x1 + v[11]*x2 + v[17]*x3 + v[23]*x4 + v[29]*x5 + v[35]*x6;
      v += 36;
    }
    idc = 6*(*c--);
    v   = aa + 36*diag[i];
    x[idc]   = t[idt]   = v[0]*s1+v[6]*s2+v[12]*s3+
                                 v[18]*s4+v[24]*s5+v[30]*s6;
    x[1+idc] = t[1+idt] = v[1]*s1+v[7]*s2+v[13]*s3+
                                 v[19]*s4+v[25]*s5+v[31]*s6;
    x[2+idc] = t[2+idt] = v[2]*s1+v[8]*s2+v[14]*s3+
                                 v[20]*s4+v[26]*s5+v[32]*s6;
    x[3+idc] = t[3+idt] = v[3]*s1+v[9]*s2+v[15]*s3+
                                 v[21]*s4+v[27]*s5+v[33]*s6;
    x[4+idc] = t[4+idt] = v[4]*s1+v[10]*s2+v[16]*s3+
                                 v[22]*s4+v[28]*s5+v[34]*s6;
    x[5+idc] = t[5+idt] = v[5]*s1+v[11]*s2+v[17]*s3+
                                 v[23]*s4+v[29]*s5+v[35]*s6;
  }

  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*36*(a->nz) - 6.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqBAIJ_6_newdatastruct"
PetscErrorCode MatSolve_SeqBAIJ_6_newdatastruct(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ       *a=(Mat_SeqBAIJ *)A->data;
  IS                iscol=a->col,isrow=a->row;
  PetscErrorCode    ierr;
  const PetscInt    *r,*c,*rout,*cout;
  PetscInt          i,n=a->mbs,*vi,*ai=a->i,*aj=a->j,*adiag=a->diag,nz,idx,idt,idc,m;
  const MatScalar   *aa=a->a,*v;
  PetscScalar       *x,s1,s2,s3,s4,s5,s6,x1,x2,x3,x4,x5,x6,*t;
  const PetscScalar *b;
  PetscFunctionBegin;
  ierr = VecGetArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  t  = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout;

  /* forward solve the lower triangular */
  idx    = 6*r[0]; 
  t[0] = b[idx];   t[1] = b[1+idx]; 
  t[2] = b[2+idx]; t[3] = b[3+idx]; 
  t[4] = b[4+idx]; t[5] = b[5+idx];
  for (i=1; i<n; i++) {
    v     = aa + 36*ai[i];
    vi    = aj + ai[i];
    nz    = ai[i+1] - ai[i];
    idx   = 6*r[i]; 
    s1  = b[idx];s2 = b[1+idx];s3 = b[2+idx];s4 = b[3+idx];
    s5  = b[4+idx]; s6 = b[5+idx];
    for(m=0;m<nz;m++){
      idx   = 6*vi[m];
      x1    = t[idx];   x2 = t[1+idx]; x3 = t[2+idx];
      x4    = t[3+idx]; x5 = t[4+idx]; x6 = t[5+idx];
      s1 -= v[0]*x1 + v[6]*x2  + v[12]*x3 + v[18]*x4 + v[24]*x5 + v[30]*x6;
      s2 -= v[1]*x1 + v[7]*x2  + v[13]*x3 + v[19]*x4 + v[25]*x5 + v[31]*x6;
      s3 -= v[2]*x1 + v[8]*x2  + v[14]*x3 + v[20]*x4 + v[26]*x5 + v[32]*x6;
      s4 -= v[3]*x1 + v[9]*x2  + v[15]*x3 + v[21]*x4 + v[27]*x5 + v[33]*x6;
      s5 -= v[4]*x1 + v[10]*x2 + v[16]*x3 + v[22]*x4 + v[28]*x5 + v[34]*x6;
      s6 -= v[5]*x1 + v[11]*x2 + v[17]*x3 + v[23]*x4 + v[29]*x5 + v[35]*x6;
      v += 36;
    }
    idx = 6*i;
    t[idx]   = s1;t[1+idx] = s2;
    t[2+idx] = s3;t[3+idx] = s4; 
    t[4+idx] = s5;t[5+idx] = s6;
  }
  /* backward solve the upper triangular */
  for (i=n-1; i>=0; i--){
    v    = aa + 36*(adiag[i+1]+1);
    vi   = aj + adiag[i+1]+1;
    nz   = adiag[i] - adiag[i+1] - 1;
    idt  = 6*i;
    s1 = t[idt];  s2 = t[1+idt]; 
    s3 = t[2+idt];s4 = t[3+idt]; 
    s5 = t[4+idt];s6 = t[5+idt];
    for(m=0;m<nz;m++){
      idx   = 6*vi[m];
      x1    = t[idx];   x2 = t[1+idx];
      x3    = t[2+idx]; x4 = t[3+idx]; 
      x5    = t[4+idx]; x6 = t[5+idx];
      s1 -= v[0]*x1 + v[6]*x2  + v[12]*x3 + v[18]*x4 + v[24]*x5 + v[30]*x6;
      s2 -= v[1]*x1 + v[7]*x2  + v[13]*x3 + v[19]*x4 + v[25]*x5 + v[31]*x6;
      s3 -= v[2]*x1 + v[8]*x2  + v[14]*x3 + v[20]*x4 + v[26]*x5 + v[32]*x6;
      s4 -= v[3]*x1 + v[9]*x2  + v[15]*x3 + v[21]*x4 + v[27]*x5 + v[33]*x6;
      s5 -= v[4]*x1 + v[10]*x2 + v[16]*x3 + v[22]*x4 + v[28]*x5 + v[34]*x6;
      s6 -= v[5]*x1 + v[11]*x2 + v[17]*x3 + v[23]*x4 + v[29]*x5 + v[35]*x6;
      v += 36;
    }
    idc = 6*c[i];
    x[idc]   = t[idt]   = v[0]*s1+v[6]*s2+v[12]*s3+
                                 v[18]*s4+v[24]*s5+v[30]*s6;
    x[1+idc] = t[1+idt] = v[1]*s1+v[7]*s2+v[13]*s3+
                                 v[19]*s4+v[25]*s5+v[31]*s6;
    x[2+idc] = t[2+idt] = v[2]*s1+v[8]*s2+v[14]*s3+
                                 v[20]*s4+v[26]*s5+v[32]*s6;
    x[3+idc] = t[3+idt] = v[3]*s1+v[9]*s2+v[15]*s3+
                                 v[21]*s4+v[27]*s5+v[33]*s6;
    x[4+idc] = t[4+idt] = v[4]*s1+v[10]*s2+v[16]*s3+
                                 v[22]*s4+v[28]*s5+v[34]*s6;
    x[5+idc] = t[5+idt] = v[5]*s1+v[11]*s2+v[17]*s3+
                                 v[23]*s4+v[29]*s5+v[35]*s6;
  }

  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*36*(a->nz) - 6.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqBAIJ_6_NaturalOrdering"
PetscErrorCode MatSolve_SeqBAIJ_6_NaturalOrdering(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ       *a = (Mat_SeqBAIJ *)A->data;
  PetscInt          i,n=a->mbs,*vi,*ai=a->i,*aj=a->j,nz,idx,idt;
  PetscErrorCode    ierr;
  PetscInt          *diag = a->diag,jdx;
  const MatScalar   *aa=a->a,*v;
  PetscScalar       *x,s1,s2,s3,s4,s5,s6,x1,x2,x3,x4,x5,x6;
  const PetscScalar *b;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,(PetscScalar**)&b);CHKERRQ(ierr); 
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr); 
  /* forward solve the lower triangular */
  idx    = 0;
  x[0] = b[idx];   x[1] = b[1+idx]; x[2] = b[2+idx]; 
  x[3] = b[3+idx]; x[4] = b[4+idx]; x[5] = b[5+idx];
  for (i=1; i<n; i++) {
    v     =  aa + 36*ai[i];
    vi    =  aj + ai[i];
    nz    =  diag[i] - ai[i];
    idx   =  6*i;
    s1  =  b[idx];   s2 = b[1+idx]; s3 = b[2+idx];
    s4  =  b[3+idx]; s5 = b[4+idx]; s6 = b[5+idx];
    while (nz--) {
      jdx   = 6*(*vi++);
      x1    = x[jdx];   x2 = x[1+jdx]; x3 = x[2+jdx];
      x4    = x[3+jdx]; x5 = x[4+jdx]; x6 = x[5+jdx];
      s1 -= v[0]*x1 + v[6]*x2  + v[12]*x3 + v[18]*x4 + v[24]*x5 + v[30]*x6;
      s2 -= v[1]*x1 + v[7]*x2  + v[13]*x3 + v[19]*x4 + v[25]*x5 + v[31]*x6;
      s3 -= v[2]*x1 + v[8]*x2  + v[14]*x3 + v[20]*x4 + v[26]*x5 + v[32]*x6;
      s4 -= v[3]*x1 + v[9]*x2  + v[15]*x3 + v[21]*x4 + v[27]*x5 + v[33]*x6;
      s5 -= v[4]*x1 + v[10]*x2 + v[16]*x3 + v[22]*x4 + v[28]*x5 + v[34]*x6;
      s6 -= v[5]*x1 + v[11]*x2 + v[17]*x3 + v[23]*x4 + v[29]*x5 + v[35]*x6;
      v += 36;
     }
    x[idx]   = s1;
    x[1+idx] = s2;
    x[2+idx] = s3;
    x[3+idx] = s4;
    x[4+idx] = s5;
    x[5+idx] = s6;
  }
  /* backward solve the upper triangular */
  for (i=n-1; i>=0; i--){
    v    = aa + 36*diag[i] + 36;
    vi   = aj + diag[i] + 1;
    nz   = ai[i+1] - diag[i] - 1;
    idt  = 6*i;
    s1 = x[idt];   s2 = x[1+idt]; 
    s3 = x[2+idt]; s4 = x[3+idt]; 
    s5 = x[4+idt]; s6 = x[5+idt];
    while (nz--) {
      idx   = 6*(*vi++);
      x1    = x[idx];   x2 = x[1+idx]; x3 = x[2+idx]; 
      x4    = x[3+idx]; x5 = x[4+idx]; x6 = x[5+idx];   
      s1 -= v[0]*x1 + v[6]*x2  + v[12]*x3 + v[18]*x4 + v[24]*x5 + v[30]*x6;
      s2 -= v[1]*x1 + v[7]*x2  + v[13]*x3 + v[19]*x4 + v[25]*x5 + v[31]*x6;
      s3 -= v[2]*x1 + v[8]*x2  + v[14]*x3 + v[20]*x4 + v[26]*x5 + v[32]*x6;
      s4 -= v[3]*x1 + v[9]*x2  + v[15]*x3 + v[21]*x4 + v[27]*x5 + v[33]*x6;
      s5 -= v[4]*x1 + v[10]*x2 + v[16]*x3 + v[22]*x4 + v[28]*x5 + v[34]*x6;
      s6 -= v[5]*x1 + v[11]*x2 + v[17]*x3 + v[23]*x4 + v[29]*x5 + v[35]*x6;
      v += 36;
    }
    v        = aa + 36*diag[i];
    x[idt]   = v[0]*s1 + v[6]*s2  + v[12]*s3 + v[18]*s4 + v[24]*s5 + v[30]*s6;
    x[1+idt] = v[1]*s1 + v[7]*s2  + v[13]*s3 + v[19]*s4 + v[25]*s5 + v[31]*s6;
    x[2+idt] = v[2]*s1 + v[8]*s2  + v[14]*s3 + v[20]*s4 + v[26]*s5 + v[32]*s6;
    x[3+idt] = v[3]*s1 + v[9]*s2  + v[15]*s3 + v[21]*s4 + v[27]*s5 + v[33]*s6;
    x[4+idt] = v[4]*s1 + v[10]*s2 + v[16]*s3 + v[22]*s4 + v[28]*s5 + v[34]*s6;
    x[5+idt] = v[5]*s1 + v[11]*s2 + v[17]*s3 + v[23]*s4 + v[29]*s5 + v[35]*s6;
  }

  ierr = VecRestoreArray(bb,(PetscScalar**)&b);CHKERRQ(ierr); 
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr); 
  ierr = PetscLogFlops(2.0*36*(a->nz) - 6.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqBAIJ_6_NaturalOrdering_newdatastruct"
PetscErrorCode MatSolve_SeqBAIJ_6_NaturalOrdering_newdatastruct(Mat A,Vec bb,Vec xx)
{
    Mat_SeqBAIJ       *a = (Mat_SeqBAIJ *)A->data;
    PetscInt          i,k,n=a->mbs,*vi,*ai=a->i,*aj=a->j,*adiag=a->diag,nz;
    PetscErrorCode    ierr;
    PetscInt          idx,jdx,idt;
    PetscInt          bs = A->rmap->bs,bs2 = a->bs2;
    const MatScalar   *aa=a->a,*v;
    PetscScalar       *x;
    const PetscScalar *b;
    PetscScalar        s1,s2,s3,s4,s5,s6,x1,x2,x3,x4,x5,x6; 

    PetscFunctionBegin;
    ierr = VecGetArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
    ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
    /* forward solve the lower triangular */
    idx    = 0;
    x[0] = b[idx]; x[1] = b[1+idx];x[2] = b[2+idx];x[3] = b[3+idx];
    x[4] = b[4+idx];x[5] = b[5+idx];
    for (i=1; i<n; i++) {
       v    = aa + bs2*ai[i];
       vi   = aj + ai[i];
       nz   = ai[i+1] - ai[i];
      idx   = bs*i;
       s1   = b[idx];s2 = b[1+idx];s3 = b[2+idx];s4 = b[3+idx];
       s5   = b[4+idx];s6 = b[5+idx];
       for(k=0;k<nz;k++){
          jdx   = bs*vi[k];
          x1    = x[jdx];x2 = x[1+jdx]; x3 =x[2+jdx];x4 =x[3+jdx];
	  x5    = x[4+jdx]; x6 = x[5+jdx];
          s1   -= v[0]*x1 + v[6]*x2 + v[12]*x3 + v[18]*x4  + v[24]*x5 + v[30]*x6;
          s2   -= v[1]*x1 + v[7]*x2 + v[13]*x3 + v[19]*x4  + v[25]*x5 + v[31]*x6;;
          s3   -= v[2]*x1 + v[8]*x2 + v[14]*x3 + v[20]*x4  + v[26]*x5 + v[32]*x6;
	  s4   -= v[3]*x1 + v[9]*x2 + v[15]*x3 + v[21]*x4  + v[27]*x5 + v[33]*x6;
          s5   -= v[4]*x1 + v[10]*x2 + v[16]*x3 + v[22]*x4  + v[28]*x5 + v[34]*x6;
	  s6   -= v[5]*x1 + v[11]*x2 + v[17]*x3 + v[23]*x4  + v[29]*x5 + v[35]*x6;
          v   +=  bs2;
        }

       x[idx]   = s1;
       x[1+idx] = s2;
       x[2+idx] = s3;
       x[3+idx] = s4;
       x[4+idx] = s5;
       x[5+idx] = s6;			
    }
 
   /* backward solve the upper triangular */
  for (i=n-1; i>=0; i--){
    v   = aa + bs2*(adiag[i+1]+1);
     vi  = aj + adiag[i+1]+1;
     nz  = adiag[i] - adiag[i+1]-1;
     idt = bs*i;
     s1 = x[idt];  s2 = x[1+idt];s3 = x[2+idt];s4 = x[3+idt];
     s5 = x[4+idt];s6 = x[5+idt];	
     for(k=0;k<nz;k++){
      idx   = bs*vi[k];
       x1    = x[idx];   x2 = x[1+idx]; x3 = x[2+idx];x4 = x[3+idx];
       x5    = x[4+idx];x6 = x[5+idx];
       s1   -= v[0]*x1 + v[6]*x2 + v[12]*x3 + v[18]*x4  + v[24]*x5 + v[30]*x6;
       s2   -= v[1]*x1 + v[7]*x2 + v[13]*x3 + v[19]*x4  + v[25]*x5 + v[31]*x6;;
       s3   -= v[2]*x1 + v[8]*x2 + v[14]*x3 + v[20]*x4  + v[26]*x5 + v[32]*x6;
       s4   -= v[3]*x1 + v[9]*x2 + v[15]*x3 + v[21]*x4  + v[27]*x5 + v[33]*x6;
       s5   -= v[4]*x1 + v[10]*x2 + v[16]*x3 + v[22]*x4  + v[28]*x5 + v[34]*x6;
       s6   -= v[5]*x1 + v[11]*x2 + v[17]*x3 + v[23]*x4  + v[29]*x5 + v[35]*x6;
        v   +=  bs2;
    }
    /* x = inv_diagonal*x */
   x[idt]   = v[0]*s1 + v[6]*s2 + v[12]*s3 + v[18]*s4 + v[24]*s5 + v[30]*s6;
   x[1+idt] = v[1]*s1 + v[7]*s2 + v[13]*s3 + v[19]*s4 + v[25]*s5 + v[31]*s6;
   x[2+idt] = v[2]*s1 + v[8]*s2 + v[14]*s3 + v[20]*s4 + v[26]*s5 + v[32]*s6;
   x[3+idt] = v[3]*s1 + v[9]*s2 + v[15]*s3 + v[21]*s4 + v[27]*s5 + v[33]*s6;
   x[4+idt] = v[4]*s1 + v[10]*s2 + v[16]*s3 + v[22]*s4 + v[28]*s5 + v[34]*s6;
   x[5+idt] = v[5]*s1 + v[11]*s2 + v[17]*s3 + v[23]*s4 + v[29]*s5 + v[35]*s6;
  } 

  ierr = VecRestoreArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*bs2*(a->nz) - bs*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqBAIJ_5"
PetscErrorCode MatSolve_SeqBAIJ_5(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ       *a=(Mat_SeqBAIJ *)A->data;
  IS                iscol=a->col,isrow=a->row;
  PetscErrorCode    ierr;
  const PetscInt    *r,*c,*rout,*cout,*diag = a->diag;
  PetscInt          i,n=a->mbs,*vi,*ai=a->i,*aj=a->j,nz,idx,idt,idc;
  const MatScalar   *aa=a->a,*v;
  PetscScalar       *x,s1,s2,s3,s4,s5,x1,x2,x3,x4,x5,*t;
  const PetscScalar *b;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  t  = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout + (n-1);

  /* forward solve the lower triangular */
  idx    = 5*(*r++); 
  t[0] = b[idx];   t[1] = b[1+idx]; 
  t[2] = b[2+idx]; t[3] = b[3+idx]; t[4] = b[4+idx];
  for (i=1; i<n; i++) {
    v     = aa + 25*ai[i];
    vi    = aj + ai[i];
    nz    = diag[i] - ai[i];
    idx   = 5*(*r++); 
    s1  = b[idx];s2 = b[1+idx];s3 = b[2+idx];s4 = b[3+idx];
    s5  = b[4+idx];
    while (nz--) {
      idx   = 5*(*vi++);
      x1    = t[idx];  x2 = t[1+idx];x3 = t[2+idx];
      x4    = t[3+idx];x5 = t[4+idx];
      s1 -= v[0]*x1 + v[5]*x2 + v[10]*x3 + v[15]*x4 + v[20]*x5;
      s2 -= v[1]*x1 + v[6]*x2 + v[11]*x3 + v[16]*x4 + v[21]*x5;
      s3 -= v[2]*x1 + v[7]*x2 + v[12]*x3 + v[17]*x4 + v[22]*x5;
      s4 -= v[3]*x1 + v[8]*x2 + v[13]*x3 + v[18]*x4 + v[23]*x5;
      s5 -= v[4]*x1 + v[9]*x2 + v[14]*x3 + v[19]*x4 + v[24]*x5;
      v += 25;
    }
    idx = 5*i;
    t[idx]   = s1;t[1+idx] = s2;
    t[2+idx] = s3;t[3+idx] = s4; t[4+idx] = s5;
  }
  /* backward solve the upper triangular */
  for (i=n-1; i>=0; i--){
    v    = aa + 25*diag[i] + 25;
    vi   = aj + diag[i] + 1;
    nz   = ai[i+1] - diag[i] - 1;
    idt  = 5*i;
    s1 = t[idt];  s2 = t[1+idt]; 
    s3 = t[2+idt];s4 = t[3+idt]; s5 = t[4+idt];
    while (nz--) {
      idx   = 5*(*vi++);
      x1    = t[idx];   x2 = t[1+idx];
      x3    = t[2+idx]; x4 = t[3+idx]; x5 = t[4+idx];
      s1 -= v[0]*x1 + v[5]*x2 + v[10]*x3 + v[15]*x4 + v[20]*x5;
      s2 -= v[1]*x1 + v[6]*x2 + v[11]*x3 + v[16]*x4 + v[21]*x5; 
      s3 -= v[2]*x1 + v[7]*x2 + v[12]*x3 + v[17]*x4 + v[22]*x5;
      s4 -= v[3]*x1 + v[8]*x2 + v[13]*x3 + v[18]*x4 + v[23]*x5;
      s5 -= v[4]*x1 + v[9]*x2 + v[14]*x3 + v[19]*x4 + v[24]*x5;
      v += 25;
    }
    idc = 5*(*c--);
    v   = aa + 25*diag[i];
    x[idc]   = t[idt]   = v[0]*s1+v[5]*s2+v[10]*s3+
                                 v[15]*s4+v[20]*s5;
    x[1+idc] = t[1+idt] = v[1]*s1+v[6]*s2+v[11]*s3+
                                 v[16]*s4+v[21]*s5;
    x[2+idc] = t[2+idt] = v[2]*s1+v[7]*s2+v[12]*s3+
                                 v[17]*s4+v[22]*s5;
    x[3+idc] = t[3+idt] = v[3]*s1+v[8]*s2+v[13]*s3+
                                 v[18]*s4+v[23]*s5;
    x[4+idc] = t[4+idt] = v[4]*s1+v[9]*s2+v[14]*s3+
                                 v[19]*s4+v[24]*s5;
  }

  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*25*(a->nz) - 5.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqBAIJ_5_newdatastruct"
PetscErrorCode MatSolve_SeqBAIJ_5_newdatastruct(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ       *a=(Mat_SeqBAIJ *)A->data;
  IS                iscol=a->col,isrow=a->row;
  PetscErrorCode    ierr;
  const PetscInt    *r,*c,*rout,*cout;
  PetscInt          i,n=a->mbs,*vi,*ai=a->i,*aj=a->j,*adiag=a->diag,nz,idx,idt,idc,m;
  const MatScalar   *aa=a->a,*v;
  PetscScalar       *x,s1,s2,s3,s4,s5,x1,x2,x3,x4,x5,*t;
  const PetscScalar *b;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  t  = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout;

  /* forward solve the lower triangular */
  idx    = 5*r[0]; 
  t[0] = b[idx];   t[1] = b[1+idx]; 
  t[2] = b[2+idx]; t[3] = b[3+idx]; t[4] = b[4+idx];
  for (i=1; i<n; i++) {
    v     = aa + 25*ai[i];
    vi    = aj + ai[i];
    nz    = ai[i+1] - ai[i];
    idx   = 5*r[i]; 
    s1  = b[idx];s2 = b[1+idx];s3 = b[2+idx];s4 = b[3+idx];
    s5  = b[4+idx];
    for(m=0;m<nz;m++){
      idx   = 5*vi[m];
      x1    = t[idx];  x2 = t[1+idx];x3 = t[2+idx];
      x4    = t[3+idx];x5 = t[4+idx];
      s1 -= v[0]*x1 + v[5]*x2 + v[10]*x3 + v[15]*x4 + v[20]*x5;
      s2 -= v[1]*x1 + v[6]*x2 + v[11]*x3 + v[16]*x4 + v[21]*x5;
      s3 -= v[2]*x1 + v[7]*x2 + v[12]*x3 + v[17]*x4 + v[22]*x5;
      s4 -= v[3]*x1 + v[8]*x2 + v[13]*x3 + v[18]*x4 + v[23]*x5;
      s5 -= v[4]*x1 + v[9]*x2 + v[14]*x3 + v[19]*x4 + v[24]*x5;
      v += 25;
    }
    idx = 5*i;
    t[idx]   = s1;t[1+idx] = s2;
    t[2+idx] = s3;t[3+idx] = s4; t[4+idx] = s5;
  }
  /* backward solve the upper triangular */
  for (i=n-1; i>=0; i--){
    v    = aa + 25*(adiag[i+1]+1);
    vi   = aj + adiag[i+1]+1;
    nz   = adiag[i] - adiag[i+1] - 1;
    idt  = 5*i;
    s1 = t[idt];  s2 = t[1+idt]; 
    s3 = t[2+idt];s4 = t[3+idt]; s5 = t[4+idt];
    for(m=0;m<nz;m++){
      idx   = 5*vi[m];
      x1    = t[idx];   x2 = t[1+idx];
      x3    = t[2+idx]; x4 = t[3+idx]; x5 = t[4+idx];
      s1 -= v[0]*x1 + v[5]*x2 + v[10]*x3 + v[15]*x4 + v[20]*x5;
      s2 -= v[1]*x1 + v[6]*x2 + v[11]*x3 + v[16]*x4 + v[21]*x5; 
      s3 -= v[2]*x1 + v[7]*x2 + v[12]*x3 + v[17]*x4 + v[22]*x5;
      s4 -= v[3]*x1 + v[8]*x2 + v[13]*x3 + v[18]*x4 + v[23]*x5;
      s5 -= v[4]*x1 + v[9]*x2 + v[14]*x3 + v[19]*x4 + v[24]*x5;
      v += 25;
    }
    idc = 5*c[i];
    x[idc]   = t[idt]   = v[0]*s1+v[5]*s2+v[10]*s3+
                                 v[15]*s4+v[20]*s5;
    x[1+idc] = t[1+idt] = v[1]*s1+v[6]*s2+v[11]*s3+
                                 v[16]*s4+v[21]*s5;
    x[2+idc] = t[2+idt] = v[2]*s1+v[7]*s2+v[12]*s3+
                                 v[17]*s4+v[22]*s5;
    x[3+idc] = t[3+idt] = v[3]*s1+v[8]*s2+v[13]*s3+
                                 v[18]*s4+v[23]*s5;
    x[4+idc] = t[4+idt] = v[4]*s1+v[9]*s2+v[14]*s3+
                                 v[19]*s4+v[24]*s5;
  }

  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*25*(a->nz) - 5.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqBAIJ_5_NaturalOrdering"
PetscErrorCode MatSolve_SeqBAIJ_5_NaturalOrdering(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ       *a = (Mat_SeqBAIJ *)A->data;
  PetscInt          i,n=a->mbs,*vi,*ai=a->i,*aj=a->j,nz,idx,idt;
  PetscErrorCode    ierr;
  PetscInt          *diag = a->diag,jdx;
  const MatScalar   *aa=a->a,*v;
  PetscScalar       *x,s1,s2,s3,s4,s5,x1,x2,x3,x4,x5;
  const PetscScalar *b;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,(PetscScalar**)&b);CHKERRQ(ierr); 
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr); 
  /* forward solve the lower triangular */
  idx    = 0;
  x[0] = b[idx]; x[1] = b[1+idx]; x[2] = b[2+idx]; x[3] = b[3+idx];x[4] = b[4+idx];
  for (i=1; i<n; i++) {
    v     =  aa + 25*ai[i];
    vi    =  aj + ai[i];
    nz    =  diag[i] - ai[i];
    idx   =  5*i;
    s1  =  b[idx];s2 = b[1+idx];s3 = b[2+idx];s4 = b[3+idx];s5 = b[4+idx];
    while (nz--) {
      jdx   = 5*(*vi++);
      x1    = x[jdx];x2 = x[1+jdx];x3 = x[2+jdx];x4 = x[3+jdx];x5 = x[4+jdx];
      s1 -= v[0]*x1 + v[5]*x2 + v[10]*x3  + v[15]*x4 + v[20]*x5;
      s2 -= v[1]*x1 + v[6]*x2 + v[11]*x3  + v[16]*x4 + v[21]*x5;
      s3 -= v[2]*x1 + v[7]*x2 + v[12]*x3  + v[17]*x4 + v[22]*x5;
      s4 -= v[3]*x1 + v[8]*x2 + v[13]*x3  + v[18]*x4 + v[23]*x5;
      s5 -= v[4]*x1 + v[9]*x2 + v[14]*x3  + v[19]*x4 + v[24]*x5;
      v    += 25;
    }
    x[idx]   = s1;
    x[1+idx] = s2;
    x[2+idx] = s3;
    x[3+idx] = s4;
    x[4+idx] = s5;
  }
  /* backward solve the upper triangular */
  for (i=n-1; i>=0; i--){
    v    = aa + 25*diag[i] + 25;
    vi   = aj + diag[i] + 1;
    nz   = ai[i+1] - diag[i] - 1;
    idt  = 5*i;
    s1 = x[idt];  s2 = x[1+idt]; 
    s3 = x[2+idt];s4 = x[3+idt]; s5 = x[4+idt];
    while (nz--) {
      idx   = 5*(*vi++);
      x1    = x[idx];   x2 = x[1+idx];x3    = x[2+idx]; x4 = x[3+idx]; x5 = x[4+idx];
      s1 -= v[0]*x1 + v[5]*x2 + v[10]*x3  + v[15]*x4 + v[20]*x5;
      s2 -= v[1]*x1 + v[6]*x2 + v[11]*x3  + v[16]*x4 + v[21]*x5;
      s3 -= v[2]*x1 + v[7]*x2 + v[12]*x3  + v[17]*x4 + v[22]*x5;
      s4 -= v[3]*x1 + v[8]*x2 + v[13]*x3  + v[18]*x4 + v[23]*x5;
      s5 -= v[4]*x1 + v[9]*x2 + v[14]*x3  + v[19]*x4 + v[24]*x5;
      v    += 25;
    }
    v        = aa + 25*diag[i];
    x[idt]   = v[0]*s1 + v[5]*s2 + v[10]*s3  + v[15]*s4 + v[20]*s5;
    x[1+idt] = v[1]*s1 + v[6]*s2 + v[11]*s3  + v[16]*s4 + v[21]*s5;
    x[2+idt] = v[2]*s1 + v[7]*s2 + v[12]*s3  + v[17]*s4 + v[22]*s5;
    x[3+idt] = v[3]*s1 + v[8]*s2 + v[13]*s3  + v[18]*s4 + v[23]*s5;
    x[4+idt] = v[4]*s1 + v[9]*s2 + v[14]*s3  + v[19]*s4 + v[24]*s5;
  }

  ierr = VecRestoreArray(bb,(PetscScalar**)&b);CHKERRQ(ierr); 
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr); 
  ierr = PetscLogFlops(2.0*25*(a->nz) - 5.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqBAIJ_5_NaturalOrdering_newdatastruct"
PetscErrorCode MatSolve_SeqBAIJ_5_NaturalOrdering_newdatastruct(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ       *a = (Mat_SeqBAIJ *)A->data;
  PetscInt          i,k,n=a->mbs,*vi,*ai=a->i,*aj=a->j,*adiag=a->diag,nz,idx,idt;
  PetscErrorCode    ierr;
  PetscInt          jdx;
  const MatScalar   *aa=a->a,*v;
  PetscScalar       *x,s1,s2,s3,s4,s5,x1,x2,x3,x4,x5;
  const PetscScalar *b;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,(PetscScalar**)&b);CHKERRQ(ierr); 
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr); 
  /* forward solve the lower triangular */
  idx    = 0;
  x[0] = b[idx]; x[1] = b[1+idx]; x[2] = b[2+idx]; x[3] = b[3+idx];x[4] = b[4+idx];
  for (i=1; i<n; i++) {
    v   = aa + 25*ai[i];
    vi  = aj + ai[i];
    nz  = ai[i+1] - ai[i];
    idx = 5*i;
    s1  = b[idx];s2 = b[1+idx];s3 = b[2+idx];s4 = b[3+idx];s5 = b[4+idx];
    for(k=0;k<nz;k++) {
      jdx   = 5*vi[k];
      x1    = x[jdx];x2 = x[1+jdx];x3 = x[2+jdx];x4 = x[3+jdx];x5 = x[4+jdx];
      s1 -= v[0]*x1 + v[5]*x2 + v[10]*x3  + v[15]*x4 + v[20]*x5;
      s2 -= v[1]*x1 + v[6]*x2 + v[11]*x3  + v[16]*x4 + v[21]*x5;
      s3 -= v[2]*x1 + v[7]*x2 + v[12]*x3  + v[17]*x4 + v[22]*x5;
      s4 -= v[3]*x1 + v[8]*x2 + v[13]*x3  + v[18]*x4 + v[23]*x5;
      s5 -= v[4]*x1 + v[9]*x2 + v[14]*x3  + v[19]*x4 + v[24]*x5;
      v    += 25;
    }
    x[idx]   = s1;
    x[1+idx] = s2;
    x[2+idx] = s3;
    x[3+idx] = s4;
    x[4+idx] = s5;
  }

  /* backward solve the upper triangular */
  for (i=n-1; i>=0; i--){
    v   = aa + 25*(adiag[i+1]+1);
    vi  = aj + adiag[i+1]+1;
    nz  = adiag[i] - adiag[i+1]-1;
    idt = 5*i;
    s1 = x[idt];  s2 = x[1+idt];
    s3 = x[2+idt];s4 = x[3+idt]; s5 = x[4+idt];
    for(k=0;k<nz;k++){
      idx   = 5*vi[k];
      x1    = x[idx];   x2 = x[1+idx];x3    = x[2+idx]; x4 = x[3+idx]; x5 = x[4+idx];
      s1 -= v[0]*x1 + v[5]*x2 + v[10]*x3  + v[15]*x4 + v[20]*x5;
      s2 -= v[1]*x1 + v[6]*x2 + v[11]*x3  + v[16]*x4 + v[21]*x5;
      s3 -= v[2]*x1 + v[7]*x2 + v[12]*x3  + v[17]*x4 + v[22]*x5;
      s4 -= v[3]*x1 + v[8]*x2 + v[13]*x3  + v[18]*x4 + v[23]*x5;
      s5 -= v[4]*x1 + v[9]*x2 + v[14]*x3  + v[19]*x4 + v[24]*x5;
      v    += 25;
    }
    /* x = inv_diagonal*x */
    x[idt]   = v[0]*s1 + v[5]*s2 + v[10]*s3  + v[15]*s4 + v[20]*s5;
    x[1+idt] = v[1]*s1 + v[6]*s2 + v[11]*s3  + v[16]*s4 + v[21]*s5;
    x[2+idt] = v[2]*s1 + v[7]*s2 + v[12]*s3  + v[17]*s4 + v[22]*s5;
    x[3+idt] = v[3]*s1 + v[8]*s2 + v[13]*s3  + v[18]*s4 + v[23]*s5;
    x[4+idt] = v[4]*s1 + v[9]*s2 + v[14]*s3  + v[19]*s4 + v[24]*s5;
  }

  ierr = VecRestoreArray(bb,(PetscScalar**)&b);CHKERRQ(ierr); 
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr); 
  ierr = PetscLogFlops(2.0*25*(a->nz) - 5.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqBAIJ_4"
PetscErrorCode MatSolve_SeqBAIJ_4(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ       *a = (Mat_SeqBAIJ *)A->data;
  IS                iscol=a->col,isrow=a->row;
  PetscErrorCode    ierr;
  PetscInt          i,n=a->mbs,*vi,*ai=a->i,*aj=a->j,nz,idx,idt,idc;
  const PetscInt    *r,*c,*diag = a->diag,*rout,*cout;
  const MatScalar   *aa=a->a,*v;
  PetscScalar       *x,s1,s2,s3,s4,x1,x2,x3,x4,*t;
  const PetscScalar *b;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  t  = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout + (n-1);

  /* forward solve the lower triangular */
  idx    = 4*(*r++); 
  t[0] = b[idx];   t[1] = b[1+idx]; 
  t[2] = b[2+idx]; t[3] = b[3+idx];
  for (i=1; i<n; i++) {
    v     = aa + 16*ai[i];
    vi    = aj + ai[i];
    nz    = diag[i] - ai[i];
    idx   = 4*(*r++); 
    s1  = b[idx];s2 = b[1+idx];s3 = b[2+idx];s4 = b[3+idx];
    while (nz--) {
      idx   = 4*(*vi++);
      x1    = t[idx];x2 = t[1+idx];x3 = t[2+idx];x4 = t[3+idx];
      s1 -= v[0]*x1 + v[4]*x2 + v[8]*x3  + v[12]*x4;
      s2 -= v[1]*x1 + v[5]*x2 + v[9]*x3  + v[13]*x4;
      s3 -= v[2]*x1 + v[6]*x2 + v[10]*x3 + v[14]*x4;
      s4 -= v[3]*x1 + v[7]*x2 + v[11]*x3 + v[15]*x4;
      v    += 16;
    }
    idx        = 4*i;
    t[idx]   = s1;t[1+idx] = s2;
    t[2+idx] = s3;t[3+idx] = s4;
  }
  /* backward solve the upper triangular */
  for (i=n-1; i>=0; i--){
    v    = aa + 16*diag[i] + 16;
    vi   = aj + diag[i] + 1;
    nz   = ai[i+1] - diag[i] - 1;
    idt  = 4*i;
    s1 = t[idt];  s2 = t[1+idt]; 
    s3 = t[2+idt];s4 = t[3+idt];
    while (nz--) {
      idx   = 4*(*vi++);
      x1    = t[idx];   x2 = t[1+idx];
      x3    = t[2+idx]; x4 = t[3+idx];
      s1 -= v[0]*x1 + v[4]*x2 + v[8]*x3   + v[12]*x4;
      s2 -= v[1]*x1 + v[5]*x2 + v[9]*x3   + v[13]*x4; 
      s3 -= v[2]*x1 + v[6]*x2 + v[10]*x3  + v[14]*x4;
      s4 -= v[3]*x1 + v[7]*x2 + v[11]*x3  + v[15]*x4;
      v += 16;
    }
    idc      = 4*(*c--);
    v        = aa + 16*diag[i];
    x[idc]   = t[idt]   = v[0]*s1+v[4]*s2+v[8]*s3+v[12]*s4;
    x[1+idc] = t[1+idt] = v[1]*s1+v[5]*s2+v[9]*s3+v[13]*s4;
    x[2+idc] = t[2+idt] = v[2]*s1+v[6]*s2+v[10]*s3+v[14]*s4;
    x[3+idc] = t[3+idt] = v[3]*s1+v[7]*s2+v[11]*s3+v[15]*s4;
  }

  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*16*(a->nz) - 4.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqBAIJ_4_newdatastruct"
PetscErrorCode MatSolve_SeqBAIJ_4_newdatastruct(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ       *a = (Mat_SeqBAIJ *)A->data;
  IS                iscol=a->col,isrow=a->row;
  PetscErrorCode    ierr;
  PetscInt          i,n=a->mbs,*vi,*ai=a->i,*aj=a->j,*adiag=a->diag,nz,idx,idt,idc,m;
  const PetscInt    *r,*c,*rout,*cout;
  const MatScalar   *aa=a->a,*v;
  PetscScalar       *x,s1,s2,s3,s4,x1,x2,x3,x4,*t;
  const PetscScalar *b;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  t  = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout;

  /* forward solve the lower triangular */
  idx    = 4*r[0]; 
  t[0] = b[idx];   t[1] = b[1+idx]; 
  t[2] = b[2+idx]; t[3] = b[3+idx];
  for (i=1; i<n; i++) {
    v     = aa + 16*ai[i];
    vi    = aj + ai[i];
    nz    = ai[i+1] - ai[i];
    idx   = 4*r[i]; 
    s1  = b[idx];s2 = b[1+idx];s3 = b[2+idx];s4 = b[3+idx];
    for(m=0;m<nz;m++){
      idx   = 4*vi[m];
      x1    = t[idx];x2 = t[1+idx];x3 = t[2+idx];x4 = t[3+idx];
      s1 -= v[0]*x1 + v[4]*x2 + v[8]*x3  + v[12]*x4;
      s2 -= v[1]*x1 + v[5]*x2 + v[9]*x3  + v[13]*x4;
      s3 -= v[2]*x1 + v[6]*x2 + v[10]*x3 + v[14]*x4;
      s4 -= v[3]*x1 + v[7]*x2 + v[11]*x3 + v[15]*x4;
      v    += 16;
    }
    idx        = 4*i;
    t[idx]   = s1;t[1+idx] = s2;
    t[2+idx] = s3;t[3+idx] = s4;
  }
  /* backward solve the upper triangular */
  for (i=n-1; i>=0; i--){	
    v    = aa + 16*(adiag[i+1]+1);
    vi   = aj + adiag[i+1]+1;
    nz   = adiag[i] - adiag[i+1] - 1;
    idt  = 4*i;
    s1 = t[idt];  s2 = t[1+idt]; 
    s3 = t[2+idt];s4 = t[3+idt];
    for(m=0;m<nz;m++){
      idx   = 4*vi[m];
      x1    = t[idx];   x2 = t[1+idx];
      x3    = t[2+idx]; x4 = t[3+idx];
      s1 -= v[0]*x1 + v[4]*x2 + v[8]*x3   + v[12]*x4;
      s2 -= v[1]*x1 + v[5]*x2 + v[9]*x3   + v[13]*x4; 
      s3 -= v[2]*x1 + v[6]*x2 + v[10]*x3  + v[14]*x4;
      s4 -= v[3]*x1 + v[7]*x2 + v[11]*x3  + v[15]*x4;
      v += 16;
    }
    idc      = 4*c[i];
    x[idc]   = t[idt]   = v[0]*s1+v[4]*s2+v[8]*s3+v[12]*s4;
    x[1+idc] = t[1+idt] = v[1]*s1+v[5]*s2+v[9]*s3+v[13]*s4;
    x[2+idc] = t[2+idt] = v[2]*s1+v[6]*s2+v[10]*s3+v[14]*s4;
    x[3+idc] = t[3+idt] = v[3]*s1+v[7]*s2+v[11]*s3+v[15]*s4;
  }

  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*16*(a->nz) - 4.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqBAIJ_4_Demotion"
PetscErrorCode MatSolve_SeqBAIJ_4_Demotion(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ       *a = (Mat_SeqBAIJ *)A->data;
  IS                iscol=a->col,isrow=a->row;
  PetscErrorCode    ierr;
  PetscInt          i,n=a->mbs,*vi,*ai=a->i,*aj=a->j,nz,idx,idt,idc;
  const PetscInt    *r,*c,*diag = a->diag,*rout,*cout;
  const MatScalar   *aa=a->a,*v;
  MatScalar         s1,s2,s3,s4,x1,x2,x3,x4,*t;
  PetscScalar       *x;
  const PetscScalar *b;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  t  = (MatScalar *)a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout + (n-1);

  /* forward solve the lower triangular */
  idx    = 4*(*r++); 
  t[0] = (MatScalar)b[idx];
  t[1] = (MatScalar)b[1+idx]; 
  t[2] = (MatScalar)b[2+idx];
  t[3] = (MatScalar)b[3+idx];
  for (i=1; i<n; i++) {
    v     = aa + 16*ai[i];
    vi    = aj + ai[i];
    nz    = diag[i] - ai[i];
    idx   = 4*(*r++); 
    s1 = (MatScalar)b[idx];
    s2 = (MatScalar)b[1+idx];
    s3 = (MatScalar)b[2+idx];
    s4 = (MatScalar)b[3+idx];
    while (nz--) {
      idx   = 4*(*vi++);
      x1  = t[idx];
      x2  = t[1+idx];
      x3  = t[2+idx];
      x4  = t[3+idx];
      s1 -= v[0]*x1 + v[4]*x2 + v[8]*x3  + v[12]*x4;
      s2 -= v[1]*x1 + v[5]*x2 + v[9]*x3  + v[13]*x4;
      s3 -= v[2]*x1 + v[6]*x2 + v[10]*x3 + v[14]*x4;
      s4 -= v[3]*x1 + v[7]*x2 + v[11]*x3 + v[15]*x4;
      v    += 16;
    }
    idx        = 4*i;
    t[idx]   = s1;
    t[1+idx] = s2;
    t[2+idx] = s3;
    t[3+idx] = s4;
  }
  /* backward solve the upper triangular */
  for (i=n-1; i>=0; i--){
    v    = aa + 16*diag[i] + 16;
    vi   = aj + diag[i] + 1;
    nz   = ai[i+1] - diag[i] - 1;
    idt  = 4*i;
    s1 = t[idt];
    s2 = t[1+idt]; 
    s3 = t[2+idt];
    s4 = t[3+idt];
    while (nz--) {
      idx   = 4*(*vi++);
      x1  = t[idx];
      x2  = t[1+idx];
      x3  = t[2+idx];
      x4  = t[3+idx];
      s1 -= v[0]*x1 + v[4]*x2 + v[8]*x3   + v[12]*x4;
      s2 -= v[1]*x1 + v[5]*x2 + v[9]*x3   + v[13]*x4; 
      s3 -= v[2]*x1 + v[6]*x2 + v[10]*x3  + v[14]*x4;
      s4 -= v[3]*x1 + v[7]*x2 + v[11]*x3  + v[15]*x4;
      v += 16;
    }
    idc      = 4*(*c--);
    v        = aa + 16*diag[i];
    t[idt]   = v[0]*s1+v[4]*s2+v[8]*s3+v[12]*s4;
    t[1+idt] = v[1]*s1+v[5]*s2+v[9]*s3+v[13]*s4;
    t[2+idt] = v[2]*s1+v[6]*s2+v[10]*s3+v[14]*s4;
    t[3+idt] = v[3]*s1+v[7]*s2+v[11]*s3+v[15]*s4;
    x[idc]   = (PetscScalar)t[idt];
    x[1+idc] = (PetscScalar)t[1+idt];
    x[2+idc] = (PetscScalar)t[2+idt];
    x[3+idc] = (PetscScalar)t[3+idt];
 }

  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*16*(a->nz) - 4.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#if defined (PETSC_HAVE_SSE)

#include PETSC_HAVE_SSE

#undef __FUNCT__
#define __FUNCT__ "MatSolve_SeqBAIJ_4_SSE_Demotion"
PetscErrorCode MatSolve_SeqBAIJ_4_SSE_Demotion(Mat A,Vec bb,Vec xx)
{
  /* 
     Note: This code uses demotion of double
     to float when performing the mixed-mode computation.
     This may not be numerically reasonable for all applications.
  */
  Mat_SeqBAIJ    *a = (Mat_SeqBAIJ *)A->data;
  IS             iscol=a->col,isrow=a->row;
  PetscErrorCode ierr;
  PetscInt       i,n=a->mbs,*vi,*ai=a->i,*aj=a->j,nz,idx,idt,idc,ai16;
  const PetscInt *r,*c,*diag = a->diag,*rout,*cout;
  MatScalar      *aa=a->a,*v;
  PetscScalar    *x,*b,*t;

  /* Make space in temp stack for 16 Byte Aligned arrays */
  float           ssealignedspace[11],*tmps,*tmpx;
  unsigned long   offset;
      
  PetscFunctionBegin;
  SSE_SCOPE_BEGIN;

    offset = (unsigned long)ssealignedspace % 16;
    if (offset) offset = (16 - offset)/4;
    tmps = &ssealignedspace[offset];
    tmpx = &ssealignedspace[offset+4];
    PREFETCH_NTA(aa+16*ai[1]);

    ierr = VecGetArray(bb,&b);CHKERRQ(ierr); 
    ierr = VecGetArray(xx,&x);CHKERRQ(ierr); 
    t  = a->solve_work;

    ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
    ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout + (n-1);

    /* forward solve the lower triangular */
    idx  = 4*(*r++);
    t[0] = b[idx];   t[1] = b[1+idx];
    t[2] = b[2+idx]; t[3] = b[3+idx];
    v    =  aa + 16*ai[1];

    for (i=1; i<n;) {
      PREFETCH_NTA(&v[8]);
      vi   =  aj      + ai[i];
      nz   =  diag[i] - ai[i];
      idx  =  4*(*r++);

      /* Demote sum from double to float */
      CONVERT_DOUBLE4_FLOAT4(tmps,&b[idx]);
      LOAD_PS(tmps,XMM7);

      while (nz--) {
        PREFETCH_NTA(&v[16]);
        idx = 4*(*vi++);
        
        /* Demote solution (so far) from double to float */
        CONVERT_DOUBLE4_FLOAT4(tmpx,&x[idx]);

        /* 4x4 Matrix-Vector product with negative accumulation: */
        SSE_INLINE_BEGIN_2(tmpx,v)
          SSE_LOAD_PS(SSE_ARG_1,FLOAT_0,XMM6)

          /* First Column */
          SSE_COPY_PS(XMM0,XMM6)
          SSE_SHUFFLE(XMM0,XMM0,0x00)
          SSE_MULT_PS_M(XMM0,SSE_ARG_2,FLOAT_0)
          SSE_SUB_PS(XMM7,XMM0)
          
          /* Second Column */
          SSE_COPY_PS(XMM1,XMM6)
          SSE_SHUFFLE(XMM1,XMM1,0x55)
          SSE_MULT_PS_M(XMM1,SSE_ARG_2,FLOAT_4)
          SSE_SUB_PS(XMM7,XMM1)
          
          SSE_PREFETCH_NTA(SSE_ARG_2,FLOAT_24)
          
          /* Third Column */
          SSE_COPY_PS(XMM2,XMM6)
          SSE_SHUFFLE(XMM2,XMM2,0xAA)
          SSE_MULT_PS_M(XMM2,SSE_ARG_2,FLOAT_8)
          SSE_SUB_PS(XMM7,XMM2)

          /* Fourth Column */
          SSE_COPY_PS(XMM3,XMM6)
          SSE_SHUFFLE(XMM3,XMM3,0xFF)
          SSE_MULT_PS_M(XMM3,SSE_ARG_2,FLOAT_12)
          SSE_SUB_PS(XMM7,XMM3)
        SSE_INLINE_END_2
        
        v  += 16;
      }
      idx = 4*i;
      v   = aa + 16*ai[++i];
      PREFETCH_NTA(v);
      STORE_PS(tmps,XMM7);

      /* Promote result from float to double */
      CONVERT_FLOAT4_DOUBLE4(&t[idx],tmps);
    }
    /* backward solve the upper triangular */
    idt  = 4*(n-1);
    ai16 = 16*diag[n-1];
    v    = aa + ai16 + 16;
    for (i=n-1; i>=0;){
      PREFETCH_NTA(&v[8]);
      vi = aj + diag[i] + 1;
      nz = ai[i+1] - diag[i] - 1;
      
      /* Demote accumulator from double to float */
      CONVERT_DOUBLE4_FLOAT4(tmps,&t[idt]);
      LOAD_PS(tmps,XMM7);

      while (nz--) {
        PREFETCH_NTA(&v[16]);
        idx = 4*(*vi++);

        /* Demote solution (so far) from double to float */
        CONVERT_DOUBLE4_FLOAT4(tmpx,&t[idx]);

        /* 4x4 Matrix-Vector Product with negative accumulation: */
        SSE_INLINE_BEGIN_2(tmpx,v)
          SSE_LOAD_PS(SSE_ARG_1,FLOAT_0,XMM6)

          /* First Column */
          SSE_COPY_PS(XMM0,XMM6)
          SSE_SHUFFLE(XMM0,XMM0,0x00)
          SSE_MULT_PS_M(XMM0,SSE_ARG_2,FLOAT_0)
          SSE_SUB_PS(XMM7,XMM0)

          /* Second Column */
          SSE_COPY_PS(XMM1,XMM6)
          SSE_SHUFFLE(XMM1,XMM1,0x55)
          SSE_MULT_PS_M(XMM1,SSE_ARG_2,FLOAT_4)
          SSE_SUB_PS(XMM7,XMM1)

          SSE_PREFETCH_NTA(SSE_ARG_2,FLOAT_24)
          
          /* Third Column */
          SSE_COPY_PS(XMM2,XMM6)
          SSE_SHUFFLE(XMM2,XMM2,0xAA)
          SSE_MULT_PS_M(XMM2,SSE_ARG_2,FLOAT_8)
          SSE_SUB_PS(XMM7,XMM2)

          /* Fourth Column */
          SSE_COPY_PS(XMM3,XMM6)
          SSE_SHUFFLE(XMM3,XMM3,0xFF)
          SSE_MULT_PS_M(XMM3,SSE_ARG_2,FLOAT_12)
          SSE_SUB_PS(XMM7,XMM3)
        SSE_INLINE_END_2
        v  += 16;
      }
      v    = aa + ai16;
      ai16 = 16*diag[--i];
      PREFETCH_NTA(aa+ai16+16);
      /* 
         Scale the result by the diagonal 4x4 block, 
         which was inverted as part of the factorization
      */
      SSE_INLINE_BEGIN_3(v,tmps,aa+ai16)
        /* First Column */
        SSE_COPY_PS(XMM0,XMM7)
        SSE_SHUFFLE(XMM0,XMM0,0x00)
        SSE_MULT_PS_M(XMM0,SSE_ARG_1,FLOAT_0)

        /* Second Column */
        SSE_COPY_PS(XMM1,XMM7)
        SSE_SHUFFLE(XMM1,XMM1,0x55)
        SSE_MULT_PS_M(XMM1,SSE_ARG_1,FLOAT_4)
        SSE_ADD_PS(XMM0,XMM1)

        SSE_PREFETCH_NTA(SSE_ARG_3,FLOAT_24)
        
        /* Third Column */
        SSE_COPY_PS(XMM2,XMM7)
        SSE_SHUFFLE(XMM2,XMM2,0xAA)
        SSE_MULT_PS_M(XMM2,SSE_ARG_1,FLOAT_8)
        SSE_ADD_PS(XMM0,XMM2)

        /* Fourth Column */ 
        SSE_COPY_PS(XMM3,XMM7)
        SSE_SHUFFLE(XMM3,XMM3,0xFF)
        SSE_MULT_PS_M(XMM3,SSE_ARG_1,FLOAT_12)
        SSE_ADD_PS(XMM0,XMM3)
          
        SSE_STORE_PS(SSE_ARG_2,FLOAT_0,XMM0)
      SSE_INLINE_END_3

      /* Promote solution from float to double */
      CONVERT_FLOAT4_DOUBLE4(&t[idt],tmps);

      /* Apply reordering to t and stream into x.    */
      /* This way, x doesn't pollute the cache.      */
      /* Be careful with size: 2 doubles = 4 floats! */
      idc  = 4*(*c--);
      SSE_INLINE_BEGIN_2((float *)&t[idt],(float *)&x[idc])
        /*  x[idc]   = t[idt];   x[1+idc] = t[1+idc]; */
        SSE_LOAD_PS(SSE_ARG_1,FLOAT_0,XMM0)
        SSE_STREAM_PS(SSE_ARG_2,FLOAT_0,XMM0)
        /*  x[idc+2] = t[idt+2]; x[3+idc] = t[3+idc]; */
        SSE_LOAD_PS(SSE_ARG_1,FLOAT_4,XMM1)
        SSE_STREAM_PS(SSE_ARG_2,FLOAT_4,XMM1)
      SSE_INLINE_END_2
      v    = aa + ai16 + 16;
      idt -= 4;
    }

    ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
    ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
    ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr); 
    ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr); 
    ierr = PetscLogFlops(2.0*16*(a->nz) - 4.0*A->cmap->n);CHKERRQ(ierr);
  SSE_SCOPE_END;
  PetscFunctionReturn(0);
}

#endif


/*
      Special case where the matrix was ILU(0) factored in the natural
   ordering. This eliminates the need for the column and row permutation.
*/
#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqBAIJ_4_NaturalOrdering"
PetscErrorCode MatSolve_SeqBAIJ_4_NaturalOrdering(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ       *a = (Mat_SeqBAIJ *)A->data;
  PetscInt          n=a->mbs;
  const PetscInt    *ai=a->i,*aj=a->j;
  PetscErrorCode    ierr;
  const PetscInt    *diag = a->diag;
  const MatScalar   *aa=a->a;
  PetscScalar       *x;
  const PetscScalar *b;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,(PetscScalar**)&b);CHKERRQ(ierr); 
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr); 

#if defined(PETSC_USE_FORTRAN_KERNEL_SOLVEBAIJBLAS)
  {
    static PetscScalar w[2000]; /* very BAD need to fix */
    fortransolvebaij4blas_(&n,x,ai,aj,diag,aa,b,w);
  }
#elif defined(PETSC_USE_FORTRAN_KERNEL_SOLVEBAIJ)
  {
    static PetscScalar w[2000]; /* very BAD need to fix */
    fortransolvebaij4_(&n,x,ai,aj,diag,aa,b,w);
  }
#elif defined(PETSC_USE_FORTRAN_KERNEL_SOLVEBAIJUNROLL)
  fortransolvebaij4unroll_(&n,x,ai,aj,diag,aa,b);
#else
  {
    PetscScalar     s1,s2,s3,s4,x1,x2,x3,x4;
    const MatScalar *v;
    PetscInt        jdx,idt,idx,nz,i,ai16;
    const PetscInt  *vi;

  /* forward solve the lower triangular */
  idx    = 0;
  x[0]   = b[0]; x[1] = b[1]; x[2] = b[2]; x[3] = b[3];
  for (i=1; i<n; i++) {
    v     =  aa      + 16*ai[i];
    vi    =  aj      + ai[i];
    nz    =  diag[i] - ai[i];
    idx   +=  4;
    s1  =  b[idx];s2 = b[1+idx];s3 = b[2+idx];s4 = b[3+idx];
    while (nz--) {
      jdx   = 4*(*vi++);
      x1    = x[jdx];x2 = x[1+jdx];x3 = x[2+jdx];x4 = x[3+jdx];
      s1 -= v[0]*x1 + v[4]*x2 + v[8]*x3  + v[12]*x4;
      s2 -= v[1]*x1 + v[5]*x2 + v[9]*x3  + v[13]*x4;
      s3 -= v[2]*x1 + v[6]*x2 + v[10]*x3 + v[14]*x4;
      s4 -= v[3]*x1 + v[7]*x2 + v[11]*x3 + v[15]*x4;
      v    += 16;
    }
    x[idx]   = s1;
    x[1+idx] = s2;
    x[2+idx] = s3;
    x[3+idx] = s4;
  }
  /* backward solve the upper triangular */
  idt = 4*(n-1);
  for (i=n-1; i>=0; i--){
    ai16 = 16*diag[i];
    v    = aa + ai16 + 16;
    vi   = aj + diag[i] + 1;
    nz   = ai[i+1] - diag[i] - 1;
    s1 = x[idt];  s2 = x[1+idt]; 
    s3 = x[2+idt];s4 = x[3+idt];
    while (nz--) {
      idx   = 4*(*vi++);
      x1    = x[idx];   x2 = x[1+idx];x3    = x[2+idx]; x4 = x[3+idx];
      s1 -= v[0]*x1 + v[4]*x2 + v[8]*x3   + v[12]*x4;
      s2 -= v[1]*x1 + v[5]*x2 + v[9]*x3   + v[13]*x4; 
      s3 -= v[2]*x1 + v[6]*x2 + v[10]*x3  + v[14]*x4;
      s4 -= v[3]*x1 + v[7]*x2 + v[11]*x3  + v[15]*x4;
      v    += 16;
    }
    v        = aa + ai16;
    x[idt]   = v[0]*s1 + v[4]*s2 + v[8]*s3  + v[12]*s4;
    x[1+idt] = v[1]*s1 + v[5]*s2 + v[9]*s3  + v[13]*s4;
    x[2+idt] = v[2]*s1 + v[6]*s2 + v[10]*s3 + v[14]*s4;
    x[3+idt] = v[3]*s1 + v[7]*s2 + v[11]*s3 + v[15]*s4;
    idt -= 4;
  }
  }
#endif

  ierr = VecRestoreArray(bb,(PetscScalar**)&b);CHKERRQ(ierr); 
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr); 
  ierr = PetscLogFlops(2.0*16*(a->nz) - 4.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqBAIJ_4_NaturalOrdering_newdatastruct"
PetscErrorCode MatSolve_SeqBAIJ_4_NaturalOrdering_newdatastruct(Mat A,Vec bb,Vec xx)
{
    Mat_SeqBAIJ       *a = (Mat_SeqBAIJ *)A->data;
    PetscInt          i,k,n=a->mbs,*vi,*ai=a->i,*aj=a->j,*adiag=a->diag,nz;
    PetscErrorCode    ierr;
    PetscInt          idx,jdx,idt;
    PetscInt          bs = A->rmap->bs,bs2 = a->bs2;
    const MatScalar   *aa=a->a,*v;
    PetscScalar       *x;
    const PetscScalar *b;
    PetscScalar        s1,s2,s3,s4,x1,x2,x3,x4; 

    PetscFunctionBegin;
    ierr = VecGetArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
    ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
    /* forward solve the lower triangular */
    idx    = 0;
    x[0] = b[idx]; x[1] = b[1+idx];x[2] = b[2+idx];x[3] = b[3+idx];
    for (i=1; i<n; i++) {
       v    = aa + bs2*ai[i];
       vi   = aj + ai[i];
       nz   = ai[i+1] - ai[i];
      idx   = bs*i;
       s1   = b[idx];s2 = b[1+idx];s3 = b[2+idx];s4 = b[3+idx];
      for(k=0;k<nz;k++) {
          jdx   = bs*vi[k];
          x1    = x[jdx];x2 = x[1+jdx]; x3 =x[2+jdx];x4 =x[3+jdx];
          s1   -= v[0]*x1 + v[4]*x2 + v[8]*x3 + v[12]*x4;
          s2   -= v[1]*x1 + v[5]*x2 + v[9]*x3 + v[13]*x4;
          s3   -= v[2]*x1 + v[6]*x2 + v[10]*x3 + v[14]*x4;
	  s4   -= v[3]*x1 + v[7]*x2 + v[11]*x3 + v[15]*x4;
  
          v   +=  bs2;
        }

       x[idx]   = s1;
       x[1+idx] = s2;
       x[2+idx] = s3;
       x[3+idx] = s4;	
    }
 
   /* backward solve the upper triangular */
  for (i=n-1; i>=0; i--){
    v   = aa + bs2*(adiag[i+1]+1);
     vi  = aj + adiag[i+1]+1;
     nz  = adiag[i] - adiag[i+1]-1;
     idt = bs*i;
     s1 = x[idt];  s2 = x[1+idt];s3 = x[2+idt];s4 = x[3+idt];
	
    for(k=0;k<nz;k++){
      idx   = bs*vi[k];
       x1    = x[idx];   x2 = x[1+idx]; x3 = x[2+idx];x4 = x[3+idx];
       s1   -= v[0]*x1 + v[4]*x2 + v[8]*x3 + v[12]*x4;
       s2   -= v[1]*x1 + v[5]*x2 + v[9]*x3 + v[13]*x4;
       s3   -= v[2]*x1 + v[6]*x2 + v[10]*x3 + v[14]*x4;
       s4   -= v[3]*x1 + v[7]*x2 + v[11]*x3 + v[15]*x4;

        v   +=  bs2;
    }
    /* x = inv_diagonal*x */
   x[idt]   = v[0]*s1 + v[4]*s2 + v[8]*s3 + v[12]*s4;
   x[1+idt] = v[1]*s1 + v[5]*s2 + v[9]*s3 + v[13]*s4;;
   x[2+idt] = v[2]*s1 + v[6]*s2 + v[10]*s3 + v[14]*s4;
   x[3+idt] = v[3]*s1 + v[7]*s2 + v[11]*s3 + v[15]*s4;

  } 

  ierr = VecRestoreArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*bs2*(a->nz) - bs*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqBAIJ_4_NaturalOrdering_Demotion"
PetscErrorCode MatSolve_SeqBAIJ_4_NaturalOrdering_Demotion(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a = (Mat_SeqBAIJ *)A->data;
  PetscInt       n=a->mbs,*ai=a->i,*aj=a->j;
  PetscErrorCode ierr;
  PetscInt       *diag = a->diag;
  MatScalar      *aa=a->a;
  PetscScalar    *x,*b;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr); 
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr); 

  {
    MatScalar  s1,s2,s3,s4,x1,x2,x3,x4;
    MatScalar  *v,*t=(MatScalar *)x;
    PetscInt   jdx,idt,idx,nz,*vi,i,ai16;

    /* forward solve the lower triangular */
    idx  = 0;
    t[0] = (MatScalar)b[0];
    t[1] = (MatScalar)b[1];
    t[2] = (MatScalar)b[2];
    t[3] = (MatScalar)b[3];
    for (i=1; i<n; i++) {
      v     =  aa      + 16*ai[i];
      vi    =  aj      + ai[i];
      nz    =  diag[i] - ai[i];
      idx   +=  4;
      s1 = (MatScalar)b[idx];
      s2 = (MatScalar)b[1+idx];
      s3 = (MatScalar)b[2+idx];
      s4 = (MatScalar)b[3+idx];
      while (nz--) {
        jdx = 4*(*vi++);
        x1  = t[jdx];
        x2  = t[1+jdx];
        x3  = t[2+jdx];
        x4  = t[3+jdx];
        s1 -= v[0]*x1 + v[4]*x2 + v[8]*x3  + v[12]*x4;
        s2 -= v[1]*x1 + v[5]*x2 + v[9]*x3  + v[13]*x4;
        s3 -= v[2]*x1 + v[6]*x2 + v[10]*x3 + v[14]*x4;
        s4 -= v[3]*x1 + v[7]*x2 + v[11]*x3 + v[15]*x4;
        v    += 16;
      }
      t[idx]   = s1;
      t[1+idx] = s2;
      t[2+idx] = s3;
      t[3+idx] = s4;
    }
    /* backward solve the upper triangular */
    idt = 4*(n-1);
    for (i=n-1; i>=0; i--){
      ai16 = 16*diag[i];
      v    = aa + ai16 + 16;
      vi   = aj + diag[i] + 1;
      nz   = ai[i+1] - diag[i] - 1;
      s1   = t[idt];
      s2   = t[1+idt]; 
      s3   = t[2+idt];
      s4   = t[3+idt];
      while (nz--) {
        idx = 4*(*vi++);
        x1  = (MatScalar)x[idx];
        x2  = (MatScalar)x[1+idx];
        x3  = (MatScalar)x[2+idx];
        x4  = (MatScalar)x[3+idx];
        s1 -= v[0]*x1 + v[4]*x2 + v[8]*x3  + v[12]*x4;
        s2 -= v[1]*x1 + v[5]*x2 + v[9]*x3  + v[13]*x4; 
        s3 -= v[2]*x1 + v[6]*x2 + v[10]*x3 + v[14]*x4;
        s4 -= v[3]*x1 + v[7]*x2 + v[11]*x3 + v[15]*x4;
        v    += 16;
      }
      v        = aa + ai16;
      x[idt]   = (PetscScalar)(v[0]*s1 + v[4]*s2 + v[8]*s3  + v[12]*s4);
      x[1+idt] = (PetscScalar)(v[1]*s1 + v[5]*s2 + v[9]*s3  + v[13]*s4);
      x[2+idt] = (PetscScalar)(v[2]*s1 + v[6]*s2 + v[10]*s3 + v[14]*s4);
      x[3+idt] = (PetscScalar)(v[3]*s1 + v[7]*s2 + v[11]*s3 + v[15]*s4);
      idt -= 4;
    }
  }

  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr); 
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr); 
  ierr = PetscLogFlops(2.0*16*(a->nz) - 4.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#if defined (PETSC_HAVE_SSE)

#include PETSC_HAVE_SSE
#undef __FUNCT__
#define __FUNCT__ "MatSolve_SeqBAIJ_4_NaturalOrdering_SSE_Demotion_usj"
PetscErrorCode MatSolve_SeqBAIJ_4_NaturalOrdering_SSE_Demotion_usj(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a = (Mat_SeqBAIJ *)A->data;
  unsigned short *aj=(unsigned short *)a->j;
  PetscErrorCode ierr;
  int            *ai=a->i,n=a->mbs,*diag = a->diag;
  MatScalar      *aa=a->a;
  PetscScalar    *x,*b;

  PetscFunctionBegin;
  SSE_SCOPE_BEGIN;
  /* 
     Note: This code currently uses demotion of double
     to float when performing the mixed-mode computation.
     This may not be numerically reasonable for all applications.
  */
  PREFETCH_NTA(aa+16*ai[1]);

  ierr = VecGetArray(bb,&b);CHKERRQ(ierr); 
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr); 
  {
    /* x will first be computed in single precision then promoted inplace to double */
    MatScalar      *v,*t=(MatScalar *)x;
    int            nz,i,idt,ai16;
    unsigned int   jdx,idx;
    unsigned short *vi;
    /* Forward solve the lower triangular factor. */

    /* First block is the identity. */
    idx  = 0;
    CONVERT_DOUBLE4_FLOAT4(t,b);
    v    =  aa + 16*((unsigned int)ai[1]);

    for (i=1; i<n;) {
      PREFETCH_NTA(&v[8]);
      vi   =  aj      + ai[i];
      nz   =  diag[i] - ai[i];
      idx +=  4;

      /* Demote RHS from double to float. */
      CONVERT_DOUBLE4_FLOAT4(&t[idx],&b[idx]);
      LOAD_PS(&t[idx],XMM7);

      while (nz--) {
        PREFETCH_NTA(&v[16]);
        jdx = 4*((unsigned int)(*vi++));
        
        /* 4x4 Matrix-Vector product with negative accumulation: */
        SSE_INLINE_BEGIN_2(&t[jdx],v)
          SSE_LOAD_PS(SSE_ARG_1,FLOAT_0,XMM6)

          /* First Column */
          SSE_COPY_PS(XMM0,XMM6)
          SSE_SHUFFLE(XMM0,XMM0,0x00)
          SSE_MULT_PS_M(XMM0,SSE_ARG_2,FLOAT_0)
          SSE_SUB_PS(XMM7,XMM0)

          /* Second Column */
          SSE_COPY_PS(XMM1,XMM6)
          SSE_SHUFFLE(XMM1,XMM1,0x55)
          SSE_MULT_PS_M(XMM1,SSE_ARG_2,FLOAT_4)
          SSE_SUB_PS(XMM7,XMM1)

          SSE_PREFETCH_NTA(SSE_ARG_2,FLOAT_24)
          
          /* Third Column */
          SSE_COPY_PS(XMM2,XMM6)
          SSE_SHUFFLE(XMM2,XMM2,0xAA)
          SSE_MULT_PS_M(XMM2,SSE_ARG_2,FLOAT_8)
          SSE_SUB_PS(XMM7,XMM2)

          /* Fourth Column */
          SSE_COPY_PS(XMM3,XMM6)
          SSE_SHUFFLE(XMM3,XMM3,0xFF)
          SSE_MULT_PS_M(XMM3,SSE_ARG_2,FLOAT_12)
          SSE_SUB_PS(XMM7,XMM3)
        SSE_INLINE_END_2
        
        v  += 16;
      }
      v    =  aa + 16*ai[++i];
      PREFETCH_NTA(v);
      STORE_PS(&t[idx],XMM7);
    }

    /* Backward solve the upper triangular factor.*/

    idt  = 4*(n-1);
    ai16 = 16*diag[n-1];
    v    = aa + ai16 + 16;
    for (i=n-1; i>=0;){
      PREFETCH_NTA(&v[8]);
      vi = aj + diag[i] + 1;
      nz = ai[i+1] - diag[i] - 1;
      
      LOAD_PS(&t[idt],XMM7);

      while (nz--) {
        PREFETCH_NTA(&v[16]);
        idx = 4*((unsigned int)(*vi++));

        /* 4x4 Matrix-Vector Product with negative accumulation: */
        SSE_INLINE_BEGIN_2(&t[idx],v)
          SSE_LOAD_PS(SSE_ARG_1,FLOAT_0,XMM6)

          /* First Column */
          SSE_COPY_PS(XMM0,XMM6)
          SSE_SHUFFLE(XMM0,XMM0,0x00)
          SSE_MULT_PS_M(XMM0,SSE_ARG_2,FLOAT_0)
          SSE_SUB_PS(XMM7,XMM0)

          /* Second Column */
          SSE_COPY_PS(XMM1,XMM6)
          SSE_SHUFFLE(XMM1,XMM1,0x55)
          SSE_MULT_PS_M(XMM1,SSE_ARG_2,FLOAT_4)
          SSE_SUB_PS(XMM7,XMM1)

          SSE_PREFETCH_NTA(SSE_ARG_2,FLOAT_24)
          
          /* Third Column */
          SSE_COPY_PS(XMM2,XMM6)
          SSE_SHUFFLE(XMM2,XMM2,0xAA)
          SSE_MULT_PS_M(XMM2,SSE_ARG_2,FLOAT_8)
          SSE_SUB_PS(XMM7,XMM2)

          /* Fourth Column */
          SSE_COPY_PS(XMM3,XMM6)
          SSE_SHUFFLE(XMM3,XMM3,0xFF)
          SSE_MULT_PS_M(XMM3,SSE_ARG_2,FLOAT_12)
          SSE_SUB_PS(XMM7,XMM3)
        SSE_INLINE_END_2
        v  += 16;
      }
      v    = aa + ai16;
      ai16 = 16*diag[--i];
      PREFETCH_NTA(aa+ai16+16);
      /* 
         Scale the result by the diagonal 4x4 block, 
         which was inverted as part of the factorization
      */
      SSE_INLINE_BEGIN_3(v,&t[idt],aa+ai16)
        /* First Column */
        SSE_COPY_PS(XMM0,XMM7)
        SSE_SHUFFLE(XMM0,XMM0,0x00)
        SSE_MULT_PS_M(XMM0,SSE_ARG_1,FLOAT_0)

        /* Second Column */
        SSE_COPY_PS(XMM1,XMM7)
        SSE_SHUFFLE(XMM1,XMM1,0x55)
        SSE_MULT_PS_M(XMM1,SSE_ARG_1,FLOAT_4)
        SSE_ADD_PS(XMM0,XMM1)

        SSE_PREFETCH_NTA(SSE_ARG_3,FLOAT_24)
        
        /* Third Column */
        SSE_COPY_PS(XMM2,XMM7)
        SSE_SHUFFLE(XMM2,XMM2,0xAA)
        SSE_MULT_PS_M(XMM2,SSE_ARG_1,FLOAT_8)
        SSE_ADD_PS(XMM0,XMM2)

        /* Fourth Column */ 
        SSE_COPY_PS(XMM3,XMM7)
        SSE_SHUFFLE(XMM3,XMM3,0xFF)
        SSE_MULT_PS_M(XMM3,SSE_ARG_1,FLOAT_12)
        SSE_ADD_PS(XMM0,XMM3)

        SSE_STORE_PS(SSE_ARG_2,FLOAT_0,XMM0)
      SSE_INLINE_END_3

      v    = aa + ai16 + 16;
      idt -= 4;
    }

    /* Convert t from single precision back to double precision (inplace)*/
    idt = 4*(n-1);
    for (i=n-1;i>=0;i--) {
      /*     CONVERT_FLOAT4_DOUBLE4(&x[idt],&t[idt]); */
      /* Unfortunately, CONVERT_ will count from 0 to 3 which doesn't work here. */
      PetscScalar *xtemp=&x[idt];
      MatScalar   *ttemp=&t[idt];
      xtemp[3] = (PetscScalar)ttemp[3];
      xtemp[2] = (PetscScalar)ttemp[2];
      xtemp[1] = (PetscScalar)ttemp[1];
      xtemp[0] = (PetscScalar)ttemp[0];
      idt -= 4;
    }

  } /* End of artificial scope. */
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr); 
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr); 
  ierr = PetscLogFlops(2.0*16*(a->nz) - 4.0*A->cmap->n);CHKERRQ(ierr);
  SSE_SCOPE_END;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "MatSolve_SeqBAIJ_4_NaturalOrdering_SSE_Demotion"
PetscErrorCode MatSolve_SeqBAIJ_4_NaturalOrdering_SSE_Demotion(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a = (Mat_SeqBAIJ *)A->data;
  int            *aj=a->j;
  PetscErrorCode ierr;
  int            *ai=a->i,n=a->mbs,*diag = a->diag;
  MatScalar      *aa=a->a;
  PetscScalar    *x,*b;

  PetscFunctionBegin;
  SSE_SCOPE_BEGIN;
  /* 
     Note: This code currently uses demotion of double
     to float when performing the mixed-mode computation.
     This may not be numerically reasonable for all applications.
  */
  PREFETCH_NTA(aa+16*ai[1]);

  ierr = VecGetArray(bb,&b);CHKERRQ(ierr); 
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr); 
  {
    /* x will first be computed in single precision then promoted inplace to double */
    MatScalar *v,*t=(MatScalar *)x;
    int       nz,i,idt,ai16;
    int       jdx,idx;
    int       *vi;
    /* Forward solve the lower triangular factor. */

    /* First block is the identity. */
    idx  = 0;
    CONVERT_DOUBLE4_FLOAT4(t,b);
    v    =  aa + 16*ai[1];

    for (i=1; i<n;) {
      PREFETCH_NTA(&v[8]);
      vi   =  aj      + ai[i];
      nz   =  diag[i] - ai[i];
      idx +=  4;

      /* Demote RHS from double to float. */
      CONVERT_DOUBLE4_FLOAT4(&t[idx],&b[idx]);
      LOAD_PS(&t[idx],XMM7);

      while (nz--) {
        PREFETCH_NTA(&v[16]);
        jdx = 4*(*vi++);
/*          jdx = *vi++; */
        
        /* 4x4 Matrix-Vector product with negative accumulation: */
        SSE_INLINE_BEGIN_2(&t[jdx],v)
          SSE_LOAD_PS(SSE_ARG_1,FLOAT_0,XMM6)

          /* First Column */
          SSE_COPY_PS(XMM0,XMM6)
          SSE_SHUFFLE(XMM0,XMM0,0x00)
          SSE_MULT_PS_M(XMM0,SSE_ARG_2,FLOAT_0)
          SSE_SUB_PS(XMM7,XMM0)

          /* Second Column */
          SSE_COPY_PS(XMM1,XMM6)
          SSE_SHUFFLE(XMM1,XMM1,0x55)
          SSE_MULT_PS_M(XMM1,SSE_ARG_2,FLOAT_4)
          SSE_SUB_PS(XMM7,XMM1)

          SSE_PREFETCH_NTA(SSE_ARG_2,FLOAT_24)
          
          /* Third Column */
          SSE_COPY_PS(XMM2,XMM6)
          SSE_SHUFFLE(XMM2,XMM2,0xAA)
          SSE_MULT_PS_M(XMM2,SSE_ARG_2,FLOAT_8)
          SSE_SUB_PS(XMM7,XMM2)

          /* Fourth Column */
          SSE_COPY_PS(XMM3,XMM6)
          SSE_SHUFFLE(XMM3,XMM3,0xFF)
          SSE_MULT_PS_M(XMM3,SSE_ARG_2,FLOAT_12)
          SSE_SUB_PS(XMM7,XMM3)
        SSE_INLINE_END_2
        
        v  += 16;
      }
      v    =  aa + 16*ai[++i];
      PREFETCH_NTA(v);
      STORE_PS(&t[idx],XMM7);
    }

    /* Backward solve the upper triangular factor.*/

    idt  = 4*(n-1);
    ai16 = 16*diag[n-1];
    v    = aa + ai16 + 16;
    for (i=n-1; i>=0;){
      PREFETCH_NTA(&v[8]);
      vi = aj + diag[i] + 1;
      nz = ai[i+1] - diag[i] - 1;
      
      LOAD_PS(&t[idt],XMM7);

      while (nz--) {
        PREFETCH_NTA(&v[16]);
        idx = 4*(*vi++);
/*          idx = *vi++; */

        /* 4x4 Matrix-Vector Product with negative accumulation: */
        SSE_INLINE_BEGIN_2(&t[idx],v)
          SSE_LOAD_PS(SSE_ARG_1,FLOAT_0,XMM6)

          /* First Column */
          SSE_COPY_PS(XMM0,XMM6)
          SSE_SHUFFLE(XMM0,XMM0,0x00)
          SSE_MULT_PS_M(XMM0,SSE_ARG_2,FLOAT_0)
          SSE_SUB_PS(XMM7,XMM0)

          /* Second Column */
          SSE_COPY_PS(XMM1,XMM6)
          SSE_SHUFFLE(XMM1,XMM1,0x55)
          SSE_MULT_PS_M(XMM1,SSE_ARG_2,FLOAT_4)
          SSE_SUB_PS(XMM7,XMM1)

          SSE_PREFETCH_NTA(SSE_ARG_2,FLOAT_24)
          
          /* Third Column */
          SSE_COPY_PS(XMM2,XMM6)
          SSE_SHUFFLE(XMM2,XMM2,0xAA)
          SSE_MULT_PS_M(XMM2,SSE_ARG_2,FLOAT_8)
          SSE_SUB_PS(XMM7,XMM2)

          /* Fourth Column */
          SSE_COPY_PS(XMM3,XMM6)
          SSE_SHUFFLE(XMM3,XMM3,0xFF)
          SSE_MULT_PS_M(XMM3,SSE_ARG_2,FLOAT_12)
          SSE_SUB_PS(XMM7,XMM3)
        SSE_INLINE_END_2
        v  += 16;
      }
      v    = aa + ai16;
      ai16 = 16*diag[--i];
      PREFETCH_NTA(aa+ai16+16);
      /* 
         Scale the result by the diagonal 4x4 block, 
         which was inverted as part of the factorization
      */
      SSE_INLINE_BEGIN_3(v,&t[idt],aa+ai16)
        /* First Column */
        SSE_COPY_PS(XMM0,XMM7)
        SSE_SHUFFLE(XMM0,XMM0,0x00)
        SSE_MULT_PS_M(XMM0,SSE_ARG_1,FLOAT_0)

        /* Second Column */
        SSE_COPY_PS(XMM1,XMM7)
        SSE_SHUFFLE(XMM1,XMM1,0x55)
        SSE_MULT_PS_M(XMM1,SSE_ARG_1,FLOAT_4)
        SSE_ADD_PS(XMM0,XMM1)

        SSE_PREFETCH_NTA(SSE_ARG_3,FLOAT_24)
        
        /* Third Column */
        SSE_COPY_PS(XMM2,XMM7)
        SSE_SHUFFLE(XMM2,XMM2,0xAA)
        SSE_MULT_PS_M(XMM2,SSE_ARG_1,FLOAT_8)
        SSE_ADD_PS(XMM0,XMM2)

        /* Fourth Column */ 
        SSE_COPY_PS(XMM3,XMM7)
        SSE_SHUFFLE(XMM3,XMM3,0xFF)
        SSE_MULT_PS_M(XMM3,SSE_ARG_1,FLOAT_12)
        SSE_ADD_PS(XMM0,XMM3)

        SSE_STORE_PS(SSE_ARG_2,FLOAT_0,XMM0)
      SSE_INLINE_END_3

      v    = aa + ai16 + 16;
      idt -= 4;
    }

    /* Convert t from single precision back to double precision (inplace)*/
    idt = 4*(n-1);
    for (i=n-1;i>=0;i--) {
      /*     CONVERT_FLOAT4_DOUBLE4(&x[idt],&t[idt]); */
      /* Unfortunately, CONVERT_ will count from 0 to 3 which doesn't work here. */
      PetscScalar *xtemp=&x[idt];
      MatScalar   *ttemp=&t[idt];
      xtemp[3] = (PetscScalar)ttemp[3];
      xtemp[2] = (PetscScalar)ttemp[2];
      xtemp[1] = (PetscScalar)ttemp[1];
      xtemp[0] = (PetscScalar)ttemp[0];
      idt -= 4;
    }

  } /* End of artificial scope. */
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr); 
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr); 
  ierr = PetscLogFlops(2.0*16*(a->nz) - 4.0*A->cmap->n);CHKERRQ(ierr);
  SSE_SCOPE_END;
  PetscFunctionReturn(0);
}

#endif

#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqBAIJ_3"
PetscErrorCode MatSolve_SeqBAIJ_3(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ       *a=(Mat_SeqBAIJ *)A->data;
  IS                iscol=a->col,isrow=a->row;
  PetscErrorCode    ierr;
  PetscInt          i,n=a->mbs,*vi,*ai=a->i,*aj=a->j,nz,idx,idt,idc;
  const PetscInt    *r,*c,*diag = a->diag,*rout,*cout;
  const MatScalar   *aa=a->a,*v;
  PetscScalar       *x,s1,s2,s3,x1,x2,x3,*t;
  const PetscScalar *b;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  t  = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout + (n-1);

  /* forward solve the lower triangular */
  idx    = 3*(*r++); 
  t[0] = b[idx]; t[1] = b[1+idx]; t[2] = b[2+idx];
  for (i=1; i<n; i++) {
    v     = aa + 9*ai[i];
    vi    = aj + ai[i];
    nz    = diag[i] - ai[i];
    idx   = 3*(*r++); 
    s1  = b[idx]; s2 = b[1+idx]; s3 = b[2+idx];
    while (nz--) {
      idx   = 3*(*vi++);
      x1    = t[idx]; x2 = t[1+idx]; x3 = t[2+idx];
      s1 -= v[0]*x1 + v[3]*x2 + v[6]*x3;
      s2 -= v[1]*x1 + v[4]*x2 + v[7]*x3;
      s3 -= v[2]*x1 + v[5]*x2 + v[8]*x3;
      v += 9;
    }
    idx = 3*i;
    t[idx] = s1; t[1+idx] = s2; t[2+idx] = s3;
  }
  /* backward solve the upper triangular */
  for (i=n-1; i>=0; i--){
    v    = aa + 9*diag[i] + 9;
    vi   = aj + diag[i] + 1;
    nz   = ai[i+1] - diag[i] - 1;
    idt  = 3*i;
    s1 = t[idt]; s2 = t[1+idt]; s3 = t[2+idt];
    while (nz--) {
      idx   = 3*(*vi++);
      x1    = t[idx]; x2 = t[1+idx]; x3 = t[2+idx];
      s1 -= v[0]*x1 + v[3]*x2 + v[6]*x3;
      s2 -= v[1]*x1 + v[4]*x2 + v[7]*x3;
      s3 -= v[2]*x1 + v[5]*x2 + v[8]*x3;
      v += 9;
    }
    idc = 3*(*c--);
    v   = aa + 9*diag[i];
    x[idc]   = t[idt]   = v[0]*s1 + v[3]*s2 + v[6]*s3;
    x[1+idc] = t[1+idt] = v[1]*s1 + v[4]*s2 + v[7]*s3;
    x[2+idc] = t[2+idt] = v[2]*s1 + v[5]*s2 + v[8]*s3;
  }
  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,(PetscScalar**)&b);CHKERRQ(ierr); 
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr); 
  ierr = PetscLogFlops(2.0*9*(a->nz) - 3.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqBAIJ_3_newdatastruct"
PetscErrorCode MatSolve_SeqBAIJ_3_newdatastruct(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ       *a=(Mat_SeqBAIJ *)A->data;
  IS                iscol=a->col,isrow=a->row;
  PetscErrorCode    ierr;
  PetscInt          i,n=a->mbs,*vi,*ai=a->i,*aj=a->j,*adiag=a->diag,nz,idx,idt,idc,m;
  const PetscInt    *r,*c,*rout,*cout;
  const MatScalar   *aa=a->a,*v;
  PetscScalar       *x,s1,s2,s3,x1,x2,x3,*t;
  const PetscScalar *b;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  t  = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout;

  /* forward solve the lower triangular */
  idx    = 3*r[0]; 
  t[0] = b[idx]; t[1] = b[1+idx]; t[2] = b[2+idx];
  for (i=1; i<n; i++) {
    v     = aa + 9*ai[i];
    vi    = aj + ai[i];
    nz    = ai[i+1] - ai[i];
    idx   = 3*r[i]; 
    s1  = b[idx]; s2 = b[1+idx]; s3 = b[2+idx];
    for(m=0;m<nz;m++){
      idx   = 3*vi[m];
      x1    = t[idx]; x2 = t[1+idx]; x3 = t[2+idx];
      s1 -= v[0]*x1 + v[3]*x2 + v[6]*x3;
      s2 -= v[1]*x1 + v[4]*x2 + v[7]*x3;
      s3 -= v[2]*x1 + v[5]*x2 + v[8]*x3;
      v += 9;
    }
    idx = 3*i;
    t[idx] = s1; t[1+idx] = s2; t[2+idx] = s3;
  }
  /* backward solve the upper triangular */
  for (i=n-1; i>=0; i--){
    v    = aa + 9*(adiag[i+1]+1);
    vi   = aj + adiag[i+1]+1;
    nz   = adiag[i] - adiag[i+1] - 1;
    idt  = 3*i;
    s1 = t[idt]; s2 = t[1+idt]; s3 = t[2+idt];
    for(m=0;m<nz;m++){
      idx   = 3*vi[m];
      x1    = t[idx]; x2 = t[1+idx]; x3 = t[2+idx];
      s1 -= v[0]*x1 + v[3]*x2 + v[6]*x3;
      s2 -= v[1]*x1 + v[4]*x2 + v[7]*x3;
      s3 -= v[2]*x1 + v[5]*x2 + v[8]*x3;
      v += 9;
    }
    idc = 3*c[i];
    x[idc]   = t[idt]   = v[0]*s1 + v[3]*s2 + v[6]*s3;
    x[1+idc] = t[1+idt] = v[1]*s1 + v[4]*s2 + v[7]*s3;
    x[2+idc] = t[2+idt] = v[2]*s1 + v[5]*s2 + v[8]*s3;
  }
  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,(PetscScalar**)&b);CHKERRQ(ierr); 
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr); 
  ierr = PetscLogFlops(2.0*9*(a->nz) - 3.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*
      Special case where the matrix was ILU(0) factored in the natural
   ordering. This eliminates the need for the column and row permutation.
*/
#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqBAIJ_3_NaturalOrdering"
PetscErrorCode MatSolve_SeqBAIJ_3_NaturalOrdering(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ       *a = (Mat_SeqBAIJ *)A->data;
  PetscInt          n=a->mbs,*ai=a->i,*aj=a->j;
  PetscErrorCode    ierr;
  PetscInt          *diag = a->diag;
  const MatScalar   *aa=a->a,*v;
  PetscScalar       *x,s1,s2,s3,x1,x2,x3;
  const PetscScalar *b;
  PetscInt          jdx,idt,idx,nz,*vi,i;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,(PetscScalar**)&b);CHKERRQ(ierr); 
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr); 

  /* forward solve the lower triangular */
  idx    = 0;
  x[0]   = b[0]; x[1] = b[1]; x[2] = b[2];
  for (i=1; i<n; i++) {
    v     =  aa      + 9*ai[i];
    vi    =  aj      + ai[i];
    nz    =  diag[i] - ai[i];
    idx   +=  3;
    s1  =  b[idx];s2 = b[1+idx];s3 = b[2+idx];
    while (nz--) {
      jdx   = 3*(*vi++);
      x1    = x[jdx];x2 = x[1+jdx];x3 = x[2+jdx];
      s1 -= v[0]*x1 + v[3]*x2 + v[6]*x3;
      s2 -= v[1]*x1 + v[4]*x2 + v[7]*x3;
      s3 -= v[2]*x1 + v[5]*x2 + v[8]*x3;
      v    += 9;
    }
    x[idx]   = s1;
    x[1+idx] = s2;
    x[2+idx] = s3;
  }
  /* backward solve the upper triangular */
  for (i=n-1; i>=0; i--){
    v    = aa + 9*diag[i] + 9;
    vi   = aj + diag[i] + 1;
    nz   = ai[i+1] - diag[i] - 1;
    idt  = 3*i;
    s1 = x[idt];  s2 = x[1+idt]; 
    s3 = x[2+idt];
    while (nz--) {
      idx   = 3*(*vi++);
      x1    = x[idx];   x2 = x[1+idx];x3    = x[2+idx];
      s1 -= v[0]*x1 + v[3]*x2 + v[6]*x3;
      s2 -= v[1]*x1 + v[4]*x2 + v[7]*x3;
      s3 -= v[2]*x1 + v[5]*x2 + v[8]*x3;
      v    += 9;
    }
    v        = aa +  9*diag[i];
    x[idt]   = v[0]*s1 + v[3]*s2 + v[6]*s3;
    x[1+idt] = v[1]*s1 + v[4]*s2 + v[7]*s3;
    x[2+idt] = v[2]*s1 + v[5]*s2 + v[8]*s3;
  }

  ierr = VecRestoreArray(bb,(PetscScalar**)&b);CHKERRQ(ierr); 
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr); 
  ierr = PetscLogFlops(2.0*9*(a->nz) - 3.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqBAIJ_3_NaturalOrdering_newdatastruct"
PetscErrorCode MatSolve_SeqBAIJ_3_NaturalOrdering_newdatastruct(Mat A,Vec bb,Vec xx)
{
    Mat_SeqBAIJ       *a = (Mat_SeqBAIJ *)A->data;
    PetscInt          i,k,n=a->mbs,*vi,*ai=a->i,*aj=a->j,*adiag=a->diag,nz;
    PetscErrorCode    ierr;
    PetscInt          idx,jdx,idt;
    PetscInt          bs = A->rmap->bs,bs2 = a->bs2;
    const MatScalar   *aa=a->a,*v;
    PetscScalar       *x;
    const PetscScalar *b;
    PetscScalar        s1,s2,s3,x1,x2,x3; 

    PetscFunctionBegin;
    ierr = VecGetArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
    ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
    /* forward solve the lower triangular */
    idx    = 0;
    x[0] = b[idx]; x[1] = b[1+idx];x[2] = b[2+idx];
    for (i=1; i<n; i++) {
       v    = aa + bs2*ai[i];
       vi   = aj + ai[i];
       nz   = ai[i+1] - ai[i];
      idx   = bs*i;
       s1   = b[idx];s2 = b[1+idx];s3 = b[2+idx];
      for(k=0;k<nz;k++){
         jdx   = bs*vi[k];
          x1    = x[jdx];x2 = x[1+jdx]; x3 =x[2+jdx];
          s1   -= v[0]*x1 + v[3]*x2 + v[6]*x3;
          s2   -= v[1]*x1 + v[4]*x2 + v[7]*x3;
          s3   -= v[2]*x1 + v[5]*x2 + v[8]*x3;
  
          v   +=  bs2;
        }

       x[idx]   = s1;
       x[1+idx] = s2;
       x[2+idx] = s3;
    }
 
   /* backward solve the upper triangular */
  for (i=n-1; i>=0; i--){
    v   = aa + bs2*(adiag[i+1]+1);
     vi  = aj + adiag[i+1]+1;
     nz  = adiag[i] - adiag[i+1]-1;
     idt = bs*i;
     s1 = x[idt];  s2 = x[1+idt];s3 = x[2+idt];
	
     for(k=0;k<nz;k++){
       idx   = bs*vi[k];
       x1    = x[idx];   x2 = x[1+idx]; x3 = x[2+idx];
       s1 -= v[0]*x1 + v[3]*x2 + v[6]*x3;
       s2 -= v[1]*x1 + v[4]*x2 + v[7]*x3;
       s3 -= v[2]*x1 + v[5]*x2 + v[8]*x3;

        v   +=  bs2;
    }
    /* x = inv_diagonal*x */
   x[idt]   = v[0]*s1 + v[3]*s2 + v[6]*s3;
   x[1+idt] = v[1]*s1 + v[4]*s2 + v[7]*s3;
   x[2+idt] = v[2]*s1 + v[5]*s2 + v[8]*s3;

  } 

  ierr = VecRestoreArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*bs2*(a->nz) - bs*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqBAIJ_2"
PetscErrorCode MatSolve_SeqBAIJ_2(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ       *a=(Mat_SeqBAIJ *)A->data;
  IS                iscol=a->col,isrow=a->row;
  PetscErrorCode    ierr;
  PetscInt          i,n=a->mbs,*vi,*ai=a->i,*aj=a->j,nz,idx,idt,idc;
  const PetscInt    *r,*c,*diag = a->diag,*rout,*cout;
  const MatScalar   *aa=a->a,*v;
  PetscScalar       *x,s1,s2,x1,x2,*t;
  const PetscScalar *b;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  t  = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout + (n-1);

  /* forward solve the lower triangular */
  idx    = 2*(*r++); 
  t[0] = b[idx]; t[1] = b[1+idx];
  for (i=1; i<n; i++) {
    v     = aa + 4*ai[i];
    vi    = aj + ai[i];
    nz    = diag[i] - ai[i];
    idx   = 2*(*r++); 
    s1  = b[idx]; s2 = b[1+idx];
    while (nz--) {
      idx   = 2*(*vi++);
      x1    = t[idx]; x2 = t[1+idx];
      s1 -= v[0]*x1 + v[2]*x2;
      s2 -= v[1]*x1 + v[3]*x2;
      v += 4;
    }
    idx = 2*i;
    t[idx] = s1; t[1+idx] = s2;
  }
  /* backward solve the upper triangular */
  for (i=n-1; i>=0; i--){
    v    = aa + 4*diag[i] + 4;
    vi   = aj + diag[i] + 1;
    nz   = ai[i+1] - diag[i] - 1;
    idt  = 2*i;
    s1 = t[idt]; s2 = t[1+idt];
    while (nz--) {
      idx   = 2*(*vi++);
      x1    = t[idx]; x2 = t[1+idx];
      s1 -= v[0]*x1 + v[2]*x2;
      s2 -= v[1]*x1 + v[3]*x2;
      v += 4;
    }
    idc = 2*(*c--);
    v   = aa + 4*diag[i];
    x[idc]   = t[idt]   = v[0]*s1 + v[2]*s2;
    x[1+idc] = t[1+idt] = v[1]*s1 + v[3]*s2;
  }
  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr); 
  ierr = PetscLogFlops(2.0*4*(a->nz) - 2.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqBAIJ_2_newdatastruct"
PetscErrorCode MatSolve_SeqBAIJ_2_newdatastruct(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ       *a=(Mat_SeqBAIJ *)A->data;
  IS                iscol=a->col,isrow=a->row;
  PetscErrorCode    ierr;
  PetscInt          i,n=a->mbs,*vi,*ai=a->i,*aj=a->j,*adiag=a->diag,nz,idx,jdx,idt,idc,m;
  const PetscInt    *r,*c,*rout,*cout;
  const MatScalar   *aa=a->a,*v;
  PetscScalar       *x,s1,s2,x1,x2,*t;
  const PetscScalar *b;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  t  = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout;

  /* forward solve the lower triangular */
  idx    = 2*r[0]; 
  t[0] = b[idx]; t[1] = b[1+idx];
  for (i=1; i<n; i++) {
    v     = aa + 4*ai[i];
    vi    = aj + ai[i];
    nz    = ai[i+1] - ai[i];
    idx   = 2*r[i]; 
    s1  = b[idx]; s2 = b[1+idx];
    for(m=0;m<nz;m++){
      jdx   = 2*vi[m];
      x1    = t[jdx]; x2 = t[1+jdx];
      s1 -= v[0]*x1 + v[2]*x2;
      s2 -= v[1]*x1 + v[3]*x2;
      v += 4;
    }
    idx = 2*i;
    t[idx] = s1; t[1+idx] = s2;
  }
  /* backward solve the upper triangular */
  for (i=n-1; i>=0; i--){
    v    = aa + 4*(adiag[i+1]+1);
    vi   = aj + adiag[i+1]+1;
    nz   = adiag[i] - adiag[i+1] - 1;
    idt  = 2*i;
    s1 = t[idt]; s2 = t[1+idt];
    for(m=0;m<nz;m++){
      idx   = 2*vi[m];
      x1    = t[idx]; x2 = t[1+idx];
      s1 -= v[0]*x1 + v[2]*x2;
      s2 -= v[1]*x1 + v[3]*x2;
      v += 4;
    }
    idc = 2*c[i];
    x[idc]   = t[idt]   = v[0]*s1 + v[2]*s2;
    x[1+idc] = t[1+idt] = v[1]*s1 + v[3]*s2;
  }
  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr); 
  ierr = PetscLogFlops(2.0*4*(a->nz) - 2.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*
      Special case where the matrix was ILU(0) factored in the natural
   ordering. This eliminates the need for the column and row permutation.
*/
#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqBAIJ_2_NaturalOrdering"
PetscErrorCode MatSolve_SeqBAIJ_2_NaturalOrdering(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ       *a = (Mat_SeqBAIJ *)A->data;
  PetscInt          n=a->mbs,*ai=a->i,*aj=a->j;
  PetscErrorCode    ierr;
  PetscInt          *diag = a->diag;
  const MatScalar   *aa=a->a,*v;
  PetscScalar       *x,s1,s2,x1,x2;
  const PetscScalar *b;
  PetscInt          jdx,idt,idx,nz,*vi,i;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,(PetscScalar**)&b);CHKERRQ(ierr); 
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr); 

  /* forward solve the lower triangular */
  idx    = 0;
  x[0]   = b[0]; x[1] = b[1];
  for (i=1; i<n; i++) {
    v     =  aa      + 4*ai[i];
    vi    =  aj      + ai[i];
    nz    =  diag[i] - ai[i];
    idx   +=  2;
    s1  =  b[idx];s2 = b[1+idx];
    while (nz--) {
      jdx   = 2*(*vi++);
      x1    = x[jdx];x2 = x[1+jdx];
      s1 -= v[0]*x1 + v[2]*x2;
      s2 -= v[1]*x1 + v[3]*x2;
      v    += 4;
    }
    x[idx]   = s1;
    x[1+idx] = s2;
  }
  /* backward solve the upper triangular */
  for (i=n-1; i>=0; i--){
    v    = aa + 4*diag[i] + 4;
    vi   = aj + diag[i] + 1;
    nz   = ai[i+1] - diag[i] - 1;
    idt  = 2*i;
    s1 = x[idt];  s2 = x[1+idt]; 
    while (nz--) {
      idx   = 2*(*vi++);
      x1    = x[idx];   x2 = x[1+idx];
      s1 -= v[0]*x1 + v[2]*x2;
      s2 -= v[1]*x1 + v[3]*x2;
      v    += 4;
    }
    v        = aa +  4*diag[i];
    x[idt]   = v[0]*s1 + v[2]*s2;
    x[1+idt] = v[1]*s1 + v[3]*s2;
  }

  ierr = VecRestoreArray(bb,(PetscScalar**)&b);CHKERRQ(ierr); 
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr); 
  ierr = PetscLogFlops(2.0*4*(a->nz) - 2.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqBAIJ_2_NaturalOrdering_newdatastruct"
PetscErrorCode MatSolve_SeqBAIJ_2_NaturalOrdering_newdatastruct(Mat A,Vec bb,Vec xx)
{
    Mat_SeqBAIJ       *a = (Mat_SeqBAIJ *)A->data;
    PetscInt          i,k,n=a->mbs,*vi,*ai=a->i,*aj=a->j,*adiag=a->diag,nz,idx,idt;
    PetscErrorCode    ierr;
    PetscInt          jdx;
    const MatScalar   *aa=a->a,*v;
    PetscScalar       *x,s1,s2,x1,x2;
    const PetscScalar *b;
 
    PetscFunctionBegin;
    ierr = VecGetArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
    ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
    /* forward solve the lower triangular */
    idx    = 0;
    x[0] = b[idx]; x[1] = b[1+idx];
    for (i=1; i<n; i++) {
        v   = aa + 4*ai[i];
       vi   = aj + ai[i];
       nz   = ai[i+1] - ai[i];
       idx  = 2*i;
       s1   = b[idx];s2 = b[1+idx];
      for(k=0;k<nz;k++){
         jdx   = 2*vi[k];
          x1    = x[jdx];x2 = x[1+jdx];
          s1   -= v[0]*x1 + v[2]*x2;
          s2   -= v[1]*x1 + v[3]*x2;
           v   +=  4;
        }
       x[idx]   = s1;
       x[1+idx] = s2;
    }
 
   /* backward solve the upper triangular */
  for (i=n-1; i>=0; i--){
     v   = aa + 4*(adiag[i+1]+1);
     vi  = aj + adiag[i+1]+1;
     nz  = adiag[i] - adiag[i+1]-1;
     idt = 2*i;
     s1 = x[idt];  s2 = x[1+idt];
     for(k=0;k<nz;k++){	     
      idx   = 2*vi[k];
       x1    = x[idx];   x2 = x[1+idx];
       s1 -= v[0]*x1 + v[2]*x2;
       s2 -= v[1]*x1 + v[3]*x2;
         v    += 4;
    }
    /* x = inv_diagonal*x */
   x[idt]   = v[0]*s1 + v[2]*s2;
   x[1+idt] = v[1]*s1 + v[3]*s2;
  } 

  ierr = VecRestoreArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*4*(a->nz) - 2.0*A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqBAIJ_1"
PetscErrorCode MatSolve_SeqBAIJ_1(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ *)A->data;
  IS             iscol=a->col,isrow=a->row;
  PetscErrorCode ierr;
  PetscInt       i,n=a->mbs,*vi,*ai=a->i,*aj=a->j,nz;
  const PetscInt *r,*c,*diag = a->diag,*rout,*cout;
  MatScalar      *aa=a->a,*v;
  PetscScalar    *x,*b,s1,*t;

  PetscFunctionBegin;
  if (!n) PetscFunctionReturn(0);

  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  t  = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout + (n-1);

  /* forward solve the lower triangular */
  t[0] = b[*r++];
  for (i=1; i<n; i++) {
    v     = aa + ai[i];
    vi    = aj + ai[i];
    nz    = diag[i] - ai[i];
    s1  = b[*r++];
    while (nz--) {
      s1 -= (*v++)*t[*vi++];
    }
    t[i] = s1;
  }
  /* backward solve the upper triangular */
  for (i=n-1; i>=0; i--){
    v    = aa + diag[i] + 1;
    vi   = aj + diag[i] + 1;
    nz   = ai[i+1] - diag[i] - 1;
    s1 = t[i];
    while (nz--) {
      s1 -= (*v++)*t[*vi++];
    }
    x[*c--] = t[i] = aa[diag[i]]*s1;
  }

  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2.0*1*(a->nz) - A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
/*
      Special case where the matrix was ILU(0) factored in the natural
   ordering. This eliminates the need for the column and row permutation.
*/
#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqBAIJ_1_NaturalOrdering"
PetscErrorCode MatSolve_SeqBAIJ_1_NaturalOrdering(Mat A,Vec bb,Vec xx)
{
  Mat_SeqBAIJ    *a = (Mat_SeqBAIJ *)A->data;
  PetscInt       n=a->mbs,*ai=a->i,*aj=a->j;
  PetscErrorCode ierr;
  PetscInt       *diag = a->diag;
  MatScalar      *aa=a->a;
  PetscScalar    *x,*b;
  PetscScalar    s1,x1;
  MatScalar      *v;
  PetscInt       jdx,idt,idx,nz,*vi,i;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr); 
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr); 

  /* forward solve the lower triangular */
  idx    = 0;
  x[0]   = b[0];
  for (i=1; i<n; i++) {
    v     =  aa      + ai[i];
    vi    =  aj      + ai[i];
    nz    =  diag[i] - ai[i];
    idx   +=  1;
    s1  =  b[idx];
    while (nz--) {
      jdx   = *vi++;
      x1    = x[jdx];
      s1 -= v[0]*x1;
      v    += 1;
    }
    x[idx]   = s1;
  }
  /* backward solve the upper triangular */
  for (i=n-1; i>=0; i--){
    v    = aa + diag[i] + 1;
    vi   = aj + diag[i] + 1;
    nz   = ai[i+1] - diag[i] - 1;
    idt  = i;
    s1 = x[idt]; 
    while (nz--) {
      idx   = *vi++;
      x1    = x[idx];
      s1 -= v[0]*x1;
      v    += 1;
    }
    v        = aa +  diag[i];
    x[idt]   = v[0]*s1;
  }
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr); 
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr); 
  ierr = PetscLogFlops(2.0*(a->nz) - A->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* ----------------------------------------------------------------*/
EXTERN PetscErrorCode MatDuplicateNoCreate_SeqBAIJ(Mat,Mat,MatDuplicateOption,PetscTruth);
EXTERN PetscErrorCode MatSeqBAIJSetNumericFactorization(Mat,PetscTruth);
EXTERN PetscErrorCode MatSeqBAIJSetNumericFactorization_newdatastruct(Mat,PetscTruth);

#undef __FUNCT__  
#define __FUNCT__ "MatLUFactorNumeric_SeqBAIJ_N_newdatastruct"
PetscErrorCode MatLUFactorNumeric_SeqBAIJ_N_newdatastruct(Mat B,Mat A,const MatFactorInfo *info)
{
  Mat            C=B;
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ*)A->data,*b=(Mat_SeqBAIJ *)C->data;
  IS             isrow = b->row,isicol = b->icol;
  PetscErrorCode ierr;
  const PetscInt *r,*ic,*ics;
  PetscInt       i,j,k,n=a->mbs,*ai=a->i,*aj=a->j,*bi=b->i,*bj=b->j;
  PetscInt       *ajtmp,*bjtmp,nz,nzL,row,*bdiag=b->diag,*pj;
  MatScalar      *rtmp,*pc,*mwork,*v,*pv,*aa=a->a;
  PetscInt       bs=A->rmap->bs,bs2 = a->bs2,*v_pivots,flg;
  MatScalar      *v_work;
  PetscTruth     col_identity,row_identity,both_identity;

  PetscFunctionBegin;
  ierr = ISGetIndices(isrow,&r);CHKERRQ(ierr);
  ierr = ISGetIndices(isicol,&ic);CHKERRQ(ierr);
 
  ierr = PetscMalloc(bs2*n*sizeof(MatScalar),&rtmp);CHKERRQ(ierr);
  ierr = PetscMemzero(rtmp,bs2*n*sizeof(MatScalar));CHKERRQ(ierr);
  ics  = ic;

  /* generate work space needed by dense LU factorization */
  ierr  = PetscMalloc3(bs,MatScalar,&v_work,bs2,MatScalar,&mwork,bs,PetscInt,&v_pivots);CHKERRQ(ierr);

  for (i=0; i<n; i++){
    /* zero rtmp */
    /* L part */
    nz    = bi[i+1] - bi[i];
    bjtmp = bj + bi[i];
    for  (j=0; j<nz; j++){
      ierr = PetscMemzero(rtmp+bs2*bjtmp[j],bs2*sizeof(MatScalar));CHKERRQ(ierr);
    }

    /* U part */
    nz = bdiag[i] - bdiag[i+1]; 
    bjtmp = bj + bdiag[i+1]+1; 
    for  (j=0; j<nz; j++){
      ierr = PetscMemzero(rtmp+bs2*bjtmp[j],bs2*sizeof(MatScalar));CHKERRQ(ierr);
    }
 
    /* load in initial (unfactored row) */
    nz    = ai[r[i]+1] - ai[r[i]];
    ajtmp = aj + ai[r[i]];
    v     = aa + bs2*ai[r[i]];
    for (j=0; j<nz; j++) {
      ierr = PetscMemcpy(rtmp+bs2*ic[ajtmp[j]],v+bs2*j,bs2*sizeof(MatScalar));CHKERRQ(ierr);
    }

    /* elimination */
    bjtmp = bj + bi[i];
    nzL   = bi[i+1] - bi[i];
    for(k=0;k < nzL;k++) {
      row = bjtmp[k];
      pc = rtmp + bs2*row;
      for (flg=0,j=0; j<bs2; j++) { if (pc[j]!=0.0) { flg = 1; break; }}
      if (flg) {
        pv         = b->a + bs2*bdiag[row];      
        Kernel_A_gets_A_times_B(bs,pc,pv,mwork); /* *pc = *pc * (*pv); */
        pj         = b->j + bdiag[row+1]+1; /* begining of U(row,:) */
        pv         = b->a + bs2*(bdiag[row+1]+1); 
        nz         = bdiag[row] - bdiag[row+1] - 1; /* num of entries inU(row,:), excluding diag */
        for (j=0; j<nz; j++) {
          Kernel_A_gets_A_minus_B_times_C(bs,rtmp+bs2*pj[j],pc,pv+bs2*j);
        }
        ierr = PetscLogFlops(2*bs2*bs*(nz+1)-bs2);CHKERRQ(ierr); /* flops = 2*bs^3*nz + 2*bs^3 - bs2) */
      }
    }

    /* finished row so stick it into b->a */
    /* L part */
    pv   = b->a + bs2*bi[i] ;
    pj   = b->j + bi[i] ;
    nz   = bi[i+1] - bi[i];
    for (j=0; j<nz; j++) {
      ierr = PetscMemcpy(pv+bs2*j,rtmp+bs2*pj[j],bs2*sizeof(MatScalar));CHKERRQ(ierr);
    }

    /* Mark diagonal and invert diagonal for simplier triangular solves */
    pv  = b->a + bs2*bdiag[i];
    pj  = b->j + bdiag[i];
    /* if (*pj != i)SETERRQ2(PETSC_ERR_SUP,"row %d != *pj %d",i,*pj); */
    ierr = PetscMemcpy(pv,rtmp+bs2*pj[0],bs2*sizeof(MatScalar));CHKERRQ(ierr);   
    ierr = Kernel_A_gets_inverse_A(bs,pv,v_pivots,v_work);CHKERRQ(ierr);
      
    /* U part */
    pv = b->a + bs2*(bdiag[i+1]+1);
    pj = b->j + bdiag[i+1]+1;
    nz = bdiag[i] - bdiag[i+1] - 1; 
    for (j=0; j<nz; j++){
      ierr = PetscMemcpy(pv+bs2*j,rtmp+bs2*pj[j],bs2*sizeof(MatScalar));CHKERRQ(ierr);
    }
  }

  ierr = PetscFree(rtmp);CHKERRQ(ierr);
  ierr = PetscFree3(v_work,mwork,v_pivots);CHKERRQ(ierr);
  ierr = ISRestoreIndices(isicol,&ic);CHKERRQ(ierr);
  ierr = ISRestoreIndices(isrow,&r);CHKERRQ(ierr);

  ierr = ISIdentity(isrow,&row_identity);CHKERRQ(ierr);
  ierr = ISIdentity(isicol,&col_identity);CHKERRQ(ierr);
  both_identity = (PetscTruth) (row_identity && col_identity);
  if (both_identity){
    C->ops->solve = MatSolve_SeqBAIJ_N_NaturalOrdering_newdatastruct;
  } else {
    C->ops->solve = MatSolve_SeqBAIJ_N_newdatastruct;
  }
  C->ops->solvetranspose = MatSolveTranspose_SeqBAIJ_N_newdatastruct;
 
  C->assembled = PETSC_TRUE;
  ierr = PetscLogFlops(1.3333*bs*bs2*b->mbs);CHKERRQ(ierr); /* from inverting diagonal blocks */
  PetscFunctionReturn(0);
}

/* 
   ilu(0) with natural ordering under new data structure.
   See MatILUFactorSymbolic_SeqAIJ_ilu0_newdatastruct() for detailed description
   because this code is almost identical to MatILUFactorSymbolic_SeqAIJ_ilu0_newdatastruct().
*/

#undef __FUNCT__  
#define __FUNCT__ "MatILUFactorSymbolic_SeqBAIJ_ilu0_newdatastruct"
PetscErrorCode MatILUFactorSymbolic_SeqBAIJ_ilu0_newdatastruct(Mat fact,Mat A,IS isrow,IS iscol,const MatFactorInfo *info)
{
  
  Mat_SeqBAIJ        *a = (Mat_SeqBAIJ*)A->data,*b;
  PetscErrorCode     ierr;
  PetscInt           n=a->mbs,*ai=a->i,*aj,*adiag=a->diag,bs2 = a->bs2;
  PetscInt           i,j,nz,*bi,*bj,*bdiag,bi_temp;

  PetscFunctionBegin; 
  ierr = MatDuplicateNoCreate_SeqBAIJ(fact,A,MAT_DO_NOT_COPY_VALUES,PETSC_FALSE);CHKERRQ(ierr);
  b    = (Mat_SeqBAIJ*)(fact)->data;
 
  /* allocate matrix arrays for new data structure */
  ierr = PetscMalloc3(bs2*ai[n]+1,PetscScalar,&b->a,ai[n]+1,PetscInt,&b->j,n+1,PetscInt,&b->i);CHKERRQ(ierr);
  ierr = PetscLogObjectMemory(fact,ai[n]*(bs2*sizeof(PetscScalar)+sizeof(PetscInt))+(n+1)*sizeof(PetscInt));CHKERRQ(ierr);
  b->singlemalloc = PETSC_TRUE;
  if (!b->diag){
    ierr = PetscMalloc((n+1)*sizeof(PetscInt),&b->diag);CHKERRQ(ierr);
    ierr = PetscLogObjectMemory(fact,(n+1)*sizeof(PetscInt));CHKERRQ(ierr);
  }
  bdiag = b->diag;
 
  if (n > 0) {
    ierr = PetscMemzero(b->a,bs2*ai[n]*sizeof(MatScalar));CHKERRQ(ierr);
  }
  
  /* set bi and bj with new data structure */
  bi = b->i;
  bj = b->j;

  /* L part */
  bi[0] = 0;
  for (i=0; i<n; i++){
    nz = adiag[i] - ai[i];
    bi[i+1] = bi[i] + nz;
    aj = a->j + ai[i];
    for (j=0; j<nz; j++){
      *bj = aj[j]; bj++;
    }
  }
      
  /* U part */
  bi_temp = bi[n];
  bdiag[n] = bi[n]-1;
  for (i=n-1; i>=0; i--){
    nz = ai[i+1] - adiag[i] - 1;
    bi_temp = bi_temp + nz + 1;
    aj = a->j + adiag[i] + 1;
    for (j=0; j<nz; j++){
      *bj = aj[j]; bj++;
    }
    /* diag[i] */
    *bj = i; bj++;
    bdiag[i] = bi_temp - 1;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatILUFactorSymbolic_SeqBAIJ_newdatastruct"
PetscErrorCode MatILUFactorSymbolic_SeqBAIJ_newdatastruct(Mat fact,Mat A,IS isrow,IS iscol,const MatFactorInfo *info)
{
  Mat_SeqBAIJ        *a = (Mat_SeqBAIJ*)A->data,*b;
  IS                 isicol;
  PetscErrorCode     ierr;
  const PetscInt     *r,*ic;
  PetscInt           n=a->mbs,*ai=a->i,*aj=a->j,d; 
  PetscInt           *bi,*cols,nnz,*cols_lvl;
  PetscInt           *bdiag,prow,fm,nzbd,reallocs=0,dcount=0;
  PetscInt           i,levels,diagonal_fill;
  PetscTruth         col_identity,row_identity,both_identity;
  PetscReal          f;
  PetscInt           nlnk,*lnk,*lnk_lvl=PETSC_NULL;
  PetscBT            lnkbt;
  PetscInt           nzi,*bj,**bj_ptr,**bjlvl_ptr; 
  PetscFreeSpaceList free_space=PETSC_NULL,current_space=PETSC_NULL; 
  PetscFreeSpaceList free_space_lvl=PETSC_NULL,current_space_lvl=PETSC_NULL; 
  PetscTruth         missing;
  PetscInt           bs=A->rmap->bs,bs2=a->bs2;

  PetscFunctionBegin;
  if (A->rmap->n != A->cmap->n) SETERRQ2(PETSC_ERR_ARG_WRONG,"Must be square matrix, rows %D columns %D",A->rmap->n,A->cmap->n);
  ierr = MatMissingDiagonal(A,&missing,&d);CHKERRQ(ierr);
  if (missing) SETERRQ1(PETSC_ERR_ARG_WRONGSTATE,"Matrix is missing diagonal entry %D",d);

  f             = info->fill;
  levels        = (PetscInt)info->levels;
  diagonal_fill = (PetscInt)info->diagonal_fill;
  ierr = ISInvertPermutation(iscol,PETSC_DECIDE,&isicol);CHKERRQ(ierr);

  ierr = ISIdentity(isrow,&row_identity);CHKERRQ(ierr);
  ierr = ISIdentity(iscol,&col_identity);CHKERRQ(ierr);
  both_identity = (PetscTruth) (row_identity && col_identity);
  
  if (!levels && both_identity) { 
    /* special case: ilu(0) with natural ordering */
    ierr = MatILUFactorSymbolic_SeqBAIJ_ilu0_newdatastruct(fact,A,isrow,iscol,info);CHKERRQ(ierr);
    ierr = MatSeqBAIJSetNumericFactorization_newdatastruct(fact,both_identity);CHKERRQ(ierr);

    fact->factor = MAT_FACTOR_ILU;
    (fact)->info.factor_mallocs    = 0;
    (fact)->info.fill_ratio_given  = info->fill;
    (fact)->info.fill_ratio_needed = 1.0;
    b                = (Mat_SeqBAIJ*)(fact)->data;
    b->row           = isrow;
    b->col           = iscol;
    b->icol          = isicol;
    ierr             = PetscObjectReference((PetscObject)isrow);CHKERRQ(ierr);
    ierr             = PetscObjectReference((PetscObject)iscol);CHKERRQ(ierr);
    b->pivotinblocks = (info->pivotinblocks) ? PETSC_TRUE : PETSC_FALSE;
    ierr = PetscMalloc((n+1)*bs*sizeof(PetscScalar),&b->solve_work);CHKERRQ(ierr);
    PetscFunctionReturn(0);
  }
 
  ierr = ISGetIndices(isrow,&r);CHKERRQ(ierr);
  ierr = ISGetIndices(isicol,&ic);CHKERRQ(ierr);
 
  /* get new row pointers */
  ierr = PetscMalloc((n+1)*sizeof(PetscInt),&bi);CHKERRQ(ierr);
  bi[0] = 0;
  /* bdiag is location of diagonal in factor */
  ierr = PetscMalloc((n+1)*sizeof(PetscInt),&bdiag);CHKERRQ(ierr);
  bdiag[0]  = 0;

  ierr = PetscMalloc2(n,PetscInt*,&bj_ptr,n,PetscInt*,&bjlvl_ptr);CHKERRQ(ierr); 

  /* create a linked list for storing column indices of the active row */
  nlnk = n + 1;
  ierr = PetscIncompleteLLCreate(n,n,nlnk,lnk,lnk_lvl,lnkbt);CHKERRQ(ierr);
  
  /* initial FreeSpace size is f*(ai[n]+1) */
  ierr = PetscFreeSpaceGet((PetscInt)(f*(ai[n]+1)),&free_space);CHKERRQ(ierr);
  current_space = free_space;
  ierr = PetscFreeSpaceGet((PetscInt)(f*(ai[n]+1)),&free_space_lvl);CHKERRQ(ierr);
  current_space_lvl = free_space_lvl;
 
  for (i=0; i<n; i++) {
    nzi = 0;
    /* copy current row into linked list */
    nnz  = ai[r[i]+1] - ai[r[i]];
    if (!nnz) SETERRQ2(PETSC_ERR_MAT_LU_ZRPVT,"Empty row in matrix: row in original ordering %D in permuted ordering %D",r[i],i);
    cols = aj + ai[r[i]];
    lnk[i] = -1; /* marker to indicate if diagonal exists */
    ierr = PetscIncompleteLLInit(nnz,cols,n,ic,nlnk,lnk,lnk_lvl,lnkbt);CHKERRQ(ierr);
    nzi += nlnk;

    /* make sure diagonal entry is included */
    if (diagonal_fill && lnk[i] == -1) {
      fm = n;
      while (lnk[fm] < i) fm = lnk[fm];
      lnk[i]     = lnk[fm]; /* insert diagonal into linked list */
      lnk[fm]    = i;
      lnk_lvl[i] = 0;
      nzi++; dcount++; 
    }

    /* add pivot rows into the active row */
    nzbd = 0;
    prow = lnk[n];
    while (prow < i) {
      nnz      = bdiag[prow];
      cols     = bj_ptr[prow] + nnz + 1;
      cols_lvl = bjlvl_ptr[prow] + nnz + 1; 
      nnz      = bi[prow+1] - bi[prow] - nnz - 1;
      ierr = PetscILULLAddSorted(nnz,cols,levels,cols_lvl,prow,nlnk,lnk,lnk_lvl,lnkbt,prow);CHKERRQ(ierr);
      nzi += nlnk;
      prow = lnk[prow];
      nzbd++;
    }
    bdiag[i] = nzbd;
    bi[i+1]  = bi[i] + nzi;

    /* if free space is not available, make more free space */
    if (current_space->local_remaining<nzi) {
      nnz = 2*nzi*(n - i); /* estimated and max additional space needed */
      ierr = PetscFreeSpaceGet(nnz,&current_space);CHKERRQ(ierr);
      ierr = PetscFreeSpaceGet(nnz,&current_space_lvl);CHKERRQ(ierr);
      reallocs++;
    }

    /* copy data into free_space and free_space_lvl, then initialize lnk */
    ierr = PetscIncompleteLLClean(n,n,nzi,lnk,lnk_lvl,current_space->array,current_space_lvl->array,lnkbt);CHKERRQ(ierr);
    bj_ptr[i]    = current_space->array;
    bjlvl_ptr[i] = current_space_lvl->array;

    /* make sure the active row i has diagonal entry */
    if (*(bj_ptr[i]+bdiag[i]) != i) {
      SETERRQ1(PETSC_ERR_MAT_LU_ZRPVT,"Row %D has missing diagonal in factored matrix\n\
    try running with -pc_factor_nonzeros_along_diagonal or -pc_factor_diagonal_fill",i);
    }

    current_space->array           += nzi;
    current_space->local_used      += nzi;
    current_space->local_remaining -= nzi;
    current_space_lvl->array           += nzi;
    current_space_lvl->local_used      += nzi;
    current_space_lvl->local_remaining -= nzi;
  } 
  
  ierr = ISRestoreIndices(isrow,&r);CHKERRQ(ierr);
  ierr = ISRestoreIndices(isicol,&ic);CHKERRQ(ierr);

  /* destroy list of free space and other temporary arrays */
  ierr = PetscMalloc((bi[n]+1)*sizeof(PetscInt),&bj);CHKERRQ(ierr);
  
  /* copy free_space into bj and free free_space; set bi, bj, bdiag in new datastructure; */
  ierr = PetscFreeSpaceContiguous_LU_v2(&free_space,bj,n,bi,bdiag);CHKERRQ(ierr); 
  
  ierr = PetscIncompleteLLDestroy(lnk,lnkbt);CHKERRQ(ierr);
  ierr = PetscFreeSpaceDestroy(free_space_lvl);CHKERRQ(ierr); 
  ierr = PetscFree2(bj_ptr,bjlvl_ptr);CHKERRQ(ierr);

#if defined(PETSC_USE_INFO)
  {
    PetscReal af = ((PetscReal)bi[n])/((PetscReal)ai[n]);
    ierr = PetscInfo3(A,"Reallocs %D Fill ratio:given %G needed %G\n",reallocs,f,af);CHKERRQ(ierr);
    ierr = PetscInfo1(A,"Run with -[sub_]pc_factor_fill %G or use \n",af);CHKERRQ(ierr);
    ierr = PetscInfo1(A,"PCFactorSetFill([sub]pc,%G);\n",af);CHKERRQ(ierr);
    ierr = PetscInfo(A,"for best performance.\n");CHKERRQ(ierr);
    if (diagonal_fill) {
      ierr = PetscInfo1(A,"Detected and replaced %D missing diagonals",dcount);CHKERRQ(ierr);
    }
  }
#endif

  /* put together the new matrix */
  ierr = MatSeqBAIJSetPreallocation_SeqBAIJ(fact,bs,MAT_SKIP_ALLOCATION,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscLogObjectParent(fact,isicol);CHKERRQ(ierr);
  b = (Mat_SeqBAIJ*)(fact)->data;
  b->free_a       = PETSC_TRUE;
  b->free_ij      = PETSC_TRUE;
  b->singlemalloc = PETSC_FALSE;
  ierr = PetscMalloc( (bs2*(bdiag[0]+1) )*sizeof(MatScalar),&b->a);CHKERRQ(ierr);
  b->j          = bj;
  b->i          = bi;
  b->diag       = bdiag;
  b->free_diag  = PETSC_TRUE;
  b->ilen       = 0;
  b->imax       = 0;
  b->row        = isrow;
  b->col        = iscol;
  ierr          = PetscObjectReference((PetscObject)isrow);CHKERRQ(ierr);
  ierr          = PetscObjectReference((PetscObject)iscol);CHKERRQ(ierr);
  b->icol       = isicol;
  ierr = PetscMalloc((bs*n+bs)*sizeof(PetscScalar),&b->solve_work);CHKERRQ(ierr);
  /* In b structure:  Free imax, ilen, old a, old j.  
     Allocate bdiag, solve_work, new a, new j */
  ierr = PetscLogObjectMemory(fact,(bdiag[0]+1) * (sizeof(PetscInt)+bs2*sizeof(PetscScalar)));CHKERRQ(ierr);
  b->maxnz = b->nz = bdiag[0]+1;
  fact->info.factor_mallocs    = reallocs;
  fact->info.fill_ratio_given  = f;
  fact->info.fill_ratio_needed = ((PetscReal)(bdiag[0]+1))/((PetscReal)ai[n]);
  ierr = MatSeqBAIJSetNumericFactorization_newdatastruct(fact,both_identity);CHKERRQ(ierr); 
  PetscFunctionReturn(0); 
}


/*
     This code is virtually identical to MatILUFactorSymbolic_SeqAIJ
   except that the data structure of Mat_SeqAIJ is slightly different.
   Not a good example of code reuse.
*/
#undef __FUNCT__  
#define __FUNCT__ "MatILUFactorSymbolic_SeqBAIJ"
PetscErrorCode MatILUFactorSymbolic_SeqBAIJ(Mat fact,Mat A,IS isrow,IS iscol,const MatFactorInfo *info)
{
  Mat_SeqBAIJ    *a = (Mat_SeqBAIJ*)A->data,*b;
  IS             isicol;
  PetscErrorCode ierr;
  const PetscInt *r,*ic,*ai = a->i,*aj = a->j,*xi;
  PetscInt       prow,n = a->mbs,*ainew,*ajnew,jmax,*fill,nz,*im,*ajfill,*flev,*xitmp;
  PetscInt       *dloc,idx,row,m,fm,nzf,nzi,reallocate = 0,dcount = 0;
  PetscInt       incrlev,nnz,i,bs = A->rmap->bs,bs2 = a->bs2,levels,diagonal_fill,dd;
  PetscTruth     col_identity,row_identity,both_identity,flg;
  PetscReal      f;
  PetscTruth     newdatastruct = PETSC_FALSE;

  PetscFunctionBegin;
  ierr = PetscOptionsGetTruth(PETSC_NULL,"-ilu_new",&newdatastruct,PETSC_NULL);CHKERRQ(ierr);
  if (newdatastruct){
    ierr = MatILUFactorSymbolic_SeqBAIJ_newdatastruct(fact,A,isrow,iscol,info);CHKERRQ(ierr);
    PetscFunctionReturn(0);
  }
  
  ierr = MatMissingDiagonal_SeqBAIJ(A,&flg,&dd);CHKERRQ(ierr);
  if (flg) SETERRQ1(PETSC_ERR_ARG_WRONGSTATE,"Matrix A is missing diagonal entry in row %D",dd);
  
  f             = info->fill;
  levels        = (PetscInt)info->levels;
  diagonal_fill = (PetscInt)info->diagonal_fill;
  ierr = ISInvertPermutation(iscol,PETSC_DECIDE,&isicol);CHKERRQ(ierr);

  ierr = ISIdentity(isrow,&row_identity);CHKERRQ(ierr);
  ierr = ISIdentity(iscol,&col_identity);CHKERRQ(ierr);
  both_identity = (PetscTruth) (row_identity && col_identity);

  if (!levels && both_identity) {  /* special case copy the nonzero structure */  
    ierr = MatDuplicateNoCreate_SeqBAIJ(fact,A,MAT_DO_NOT_COPY_VALUES,PETSC_TRUE);CHKERRQ(ierr);
    ierr = MatSeqBAIJSetNumericFactorization(fact,both_identity);CHKERRQ(ierr);

    fact->factor = MAT_FACTOR_ILU;
    b            = (Mat_SeqBAIJ*)fact->data;
    b->row       = isrow;
    b->col       = iscol;
    ierr         = PetscObjectReference((PetscObject)isrow);CHKERRQ(ierr);
    ierr         = PetscObjectReference((PetscObject)iscol);CHKERRQ(ierr);
    b->icol      = isicol;
    b->pivotinblocks = (info->pivotinblocks) ? PETSC_TRUE : PETSC_FALSE;
    ierr         = PetscMalloc((n+1)*bs*sizeof(PetscScalar),&b->solve_work);CHKERRQ(ierr);
    PetscFunctionReturn(0); 
  } 

  /* general case perform the symbolic factorization */
    ierr = ISGetIndices(isrow,&r);CHKERRQ(ierr);
    ierr = ISGetIndices(isicol,&ic);CHKERRQ(ierr);

    /* get new row pointers */
    ierr = PetscMalloc((n+1)*sizeof(PetscInt),&ainew);CHKERRQ(ierr);
    ainew[0] = 0;
    /* don't know how many column pointers are needed so estimate */
    jmax = (PetscInt)(f*ai[n] + 1);
    ierr = PetscMalloc((jmax)*sizeof(PetscInt),&ajnew);CHKERRQ(ierr);
    /* ajfill is level of fill for each fill entry */
    ierr = PetscMalloc((jmax)*sizeof(PetscInt),&ajfill);CHKERRQ(ierr);
    /* fill is a linked list of nonzeros in active row */
    ierr = PetscMalloc((n+1)*sizeof(PetscInt),&fill);CHKERRQ(ierr);
    /* im is level for each filled value */
    ierr = PetscMalloc((n+1)*sizeof(PetscInt),&im);CHKERRQ(ierr);
    /* dloc is location of diagonal in factor */
    ierr = PetscMalloc((n+1)*sizeof(PetscInt),&dloc);CHKERRQ(ierr);
    dloc[0]  = 0;
    for (prow=0; prow<n; prow++) {

      /* copy prow into linked list */
      nzf        = nz  = ai[r[prow]+1] - ai[r[prow]];
      if (!nz) SETERRQ2(PETSC_ERR_MAT_LU_ZRPVT,"Empty row in matrix: row in original ordering %D in permuted ordering %D",r[prow],prow);
      xi         = aj + ai[r[prow]];
      fill[n]    = n;
      fill[prow] = -1; /* marker for diagonal entry */
      while (nz--) {
	fm  = n;
	idx = ic[*xi++];
	do {
	  m  = fm;
	  fm = fill[m];
	} while (fm < idx);
	fill[m]   = idx;
	fill[idx] = fm;
	im[idx]   = 0;
      }

      /* make sure diagonal entry is included */
      if (diagonal_fill && fill[prow] == -1) {
	fm = n;
	while (fill[fm] < prow) fm = fill[fm];
	fill[prow] = fill[fm];  /* insert diagonal into linked list */
	fill[fm]   = prow;
	im[prow]   = 0;
	nzf++;
	dcount++;
      }

      nzi = 0;
      row = fill[n];
      while (row < prow) {
	incrlev = im[row] + 1;
	nz      = dloc[row];
	xi      = ajnew  + ainew[row] + nz + 1;
	flev    = ajfill + ainew[row] + nz + 1;
	nnz     = ainew[row+1] - ainew[row] - nz - 1;
	fm      = row;
	while (nnz-- > 0) {
	  idx = *xi++;
	  if (*flev + incrlev > levels) {
	    flev++;
	    continue;
	  }
	  do {
	    m  = fm;
	    fm = fill[m];
	  } while (fm < idx);
	  if (fm != idx) {
	    im[idx]   = *flev + incrlev;
	    fill[m]   = idx;
	    fill[idx] = fm;
	    fm        = idx;
	    nzf++;
	  } else {
	    if (im[idx] > *flev + incrlev) im[idx] = *flev+incrlev;
	  }
	  flev++;
	}
	row = fill[row];
	nzi++;
      }
      /* copy new filled row into permanent storage */
      ainew[prow+1] = ainew[prow] + nzf;
      if (ainew[prow+1] > jmax) {

	/* estimate how much additional space we will need */
	/* use the strategy suggested by David Hysom <hysom@perch-t.icase.edu> */
	/* just double the memory each time */
	PetscInt maxadd = jmax;
	/* maxadd = (int)(((f*ai[n]+1)*(n-prow+5))/n); */
	if (maxadd < nzf) maxadd = (n-prow)*(nzf+1);
	jmax += maxadd;

	/* allocate a longer ajnew and ajfill */
	ierr = PetscMalloc(jmax*sizeof(PetscInt),&xitmp);CHKERRQ(ierr);
	ierr = PetscMemcpy(xitmp,ajnew,ainew[prow]*sizeof(PetscInt));CHKERRQ(ierr);
	ierr = PetscFree(ajnew);CHKERRQ(ierr);
	ajnew = xitmp;
	ierr = PetscMalloc(jmax*sizeof(PetscInt),&xitmp);CHKERRQ(ierr);
	ierr = PetscMemcpy(xitmp,ajfill,ainew[prow]*sizeof(PetscInt));CHKERRQ(ierr);
	ierr = PetscFree(ajfill);CHKERRQ(ierr);
	ajfill = xitmp;
	reallocate++; /* count how many reallocations are needed */
      }
      xitmp       = ajnew + ainew[prow];
      flev        = ajfill + ainew[prow];
      dloc[prow]  = nzi;
      fm          = fill[n];
      while (nzf--) {
	*xitmp++ = fm;
	*flev++ = im[fm];
	fm      = fill[fm];
      }
      /* make sure row has diagonal entry */
      if (ajnew[ainew[prow]+dloc[prow]] != prow) {
	SETERRQ1(PETSC_ERR_MAT_LU_ZRPVT,"Row %D has missing diagonal in factored matrix\n\
    try running with -pc_factor_nonzeros_along_diagonal or -pc_factor_diagonal_fill",prow);
      }
    }
    ierr = PetscFree(ajfill);CHKERRQ(ierr);
    ierr = ISRestoreIndices(isrow,&r);CHKERRQ(ierr);
    ierr = ISRestoreIndices(isicol,&ic);CHKERRQ(ierr);
    ierr = PetscFree(fill);CHKERRQ(ierr);
    ierr = PetscFree(im);CHKERRQ(ierr);

#if defined(PETSC_USE_INFO)
    {
      PetscReal af = ((PetscReal)ainew[n])/((PetscReal)ai[n]);
      ierr = PetscInfo3(A,"Reallocs %D Fill ratio:given %G needed %G\n",reallocate,f,af);CHKERRQ(ierr);
      ierr = PetscInfo1(A,"Run with -pc_factor_fill %G or use \n",af);CHKERRQ(ierr);
      ierr = PetscInfo1(A,"PCFactorSetFill(pc,%G);\n",af);CHKERRQ(ierr);
      ierr = PetscInfo(A,"for best performance.\n");CHKERRQ(ierr);
      if (diagonal_fill) {
	ierr = PetscInfo1(A,"Detected and replaced %D missing diagonals\n",dcount);CHKERRQ(ierr);
      }
    }
#endif

    /* put together the new matrix */
    ierr = MatSeqBAIJSetPreallocation_SeqBAIJ(fact,bs,MAT_SKIP_ALLOCATION,PETSC_NULL);CHKERRQ(ierr);
    ierr = PetscLogObjectParent(fact,isicol);CHKERRQ(ierr);
    b    = (Mat_SeqBAIJ*)fact->data;
    b->free_a       = PETSC_TRUE;
    b->free_ij      = PETSC_TRUE;
    b->singlemalloc = PETSC_FALSE;
    ierr = PetscMalloc(bs2*ainew[n]*sizeof(MatScalar),&b->a);CHKERRQ(ierr);
    b->j          = ajnew;
    b->i          = ainew;
    for (i=0; i<n; i++) dloc[i] += ainew[i];
    b->diag       = dloc;
    b->free_diag  = PETSC_TRUE;
    b->ilen       = 0;
    b->imax       = 0;
    b->row        = isrow;
    b->col        = iscol;
    b->pivotinblocks = (info->pivotinblocks) ? PETSC_TRUE : PETSC_FALSE;
    ierr          = PetscObjectReference((PetscObject)isrow);CHKERRQ(ierr);
    ierr          = PetscObjectReference((PetscObject)iscol);CHKERRQ(ierr);
    b->icol       = isicol;
    ierr = PetscMalloc((bs*n+bs)*sizeof(PetscScalar),&b->solve_work);CHKERRQ(ierr);
    /* In b structure:  Free imax, ilen, old a, old j.  
       Allocate dloc, solve_work, new a, new j */
    ierr = PetscLogObjectMemory(fact,(ainew[n]-n)*(sizeof(PetscInt))+bs2*ainew[n]*sizeof(PetscScalar));CHKERRQ(ierr);
    b->maxnz          = b->nz = ainew[n];

    fact->info.factor_mallocs    = reallocate;
    fact->info.fill_ratio_given  = f;
    fact->info.fill_ratio_needed = ((PetscReal)ainew[n])/((PetscReal)ai[prow]);

  ierr = MatSeqBAIJSetNumericFactorization(fact,both_identity);CHKERRQ(ierr);
  PetscFunctionReturn(0); 
}

#undef __FUNCT__
#define __FUNCT__ "MatSetUnfactored_SeqBAIJ_4_NaturalOrdering_SSE"
PetscErrorCode MatSetUnfactored_SeqBAIJ_4_NaturalOrdering_SSE(Mat A)
{
  /* Mat_SeqBAIJ *a = (Mat_SeqBAIJ *)A->data; */
  /* int i,*AJ=a->j,nz=a->nz; */
  PetscFunctionBegin;
  /* Undo Column scaling */
/*    while (nz--) { */
/*      AJ[i] = AJ[i]/4; */
/*    } */
  /* This should really invoke a push/pop logic, but we don't have that yet. */
  A->ops->setunfactored = PETSC_NULL;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "MatSetUnfactored_SeqBAIJ_4_NaturalOrdering_SSE_usj"
PetscErrorCode MatSetUnfactored_SeqBAIJ_4_NaturalOrdering_SSE_usj(Mat A)
{
  Mat_SeqBAIJ    *a = (Mat_SeqBAIJ *)A->data;
  PetscInt       *AJ=a->j,nz=a->nz;
  unsigned short *aj=(unsigned short *)AJ;
  PetscFunctionBegin;
  /* Is this really necessary? */
  while (nz--) {
    AJ[nz] = (int)((unsigned int)aj[nz]); /* First extend, then convert to signed. */
  }
  A->ops->setunfactored = PETSC_NULL;
  PetscFunctionReturn(0);
}


