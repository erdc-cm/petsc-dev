/*$Id: ex46.c,v 1.17 2001/08/07 03:03:07 balay Exp $*/

static char help[] = "Tests generating a nonsymmetric BlockSolve95 (MATMPIROWBS) matrix.\n\n";

#include "petscmat.h"

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **args)
{
  Mat     C,A;
  PetscScalar  v;
  int     i,j,I,J,ierr,Istart,Iend,N,m = 4,n = 4,rank,size;

  PetscInitialize(&argc,&args,0,help);
  ierr = MPI_Comm_rank(PETSC_COMM_WORLD,&rank);CHKERRQ(ierr);
  ierr = MPI_Comm_size(PETSC_COMM_WORLD,&size);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-m",&m,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-n",&n,PETSC_NULL);CHKERRQ(ierr);
  N = m*n;

  /* Generate matrix */
  ierr = MatCreateMPIRowbs(PETSC_COMM_WORLD,PETSC_DECIDE,N,0,0,&C);CHKERRQ(ierr);
  ierr = MatGetOwnershipRange(C,&Istart,&Iend);CHKERRQ(ierr);
  for (I=Istart; I<Iend; I++) { 
    v = -1.0; i = I/n; j = I - i*n;  
    if (i >  0)  {J = I - n; ierr = MatSetValues(C,1,&I,1,&J,&v,INSERT_VALUES);CHKERRQ(ierr);}
    if (j >  0)  {J = I - 1; ierr = MatSetValues(C,1,&I,1,&J,&v,INSERT_VALUES);CHKERRQ(ierr);}
    if (I != 8) {v = 4.0; ierr = MatSetValues(C,1,&I,1,&I,&v,INSERT_VALUES);CHKERRQ(ierr);}
  }
  ierr = MatAssemblyBegin(C,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(C,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);

  ierr = MatConvert(C,MATMPIAIJ,&A);CHKERRQ(ierr);
  ierr = MatDestroy(C);CHKERRQ(ierr);
  ierr = MatDestroy(A);CHKERRQ(ierr);
  ierr = PetscFinalize();CHKERRQ(ierr);
  return 0;
}


