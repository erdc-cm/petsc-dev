/*$Id: damgsnes.c,v 1.51 2001/08/06 21:18:06 bsmith Exp $*/
 
#include "petscda.h"      /*I      "petscda.h"     I*/
#include "petscmg.h"      /*I      "petscmg.h"    I*/


/*
   Evaluates the Jacobian on all of the grids. It is used by DMMG to provide the 
   ComputeJacobian() function that SNESSetJacobian() requires.
*/
#undef __FUNCT__
#define __FUNCT__ "DMMGComputeJacobian_Multigrid"
int DMMGComputeJacobian_Multigrid(SNES snes,Vec X,Mat *J,Mat *B,MatStructure *flag,void *ptr)
{
  DMMG       *dmmg = (DMMG*)ptr;
  int        ierr,i,nlevels = dmmg[0]->nlevels;
  SLES       sles,lsles;
  PC         pc;
  PetscTruth ismg;
  Vec        W;

  PetscFunctionBegin;
  if (!dmmg) SETERRQ(1,"Passing null as user context which should contain DMMG");

  /* compute Jacobian on finest grid */
  ierr = (*DMMGGetFine(dmmg)->computejacobian)(snes,X,J,B,flag,DMMGGetFine(dmmg));CHKERRQ(ierr);
  ierr = MatSNESMFSetBase(DMMGGetFine(dmmg)->J,X);CHKERRQ(ierr);

  /* create coarser grid Jacobians for preconditioner if multigrid is the preconditioner */
  ierr = SNESGetSLES(snes,&sles);CHKERRQ(ierr);
  ierr = SLESGetPC(sles,&pc);CHKERRQ(ierr);
  ierr = PetscTypeCompare((PetscObject)pc,PCMG,&ismg);CHKERRQ(ierr);
  if (ismg) {

    ierr = MGGetSmoother(pc,nlevels-1,&lsles);CHKERRQ(ierr);
    ierr = SLESSetOperators(lsles,DMMGGetFine(dmmg)->J,DMMGGetFine(dmmg)->B,*flag);CHKERRQ(ierr);

    for (i=nlevels-1; i>0; i--) {

      if (!dmmg[i-1]->w) {
        ierr = VecDuplicate(dmmg[i-1]->x,&dmmg[i-1]->w);CHKERRQ(ierr);
      }

      W    = dmmg[i-1]->w;
      /* restrict X to coarser grid */
      ierr = MatRestrict(dmmg[i]->R,X,W);CHKERRQ(ierr);
      X    = W;      

      /* scale to "natural" scaling for that grid */
      ierr = VecPointwiseMult(dmmg[i]->Rscale,X,X);CHKERRQ(ierr);

      /* tell the base vector for matrix free multiplies */
      ierr = MatSNESMFSetBase(dmmg[i-1]->J,X);CHKERRQ(ierr);

      /* compute Jacobian on coarse grid */
      ierr = (*dmmg[i-1]->computejacobian)(snes,X,&dmmg[i-1]->J,&dmmg[i-1]->B,flag,dmmg[i-1]);CHKERRQ(ierr);

      ierr = MGGetSmoother(pc,i-1,&lsles);CHKERRQ(ierr);
      ierr = SLESSetOperators(lsles,dmmg[i-1]->J,dmmg[i-1]->B,*flag);CHKERRQ(ierr);
    }
  }
  PetscFunctionReturn(0);
}

/* ---------------------------------------------------------------------------*/

#undef __FUNCT__
#define __FUNCT__ "DMMGFormFunction"
/* 
   DMMGFormFunction - This is a universal global FormFunction used by the DMMG code
   when the user provides a local function.

   Input Parameters:
+  snes - the SNES context
.  X - input vector
-  ptr - optional user-defined context, as set by SNESSetFunction()

   Output Parameter:
.  F - function vector

 */
