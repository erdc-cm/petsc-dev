/*$Id: ex5.c,v 1.50 2001/08/07 03:02:34 balay Exp $*/

static char help[] = "Tests binary I/O of vectors and illustrates the use of user-defined event logging.\n\n";

#include "petscvec.h"

/* Note:  Most applications would not read and write a vector within
  the same program.  This example is intended only to demonstrate
  both input and output. */

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **args)
{
  int          i,m = 10,rank,size,low,high,ldim,iglobal,ierr;
  PetscScalar  v;
  Vec          u;
  PetscViewer  viewer;
  int          VECTOR_GENERATE,VECTOR_READ;

  PetscInitialize(&argc,&args,(char *)0,help);
  ierr = MPI_Comm_rank(PETSC_COMM_WORLD,&rank);CHKERRQ(ierr);
  ierr = MPI_Comm_size(PETSC_COMM_WORLD,&size);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-m",&m,PETSC_NULL);CHKERRQ(ierr);

  /* PART 1:  Generate vector, then write it in binary format */

  ierr = PetscLogEventRegister(&VECTOR_GENERATE,"Generate Vector",VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventBegin(VECTOR_GENERATE,0,0,0,0);CHKERRQ(ierr);
  /* Generate vector */
  ierr = VecCreate(PETSC_COMM_WORLD,&u);CHKERRQ(ierr);
  ierr = VecSetSizes(u,PETSC_DECIDE,m);CHKERRQ(ierr);
  ierr = VecSetFromOptions(u);CHKERRQ(ierr);
  ierr = VecGetOwnershipRange(u,&low,&high);CHKERRQ(ierr);
  ierr = VecGetLocalSize(u,&ldim);CHKERRQ(ierr);
  for (i=0; i<ldim; i++) {
    iglobal = i + low;
    v = (PetscScalar)(i + 100*rank);
    ierr = VecSetValues(u,1,&iglobal,&v,INSERT_VALUES);CHKERRQ(ierr);
  }
  ierr = VecAssemblyBegin(u);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(u);CHKERRQ(ierr);
  ierr = VecView(u,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);

  ierr = PetscPrintf(PETSC_COMM_WORLD,"writing vector in binary to vector.dat ...\n");CHKERRQ(ierr);

  ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,"vector.dat",PETSC_FILE_CREATE,&viewer);CHKERRQ(ierr);
  ierr = VecView(u,viewer);CHKERRQ(ierr);
  ierr = PetscViewerDestroy(viewer);CHKERRQ(ierr);
  ierr = VecDestroy(u);CHKERRQ(ierr);
  ierr = PetscLogEventEnd(VECTOR_GENERATE,0,0,0,0);CHKERRQ(ierr);

  /* PART 2:  Read in vector in binary format */

  /* All processors wait until test vector has been dumped */
  ierr = MPI_Barrier(PETSC_COMM_WORLD);CHKERRQ(ierr);
  ierr = PetscSleep(10);CHKERRQ(ierr);

  /* Read new vector in binary format */
  ierr = PetscLogEventRegister(&VECTOR_READ,"Read Vector",VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventBegin(VECTOR_READ,0,0,0,0);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"reading vector in binary from vector.dat ...\n");CHKERRQ(ierr);
  ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,"vector.dat",PETSC_FILE_RDONLY,&viewer);CHKERRQ(ierr);
  ierr = VecLoad(viewer,&u);CHKERRQ(ierr);
  ierr = PetscViewerDestroy(viewer);CHKERRQ(ierr);
  ierr = PetscLogEventEnd(VECTOR_READ,0,0,0,0);CHKERRQ(ierr);
  ierr = VecView(u,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);

  /* Free data structures */
  ierr = VecDestroy(u);CHKERRQ(ierr);
  ierr = PetscFinalize();CHKERRQ(ierr);
  return 0;
}

