/*$Id: dalocal.c,v 1.35 2001/08/07 03:04:39 balay Exp $*/
 
/*
  Code for manipulating distributed regular arrays in parallel.
*/

#include "src/dm/da/daimpl.h"    /*I   "petscda.h"   I*/

#undef __FUNCT__  
#define __FUNCT__ "DACreateLocalVector"
/*@C
   DACreateLocalVector - Creates a Seq PETSc vector that
   may be used with the DAXXX routines.

   Not Collective

   Input Parameter:
.  da - the distributed array

   Output Parameter:
.  g - the local vector

   Level: beginner

   Note:
   The output parameter, g, is a regular PETSc vector that should be destroyed
   with a call to VecDestroy() when usage is finished.

.keywords: distributed array, create, local, vector

.seealso: DACreateGlobalVector(), VecDuplicate(), VecDuplicateVecs(),
          DACreate1d(), DACreate2d(), DACreate3d(), DAGlobalToLocalBegin(),
          DAGlobalToLocalEnd(), DALocalToGlobal(), DAGetLocalVector(), DARestoreLocalVector()
@*/
int DACreateLocalVector(DA da,Vec* g)
{
  int ierr;

  PetscFunctionBegin; 
  PetscValidHeaderSpecific(da,DA_COOKIE);
  ierr = VecDuplicate(da->local,g);CHKERRQ(ierr);
  ierr = PetscObjectCompose((PetscObject)*g,"DA",(PetscObject)da);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DAGetLocalVector"
/*@C
   DAGetLocalVector - Gets a Seq PETSc vector that
   may be used with the DAXXX routines.

   Not Collective

   Input Parameter:
.  da - the distributed array

   Output Parameter:
.  g - the local vector

   Level: beginner

   Note:
   The output parameter, g, is a regular PETSc vector that should be returned with 
   DARestoreLocalVector() DO NOT call VecDestroy() on it.

   VecStride*() operations can be useful when using DA with dof > 1

.keywords: distributed array, create, local, vector

.seealso: DACreateGlobalVector(), VecDuplicate(), VecDuplicateVecs(),
          DACreate1d(), DACreate2d(), DACreate3d(), DAGlobalToLocalBegin(),
          DAGlobalToLocalEnd(), DALocalToGlobal(), DACreateLocalVector(), DARestoreLocalVector(),
          VecStrideMax(), VecStrideMin(), VecStrideNorm()
@*/
int DAGetLocalVector(DA da,Vec* g)
{
  int ierr,i;

  PetscFunctionBegin; 
  PetscValidHeaderSpecific(da,DA_COOKIE);
  for (i=0; i<DA_MAX_WORK_VECTORS; i++) {
    if (da->localin[i]) {
      *g             = da->localin[i];
      da->localin[i] = PETSC_NULL;
      goto alldone;
    }
  }
  ierr = VecDuplicate(da->local,g);CHKERRQ(ierr);
  ierr = PetscObjectCompose((PetscObject)*g,"DA",(PetscObject)da);CHKERRQ(ierr);

  alldone:
  for (i=0; i<DA_MAX_WORK_VECTORS; i++) {
    if (!da->localout[i]) {
      da->localout[i] = *g;
      break;
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DARestoreLocalVector"
/*@C
   DARestoreLocalVector - Returns a Seq PETSc vector that
     obtained from DAGetLocalVector(). Do not use with vector obtained via
     DACreateLocalVector().

   Not Collective

   Input Parameter:
+  da - the distributed array
-  g - the local vector

   Level: beginner

.keywords: distributed array, create, local, vector

.seealso: DACreateGlobalVector(), VecDuplicate(), VecDuplicateVecs(),
          DACreate1d(), DACreate2d(), DACreate3d(), DAGlobalToLocalBegin(),
          DAGlobalToLocalEnd(), DALocalToGlobal(), DACreateLocalVector(), DAGetLocalVector()
@*/
int DARestoreLocalVector(DA da,Vec* g)
{
  int ierr,i,j;

  PetscFunctionBegin; 
  PetscValidHeaderSpecific(da,DA_COOKIE);
  for (j=0; j<DA_MAX_WORK_VECTORS; j++) {
    if (*g == da->localout[j]) {
      da->localout[j] = PETSC_NULL;
      for (i=0; i<DA_MAX_WORK_VECTORS; i++) {
        if (!da->localin[i]) {
          da->localin[i] = *g;
          goto alldone;
        }
      }
    }
  }
  ierr = VecDestroy(*g);CHKERRQ(ierr);
  alldone:
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DAGetGlobalVector"
/*@C
   DAGetGlobalVector - Gets a MPI PETSc vector that
   may be used with the DAXXX routines.

   Collective on DA

   Input Parameter:
.  da - the distributed array

   Output Parameter:
.  g - the global vector

   Level: beginner

   Note:
   The output parameter, g, is a regular PETSc vector that should be returned with 
   DARestoreGlobalVector() DO NOT call VecDestroy() on it.

   VecStride*() operations can be useful when using DA with dof > 1

.keywords: distributed array, create, Global, vector

.seealso: DACreateGlobalVector(), VecDuplicate(), VecDuplicateVecs(),
          DACreate1d(), DACreate2d(), DACreate3d(), DAGlobalToLocalBegin(),
          DAGlobalToLocalEnd(), DALocalToGlobal(), DACreateLocalVector(), DARestoreLocalVector()
          VecStrideMax(), VecStrideMin(), VecStrideNorm()

@*/
int DAGetGlobalVector(DA da,Vec* g)
{
  int ierr,i;

  PetscFunctionBegin; 
  PetscValidHeaderSpecific(da,DA_COOKIE);
  for (i=0; i<DA_MAX_WORK_VECTORS; i++) {
    if (da->globalin[i]) {
      *g             = da->globalin[i];
      da->globalin[i] = PETSC_NULL;
      goto alldone;
    }
  }
  ierr = VecDuplicate(da->global,g);CHKERRQ(ierr);
  ierr = PetscObjectCompose((PetscObject)*g,"DA",(PetscObject)da);CHKERRQ(ierr);

  alldone:
  for (i=0; i<DA_MAX_WORK_VECTORS; i++) {
    if (!da->globalout[i]) {
      da->globalout[i] = *g;
      break;
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DARestoreGlobalVector"
/*@C
   DARestoreGlobalVector - Returns a Seq PETSc vector that
     obtained from DAGetGlobalVector(). Do not use with vector obtained via
     DACreateGlobalVector().

   Not Collective

   Input Parameter:
+  da - the distributed array
-  g - the global vector

   Level: beginner

.keywords: distributed array, create, global, vector

.seealso: DACreateGlobalVector(), VecDuplicate(), VecDuplicateVecs(),
          DACreate1d(), DACreate2d(), DACreate3d(), DAGlobalToGlobalBegin(),
          DAGlobalToGlobalEnd(), DAGlobalToGlobal(), DACreateLocalVector(), DAGetGlobalVector()
@*/
int DARestoreGlobalVector(DA da,Vec* g)
{
  int ierr,i,j;

  PetscFunctionBegin; 
  PetscValidHeaderSpecific(da,DA_COOKIE);
  for (j=0; j<DA_MAX_WORK_VECTORS; j++) {
    if (*g == da->globalout[j]) {
      da->globalout[j] = PETSC_NULL;
      for (i=0; i<DA_MAX_WORK_VECTORS; i++) {
        if (!da->globalin[i]) {
          da->globalin[i] = *g;
          goto alldone;
        }
      }
    }
  }
  ierr = VecDestroy(*g);CHKERRQ(ierr);
  alldone:
  PetscFunctionReturn(0);
}

/* ------------------------------------------------------------------- */
#if defined(PETSC_HAVE_ADIC) && !defined(PETSC_USE_COMPLEX)

EXTERN_C_BEGIN
#include "adic/ad_utils.h"
EXTERN_C_END

#undef __FUNCT__
#define __FUNCT__ "DAGetAdicArray"
/*@C
     DAGetAdicArray - Gets an array of derivative types for a DA
          
    Input Parameter:
+    da - information about my local patch
-    ghosted - do you want arrays for the ghosted or nonghosted patch

    Output Parameters:
+    ptr - array data structured to be passed to ad_FormFunctionLocal()
.    array_start - actual start of 1d array of all values that adiC can access directly (may be null)
-    tdof - total number of degrees of freedom represented in array_start (may be null)

     Notes: Returns the same type of object as the DAVecGetArray() except its elements are 
           derivative types instead of PetscScalars

     Level: advanced

.seealso: DARestoreAdicArray()

@*/
int DAGetAdicArray(DA da,PetscTruth ghosted,void **iptr,void **array_start,int *tdof)
{
  int  ierr,j,i,deriv_type_size,xs,ys,xm,ym,zs,zm,itdof;
  char *iarray_start;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(da,DA_COOKIE);
  if (ghosted) {
    for (i=0; i<DA_MAX_AD_ARRAYS; i++) {
      if (da->adarrayghostedin[i]) {
        *iptr                   = da->adarrayghostedin[i];
        iarray_start            = (char*)da->adstartghostedin[i];
        itdof                   = da->ghostedtdof;
        da->adarrayghostedin[i] = PETSC_NULL;
        da->adstartghostedin[i] = PETSC_NULL;
        
        goto done;
      }
    }
    xs = da->Xs;
    ys = da->Ys;
    zs = da->Zs;
    xm = da->Xe-da->Xs;
    ym = da->Ye-da->Ys;
    zm = da->Ze-da->Zs;
  } else {
    for (i=0; i<DA_MAX_AD_ARRAYS; i++) {
      if (da->adarrayin[i]) {
        *iptr            = da->adarrayin[i];
        iarray_start     = (char*)da->adstartin[i];
        itdof            = da->tdof;
        da->adarrayin[i] = PETSC_NULL;
        da->adstartin[i] = PETSC_NULL;
        
        goto done;
      }
    }
    xs = da->xs;
    ys = da->ys;
    zs = da->zs;
    xm = da->xe-da->xs;
    ym = da->ye-da->ys;
    zm = da->ze-da->zs;
  }
  deriv_type_size = PetscADGetDerivTypeSize();

  switch (da->dim) {
    case 1: {
      void *ptr;
      itdof = xm;

      ierr  = PetscMalloc(xm*deriv_type_size,&iarray_start);CHKERRQ(ierr);
      ierr  = PetscMemzero(iarray_start,xm*deriv_type_size);CHKERRQ(ierr);

      ptr   = (void*)(iarray_start - xs*deriv_type_size);
      *iptr = (void*)ptr; 
      break;}
    case 2: {
      void **ptr;
      itdof = xm*ym;

      ierr  = PetscMalloc((ym+1)*sizeof(void *)+xm*ym*deriv_type_size,&iarray_start);CHKERRQ(ierr);
      ierr  = PetscMemzero(iarray_start,xm*ym*deriv_type_size);CHKERRQ(ierr);

      ptr  = (void**)(iarray_start + xm*ym*deriv_type_size - ys*sizeof(void*));
      for(j=ys;j<ys+ym;j++) {
        ptr[j] = iarray_start + deriv_type_size*(xm*(j-ys) - xs);
      }
      *iptr = (void*)ptr; 
      break;}
    case 3: {
      void ***ptr,**bptr;
      itdof = xm*ym*zm;

      ierr  = PetscMalloc((zm+1)*sizeof(void **)+(ym*zm+1)*sizeof(void*)+xm*ym*zm*deriv_type_size,&iarray_start);CHKERRQ(ierr);
      ierr  = PetscMemzero(iarray_start,xm*ym*zm*deriv_type_size);CHKERRQ(ierr);

      ptr  = (void***)(iarray_start + xm*ym*zm*deriv_type_size - zs*sizeof(void*));
      bptr = (void**)(iarray_start + xm*ym*zm*deriv_type_size + zm*sizeof(void**));
      for(i=zs;i<zs+zm;i++) {
        ptr[i] = bptr + ((i-zs)*ym* - ys)*sizeof(void*);
      }
      for (i=zs; i<zs+zm; i++) {
        for (j=ys; j<ys+ym; j++) {
          ptr[i][j] = iarray_start + deriv_type_size*(xm*ym*(i-zs) + xm*(j-ys) - xs);
        }
      }

      *iptr = (void*)ptr; 
      break;}
    default:
      SETERRQ1(1,"Dimension %d not supported",da->dim);
  }

  done:
  /* add arrays to the checked out list */
  if (ghosted) {
    for (i=0; i<DA_MAX_AD_ARRAYS; i++) {
      if (!da->adarrayghostedout[i]) {
        da->adarrayghostedout[i] = *iptr ;
        da->adstartghostedout[i] = iarray_start;
        da->ghostedtdof          = itdof;
        break;
      }
    }
  } else {
    for (i=0; i<DA_MAX_AD_ARRAYS; i++) {
      if (!da->adarrayout[i]) {
        da->adarrayout[i] = *iptr ;
        da->adstartout[i] = iarray_start;
        da->tdof          = itdof;
        break;
      }
    }
  }
  if (i == DA_MAX_AD_ARRAYS+1) SETERRQ(1,"Too many DA ADIC arrays obtained");
  if (tdof)        *tdof = itdof;
  if (array_start) *array_start = iarray_start;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DARestoreAdicArray"
/*@C
     DARestoreAdicArray - Restores an array of derivative types for a DA
          
    Input Parameter:
+    da - information about my local patch
-    ghosted - do you want arrays for the ghosted or nonghosted patch

    Output Parameters:
+    ptr - array data structured to be passed to ad_FormFunctionLocal()
.    array_start - actual start of 1d array of all values that adiC can access directly
-    tdof - total number of degrees of freedom represented in array_start

     Level: advanced

.seealso: DAGetAdicArray()

@*/
int DARestoreAdicArray(DA da,PetscTruth ghosted,void **iptr,void **array_start,int *tdof)
{
  int  i;
  void *iarray_start = 0;
  
  PetscFunctionBegin;
  PetscValidHeaderSpecific(da,DA_COOKIE);
  if (ghosted) {
    for (i=0; i<DA_MAX_AD_ARRAYS; i++) {
      if (da->adarrayghostedout[i] == *iptr) {
        iarray_start             = da->adstartghostedout[i];
        da->adarrayghostedout[i] = PETSC_NULL;
        da->adstartghostedout[i] = PETSC_NULL;
        break;
      }
    }
    if (!iarray_start) SETERRQ(1,"Could not find array in checkout list");
    for (i=0; i<DA_MAX_AD_ARRAYS; i++) {
      if (!da->adarrayghostedin[i]){
        da->adarrayghostedin[i] = *iptr;
        da->adstartghostedin[i] = iarray_start;
        break;
      }
    }
  } else {
    for (i=0; i<DA_MAX_AD_ARRAYS; i++) {
      if (da->adarrayout[i] == *iptr) {
        iarray_start      = da->adstartout[i];
        da->adarrayout[i] = PETSC_NULL;
        da->adstartout[i] = PETSC_NULL;
        break;
      }
    }
    if (!iarray_start) SETERRQ(1,"Could not find array in checkout list");
    for (i=0; i<DA_MAX_AD_ARRAYS; i++) {
      if (!da->adarrayin[i]){
        da->adarrayin[i]   = *iptr;
        da->adstartin[i]   = iarray_start;
        break;
      }
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "ad_DAGetArray"
int ad_DAGetArray(DA da,PetscTruth ghosted,void **iptr)
{
  int ierr;
  PetscFunctionBegin;
  ierr = DAGetAdicArray(da,ghosted,iptr,0,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "ad_DARestoreArray"
int ad_DARestoreArray(DA da,PetscTruth ghosted,void **iptr)
{
  int ierr;
  PetscFunctionBegin;
  ierr = DARestoreAdicArray(da,ghosted,iptr,0,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#endif

#undef __FUNCT__
#define __FUNCT__ "DAGetArray"
/*@C
     DAGetArray - Gets a work array for a DA
          
    Input Parameter:
+    da - information about my local patch
-    ghosted - do you want arrays for the ghosted or nonghosted patch

    Output Parameters:
.    ptr - array data structured

     Level: advanced

.seealso: DARestoreArray(), DAGetAdicArray()

@*/
int DAGetArray(DA da,PetscTruth ghosted,void **iptr)
{
  int  ierr,j,i,xs,ys,xm,ym,zs,zm;
  char *iarray_start;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(da,DA_COOKIE);
  if (ghosted) {
    for (i=0; i<DA_MAX_WORK_ARRAYS; i++) {
      if (da->arrayghostedin[i]) {
        *iptr                 = da->arrayghostedin[i];
        iarray_start          = (char*)da->startghostedin[i];
        da->arrayghostedin[i] = PETSC_NULL;
        da->startghostedin[i] = PETSC_NULL;
        
        goto done;
      }
    }
    xs = da->Xs;
    ys = da->Ys;
    zs = da->Zs;
    xm = da->Xe-da->Xs;
    ym = da->Ye-da->Ys;
    zm = da->Ze-da->Zs;
  } else {
    for (i=0; i<DA_MAX_WORK_ARRAYS; i++) {
      if (da->arrayin[i]) {
        *iptr          = da->arrayin[i];
        iarray_start   = (char*)da->startin[i];
        da->arrayin[i] = PETSC_NULL;
        da->startin[i] = PETSC_NULL;
        
        goto done;
      }
    }
    xs = da->xs;
    ys = da->ys;
    zs = da->zs;
    xm = da->xe-da->xs;
    ym = da->ye-da->ys;
    zm = da->ze-da->zs;
  }

  switch (da->dim) {
    case 1: {
      void *ptr;

      ierr  = PetscMalloc(xm*sizeof(PetscScalar),&iarray_start);CHKERRQ(ierr);
      ierr  = PetscMemzero(iarray_start,xm*sizeof(PetscScalar));CHKERRQ(ierr);

      ptr   = (void*)(iarray_start - xs*sizeof(PetscScalar));
      *iptr = (void*)ptr; 
      break;}
    case 2: {
      void **ptr;

      ierr  = PetscMalloc((ym+1)*sizeof(void *)+xm*ym*sizeof(PetscScalar),&iarray_start);CHKERRQ(ierr);
      ierr  = PetscMemzero(iarray_start,xm*ym*sizeof(PetscScalar));CHKERRQ(ierr);

      ptr  = (void**)(iarray_start + xm*ym*sizeof(PetscScalar) - ys*sizeof(void*));
      for(j=ys;j<ys+ym;j++) {
        ptr[j] = iarray_start + sizeof(PetscScalar)*(xm*(j-ys) - xs);
      }
      *iptr = (void*)ptr; 
      break;}
    case 3: {
      void ***ptr,**bptr;

      ierr  = PetscMalloc((zm+1)*sizeof(void **)+(ym*zm+1)*sizeof(void*)+xm*ym*zm*sizeof(PetscScalar),&iarray_start);CHKERRQ(ierr);
      ierr  = PetscMemzero(iarray_start,xm*ym*zm*sizeof(PetscScalar));CHKERRQ(ierr);

      ptr  = (void***)(iarray_start + xm*ym*zm*sizeof(PetscScalar) - zs*sizeof(void*));
      bptr = (void**)(iarray_start + xm*ym*zm*sizeof(PetscScalar) + zm*sizeof(void**));
      for(i=zs;i<zs+zm;i++) {
        ptr[i] = bptr + ((i-zs)*ym* - ys)*sizeof(void*);
      }
      for (i=zs; i<zs+zm; i++) {
        for (j=ys; j<ys+ym; j++) {
          ptr[i][j] = iarray_start + sizeof(PetscScalar)*(xm*ym*(i-zs) + xm*(j-ys) - xs);
        }
      }

      *iptr = (void*)ptr; 
      break;}
    default:
      SETERRQ1(1,"Dimension %d not supported",da->dim);
  }

  done:
  /* add arrays to the checked out list */
  if (ghosted) {
    for (i=0; i<DA_MAX_WORK_ARRAYS; i++) {
      if (!da->arrayghostedout[i]) {
        da->arrayghostedout[i] = *iptr ;
        da->startghostedout[i] = iarray_start;
        break;
      }
    }
  } else {
    for (i=0; i<DA_MAX_WORK_ARRAYS; i++) {
      if (!da->arrayout[i]) {
        da->arrayout[i] = *iptr ;
        da->startout[i] = iarray_start;
        break;
      }
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DARestoreArray"
/*@C
     DARestoreArray - Restores an array of derivative types for a DA
          
    Input Parameter:
+    da - information about my local patch
.    ghosted - do you want arrays for the ghosted or nonghosted patch
-    ptr - array data structured to be passed to ad_FormFunctionLocal()

     Level: advanced

.seealso: DAGetArray(), DAGetAdicArray()

@*/
int DARestoreArray(DA da,PetscTruth ghosted,void **iptr)
{
  int  i;
  void *iarray_start = 0;
  
  PetscFunctionBegin;
  PetscValidHeaderSpecific(da,DA_COOKIE);
  if (ghosted) {
    for (i=0; i<DA_MAX_WORK_ARRAYS; i++) {
      if (da->arrayghostedout[i] == *iptr) {
        iarray_start           = da->startghostedout[i];
        da->arrayghostedout[i] = PETSC_NULL;
        da->startghostedout[i] = PETSC_NULL;
        break;
      }
    }
    for (i=0; i<DA_MAX_WORK_ARRAYS; i++) {
      if (!da->arrayghostedin[i]){
        da->arrayghostedin[i] = *iptr;
        da->startghostedin[i] = iarray_start;
        break;
      }
    }
  } else {
    for (i=0; i<DA_MAX_WORK_ARRAYS; i++) {
      if (da->arrayout[i] == *iptr) {
        iarray_start    = da->startout[i];
        da->arrayout[i] = PETSC_NULL;
        da->startout[i] = PETSC_NULL;
        break;
      }
    }
    for (i=0; i<DA_MAX_WORK_ARRAYS; i++) {
      if (!da->arrayin[i]){
        da->arrayin[i]  = *iptr;
        da->startin[i]  = iarray_start;
        break;
      }
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DAGetAdicMFArray"
/*@C
     DAGetAdicMFArray - Gets an array of derivative types for a DA for matrix-free ADIC.
          
     Input Parameter:
+    da - information about my local patch
-    ghosted - do you want arrays for the ghosted or nonghosted patch?

     Output Parameters:
+    ptr - array data structured to be passed to ad_FormFunctionLocal()
.    array_start - actual start of 1d array of all values that adiC can access directly (may be null)
-    tdof - total number of degrees of freedom represented in array_start (may be null)

     Notes: 
     This routine returns the same type of object as the DAVecGetArray(), except its
     elements are derivative types instead of PetscScalars.

     Level: advanced

.seealso: DARestoreAdicMFArray(), DAGetArray(), DAGetAdicArray()

@*/
int DAGetAdicMFArray(DA da,PetscTruth ghosted,void **iptr,void **array_start,int *tdof)
{
  int  ierr,j,i,xs,ys,xm,ym,zs,zm,itdof;
  char *iarray_start;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(da,DA_COOKIE);
  if (ghosted) {
    for (i=0; i<DA_MAX_AD_ARRAYS; i++) {
      if (da->admfarrayghostedin[i]) {
        *iptr                     = da->admfarrayghostedin[i];
        iarray_start              = (char*)da->admfstartghostedin[i];
        itdof                     = da->ghostedtdof;
        da->admfarrayghostedin[i] = PETSC_NULL;
        da->admfstartghostedin[i] = PETSC_NULL;
        
        goto done;
      }
    }
    xs = da->Xs;
    ys = da->Ys;
    zs = da->Zs;
    xm = da->Xe-da->Xs;
    ym = da->Ye-da->Ys;
    zm = da->Ze-da->Zs;
  } else {
    for (i=0; i<DA_MAX_AD_ARRAYS; i++) {
      if (da->admfarrayin[i]) {
        *iptr              = da->admfarrayin[i];
        iarray_start       = (char*)da->admfstartin[i];
        itdof              = da->tdof;
        da->admfarrayin[i] = PETSC_NULL;
        da->admfstartin[i] = PETSC_NULL;
        
        goto done;
      }
    }
    xs = da->xs;
    ys = da->ys;
    zs = da->zs;
    xm = da->xe-da->xs;
    ym = da->ye-da->ys;
    zm = da->ze-da->zs;
  }

  switch (da->dim) {
    case 1: {
      void *ptr;
      itdof = xm;

      ierr  = PetscMalloc(xm*2*sizeof(PetscScalar),&iarray_start);CHKERRQ(ierr);
      ierr  = PetscMemzero(iarray_start,xm*2*sizeof(PetscScalar));CHKERRQ(ierr);

      ptr   = (void*)(iarray_start - xs*2*sizeof(PetscScalar));
      *iptr = (void*)ptr; 
      break;}
    case 2: {
      void **ptr;
      itdof = xm*ym;

      ierr  = PetscMalloc((ym+1)*sizeof(void *)+xm*ym*2*sizeof(PetscScalar),&iarray_start);CHKERRQ(ierr);
      ierr  = PetscMemzero(iarray_start,xm*ym*2*sizeof(PetscScalar));CHKERRQ(ierr);

      ptr  = (void**)(iarray_start + xm*ym*2*sizeof(PetscScalar) - ys*sizeof(void*));
      for(j=ys;j<ys+ym;j++) {
        ptr[j] = iarray_start + 2*sizeof(PetscScalar)*(xm*(j-ys) - xs);
      }
      *iptr = (void*)ptr; 
      break;}
    case 3: {
      void ***ptr,**bptr;
      itdof = xm*ym*zm;

      ierr  = PetscMalloc((zm+1)*sizeof(void **)+(ym*zm+1)*sizeof(void*)+xm*ym*zm*2*sizeof(PetscScalar),&iarray_start);CHKERRQ(ierr);
      ierr  = PetscMemzero(iarray_start,xm*ym*zm*2*sizeof(PetscScalar));CHKERRQ(ierr);

      ptr  = (void***)(iarray_start + xm*ym*zm*2*sizeof(PetscScalar) - zs*sizeof(void*));
      bptr = (void**)(iarray_start + xm*ym*zm*2*sizeof(PetscScalar) + zm*sizeof(void**));
      for(i=zs;i<zs+zm;i++) {
        ptr[i] = bptr + ((i-zs)*ym* - ys)*sizeof(void*);
      }
      for (i=zs; i<zs+zm; i++) {
        for (j=ys; j<ys+ym; j++) {
          ptr[i][j] = iarray_start + 2*sizeof(PetscScalar)*(xm*ym*(i-zs) + xm*(j-ys) - xs);
        }
      }

      *iptr = (void*)ptr; 
      break;}
    default:
      SETERRQ1(1,"Dimension %d not supported",da->dim);
  }

  done:
  /* add arrays to the checked out list */
  if (ghosted) {
    for (i=0; i<DA_MAX_AD_ARRAYS; i++) {
      if (!da->admfarrayghostedout[i]) {
        da->admfarrayghostedout[i] = *iptr ;
        da->admfstartghostedout[i] = iarray_start;
        da->ghostedtdof            = itdof;
        break;
      }
    }
  } else {
    for (i=0; i<DA_MAX_AD_ARRAYS; i++) {
      if (!da->admfarrayout[i]) {
        da->admfarrayout[i] = *iptr ;
        da->admfstartout[i] = iarray_start;
        da->tdof            = itdof;
        break;
      }
    }
  }
  if (i == DA_MAX_AD_ARRAYS+1) SETERRQ(1,"Too many DA ADIC arrays obtained");
  if (tdof)        *tdof = itdof;
  if (array_start) *array_start = iarray_start;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DARestoreAdicMFArray"
/*@C
     DARestoreAdicMFArray - Restores an array of derivative types for a DA.
          
     Input Parameter:
+    da - information about my local patch
-    ghosted - do you want arrays for the ghosted or nonghosted patch?

     Output Parameters:
+    ptr - array data structure to be passed to ad_FormFunctionLocal()
.    array_start - actual start of 1d array of all values that adiC can access directly
-    tdof - total number of degrees of freedom represented in array_start

     Level: advanced

.seealso: DAGetAdicArray()

@*/
int DARestoreAdicMFArray(DA da,PetscTruth ghosted,void **iptr,void **array_start,int *tdof)
{
  int  i;
  void *iarray_start = 0;
  
  PetscFunctionBegin;
  PetscValidHeaderSpecific(da,DA_COOKIE);
  if (ghosted) {
    for (i=0; i<DA_MAX_AD_ARRAYS; i++) {
      if (da->admfarrayghostedout[i] == *iptr) {
        iarray_start               = da->admfstartghostedout[i];
        da->admfarrayghostedout[i] = PETSC_NULL;
        da->admfstartghostedout[i] = PETSC_NULL;
        break;
      }
    }
    if (!iarray_start) SETERRQ(1,"Could not find array in checkout list");
    for (i=0; i<DA_MAX_AD_ARRAYS; i++) {
      if (!da->admfarrayghostedin[i]){
        da->admfarrayghostedin[i] = *iptr;
        da->admfstartghostedin[i] = iarray_start;
        break;
      }
    }
  } else {
    for (i=0; i<DA_MAX_AD_ARRAYS; i++) {
      if (da->admfarrayout[i] == *iptr) {
        iarray_start        = da->admfstartout[i];
        da->admfarrayout[i] = PETSC_NULL;
        da->admfstartout[i] = PETSC_NULL;
        break;
      }
    }
    if (!iarray_start) SETERRQ(1,"Could not find array in checkout list");
    for (i=0; i<DA_MAX_AD_ARRAYS; i++) {
      if (!da->admfarrayin[i]){
        da->admfarrayin[i] = *iptr;
        da->admfstartin[i] = iarray_start;
        break;
      }
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "admf_DAGetArray"
int admf_DAGetArray(DA da,PetscTruth ghosted,void **iptr)
{
  int ierr;
  PetscFunctionBegin;
  ierr = DAGetAdicMFArray(da,ghosted,iptr,0,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "admf_DARestoreArray"
int admf_DARestoreArray(DA da,PetscTruth ghosted,void **iptr)
{
  int ierr;
  PetscFunctionBegin;
  ierr = DARestoreAdicMFArray(da,ghosted,iptr,0,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
