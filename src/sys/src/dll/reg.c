/*$Id: reg.c,v 1.77 2001/09/07 20:08:26 bsmith Exp $*/
/*
    Provides a general mechanism to allow one to register new routines in
    dynamic libraries for many of the PETSc objects (including, e.g., KSP and PC).
*/
#include "petsc.h"           /*I "petsc.h" I*/
#include "petscsys.h"

#undef __FUNCT__  
#define __FUNCT__ "PetscFListGetPathAndFunction"
int PetscFListGetPathAndFunction(const char name[],char *path[],char *function[])
{
  char work[256],*lfunction,ierr;

  PetscFunctionBegin;
  ierr = PetscStrncpy(work,name,256);CHKERRQ(ierr);
  ierr = PetscStrchr(work,':',&lfunction);CHKERRQ(ierr);
  if (lfunction != work && lfunction && lfunction[1] != ':') {
    lfunction[0] = 0;
    ierr = PetscStrallocpy(work,path);CHKERRQ(ierr);
    ierr = PetscStrallocpy(lfunction+1,function);CHKERRQ(ierr);
  } else {
    *path = 0;
    ierr = PetscStrallocpy(name,function);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#if defined(PETSC_USE_DYNAMIC_LIBRARIES)

/*
    This is the list used by the DLRegister routines
*/
PetscDLLibraryList DLLibrariesLoaded = 0;

#undef __FUNCT__  
#define __FUNCT__ "PetscInitialize_DynamicLibraries"
/*
    PetscInitialize_DynamicLibraries - Adds the default dynamic link libraries to the 
    search path.
*/ 
int PetscInitialize_DynamicLibraries(void)
{
  char       *libname[32],libs[256],dlib[1024];
  int        nmax,i,ierr;
  PetscTruth found;

  PetscFunctionBegin;

  nmax = 32;
  ierr = PetscOptionsGetStringArray(PETSC_NULL,"-dll_prepend",libname,&nmax,PETSC_NULL);CHKERRQ(ierr);
  for (i=0; i<nmax; i++) {
    ierr = PetscDLLibraryPrepend(PETSC_COMM_WORLD,&DLLibrariesLoaded,libname[i]);CHKERRQ(ierr);
    ierr = PetscFree(libname[i]);CHKERRQ(ierr);
  }

  ierr = PetscStrcpy(libs,PETSC_LIB_DIR);CHKERRQ(ierr);
  ierr = PetscStrcat(libs,"/libpetsc");CHKERRQ(ierr);
  ierr = PetscDLLibraryRetrieve(PETSC_COMM_WORLD,libs,dlib,1024,&found);CHKERRQ(ierr);
  if (found) {
    ierr = PetscDLLibraryAppend(PETSC_COMM_WORLD,&DLLibrariesLoaded,libs);CHKERRQ(ierr);
  } else {
    SETERRQ1(1,"Unable to locate PETSc dynamic library %s \n You cannot move the dynamic libraries!\n or remove USE_DYNAMIC_LIBRARIES from ${PETSC_DIR}/bmake/$PETSC_ARCH/petscconf.h\n and rebuild libraries before moving",libs);
  }

  ierr = PetscStrcpy(libs,PETSC_LIB_DIR);CHKERRQ(ierr);
  ierr = PetscStrcat(libs,"/libpetscvec");CHKERRQ(ierr);
  ierr = PetscDLLibraryRetrieve(PETSC_COMM_WORLD,libs,dlib,1024,&found);CHKERRQ(ierr);
  if (found) {
    ierr = PetscDLLibraryAppend(PETSC_COMM_WORLD,&DLLibrariesLoaded,libs);CHKERRQ(ierr);
  }

  ierr = PetscStrcpy(libs,PETSC_LIB_DIR);CHKERRQ(ierr);
  ierr = PetscStrcat(libs,"/libpetscmat");CHKERRQ(ierr);
  ierr = PetscDLLibraryRetrieve(PETSC_COMM_WORLD,libs,dlib,1024,&found);CHKERRQ(ierr);
  if (found) {
    ierr = PetscDLLibraryAppend(PETSC_COMM_WORLD,&DLLibrariesLoaded,libs);CHKERRQ(ierr);
  }

  ierr = PetscStrcpy(libs,PETSC_LIB_DIR);CHKERRQ(ierr);
  ierr = PetscStrcat(libs,"/libpetscdm");CHKERRQ(ierr);
  ierr = PetscDLLibraryRetrieve(PETSC_COMM_WORLD,libs,dlib,1024,&found);CHKERRQ(ierr);
  if (found) {
    ierr = PetscDLLibraryAppend(PETSC_COMM_WORLD,&DLLibrariesLoaded,libs);CHKERRQ(ierr);
  }

  ierr = PetscStrcpy(libs,PETSC_LIB_DIR);CHKERRQ(ierr);
  ierr = PetscStrcat(libs,"/libpetscsles");CHKERRQ(ierr);
  ierr = PetscDLLibraryRetrieve(PETSC_COMM_WORLD,libs,dlib,1024,&found);CHKERRQ(ierr);
  if (found) {
    ierr = PetscDLLibraryAppend(PETSC_COMM_WORLD,&DLLibrariesLoaded,libs);CHKERRQ(ierr);
  }

  ierr = PetscStrcpy(libs,PETSC_LIB_DIR);CHKERRQ(ierr);
  ierr = PetscStrcat(libs,"/libpetscsnes");CHKERRQ(ierr);
  ierr = PetscDLLibraryRetrieve(PETSC_COMM_WORLD,libs,dlib,1024,&found);CHKERRQ(ierr);
  if (found) {
    ierr = PetscDLLibraryAppend(PETSC_COMM_WORLD,&DLLibrariesLoaded,libs);CHKERRQ(ierr);
  }

  ierr = PetscStrcpy(libs,PETSC_LIB_DIR);CHKERRQ(ierr);
  ierr = PetscStrcat(libs,"/libpetscts");CHKERRQ(ierr);
  ierr = PetscDLLibraryRetrieve(PETSC_COMM_WORLD,libs,dlib,1024,&found);CHKERRQ(ierr);
  if (found) {
    ierr = PetscDLLibraryAppend(PETSC_COMM_WORLD,&DLLibrariesLoaded,libs);CHKERRQ(ierr);
  }

  ierr = PetscStrcpy(libs,PETSC_LIB_DIR);CHKERRQ(ierr);
  ierr = PetscStrcat(libs,"/libpetscdm");CHKERRQ(ierr);
  ierr = PetscDLLibraryRetrieve(PETSC_COMM_WORLD,libs,dlib,1024,&found);CHKERRQ(ierr);
  if (found) {
    ierr = PetscDLLibraryAppend(PETSC_COMM_WORLD,&DLLibrariesLoaded,libs);CHKERRQ(ierr);
  }

  ierr = PetscStrcpy(libs,PETSC_LIB_DIR);CHKERRQ(ierr);
  ierr = PetscStrcat(libs,"/libpetscmesh");CHKERRQ(ierr);
  ierr = PetscDLLibraryRetrieve(PETSC_COMM_WORLD,libs,dlib,1024,&found);CHKERRQ(ierr);
  if (found) {
    ierr = PetscDLLibraryAppend(PETSC_COMM_WORLD,&DLLibrariesLoaded,libs);CHKERRQ(ierr);
  }

  ierr = PetscStrcpy(libs,PETSC_LIB_DIR);CHKERRQ(ierr);
  ierr = PetscStrcat(libs,"/libpetscgrid");CHKERRQ(ierr);
  ierr = PetscDLLibraryRetrieve(PETSC_COMM_WORLD,libs,dlib,1024,&found);CHKERRQ(ierr);
  if (found) {
    ierr = PetscDLLibraryAppend(PETSC_COMM_WORLD,&DLLibrariesLoaded,libs);CHKERRQ(ierr);
  }

  ierr = PetscStrcpy(libs,PETSC_LIB_DIR);CHKERRQ(ierr);
  ierr = PetscStrcat(libs,"/libpetscgsolver");CHKERRQ(ierr);
  ierr = PetscDLLibraryRetrieve(PETSC_COMM_WORLD,libs,dlib,1024,&found);CHKERRQ(ierr);
  if (found) {
    ierr = PetscDLLibraryAppend(PETSC_COMM_WORLD,&DLLibrariesLoaded,libs);CHKERRQ(ierr);
  }

  nmax = 32;
  ierr = PetscOptionsGetStringArray(PETSC_NULL,"-dll_append",libname,&nmax,PETSC_NULL);CHKERRQ(ierr);
  for (i=0; i<nmax; i++) {
    ierr = PetscDLLibraryAppend(PETSC_COMM_WORLD,&DLLibrariesLoaded,libname[i]);CHKERRQ(ierr);
    ierr = PetscDLLibraryCCAAppend(PETSC_COMM_WORLD,&DLLibrariesLoaded,libname[i]);CHKERRQ(ierr);
    ierr = PetscFree(libname[i]);CHKERRQ(ierr);
  }

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscFinalize_DynamicLibraries"
/*
     PetscFinalize_DynamicLibraries - Closes the opened dynamic libraries.
*/ 
int PetscFinalize_DynamicLibraries(void)
{
  int        ierr;
  PetscTruth flg;

  PetscFunctionBegin;
  ierr = PetscOptionsHasName(PETSC_NULL,"-dll_view",&flg);CHKERRQ(ierr);
  if (flg) {
    ierr = PetscDLLibraryPrintPath();CHKERRQ(ierr);
  }
  ierr = PetscDLLibraryClose(DLLibrariesLoaded);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#else /* not using dynamic libraries */

EXTERN int PetscInitializePackage(char *);

#undef __FUNCT__  
#define __FUNCT__ "PetscInitalize_DynamicLibraries"
int PetscInitialize_DynamicLibraries(void)
{
  int ierr;

  PetscFunctionBegin;
  /*
      This just initializes the draw and viewer methods, since those
    are ALWAYS available. The other classes are initialized the first
    time an XXSetType() is called.
  */
  ierr = PetscInitializePackage(PETSC_NULL);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
#undef __FUNCT__  
#define __FUNCT__ "PetscFinalize_DynamicLibraries"
int PetscFinalize_DynamicLibraries(void)
{
  PetscFunctionBegin;

  PetscFunctionReturn(0);
}
#endif

/* ------------------------------------------------------------------------------*/
struct _PetscFList {
  void        (*routine)(void);   /* the routine */
  char        *path;              /* path of link library containing routine */
  char        *name;              /* string to identify routine */
  char        *rname;             /* routine name in dynamic library */
  PetscFList  next;               /* next pointer */
  PetscFList  next_list;          /* used to maintain list of all lists for freeing */
};

/*
     Keep a linked list of PetscFLists so that we can destroy all the left-over ones.
*/
static PetscFList   dlallhead = 0;

#undef __FUNCT__  
#define __FUNCT__ "PetscFListAdd"
/*@C
   PetscFListAddDynamic - Given a routine and a string id, saves that routine in the
   specified registry.

   Synopsis:
   int PetscFListAddDynamic(PetscFList *fl,char *name,char *rname,int (*fnc)(void *))

   Input Parameters:
+  fl    - pointer registry
.  name  - string to identify routine
.  rname - routine name in dynamic library
-  fnc   - function pointer (optional if using dynamic libraries)

   Notes:
   Users who wish to register new methods for use by a particular PETSc
   component (e.g., SNES) should generally call the registration routine
   for that particular component (e.g., SNESRegisterDynamic()) instead of
   calling PetscFListAddDynamic() directly.

   ${PETSC_ARCH}, ${PETSC_DIR}, ${PETSC_LIB_DIR}, ${BOPT}, or ${any environmental variable}
  occuring in pathname will be replaced with appropriate values.

   Level: developer

.seealso: PetscFListDestroy(), SNESRegisterDynamic(), KSPRegisterDynamic(),
          PCRegisterDynamic(), TSRegisterDynamic(), PetscFList
@*/
int PetscFListAdd(PetscFList *fl,const char name[],const char rname[],void (*fnc)(void))
{
  PetscFList entry,ne;
  int        ierr;
  char       *fpath,*fname;

  PetscFunctionBegin;

  if (!*fl) {
    ierr           = PetscNew(struct _PetscFList,&entry);CHKERRQ(ierr);
    ierr           = PetscStrallocpy(name,&entry->name);CHKERRQ(ierr);
    ierr           = PetscFListGetPathAndFunction(rname,&fpath,&fname);CHKERRQ(ierr);
    entry->path    = fpath;
    entry->rname   = fname;
    entry->routine = fnc;
    entry->next    = 0;
    *fl = entry;

    /* add this new list to list of all lists */
    if (!dlallhead) {
      dlallhead        = *fl;
      (*fl)->next_list = 0;
    } else {
      ne               = dlallhead;
      dlallhead        = *fl;
      (*fl)->next_list = ne;
    }
  } else {
    /* search list to see if it is already there */
    ne = *fl;
    while (ne) {
      PetscTruth founddup;

      ierr = PetscStrcmp(ne->name,name,&founddup);CHKERRQ(ierr);
      if (founddup) { /* found duplicate */
        ierr = PetscFListGetPathAndFunction(rname,&fpath,&fname);CHKERRQ(ierr);
        ierr = PetscStrfree(ne->path);CHKERRQ(ierr);
        ierr = PetscStrfree(ne->rname);CHKERRQ(ierr);
        ne->path    = fpath;
        ne->rname   = fname;
        ne->routine = fnc;
        PetscFunctionReturn(0);
      }
      if (ne->next) ne = ne->next; else break;
    }
    /* create new entry and add to end of list */
    ierr           = PetscNew(struct _PetscFList,&entry);CHKERRQ(ierr);
    ierr           = PetscStrallocpy(name,&entry->name);CHKERRQ(ierr);
    ierr           = PetscFListGetPathAndFunction(rname,&fpath,&fname);CHKERRQ(ierr);
    entry->path    = fpath;
    entry->rname   = fname;
    entry->routine = fnc;
    entry->next    = 0;
    ne->next       = entry;
  }

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscFListDestroy"
/*@
    PetscFListDestroy - Destroys a list of registered routines.

    Input Parameter:
.   fl  - pointer to list

    Level: developer

.seealso: PetscFListAddDynamic(), PetscFList
@*/
int PetscFListDestroy(PetscFList *fl)
{
  PetscFList next,entry,tmp = dlallhead;
  int        ierr;

  PetscFunctionBegin;
  if (!*fl) PetscFunctionReturn(0);

  if (!dlallhead) {
    PetscFunctionReturn(0);
  }

  /*
       Remove this entry from the master DL list (if it is in it)
  */
  if (dlallhead == *fl) {
    if (dlallhead->next_list) {
      dlallhead = dlallhead->next_list;
    } else {
      dlallhead = 0;
    }
  } else {
    while (tmp->next_list != *fl) {
      tmp = tmp->next_list;
      if (!tmp->next_list) break;
    }
    if (tmp->next_list) tmp->next_list = tmp->next_list->next_list;
  }

  /* free this list */
  entry = *fl;
  while (entry) {
    next = entry->next;
    ierr = PetscStrfree(entry->path);CHKERRQ(ierr);
    ierr = PetscFree(entry->name);CHKERRQ(ierr);
    ierr = PetscFree(entry->rname);CHKERRQ(ierr);
    ierr = PetscFree(entry);CHKERRQ(ierr);
    entry = next;
  }
  *fl = 0;
  PetscFunctionReturn(0);
}

/*
   Destroys all the function lists that anyone has every registered, such as KSPList, VecList, etc.
*/
#undef __FUNCT__  
#define __FUNCT__ "PetscFListDestroyAll"
int PetscFListDestroyAll(void)
{
  PetscFList tmp2,tmp1 = dlallhead;
  int        ierr;

  PetscFunctionBegin;
  while (tmp1) {
    tmp2 = tmp1->next_list;
    ierr = PetscFListDestroy(&tmp1);CHKERRQ(ierr);
    tmp1 = tmp2;
  }
  dlallhead = 0;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscFListFind"
/*@C
    PetscFListFind - Given a name, finds the matching routine.

    Input Parameters:
+   comm - processors looking for routine
.   fl   - pointer to list
-   name - name string

    Output Parameters:
.   r - the routine

    Level: developer

.seealso: PetscFListAddDynamic(), PetscFList
@*/
int PetscFListFind(MPI_Comm comm,PetscFList fl,const char name[],void (**r)(void))
{
  PetscFList   entry = fl;
  int          ierr;
  char         *function,*path;
#if defined(PETSC_USE_DYNAMIC_LIBRARIES)
  char         *newpath;
#endif
  PetscTruth   flg,f1,f2,f3;
 
  PetscFunctionBegin;
  if (!name) SETERRQ(1,"Trying to find routine with null name");

  *r = 0;
  ierr = PetscFListGetPathAndFunction(name,&path,&function);CHKERRQ(ierr);

  /*
        If path then append it to search libraries
  */
#if defined(PETSC_USE_DYNAMIC_LIBRARIES)
  if (path) {
    ierr = PetscDLLibraryAppend(comm,&DLLibrariesLoaded,path);CHKERRQ(ierr);
  }
#endif

  while (entry) {
    flg = PETSC_FALSE;
    if (path && entry->path) {
      ierr = PetscStrcmp(path,entry->path,&f1);CHKERRQ(ierr);
      ierr = PetscStrcmp(function,entry->rname,&f2);CHKERRQ(ierr);
      ierr = PetscStrcmp(function,entry->name,&f3);CHKERRQ(ierr);
      flg =  (PetscTruth) ((f1 && f2) || (f1 && f3));
    } else if (!path) {
      ierr = PetscStrcmp(function,entry->name,&f1);CHKERRQ(ierr);
      ierr = PetscStrcmp(function,entry->rname,&f2);CHKERRQ(ierr);
      flg =  (PetscTruth) (f1 || f2);
    } else {
      ierr = PetscStrcmp(function,entry->name,&flg);CHKERRQ(ierr);
      if (flg) {
        ierr = PetscFree(function);CHKERRQ(ierr);
        ierr = PetscStrallocpy(entry->rname,&function);CHKERRQ(ierr);
      } else {
        ierr = PetscStrcmp(function,entry->rname,&flg);CHKERRQ(ierr);
      }
    }

    if (flg) {

      if (entry->routine) {
        *r   = entry->routine; 
        ierr = PetscStrfree(path);CHKERRQ(ierr);
        ierr = PetscFree(function);CHKERRQ(ierr);
        PetscFunctionReturn(0);
      }
 
      if ((path && entry->path && f3) || (!path && f1)) { /* convert name of function (alias) to actual function name */
        ierr = PetscFree(function);CHKERRQ(ierr);
        ierr = PetscStrallocpy(entry->rname,&function);CHKERRQ(ierr);
      }

      /* it is not yet in memory so load from dynamic library */
#if defined(PETSC_USE_DYNAMIC_LIBRARIES)
      newpath = path;
      if (!path) newpath = entry->path;
      ierr = PetscDLLibrarySym(comm,&DLLibrariesLoaded,newpath,entry->rname,(void **)r);CHKERRQ(ierr);
      if (*r) {
        entry->routine = *r;
        ierr = PetscStrfree(path);CHKERRQ(ierr);
        ierr = PetscFree(function);CHKERRQ(ierr);
        PetscFunctionReturn(0);
      } else {
        PetscErrorPrintf("Unable to find function. Search path:\n");
        ierr = PetscDLLibraryPrintPath();CHKERRQ(ierr);
        SETERRQ1(1,"Unable to find function:%s: either it is mis-spelled or dynamic library is not in path",entry->rname);
      }
#endif
    }
    entry = entry->next;
  }

#if defined(PETSC_USE_DYNAMIC_LIBRARIES)
  /* Function never registered; try for it anyway */
  ierr = PetscDLLibrarySym(comm,&DLLibrariesLoaded,path,function,(void **)r);CHKERRQ(ierr);
  ierr = PetscStrfree(path);CHKERRQ(ierr);
  if (*r) {
    ierr = PetscFListAddDynamic(&fl,name,name,r);CHKERRQ(ierr);
  }
#endif

  /*
       Do not generate error, just end
  PetscErrorPrintf("Function name: %s\n",function);
  ierr = PetscDLLibraryPrintPath();CHKERRQ(ierr);
  SETERRQ(1,"Unable to find function: either it is mis-spelled or dynamic library is not in path");
  */

  ierr = PetscFree(function);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscFListView"
/*@
   PetscFListView - prints out contents of an PetscFList

   Collective over MPI_Comm

   Input Parameters:
+  list - the list of functions
-  viewer - currently ignored

   Level: developer

.seealso: PetscFListAddDynamic(), PetscFListPrintTypes(), PetscFList
@*/
int PetscFListView(PetscFList list,PetscViewer viewer)
{
  int        ierr;
  PetscTruth isascii;

  PetscFunctionBegin;
  if (!viewer) viewer = PETSC_VIEWER_STDOUT_SELF;
  PetscValidHeaderSpecific(viewer,PETSC_VIEWER_COOKIE);
  PetscValidPointer(list);
  
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_ASCII,&isascii);CHKERRQ(ierr);
  if (!isascii) SETERRQ(1,"Only ASCII viewer supported");

  while (list) {
    if (list->path) {
      ierr = PetscViewerASCIIPrintf(viewer," %s %s %s\n",list->path,list->name,list->rname);CHKERRQ(ierr);
    } else {
      ierr = PetscViewerASCIIPrintf(viewer," %s %s\n",list->name,list->rname);CHKERRQ(ierr);
    }
    list = list->next;
  }
  ierr = PetscViewerASCIIPrintf(viewer,"\n");CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscFListGet"
/*@
   PetscFListGet - Gets an array the contains the entries in PetscFList, this is used
         by help etc.

   Collective over MPI_Comm

   Input Parameter:
.  list   - list of types

   Output Parameter:
+  array - array of names
-  n - length of array

   Notes:
       This allocates the array so that must be freed. BUT the individual entries are
    not copied so should not be freed.

   Level: developer

.seealso: PetscFListAddDynamic(), PetscFList
@*/
int PetscFListGet(PetscFList list,char ***array,int *n)
{
  int        count = 0,ierr;
  PetscFList klist = list;

  PetscFunctionBegin;
  while (list) {
    list = list->next;
    count++;
  }
  ierr  = PetscMalloc((count+1)*sizeof(char *),array);CHKERRQ(ierr);
  count = 0;
  while (klist) {
    (*array)[count] = klist->name;
    klist = klist->next;
    count++;
  }
  (*array)[count] = 0;
  *n = count+1;

  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "PetscFListPrintTypes"
/*@C
   PetscFListPrintTypes - Prints the methods available.

   Collective over MPI_Comm

   Input Parameters:
+  comm   - the communicator (usually MPI_COMM_WORLD)
.  fd     - file to print to, usually stdout
.  prefix - prefix to prepend to name (optional)
.  name   - option string (for example, "-ksp_type")
.  text - short description of the object (for example, "Krylov solvers")
.  man - name of manual page that discusses the object (for example, "KSPCreate")
-  list   - list of types

   Level: developer

.seealso: PetscFListAddDynamic(), PetscFList
@*/
int PetscFListPrintTypes(MPI_Comm comm,FILE *fd,const char prefix[],const char name[],char *text,char *man,PetscFList list)
{
  int      ierr,count = 0;
  char     p[64];

  PetscFunctionBegin;
  if (!fd) fd = stdout;

  ierr = PetscStrcpy(p,"-");CHKERRQ(ierr);
  if (prefix) {ierr = PetscStrcat(p,prefix);CHKERRQ(ierr);}
  ierr = PetscFPrintf(comm,fd,"  %s%s %s:(one of)",p,name+1,text);CHKERRQ(ierr);

  while (list) {
    ierr = PetscFPrintf(comm,fd," %s",list->name);CHKERRQ(ierr);
    list = list->next;
    count++;
    if (count == 8) {ierr = PetscFPrintf(comm,fd,"\n     ");CHKERRQ(ierr);}
  }
  ierr = PetscFPrintf(comm,fd," (see manual page %s)\n",man);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscFListDuplicate"
/*@
    PetscFListDuplicate - Creates a new list from a given object list.

    Input Parameters:
.   fl   - pointer to list

    Output Parameters:
.   nl - the new list (should point to 0 to start, otherwise appends)

    Level: developer

.seealso: PetscFList, PetscFListAdd(), PetscFlistDestroy()

@*/
int PetscFListDuplicate(PetscFList fl,PetscFList *nl)
{
  int  ierr;
  char path[1024];

  PetscFunctionBegin;
  while (fl) {
    /* this is silly, rebuild the complete pathname */
    if (fl->path) {
      ierr = PetscStrcpy(path,fl->path);CHKERRQ(ierr);
      ierr = PetscStrcat(path,":");CHKERRQ(ierr);
      ierr = PetscStrcat(path,fl->name);CHKERRQ(ierr);
    } else {
      ierr = PetscStrcpy(path,fl->name);CHKERRQ(ierr);
    }       
    ierr = PetscFListAddDynamic(nl,path,fl->rname,fl->routine);CHKERRQ(ierr);
    fl   = fl->next;
  }
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "PetscFListConcat"
/*
    PetscFListConcat - joins name of a libary, and the path where it is located
    into a single string.

    Input Parameters:
.   path   - path to the library name.
.   name   - name of the library

    Output Parameters:
.   fullname - the name that is the union of the path and the library name,
               delimited by a semicolon, i.e., path:name

    Notes:
    If the path is NULL, assumes that the name, specified also includes
    the path as path:name

*/
int PetscFListConcat(const char path[],const char name[],char fullname[])
{
  int ierr;
  PetscFunctionBegin;
  if (path) {
    ierr = PetscStrcpy(fullname,path);CHKERRQ(ierr);
    ierr = PetscStrcat(fullname,":");CHKERRQ(ierr);
    ierr = PetscStrcat(fullname,name);CHKERRQ(ierr);
  } else {
    ierr = PetscStrcpy(fullname,name);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}
