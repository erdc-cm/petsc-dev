/*
 * Example code testing SeqDense matrices with an LDA (leading dimension
 * of the user-allocated arrray) larger than M.
 * This example tests the functionality of MatSolve.
 */
#include <stdlib.h>
#include "petscmat.h"
#include "petscsles.h"

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **argv)
{
  SLES solver; KSP itmeth; PC pc;
  Mat A,B,A11,A12,A21,A22;
  Vec X,X1,X2,Y,Z,Z1,Z2;
  PetscScalar *a,*b,*x,*y,*z,v,one=1,mone=-1;
  PetscReal nrm;
  int ierr,size=8,lda=10, i,j;

  PetscInitialize(&argc,&argv,0,0);

  /*
   * Create matrix and three vectors: these are all normal
   */
  ierr = PetscMalloc(size*size*sizeof(PetscScalar),&a); CHKERRQ(ierr);
  ierr = PetscMalloc(lda*size*sizeof(PetscScalar),&b); CHKERRQ(ierr);
  for (i=0; i<size; i++) {
    for (j=0; j<size; j++) {
      a[i+j*size] = rand(); b[i+j*lda] = a[i+j*size];
    }
  }
  ierr = MatCreate(MPI_COMM_SELF,size,size,size,size,&A); CHKERRQ(ierr);
  ierr = MatSetType(A,MATSEQDENSE); CHKERRQ(ierr);
  ierr = MatSeqDenseSetPreallocation(A,a); CHKERRQ(ierr);
  ierr = MatAssemblyBegin(A,MAT_FINAL_ASSEMBLY); CHKERRQ(ierr);
  ierr = MatAssemblyEnd(A,MAT_FINAL_ASSEMBLY); CHKERRQ(ierr);

  ierr = MatCreate(MPI_COMM_SELF,size,size,size,size,&B); CHKERRQ(ierr);
  ierr = MatSetType(B,MATSEQDENSE); CHKERRQ(ierr);
  ierr = MatSeqDenseSetPreallocation(B,b); CHKERRQ(ierr);
  ierr = MatSeqDenseSetLDA(B,lda); CHKERRQ(ierr);
  ierr = MatAssemblyBegin(B,MAT_FINAL_ASSEMBLY); CHKERRQ(ierr);
  ierr = MatAssemblyEnd(B,MAT_FINAL_ASSEMBLY); CHKERRQ(ierr);

  ierr = PetscMalloc(size*sizeof(PetscScalar),&x); CHKERRQ(ierr);
  for (i=0; i<size; i++) {
    x[i] = one;
  }
  ierr = VecCreateSeqWithArray(MPI_COMM_SELF,size,x,&X); CHKERRQ(ierr);
  ierr = VecAssemblyBegin(X); CHKERRQ(ierr);
  ierr = VecAssemblyEnd(X); CHKERRQ(ierr);

  ierr = PetscMalloc(size*sizeof(PetscScalar),&y); CHKERRQ(ierr);
  ierr = VecCreateSeqWithArray(MPI_COMM_SELF,size,y,&Y); CHKERRQ(ierr);
  ierr = VecAssemblyBegin(Y); CHKERRQ(ierr);
  ierr = VecAssemblyEnd(Y); CHKERRQ(ierr);

  ierr = PetscMalloc(size*sizeof(PetscScalar),&z); CHKERRQ(ierr);
  ierr = VecCreateSeqWithArray(MPI_COMM_SELF,size,z,&Z); CHKERRQ(ierr);
  ierr = VecAssemblyBegin(Z); CHKERRQ(ierr);
  ierr = VecAssemblyEnd(Z); CHKERRQ(ierr);

  /*
   * Solve with A and B
   */
  ierr = SLESCreate(MPI_COMM_SELF,&solver); CHKERRQ(ierr);
  ierr = SLESGetKSP(solver,&itmeth); CHKERRQ(ierr);
  ierr = KSPSetType(itmeth,KSPPREONLY); CHKERRQ(ierr);
  ierr = SLESGetPC(solver,&pc); CHKERRQ(ierr);
  ierr = PCSetType(pc,PCLU); CHKERRQ(ierr);
  ierr = SLESSetOperators(solver,A,A,DIFFERENT_NONZERO_PATTERN); CHKERRQ(ierr);
  ierr = SLESSolve(solver,X,Y,PETSC_NULL); CHKERRQ(ierr);
  ierr = SLESSetOperators(solver,B,B,DIFFERENT_NONZERO_PATTERN); CHKERRQ(ierr);
  ierr = SLESSolve(solver,X,Z,PETSC_NULL); CHKERRQ(ierr);
  ierr = VecAXPY(&mone,Y,Z); CHKERRQ(ierr);
  ierr = VecNorm(Z,NORM_2,&nrm);
  printf("Test1; error norm=%e\n",nrm);

  ierr = PetscFinalize();CHKERRQ(ierr);
  return 0;
}
