/* $Id: fgmres.c,v 1.29 2001/08/07 21:30:49 bsmith Exp $ */

/*
    This file implements FGMRES (a Generalized Minimal Residual) method.  
    Reference:  Saad, 1993.

    Preconditioning:  It the preconditioner is constant then this fgmres
    code is equivalent to RIGHT-PRECONDITIONED GMRES.

    Restarts:  Restarts are basically solves with x0 not equal to zero.
 
       Contributed by Allison Baker

*/

#include "src/ksp/ksp/impls/fgmres/fgmresp.h"       /*I  "petscksp.h"  I*/
#define FGMRES_DELTA_DIRECTIONS 10
#define FGMRES_DEFAULT_MAXK     30
static int    FGMRESGetNewVectors(KSP,int);
static int    FGMRESUpdateHessenberg(KSP,int,PetscTruth,PetscReal *);
static int    BuildFgmresSoln(PetscScalar*,Vec,Vec,KSP,int);

extern int KSPView_GMRES(KSP,PetscViewer);
/*

    KSPSetUp_FGMRES - Sets up the workspace needed by fgmres.

    This is called once, usually automatically by KSPSolveQ() or KSPSetUp(),
    but can be called directly by KSPSetUp().

*/
#undef __FUNCT__  
#define __FUNCT__ "KSPSetUp_FGMRES"
int    KSPSetUp_FGMRES(KSP ksp)
{
  unsigned  int size,hh,hes,rs,cc;
  int           ierr,max_k,k;
  KSP_FGMRES    *fgmres = (KSP_FGMRES *)ksp->data;

  PetscFunctionBegin;
  if (ksp->pc_side == PC_SYMMETRIC) {
    SETERRQ(2,"no symmetric preconditioning for KSPFGMRES");
  } else if (ksp->pc_side == PC_LEFT) {
    SETERRQ(2,"no left preconditioning for KSPFGMRES");
  }
  max_k         = fgmres->max_k;
  hh            = (max_k + 2) * (max_k + 1);
  hes           = (max_k + 1) * (max_k + 1);
  rs            = (max_k + 2);
  cc            = (max_k + 1);  /* SS and CC are the same size */
  size          = (hh + hes + rs + 2*cc) * sizeof(PetscScalar);

  /* Allocate space and set pointers to beginning */
  ierr = PetscMalloc(size,&fgmres->hh_origin);CHKERRQ(ierr);
  ierr = PetscMemzero(fgmres->hh_origin,size);CHKERRQ(ierr);
  PetscLogObjectMemory(ksp,size);                      /* HH - modified (by plane 
                                                     rotations) hessenburg */
  fgmres->hes_origin = fgmres->hh_origin + hh;     /* HES - unmodified hessenburg */
  fgmres->rs_origin  = fgmres->hes_origin + hes;   /* RS - the right-hand-side of the 
                                                      Hessenberg system */
  fgmres->cc_origin  = fgmres->rs_origin + rs;     /* CC - cosines for rotations */
  fgmres->ss_origin  = fgmres->cc_origin + cc;     /* SS - sines for rotations */

  if (ksp->calc_sings) {
    /* Allocate workspace to hold Hessenberg matrix needed by Eispack */
    size = (max_k + 3)*(max_k + 9)*sizeof(PetscScalar);
    ierr = PetscMalloc(size,&fgmres->Rsvd);CHKERRQ(ierr);
    ierr = PetscMalloc(5*(max_k+2)*sizeof(PetscReal),&fgmres->Dsvd);CHKERRQ(ierr);
    PetscLogObjectMemory(ksp,size+5*(max_k+2)*sizeof(PetscReal));
  }

  /* Allocate array to hold pointers to user vectors.  Note that we need
   4 + max_k + 1 (since we need it+1 vectors, and it <= max_k) */
  ierr = PetscMalloc((VEC_OFFSET+2+max_k)*sizeof(void *),&fgmres->vecs);CHKERRQ(ierr);
  fgmres->vecs_allocated = VEC_OFFSET + 2 + max_k;
  ierr = PetscMalloc((VEC_OFFSET+2+max_k)*sizeof(void *),&fgmres->user_work);CHKERRQ(ierr);
  ierr = PetscMalloc((VEC_OFFSET+2+max_k)*sizeof(int),&fgmres->mwork_alloc);CHKERRQ(ierr);
  PetscLogObjectMemory(ksp,(VEC_OFFSET+2+max_k)*(2*sizeof(void *)+sizeof(int)));

  /* New for FGMRES - Allocate array to hold pointers to preconditioned 
     vectors - same sizes as user vectors above */
  ierr = PetscMalloc((VEC_OFFSET+2+max_k)*sizeof(void *),&fgmres->prevecs);CHKERRQ(ierr);
  ierr = PetscMalloc((VEC_OFFSET+2+max_k)*sizeof(void *),&fgmres->prevecs_user_work);CHKERRQ(ierr);
  PetscLogObjectMemory(ksp,(VEC_OFFSET+2+max_k)*(2*sizeof(void *)));


  /* if q_preallocate = 0 then only allocate one "chunck" of space (for 
     5 vectors) - additional will then be allocated from FGMREScycle() 
     as needed.  Otherwise, allocate all of the space that could be needed */
  if (fgmres->q_preallocate) {
    fgmres->vv_allocated   = VEC_OFFSET + 2 + max_k;
  } else {
    fgmres->vv_allocated    = 5;
  }

  /* space for work vectors */
  ierr = VecDuplicateVecs(VEC_RHS,fgmres->vv_allocated,&fgmres->user_work[0]);CHKERRQ(ierr);
  PetscLogObjectParents(ksp,fgmres->vv_allocated,fgmres->user_work[0]);
  for (k=0; k < fgmres->vv_allocated; k++) {
    fgmres->vecs[k] = fgmres->user_work[0][k];
  } 

  /* space for preconditioned vectors */
  ierr = VecDuplicateVecs(VEC_RHS,fgmres->vv_allocated,&fgmres->prevecs_user_work[0]);CHKERRQ(ierr);
  PetscLogObjectParents(ksp,fgmres->vv_allocated,fgmres->prevecs_user_work[0]);
  for (k=0; k < fgmres->vv_allocated; k++) {
    fgmres->prevecs[k] = fgmres->prevecs_user_work[0][k];
  } 

  /* specify how many work vectors have been allocated in this 
     chunck" (the first one) */
  fgmres->mwork_alloc[0] = fgmres->vv_allocated;
  fgmres->nwork_alloc    = 1;

  PetscFunctionReturn(0);
}

