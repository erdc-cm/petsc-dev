/*$Id: mpisbaijspooles.c,v 1.10 2001/08/15 15:56:50 bsmith Exp $*/
/* 
   Provides an interface to the Spooles parallel sparse solver (MPI SPOOLES)
*/

#include "src/mat/impls/aij/seq/spooles/spooles.h"

#undef __FUNCT__  
#define __FUNCT__ "MatDestroy_MPISBAIJ_Spooles"
int MatDestroy_MPISBAIJ_Spooles(Mat A) {
  int         ierr;
  
  PetscFunctionBegin;
  /* MPISBAIJ_Spooles isn't really the matrix that USES spooles, */
  /* rather it is a factory class for creating a symmetric matrix that can */
  /* invoke Spooles' parallel cholesky solver. */
  /* As a result, we don't have to clean up the stuff set for use in spooles */
  /* as in MatDestroy_MPIAIJ_Spooles. */
  ierr = MatConvert_Spooles_Base(A,MATMPISBAIJ,&A);CHKERRQ(ierr);
  ierr = (*A->ops->destroy)(A);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "MatAssemblyEnd_MPISBAIJ_Spooles"
int MatAssemblyEnd_MPISBAIJ_Spooles(Mat A,MatAssemblyType mode) {
  int         ierr,bs;
  Mat_Spooles *lu=(Mat_Spooles *)(A->spptr);

  PetscFunctionBegin;
  ierr = (*lu->MatAssemblyEnd)(A,mode);CHKERRQ(ierr);
  ierr = MatGetBlockSize(A,&bs);CHKERRQ(ierr);
  if (bs > 1) SETERRQ1(1,"Block size %d not supported by Spooles",bs);
  lu->MatCholeskyFactorSymbolic  = A->ops->choleskyfactorsymbolic;
  A->ops->choleskyfactorsymbolic = MatCholeskyFactorSymbolic_MPISBAIJ_Spooles;  
  PetscFunctionReturn(0);
}

/* Note the Petsc r permutation is ignored */
#undef __FUNCT__  
#define __FUNCT__ "MatCholeskyFactorSymbolic_MPISBAIJ_Spooles"
int MatCholeskyFactorSymbolic_MPISBAIJ_Spooles(Mat A,IS r,MatFactorInfo *info,Mat *F)
{
  Mat           B;
  Mat_Spooles   *lu;   
  int           ierr;
  
  PetscFunctionBegin;	

  /* Create the factorization matrix */  
  ierr = MatCreate(A->comm,A->m,A->n,A->M,A->N,&B);
  ierr = MatSetType(B,MATMPIAIJSPOOLES);CHKERRQ(ierr);
  ierr = MatMPIAIJSetPreallocation(B,0,PETSC_NULL,0,PETSC_NULL);CHKERRQ(ierr);
  
  B->ops->choleskyfactornumeric = MatFactorNumeric_MPIAIJ_Spooles;
  B->factor                     = FACTOR_CHOLESKY;  

  lu                       = (Mat_Spooles*)(B->spptr);
  lu->options.pivotingflag = SPOOLES_NO_PIVOTING; 
  lu->flg                  = DIFFERENT_NONZERO_PATTERN;
  lu->options.useQR        = PETSC_FALSE;
  lu->options.symflag      = SPOOLES_SYMMETRIC;  /* default */

  ierr = MPI_Comm_dup(A->comm,&(lu->comm_spooles));CHKERRQ(ierr);
  *F = B;
  PetscFunctionReturn(0); 
}

EXTERN_C_BEGIN
#undef __FUNCT__
#define __FUNCT__ "MatConvert_MPISBAIJ_Spooles"
int MatConvert_MPISBAIJ_Spooles(Mat A,MatType type,Mat *newmat) {
  /* This routine is only called to convert a MATMPISBAIJ matrix */
  /* to a MATMPISBAIJSPOOLES matrix, so we will ignore 'MatType type'. */
  int         ierr;
  Mat         B=*newmat;
  Mat_Spooles *lu;

  PetscFunctionBegin;
  if (B != A) {
    /* This routine is inherited, so we know the type is correct. */
    ierr = MatDuplicate(A,MAT_COPY_VALUES,&B);CHKERRQ(ierr);
  }

  ierr = PetscNew(Mat_Spooles,&lu);CHKERRQ(ierr);
  B->spptr                       = (void*)lu;

  lu->basetype                   = MATMPISBAIJ;
  lu->MatCholeskyFactorSymbolic  = A->ops->choleskyfactorsymbolic;
  lu->MatLUFactorSymbolic        = A->ops->lufactorsymbolic; 
  lu->MatView                    = A->ops->view;
  lu->MatAssemblyEnd             = A->ops->assemblyend;
  lu->MatDestroy                 = A->ops->destroy;
  B->ops->choleskyfactorsymbolic = MatCholeskyFactorSymbolic_MPISBAIJ_Spooles;
  B->ops->assemblyend            = MatAssemblyEnd_MPISBAIJ_Spooles;
  B->ops->destroy                = MatDestroy_MPISBAIJ_Spooles;

  ierr = PetscObjectComposeFunctionDynamic((PetscObject)B,"MatConvert_spooles_mpisbaij_C",
                                           "MatConvert_Spooles_Base",MatConvert_Spooles_Base);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)B,"MatConvert_mpisbaij_spooles_C",
                                           "MatConvert_MPISBAIJ_Spooles",MatConvert_MPISBAIJ_Spooles);CHKERRQ(ierr);
  ierr = PetscObjectChangeTypeName((PetscObject)B,MATMPISBAIJSPOOLES);CHKERRQ(ierr);
  *newmat = B;
  PetscFunctionReturn(0);
}
EXTERN_C_END

EXTERN_C_BEGIN
#undef __FUNCT__
#define __FUNCT__ "MatCreate_MPISBAIJ_Spooles"
int MatCreate_MPISBAIJ_Spooles(Mat A) {
  int ierr;

  PetscFunctionBegin;
  ierr = MatSetType(A,MATMPISBAIJ);CHKERRQ(ierr);
  ierr = MatConvert_MPISBAIJ_Spooles(A,MATMPISBAIJSPOOLES,&A);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END
