/*$Id: superlu.c,v 1.10 2001/08/15 15:56:50 bsmith Exp $*/

/* 
        Provides an interface to the SuperLU sparse solver
          Modified for SuperLU 2.0 by Matthew Knepley

*/

#include "src/mat/impls/aij/seq/aij.h"

#if defined(PETSC_HAVE_SUPERLU) && !defined(PETSC_USE_SINGLE) && !defined(PETSC_USE_COMPLEX)
EXTERN_C_BEGIN
#include "dsp_defs.h"
EXTERN_C_END


typedef struct {
  SuperMatrix A;
  SuperMatrix B;
  SuperMatrix AC;
  SuperMatrix L;
  SuperMatrix U;
  int        *perm_r;
  int        *perm_c;
  int         relax;
  int         panel_size;
  double      pivot_threshold;
} Mat_SeqAIJ_SuperLU;


extern int MatDestroy_SeqAIJ(Mat);

#undef __FUNCT__  
#define __FUNCT__ "MatDestroy_SeqAIJ_SuperLU"
extern int MatDestroy_SeqAIJ_SuperLU(Mat A)
{
  Mat_SeqAIJ         *a  = (Mat_SeqAIJ*)A->data;
  Mat_SeqAIJ_SuperLU *lu = (Mat_SeqAIJ_SuperLU*)a->spptr;
  int                ierr;

  PetscFunctionBegin;
  if (--A->refct > 0)PetscFunctionReturn(0);
  /* We have to free the global data or SuperLU crashes (sucky design)*/
  StatFree();
  /* Free the SuperLU datastructures */
  Destroy_CompCol_Permuted(&lu->AC);
  Destroy_SuperNode_Matrix(&lu->L);
  Destroy_CompCol_Matrix(&lu->U);
  ierr = PetscFree(lu->B.Store);CHKERRQ(ierr);
  ierr = PetscFree(lu->A.Store);CHKERRQ(ierr);
  ierr = PetscFree(lu->perm_r);CHKERRQ(ierr);
  ierr = PetscFree(lu->perm_c);CHKERRQ(ierr);
  ierr = PetscFree(lu);CHKERRQ(ierr);
  ierr = MatDestroy_SeqAIJ(A);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#include "src/mat/impls/dense/seq/dense.h"
#undef __FUNCT__  
#define __FUNCT__ "MatCreateNull_SeqAIJ_SuperLU"
int MatCreateNull_SeqAIJ_SuperLU(Mat A,Mat *nullMat)
{
  Mat_SeqAIJ         *a       = (Mat_SeqAIJ*)A->data;
  Mat_SeqAIJ_SuperLU *lu = (Mat_SeqAIJ_SuperLU*)a->spptr;
  int                 numRows = A->m;
  int                 numCols = A->n;
  SCformat           *Lstore;
  int                 numNullCols,size;
  PetscScalar             *nullVals,*workVals;
  int                 row,newRow,col,newCol,block,b;
  int                 ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(A,MAT_COOKIE);
  PetscValidPointer(nullMat);
  if (!A->factor) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Unfactored matrix");
  numNullCols = numCols - numRows;
  if (numNullCols < 0) SETERRQ(PETSC_ERR_ARG_WRONG,"Function only applies to underdetermined problems");
  /* Create the null matrix */
  ierr = MatCreateSeqDense(A->comm,numRows,numNullCols,PETSC_NULL,nullMat);CHKERRQ(ierr);
  if (numNullCols == 0) {
    ierr = MatAssemblyBegin(*nullMat,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
    ierr = MatAssemblyEnd(*nullMat,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
    PetscFunctionReturn(0);
  }
  nullVals = ((Mat_SeqDense*)(*nullMat)->data)->v;
  /* Copy in the columns */
  Lstore = (SCformat*)lu->L.Store;
  for(block = 0; block <= Lstore->nsuper; block++) {
    newRow = Lstore->sup_to_col[block];
    size   = Lstore->sup_to_col[block+1] - Lstore->sup_to_col[block];
    for(col = Lstore->rowind_colptr[newRow]; col < Lstore->rowind_colptr[newRow+1]; col++) {
      newCol = Lstore->rowind[col];
      if (newCol >= numRows) {
        for(b = 0; b < size; b++)
          nullVals[(newCol-numRows)*numRows+newRow+b] = ((double*)Lstore->nzval)[Lstore->nzval_colptr[newRow+b]+col];
      }
    }
  }
  /* Permute rhs to form P^T_c B */
  ierr = PetscMalloc(numRows*sizeof(double),&workVals);CHKERRQ(ierr);
  for(b = 0; b < numNullCols; b++) {
    for(row = 0; row < numRows; row++) workVals[lu->perm_c[row]] = nullVals[b*numRows+row];
    for(row = 0; row < numRows; row++) nullVals[b*numRows+row]   = workVals[row];
  }
  /* Backward solve the upper triangle A x = b */
  for(b = 0; b < numNullCols; b++) {
    sp_dtrsv("L","T","U",&lu->L,&lu->U,&nullVals[b*numRows],&ierr);
    if (ierr < 0)
      SETERRQ1(PETSC_ERR_ARG_WRONG,"The argument %d was invalid",-ierr);
  }
  ierr = PetscFree(workVals);CHKERRQ(ierr);

  ierr = MatAssemblyBegin(*nullMat,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(*nullMat,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqAIJ_SuperLU"
extern int MatSolve_SeqAIJ_SuperLU(Mat A,Vec b,Vec x)
{
  Mat_SeqAIJ         *a  = (Mat_SeqAIJ*)A->data;
  Mat_SeqAIJ_SuperLU *lu = (Mat_SeqAIJ_SuperLU*)a->spptr;
  PetscScalar             *array;
  int                 m;
  int                 ierr;

  PetscFunctionBegin;
  ierr = VecGetLocalSize(b,&m);CHKERRQ(ierr);
  ierr = VecCopy(b,x);CHKERRQ(ierr);
  ierr = VecGetArray(x,&array);CHKERRQ(ierr);
  /* Create the Rhs */
  lu->B.Stype        = DN;
  lu->B.Dtype        = _D;
  lu->B.Mtype        = GE;
  lu->B.nrow         = m;
  lu->B.ncol         = 1;
  ((DNformat*)lu->B.Store)->lda   = m;
  ((DNformat*)lu->B.Store)->nzval = array;
  dgstrs("T",&lu->L,&lu->U,lu->perm_r,lu->perm_c,&lu->B,&ierr);
  if (ierr < 0) SETERRQ1(PETSC_ERR_ARG_WRONG,"The diagonal element of row %d was invalid",-ierr);
  ierr = VecRestoreArray(x,&array);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*
   Note the r permutation is ignored
*/
#undef __FUNCT__  
#define __FUNCT__ "MatLUFactorSymbolic_SeqAIJ_SuperLU"
extern int MatLUFactorSymbolic_SeqAIJ_SuperLU(Mat A,IS r,IS c,MatLUInfo *info,Mat *F)
{
  Mat_SeqAIJ         *b;
  Mat                 B;
  Mat_SeqAIJ_SuperLU *lu;
  int                 ierr,*ca;

  PetscFunctionBegin;
  ierr            = MatCreateSeqAIJ(A->comm,A->m,A->n,0,PETSC_NULL,F);CHKERRQ(ierr);
  B               = *F;
  B->ops->solve   = MatSolve_SeqAIJ_SuperLU;
  B->ops->destroy = MatDestroy_SeqAIJ_SuperLU;
  B->factor       = FACTOR_LU;
  b               = (Mat_SeqAIJ*)B->data;
  ierr            = PetscNew(Mat_SeqAIJ_SuperLU,&lu);CHKERRQ(ierr);
  b->spptr        = (void*)lu;
  ierr = PetscObjectComposeFunction((PetscObject)B,"MatCreateNull","MatCreateNull_SeqAIJ_SuperLU",
                                    (void*)MatCreateNull_SeqAIJ_SuperLU);CHKERRQ(ierr);

  /* Allocate the work arrays required by SuperLU (notice sizes are for the transpose) */
  ierr = PetscMalloc(A->n*sizeof(int),&lu->perm_r);CHKERRQ(ierr);
  ierr = PetscMalloc(A->m*sizeof(int),&lu->perm_c);CHKERRQ(ierr);
  ierr = ISGetIndices(c,&ca);CHKERRQ(ierr);
  ierr = PetscMemcpy(lu->perm_c,ca,A->m*sizeof(int));CHKERRQ(ierr);
  ierr = ISRestoreIndices(c,&ca);CHKERRQ(ierr);
  
  if (info) {
    lu->pivot_threshold = info->dtcol;
  } else {
    lu->pivot_threshold = 0.0; /* no pivoting */
  }

  PetscLogObjectMemory(B,(A->m+A->n)*sizeof(int)+sizeof(Mat_SeqAIJ_SuperLU));
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatLUFactorNumeric_SeqAIJ_SuperLU"
extern int MatLUFactorNumeric_SeqAIJ_SuperLU(Mat A,Mat *F)
{
  Mat_SeqAIJ         *a  = (Mat_SeqAIJ*)(*F)->data;
  Mat_SeqAIJ         *aa = (Mat_SeqAIJ*)(A)->data;
  Mat_SeqAIJ_SuperLU *lu = (Mat_SeqAIJ_SuperLU*)a->spptr;
  NCformat           *store;
  int                *etree,i,ierr;

  PetscFunctionBegin;
  /* Create the SuperMatrix for A^T:

       Since SuperLU only likes column-oriented matrices,we pass it the transpose,
       and then solve A^T X = B in MatSolve().
  */
  lu->A.Stype   = NC;
  lu->A.Dtype   = _D;
  lu->A.Mtype   = GE;
  lu->A.nrow    = A->n;
  lu->A.ncol    = A->m;
  ierr          = PetscMalloc(sizeof(NCformat),&store);CHKERRQ(ierr);
  store->nnz    = aa->nz;
  store->colptr = aa->i;
  store->rowind = aa->j;
  store->nzval  = aa->a;
  lu->A.Store   = store;
  ierr          = PetscMalloc(sizeof(DNformat),&lu->B.Store);CHKERRQ(ierr);

  /* Shift indices down */
  if (aa->indexshift) {
    for(i = 0; i < A->m+1; i++) aa->i[i]--;
    for(i = 0; i < aa->nz; i++) aa->j[i]--;
  }
  
  /* Set SuperLU options */
  lu->relax      = sp_ienv(2);
  lu->panel_size = sp_ienv(1);
  ierr           = PetscMalloc(A->n*sizeof(int),&etree);CHKERRQ(ierr);
  /* We have to initialize global data or SuperLU crashes (sucky design) */
  StatInit(lu->panel_size,lu->relax);

  /* Create the elimination tree */
  sp_preorder("N",&lu->A,lu->perm_c,etree,&lu->AC);
  /* Factor the matrix */
  dgstrf("N",&lu->AC,lu->pivot_threshold,0.0,lu->relax,lu->panel_size,etree,PETSC_NULL,0,lu->perm_r,lu->perm_c,&lu->L,&lu->U,&ierr);
  if (ierr < 0) {
    SETERRQ1(PETSC_ERR_ARG_WRONG,"The diagonal element of row %d was invalid",-ierr);
  } else if (ierr > 0) {
    if (ierr <= A->m) {
      SETERRQ1(PETSC_ERR_ARG_WRONG,"The diagonal element %d of U is exactly zero",ierr);
    } else {
      SETERRQ1(PETSC_ERR_ARG_WRONG,"Memory allocation failure after %d bytes were allocated",ierr-A->m);
    }
  }

  /* Shift indices up */
  if (aa->indexshift) {
    for (i = 0; i < A->n+1; i++)  aa->i[i]++;
    for (i = 0; i < aa->nz; i++)  aa->j[i]++;
  }

  /* Cleanup */
  ierr = PetscFree(etree);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "MatUseSuperLU_SeqAIJ"
int MatUseSuperLU_SeqAIJ(Mat A)
{
  PetscTruth flg;
  int        ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(A,MAT_COOKIE);  
  ierr = PetscTypeCompare((PetscObject)A,MATSEQAIJ,&flg);
  if (!flg) PetscFunctionReturn(0);

  A->ops->lufactorsymbolic = MatLUFactorSymbolic_SeqAIJ_SuperLU;
  A->ops->lufactornumeric  = MatLUFactorNumeric_SeqAIJ_SuperLU;
  A->ops->solve            = MatSolve_SeqAIJ_SuperLU;

  PetscFunctionReturn(0);
}

#else

#undef __FUNCT__  
#define __FUNCT__ "MatUseSuperLU_SeqAIJ"
int MatUseSuperLU_SeqAIJ(Mat A)
{
  PetscFunctionBegin;
  PetscFunctionReturn(0);
}

#endif


