#define PETSCSNES_DLL

#include "../src/snes/impls/lsvi/lsviimpl.h"

/*
     Checks if J^T F = 0 which implies we've found a local minimum of the norm of the function,
    || F(u) ||_2 but not a zero, F(u) = 0. In the case when one cannot compute J^T F we use the fact that
    0 = (J^T F)^T W = F^T J W iff W not in the null space of J. Thanks for Jorge More 
    for this trick. One assumes that the probability that W is in the null space of J is very, very small.
*/ 
#undef __FUNCT__  
#define __FUNCT__ "SNESLSVICheckLocalMin_Private"
PetscErrorCode SNESLSVICheckLocalMin_Private(SNES snes,Mat A,Vec F,Vec W,PetscReal fnorm,PetscTruth *ismin)
{
  PetscReal      a1;
  PetscErrorCode ierr;
  PetscTruth     hastranspose;

  PetscFunctionBegin;
  *ismin = PETSC_FALSE;
  ierr = MatHasOperation(A,MATOP_MULT_TRANSPOSE,&hastranspose);CHKERRQ(ierr);
  if (hastranspose) {
    /* Compute || J^T F|| */
    ierr = MatMultTranspose(A,F,W);CHKERRQ(ierr);
    ierr = VecNorm(W,NORM_2,&a1);CHKERRQ(ierr);
    ierr = PetscInfo1(snes,"|| J^T F|| %G near zero implies found a local minimum\n",a1/fnorm);CHKERRQ(ierr);
    if (a1/fnorm < 1.e-4) *ismin = PETSC_TRUE;
  } else {
    Vec         work;
    PetscScalar result;
    PetscReal   wnorm;

    ierr = VecSetRandom(W,PETSC_NULL);CHKERRQ(ierr);
    ierr = VecNorm(W,NORM_2,&wnorm);CHKERRQ(ierr);
    ierr = VecDuplicate(W,&work);CHKERRQ(ierr);
    ierr = MatMult(A,W,work);CHKERRQ(ierr);
    ierr = VecDot(F,work,&result);CHKERRQ(ierr);
    ierr = VecDestroy(work);CHKERRQ(ierr);
    a1   = PetscAbsScalar(result)/(fnorm*wnorm);
    ierr = PetscInfo1(snes,"(F^T J random)/(|| F ||*||random|| %G near zero implies found a local minimum\n",a1);CHKERRQ(ierr);
    if (a1 < 1.e-4) *ismin = PETSC_TRUE;
  }
  PetscFunctionReturn(0);
}

/*
     Checks if J^T(F - J*X) = 0 
*/ 
#undef __FUNCT__  
#define __FUNCT__ "SNESLSVICheckResidual_Private"
PetscErrorCode SNESLSVICheckResidual_Private(SNES snes,Mat A,Vec F,Vec X,Vec W1,Vec W2)
{
  PetscReal      a1,a2;
  PetscErrorCode ierr;
  PetscTruth     hastranspose;

  PetscFunctionBegin;
  ierr = MatHasOperation(A,MATOP_MULT_TRANSPOSE,&hastranspose);CHKERRQ(ierr);
  if (hastranspose) {
    ierr = MatMult(A,X,W1);CHKERRQ(ierr);
    ierr = VecAXPY(W1,-1.0,F);CHKERRQ(ierr);

    /* Compute || J^T W|| */
    ierr = MatMultTranspose(A,W1,W2);CHKERRQ(ierr);
    ierr = VecNorm(W1,NORM_2,&a1);CHKERRQ(ierr);
    ierr = VecNorm(W2,NORM_2,&a2);CHKERRQ(ierr);
    if (a1 != 0.0) {
      ierr = PetscInfo1(snes,"||J^T(F-Ax)||/||F-AX|| %G near zero implies inconsistent rhs\n",a2/a1);CHKERRQ(ierr);
    }
  }
  PetscFunctionReturn(0);
}

/*
  SNESDefaultConverged_LSVI - Checks the convergence of the semismooth newton algorithm.

  Notes:
  The convergence criterion currently implemented is
  merit < abstol
  merit < rtol*merit_initial
*/
PetscErrorCode SNESDefaultConverged_LSVI(SNES snes,PetscInt it,PetscReal xnorm,PetscReal gradnorm,PetscReal merit,SNESConvergedReason *reason,void *dummy)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(snes,SNES_CLASSID,1);
  PetscValidPointer(reason,6);
  
  *reason = SNES_CONVERGED_ITERATING;

  if (!it) {
    /* set parameter for default relative tolerance convergence test */
    snes->ttol = merit*snes->rtol;
  }
  if (merit != merit) {
    ierr = PetscInfo(snes,"Failed to converged, function norm is NaN\n");CHKERRQ(ierr);
    *reason = SNES_DIVERGED_FNORM_NAN;
  } else if (merit < snes->abstol) {
    ierr = PetscInfo2(snes,"Converged due to merit function %G < %G\n",merit,snes->abstol);CHKERRQ(ierr);
    *reason = SNES_CONVERGED_FNORM_ABS;
  } else if (snes->nfuncs >= snes->max_funcs) {
    ierr = PetscInfo2(snes,"Exceeded maximum number of function evaluations: %D > %D\n",snes->nfuncs,snes->max_funcs);CHKERRQ(ierr);
    *reason = SNES_DIVERGED_FUNCTION_COUNT;
  }

  if (it && !*reason) {
    if (merit < snes->ttol) {
      ierr = PetscInfo2(snes,"Converged due to merit function %G < %G (relative tolerance)\n",merit,snes->ttol);CHKERRQ(ierr);
      *reason = SNES_CONVERGED_FNORM_RELATIVE;
    }
  } 
  PetscFunctionReturn(0);
}

/*
  SNESLSVIComputeMeritFunction - Evaluates the merit function for the mixed complementarity problem.

  Input Parameter:
. phi - the semismooth function

  Output Parameter:
. merit - the merit function
. phinorm - ||phi||

  Notes:
  The merit function for the mixed complementarity problem is defined as
     merit = 0.5*phi^T*phi
*/
#undef __FUNCT__
#define __FUNCT__ "SNESLSVIComputeMeritFunction"
static PetscErrorCode SNESLSVIComputeMeritFunction(Vec phi, PetscReal* merit,PetscReal* phinorm)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = VecNormBegin(phi,NORM_2,phinorm);
  ierr = VecNormEnd(phi,NORM_2,phinorm);

  *merit = 0.5*(*phinorm)*(*phinorm);
  PetscFunctionReturn(0);
}

/*
  ComputeFischerFunction - Computes the semismooth fischer burmeister function for a mixed complementarity equation.

  Notes:
  The Fischer-Burmeister function is defined as
       ff(a,b) := sqrt(a*a + b*b) - a - b                                                            
  and is used reformulate a complementarity equation  as a semismooth equation.    
*/

#undef __FUNCT__
#define __FUNCT__ "ComputeFischerFunction"
static PetscErrorCode ComputeFischerFunction(PetscScalar a, PetscScalar b, PetscScalar* ff)
{
  PetscFunctionBegin;
  *ff = sqrt(a*a + b*b) - a - b;
  PetscFunctionReturn(0);
}
  
