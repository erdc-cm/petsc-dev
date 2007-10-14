#define PETSCMAT_DLL

#include "include/private/matimpl.h"          /*I "petscmat.h" I*/

typedef struct {
  Mat A;
  Vec w;
} Mat_Normal;

#undef __FUNCT__  
#define __FUNCT__ "MatMult_Normal"
PetscErrorCode MatMult_Normal(Mat N,Vec x,Vec y)
{
  Mat_Normal     *Na = (Mat_Normal*)N->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = MatMult(Na->A,x,Na->w);CHKERRQ(ierr);
  ierr = MatMultTranspose(Na->A,Na->w,y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
 
#undef __FUNCT__   
#define __FUNCT__ "MatMultAdd_Normal" 
PetscErrorCode MatMultAdd_Normal(Mat N,Vec v1,Vec v2,Vec v3) 
{ 
  Mat_Normal     *Na = (Mat_Normal*)N->data; 
  PetscErrorCode ierr; 
 
  PetscFunctionBegin; 
  ierr = MatMult(Na->A,v1,Na->w);CHKERRQ(ierr); 
  ierr = MatMultTransposeAdd(Na->A,Na->w,v2,v3);CHKERRQ(ierr); 
  PetscFunctionReturn(0); 
} 

#undef __FUNCT__  
#define __FUNCT__ "MatDestroy_Normal"
PetscErrorCode MatDestroy_Normal(Mat N)
{
  Mat_Normal     *Na = (Mat_Normal*)N->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (Na->A) { ierr = MatDestroy(Na->A);CHKERRQ(ierr); }
  if (Na->w) { ierr = VecDestroy(Na->w);CHKERRQ(ierr); }
  ierr = PetscFree(Na);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
  
/*
      Slow, nonscalable version
*/
#undef __FUNCT__  
#define __FUNCT__ "MatGetDiagonal_Normal"
PetscErrorCode MatGetDiagonal_Normal(Mat N,Vec v)
{
  Mat_Normal        *Na = (Mat_Normal*)N->data;
  Mat               A = Na->A;
  PetscErrorCode    ierr;
  PetscInt          i,j,rstart,rend,nnz;
  const PetscInt    *cols;
  PetscScalar       *diag,*work,*values;
  const PetscScalar *mvalues;

  PetscFunctionBegin;
  ierr = PetscMalloc(2*A->cmap.N*sizeof(PetscScalar),&diag);CHKERRQ(ierr);
  work = diag + A->cmap.N;
  ierr = PetscMemzero(work,A->cmap.N*sizeof(PetscScalar));CHKERRQ(ierr);
  ierr = MatGetOwnershipRange(A,&rstart,&rend);CHKERRQ(ierr);
  for (i=rstart; i<rend; i++) {
    ierr = MatGetRow(A,i,&nnz,&cols,&mvalues);CHKERRQ(ierr);
    for (j=0; j<nnz; j++) {
      work[cols[j]] += mvalues[j]*mvalues[j];
    }
    ierr = MatRestoreRow(A,i,&nnz,&cols,&mvalues);CHKERRQ(ierr);
  }
  ierr = MPI_Allreduce(work,diag,A->cmap.N,MPIU_SCALAR,MPI_SUM,((PetscObject)N)->comm);CHKERRQ(ierr);
  rstart = N->cmap.rstart;
  rend   = N->cmap.rend;
  ierr = VecGetArray(v,&values);CHKERRQ(ierr);
  ierr = PetscMemcpy(values,diag+rstart,(rend-rstart)*sizeof(PetscScalar));CHKERRQ(ierr);
  ierr = VecRestoreArray(v,&values);CHKERRQ(ierr);
  ierr = PetscFree(diag);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatCreateNormal"
/*@
      MatCreateNormal - Creates a new matrix object that behaves like A'*A.

   Collective on Mat

   Input Parameter:
.   A  - the (possibly rectangular) matrix

   Output Parameter:
.   N - the matrix that represents A'*A

   Level: intermediate

   Notes: The product A'*A is NOT actually formed! Rather the new matrix
          object performs the matrix-vector product by first multiplying by
          A and then A'
@*/
PetscErrorCode PETSCMAT_DLLEXPORT MatCreateNormal(Mat A,Mat *N)
{
  PetscErrorCode ierr;
  PetscInt       m,n;
  Mat_Normal     *Na;  

  PetscFunctionBegin;
  ierr = MatGetLocalSize(A,&m,&n);CHKERRQ(ierr);
  ierr = MatCreate(((PetscObject)A)->comm,N);CHKERRQ(ierr);
  ierr = MatSetSizes(*N,n,n,PETSC_DECIDE,PETSC_DECIDE);CHKERRQ(ierr);
  ierr = PetscObjectChangeTypeName((PetscObject)*N,MATNORMAL);CHKERRQ(ierr);
  
  ierr      = PetscNewLog(*N,Mat_Normal,&Na);CHKERRQ(ierr);
  (*N)->data = (void*) Na;
  ierr      = PetscObjectReference((PetscObject)A);CHKERRQ(ierr);
  Na->A     = A;

  ierr    = VecCreateMPI(((PetscObject)A)->comm,m,PETSC_DECIDE,&Na->w);CHKERRQ(ierr);
  (*N)->ops->destroy     = MatDestroy_Normal;
  (*N)->ops->mult        = MatMult_Normal;
  (*N)->ops->multadd     = MatMultAdd_Normal; 
  (*N)->ops->getdiagonal = MatGetDiagonal_Normal;
  (*N)->assembled        = PETSC_TRUE;
  (*N)->cmap.N           = A->cmap.N;
  (*N)->rmap.N           = A->cmap.N;
  (*N)->cmap.n           = A->cmap.n;
  (*N)->rmap.n           = A->cmap.n;
  PetscFunctionReturn(0);
}

