/*
  Implements an alternative approach for computing the differencing parameter
  h used with the finite difference based matrix-free Jacobian.  This code
  implements the strategy of M. Pernice and H. Walker:

      h = error_rel * sqrt(1 + ||U||) / ||a||

      Notes:
        1) || U || does not change between linear iterations so can be reused
        2) In GMRES || a || == 1 and so does not need to ever be computed if you never 
           have a restart. Unfortunately a RESTART computes a matrix vector product 
           with ||a|| != 0 which breaks this

      Reference:  M. Pernice and H. F. Walker, "NITSOL: A Newton Iterative 
      Solver for Nonlinear Systems", SIAM J. Sci. Stat. Comput.", 1998, 
      vol 19, pp. 302--318.

   See snesmfjdef.c for  a full set of comments on the routines below.
*/

/*
    This include file defines the data structure  MatSNESMF that 
   includes information about the computation of h. It is shared by 
   all implementations that people provide.
*/
#include "src/mat/matimpl.h"
#include "src/snes/mf/snesmfj.h"   /*I  "petscsnes.h"   I*/

typedef struct {
  PetscReal  normUfact;                   /* previous sqrt(1.0 + || U ||) */
  PetscTruth computenorma,computenormU;   
} MatSNESMFWP;

#undef __FUNCT__  
#define __FUNCT__ "MatSNESMFCompute_WP"
/*
     MatSNESMFCompute_WP - Standard PETSc code for 
   computing h with matrix-free finite differences.

  Input Parameters:
+   ctx - the matrix free context
.   U - the location at which you want the Jacobian
-   a - the direction you want the derivative

  Output Parameter:
.   h - the scale computed

*/
static int MatSNESMFCompute_WP(MatSNESMFCtx ctx,Vec U,Vec a,PetscScalar *h)
{
  MatSNESMFWP   *hctx = (MatSNESMFWP*)ctx->hctx;
  PetscReal     normU,norma = 1.0;
  int           ierr;

  PetscFunctionBegin;

  if (!(ctx->count % ctx->recomputeperiod)) {
    if (hctx->computenorma && (hctx->computenormU || !ctx->ncurrenth)) {
      ierr = VecNormBegin(U,NORM_2,&normU);CHKERRQ(ierr);
      ierr = VecNormBegin(a,NORM_2,&norma);CHKERRQ(ierr);
      ierr = VecNormEnd(U,NORM_2,&normU);CHKERRQ(ierr);
      ierr = VecNormEnd(a,NORM_2,&norma);CHKERRQ(ierr);
      hctx->normUfact = sqrt(1.0+normU);
    } else if (hctx->computenormU || !ctx->ncurrenth) {
      ierr = VecNorm(U,NORM_2,&normU);CHKERRQ(ierr);
      hctx->normUfact = sqrt(1.0+normU);
    } else if (hctx->computenorma) {
      ierr = VecNorm(a,NORM_2,&norma);CHKERRQ(ierr);
    }
    *h = ctx->error_rel*hctx->normUfact/norma;
  } else {
    *h = ctx->currenth;
  }
  PetscFunctionReturn(0);
} 

