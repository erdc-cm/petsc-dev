/*$Id: grpath.c,v 1.40 2001/03/23 23:20:30 balay Exp $*/

#include "petsc.h"
#include "petscsys.h"
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
#if defined(PETSC_HAVE_SYS_SYSTEMINFO_H)
#include <sys/systeminfo.h>
#endif
#include "petscfix.h"

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif


#undef __FUNCT__  
#define __FUNCT__ "PetscGetRealPath"
/*@C
   PetscGetRealPath - Get the path without symbolic links etc. and in absolute form.

   Not Collective

   Input Parameter:
.  path - path to resolve

   Output Parameter:
.  rpath - resolved path

   Level: developer

   Notes: 
   rpath is assumed to be of length MAXPATHLEN.

   Systems that use the automounter often generate absolute paths
   of the form "/tmp_mnt....".  However, the automounter will fail to
   mount this path if it is not already mounted, so we remove this from
   the head of the line.  This may cause problems if, for some reason,
   /tmp_mnt is valid and not the result of the automounter.

   Concepts: real path
   Concepts: path^real

.seealso: PetscGetFullPath()
@*/
int PetscGetRealPath(char path[],char rpath[])
{
  int        ierr;
  char       tmp3[MAXPATHLEN];
  PetscTruth flg;
#if defined(PETSC_HAVE_READLINK)
  char       tmp1[MAXPATHLEN],tmp4[MAXPATHLEN],*tmp2;
  int        n,m,N,len,len1,len2;
#endif

  PetscFunctionBegin;
#if defined(PETSC_HAVE_REALPATH)
  realpath(path,rpath);
#elif defined (PARCH_win32)
  ierr = PetscStrcpy(rpath,path);CHKERRQ(ierr);
#elif !defined(PETSC_HAVE_READLINK)
  ierr = PetscStrcpy(rpath,path);CHKERRQ(ierr);
#else

  /* Algorithm: we move through the path, replacing links with the real paths.   */
  ierr = PetscStrcpy(rpath,path);CHKERRQ(ierr);
  ierr = PetscStrlen(rpath,&N);CHKERRQ(ierr);
  while (N) {
    ierr = PetscStrncpy(tmp1,rpath,N);CHKERRQ(ierr);
    tmp1[N] = 0;
    n = readlink(tmp1,tmp3,MAXPATHLEN);
    if (n > 0) {
      tmp3[n] = 0; /* readlink does not automatically add 0 to string end */
      if (tmp3[0] != '/') {
        ierr = PetscStrchr(tmp1,'/',&tmp2);CHKERRQ(ierr);
        ierr = PetscStrlen(tmp1,&len1);CHKERRQ(ierr);
        ierr = PetscStrlen(tmp2,&len2);CHKERRQ(ierr);
        m    = len1 - len2;
        ierr = PetscStrncpy(tmp4,tmp1,m);CHKERRQ(ierr);
        tmp4[m] = 0;
        ierr = PetscStrlen(tmp4,&len);CHKERRQ(ierr);
        ierr = PetscStrncat(tmp4,"/",MAXPATHLEN - len);CHKERRQ(ierr);
        ierr = PetscStrlen(tmp4,&len);CHKERRQ(ierr);
        ierr = PetscStrncat(tmp4,tmp3,MAXPATHLEN - len);CHKERRQ(ierr);
        ierr = PetscGetRealPath(tmp4,rpath);CHKERRQ(ierr);
        ierr = PetscStrlen(rpath,&len);CHKERRQ(ierr);
        ierr = PetscStrncat(rpath,path+N,MAXPATHLEN - len);CHKERRQ(ierr);
      } else {
        ierr = PetscGetRealPath(tmp3,tmp1);CHKERRQ(ierr);
        ierr = PetscStrncpy(rpath,tmp1,MAXPATHLEN);CHKERRQ(ierr);
        ierr = PetscStrlen(rpath,&len);CHKERRQ(ierr);
        ierr = PetscStrncat(rpath,path+N,MAXPATHLEN - len);CHKERRQ(ierr);
      }
      PetscFunctionReturn(0);
    }  
    ierr = PetscStrchr(tmp1,'/',&tmp2);CHKERRQ(ierr);
    if (tmp2) {
      ierr = PetscStrlen(tmp1,&len1);CHKERRQ(ierr);
      ierr = PetscStrlen(tmp2,&len2);CHKERRQ(ierr);
      N    = len1 - len2;
    } else {
      ierr = PetscStrlen(tmp1,&N);CHKERRQ(ierr);
    }
  }
  ierr = PetscStrncpy(rpath,path,MAXPATHLEN);CHKERRQ(ierr);
#endif

  /* remove garbage some automounters put at the beginning of the path */
  ierr = PetscStrncmp("/tmp_mnt/",rpath,9,&flg);CHKERRQ(ierr); 
  if (flg) {
    ierr = PetscStrcpy(tmp3,rpath + 8);CHKERRQ(ierr);
    ierr = PetscStrcpy(rpath,tmp3);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}
