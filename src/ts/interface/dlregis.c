
#include "petscts.h"

#undef __FUNCT__  
#define __FUNCT__ "TSInitializePackage"
/*@C
  TSInitializePackage - This function initializes everything in the TS package. It is called
  from PetscDLLibraryRegister() when using dynamic libraries, and on the first call to TSCreate()
  when using static libraries.

  Input Parameter:
  path - The dynamic library path, or PETSC_NULL

  Level: developer

.keywords: TS, initialize, package
.seealso: PetscInitialize()
@*/
PetscErrorCode TSInitializePackage(const char path[]) {
  static PetscTruth initialized = PETSC_FALSE;
  char              logList[256];
  char             *className;
  PetscTruth        opt;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (initialized == PETSC_TRUE) PetscFunctionReturn(0);
  initialized = PETSC_TRUE;
  /* Register Classes */
  ierr = PetscLogClassRegister(&TS_COOKIE, "TS");CHKERRQ(ierr);
  /* Register Constructors */
  ierr = TSRegisterAll(path);CHKERRQ(ierr);
  /* Register Events */
  ierr = PetscLogEventRegister(&TS_Step,                  "TSStep",           TS_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&TS_PseudoComputeTimeStep, "TSPseudoCmptTStp", TS_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&TS_FunctionEval,          "TSFunctionEval",   TS_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&TS_JacobianEval,          "TSJacobianEval",   TS_COOKIE);CHKERRQ(ierr);
  /* Process info exclusions */
  ierr = PetscOptionsGetString(PETSC_NULL, "-log_info_exclude", logList, 256, &opt);CHKERRQ(ierr);
  if (opt == PETSC_TRUE) {
    ierr = PetscStrstr(logList, "ts", &className);CHKERRQ(ierr);
    if (className) {
      ierr = PetscLogInfoDeactivateClass(TS_COOKIE);CHKERRQ(ierr);
    }
  }
  /* Process summary exclusions */
  ierr = PetscOptionsGetString(PETSC_NULL, "-log_summary_exclude", logList, 256, &opt);CHKERRQ(ierr);
  if (opt == PETSC_TRUE) {
    ierr = PetscStrstr(logList, "ts", &className);CHKERRQ(ierr);
    if (className) {
      ierr = PetscLogEventDeactivateClass(TS_COOKIE);CHKERRQ(ierr);
    }
  }
  PetscFunctionReturn(0);
}

#ifdef PETSC_USE_DYNAMIC_LIBRARIES
EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "PetscDLLibraryRegister"
/*
  PetscDLLibraryRegister - This function is called when the dynamic library it is in is opened.

  This one registers all the TS methods that are in the basic PETSc libpetscts library.

  Input Parameter:
  path - library path
 */
PetscErrorCode PetscDLLibraryRegister(char *path)
{
  PetscErrorCode ierr;

  ierr = PetscInitializeNoArguments(); if (ierr) return 1;

  PetscFunctionBegin;
  /*
      If we got here then PETSc was properly loaded
  */
  ierr = TSInitializePackage(path);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END

/* --------------------------------------------------------------------------*/
static const char *contents = "PETSc timestepping library. \n\
     Euler\n\
     Backward Euler\n\
     PVODE interface\n";
static const char *authors  = PETSC_AUTHOR_INFO;

#include "src/sys/src/utils/dlregis.h"

#endif /* PETSC_USE_DYNAMIC_LIBRARIES */
