/*$Id: dlregis.c,v 1.19 2001/03/23 23:24:34 balay Exp $*/

#include "petscvec.h"

#undef __FUNCT__  
#define __FUNCT__ "VecInitializePackage"
/*@C
  VecInitializePackage - This function initializes everything in the Vec package. It is called
  from PetscDLLibraryRegister() when using dynamic libraries, and on the first call to VecCreate()
  when using static libraries.

  Input Parameter:
  path - The dynamic library path, or PETSC_NULL

  Level: developer

.keywords: Vec, initialize, package
.seealso: PetscInitialize()
@*/
int VecInitializePackage(char *path) {
  static PetscTruth initialized = PETSC_FALSE;
  char              logList[256];
  char             *className;
  PetscTruth        opt;
  int               ierr;

  PetscFunctionBegin;
  if (initialized == PETSC_TRUE) PetscFunctionReturn(0);
  initialized = PETSC_TRUE;
  /* Register Classes */
  ierr = PetscLogClassRegister(&IS_COOKIE,          "Index Set");                                         CHKERRQ(ierr);
  ierr = PetscLogClassRegister(&MAP_COOKIE,         "Map");                                               CHKERRQ(ierr);
  ierr = PetscLogClassRegister(&VEC_COOKIE,         "Vec");                                               CHKERRQ(ierr);
  ierr = PetscLogClassRegister(&VEC_SCATTER_COOKIE, "Vec Scatter");                                       CHKERRQ(ierr);
  /* Register Constructors and Serializers */
  ierr = VecRegisterAll(path);                                                                            CHKERRQ(ierr);
  /* Register Events */
  ierr = PetscLogEventRegister(&VecEvents[VEC_View],                "VecView",          PETSC_NULL, VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&VecEvents[VEC_Max],                 "VecMax",           PETSC_NULL, VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&VecEvents[VEC_Min],                 "VecMin",           PETSC_NULL, VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&VecEvents[VEC_DotBarrier],          "VecDotBarrier",    PETSC_NULL, VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&VecEvents[VEC_Dot],                 "VecDot",           PETSC_NULL, VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&VecEvents[VEC_MDotBarrier],         "VecMDotBarrier",   PETSC_NULL, VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&VecEvents[VEC_MDot],                "VecMDot",          PETSC_NULL, VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&VecEvents[VEC_TDot],                "VecTDot",          PETSC_NULL, VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&VecEvents[VEC_MTDot],               "VecMTDot",         PETSC_NULL, VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&VecEvents[VEC_NormBarrier],         "VecNormBarrier",   PETSC_NULL, VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&VecEvents[VEC_Norm],                "VecNorm",          PETSC_NULL, VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&VecEvents[VEC_Scale],               "VecScale",         PETSC_NULL, VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&VecEvents[VEC_Copy],                "VecCopy",          PETSC_NULL, VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&VecEvents[VEC_Set],                 "VecSet",           PETSC_NULL, VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&VecEvents[VEC_AXPY],                "VecAXPY",          PETSC_NULL, VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&VecEvents[VEC_AYPX],                "VecAYPX",          PETSC_NULL, VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&VecEvents[VEC_WAXPY],               "VecWAXPY",         PETSC_NULL, VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&VecEvents[VEC_MAXPY],               "VecMAXPY",         PETSC_NULL, VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&VecEvents[VEC_Swap],                "VecSwap",          PETSC_NULL, VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&VecEvents[VEC_AssemblyBegin],       "VecAssemblyBegin", PETSC_NULL, VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&VecEvents[VEC_AssemblyEnd],         "VecAssemblyEnd",   PETSC_NULL, VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&VecEvents[VEC_PointwiseMult],       "VecPointwiseMult", PETSC_NULL, VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&VecEvents[VEC_SetValues],           "VecSetValues",     PETSC_NULL, VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&VecEvents[VEC_Load],                "VecLoad",          PETSC_NULL, VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&VecEvents[VEC_ScatterBarrier],      "VecScatterBarrie", PETSC_NULL, VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&VecEvents[VEC_ScatterBegin],        "VecScatterBegin",  PETSC_NULL, VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&VecEvents[VEC_ScatterEnd],          "VecScatterEnd",    PETSC_NULL, VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&VecEvents[VEC_SetRandom],           "VecSetRandom",     PETSC_NULL, VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&VecEvents[VEC_ReduceArithmetic],    "VecReduceArith",   PETSC_NULL, VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&VecEvents[VEC_ReduceBarrier],       "VecReduceBarrier", PETSC_NULL, VEC_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&VecEvents[VEC_ReduceCommunication], "VecReduceComm",    PETSC_NULL, VEC_COOKIE);CHKERRQ(ierr);
  /* Process info exclusions */
  ierr = PetscOptionsGetString(PETSC_NULL, "-log_info_exclude", logList, 256, &opt);                      CHKERRQ(ierr);
  if (opt == PETSC_TRUE) {
    ierr = PetscStrstr(logList, "is", &className);                                                        CHKERRQ(ierr);
    if (className) {
      ierr = PetscLogInfoDeactivateClass(IS_COOKIE);                                                      CHKERRQ(ierr);
    }
    ierr = PetscStrstr(logList, "map", &className);                                                       CHKERRQ(ierr);
    if (className) {
      ierr = PetscLogInfoDeactivateClass(MAP_COOKIE);                                                     CHKERRQ(ierr);
    }
    ierr = PetscStrstr(logList, "vec", &className);                                                       CHKERRQ(ierr);
    if (className) {
      ierr = PetscLogInfoDeactivateClass(VEC_COOKIE);                                                     CHKERRQ(ierr);
    }
  }
  /* Process summary exclusions */
  ierr = PetscOptionsGetString(PETSC_NULL, "-log_summary_exclude", logList, 256, &opt);                   CHKERRQ(ierr);
  if (opt == PETSC_TRUE) {
    ierr = PetscStrstr(logList, "is", &className);                                                        CHKERRQ(ierr);
    if (className) {
      ierr = PetscLogEventDeactivateClass(IS_COOKIE);                                                     CHKERRQ(ierr);
    }
    ierr = PetscStrstr(logList, "map", &className);                                                       CHKERRQ(ierr);
    if (className) {
      ierr = PetscLogEventDeactivateClass(MAP_COOKIE);                                                    CHKERRQ(ierr);
    }
    ierr = PetscStrstr(logList, "vec", &className);                                                       CHKERRQ(ierr);
    if (className) {
      ierr = PetscLogEventDeactivateClass(VEC_COOKIE);                                                    CHKERRQ(ierr);
    }
  }
  /* Special processing */
  ierr = PetscOptionsHasName(PETSC_NULL, "-log_sync", &opt);                                              CHKERRQ(ierr);
  if (opt == PETSC_TRUE) {
    ierr = PetscLogEventActivate(VEC_ScatterBarrier);                                                     CHKERRQ(ierr);
    ierr = PetscLogEventActivate(VEC_NormBarrier);                                                        CHKERRQ(ierr);
    ierr = PetscLogEventActivate(VEC_DotBarrier);                                                         CHKERRQ(ierr);
    ierr = PetscLogEventActivate(VEC_MDotBarrier);                                                        CHKERRQ(ierr);
    ierr = PetscLogEventActivate(VEC_ReduceBarrier);                                                      CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#ifdef PETSC_USE_DYNAMIC_LIBRARIES
EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "PetscDLLibraryRegister"
/*
  PetscDLLibraryRegister - This function is called when the dynamic library it is in is opened.

  This one registers all the TS methods that are in the basic PETSc Vec library.

  Input Parameter:
  path - library path
 */
int PetscDLLibraryRegister(char *path)
{
  int ierr;

  ierr = PetscInitializeNoArguments(); if (ierr) return 1;

  PetscFunctionBegin;
  /*
      If we got here then PETSc was properly loaded
  */
  ierr = VecInitializePackage(path);                                                                      CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END

/* --------------------------------------------------------------------------*/
static char *contents = "PETSc Vector library. \n";

#include "src/sys/src/utils/dlregis.h"

#endif /* PETSC_USE_DYNAMIC_LIBRARIES */
