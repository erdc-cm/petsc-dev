/*$Id: cr.c,v 1.64 2001/08/07 03:03:49 balay Exp $*/

/*                       
           This implements Preconditioned Conjugate Residuals.       
*/
#include "src/ksp/ksp/kspimpl.h"

#undef __FUNCT__  
#define __FUNCT__ "KSPSetUp_CR"
static int KSPSetUp_CR(KSP ksp)
{
  int ierr;

  PetscFunctionBegin;
  if (ksp->pc_side == PC_RIGHT) {SETERRQ(2,"no right preconditioning for KSPCR");}
  else if (ksp->pc_side == PC_SYMMETRIC) {SETERRQ(2,"no symmetric preconditioning for KSPCR");}
  ierr = KSPDefaultGetWork(ksp,6);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "KSPSolve_CR"
static int  KSPSolve_CR(KSP ksp)
{
  int          i = 0, ierr;
  MatStructure pflag;
  PetscReal    dp;
  PetscScalar  ai, bi;
  PetscScalar  apq,btop, bbot, mone = -1.0;
  Vec          X,B,R,RT,P,AP,ART,Q;
  Mat          Amat, Pmat;

  PetscFunctionBegin;

  X       = ksp->vec_sol;
  B       = ksp->vec_rhs;
  R       = ksp->work[0];
  RT      = ksp->work[1];
  P       = ksp->work[2];
  AP      = ksp->work[3];
  ART     = ksp->work[4];
  Q       = ksp->work[5];

  /* R is the true residual norm, RT is the preconditioned residual norm */
  ierr = PCGetOperators(ksp->B,&Amat,&Pmat,&pflag);CHKERRQ(ierr);
  if (!ksp->guess_zero) {
    ierr = KSP_MatMult(ksp,Amat,X,R);CHKERRQ(ierr);     /*   R <- A*X           */
    ierr = VecAYPX(&mone,B,R);CHKERRQ(ierr);            /*   R <- B-R == B-A*X  */
  } else { 
    ierr = VecCopy(B,R);CHKERRQ(ierr);                  /*   R <- B (X is 0)    */
  }
  ierr = KSP_PCApply(ksp,ksp->B,R,P);CHKERRQ(ierr);     /*   P   <- B*R         */
  ierr = KSP_MatMult(ksp,Amat,P,AP);CHKERRQ(ierr);      /*   AP  <- A*P         */
  ierr = VecCopy(P,RT);CHKERRQ(ierr);                   /*   RT  <- P           */
  ierr = VecCopy(AP,ART);CHKERRQ(ierr);                 /*   ART <- AP          */
  ierr   = VecDot(RT,ART,&btop);CHKERRQ(ierr);          /*   (RT,ART)           */
  if (ksp->normtype == KSP_PRECONDITIONED_NORM) {
    ierr = VecNorm(RT,NORM_2,&dp);CHKERRQ(ierr);        /*   dp <- RT'*RT       */
  } else if (ksp->normtype == KSP_UNPRECONDITIONED_NORM) {
    ierr = VecNorm(R,NORM_2,&dp);CHKERRQ(ierr);         /*   dp <- R'*R         */
  } else if (ksp->normtype == KSP_NATURAL_NORM) {
    dp = sqrt(PetscAbsScalar(btop));                    /* dp = sqrt(R,AR)      */
  }
  ksp->its = 0;
  KSPMonitor(ksp,0,dp);
  ierr = PetscObjectTakeAccess(ksp);CHKERRQ(ierr);
  ksp->rnorm              = dp;
  ierr = PetscObjectGrantAccess(ksp);CHKERRQ(ierr);
  KSPLogResidualHistory(ksp,dp);
  ierr = (*ksp->converged)(ksp,0,dp,&ksp->reason,ksp->cnvP);CHKERRQ(ierr);
  if (ksp->reason) PetscFunctionReturn(0);

  i = 0;
  do {
    ierr   = KSP_PCApply(ksp,ksp->B,AP,Q);CHKERRQ(ierr);/*   Q <- B* AP          */
                                                        /* Step 3                */

    ierr   = VecDot(AP,Q,&apq);CHKERRQ(ierr);  
    ai = btop/apq;                                      /* ai = (RT,ART)/(AP,Q)  */

    ierr   = VecAXPY(&ai,P,X);CHKERRQ(ierr);            /*   X   <- X + ai*P     */
    ai     = -ai; 
    ierr   = VecAXPY(&ai,Q,RT);CHKERRQ(ierr);           /*   RT  <- RT - ai*Q    */
    ierr   = KSP_MatMult(ksp,Amat,RT,ART);CHKERRQ(ierr);/*   ART <-   A*RT       */
    bbot = btop;
    ierr   = VecDot(RT,ART,&btop);CHKERRQ(ierr);

    if (ksp->normtype == KSP_PRECONDITIONED_NORM) {
      ierr = VecNorm(RT,NORM_2,&dp);CHKERRQ(ierr);      /*   dp <- || RT ||      */
    } else if (ksp->normtype == KSP_NATURAL_NORM) {
      dp = sqrt(PetscAbsScalar(btop));                  /* dp = sqrt(R,AR)       */
    } else if (ksp->normtype == KSP_NO_NORM) {
      dp = 0.0; 
    } else if (ksp->normtype == KSP_UNPRECONDITIONED_NORM) {
      ierr = VecAXPY(&ai,AP,R);CHKERRQ(ierr);           /*   R   <- R - ai*AP    */
      ierr = VecNorm(R,NORM_2,&dp);CHKERRQ(ierr);       /*   dp <- R'*R          */
    } else {
      SETERRQ1(1,"KSPNormType of %d not supported",(int)ksp->normtype);
    }

    ierr = PetscObjectTakeAccess(ksp);CHKERRQ(ierr);
    ksp->its++;
    ksp->rnorm = dp;
    ierr = PetscObjectGrantAccess(ksp);CHKERRQ(ierr);

    KSPLogResidualHistory(ksp,dp);
    KSPMonitor(ksp,i+1,dp);
    ierr = (*ksp->converged)(ksp,i+1,dp,&ksp->reason,ksp->cnvP);CHKERRQ(ierr);
    if (ksp->reason) break;

    bi = btop/bbot;
    ierr = VecAYPX(&bi,RT,P);CHKERRQ(ierr);              /*   P <- RT + Bi P     */
    ierr = VecAYPX(&bi,ART,AP);CHKERRQ(ierr);            /*   AP <- ART + Bi AP  */
    i++;
  } while (i<ksp->max_it);
  if (i == ksp->max_it) {
    ksp->reason =  KSP_DIVERGED_ITS;
  }
  PetscFunctionReturn(0);
}


EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "KSPCreate_CR"
int KSPCreate_CR(KSP ksp)
{
  PetscFunctionBegin;
  ksp->pc_side                   = PC_LEFT;
  ksp->ops->setup                = KSPSetUp_CR;
  ksp->ops->solve                = KSPSolve_CR;
  ksp->ops->destroy              = KSPDefaultDestroy;
  ksp->ops->buildsolution        = KSPDefaultBuildSolution;
  ksp->ops->buildresidual        = KSPDefaultBuildResidual;
  ksp->ops->setfromoptions       = 0;
  ksp->ops->view                 = 0;
  PetscFunctionReturn(0);
}
EXTERN_C_END
