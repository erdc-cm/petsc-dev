/*$Id: borthog.c,v 1.58 2001/08/07 03:03:51 balay Exp $*/
/*
    Routines used for the orthogonalization of the Hessenberg matrix.

    Note that for the complex numbers version, the VecDot() and
    VecMDot() arguments within the code MUST remain in the order
    given for correct computation of inner products.
*/
#include "src/ksp/ksp/impls/gmres/gmresp.h"

/*@C
     KSPGMRESModifiedGramSchmidtOrthogonalization -  This is the basic orthogonalization routine 
                using modified Gram-Schmidt.

     Collective on KSP

  Input Parameters:
+   ksp - KSP object, must be associated with GMRES, FGMRES, or LGMRES Krylov method
-   its - one less then the current GMRES restart iteration, i.e. the size of the Krylov space

   Options Database Keys:
.  -ksp_gmres_modifiedgramschmidt - Activates KSPGMRESModifiedGramSchmidtOrthogonalization()

   Level: intermediate

.seealso:  KSPGMRESSetOrthogonalization(), KSPGMRESClassicalGramSchmidtOrthogonalization()

@*/
#undef __FUNCT__  
#define __FUNCT__ "KSPGMRESModifiedGramSchmidtOrthogonalization"
int KSPGMRESModifiedGramSchmidtOrthogonalization(KSP ksp,int it)
{
  KSP_GMRES *gmres = (KSP_GMRES *)(ksp->data);
  int       ierr,j;
  PetscScalar    *hh,*hes,tmp;

  PetscFunctionBegin;
  ierr = PetscLogEventBegin(KSP_GMRESOrthogonalization,ksp,0,0,0);CHKERRQ(ierr);
  /* update Hessenberg matrix and do Gram-Schmidt */
  hh  = HH(0,it);
  hes = HES(0,it);
  for (j=0; j<=it; j++) {
    /* (vv(it+1), vv(j)) */
    ierr   = VecDot(VEC_VV(it+1),VEC_VV(j),hh);CHKERRQ(ierr);
    *hes++ = *hh;
    /* vv(it+1) <- vv(it+1) - hh[it+1][j] vv(j) */
    tmp    = - (*hh++);  
    ierr   = VecAXPY(&tmp,VEC_VV(j),VEC_VV(it+1));CHKERRQ(ierr);
  }
  ierr = PetscLogEventEnd(KSP_GMRESOrthogonalization,ksp,0,0,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


