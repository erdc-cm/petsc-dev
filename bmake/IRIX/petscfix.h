/*$Id: petscfix.h,v 1.100 2000/01/11 20:57:57 bsmith Exp $*/

/*
    This fixes various things in system files that are incomplete, for 
  instance many systems don't properly prototype all system functions.
  It is not intended to DUPLICATE anything in the system include files;
  if the compiler reports a conflict between a prototye in a system file
  and this file then the prototype in this file should be removed.

    This is included by files in src/sys/src
*/

#if !defined(_PETSCFIX_H)
#define _PETSCFIX_H

#include "petsc.h"

/*
  This prototype lets us resolve the datastructure 'rusage' only in
  the source files using getrusage, and not in other source files.
*/
typedef struct rusage* s_rusage;

/* -----------------------SGI IRIX -----------------------------------------*/
#if defined(__cplusplus)
extern "C" {
/* 
    Variation needed on older versions of the OS

    extern int gettimeofday(struct timeval *, struct timezone *);
*/
#include <sys/resource.h>
extern int gettimeofday(struct timeval *,...);
}
#else
#endif
#endif