/* 
   SNESLSVIComputeSSFunction - Reformulates a system of nonlinear equations in mixed complementarity form to a system of nonlinear equations in semismooth form. 

   Input Parameters:  
.  snes - the SNES context
.  x - current iterate

   Output Parameters:
.  phi - Semismooth function

   Notes:
   The result of this function is done by cases:
+  l[i] == -infinity, u[i] == infinity  -- phi[i] = -f[i]
.  l[i] == -infinity, u[i] finite       -- phi[i] = ss(u[i]-x[i], -f[i])                             
.  l[i] finite,       u[i] == infinity  -- phi[i] = ss(x[i]-l[i],  f[i])                              
.  l[i] finite < u[i] finite -- phi[i] = phi(x[i]-l[i], ss(u[i]-x[i], -f[u])) 
-  otherwise l[i] == u[i] -- phi[i] = l[i] - x[i]
   ss is the semismoothing function used to reformulate the nonlinear equation in complementarity
   form to semismooth form

*/
#undef __FUNCT__
#define __FUNCT__ "SNESLSVIComputeSSFunction"
static PetscErrorCode SNESLSVIComputeSSFunction(SNES snes,Vec X,Vec phi)
{
  PetscErrorCode  ierr;
  SNES_LSVI       *lsvi = (SNES_LSVI*)snes->data;
  Vec             Xl = lsvi->xl,Xu = lsvi->xu,F = snes->vec_func;
  PetscScalar     *phi_arr,*x_arr,*f_arr,*l,*u,t;
  PetscInt        i,n;

  PetscFunctionBegin;
  ierr = SNESComputeFunction(snes,X,F);CHKERRQ(ierr);
  ierr = VecGetLocalSize(X,&n);CHKERRQ(ierr);
  
  ierr = VecGetArray(X,&x_arr);CHKERRQ(ierr);
  ierr = VecGetArray(F,&f_arr);CHKERRQ(ierr);
  ierr = VecGetArray(Xl,&l);CHKERRQ(ierr);
  ierr = VecGetArray(Xu,&u);CHKERRQ(ierr);
  ierr = VecGetArray(phi,&phi_arr);CHKERRQ(ierr);

  /* Here we are assuming that the distribution of all the input vectors and the ouput
     vector is same. 
  */  
  for (i=0;i < n;i++) {
    if ((l[i] <= PETSC_LSVI_NINF) && (u[i] >= PETSC_LSVI_INF)) {
      phi_arr[i] = -f_arr[i];
    }
    else if (l[i] <= PETSC_LSVI_NINF) {
      t = u[i] - x_arr[i];
      ierr = ComputeFischerFunction(t,-f_arr[i],&phi_arr[i]);CHKERRQ(ierr);
      phi_arr[i] = -phi_arr[i];
    }
    else if (u[i] >= PETSC_LSVI_INF) {
      t = x_arr[i] - l[i];
      ierr = ComputeFischerFunction(t,f_arr[i],&phi_arr[i]);CHKERRQ(ierr);
    }
    else if (l[i] == u[i]) {
      phi_arr[i] = l[i] - x_arr[i];
    }
    else {
      t = u[i] - x_arr[i];
      ierr = ComputeFischerFunction(t,-f_arr[i],&phi_arr[i]);
      t = x_arr[i] - l[i];
      ierr = ComputeFischerFunction(t,phi_arr[i],&phi_arr[i]);
    }
  }
  
  ierr = VecRestoreArray(X,&x_arr);CHKERRQ(ierr);
  ierr = VecRestoreArray(F,&f_arr);CHKERRQ(ierr);
  ierr = VecRestoreArray(Xl,&l);CHKERRQ(ierr);
  ierr = VecRestoreArray(Xu,&u);CHKERRQ(ierr);
  ierr = VecRestoreArray(phi,&phi_arr);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}

/*
   SNESLSVIComputeBsubdifferential - Calculates an element of the B-subdifferential of the
   Fischer-Burmeister function for complementarity problems.

   Input Parameters:
.  snes     - the SNES context
.  X        - the current iterate
.  vec_func - nonlinear function evaluated at x

   Output Parameters:
.  jac      - B-subdifferential matrix
.  jac_pre  - optional preconditioning matrix
.  flag     - flag passed on by SNESComputeJacobian.

   Notes:
   The B subdifferential matrix is given by
   H = Da + Db*jac
   where Db is the row scaling matrix stored as a vector
   and   Da is the diagonal perturbation matrix stored as a vector
*/
#undef __FUNCT__
#define __FUNCT__ "SNESLSVIComputeBsubdifferential"
PetscErrorCode SNESLSVIComputeBsubdifferential(SNES snes,Vec X,Vec vec_func,Mat jac, Mat jac_pre, MatStructure *flg)
{
  PetscErrorCode ierr;
  SNES_LSVI      *lsvi = (SNES_LSVI*)snes->data;
  PetscScalar    *l,*u,*x,*f,*da,*db;
  PetscInt       n = X->map->n,i;

  PetscFunctionBegin;
  ierr = VecGetArray(X,&x);CHKERRQ(ierr);
  ierr = VecGetArray(vec_func,&f);CHKERRQ(ierr);
  ierr = VecGetArray(lsvi->xl,&l);CHKERRQ(ierr);
  ierr = VecGetArray(lsvi->xu,&u);CHKERRQ(ierr);
  ierr = VecGetArray(lsvi->Da,&da);CHKERRQ(ierr);
  ierr = VecGetArray(lsvi->Db,&db);CHKERRQ(ierr);
  
  /* Compute the elements of the diagonal perturbation vector Da and row scaling vector Db */
  for(i=0;i< n;i++) {
    /* Free variables */
    if ((l[i] <= PETSC_LSVI_NINF) && (u[i] >= PETSC_LSVI_INF)) {
      da[i] = 0; db[i] = -1;
    }
  }

  ierr = VecRestoreArray(X,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(vec_func,&f);CHKERRQ(ierr);
  ierr = VecRestoreArray(lsvi->xl,&l);CHKERRQ(ierr);
  ierr = VecRestoreArray(lsvi->xu,&u);CHKERRQ(ierr);
  ierr = VecRestoreArray(lsvi->Da,&da);CHKERRQ(ierr);
  ierr = VecRestoreArray(lsvi->Db,&db);CHKERRQ(ierr);

  /* Do row scaling */
  ierr = MatDiagonalScale(jac,lsvi->Db,PETSC_NULL);
  ierr = MatDiagonalScale(jac_pre,lsvi->Db,PETSC_NULL);

  /* Add diagonal perturbation */
  ierr = MatDiagonalSet(jac,lsvi->Da,ADD_VALUES);CHKERRQ(ierr);
  ierr = MatDiagonalSet(jac_pre,lsvi->Da,ADD_VALUES);CHKERRQ(ierr);
  

  PetscFunctionReturn(0);
}
  
/*
   SNESLSVIComputeMeritFunctionGradient - Computes the gradient of the merit function psi.

   Input Parameters:
.  phi - semismooth function.
.  H   - B-subdifferential

   Output Parameters:
.  dpsi - merit function gradient

   Notes:
   The merit function gradient is computed as follows
   dpsi = H^T*phi
*/
#undef __FUNCT__
#define __FUNCT__ "SNESLSVIComputeMeritFunctionGradient"
PetscErrorCode SNESLSVIComputeMeritFunctionGradient(Mat H, Vec phi, Vec dpsi)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = MatMultTranspose(H,phi,dpsi);

  PetscFunctionReturn(0);
}

/*
   SNESLSVISetDescentDirection - Sets the descent direction for the semismooth algorithm

   Input Parameters:
.  snes - the SNES context.
.  dpsi - gradient of the merit function.

   Output Parameters:
.  flg  - PETSC_TRUE if the sufficient descent condition is not satisfied.

   Notes: 
   The condition for the sufficient descent direction is
        dpsi^T*Y <= -rho*||Y||^delta
   where rho, delta are as defined in the SNES_LSVI structure.
   If this condition is satisfied then the existing descent direction i.e.
   the direction given by the linear solve should be used otherwise it should be set to the negative of the
   merit function gradient i.e -dpsi.
*/
#undef __FUNCT__
#define __FUNCT__ "SNESLSVICheckDescentDirection"
PetscErrorCode SNESLSVICheckDescentDirection(SNES snes,Vec dpsi, Vec Y,PetscTruth* flg)
{
  PetscErrorCode  ierr;
  SNES_LSVI       *lsvi = (SNES_LSVI*)snes->data;
  PetscScalar     dpsidotY;
  PetscReal       norm_Y,rhs;
  const PetscReal rho = lsvi->rho,delta=lsvi->delta;

  PetscFunctionBegin;

  *flg = PETSC_FALSE;
  ierr = VecDot(dpsi,Y,&dpsidotY);CHKERRQ(ierr);
  ierr = VecNormBegin(Y,NORM_2,&norm_Y);CHKERRQ(ierr);
  ierr = VecNormEnd(Y,NORM_2,&norm_Y);CHKERRQ(ierr);

  rhs = -delta*PetscPowScalar(norm_Y,rho);

  if (dpsidotY > rhs) *flg = PETSC_TRUE;
 
  PetscFunctionReturn(0);
}

