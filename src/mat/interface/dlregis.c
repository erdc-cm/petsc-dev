/*$Id: dlregis.c,v 1.19 2001/03/23 23:24:34 balay Exp $*/

#include "petscmat.h"

#undef __FUNCT__  
#define __FUNCT__ "MatInitializePackage"
/*@C
  MatInitializePackage - This function initializes everything in the Mat package. It is called
  from PetscDLLibraryRegister() when using dynamic libraries, and on the first call to MatCreate()
  when using static libraries.

  Input Parameter:
  path - The dynamic library path, or PETSC_NULL

  Level: developer

.keywords: Mat, initialize, package
.seealso: PetscInitialize()
@*/
int MatInitializePackage(char *path) {
  static PetscTruth initialized = PETSC_FALSE;
  char              logList[256];
  char             *className;
  PetscTruth        opt;
  int               ierr;

  PetscFunctionBegin;
  if (initialized == PETSC_TRUE) PetscFunctionReturn(0);
  initialized = PETSC_TRUE;
  /* Register Classes */
  ierr = PetscLogClassRegister(&MAT_COOKIE,              "Matrix");                                       CHKERRQ(ierr);
  ierr = PetscLogClassRegister(&MAT_FDCOLORING_COOKIE,   "Matrix FD Coloring");                           CHKERRQ(ierr);
  ierr = PetscLogClassRegister(&MAT_PARTITIONING_COOKIE, "Matrix Partitioning");                          CHKERRQ(ierr);
  ierr = PetscLogClassRegister(&MAT_NULLSPACE_COOKIE,    "Matrix Null Space");                            CHKERRQ(ierr);
  /* Register Constructors and Serializers */
  ierr = MatRegisterAll(path);                                                                            CHKERRQ(ierr);
  /* Register Events */
  ierr = PetscLogEventRegister(&MAT_Mult,                     "MatMult",          MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_MultMatrixFree,           "MatMultMatrixFre", MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_MultMultiple,             "MatMultMultiple",  MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_MultConstrained,          "MatMultConstr",    MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_MultAdd,                  "MatMultAdd",       MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_MultTranspose,            "MatMultTranspose", MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_MultTransposeConstrained, "MatMultTrConstr",  MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_MultTransposeAdd,         "MatMultTrAdd",     MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_Solve,                    "MatSolve",         MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_SolveMultiple,            "MatSolveMultiple", MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_SolveAdd,                 "MatSolveAdd",      MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_SolveTranspose,           "MatSolveTranspos", MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_SolveTransposeAdd,        "MatSolveTrAdd",    MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_Relax,                    "MatRelax",         MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_ForwardSolve,             "MatForwardSolve",  MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_BackwardSolve,            "MatBackwardSolve", MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_LUFactor,                 "MatLUFactor",      MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_LUFactorSymbolic,         "MatLUFactorSym",   MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_LUFactorNumeric,          "MatLUFactorNum",   MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_CholeskyFactor,           "MatCholeskyFctr",  MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_CholeskyFactorSymbolic,   "MatCholFctrSym",   MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_CholeskyFactorNumeric,    "MatCholFctrNum",   MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_ILUFactor,                "MatILUFactor",     MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_ILUFactorSymbolic,        "MatILUFactorSym",  MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_ICCFactorSymbolic,        "MatICCFactorSym",  MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_Copy,                     "MatCopy",          MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_Convert,                  "MatConvert",       MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_Scale,                    "MatScale",         MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_AssemblyBegin,            "MatAssemblyBegin", MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_AssemblyEnd,              "MatAssemblyEnd",   MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_SetValues,                "MatSetValues",     MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_GetValues,                "MatGetValues",     MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_GetRow,                   "MatGetRow",        MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_GetSubMatrices,           "MatGetSubMatrice", MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_GetColoring,              "MatGetColoring",   MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_GetOrdering,              "MatGetOrdering",   MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_IncreaseOverlap,          "MatIncreaseOvrlp", MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_Partitioning,             "MatPartitioning",  MAT_PARTITIONING_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_ZeroEntries,              "MatZeroEntries",   MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_Load,                     "MatLoad",          MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_View,                     "MatView",          MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_AXPY,                     "MatAXPY",          MAT_COOKIE);            CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_FDColoringCreate,         "MatFDColorCreate", MAT_FDCOLORING_COOKIE); CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_FDColoringApply,          "MatFDColorApply",  MAT_FDCOLORING_COOKIE); CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_Transpose,"MatTranspose",MAT_COOKIE);CHKERRQ(ierr);
  /* Process info exclusions */
  ierr = PetscOptionsGetString(PETSC_NULL, "-log_info_exclude", logList, 256, &opt);                      CHKERRQ(ierr);
  if (opt == PETSC_TRUE) {
    ierr = PetscStrstr(logList, "mat", &className);                                                       CHKERRQ(ierr);
    if (className) {
      ierr = PetscLogInfoDeactivateClass(MAT_COOKIE);                                                     CHKERRQ(ierr);
    }
  }
  /* Process summary exclusions */
  ierr = PetscOptionsGetString(PETSC_NULL, "-log_summary_exclude", logList, 256, &opt);                   CHKERRQ(ierr);
  if (opt == PETSC_TRUE) {
    ierr = PetscStrstr(logList, "mat", &className);                                                       CHKERRQ(ierr);
    if (className) {
      ierr = PetscLogEventDeactivateClass(MAT_COOKIE);                                                    CHKERRQ(ierr);
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

  This one registers all the TS methods that are in the basic PETSc Matrix library.

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
  ierr = MatInitializePackage(path);                                                                      CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END

/* --------------------------------------------------------------------------*/
static char *contents = "PETSc Matrix library. \n";
static char *authors  = PETSC_AUTHOR_INFO;

#include "src/sys/src/utils/dlregis.h"

#endif /* PETSC_USE_DYNAMIC_LIBRARIES */
