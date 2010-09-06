#define PETSC_DLL
/*
   Low-level routines for managing dynamic link libraries (DLLs).
*/

#include "../src/sys/dll/dlimpl.h"

/* XXX Should be done better !!!*/
#if !defined(PETSC_HAVE_DYNAMIC_LIBRARIES)
#undef PETSC_HAVE_WINDOWS_H
#undef PETSC_HAVE_DLFCN_H
#endif

#if defined(PETSC_HAVE_WINDOWS_H)
#include <windows.h>
#elif defined(PETSC_HAVE_DLFCN_H)
#include <dlfcn.h>
#endif

#if defined(PETSC_HAVE_WINDOWS_H)
typedef HMODULE dlhandle_t;
typedef FARPROC dlsymbol_t;
#elif defined(PETSC_HAVE_DLFCN_H)
typedef void* dlhandle_t;
typedef void* dlsymbol_t;
#else
typedef void* dlhandle_t;
typedef void* dlsymbol_t;
#endif

#undef __FUNCT__  
#define __FUNCT__ "PetscDLOpen"
/*@C
   PetscDLOpen - opens dynamic library

   Not Collective

   Input Parameters:
+    name - name of library
-    flags - options on how to open library

   Output Parameter:
.    handle

   Level: developer

@*/
PetscErrorCode PETSCSYS_DLLEXPORT PetscDLOpen(const char name[],int flags,PetscDLHandle *handle)
{
  int        dlflags1,dlflags2;
  dlhandle_t dlhandle;

  PetscFunctionBegin;
  PetscValidCharPointer(name,1);
  PetscValidPointer(handle,3);

  dlflags1 = 0;
  dlflags2 = 0;
  dlhandle = (dlhandle_t) 0;
  *handle  = (PetscDLHandle) 0;

  /* 
     --- LoadLibrary ---
  */  
#if defined(PETSC_HAVE_WINDOWS_H) && defined(PETSC_HAVE_LOADLIBRARY)
  dlhandle = LoadLibrary(name);
  if (!dlhandle) {
#if defined(PETSC_HAVE_GETLASTERROR)
    PetscErrorCode ierr;
    DWORD erc;
    char  *buff = NULL;
    erc = GetLastError();
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
		  NULL,erc,MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),(LPSTR)&buff,0,NULL);
    ierr = PetscError(PETSC_COMM_SELF,__LINE__,__FUNCT__,__FILE__,__SDIR__,PETSC_ERR_FILE_OPEN,PETSC_ERROR_REPEAT,
		      "Unable to open dynamic library:\n  %s\n  Error message from LoadLibrary() %s\n",name,buff);
    LocalFree(buff);
    PetscFunctionReturn(ierr);
#else
    SETERRQ2(PETSC_COMM_SELF,PETSC_ERR_FILE_OPEN,"Unable to open dynamic library:\n  %s\n  Error message from LoadLibrary() %s\n",name,"unavailable");
#endif
  }

  /* 
     --- dlopen ---
  */  
#elif defined(PETSC_HAVE_DLFCN_H) && defined(PETSC_HAVE_DLOPEN)
  /*
      Mode indicates symbols required by symbol loaded with dlsym() 
     are only loaded when required (not all together) also indicates
     symbols required can be contained in other libraries also opened
     with dlopen()
  */
#if defined(PETSC_HAVE_RTLD_LAZY)
  dlflags1 = RTLD_LAZY;
#endif
#if defined(PETSC_HAVE_RTLD_NOW)
  if (flags & PETSC_DL_NOW)
    dlflags1 = RTLD_NOW;
#endif
#if defined(PETSC_HAVE_RTLD_GLOBAL)
  dlflags2 = RTLD_GLOBAL;
#endif
#if defined(PETSC_HAVE_RTLD_LOCAL)
  if (flags & PETSC_DL_LOCAL)
    dlflags2 = RTLD_LOCAL;
#endif
#if defined(PETSC_HAVE_DLERROR)
  dlerror(); /* clear any previous error */
#endif
  dlhandle = dlopen(name,dlflags1|dlflags2);
  if (!dlhandle) {
#if defined(PETSC_HAVE_DLERROR)
    const char *errmsg = dlerror();
#else
    const char *errmsg = "unavailable";
#endif
    SETERRQ2(PETSC_COMM_SELF,PETSC_ERR_FILE_OPEN,"Unable to open dynamic library:\n  %s\n  Error message from dlopen() %s\n",name,errmsg);
  }

  /* 
     --- unimplemented ---
  */  
#else
  SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SUP_SYS, "Cannot use dynamic libraries on this platform");
#endif

  *handle = (PetscDLHandle) dlhandle;

  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "PetscDLClose"
/*@C
   PetscDLClose -  closes a dynamic library

   Not Collective

  Input Parameter:
.   handle - the handle for the library obtained with PetscDLOpen()

  Level: developer
@*/
PetscErrorCode PETSCSYS_DLLEXPORT PetscDLClose(PetscDLHandle *handle)
{
  dlhandle_t dlhandle;

  PetscFunctionBegin;
  PetscValidPointer(handle,1);

  dlhandle = (dlhandle_t) *handle;

  /* 
     --- FreeLibrary ---
  */  
#if defined(PETSC_HAVE_WINDOWS_H)
#if defined(PETSC_HAVE_FREELIBRARY)
  if (FreeLibrary(dlhandle) == 0) {
#if defined(PETSC_HAVE_GETLASTERROR)
    char  *buff = NULL;
    DWORD erc   = GetLastError();
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
		  NULL,erc,MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),(LPSTR)&buff,0,NULL);
    PetscErrorPrintf("Error closing dynamic library:\n  Error message from FreeLibrary() %s\n",buff);
    LocalFree(buff);
