#define PETSCKSP_DLL

#include "../src/ksp/pc/impls/mg/mgimpl.h"       /*I "petscksp.h" I*/
                          /*I "petscmg.h"   I*/

#undef __FUNCT__  
#define __FUNCT__ "PCMGDefaultResidual"
/*@C
   PCMGDefaultResidual - Default routine to calculate the residual.

   Collective on Mat and Vec

   Input Parameters:
+  mat - the matrix
.  b   - the right-hand-side
-  x   - the approximate solution
 
   Output Parameter:
.  r - location to store the residual

   Level: advanced

.keywords: MG, default, multigrid, residual

.seealso: PCMGSetResidual()
@*/
PetscErrorCode PETSCKSP_DLLEXPORT PCMGDefaultResidual(Mat mat,Vec b,Vec x,Vec r)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = MatMult(mat,x,r);CHKERRQ(ierr);
  ierr = VecAYPX(r,-1.0,b);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* ---------------------------------------------------------------------------*/

#undef __FUNCT__  
#define __FUNCT__ "PCMGGetCoarseSolve"
/*@
   PCMGGetCoarseSolve - Gets the solver context to be used on the coarse grid.

   Not Collective

   Input Parameter:
.  pc - the multigrid context 

   Output Parameter:
.  ksp - the coarse grid solver context 

   Level: advanced

.keywords: MG, multigrid, get, coarse grid
@*/ 
PetscErrorCode PETSCKSP_DLLEXPORT PCMGGetCoarseSolve(PC pc,KSP *ksp)  
{ 
  PC_MG **mg = (PC_MG**)pc->data;

  PetscFunctionBegin;
  *ksp =  mg[0]->smoothd;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PCMGSetResidual"
/*@C
   PCMGSetResidual - Sets the function to be used to calculate the residual 
   on the lth level. 

   Collective on PC and Mat

   Input Parameters:
+  pc       - the multigrid context
.  l        - the level (0 is coarsest) to supply
.  residual - function used to form residual (usually PCMGDefaultResidual)
-  mat      - matrix associated with residual

   Level: advanced

.keywords:  MG, set, multigrid, residual, level

.seealso: PCMGDefaultResidual()
@*/
PetscErrorCode PETSCKSP_DLLEXPORT PCMGSetResidual(PC pc,PetscInt l,PetscErrorCode (*residual)(Mat,Vec,Vec,Vec),Mat mat) 
{
  PC_MG **mg = (PC_MG**)pc->data;

  PetscFunctionBegin;
  if (!mg) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Must set MG levels before calling");

  mg[l]->residual = residual;  
  mg[l]->A        = mat;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PCMGSetInterpolation"
/*@
   PCMGSetInterpolation - Sets the function to be used to calculate the 
   interpolation from l-1 to the lth level

   Collective on PC and Mat

   Input Parameters:
+  pc  - the multigrid context
.  mat - the interpolation operator
-  l   - the level (0 is coarsest) to supply [do not supply 0]

   Level: advanced

   Notes:
          Usually this is the same matrix used also to set the restriction
    for the same level.

          One can pass in the interpolation matrix or its transpose; PETSc figures
    out from the matrix size which one it is.

.keywords:  multigrid, set, interpolate, level

.seealso: PCMGSetRestriction()
@*/
PetscErrorCode PETSCKSP_DLLEXPORT PCMGSetInterpolation(PC pc,PetscInt l,Mat mat)
{ 
  PC_MG          **mg = (PC_MG**)pc->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!mg) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Must set MG levels before calling");
  if (!l) SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"Do not set interpolation routine for coarsest level");
  ierr = PetscObjectReference((PetscObject)mat);CHKERRQ(ierr);
  if (mg[l]->interpolate) {ierr = MatDestroy(mg[l]->interpolate);CHKERRQ(ierr);}
  mg[l]->interpolate = mat;  
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PCMGSetRestriction"
/*@
   PCMGSetRestriction - Sets the function to be used to restrict vector
   from level l to l-1. 

   Collective on PC and Mat

   Input Parameters:
+  pc - the multigrid context 
.  mat - the restriction matrix
-  l - the level (0 is coarsest) to supply [Do not supply 0]

   Level: advanced

   Notes: 
          Usually this is the same matrix used also to set the interpolation
    for the same level.

          One can pass in the interpolation matrix or its transpose; PETSc figures
    out from the matrix size which one it is.

         If you do not set this, the transpose of the Mat set with PCMGSetInterpolation()
    is used.

.keywords: MG, set, multigrid, restriction, level

.seealso: PCMGSetInterpolation()
@*/
PetscErrorCode PETSCKSP_DLLEXPORT PCMGSetRestriction(PC pc,PetscInt l,Mat mat)  
{
  PetscErrorCode ierr;
  PC_MG          **mg = (PC_MG**)pc->data;

  PetscFunctionBegin;
  if (!mg) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Must set MG levels before calling");
  if (!l) SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"Do not set restriction routine for coarsest level");
  ierr = PetscObjectReference((PetscObject)mat);CHKERRQ(ierr);
  if (mg[l]->restrct) {ierr = MatDestroy(mg[l]->restrct);CHKERRQ(ierr);}
  mg[l]->restrct  = mat;  
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PCMGGetSmoother"
/*@
   PCMGGetSmoother - Gets the KSP context to be used as smoother for 
   both pre- and post-smoothing.  Call both PCMGGetSmootherUp() and 
   PCMGGetSmootherDown() to use different functions for pre- and 
   post-smoothing.

   Not Collective, KSP returned is parallel if PC is 

   Input Parameters:
+  pc - the multigrid context 
-  l - the level (0 is coarsest) to supply

   Ouput Parameters:
.  ksp - the smoother

   Level: advanced

.keywords: MG, get, multigrid, level, smoother, pre-smoother, post-smoother

.seealso: PCMGGetSmootherUp(), PCMGGetSmootherDown()
@*/
PetscErrorCode PETSCKSP_DLLEXPORT PCMGGetSmoother(PC pc,PetscInt l,KSP *ksp)
{
  PC_MG **mg = (PC_MG**)pc->data;

  PetscFunctionBegin;
  *ksp = mg[l]->smoothd;  
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PCMGGetSmootherUp"
/*@
   PCMGGetSmootherUp - Gets the KSP context to be used as smoother after 
   coarse grid correction (post-smoother). 

   Not Collective, KSP returned is parallel if PC is

   Input Parameters:
+  pc - the multigrid context 
-  l  - the level (0 is coarsest) to supply

   Ouput Parameters:
.  ksp - the smoother

   Level: advanced

.keywords: MG, multigrid, get, smoother, up, post-smoother, level

.seealso: PCMGGetSmootherUp(), PCMGGetSmootherDown()
@*/
PetscErrorCode PETSCKSP_DLLEXPORT PCMGGetSmootherUp(PC pc,PetscInt l,KSP *ksp)
{
  PC_MG          **mg = (PC_MG**)pc->data;
  PetscErrorCode ierr;
  const char     *prefix;
  MPI_Comm       comm;

  PetscFunctionBegin;
  /*
     This is called only if user wants a different pre-smoother from post.
     Thus we check if a different one has already been allocated, 
     if not we allocate it.
  */
  if (!l) SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"There is no such thing as a up smoother on the coarse grid");
  if (mg[l]->smoothu == mg[l]->smoothd) {
    ierr = PetscObjectGetComm((PetscObject)mg[l]->smoothd,&comm);CHKERRQ(ierr);
    ierr = KSPGetOptionsPrefix(mg[l]->smoothd,&prefix);CHKERRQ(ierr);
    ierr = KSPCreate(comm,&mg[l]->smoothu);CHKERRQ(ierr);
    ierr = PetscObjectIncrementTabLevel((PetscObject)mg[l]->smoothu,(PetscObject)pc,mg[0]->levels-l);CHKERRQ(ierr);
    ierr = KSPSetTolerances(mg[l]->smoothu,PETSC_DEFAULT,PETSC_DEFAULT,PETSC_DEFAULT,1);CHKERRQ(ierr);
    ierr = KSPSetOptionsPrefix(mg[l]->smoothu,prefix);CHKERRQ(ierr);
    ierr = PetscLogObjectParent(pc,mg[l]->smoothu);CHKERRQ(ierr);
  }
  if (ksp) *ksp = mg[l]->smoothu;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PCMGGetSmootherDown"
/*@
   PCMGGetSmootherDown - Gets the KSP context to be used as smoother before 
   coarse grid correction (pre-smoother). 

   Not Collective, KSP returned is parallel if PC is

   Input Parameters:
+  pc - the multigrid context 
-  l  - the level (0 is coarsest) to supply

   Ouput Parameters:
.  ksp - the smoother

   Level: advanced

.keywords: MG, multigrid, get, smoother, down, pre-smoother, level

.seealso: PCMGGetSmootherUp(), PCMGGetSmoother()
@*/
PetscErrorCode PETSCKSP_DLLEXPORT PCMGGetSmootherDown(PC pc,PetscInt l,KSP *ksp)
{
  PetscErrorCode ierr;
  PC_MG          **mg = (PC_MG**)pc->data;

  PetscFunctionBegin;
  /* make sure smoother up and down are different */
  if (l != 0) {
    ierr = PCMGGetSmootherUp(pc,l,PETSC_NULL);CHKERRQ(ierr);
  }
  *ksp = mg[l]->smoothd;  
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PCMGSetCyclesOnLevel"
/*@
   PCMGSetCyclesOnLevel - Sets the number of cycles to run on this level. 

   Collective on PC

   Input Parameters:
+  pc - the multigrid context 
.  l  - the level (0 is coarsest) this is to be used for
-  n  - the number of cycles

   Level: advanced

.keywords: MG, multigrid, set, cycles, V-cycle, W-cycle, level

.seealso: PCMGSetCycles()
@*/
PetscErrorCode PETSCKSP_DLLEXPORT PCMGSetCyclesOnLevel(PC pc,PetscInt l,PetscInt c) 
{
  PC_MG **mg = (PC_MG**)pc->data;

  PetscFunctionBegin;
  if (!mg) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Must set MG levels before calling");
  mg[l]->cycles  = c;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PCMGSetRhs"
/*@
   PCMGSetRhs - Sets the vector space to be used to store the right-hand side
   on a particular level. 

   Collective on PC and Vec

   Input Parameters:
+  pc - the multigrid context 
.  l  - the level (0 is coarsest) this is to be used for
-  c  - the space

   Level: advanced

   Notes: If this is not provided PETSc will automatically generate one.

          You do not need to keep a reference to this vector if you do 
          not need it PCDestroy() will properly free it.

.keywords: MG, multigrid, set, right-hand-side, rhs, level

.seealso: PCMGSetX(), PCMGSetR()
@*/
PetscErrorCode PETSCKSP_DLLEXPORT PCMGSetRhs(PC pc,PetscInt l,Vec c)  
{ 
  PetscErrorCode ierr;
  PC_MG          **mg = (PC_MG**)pc->data;

  PetscFunctionBegin;
  if (!mg) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Must set MG levels before calling");
  if (l == mg[0]->levels-1) SETERRQ(PETSC_ERR_ARG_INCOMP,"Do not set rhs for finest level");
  ierr = PetscObjectReference((PetscObject)c);CHKERRQ(ierr);
  if (mg[l]->b) {ierr = VecDestroy(mg[l]->b);CHKERRQ(ierr);}
  mg[l]->b  = c;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PCMGSetX"
/*@
   PCMGSetX - Sets the vector space to be used to store the solution on a 
   particular level.

   Collective on PC and Vec

   Input Parameters:
+  pc - the multigrid context 
.  l - the level (0 is coarsest) this is to be used for
-  c - the space

   Level: advanced

   Notes: If this is not provided PETSc will automatically generate one.

          You do not need to keep a reference to this vector if you do 
          not need it PCDestroy() will properly free it.

.keywords: MG, multigrid, set, solution, level

.seealso: PCMGSetRhs(), PCMGSetR()
@*/
PetscErrorCode PETSCKSP_DLLEXPORT PCMGSetX(PC pc,PetscInt l,Vec c)  
{ 
  PetscErrorCode ierr;
  PC_MG          **mg = (PC_MG**)pc->data;

  PetscFunctionBegin;
  if (!mg) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Must set MG levels before calling");
  if (l == mg[0]->levels-1) SETERRQ(PETSC_ERR_ARG_INCOMP,"Do not set rhs for finest level");
  ierr = PetscObjectReference((PetscObject)c);CHKERRQ(ierr);
  if (mg[l]->x) {ierr = VecDestroy(mg[l]->x);CHKERRQ(ierr);}
  mg[l]->x  = c;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PCMGSetR"
/*@
   PCMGSetR - Sets the vector space to be used to store the residual on a
   particular level. 

   Collective on PC and Vec

   Input Parameters:
+  pc - the multigrid context 
.  l - the level (0 is coarsest) this is to be used for
-  c - the space

   Level: advanced

   Notes: If this is not provided PETSc will automatically generate one.

          You do not need to keep a reference to this vector if you do 
          not need it PCDestroy() will properly free it.

.keywords: MG, multigrid, set, residual, level
@*/
PetscErrorCode PETSCKSP_DLLEXPORT PCMGSetR(PC pc,PetscInt l,Vec c)
{ 
  PetscErrorCode ierr;
  PC_MG          **mg = (PC_MG**)pc->data;

  PetscFunctionBegin;
  if (!mg) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Must set MG levels before calling");
  if (!l) SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"Need not set residual vector for coarse grid");
  ierr = PetscObjectReference((PetscObject)c);CHKERRQ(ierr);
  if (mg[l]->r) {ierr = VecDestroy(mg[l]->r);CHKERRQ(ierr);}
  mg[l]->r  = c;
  PetscFunctionReturn(0);
}
