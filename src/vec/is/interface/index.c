/*$Id: index.c,v 1.85 2001/08/06 21:14:32 bsmith Exp $*/
/*  
   Defines the abstract operations on index sets, i.e. the public interface. 
*/
#include "src/vec/is/isimpl.h" 
     /*I "petscis.h" I*/

#undef __FUNCT__  
#define __FUNCT__ "ISIdentity" 
/*@C
   ISIdentity - Determines whether index set is the identity mapping.

   Collective on IS

   Input Parmeters:
.  is - the index set

   Output Parameters:
.  ident - PETSC_TRUE if an identity, else PETSC_FALSE

   Level: intermediate

   Concepts: identity mapping
   Concepts: index sets^is identity

.seealso: ISSetIdentity()
@*/
int ISIdentity(IS is,PetscTruth *ident)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(is,IS_COOKIE);
  PetscValidIntPointer(ident);
  *ident = is->isidentity;
  if (*ident) PetscFunctionReturn(0);
  if (is->ops->identity) {
    ierr = (*is->ops->identity)(is,ident);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "ISSetIdentity" 
/*@
   ISSetIdentity - Informs the index set that it is an identity.

   Collective on IS

   Input Parmeters:
.  is - the index set

   Level: intermediate

   Concepts: identity mapping
   Concepts: index sets^is identity

.seealso: ISIdentity()
@*/
int ISSetIdentity(IS is)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(is,IS_COOKIE);
  is->isidentity = PETSC_TRUE;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "ISPermutation" 
/*@C
   ISPermutation - PETSC_TRUE or PETSC_FALSE depending on whether the 
   index set has been declared to be a permutation.

   Collective on IS

   Input Parmeters:
.  is - the index set

   Output Parameters:
.  perm - PETSC_TRUE if a permutation, else PETSC_FALSE

   Level: intermediate

  Concepts: permutation
  Concepts: index sets^is permutation

.seealso: ISSetPermutation()
@*/
int ISPermutation(IS is,PetscTruth *perm)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(is,IS_COOKIE);
  PetscValidIntPointer(perm);
  *perm = (PetscTruth) is->isperm;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "ISSetPermutation" 
/*@
   ISSetPermutation - Informs the index set that it is a permutation.

   Collective on IS

   Input Parmeters:
.  is - the index set

   Level: intermediate

  Concepts: permutation
  Concepts: index sets^permutation

.seealso: ISPermutation()
@*/
int ISSetPermutation(IS is)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(is,IS_COOKIE);
  is->isperm = PETSC_TRUE;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "ISDestroy" 
/*@C
   ISDestroy - Destroys an index set.

   Collective on IS

   Input Parameters:
.  is - the index set

   Level: beginner

.seealso: ISCreateGeneral(), ISCreateStride(), ISCreateBlocked()
@*/
int ISDestroy(IS is)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(is,IS_COOKIE);
  if (--is->refct > 0) PetscFunctionReturn(0);

  /* if memory was published with AMS then destroy it */
  ierr = PetscObjectDepublish(is);CHKERRQ(ierr);

  ierr = (*is->ops->destroy)(is);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "ISInvertPermutation" 
/*@C
   ISInvertPermutation - Creates a new permutation that is the inverse of 
                         a given permutation.

   Collective on IS

   Input Parameter:
+  is - the index set
-  nlocal - number of indices on this processor in result (ignored for 1 proccessor) or
            use PETSC_DECIDE

   Output Parameter:
.  isout - the inverse permutation

   Level: intermediate

   Notes: For parallel index sets this does the complete parallel permutation, but the 
    code is not efficient for huge index sets (10,000,000 indices).

   Concepts: inverse permutation
   Concepts: permutation^inverse
   Concepts: index sets^inverting
@*/
int ISInvertPermutation(IS is,int nlocal,IS *isout)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(is,IS_COOKIE);
  if (!is->isperm) SETERRQ(PETSC_ERR_ARG_WRONG,"not a permutation");
  ierr = (*is->ops->invertpermutation)(is,nlocal,isout);CHKERRQ(ierr);
  ierr = ISSetPermutation(*isout);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#if defined(__cplusplus)
#undef __FUNCT__  
#define __FUNCT__ "ISGetSizeNew" 
int ISGetSizeNew(IS is,int *size)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(is,IS_COOKIE);
  PetscValidIntPointer(size);
  ierr = is->cops->getsize(size);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
#endif

#undef __FUNCT__  
#define __FUNCT__ "ISGetSize" 
/*@
   ISGetSize - Returns the global length of an index set. 

   Not Collective

   Input Parameter:
.  is - the index set

   Output Parameter:
.  size - the global size

   Level: beginner

   Concepts: size^of index set
   Concepts: index sets^size

@*/
int ISGetSize(IS is,int *size)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(is,IS_COOKIE);
  PetscValidIntPointer(size);
  ierr = (*is->ops->getsize)(is,size);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "ISGetLocalSize" 
/*@
   ISGetLocalSize - Returns the local (processor) length of an index set. 

   Not Collective

   Input Parameter:
.  is - the index set

   Output Parameter:
.  size - the local size

   Level: beginner

   Concepts: size^of index set
   Concepts: local size^of index set
   Concepts: index sets^local size
  
@*/
int ISGetLocalSize(IS is,int *size)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(is,IS_COOKIE);
  PetscValidIntPointer(size);
  ierr = (*is->ops->getlocalsize)(is,size);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "ISGetIndices" 
/*@C
   ISGetIndices - Returns a pointer to the indices.  The user should call 
   ISRestoreIndices() after having looked at the indices.  The user should 
   NOT change the indices.

   Not Collective

   Input Parameter:
.  is - the index set

   Output Parameter:
.  ptr - the location to put the pointer to the indices

   Fortran Note:
   This routine is used differently from Fortran
$    IS          is
$    integer     is_array(1)
$    PetscOffset i_is
$    int         ierr
$       call ISGetIndices(is,is_array,i_is,ierr)
$
$   Access first local entry in list
$      value = is_array(i_is + 1)
$
$      ...... other code
$       call ISRestoreIndices(is,is_array,i_is,ierr)

   See the Fortran chapter of the users manual and 
   petsc/src/is/examples/[tutorials,tests] for details.

   Level: intermediate

   Concepts: index sets^getting indices
   Concepts: indices of index set

.seealso: ISRestoreIndices(), ISGetIndicesF90()
@*/
int ISGetIndices(IS is,int *ptr[])
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(is,IS_COOKIE);
  PetscValidPointer(ptr);
  ierr = (*is->ops->getindices)(is,ptr);CHKERRQ(ierr);
  PetscFunctionReturn(0);
} 