/*  -------------------------------------------------------------------- 

     This file implements a semismooth truncated Newton method with a line search,
     for solving a system of nonlinear equations in complementarity form, using the KSP, Vec, 
     and Mat interfaces for linear solvers, vectors, and matrices, 
     respectively.

     The following basic routines are required for each nonlinear solver:
          SNESCreate_XXX()          - Creates a nonlinear solver context
          SNESSetFromOptions_XXX()  - Sets runtime options
          SNESSolve_XXX()           - Solves the nonlinear system
          SNESDestroy_XXX()         - Destroys the nonlinear solver context
     The suffix "_XXX" denotes a particular implementation, in this case
     we use _LSVI (e.g., SNESCreate_LSVI, SNESSolve_LSVI) for solving
     systems of nonlinear equations with a line search (LS) method.
     These routines are actually called via the common user interface
     routines SNESCreate(), SNESSetFromOptions(), SNESSolve(), and 
     SNESDestroy(), so the application code interface remains identical 
     for all nonlinear solvers.

     Another key routine is:
          SNESSetUp_XXX()           - Prepares for the use of a nonlinear solver
     by setting data structures and options.   The interface routine SNESSetUp()
     is not usually called directly by the user, but instead is called by
     SNESSolve() if necessary.

     Additional basic routines are:
          SNESView_XXX()            - Prints details of runtime options that
                                      have actually been used.
     These are called by application codes via the interface routines
     SNESView().

     The various types of solvers (preconditioners, Krylov subspace methods,
     nonlinear solvers, timesteppers) are all organized similarly, so the
     above description applies to these categories also.  

    -------------------------------------------------------------------- */
/*
   SNESSolve_LSVI - Solves the complementarity problem with a semismooth Newton
   method using a line search.

   Input Parameters:
.  snes - the SNES context

   Output Parameter:
.  outits - number of iterations until termination

   Application Interface Routine: SNESSolve()

   Notes:
   This implements essentially a semismooth Newton method with a
   line search.  By default a cubic backtracking line search 
   is employed, as described in the text "Numerical Methods for
   Unconstrained Optimization and Nonlinear Equations" by Dennis 
   and Schnabel.
*/
#undef __FUNCT__  
#define __FUNCT__ "SNESSolve_LSVI"
PetscErrorCode SNESSolve_LSVI(SNES snes)
{ 
  SNES_LSVI          *lsvi = (SNES_LSVI*)snes->data;
  PetscErrorCode     ierr;
  PetscInt           maxits,i,lits;
  PetscTruth         lssucceed,changedir;
  MatStructure       flg = DIFFERENT_NONZERO_PATTERN;
  PetscReal          gnorm,xnorm=0,ynorm;
  Vec                Y,X,F,G,W;
  KSPConvergedReason kspreason;

  PetscFunctionBegin;
  snes->numFailures            = 0;
  snes->numLinearSolveFailures = 0;
  snes->reason                 = SNES_CONVERGED_ITERATING;

  maxits	= snes->max_its;	/* maximum number of iterations */
  X		= snes->vec_sol;	/* solution vector */
  F		= snes->vec_func;	/* residual vector */
  Y		= snes->work[0];	/* work vectors */
  G		= snes->work[1];
  W		= snes->work[2];

  ierr = PetscObjectTakeAccess(snes);CHKERRQ(ierr);
  snes->iter = 0;
  snes->norm = 0.0;
  ierr = PetscObjectGrantAccess(snes);CHKERRQ(ierr);
  ierr = SNESLSVIComputeSSFunction(snes,X,lsvi->phi);CHKERRQ(ierr);
  if (snes->domainerror) {
    snes->reason = SNES_DIVERGED_FUNCTION_DOMAIN;
    PetscFunctionReturn(0);
  }

   /* Compute Merit function */
  ierr = SNESLSVIComputeMeritFunction(lsvi->phi,&lsvi->merit,&lsvi->phinorm);CHKERRQ(ierr);

  ierr = VecNormBegin(X,NORM_2,&xnorm);CHKERRQ(ierr);	/* xnorm <- ||x||  */
  ierr = VecNormEnd(X,NORM_2,&xnorm);CHKERRQ(ierr);
  if PetscIsInfOrNanReal(lsvi->merit) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_FP,"User provided compute function generated a Not-a-Number");

  ierr = PetscObjectTakeAccess(snes);CHKERRQ(ierr);
  snes->norm = lsvi->phinorm;
  ierr = PetscObjectGrantAccess(snes);CHKERRQ(ierr);
  SNESLogConvHistory(snes,lsvi->phinorm,0);
  SNESMonitor(snes,0,lsvi->phinorm);

  /* set parameter for default relative tolerance convergence test */
  snes->ttol = lsvi->merit*snes->rtol;
  /* test convergence */
  ierr = (*snes->ops->converged)(snes,0,0.0,0.0,lsvi->phinorm,&snes->reason,snes->cnvP);CHKERRQ(ierr);
  if (snes->reason) PetscFunctionReturn(0);

  for (i=0; i<maxits; i++) {

    /* Call general purpose update function */
    if (snes->ops->update) {
      ierr = (*snes->ops->update)(snes, snes->iter);CHKERRQ(ierr);
    }

    /* Solve J Y = Phi, where J is the B-subdifferential matrix */
    ierr = SNESComputeJacobian(snes,X,&snes->jacobian,&snes->jacobian_pre,&flg);CHKERRQ(ierr);
    ierr = SNESLSVIComputeBsubdifferential(snes,X,F,snes->jacobian,snes->jacobian_pre,&flg);CHKERRQ(ierr);
    ierr = KSPSetOperators(snes->ksp,snes->jacobian,snes->jacobian_pre,flg);CHKERRQ(ierr);
    ierr = SNES_KSPSolve(snes,snes->ksp,F,Y);CHKERRQ(ierr);
    ierr = KSPGetConvergedReason(snes->ksp,&kspreason);CHKERRQ(ierr);
    ierr = SNESLSVIComputeMeritFunctionGradient(snes->jacobian,lsvi->phi,lsvi->dpsi);CHKERRQ(ierr);
    ierr = SNESLSVICheckDescentDirection(snes,lsvi->dpsi,Y,&changedir);CHKERRQ(ierr);
    if (kspreason < 0 || changedir) {
      if (++snes->numLinearSolveFailures >= snes->maxLinearSolveFailures) {
        ierr = PetscInfo2(snes,"iter=%D, number linear solve failures %D greater than current SNES allowed, stopping solve\n",snes->iter,snes->numLinearSolveFailures);CHKERRQ(ierr);
        snes->reason = SNES_DIVERGED_LINEAR_SOLVE;
        break;
      }
      ierr = VecCopy(lsvi->dpsi,Y);CHKERRQ(ierr);
    }
    ierr = KSPGetIterationNumber(snes->ksp,&lits);CHKERRQ(ierr);
    snes->linear_its += lits;
    ierr = PetscInfo2(snes,"iter=%D, linear solve iterations=%D\n",snes->iter,lits);CHKERRQ(ierr);

    if (lsvi->precheckstep) {
      PetscTruth changed_y = PETSC_FALSE;
      ierr = (*lsvi->precheckstep)(snes,X,Y,lsvi->precheck,&changed_y);CHKERRQ(ierr);
    }

    if (PetscLogPrintInfo){
      ierr = SNESLSVICheckResidual_Private(snes,snes->jacobian,F,Y,G,W);CHKERRQ(ierr);
    }

    /* Compute a (scaled) negative update in the line search routine: 
         Y <- X - lambda*Y 
       and evaluate G = function(Y) (depends on the line search). 
    */
    ierr = VecCopy(Y,snes->vec_sol_update);CHKERRQ(ierr);
    ynorm = 1; gnorm = lsvi->phinorm;
    ierr = (*lsvi->LineSearch)(snes,lsvi->lsP,X,lsvi->phi,G,Y,W,lsvi->phinorm,xnorm,&ynorm,&gnorm,&lssucceed);CHKERRQ(ierr);
    ierr = PetscInfo4(snes,"fnorm=%18.16e, gnorm=%18.16e, ynorm=%18.16e, lssucceed=%d\n",lsvi->phinorm,gnorm,ynorm,(int)lssucceed);CHKERRQ(ierr);
    if (snes->reason == SNES_DIVERGED_FUNCTION_COUNT) break;
    if (snes->domainerror) {
      snes->reason = SNES_DIVERGED_FUNCTION_DOMAIN;
      PetscFunctionReturn(0);
    }
    if (!lssucceed) {
      if (++snes->numFailures >= snes->maxFailures) {
	PetscTruth ismin;
        snes->reason = SNES_DIVERGED_LS_FAILURE;
        ierr = SNESLSVICheckLocalMin_Private(snes,snes->jacobian,G,W,gnorm,&ismin);CHKERRQ(ierr);
        if (ismin) snes->reason = SNES_DIVERGED_LOCAL_MIN;
        break;
      }
    }
    /* Update function and solution vectors */
    lsvi->phinorm = gnorm;
    lsvi->merit = 0.5*lsvi->phinorm*lsvi->phinorm;
    ierr = VecCopy(G,lsvi->phi);CHKERRQ(ierr);
    ierr = VecCopy(W,X);CHKERRQ(ierr);
    /* Monitor convergence */
    ierr = PetscObjectTakeAccess(snes);CHKERRQ(ierr);
    snes->iter = i+1;
    snes->norm = lsvi->phinorm;
    ierr = PetscObjectGrantAccess(snes);CHKERRQ(ierr);
    SNESLogConvHistory(snes,snes->norm,lits);
    SNESMonitor(snes,snes->iter,snes->norm);
    /* Test for convergence, xnorm = || X || */
    if (snes->ops->converged != SNESSkipConverged) { ierr = VecNorm(X,NORM_2,&xnorm);CHKERRQ(ierr); }
    ierr = (*snes->ops->converged)(snes,snes->iter,xnorm,ynorm,lsvi->phinorm,&snes->reason,snes->cnvP);CHKERRQ(ierr);
    if (snes->reason) break;
  }
  if (i == maxits) {
    ierr = PetscInfo1(snes,"Maximum number of iterations has been reached: %D\n",maxits);CHKERRQ(ierr);
    if(!snes->reason) snes->reason = SNES_DIVERGED_MAX_IT;
  }
  PetscFunctionReturn(0);
}
/* -------------------------------------------------------------------------- */
/*
   SNESSetUp_LSVI - Sets up the internal data structures for the later use
   of the SNESLSVI nonlinear solver.

   Input Parameter:
.  snes - the SNES context
.  x - the solution vector

   Application Interface Routine: SNESSetUp()

   Notes:
   For basic use of the SNES solvers, the user need not explicitly call
   SNESSetUp(), since these actions will automatically occur during
   the call to SNESSolve().
 */
