
static char help[] = "Tests various 1-dimensional DA routines.\n\n";

#include "petscda.h"
#include "petscsys.h"

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **argv)
{
  PetscMPIInt    rank;
  PetscInt       M = 13,w=1,s=1,wrap=1;
  PetscErrorCode ierr;
  DA             da;
  PetscViewer    viewer;
  Vec            local,global;
  PetscScalar    value;
  PetscDraw      draw;
  PetscTruth     flg;

  ierr = PetscInitialize(&argc,&argv,(char*)0,help);CHKERRQ(ierr); 

  /* Create viewers */
  ierr = PetscViewerDrawOpen(PETSC_COMM_WORLD,0,"",280,480,600,200,&viewer);CHKERRQ(ierr);
  ierr = PetscViewerDrawGetDraw(viewer,0,&draw);CHKERRQ(ierr);
  ierr = PetscDrawSetDoubleBuffer(draw);CHKERRQ(ierr);

  /* Readoptions */
  ierr = PetscOptionsGetInt(PETSC_NULL,"-M",&M,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-w",&w,PETSC_NULL);CHKERRQ(ierr); 
  ierr = PetscOptionsGetInt(PETSC_NULL,"-s",&s,PETSC_NULL);CHKERRQ(ierr); 

  /* Create distributed array and get vectors */
  ierr = DACreate1d(PETSC_COMM_WORLD,(DAPeriodicType)wrap,M,w,s,PETSC_NULL,&da);CHKERRQ(ierr);
  ierr = DAView(da,viewer);CHKERRQ(ierr);
  ierr = DACreateGlobalVector(da,&global);CHKERRQ(ierr);
  ierr = DACreateLocalVector(da,&local);CHKERRQ(ierr);

  /* Set global vector; send ghost points to local vectors */
  value = 1;
  ierr = VecSet(&value,global);CHKERRQ(ierr);
  ierr = DAGlobalToLocalBegin(da,global,INSERT_VALUES,local);CHKERRQ(ierr);
  ierr = DAGlobalToLocalEnd(da,global,INSERT_VALUES,local);CHKERRQ(ierr);

  /* Scale local vectors according to processor rank; pass to global vector */
  ierr = MPI_Comm_rank(PETSC_COMM_WORLD,&rank);CHKERRQ(ierr);
  value = rank+1;
  ierr = VecScale(&value,local);CHKERRQ(ierr);
  ierr = DALocalToGlobal(da,local,INSERT_VALUES,global);CHKERRQ(ierr);

  ierr = VecView(global,viewer);CHKERRQ(ierr); 
  ierr = PetscPrintf(PETSC_COMM_WORLD,"\nGlobal Vector:\n");CHKERRQ(ierr);
  ierr = VecView(global,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr); 
  ierr = PetscPrintf(PETSC_COMM_WORLD,"\n");CHKERRQ(ierr);

  /* Send ghost points to local vectors */
  ierr = DAGlobalToLocalBegin(da,global,INSERT_VALUES,local);CHKERRQ(ierr);
  ierr = DAGlobalToLocalEnd(da,global,INSERT_VALUES,local);CHKERRQ(ierr);

  ierr = PetscOptionsHasName(PETSC_NULL,"-local_print",&flg);CHKERRQ(ierr);
  if (flg) {
    PetscViewer sviewer;
    ierr = PetscSynchronizedPrintf(PETSC_COMM_WORLD,"\nLocal Vector: processor %d\n",rank);CHKERRQ(ierr);
    ierr = PetscViewerGetSingleton(PETSC_VIEWER_STDOUT_WORLD,&sviewer);CHKERRQ(ierr);
    ierr = VecView(local,sviewer);CHKERRQ(ierr); 
    ierr = PetscViewerRestoreSingleton(PETSC_VIEWER_STDOUT_WORLD,&sviewer);CHKERRQ(ierr);
    ierr = PetscSynchronizedFlush(PETSC_COMM_WORLD);CHKERRQ(ierr);
  }

  /* Free memory */
  ierr = PetscViewerDestroy(viewer);CHKERRQ(ierr);
  ierr = VecDestroy(global);CHKERRQ(ierr);
  ierr = VecDestroy(local);CHKERRQ(ierr);
  ierr = DADestroy(da);CHKERRQ(ierr);
  ierr = PetscFinalize();CHKERRQ(ierr);
  return 0;
}
 









