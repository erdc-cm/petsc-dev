/*
    This is a first attempt at a Chebychev routine, it is not 
    necessarily well optimized.
*/
#include "src/ksp/ksp/kspimpl.h"                    /*I "petscksp.h" I*/
#include "src/ksp/ksp/impls/cheby/chebctx.h"

#undef __FUNCT__  
#define __FUNCT__ "KSPSetUp_Chebychev"
int KSPSetUp_Chebychev(KSP ksp)
{
  int ierr;

  PetscFunctionBegin;
  if (ksp->pc_side == PC_SYMMETRIC) SETERRQ(2,"no symmetric preconditioning for KSPCHEBYCHEV");
  ierr = KSPDefaultGetWork(ksp,3);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "KSPChebychevSetEigenvalues_Chebychev"
int KSPChebychevSetEigenvalues_Chebychev(KSP ksp,PetscReal emax,PetscReal emin)
{
  KSP_Chebychev *chebychevP = (KSP_Chebychev*)ksp->data;

  PetscFunctionBegin;
  if (emax <= emin) SETERRQ2(1,"Maximum eigenvalue must be larger than minimum: max %g min %g",emax,emin);
  if (emax*emin <= 0.0) SETERRQ2(1,"Both eigenvalues must be of the same sign: max %g min %g",emax,emin);
  chebychevP->emax = emax;
  chebychevP->emin = emin;
  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "KSPChebychevSetEigenvalues"
/*@
   KSPChebychevSetEigenvalues - Sets estimates for the extreme eigenvalues
   of the preconditioned problem.

   Collective on KSP

   Input Parameters:
+  ksp - the Krylov space context
-  emax, emin - the eigenvalue estimates

  Options Database:
.  -ksp_chebychev_eigenvalues emin,emax

   Level: intermediate

.keywords: KSP, Chebyshev, set, eigenvalues
@*/
int KSPChebychevSetEigenvalues(KSP ksp,PetscReal emax,PetscReal emin)
{
  int ierr,(*f)(KSP,PetscReal,PetscReal);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ksp,KSP_COOKIE,1);
  ierr = PetscObjectQueryFunction((PetscObject)ksp,"KSPChebychevSetEigenvalues_C",(void (**)(void))&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(ksp,emax,emin);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "KSPSetFromOptions_Chebychev"
int KSPSetFromOptions_Chebychev(KSP ksp)
{
  KSP_Chebychev *cheb = (KSP_Chebychev*)ksp->data;
  int            ierr;
  int            two = 2;

  PetscFunctionBegin;
  ierr = PetscOptionsHead("KSP Chebychev Options");CHKERRQ(ierr);
    ierr = PetscOptionsRealArray("-ksp_chebychev_eigenvalues","extreme eigenvalues","KSPChebychevSetEigenvalues",&cheb->emin,&two,0);CHKERRQ(ierr);
  ierr = PetscOptionsTail();CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "KSPSolve_Chebychev"
int KSPSolve_Chebychev(KSP ksp)
{
  int              k,kp1,km1,maxit,ktmp,i,ierr;
  PetscScalar      alpha,omegaprod,mu,omega,Gamma,c[3],scale,mone = -1.0,tmp;
  PetscReal        rnorm;
  Vec              x,b,p[3],r;
  KSP_Chebychev    *chebychevP = (KSP_Chebychev*)ksp->data;
  Mat              Amat,Pmat;
  MatStructure     pflag;
  PetscTruth       diagonalscale;

  PetscFunctionBegin;
  ierr    = PCDiagonalScale(ksp->pc,&diagonalscale);CHKERRQ(ierr);
  if (diagonalscale) SETERRQ1(1,"Krylov method %s does not support diagonal scaling",ksp->type_name);

  ksp->its = 0;
  ierr     = PCGetOperators(ksp->pc,&Amat,&Pmat,&pflag);CHKERRQ(ierr);
  maxit    = ksp->max_it;

  /* These three point to the three active solutions, we
     rotate these three at each solution update */
  km1    = 0; k = 1; kp1 = 2;
  x      = ksp->vec_sol;
  b      = ksp->vec_rhs;
  p[km1] = x;
  p[k]   = ksp->work[0];
  p[kp1] = ksp->work[1];
  r      = ksp->work[2];

  /* use scale*B as our preconditioner */
  scale  = 2.0/(chebychevP->emax + chebychevP->emin);

  /*   -alpha <=  scale*lambda(B^{-1}A) <= alpha   */
  alpha  = 1.0 - scale*(chebychevP->emin); ;
  Gamma  = 1.0;
  mu     = 1.0/alpha; 
  omegaprod = 2.0/alpha;

  c[km1] = 1.0;
  c[k]   = mu;

  if (!ksp->guess_zero) {
    ierr = KSP_MatMult(ksp,Amat,x,r);CHKERRQ(ierr);     /*  r = b - Ax     */
    ierr = VecAYPX(&mone,b,r);CHKERRQ(ierr);
  } else {
    ierr = VecCopy(b,r);CHKERRQ(ierr);
  }
                  
  ierr = KSP_PCApply(ksp,r,p[k]);CHKERRQ(ierr);  /* p[k] = scale B^{-1}r + x */
  ierr = VecAYPX(&scale,x,p[k]);CHKERRQ(ierr);

  for (i=0; i<maxit; i++) {
    ierr = PetscObjectTakeAccess(ksp);CHKERRQ(ierr);
    ksp->its++;
    ierr = PetscObjectGrantAccess(ksp);CHKERRQ(ierr);
    c[kp1] = 2.0*mu*c[k] - c[km1];
    omega = omegaprod*c[k]/c[kp1];

    ierr = KSP_MatMult(ksp,Amat,p[k],r);CHKERRQ(ierr);                 /*  r = b - Ap[k]    */
    ierr = VecAYPX(&mone,b,r);CHKERRQ(ierr);                       
    ierr = KSP_PCApply(ksp,r,p[kp1]);CHKERRQ(ierr);             /*  p[kp1] = B^{-1}z  */

    /* calculate residual norm if requested */
    if (ksp->normtype != KSP_NO_NORM) {
      if (ksp->normtype == KSP_UNPRECONDITIONED_NORM) {ierr = VecNorm(r,NORM_2,&rnorm);CHKERRQ(ierr);}
      else {ierr = VecNorm(p[kp1],NORM_2,&rnorm);CHKERRQ(ierr);}
      ierr = PetscObjectTakeAccess(ksp);CHKERRQ(ierr);
      ksp->rnorm                              = rnorm;
      ierr = PetscObjectGrantAccess(ksp);CHKERRQ(ierr);
      ksp->vec_sol = p[k]; 
      KSPLogResidualHistory(ksp,rnorm);
      KSPMonitor(ksp,i,rnorm);
      ierr = (*ksp->converged)(ksp,i,rnorm,&ksp->reason,ksp->cnvP);CHKERRQ(ierr);
      if (ksp->reason) break;
    }

    /* y^{k+1} = omega(y^{k} - y^{k-1} + Gamma*r^{k}) + y^{k-1} */
    tmp  = omega*Gamma*scale;
    ierr = VecScale(&tmp,p[kp1]);CHKERRQ(ierr);
    tmp  = 1.0-omega; VecAXPY(&tmp,p[km1],p[kp1]);
    ierr = VecAXPY(&omega,p[k],p[kp1]);CHKERRQ(ierr);

    ktmp = km1;
    km1  = k;
    k    = kp1;
    kp1  = ktmp;
  }
  if (!ksp->reason && ksp->normtype != KSP_NO_NORM) {
    ksp->reason = KSP_DIVERGED_ITS;
    ierr = KSP_MatMult(ksp,Amat,p[k],r);CHKERRQ(ierr);       /*  r = b - Ap[k]    */
    ierr = VecAYPX(&mone,b,r);CHKERRQ(ierr);
    if (ksp->normtype == KSP_UNPRECONDITIONED_NORM) {ierr = VecNorm(r,NORM_2,&rnorm);CHKERRQ(ierr);}
    else {
      ierr = KSP_PCApply(ksp,r,p[kp1]);CHKERRQ(ierr); /* p[kp1] = B^{-1}z */
      ierr = VecNorm(p[kp1],NORM_2,&rnorm);CHKERRQ(ierr);
    }
    ierr = PetscObjectTakeAccess(ksp);CHKERRQ(ierr);
    ksp->rnorm                              = rnorm;
    ierr = PetscObjectGrantAccess(ksp);CHKERRQ(ierr);
    ksp->vec_sol = p[k]; 
    KSPLogResidualHistory(ksp,rnorm);
    KSPMonitor(ksp,i,rnorm);
  }

  /* make sure solution is in vector x */
  ksp->vec_sol = x;
  if (k) {
    ierr = VecCopy(p[k],x);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "KSPView_Chebychev" 
int KSPView_Chebychev(KSP ksp,PetscViewer viewer)
{
  KSP_Chebychev *cheb = (KSP_Chebychev*)ksp->data;
  int           ierr;
  PetscTruth    isascii;

  PetscFunctionBegin;
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_ASCII,&isascii);CHKERRQ(ierr);
  if (isascii) {
    ierr = PetscViewerASCIIPrintf(viewer,"  Chebychev: eigenvalue estimates:  min = %g, max = %g\n",cheb->emin,cheb->emax);CHKERRQ(ierr);
  } else {
    SETERRQ1(1,"Viewer type %s not supported for KSP Chebychev",((PetscObject)viewer)->type_name);
  }
  PetscFunctionReturn(0);
}

/*MC
     KSPCHEBYCHEV - The preconditioned Chebychev iterative method

   Options Database Keys:
.   -ksp_chebychev_eigenvalues <emin,emax> - set approximations to the smallest and largest eigenvalues
                  of the preconditioned operator. If these are accurate you will get much faster convergence.

   Level: beginner

   Notes: The Chebychev method requires both the matrix and preconditioner to 
          be symmetric positive (semi) definite

.seealso:  KSPCreate(), KSPSetType(), KSPType (for list of available types), KSP,
           KSPChebychevSetEigenvalues()

M*/

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "KSPCreate_Chebychev"
int KSPCreate_Chebychev(KSP ksp)
{
  int           ierr;
  KSP_Chebychev *chebychevP;

  PetscFunctionBegin;
  ierr = PetscNew(KSP_Chebychev,&chebychevP);CHKERRQ(ierr);
  PetscLogObjectMemory(ksp,sizeof(KSP_Chebychev));

  ksp->data                      = (void*)chebychevP;
  ksp->pc_side                   = PC_LEFT;

  chebychevP->emin               = 1.e-2;
  chebychevP->emax               = 1.e+2;

  ksp->ops->setup                = KSPSetUp_Chebychev;
  ksp->ops->solve                = KSPSolve_Chebychev;
  ksp->ops->destroy              = KSPDefaultDestroy;
  ksp->ops->buildsolution        = KSPDefaultBuildSolution;
  ksp->ops->buildresidual        = KSPDefaultBuildResidual;
  ksp->ops->setfromoptions       = KSPSetFromOptions_Chebychev;
  ksp->ops->view                 = KSPView_Chebychev;

  ierr = PetscObjectComposeFunctionDynamic((PetscObject)ksp,"KSPChebychevSetEigenvalues_C",
                                    "KSPChebychevSetEigenvalues_Chebychev",
                                    KSPChebychevSetEigenvalues_Chebychev);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END
