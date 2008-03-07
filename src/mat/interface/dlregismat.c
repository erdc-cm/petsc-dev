#define PETSCMAT_DLL

#include "include/private/matimpl.h"

const char *MatOptions[] = {"ROW_ORIENTED","NEW_NONZERO_LOCATIONS",
              "SYMMETRIC",
              "STRUCTURALLY_SYMMETRIC",
              "NEW_DIAGONALS",
              "IGNORE_OFF_PROC_ENTRIES",
              "NEW_NONZERO_LOCATION_ERR",
              "NEW_NONZERO_ALLOCATION_ERR","USE_HASH_TABLE",
              "KEEP_ZEROED_ROWS","IGNORE_ZERO_ENTRIES","USE_INODES",
              "HERMITIAN",
              "SYMMETRY_ETERNAL",
              "USE_COMPRESSEDROW",
              "IGNORE_LOWER_TRIANGULAR","ERROR_LOWER_TRIANGULAR","GETROW_UPPERTRIANGULAR","MatOption","MAT_",0};

EXTERN PetscErrorCode PETSCMAT_DLLEXPORT MatMFFDInitializePackage(const char[]);
EXTERN PetscErrorCode PETSCMAT_DLLEXPORT PetscPLAPACKInitializePackage(const char[]);

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
PetscErrorCode PETSCMAT_DLLEXPORT MatInitializePackage(const char path[]) 
{
  static PetscTruth initialized = PETSC_FALSE;
  char              logList[256];
  char              *className;
  PetscTruth        opt;
  PetscErrorCode    ierr;

  PetscFunctionBegin;
  /* Inialize subpackage */
  ierr = MatMFFDInitializePackage(PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscPLAPACKInitializePackage(PETSC_NULL);CHKERRQ(ierr);
  if (initialized) PetscFunctionReturn(0);
  initialized = PETSC_TRUE;
  /* Register Classes */
  ierr = PetscLogClassRegister(&MAT_COOKIE,              "Matrix");CHKERRQ(ierr);
  ierr = PetscLogClassRegister(&MAT_FDCOLORING_COOKIE,   "Matrix FD Coloring");CHKERRQ(ierr);
  ierr = PetscLogClassRegister(&MAT_PARTITIONING_COOKIE, "Matrix Partitioning");CHKERRQ(ierr);
  ierr = PetscLogClassRegister(&MAT_NULLSPACE_COOKIE,    "Matrix Null Space");CHKERRQ(ierr);
  /* Register Constructors */
  ierr = MatRegisterAll(path);CHKERRQ(ierr);
  /* Register Events */
  ierr = PetscLogEventRegister(&MAT_Mult,                     "MatMult",          MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_Mults,                    "MatMults",         MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_MultConstrained,          "MatMultConstr",    MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_MultAdd,                  "MatMultAdd",       MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_MultTranspose,            "MatMultTranspose", MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_MultTransposeConstrained, "MatMultTrConstr",  MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_MultTransposeAdd,         "MatMultTrAdd",     MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_Solve,                    "MatSolve",         MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_Solves,                   "MatSolves",        MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_SolveAdd,                 "MatSolveAdd",      MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_SolveTranspose,           "MatSolveTranspos", MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_SolveTransposeAdd,        "MatSolveTrAdd",    MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_Relax,                    "MatRelax",         MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_ForwardSolve,             "MatForwardSolve",  MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_BackwardSolve,            "MatBackwardSolve", MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_LUFactor,                 "MatLUFactor",      MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_LUFactorSymbolic,         "MatLUFactorSym",   MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_LUFactorNumeric,          "MatLUFactorNum",   MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_CholeskyFactor,           "MatCholeskyFctr",  MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_CholeskyFactorSymbolic,   "MatCholFctrSym",   MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_CholeskyFactorNumeric,    "MatCholFctrNum",   MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_ILUFactor,                "MatILUFactor",     MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_ILUFactorSymbolic,        "MatILUFactorSym",  MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_ICCFactorSymbolic,        "MatICCFactorSym",  MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_Copy,                     "MatCopy",          MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_Convert,                  "MatConvert",       MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_Scale,                    "MatScale",         MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_AssemblyBegin,            "MatAssemblyBegin", MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_AssemblyEnd,              "MatAssemblyEnd",   MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_SetValues,                "MatSetValues",     MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_GetValues,                "MatGetValues",     MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_GetRow,                   "MatGetRow",        MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_GetRowIJ,                 "MatGetRowIJ",      MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_GetSubMatrices,           "MatGetSubMatrice", MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_GetColoring,              "MatGetColoring",   MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_GetOrdering,              "MatGetOrdering",   MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_IncreaseOverlap,          "MatIncreaseOvrlp", MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_Partitioning,             "MatPartitioning",  MAT_PARTITIONING_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_ZeroEntries,              "MatZeroEntries",   MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_Load,                     "MatLoad",          MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_View,                     "MatView",          MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_AXPY,                     "MatAXPY",          MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_FDColoringCreate,         "MatFDColorCreate", MAT_FDCOLORING_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_FDColoringApply,          "MatFDColorApply",  MAT_FDCOLORING_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_FDColoringFunction,       "MatFDColorFunc",   MAT_FDCOLORING_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_Transpose,                "MatTranspose",     MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_MatMult,                  "MatMatMult",       MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_MatMultSymbolic,          "MatMatMultSym",    MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_MatMultNumeric,           "MatMatMultNum",    MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_PtAP,                     "MatPtAP",          MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_PtAPSymbolic,             "MatPtAPSymbolic",  MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_PtAPNumeric,              "MatPtAPNumeric",   MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_MatMultTranspose,         "MatMatMultTrans",  MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_MatMultTransposeSymbolic, "MatMatMultTrnSym" ,MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_MatMultTransposeNumeric,  "MatMatMultTrnNum", MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_GetRedundantMatrix,       "MAT_GetRedundantMatrix",MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_GetSequentialNonzeroStructure, "MAT_GetSequentialNonzeroStructure",MAT_COOKIE);CHKERRQ(ierr);

  /* these may be specific to MPIAIJ matrices */
  ierr = PetscLogEventRegister(&MAT_Seqstompinum,             "MatMerge_SeqsToMPINumeric",MAT_COOKIE);
  ierr = PetscLogEventRegister(&MAT_Seqstompisym,             "MatMerge_SeqsToMPISymbolic",MAT_COOKIE);
  ierr = PetscLogEventRegister(&MAT_Seqstompi,                "MatMerge_SeqsToMPI",MAT_COOKIE);
  ierr = PetscLogEventRegister(&MAT_Getlocalmat,              "MatGetLocalMat",MAT_COOKIE);
  ierr = PetscLogEventRegister(&MAT_Getlocalmatcondensed,     "MatGetLocalMatCondensed",MAT_COOKIE);
  ierr = PetscLogEventRegister(&MAT_GetBrowsOfAcols,          "MatGetBrowsOfAcols",MAT_COOKIE);
  ierr = PetscLogEventRegister(&MAT_GetBrowsOfAocols,         "MatGetBrAoCol",MAT_COOKIE);

  ierr = PetscLogEventRegister(&MAT_Applypapt_symbolic,       "MatApplyPAPt_Symbolic",MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_Applypapt_numeric,        "MatApplyPAPt_Numeric",MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_Applypapt,                "MatApplyPAPt",MAT_COOKIE);CHKERRQ(ierr);

  ierr = PetscLogEventRegister(&MAT_Getsymtranspose,          "MatGetSymbolicTranspose",MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_Getsymtransreduced,       "MatGetSymbolicTransposeReduced",MAT_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&MAT_Transpose_SeqAIJ,         "MatTranspose_SeqAIJ_FAST",MAT_COOKIE);CHKERRQ(ierr);

  /* Turn off high traffic events by default */
  ierr = PetscLogEventSetActiveAll(MAT_SetValues, PETSC_FALSE);CHKERRQ(ierr);
  /* Process info exclusions */
  ierr = PetscOptionsGetString(PETSC_NULL, "-info_exclude", logList, 256, &opt);CHKERRQ(ierr);
  if (opt) {
    ierr = PetscStrstr(logList, "mat", &className);CHKERRQ(ierr);
    if (className) {
      ierr = PetscInfoDeactivateClass(MAT_COOKIE);CHKERRQ(ierr);
    }
  }
  /* Process summary exclusions */
  ierr = PetscOptionsGetString(PETSC_NULL, "-log_summary_exclude", logList, 256, &opt);CHKERRQ(ierr);
  if (opt) {
    ierr = PetscStrstr(logList, "mat", &className);CHKERRQ(ierr);
    if (className) {
      ierr = PetscLogEventDeactivateClass(MAT_COOKIE);CHKERRQ(ierr);
    }
  }
  PetscFunctionReturn(0);
}

#ifdef PETSC_USE_DYNAMIC_LIBRARIES
EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "PetscDLLibraryRegister_petscmat"
/*
  PetscDLLibraryRegister - This function is called when the dynamic library it is in is opened.

  This one registers all the matrix methods that are in the basic PETSc Matrix library.

  Input Parameter:
  path - library path
 */
PetscErrorCode PETSCMAT_DLLEXPORT PetscDLLibraryRegister_petscmat(const char path[])
{
  PetscErrorCode ierr;

  ierr = PetscInitializeNoArguments(); if (ierr) return 1;

  PetscFunctionBegin;
  /*
      If we got here then PETSc was properly loaded
  */
  ierr = MatInitializePackage(path);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END


#endif /* PETSC_USE_DYNAMIC_LIBRARIES */