#undef __FUNCT__  
#define __FUNCT__ "MatSNESMFView_WP"
/*
   MatSNESMFView_WP - Prints information about this particular 
     method for computing h. Note that this does not print the general
     information about the matrix free, that is printed by the calling
     routine.

  Input Parameters:
+   ctx - the matrix free context
-   viewer - the PETSc viewer

*/   
static int MatSNESMFView_WP(MatSNESMFCtx ctx,PetscViewer viewer)
{
  MatSNESMFWP *hctx = (MatSNESMFWP *)ctx->hctx;
  int         ierr;
  PetscTruth  isascii;

  PetscFunctionBegin;
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_ASCII,&isascii);CHKERRQ(ierr);
  if (isascii) {
    if (hctx->computenorma){ierr = PetscViewerASCIIPrintf(viewer,"    Computes normA\n");CHKERRQ(ierr);}
    else                   {ierr =  PetscViewerASCIIPrintf(viewer,"    Does not compute normA\n");CHKERRQ(ierr);}
    if (hctx->computenormU){ierr =  PetscViewerASCIIPrintf(viewer,"    Computes normU\n");CHKERRQ(ierr);}  
    else                   {ierr =  PetscViewerASCIIPrintf(viewer,"    Does not compute normU\n");CHKERRQ(ierr);}  
  } else {
    SETERRQ1(1,"Viewer type %s not supported for SNES matrix-free WP",((PetscObject)viewer)->type_name);
  }    
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSNESMFSetFromOptions_WP"
/*
   MatSNESMFSetFromOptions_WP - Looks in the options database for 
     any options appropriate for this method

  Input Parameter:
.  ctx - the matrix free context

*/
static int MatSNESMFSetFromOptions_WP(MatSNESMFCtx ctx)
{
  int         ierr;
  MatSNESMFWP *hctx = (MatSNESMFWP*)ctx->hctx;

  PetscFunctionBegin;
  ierr = PetscOptionsHead("Walker-Pernice options");CHKERRQ(ierr);
    ierr = PetscOptionsLogical("-snes_mf_compute_norma","Compute the norm of a","MatSNESMFWPSetComputeNormA",
                          hctx->computenorma,&hctx->computenorma,0);CHKERRQ(ierr);
    ierr = PetscOptionsLogical("-snes_mf_compute_normu","Compute the norm of u","MatSNESMFWPSetComputeNormU",
                          hctx->computenorma,&hctx->computenorma,0);CHKERRQ(ierr);
  ierr = PetscOptionsTail();CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSNESMFDestroy_WP"
/*
   MatSNESMFDestroy_WP - Frees the space allocated by 
       MatSNESMFCreate_WP(). 

  Input Parameter:
.  ctx - the matrix free context

   Notes: does not free the ctx, that is handled by the calling routine

*/
static int MatSNESMFDestroy_WP(MatSNESMFCtx ctx)
{
  int ierr;
  PetscFunctionBegin;
  ierr = PetscFree(ctx->hctx);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "MatSNESMFWPSetComputeNormA_P"
int MatSNESMFWPSetComputeNormA_P(Mat mat,PetscTruth flag)
{
  MatSNESMFCtx ctx = (MatSNESMFCtx)mat->data;
  MatSNESMFWP  *hctx;

  PetscFunctionBegin;
  hctx               = (MatSNESMFWP*)ctx->hctx;
  hctx->computenorma = flag;
  PetscFunctionReturn(0);
} 
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "MatSNESMFWPSetComputeNormA"
/*@
    MatSNESMFWPSetComputeNormA - Sets whether it computes the ||a|| used by the WP
             PETSc routine for computing h. With GMRES since the ||a|| is always
             one, you can save communication by setting this to false.

  Input Parameters:
+   A - the matrix created with MatCreateSNESMF()
-   flag - PETSC_TRUE causes it to compute ||a||, PETSC_FALSE assumes it is 1.

  Options Database Key:
.   -snes_mf_compute_norma <true,false> - can be set false for GMRES to speed up code

  Level: advanced

  Notes:
   See the manual page for MatCreateSNESMF() for a complete description of the
   algorithm used to compute h.

.seealso: MatSNESMFSetFunctionError(), MatCreateSNESMF()

@*/
int MatSNESMFWPSetComputeNormA(Mat A,PetscTruth flag)
{
  int ierr,(*f)(Mat,PetscTruth);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(A,MAT_COOKIE,1);
  ierr = PetscObjectQueryFunction((PetscObject)A,"MatSNESMFWPSetComputeNormA_C",(void (**)(void))&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(A,flag);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "MatSNESMFWPSetComputeNormU_P"
int MatSNESMFWPSetComputeNormU_P(Mat mat,PetscTruth flag)
{
  MatSNESMFCtx ctx = (MatSNESMFCtx)mat->data;
  MatSNESMFWP  *hctx;

  PetscFunctionBegin;
  hctx               = (MatSNESMFWP*)ctx->hctx;
  hctx->computenormU = flag;
  PetscFunctionReturn(0);
} 
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "MatSNESMFWPSetComputeNormU"
/*@
    MatSNESMFWPSetComputeNormU - Sets whether it computes the ||U|| used by the WP
             PETSc routine for computing h. With any Krylov solver this need only 
             be computed during the first iteration and kept for later.

  Input Parameters:
+   A - the matrix created with MatCreateSNESMF()
-   flag - PETSC_TRUE causes it to compute ||U||, PETSC_FALSE uses the previous value

  Options Database Key:
.   -snes_mf_compute_normu <true,false> - true by default, false causes extra calculations

  Level: advanced

  Notes:
   See the manual page for MatCreateSNESMF() for a complete description of the
   algorithm used to compute h.

.seealso: MatSNESMFSetFunctionError(), MatCreateSNESMF()

@*/
int MatSNESMFWPSetComputeNormU(Mat A,PetscTruth flag)
{
  int ierr,(*f)(Mat,PetscTruth);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(A,MAT_COOKIE,1);
  ierr = PetscObjectQueryFunction((PetscObject)A,"MatSNESMFWPSetComputeNormU_C",(void (**)(void))&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(A,flag);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "MatSNESMFCreate_WP"
/*
     MatSNESMFCreate_WP - Standard PETSc code for 
   computing h with matrix-free finite differences.

   Input Parameter:
.  ctx - the matrix free context created by MatSNESMFCreate()

*/
int MatSNESMFCreate_WP(MatSNESMFCtx ctx)
{
  int         ierr;
  MatSNESMFWP *hctx;

  PetscFunctionBegin;

  /* allocate my own private data structure */
  ierr               = PetscNew(MatSNESMFWP,&hctx);CHKERRQ(ierr);
  ctx->hctx          = (void*)hctx;
  hctx->computenormU = PETSC_FALSE;
  hctx->computenorma = PETSC_TRUE;

  /* set the functions I am providing */
  ctx->ops->compute        = MatSNESMFCompute_WP;
  ctx->ops->destroy        = MatSNESMFDestroy_WP;
  ctx->ops->view           = MatSNESMFView_WP;  
  ctx->ops->setfromoptions = MatSNESMFSetFromOptions_WP;  

  ierr = PetscObjectComposeFunctionDynamic((PetscObject)ctx->mat,"MatSNESMFWPSetComputeNormA_C",
                            "MatSNESMFWPSetComputeNormA_P",
                             MatSNESMFWPSetComputeNormA_P);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)ctx->mat,"MatSNESMFWPSetComputeNormU_C",
                            "MatSNESMFWPSetComputeNormU_P",
                             MatSNESMFWPSetComputeNormU_P);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}
EXTERN_C_END



