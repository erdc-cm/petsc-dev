#define PETSCKSP_DLL

#include "private/kspimpl.h"         /*I "petscksp.h" I*/
#include "../src/ksp/ksp/impls/rich/richardsonimpl.h"


#undef __FUNCT__  
#define __FUNCT__ "KSPRichardsonSetScale"
/*@
    KSPRichardsonSetScale - Set the damping factor; if this routine is not called, the factor 
    defaults to 1.0.

    Collective on KSP

    Input Parameters:
+   ksp - the iterative context
-   scale - the relaxation factor

    Level: intermediate

.keywords: KSP, Richardson, set, scale
@*/
PetscErrorCode PETSCKSP_DLLEXPORT KSPRichardsonSetScale(KSP ksp,PetscReal scale)
{
  PetscErrorCode ierr,(*f)(KSP,PetscReal);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ksp,KSP_CLASSID,1);
  ierr = PetscObjectQueryFunction((PetscObject)ksp,"KSPRichardsonSetScale_C",(void (**)(void))&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(ksp,scale);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "KSPRichardsonSetSelfScale"
/*@
    KSPRichardsonSetSelfScale - Sets Richardson to automatically determine optimal scaling at each iteration to minimize the 2-norm of the 
       preconditioned residual

    Collective on KSP

    Input Parameters:
+   ksp - the iterative context
-   scale - PETSC_TRUE or the default of PETSC_FALSE

    Level: intermediate

    Notes: Requires two extra work vectors. Uses an extra axpy() and VecDotNorm2() per iteration.

    Developer Notes: Could also minimize the 2-norm of the true residual with one less work vector


.keywords: KSP, Richardson, set, scale
@*/
PetscErrorCode PETSCKSP_DLLEXPORT KSPRichardsonSetSelfScale(KSP ksp,PetscTruth scale)
{
  PetscErrorCode ierr,(*f)(KSP,PetscTruth);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ksp,KSP_CLASSID,1);
  ierr = PetscObjectQueryFunction((PetscObject)ksp,"KSPRichardsonSetSelfScale_C",(void (**)(void))&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(ksp,scale);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}