/* 
    FGMRESResidual - This routine computes the initial residual (NOT PRECONDITIONED) 
*/
#undef __FUNCT__  
#define __FUNCT__ "FGMRESResidual"
static int FGMRESResidual(KSP ksp)
{
  KSP_FGMRES   *fgmres = (KSP_FGMRES *)(ksp->data);
  PetscScalar  mone = -1.0;
  Mat          Amat,Pmat;
  MatStructure pflag;
  int          ierr;

  PetscFunctionBegin;
  ierr = PCGetOperators(ksp->B,&Amat,&Pmat,&pflag);CHKERRQ(ierr);

  /* put A*x into VEC_TEMP */
  ierr = MatMult(Amat,VEC_SOLN,VEC_TEMP);CHKERRQ(ierr);
  /* now put residual (-A*x + f) into vec_vv(0) */
  ierr = VecWAXPY(&mone,VEC_TEMP,VEC_RHS,VEC_VV(0));CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*

    FGMRESCycle - Run fgmres, possibly with restart.  Return residual 
                  history if requested.

    input parameters:
.	 fgmres  - structure containing parameters and work areas

    output parameters:
.        itcount - number of iterations used.  If null, ignored.
.        converged - 0 if not converged

		  
    Notes:
    On entry, the value in vector VEC_VV(0) should be 
    the initial residual.


 */
#undef __FUNCT__  
#define __FUNCT__ "FGMREScycle"
int FGMREScycle(int *itcount,KSP ksp)
{

  KSP_FGMRES   *fgmres = (KSP_FGMRES *)(ksp->data);
  PetscReal    res_norm;             
  PetscReal    hapbnd,tt;
  PetscScalar  zero = 0.0;
  PetscScalar  tmp;
  PetscTruth   hapend = PETSC_FALSE;  /* indicates happy breakdown ending */
  int          ierr;
  int          loc_it;                /* local count of # of dir. in Krylov space */ 
  int          max_k = fgmres->max_k; /* max # of directions Krylov space */
  Mat          Amat,Pmat;
  MatStructure pflag;

  PetscFunctionBegin;

  /* Number of pseudo iterations since last restart is the number 
     of prestart directions */
  loc_it = 0;

  /* initial residual is in VEC_VV(0)  - compute its norm*/ 
  ierr   = VecNorm(VEC_VV(0),NORM_2,&res_norm);CHKERRQ(ierr);

  /* first entry in right-hand-side of hessenberg system is just 
     the initial residual norm */
  *RS(0) = res_norm;

  /* FYI: AMS calls are for memory snooper */
  ierr = PetscObjectTakeAccess(ksp);CHKERRQ(ierr);
  ksp->rnorm = res_norm;
  ierr = PetscObjectGrantAccess(ksp);CHKERRQ(ierr);
  KSPLogResidualHistory(ksp,res_norm);

  /* check for the convergence - maybe the current guess is good enough */
  ierr = (*ksp->converged)(ksp,ksp->its,res_norm,&ksp->reason,ksp->cnvP);CHKERRQ(ierr);
  if (ksp->reason) {
    if (itcount) *itcount = 0;
    PetscFunctionReturn(0);
  }

  /* scale VEC_VV (the initial residual) */
  tmp = 1.0/res_norm; ierr = VecScale(&tmp,VEC_VV(0));CHKERRQ(ierr);



  /* note: (fgmres->it) is always set one less than (loc_it) It is used in 
     KSPBUILDSolution_FGMRES, where it is passed to BuildFGmresSoln.  
     Note that when BuildFGmresSoln is called from this function, 
     (loc_it -1) is passed, so the two are equivalent */
  fgmres->it = (loc_it - 1);
   
  /* MAIN ITERATION LOOP BEGINNING*/
  /* keep iterating until we have converged OR generated the max number
     of directions OR reached the max number of iterations for the method */ 
  ierr = (*ksp->converged)(ksp,ksp->its,res_norm,&ksp->reason,ksp->cnvP);CHKERRQ(ierr);
  while (!ksp->reason && loc_it < max_k && ksp->its < ksp->max_it) {
    KSPLogResidualHistory(ksp,res_norm);
    fgmres->it = (loc_it - 1);
    KSPMonitor(ksp,ksp->its,res_norm); 

    /* see if more space is needed for work vectors */
    if (fgmres->vv_allocated <= loc_it + VEC_OFFSET + 1) {
      ierr = FGMRESGetNewVectors(ksp,loc_it+1);CHKERRQ(ierr);
      /* (loc_it+1) is passed in as number of the first vector that should
         be allocated */
    }

    /* CHANGE THE PRECONDITIONER? */ 
    /* ModifyPC is the callback function that can be used to
       change the PC or its attributes before its applied */
    (*fgmres->modifypc)(ksp,ksp->its,loc_it,res_norm,fgmres->modifyctx);
   
  
    /* apply PRECONDITIONER to direction vector and store with 
       preconditioned vectors in prevec */
    ierr = PCApply(ksp->B,VEC_VV(loc_it),PREVEC(loc_it),PC_RIGHT);CHKERRQ(ierr);
     
    ierr = PCGetOperators(ksp->B,&Amat,&Pmat,&pflag);CHKERRQ(ierr);
    /* Multiply preconditioned vector by operator - put in VEC_VV(loc_it+1) */
    ierr = MatMult(Amat,PREVEC(loc_it),VEC_VV(1+loc_it));CHKERRQ(ierr);

 
    /* update hessenberg matrix and do Gram-Schmidt - new direction is in
       VEC_VV(1+loc_it)*/
    ierr = (*fgmres->orthog)(ksp,loc_it);CHKERRQ(ierr);

    /* new entry in hessenburg is the 2-norm of our new direction */
    ierr = VecNorm(VEC_VV(loc_it+1),NORM_2,&tt);CHKERRQ(ierr);
    *HH(loc_it+1,loc_it)   = tt;
    *HES(loc_it+1,loc_it)  = tt;

    /* Happy Breakdown Check */
    hapbnd  = PetscAbsScalar((tt) / *RS(loc_it));
    /* RS(loc_it) contains the res_norm from the last iteration  */
    hapbnd = PetscMin(fgmres->haptol,hapbnd);
    if (tt > hapbnd) {
        tmp = 1.0/tt; 
        /* scale new direction by its norm */
        ierr = VecScale(&tmp,VEC_VV(loc_it+1));CHKERRQ(ierr);
    } else {
        /* This happens when the solution is exactly reached. */
        /* So there is no new direction... */
          ierr   = VecSet(&zero,VEC_TEMP);CHKERRQ(ierr); /* set VEC_TEMP to 0 */
          hapend = PETSC_TRUE;
    }
    /* note that for FGMRES we could get HES(loc_it+1, loc_it)  = 0 and the
       current solution would not be exact if HES was singular.  Note that 
       HH non-singular implies that HES is no singular, and HES is guaranteed
       to be nonsingular when PREVECS are linearly independent and A is 
       nonsingular (in GMRES, the nonsingularity of A implies the nonsingularity 
       of HES). So we should really add a check to verify that HES is nonsingular.*/

 
    /* Now apply rotations to new col of hessenberg (and right side of system), 
       calculate new rotation, and get new residual norm at the same time*/
    ierr = FGMRESUpdateHessenberg(ksp,loc_it,hapend,&res_norm);CHKERRQ(ierr);
    loc_it++;
    fgmres->it  = (loc_it-1);  /* Add this here in case it has converged */
 
    ierr = PetscObjectTakeAccess(ksp);CHKERRQ(ierr);
    ksp->its++;
    ksp->rnorm = res_norm;
    ierr = PetscObjectGrantAccess(ksp);CHKERRQ(ierr);

    ierr = (*ksp->converged)(ksp,ksp->its,res_norm,&ksp->reason,ksp->cnvP);CHKERRQ(ierr);

    /* Catch error in happy breakdown and signal convergence and break from loop */
    if (hapend) {
      if (!ksp->reason) {
        SETERRQ(0,"You reached the happy break down,but convergence was not indicated.");
      }
      break;
    }
  }
  /* END OF ITERATION LOOP */

  KSPLogResidualHistory(ksp,res_norm);

  /*
     Monitor if we know that we will not return for a restart */
  if (ksp->reason || ksp->its >= ksp->max_it) {
    KSPMonitor(ksp,ksp->its,res_norm);
  }

  if (itcount) *itcount    = loc_it;

  /*
    Down here we have to solve for the "best" coefficients of the Krylov
    columns, add the solution values together, and possibly unwind the
    preconditioning from the solution
   */
 
  /* Form the solution (or the solution so far) */
  /* Note: must pass in (loc_it-1) for iteration count so that BuildFgmresSoln
     properly navigates */

  ierr = BuildFgmresSoln(RS(0),VEC_SOLN,VEC_SOLN,ksp,loc_it-1);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}

/*  
    KSPSolve_FGMRES - This routine applies the FGMRES method.


   Input Parameter:
.     ksp - the Krylov space object that was set to use fgmres

   Output Parameter:
.     outits - number of iterations used

*/
#undef __FUNCT__  
#define __FUNCT__ "KSPSolve_FGMRES"

int KSPSolve_FGMRES(KSP ksp)
{
  int        ierr;
  int        cycle_its; /* iterations done in a call to FGMREScycle */
  int        itcount;   /* running total of iterations, incl. those in restarts */
  KSP_FGMRES *fgmres = (KSP_FGMRES *)ksp->data;
  PetscTruth diagonalscale;

  PetscFunctionBegin;
  ierr    = PCDiagonalScale(ksp->B,&diagonalscale);CHKERRQ(ierr);
  if (diagonalscale) SETERRQ1(1,"Krylov method %s does not support diagonal scaling",ksp->type_name);

  ierr = PetscObjectTakeAccess(ksp);CHKERRQ(ierr);
  ksp->its = 0;
  ierr = PetscObjectGrantAccess(ksp);CHKERRQ(ierr);

  /* initialize */
  itcount  = 0;

  /* Compute the initial (NOT preconditioned) residual */
  if (!ksp->guess_zero) {
    ierr = FGMRESResidual(ksp);CHKERRQ(ierr);
  } else { /* guess is 0 so residual is F (which is in VEC_RHS) */
    ierr = VecCopy(VEC_RHS,VEC_VV(0));CHKERRQ(ierr);
  }
  /* now the residual is in VEC_VV(0) - which is what 
     FGMREScycle expects... */
  
  ierr    = FGMREScycle(&cycle_its,ksp);CHKERRQ(ierr);
  itcount += cycle_its;
  while (!ksp->reason) {
    ierr     = FGMRESResidual(ksp);CHKERRQ(ierr);
    if (itcount >= ksp->max_it) break;
    ierr     = FGMREScycle(&cycle_its,ksp);CHKERRQ(ierr);
    itcount += cycle_its;  
  }
  /* mark lack of convergence */
  if (itcount >= ksp->max_it) ksp->reason = KSP_DIVERGED_ITS;

  PetscFunctionReturn(0);
}

/*

   KSPDestroy_FGMRES - Frees all memory space used by the Krylov method.

*/
#undef __FUNCT__  
#define __FUNCT__ "KSPDestroy_FGMRES" 
int KSPDestroy_FGMRES(KSP ksp)
{
  KSP_FGMRES *fgmres = (KSP_FGMRES*)ksp->data;
  int       i,ierr;

  PetscFunctionBegin;
  /* Free the Hessenberg matrices */
  if (fgmres->hh_origin) {ierr = PetscFree(fgmres->hh_origin);CHKERRQ(ierr);}

  /* Free pointers to user variables */
  if (fgmres->vecs) {ierr = PetscFree(fgmres->vecs);CHKERRQ(ierr);}
  if (fgmres->prevecs) {ierr = PetscFree (fgmres->prevecs);CHKERRQ(ierr);}

  /* free work vectors */
  for (i=0; i < fgmres->nwork_alloc; i++) {
    ierr = VecDestroyVecs(fgmres->user_work[i],fgmres->mwork_alloc[i]);CHKERRQ(ierr);
  }
  if (fgmres->user_work)  {ierr = PetscFree(fgmres->user_work);CHKERRQ(ierr);}

  for (i=0; i < fgmres->nwork_alloc; i++) {
    ierr = VecDestroyVecs(fgmres->prevecs_user_work[i],fgmres->mwork_alloc[i]);CHKERRQ(ierr);
  }
  if (fgmres->prevecs_user_work) {ierr = PetscFree(fgmres->prevecs_user_work);CHKERRQ(ierr);}

  if (fgmres->mwork_alloc) {ierr = PetscFree(fgmres->mwork_alloc);CHKERRQ(ierr);}
  if (fgmres->nrs) {ierr = PetscFree(fgmres->nrs);CHKERRQ(ierr);}
  if (fgmres->sol_temp) {ierr = VecDestroy(fgmres->sol_temp);CHKERRQ(ierr);}
  if (fgmres->Rsvd) {ierr = PetscFree(fgmres->Rsvd);CHKERRQ(ierr);}
  if (fgmres->Dsvd) {ierr = PetscFree(fgmres->Dsvd);CHKERRQ(ierr);}
  if (fgmres->modifydestroy) {
    ierr = (*fgmres->modifydestroy)(fgmres->modifyctx);CHKERRQ(ierr);
  }
  ierr = PetscFree(fgmres);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*
    BuildFgmresSoln - create the solution from the starting vector and the
                      current iterates.

    Input parameters:
        nrs - work area of size it + 1.
	vguess  - index of initial guess
	vdest - index of result.  Note that vguess may == vdest (replace
	        guess with the solution).
        it - HH upper triangular part is a block of size (it+1) x (it+1)  

     This is an internal routine that knows about the FGMRES internals.
 */
#undef __FUNCT__  
#define __FUNCT__ "BuildFgmresSoln"
static int BuildFgmresSoln(PetscScalar* nrs,Vec vguess,Vec vdest,KSP ksp,int it)
{
  PetscScalar  tt,zero = 0.0,one = 1.0;
  int          ierr,ii,k,j;
  KSP_FGMRES   *fgmres = (KSP_FGMRES *)(ksp->data);

  PetscFunctionBegin;
  /* Solve for solution vector that minimizes the residual */

  /* If it is < 0, no fgmres steps have been performed */
  if (it < 0) {
    if (vdest != vguess) {
      ierr = VecCopy(vguess,vdest);CHKERRQ(ierr);
    }
    PetscFunctionReturn(0);
  }

  /* so fgmres steps HAVE been performed */

  /* solve the upper triangular system - RS is the right side and HH is 
     the upper triangular matrix  - put soln in nrs */
  nrs[it] = *RS(it) / *HH(it,it);
  for (ii=1; ii<=it; ii++) {
    k   = it - ii;
    tt  = *RS(k);
    for (j=k+1; j<=it; j++) tt  = tt - *HH(k,j) * nrs[j];
    nrs[k]   = tt / *HH(k,k);
  }

  /* Accumulate the correction to the soln of the preconditioned prob. in 
     VEC_TEMP - note that we use the preconditioned vectors  */
  ierr = VecSet(&zero,VEC_TEMP);CHKERRQ(ierr); /* set VEC_TEMP components to 0 */
  ierr = VecMAXPY(it+1,nrs,VEC_TEMP,&PREVEC(0));CHKERRQ(ierr); 

  /* put updated solution into vdest.*/
  if (vdest != vguess) {
    ierr = VecCopy(VEC_TEMP,vdest);CHKERRQ(ierr);
    ierr = VecAXPY(&one,vguess,vdest);CHKERRQ(ierr);
  } else  {/* replace guess with solution */
    ierr = VecAXPY(&one,VEC_TEMP,vdest);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

/*

    FGMRESUpdateHessenberg - Do the scalar work for the orthogonalization.  
                            Return new residual.

    input parameters:

.        ksp -    Krylov space object
.	 it  -    plane rotations are applied to the (it+1)th column of the 
                  modified hessenberg (i.e. HH(:,it))
.        hapend - PETSC_FALSE not happy breakdown ending.

    output parameters:
.        res - the new residual
	
 */
#undef __FUNCT__  
#define __FUNCT__ "FGMRESUpdateHessenberg"
static int FGMRESUpdateHessenberg(KSP ksp,int it,PetscTruth hapend,PetscReal *res)
{
  PetscScalar   *hh,*cc,*ss,tt;
  int           j;
  KSP_FGMRES    *fgmres = (KSP_FGMRES *)(ksp->data);

  PetscFunctionBegin;
  hh  = HH(0,it);  /* pointer to beginning of column to update - so 
                      incrementing hh "steps down" the (it+1)th col of HH*/ 
  cc  = CC(0);     /* beginning of cosine rotations */ 
  ss  = SS(0);     /* beginning of sine rotations */

  /* Apply all the previously computed plane rotations to the new column
     of the Hessenberg matrix */
  /* Note: this uses the rotation [conj(c)  s ; -s   c], c= cos(theta), s= sin(theta),
     and some refs have [c   s ; -conj(s)  c] (don't be confused!) */

  for (j=1; j<=it; j++) {
    tt  = *hh;
#if defined(PETSC_USE_COMPLEX)
    *hh = PetscConj(*cc) * tt + *ss * *(hh+1);
#else
    *hh = *cc * tt + *ss * *(hh+1);
#endif
    hh++;
    *hh = *cc++ * *hh - (*ss++ * tt);
    /* hh, cc, and ss have all been incremented one by end of loop */
  }

  /*
    compute the new plane rotation, and apply it to:
     1) the right-hand-side of the Hessenberg system (RS)
        note: it affects RS(it) and RS(it+1)
     2) the new column of the Hessenberg matrix
        note: it affects HH(it,it) which is currently pointed to 
        by hh and HH(it+1, it) (*(hh+1))  
    thus obtaining the updated value of the residual...
  */

  /* compute new plane rotation */

  if (!hapend) {
#if defined(PETSC_USE_COMPLEX)
    tt        = PetscSqrtScalar(PetscConj(*hh) * *hh + PetscConj(*(hh+1)) * *(hh+1));
#else
    tt        = PetscSqrtScalar(*hh * *hh + *(hh+1) * *(hh+1));
#endif
    if (tt == 0.0) {SETERRQ(PETSC_ERR_KSP_BRKDWN,"Your matrix or preconditioner is the null operator");}
    *cc       = *hh / tt;   /* new cosine value */
    *ss       = *(hh+1) / tt;  /* new sine value */

    /* apply to 1) and 2) */
    *RS(it+1) = - (*ss * *RS(it));
#if defined(PETSC_USE_COMPLEX)
    *RS(it)   = PetscConj(*cc) * *RS(it);
    *hh       = PetscConj(*cc) * *hh + *ss * *(hh+1);
#else
    *RS(it)   = *cc * *RS(it);
    *hh       = *cc * *hh + *ss * *(hh+1);
#endif

    /* residual is the last element (it+1) of right-hand side! */
    *res      = PetscAbsScalar(*RS(it+1));

  } else { /* happy breakdown: HH(it+1, it) = 0, therfore we don't need to apply 
            another rotation matrix (so RH doesn't change).  The new residual is 
            always the new sine term times the residual from last time (RS(it)), 
            but now the new sine rotation would be zero...so the residual should
            be zero...so we will multiply "zero" by the last residual.  This might
            not be exactly what we want to do here -could just return "zero". */
 
    *res = 0.0;
  }
  PetscFunctionReturn(0);
}

/*

   FGMRESGetNewVectors - This routine allocates more work vectors, starting from 
                         VEC_VV(it), and more preconditioned work vectors, starting 
                         from PREVEC(i).

*/
#undef __FUNCT__  
#define __FUNCT__ "FGMRESGetNewVectors" 
static int FGMRESGetNewVectors(KSP ksp,int it)
{
  KSP_FGMRES *fgmres = (KSP_FGMRES *)ksp->data;
  int        nwork = fgmres->nwork_alloc; /* number of work vector chunks allocated */
  int        nalloc;                      /* number to allocate */
  int        k,ierr;
 
  PetscFunctionBegin;
  nalloc = fgmres->delta_allocate; /* number of vectors to allocate 
                                      in a single chunk */

  /* Adjust the number to allocate to make sure that we don't exceed the
     number of available slots (fgmres->vecs_allocated)*/
  if (it + VEC_OFFSET + nalloc >= fgmres->vecs_allocated){
    nalloc = fgmres->vecs_allocated - it - VEC_OFFSET;
  }
  if (!nalloc) PetscFunctionReturn(0);

  fgmres->vv_allocated += nalloc; /* vv_allocated is the number of vectors allocated */

  /* work vectors */
  ierr = VecDuplicateVecs(VEC_RHS,nalloc,&fgmres->user_work[nwork]);CHKERRQ(ierr);
  PetscLogObjectParents(ksp,nalloc,fgmres->user_work[nwork]); 
  for (k=0; k < nalloc; k++) {
    fgmres->vecs[it+VEC_OFFSET+k] = fgmres->user_work[nwork][k];
  }
  /* specify size of chunk allocated */
  fgmres->mwork_alloc[nwork] = nalloc;

  /* preconditioned vectors */
  ierr = VecDuplicateVecs(VEC_RHS,nalloc,&fgmres->prevecs_user_work[nwork]);CHKERRQ(ierr);
  PetscLogObjectParents(ksp,nalloc,fgmres->prevecs_user_work[nwork]);CHKERRQ(ierr);
  for (k=0; k < nalloc; k++) {
    fgmres->prevecs[it+VEC_OFFSET+k] = fgmres->prevecs_user_work[nwork][k];
  } 

  /* increment the number of work vector chunks */
  fgmres->nwork_alloc++;
  PetscFunctionReturn(0);
}

/* 

   KSPBuildSolution_FGMRES

     Input Parameter:
.     ksp - the Krylov space object
.     ptr-

   Output Parameter:
.     result - the solution

   Note: this calls BuildFgmresSoln - the same function that FGMREScycle
   calls directly.  

*/
#undef __FUNCT__  
#define __FUNCT__ "KSPBuildSolution_FGMRES"
int KSPBuildSolution_FGMRES(KSP ksp,Vec ptr,Vec *result)
{
  KSP_FGMRES *fgmres = (KSP_FGMRES *)ksp->data; 
  int        ierr;

  PetscFunctionBegin;
  if (!ptr) {
    if (!fgmres->sol_temp) {
      ierr = VecDuplicate(ksp->vec_sol,&fgmres->sol_temp);CHKERRQ(ierr);
      PetscLogObjectParent(ksp,fgmres->sol_temp);
    }
    ptr = fgmres->sol_temp;
  }
  if (!fgmres->nrs) {
    /* allocate the work area */
    ierr = PetscMalloc(fgmres->max_k*sizeof(PetscScalar),&fgmres->nrs);CHKERRQ(ierr);
    PetscLogObjectMemory(ksp,fgmres->max_k*sizeof(PetscScalar));
  }
 
  ierr = BuildFgmresSoln(fgmres->nrs,VEC_SOLN,ptr,ksp,fgmres->it);CHKERRQ(ierr);
  *result = ptr; 
  
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "KSPSetFromOptions_FGMRES"
int KSPSetFromOptions_FGMRES(KSP ksp)
{
  int         ierr,restart,indx;
  PetscReal   haptol;
  KSP_FGMRES *gmres = (KSP_FGMRES*)ksp->data;
  PetscTruth  flg;
  const char  *types[] = {"never","ifneeded","always"};

  PetscFunctionBegin;
  ierr = PetscOptionsHead("KSP flexible GMRES Options");CHKERRQ(ierr);
    ierr = PetscOptionsInt("-ksp_gmres_restart","Number of Krylov search directions","KSPGMRESSetRestart",gmres->max_k,&restart,&flg);CHKERRQ(ierr);
    if (flg) { ierr = KSPGMRESSetRestart(ksp,restart);CHKERRQ(ierr); }
    ierr = PetscOptionsReal("-ksp_gmres_haptol","Tolerance for declaring exact convergence (happy ending)","KSPGMRESSetHapTol",gmres->haptol,&haptol,&flg);CHKERRQ(ierr);
    if (flg) { ierr = KSPGMRESSetHapTol(ksp,haptol);CHKERRQ(ierr); }
    ierr = PetscOptionsName("-ksp_gmres_preallocate","Preallocate all Krylov vectors","KSPGMRESSetPreAllocateVectors",&flg);CHKERRQ(ierr);
    if (flg) {ierr = KSPGMRESSetPreAllocateVectors(ksp);CHKERRQ(ierr);}
    ierr = PetscOptionsLogicalGroupBegin("-ksp_gmres_classicalgramschmidt","Use classical (unmodified) Gram-Schmidt (fast)","KSPGMRESSetOrthogonalization",&flg);CHKERRQ(ierr);
    if (flg) {ierr = KSPGMRESSetOrthogonalization(ksp,KSPGMRESClassicalGramSchmidtOrthogonalization);CHKERRQ(ierr);}
    ierr = PetscOptionsLogicalGroup("-ksp_gmres_modifiedgramschmidt","Use modified Gram-Schmidt (slow but more stable)","KSPGMRESSetOrthogonalization",&flg);CHKERRQ(ierr);
    if (flg) {ierr = KSPGMRESSetOrthogonalization(ksp,KSPGMRESModifiedGramSchmidtOrthogonalization);CHKERRQ(ierr);}
    ierr = PetscOptionsEList("-ksp_gmres_cgs_refinement_type","Type of iterative refinement for classical (unmodified) Gram-Schmidt","KSPGMRESSetCGSRefinementType()",types,3,types[gmres->cgstype],&indx,&flg);CHKERRQ(ierr);    
    if (flg) {
      ierr = KSPGMRESSetCGSRefinementType(ksp,(KSPGMRESCGSRefinementType)indx);CHKERRQ(ierr);
    }
    ierr = PetscOptionsName("-ksp_gmres_krylov_monitor","Graphically plot the Krylov directions","KSPSetMonitor",&flg);CHKERRQ(ierr);
    if (flg) {
      PetscViewers viewers;
      ierr = PetscViewersCreate(ksp->comm,&viewers);CHKERRQ(ierr);
      ierr = KSPSetMonitor(ksp,KSPGMRESKrylovMonitor,viewers,(int (*)(void*))PetscViewersDestroy);CHKERRQ(ierr);
    }
    ierr = PetscOptionsLogicalGroupBegin("-ksp_fgmres_modifypcnochange","do not vary the preconditioner","KSPFGMRESSetModifyPC",&flg);CHKERRQ(ierr);
    if (flg) {ierr = KSPFGMRESSetModifyPC(ksp,KSPFGMRESModifyPCNoChange,0,0);CHKERRQ(ierr);} 
    ierr = PetscOptionsLogicalGroupEnd("-ksp_fgmres_modifypcksp","vary the KSP based preconditioner","KSPFGMRESSetModifyPC",&flg);CHKERRQ(ierr);
    if (flg) {ierr = KSPFGMRESSetModifyPC(ksp,KSPFGMRESModifyPCKSP,0,0);CHKERRQ(ierr);} 
  ierr = PetscOptionsTail();CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

EXTERN int KSPComputeExtremeSingularValues_GMRES(KSP,PetscReal *,PetscReal *);
EXTERN int KSPComputeEigenvalues_GMRES(KSP,int,PetscReal *,PetscReal *,int *);

typedef int (*FCN1)(KSP,int,int,PetscReal,void*); /* force argument to next function to not be extern C*/
typedef int (*FCN2)(void*);
EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "KSPFGMRESSetModifyPC_FGMRES" 
int KSPFGMRESSetModifyPC_FGMRES(KSP ksp,FCN1 fcn,void *ctx,FCN2 d)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ksp,KSP_COOKIE,1);
  ((KSP_FGMRES *)ksp->data)->modifypc      = fcn;
  ((KSP_FGMRES *)ksp->data)->modifydestroy = d;
  ((KSP_FGMRES *)ksp->data)->modifyctx     = ctx;
  PetscFunctionReturn(0);
}
EXTERN_C_END

EXTERN_C_BEGIN
EXTERN int KSPGMRESSetPreAllocateVectors_GMRES(KSP);
EXTERN int KSPGMRESSetRestart_GMRES(KSP,int);
EXTERN int KSPGMRESSetOrthogonalization_GMRES(KSP,int (*)(KSP,int));
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "KSPDestroy_FGMRES_Internal" 
int KSPDestroy_FGMRES_Internal(KSP ksp)
{
  KSP_FGMRES *gmres = (KSP_FGMRES*)ksp->data;
  int       i,ierr;

  PetscFunctionBegin;
  /* Free the Hessenberg matrix */
  if (gmres->hh_origin) {ierr = PetscFree(gmres->hh_origin);CHKERRQ(ierr);}

  /* Free the pointer to user variables */
  if (gmres->vecs) {ierr = PetscFree(gmres->vecs);CHKERRQ(ierr);}

  /* free work vectors */
  for (i=0; i<gmres->nwork_alloc; i++) {
    ierr = VecDestroyVecs(gmres->user_work[i],gmres->mwork_alloc[i]);CHKERRQ(ierr);
  }
  if (gmres->user_work)  {ierr = PetscFree(gmres->user_work);CHKERRQ(ierr);}
  if (gmres->mwork_alloc) {ierr = PetscFree(gmres->mwork_alloc);CHKERRQ(ierr);}
  if (gmres->nrs) {ierr = PetscFree(gmres->nrs);CHKERRQ(ierr);}
  if (gmres->sol_temp) {ierr = VecDestroy(gmres->sol_temp);CHKERRQ(ierr);}
  if (gmres->Rsvd) {ierr = PetscFree(gmres->Rsvd);CHKERRQ(ierr);}
  if (gmres->Dsvd) {ierr = PetscFree(gmres->Dsvd);CHKERRQ(ierr);}

  PetscFunctionReturn(0);
}

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "KSPGMRESSetRestart_FGMRES" 
int KSPGMRESSetRestart_FGMRES(KSP ksp,int max_k)
{
  KSP_FGMRES *gmres = (KSP_FGMRES *)ksp->data;
  int        ierr;

  PetscFunctionBegin;
  if (max_k < 1) SETERRQ(1,"Restart must be positive");
  if (!ksp->setupcalled) {
    gmres->max_k = max_k;
  } else if (gmres->max_k != max_k) {
     gmres->max_k = max_k;
     ksp->setupcalled = 0;
     /* free the data structures, then create them again */
     ierr = KSPDestroy_FGMRES_Internal(ksp);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}
EXTERN_C_END

EXTERN_C_BEGIN
EXTERN int KSPGMRESSetCGSRefinementType_GMRES(KSP,KSPGMRESCGSRefinementType);
EXTERN_C_END

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "KSPCreate_FGMRES"
int KSPCreate_FGMRES(KSP ksp)
{
  KSP_FGMRES *fgmres;
  int        ierr;

  PetscFunctionBegin;
  ierr = PetscNew(KSP_FGMRES,&fgmres);CHKERRQ(ierr);
  PetscMemzero(fgmres,sizeof(KSP_FGMRES));
  PetscLogObjectMemory(ksp,sizeof(KSP_FGMRES));
  ksp->data                              = (void*)fgmres;
  ksp->ops->buildsolution                = KSPBuildSolution_FGMRES;

  ksp->ops->setup                        = KSPSetUp_FGMRES;
  ksp->ops->solve                        = KSPSolve_FGMRES;
  ksp->ops->destroy                      = KSPDestroy_FGMRES;
  ksp->ops->view                         = KSPView_GMRES;
  ksp->ops->setfromoptions               = KSPSetFromOptions_FGMRES;
  ksp->ops->computeextremesingularvalues = KSPComputeExtremeSingularValues_GMRES;
  ksp->ops->computeeigenvalues           = KSPComputeEigenvalues_GMRES;

  ierr = PetscObjectComposeFunctionDynamic((PetscObject)ksp,"KSPGMRESSetPreAllocateVectors_C",
                                    "KSPGMRESSetPreAllocateVectors_GMRES",
                                     KSPGMRESSetPreAllocateVectors_GMRES);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)ksp,"KSPGMRESSetOrthogonalization_C",
                                    "KSPGMRESSetOrthogonalization_GMRES",
                                     KSPGMRESSetOrthogonalization_GMRES);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)ksp,"KSPGMRESSetRestart_C",
                                    "KSPGMRESSetRestart_FGMRES",
                                     KSPGMRESSetRestart_FGMRES);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)ksp,"KSPFGMRESSetModifyPC_C",
                                    "KSPFGMRESSetModifyPC_FGMRES",
                                     KSPFGMRESSetModifyPC_FGMRES);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)ksp,"KSPGMRESSetCGSRefinementType_C",
                                    "KSPGMRESSetCGSRefinementType_GMRES",
                                     KSPGMRESSetCGSRefinementType_GMRES);CHKERRQ(ierr);


  fgmres->haptol              = 1.0e-30;
  fgmres->q_preallocate       = 0;
  fgmres->delta_allocate      = FGMRES_DELTA_DIRECTIONS;
  fgmres->orthog              = KSPGMRESClassicalGramSchmidtOrthogonalization;
  fgmres->nrs                 = 0;
  fgmres->sol_temp            = 0;
  fgmres->max_k               = FGMRES_DEFAULT_MAXK;
  fgmres->Rsvd                = 0;
  fgmres->modifypc            = KSPFGMRESModifyPCNoChange;
  fgmres->modifyctx           = PETSC_NULL;
  fgmres->modifydestroy       = PETSC_NULL;
  fgmres->cgstype             = KSP_GMRES_CGS_REFINE_NEVER;
  /*
        This is not great since it changes this without explicit request from the user
     but there is no left preconditioning in the FGMRES
  */
  PetscLogInfo(ksp,"Warning: Setting PC_SIDE for FGMRES to right!\n");
  ksp->pc_side                = PC_RIGHT;

  PetscFunctionReturn(0);
}
EXTERN_C_END