#undef __FUNCT__  
#define __FUNCT__ "SNESSetUp_LSVI"
PetscErrorCode SNESSetUp_LSVI(SNES snes)
{
  PetscErrorCode ierr;
  SNES_LSVI      *lsvi = (SNES_LSVI*) snes->data;

  PetscFunctionBegin;
  if (!snes->vec_sol_update) {
    ierr = VecDuplicate(snes->vec_sol,&snes->vec_sol_update);CHKERRQ(ierr);
    ierr = PetscLogObjectParent(snes,snes->vec_sol_update);CHKERRQ(ierr);
  }
  if (!snes->work) {
    snes->nwork = 3;
    ierr = VecDuplicateVecs(snes->vec_sol,snes->nwork,&snes->work);CHKERRQ(ierr);
    ierr = PetscLogObjectParents(snes,snes->nwork,snes->work);CHKERRQ(ierr);
  }

  ierr = VecDuplicate(snes->vec_sol, &lsvi->phi); CHKERRQ(ierr);
  ierr = VecDuplicate(snes->vec_sol, &lsvi->dpsi); CHKERRQ(ierr);
  ierr = VecDuplicate(snes->vec_sol, &lsvi->Da); CHKERRQ(ierr);
  ierr = VecDuplicate(snes->vec_sol, &lsvi->Db); CHKERRQ(ierr);

  /* If the lower and upper bound on variables are not set, set it to
     -Inf and Inf */
  if (!lsvi->xl && !lsvi->xu) {
    lsvi->usersetxbounds = PETSC_FALSE;
    ierr = VecDuplicate(snes->vec_sol, &lsvi->xl); CHKERRQ(ierr);
    ierr = VecSet(lsvi->xl,PETSC_LSVI_NINF);CHKERRQ(ierr);
    ierr = VecDuplicate(snes->vec_sol, &lsvi->xu); CHKERRQ(ierr);
    ierr = VecSet(lsvi->xu,PETSC_LSVI_INF);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}
/* -------------------------------------------------------------------------- */
/*
   SNESDestroy_LSVI - Destroys the private SNES_LSVI context that was created
   with SNESCreate_LSVI().

   Input Parameter:
.  snes - the SNES context

   Application Interface Routine: SNESDestroy()
 */
#undef __FUNCT__  
#define __FUNCT__ "SNESDestroy_LSVI"
PetscErrorCode SNESDestroy_LSVI(SNES snes)
{
  SNES_LSVI        *lsvi = (SNES_LSVI*) snes->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (snes->vec_sol_update) {
    ierr = VecDestroy(snes->vec_sol_update);CHKERRQ(ierr);
    snes->vec_sol_update = PETSC_NULL;
  }
  if (snes->nwork) {
    ierr = VecDestroyVecs(snes->work,snes->nwork);CHKERRQ(ierr);
    snes->nwork = 0;
    snes->work  = PETSC_NULL;
  }

  /* clear vectors */
  ierr = VecDestroy(lsvi->phi); CHKERRQ(ierr);
  ierr = VecDestroy(lsvi->dpsi); CHKERRQ(ierr);
  ierr = VecDestroy(lsvi->Da); CHKERRQ(ierr);
  ierr = VecDestroy(lsvi->Db); CHKERRQ(ierr);
  if (!lsvi->usersetxbounds) {
    ierr = VecDestroy(lsvi->xl); CHKERRQ(ierr);
    ierr = VecDestroy(lsvi->xu); CHKERRQ(ierr);
  }
  if (lsvi->monitor) {
    ierr = PetscViewerASCIIMonitorDestroy(lsvi->monitor);CHKERRQ(ierr);
  } 
  ierr = PetscFree(snes->data);CHKERRQ(ierr);

  /* clear composed functions */
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)snes,"SNESLineSearchSet_C","",PETSC_NULL);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}
/* -------------------------------------------------------------------------- */
#undef __FUNCT__  
#define __FUNCT__ "SNESLineSearchNo_LSVI"

/*
  This routine is a copy of SNESLineSearchNo routine in snes/impls/ls/ls.c

*/
PetscErrorCode SNESLineSearchNo_LSVI(SNES snes,void *lsctx,Vec x,Vec f,Vec g,Vec y,Vec w,PetscReal fnorm,PetscReal xnorm,PetscReal *ynorm,PetscReal *gnorm,PetscTruth *flag)
{
  PetscErrorCode ierr;
  SNES_LSVI        *lsvi = (SNES_LSVI*)snes->data;
  PetscTruth     changed_w = PETSC_FALSE,changed_y = PETSC_FALSE;

  PetscFunctionBegin;
  *flag = PETSC_TRUE; 
  ierr = PetscLogEventBegin(SNES_LineSearch,snes,x,f,g);CHKERRQ(ierr);
  ierr = VecNorm(y,NORM_2,ynorm);CHKERRQ(ierr);         /* ynorm = || y || */
  ierr = VecWAXPY(w,-1.0,y,x);CHKERRQ(ierr);            /* w <- x - y   */
  if (lsvi->postcheckstep) {
   ierr = (*lsvi->postcheckstep)(snes,x,y,w,lsvi->postcheck,&changed_y,&changed_w);CHKERRQ(ierr);
  }
  if (changed_y) {
    ierr = VecWAXPY(w,-1.0,y,x);CHKERRQ(ierr);            /* w <- x - y   */
  }
  ierr = SNESLSVIComputeSSFunction(snes,w,g);CHKERRQ(ierr);
  if (!snes->domainerror) {
    ierr = VecNorm(g,NORM_2,gnorm);CHKERRQ(ierr);  /* gnorm = || g || */
    if PetscIsInfOrNanReal(*gnorm) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_FP,"User provided compute function generated a Not-a-Number");
  }
  ierr = PetscLogEventEnd(SNES_LineSearch,snes,x,f,g);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* -------------------------------------------------------------------------- */
#undef __FUNCT__  
#define __FUNCT__ "SNESLineSearchNoNorms_LSVI"

