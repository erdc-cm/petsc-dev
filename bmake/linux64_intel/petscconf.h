#ifdef PETSC_RCS_HEADER
"$Id: petscconf.h,v 1.3 2001/08/30 20:04:19 balay Exp $"
"Defines the configuration for this machine"
#endif

#if !defined(INCLUDED_PETSCCONF_H)
#define INCLUDED_PETSCCONF_H

#define PARCH_linux
#define PETSC_ARCH_NAME "linux"

#define PETSC_HAVE_POPEN
#define PETSC_HAVE_LIMITS_H
#define PETSC_HAVE_PWD_H 
#define PETSC_HAVE_MALLOC_H 
#define PETSC_HAVE_STRING_H 
#define PETSC_HAVE_GETDOMAINNAME
#define PETSC_HAVE_DRAND48 
#define PETSC_HAVE_UNAME 
#define PETSC_HAVE_UNISTD_H 
#define PETSC_HAVE_SYS_TIME_H 
#define PETSC_HAVE_STDLIB_H
#define HAVE_GETCWD

#define PETSC_HAVE_FORTRAN_UNDERSCORE 

#define PETSC_HAVE_READLINK
#define PETSC_HAVE_MEMMOVE
#define PETSC_HAVE_STD_COMPLEX

#define PETSC_HAVE_DOUBLE_ALIGN_MALLOC
#define HAVE_MEMALIGN
#define PETSC_HAVE_SYS_RESOURCE_H
#define SIZEOF_VOID_P 8
#define SIZEOF_INT 4
#define SIZEOF_DOUBLE 8

#if defined(fixedsobug)
#define PETSC_USE_DYNAMIC_LIBRARIES 1
#define PETSC_HAVE_RTLD_GLOBAL 1
#endif

#define PETSC_HAVE_SYS_UTSNAME_H

#define PETSC_MISSING_SIGSYS

#ifdef PETSC_USE_MAT_SINGLE
#  define PETSC_MEMALIGN 16
#  define PETSC_HAVE_SSE "iclsse.h"
#endif

#endif
