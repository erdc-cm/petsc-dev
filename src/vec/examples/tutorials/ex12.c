/*$Id: ex12.c,v 1.18 2001/09/11 16:32:18 bsmith Exp $*/

/* Program usage:  mpirun ex1 [-help] [all PETSc options] */

static char help[] = "Demonstrates VecStrideScatter() and VecStrideGather().\n\n";

/*T
   Concepts: vectors^sub-vectors;
   Processors: n
T*/

/* 
  Include "petscvec.h" so that we can use vectors.  Note that this file
  automatically includes:
     petsc.h       - base PETSc routines   petscis.h     - index sets
     petscsys.h    - system routines       petscviewer.h - viewers
*/

#include "petscvec.h"

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **argv)
{
  Vec      v,s;               /* vectors */
  int      n = 20,ierr;
  PetscScalar   one = 1.0;

  ierr = PetscInitialize(&argc,&argv,(char*)0,help);CHKERRQ(ierr); 
  ierr = PetscOptionsGetInt(PETSC_NULL,"-n",&n,PETSC_NULL);CHKERRQ(ierr);

  /* 
      Create multi-component vector with 2 components
  */
  ierr = VecCreate(PETSC_COMM_WORLD,PETSC_DECIDE,n,&v);CHKERRQ(ierr);
  ierr = VecSetBlockSize(v,2);CHKERRQ(ierr);
  ierr = VecSetFromOptions(v);CHKERRQ(ierr);

  /* 
      Create single-component vector
  */
  ierr = VecCreate(PETSC_COMM_WORLD,PETSC_DECIDE,n/2,&s);CHKERRQ(ierr);
  ierr = VecSetFromOptions(s);CHKERRQ(ierr);

  /*
     Set the vectors to entries to a constant value.
  */
  ierr = VecSet(&one,v);CHKERRQ(ierr);

  /*
     Get the first component from the multi-component vector to the single vector
  */
  ierr = VecStrideGather(v,0,s,INSERT_VALUES);CHKERRQ(ierr);

  ierr = VecView(s,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);

  /*
     Put the values back into the second component 
  */
  ierr = VecStrideScatter(s,1,v,ADD_VALUES);CHKERRQ(ierr);

  ierr = VecView(v,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);

  /* 
     Free work space.  All PETSc objects should be destroyed when they
     are no longer needed.
  */
  ierr = VecDestroy(v);CHKERRQ(ierr);
  ierr = VecDestroy(s);CHKERRQ(ierr);
  ierr = PetscFinalize();CHKERRQ(ierr);
  return 0;
}
 