int DMMGFormFunction(SNES snes,Vec X,Vec F,void *ptr)
{
  DMMG             dmmg = (DMMG)ptr;
  int              ierr;
  Vec              localX;
  DA               da = (DA)dmmg->dm;

  PetscFunctionBegin;
  ierr = DAGetLocalVector(da,&localX);CHKERRQ(ierr);
  /*
     Scatter ghost points to local vector, using the 2-step process
        DAGlobalToLocalBegin(), DAGlobalToLocalEnd().
  */
  ierr = DAGlobalToLocalBegin(da,X,INSERT_VALUES,localX);CHKERRQ(ierr);
  ierr = DAGlobalToLocalEnd(da,X,INSERT_VALUES,localX);CHKERRQ(ierr);
  ierr = DAFormFunction1(da,localX,F,dmmg->user);
  ierr = DARestoreLocalVector(da,&localX);CHKERRQ(ierr);
  PetscFunctionReturn(0); 
} 

#undef __FUNCT__
#define __FUNCT__ "SNESDAFormFunction"
/*@C 
   SNESDAFormFunction - This is a universal function evaluation routine that
   may be used with SNESSetFunction() as long as the user context has a DA
   as its first record and the user has called DASetLocalFunction().

   Collective on SNES

   Input Parameters:
+  snes - the SNES context
.  X - input vector
.  F - function vector
-  ptr - pointer to a structure that must have a DA as its first entry. For example this 
         could be a DMMG

   Level: intermediate

.seealso: DASetLocalFunction(), DASetLocalJacobian(), DASetLocalAdicFunction(), DASetLocalAdicMFFunction(),
          SNESSetFunction(), SNESSetJacobian()

@*/
int SNESDAFormFunction(SNES snes,Vec X,Vec F,void *ptr)
{
  int              ierr;
  Vec              localX;
  DA               da = *(DA*)ptr;

  PetscFunctionBegin;
  ierr = DAGetLocalVector(da,&localX);CHKERRQ(ierr);
  /*
     Scatter ghost points to local vector, using the 2-step process
        DAGlobalToLocalBegin(), DAGlobalToLocalEnd().
  */
  ierr = DAGlobalToLocalBegin(da,X,INSERT_VALUES,localX);CHKERRQ(ierr);
  ierr = DAGlobalToLocalEnd(da,X,INSERT_VALUES,localX);CHKERRQ(ierr);
  ierr = DAFormFunction1(da,localX,F,ptr);
  ierr = DARestoreLocalVector(da,&localX);CHKERRQ(ierr);
  PetscFunctionReturn(0); 
} 

/* ---------------------------------------------------------------------------------------------------------------------------*/

