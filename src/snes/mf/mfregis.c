#define PETSCSNES_DLL

#include "src/snes/mf/snesmfj.h"   /*I  "petscsnes.h"   I*/

EXTERN_C_BEGIN
EXTERN PetscErrorCode PETSCSNES_DLLEXPORT MatSNESMFCreate_DS(MatSNESMF);
EXTERN PetscErrorCode PETSCSNES_DLLEXPORT MatSNESMFCreate_WP(MatSNESMF);
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "MatSNESMFRegisterAll"
/*@C
  MatSNESMFRegisterAll - Registers all of the compute-h in the MatSNESMF package.

  Not Collective

  Level: developer

.keywords: MatSNESMF, register, all

.seealso:  MatSNESMFRegisterDestroy(), MatSNESMFRegisterDynamic), MatSNESMFCreate(), 
           MatSNESMFSetType()
@*/
PetscErrorCode PETSCSNES_DLLEXPORT MatSNESMFRegisterAll(const char *path)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  MatSNESMFRegisterAllCalled = PETSC_TRUE;

  ierr = MatSNESMFRegisterDynamic(MATSNESMF_DS,path,"MatSNESMFCreate_DS",MatSNESMFCreate_DS);CHKERRQ(ierr);
  ierr = MatSNESMFRegisterDynamic(MATSNESMF_WP,path,"MatSNESMFCreate_WP",MatSNESMFCreate_WP);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

