
#include "src/sys/src/viewer/viewerimpl.h"  /*I "petsc.h" I*/  

EXTERN_C_BEGIN
EXTERN PetscErrorCode PetscViewerCreate_Socket(PetscViewer);
EXTERN PetscErrorCode PetscViewerCreate_ASCII(PetscViewer);
EXTERN PetscErrorCode PetscViewerCreate_Binary(PetscViewer);
EXTERN PetscErrorCode PetscViewerCreate_String(PetscViewer);
EXTERN PetscErrorCode PetscViewerCreate_Draw(PetscViewer);
EXTERN PetscErrorCode PetscViewerCreate_AMS(PetscViewer);
EXTERN PetscErrorCode PetscViewerCreate_VU(PetscViewer);
EXTERN PetscErrorCode PetscViewerCreate_Mathematica(PetscViewer);
EXTERN PetscErrorCode PetscViewerCreate_Netcdf(PetscViewer);
EXTERN PetscErrorCode PetscViewerCreate_HDF4(PetscViewer);
EXTERN PetscErrorCode PetscViewerCreate_Matlab(PetscViewer);
EXTERN_C_END
  
#undef __FUNCT__  
#define __FUNCT__ "PetscViewerRegisterAll" 
/*@C
  PetscViewerRegisterAll - Registers all of the graphics methods in the PetscViewer package.

  Not Collective

   Level: developer

.seealso:  PetscViewerRegisterDestroy()
@*/
PetscErrorCode PetscViewerRegisterAll(const char *path)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscViewerRegisterDynamic(PETSC_VIEWER_ASCII,      path,"PetscViewerCreate_ASCII",      PetscViewerCreate_ASCII);CHKERRQ(ierr);
  ierr = PetscViewerRegisterDynamic(PETSC_VIEWER_BINARY,     path,"PetscViewerCreate_Binary",     PetscViewerCreate_Binary);CHKERRQ(ierr);
  ierr = PetscViewerRegisterDynamic(PETSC_VIEWER_STRING,     path,"PetscViewerCreate_String",     PetscViewerCreate_String);CHKERRQ(ierr);
  ierr = PetscViewerRegisterDynamic(PETSC_VIEWER_DRAW,       path,"PetscViewerCreate_Draw",       PetscViewerCreate_Draw);CHKERRQ(ierr);
  ierr = PetscViewerRegisterDynamic(PETSC_VIEWER_SOCKET,     path,"PetscViewerCreate_Socket",     PetscViewerCreate_Socket);CHKERRQ(ierr);
#if defined(PETSC_HAVE_AMS)
  ierr = PetscViewerRegisterDynamic(PETSC_VIEWER_AMS,        path,"PetscViewerCreate_AMS",        PetscViewerCreate_AMS);CHKERRQ(ierr); 
#endif
#if defined(PETSC_HAVE_MATHEMATICA)
  ierr = PetscViewerRegisterDynamic(PETSC_VIEWER_MATHEMATICA,path,"PetscViewerCreate_Mathematica",PetscViewerCreate_Mathematica);CHKERRQ(ierr); 
#endif
  ierr = PetscViewerRegisterDynamic(PETSC_VIEWER_VU,         path,"PetscViewerCreate_VU",         PetscViewerCreate_VU);CHKERRQ(ierr); 
#if defined(PETSC_HAVE_PNETCDF)
  ierr = PetscViewerRegisterDynamic(PETSC_VIEWER_NETCDF,     path,"PetscViewerCreate_Netcdf",     PetscViewerCreate_Netcdf);CHKERRQ(ierr); 
#endif
#if defined(PETSC_HAVE_HDF4) && !defined(PETSC_USE_COMPLEX)
  ierr = PetscViewerRegisterDynamic(PETSC_VIEWER_HDF4,       path,"PetscViewerCreate_HDF4",       PetscViewerCreate_HDF4);CHKERRQ(ierr); 
#endif
#if defined(PETSC_HAVE_MATLAB) && !defined(PETSC_USE_COMPLEX) && !defined(PETSC_USE_SINGLE)
  ierr = PetscViewerRegisterDynamic(PETSC_VIEWER_MATLAB,     path,"PetscViewerCreate_Matlab",     PetscViewerCreate_Matlab);CHKERRQ(ierr); 
#endif
  PetscFunctionReturn(0);
}

