/*$Id: petscfix.h,v 1.1 2000/04/04 21:54:07 balay Exp $*/

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

/* -----------------------beos ------------------------------------------*/
#if defined(__cplusplus)
extern "C" {
}
#endif
#endif
