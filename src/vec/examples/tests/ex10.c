/*$Id: ex10.c,v 1.20 2001/09/11 16:32:10 bsmith Exp $*/

static char help[]= "Scatters from a parallel vector to a sequential vector.\n\
uses block index sets\n\n";

#include "petsc.h"
#include "petscis.h"
#include "petscvec.h"
#include "petscsys.h"

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **argv)
{
  int           bs = 1,n = 5,ierr,ix0[3] = {5,7,9},ix1[3] = {2,3,4};
  int           size,rank,i,iy0[3] = {1,2,4},iy1[3] = {0,1,3};
  PetscScalar   value;
  Vec           x,y;
  IS            isx,isy;
  VecScatter    ctx = 0,newctx;

  ierr = PetscInitialize(&argc,&argv,(char*)0,help);CHKERRQ(ierr);  
  ierr = MPI_Comm_size(PETSC_COMM_WORLD,&size);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(PETSC_COMM_WORLD,&rank);CHKERRQ(ierr);

  if (size != 2) SETERRQ(1,"Must run with 2 processors");

  ierr = PetscOptionsGetInt(PETSC_NULL,"-bs",&bs,PETSC_NULL);CHKERRQ(ierr);
  n = bs*n;

  /* create two vectors */
  ierr = VecCreate(PETSC_COMM_WORLD,PETSC_DECIDE,size*n,&x);CHKERRQ(ierr);
  ierr = VecSetFromOptions(x);CHKERRQ(ierr);
  ierr = VecCreateSeq(PETSC_COMM_SELF,n,&y);CHKERRQ(ierr);

  /* create two index sets */
  for (i=0; i<3; i++) {
    ix0[i] *= bs; ix1[i] *= bs; 
    iy0[i] *= bs; iy1[i] *= bs; 
  }

  if (!rank) {
    ierr = ISCreateBlock(PETSC_COMM_SELF,bs,3,ix0,&isx);CHKERRQ(ierr);
    ierr = ISCreateBlock(PETSC_COMM_SELF,bs,3,iy0,&isy);CHKERRQ(ierr);
  } else {
    ierr = ISCreateBlock(PETSC_COMM_SELF,bs,3,ix1,&isx);CHKERRQ(ierr);
    ierr = ISCreateBlock(PETSC_COMM_SELF,bs,3,iy1,&isy);CHKERRQ(ierr);
  }

  /* fill local part of parallel vector */
  for (i=n*rank; i<n*(rank+1); i++) {
    value = (PetscScalar) i;
    ierr = VecSetValues(x,1,&i,&value,INSERT_VALUES);CHKERRQ(ierr);
  }
  ierr = VecAssemblyBegin(x);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(x);CHKERRQ(ierr);

  ierr = VecView(x,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);

  /* fill local part of parallel vector */
  for (i=0; i<n; i++) {
    value = -(PetscScalar) (i + 100*rank);
    ierr = VecSetValues(y,1,&i,&value,INSERT_VALUES);CHKERRQ(ierr);
  }
  ierr = VecAssemblyBegin(y);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(y);CHKERRQ(ierr);


  ierr = VecScatterCreate(x,isx,y,isy,&ctx);CHKERRQ(ierr);
  ierr = VecScatterCopy(ctx,&newctx);CHKERRQ(ierr);
  ierr = VecScatterDestroy(ctx);CHKERRQ(ierr);

  ierr = VecScatterBegin(y,x,INSERT_VALUES,SCATTER_REVERSE,newctx);CHKERRQ(ierr);
  ierr = VecScatterEnd(y,x,INSERT_VALUES,SCATTER_REVERSE,newctx);CHKERRQ(ierr);
  ierr = VecScatterDestroy(newctx);CHKERRQ(ierr);

  ierr = VecView(x,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);

  ierr = ISDestroy(isx);CHKERRQ(ierr);
  ierr = ISDestroy(isy);CHKERRQ(ierr);
  ierr = VecDestroy(x);CHKERRQ(ierr);
  ierr = VecDestroy(y);CHKERRQ(ierr);

  ierr = PetscFinalize();CHKERRQ(ierr);
  return 0;
}
 
