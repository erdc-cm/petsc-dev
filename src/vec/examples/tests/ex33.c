/*$Id: ex33.c,v 1.1 2001/09/24 21:04:45 balay Exp $*/

static char help[] = "Tests the routines VecConvertMPIToSeqAll, VecConvertMPIToMPIZero\n\n";

#include "petscvec.h"

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **argv)
{
  int           start,end;
  int           n = 3,ierr,size,rank,i,len;
  PetscScalar   value,*yy;
  Vec           x,y,z,y_t;

  ierr = PetscInitialize(&argc,&argv,(char*)0,help);CHKERRQ(ierr); 
  ierr = MPI_Comm_size(PETSC_COMM_WORLD,&size);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(PETSC_COMM_WORLD,&rank);CHKERRQ(ierr);

  /* create two vectors */
  ierr = VecCreateMPI(PETSC_COMM_WORLD,PETSC_DECIDE,size*n,&x);CHKERRQ(ierr);

  /* each processor inserts its values */

  ierr = VecGetOwnershipRange(x,&start,&end);CHKERRQ(ierr);
  for (i=start; i<end; i++) {
    value = (PetscScalar) i;
    ierr = VecSetValues(x,1,&i,&value,INSERT_VALUES);CHKERRQ(ierr);
  }
  ierr = VecAssemblyBegin(x);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(x);CHKERRQ(ierr);
  ierr = VecView(x,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);

  ierr = VecConvertMPIToSeqAll(x,&y);CHKERRQ(ierr);

  /* Cannot view the above vector with VecView(), so place it in an MPI Vec
     and do a VecView() */
  ierr = VecGetArray(y,&yy);CHKERRQ(ierr);
  ierr = VecGetLocalSize(y,&len);CHKERRQ(ierr);
  ierr = VecCreateMPIWithArray(PETSC_COMM_WORLD,len,PETSC_DECIDE,yy,&y_t);CHKERRQ(ierr);
  ierr = VecView(y_t,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
  ierr = VecDestroy(y_t);
  ierr = VecRestoreArray(y,&yy);CHKERRQ(ierr);

  ierr = VecConvertMPIToMPIZero(x,&z);CHKERRQ(ierr);
  ierr = VecView(z,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);


  ierr = VecDestroy(x);CHKERRQ(ierr);
  ierr = VecDestroy(y);CHKERRQ(ierr);
  ierr = VecDestroy(z);CHKERRQ(ierr);

  ierr = PetscFinalize();CHKERRQ(ierr);
  return 0;
}
 
