/*$Id: shvec.c,v 1.53 2001/09/07 20:09:02 bsmith Exp $*/

/*
   This file contains routines for Parallel vector operations that use shared memory
 */
#include "src/vec/impls/mpi/pvecimpl.h"   /*I  "petscvec.h"   I*/

/*
     Could not get the include files to work properly on the SGI with 
  the C++ compiler.
*/
#if defined(PETSC_USE_SHARED_MEMORY) && !defined(__cplusplus)

EXTERN int PetscSharedMalloc(MPI_Comm,int,int,void**);

#undef __FUNCT__  
#define __FUNCT__ "VecDuplicate_Shared"
int VecDuplicate_Shared(Vec win,Vec *v)
{
  int          ierr;
  Vec_MPI      *vw,*w = (Vec_MPI *)win->data;
  PetscScalar  *array;

  PetscFunctionBegin;

  /* first processor allocates entire array and sends it's address to the others */
  ierr = PetscSharedMalloc(win->comm,win->n*sizeof(PetscScalar),win->N*sizeof(PetscScalar),(void**)&array);CHKERRQ(ierr);

  ierr = VecCreate(win->comm,win->n,win->N,v);CHKERRQ(ierr);
  ierr = VecCreate_MPI_Private(*v,w->nghost,array,win->map);CHKERRQ(ierr);
  vw   = (Vec_MPI *)(*v)->data;

  /* New vector should inherit stashing property of parent */
  (*v)->stash.donotstash = win->stash.donotstash;
  
  ierr = PetscOListDuplicate(win->olist,&(*v)->olist);CHKERRQ(ierr);
  ierr = PetscFListDuplicate(win->qlist,&(*v)->qlist);CHKERRQ(ierr);

  if (win->mapping) {
    (*v)->mapping = win->mapping;
    ierr = PetscObjectReference((PetscObject)win->mapping);CHKERRQ(ierr);
  }
  (*v)->ops->duplicate = VecDuplicate_Shared;
  (*v)->bs        = win->bs;
  (*v)->bstash.bs = win->bstash.bs;
  PetscFunctionReturn(0);
}


EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "VecCreate_Shared"
int VecCreate_Shared(Vec vv, ParameterDict dict)
{
  int          ierr;
  PetscScalar  *array;

  PetscFunctionBegin;
  ierr = PetscSplitOwnership(vv->comm,&vv->n,&vv->N);CHKERRQ(ierr);
  ierr = PetscSharedMalloc(vv->comm,vv->n*sizeof(PetscScalar),vv->N*sizeof(PetscScalar),(void**)&array);CHKERRQ(ierr); 

  ierr = VecCreate_MPI_Private(vv,0,array,PETSC_NULL);CHKERRQ(ierr);
  vv->ops->duplicate = VecDuplicate_Shared;

  PetscFunctionReturn(0);
}
EXTERN_C_END


/* ----------------------------------------------------------------------------------------
     Code to manage shared memory allocation under the SGI with MPI

  We associate with a communicator a shared memory "areana" from which memory may be shmalloced.
*/
#include "petscsys.h"
#include "petscfix.h"
#if defined(HAVE_PWD_H)
#include <pwd.h>
#endif
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
#if defined(HAVE_STDLIB_H)
#include <stdlib.h>
#endif
#if !defined(PARCH_win32)
#include <sys/param.h>
#include <sys/utsname.h>
#endif
#if defined(PARCH_win32)
#include <windows.h>
#include <io.h>
#include <direct.h>
#endif
#if defined (PARCH_win32_gnu)
#include <windows.h>
#endif
#include <fcntl.h>
#include <time.h>  
#if defined(HAVE_SYS_SYSTEMINFO_H)
#include <sys/systeminfo.h>
#endif
#include "petscfix.h"

static int Petsc_Shared_keyval = MPI_KEYVAL_INVALID;
static int Petsc_Shared_size   = 100000000;