#else
    PetscErrorPrintf("Error closing dynamic library:\n  Error message from FreeLibrary() %s\n","unavailable");
#endif
  }
#endif /* !PETSC_HAVE_FREELIBRARY */

  /* 
     --- dclose --- 
  */  
#elif defined(PETSC_HAVE_DLFCN_H)
#if defined(PETSC_HAVE_DLCLOSE)
#if defined(PETSC_HAVE_DLERROR)
  dlerror(); /* clear any previous error */
#endif
  if (dlclose(dlhandle) < 0) {
#if defined(PETSC_HAVE_DLERROR)
    const char *errmsg = dlerror();
#else
    const char *errmsg = "unavailable";
#endif
    PetscErrorPrintf("Error closing dynamic library:\n  Error message from dlclose() %s\n", errmsg);
  }
#endif /* !PETSC_HAVE_DLCLOSE */

  /* 
     --- unimplemented --- 
  */  
#else
  SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SUP_SYS, "Cannot use dynamic libraries on this platform");
#endif

  *handle = PETSC_NULL;

  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "PetscDLSym"
/*@C
   PetscDLSym - finds a symbol in a dynamic library

   Not Collective

   Input Parameters:
+   handle - obtained with PetscDLOpen() or PETSC_NULL
-   symbol - name of symbol

   Output Parameter:
.   value - pointer to the function

   Level: developer

  Notes:
   If handle is PETSC_NULL, the symbol is looked for in the main executable's dynamic symbol table.
   In order to be dynamically loadable, the symbol has to be exported as such.  On many UNIX-like 
   systems this requires platform-specific linker flags.

@*/
PetscErrorCode PETSCSYS_DLLEXPORT PetscDLSym(PetscDLHandle handle,const char symbol[],void **value)
{
  dlhandle_t dlhandle;
  dlsymbol_t dlsymbol;
#if defined(PETSC_HAVE_DLERROR)
  char *e;
#endif  


  PetscFunctionBegin;
  PetscValidCharPointer(symbol,2);
  PetscValidPointer(value,3);

  dlhandle = (dlhandle_t) 0;
  dlsymbol = (dlsymbol_t) 0;

  *value   = (void *) 0;

  /* 
     --- GetProcAddress ---
  */  
#if defined(PETSC_HAVE_WINDOWS_H) 
#if defined(PETSC_HAVE_GETPROCADDRESS)
  if (handle != PETSC_NULL)
    dlhandle = (dlhandle_t) handle;
  else
    dlhandle = (dlhandle_t) GetCurrentProcess();
  dlsymbol = (dlsymbol_t) GetProcAddress(dlhandle,symbol);
#if defined(PETSC_HAVE_SETLASTERROR)
  SetLastError((DWORD)0); /* clear any previous error */
#endif
#endif /* !PETSC_HAVE_GETPROCADDRESS */

  /* 
     --- dlsym ---
  */  
#elif defined(PETSC_HAVE_DLFCN_H)
#if defined(PETSC_HAVE_DLSYM)
  if (handle != PETSC_NULL) {
    dlhandle = (dlhandle_t) handle;
  }
  else {
    dlhandle = (dlhandle_t) 0;
    
#if defined(PETSC_HAVE_DLOPEN)
    /* Attempt to retrieve the main executable's dlhandle. */
    { int dlflags1 = 0, dlflags2 = 0;
#if defined(PETSC_HAVE_RTLD_LAZY)
      dlflags1 = RTLD_LAZY;
#endif
      if(!dlflags1) {
#if defined(PETSC_HAVE_RTLD_NOW)
        dlflags1 = RTLD_NOW;
#endif
      }
#if defined(PETSC_HAVE_RTLD_LOCAL)
      dlflags2 = RTLD_LOCAL;
#endif
      if(!dlflags2) {
#if defined(PETSC_HAVE_RTLD_GLOBAL)
        dlflags2 = RTLD_GLOBAL;
#endif
      }
#if defined(PETSC_HAVE_DLERROR)
      dlerror(); /* clear any previous error */
#endif
      /* Attempt to open the main executable as a dynamic library. */
      dlhandle = dlopen(0, dlflags1|dlflags2); 
    }
#endif
#if defined(PETSC_HAVE_DLERROR)
    e = dlerror();
    if(e){
      SETERRQ1(PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Error opening main executable as a dynamic library:\n  Error message from dlopen(): '%s'\n", e);
    }
  }
#endif
#if defined(PETSC_HAVE_DLERROR)
  dlerror(); /* clear any previous error */
#endif
  dlsymbol = (dlsymbol_t) dlsym(dlhandle,symbol);
#if defined(PETSC_HAVE_DLERROR)
/*   e = dlerror(); */
/*   if(e){ */
/*     SETERRQ2(PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG,"Error obtaining symbol '%s' from dynamic library:\n  Error message from dlsym(): '%s'\n", symbol, e); */
/*   } */
#endif
#endif /* !PETSC_HAVE_DLSYM */

  /* 
     --- unimplemented --- 
  */  
#else
  SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SUP_SYS, "Cannot use dynamic libraries on this platform");
#endif

  *value = *((void**)&dlsymbol);

  PetscFunctionReturn(0);
}
