/*$Id: mpiaijspooles.c,v 1.10 2001/08/15 15:56:50 bsmith Exp $*/
/* 
   Provides an interface to the Spooles parallel sparse solver (MPI SPOOLES)
*/


#include "src/mat/impls/aij/mpi/mpiaij.h"
#include "src/mat/impls/aij/seq/spooles/spooles.h"

#undef __FUNCT__
#define __FUNCT__ "MatAssemblyEnd_MPIAIJSpooles"
int MatAssemblyEnd_MPIAIJSpooles(Mat A,MatAssemblyType mode) {
  int         ierr;
  Mat_Spooles *lu=(Mat_Spooles *)(A->spptr);

  PetscFunctionBegin;
  ierr = (*lu->MatAssemblyEnd)(A,mode);CHKERRQ(ierr);
  lu->MatLUFactorSymbolic  = A->ops->lufactorsymbolic;
  A->ops->lufactorsymbolic = MatLUFactorSymbolic_MPIAIJSpooles;  
  PetscFunctionReturn(0);
}

/* Note the Petsc r and c permutations are ignored */
#undef __FUNCT__  
#define __FUNCT__ "MatLUFactorSymbolic_MPIAIJSpooles"
int MatLUFactorSymbolic_MPIAIJSpooles(Mat A,IS r,IS c,MatFactorInfo *info,Mat *F)
{
  Mat_Spooles *lu;
  Mat         B;
  int         ierr;

  PetscFunctionBegin;	

  /* Create the factorization matrix F */  
  ierr = MatCreate(A->comm,A->m,A->n,A->M,A->N,&B);CHKERRQ(ierr);
  ierr = MatSetType(B,A->type_name);CHKERRQ(ierr);
  ierr = MatMPIAIJSetPreallocation(B,0,PETSC_NULL,0,PETSC_NULL);CHKERRQ(ierr);

  B->ops->lufactornumeric = MatFactorNumeric_MPIAIJSpooles;
  B->factor               = FACTOR_LU;  

  lu                       = (Mat_Spooles *)(B->spptr);
  lu->options.symflag      = SPOOLES_NONSYMMETRIC;
  lu->options.pivotingflag = SPOOLES_PIVOTING; 
  lu->flg                  = DIFFERENT_NONZERO_PATTERN;
  lu->options.useQR        = PETSC_FALSE;

  ierr = MPI_Comm_dup(A->comm,&(lu->comm_spooles));CHKERRQ(ierr);

  if (info->dtcol == 0.0) {
    lu->options.pivotingflag  = SPOOLES_NO_PIVOTING;
  }
  *F = B;
  PetscFunctionReturn(0); 
}
