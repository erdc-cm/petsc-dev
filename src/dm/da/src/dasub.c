/*$Id: dasub.c,v 1.33 2001/03/23 23:25:00 balay Exp $*/
 
/*
  Code for manipulating distributed regular arrays in parallel.
*/

#include "src/dm/da/daimpl.h"    /*I   "petscda.h"   I*/

#undef __FUNCT__  
#define __FUNCT__ "DAGetProcessorSubset"
/*@C
   DAGetProcessorSubset - Returns a communicator consisting only of the
   processors in a DA that own a particular global x, y, or z grid point
   (corresponding to a logical plane in a 3D grid or a line in a 2D grid).

   Collective on DA

   Input Parameters:
+  da - the distributed array
.  dir - Cartesian direction, either DA_X, DA_Y, or DA_Z
-  gp - global grid point number in this direction

   Output Parameters:
.  comm - new communicator

   Level: advanced

   Notes:
   This routine is particularly useful to compute boundary conditions
   or other application-specific calculations that require manipulating
   sets of data throughout a logical plane of grid points.

.keywords: distributed array, get, processor subset
@*/
int DAGetProcessorSubset(DA da,DADirection dir,int gp,MPI_Comm *comm)
{
  MPI_Group group,subgroup;
  int       ierr,i,ict,flag,size,*ranks,*owners,xs,xm,ys,ym,zs,zm;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(da,DA_COOKIE,1);
  flag = 0; 
  ierr = DAGetCorners(da,&xs,&xm,&ys,&ym,&zs,&zm);CHKERRQ(ierr);
  ierr = MPI_Comm_size(da->comm,&size);CHKERRQ(ierr);
  if (dir == DA_Z) {
    if (da->dim < 3) SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"DA_Z invalid for DA dim < 3");
    if (gp < 0 || gp > da->P) SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"invalid grid point");
    if (gp >= zs && gp < zs+zm) flag = 1;
  } else if (dir == DA_Y) {
    if (da->dim == 1) SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"DA_Y invalid for DA dim = 1");
    if (gp < 0 || gp > da->N) SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"invalid grid point");
    if (gp >= ys && gp < ys+ym) flag = 1;
  } else if (dir == DA_X) {
    if (gp < 0 || gp > da->M) SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"invalid grid point");
    if (gp >= xs && gp < xs+xm) flag = 1;
  } else SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"Invalid direction");

  ierr = PetscMalloc(2*size*sizeof(int),&owners);CHKERRQ(ierr);
  ranks = owners + size;
  ierr = MPI_Allgather(&flag,1,MPI_INT,owners,1,MPI_INT,da->comm);CHKERRQ(ierr);
  ict = 0;
  PetscLogInfo(da,"DAGetProcessorSubset: dim=%d, direction=%d, procs: ",da->dim,(int)dir);
  for (i=0; i<size; i++) {
    if (owners[i]) {
      ranks[ict] = i; ict++;
      PetscLogInfo(da,"%d ",i);
    }
  }
  PetscLogInfo(da,"\n");
  ierr = MPI_Comm_group(da->comm,&group);CHKERRQ(ierr);
  ierr = MPI_Group_incl(group,ict,ranks,&subgroup);CHKERRQ(ierr);
  ierr = MPI_Comm_create(da->comm,subgroup,comm);CHKERRQ(ierr);
  ierr = PetscFree(owners);CHKERRQ(ierr);
  PetscFunctionReturn(0);
} 
