/*$Id: daindex.c,v 1.33 2001/06/21 21:19:09 bsmith Exp $*/
 
/*
  Code for manipulating distributed regular arrays in parallel.
*/

#include "src/dm/da/daimpl.h"    /*I   "petscda.h"   I*/

#undef __FUNCT__  
#define __FUNCT__ "DAGetGlobalIndices"
/*@C
   DAGetGlobalIndices - Returns the global node number of all local nodes,
   including ghost nodes.

   Not Collective

   Input Parameter:
.  da - the distributed array

   Output Parameters:
+  n - the number of local elements, including ghost nodes (or PETSC_NULL)
-  idx - the global indices

   Level: intermediate

   Note: 
   For DA_STENCIL_STAR stencils the inactive corner ghost nodes are also included
   in the list of local indices (even though those nodes are not updated 
   during calls to DAXXXToXXX().

   Essentially the same data is returned in the form of a local-to-global mapping
   with the routine DAGetISLocalToGlobalMapping();

   Fortran Note:
   This routine is used differently from Fortran
.vb
        DA          da
        integer     n,da_array(1)
        PetscOffset i_da
        integer     ierr
        call DAGetGlobalIndices(da,n,da_array,i_da,ierr)

   C Access first local entry in list
        value = da_array(i_da + 1)
.ve

   See the Fortran chapter of the users manual for details

.keywords: distributed array, get, global, indices, local-to-global

.seealso: DACreate2d(), DAGetGhostCorners(), DAGetCorners(), DALocalToGlobal()
          DAGlobalToLocalBegin(), DAGlobalToLocalEnd(), DALocalToLocalBegin(), DAGetAO(), DAGetGlobalIndicesF90()
          DAGetISLocalToGlobalMapping(), DACreate3d(), DACreate1d(), DALocalToLocalEnd()
@*/
int DAGetGlobalIndices(DA da,int *n,int **idx)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(da,DA_COOKIE,1);
  if (n)   *n   = da->Nl;
  if (idx) *idx = da->idx;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DAGetNatural_Private"
/*
   Gets the natural number for each global number on the process.

   Used by DAGetAO() and DAGlobalToNatural_Create()
*/
int DAGetNatural_Private(DA da,int *outNlocal,IS *isnatural)
{
  int Nlocal,i,j,k,ierr,*lidx,lict = 0;

  PetscFunctionBegin;
  Nlocal = (da->xe-da->xs);
  if (da->dim > 1) {
    Nlocal *= (da->ye-da->ys);
  } 
  if (da->dim > 2) {
    Nlocal *= (da->ze-da->zs);
  }
  
  ierr = PetscMalloc(Nlocal*sizeof(int),&lidx);CHKERRQ(ierr);
  
  if (da->dim == 1) {
    for (i=da->xs; i<da->xe; i++) {
      /*  global number in natural ordering */
      lidx[lict++] = i;
    }
  } else if (da->dim == 2) {
    for (j=da->ys; j<da->ye; j++) {
      for (i=da->xs; i<da->xe; i++) {
	/*  global number in natural ordering */
	lidx[lict++] = i + j*da->M*da->w;
      }
    }
  } else if (da->dim == 3) {
    for (k=da->zs; k<da->ze; k++) {
      for (j=da->ys; j<da->ye; j++) {
	for (i=da->xs; i<da->xe; i++) {
	  lidx[lict++] = i + j*da->M*da->w + k*da->M*da->N*da->w;
	}
      }
    }
  }
  *outNlocal = Nlocal;
  ierr = ISCreateGeneral(da->comm,Nlocal,lidx,isnatural);CHKERRQ(ierr);
  ierr = PetscFree(lidx);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DAGetAO"
/*@C
   DAGetAO - Gets the application ordering context for a distributed array.

   Collective on DA

   Input Parameter:
.  da - the distributed array

   Output Parameters:
.  ao - the application ordering context for DAs

   Level: intermediate

   Notes:
   In this case, the AO maps to the natural grid ordering that would be used
   for the DA if only 1 processor were employed (ordering most rapidly in the
   x-direction, then y, then z).  Multiple degrees of freedom are numbered
   for each node (rather than 1 component for the whole grid, then the next
   component, etc.)

.keywords: distributed array, get, global, indices, local-to-global

.seealso: DACreate2d(), DAGetGhostCorners(), DAGetCorners(), DALocalToGlocal()
          DAGlobalToLocalBegin(), DAGlobalToLocalEnd(), DALocalToLocalBegin(), DALocalToLocalEnd(), DAGetGlobalIndices()
@*/
int DAGetAO(DA da,AO *ao)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(da,DA_COOKIE,1);
  PetscValidPointer(ao,2);

  /* 
     Build the natural ordering to PETSc ordering mappings.
  */
  if (!da->ao) {
    IS  ispetsc,isnatural;
    int ierr,Nlocal;

    ierr = DAGetNatural_Private(da,&Nlocal,&isnatural);CHKERRQ(ierr);
    ierr = ISCreateStride(da->comm,Nlocal,da->base,1,&ispetsc);CHKERRQ(ierr);
    ierr = AOCreateBasicIS(isnatural,ispetsc,&da->ao);CHKERRQ(ierr);
    PetscLogObjectParent(da,da->ao);
    ierr = ISDestroy(ispetsc);CHKERRQ(ierr);
    ierr = ISDestroy(isnatural);CHKERRQ(ierr);
  }
  *ao = da->ao;
  PetscFunctionReturn(0);
}

/*MC
    DAGetGlobalIndicesF90 - Returns a Fortran90 pointer to the list of 
    global indices (global node number of all local nodes, including
    ghost nodes).

    Synopsis:
    DAGetGlobalIndicesF90(DA da,integer n,{integer, pointer :: idx(:)},integer ierr)

    Input Parameter:
.   da - the distributed array

    Output Parameters:
+   n - the number of local elements, including ghost nodes (or PETSC_NULL)
.   idx - the Fortran90 pointer to the global indices
-   ierr - error code

    Level: intermediate

    Notes:
     Not yet supported for all F90 compilers

.keywords: distributed array, get, global, indices, local-to-global, f90

.seealso: DAGetGlobalIndices()
M*/
