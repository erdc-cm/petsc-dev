/*$Id: maij.c,v 1.19 2001/08/07 03:03:00 balay Exp $*/
/*
    Defines the basic matrix operations for the MAIJ  matrix storage format.
  This format is used for restriction and interpolation operations for 
  multicomponent problems. It interpolates each component the same way
  independently.

     We provide:
         MatMult()
         MatMultTranspose()
         MatMultTransposeAdd()
         MatMultAdd()
          and
         MatCreateMAIJ(Mat,dof,Mat*)

     This single directory handles both the sequential and parallel codes
*/

#include "src/mat/impls/maij/maij.h"
#include "vecimpl.h"

#undef __FUNCT__  
#define __FUNCT__ "MatMAIJGetAIJ" 
int MatMAIJGetAIJ(Mat A,Mat *B)
{
  int         ierr;
  PetscTruth  ismpimaij,isseqmaij;

  PetscFunctionBegin;
  ierr = PetscTypeCompare((PetscObject)A,MATMPIMAIJ,&ismpimaij);CHKERRQ(ierr);  
  ierr = PetscTypeCompare((PetscObject)A,MATSEQMAIJ,&isseqmaij);CHKERRQ(ierr);  
  if (ismpimaij) {
    Mat_MPIMAIJ *b = (Mat_MPIMAIJ*)A->data;

    *B = b->A;
  } else if (isseqmaij) {
    Mat_SeqMAIJ *b = (Mat_SeqMAIJ*)A->data;

    *B = b->AIJ;
  } else {
    *B = A;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMAIJRedimension" 
int MatMAIJRedimension(Mat A,int dof,Mat *B)
{
  int ierr;
  Mat Aij;

  PetscFunctionBegin;
  ierr = MatMAIJGetAIJ(A,&Aij);CHKERRQ(ierr);
  ierr = MatCreateMAIJ(Aij,dof,B);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatDestroy_SeqMAIJ" 
int MatDestroy_SeqMAIJ(Mat A)
{
  int         ierr;
  Mat_SeqMAIJ *b = (Mat_SeqMAIJ*)A->data;

  PetscFunctionBegin;
  if (b->AIJ) {
    ierr = MatDestroy(b->AIJ);CHKERRQ(ierr);
  }
  ierr = PetscFree(b);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatDestroy_MPIMAIJ" 
int MatDestroy_MPIMAIJ(Mat A)
{
  int         ierr;
  Mat_MPIMAIJ *b = (Mat_MPIMAIJ*)A->data;

  PetscFunctionBegin;
  if (b->AIJ) {
    ierr = MatDestroy(b->AIJ);CHKERRQ(ierr);
  }
  if (b->OAIJ) {
    ierr = MatDestroy(b->OAIJ);CHKERRQ(ierr);
  }
  if (b->A) {
    ierr = MatDestroy(b->A);CHKERRQ(ierr);
  }
  if (b->ctx) {
    ierr = VecScatterDestroy(b->ctx);CHKERRQ(ierr);
  }
  if (b->w) {
    ierr = VecDestroy(b->w);CHKERRQ(ierr);
  }
  ierr = PetscFree(b);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*MC
  MATMAIJ - MATMAIJ = "maij" - A matrix type to be used for restriction and interpolation operations for 
  multicomponent problems, interpolating or restricting each component the same way independently.
  The matrix type is based on MATSEQAIJ for sequential matrices, and MATMPIAIJ for distributed matrices.

  Operations provided:
. MatMult
. MatMultTranspose
. MatMultAdd
. MatMultTransposeAdd

  Level: advanced

.seealso: MatCreateSeqDense
M*/

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "MatCreate_MAIJ" 
int MatCreate_MAIJ(Mat A)
{
  int         ierr;
  Mat_MPIMAIJ *b;

  PetscFunctionBegin;
  ierr     = PetscNew(Mat_MPIMAIJ,&b);CHKERRQ(ierr);
  A->data  = (void*)b;
  ierr = PetscMemzero(b,sizeof(Mat_MPIMAIJ));CHKERRQ(ierr);
  ierr = PetscMemzero(A->ops,sizeof(struct _MatOps));CHKERRQ(ierr);
  A->factor           = 0;
  A->mapping          = 0;

  b->AIJ  = 0;
  b->dof  = 0;  
  b->OAIJ = 0;
  b->ctx  = 0;
  b->w    = 0;  
  PetscFunctionReturn(0);
}
EXTERN_C_END

/* --------------------------------------------------------------------------------------*/
#undef __FUNCT__  
#define __FUNCT__ "MatMult_SeqMAIJ_2"
int MatMult_SeqMAIJ_2(Mat A,Vec xx,Vec yy)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,sum1, sum2;
  int           ierr,m = b->AIJ->m,*idx,*ii;
  int           n,i,jrow,j;

  PetscFunctionBegin;
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(yy,&y);CHKERRQ(ierr);
  idx  = a->j;
  v    = a->a;
  ii   = a->i;

  for (i=0; i<m; i++) {
    jrow = ii[i];
    n    = ii[i+1] - jrow;
    sum1  = 0.0;
    sum2  = 0.0;
    for (j=0; j<n; j++) {
      sum1 += v[jrow]*x[2*idx[jrow]];
      sum2 += v[jrow]*x[2*idx[jrow]+1];
      jrow++;
     }
    y[2*i]   = sum1;
    y[2*i+1] = sum2;
  }

  PetscLogFlops(4*a->nz - 2*m);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(yy,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMultTranspose_SeqMAIJ_2"
int MatMultTranspose_SeqMAIJ_2(Mat A,Vec xx,Vec yy)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,alpha1,alpha2,zero = 0.0;
  int           ierr,m = b->AIJ->m,n,i,*idx;

  PetscFunctionBegin; 
  ierr = VecSet(&zero,yy);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(yy,&y);CHKERRQ(ierr);
 
  for (i=0; i<m; i++) {
    idx    = a->j + a->i[i] ;
    v      = a->a + a->i[i] ;
    n      = a->i[i+1] - a->i[i];
    alpha1 = x[2*i];
    alpha2 = x[2*i+1];
    while (n-->0) {y[2*(*idx)] += alpha1*(*v); y[2*(*idx)+1] += alpha2*(*v); idx++; v++;}
  }
  PetscLogFlops(4*a->nz - 2*b->AIJ->n);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(yy,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMultAdd_SeqMAIJ_2"
int MatMultAdd_SeqMAIJ_2(Mat A,Vec xx,Vec yy,Vec zz)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,sum1, sum2;
  int           ierr,m = b->AIJ->m,*idx,*ii;
  int           n,i,jrow,j;

  PetscFunctionBegin;
  if (yy != zz) {ierr = VecCopy(yy,zz);CHKERRQ(ierr);}
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(zz,&y);CHKERRQ(ierr);
  idx  = a->j;
  v    = a->a;
  ii   = a->i;

  for (i=0; i<m; i++) {
    jrow = ii[i];
    n    = ii[i+1] - jrow;
    sum1  = 0.0;
    sum2  = 0.0;
    for (j=0; j<n; j++) {
      sum1 += v[jrow]*x[2*idx[jrow]];
      sum2 += v[jrow]*x[2*idx[jrow]+1];
      jrow++;
     }
    y[2*i]   += sum1;
    y[2*i+1] += sum2;
  }

  PetscLogFlops(4*a->nz - 2*m);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(zz,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
#undef __FUNCT__  
#define __FUNCT__ "MatMultTransposeAdd_SeqMAIJ_2"
int MatMultTransposeAdd_SeqMAIJ_2(Mat A,Vec xx,Vec yy,Vec zz)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,alpha1,alpha2;
  int           ierr,m = b->AIJ->m,n,i,*idx;

  PetscFunctionBegin; 
  if (yy != zz) {ierr = VecCopy(yy,zz);CHKERRQ(ierr);}
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(zz,&y);CHKERRQ(ierr);
 
  for (i=0; i<m; i++) {
    idx   = a->j + a->i[i] ;
    v     = a->a + a->i[i] ;
    n     = a->i[i+1] - a->i[i];
    alpha1 = x[2*i];
    alpha2 = x[2*i+1];
    while (n-->0) {y[2*(*idx)] += alpha1*(*v); y[2*(*idx)+1] += alpha2*(*v); idx++; v++;}
  }
  PetscLogFlops(4*a->nz - 2*b->AIJ->n);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(zz,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
/* --------------------------------------------------------------------------------------*/
#undef __FUNCT__  
#define __FUNCT__ "MatMult_SeqMAIJ_3"
int MatMult_SeqMAIJ_3(Mat A,Vec xx,Vec yy)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,sum1, sum2, sum3;
  int           ierr,m = b->AIJ->m,*idx,*ii;
  int           n,i,jrow,j;

  PetscFunctionBegin;
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(yy,&y);CHKERRQ(ierr);
  idx  = a->j;
  v    = a->a;
  ii   = a->i;

  for (i=0; i<m; i++) {
    jrow = ii[i];
    n    = ii[i+1] - jrow;
    sum1  = 0.0;
    sum2  = 0.0;
    sum3  = 0.0;
    for (j=0; j<n; j++) {
      sum1 += v[jrow]*x[3*idx[jrow]];
      sum2 += v[jrow]*x[3*idx[jrow]+1];
      sum3 += v[jrow]*x[3*idx[jrow]+2];
      jrow++;
     }
    y[3*i]   = sum1;
    y[3*i+1] = sum2;
    y[3*i+2] = sum3;
  }

  PetscLogFlops(6*a->nz - 3*m);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(yy,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMultTranspose_SeqMAIJ_3"
int MatMultTranspose_SeqMAIJ_3(Mat A,Vec xx,Vec yy)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,alpha1,alpha2,alpha3,zero = 0.0;
  int           ierr,m = b->AIJ->m,n,i,*idx;

  PetscFunctionBegin; 
  ierr = VecSet(&zero,yy);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(yy,&y);CHKERRQ(ierr);
  
  for (i=0; i<m; i++) {
    idx    = a->j + a->i[i];
    v      = a->a + a->i[i];
    n      = a->i[i+1] - a->i[i];
    alpha1 = x[3*i];
    alpha2 = x[3*i+1];
    alpha3 = x[3*i+2];
    while (n-->0) {
      y[3*(*idx)]   += alpha1*(*v);
      y[3*(*idx)+1] += alpha2*(*v);
      y[3*(*idx)+2] += alpha3*(*v);
      idx++; v++;
    }
  }
  PetscLogFlops(6*a->nz - 3*b->AIJ->n);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(yy,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMultAdd_SeqMAIJ_3"
int MatMultAdd_SeqMAIJ_3(Mat A,Vec xx,Vec yy,Vec zz)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,sum1, sum2, sum3;
  int           ierr,m = b->AIJ->m,*idx,*ii;
  int           n,i,jrow,j;

  PetscFunctionBegin;
  if (yy != zz) {ierr = VecCopy(yy,zz);CHKERRQ(ierr);}
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(zz,&y);CHKERRQ(ierr);
  idx  = a->j;
  v    = a->a;
  ii   = a->i;

  for (i=0; i<m; i++) {
    jrow = ii[i];
    n    = ii[i+1] - jrow;
    sum1  = 0.0;
    sum2  = 0.0;
    sum3  = 0.0;
    for (j=0; j<n; j++) {
      sum1 += v[jrow]*x[3*idx[jrow]];
      sum2 += v[jrow]*x[3*idx[jrow]+1];
      sum3 += v[jrow]*x[3*idx[jrow]+2];
      jrow++;
     }
    y[3*i]   += sum1;
    y[3*i+1] += sum2;
    y[3*i+2] += sum3;
  }

  PetscLogFlops(6*a->nz);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(zz,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
#undef __FUNCT__  
#define __FUNCT__ "MatMultTransposeAdd_SeqMAIJ_3"
int MatMultTransposeAdd_SeqMAIJ_3(Mat A,Vec xx,Vec yy,Vec zz)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,alpha1,alpha2,alpha3;
  int           ierr,m = b->AIJ->m,n,i,*idx;

  PetscFunctionBegin; 
  if (yy != zz) {ierr = VecCopy(yy,zz);CHKERRQ(ierr);}
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(zz,&y);CHKERRQ(ierr);
  for (i=0; i<m; i++) {
    idx    = a->j + a->i[i] ;
    v      = a->a + a->i[i] ;
    n      = a->i[i+1] - a->i[i];
    alpha1 = x[3*i];
    alpha2 = x[3*i+1];
    alpha3 = x[3*i+2];
    while (n-->0) {
      y[3*(*idx)]   += alpha1*(*v);
      y[3*(*idx)+1] += alpha2*(*v);
      y[3*(*idx)+2] += alpha3*(*v);
      idx++; v++;
    }
  }
  PetscLogFlops(6*a->nz);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(zz,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* ------------------------------------------------------------------------------*/
#undef __FUNCT__  
#define __FUNCT__ "MatMult_SeqMAIJ_4"
int MatMult_SeqMAIJ_4(Mat A,Vec xx,Vec yy)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,sum1, sum2, sum3, sum4;
  int           ierr,m = b->AIJ->m,*idx,*ii;
  int           n,i,jrow,j;

  PetscFunctionBegin;
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(yy,&y);CHKERRQ(ierr);
  idx  = a->j;
  v    = a->a;
  ii   = a->i;

  for (i=0; i<m; i++) {
    jrow = ii[i];
    n    = ii[i+1] - jrow;
    sum1  = 0.0;
    sum2  = 0.0;
    sum3  = 0.0;
    sum4  = 0.0;
    for (j=0; j<n; j++) {
      sum1 += v[jrow]*x[4*idx[jrow]];
      sum2 += v[jrow]*x[4*idx[jrow]+1];
      sum3 += v[jrow]*x[4*idx[jrow]+2];
      sum4 += v[jrow]*x[4*idx[jrow]+3];
      jrow++;
     }
    y[4*i]   = sum1;
    y[4*i+1] = sum2;
    y[4*i+2] = sum3;
    y[4*i+3] = sum4;
  }

  PetscLogFlops(8*a->nz - 4*m);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(yy,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMultTranspose_SeqMAIJ_4"
int MatMultTranspose_SeqMAIJ_4(Mat A,Vec xx,Vec yy)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,alpha1,alpha2,alpha3,alpha4,zero = 0.0;
  int           ierr,m = b->AIJ->m,n,i,*idx;

  PetscFunctionBegin; 
  ierr = VecSet(&zero,yy);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(yy,&y);CHKERRQ(ierr);
  for (i=0; i<m; i++) {
    idx    = a->j + a->i[i] ;
    v      = a->a + a->i[i] ;
    n      = a->i[i+1] - a->i[i];
    alpha1 = x[4*i];
    alpha2 = x[4*i+1];
    alpha3 = x[4*i+2];
    alpha4 = x[4*i+3];
    while (n-->0) {
      y[4*(*idx)]   += alpha1*(*v);
      y[4*(*idx)+1] += alpha2*(*v);
      y[4*(*idx)+2] += alpha3*(*v);
      y[4*(*idx)+3] += alpha4*(*v);
      idx++; v++;
    }
  }
  PetscLogFlops(8*a->nz - 4*b->AIJ->n);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(yy,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMultAdd_SeqMAIJ_4"
int MatMultAdd_SeqMAIJ_4(Mat A,Vec xx,Vec yy,Vec zz)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,sum1, sum2, sum3, sum4;
  int           ierr,m = b->AIJ->m,*idx,*ii;
  int           n,i,jrow,j;

  PetscFunctionBegin;
  if (yy != zz) {ierr = VecCopy(yy,zz);CHKERRQ(ierr);}
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(zz,&y);CHKERRQ(ierr);
  idx  = a->j;
  v    = a->a;
  ii   = a->i;

  for (i=0; i<m; i++) {
    jrow = ii[i];
    n    = ii[i+1] - jrow;
    sum1  = 0.0;
    sum2  = 0.0;
    sum3  = 0.0;
    sum4  = 0.0;
    for (j=0; j<n; j++) {
      sum1 += v[jrow]*x[4*idx[jrow]];
      sum2 += v[jrow]*x[4*idx[jrow]+1];
      sum3 += v[jrow]*x[4*idx[jrow]+2];
      sum4 += v[jrow]*x[4*idx[jrow]+3];
      jrow++;
     }
    y[4*i]   += sum1;
    y[4*i+1] += sum2;
    y[4*i+2] += sum3;
    y[4*i+3] += sum4;
  }

  PetscLogFlops(8*a->nz - 4*m);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(zz,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
#undef __FUNCT__  
#define __FUNCT__ "MatMultTransposeAdd_SeqMAIJ_4"
int MatMultTransposeAdd_SeqMAIJ_4(Mat A,Vec xx,Vec yy,Vec zz)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,alpha1,alpha2,alpha3,alpha4;
  int           ierr,m = b->AIJ->m,n,i,*idx;

  PetscFunctionBegin; 
  if (yy != zz) {ierr = VecCopy(yy,zz);CHKERRQ(ierr);}
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(zz,&y);CHKERRQ(ierr);
  
  for (i=0; i<m; i++) {
    idx    = a->j + a->i[i] ;
    v      = a->a + a->i[i] ;
    n      = a->i[i+1] - a->i[i];
    alpha1 = x[4*i];
    alpha2 = x[4*i+1];
    alpha3 = x[4*i+2];
    alpha4 = x[4*i+3];
    while (n-->0) {
      y[4*(*idx)]   += alpha1*(*v);
      y[4*(*idx)+1] += alpha2*(*v);
      y[4*(*idx)+2] += alpha3*(*v);
      y[4*(*idx)+3] += alpha4*(*v);
      idx++; v++;
    }
  }
  PetscLogFlops(8*a->nz - 4*b->AIJ->n);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(zz,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
/* ------------------------------------------------------------------------------*/

#undef __FUNCT__  
#define __FUNCT__ "MatMult_SeqMAIJ_5"
int MatMult_SeqMAIJ_5(Mat A,Vec xx,Vec yy)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,sum1, sum2, sum3, sum4, sum5;
  int           ierr,m = b->AIJ->m,*idx,*ii;
  int           n,i,jrow,j;

  PetscFunctionBegin;
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(yy,&y);CHKERRQ(ierr);
  idx  = a->j;
  v    = a->a;
  ii   = a->i;

  for (i=0; i<m; i++) {
    jrow = ii[i];
    n    = ii[i+1] - jrow;
    sum1  = 0.0;
    sum2  = 0.0;
    sum3  = 0.0;
    sum4  = 0.0;
    sum5  = 0.0;
    for (j=0; j<n; j++) {
      sum1 += v[jrow]*x[5*idx[jrow]];
      sum2 += v[jrow]*x[5*idx[jrow]+1];
      sum3 += v[jrow]*x[5*idx[jrow]+2];
      sum4 += v[jrow]*x[5*idx[jrow]+3];
      sum5 += v[jrow]*x[5*idx[jrow]+4];
      jrow++;
     }
    y[5*i]   = sum1;
    y[5*i+1] = sum2;
    y[5*i+2] = sum3;
    y[5*i+3] = sum4;
    y[5*i+4] = sum5;
  }

  PetscLogFlops(10*a->nz - 5*m);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(yy,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMultTranspose_SeqMAIJ_5"
int MatMultTranspose_SeqMAIJ_5(Mat A,Vec xx,Vec yy)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,alpha1,alpha2,alpha3,alpha4,alpha5,zero = 0.0;
  int           ierr,m = b->AIJ->m,n,i,*idx;

  PetscFunctionBegin; 
  ierr = VecSet(&zero,yy);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(yy,&y);CHKERRQ(ierr);
  
  for (i=0; i<m; i++) {
    idx    = a->j + a->i[i] ;
    v      = a->a + a->i[i] ;
    n      = a->i[i+1] - a->i[i];
    alpha1 = x[5*i];
    alpha2 = x[5*i+1];
    alpha3 = x[5*i+2];
    alpha4 = x[5*i+3];
    alpha5 = x[5*i+4];
    while (n-->0) {
      y[5*(*idx)]   += alpha1*(*v);
      y[5*(*idx)+1] += alpha2*(*v);
      y[5*(*idx)+2] += alpha3*(*v);
      y[5*(*idx)+3] += alpha4*(*v);
      y[5*(*idx)+4] += alpha5*(*v);
      idx++; v++;
    }
  }
  PetscLogFlops(10*a->nz - 5*b->AIJ->n);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(yy,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMultAdd_SeqMAIJ_5"
int MatMultAdd_SeqMAIJ_5(Mat A,Vec xx,Vec yy,Vec zz)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,sum1, sum2, sum3, sum4, sum5;
  int           ierr,m = b->AIJ->m,*idx,*ii;
  int           n,i,jrow,j;

  PetscFunctionBegin;
  if (yy != zz) {ierr = VecCopy(yy,zz);CHKERRQ(ierr);}
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(zz,&y);CHKERRQ(ierr);
  idx  = a->j;
  v    = a->a;
  ii   = a->i;

  for (i=0; i<m; i++) {
    jrow = ii[i];
    n    = ii[i+1] - jrow;
    sum1  = 0.0;
    sum2  = 0.0;
    sum3  = 0.0;
    sum4  = 0.0;
    sum5  = 0.0;
    for (j=0; j<n; j++) {
      sum1 += v[jrow]*x[5*idx[jrow]];
      sum2 += v[jrow]*x[5*idx[jrow]+1];
      sum3 += v[jrow]*x[5*idx[jrow]+2];
      sum4 += v[jrow]*x[5*idx[jrow]+3];
      sum5 += v[jrow]*x[5*idx[jrow]+4];
      jrow++;
     }
    y[5*i]   += sum1;
    y[5*i+1] += sum2;
    y[5*i+2] += sum3;
    y[5*i+3] += sum4;
    y[5*i+4] += sum5;
  }

  PetscLogFlops(10*a->nz);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(zz,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMultTransposeAdd_SeqMAIJ_5"
int MatMultTransposeAdd_SeqMAIJ_5(Mat A,Vec xx,Vec yy,Vec zz)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,alpha1,alpha2,alpha3,alpha4,alpha5;
  int           ierr,m = b->AIJ->m,n,i,*idx;

  PetscFunctionBegin; 
  if (yy != zz) {ierr = VecCopy(yy,zz);CHKERRQ(ierr);}
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(zz,&y);CHKERRQ(ierr);
 
  for (i=0; i<m; i++) {
    idx    = a->j + a->i[i] ;
    v      = a->a + a->i[i] ;
    n      = a->i[i+1] - a->i[i];
    alpha1 = x[5*i];
    alpha2 = x[5*i+1];
    alpha3 = x[5*i+2];
    alpha4 = x[5*i+3];
    alpha5 = x[5*i+4];
    while (n-->0) {
      y[5*(*idx)]   += alpha1*(*v);
      y[5*(*idx)+1] += alpha2*(*v);
      y[5*(*idx)+2] += alpha3*(*v);
      y[5*(*idx)+3] += alpha4*(*v);
      y[5*(*idx)+4] += alpha5*(*v);
      idx++; v++;
    }
  }
  PetscLogFlops(10*a->nz);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(zz,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* ------------------------------------------------------------------------------*/
#undef __FUNCT__  
#define __FUNCT__ "MatMult_SeqMAIJ_6"
int MatMult_SeqMAIJ_6(Mat A,Vec xx,Vec yy)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,sum1, sum2, sum3, sum4, sum5, sum6;
  int           ierr,m = b->AIJ->m,*idx,*ii;
  int           n,i,jrow,j;

  PetscFunctionBegin;
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(yy,&y);CHKERRQ(ierr);
  idx  = a->j;
  v    = a->a;
  ii   = a->i;

  for (i=0; i<m; i++) {
    jrow = ii[i];
    n    = ii[i+1] - jrow;
    sum1  = 0.0;
    sum2  = 0.0;
    sum3  = 0.0;
    sum4  = 0.0;
    sum5  = 0.0;
    sum6  = 0.0;
    for (j=0; j<n; j++) {
      sum1 += v[jrow]*x[6*idx[jrow]];
      sum2 += v[jrow]*x[6*idx[jrow]+1];
      sum3 += v[jrow]*x[6*idx[jrow]+2];
      sum4 += v[jrow]*x[6*idx[jrow]+3];
      sum5 += v[jrow]*x[6*idx[jrow]+4];
      sum6 += v[jrow]*x[6*idx[jrow]+5];
      jrow++;
     }
    y[6*i]   = sum1;
    y[6*i+1] = sum2;
    y[6*i+2] = sum3;
    y[6*i+3] = sum4;
    y[6*i+4] = sum5;
    y[6*i+5] = sum6;
  }

  PetscLogFlops(12*a->nz - 6*m);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(yy,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMultTranspose_SeqMAIJ_6"
int MatMultTranspose_SeqMAIJ_6(Mat A,Vec xx,Vec yy)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,alpha1,alpha2,alpha3,alpha4,alpha5,alpha6,zero = 0.0;
  int           ierr,m = b->AIJ->m,n,i,*idx;

  PetscFunctionBegin; 
  ierr = VecSet(&zero,yy);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(yy,&y);CHKERRQ(ierr);

  for (i=0; i<m; i++) {
    idx    = a->j + a->i[i] ;
    v      = a->a + a->i[i] ;
    n      = a->i[i+1] - a->i[i];
    alpha1 = x[6*i];
    alpha2 = x[6*i+1];
    alpha3 = x[6*i+2];
    alpha4 = x[6*i+3];
    alpha5 = x[6*i+4];
    alpha6 = x[6*i+5];
    while (n-->0) {
      y[6*(*idx)]   += alpha1*(*v);
      y[6*(*idx)+1] += alpha2*(*v);
      y[6*(*idx)+2] += alpha3*(*v);
      y[6*(*idx)+3] += alpha4*(*v);
      y[6*(*idx)+4] += alpha5*(*v);
      y[6*(*idx)+5] += alpha6*(*v);
      idx++; v++;
    }
  }
  PetscLogFlops(12*a->nz - 6*b->AIJ->n);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(yy,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMultAdd_SeqMAIJ_6"
int MatMultAdd_SeqMAIJ_6(Mat A,Vec xx,Vec yy,Vec zz)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,sum1, sum2, sum3, sum4, sum5, sum6;
  int           ierr,m = b->AIJ->m,*idx,*ii;
  int           n,i,jrow,j;

  PetscFunctionBegin;
  if (yy != zz) {ierr = VecCopy(yy,zz);CHKERRQ(ierr);}
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(zz,&y);CHKERRQ(ierr);
  idx  = a->j;
  v    = a->a;
  ii   = a->i;

  for (i=0; i<m; i++) {
    jrow = ii[i];
    n    = ii[i+1] - jrow;
    sum1  = 0.0;
    sum2  = 0.0;
    sum3  = 0.0;
    sum4  = 0.0;
    sum5  = 0.0;
    sum6  = 0.0;
    for (j=0; j<n; j++) {
      sum1 += v[jrow]*x[6*idx[jrow]];
      sum2 += v[jrow]*x[6*idx[jrow]+1];
      sum3 += v[jrow]*x[6*idx[jrow]+2];
      sum4 += v[jrow]*x[6*idx[jrow]+3];
      sum5 += v[jrow]*x[6*idx[jrow]+4];
      sum6 += v[jrow]*x[6*idx[jrow]+5];
      jrow++;
     }
    y[6*i]   += sum1;
    y[6*i+1] += sum2;
    y[6*i+2] += sum3;
    y[6*i+3] += sum4;
    y[6*i+4] += sum5;
    y[6*i+5] += sum6;
  }

  PetscLogFlops(12*a->nz);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(zz,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMultTransposeAdd_SeqMAIJ_6"
int MatMultTransposeAdd_SeqMAIJ_6(Mat A,Vec xx,Vec yy,Vec zz)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,alpha1,alpha2,alpha3,alpha4,alpha5,alpha6;
  int           ierr,m = b->AIJ->m,n,i,*idx;

  PetscFunctionBegin; 
  if (yy != zz) {ierr = VecCopy(yy,zz);CHKERRQ(ierr);}
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(zz,&y);CHKERRQ(ierr);
 
  for (i=0; i<m; i++) {
    idx    = a->j + a->i[i] ;
    v      = a->a + a->i[i] ;
    n      = a->i[i+1] - a->i[i];
    alpha1 = x[6*i];
    alpha2 = x[6*i+1];
    alpha3 = x[6*i+2];
    alpha4 = x[6*i+3];
    alpha5 = x[6*i+4];
    alpha6 = x[6*i+5];
    while (n-->0) {
      y[6*(*idx)]   += alpha1*(*v);
      y[6*(*idx)+1] += alpha2*(*v);
      y[6*(*idx)+2] += alpha3*(*v);
      y[6*(*idx)+3] += alpha4*(*v);
      y[6*(*idx)+4] += alpha5*(*v);
      y[6*(*idx)+5] += alpha6*(*v);
      idx++; v++;
    }
  }
  PetscLogFlops(12*a->nz);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(zz,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* ------------------------------------------------------------------------------*/
#undef __FUNCT__  
#define __FUNCT__ "MatMult_SeqMAIJ_8"
int MatMult_SeqMAIJ_8(Mat A,Vec xx,Vec yy)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,sum1, sum2, sum3, sum4, sum5, sum6, sum7, sum8;
  int           ierr,m = b->AIJ->m,*idx,*ii;
  int           n,i,jrow,j;

  PetscFunctionBegin;
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(yy,&y);CHKERRQ(ierr);
  idx  = a->j;
  v    = a->a;
  ii   = a->i;

  for (i=0; i<m; i++) {
    jrow = ii[i];
    n    = ii[i+1] - jrow;
    sum1  = 0.0;
    sum2  = 0.0;
    sum3  = 0.0;
    sum4  = 0.0;
    sum5  = 0.0;
    sum6  = 0.0;
    sum7  = 0.0;
    sum8  = 0.0;
    for (j=0; j<n; j++) {
      sum1 += v[jrow]*x[8*idx[jrow]];
      sum2 += v[jrow]*x[8*idx[jrow]+1];
      sum3 += v[jrow]*x[8*idx[jrow]+2];
      sum4 += v[jrow]*x[8*idx[jrow]+3];
      sum5 += v[jrow]*x[8*idx[jrow]+4];
      sum6 += v[jrow]*x[8*idx[jrow]+5];
      sum7 += v[jrow]*x[8*idx[jrow]+6];
      sum8 += v[jrow]*x[8*idx[jrow]+7];
      jrow++;
     }
    y[8*i]   = sum1;
    y[8*i+1] = sum2;
    y[8*i+2] = sum3;
    y[8*i+3] = sum4;
    y[8*i+4] = sum5;
    y[8*i+5] = sum6;
    y[8*i+6] = sum7;
    y[8*i+7] = sum8;
  }

  PetscLogFlops(16*a->nz - 8*m);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(yy,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMultTranspose_SeqMAIJ_8"
int MatMultTranspose_SeqMAIJ_8(Mat A,Vec xx,Vec yy)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,alpha1,alpha2,alpha3,alpha4,alpha5,alpha6,alpha7,alpha8,zero = 0.0;
  int           ierr,m = b->AIJ->m,n,i,*idx;

  PetscFunctionBegin; 
  ierr = VecSet(&zero,yy);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(yy,&y);CHKERRQ(ierr);

  for (i=0; i<m; i++) {
    idx    = a->j + a->i[i] ;
    v      = a->a + a->i[i] ;
    n      = a->i[i+1] - a->i[i];
    alpha1 = x[8*i];
    alpha2 = x[8*i+1];
    alpha3 = x[8*i+2];
    alpha4 = x[8*i+3];
    alpha5 = x[8*i+4];
    alpha6 = x[8*i+5];
    alpha7 = x[8*i+6];
    alpha8 = x[8*i+7];
    while (n-->0) {
      y[8*(*idx)]   += alpha1*(*v);
      y[8*(*idx)+1] += alpha2*(*v);
      y[8*(*idx)+2] += alpha3*(*v);
      y[8*(*idx)+3] += alpha4*(*v);
      y[8*(*idx)+4] += alpha5*(*v);
      y[8*(*idx)+5] += alpha6*(*v);
      y[8*(*idx)+6] += alpha7*(*v);
      y[8*(*idx)+7] += alpha8*(*v);
      idx++; v++;
    }
  }
  PetscLogFlops(16*a->nz - 8*b->AIJ->n);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(yy,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMultAdd_SeqMAIJ_8"
int MatMultAdd_SeqMAIJ_8(Mat A,Vec xx,Vec yy,Vec zz)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,sum1, sum2, sum3, sum4, sum5, sum6, sum7, sum8;
  int           ierr,m = b->AIJ->m,*idx,*ii;
  int           n,i,jrow,j;

  PetscFunctionBegin;
  if (yy != zz) {ierr = VecCopy(yy,zz);CHKERRQ(ierr);}
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(zz,&y);CHKERRQ(ierr);
  idx  = a->j;
  v    = a->a;
  ii   = a->i;

  for (i=0; i<m; i++) {
    jrow = ii[i];
    n    = ii[i+1] - jrow;
    sum1  = 0.0;
    sum2  = 0.0;
    sum3  = 0.0;
    sum4  = 0.0;
    sum5  = 0.0;
    sum6  = 0.0;
    sum7  = 0.0;
    sum8  = 0.0;
    for (j=0; j<n; j++) {
      sum1 += v[jrow]*x[8*idx[jrow]];
      sum2 += v[jrow]*x[8*idx[jrow]+1];
      sum3 += v[jrow]*x[8*idx[jrow]+2];
      sum4 += v[jrow]*x[8*idx[jrow]+3];
      sum5 += v[jrow]*x[8*idx[jrow]+4];
      sum6 += v[jrow]*x[8*idx[jrow]+5];
      sum7 += v[jrow]*x[8*idx[jrow]+6];
      sum8 += v[jrow]*x[8*idx[jrow]+7];
      jrow++;
     }
    y[8*i]   += sum1;
    y[8*i+1] += sum2;
    y[8*i+2] += sum3;
    y[8*i+3] += sum4;
    y[8*i+4] += sum5;
    y[8*i+5] += sum6;
    y[8*i+6] += sum7;
    y[8*i+7] += sum8;
  }

  PetscLogFlops(16*a->nz);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(zz,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMultTransposeAdd_SeqMAIJ_8"
int MatMultTransposeAdd_SeqMAIJ_8(Mat A,Vec xx,Vec yy,Vec zz)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,alpha1,alpha2,alpha3,alpha4,alpha5,alpha6,alpha7,alpha8;
  int           ierr,m = b->AIJ->m,n,i,*idx;

  PetscFunctionBegin; 
  if (yy != zz) {ierr = VecCopy(yy,zz);CHKERRQ(ierr);}
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(zz,&y);CHKERRQ(ierr);
  for (i=0; i<m; i++) {
    idx    = a->j + a->i[i] ;
    v      = a->a + a->i[i] ;
    n      = a->i[i+1] - a->i[i];
    alpha1 = x[8*i];
    alpha2 = x[8*i+1];
    alpha3 = x[8*i+2];
    alpha4 = x[8*i+3];
    alpha5 = x[8*i+4];
    alpha6 = x[8*i+5];
    alpha7 = x[8*i+6];
    alpha8 = x[8*i+7];
    while (n-->0) {
      y[8*(*idx)]   += alpha1*(*v);
      y[8*(*idx)+1] += alpha2*(*v);
      y[8*(*idx)+2] += alpha3*(*v);
      y[8*(*idx)+3] += alpha4*(*v);
      y[8*(*idx)+4] += alpha5*(*v);
      y[8*(*idx)+5] += alpha6*(*v);
      y[8*(*idx)+6] += alpha7*(*v);
      y[8*(*idx)+7] += alpha8*(*v);
      idx++; v++;
    }
  }
  PetscLogFlops(16*a->nz);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(zz,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* ------------------------------------------------------------------------------*/
#undef __FUNCT__  
#define __FUNCT__ "MatMult_SeqMAIJ_9"
int MatMult_SeqMAIJ_9(Mat A,Vec xx,Vec yy)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,sum1, sum2, sum3, sum4, sum5, sum6, sum7, sum8, sum9;
  int           ierr,m = b->AIJ->m,*idx,*ii;
  int           n,i,jrow,j;

  PetscFunctionBegin;
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(yy,&y);CHKERRQ(ierr);
  idx  = a->j;
  v    = a->a;
  ii   = a->i;

  for (i=0; i<m; i++) {
    jrow = ii[i];
    n    = ii[i+1] - jrow;
    sum1  = 0.0;
    sum2  = 0.0;
    sum3  = 0.0;
    sum4  = 0.0;
    sum5  = 0.0;
    sum6  = 0.0;
    sum7  = 0.0;
    sum8  = 0.0;
    sum9  = 0.0;
    for (j=0; j<n; j++) {
      sum1 += v[jrow]*x[9*idx[jrow]];
      sum2 += v[jrow]*x[9*idx[jrow]+1];
      sum3 += v[jrow]*x[9*idx[jrow]+2];
      sum4 += v[jrow]*x[9*idx[jrow]+3];
      sum5 += v[jrow]*x[9*idx[jrow]+4];
      sum6 += v[jrow]*x[9*idx[jrow]+5];
      sum7 += v[jrow]*x[9*idx[jrow]+6];
      sum8 += v[jrow]*x[9*idx[jrow]+7];
      sum9 += v[jrow]*x[9*idx[jrow]+8];
      jrow++;
     }
    y[9*i]   = sum1;
    y[9*i+1] = sum2;
    y[9*i+2] = sum3;
    y[9*i+3] = sum4;
    y[9*i+4] = sum5;
    y[9*i+5] = sum6;
    y[9*i+6] = sum7;
    y[9*i+7] = sum8;
    y[9*i+8] = sum9;
  }

  PetscLogFlops(18*a->nz - 9*m);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(yy,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMultTranspose_SeqMAIJ_9"
int MatMultTranspose_SeqMAIJ_9(Mat A,Vec xx,Vec yy)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,alpha1,alpha2,alpha3,alpha4,alpha5,alpha6,alpha7,alpha8,alpha9,zero = 0.0;
  int           ierr,m = b->AIJ->m,n,i,*idx;

  PetscFunctionBegin; 
  ierr = VecSet(&zero,yy);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(yy,&y);CHKERRQ(ierr);

  for (i=0; i<m; i++) {
    idx    = a->j + a->i[i] ;
    v      = a->a + a->i[i] ;
    n      = a->i[i+1] - a->i[i];
    alpha1 = x[9*i];
    alpha2 = x[9*i+1];
    alpha3 = x[9*i+2];
    alpha4 = x[9*i+3];
    alpha5 = x[9*i+4];
    alpha6 = x[9*i+5];
    alpha7 = x[9*i+6];
    alpha8 = x[9*i+7];
    alpha9 = x[9*i+8];
    while (n-->0) {
      y[9*(*idx)]   += alpha1*(*v);
      y[9*(*idx)+1] += alpha2*(*v);
      y[9*(*idx)+2] += alpha3*(*v);
      y[9*(*idx)+3] += alpha4*(*v);
      y[9*(*idx)+4] += alpha5*(*v);
      y[9*(*idx)+5] += alpha6*(*v);
      y[9*(*idx)+6] += alpha7*(*v);
      y[9*(*idx)+7] += alpha8*(*v);
      y[9*(*idx)+8] += alpha9*(*v);
      idx++; v++;
    }
  }
  PetscLogFlops(18*a->nz - 9*b->AIJ->n);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(yy,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMultAdd_SeqMAIJ_9"
int MatMultAdd_SeqMAIJ_9(Mat A,Vec xx,Vec yy,Vec zz)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,sum1, sum2, sum3, sum4, sum5, sum6, sum7, sum8, sum9;
  int           ierr,m = b->AIJ->m,*idx,*ii;
  int           n,i,jrow,j;

  PetscFunctionBegin;
  if (yy != zz) {ierr = VecCopy(yy,zz);CHKERRQ(ierr);}
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(zz,&y);CHKERRQ(ierr);
  idx  = a->j;
  v    = a->a;
  ii   = a->i;

  for (i=0; i<m; i++) {
    jrow = ii[i];
    n    = ii[i+1] - jrow;
    sum1  = 0.0;
    sum2  = 0.0;
    sum3  = 0.0;
    sum4  = 0.0;
    sum5  = 0.0;
    sum6  = 0.0;
    sum7  = 0.0;
    sum8  = 0.0;
    sum9  = 0.0;
    for (j=0; j<n; j++) {
      sum1 += v[jrow]*x[9*idx[jrow]];
      sum2 += v[jrow]*x[9*idx[jrow]+1];
      sum3 += v[jrow]*x[9*idx[jrow]+2];
      sum4 += v[jrow]*x[9*idx[jrow]+3];
      sum5 += v[jrow]*x[9*idx[jrow]+4];
      sum6 += v[jrow]*x[9*idx[jrow]+5];
      sum7 += v[jrow]*x[9*idx[jrow]+6];
      sum8 += v[jrow]*x[9*idx[jrow]+7];
      sum9 += v[jrow]*x[9*idx[jrow]+8];
      jrow++;
     }
    y[9*i]   += sum1;
    y[9*i+1] += sum2;
    y[9*i+2] += sum3;
    y[9*i+3] += sum4;
    y[9*i+4] += sum5;
    y[9*i+5] += sum6;
    y[9*i+6] += sum7;
    y[9*i+7] += sum8;
    y[9*i+8] += sum9;
  }

  PetscLogFlops(18*a->nz);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(zz,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMultTransposeAdd_SeqMAIJ_9"
int MatMultTransposeAdd_SeqMAIJ_9(Mat A,Vec xx,Vec yy,Vec zz)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,alpha1,alpha2,alpha3,alpha4,alpha5,alpha6,alpha7,alpha8,alpha9;
  int           ierr,m = b->AIJ->m,n,i,*idx;

  PetscFunctionBegin; 
  if (yy != zz) {ierr = VecCopy(yy,zz);CHKERRQ(ierr);}
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(zz,&y);CHKERRQ(ierr);
  for (i=0; i<m; i++) {
    idx    = a->j + a->i[i] ;
    v      = a->a + a->i[i] ;
    n      = a->i[i+1] - a->i[i];
    alpha1 = x[9*i];
    alpha2 = x[9*i+1];
    alpha3 = x[9*i+2];
    alpha4 = x[9*i+3];
    alpha5 = x[9*i+4];
    alpha6 = x[9*i+5];
    alpha7 = x[9*i+6];
    alpha8 = x[9*i+7];
    alpha9 = x[9*i+8];
    while (n-->0) {
      y[9*(*idx)]   += alpha1*(*v);
      y[9*(*idx)+1] += alpha2*(*v);
      y[9*(*idx)+2] += alpha3*(*v);
      y[9*(*idx)+3] += alpha4*(*v);
      y[9*(*idx)+4] += alpha5*(*v);
      y[9*(*idx)+5] += alpha6*(*v);
      y[9*(*idx)+6] += alpha7*(*v);
      y[9*(*idx)+7] += alpha8*(*v);
      y[9*(*idx)+8] += alpha9*(*v);
      idx++; v++;
    }
  }
  PetscLogFlops(18*a->nz);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(zz,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*--------------------------------------------------------------------------------------------*/
#undef __FUNCT__  
#define __FUNCT__ "MatMult_SeqMAIJ_16"
int MatMult_SeqMAIJ_16(Mat A,Vec xx,Vec yy)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,sum1, sum2, sum3, sum4, sum5, sum6, sum7, sum8;
  PetscScalar   sum9, sum10, sum11, sum12, sum13, sum14, sum15, sum16;
  int           ierr,m = b->AIJ->m,*idx,*ii;
  int           n,i,jrow,j;

  PetscFunctionBegin;
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(yy,&y);CHKERRQ(ierr);
  idx  = a->j;
  v    = a->a;
  ii   = a->i;

  for (i=0; i<m; i++) {
    jrow = ii[i];
    n    = ii[i+1] - jrow;
    sum1  = 0.0;
    sum2  = 0.0;
    sum3  = 0.0;
    sum4  = 0.0;
    sum5  = 0.0;
    sum6  = 0.0;
    sum7  = 0.0;
    sum8  = 0.0;
    sum9  = 0.0;
    sum10 = 0.0;
    sum11 = 0.0;
    sum12 = 0.0;
    sum13 = 0.0;
    sum14 = 0.0;
    sum15 = 0.0;
    sum16 = 0.0;
    for (j=0; j<n; j++) {
      sum1  += v[jrow]*x[16*idx[jrow]];
      sum2  += v[jrow]*x[16*idx[jrow]+1];
      sum3  += v[jrow]*x[16*idx[jrow]+2];
      sum4  += v[jrow]*x[16*idx[jrow]+3];
      sum5  += v[jrow]*x[16*idx[jrow]+4];
      sum6  += v[jrow]*x[16*idx[jrow]+5];
      sum7  += v[jrow]*x[16*idx[jrow]+6];
      sum8  += v[jrow]*x[16*idx[jrow]+7];
      sum9  += v[jrow]*x[16*idx[jrow]+8];
      sum10 += v[jrow]*x[16*idx[jrow]+9];
      sum11 += v[jrow]*x[16*idx[jrow]+10];
      sum12 += v[jrow]*x[16*idx[jrow]+11];
      sum13 += v[jrow]*x[16*idx[jrow]+12];
      sum14 += v[jrow]*x[16*idx[jrow]+13];
      sum15 += v[jrow]*x[16*idx[jrow]+14];
      sum16 += v[jrow]*x[16*idx[jrow]+15];
      jrow++;
     }
    y[16*i]    = sum1;
    y[16*i+1]  = sum2;
    y[16*i+2]  = sum3;
    y[16*i+3]  = sum4;
    y[16*i+4]  = sum5;
    y[16*i+5]  = sum6;
    y[16*i+6]  = sum7;
    y[16*i+7]  = sum8;
    y[16*i+8]  = sum9;
    y[16*i+9]  = sum10;
    y[16*i+10] = sum11;
    y[16*i+11] = sum12;
    y[16*i+12] = sum13;
    y[16*i+13] = sum14;
    y[16*i+14] = sum15;
    y[16*i+15] = sum16;
  }

  PetscLogFlops(32*a->nz - 16*m);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(yy,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMultTranspose_SeqMAIJ_16"
int MatMultTranspose_SeqMAIJ_16(Mat A,Vec xx,Vec yy)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,alpha1,alpha2,alpha3,alpha4,alpha5,alpha6,alpha7,alpha8,zero = 0.0;
  PetscScalar   alpha9,alpha10,alpha11,alpha12,alpha13,alpha14,alpha15,alpha16;
  int           ierr,m = b->AIJ->m,n,i,*idx;

  PetscFunctionBegin; 
  ierr = VecSet(&zero,yy);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(yy,&y);CHKERRQ(ierr);

  for (i=0; i<m; i++) {
    idx    = a->j + a->i[i] ;
    v      = a->a + a->i[i] ;
    n      = a->i[i+1] - a->i[i];
    alpha1  = x[16*i];
    alpha2  = x[16*i+1];
    alpha3  = x[16*i+2];
    alpha4  = x[16*i+3];
    alpha5  = x[16*i+4];
    alpha6  = x[16*i+5];
    alpha7  = x[16*i+6];
    alpha8  = x[16*i+7];
    alpha9  = x[16*i+8];
    alpha10 = x[16*i+9];
    alpha11 = x[16*i+10];
    alpha12 = x[16*i+11];
    alpha13 = x[16*i+12];
    alpha14 = x[16*i+13];
    alpha15 = x[16*i+14];
    alpha16 = x[16*i+15];
    while (n-->0) {
      y[16*(*idx)]    += alpha1*(*v);
      y[16*(*idx)+1]  += alpha2*(*v);
      y[16*(*idx)+2]  += alpha3*(*v);
      y[16*(*idx)+3]  += alpha4*(*v);
      y[16*(*idx)+4]  += alpha5*(*v);
      y[16*(*idx)+5]  += alpha6*(*v);
      y[16*(*idx)+6]  += alpha7*(*v);
      y[16*(*idx)+7]  += alpha8*(*v);
      y[16*(*idx)+8]  += alpha9*(*v);
      y[16*(*idx)+9]  += alpha10*(*v);
      y[16*(*idx)+10] += alpha11*(*v);
      y[16*(*idx)+11] += alpha12*(*v);
      y[16*(*idx)+12] += alpha13*(*v);
      y[16*(*idx)+13] += alpha14*(*v);
      y[16*(*idx)+14] += alpha15*(*v);
      y[16*(*idx)+15] += alpha16*(*v);
      idx++; v++;
    }
  }
  PetscLogFlops(32*a->nz - 16*b->AIJ->n);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(yy,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMultAdd_SeqMAIJ_16"
int MatMultAdd_SeqMAIJ_16(Mat A,Vec xx,Vec yy,Vec zz)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,sum1, sum2, sum3, sum4, sum5, sum6, sum7, sum8;
  PetscScalar   sum9, sum10, sum11, sum12, sum13, sum14, sum15, sum16;
  int           ierr,m = b->AIJ->m,*idx,*ii;
  int           n,i,jrow,j;

  PetscFunctionBegin;
  if (yy != zz) {ierr = VecCopy(yy,zz);CHKERRQ(ierr);}
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(zz,&y);CHKERRQ(ierr);
  idx  = a->j;
  v    = a->a;
  ii   = a->i;

  for (i=0; i<m; i++) {
    jrow = ii[i];
    n    = ii[i+1] - jrow;
    sum1  = 0.0;
    sum2  = 0.0;
    sum3  = 0.0;
    sum4  = 0.0;
    sum5  = 0.0;
    sum6  = 0.0;
    sum7  = 0.0;
    sum8  = 0.0;
    sum9  = 0.0;
    sum10 = 0.0;
    sum11 = 0.0;
    sum12 = 0.0;
    sum13 = 0.0;
    sum14 = 0.0;
    sum15 = 0.0;
    sum16 = 0.0;
    for (j=0; j<n; j++) {
      sum1  += v[jrow]*x[16*idx[jrow]];
      sum2  += v[jrow]*x[16*idx[jrow]+1];
      sum3  += v[jrow]*x[16*idx[jrow]+2];
      sum4  += v[jrow]*x[16*idx[jrow]+3];
      sum5  += v[jrow]*x[16*idx[jrow]+4];
      sum6  += v[jrow]*x[16*idx[jrow]+5];
      sum7  += v[jrow]*x[16*idx[jrow]+6];
      sum8  += v[jrow]*x[16*idx[jrow]+7];
      sum9  += v[jrow]*x[16*idx[jrow]+8];
      sum10 += v[jrow]*x[16*idx[jrow]+9];
      sum11 += v[jrow]*x[16*idx[jrow]+10];
      sum12 += v[jrow]*x[16*idx[jrow]+11];
      sum13 += v[jrow]*x[16*idx[jrow]+12];
      sum14 += v[jrow]*x[16*idx[jrow]+13];
      sum15 += v[jrow]*x[16*idx[jrow]+14];
      sum16 += v[jrow]*x[16*idx[jrow]+15];
      jrow++;
     }
    y[16*i]    += sum1;
    y[16*i+1]  += sum2;
    y[16*i+2]  += sum3;
    y[16*i+3]  += sum4;
    y[16*i+4]  += sum5;
    y[16*i+5]  += sum6;
    y[16*i+6]  += sum7;
    y[16*i+7]  += sum8;
    y[16*i+8]  += sum9;
    y[16*i+9]  += sum10;
    y[16*i+10] += sum11;
    y[16*i+11] += sum12;
    y[16*i+12] += sum13;
    y[16*i+13] += sum14;
    y[16*i+14] += sum15;
    y[16*i+15] += sum16;
  }

  PetscLogFlops(32*a->nz);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(zz,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMultTransposeAdd_SeqMAIJ_16"
int MatMultTransposeAdd_SeqMAIJ_16(Mat A,Vec xx,Vec yy,Vec zz)
{
  Mat_SeqMAIJ   *b = (Mat_SeqMAIJ*)A->data;
  Mat_SeqAIJ    *a = (Mat_SeqAIJ*)b->AIJ->data;
  PetscScalar   *x,*y,*v,alpha1,alpha2,alpha3,alpha4,alpha5,alpha6,alpha7,alpha8;
  PetscScalar   alpha9,alpha10,alpha11,alpha12,alpha13,alpha14,alpha15,alpha16;
  int           ierr,m = b->AIJ->m,n,i,*idx;

  PetscFunctionBegin; 
  if (yy != zz) {ierr = VecCopy(yy,zz);CHKERRQ(ierr);}
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(zz,&y);CHKERRQ(ierr);
  for (i=0; i<m; i++) {
    idx    = a->j + a->i[i] ;
    v      = a->a + a->i[i] ;
    n      = a->i[i+1] - a->i[i];
    alpha1 = x[16*i];
    alpha2 = x[16*i+1];
    alpha3 = x[16*i+2];
    alpha4 = x[16*i+3];
    alpha5 = x[16*i+4];
    alpha6 = x[16*i+5];
    alpha7 = x[16*i+6];
    alpha8 = x[16*i+7];
    alpha9  = x[16*i+8];
    alpha10 = x[16*i+9];
    alpha11 = x[16*i+10];
    alpha12 = x[16*i+11];
    alpha13 = x[16*i+12];
    alpha14 = x[16*i+13];
    alpha15 = x[16*i+14];
    alpha16 = x[16*i+15];
    while (n-->0) {
      y[16*(*idx)]   += alpha1*(*v);
      y[16*(*idx)+1] += alpha2*(*v);
      y[16*(*idx)+2] += alpha3*(*v);
      y[16*(*idx)+3] += alpha4*(*v);
      y[16*(*idx)+4] += alpha5*(*v);
      y[16*(*idx)+5] += alpha6*(*v);
      y[16*(*idx)+6] += alpha7*(*v);
      y[16*(*idx)+7] += alpha8*(*v);
      y[16*(*idx)+8]  += alpha9*(*v);
      y[16*(*idx)+9]  += alpha10*(*v);
      y[16*(*idx)+10] += alpha11*(*v);
      y[16*(*idx)+11] += alpha12*(*v);
      y[16*(*idx)+12] += alpha13*(*v);
      y[16*(*idx)+13] += alpha14*(*v);
      y[16*(*idx)+14] += alpha15*(*v);
      y[16*(*idx)+15] += alpha16*(*v);
      idx++; v++;
    }
  }
  PetscLogFlops(32*a->nz);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(zz,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*===================================================================================*/
#undef __FUNCT__  
#define __FUNCT__ "MatMult_MPIMAIJ_dof"
int MatMult_MPIMAIJ_dof(Mat A,Vec xx,Vec yy)
{
  Mat_MPIMAIJ *b = (Mat_MPIMAIJ*)A->data;
  int         ierr;
  PetscFunctionBegin;

  /* start the scatter */
  ierr = VecScatterBegin(xx,b->w,INSERT_VALUES,SCATTER_FORWARD,b->ctx);CHKERRQ(ierr);
  ierr = (*b->AIJ->ops->mult)(b->AIJ,xx,yy);CHKERRQ(ierr);
  ierr = VecScatterEnd(xx,b->w,INSERT_VALUES,SCATTER_FORWARD,b->ctx);CHKERRQ(ierr);
  ierr = (*b->OAIJ->ops->multadd)(b->OAIJ,b->w,yy,yy);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMultTranspose_MPIMAIJ_dof"
int MatMultTranspose_MPIMAIJ_dof(Mat A,Vec xx,Vec yy)
{
  Mat_MPIMAIJ *b = (Mat_MPIMAIJ*)A->data;
  int         ierr;
  PetscFunctionBegin;
  ierr = (*b->OAIJ->ops->multtranspose)(b->OAIJ,xx,b->w);CHKERRQ(ierr);
  ierr = VecScatterBegin(b->w,yy,ADD_VALUES,SCATTER_REVERSE,b->ctx);CHKERRQ(ierr);
  ierr = (*b->AIJ->ops->multtranspose)(b->AIJ,xx,yy);CHKERRQ(ierr);
  ierr = VecScatterEnd(b->w,yy,ADD_VALUES,SCATTER_REVERSE,b->ctx);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMultAdd_MPIMAIJ_dof"
int MatMultAdd_MPIMAIJ_dof(Mat A,Vec xx,Vec yy,Vec zz)
{
  Mat_MPIMAIJ *b = (Mat_MPIMAIJ*)A->data;
  int         ierr;
  PetscFunctionBegin;

  /* start the scatter */
  ierr = VecScatterBegin(xx,b->w,INSERT_VALUES,SCATTER_FORWARD,b->ctx);CHKERRQ(ierr);
  ierr = (*b->AIJ->ops->multadd)(b->AIJ,xx,yy,zz);CHKERRQ(ierr);
  ierr = VecScatterEnd(xx,b->w,INSERT_VALUES,SCATTER_FORWARD,b->ctx);CHKERRQ(ierr);
  ierr = (*b->OAIJ->ops->multadd)(b->OAIJ,b->w,yy,zz);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMultTransposeAdd_MPIMAIJ_dof"
int MatMultTransposeAdd_MPIMAIJ_dof(Mat A,Vec xx,Vec yy,Vec zz)
{
  Mat_MPIMAIJ *b = (Mat_MPIMAIJ*)A->data;
  int         ierr;
  PetscFunctionBegin;
  ierr = (*b->OAIJ->ops->multtranspose)(b->OAIJ,xx,b->w);CHKERRQ(ierr);
  ierr = VecScatterBegin(b->w,zz,ADD_VALUES,SCATTER_REVERSE,b->ctx);CHKERRQ(ierr);
  ierr = (*b->AIJ->ops->multtransposeadd)(b->AIJ,xx,yy,zz);CHKERRQ(ierr);
  ierr = VecScatterEnd(b->w,zz,ADD_VALUES,SCATTER_REVERSE,b->ctx);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* ---------------------------------------------------------------------------------- */
/*MC
  MatCreateMAIJ - Creates a matrix type providing restriction and interpolation 
  operations for multicomponent problems.  It interpolates each component the same
  way independently.  The matrix type is based on MATSEQAIJ for sequential matrices,
  and MATMPIAIJ for distributed matrices.

  Operations provided:
. MatMult
. MatMultTranspose
. MatMultAdd
. MatMultTransposeAdd

  Level: advanced

M*/
#undef __FUNCT__  
#define __FUNCT__ "MatCreateMAIJ" 
int MatCreateMAIJ(Mat A,int dof,Mat *maij)
{
  int         ierr,size,n;
  Mat_MPIMAIJ *b;
  Mat         B;

  PetscFunctionBegin;
  ierr   = PetscObjectReference((PetscObject)A);CHKERRQ(ierr);

  if (dof == 1) {
    *maij = A;
  } else {
    ierr = MatCreate(A->comm,dof*A->m,dof*A->n,dof*A->M,dof*A->N,&B);CHKERRQ(ierr);
    B->assembled    = PETSC_TRUE;

    ierr = MPI_Comm_size(A->comm,&size);CHKERRQ(ierr);
    if (size == 1) {
      ierr = MatSetType(B,MATSEQMAIJ);CHKERRQ(ierr);
      B->ops->destroy = MatDestroy_SeqMAIJ;
      b      = (Mat_MPIMAIJ*)B->data;
      b->dof = dof;
      b->AIJ = A;
      if (dof == 2) {
        B->ops->mult             = MatMult_SeqMAIJ_2;
        B->ops->multadd          = MatMultAdd_SeqMAIJ_2;
        B->ops->multtranspose    = MatMultTranspose_SeqMAIJ_2;
        B->ops->multtransposeadd = MatMultTransposeAdd_SeqMAIJ_2;
      } else if (dof == 3) {
        B->ops->mult             = MatMult_SeqMAIJ_3;
        B->ops->multadd          = MatMultAdd_SeqMAIJ_3;
        B->ops->multtranspose    = MatMultTranspose_SeqMAIJ_3;
        B->ops->multtransposeadd = MatMultTransposeAdd_SeqMAIJ_3;
      } else if (dof == 4) {
        B->ops->mult             = MatMult_SeqMAIJ_4;
        B->ops->multadd          = MatMultAdd_SeqMAIJ_4;
        B->ops->multtranspose    = MatMultTranspose_SeqMAIJ_4;
        B->ops->multtransposeadd = MatMultTransposeAdd_SeqMAIJ_4;
      } else if (dof == 5) {
        B->ops->mult             = MatMult_SeqMAIJ_5;
        B->ops->multadd          = MatMultAdd_SeqMAIJ_5;
        B->ops->multtranspose    = MatMultTranspose_SeqMAIJ_5;
        B->ops->multtransposeadd = MatMultTransposeAdd_SeqMAIJ_5;
      } else if (dof == 6) {
        B->ops->mult             = MatMult_SeqMAIJ_6;
        B->ops->multadd          = MatMultAdd_SeqMAIJ_6;
        B->ops->multtranspose    = MatMultTranspose_SeqMAIJ_6;
        B->ops->multtransposeadd = MatMultTransposeAdd_SeqMAIJ_6;
      } else if (dof == 8) {
        B->ops->mult             = MatMult_SeqMAIJ_8;
        B->ops->multadd          = MatMultAdd_SeqMAIJ_8;
        B->ops->multtranspose    = MatMultTranspose_SeqMAIJ_8;
        B->ops->multtransposeadd = MatMultTransposeAdd_SeqMAIJ_8;
      } else if (dof == 9) {
        B->ops->mult             = MatMult_SeqMAIJ_9;
        B->ops->multadd          = MatMultAdd_SeqMAIJ_9;
        B->ops->multtranspose    = MatMultTranspose_SeqMAIJ_9;
        B->ops->multtransposeadd = MatMultTransposeAdd_SeqMAIJ_9;
      } else if (dof == 16) {
        B->ops->mult             = MatMult_SeqMAIJ_16;
        B->ops->multadd          = MatMultAdd_SeqMAIJ_16;
        B->ops->multtranspose    = MatMultTranspose_SeqMAIJ_16;
        B->ops->multtransposeadd = MatMultTransposeAdd_SeqMAIJ_16;
      } else {
        SETERRQ1(1,"Cannot handle a dof of %d. Send request for code to petsc-maint@mcs.anl.gov\n",dof);
      }
    } else {
      Mat_MPIAIJ *mpiaij = (Mat_MPIAIJ *)A->data;
      IS         from,to;
      Vec        gvec;
      int        *garray,i;

      ierr = MatSetType(B,MATMPIMAIJ);CHKERRQ(ierr);
      B->ops->destroy = MatDestroy_MPIMAIJ;
      b      = (Mat_MPIMAIJ*)B->data;
      b->dof = dof;
      b->A   = A;
      ierr = MatCreateMAIJ(mpiaij->A,dof,&b->AIJ);CHKERRQ(ierr);
      ierr = MatCreateMAIJ(mpiaij->B,dof,&b->OAIJ);CHKERRQ(ierr);

      ierr = VecGetSize(mpiaij->lvec,&n);CHKERRQ(ierr);
      ierr = VecCreateSeq(PETSC_COMM_SELF,n*dof,&b->w);CHKERRQ(ierr);

      /* create two temporary Index sets for build scatter gather */
      ierr = PetscMalloc((n+1)*sizeof(int),&garray);CHKERRQ(ierr);
      for (i=0; i<n; i++) garray[i] = dof*mpiaij->garray[i];
      ierr = ISCreateBlock(A->comm,dof,n,garray,&from);CHKERRQ(ierr);
      ierr = PetscFree(garray);CHKERRQ(ierr);
      ierr = ISCreateStride(PETSC_COMM_SELF,n*dof,0,1,&to);CHKERRQ(ierr);

      /* create temporary global vector to generate scatter context */
      ierr = VecCreateMPI(A->comm,dof*A->n,dof*A->N,&gvec);CHKERRQ(ierr);

      /* generate the scatter context */
      ierr = VecScatterCreate(gvec,from,b->w,to,&b->ctx);CHKERRQ(ierr);

      ierr = ISDestroy(from);CHKERRQ(ierr);
      ierr = ISDestroy(to);CHKERRQ(ierr);
      ierr = VecDestroy(gvec);CHKERRQ(ierr);

      B->ops->mult             = MatMult_MPIMAIJ_dof;
      B->ops->multtranspose    = MatMultTranspose_MPIMAIJ_dof;
      B->ops->multadd          = MatMultAdd_MPIMAIJ_dof;
      B->ops->multtransposeadd = MatMultTransposeAdd_MPIMAIJ_dof;
    }
    *maij = B;
  }
  PetscFunctionReturn(0);
}