#undef __FUNCT__
#define __FUNCT__ "DMMGComputeJacobianWithFD"
int DMMGComputeJacobianWithFD(SNES snes,Vec x1,Mat *J,Mat *B,MatStructure *flag,void *ctx)
{
  int  ierr;
  DMMG dmmg = (DMMG)ctx;
  
  PetscFunctionBegin;
  ierr = SNESDefaultComputeJacobianColor(snes,x1,J,B,flag,dmmg->fdcoloring);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMMGComputeJacobianWithMF"
int DMMGComputeJacobianWithMF(SNES snes,Vec x1,Mat *J,Mat *B,MatStructure *flag,void *ctx)
{
  int  ierr;
  
  PetscFunctionBegin;
  ierr = MatAssemblyBegin(*J,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(*J,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMMGComputeJacobianWithAdic"
/*
    DMMGComputeJacobianWithAdic - Evaluates the Jacobian via Adic when the user has provided
    a local function evaluation routine.
*/
int DMMGComputeJacobianWithAdic(SNES snes,Vec X,Mat *J,Mat *B,MatStructure *flag,void *ptr)
{
  DMMG             dmmg = (DMMG) ptr;
  int              ierr;
  Vec              localX;
  DA               da = (DA) dmmg->dm;

  PetscFunctionBegin;
  ierr = DAGetLocalVector(da,&localX);CHKERRQ(ierr);
  ierr = DAGlobalToLocalBegin(da,X,INSERT_VALUES,localX);CHKERRQ(ierr);
  ierr = DAGlobalToLocalEnd(da,X,INSERT_VALUES,localX);CHKERRQ(ierr);
  ierr = DAComputeJacobian1WithAdic(da,localX,*B,dmmg->user);CHKERRQ(ierr);
  ierr = DARestoreLocalVector(da,&localX);CHKERRQ(ierr);
  /* Assemble true Jacobian; if it is different */
  if (*J != *B) {
    ierr  = MatAssemblyBegin(*J,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
    ierr  = MatAssemblyEnd(*J,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  }
  ierr  = MatSetOption(*B,MAT_NEW_NONZERO_LOCATION_ERR);CHKERRQ(ierr);
  *flag = SAME_NONZERO_PATTERN;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "SNESDAComputeJacobianWithAdic"
/*@
    SNESDAComputeJacobianWithAdic - This is a universal Jacobian evaluation routine
    that may be used with SNESSetJacobian() as long as the user context has a DA as
    its first record and DASetLocalAdicFunction() has been called.  

   Collective on SNES

   Input Parameters:
+  snes - the SNES context
.  X - input vector
.  J - Jacobian
.  B - Jacobian used in preconditioner (usally same as J)
.  flag - indicates if the matrix changed its structure
-  ptr - optional user-defined context, as set by SNESSetFunction()

   Level: intermediate

.seealso: DASetLocalFunction(), DASetLocalAdicFunction(), SNESSetFunction(), SNESSetJacobian()

@*/
int SNESDAComputeJacobianWithAdic(SNES snes,Vec X,Mat *J,Mat *B,MatStructure *flag,void *ptr)
{
  DA   da = *(DA*) ptr;
  int  ierr;
  Vec  localX;

  PetscFunctionBegin;
  ierr = DAGetLocalVector(da,&localX);CHKERRQ(ierr);
  ierr = DAGlobalToLocalBegin(da,X,INSERT_VALUES,localX);CHKERRQ(ierr);
  ierr = DAGlobalToLocalEnd(da,X,INSERT_VALUES,localX);CHKERRQ(ierr);
  ierr = DAComputeJacobian1WithAdic(da,localX,*B,ptr);CHKERRQ(ierr);
  ierr = DARestoreLocalVector(da,&localX);CHKERRQ(ierr);
  /* Assemble true Jacobian; if it is different */
  if (*J != *B) {
    ierr  = MatAssemblyBegin(*J,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
    ierr  = MatAssemblyEnd(*J,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  }
  ierr  = MatSetOption(*B,MAT_NEW_NONZERO_LOCATION_ERR);CHKERRQ(ierr);
  *flag = SAME_NONZERO_PATTERN;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "SNESDAComputeJacobianWithAdifor"
/*
    SNESDAComputeJacobianWithAdifor - This is a universal Jacobian evaluation routine
    that may be used with SNESSetJacobian() from Fortran as long as the user context has 
    a DA as its first record and DASetLocalAdiforFunction() has been called.  

   Collective on SNES

   Input Parameters:
+  snes - the SNES context
.  X - input vector
.  J - Jacobian
.  B - Jacobian used in preconditioner (usally same as J)
.  flag - indicates if the matrix changed its structure
-  ptr - optional user-defined context, as set by SNESSetFunction()

   Level: intermediate

.seealso: DASetLocalFunction(), DASetLocalAdicFunction(), SNESSetFunction(), SNESSetJacobian()

*/
int SNESDAComputeJacobianWithAdifor(SNES snes,Vec X,Mat *J,Mat *B,MatStructure *flag,void *ptr)
{
  DA   da = *(DA*) ptr;
  int  ierr;
  Vec  localX;

  PetscFunctionBegin;
  ierr = DAGetLocalVector(da,&localX);CHKERRQ(ierr);
  ierr = DAGlobalToLocalBegin(da,X,INSERT_VALUES,localX);CHKERRQ(ierr);
  ierr = DAGlobalToLocalEnd(da,X,INSERT_VALUES,localX);CHKERRQ(ierr);
  ierr = DAComputeJacobian1WithAdifor(da,localX,*B,ptr);CHKERRQ(ierr);
  ierr = DARestoreLocalVector(da,&localX);CHKERRQ(ierr);
  /* Assemble true Jacobian; if it is different */
  if (*J != *B) {
    ierr  = MatAssemblyBegin(*J,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
    ierr  = MatAssemblyEnd(*J,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  }
  ierr  = MatSetOption(*B,MAT_NEW_NONZERO_LOCATION_ERR);CHKERRQ(ierr);
  *flag = SAME_NONZERO_PATTERN;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "SNESDAComputeJacobian"
/*
   SNESDAComputeJacobian - This is a universal Jacobian evaluation routine for a
   locally provided Jacobian.

   Collective on SNES

   Input Parameters:
+  snes - the SNES context
.  X - input vector
.  J - Jacobian
.  B - Jacobian used in preconditioner (usally same as J)
.  flag - indicates if the matrix changed its structure
-  ptr - optional user-defined context, as set by SNESSetFunction()

   Level: intermediate

.seealso: DASetLocalFunction(), DASetLocalJacobian(), SNESSetFunction(), SNESSetJacobian()

*/
int SNESDAComputeJacobian(SNES snes,Vec X,Mat *J,Mat *B,MatStructure *flag,void *ptr)
{
  DA   da = *(DA*) ptr;
  int  ierr;
  Vec  localX;

  PetscFunctionBegin;
  ierr = DAGetLocalVector(da,&localX);CHKERRQ(ierr);
  ierr = DAGlobalToLocalBegin(da,X,INSERT_VALUES,localX);CHKERRQ(ierr);
  ierr = DAGlobalToLocalEnd(da,X,INSERT_VALUES,localX);CHKERRQ(ierr);
  ierr = DAComputeJacobian1(da,localX,*B,ptr);CHKERRQ(ierr);
  ierr = DARestoreLocalVector(da,&localX);CHKERRQ(ierr);
  /* Assemble true Jacobian; if it is different */
  if (*J != *B) {
    ierr  = MatAssemblyBegin(*J,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
    ierr  = MatAssemblyEnd(*J,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  }
  ierr  = MatSetOption(*B,MAT_NEW_NONZERO_LOCATION_ERR);CHKERRQ(ierr);
  *flag = SAME_NONZERO_PATTERN;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DMMGSolveSNES"
int DMMGSolveSNES(DMMG *dmmg,int level)
{
  int  ierr,nlevels = dmmg[0]->nlevels,its;

  PetscFunctionBegin;
  dmmg[0]->nlevels = level+1;
  ierr             = SNESSolve(dmmg[level]->snes,dmmg[level]->x,&its);CHKERRQ(ierr);
  dmmg[0]->nlevels = nlevels;
  PetscFunctionReturn(0);
}

/* ===========================================================================================================*/

#undef __FUNCT__  
#define __FUNCT__ "DMMGSetSNES"
/*@C
    DMMGSetSNES - Sets the nonlinear function that defines the nonlinear set of equations
    to be solved using the grid hierarchy.

    Collective on DMMG

    Input Parameter:
+   dmmg - the context
.   function - the function that defines the nonlinear system
-   jacobian - optional function to compute Jacobian

    Options Database:
+    -dmmg_snes_monitor
.    -dmmg_jacobian_fd
.    -dmmg_jacobian_ad
.    -dmmg_jacobian_mf_fd_operator
.    -dmmg_jacobian_mf_fd
.    -dmmg_jacobian_mf_ad_operator
-    -dmmg_jacobian_mf_ad

    Level: advanced

.seealso DMMGCreate(), DMMGDestroy, DMMGSetSLES(), DMMGSetSNESLocal()

@*/
int DMMGSetSNES(DMMG *dmmg,int (*function)(SNES,Vec,Vec,void*),int (*jacobian)(SNES,Vec,Mat*,Mat*,MatStructure*,void*))
{
  int         ierr,i,nlevels = dmmg[0]->nlevels;
  PetscTruth  snesmonitor,mffdoperator,mffd,fdjacobian;
#if defined(PETSC_HAVE_ADIC) && !defined(PETSC_USE_COMPLEX) && !defined(PETSC_USE_SINGLE)
  PetscTruth  mfadoperator,mfad,adjacobian;
#endif
  SLES        sles;
  PetscViewer ascii;
  MPI_Comm    comm;

  PetscFunctionBegin;
  if (!dmmg)     SETERRQ(1,"Passing null as DMMG");
  if (!jacobian) jacobian = DMMGComputeJacobianWithFD;

  ierr = PetscOptionsBegin(dmmg[0]->comm,PETSC_NULL,"DMMG Options","SNES");CHKERRQ(ierr);
    ierr = PetscOptionsName("-dmmg_snes_monitor","Monitor nonlinear convergence","SNESSetMonitor",&snesmonitor);CHKERRQ(ierr);


    ierr = PetscOptionsName("-dmmg_jacobian_fd","Compute sparse Jacobian explicitly with finite differencing","DMMGSetSNES",&fdjacobian);CHKERRQ(ierr);
    if (fdjacobian) jacobian = DMMGComputeJacobianWithFD;
#if defined(PETSC_HAVE_ADIC) && !defined(PETSC_USE_COMPLEX) && !defined(PETSC_USE_SINGLE)
    ierr = PetscOptionsName("-dmmg_jacobian_ad","Compute sparse Jacobian explicitly with ADIC (automatic differentiation)","DMMGSetSNES",&adjacobian);CHKERRQ(ierr);
    if (adjacobian) jacobian = DMMGComputeJacobianWithAdic;
#endif

    ierr = PetscOptionsLogicalGroupBegin("-dmmg_jacobian_mf_fd_operator","Apply Jacobian via matrix free finite differencing","DMMGSetSNES",&mffdoperator);CHKERRQ(ierr);
    ierr = PetscOptionsLogicalGroupEnd("-dmmg_jacobian_mf_fd","Apply Jacobian via matrix free finite differencing even in computing preconditioner","DMMGSetSNES",&mffd);CHKERRQ(ierr);
    if (mffd) mffdoperator = PETSC_TRUE;
#if defined(PETSC_HAVE_ADIC) && !defined(PETSC_USE_COMPLEX) && !defined(PETSC_USE_SINGLE)
    ierr = PetscOptionsLogicalGroupBegin("-dmmg_jacobian_mf_ad_operator","Apply Jacobian via matrix free ADIC (automatic differentiation)","DMMGSetSNES",&mfadoperator);CHKERRQ(ierr);
    ierr = PetscOptionsLogicalGroupEnd("-dmmg_jacobian_mf_ad","Apply Jacobian via matrix free ADIC (automatic differentiation) even in computing preconditioner","DMMGSetSNES",&mfad);CHKERRQ(ierr);
    if (mfad) mfadoperator = PETSC_TRUE;
#endif
  ierr = PetscOptionsEnd();CHKERRQ(ierr);

  /* create solvers for each level */
  for (i=0; i<nlevels; i++) {
    ierr = SNESCreate(dmmg[i]->comm,SNES_NONLINEAR_EQUATIONS,&dmmg[i]->snes);CHKERRQ(ierr);
    if (snesmonitor) {
      ierr = PetscObjectGetComm((PetscObject)dmmg[i]->snes,&comm);CHKERRQ(ierr);
      ierr = PetscViewerASCIIOpen(comm,"stdout",&ascii);CHKERRQ(ierr);
      ierr = PetscViewerASCIISetTab(ascii,nlevels-i);CHKERRQ(ierr);
      ierr = SNESSetMonitor(dmmg[i]->snes,SNESDefaultMonitor,ascii,(int(*)(void*))PetscViewerDestroy);CHKERRQ(ierr);
    }

    if (mffdoperator) {
      ierr = MatCreateSNESMF(dmmg[i]->snes,dmmg[i]->x,&dmmg[i]->J);CHKERRQ(ierr);
      ierr = VecDuplicate(dmmg[i]->x,&dmmg[i]->work1);CHKERRQ(ierr);
      ierr = VecDuplicate(dmmg[i]->x,&dmmg[i]->work2);CHKERRQ(ierr);
      ierr = MatSNESMFSetFunction(dmmg[i]->J,dmmg[i]->work1,function,dmmg[i]);CHKERRQ(ierr);
      if (mffd) {
        dmmg[i]->B = dmmg[i]->J;
        jacobian   = DMMGComputeJacobianWithMF;
      }
#if defined(PETSC_HAVE_ADIC) && !defined(PETSC_USE_COMPLEX) && !defined(PETSC_USE_SINGLE)
    } else if (mfadoperator) {
      ierr = MatRegisterDAAD();CHKERRQ(ierr);
      ierr = MatCreateDAAD((DA)dmmg[i]->dm,&dmmg[i]->J);CHKERRQ(ierr);
      ierr = MatDAADSetCtx(dmmg[i]->J,dmmg[i]->user);CHKERRQ(ierr);
      if (mfad) {
        dmmg[i]->B = dmmg[i]->J;
        jacobian   = DMMGComputeJacobianWithMF;
      }
#endif
    }
    
    if (!dmmg[i]->B) {
      ierr = DMGetMatrix(dmmg[i]->dm,MATMPIAIJ,&dmmg[i]->B);CHKERRQ(ierr);
    } 
    if (!dmmg[i]->J) {
      dmmg[i]->J = dmmg[i]->B;
    }

    ierr = SNESGetSLES(dmmg[i]->snes,&sles);CHKERRQ(ierr);
    ierr = DMMGSetUpLevel(dmmg,sles,i+1);CHKERRQ(ierr);
    
    /*
       if the number of levels is > 1 then we want the coarse solve in the grid sequencing to use LU
       when possible 
    */
    if (nlevels > 1 && i == 0) {
      PC         pc;
      SLES       csles;
      PetscTruth flg1,flg2,flg3;

      ierr = SLESGetPC(sles,&pc);CHKERRQ(ierr);
      ierr = MGGetCoarseSolve(pc,&csles);CHKERRQ(ierr);
      ierr = SLESGetPC(csles,&pc);CHKERRQ(ierr);
      ierr = PetscTypeCompare((PetscObject)pc,PCILU,&flg1);CHKERRQ(ierr);
      ierr = PetscTypeCompare((PetscObject)pc,PCSOR,&flg2);CHKERRQ(ierr);
      ierr = PetscTypeCompare((PetscObject)pc,PETSC_NULL,&flg3);CHKERRQ(ierr);
      if (flg1 || flg2 || flg3) {
        ierr = PCSetType(pc,PCLU);CHKERRQ(ierr);
      }
    }

    ierr = SNESSetFromOptions(dmmg[i]->snes);CHKERRQ(ierr);
    dmmg[i]->solve           = DMMGSolveSNES;
    dmmg[i]->computejacobian = jacobian;
    dmmg[i]->computefunction = function;
  }


  if (jacobian == DMMGComputeJacobianWithFD) {
    ISColoring iscoloring;
    for (i=0; i<nlevels; i++) {
      ierr = DMGetColoring(dmmg[i]->dm,IS_COLORING_LOCAL,&iscoloring);CHKERRQ(ierr);
      ierr = MatFDColoringCreate(dmmg[i]->B,iscoloring,&dmmg[i]->fdcoloring);CHKERRQ(ierr);
      ierr = ISColoringDestroy(iscoloring);CHKERRQ(ierr);
      ierr = MatFDColoringSetFunction(dmmg[i]->fdcoloring,(int(*)(void))function,dmmg[i]);CHKERRQ(ierr);
      ierr = MatFDColoringSetFromOptions(dmmg[i]->fdcoloring);CHKERRQ(ierr);
    }
#if defined(PETSC_HAVE_ADIC) && !defined(PETSC_USE_COMPLEX) && !defined(PETSC_USE_SINGLE)
  } else if (jacobian == DMMGComputeJacobianWithAdic) {
    for (i=0; i<nlevels; i++) {
      ISColoring iscoloring;
      ierr = DMGetColoring(dmmg[i]->dm,IS_COLORING_GHOSTED,&iscoloring);CHKERRQ(ierr);
      ierr = MatSetColoring(dmmg[i]->B,iscoloring);CHKERRQ(ierr);
      ierr = ISColoringDestroy(iscoloring);CHKERRQ(ierr);
    }
#endif
  }

  for (i=0; i<nlevels; i++) {
    ierr = SNESSetJacobian(dmmg[i]->snes,dmmg[i]->J,dmmg[i]->B,DMMGComputeJacobian_Multigrid,dmmg);CHKERRQ(ierr);
    ierr = SNESSetFunction(dmmg[i]->snes,dmmg[i]->b,function,dmmg[i]);CHKERRQ(ierr);
  }

  /* Create interpolation scaling */
  for (i=1; i<nlevels; i++) {
    ierr = DMGetInterpolationScale(dmmg[i-1]->dm,dmmg[i]->dm,dmmg[i]->R,&dmmg[i]->Rscale);CHKERRQ(ierr);
  }

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DMMGSetInitialGuess"
/*@C
    DMMGSetInitialGuess - Sets the function that computes an initial guess, if not given
    uses 0.

    Collective on DMMG and SNES

    Input Parameter:
+   dmmg - the context
-   guess - the function

    Level: advanced

.seealso DMMGCreate(), DMMGDestroy, DMMGSetSLES()

@*/
int DMMGSetInitialGuess(DMMG *dmmg,int (*guess)(SNES,Vec,void*))
{
  int i,nlevels = dmmg[0]->nlevels;

  PetscFunctionBegin;
  for (i=0; i<nlevels; i++) {
    dmmg[i]->initialguess = guess;
  }
  PetscFunctionReturn(0);
}


/*M
    DMMGSetSNESLocal - Sets the local user function that defines the nonlinear set of equations
    that will use the grid hierarchy and (optionally) its derivative.

    Collective on DMMG

   Synopsis:
   int DMMGSetSNESLocal(DMMG *dmmg,DALocalFunction1 function, DALocalFunction1 jacobian,
                        DALocalFunction1 ad_function, DALocalFunction1 admf_function);

    Input Parameter:
+   dmmg - the context
.   function - the function that defines the nonlinear system
.   jacobian - function defines the local part of the Jacobian (not currently supported)
.   ad_function - the name of the function with an ad_ prefix. This is ignored if ADIC is
                  not installed
-   admf_function - the name of the function with an ad_ prefix. This is ignored if ADIC is
                  not installed

    Level: intermediate

    Notes: 
    If ADIC or ADIFOR have been installed, this routine can use ADIC or ADIFOR to compute
    the derivative; however, that function cannot call other functions except those in
    standard C math libraries.

    If ADIC/ADIFOR have not been installed and the Jacobian is not provided, this routine
    uses finite differencing to approximate the Jacobian.

.seealso DMMGCreate(), DMMGDestroy, DMMGSetSLES(), DMMGSetSNES()

M*/

#undef __FUNCT__  
#define __FUNCT__ "DMMGSetSNESLocal_Private"
int DMMGSetSNESLocal_Private(DMMG *dmmg,DALocalFunction1 function,DALocalFunction1 jacobian,DALocalFunction1 ad_function,DALocalFunction1 admf_function)
{
  int ierr,i,nlevels = dmmg[0]->nlevels;
  int (*computejacobian)(SNES,Vec,Mat*,Mat*,MatStructure*,void*) = 0;


  PetscFunctionBegin;
  if (jacobian)         computejacobian = SNESDAComputeJacobian;
#if defined(PETSC_HAVE_ADIC) && !defined(PETSC_USE_COMPLEX) && !defined(PETSC_USE_SINGLE)
  else if (ad_function) computejacobian = DMMGComputeJacobianWithAdic;
#endif

  ierr = DMMGSetSNES(dmmg,DMMGFormFunction,computejacobian);CHKERRQ(ierr);
  for (i=0; i<nlevels; i++) {
    ierr = DASetLocalFunction((DA)dmmg[i]->dm,function);CHKERRQ(ierr);
    ierr = DASetLocalJacobian((DA)dmmg[i]->dm,jacobian);CHKERRQ(ierr);
    ierr = DASetLocalAdicFunction((DA)dmmg[i]->dm,ad_function);CHKERRQ(ierr);
    ierr = DASetLocalAdicMFFunction((DA)dmmg[i]->dm,admf_function);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DMMGFunctioni"
static int DMMGFunctioni(int i,Vec u,PetscScalar* r,void* ctx)
{
  DMMG       dmmg = (DMMG)ctx;
  Vec        U = dmmg->lwork1;
  int        ierr;
  VecScatter gtol;

  PetscFunctionBegin;
  /* copy u into interior part of U */
  ierr = DAGetScatter((DA)dmmg->dm,0,&gtol,0);CHKERRQ(ierr);
  ierr = VecScatterBegin(u,U,INSERT_VALUES,SCATTER_FORWARD_LOCAL,gtol);CHKERRQ(ierr);
  ierr = VecScatterEnd(u,U,INSERT_VALUES,SCATTER_FORWARD_LOCAL,gtol);CHKERRQ(ierr);

  ierr = DAFormFunctioni1((DA)dmmg->dm,i,U,r,dmmg->user);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DMMGFunctioniBase"
static int DMMGFunctioniBase(Vec u,void* ctx)
{
  DMMG dmmg = (DMMG)ctx;
  Vec  U = dmmg->lwork1;
  int  ierr;

  PetscFunctionBegin;
  ierr = DAGlobalToLocalBegin((DA)dmmg->dm,u,INSERT_VALUES,U);CHKERRQ(ierr);  
  ierr = DAGlobalToLocalEnd((DA)dmmg->dm,u,INSERT_VALUES,U);CHKERRQ(ierr);  
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DMMGSetSNESLocali_Private"
int DMMGSetSNESLocali_Private(DMMG *dmmg,int (*functioni)(DALocalInfo*,MatStencil*,void*,PetscScalar*,void*),int (*adi)(DALocalInfo*,MatStencil*,void*,void*,void*),int (*adimf)(DALocalInfo*,MatStencil*,void*,void*,void*))
{
  int ierr,i,nlevels = dmmg[0]->nlevels;

  PetscFunctionBegin;
CHKMEMQ;
  for (i=0; i<nlevels; i++) {
CHKMEMQ;
    ierr = DASetLocalFunctioni((DA)dmmg[i]->dm,functioni);CHKERRQ(ierr);
CHKMEMQ;
    ierr = DASetLocalAdicFunctioni((DA)dmmg[i]->dm,adi);CHKERRQ(ierr);
CHKMEMQ;
    ierr = DASetLocalAdicMFFunctioni((DA)dmmg[i]->dm,adimf);CHKERRQ(ierr);
CHKMEMQ;
    ierr = MatSNESMFSetFunctioni(dmmg[i]->J,DMMGFunctioni);CHKERRQ(ierr);
CHKMEMQ;
    ierr = MatSNESMFSetFunctioniBase(dmmg[i]->J,DMMGFunctioniBase);CHKERRQ(ierr);    
CHKMEMQ;
    ierr = DACreateLocalVector((DA)dmmg[i]->dm,&dmmg[i]->lwork1);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}


#if defined(PETSC_HAVE_ADIC) && !defined(PETSC_USE_COMPLEX) && !defined(PETSC_USE_SINGLE)
EXTERN_C_BEGIN
#include "adic_utils.h"
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "PetscADView"
int PetscADView(int N,int nc,double *ptr,PetscViewer viewer)
{
  int        i,j,nlen  = my_AD_GetDerivTypeSize();
  char       *cptr = (char*)ptr;
  double     *values;

  PetscFunctionBegin;
  for (i=0; i<N; i++) {
    printf("Element %d value %g derivatives: ",i,*(double*)cptr);
    values = my_AD_GetGradArray(cptr);
    for (j=0; j<nc; j++) {
      printf("%g ",*values++);
    }
    printf("\n");
    cptr += nlen;
  }

  PetscFunctionReturn(0);
}

#endif



