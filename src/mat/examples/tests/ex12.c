
static char help[] = "Tests the use of MatZeroRows() for parallel matrices.\n\
This example also tests the use of MatDuplicate() for both MPIAIJ and MPIBAIJ matrices";

#include "petscmat.h"

EXTERN PetscErrorCode TestMatZeroRows_Basic(Mat,IS,const PetscScalar[] );
EXTERN PetscErrorCode TestMatZeroRows_with_no_allocation(Mat,IS,const PetscScalar[]);

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **args)
{
  Mat            A;
  PetscInt       i,j,m = 3,n,I,J,Imax;
  PetscMPIInt    rank,size;
  PetscErrorCode ierr;
  PetscScalar    v,diag=-4.0;
  IS             is;

  PetscInitialize(&argc,&args,(char *)0,help);
  ierr = MPI_Comm_rank(PETSC_COMM_WORLD,&rank);CHKERRQ(ierr);
  ierr = MPI_Comm_size(PETSC_COMM_WORLD,&size);CHKERRQ(ierr);
  n = 2*size;

  /* create A Square matrix for the five point stencil,YET AGAIN*/
  ierr = MatCreate(PETSC_COMM_WORLD,PETSC_DECIDE,PETSC_DECIDE,m*n,m*n,&A);CHKERRQ(ierr);
  ierr = MatSetFromOptions(A);CHKERRQ(ierr);
  for (i=0; i<m; i++) { 
    for (j=2*rank; j<2*rank+2; j++) {
      v = -1.0;  I = j + n*i;
      if (i>0)   {J = I - n; ierr = MatSetValues(A,1,&I,1,&J,&v,INSERT_VALUES);CHKERRQ(ierr);}
      if (i<m-1) {J = I + n; ierr = MatSetValues(A,1,&I,1,&J,&v,INSERT_VALUES);CHKERRQ(ierr);}
      if (j>0)   {J = I - 1; ierr = MatSetValues(A,1,&I,1,&J,&v,INSERT_VALUES);CHKERRQ(ierr);}
      if (j<n-1) {J = I + 1; ierr = MatSetValues(A,1,&I,1,&J,&v,INSERT_VALUES);CHKERRQ(ierr);}
      v = 4.0; ierr = MatSetValues(A,1,&I,1,&I,&v,INSERT_VALUES);CHKERRQ(ierr);
    }
  }
  ierr = MatAssemblyBegin(A,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(A,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);

  /* Create AN IS required by MatZeroRows() */
  Imax = n*rank; if (Imax>= n*m -m - 1) Imax = m*n - m - 1;
  ierr = ISCreateStride(PETSC_COMM_SELF,m,Imax,1,&is);CHKERRQ(ierr);

  ierr = TestMatZeroRows_Basic(A,is,PETSC_NULL);CHKERRQ(ierr);
  ierr = TestMatZeroRows_Basic(A,is,&diag);CHKERRQ(ierr);

  ierr = TestMatZeroRows_with_no_allocation(A,is,PETSC_NULL);CHKERRQ(ierr);
  ierr = TestMatZeroRows_with_no_allocation(A,is,&diag);CHKERRQ(ierr);

  ierr = MatDestroy(A);CHKERRQ(ierr);

  /* Now Create a rectangular matrix with five point stencil (app) 
   n+size is used so that this dimension is always divisible by size.
   This way, we can always use bs = size for any number of procs */
  ierr = MatCreate(PETSC_COMM_WORLD,PETSC_DECIDE,PETSC_DECIDE,m*n,m*(n+size),&A);CHKERRQ(ierr);
  ierr = MatSetFromOptions(A);CHKERRQ(ierr);
  for (i=0; i<m; i++) { 
    for (j=2*rank; j<2*rank+2; j++) {
      v = -1.0;  I = j + n*i;
      if (i>0)   {J = I - n; ierr = MatSetValues(A,1,&I,1,&J,&v,INSERT_VALUES);CHKERRQ(ierr);}
      if (i<m-1) {J = I + n; ierr = MatSetValues(A,1,&I,1,&J,&v,INSERT_VALUES);CHKERRQ(ierr);}
      if (j>0)   {J = I - 1; ierr = MatSetValues(A,1,&I,1,&J,&v,INSERT_VALUES);CHKERRQ(ierr);}
      if (j<n+size-1) {J = I + 1; ierr = MatSetValues(A,1,&I,1,&J,&v,INSERT_VALUES);CHKERRQ(ierr);}
      v = 4.0; ierr = MatSetValues(A,1,&I,1,&I,&v,INSERT_VALUES);CHKERRQ(ierr);
    }
  }
  ierr = MatAssemblyBegin(A,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(A,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);

  ierr = TestMatZeroRows_Basic(A,is,PETSC_NULL);CHKERRQ(ierr);
  ierr = TestMatZeroRows_Basic(A,is,&diag);CHKERRQ(ierr);

  ierr = MatDestroy(A);CHKERRQ(ierr);
  ierr = ISDestroy(is);CHKERRQ(ierr); 
  ierr = PetscFinalize();CHKERRQ(ierr);
  return 0;
}

#undef __FUNCT__
#define __FUNCT__ "TestMatZeroRows_Basic"
PetscErrorCode TestMatZeroRows_Basic(Mat A,IS is,const PetscScalar diag[])
{
  Mat            B;
  PetscErrorCode ierr;
  PetscTruth     keepzeroedrows;

  /* Now copy A into B, and test it with MatZeroRows() */
  ierr = MatDuplicate(A,MAT_COPY_VALUES,&B);CHKERRQ(ierr);

  ierr = PetscOptionsHasName(PETSC_NULL,"-keep_zeroed_rows",&keepzeroedrows);CHKERRQ(ierr);
  if (keepzeroedrows) {
    ierr = MatSetOption(B,MAT_KEEP_ZEROED_ROWS);CHKERRQ(ierr);
  }

  ierr = MatZeroRows(B,is,diag);CHKERRQ(ierr);
  ierr = MatView(B,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr); 
  ierr = MatDestroy(B);CHKERRQ(ierr);
  return 0;
}

#undef __FUNCT__
#define __FUNCT__ "TestMatZeroRows_with_no_allocation"
PetscErrorCode TestMatZeroRows_with_no_allocation(Mat A,IS is,const PetscScalar diag[])
{
  Mat            B;
  PetscErrorCode ierr;

  /* Now copy A into B, and test it with MatZeroRows() */
  ierr = MatDuplicate(A,MAT_COPY_VALUES,&B);CHKERRQ(ierr);
  /* Set this flag after assembly. This way, it affects only MatZeroRows() */
  ierr = MatSetOption(B,MAT_NEW_NONZERO_ALLOCATION_ERR);CHKERRQ(ierr);

  ierr = MatZeroRows(B,is,diag);CHKERRQ(ierr);
  ierr = MatView(B,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr); 
  ierr = MatDestroy(B);CHKERRQ(ierr);
  return 0;
}
