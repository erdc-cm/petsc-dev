/*$Id: snesut.c,v 1.66 2001/08/06 21:17:07 bsmith Exp $*/

#include "src/snes/snesimpl.h"       /*I   "petscsnes.h"   I*/

#undef __FUNCT__  
#define __FUNCT__ "SNESVecViewMonitor"
/*@C
   SNESVecViewMonitor - Monitors progress of the SNES solvers by calling 
   VecView() for the approximate solution at each iteration.

   Collective on SNES

   Input Parameters:
+  snes - the SNES context
.  its - iteration number
.  fgnorm - 2-norm of residual (or gradient)
-  dummy - either a viewer or PETSC_NULL

   Level: intermediate

.keywords: SNES, nonlinear, vector, monitor, view

.seealso: SNESSetMonitor(), SNESDefaultMonitor(), VecView()
@*/
int SNESVecViewMonitor(SNES snes,int its,PetscReal fgnorm,void *dummy)
{
  int         ierr;
  Vec         x;
  PetscViewer viewer = (PetscViewer) dummy;

  PetscFunctionBegin;
  ierr = SNESGetSolution(snes,&x);CHKERRQ(ierr);
  if (!viewer) {
    MPI_Comm comm;
    ierr   = PetscObjectGetComm((PetscObject)snes,&comm);CHKERRQ(ierr);
    viewer = PETSC_VIEWER_DRAW_(comm);
  }
  ierr = VecView(x,viewer);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "SNESVecViewUpdateMonitor"
/*@C
   SNESVecViewUpdateMonitor - Monitors progress of the SNES solvers by calling 
   VecView() for the UPDATE to the solution at each iteration.

   Collective on SNES

   Input Parameters:
+  snes - the SNES context
.  its - iteration number
.  fgnorm - 2-norm of residual (or gradient)
-  dummy - either a viewer or PETSC_NULL

   Level: intermediate

.keywords: SNES, nonlinear, vector, monitor, view

.seealso: SNESSetMonitor(), SNESDefaultMonitor(), VecView()
@*/
int SNESVecViewUpdateMonitor(SNES snes,int its,PetscReal fgnorm,void *dummy)
{
  int         ierr;
  Vec         x;
  PetscViewer viewer = (PetscViewer) dummy;

  PetscFunctionBegin;
  ierr = SNESGetSolutionUpdate(snes,&x);CHKERRQ(ierr);
  if (!viewer) {
    MPI_Comm comm;
    ierr   = PetscObjectGetComm((PetscObject)snes,&comm);CHKERRQ(ierr);
    viewer = PETSC_VIEWER_DRAW_(comm);
  }
  ierr = VecView(x,viewer);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "SNESDefaultMonitor"
/*@C
   SNESDefaultMonitor - Monitoring progress of the SNES solvers (default).

   Collective on SNES

   Input Parameters:
+  snes - the SNES context
.  its - iteration number
.  fgnorm - 2-norm of residual (or gradient)
-  dummy - unused context

   Notes:
   For SNES_NONLINEAR_EQUATIONS methods the routine prints the 
   residual norm at each iteration.

   For SNES_UNCONSTRAINED_MINIMIZATION methods the routine prints the
   function value and gradient norm at each iteration.

   Level: intermediate

.keywords: SNES, nonlinear, default, monitor, norm

.seealso: SNESSetMonitor(), SNESVecViewMonitor()
@*/
int SNESDefaultMonitor(SNES snes,int its,PetscReal fgnorm,void *dummy)
{
  int         ierr;
  PetscViewer viewer = (PetscViewer) dummy;

  PetscFunctionBegin;
  if (!viewer) viewer = PETSC_VIEWER_STDOUT_(snes->comm);

  if (snes->method_class == SNES_NONLINEAR_EQUATIONS) {
    ierr = PetscViewerASCIIPrintf(viewer,"%3d SNES Function norm %14.12e \n",its,fgnorm);CHKERRQ(ierr);
  } else if (snes->method_class == SNES_UNCONSTRAINED_MINIMIZATION) {
    ierr = PetscViewerASCIIPrintf(viewer,"%3d SNES Function value %14.12e, Gradient norm %14.12e \n",its,snes->fc,fgnorm);CHKERRQ(ierr);
  } else SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"Unknown method class");
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "SNESRatioMonitor"
/*@C
   SNESRatioMonitor - Monitoring progress of the SNES solvers, prints ratio
      of residual norm at each iteration to previous

   Collective on SNES

   Input Parameters:
+  snes - the SNES context
.  its - iteration number
.  fgnorm - 2-norm of residual (or gradient)
-  dummy - unused context

   Level: intermediate

.keywords: SNES, nonlinear, monitor, norm

.seealso: SNESSetMonitor(), SNESVecViewMonitor()
@*/
int SNESRatioMonitor(SNES snes,int its,PetscReal fgnorm,void *dummy)
{
  int         ierr,len;
  PetscReal   *history;
  PetscViewer viewer;

  PetscFunctionBegin;
  viewer = PETSC_VIEWER_STDOUT_(snes->comm);

  ierr = SNESGetConvergenceHistory(snes,&history,PETSC_NULL,&len);CHKERRQ(ierr);
  if (its == 0 || !history || its > len) {
    ierr = PetscViewerASCIIPrintf(viewer,"%3d SNES Function norm %14.12e \n",its,fgnorm);CHKERRQ(ierr);
  } else {
    PetscReal ratio = fgnorm/history[its-1];
    ierr = PetscViewerASCIIPrintf(viewer,"%3d SNES Function norm %14.12e %g \n",its,fgnorm,ratio);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

/*
   If the we set the history monitor space then we must destroy it
*/
#undef __FUNCT__  
#define __FUNCT__ "SNESRatioMonitorDestroy"
int SNESRatioMonitorDestroy(void *history)
{
  int         ierr;

  PetscFunctionBegin;
  ierr = PetscFree(history);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "SNESSetRatioMonitor"
/*@C
   SNESSetRatioMonitor - Sets SNES to use a monitor that prints the 
     ratio of the function norm at each iteration

   Collective on SNES

   Input Parameters:
.   snes - the SNES context

   Level: intermediate

.keywords: SNES, nonlinear, monitor, norm

.seealso: SNESSetMonitor(), SNESVecViewMonitor(), SNESDefaultMonitor()
@*/
int SNESSetRatioMonitor(SNES snes)
{
  int       ierr;
  PetscReal *history;

  PetscFunctionBegin;

  ierr = SNESGetConvergenceHistory(snes,&history,PETSC_NULL,PETSC_NULL);CHKERRQ(ierr);
  if (!history) {
    ierr = PetscMalloc(100*sizeof(double),&history);CHKERRQ(ierr);
    ierr = SNESSetConvergenceHistory(snes,history,0,100,PETSC_TRUE);CHKERRQ(ierr);
    ierr = SNESSetMonitor(snes,SNESRatioMonitor,history,SNESRatioMonitorDestroy);CHKERRQ(ierr);
  } else {
    ierr = SNESSetMonitor(snes,SNESRatioMonitor,0,0);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

/* ---------------------------------------------------------------- */
#undef __FUNCT__  
#define __FUNCT__ "SNESDefaultSMonitor"
/*
     Default (short) SNES Monitor, same as SNESDefaultMonitor() except
  it prints fewer digits of the residual as the residual gets smaller.
  This is because the later digits are meaningless and are often 
  different on different machines; by using this routine different 
  machines will usually generate the same output.
*/
int SNESDefaultSMonitor(SNES snes,int its,PetscReal fgnorm,void *dummy)
{
  int ierr;

  PetscFunctionBegin;
  if (snes->method_class == SNES_NONLINEAR_EQUATIONS) {
    if (fgnorm > 1.e-9) {
      ierr = PetscPrintf(snes->comm,"%3d SNES Function norm %g \n",its,fgnorm);CHKERRQ(ierr);
    } else if (fgnorm > 1.e-11){
      ierr = PetscPrintf(snes->comm,"%3d SNES Function norm %5.3e \n",its,fgnorm);CHKERRQ(ierr);
    } else {
      ierr = PetscPrintf(snes->comm,"%3d SNES Function norm < 1.e-11\n",its);CHKERRQ(ierr);
    }
  } else if (snes->method_class == SNES_UNCONSTRAINED_MINIMIZATION) {
    if (fgnorm > 1.e-9) {
      ierr = PetscPrintf(snes->comm,"%3d SNES Function value %g, Gradient norm %g \n",its,snes->fc,fgnorm);CHKERRQ(ierr);
    } else if (fgnorm > 1.e-11) {
      ierr = PetscPrintf(snes->comm,"%3d SNES Function value %g, Gradient norm %5.3e \n",its,snes->fc,fgnorm);CHKERRQ(ierr);
    } else {
      ierr = PetscPrintf(snes->comm,"%3d SNES Function value %g, Gradient norm < 1.e-11\n",its,snes->fc);CHKERRQ(ierr);
    }
  } else SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"Unknown method class");
  PetscFunctionReturn(0);
}
/* ---------------------------------------------------------------- */
#undef __FUNCT__  
#define __FUNCT__ "SNESConverged_EQ_LS"
/*@C 
   SNESConverged_EQ_LS - Monitors the convergence of the solvers for
   systems of nonlinear equations (default).

   Collective on SNES

   Input Parameters:
+  snes - the SNES context
.  xnorm - 2-norm of current iterate
.  pnorm - 2-norm of current step 
.  fnorm - 2-norm of function
-  dummy - unused context

   Output Parameter:
.   reason  - one of
$  SNES_CONVERGED_FNORM_ABS       - (fnorm < atol),
$  SNES_CONVERGED_PNORM_RELATIVE  - (pnorm == 0.0),
$  SNES_CONVERGED_FNORM_RELATIVE  - (fnorm < rtol*fnorm0),
$  SNES_DIVERGED_FUNCTION_COUNT   - (nfct > maxf),
$  SNES_DIVERGED_FNORM_NAN        - (fnorm == NaN),
$  SNES_CONVERGED_ITERATING       - (otherwise),

   where
+    maxf - maximum number of function evaluations,
            set with SNESSetTolerances()
.    nfct - number of function evaluations,
.    atol - absolute function norm tolerance,
            set with SNESSetTolerances()
-    rtol - relative function norm tolerance, set with SNESSetTolerances()

   Level: intermediate

.keywords: SNES, nonlinear, default, converged, convergence

.seealso: SNESSetConvergenceTest(), SNESEisenstatWalkerConverged()
@*/
int SNESConverged_EQ_LS(SNES snes,PetscReal xnorm,PetscReal pnorm,PetscReal fnorm,SNESConvergedReason *reason,void *dummy)
{
  PetscFunctionBegin;
  if (snes->method_class != SNES_NONLINEAR_EQUATIONS) {
     SETERRQ(PETSC_ERR_ARG_WRONG,"For SNES_NONLINEAR_EQUATIONS only");
  }

  if (fnorm != fnorm) {
    PetscLogInfo(snes,"SNESConverged_EQ_LS:Failed to converged, function norm is NaN\n");
    *reason = SNES_DIVERGED_FNORM_NAN;
  } else if (fnorm <= snes->ttol) {
    PetscLogInfo(snes,"SNESConverged_EQ_LS:Converged due to function norm %g < %g (relative tolerance)\n",fnorm,snes->ttol);
    *reason = SNES_CONVERGED_FNORM_RELATIVE;
  } else if (fnorm < snes->atol) {
    PetscLogInfo(snes,"SNESConverged_EQ_LS:Converged due to function norm %g < %g\n",fnorm,snes->atol);
    *reason = SNES_CONVERGED_FNORM_ABS;
  } else if (pnorm == 0.0) {
    PetscLogInfo(snes,"SNESConverged_EQ_LS:Converged due to small update length: %g < %g * %g\n",pnorm,snes->xtol,xnorm);
    *reason = SNES_CONVERGED_PNORM_RELATIVE;
  } else if (snes->nfuncs > snes->max_funcs) {
    PetscLogInfo(snes,"SNESConverged_EQ_LS:Exceeded maximum number of function evaluations: %d > %d\n",snes->nfuncs,snes->max_funcs);
    *reason = SNES_DIVERGED_FUNCTION_COUNT ;
  } else {
    *reason = SNES_CONVERGED_ITERATING;
  }
  PetscFunctionReturn(0);
}
/* ------------------------------------------------------------ */
#undef __FUNCT__  
#define __FUNCT__ "SNES_KSP_SetConvergenceTestEW"
/*@
   SNES_KSP_SetConvergenceTestEW - Sets alternative convergence test
   for the linear solvers within an inexact Newton method.  

   Collective on SNES

   Input Parameter:
.  snes - SNES context

   Notes:
   Currently, the default is to use a constant relative tolerance for 
   the inner linear solvers.  Alternatively, one can use the 
   Eisenstat-Walker method, where the relative convergence tolerance 
   is reset at each Newton iteration according progress of the nonlinear 
   solver. 

   Level: advanced

   Reference:
   S. C. Eisenstat and H. F. Walker, "Choosing the forcing terms in an 
   inexact Newton method", SISC 17 (1), pp.16-32, 1996.

.keywords: SNES, KSP, Eisenstat, Walker, convergence, test, inexact, Newton
@*/
int SNES_KSP_SetConvergenceTestEW(SNES snes)
{
  PetscFunctionBegin;
  snes->ksp_ewconv = PETSC_TRUE;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "SNES_KSP_SetParametersEW"
/*@
   SNES_KSP_SetParametersEW - Sets parameters for Eisenstat-Walker
   convergence criteria for the linear solvers within an inexact
   Newton method.

   Collective on SNES
 
   Input Parameters:
+    snes - SNES context
.    version - version 1 or 2 (default is 2)
.    rtol_0 - initial relative tolerance (0 <= rtol_0 < 1)
.    rtol_max - maximum relative tolerance (0 <= rtol_max < 1)
.    alpha - power for version 2 rtol computation (1 < alpha <= 2)
.    alpha2 - power for safeguard
.    gamma2 - multiplicative factor for version 2 rtol computation
              (0 <= gamma2 <= 1)
-    threshold - threshold for imposing safeguard (0 < threshold < 1)

   Note:
   Use PETSC_DEFAULT to retain the default for any of the parameters.

   Level: advanced

   Reference:
   S. C. Eisenstat and H. F. Walker, "Choosing the forcing terms in an 
   inexact Newton method", Utah State University Math. Stat. Dept. Res. 
   Report 6/94/75, June, 1994, to appear in SIAM J. Sci. Comput. 

.keywords: SNES, KSP, Eisenstat, Walker, set, parameters

.seealso: SNES_KSP_SetConvergenceTestEW()
@*/
int SNES_KSP_SetParametersEW(SNES snes,int version,PetscReal rtol_0,
                             PetscReal rtol_max,PetscReal gamma2,PetscReal alpha,
                             PetscReal alpha2,PetscReal threshold)
{
  SNES_KSP_EW_ConvCtx *kctx = (SNES_KSP_EW_ConvCtx*)snes->kspconvctx;

  PetscFunctionBegin;
  if (!kctx) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"No Eisenstat-Walker context existing");
  if (version != PETSC_DEFAULT)   kctx->version   = version;
  if (rtol_0 != PETSC_DEFAULT)    kctx->rtol_0    = rtol_0;
  if (rtol_max != PETSC_DEFAULT)  kctx->rtol_max  = rtol_max;
  if (gamma2 != PETSC_DEFAULT)    kctx->gamma     = gamma2;
  if (alpha != PETSC_DEFAULT)     kctx->alpha     = alpha;
  if (alpha2 != PETSC_DEFAULT)    kctx->alpha2    = alpha2;
  if (threshold != PETSC_DEFAULT) kctx->threshold = threshold;
  if (kctx->rtol_0 < 0.0 || kctx->rtol_0 >= 1.0) {
    SETERRQ1(PETSC_ERR_ARG_OUTOFRANGE,"0.0 <= rtol_0 < 1.0: %g",kctx->rtol_0);
  }
  if (kctx->rtol_max < 0.0 || kctx->rtol_max >= 1.0) {
    SETERRQ1(PETSC_ERR_ARG_OUTOFRANGE,"0.0 <= rtol_max (%g) < 1.0\n",kctx->rtol_max);
  }
  if (kctx->threshold <= 0.0 || kctx->threshold >= 1.0) {
    SETERRQ1(PETSC_ERR_ARG_OUTOFRANGE,"0.0 < threshold (%g) < 1.0\n",kctx->threshold);
  }
  if (kctx->gamma < 0.0 || kctx->gamma > 1.0) {
    SETERRQ1(PETSC_ERR_ARG_OUTOFRANGE,"0.0 <= gamma (%g) <= 1.0\n",kctx->gamma);
  }
  if (kctx->alpha <= 1.0 || kctx->alpha > 2.0) {
    SETERRQ1(PETSC_ERR_ARG_OUTOFRANGE,"1.0 < alpha (%g) <= 2.0\n",kctx->alpha);
  }
  if (kctx->version != 1 && kctx->version !=2) {
    SETERRQ1(PETSC_ERR_ARG_OUTOFRANGE,"Only versions 1 and 2 are supported: %d",kctx->version);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "SNES_KSP_EW_ComputeRelativeTolerance_Private"
int SNES_KSP_EW_ComputeRelativeTolerance_Private(SNES snes,KSP ksp)
{
  SNES_KSP_EW_ConvCtx *kctx = (SNES_KSP_EW_ConvCtx*)snes->kspconvctx;
  PetscReal           rtol = 0.0,stol;
  int                 ierr;

  PetscFunctionBegin;
  if (!kctx) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"No Eisenstat-Walker context exists");
  if (!snes->iter) { /* first time in, so use the original user rtol */
    rtol = kctx->rtol_0;
  } else {
    if (kctx->version == 1) {
      rtol = (snes->norm - kctx->lresid_last)/kctx->norm_last; 
      if (rtol < 0.0) rtol = -rtol;
      stol = pow(kctx->rtol_last,kctx->alpha2);
      if (stol > kctx->threshold) rtol = PetscMax(rtol,stol);
    } else if (kctx->version == 2) {
      rtol = kctx->gamma * pow(snes->norm/kctx->norm_last,kctx->alpha);
      stol = kctx->gamma * pow(kctx->rtol_last,kctx->alpha);
      if (stol > kctx->threshold) rtol = PetscMax(rtol,stol);
    } else SETERRQ1(PETSC_ERR_ARG_OUTOFRANGE,"Only versions 1 or 2 are supported: %d",kctx->version);
  }
  rtol = PetscMin(rtol,kctx->rtol_max);
  kctx->rtol_last = rtol;
  PetscLogInfo(snes,"SNES_KSP_EW_ComputeRelativeTolerance_Private: iter %d, Eisenstat-Walker (version %d) KSP rtol = %g\n",snes->iter,kctx->version,rtol);
  ierr = KSPSetTolerances(ksp,rtol,PETSC_DEFAULT,PETSC_DEFAULT,PETSC_DEFAULT);CHKERRQ(ierr);
  kctx->norm_last = snes->norm;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "SNES_KSP_EW_Converged_Private"
int SNES_KSP_EW_Converged_Private(KSP ksp,int n,PetscReal rnorm,KSPConvergedReason *reason,void *ctx)
{
  SNES                snes = (SNES)ctx;
  SNES_KSP_EW_ConvCtx *kctx = (SNES_KSP_EW_ConvCtx*)snes->kspconvctx;
  int                 ierr;

  PetscFunctionBegin;
  if (!kctx) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"No Eisenstat-Walker context set");
  if (n == 0) {ierr = SNES_KSP_EW_ComputeRelativeTolerance_Private(snes,ksp);CHKERRQ(ierr);}
  ierr = KSPDefaultConverged(ksp,n,rnorm,reason,ctx);CHKERRQ(ierr);
  kctx->lresid_last = rnorm;
  if (*reason) {
    PetscLogInfo(snes,"SNES_KSP_EW_Converged_Private: KSP iterations=%d, rnorm=%g\n",n,rnorm);
  }
  PetscFunctionReturn(0);
}


