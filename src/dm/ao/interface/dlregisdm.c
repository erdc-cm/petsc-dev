#define PETSCDM_DLL


#include "src/dm/ao/aoimpl.h"
#include "src/dm/da/daimpl.h"
#ifdef PETSC_HAVE_SIEVE
#include "private/meshimpl.h"
#endif

#undef __FUNCT__  
#define __FUNCT__ "DMInitializePackage"
/*@C
  DMInitializePackage - This function initializes everything in the DM package. It is called
  from PetscDLLibraryRegister() when using dynamic libraries, and on the first call to AOCreate()
  or DACreate() when using static libraries.

  Input Parameter:
  path - The dynamic library path, or PETSC_NULL

  Level: developer

.keywords: AO, DA, initialize, package
.seealso: PetscInitialize()
@*/
PetscErrorCode PETSCDM_DLLEXPORT DMInitializePackage(const char path[]) {
  static PetscTruth initialized = PETSC_FALSE;
  char              logList[256];
  char             *className;
  PetscTruth        opt;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (initialized) PetscFunctionReturn(0);
  initialized = PETSC_TRUE;
  /* Register Classes */
  ierr = PetscLogClassRegister(&AO_COOKIE,     "Application Order");CHKERRQ(ierr);
  ierr = PetscLogClassRegister(&DA_COOKIE,     "Distributed array");CHKERRQ(ierr);
  ierr = PetscLogClassRegister(&ADDA_COOKIE,   "Arbitrary Dimension Distributed array");CHKERRQ(ierr);
#ifdef PETSC_HAVE_SIEVE
  ierr = PetscLogClassRegister(&MESH_COOKIE,       "Mesh");CHKERRQ(ierr);
  ierr = PetscLogClassRegister(&SECTIONREAL_COOKIE,"SectionReal");CHKERRQ(ierr);
  ierr = PetscLogClassRegister(&SECTIONINT_COOKIE, "SectionInt");CHKERRQ(ierr);
#endif
  /* Register Constructors */
#ifdef PETSC_HAVE_SIEVE
  ierr = MeshRegisterAll(path);CHKERRQ(ierr);
#endif
  /* Register Events */
  ierr = PetscLogEventRegister(&AO_PetscToApplication,       "AOPetscToApplication", AO_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&AO_ApplicationToPetsc,       "AOApplicationToPetsc", AO_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&DA_GlobalToLocal,            "DAGlobalToLocal",      DA_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&DA_LocalToGlobal,            "DALocalToGlobal",      DA_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&DA_LocalADFunction,          "DALocalADFunc",        DA_COOKIE);CHKERRQ(ierr);
#ifdef PETSC_HAVE_SIEVE
  ierr = PetscLogEventRegister(&Mesh_View,                   "MeshView",             MESH_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&Mesh_GetGlobalScatter,       "MeshGetGlobalScatter", MESH_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&Mesh_restrictVector,         "MeshRestrictVector",   MESH_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&Mesh_assembleVector,         "MeshAssembleVector",   MESH_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&Mesh_assembleVectorComplete, "MeshAssemVecComplete", MESH_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&Mesh_assembleMatrix,         "MeshAssembleMatrix",   MESH_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&Mesh_updateOperator,         "MeshUpdateOperator",   MESH_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&SectionReal_View,            "SectionRealView",      SECTIONREAL_COOKIE);CHKERRQ(ierr);
  ierr = PetscLogEventRegister(&SectionInt_View,             "SectionIntView",       SECTIONINT_COOKIE);CHKERRQ(ierr);
#endif
  /* Process info exclusions */
  ierr = PetscOptionsGetString(PETSC_NULL, "-info_exclude", logList, 256, &opt);CHKERRQ(ierr);
  if (opt) {
    ierr = PetscStrstr(logList, "ao", &className);CHKERRQ(ierr);
    if (className) {
      ierr = PetscInfoDeactivateClass(AO_COOKIE);CHKERRQ(ierr);
    }
    ierr = PetscStrstr(logList, "da", &className);CHKERRQ(ierr);
    if (className) {
      ierr = PetscInfoDeactivateClass(DA_COOKIE);CHKERRQ(ierr);
    }
#ifdef PETSC_HAVE_SIEVE
    ierr = PetscStrstr(logList, "mesh", &className);CHKERRQ(ierr);
    if (className) {
      ierr = PetscInfoDeactivateClass(MESH_COOKIE);CHKERRQ(ierr);
    }
    ierr = PetscStrstr(logList, "sectionreal", &className);CHKERRQ(ierr);
    if (className) {
      ierr = PetscInfoDeactivateClass(SECTIONREAL_COOKIE);CHKERRQ(ierr);
    }
    ierr = PetscStrstr(logList, "sectionint", &className);CHKERRQ(ierr);
    if (className) {
      ierr = PetscInfoDeactivateClass(SECTIONINT_COOKIE);CHKERRQ(ierr);
    }
#endif
  }
  /* Process summary exclusions */
  ierr = PetscOptionsGetString(PETSC_NULL, "-log_summary_exclude", logList, 256, &opt);CHKERRQ(ierr);
  if (opt) {
    ierr = PetscStrstr(logList, "ao", &className);CHKERRQ(ierr);
    if (className) {
      ierr = PetscLogEventDeactivateClass(AO_COOKIE);CHKERRQ(ierr);
    }
    ierr = PetscStrstr(logList, "da", &className);CHKERRQ(ierr);
    if (className) {
      ierr = PetscLogEventDeactivateClass(DA_COOKIE);CHKERRQ(ierr);
    }
#ifdef PETSC_HAVE_SIEVE
    ierr = PetscStrstr(logList, "mesh", &className);CHKERRQ(ierr);
    if (className) {
      ierr = PetscLogEventDeactivateClass(MESH_COOKIE);CHKERRQ(ierr);
    }
    ierr = PetscStrstr(logList, "sectionreal", &className);CHKERRQ(ierr);
    if (className) {
      ierr = PetscLogEventDeactivateClass(SECTIONREAL_COOKIE);CHKERRQ(ierr);
    }
    ierr = PetscStrstr(logList, "sectionint", &className);CHKERRQ(ierr);
    if (className) {
      ierr = PetscLogEventDeactivateClass(SECTIONINT_COOKIE);CHKERRQ(ierr);
    }
#endif
  }
  PetscFunctionReturn(0);
}

