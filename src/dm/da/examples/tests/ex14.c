/*$Id: ex14.c,v 1.17 2001/08/07 03:04:42 balay Exp $*/

static char help[] = "Tests saving DA vectors to files.\n\n";

#include "petscda.h"
#include "petscsys.h"

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **argv)
{
  int         rank,M = 10,N = 8,m = PETSC_DECIDE,n = PETSC_DECIDE,ierr;
  int         dof = 1;
  DA          da;
  Vec         local,global,natural;
  PetscScalar value;
  PetscViewer bviewer;

  ierr = PetscInitialize(&argc,&argv,(char*)0,help);CHKERRQ(ierr); 

  /* Read options */
  ierr = PetscOptionsGetInt(PETSC_NULL,"-M",&M,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-N",&N,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-m",&m,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-n",&n,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-dof",&dof,PETSC_NULL);CHKERRQ(ierr);

  /* Create distributed array and get vectors */
  ierr = DACreate2d(PETSC_COMM_WORLD,DA_NONPERIODIC,DA_STENCIL_BOX,
                    M,N,m,n,dof,1,PETSC_NULL,PETSC_NULL,&da);CHKERRQ(ierr);
  ierr = DACreateGlobalVector(da,&global);CHKERRQ(ierr);
  ierr = DACreateLocalVector(da,&local);CHKERRQ(ierr);

  value = -3.0;
  ierr = VecSet(&value,global);CHKERRQ(ierr);
  ierr = DAGlobalToLocalBegin(da,global,INSERT_VALUES,local);CHKERRQ(ierr);
  ierr = DAGlobalToLocalEnd(da,global,INSERT_VALUES,local);CHKERRQ(ierr);

  ierr = MPI_Comm_rank(PETSC_COMM_WORLD,&rank);CHKERRQ(ierr);
  value = rank+1;
  ierr = VecScale(&value,local);CHKERRQ(ierr);
  ierr = DALocalToGlobal(da,local,ADD_VALUES,global);CHKERRQ(ierr);

  ierr = DACreateNaturalVector(da,&natural);CHKERRQ(ierr);
  ierr = DAGlobalToNaturalBegin(da,global,INSERT_VALUES,natural);CHKERRQ(ierr);
  ierr = DAGlobalToNaturalEnd(da,global,INSERT_VALUES,natural);CHKERRQ(ierr);

  ierr = DASetFieldName(da,0,"First field");CHKERRQ(ierr);
  ierr = VecView(global,PETSC_VIEWER_DRAW_WORLD);CHKERRQ(ierr); 

  ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,"daoutput",PETSC_BINARY_CREATE,&bviewer);CHKERRQ(ierr);
  ierr = DAView(da,bviewer);CHKERRQ(ierr);
  ierr = VecView(global,bviewer);CHKERRQ(ierr);
  ierr = PetscViewerDestroy(bviewer);CHKERRQ(ierr);

  /* Free memory */
  ierr = VecDestroy(local);CHKERRQ(ierr);
  ierr = VecDestroy(global);CHKERRQ(ierr);
  ierr = VecDestroy(natural);CHKERRQ(ierr);
  ierr = DADestroy(da);CHKERRQ(ierr);
  ierr = PetscFinalize();CHKERRQ(ierr);
  return 0;
}
 
