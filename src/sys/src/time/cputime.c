/*$Id: cputime.c,v 1.41 2001/06/21 21:15:40 bsmith Exp $*/
/*
  This is to allow one to measure CPU time usage of their job, 
  NOT real time usage. Do not use this for reported timings, speedup etc.
*/

#include "petscsys.h"                       /*I "petsc.h" I*/
#include "petscfix.h"
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#if defined (PETSC_HAVE_STDLIB_H)
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
#include <time.h>  
#if defined(PETSC_HAVE_SYS_SYSTEMINFO_H)
#include <sys/systeminfo.h>
#endif
#include "petscfix.h"

#if defined (PETSC_HAVE_SYS_TIMES_H)

#include <sys/times.h>
#include <limits.h>
#undef __FUNCT__  
#define __FUNCT__ "PetscGetCPUTime"
int PetscGetCPUTime(PetscLogDouble *t)
{
  struct tms temp;

  PetscFunctionBegin;
  times(&temp);
  *t = ((double)temp.tms_utime)/((double)CLOCKS_PER_SEC);
  PetscFunctionReturn(0);
}

#elif defined(PETSC_HAVE_CLOCK)

#include <time.h>
#include <sys/types.h>

#undef __FUNCT__  
#define __FUNCT__ "PetscGetCPUTime"
int PetscGetCPUTime(PetscLogDouble *t)
{
  PetscFunctionBegin;
  *t = ((double)clock()) / ((double)CLOCKS_PER_SEC);
  PetscFunctionReturn(0);
}  

#else

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#undef __FUNCT__  
#define __FUNCT__ "PetscGetCPUTime"
/*@C
    PetscGetCPUTime - Returns the CPU time in seconds used by the process.

    Not Collective

    Output Parameter:
.   t - Time in seconds charged to the process.

    Example:
.vb
    #include "petsc.h"
    ...
    PetscLogDouble t1, t2;
 
    ierr = PetscGetCPUTime(&t1);CHKERRQ(ierr);
    ... code to time ...
    ierr = PetscGetCPUTime(&t2);CHKERRQ(ierr);
    printf("Code took %f CPU seconds\n", t2-t1);
.ve

    Level: intermediate

    Notes:
    One should use PetscGetTime() or the -log_summary option of 
    PETSc for profiling. The CPU time is NOT a realistic number to
    use since it does not include the time for message passing etc.
    Also on many systems the accuracy is only on the order of microseconds.
@*/
int PetscGetCPUTime(PetscLogDouble *t)
{
  static struct rusage temp;
  PetscLogDouble       foo,foo1;

  PetscFunctionBegin;
  getrusage(RUSAGE_SELF,&temp);
  foo     = temp.ru_utime.tv_sec;     /* seconds */
  foo1    = temp.ru_utime.tv_usec;    /* uSecs */
  *t      = foo + foo1 * 1.0e-6;
  PetscFunctionReturn(0);
}

#endif
