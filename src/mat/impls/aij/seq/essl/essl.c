#define PETSCMAT_DLL

/* 
        Provides an interface to the IBM RS6000 Essl sparse solver

*/
#include "src/mat/impls/aij/seq/aij.h"

/* #include <essl.h> This doesn't work!  */

EXTERN_C_BEGIN
void dgss(int*,int*,double*,int*,int*,int*,double*,double*,int*);
void dgsf(int*,int*,int*,double*,int*,int*,int*,int*,double*,double*,double*,int*);
EXTERN_C_END

typedef struct {
  int         n,nz;
  PetscScalar *a;
  int         *ia;
  int         *ja;
  int         lna;
  int         iparm[5];
  PetscReal   rparm[5];
  PetscReal   oparm[5];
  PetscScalar *aux;
  int         naux;

  PetscTruth CleanUpESSL;
} Mat_Essl;

#undef __FUNCT__  
#define __FUNCT__ "MatDestroy_Essl"
PetscErrorCode MatDestroy_Essl(Mat A) 
{
  PetscErrorCode ierr;
  Mat_Essl       *essl=(Mat_Essl*)A->spptr;

  PetscFunctionBegin;
  if (essl->CleanUpESSL) {
    ierr = PetscFree(essl->a);CHKERRQ(ierr);
  }
  ierr = PetscFree(essl);CHKERRQ(ierr);
  ierr = MatDestroy_SeqAIJ(A);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolve_Essl"
PetscErrorCode MatSolve_Essl(Mat A,Vec b,Vec x) 
{
  Mat_Essl       *essl = (Mat_Essl*)A->spptr;
  PetscScalar    *xx;
  PetscErrorCode ierr;
  int            m,zero = 0;

  PetscFunctionBegin;
  ierr = VecGetLocalSize(b,&m);CHKERRQ(ierr);
  ierr = VecCopy(b,x);CHKERRQ(ierr);
  ierr = VecGetArray(x,&xx);CHKERRQ(ierr);
  dgss(&zero,&A->cmap.n,essl->a,essl->ia,essl->ja,&essl->lna,xx,essl->aux,&essl->naux);
  ierr = VecRestoreArray(x,&xx);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatLUFactorNumeric_Essl"
PetscErrorCode MatLUFactorNumeric_Essl(Mat A,MatFactorInfo *info,Mat *F) 
{
  Mat_SeqAIJ     *aa=(Mat_SeqAIJ*)(A)->data;
  Mat_Essl       *essl=(Mat_Essl*)(*F)->spptr;
  PetscErrorCode ierr;
  int            i,one = 1;

  PetscFunctionBegin;
  /* copy matrix data into silly ESSL data structure (1-based Frotran style) */
  for (i=0; i<A->rmap.n+1; i++) essl->ia[i] = aa->i[i] + 1;
  for (i=0; i<aa->nz; i++) essl->ja[i]  = aa->j[i] + 1;
 
  ierr = PetscMemcpy(essl->a,aa->a,(aa->nz)*sizeof(PetscScalar));CHKERRQ(ierr);
  
  /* set Essl options */
  essl->iparm[0] = 1; 
  essl->iparm[1] = 5;
  essl->iparm[2] = 1;
  essl->iparm[3] = 0;
  essl->rparm[0] = 1.e-12;
  essl->rparm[1] = 1.0;
  ierr = PetscOptionsGetReal(((PetscObject)A)->prefix,"-matessl_lu_threshold",&essl->rparm[1],PETSC_NULL);CHKERRQ(ierr);

  dgsf(&one,&A->rmap.n,&essl->nz,essl->a,essl->ia,essl->ja,&essl->lna,essl->iparm,essl->rparm,essl->oparm,essl->aux,&essl->naux);

  (*F)->assembled = PETSC_TRUE;
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "MatLUFactorSymbolic_Essl"
PetscErrorCode MatLUFactorSymbolic_Essl(Mat A,IS r,IS c,MatFactorInfo *info,Mat *F) 
{
  Mat            B;
  Mat_SeqAIJ     *a = (Mat_SeqAIJ*)A->data;
  PetscErrorCode ierr;
  int            len;
  Mat_Essl       *essl;
  PetscReal      f = 1.0;

  PetscFunctionBegin;
  if (A->cmap.N != A->rmap.N) SETERRQ(PETSC_ERR_ARG_SIZ,"matrix must be square"); 
  ierr = MatCreate(((PetscObject)A)->comm,&B);CHKERRQ(ierr);
  ierr = MatSetSizes(B,PETSC_DECIDE,PETSC_DECIDE,A->rmap.n,A->cmap.n);CHKERRQ(ierr);
  ierr = MatSetType(B,((PetscObject)A)->type_name);CHKERRQ(ierr);
  ierr = MatSeqAIJSetPreallocation(B,0,PETSC_NULL);CHKERRQ(ierr);

  B->ops->solve           = MatSolve_Essl;
  B->ops->lufactornumeric = MatLUFactorNumeric_Essl;
  B->factor               = FACTOR_LU;
  
  essl = (Mat_Essl *)(B->spptr);

  /* allocate the work arrays required by ESSL */
  f = info->fill;
  essl->nz   = a->nz;
  essl->lna  = (int)a->nz*f;
  essl->naux = 100 + 10*A->rmap.n;

  /* since malloc is slow on IBM we try a single malloc */
  len               = essl->lna*(2*sizeof(int)+sizeof(PetscScalar)) + essl->naux*sizeof(PetscScalar);
  ierr              = PetscMalloc(len,&essl->a);CHKERRQ(ierr);
  essl->aux         = essl->a + essl->lna;
  essl->ia          = (int*)(essl->aux + essl->naux);
  essl->ja          = essl->ia + essl->lna;
  essl->CleanUpESSL = PETSC_TRUE;

  ierr = PetscLogObjectMemory(B,len);CHKERRQ(ierr);
  *F = B;
  PetscFunctionReturn(0);
}

/*MC
  MATESSL - MATESSL = "essl" - A matrix type providing direct solvers (LU) for sequential matrices 
  via the external package ESSL.

  If ESSL is installed (see the manual for
  instructions on how to declare the existence of external packages),
  a matrix type can be constructed which invokes ESSL solvers.
  After calling MatCreate(...,A), simply call MatSetType(A,MATESSL).
  This matrix type is only supported for double precision real.

  This matrix inherits from MATSEQAIJ.  As a result, MatSeqAIJSetPreallocation is 
  supported for this matrix type.  One can also call MatConvert for an inplace conversion to or from 
  the MATSEQAIJ type without data copy.

  Options Database Keys:
. -mat_type essl - sets the matrix type to "essl" during a call to MatSetFromOptions()

   Level: beginner

.seealso: PCLU
M*/

#undef __FUNCT__  
#define __FUNCT__ "MatGetFactor_seqaij_essl"
PetscErrorCode MatGetFactor_seqaij_essl(Mat A,Mat *F) 
{
  Mat            B;
  Mat_SeqAIJ     *a = (Mat_SeqAIJ*)A->data;
  PetscErrorCode ierr;
  Mat_Essl       *essl;

  PetscFunctionBegin;
  if (A->cmap.N != A->rmap.N) SETERRQ(PETSC_ERR_ARG_SIZ,"matrix must be square"); 
  ierr = MatCreate(((PetscObject)A)->comm,&B);CHKERRQ(ierr);
  ierr = MatSetSizes(B,PETSC_DECIDE,PETSC_DECIDE,A->rmap.n,A->cmap.n);CHKERRQ(ierr);
  ierr = MatSetType(B,((PetscObject)A)->type_name);CHKERRQ(ierr);
  ierr = MatSeqAIJSetPreallocation(B,0,PETSC_NULL);CHKERRQ(ierr);

  ierr = PetscNewLog(B,Mat_Essl,&essl);CHKERRQ(ierr);
  B->spptr                 = essl;
  B->ops->solve            = MatSolve_Essl;
  B->ops->lufactornumeric  = MatLUFactorNumeric_Essl;
  B->ops->lufactorsymbolic = MatLUFactorSymbolic_Essl;
  B->factor                = FACTOR_LU;
  *F                       = B;
  PetscFunctionReturn(0);
}
