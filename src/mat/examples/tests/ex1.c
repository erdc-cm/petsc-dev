/*$Id: ex1.c,v 1.23 2001/08/07 21:30:08 bsmith Exp $*/

static char help[] = "Tests LU and Cholesky factorization for a dense matrix.\n\n";

#include "petscmat.h"

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **argv)
{
  Mat          mat,fact;
  MatInfo      info;
  int          m = 10,n = 10,i = 4,ierr,rstart,rend;
  PetscScalar  value = 1.0;
  Vec          x,y,b;
  PetscReal    norm;
  IS           perm;
  MatFactorInfo luinfo,factinfo;

  PetscInitialize(&argc,&argv,(char*) 0,help);

  ierr = VecCreate(PETSC_COMM_WORLD,&y);CHKERRQ(ierr); 
  ierr = VecSetSizes(y,PETSC_DECIDE,m);CHKERRQ(ierr); 
  ierr = VecSetFromOptions(y);CHKERRQ(ierr);
  ierr = VecDuplicate(y,&x);CHKERRQ(ierr);
  ierr = VecSet(&value,x);CHKERRQ(ierr);
  ierr = VecCreate(PETSC_COMM_WORLD,&b);CHKERRQ(ierr);
  ierr = VecSetSizes(b,PETSC_DECIDE,n);CHKERRQ(ierr);
  ierr = VecSetFromOptions(b);CHKERRQ(ierr);
  ierr = ISCreateStride(PETSC_COMM_WORLD,m,0,1,&perm);CHKERRQ(ierr);

  ierr = MatCreateSeqDense(PETSC_COMM_WORLD,m,n,PETSC_NULL,&mat);CHKERRQ(ierr);

  ierr = MatGetOwnershipRange(mat,&rstart,&rend);CHKERRQ(ierr);
  for (i=rstart; i<rend; i++) {
    value = (PetscReal)i+1;
    ierr = MatSetValues(mat,1,&i,1,&i,&value,INSERT_VALUES);CHKERRQ(ierr);
  }
  ierr = MatAssemblyBegin(mat,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(mat,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);

  ierr = MatGetInfo(mat,MAT_LOCAL,&info);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"matrix nonzeros = %d, allocated nonzeros = %d\n",
    (int)info.nz_used,(int)info.nz_allocated);CHKERRQ(ierr);

  /* Cholesky factorization is not yet in place for this matrix format */
  factinfo.fill = 1.0;
  ierr = MatMult(mat,x,b);CHKERRQ(ierr);
  ierr = MatConvert(mat,MATSAME,&fact);CHKERRQ(ierr);
  ierr = MatCholeskyFactor(fact,perm,&factinfo);CHKERRQ(ierr);
  ierr = MatSolve(fact,b,y);CHKERRQ(ierr);
  ierr = MatDestroy(fact);CHKERRQ(ierr);
  value = -1.0; ierr = VecAXPY(&value,x,y);CHKERRQ(ierr);
  ierr = VecNorm(y,NORM_2,&norm);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Norm of error for Cholesky %A\n",norm);CHKERRQ(ierr);
  ierr = MatCholeskyFactorSymbolic(mat,perm,&factinfo,&fact);CHKERRQ(ierr);
  ierr = MatCholeskyFactorNumeric(mat,&fact);CHKERRQ(ierr);
  ierr = MatSolve(fact,b,y);CHKERRQ(ierr);
  value = -1.0; ierr = VecAXPY(&value,x,y);CHKERRQ(ierr);
  ierr = VecNorm(y,NORM_2,&norm);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Norm of error for Cholesky %A\n",norm);CHKERRQ(ierr);
  ierr = MatDestroy(fact);CHKERRQ(ierr);

  i = m-1; value = 1.0;
  ierr = MatSetValues(mat,1,&i,1,&i,&value,INSERT_VALUES);CHKERRQ(ierr);
  ierr = MatAssemblyBegin(mat,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(mat,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatMult(mat,x,b);CHKERRQ(ierr);
  ierr = MatConvert(mat,MATSAME,&fact);CHKERRQ(ierr);
  luinfo.fill           = 5.0;
  luinfo.dtcol          = 1.e-6; /* default to pivoting; this is only thing PETSc LU supports */
  luinfo.damping        = 0.0;
  luinfo.zeropivot      = 1.e-12;
  luinfo.pivotinblocks  = 1.0;
  luinfo.shift          = PETSC_FALSE;
  luinfo.shift_fraction = 0.0;
  ierr = MatLUFactor(fact,perm,perm,&luinfo);CHKERRQ(ierr);
  ierr = MatSolve(fact,b,y);CHKERRQ(ierr);
  value = -1.0; ierr = VecAXPY(&value,x,y);CHKERRQ(ierr);
  ierr = VecNorm(y,NORM_2,&norm);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Norm of error for LU %A\n",norm);CHKERRQ(ierr);
  ierr = MatDestroy(fact);CHKERRQ(ierr);

  luinfo.fill = 2.0;
  luinfo.dtcol = 0.0; 
  luinfo.damping = 0.0; 
  luinfo.zeropivot = 1.e-14; 
  luinfo.pivotinblocks = 1.0; 
  ierr = MatLUFactorSymbolic(mat,perm,perm,&luinfo,&fact);CHKERRQ(ierr);
  ierr = MatLUFactorNumeric(mat,&fact);CHKERRQ(ierr);
  ierr = MatSolve(fact,b,y);CHKERRQ(ierr);
  value = -1.0; ierr = VecAXPY(&value,x,y);CHKERRQ(ierr);
  ierr = VecNorm(y,NORM_2,&norm);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Norm of error for LU %A\n",norm);CHKERRQ(ierr);  
  ierr = MatDestroy(fact);CHKERRQ(ierr);
  ierr = MatDestroy(mat);CHKERRQ(ierr);
  ierr = VecDestroy(x);CHKERRQ(ierr);
  ierr = VecDestroy(b);CHKERRQ(ierr);
  ierr = VecDestroy(y);CHKERRQ(ierr);
  ierr = ISDestroy(perm);CHKERRQ(ierr);

  ierr = PetscFinalize();CHKERRQ(ierr);
  return 0;
}
 
