/*$Id: ex17.c,v 1.21 2001/08/07 21:30:08 bsmith Exp $*/

static char help[] = "Tests the use of MatSolveTranspose().\n\n";

#include "petscmat.h"

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **args)
{
  Mat          C,A;
  int          i,j,m = 5,n = 5,I,J,ierr;
  PetscScalar  v,five = 5.0,one = 1.0,mone = -1.0;
  IS           isrow,row,col;
  Vec          x,u,b;
  PetscReal    norm;

  PetscInitialize(&argc,&args,(char *)0,help);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-m",&m,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-n",&n,PETSC_NULL);CHKERRQ(ierr);

  ierr = MatCreateSeqAIJ(PETSC_COMM_SELF,m*n,m*n,5,PETSC_NULL,&C);CHKERRQ(ierr);

  /* create the matrix for the five point stencil, YET AGAIN*/
  for (i=0; i<m; i++) {
    for (j=0; j<n; j++) {
      v = -1.0;  I = j + n*i;
      if (i>0)   {J = I - n; ierr = MatSetValues(C,1,&I,1,&J,&v,INSERT_VALUES);CHKERRQ(ierr);}
      if (i<m-1) {J = I + n; ierr = MatSetValues(C,1,&I,1,&J,&v,INSERT_VALUES);CHKERRQ(ierr);}
      if (j>0)   {J = I - 1; ierr = MatSetValues(C,1,&I,1,&J,&v,INSERT_VALUES);CHKERRQ(ierr);}
      if (j<n-1) {J = I + 1; ierr = MatSetValues(C,1,&I,1,&J,&v,INSERT_VALUES);CHKERRQ(ierr);}
      v = 4.0; ierr = MatSetValues(C,1,&I,1,&I,&v,INSERT_VALUES);CHKERRQ(ierr);
    }
  }
  ierr = MatAssemblyBegin(C,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(C,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);

  ierr = ISCreateStride(PETSC_COMM_SELF,(m*n)/2,0,2,&isrow);CHKERRQ(ierr);
  ierr = MatZeroRows(C,isrow,&five);CHKERRQ(ierr);

  ierr = VecCreateSeq(PETSC_COMM_SELF,m*n,&u);CHKERRQ(ierr);
  ierr = VecDuplicate(u,&x);CHKERRQ(ierr);
  ierr = VecDuplicate(u,&b);CHKERRQ(ierr);
  ierr = VecSet(&one,u);CHKERRQ(ierr);

  ierr = MatMultTranspose(C,u,b);CHKERRQ(ierr);

  /* Set default ordering to be Quotient Minimum Degree; also read
     orderings from the options database */
  ierr = MatGetOrdering(C,MATORDERING_QMD,&row,&col);CHKERRQ(ierr);

  ierr = MatLUFactorSymbolic(C,row,col,PETSC_NULL,&A);CHKERRQ(ierr);
  ierr = MatLUFactorNumeric(C,&A);CHKERRQ(ierr);
  ierr = MatSolveTranspose(A,b,x);CHKERRQ(ierr);

  ierr = ISView(row,PETSC_VIEWER_STDOUT_SELF);CHKERRQ(ierr);
  ierr = VecAXPY(&mone,u,x);CHKERRQ(ierr);
  ierr = VecNorm(x,NORM_2,&norm);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_SELF,"Norm of error %g\n",norm);CHKERRQ(ierr);

  ierr = ISDestroy(row);CHKERRQ(ierr);
  ierr = ISDestroy(col);CHKERRQ(ierr);
  ierr = ISDestroy(isrow);CHKERRQ(ierr);
  ierr = VecDestroy(u);CHKERRQ(ierr);
  ierr = VecDestroy(x);CHKERRQ(ierr);
  ierr = VecDestroy(b);CHKERRQ(ierr);
  ierr = MatDestroy(C);CHKERRQ(ierr);
  ierr = MatDestroy(A);CHKERRQ(ierr);
  ierr = PetscFinalize();CHKERRQ(ierr);
  return 0;
}