/*
  This routine is a copy of SNESLineSearchNoNorms in snes/impls/ls/ls.c
*/
PetscErrorCode SNESLineSearchNoNorms_LSVI(SNES snes,void *lsctx,Vec x,Vec f,Vec g,Vec y,Vec w,PetscReal fnorm,PetscReal xnorm,PetscReal *ynorm,PetscReal *gnorm,PetscTruth *flag)
{
  PetscErrorCode ierr;
  SNES_LSVI        *lsvi = (SNES_LSVI*)snes->data;
  PetscTruth     changed_w = PETSC_FALSE,changed_y = PETSC_FALSE;

  PetscFunctionBegin;
  *flag = PETSC_TRUE; 
  ierr = PetscLogEventBegin(SNES_LineSearch,snes,x,f,g);CHKERRQ(ierr);
  ierr = VecWAXPY(w,-1.0,y,x);CHKERRQ(ierr);            /* w <- x - y      */
  if (lsvi->postcheckstep) {
   ierr = (*lsvi->postcheckstep)(snes,x,y,w,lsvi->postcheck,&changed_y,&changed_w);CHKERRQ(ierr);
  }
  if (changed_y) {
    ierr = VecWAXPY(w,-1.0,y,x);CHKERRQ(ierr);            /* w <- x - y   */
  }
  
  /* don't evaluate function the last time through */
  if (snes->iter < snes->max_its-1) {
    ierr = SNESComputeFunction(snes,w,g);CHKERRQ(ierr);
  }
  ierr = PetscLogEventEnd(SNES_LineSearch,snes,x,f,g);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
/* -------------------------------------------------------------------------- */
#undef __FUNCT__  
#define __FUNCT__ "SNESLineSearchCubic_LSVI"
/*
  This routine is a copy of SNESLineSearchCubic in snes/impls/ls/ls.c
*/
PetscErrorCode SNESLineSearchCubic_LSVI(SNES snes,void *lsctx,Vec x,Vec f,Vec g,Vec y,Vec w,PetscReal fnorm,PetscReal xnorm,PetscReal *ynorm,PetscReal *gnorm,PetscTruth *flag)
{
  /* 
     Note that for line search purposes we work with with the related
     minimization problem:
        min  z(x):  R^n -> R,
     where z(x) = .5 * fnorm*fnorm, and fnorm = || f ||_2.
     This function z(x) is same as the merit function for the complementarity problem.
   */
        
  PetscReal      initslope,lambdaprev,gnormprev,a,b,d,t1,t2,rellength;
  PetscReal      minlambda,lambda,lambdatemp;
#if defined(PETSC_USE_COMPLEX)
  PetscScalar    cinitslope;
#endif
  PetscErrorCode ierr;
  PetscInt       count;
  SNES_LSVI        *lsvi = (SNES_LSVI*)snes->data;
  PetscTruth     changed_w = PETSC_FALSE,changed_y = PETSC_FALSE;
  MPI_Comm       comm;

  PetscFunctionBegin;
  ierr = PetscObjectGetComm((PetscObject)snes,&comm);CHKERRQ(ierr);
  ierr = PetscLogEventBegin(SNES_LineSearch,snes,x,f,g);CHKERRQ(ierr);
  *flag   = PETSC_TRUE;

  ierr = VecNorm(y,NORM_2,ynorm);CHKERRQ(ierr);
  if (*ynorm == 0.0) {
    if (lsvi->monitor) {
      ierr = PetscViewerASCIIMonitorPrintf(lsvi->monitor,"    Line search: Initial direction and size is 0\n");CHKERRQ(ierr);
    }
    *gnorm = fnorm;
    ierr   = VecCopy(x,w);CHKERRQ(ierr);
    ierr   = VecCopy(f,g);CHKERRQ(ierr);
    *flag  = PETSC_FALSE;
    goto theend1;
  }
  if (*ynorm > lsvi->maxstep) {	/* Step too big, so scale back */
    if (lsvi->monitor) {
      ierr = PetscViewerASCIIMonitorPrintf(lsvi->monitor,"    Line search: Scaling step by %G old ynorm %G\n",lsvi->maxstep/(*ynorm),*ynorm);CHKERRQ(ierr);
    }
    ierr = VecScale(y,lsvi->maxstep/(*ynorm));CHKERRQ(ierr);
    *ynorm = lsvi->maxstep;
  }
  ierr      = VecMaxPointwiseDivide(y,x,&rellength);CHKERRQ(ierr);
  minlambda = lsvi->minlambda/rellength;
  ierr      = MatMult(snes->jacobian,y,w);CHKERRQ(ierr);
#if defined(PETSC_USE_COMPLEX)
  ierr      = VecDot(f,w,&cinitslope);CHKERRQ(ierr);
  initslope = PetscRealPart(cinitslope);
#else
  ierr      = VecDot(f,w,&initslope);CHKERRQ(ierr);
#endif
  if (initslope > 0.0)  initslope = -initslope;
  if (initslope == 0.0) initslope = -1.0;

  ierr = VecWAXPY(w,-1.0,y,x);CHKERRQ(ierr);
  if (snes->nfuncs >= snes->max_funcs) {
    ierr  = PetscInfo(snes,"Exceeded maximum function evaluations, while checking full step length!\n");CHKERRQ(ierr);
    *flag = PETSC_FALSE;
    snes->reason = SNES_DIVERGED_FUNCTION_COUNT;
    goto theend1;
  }
  ierr = SNESLSVIComputeSSFunction(snes,w,g);CHKERRQ(ierr);
  if (snes->domainerror) {
    ierr = PetscLogEventEnd(SNES_LineSearch,snes,x,f,g);CHKERRQ(ierr);
    PetscFunctionReturn(0);
  }
  ierr = VecNorm(g,NORM_2,gnorm);CHKERRQ(ierr);
  if PetscIsInfOrNanReal(*gnorm) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_FP,"User provided compute function generated a Not-a-Number");
  ierr = PetscInfo2(snes,"Initial fnorm %G gnorm %G\n",fnorm,*gnorm);CHKERRQ(ierr);
  if (.5*(*gnorm)*(*gnorm) <= .5*fnorm*fnorm + lsvi->alpha*initslope) { /* Sufficient reduction */
    if (lsvi->monitor) {
      ierr = PetscViewerASCIIMonitorPrintf(lsvi->monitor,"    Line search: Using full step: fnorm %G gnorm %G\n",fnorm,*gnorm);CHKERRQ(ierr);
    }
    goto theend1;
  }

  /* Fit points with quadratic */
  lambda     = 1.0;
  lambdatemp = -initslope/((*gnorm)*(*gnorm) - fnorm*fnorm - 2.0*initslope);
  lambdaprev = lambda;
  gnormprev  = *gnorm;
  if (lambdatemp > .5*lambda)  lambdatemp = .5*lambda;
  if (lambdatemp <= .1*lambda) lambda = .1*lambda; 
  else                         lambda = lambdatemp;

  ierr  = VecWAXPY(w,-lambda,y,x);CHKERRQ(ierr);
  if (snes->nfuncs >= snes->max_funcs) {
    ierr  = PetscInfo1(snes,"Exceeded maximum function evaluations, while attempting quadratic backtracking! %D \n",snes->nfuncs);CHKERRQ(ierr);
    *flag = PETSC_FALSE;
    snes->reason = SNES_DIVERGED_FUNCTION_COUNT;
    goto theend1;
  }
  ierr = SNESLSVIComputeSSFunction(snes,w,g);CHKERRQ(ierr);
  if (snes->domainerror) {
    ierr = PetscLogEventEnd(SNES_LineSearch,snes,x,f,g);CHKERRQ(ierr);
    PetscFunctionReturn(0);
  }
  ierr = VecNorm(g,NORM_2,gnorm);CHKERRQ(ierr);
  if PetscIsInfOrNanReal(*gnorm) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_FP,"User provided compute function generated a Not-a-Number");
  if (lsvi->monitor) {
    ierr = PetscViewerASCIIMonitorPrintf(lsvi->monitor,"    Line search: gnorm after quadratic fit %G\n",*gnorm);CHKERRQ(ierr);
  }
  if (.5*(*gnorm)*(*gnorm) < .5*fnorm*fnorm + lambda*lsvi->alpha*initslope) { /* sufficient reduction */
    if (lsvi->monitor) {
      ierr = PetscViewerASCIIMonitorPrintf(lsvi->monitor,"    Line search: Quadratically determined step, lambda=%18.16e\n",lambda);CHKERRQ(ierr);
    }
    goto theend1;
  }

  /* Fit points with cubic */
  count = 1;
  while (PETSC_TRUE) {
    if (lambda <= minlambda) { 
      if (lsvi->monitor) {
	ierr = PetscViewerASCIIMonitorPrintf(lsvi->monitor,"    Line search: unable to find good step length! After %D tries \n",count);CHKERRQ(ierr);
	ierr = PetscViewerASCIIMonitorPrintf(lsvi->monitor,"    Line search: fnorm=%18.16e, gnorm=%18.16e, ynorm=%18.16e, minlambda=%18.16e, lambda=%18.16e, initial slope=%18.16e\n",fnorm,*gnorm,*ynorm,minlambda,lambda,initslope);CHKERRQ(ierr);
      }
      *flag = PETSC_FALSE; 
      break;
    }
    t1 = .5*((*gnorm)*(*gnorm) - fnorm*fnorm) - lambda*initslope;
    t2 = .5*(gnormprev*gnormprev  - fnorm*fnorm) - lambdaprev*initslope;
    a  = (t1/(lambda*lambda) - t2/(lambdaprev*lambdaprev))/(lambda-lambdaprev);
    b  = (-lambdaprev*t1/(lambda*lambda) + lambda*t2/(lambdaprev*lambdaprev))/(lambda-lambdaprev);
    d  = b*b - 3*a*initslope;
    if (d < 0.0) d = 0.0;
    if (a == 0.0) {
      lambdatemp = -initslope/(2.0*b);
    } else {
      lambdatemp = (-b + sqrt(d))/(3.0*a);
    }
    lambdaprev = lambda;
    gnormprev  = *gnorm;
    if (lambdatemp > .5*lambda)  lambdatemp = .5*lambda;
    if (lambdatemp <= .1*lambda) lambda     = .1*lambda;
    else                         lambda     = lambdatemp;
    ierr  = VecWAXPY(w,-lambda,y,x);CHKERRQ(ierr);
    if (snes->nfuncs >= snes->max_funcs) {
      ierr = PetscInfo1(snes,"Exceeded maximum function evaluations, while looking for good step length! %D \n",count);CHKERRQ(ierr);
      ierr = PetscInfo5(snes,"fnorm=%18.16e, gnorm=%18.16e, ynorm=%18.16e, lambda=%18.16e, initial slope=%18.16e\n",fnorm,*gnorm,*ynorm,lambda,initslope);CHKERRQ(ierr);
      *flag = PETSC_FALSE;
      snes->reason = SNES_DIVERGED_FUNCTION_COUNT;
      break;
    }
    ierr = SNESLSVIComputeSSFunction(snes,w,g);CHKERRQ(ierr);
    if (snes->domainerror) {
      ierr = PetscLogEventEnd(SNES_LineSearch,snes,x,f,g);CHKERRQ(ierr);
      PetscFunctionReturn(0);
    }
    ierr = VecNorm(g,NORM_2,gnorm);CHKERRQ(ierr);
    if PetscIsInfOrNanReal(*gnorm) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_FP,"User provided compute function generated a Not-a-Number");
    if (.5*(*gnorm)*(*gnorm) < .5*fnorm*fnorm + lambda*lsvi->alpha*initslope) { /* is reduction enough? */
      if (lsvi->monitor) {
	ierr = PetscPrintf(comm,"    Line search: Cubically determined step, current gnorm %G lambda=%18.16e\n",*gnorm,lambda);CHKERRQ(ierr);
      }
      break;
    } else {
      if (lsvi->monitor) {
        ierr = PetscPrintf(comm,"    Line search: Cubic step no good, shrinking lambda, current gnorem %G lambda=%18.16e\n",*gnorm,lambda);CHKERRQ(ierr);
      }
    }
    count++;
  }
  theend1:
  /* Optional user-defined check for line search step validity */
  if (lsvi->postcheckstep && *flag) {
    ierr = (*lsvi->postcheckstep)(snes,x,y,w,lsvi->postcheck,&changed_y,&changed_w);CHKERRQ(ierr);
    if (changed_y) {
      ierr = VecWAXPY(w,-1.0,y,x);CHKERRQ(ierr);
    }
    if (changed_y || changed_w) { /* recompute the function if the step has changed */
      ierr = SNESLSVIComputeSSFunction(snes,w,g);CHKERRQ(ierr);
      if (snes->domainerror) {
        ierr = PetscLogEventEnd(SNES_LineSearch,snes,x,f,g);CHKERRQ(ierr);
        PetscFunctionReturn(0);
      }
      ierr = VecNormBegin(g,NORM_2,gnorm);CHKERRQ(ierr);
      if PetscIsInfOrNanReal(*gnorm) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_FP,"User provided compute function generated a Not-a-Number");
      ierr = VecNormBegin(y,NORM_2,ynorm);CHKERRQ(ierr);
      ierr = VecNormEnd(g,NORM_2,gnorm);CHKERRQ(ierr);
      ierr = VecNormEnd(y,NORM_2,ynorm);CHKERRQ(ierr);
    }
  }
  ierr = PetscLogEventEnd(SNES_LineSearch,snes,x,f,g);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