#undef __FUNCT__  
#define __FUNCT__ "ISRestoreIndices" 
/*@C
   ISRestoreIndices - Restores an index set to a usable state after a call 
                      to ISGetIndices().

   Not Collective

   Input Parameters:
+  is - the index set
-  ptr - the pointer obtained by ISGetIndices()

   Fortran Note:
   This routine is used differently from Fortran
$    IS          is
$    integer     is_array(1)
$    PetscOffset i_is
$    int         ierr
$       call ISGetIndices(is,is_array,i_is,ierr)
$
$   Access first local entry in list
$      value = is_array(i_is + 1)
$
$      ...... other code
$       call ISRestoreIndices(is,is_array,i_is,ierr)

   See the Fortran chapter of the users manual and 
   petsc/src/is/examples/[tutorials,tests] for details.

   Level: intermediate

.seealso: ISGetIndices(), ISRestoreIndicesF90()
@*/
int ISRestoreIndices(IS is,int *ptr[])
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(is,IS_COOKIE);
  PetscValidPointer(ptr);
  if (is->ops->restoreindices) {
    ierr = (*is->ops->restoreindices)(is,ptr);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "ISView" 
/*@C
   ISView - Displays an index set.

   Collective on IS

   Input Parameters:
+  is - the index set
-  viewer - viewer used to display the set, for example PETSC_VIEWER_STDOUT_SELF.

   Level: intermediate

.seealso: PetscViewerASCIIOpen()
@*/
int ISView(IS is,PetscViewer viewer)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(is,IS_COOKIE);
  if (!viewer) viewer = PETSC_VIEWER_STDOUT_(is->comm); 
  PetscValidHeaderSpecific(viewer,PETSC_VIEWER_COOKIE);
  PetscCheckSameComm(is,viewer);
  
  ierr = (*is->ops->view)(is,viewer);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "ISSort" 
/*@
   ISSort - Sorts the indices of an index set.

   Collective on IS

   Input Parameters:
.  is - the index set

   Level: intermediate

   Concepts: index sets^sorting
   Concepts: sorting^index set

.seealso: ISSorted()
@*/
int ISSort(IS is)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(is,IS_COOKIE);
  ierr = (*is->ops->sortindices)(is);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "ISSorted" 
/*@C
   ISSorted - Checks the indices to determine whether they have been sorted.

   Collective on IS

   Input Parameter:
.  is - the index set

   Output Parameter:
.  flg - output flag, either PETSC_TRUE if the index set is sorted, 
         or PETSC_FALSE otherwise.

   Level: intermediate

.seealso: ISSort()
@*/
int ISSorted(IS is,PetscTruth *flg)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(is,IS_COOKIE);
  PetscValidIntPointer(flg);
  ierr = (*is->ops->sorted)(is,flg);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "ISDuplicate" 
/*@C
   ISDuplicate - Creates a duplicate copy of an index set.

   Collective on IS

   Input Parmeters:
.  is - the index set

   Output Parameters:
.  isnew - the copy of the index set

   Level: beginner

   Concepts: index sets^duplicating

.seealso: ISCreateGeneral()
@*/
int ISDuplicate(IS is,IS *newIS)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(is,IS_COOKIE);
  PetscValidPointer(newIS);
  ierr = (*is->ops->duplicate)(is,newIS);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


/*MC
    ISGetIndicesF90 - Accesses the elements of an index set from Fortran90.
    The users should call ISRestoreIndicesF90() after having looked at the
    indices.  The user should NOT change the indices.

    Synopsis:
    ISGetIndicesF90(IS x,{integer, pointer :: xx_v(:)},integer ierr)

    Not collective

    Input Parameter:
.   x - index set

    Output Parameters:
+   xx_v - the Fortran90 pointer to the array
-   ierr - error code

    Example of Usage: 
.vb
    PetscScalar, pointer xx_v(:)
    ....
    call ISGetIndicesF90(x,xx_v,ierr)
    a = xx_v(3)
    call ISRestoreIndicesF90(x,xx_v,ierr)
.ve

    Notes:
    Not yet supported for all F90 compilers.

    Level: intermediate

.seealso:  ISRestoreIndicesF90(), ISGetIndices(), ISRestoreIndices()

  Concepts: index sets^getting indices in f90
  Concepts: indices of index set in f90

M*/

/*MC
    ISRestoreIndicesF90 - Restores an index set to a usable state after
    a call to ISGetIndicesF90().

    Synopsis:
    ISRestoreIndicesF90(IS x,{integer, pointer :: xx_v(:)},integer ierr)

    Not collective

    Input Parameters:
.   x - index set
.   xx_v - the Fortran90 pointer to the array

    Output Parameter:
.   ierr - error code


    Example of Usage: 
.vb
    PetscScalar, pointer xx_v(:)
    ....
    call ISGetIndicesF90(x,xx_v,ierr)
    a = xx_v(3)
    call ISRestoreIndicesF90(x,xx_v,ierr)
.ve
   
    Notes:
    Not yet supported for all F90 compilers.

    Level: intermediate

.seealso:  ISGetIndicesF90(), ISGetIndices(), ISRestoreIndices()

M*/

/*MC
    ISBlockGetIndicesF90 - Accesses the elements of an index set from Fortran90.
    The users should call ISBlockRestoreIndicesF90() after having looked at the
    indices.  The user should NOT change the indices.

    Synopsis:
    ISBlockGetIndicesF90(IS x,{integer, pointer :: xx_v(:)},integer ierr)

    Not collective

    Input Parameter:
.   x - index set

    Output Parameters:
+   xx_v - the Fortran90 pointer to the array
-   ierr - error code
    Example of Usage: 
.vb
    PetscScalar, pointer xx_v(:)
    ....
    call ISBlockGetIndicesF90(x,xx_v,ierr)
    a = xx_v(3)
    call ISBlockRestoreIndicesF90(x,xx_v,ierr)
.ve

    Notes:
    Not yet supported for all F90 compilers

    Level: intermediate

.seealso:  ISBlockRestoreIndicesF90(), ISGetIndices(), ISRestoreIndices(),
           ISRestoreIndices()

  Concepts: index sets^getting block indices in f90
  Concepts: indices of index set in f90
  Concepts: block^ indices of index set in f90

M*/

/*MC
    ISBlockRestoreIndicesF90 - Restores an index set to a usable state after
    a call to ISBlockGetIndicesF90().

    Synopsis:
    ISBlockRestoreIndicesF90(IS x,{integer, pointer :: xx_v(:)},integer ierr)

    Input Parameters:
+   x - index set
-   xx_v - the Fortran90 pointer to the array

    Output Parameter:
.   ierr - error code

    Example of Usage: 
.vb
    PetscScalar, pointer xx_v(:)
    ....
    call ISBlockGetIndicesF90(x,xx_v,ierr)
    a = xx_v(3)
    call ISBlockRestoreIndicesF90(x,xx_v,ierr)
.ve
   
    Notes:
    Not yet supported for all F90 compilers

    Level: intermediate

.seealso:  ISBlockGetIndicesF90(), ISGetIndices(), ISRestoreIndices(), ISRestoreIndicesF90()

M*/