#ifdef PETSC_USE_DYNAMIC_LIBRARIES
EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "DMFinalizePackage"
/*@C
  DMFinalizePackage - This function finalizes everything in the DM package. It is called
  from PetscFinalize().

  Level: developer

.keywords: AO, DA, initialize, package
.seealso: PetscInitialize()
@*/
PetscErrorCode PETSCDM_DLLEXPORT DMFinalizePackage() {
#ifdef PETSC_HAVE_SIEVE
  PetscErrorCode ierr;
#endif

  PetscFunctionBegin;
#ifdef PETSC_HAVE_SIEVE
  ierr = MeshFinalize();CHKERRQ(ierr);
#endif
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscDLLibraryRegister_petscdm"
/*
  PetscDLLibraryRegister - This function is called when the dynamic library it is in is opened.

  This one registers all the mesh generators and partitioners that are in
  the basic DM library.

  Input Parameter:
  path - library path
*/
PetscErrorCode PETSCDM_DLLEXPORT PetscDLLibraryRegister_petscdm(const char path[])
{
  PetscErrorCode ierr;

  ierr = PetscInitializeNoArguments();
  if (ierr) return(1);

  /*
      If we got here then PETSc was properly loaded
  */
  ierr = DMInitializePackage(path);CHKERRQ(ierr);
  return(0);
}
EXTERN_C_END

#endif /* PETSC_USE_DYNAMIC_LIBRARIES */