/* -------------------------------------------------------------------------- */
#undef __FUNCT__  
#define __FUNCT__ "SNESLineSearchQuadratic_LSVI"
/*
  This routine is a copy of SNESLineSearchQuadratic in snes/impls/ls/ls.c
*/
PetscErrorCode SNESLineSearchQuadratic_LSVI(SNES snes,void *lsctx,Vec x,Vec f,Vec g,Vec y,Vec w,PetscReal fnorm,PetscReal xnorm,PetscReal *ynorm,PetscReal *gnorm,PetscTruth *flag)
{
  /* 
     Note that for line search purposes we work with with the related
     minimization problem:
        min  z(x):  R^n -> R,
     where z(x) = .5 * fnorm*fnorm,and fnorm = || f ||_2.
     z(x) is the same as the merit function for the complementarity problem
   */
  PetscReal      initslope,minlambda,lambda,lambdatemp,rellength;
#if defined(PETSC_USE_COMPLEX)
  PetscScalar    cinitslope;
#endif
  PetscErrorCode ierr;
  PetscInt       count;
  SNES_LSVI        *lsvi = (SNES_LSVI*)snes->data;
  PetscTruth     changed_w = PETSC_FALSE,changed_y = PETSC_FALSE;

  PetscFunctionBegin;
  ierr    = PetscLogEventBegin(SNES_LineSearch,snes,x,f,g);CHKERRQ(ierr);
  *flag   = PETSC_TRUE;

  ierr = VecNorm(y,NORM_2,ynorm);CHKERRQ(ierr);
  if (*ynorm == 0.0) {
    if (lsvi->monitor) {
      ierr = PetscViewerASCIIMonitorPrintf(lsvi->monitor,"Line search: Direction and size is 0\n");CHKERRQ(ierr);
    }
    *gnorm = fnorm;
    ierr   = VecCopy(x,w);CHKERRQ(ierr);
    ierr   = VecCopy(f,g);CHKERRQ(ierr);
    *flag  = PETSC_FALSE;
    goto theend2;
  }
  if (*ynorm > lsvi->maxstep) {	/* Step too big, so scale back */
    ierr   = VecScale(y,lsvi->maxstep/(*ynorm));CHKERRQ(ierr);
    *ynorm = lsvi->maxstep;
  }
  ierr      = VecMaxPointwiseDivide(y,x,&rellength);CHKERRQ(ierr);
  minlambda = lsvi->minlambda/rellength;
  ierr = MatMult(snes->jacobian,y,w);CHKERRQ(ierr);
#if defined(PETSC_USE_COMPLEX)
  ierr      = VecDot(f,w,&cinitslope);CHKERRQ(ierr);
  initslope = PetscRealPart(cinitslope);
#else
  ierr = VecDot(f,w,&initslope);CHKERRQ(ierr);
#endif
  if (initslope > 0.0)  initslope = -initslope;
  if (initslope == 0.0) initslope = -1.0;
  ierr = PetscInfo1(snes,"Initslope %G \n",initslope);CHKERRQ(ierr);

  ierr = VecWAXPY(w,-1.0,y,x);CHKERRQ(ierr);
  if (snes->nfuncs >= snes->max_funcs) {
    ierr  = PetscInfo(snes,"Exceeded maximum function evaluations, while checking full step length!\n");CHKERRQ(ierr);
    *flag = PETSC_FALSE;
    snes->reason = SNES_DIVERGED_FUNCTION_COUNT;
    goto theend2;
  }
  ierr = SNESLSVIComputeSSFunction(snes,w,g);CHKERRQ(ierr);
  if (snes->domainerror) {
    ierr = PetscLogEventEnd(SNES_LineSearch,snes,x,f,g);CHKERRQ(ierr);
    PetscFunctionReturn(0);
  }
  ierr = VecNorm(g,NORM_2,gnorm);CHKERRQ(ierr);
  if PetscIsInfOrNanReal(*gnorm) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_FP,"User provided compute function generated a Not-a-Number");
  if (.5*(*gnorm)*(*gnorm) <= .5*fnorm*fnorm + lsvi->alpha*initslope) { /* Sufficient reduction */
    if (lsvi->monitor) {
      ierr = PetscViewerASCIIMonitorPrintf(lsvi->monitor,"    Line Search: Using full step\n");CHKERRQ(ierr);
    }
    goto theend2;
  }

  /* Fit points with quadratic */
  lambda = 1.0;
  count = 1;
  while (PETSC_TRUE) {
    if (lambda <= minlambda) { /* bad luck; use full step */
      if (lsvi->monitor) {
        ierr = PetscViewerASCIIMonitorPrintf(lsvi->monitor,"Line search: Unable to find good step length! %D \n",count);CHKERRQ(ierr);
        ierr = PetscViewerASCIIMonitorPrintf(lsvi->monitor,"Line search: fnorm=%G, gnorm=%G, ynorm=%G, lambda=%G, initial slope=%G\n",fnorm,*gnorm,*ynorm,lambda,initslope);CHKERRQ(ierr);
      }
      ierr = VecCopy(x,w);CHKERRQ(ierr);
      *flag = PETSC_FALSE;
      break;
    }
    lambdatemp = -initslope/((*gnorm)*(*gnorm) - fnorm*fnorm - 2.0*initslope);
    if (lambdatemp > .5*lambda)  lambdatemp = .5*lambda;
    if (lambdatemp <= .1*lambda) lambda     = .1*lambda; 
    else                         lambda     = lambdatemp;
    
    ierr = VecWAXPY(w,-lambda,y,x);CHKERRQ(ierr);
    if (snes->nfuncs >= snes->max_funcs) {
      ierr  = PetscInfo1(snes,"Exceeded maximum function evaluations, while looking for good step length! %D \n",count);CHKERRQ(ierr);
      ierr  = PetscInfo5(snes,"fnorm=%18.16e, gnorm=%18.16e, ynorm=%18.16e, lambda=%18.16e, initial slope=%18.16e\n",fnorm,*gnorm,*ynorm,lambda,initslope);CHKERRQ(ierr);
      *flag = PETSC_FALSE;
      snes->reason = SNES_DIVERGED_FUNCTION_COUNT;
      break;
    }
    ierr = SNESLSVIComputeSSFunction(snes,w,g);CHKERRQ(ierr);
    if (snes->domainerror) {
      ierr = PetscLogEventEnd(SNES_LineSearch,snes,x,f,g);CHKERRQ(ierr);
      PetscFunctionReturn(0);
    }
    ierr = VecNorm(g,NORM_2,gnorm);CHKERRQ(ierr);
    if PetscIsInfOrNanReal(*gnorm) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_FP,"User provided compute function generated a Not-a-Number");
    if (.5*(*gnorm)*(*gnorm) < .5*fnorm*fnorm + lambda*lsvi->alpha*initslope) { /* sufficient reduction */
      if (lsvi->monitor) {
        ierr = PetscViewerASCIIMonitorPrintf(lsvi->monitor,"    Line Search: Quadratically determined step, lambda=%G\n",lambda);CHKERRQ(ierr);
      }
      break;
    }
    count++;
  }
  theend2:
  /* Optional user-defined check for line search step validity */
  if (lsvi->postcheckstep) {
    ierr = (*lsvi->postcheckstep)(snes,x,y,w,lsvi->postcheck,&changed_y,&changed_w);CHKERRQ(ierr);
    if (changed_y) {
      ierr = VecWAXPY(w,-1.0,y,x);CHKERRQ(ierr);
    }
    if (changed_y || changed_w) { /* recompute the function if the step has changed */
      ierr = SNESLSVIComputeSSFunction(snes,w,g);
      if (snes->domainerror) {
        ierr = PetscLogEventEnd(SNES_LineSearch,snes,x,f,g);CHKERRQ(ierr);
        PetscFunctionReturn(0);
      }
      ierr = VecNormBegin(g,NORM_2,gnorm);CHKERRQ(ierr);
      ierr = VecNormBegin(y,NORM_2,ynorm);CHKERRQ(ierr);
      ierr = VecNormEnd(g,NORM_2,gnorm);CHKERRQ(ierr);
      ierr = VecNormEnd(y,NORM_2,ynorm);CHKERRQ(ierr);
      if PetscIsInfOrNanReal(*gnorm) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_FP,"User provided compute function generated a Not-a-Number");
    }
  }
  ierr = PetscLogEventEnd(SNES_LineSearch,snes,x,f,g);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

