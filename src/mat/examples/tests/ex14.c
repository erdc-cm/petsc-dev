
static char help[] = "Tests MatGetRow() and MatRestoreRow().\n";

#include "petscmat.h"

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **args)
{
  Mat    C; 
  int    i,j,m = 5,n = 5,I,J,ierr,*idx,nz;
  PetscScalar v,*values;

  PetscInitialize(&argc,&args,(char *)0,help);

  /* Create the matrix for the five point stencil, YET AGAIN */
  ierr = MatCreate(PETSC_COMM_SELF,PETSC_DECIDE,PETSC_DECIDE,m*n,m*n,&C);CHKERRQ(ierr);
  ierr = MatSetFromOptions(C);CHKERRQ(ierr);
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
  ierr = MatView(C,PETSC_VIEWER_STDOUT_SELF);CHKERRQ(ierr);

  for (i=0; i<m*n; i++) {
    ierr = MatGetRow(C,i,&nz,&idx,&values);CHKERRQ(ierr);
#if defined(PETSC_USE_COMPLEX)
    for (j=0; j<nz; j++) {ierr = PetscPrintf(PETSC_COMM_SELF,"%d %g ",idx[j],PetscRealPart(values[j]));CHKERRQ(ierr);}
#else
    for (j=0; j<nz; j++) {ierr = PetscPrintf(PETSC_COMM_SELF,"%d %g ",idx[j],values[j]);CHKERRQ(ierr);}
#endif
    ierr = PetscPrintf(PETSC_COMM_SELF,"\n");CHKERRQ(ierr);
    ierr = MatRestoreRow(C,i,&nz,&idx,&values);CHKERRQ(ierr);
  }

  ierr = MatDestroy(C);CHKERRQ(ierr);
  ierr = PetscFinalize();CHKERRQ(ierr);
  return 0;
}
