/*
     Provides utility routines for manipulating any type of PETSc object.
*/
#include "petsc.h"  /*I   "petsc.h"    I*/
#include "petscsys.h"

EXTERN PetscErrorCode PetscObjectGetComm_Petsc(PetscObject,MPI_Comm *);
EXTERN PetscErrorCode PetscObjectCompose_Petsc(PetscObject,const char[],PetscObject);
EXTERN PetscErrorCode PetscObjectQuery_Petsc(PetscObject,const char[],PetscObject *);
EXTERN PetscErrorCode PetscObjectComposeFunction_Petsc(PetscObject,const char[],const char[],void (*)(void));
EXTERN PetscErrorCode PetscObjectQueryFunction_Petsc(PetscObject,const char[],void (**)(void));
EXTERN PetscErrorCode PetscObjectComposeLanguage_Petsc(PetscObject,PetscLanguage,void *);
EXTERN PetscErrorCode PetscObjectQueryLanguage_Petsc(PetscObject,PetscLanguage,void **);

#undef __FUNCT__  
#define __FUNCT__ "PetscHeaderCreate_Private"
/*
   PetscHeaderCreate_Private - Creates a base PETSc object header and fills
   in the default values.  Called by the macro PetscHeaderCreate().
*/
PetscErrorCode PetscHeaderCreate_Private(PetscObject h,int cookie,int type,const char class_name[],MPI_Comm comm,
                              int (*des)(PetscObject),int (*vie)(PetscObject,PetscViewer))
{
  static int idcnt = 1;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  h->cookie                 = cookie;
  h->type                   = type;
  h->class_name             = (char*)class_name;
  h->prefix                 = 0;
  h->refct                  = 1;
  h->amem                   = -1;
  h->id                     = idcnt++;
  h->parentid               = 0;
  h->qlist                  = 0;
  h->olist                  = 0;
  h->bops->destroy          = des;
  h->bops->view             = vie;
  h->bops->getcomm          = PetscObjectGetComm_Petsc;
  h->bops->compose          = PetscObjectCompose_Petsc;
  h->bops->query            = PetscObjectQuery_Petsc;
  h->bops->composefunction  = PetscObjectComposeFunction_Petsc;
  h->bops->queryfunction    = PetscObjectQueryFunction_Petsc;
  h->bops->querylanguage    = PetscObjectQueryLanguage_Petsc;
  h->bops->composelanguage  = PetscObjectComposeLanguage_Petsc;
  ierr = PetscCommDuplicate(comm,&h->comm,&h->tag);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscHeaderDestroy_Private"
/*
    PetscHeaderDestroy_Private - Destroys a base PETSc object header. Called by 
    the macro PetscHeaderDestroy().
*/
PetscErrorCode PetscHeaderDestroy_Private(PetscObject h)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (h->amem != -1) {
    SETERRQ(PETSC_ERR_ORDER,"PETSc object destroyed before its AMS publication was destroyed");
  }

  ierr = PetscCommDestroy(&h->comm);CHKERRQ(ierr);
  ierr = PetscFree(h->bops);CHKERRQ(ierr);
  ierr = PetscFree(h->ops);CHKERRQ(ierr);
  ierr = PetscOListDestroy(&h->olist);CHKERRQ(ierr);
  ierr = PetscFListDestroy(&h->qlist);CHKERRQ(ierr);
  ierr = PetscStrfree(h->type_name);CHKERRQ(ierr);
  ierr = PetscStrfree(h->name);CHKERRQ(ierr);
  h->cookie = PETSCFREEDHEADER;
  ierr = PetscStrfree(h->prefix);CHKERRQ(ierr);
  if (h->dict) {
    ierr = ParameterDictDestroy(h->dict);CHKERRQ(ierr);
  }
  if (h->fortran_func_pointers) {
    ierr = PetscFree(h->fortran_func_pointers);CHKERRQ(ierr);
  }
  if (h->intcomposeddata) {
    ierr = PetscFree(h->intcomposeddata);CHKERRQ(ierr);
  }
  if (h->intcomposedstate) {
    ierr = PetscFree(h->intcomposedstate);CHKERRQ(ierr);
  }
  if (h->realcomposeddata) {
    ierr = PetscFree(h->realcomposeddata);CHKERRQ(ierr);
  }
  if (h->realcomposedstate) {
    ierr = PetscFree(h->realcomposedstate);CHKERRQ(ierr);
  }
  if (h->scalarcomposeddata) {
    ierr = PetscFree(h->scalarcomposeddata);CHKERRQ(ierr);
  }
  if (h->scalarcomposedstate) {
    ierr = PetscFree(h->scalarcomposedstate);CHKERRQ(ierr);
  }
  ierr = PetscFree(h);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscObjectReference"
/*@C
   PetscObjectReference - Indicates to any PetscObject that it is being
   referenced by another PetscObject. This increases the reference
   count for that object by one.

   Collective on PetscObject

   Input Parameter:
.  obj - the PETSc object. This must be cast with (PetscObject), for example, 
         PetscObjectReference((PetscObject)mat);

   Level: advanced

.seealso: PetscObjectCompose(), PetscObjectDereference()
@*/
PetscErrorCode PetscObjectReference(PetscObject obj)
{
  PetscFunctionBegin;
  PetscValidHeader(obj,1);
  obj->refct++;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscObjectGetReference"
/*@C
   PetscObjectGetReference - Gets the current reference count for 
   any PETSc object.

   Not Collective

   Input Parameter:
.  obj - the PETSc object; this must be cast with (PetscObject), for example, 
         PetscObjectGetReference((PetscObject)mat,&cnt);

   Output Parameter:
.  cnt - the reference count

   Level: advanced

.seealso: PetscObjectCompose(), PetscObjectDereference(), PetscObjectReference()
@*/
PetscErrorCode PetscObjectGetReference(PetscObject obj,int *cnt)
{
  PetscFunctionBegin;
  PetscValidHeader(obj,1);
  PetscValidIntPointer(cnt,2);
  *cnt = obj->refct;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscObjectDereference"
/*@
   PetscObjectDereference - Indicates to any PetscObject that it is being
   referenced by one less PetscObject. This decreases the reference
   count for that object by one.

   Collective on PetscObject

   Input Parameter:
.  obj - the PETSc object; this must be cast with (PetscObject), for example, 
         PetscObjectDereference((PetscObject)mat);

   Level: advanced

.seealso: PetscObjectCompose(), PetscObjectReference()
@*/
PetscErrorCode PetscObjectDereference(PetscObject obj)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeader(obj,1);
  if (obj->bops->destroy) {
    ierr = (*obj->bops->destroy)(obj);CHKERRQ(ierr);
  } else if (!--obj->refct) {
    SETERRQ(PETSC_ERR_SUP,"This PETSc object does not have a generic destroy routine");
  }
  PetscFunctionReturn(0);
}

/* ----------------------------------------------------------------------- */
/*
     The following routines are the versions private to the PETSc object
     data structures.
*/
#undef __FUNCT__  
#define __FUNCT__ "PetscObjectGetComm_Petsc"
PetscErrorCode PetscObjectGetComm_Petsc(PetscObject obj,MPI_Comm *comm)
{
  PetscFunctionBegin;
  *comm = obj->comm;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscObjectCompose_Petsc"
PetscErrorCode PetscObjectCompose_Petsc(PetscObject obj,const char name[],PetscObject ptr)
{
  PetscErrorCode ierr;
  char *tname;

  PetscFunctionBegin;
  if (ptr) {
    ierr = PetscOListReverseFind(ptr->olist,obj,&tname);CHKERRQ(ierr);
    if (tname){
      SETERRQ(PETSC_ERR_ARG_INCOMP,"An object cannot be composed with an object that was compose with it");
    }
  }
  ierr = PetscOListAdd(&obj->olist,name,ptr);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscObjectQuery_Petsc"
PetscErrorCode PetscObjectQuery_Petsc(PetscObject obj,const char name[],PetscObject *ptr)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscOListFind(obj->olist,name,ptr);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscObjectComposeLanguage_Petsc"
PetscErrorCode PetscObjectComposeLanguage_Petsc(PetscObject obj,PetscLanguage lang,void *vob)
{
  PetscFunctionBegin;
  if (lang == PETSC_LANGUAGE_CPP) {
    obj->cpp = vob;
  } else {
    SETERRQ(PETSC_ERR_SUP,"No support for this language yet");
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscObjectQueryLanguage_Petsc"
PetscErrorCode PetscObjectQueryLanguage_Petsc(PetscObject obj,PetscLanguage lang,void **vob)
{
  PetscFunctionBegin;
  if (lang == PETSC_LANGUAGE_C) {
    *vob = (void*)obj;
  } else if (lang == PETSC_LANGUAGE_CPP) {
    if (obj->cpp) {
      *vob = obj->cpp;
    } else {
      SETERRQ(PETSC_ERR_SUP,"No C++ wrapper generated");
    }
  } else {
    SETERRQ(PETSC_ERR_SUP,"No support for this language yet");
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscObjectComposeFunction_Petsc"
PetscErrorCode PetscObjectComposeFunction_Petsc(PetscObject obj,const char name[],const char fname[],void (*ptr)(void))
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscFListAdd(&obj->qlist,name,fname,ptr);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscObjectQueryFunction_Petsc"
PetscErrorCode PetscObjectQueryFunction_Petsc(PetscObject obj,const char name[],void (**ptr)(void))
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscFListFind(obj->comm,obj->qlist,name,ptr);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*
        These are the versions that are usable to any CCA compliant objects
*/
#undef __FUNCT__  
#define __FUNCT__ "PetscObjectCompose"
/*@C
   PetscObjectCompose - Associates another PETSc object with a given PETSc object. 
                       
   Not Collective

   Input Parameters:
+  obj - the PETSc object; this must be cast with (PetscObject), for example, 
         PetscObjectCompose((PetscObject)mat,...);
.  name - name associated with the child object 
-  ptr - the other PETSc object to associate with the PETSc object; this must also be 
         cast with (PetscObject)

   Level: advanced

   Notes:
   The second objects reference count is automatically increased by one when it is
   composed.

   Replaces any previous object that had the same name.

   If ptr is null and name has previously been composed using an object, then that
   entry is removed from the obj.

   PetscObjectCompose() can be used with any PETSc object (such as
   Mat, Vec, KSP, SNES, etc.) or any user-provided object.  See 
   PetscObjectContainerCreate() for info on how to create an object from a 
   user-provided pointer that may then be composed with PETSc objects.
   
   Concepts: objects^composing
   Concepts: composing objects

.seealso: PetscObjectQuery(), PetscObjectContainerCreate()
@*/
PetscErrorCode PetscObjectCompose(PetscObject obj,const char name[],PetscObject ptr)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = (*obj->bops->compose)(obj,name,ptr);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscObjectQuery"
/*@C
   PetscObjectQuery  - Gets a PETSc object associated with a given object.
                       
   Not Collective

   Input Parameters:
+  obj - the PETSc object
         Thus must be cast with a (PetscObject), for example, 
         PetscObjectCompose((PetscObject)mat,...);
.  name - name associated with child object 
-  ptr - the other PETSc object associated with the PETSc object, this must also be 
         cast with (PetscObject)

   Level: advanced

   Concepts: objects^composing
   Concepts: composing objects
   Concepts: objects^querying
   Concepts: querying objects

.seealso: PetscObjectQuery()
@*/
PetscErrorCode PetscObjectQuery(PetscObject obj,const char name[],PetscObject *ptr)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = (*obj->bops->query)(obj,name,ptr);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscObjectQueryLanguage"
/*@C
   PetscObjectQueryLanguage - Returns a language specific interface to the given object
                       
   Not Collective

   Input Parameters:
+  obj - the PETSc object
         Thus must be cast with a (PetscObject), for example, 
         PetscObjectCompose((PetscObject)mat,...);
-  lang - one of PETSC_LANGUAGE_C, PETSC_LANGUAGE_F77, PETSC_LANGUAGE_CPP

   Output Parameter:
.  ptr - the language specific interface

   Level: developer

.seealso: PetscObjectQuery()
@*/
PetscErrorCode PetscObjectQueryLanguage(PetscObject obj,PetscLanguage lang,void **ptr)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = (*obj->bops->querylanguage)(obj,lang,ptr);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscObjectComposeLanguage"
/*@C
   PetscObjectComposeLanguage - Sets a language specific interface to the given object
                       
   Not Collective

   Input Parameters:
+  obj - the PETSc object
         Thus must be cast with a (PetscObject), for example, 
         PetscObjectCompose((PetscObject)mat,...);
.  lang - one of PETSC_LANGUAGE_C, PETSC_LANGUAGE_F77, PETSC_LANGUAGE_CPP
-  ptr - the language specific interface

   Level: developer

.seealso: PetscObjectQuery()
@*/
PetscErrorCode PetscObjectComposeLanguage(PetscObject obj,PetscLanguage lang,void *ptr)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = (*obj->bops->composelanguage)(obj,lang,ptr);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "PetscObjectComposeFunction"
PetscErrorCode PetscObjectComposeFunction(PetscObject obj,const char name[],const char fname[],void (*ptr)(void))
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = (*obj->bops->composefunction)(obj,name,fname,ptr);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscObjectQueryFunction"
/*@C
   PetscObjectQueryFunction - Gets a function associated with a given object.
                       
   Collective on PetscObject

   Input Parameters:
+  obj - the PETSc object; this must be cast with (PetscObject), for example, 
         PetscObjectQueryFunction((PetscObject)ksp,...);
-  name - name associated with the child function

   Output Parameter:
.  ptr - function pointer

   Level: advanced

   Concepts: objects^composing functions
   Concepts: composing functions
   Concepts: functions^querying
   Concepts: objects^querying
   Concepts: querying objects

.seealso: PetscObjectComposeFunctionDynamic()
@*/
PetscErrorCode PetscObjectQueryFunction(PetscObject obj,const char name[],void (**ptr)(void))
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = (*obj->bops->queryfunction)(obj,name,ptr);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscObjectSetParameterDict"
/*@C
  PetscObjectSetParameterDict - Sets a parameter dictionary for an object

  Input Parameters:
+ obj  - The PetscObject
- dict - The ParameterDict

  Level: intermediate

.seealso PetscObjectGetParameterDict()
@*/
PetscErrorCode PetscObjectSetParameterDict(PetscObject obj, ParameterDict dict) {
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeader(obj,1);
  if (obj->dict != PETSC_NULL) {
    ierr = PetscObjectDereference((PetscObject) obj->dict);CHKERRQ(ierr);
  }
  if (dict != PETSC_NULL) {
    ierr = PetscObjectReference((PetscObject) dict);CHKERRQ(ierr);
  }
  obj->dict = dict;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscObjectGetParameterDict"
/*@C
  PetscObjectGetParameterDict - Gets the parameter dictionary for an object

  Input Parameter:
. obj  - The PetscObject

  Output Parameter:
. dict - The ParameterDict

  Level: intermediate

.seealso PetscObjectSetParameterDict()
@*/
PetscErrorCode PetscObjectGetParameterDict(PetscObject obj, ParameterDict *dict) {
  PetscFunctionBegin;
  PetscValidHeader(obj,1);
  PetscValidPointer(dict,2);
  *dict = obj->dict;
  PetscFunctionReturn(0);
}

struct _p_PetscObjectContainer {
  PETSCHEADER(int)
  void   *ptr;
};

#undef __FUNCT__  
#define __FUNCT__ "PetscObjectContainerGetPointer"
/*@C
   PetscObjectContainerGetPointer - Gets the pointer value contained in the container.

   Collective on PetscObjectContainer

   Input Parameter:
.  obj - the object created with PetscObjectContainerCreate()

   Output Parameter:
.  ptr - the pointer value

   Level: advanced

.seealso: PetscObjectContainerCreate(), PetscObjectContainerDestroy(), 
          PetscObjectContainerSetPointer()
@*/
PetscErrorCode PetscObjectContainerGetPointer(PetscObjectContainer obj,void **ptr)
{
  PetscFunctionBegin;
  *ptr = obj->ptr;
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "PetscObjectContainerSetPointer"
/*@C
   PetscObjectContainerSetPointer - Sets the pointer value contained in the container.

   Collective on PetscObjectContainer

   Input Parameters:
+  obj - the object created with PetscObjectContainerCreate()
-  ptr - the pointer value

   Level: advanced

.seealso: PetscObjectContainerCreate(), PetscObjectContainerDestroy(), 
          PetscObjectContainerGetPointer()
@*/
PetscErrorCode PetscObjectContainerSetPointer(PetscObjectContainer obj,void *ptr)
{
  PetscFunctionBegin;
  obj->ptr = ptr;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscObjectContainerDestroy"
/*@C
   PetscObjectContainerDestroy - Destroys a PETSc container object.

   Collective on PetscObjectContainer

   Input Parameter:
.  obj - an object that was created with PetscObjectContainerCreate()

   Level: advanced

.seealso: PetscObjectContainerCreate()
@*/
PetscErrorCode PetscObjectContainerDestroy(PetscObjectContainer obj)
{
  PetscFunctionBegin;
  if (--obj->refct > 0) PetscFunctionReturn(0);
  PetscHeaderDestroy(obj);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscObjectContainerCreate"
/*@C
   PetscObjectContainerCreate - Creates a PETSc object that has room to hold
   a single pointer. This allows one to attach any type of data (accessible
   through a pointer) with the PetscObjectCompose() function to a PetscObject.
   The data item itself is attached by a call to PetscObjectContainerSetPointer.

   Collective on MPI_Comm

   Input Parameters:
.  comm - MPI communicator that shares the object

   Output Parameters:
.  container - the container created

   Level: advanced

.seealso: PetscObjectContainerDestroy(), PetscObjectContainerSetPointer(), PetscObjectContainerSetPointer()
@*/
PetscErrorCode PetscObjectContainerCreate(MPI_Comm comm,PetscObjectContainer *container)
{
  PetscObjectContainer contain;

  PetscFunctionBegin;
  PetscHeaderCreate(contain,_p_PetscObjectContainer,int,PETSC_COOKIE,0,"container",comm,PetscObjectContainerDestroy,0);
  *container = contain;
  PetscFunctionReturn(0);
}

