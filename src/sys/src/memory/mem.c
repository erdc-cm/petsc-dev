/*$Id: mem.c,v 1.55 2001/06/21 21:15:26 bsmith Exp $*/

#include "petsc.h"           /*I "petsc.h" I*/
#include "petscsys.h"
#include "petscfix.h"
#if defined(PETSC_HAVE_PWD_H)
#include <pwd.h>
#endif
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#if defined(PETSC_HAVE_UNISTD_H)
#include <unistd.h>
#endif
#if defined(PETSC_HAVE_STDLIB_H)
#include <stdlib.h>
#endif
#if !defined(PARCH_win32)
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
#if defined(PETSC_HAVE_SYS_SYSTEMINFO_H)
#include <sys/systeminfo.h>
#endif
#include "petscfix.h"

#if defined (PETSC_HAVE_SYS_RESOURCE_H)
#include <sys/resource.h>
#endif
#if defined(PETSC_HAVE_SYS_PROCFS_H)
/* #include <sys/int_types.h> Required if using gcc on solaris 2.6 */
#include <sys/procfs.h>
#endif
#if defined(PETSC_HAVE_FCNTL_H)
#include <fcntl.h>
#endif

#undef __FUNCT__  
#define __FUNCT__ "PetscGetResidentSetSize"
/*@C
   PetscGetResidentSetSize - Returns the maximum resident set size (memory used)
   for the program.

   Not Collective

   Output Parameter:
.   mem - memory usage in bytes

   Options Database Key:
.  -get_resident_set_size - Print memory usage at end of run
.  -trmalloc_log - Activate logging of memory usage

   Level: intermediate

   Notes:
   The memory usage reported here includes all Fortran arrays 
   (that may be used in application-defined sections of code).
   This routine thus provides a more complete picture of memory
   usage than PetscTrSpace() for codes that employ Fortran with
   hardwired arrays.

.seealso: PetscTrSpace()

   Concepts: resident set size
   Concepts: memory usage

@*/
int PetscGetResidentSetSize(PetscLogDouble *mem)
{
#if defined(PETSC_USE_PROCFS_FOR_SIZE)
  FILE            *file;
  int             fd;
  char            proc[PETSC_MAX_PATH_LEN];
  prpsinfo_t      prusage;
#elif defined(PETSC_USE_SBREAK_FOR_SIZE)
  long            *ii = sbreak(0); 
  int             fd = ii - (long*)0; 
#elif defined(PETSC_USE_PROC_FOR_SIZE)
  FILE            *file;
  char            proc[PETSC_MAX_PATH_LEN];
#elif defined(PETSC_HAVE_NO_GETRUSAGE)
#else
  static struct   rusage temp;
#endif

  PetscFunctionBegin;
#if defined(PETSC_USE_PROCFS_FOR_SIZE)
  sprintf(proc,"/proc/%d",(int)getpid());
  if ((fd = open(proc,O_RDONLY)) == -1) {
    SETERRQ1(PETSC_ERR_FILE_OPEN,"Unable to access system file %s to get memory usage data",file);
  }
  if (ioctl(fd,PIOCPSINFO,&prusage) == -1) {
    SETERRQ1(PETSC_ERR_FILE_READ,"Unable to access system file %s to get memory usage data",file); 
  }
  *mem = (double)prusage.pr_byrssize;
  close(fd);
#elif defined(PETSC_USE_SBREAK_FOR_SIZE)
  *mem = (PetscLogDouble)(8*fd - 4294967296); /* 2^32 - upper bits */
#elif defined(PETSC_USE_PROC_FOR_SIZE)
  sprintf(proc,"/proc/%d/status",(int)getpid());
  if (!(file = fopen(proc,"r"))) {
    SETERRQ1(PETSC_ERR_FILE_OPEN,"Unable to access system file %s to get memory usage data",proc);
  }
#elif defined(PETSC_HAVE_NO_GETRUSAGE)
  *mem = 0.0;
#else
  getrusage(RUSAGE_SELF,&temp);
#if defined(PETSC_USE_KBYTES_FOR_SIZE)
  *mem = 1024.0 * ((double)temp.ru_maxrss);
#else
  *mem = ((double)getpagesize())*((double)temp.ru_maxrss);
#endif
#endif
  PetscFunctionReturn(0);
}
