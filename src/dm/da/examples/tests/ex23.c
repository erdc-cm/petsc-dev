
static char help[] = "Tests VecView()/VecLoad() for DA vectors (this tests DAGlobalToNatural()).\n\n";

#include "petscda.h"
#include "petscsys.h"

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **argv)
{
  PetscMPIInt    size;
  PetscInt       N = 6,m=PETSC_DECIDE,n=PETSC_DECIDE,p=PETSC_DECIDE,M=8,dof=1,stencil_width=1,P=5,pt = 0,st = 0;
  PetscErrorCode ierr;
  PetscTruth     flg2,flg3;
  DAPeriodicType periodic = DA_NONPERIODIC;
  DAStencilType  stencil_type = DA_STENCIL_STAR;
  DA             da;
  Vec            global1,global2;
  PetscScalar    mone = -1.0;
  PetscReal      norm;
  PetscViewer    viewer;
  PetscRandom    rdm;

  ierr = PetscInitialize(&argc,&argv,(char*)0,help);CHKERRQ(ierr); 

  ierr = PetscOptionsGetInt(PETSC_NULL,"-M",&M,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-N",&N,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-P",&P,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-dof",&dof,PETSC_NULL);CHKERRQ(ierr); 
  ierr = PetscOptionsGetInt(PETSC_NULL,"-stencil_width",&stencil_width,PETSC_NULL);CHKERRQ(ierr); 
  ierr = PetscOptionsGetInt(PETSC_NULL,"-periodic",&pt,PETSC_NULL);CHKERRQ(ierr); 
  periodic = (DAPeriodicType) pt;
  ierr = PetscOptionsGetInt(PETSC_NULL,"-stencil_type",&st,PETSC_NULL);CHKERRQ(ierr); 
  stencil_type = (DAStencilType) st;

  ierr = PetscOptionsHasName(PETSC_NULL,"-1d",&flg2);CHKERRQ(ierr);
  ierr = PetscOptionsHasName(PETSC_NULL,"-2d",&flg2);CHKERRQ(ierr);
  ierr = PetscOptionsHasName(PETSC_NULL,"-3d",&flg3);CHKERRQ(ierr);
  if (flg2) {
    ierr = DACreate2d(PETSC_COMM_WORLD,periodic,stencil_type,M,N,m,n,dof,stencil_width,0,0,&da);CHKERRQ(ierr);
  } else if (flg3) {
    ierr = DACreate3d(PETSC_COMM_WORLD,periodic,stencil_type,M,N,P,m,n,p,dof,stencil_width,0,0,0,&da);CHKERRQ(ierr);
  }
  else {
    ierr = DACreate1d(PETSC_COMM_WORLD,periodic,M,dof,stencil_width,PETSC_NULL,&da);CHKERRQ(ierr);
  }

  ierr = DACreateGlobalVector(da,&global1);CHKERRQ(ierr);
  ierr = PetscRandomCreate(PETSC_COMM_WORLD,RANDOM_DEFAULT,&rdm);CHKERRQ(ierr);
  ierr = VecSetRandom(rdm,global1);CHKERRQ(ierr);
  ierr = PetscRandomDestroy(rdm);CHKERRQ(ierr);

  ierr = DACreateGlobalVector(da,&global2);CHKERRQ(ierr);

  ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,"temp",PETSC_FILE_CREATE,&viewer);CHKERRQ(ierr);
  ierr = VecView(global1,viewer);CHKERRQ(ierr);
  ierr = PetscViewerDestroy(viewer);CHKERRQ(ierr);

  ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,"temp",PETSC_FILE_RDONLY,&viewer);CHKERRQ(ierr);
  ierr = VecLoadIntoVector(viewer,global2);CHKERRQ(ierr);
  ierr = PetscViewerDestroy(viewer);CHKERRQ(ierr);

  ierr = VecAXPY(&mone,global1,global2);CHKERRQ(ierr);
  ierr = VecNorm(global2,NORM_MAX,&norm);CHKERRQ(ierr);
  if (norm != 0.0) {
    ierr = MPI_Comm_size(PETSC_COMM_WORLD,&size);CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Norm of difference %g should be zero\n",norm);CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_WORLD,"  Number of processors %d\n",size);CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_WORLD,"  M,N,P,dof %D %D %D %D\n",M,N,P,dof);CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_WORLD,"  stencil_width %D stencil_type %d periodic %d\n",stencil_width,(int)stencil_type,(int)periodic);CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_WORLD,"  dimension %d\n",1 + (int) flg2 + (int) flg3);CHKERRQ(ierr);
  }
   
  ierr = DADestroy(da);CHKERRQ(ierr);
  ierr = VecDestroy(global1);CHKERRQ(ierr);
  ierr = VecDestroy(global2);CHKERRQ(ierr);
  ierr = PetscFinalize();CHKERRQ(ierr);
  return 0;
}
