/*$Id: ex15.c,v 1.22 2001/08/07 21:30:08 bsmith Exp $*/

static char help[] = "Tests MatNorm(), MatLUFactor(), MatSolve() and MatSolveAdd().\n\n";

#include "petscmat.h"

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **args)
{
  Mat         C; 
  int         i,j,m = 3,n = 3,I,J,ierr;
  PetscTruth  flg;
  PetscScalar v,mone = -1.0,one = 1.0,alpha = 2.0;
  IS          perm,iperm;
  Vec         x,u,b,y;
  PetscReal   norm;

  PetscInitialize(&argc,&args,(char *)0,help);

  ierr = MatCreate(PETSC_COMM_WORLD,PETSC_DECIDE,PETSC_DECIDE,m*n,m*n,&C);CHKERRQ(ierr);
  ierr = MatSetFromOptions(C);CHKERRQ(ierr);
  ierr = PetscOptionsHasName(PETSC_NULL,"-symmetric",&flg);CHKERRQ(ierr);
  if (flg) {  /* Treat matrix as symmetric only if we set this flag */
    ierr = MatSetOption(C,MAT_SYMMETRIC);CHKERRQ(ierr);
  }

  /* Create the matrix for the five point stencil, YET AGAIN */
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
  ierr = MatGetOrdering(C,MATORDERING_RCM,&perm,&iperm);CHKERRQ(ierr);
  ierr = MatView(C,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
  ierr = ISView(perm,PETSC_VIEWER_STDOUT_SELF);CHKERRQ(ierr);
  ierr = VecCreateSeq(PETSC_COMM_SELF,m*n,&u);CHKERRQ(ierr);
  ierr = VecSet(&one,u);CHKERRQ(ierr);
  ierr = VecDuplicate(u,&x);CHKERRQ(ierr);
  ierr = VecDuplicate(u,&b);CHKERRQ(ierr);
  ierr = VecDuplicate(u,&y);CHKERRQ(ierr);
  ierr = MatMult(C,u,b);CHKERRQ(ierr);
  ierr = VecCopy(b,y);CHKERRQ(ierr);
  ierr = VecScale(&alpha,y);CHKERRQ(ierr);

  ierr = MatNorm(C,NORM_FROBENIUS,&norm);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_SELF,"Frobenius norm of matrix %g\n",norm);CHKERRQ(ierr);
  ierr = MatNorm(C,NORM_1,&norm);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_SELF,"One  norm of matrix %g\n",norm);CHKERRQ(ierr);
  ierr = MatNorm(C,NORM_INFINITY,&norm);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_SELF,"Infinity norm of matrix %g\n",norm);CHKERRQ(ierr);

  ierr = MatLUFactor(C,perm,iperm,PETSC_NULL);CHKERRQ(ierr);
  ierr = MatView(C,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);

  /* Test MatSolve */
  ierr = MatSolve(C,b,x);CHKERRQ(ierr);
  ierr = VecView(b,PETSC_VIEWER_STDOUT_SELF);CHKERRQ(ierr);
  ierr = VecView(x,PETSC_VIEWER_STDOUT_SELF);CHKERRQ(ierr);
  ierr = VecAXPY(&mone,u,x);CHKERRQ(ierr);
  ierr = VecNorm(x,NORM_2,&norm);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_SELF,"Norm of error %A\n",norm);CHKERRQ(ierr);

  /* Test MatSolveAdd */
  ierr = MatSolveAdd(C,b,y,x);CHKERRQ(ierr);

  ierr = VecAXPY(&mone,y,x);CHKERRQ(ierr);
  ierr = VecAXPY(&mone,u,x);CHKERRQ(ierr);
  ierr = VecNorm(x,NORM_2,&norm);CHKERRQ(ierr);

  ierr = PetscPrintf(PETSC_COMM_SELF,"Norm of error %A\n",norm);CHKERRQ(ierr);

  ierr = ISDestroy(perm);CHKERRQ(ierr);
  ierr = ISDestroy(iperm);CHKERRQ(ierr);
  ierr = VecDestroy(u);CHKERRQ(ierr);
  ierr = VecDestroy(y);CHKERRQ(ierr);
  ierr = VecDestroy(b);CHKERRQ(ierr);
  ierr = VecDestroy(x);CHKERRQ(ierr);
  ierr = MatDestroy(C);CHKERRQ(ierr);
  ierr = PetscFinalize();CHKERRQ(ierr);
  return 0;
}
