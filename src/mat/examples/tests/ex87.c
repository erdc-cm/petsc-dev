/*$Id: ex31.c,v 1.27 2001/08/07 03:03:07 balay Exp $*/

static char help[] = "Tests MatGetSubMatrices() for SBAIJ matrices\n\n";

#include "petscmat.h"

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **args)
{
  Mat         BAIJ,SBAIJ,*subBAIJ,*subSBAIJ;
  PetscViewer viewer;
  char        file[128];
  PetscTruth  flg;
  int         ierr,n = 2,rank,issize;
  IS          is,iss[2];

  PetscInitialize(&argc,&args,(char *)0,help);
  ierr = PetscOptionsGetString(PETSC_NULL,"-f",file,127,&flg);CHKERRQ(ierr);
  ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,file,PETSC_FILE_RDONLY,&viewer);CHKERRQ(ierr);
  ierr = MatLoad(viewer,MATMPIBAIJ,&BAIJ);CHKERRQ(ierr);
  ierr = PetscViewerDestroy(viewer);CHKERRQ(ierr);
  ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,file,PETSC_FILE_RDONLY,&viewer);CHKERRQ(ierr);
  ierr = MatLoad(viewer,MATMPISBAIJ,&SBAIJ);CHKERRQ(ierr);
  ierr = PetscViewerDestroy(viewer);CHKERRQ(ierr);

  ierr = MatGetSize(BAIJ,&issize,0);CHKERRQ(ierr);
  issize = 9;
  ierr = ISCreateStride(PETSC_COMM_SELF,issize,0,1,&is);CHKERRQ(ierr);
  iss[0] = is;iss[1] = is;
  ierr = MatGetSubMatrices(BAIJ,n,iss,iss,MAT_INITIAL_MATRIX,&subBAIJ);CHKERRQ(ierr);
  ierr = MatGetSubMatrices(SBAIJ,n,iss,iss,MAT_INITIAL_MATRIX,&subSBAIJ);CHKERRQ(ierr);

  ierr = MPI_Comm_rank(PETSC_COMM_WORLD,&rank);CHKERRQ(ierr);
  if (!rank) {
    ierr = MatView(subBAIJ[0],PETSC_VIEWER_SOCKET_SELF);CHKERRQ(ierr);
    ierr = MatView(subSBAIJ[0],PETSC_VIEWER_SOCKET_SELF);CHKERRQ(ierr);
  }

  /* Free data structures */
  ierr = ISDestroy(is);CHKERRQ(ierr);
  ierr = MatDestroyMatrices(n,&subBAIJ);CHKERRQ(ierr);
  ierr = MatDestroyMatrices(n,&subSBAIJ);CHKERRQ(ierr);
  ierr = MatDestroy(BAIJ);CHKERRQ(ierr);
  ierr = MatDestroy(SBAIJ);CHKERRQ(ierr);

  ierr = PetscFinalize();CHKERRQ(ierr);
  return 0;
}