typedef PetscErrorCode (*FCN2)(SNES,void*,Vec,Vec,Vec,Vec,Vec,PetscReal,PetscReal,PetscReal*,PetscReal*,PetscTruth*); /* force argument to next function to not be extern C*/
/* -------------------------------------------------------------------------- */
EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "SNESLineSearchSet_LSVI"
PetscErrorCode PETSCSNES_DLLEXPORT SNESLineSearchSet_LSVI(SNES snes,FCN2 func,void *lsctx)
{
  PetscFunctionBegin;
  ((SNES_LSVI *)(snes->data))->LineSearch = func;
  ((SNES_LSVI *)(snes->data))->lsP        = lsctx;
  PetscFunctionReturn(0);
}
EXTERN_C_END

/* -------------------------------------------------------------------------- */
EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "SNESLineSearchSetMonitor_LSVI"
PetscErrorCode PETSCSNES_DLLEXPORT SNESLineSearchSetMonitor_LSVI(SNES snes,PetscTruth flg)
{
  SNES_LSVI        *lsvi = (SNES_LSVI*)snes->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (flg && !lsvi->monitor) {
    ierr = PetscViewerASCIIMonitorCreate(((PetscObject)snes)->comm,"stdout",((PetscObject)snes)->tablevel,&lsvi->monitor);CHKERRQ(ierr);
  } else if (!flg && lsvi->monitor) {
    ierr = PetscViewerASCIIMonitorDestroy(lsvi->monitor);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}
EXTERN_C_END

/*
   SNESView_LSVI - Prints info from the SNESLSVI data structure.

   Input Parameters:
.  SNES - the SNES context
.  viewer - visualization context

   Application Interface Routine: SNESView()
*/
#undef __FUNCT__  
#define __FUNCT__ "SNESView_LSVI"
static PetscErrorCode SNESView_LSVI(SNES snes,PetscViewer viewer)
{
  SNES_LSVI        *lsvi = (SNES_LSVI *)snes->data;
  const char     *cstr;
  PetscErrorCode ierr;
  PetscTruth     iascii;

  PetscFunctionBegin;
  ierr = PetscTypeCompare((PetscObject)viewer,PETSCVIEWERASCII,&iascii);CHKERRQ(ierr);
  if (iascii) {
    if (lsvi->LineSearch == SNESLineSearchNo_LSVI)             cstr = "SNESLineSearchNo";
    else if (lsvi->LineSearch == SNESLineSearchQuadratic_LSVI) cstr = "SNESLineSearchQuadratic";
    else if (lsvi->LineSearch == SNESLineSearchCubic_LSVI)     cstr = "SNESLineSearchCubic";
    else                                                cstr = "unknown";
    ierr = PetscViewerASCIIPrintf(viewer,"  line search variant: %s\n",cstr);CHKERRQ(ierr);
    ierr = PetscViewerASCIIPrintf(viewer,"  alpha=%G, maxstep=%G, minlambda=%G\n",lsvi->alpha,lsvi->maxstep,lsvi->minlambda);CHKERRQ(ierr);
  } else {
    SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_SUP,"Viewer type %s not supported for SNES EQ LSVI",((PetscObject)viewer)->type_name);
  }
  PetscFunctionReturn(0);
}