#undef __FUNCT__  
#define __FUNCT__ "Petsc_DeleteShared" 
/*
   Private routine to delete internal storage when a communicator is freed.
  This is called by MPI, not by users.

  The binding for the first argument changed from MPI 1.0 to 1.1; in 1.0
  it was MPI_Comm *comm.  
*/
static int Petsc_DeleteShared(MPI_Comm comm,int keyval,void* attr_val,void* extra_state)
{
  int ierr;

  PetscFunctionBegin;
  ierr = PetscFree(attr_val);CHKERRQ(ierr);
  PetscFunctionReturn(MPI_SUCCESS);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscSharedMemorySetSize"
int PetscSharedMemorySetSize(int s)
{
  PetscFunctionBegin;
  Petsc_Shared_size = s;
  PetscFunctionReturn(0);
}

#include "petscfix.h"

#include <ulocks.h>

#undef __FUNCT__  
#define __FUNCT__ "PetscSharedInitialize"
int PetscSharedInitialize(MPI_Comm comm)
{
  int     rank,len,ierr,flag;
  char    filename[256];
  usptr_t **arena;

  PetscFunctionBegin;

  if (Petsc_Shared_keyval == MPI_KEYVAL_INVALID) {
    /* 
       The calling sequence of the 2nd argument to this function changed
       between MPI Standard 1.0 and the revisions 1.1 Here we match the 
       new standard, if you are using an MPI implementation that uses 
       the older version you will get a warning message about the next line;
       it is only a warning message and should do no harm.
    */
    ierr = MPI_Keyval_create(MPI_NULL_COPY_FN,Petsc_DeleteShared,&Petsc_Shared_keyval,0);CHKERRQ(ierr);
  }

  ierr = MPI_Attr_get(comm,Petsc_Shared_keyval,(void**)&arena,&flag);CHKERRQ(ierr);

  if (!flag) {
    /* This communicator does not yet have a shared memory areana */
    ierr = PetscMalloc(sizeof(usptr_t*),&arena);CHKERRQ(ierr);

    ierr = MPI_Comm_rank(comm,&rank);CHKERRQ(ierr);
    if (!rank) {
      ierr = PetscStrcpy(filename,"/tmp/PETScArenaXXXXXX");CHKERRQ(ierr);
      mktemp(filename);
      ierr = PetscStrlen(filename,&len);CHKERRQ(ierr);
    } 
    ierr     = MPI_Bcast(&len,1,MPI_INT,0,comm);CHKERRQ(ierr);
    ierr     = MPI_Bcast(filename,len+1,MPI_CHAR,0,comm);CHKERRQ(ierr);
    ierr     = PetscOptionsGetInt(PETSC_NULL,"-shared_size",&Petsc_Shared_size,&flag);CHKERRQ(ierr);
    usconfig(CONF_INITSIZE,Petsc_Shared_size);
    *arena   = usinit(filename); 
    ierr     = MPI_Attr_put(comm,Petsc_Shared_keyval,arena);CHKERRQ(ierr);
  } 

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscSharedMalloc"
int PetscSharedMalloc(MPI_Comm comm,int llen,int len,void **result)
{
  char    *value;
  int     ierr,shift,rank,flag;
  usptr_t **arena;

  PetscFunctionBegin;
  *result = 0;
  if (Petsc_Shared_keyval == MPI_KEYVAL_INVALID) {
    ierr = PetscSharedInitialize(comm);CHKERRQ(ierr);
  }
  ierr = MPI_Attr_get(comm,Petsc_Shared_keyval,(void**)&arena,&flag);CHKERRQ(ierr);
  if (!flag) { 
    ierr = PetscSharedInitialize(comm);CHKERRQ(ierr);
    ierr = MPI_Attr_get(comm,Petsc_Shared_keyval,(void**)&arena,&flag);CHKERRQ(ierr);
    if (!flag) SETERRQ(1,"Unable to initialize shared memory");
  } 

  ierr   = MPI_Scan(&llen,&shift,1,MPI_INT,MPI_SUM,comm);CHKERRQ(ierr);
  shift -= llen;

  ierr = MPI_Comm_rank(comm,&rank);CHKERRQ(ierr);
  if (!rank) {
    value = (char*)usmalloc((size_t) len,*arena);
    if (!value) {
      (*PetscErrorPrintf)("PETSC ERROR: Unable to allocate shared memory location\n");
      (*PetscErrorPrintf)("PETSC ERROR: Run with option -shared_size <size> \n");
      (*PetscErrorPrintf)("PETSC_ERROR: with size > %d \n",(int)(1.2*(Petsc_Shared_size+len)));
      SETERRQ(1,"Unable to malloc shared memory");
    }
  }
  ierr = MPI_Bcast(&value,8,MPI_BYTE,0,comm);CHKERRQ(ierr);
  value += shift; 

  PetscFunctionReturn(0);
}

#else

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "VecCreate_Shared"
int VecCreate_Shared(MPI_Comm comm,int n,int N,Vec *vv)
{
  int ierr,size;

  PetscFunctionBegin;
  ierr = MPI_Comm_size(comm,&size);CHKERRQ(ierr);
  if (size > 1) {
    SETERRQ(1,"No supported for shared memory vector objects on this machine");
  }
  ierr = VecCreateSeq(comm,PetscMax(n,N),vv);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END

#endif

#undef __FUNCT__  
#define __FUNCT__ "VecCreateShared"
/*@C
   VecCreateShared - Creates a parallel vector that uses shared memory.

   Input Parameters:
.  comm - the MPI communicator to use
.  n - local vector length (or PETSC_DECIDE to have calculated if N is given)
.  N - global vector length (or PETSC_DECIDE to have calculated if n is given)

   Output Parameter:
.  vv - the vector

   Collective on MPI_Comm
 
   Notes:
   Currently VecCreateShared() is available only on the SGI; otherwise,
   this routine is the same as VecCreateMPI().

   Use VecDuplicate() or VecDuplicateVecs() to form additional vectors of the
   same type as an existing vector.

   Level: advanced

   Concepts: vectors^creating with shared memory

.seealso: VecCreateSeq(), VecCreate(), VecCreateMPI(), VecDuplicate(), VecDuplicateVecs(), 
          VecCreateGhost(), VecCreateMPIWithArray(), VecCreateGhostWithArray()

@*/ 
int VecCreateShared(MPI_Comm comm,int n,int N,Vec *v)
{
  int ierr;

  PetscFunctionBegin;
  ierr = VecCreate(comm,v);CHKERRQ(ierr);
  ierr = VecSetSize(*v,n,N);CHKERRQ(ierr);
  ierr = VecSetType(*v,VEC_SHARED);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}





