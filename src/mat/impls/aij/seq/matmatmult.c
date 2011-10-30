
/*
  Defines matrix-matrix product routines for pairs of SeqAIJ matrices
          C = A * B
*/

#include <../src/mat/impls/aij/seq/aij.h> /*I "petscmat.h" I*/
#include <../src/mat/utils/freespace.h>
#include <petscbt.h>
#include <../src/mat/impls/dense/seq/dense.h> /*I "petscmat.h" I*/

EXTERN_C_BEGIN
#undef __FUNCT__
#define __FUNCT__ "MatMatMult_SeqAIJ_SeqAIJ"
PetscErrorCode MatMatMult_SeqAIJ_SeqAIJ(Mat A,Mat B,MatReuse scall,PetscReal fill,Mat *C) 
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (scall == MAT_INITIAL_MATRIX){
    ierr = MatMatMultSymbolic_SeqAIJ_SeqAIJ(A,B,fill,C);CHKERRQ(ierr);
  }
  ierr = MatMatMultNumeric_SeqAIJ_SeqAIJ(A,B,*C);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "MatMatMultSymbolic_SeqAIJ_SeqAIJ"
PetscErrorCode MatMatMultSymbolic_SeqAIJ_SeqAIJ(Mat A,Mat B,PetscReal fill,Mat *C)
{
  PetscErrorCode     ierr;
  PetscFreeSpaceList free_space=PETSC_NULL,current_space=PETSC_NULL;
  Mat_SeqAIJ         *a=(Mat_SeqAIJ*)A->data,*b=(Mat_SeqAIJ*)B->data,*c;
  PetscInt           *ai=a->i,*aj=a->j,*bi=b->i,*bj=b->j,*bjj,*ci,*cj;
  PetscInt           am=A->rmap->N,bn=B->cmap->N,bm=B->rmap->N;
  PetscInt           i,j,anzi,brow,bnzj,cnzi,nlnk,*lnk,nspacedouble=0;
  MatScalar          *ca;
  PetscBT            lnkbt;
  PetscReal          afill;

  PetscFunctionBegin;
  /* Allocate ci array, arrays for fill computation and */
  /* free space for accumulating nonzero column info */
  ierr = PetscMalloc(((am+1)+1)*sizeof(PetscInt),&ci);CHKERRQ(ierr);
  ci[0] = 0;
  
  /* create and initialize a linked list */
  nlnk = bn+1;
  ierr = PetscLLCreate(bn,bn,nlnk,lnk,lnkbt);CHKERRQ(ierr);

  /* Initial FreeSpace size is fill*(nnz(A)+nnz(B)) */
  ierr = PetscFreeSpaceGet((PetscInt)(fill*(ai[am]+bi[bm])),&free_space);CHKERRQ(ierr);
  current_space = free_space;

  /* Determine symbolic info for each row of the product: */
  for (i=0;i<am;i++) {
    anzi = ai[i+1] - ai[i];
    cnzi = 0;
    j    = anzi;
    aj   = a->j + ai[i];
    while (j){/* assume cols are almost in increasing order, starting from its end saves computation */
      j--;
      brow = *(aj + j); 
      bnzj = bi[brow+1] - bi[brow];
      bjj  = bj + bi[brow];
      /* add non-zero cols of B into the sorted linked list lnk */
      ierr = PetscLLAdd(bnzj,bjj,bn,nlnk,lnk,lnkbt);CHKERRQ(ierr);
      cnzi += nlnk;
    }

    /* If free space is not available, make more free space */
    /* Double the amount of total space in the list */
    if (current_space->local_remaining<cnzi) {
      ierr = PetscFreeSpaceGet(cnzi+current_space->total_array_size,&current_space);CHKERRQ(ierr);
      nspacedouble++;
    }

    /* Copy data into free space, then initialize lnk */
    ierr = PetscLLClean(bn,bn,cnzi,lnk,current_space->array,lnkbt);CHKERRQ(ierr);
    current_space->array           += cnzi;
    current_space->local_used      += cnzi;
    current_space->local_remaining -= cnzi;

    ci[i+1] = ci[i] + cnzi;
  }

  /* Column indices are in the list of free space */
  /* Allocate space for cj, initialize cj, and */
  /* destroy list of free space and other temporary array(s) */
  ierr = PetscMalloc((ci[am]+1)*sizeof(PetscInt),&cj);CHKERRQ(ierr);
  ierr = PetscFreeSpaceContiguous(&free_space,cj);CHKERRQ(ierr);
  ierr = PetscLLDestroy(lnk,lnkbt);CHKERRQ(ierr);
    
  /* Allocate space for ca */
  ierr = PetscMalloc((ci[am]+1)*sizeof(MatScalar),&ca);CHKERRQ(ierr);
  ierr = PetscMemzero(ca,(ci[am]+1)*sizeof(MatScalar));CHKERRQ(ierr);
  
  /* put together the new symbolic matrix */
  ierr = MatCreateSeqAIJWithArrays(((PetscObject)A)->comm,am,bn,ci,cj,ca,C);CHKERRQ(ierr);

  /* MatCreateSeqAIJWithArrays flags matrix so PETSc doesn't free the user's arrays. */
  /* These are PETSc arrays, so change flags so arrays can be deleted by PETSc */
  c = (Mat_SeqAIJ *)((*C)->data);
  c->free_a   = PETSC_TRUE;
  c->free_ij  = PETSC_TRUE;
  c->nonew    = 0;

  /* set MatInfo */
  afill = (PetscReal)ci[am]/(ai[am]+bi[bm]) + 1.e-5;
  if (afill < 1.0) afill = 1.0;
  c->maxnz                     = ci[am]; 
  c->nz                        = ci[am];
  (*C)->info.mallocs           = nspacedouble;
  (*C)->info.fill_ratio_given  = fill;               
  (*C)->info.fill_ratio_needed = afill; 

#if defined(PETSC_USE_INFO)
  if (ci[am]) {
    ierr = PetscInfo3((*C),"Reallocs %D; Fill ratio: given %G needed %G.\n",nspacedouble,fill,afill);CHKERRQ(ierr);
    ierr = PetscInfo1((*C),"Use MatMatMult(A,B,MatReuse,%G,&C) for best performance.;\n",afill);CHKERRQ(ierr);
  } else {
    ierr = PetscInfo((*C),"Empty matrix product\n");CHKERRQ(ierr);
  }
#endif
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "MatMatMultNumeric_SeqAIJ_SeqAIJ"
PetscErrorCode MatMatMultNumeric_SeqAIJ_SeqAIJ(Mat A,Mat B,Mat C)
{
  PetscErrorCode ierr;
  PetscLogDouble flops=0.0;
  Mat_SeqAIJ     *a = (Mat_SeqAIJ *)A->data;
  Mat_SeqAIJ     *b = (Mat_SeqAIJ *)B->data;
  Mat_SeqAIJ     *c = (Mat_SeqAIJ *)C->data;
  PetscInt       *ai=a->i,*aj=a->j,*bi=b->i,*bj=b->j,*bjj,*ci=c->i,*cj=c->j;
  PetscInt       am=A->rmap->N,cm=C->rmap->N;
  PetscInt       i,j,k,anzi,bnzi,cnzi,brow,nextb;
  MatScalar      *aa=a->a,*ba=b->a,*baj,*ca=c->a; 

  PetscFunctionBegin;  
  /* clean old values in C */
  ierr = PetscMemzero(ca,ci[cm]*sizeof(MatScalar));CHKERRQ(ierr);
  /* Traverse A row-wise. */
  /* Build the ith row in C by summing over nonzero columns in A, */
  /* the rows of B corresponding to nonzeros of A. */
  for (i=0;i<am;i++) {
    anzi = ai[i+1] - ai[i];
    for (j=0;j<anzi;j++) {
      brow = *aj++;
      bnzi = bi[brow+1] - bi[brow];
      bjj  = bj + bi[brow];
      baj  = ba + bi[brow];
      nextb = 0;
      for (k=0; nextb<bnzi; k++) {
        if (cj[k] == bjj[nextb]){ /* ccol == bcol */
          ca[k] += (*aa)*baj[nextb++];
        }
      }
      flops += 2*bnzi;
      aa++;
    }
    cnzi = ci[i+1] - ci[i];
    ca += cnzi;
    cj += cnzi;
  }
  ierr = MatAssemblyBegin(C,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(C,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);     

  ierr = PetscLogFlops(flops);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "MatMatMultTranspose_SeqAIJ_SeqAIJ"
PetscErrorCode MatMatMultTranspose_SeqAIJ_SeqAIJ(Mat A,Mat B,MatReuse scall,PetscReal fill,Mat *C)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (scall == MAT_INITIAL_MATRIX){
    ierr = MatMatMultTransposeSymbolic_SeqAIJ_SeqAIJ(A,B,fill,C);CHKERRQ(ierr);
  }
  ierr = MatMatMultTransposeNumeric_SeqAIJ_SeqAIJ(A,B,*C);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscContainerDestroy_Mat_MatMatMultTrans"
PetscErrorCode PetscContainerDestroy_Mat_MatMatMultTrans(void *ptr)
{
  PetscErrorCode      ierr;
  Mat_MatMatMultTrans *multtrans=(Mat_MatMatMultTrans*)ptr;

  PetscFunctionBegin;
  ierr = MatMultTransposeColoringDestroy(&multtrans->matcoloring);CHKERRQ(ierr);
  ierr = MatDestroy(&multtrans->Bt_den);CHKERRQ(ierr);
  ierr = MatDestroy(&multtrans->ABt_den);CHKERRQ(ierr);
  ierr = PetscFree(multtrans);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "MatDestroy_SeqAIJ_MatMatMultTrans"
PetscErrorCode MatDestroy_SeqAIJ_MatMatMultTrans(Mat A)
{
  PetscErrorCode      ierr;
  PetscContainer      container;
  Mat_MatMatMultTrans *multtrans=PETSC_NULL;

  PetscFunctionBegin;
  ierr = PetscObjectQuery((PetscObject)A,"Mat_MatMatMultTrans",(PetscObject *)&container);CHKERRQ(ierr);
  if (!container) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_PLIB,"Container does not exit");
  ierr = PetscContainerGetPointer(container,(void **)&multtrans);CHKERRQ(ierr);
  A->ops->destroy   = multtrans->destroy;
  if (A->ops->destroy) {
    ierr = (*A->ops->destroy)(A);CHKERRQ(ierr);
  }
  ierr = PetscObjectCompose((PetscObject)A,"Mat_MatMatMultTrans",0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMatMultTransposeSymbolic_SeqAIJ_SeqAIJ"
PetscErrorCode MatMatMultTransposeSymbolic_SeqAIJ_SeqAIJ(Mat A,Mat B,PetscReal fill,Mat *C)
{
  PetscErrorCode      ierr;
  Mat                 Bt;
  PetscInt            *bti,*btj;
  Mat_MatMatMultTrans *multtrans;
  PetscContainer      container;
  
  PetscFunctionBegin;
   /* create symbolic Bt */
  ierr = MatGetSymbolicTranspose_SeqAIJ(B,&bti,&btj);CHKERRQ(ierr);
  ierr = MatCreateSeqAIJWithArrays(PETSC_COMM_SELF,B->cmap->n,B->rmap->n,bti,btj,PETSC_NULL,&Bt);CHKERRQ(ierr);

  /* get symbolic C=A*Bt */
  ierr = MatMatMultSymbolic_SeqAIJ_SeqAIJ(A,Bt,fill,C);CHKERRQ(ierr);

  /* clean up */
  ierr = MatDestroy(&Bt);CHKERRQ(ierr);
  ierr = MatRestoreSymbolicTranspose_SeqAIJ(B,&bti,&btj);CHKERRQ(ierr);

  /* create a supporting struct for reuse intermidiate dense matrices with matcoloring */
  ierr = PetscNew(Mat_MatMatMultTrans,&multtrans);CHKERRQ(ierr);

  /* attach the supporting struct to C */
  ierr = PetscContainerCreate(PETSC_COMM_SELF,&container);CHKERRQ(ierr);
  ierr = PetscContainerSetPointer(container,multtrans);CHKERRQ(ierr);
  ierr = PetscContainerSetUserDestroy(container,PetscContainerDestroy_Mat_MatMatMultTrans);CHKERRQ(ierr);
  ierr = PetscObjectCompose((PetscObject)(*C),"Mat_MatMatMultTrans",(PetscObject)container);CHKERRQ(ierr);
  ierr = PetscContainerDestroy(&container);CHKERRQ(ierr);

  multtrans->usecoloring = PETSC_FALSE;
  multtrans->destroy = (*C)->ops->destroy;
  (*C)->ops->destroy = MatDestroy_SeqAIJ_MatMatMultTrans;

  ierr = PetscOptionsGetBool(PETSC_NULL,"-matmulttrans_color",&multtrans->usecoloring,PETSC_NULL);CHKERRQ(ierr);
  if (multtrans->usecoloring){
    /* Create MatMultTransposeColoring from symbolic C=A*B^T */
    MatMultTransposeColoring  matcoloring;
    ISColoring                iscoloring;
    Mat                       Bt_dense,C_dense;
    PetscLogDouble            t0,tf,etime0=0.0,etime1=0.0;

    ierr = PetscGetTime(&t0);CHKERRQ(ierr);
    ierr = MatGetColoring(*C,MATCOLORINGSL,&iscoloring);CHKERRQ(ierr); 
    ierr = MatMultTransposeColoringCreate(*C,iscoloring,&matcoloring);CHKERRQ(ierr);
    multtrans->matcoloring = matcoloring;
    ierr = ISColoringDestroy(&iscoloring);CHKERRQ(ierr);
    ierr = PetscGetTime(&tf);CHKERRQ(ierr);
    etime0 += tf - t0;

    ierr = PetscGetTime(&t0);CHKERRQ(ierr);
    /* Create Bt_dense and C_dense = A*Bt_dense */
    ierr = MatCreate(PETSC_COMM_SELF,&Bt_dense);CHKERRQ(ierr);
    ierr = MatSetSizes(Bt_dense,A->cmap->n,matcoloring->ncolors,A->cmap->n,matcoloring->ncolors);CHKERRQ(ierr);
    ierr = MatSetType(Bt_dense,MATSEQDENSE);CHKERRQ(ierr);
    ierr = MatSeqDenseSetPreallocation(Bt_dense,PETSC_NULL);CHKERRQ(ierr);
    Bt_dense->assembled = PETSC_TRUE;
    multtrans->Bt_den = Bt_dense;

    ierr = MatCreate(PETSC_COMM_SELF,&C_dense);CHKERRQ(ierr);
    ierr = MatSetSizes(C_dense,A->rmap->n,matcoloring->ncolors,A->rmap->n,matcoloring->ncolors);CHKERRQ(ierr);
    ierr = MatSetType(C_dense,MATSEQDENSE);CHKERRQ(ierr);
    ierr = MatSeqDenseSetPreallocation(C_dense,PETSC_NULL);CHKERRQ(ierr);
    Bt_dense->assembled = PETSC_TRUE;
    multtrans->ABt_den = C_dense;
    ierr = PetscGetTime(&tf);CHKERRQ(ierr);
    etime1 += tf - t0;
    printf("MatMultTransColorCreate %g, MatDenseCreate %g\n",etime0,etime1);
  }
  
#if defined(INEFFICIENT_ALGORITHM)
  /* The algorithm below computes am*bm sparse inner-product - inefficient! It will be deleted later. */
  PetscFreeSpaceList free_space=PETSC_NULL,current_space=PETSC_NULL;
  Mat_SeqAIJ         *a=(Mat_SeqAIJ*)A->data,*b=(Mat_SeqAIJ*)B->data,*c;
  PetscInt           *ai=a->i,*aj=a->j,*bi=b->i,*bj=b->j,*ci,*cj,*acol,*bcol;
  PetscInt           am=A->rmap->N,bm=B->rmap->N;
  PetscInt           i,j,anzi,bnzj,cnzi,nlnk,*lnk,nspacedouble=0,ka,kb,index[1];
  MatScalar          *ca;
  PetscBT            lnkbt;
  PetscReal          afill;

  /* Allocate row pointer array ci  */
  ierr = PetscMalloc(((am+1)+1)*sizeof(PetscInt),&ci);CHKERRQ(ierr);
  ci[0] = 0;
  
  /* Create and initialize a linked list for C columns */
  nlnk = bm+1;
  ierr = PetscLLCreate(bm,bm,nlnk,lnk,lnkbt);CHKERRQ(ierr);

  /* Initial FreeSpace with size fill*(nnz(A)+nnz(B)) */
  ierr = PetscFreeSpaceGet((PetscInt)(fill*(ai[am]+bi[bm])),&free_space);CHKERRQ(ierr);
  current_space = free_space;

  /* Determine symbolic info for each row of the product A*B^T: */
  for (i=0; i<am; i++) {
    anzi = ai[i+1] - ai[i];
    cnzi = 0;
    acol = aj + ai[i];
    for (j=0; j<bm; j++){
      bnzj = bi[j+1] - bi[j];
      bcol= bj + bi[j];
      /* sparse inner-product c(i,j)=A[i,:]*B[j,:]^T */
      ka = 0; kb = 0; 
      while (ka < anzi && kb < bnzj){
        while (acol[ka] < bcol[kb] && ka < anzi) ka++; 
        if (ka == anzi) break;
        while (acol[ka] > bcol[kb] && kb < bnzj) kb++;
        if (kb == bnzj) break;
        if (acol[ka] == bcol[kb]){ /* add nonzero c(i,j) to lnk */
          index[0] = j;
          ierr = PetscLLAdd(1,index,bm,nlnk,lnk,lnkbt);CHKERRQ(ierr);
          cnzi++;
          break;
        }
      }
    }
    
    /* If free space is not available, make more free space */
    /* Double the amount of total space in the list */
    if (current_space->local_remaining<cnzi) {
      ierr = PetscFreeSpaceGet(cnzi+current_space->total_array_size,&current_space);CHKERRQ(ierr);
      nspacedouble++;
    }

    /* Copy data into free space, then initialize lnk */
    ierr = PetscLLClean(bm,bm,cnzi,lnk,current_space->array,lnkbt);CHKERRQ(ierr);
    current_space->array           += cnzi;
    current_space->local_used      += cnzi;
    current_space->local_remaining -= cnzi;
 
    ci[i+1] = ci[i] + cnzi;
  }
  

  /* Column indices are in the list of free space. 
     Allocate array cj, copy column indices to cj, and destroy list of free space */
  ierr = PetscMalloc((ci[am]+1)*sizeof(PetscInt),&cj);CHKERRQ(ierr);
  ierr = PetscFreeSpaceContiguous(&free_space,cj);CHKERRQ(ierr);
  ierr = PetscLLDestroy(lnk,lnkbt);CHKERRQ(ierr);
    
  /* Allocate space for ca */
  ierr = PetscMalloc((ci[am]+1)*sizeof(MatScalar),&ca);CHKERRQ(ierr);
  ierr = PetscMemzero(ca,(ci[am]+1)*sizeof(MatScalar));CHKERRQ(ierr);
  
  /* put together the new symbolic matrix */
  ierr = MatCreateSeqAIJWithArrays(((PetscObject)A)->comm,am,bm,ci,cj,ca,C);CHKERRQ(ierr);

  /* MatCreateSeqAIJWithArrays flags matrix so PETSc doesn't free the user's arrays. */
  /* These are PETSc arrays, so change flags so arrays can be deleted by PETSc */
  c = (Mat_SeqAIJ *)((*C)->data);
  c->free_a   = PETSC_TRUE;
  c->free_ij  = PETSC_TRUE;
  c->nonew    = 0;

  /* set MatInfo */
  afill = (PetscReal)ci[am]/(ai[am]+bi[bm]) + 1.e-5;
  if (afill < 1.0) afill = 1.0;
  c->maxnz                     = ci[am]; 
  c->nz                        = ci[am];
  (*C)->info.mallocs           = nspacedouble;
  (*C)->info.fill_ratio_given  = fill;               
  (*C)->info.fill_ratio_needed = afill; 

#if defined(PETSC_USE_INFO)
  if (ci[am]) {
    ierr = PetscInfo3((*C),"Reallocs %D; Fill ratio: given %G needed %G.\n",nspacedouble,fill,afill);CHKERRQ(ierr);
    ierr = PetscInfo1((*C),"Use MatMatMultTranspose(A,B,MatReuse,%G,&C) for best performance.;\n",afill);CHKERRQ(ierr);
  } else {
    ierr = PetscInfo((*C),"Empty matrix product\n");CHKERRQ(ierr);
  }
#endif
#endif
  PetscFunctionReturn(0);
}

/* #define USE_ARRAY - for sparse dot product. Slower than !USE_ARRAY */
#undef __FUNCT__  
#define __FUNCT__ "MatMatMultTransposeNumeric_SeqAIJ_SeqAIJ"
PetscErrorCode MatMatMultTransposeNumeric_SeqAIJ_SeqAIJ(Mat A,Mat B,Mat C)
{
  PetscErrorCode ierr; 
  Mat_SeqAIJ     *a=(Mat_SeqAIJ*)A->data,*b=(Mat_SeqAIJ*)B->data,*c=(Mat_SeqAIJ*)C->data;
  PetscInt       *ai=a->i,*aj=a->j,*bi=b->i,*bj=b->j,anzi,bnzj,nexta,nextb,*acol,*bcol,brow;
  PetscInt       cm=C->rmap->n,*ci=c->i,*cj=c->j,i,j,cnzi,*ccol;
  PetscLogDouble flops=0.0;
  MatScalar      *aa=a->a,*aval,*ba=b->a,*bval,*ca=c->a,*cval;
  Mat_MatMatMultTrans *multtrans;
  PetscContainer      container;
#if defined(USE_ARRAY)
  MatScalar      *spdot;
#endif

  PetscFunctionBegin;
  ierr = PetscObjectQuery((PetscObject)C,"Mat_MatMatMultTrans",(PetscObject *)&container);CHKERRQ(ierr);
  if (!container) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_PLIB,"Container does not exit");
  ierr  = PetscContainerGetPointer(container,(void **)&multtrans);CHKERRQ(ierr);
  if (multtrans->usecoloring){
    MatMultTransposeColoring  matcoloring = multtrans->matcoloring;
    Mat                       Bt_dense;
    PetscInt                  m,n;
    PetscLogDouble t0,tf,etime0=0.0,etime1=0.0,etime2=0.0;
    Mat C_dense = multtrans->ABt_den;

    Bt_dense = multtrans->Bt_den;
    ierr = MatGetLocalSize(Bt_dense,&m,&n);CHKERRQ(ierr);
    printf("Bt_dense: %d,%d\n",m,n); 

    /* Get Bt_dense by Apply MatMultTransposeColoring to B */
    ierr = PetscGetTime(&t0);CHKERRQ(ierr);
    ierr = MatMultTransposeColoringApply(B,Bt_dense,matcoloring);CHKERRQ(ierr);
    ierr = PetscGetTime(&tf);CHKERRQ(ierr);
    etime0 += tf - t0;

    /* C_dense = A*Bt_dense */
    ierr = PetscGetTime(&t0);CHKERRQ(ierr);
    ierr = MatMatMultNumeric_SeqAIJ_SeqDense(A,Bt_dense,C_dense);CHKERRQ(ierr);
    ierr = PetscGetTime(&tf);CHKERRQ(ierr);
    etime2 += tf - t0;
  
    /* Recover C from C_dense */
    ierr = PetscGetTime(&t0);CHKERRQ(ierr);
    ierr = MatMultTransColoringApplyDenToSp(matcoloring,C_dense,C);CHKERRQ(ierr);
    ierr = PetscGetTime(&tf);CHKERRQ(ierr);
    etime1 += tf - t0;
    printf("etime ColoringApply: %g %g; MatMatMultNumeric_sp_dense: %g\n",etime0,etime1,etime2);
    PetscFunctionReturn(0);
  }

#if defined(USE_ARRAY)
  /* allocate an array for implementing sparse inner-product */
  ierr = PetscMalloc((A->cmap->n+1)*sizeof(MatScalar),&spdot);CHKERRQ(ierr);
  ierr = PetscMemzero(spdot,(A->cmap->n+1)*sizeof(MatScalar));CHKERRQ(ierr);
#endif

  /* clear old values in C */
  ierr = PetscMemzero(ca,ci[cm]*sizeof(MatScalar));CHKERRQ(ierr);

  for (i=0; i<cm; i++) {
    anzi = ai[i+1] - ai[i];
    acol = aj + ai[i];
    aval = aa + ai[i];
    cnzi = ci[i+1] - ci[i];
    ccol = cj + ci[i];
    cval = ca + ci[i];
    for (j=0; j<cnzi; j++){
      brow = ccol[j];
      bnzj = bi[brow+1] - bi[brow];
      bcol = bj + bi[brow];
      bval = ba + bi[brow];

      /* perform sparse inner-product c(i,j)=A[i,:]*B[j,:]^T */
#if defined(USE_ARRAY)
      /* put ba to spdot array */
      for (nextb=0; nextb<bnzj; nextb++) spdot[bcol[nextb]] = bval[nextb]; 
      /* c(i,j)=A[i,:]*B[j,:]^T */
      for (nexta=0; nexta<anzi; nexta++){
        cval[j] += spdot[acol[nexta]]*aval[nexta]; 
      }
      /* zero spdot array */
      for (nextb=0; nextb<bnzj; nextb++) spdot[bcol[nextb]] = 0.0;
#else
      nexta = 0; nextb = 0;
      while (nexta<anzi && nextb<bnzj){
        while (acol[nexta] < bcol[nextb] && nexta < anzi) nexta++;
        if (nexta == anzi) break;
        while (acol[nexta] > bcol[nextb] && nextb < bnzj) nextb++;
        if (nextb == bnzj) break;
        if (acol[nexta] == bcol[nextb]){ 
          cval[j] += aval[nexta]*bval[nextb];
          nexta++; nextb++; 
          flops += 2;
        }
      }
#endif
    }
  }
  ierr = MatAssemblyBegin(C,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(C,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);   
  ierr = PetscLogFlops(flops);CHKERRQ(ierr);
#if defined(USE_ARRAY)
  ierr = PetscFree(spdot);
#endif
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "MatMatTransposeMult_SeqAIJ_SeqAIJ"
PetscErrorCode MatMatTransposeMult_SeqAIJ_SeqAIJ(Mat A,Mat B,MatReuse scall,PetscReal fill,Mat *C) {
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (scall == MAT_INITIAL_MATRIX){
    ierr = MatMatTransposeMultSymbolic_SeqAIJ_SeqAIJ(A,B,fill,C);CHKERRQ(ierr);
  }
  ierr = MatMatTransposeMultNumeric_SeqAIJ_SeqAIJ(A,B,*C);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMatTransposeMultSymbolic_SeqAIJ_SeqAIJ"
PetscErrorCode MatMatTransposeMultSymbolic_SeqAIJ_SeqAIJ(Mat A,Mat B,PetscReal fill,Mat *C)
{
  PetscErrorCode ierr;
  Mat            At;
  PetscInt       *ati,*atj;

  PetscFunctionBegin;
  /* create symbolic At */
  ierr = MatGetSymbolicTranspose_SeqAIJ(A,&ati,&atj);CHKERRQ(ierr);
  ierr = MatCreateSeqAIJWithArrays(PETSC_COMM_SELF,A->cmap->n,A->rmap->n,ati,atj,PETSC_NULL,&At);CHKERRQ(ierr);

  /* get symbolic C=At*B */
  ierr = MatMatMultSymbolic_SeqAIJ_SeqAIJ(At,B,fill,C);CHKERRQ(ierr);

  /* clean up */
  ierr = MatDestroy(&At);CHKERRQ(ierr);
  ierr = MatRestoreSymbolicTranspose_SeqAIJ(A,&ati,&atj);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMatTransposeMultNumeric_SeqAIJ_SeqAIJ"
PetscErrorCode MatMatTransposeMultNumeric_SeqAIJ_SeqAIJ(Mat A,Mat B,Mat C)
{
  PetscErrorCode ierr; 
  Mat_SeqAIJ     *a=(Mat_SeqAIJ*)A->data,*b=(Mat_SeqAIJ*)B->data,*c=(Mat_SeqAIJ*)C->data;
  PetscInt       am=A->rmap->n,anzi,*ai=a->i,*aj=a->j,*bi=b->i,*bj,bnzi,nextb;
  PetscInt       cm=C->rmap->n,*ci=c->i,*cj=c->j,crow,*cjj,i,j,k;
  PetscLogDouble flops=0.0;
  MatScalar      *aa=a->a,*ba,*ca=c->a,*caj;
 
  PetscFunctionBegin;
  /* clear old values in C */
  ierr = PetscMemzero(ca,ci[cm]*sizeof(MatScalar));CHKERRQ(ierr);

  /* compute A^T*B using outer product (A^T)[:,i]*B[i,:] */
  for (i=0;i<am;i++) {
    bj   = b->j + bi[i];
    ba   = b->a + bi[i];
    bnzi = bi[i+1] - bi[i];
    anzi = ai[i+1] - ai[i];
    for (j=0; j<anzi; j++) { 
      nextb = 0;
      crow  = *aj++;
      cjj   = cj + ci[crow];
      caj   = ca + ci[crow];
      /* perform sparse axpy operation.  Note cjj includes bj. */
      for (k=0; nextb<bnzi; k++) {
        if (cjj[k] == *(bj+nextb)) { /* ccol == bcol */
          caj[k] += (*aa)*(*(ba+nextb));
          nextb++;
        }
      }
      flops += 2*bnzi;
      aa++;
    }
  }

  /* Assemble the final matrix and clean up */
  ierr = MatAssemblyBegin(C,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(C,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = PetscLogFlops(flops);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

EXTERN_C_BEGIN
#undef __FUNCT__
#define __FUNCT__ "MatMatMult_SeqAIJ_SeqDense"
PetscErrorCode MatMatMult_SeqAIJ_SeqDense(Mat A,Mat B,MatReuse scall,PetscReal fill,Mat *C) 
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (scall == MAT_INITIAL_MATRIX){
    ierr = MatMatMultSymbolic_SeqAIJ_SeqDense(A,B,fill,C);CHKERRQ(ierr);
  }
  ierr = MatMatMultNumeric_SeqAIJ_SeqDense(A,B,*C);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__
#define __FUNCT__ "MatMatMultSymbolic_SeqAIJ_SeqDense"
PetscErrorCode MatMatMultSymbolic_SeqAIJ_SeqDense(Mat A,Mat B,PetscReal fill,Mat *C) 
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = MatMatMultSymbolic_SeqDense_SeqDense(A,B,0.0,C);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMatMultNumeric_SeqAIJ_SeqDense"
PetscErrorCode MatMatMultNumeric_SeqAIJ_SeqDense(Mat A,Mat B,Mat C)
{
  Mat_SeqAIJ     *a=(Mat_SeqAIJ*)A->data;
  PetscErrorCode ierr;
  PetscScalar    *b,*c,r1,r2,r3,r4,*b1,*b2,*b3,*b4;
  MatScalar      *aa;
  PetscInt       cm=C->rmap->n, cn=B->cmap->n, bm=B->rmap->n, col, i,j,n,*aj, am = A->rmap->n;
  PetscInt       am2 = 2*am, am3 = 3*am,  bm4 = 4*bm,colam;

  PetscFunctionBegin;
  if (!cm || !cn) PetscFunctionReturn(0);
  if (bm != A->cmap->n) SETERRQ2(PETSC_COMM_SELF,PETSC_ERR_ARG_SIZ,"Number columns in A %D not equal rows in B %D\n",A->cmap->n,bm);
  if (A->rmap->n != C->rmap->n) SETERRQ2(PETSC_COMM_SELF,PETSC_ERR_ARG_SIZ,"Number rows in C %D not equal rows in A %D\n",C->rmap->n,A->rmap->n);
  if (B->cmap->n != C->cmap->n) SETERRQ2(PETSC_COMM_SELF,PETSC_ERR_ARG_SIZ,"Number columns in B %D not equal columns in C %D\n",B->cmap->n,C->cmap->n);
  ierr = MatGetArray(B,&b);CHKERRQ(ierr);
  ierr = MatGetArray(C,&c);CHKERRQ(ierr);
  b1 = b; b2 = b1 + bm; b3 = b2 + bm; b4 = b3 + bm;
  for (col=0; col<cn-4; col += 4){  /* over columns of C */
    colam = col*am;
    for (i=0; i<am; i++) {        /* over rows of C in those columns */
      r1 = r2 = r3 = r4 = 0.0;
      n   = a->i[i+1] - a->i[i]; 
      aj  = a->j + a->i[i];
      aa  = a->a + a->i[i];
      for (j=0; j<n; j++) {
        r1 += (*aa)*b1[*aj]; 
        r2 += (*aa)*b2[*aj]; 
        r3 += (*aa)*b3[*aj]; 
        r4 += (*aa++)*b4[*aj++]; 
      }
      c[colam + i]       = r1;
      c[colam + am + i]  = r2;
      c[colam + am2 + i] = r3;
      c[colam + am3 + i] = r4;
    }
    b1 += bm4;
    b2 += bm4;
    b3 += bm4;
    b4 += bm4;
  }
  for (;col<cn; col++){     /* over extra columns of C */
    for (i=0; i<am; i++) {  /* over rows of C in those columns */
      r1 = 0.0;
      n   = a->i[i+1] - a->i[i]; 
      aj  = a->j + a->i[i];
      aa  = a->a + a->i[i];

      for (j=0; j<n; j++) {
        r1 += (*aa++)*b1[*aj++]; 
      }
      c[col*am + i]     = r1;
    }
    b1 += bm;
  }
  ierr = PetscLogFlops(cn*(2.0*a->nz));CHKERRQ(ierr);
  ierr = MatRestoreArray(B,&b);CHKERRQ(ierr);
  ierr = MatRestoreArray(C,&c);CHKERRQ(ierr);
  ierr = MatAssemblyBegin(C,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(C,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*
   Note very similar to MatMult_SeqAIJ(), should generate both codes from same base
*/
#undef __FUNCT__  
#define __FUNCT__ "MatMatMultNumericAdd_SeqAIJ_SeqDense"
PetscErrorCode MatMatMultNumericAdd_SeqAIJ_SeqDense(Mat A,Mat B,Mat C)
{
  Mat_SeqAIJ     *a=(Mat_SeqAIJ*)A->data;
  PetscErrorCode ierr;
  PetscScalar    *b,*c,r1,r2,r3,r4,*b1,*b2,*b3,*b4;
  MatScalar      *aa;
  PetscInt       cm=C->rmap->n, cn=B->cmap->n, bm=B->rmap->n, col, i,j,n,*aj, am = A->rmap->n,*ii,arm;
  PetscInt       am2 = 2*am, am3 = 3*am,  bm4 = 4*bm,colam,*ridx;

  PetscFunctionBegin;
  if (!cm || !cn) PetscFunctionReturn(0);
  ierr = MatGetArray(B,&b);CHKERRQ(ierr);
  ierr = MatGetArray(C,&c);CHKERRQ(ierr);
  b1 = b; b2 = b1 + bm; b3 = b2 + bm; b4 = b3 + bm;

  if (a->compressedrow.use){ /* use compressed row format */
    for (col=0; col<cn-4; col += 4){  /* over columns of C */
      colam = col*am;
      arm   = a->compressedrow.nrows;
      ii    = a->compressedrow.i;
      ridx  = a->compressedrow.rindex;
      for (i=0; i<arm; i++) {        /* over rows of C in those columns */
	r1 = r2 = r3 = r4 = 0.0;
	n   = ii[i+1] - ii[i]; 
	aj  = a->j + ii[i];
	aa  = a->a + ii[i];
	for (j=0; j<n; j++) {
	  r1 += (*aa)*b1[*aj]; 
	  r2 += (*aa)*b2[*aj]; 
	  r3 += (*aa)*b3[*aj]; 
	  r4 += (*aa++)*b4[*aj++]; 
	}
	c[colam       + ridx[i]] += r1;
	c[colam + am  + ridx[i]] += r2;
	c[colam + am2 + ridx[i]] += r3;
	c[colam + am3 + ridx[i]] += r4;
      }
      b1 += bm4;
      b2 += bm4;
      b3 += bm4;
      b4 += bm4;
    }
    for (;col<cn; col++){     /* over extra columns of C */
      colam = col*am;
      arm   = a->compressedrow.nrows;
      ii    = a->compressedrow.i;
      ridx  = a->compressedrow.rindex;
      for (i=0; i<arm; i++) {  /* over rows of C in those columns */
	r1 = 0.0;
	n   = ii[i+1] - ii[i]; 
	aj  = a->j + ii[i];
	aa  = a->a + ii[i];

	for (j=0; j<n; j++) {
	  r1 += (*aa++)*b1[*aj++]; 
	}
	c[col*am + ridx[i]] += r1;
      }
      b1 += bm;
    }
  } else {
    for (col=0; col<cn-4; col += 4){  /* over columns of C */
      colam = col*am;
      for (i=0; i<am; i++) {        /* over rows of C in those columns */
	r1 = r2 = r3 = r4 = 0.0;
	n   = a->i[i+1] - a->i[i]; 
	aj  = a->j + a->i[i];
	aa  = a->a + a->i[i];
	for (j=0; j<n; j++) {
	  r1 += (*aa)*b1[*aj]; 
	  r2 += (*aa)*b2[*aj]; 
	  r3 += (*aa)*b3[*aj]; 
	  r4 += (*aa++)*b4[*aj++]; 
	}
	c[colam + i]       += r1;
	c[colam + am + i]  += r2;
	c[colam + am2 + i] += r3;
	c[colam + am3 + i] += r4;
      }
      b1 += bm4;
      b2 += bm4;
      b3 += bm4;
      b4 += bm4;
    }
    for (;col<cn; col++){     /* over extra columns of C */
      for (i=0; i<am; i++) {  /* over rows of C in those columns */
	r1 = 0.0;
	n   = a->i[i+1] - a->i[i]; 
	aj  = a->j + a->i[i];
	aa  = a->a + a->i[i];

	for (j=0; j<n; j++) {
	  r1 += (*aa++)*b1[*aj++]; 
	}
	c[col*am + i]     += r1;
      }
      b1 += bm;
    }
  }
  ierr = PetscLogFlops(cn*2.0*a->nz);CHKERRQ(ierr);
  ierr = MatRestoreArray(B,&b);CHKERRQ(ierr);
  ierr = MatRestoreArray(C,&c);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMultTransposeColoringApply_SeqAIJ"
PetscErrorCode  MatMultTransposeColoringApply_SeqAIJ(Mat B,Mat Btdense,MatMultTransposeColoring coloring)
{
  PetscErrorCode ierr;
  Mat_SeqAIJ     *a = (Mat_SeqAIJ*)B->data;
  Mat_SeqDense   *atdense = (Mat_SeqDense*)Btdense->data;
  PetscInt       m=Btdense->rmap->n,n=Btdense->cmap->n,j,k,l,col,anz,*atcol,brow,bcol;
  MatScalar      *atval,*bval;

  PetscFunctionBegin;    
  ierr = PetscMemzero(atdense->v,(m*n)*sizeof(MatScalar));CHKERRQ(ierr);
  for (k=0; k<coloring->ncolors; k++) { 
    for (l=0; l<coloring->ncolumns[k]; l++) { /* insert a row of B to a column of Btdense */
      col = coloring->columns[k][l];   /* =row of B */
      anz = a->i[col+1] - a->i[col];
      for (j=0; j<anz; j++){
        atcol = a->j + a->i[col];
        atval = a->a + a->i[col]; 
        bval  = atdense->v;
        brow  = atcol[j]; 
        bcol  = k;
        bval[bcol*m+brow] = atval[j];
      }
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMultTransColoringApplyDenToSp_SeqAIJ"
PetscErrorCode MatMultTransColoringApplyDenToSp_SeqAIJ(MatMultTransposeColoring matcoloring,Mat Cden,Mat Csp)
{
  PetscErrorCode ierr;
  Mat_SeqAIJ     *csp = (Mat_SeqAIJ*)Csp->data;
  PetscInt       k,l,row,m,**rows=matcoloring->rows,**spidx=matcoloring->columnsforspidx;
  PetscScalar    *ca_den,*cp_den,*ca=csp->a;
 
  PetscFunctionBegin;    
  ierr = MatGetLocalSize(Csp,&m,PETSC_NULL);CHKERRQ(ierr);
  ierr = MatGetArray(Cden,&ca_den);CHKERRQ(ierr);
  cp_den = ca_den;
  for (k=0; k<matcoloring->ncolors; k++) { 
    for (l=0; l<matcoloring->nrows[k]; l++){
      row             = rows[k][l];       
      ca[spidx[k][l]] = cp_den[row];
    }
    cp_den += m;
  }
  ierr = MatRestoreArray(Cden,&ca_den);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "MatMultTransposeColoringCreate_SeqAIJ"
PetscErrorCode MatMultTransposeColoringCreate_SeqAIJ(Mat mat,ISColoring iscoloring,MatMultTransposeColoring c)
{
  PetscErrorCode ierr;
  PetscInt       i,n,nrows,N,j,k,m,*rows,*ci,*cj,ncols,col,cm;
  const PetscInt *is;
  PetscInt       nis = iscoloring->n,*rowhit,bs = 1;
  IS             *isa;
  PetscBool      done;
  PetscBool      flg1,flg2;
  Mat_SeqAIJ     *csp = (Mat_SeqAIJ*)mat->data;
  PetscInt       l,row,*rp,low,high,*cilen = csp->ilen,t;
#if defined(PETSC_USE_DEBUG)
  PetscBool      found; 
#endif

  PetscFunctionBegin;
  ierr = ISColoringGetIS(iscoloring,PETSC_IGNORE,&isa);CHKERRQ(ierr);
  /* this is ugly way to get blocksize but cannot call MatGetBlockSize() because AIJ can have bs > 1 */
  ierr = PetscTypeCompare((PetscObject)mat,MATSEQBAIJ,&flg1);CHKERRQ(ierr);
  ierr = PetscTypeCompare((PetscObject)mat,MATMPIBAIJ,&flg2);CHKERRQ(ierr);
  if (flg1 || flg2) {
    ierr = MatGetBlockSize(mat,&bs);CHKERRQ(ierr);
  }

  N          = mat->cmap->N/bs;
  c->M       = mat->rmap->N/bs;  /* set total rows, columns and local rows */
  c->N       = mat->cmap->N/bs;
  c->m       = mat->rmap->N/bs;
  c->rstart  = 0;

  c->ncolors = nis;
  ierr       = PetscMalloc(nis*sizeof(PetscInt),&c->ncolumns);CHKERRQ(ierr);
  ierr       = PetscMalloc(nis*sizeof(PetscInt*),&c->columns);CHKERRQ(ierr); 
  ierr       = PetscMalloc(nis*sizeof(PetscInt),&c->nrows);CHKERRQ(ierr); 
  ierr       = PetscMalloc(nis*sizeof(PetscInt*),&c->rows);CHKERRQ(ierr); 
  ierr       = PetscMalloc(nis*sizeof(PetscInt*),&c->columnsforrow);CHKERRQ(ierr); 
  ierr       = PetscMalloc(nis*sizeof(PetscInt*),&c->columnsforspidx);CHKERRQ(ierr); 
  
  ierr = MatGetColumnIJ(mat,0,PETSC_FALSE,PETSC_FALSE,&ncols,&ci,&cj,&done);CHKERRQ(ierr); // column-wise storage!!!
  if (!done) SETERRQ1(((PetscObject)mat)->comm,PETSC_ERR_SUP,"MatGetColumnIJ() not supported for matrix type %s",((PetscObject)mat)->type_name);

  cm = c->m; 
  ierr = PetscMalloc((cm+1)*sizeof(PetscInt),&rowhit);CHKERRQ(ierr);
  for (i=0; i<nis; i++) {
    ierr = ISGetLocalSize(isa[i],&n);CHKERRQ(ierr);
    ierr = ISGetIndices(isa[i],&is);CHKERRQ(ierr);
    c->ncolumns[i] = n;
    if (n) {
      ierr = PetscMalloc(n*sizeof(PetscInt),&c->columns[i]);CHKERRQ(ierr);
      ierr = PetscMemcpy(c->columns[i],is,n*sizeof(PetscInt));CHKERRQ(ierr);
    } else {
      c->columns[i]  = 0;
    }

    /* fast, crude version requires O(N*N) work */
    ierr = PetscMemzero(rowhit,cm*sizeof(PetscInt));CHKERRQ(ierr);
    /* loop over columns*/
    for (j=0; j<n; j++) {
      col  = is[j];
      rows = cj + ci[col]; 
      m    = ci[col+1] - ci[col];
      /* loop over columns marking them in rowhit */
      for (k=0; k<m; k++) {
        rowhit[*rows++] = col + 1;
      }
    }
    /* count the number of hits */
    nrows = 0;
    for (j=0; j<cm; j++) {
      if (rowhit[j]) nrows++;
    }
    c->nrows[i] = nrows;
    ierr = PetscMalloc3(nrows+1,PetscInt,&c->rows[i],nrows+1,PetscInt,&c->columnsforrow[i],nrows+1,PetscInt,&c->columnsforspidx[i]);CHKERRQ(ierr);
    nrows       = 0;
    for (j=0; j<cm; j++) {
      if (rowhit[j]) {
        c->rows[i][nrows]          = j; 
        c->columnsforrow[i][nrows] = rowhit[j] - 1; 
        nrows++;
      }
    }
    ierr = ISRestoreIndices(isa[i],&is);CHKERRQ(ierr);  
  }
  ierr = MatRestoreColumnIJ(mat,0,PETSC_FALSE,PETSC_FALSE,&ncols,&ci,&cj,&done);CHKERRQ(ierr);
  ierr = PetscFree(rowhit);CHKERRQ(ierr);
  ierr = ISColoringRestoreIS(iscoloring,&isa);CHKERRQ(ierr);

  /* set c->columnsforspidx to be used by MatMultTransColoringApplyDenToSp() in MatMatMultTransposeNumeric()  */
  ci = csp->i; cj = csp->j;
  for (k=0; k<c->ncolors; k++) { 
    for (l=0; l<c->nrows[k]; l++){
      row = c->rows[k][l];             /* local row index */
      col = c->columnsforrow[k][l];    /* global column index */
      /* below is optimized from  MatSetValues(Csp,1,&row,1,&col,cval+row,INSERT_VALUES); */
      rp   = cj + ci[row]; 
      low  = 0; high = cilen[row]; 
      while (high-low > 5) {
        t = (low+high)/2;
        if (rp[t] > col) high = t;
        else             low  = t;
      }
#if defined(PETSC_USE_DEBUG)
      found = PETSC_FALSE;
#endif
      for (i=low; i<high; i++) {
        if (rp[i] == col) {
          c->columnsforspidx[k][l] = ci[row] + i; /* index of mat(row,col) in csp->j and csp->a arrays */
#if defined(PETSC_USE_DEBUG)
          found = PETSC_TRUE;
#endif
          break;
        }
      } 
#if defined(PETSC_USE_DEBUG) 
      if (!found) SETERRQ2(PETSC_COMM_SELF,PETSC_ERR_ARG_OUTOFRANGE,"%d %d is not inserted",row,col);
#endif
    }
  }
  PetscFunctionReturn(0);
}