/*
   SNESLSVISetVariableBounds - Sets the lower and upper bounds for the solution vector. xl <= x <= xu.

   Input Parameters:
.  snes - the SNES context.
.  xl   - lower bound.
.  xu   - upper bound.

   Notes:
   If this routine is not called then the lower and upper bounds are set to 
   -Infinity and Infinity respectively during SNESSetUp.
*/

#undef __FUNCT__
#define __FUNCT__ "SNESLSVISetVariableBounds"
PetscErrorCode SNESLSVISetVariableBounds(SNES snes, Vec xl, Vec xu)
{
  SNES_LSVI        *lsvi = (SNES_LSVI*)snes->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(snes,SNES_CLASSID,1);
  PetscValidHeaderSpecific(xl,VEC_CLASSID,2);
  PetscValidHeaderSpecific(xu,VEC_CLASSID,3);

  /* Check if SNESSetFunction is called */
  if(!snes->vec_func) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_ARG_WRONGSTATE,"Must call SNESSetFunction() first");

  /* Check if the vector sizes are compatible for lower and upper bounds */
  if (xl->map->N != snes->vec_func->map->N) SETERRQ2(PETSC_COMM_SELF,PETSC_ERR_ARG_INCOMP,"Incompatible vector lengths lower bound = %D solution vector = %D",xl->map->N,snes->vec_func->map->N);
  if (xu->map->N != snes->vec_func->map->N) SETERRQ2(PETSC_COMM_SELF,PETSC_ERR_ARG_INCOMP,"Incompatible vector lengths: upper bound = %D solution vector = %D",xu->map->N,snes->vec_func->map->N);
  lsvi->usersetxbounds = PETSC_TRUE;
  lsvi->xl = xl;
  lsvi->xu = xu;

  PetscFunctionReturn(0);
}
/* -------------------------------------------------------------------------- */
/*
   SNESSetFromOptions_LSVI - Sets various parameters for the SNESLSVI method.

   Input Parameter:
.  snes - the SNES context

   Application Interface Routine: SNESSetFromOptions()
*/
#undef __FUNCT__  
#define __FUNCT__ "SNESSetFromOptions_LSVI"
static PetscErrorCode SNESSetFromOptions_LSVI(SNES snes)
{
  SNES_LSVI      *lsvi = (SNES_LSVI *)snes->data;
  const char     *lses[] = {"basic","basicnonorms","quadratic","cubic"};
  PetscErrorCode ierr;
  PetscInt       indx;
  PetscTruth     flg,set;

  PetscFunctionBegin;
  ierr = PetscOptionsHead("SNES semismooth method options");CHKERRQ(ierr);
    ierr = PetscOptionsReal("-snes_lsvi_alpha","Function norm must decrease by","None",lsvi->alpha,&lsvi->alpha,0);CHKERRQ(ierr);
    ierr = PetscOptionsReal("-snes_lsvi_maxstep","Step must be less than","None",lsvi->maxstep,&lsvi->maxstep,0);CHKERRQ(ierr);
    ierr = PetscOptionsReal("-snes_lsvi_minlambda","Minimum lambda allowed","None",lsvi->minlambda,&lsvi->minlambda,0);CHKERRQ(ierr);
    ierr = PetscOptionsReal("-snes_lsvi_delta","descent test fraction","None",lsvi->delta,&lsvi->delta,0);CHKERRQ(ierr);
    ierr = PetscOptionsReal("-snes_lsvi_rho","descent test power","None",lsvi->rho,&lsvi->rho,0);CHKERRQ(ierr);
    ierr = PetscOptionsTruth("-snes_lsvi_monitor","Print progress of line searches","SNESLineSearchSetMonitor",lsvi->monitor ? PETSC_TRUE : PETSC_FALSE,&flg,&set);CHKERRQ(ierr);
    if (set) {ierr = SNESLineSearchSetMonitor(snes,flg);CHKERRQ(ierr);}

    ierr = PetscOptionsEList("-snes_lsvi","Line search used","SNESLineSearchSet",lses,4,"cubic",&indx,&flg);CHKERRQ(ierr);
    if (flg) {
      switch (indx) {
      case 0:
        ierr = SNESLineSearchSet(snes,SNESLineSearchNo_LSVI,PETSC_NULL);CHKERRQ(ierr);
        break;
      case 1:
        ierr = SNESLineSearchSet(snes,SNESLineSearchNoNorms_LSVI,PETSC_NULL);CHKERRQ(ierr);
        break;
      case 2:
        ierr = SNESLineSearchSet(snes,SNESLineSearchQuadratic_LSVI,PETSC_NULL);CHKERRQ(ierr);
        break;
      case 3:
        ierr = SNESLineSearchSet(snes,SNESLineSearchCubic_LSVI,PETSC_NULL);CHKERRQ(ierr);
        break;
      }
    }
  ierr = PetscOptionsTail();CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
/* -------------------------------------------------------------------------- */
/*MC
      SNESLSVI - Semismooth newton method based nonlinear solver that uses a line search

   Options Database:
+   -snes_lsvi [cubic,quadratic,basic,basicnonorms] - Selects line search
.   -snes_lsvi_alpha <alpha> - Sets alpha
.   -snes_lsvi_maxstep <maxstep> - Sets the maximum stepsize the line search will use (if the 2-norm(y) > maxstep then scale y to be y = (maxstep/2-norm(y)) *y)
.   -snes_lsvi_minlambda <minlambda>  - Sets the minimum lambda the line search will use  minlambda / max_i ( y[i]/x[i] )
    -snes_lsvi_delta <delta> - Sets the fraction used in the descent test.
    -snes_lsvi_rho <rho> - Sets the power used in the descent test.
     For a descent direction to be accepted it has to satisfy the condition dpsi^T*d <= -delta*||d||^rho
-   -snes_lsvi_monitor - print information about progress of line searches 


   Level: beginner

.seealso:  SNESCreate(), SNES, SNESSetType(), SNESTR, SNESLineSearchSet(), 
           SNESLineSearchSetPostCheck(), SNESLineSearchNo(), SNESLineSearchCubic(), SNESLineSearchQuadratic(), 
           SNESLineSearchSet(), SNESLineSearchNoNorms(), SNESLineSearchSetPreCheck(), SNESLineSearchSetParams(), SNESLineSearchGetParams()

M*/
EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "SNESCreate_LSVI"
PetscErrorCode PETSCSNES_DLLEXPORT SNESCreate_LSVI(SNES snes)
{
  PetscErrorCode ierr;
  SNES_LSVI      *lsvi;

  PetscFunctionBegin;
  snes->ops->setup	     = SNESSetUp_LSVI;
  snes->ops->solve	     = SNESSolve_LSVI;
  snes->ops->destroy	     = SNESDestroy_LSVI;
  snes->ops->setfromoptions  = SNESSetFromOptions_LSVI;
  snes->ops->view            = SNESView_LSVI;
  snes->ops->converged       = SNESDefaultConverged_LSVI;

  ierr                   = PetscNewLog(snes,SNES_LSVI,&lsvi);CHKERRQ(ierr);
  snes->data    	 = (void*)lsvi;
  lsvi->alpha		 = 1.e-4;
  lsvi->maxstep		 = 1.e8;
  lsvi->minlambda         = 1.e-12;
  lsvi->LineSearch        = SNESLineSearchCubic_LSVI;
  lsvi->lsP               = PETSC_NULL;
  lsvi->postcheckstep     = PETSC_NULL;
  lsvi->postcheck         = PETSC_NULL;
  lsvi->precheckstep      = PETSC_NULL;
  lsvi->precheck          = PETSC_NULL;
  lsvi->rho               = 2.1;
  lsvi->delta             = 1e-10;
  lsvi->computessfunction = ComputeFischerFunction; 

  ierr = PetscObjectComposeFunctionDynamic((PetscObject)snes,"SNESLineSearchSetMonitor_C","SNESLineSearchSetMonitor_LSVI",SNESLineSearchSetMonitor_LSVI);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)snes,"SNESLineSearchSet_C","SNESLineSearchSet_LSVI",SNESLineSearchSet_LSVI);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}
EXTERN_C_END
