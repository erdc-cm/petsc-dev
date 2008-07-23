#define PETSCMAT_DLL

#include "src/mat/impls/aij/seq/aij.h"
#include "src/inline/dot.h"
#include "src/inline/spops.h"
#include "petscbt.h"
#include "src/mat/utils/freespace.h"

#undef __FUNCT__  
#define __FUNCT__ "MatOrdering_Flow_SeqAIJ"
PetscErrorCode MatOrdering_Flow_SeqAIJ(Mat mat,const MatOrderingType type,IS *irow,IS *icol)
{
  PetscFunctionBegin;

  SETERRQ(PETSC_ERR_SUP,"Code not written");
#if !defined(PETSC_USE_DEBUG)
  PetscFunctionReturn(0);
#endif
}


#if !defined(PETSC_AVOID_GNUCOPYRIGHT_CODE)
EXTERN PetscErrorCode SPARSEKIT2dperm(PetscInt*,MatScalar*,PetscInt*,PetscInt*,MatScalar*,PetscInt*,PetscInt*,PetscInt*,PetscInt*,PetscInt*);
EXTERN PetscErrorCode SPARSEKIT2ilutp(PetscInt*,MatScalar*,PetscInt*,PetscInt*,PetscInt*,PetscReal,PetscReal*,PetscInt*,MatScalar*,PetscInt*,PetscInt*,PetscInt*,MatScalar*,PetscInt*,PetscInt*,PetscErrorCode*);
EXTERN PetscErrorCode SPARSEKIT2msrcsr(PetscInt*,MatScalar*,PetscInt*,MatScalar*,PetscInt*,PetscInt*,MatScalar*,PetscInt*);
#endif

#undef __FUNCT__  
#define __FUNCT__ "MatILUDTFactor_SeqAIJ"
  /* ------------------------------------------------------------

          This interface was contribed by Tony Caola

     This routine is an interface to the pivoting drop-tolerance 
     ILU routine written by Yousef Saad (saad@cs.umn.edu) as part of 
     SPARSEKIT2.

     The SPARSEKIT2 routines used here are covered by the GNU 
     copyright; see the file gnu in this directory.

     Thanks to Prof. Saad, Dr. Hysom, and Dr. Smith for their
     help in getting this routine ironed out.

     The major drawback to this routine is that if info->fill is 
     not large enough it fails rather than allocating more space;
     this can be fixed by hacking/improving the f2c version of 
     Yousef Saad's code.

     ------------------------------------------------------------
*/
PetscErrorCode MatILUDTFactor_SeqAIJ(Mat A,IS isrow,IS iscol,MatFactorInfo *info,Mat *fact)
{
#if defined(PETSC_AVOID_GNUCOPYRIGHT_CODE)
  PetscFunctionBegin;
  SETERRQ(PETSC_ERR_SUP_SYS,"This distribution does not include GNU Copyright code\n\
  You can obtain the drop tolerance routines by installing PETSc from\n\
  www.mcs.anl.gov/petsc\n");
#else
  Mat_SeqAIJ     *a = (Mat_SeqAIJ*)A->data,*b;
  IS             iscolf,isicol,isirow;
  PetscTruth     reorder;
  PetscErrorCode ierr,sierr;
  PetscInt       *c,*r,*ic,i,n = A->rmap.n;
  PetscInt       *old_i = a->i,*old_j = a->j,*new_i,*old_i2 = 0,*old_j2 = 0,*new_j;
  PetscInt       *ordcol,*iwk,*iperm,*jw;
  PetscInt       jmax,lfill,job,*o_i,*o_j;
  MatScalar      *old_a = a->a,*w,*new_a,*old_a2 = 0,*wk,*o_a;
  PetscReal      af;

  PetscFunctionBegin;

  if (info->dt == PETSC_DEFAULT)      info->dt      = .005;
  if (info->dtcount == PETSC_DEFAULT) info->dtcount = (PetscInt)(1.5*a->rmax); 
  if (info->dtcol == PETSC_DEFAULT)   info->dtcol   = .01;
  if (info->fill == PETSC_DEFAULT)    info->fill    = ((double)(n*(info->dtcount+1)))/a->nz;
  lfill   = (PetscInt)(info->dtcount/2.0);
  jmax    = (PetscInt)(info->fill*a->nz);


  /* ------------------------------------------------------------
     If reorder=.TRUE., then the original matrix has to be 
     reordered to reflect the user selected ordering scheme, and
     then de-reordered so it is in it's original format.  
     Because Saad's dperm() is NOT in place, we have to copy 
     the original matrix and allocate more storage. . . 
     ------------------------------------------------------------
  */

  /* set reorder to true if either isrow or iscol is not identity */
  ierr = ISIdentity(isrow,&reorder);CHKERRQ(ierr);
  if (reorder) {ierr = ISIdentity(iscol,&reorder);CHKERRQ(ierr);}
  reorder = PetscNot(reorder);

  
  /* storage for ilu factor */
  ierr = PetscMalloc((n+1)*sizeof(PetscInt),&new_i);CHKERRQ(ierr);
  ierr = PetscMalloc(jmax*sizeof(PetscInt),&new_j);CHKERRQ(ierr);
  ierr = PetscMalloc(jmax*sizeof(MatScalar),&new_a);CHKERRQ(ierr);
  ierr = PetscMalloc(n*sizeof(PetscInt),&ordcol);CHKERRQ(ierr);

  /* ------------------------------------------------------------
     Make sure that everything is Fortran formatted (1-Based)
     ------------------------------------------------------------
  */ 
  for (i=old_i[0];i<old_i[n];i++) {
    old_j[i]++;
  }
  for(i=0;i<n+1;i++) {
    old_i[i]++;
  };
 

  if (reorder) {
    ierr = ISGetIndices(iscol,&c);CHKERRQ(ierr);
    ierr = ISGetIndices(isrow,&r);CHKERRQ(ierr);
    for(i=0;i<n;i++) {
      r[i]  = r[i]+1;
      c[i]  = c[i]+1;
    }
    ierr = PetscMalloc((n+1)*sizeof(PetscInt),&old_i2);CHKERRQ(ierr);
    ierr = PetscMalloc((old_i[n]-old_i[0]+1)*sizeof(PetscInt),&old_j2);CHKERRQ(ierr);
    ierr = PetscMalloc((old_i[n]-old_i[0]+1)*sizeof(MatScalar),&old_a2);CHKERRQ(ierr);
    job  = 3; SPARSEKIT2dperm(&n,old_a,old_j,old_i,old_a2,old_j2,old_i2,r,c,&job);
    for (i=0;i<n;i++) {
      r[i]  = r[i]-1;
      c[i]  = c[i]-1;
    }
    ierr = ISRestoreIndices(iscol,&c);CHKERRQ(ierr);
    ierr = ISRestoreIndices(isrow,&r);CHKERRQ(ierr);
    o_a = old_a2;
    o_j = old_j2;
    o_i = old_i2;
  } else {
    o_a = old_a;
    o_j = old_j;
    o_i = old_i;
  }

  /* ------------------------------------------------------------
     Call Saad's ilutp() routine to generate the factorization
     ------------------------------------------------------------
  */

  ierr = PetscMalloc(2*n*sizeof(PetscInt),&iperm);CHKERRQ(ierr);
  ierr = PetscMalloc(2*n*sizeof(PetscInt),&jw);CHKERRQ(ierr);
  ierr = PetscMalloc(n*sizeof(PetscScalar),&w);CHKERRQ(ierr);

  SPARSEKIT2ilutp(&n,o_a,o_j,o_i,&lfill,(PetscReal)info->dt,&info->dtcol,&n,new_a,new_j,new_i,&jmax,w,jw,iperm,&sierr); 
  if (sierr) {
    switch (sierr) {
      case -3: SETERRQ2(PETSC_ERR_LIB,"ilutp(), matrix U overflows, need larger info->fill current fill %G space allocated %D",info->fill,jmax);
      case -2: SETERRQ2(PETSC_ERR_LIB,"ilutp(), matrix L overflows, need larger info->fill current fill %G space allocated %D",info->fill,jmax);
      case -5: SETERRQ(PETSC_ERR_LIB,"ilutp(), zero row encountered");
      case -1: SETERRQ(PETSC_ERR_LIB,"ilutp(), input matrix may be wrong");
      case -4: SETERRQ1(PETSC_ERR_LIB,"ilutp(), illegal info->fill value %D",jmax);
      default: SETERRQ1(PETSC_ERR_LIB,"ilutp(), zero pivot detected on row %D",sierr);
    }
  }

  ierr = PetscFree(w);CHKERRQ(ierr);
  ierr = PetscFree(jw);CHKERRQ(ierr);

  /* ------------------------------------------------------------
     Saad's routine gives the result in Modified Sparse Row (msr)
     Convert to Compressed Sparse Row format (csr) 
     ------------------------------------------------------------
  */

  ierr = PetscMalloc(n*sizeof(PetscScalar),&wk);CHKERRQ(ierr);   
  ierr = PetscMalloc((n+1)*sizeof(PetscInt),&iwk);CHKERRQ(ierr);

  SPARSEKIT2msrcsr(&n,new_a,new_j,new_a,new_j,new_i,wk,iwk);

  ierr = PetscFree(iwk);CHKERRQ(ierr);
  ierr = PetscFree(wk);CHKERRQ(ierr);

  if (reorder) {
    ierr = PetscFree(old_a2);CHKERRQ(ierr);
    ierr = PetscFree(old_j2);CHKERRQ(ierr);
    ierr = PetscFree(old_i2);CHKERRQ(ierr);
  } else {
    /* fix permutation of old_j that the factorization introduced */
    for (i=old_i[0]; i<old_i[n]; i++) {
      old_j[i-1] = iperm[old_j[i-1]-1]; 
    }
  }

  /* get rid of the shift to indices starting at 1 */
  for (i=0; i<n+1; i++) {
    old_i[i]--;
  }
  for (i=old_i[0];i<old_i[n];i++) {
    old_j[i]--;
  }
 
  /* Make the factored matrix 0-based */ 
  for (i=0; i<n+1; i++) {
    new_i[i]--;
  }
  for (i=new_i[0];i<new_i[n];i++) {
    new_j[i]--;
  } 

  /*-- due to the pivoting, we need to reorder iscol to correctly --*/
  /*-- permute the right-hand-side and solution vectors           --*/
  ierr = ISInvertPermutation(iscol,PETSC_DECIDE,&isicol);CHKERRQ(ierr);
  ierr = ISInvertPermutation(isrow,PETSC_DECIDE,&isirow);CHKERRQ(ierr);
  ierr = ISGetIndices(isicol,&ic);CHKERRQ(ierr);
  for(i=0; i<n; i++) {
    ordcol[i] = ic[iperm[i]-1];  
  };       
  ierr = ISRestoreIndices(isicol,&ic);CHKERRQ(ierr);
  ierr = ISDestroy(isicol);CHKERRQ(ierr);

  ierr = PetscFree(iperm);CHKERRQ(ierr);

  ierr = ISCreateGeneral(PETSC_COMM_SELF,n,ordcol,&iscolf);CHKERRQ(ierr); 
  ierr = PetscFree(ordcol);CHKERRQ(ierr);

  /*----- put together the new matrix -----*/

  ierr = MatCreate(((PetscObject)A)->comm,fact);CHKERRQ(ierr);
  ierr = MatSetSizes(*fact,n,n,n,n);CHKERRQ(ierr);
  ierr = MatSetType(*fact,((PetscObject)A)->type_name);CHKERRQ(ierr);
  ierr = MatSeqAIJSetPreallocation_SeqAIJ(*fact,MAT_SKIP_ALLOCATION,PETSC_NULL);CHKERRQ(ierr);
  (*fact)->factor    = MAT_FACTOR_LU;
  (*fact)->assembled = PETSC_TRUE;

  b = (Mat_SeqAIJ*)(*fact)->data;
  b->free_a        = PETSC_TRUE;
  b->free_ij       = PETSC_TRUE;
  b->singlemalloc  = PETSC_FALSE;
  b->a             = new_a;
  b->j             = new_j;
  b->i             = new_i;
  b->ilen          = 0;
  b->imax          = 0;
  /*  I am not sure why these are the inverses of the row and column permutations; but the other way is NO GOOD */
  b->row           = isirow;
  b->col           = iscolf;
  ierr = PetscMalloc((n+1)*sizeof(PetscScalar),&b->solve_work);CHKERRQ(ierr);
  b->maxnz = b->nz = new_i[n];
  ierr = MatMarkDiagonal_SeqAIJ(*fact);CHKERRQ(ierr);
  (*fact)->info.factor_mallocs = 0;

  af = ((double)b->nz)/((double)a->nz) + .001;
  ierr = PetscInfo2(A,"Fill ratio:given %G needed %G\n",info->fill,af);CHKERRQ(ierr);
  ierr = PetscInfo1(A,"Run with -pc_factor_fill %G or use \n",af);CHKERRQ(ierr);
  ierr = PetscInfo1(A,"PCFactorSetFill(pc,%G);\n",af);CHKERRQ(ierr);
  ierr = PetscInfo(A,"for best performance.\n");CHKERRQ(ierr);

  ierr = MatILUDTFactor_Inode(A,isrow,iscol,info,fact);CHKERRQ(ierr);

  PetscFunctionReturn(0);
#endif
}

#undef __FUNCT__  
#define __FUNCT__ "MatGetFactor_seqaij_petsc"
PetscErrorCode MatGetFactor_seqaij_petsc(Mat A,MatFactorType ftype,Mat *B)
{
  PetscInt           n = A->rmap.n;
  PetscErrorCode     ierr;

  PetscFunctionBegin;
  ierr = MatCreate(((PetscObject)A)->comm,B);CHKERRQ(ierr);
  ierr = MatSetSizes(*B,n,n,n,n);CHKERRQ(ierr);
  if (ftype == MAT_FACTOR_LU || ftype == MAT_FACTOR_ILU) {
    ierr = MatSetType(*B,MATSEQAIJ);CHKERRQ(ierr);
  } else if (ftype == MAT_FACTOR_CHOLESKY || ftype == MAT_FACTOR_ICC) {
    ierr = MatSetType(*B,MATSEQSBAIJ);CHKERRQ(ierr);
    ierr = MatSeqSBAIJSetPreallocation(*B,1,MAT_SKIP_ALLOCATION,PETSC_NULL);CHKERRQ(ierr);
    (*B)->ops->iccfactorsymbolic      = MatICCFactorSymbolic_SeqAIJ;
    (*B)->ops->choleskyfactorsymbolic = MatCholeskyFactorSymbolic_SeqAIJ;
    (*B)->ops->choleskyfactornumeric  = MatCholeskyFactorNumeric_SeqAIJ;
  } else SETERRQ(PETSC_ERR_SUP,"Factor type not supported");
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatLUFactorSymbolic_SeqAIJ"
PetscErrorCode MatLUFactorSymbolic_SeqAIJ(Mat A,IS isrow,IS iscol,MatFactorInfo *info,Mat *B)
{
  Mat_SeqAIJ         *a = (Mat_SeqAIJ*)A->data,*b;
  IS                 isicol;
  PetscErrorCode     ierr;
  PetscInt           *r,*ic,i,n=A->rmap.n,*ai=a->i,*aj=a->j;
  PetscInt           *bi,*bj,*ajtmp;
  PetscInt           *bdiag,row,nnz,nzi,reallocs=0,nzbd,*im;
  PetscReal          f;
  PetscInt           nlnk,*lnk,k,**bi_ptr;
  PetscFreeSpaceList free_space=PETSC_NULL,current_space=PETSC_NULL;
  PetscBT            lnkbt;

  PetscFunctionBegin;
  if (A->rmap.N != A->cmap.N) SETERRQ(PETSC_ERR_ARG_WRONG,"matrix must be square");
  ierr = ISInvertPermutation(iscol,PETSC_DECIDE,&isicol);CHKERRQ(ierr);
  ierr = ISGetIndices(isrow,&r);CHKERRQ(ierr);
  ierr = ISGetIndices(isicol,&ic);CHKERRQ(ierr);

  /* get new row pointers */
  ierr = PetscMalloc((n+1)*sizeof(PetscInt),&bi);CHKERRQ(ierr);
  bi[0] = 0;

  /* bdiag is location of diagonal in factor */
  ierr = PetscMalloc((n+1)*sizeof(PetscInt),&bdiag);CHKERRQ(ierr);
  bdiag[0] = 0;

  /* linked list for storing column indices of the active row */
  nlnk = n + 1;
  ierr = PetscLLCreate(n,n,nlnk,lnk,lnkbt);CHKERRQ(ierr);

  ierr = PetscMalloc2(n+1,PetscInt**,&bi_ptr,n+1,PetscInt,&im);CHKERRQ(ierr);

  /* initial FreeSpace size is f*(ai[n]+1) */
  f = info->fill;
  ierr = PetscFreeSpaceGet((PetscInt)(f*(ai[n]+1)),&free_space);CHKERRQ(ierr);
  current_space = free_space;

  for (i=0; i<n; i++) {
    /* copy previous fill into linked list */
    nzi = 0;
    nnz = ai[r[i]+1] - ai[r[i]];
    if (!nnz) SETERRQ(PETSC_ERR_MAT_LU_ZRPVT,"Empty row in matrix");
    ajtmp = aj + ai[r[i]]; 
    ierr = PetscLLAddPerm(nnz,ajtmp,ic,n,nlnk,lnk,lnkbt);CHKERRQ(ierr);
    nzi += nlnk;

    /* add pivot rows into linked list */
    row = lnk[n]; 
    while (row < i) {
      nzbd    = bdiag[row] - bi[row] + 1; /* num of entries in the row with column index <= row */
      ajtmp   = bi_ptr[row] + nzbd; /* points to the entry next to the diagonal */   
      ierr = PetscLLAddSortedLU(ajtmp,row,nlnk,lnk,lnkbt,i,nzbd,im);CHKERRQ(ierr);
      nzi += nlnk;
      row  = lnk[row];
    }
    bi[i+1] = bi[i] + nzi;
    im[i]   = nzi; 

    /* mark bdiag */
    nzbd = 0;  
    nnz  = nzi;
    k    = lnk[n]; 
    while (nnz-- && k < i){
      nzbd++;
      k = lnk[k]; 
    }
    bdiag[i] = bi[i] + nzbd;

    /* if free space is not available, make more free space */
    if (current_space->local_remaining<nzi) {
      nnz = (n - i)*nzi; /* estimated and max additional space needed */
      ierr = PetscFreeSpaceGet(nnz,&current_space);CHKERRQ(ierr);
      reallocs++;
    }

    /* copy data into free space, then initialize lnk */
    ierr = PetscLLClean(n,n,nzi,lnk,current_space->array,lnkbt);CHKERRQ(ierr); 
    bi_ptr[i] = current_space->array;
    current_space->array           += nzi;
    current_space->local_used      += nzi;
    current_space->local_remaining -= nzi;
  }
#if defined(PETSC_USE_INFO)
  if (ai[n] != 0) {
    PetscReal af = ((PetscReal)bi[n])/((PetscReal)ai[n]);
    ierr = PetscInfo3(A,"Reallocs %D Fill ratio:given %G needed %G\n",reallocs,f,af);CHKERRQ(ierr);
    ierr = PetscInfo1(A,"Run with -pc_factor_fill %G or use \n",af);CHKERRQ(ierr);
    ierr = PetscInfo1(A,"PCFactorSetFill(pc,%G);\n",af);CHKERRQ(ierr);
    ierr = PetscInfo(A,"for best performance.\n");CHKERRQ(ierr);
  } else {
    ierr = PetscInfo(A,"Empty matrix\n");CHKERRQ(ierr);
  }
#endif

  ierr = ISRestoreIndices(isrow,&r);CHKERRQ(ierr);
  ierr = ISRestoreIndices(isicol,&ic);CHKERRQ(ierr);

  /* destroy list of free space and other temporary array(s) */
  ierr = PetscMalloc((bi[n]+1)*sizeof(PetscInt),&bj);CHKERRQ(ierr);
  ierr = PetscFreeSpaceContiguous(&free_space,bj);CHKERRQ(ierr); 
  ierr = PetscLLDestroy(lnk,lnkbt);CHKERRQ(ierr);
  ierr = PetscFree2(bi_ptr,im);CHKERRQ(ierr);

  /* put together the new matrix */
  ierr = MatSeqAIJSetPreallocation_SeqAIJ(*B,MAT_SKIP_ALLOCATION,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscLogObjectParent(*B,isicol);CHKERRQ(ierr);
  b    = (Mat_SeqAIJ*)(*B)->data;
  b->free_a       = PETSC_TRUE;
  b->free_ij      = PETSC_TRUE;
  b->singlemalloc = PETSC_FALSE;
  ierr          = PetscMalloc((bi[n]+1)*sizeof(PetscScalar),&b->a);CHKERRQ(ierr);
  b->j          = bj; 
  b->i          = bi;
  b->diag       = bdiag;
  b->ilen       = 0;
  b->imax       = 0;
  b->row        = isrow;
  b->col        = iscol;
  ierr          = PetscObjectReference((PetscObject)isrow);CHKERRQ(ierr);
  ierr          = PetscObjectReference((PetscObject)iscol);CHKERRQ(ierr);
  b->icol       = isicol;
  ierr          = PetscMalloc((n+1)*sizeof(PetscScalar),&b->solve_work);CHKERRQ(ierr);

  /* In b structure:  Free imax, ilen, old a, old j.  Allocate solve_work, new a, new j */
  ierr = PetscLogObjectMemory(*B,(bi[n]-n)*(sizeof(PetscInt)+sizeof(PetscScalar)));CHKERRQ(ierr);
  b->maxnz = b->nz = bi[n] ;

  (*B)->factor                 =  MAT_FACTOR_LU;
  (*B)->info.factor_mallocs    = reallocs;
  (*B)->info.fill_ratio_given  = f;

  if (ai[n] != 0) {
    (*B)->info.fill_ratio_needed = ((PetscReal)bi[n])/((PetscReal)ai[n]);
  } else {
    (*B)->info.fill_ratio_needed = 0.0;
  }
  ierr = MatLUFactorSymbolic_Inode(A,isrow,iscol,info,B);CHKERRQ(ierr); 
  (*B)->ops->lufactornumeric =  A->ops->lufactornumeric; /* Use Inode variant ONLY if A has inodes */
  PetscFunctionReturn(0); 
}

/*
    Trouble in factorization, should we dump the original matrix?
*/
#undef __FUNCT__  
#define __FUNCT__ "MatFactorDumpMatrix"
PetscErrorCode MatFactorDumpMatrix(Mat A)
{
  PetscErrorCode ierr;
  PetscTruth     flg;

  PetscFunctionBegin;
  ierr = PetscOptionsHasName(PETSC_NULL,"-mat_factor_dump_on_error",&flg);CHKERRQ(ierr);
  if (flg) {
    PetscViewer viewer;
    char        filename[PETSC_MAX_PATH_LEN];

    ierr = PetscSNPrintf(filename,PETSC_MAX_PATH_LEN,"matrix_factor_error.%d",PetscGlobalRank);CHKERRQ(ierr);
    ierr = PetscViewerBinaryOpen(((PetscObject)A)->comm,filename,FILE_MODE_WRITE,&viewer);CHKERRQ(ierr);
    ierr = MatView(A,viewer);CHKERRQ(ierr);
    ierr = PetscViewerDestroy(viewer);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

/* ----------------------------------------------------------- */
#undef __FUNCT__  
#define __FUNCT__ "MatLUFactorNumeric_SeqAIJ"
PetscErrorCode MatLUFactorNumeric_SeqAIJ(Mat A,MatFactorInfo *info,Mat *B)
{
  Mat            C=*B;
  Mat_SeqAIJ     *a=(Mat_SeqAIJ*)A->data,*b=(Mat_SeqAIJ *)C->data;
  IS             isrow = b->row,isicol = b->icol;
  PetscErrorCode ierr;
  PetscInt       *r,*ic,i,j,n=A->rmap.n,*bi=b->i,*bj=b->j;
  PetscInt       *ajtmp,*bjtmp,nz,row,*ics;
  PetscInt       *diag_offset = b->diag,diag,*pj;
  PetscScalar    *rtmp,*pc,multiplier,*rtmps;
  MatScalar      *v,*pv;
  PetscScalar    d;
  PetscReal      rs;
  LUShift_Ctx    sctx;
  PetscInt       newshift,*ddiag;

  PetscFunctionBegin;
  ierr  = ISGetIndices(isrow,&r);CHKERRQ(ierr);
  ierr  = ISGetIndices(isicol,&ic);CHKERRQ(ierr);
  ierr  = PetscMalloc((n+1)*sizeof(PetscScalar),&rtmp);CHKERRQ(ierr);
  ierr  = PetscMemzero(rtmp,(n+1)*sizeof(PetscScalar));CHKERRQ(ierr);
  rtmps = rtmp; ics = ic;

  sctx.shift_top  = 0;
  sctx.nshift_max = 0;
  sctx.shift_lo   = 0;
  sctx.shift_hi   = 0;

  /* if both shift schemes are chosen by user, only use info->shiftpd */
  if (info->shiftpd && info->shiftnz) info->shiftnz = 0.0; 
  if (info->shiftpd) { /* set sctx.shift_top=max{rs} */
    PetscInt *aai = a->i;
    ddiag          = a->diag;
    sctx.shift_top = 0;
    for (i=0; i<n; i++) {
      /* calculate sum(|aij|)-RealPart(aii), amt of shift needed for this row */
      d  = (a->a)[ddiag[i]];
      rs = -PetscAbsScalar(d) - PetscRealPart(d);
      v  = a->a+aai[i];
      nz = aai[i+1] - aai[i];
      for (j=0; j<nz; j++) 
	rs += PetscAbsScalar(v[j]);
      if (rs>sctx.shift_top) sctx.shift_top = rs;
    }
    if (sctx.shift_top < info->zeropivot) sctx.shift_top = info->zeropivot;
    sctx.shift_top    *= 1.1;
    sctx.nshift_max   = 5;
    sctx.shift_lo     = 0.;
    sctx.shift_hi     = 1.;
  }

  sctx.shift_amount = 0;
  sctx.nshift       = 0;
  do {
    sctx.lushift = PETSC_FALSE;
    for (i=0; i<n; i++){
      nz    = bi[i+1] - bi[i];
      bjtmp = bj + bi[i];
      for  (j=0; j<nz; j++) rtmps[bjtmp[j]] = 0.0;

      /* load in initial (unfactored row) */
      nz    = a->i[r[i]+1] - a->i[r[i]];
      ajtmp = a->j + a->i[r[i]];
      v     = a->a + a->i[r[i]];
      for (j=0; j<nz; j++) {
        rtmp[ics[ajtmp[j]]] = v[j];
      }
      rtmp[ics[r[i]]] += sctx.shift_amount; /* shift the diagonal of the matrix */

      row = *bjtmp++;
      while  (row < i) {
        pc = rtmp + row;
        if (*pc != 0.0) {
          pv         = b->a + diag_offset[row];
          pj         = b->j + diag_offset[row] + 1;
          multiplier = *pc / *pv++;
          *pc        = multiplier;
          nz         = bi[row+1] - diag_offset[row] - 1;
          for (j=0; j<nz; j++) rtmps[pj[j]] -= multiplier * pv[j];
          ierr = PetscLogFlops(2*nz);CHKERRQ(ierr);
        }
        row = *bjtmp++;
      }
      /* finished row so stick it into b->a */
      pv   = b->a + bi[i] ;
      pj   = b->j + bi[i] ;
      nz   = bi[i+1] - bi[i];
      diag = diag_offset[i] - bi[i];
      rs   = 0.0;
      for (j=0; j<nz; j++) {
        pv[j] = rtmps[pj[j]];
        if (j != diag) rs += PetscAbsScalar(pv[j]);
      }

      /* 9/13/02 Victor Eijkhout suggested scaling zeropivot by rs for matrices with funny scalings */
      sctx.rs  = rs;
      sctx.pv  = pv[diag];
      ierr = MatLUCheckShift_inline(info,sctx,i,newshift);CHKERRQ(ierr);
      if (newshift == 1) break;
    } 

    if (info->shiftpd && !sctx.lushift && info->shift_fraction>0 && sctx.nshift<sctx.nshift_max) {
      /*
       * if no shift in this attempt & shifting & started shifting & can refine,
       * then try lower shift
       */
      sctx.shift_hi        = info->shift_fraction;
      info->shift_fraction = (sctx.shift_hi+sctx.shift_lo)/2.;
      sctx.shift_amount    = info->shift_fraction * sctx.shift_top;
      sctx.lushift         = PETSC_TRUE;
      sctx.nshift++;
    }
  } while (sctx.lushift);

  /* invert diagonal entries for simplier triangular solves */
  for (i=0; i<n; i++) {
    b->a[diag_offset[i]] = 1.0/b->a[diag_offset[i]];
  }
  ierr = PetscFree(rtmp);CHKERRQ(ierr);
  ierr = ISRestoreIndices(isicol,&ic);CHKERRQ(ierr);
  ierr = ISRestoreIndices(isrow,&r);CHKERRQ(ierr);
  C->factor       = MAT_FACTOR_LU;
  C->assembled    = PETSC_TRUE;
  C->preallocated = PETSC_TRUE;
  ierr = PetscLogFlops(C->cmap.n);CHKERRQ(ierr);
  if (sctx.nshift){
    if (info->shiftnz) {
      ierr = PetscInfo2(A,"number of shift_nz tries %D, shift_amount %G\n",sctx.nshift,sctx.shift_amount);CHKERRQ(ierr);
    } else if (info->shiftpd) {
      ierr = PetscInfo4(A,"number of shift_pd tries %D, shift_amount %G, diagonal shifted up by %e fraction top_value %e\n",sctx.nshift,sctx.shift_amount,info->shift_fraction,sctx.shift_top);CHKERRQ(ierr);
    }
  }
  PetscFunctionReturn(0);
}

/* 
   This routine implements inplace ILU(0) with row or/and column permutations. 
   Input: 
     A - original matrix
   Output;
     A - a->i (rowptr) is same as original rowptr, but factored i-the row is stored in rowperm[i] 
         a->j (col index) is permuted by the inverse of colperm, then sorted
         a->a reordered accordingly with a->j
         a->diag (ptr to diagonal elements) is updated.
*/
#undef __FUNCT__  
#define __FUNCT__ "MatLUFactorNumeric_SeqAIJ_InplaceWithPerm"
PetscErrorCode MatLUFactorNumeric_SeqAIJ_InplaceWithPerm(Mat A,MatFactorInfo *info,Mat *B)
{
  Mat_SeqAIJ     *a=(Mat_SeqAIJ*)A->data; 
  IS             isrow = a->row,isicol = a->icol;
  PetscErrorCode ierr;
  PetscInt       *r,*ic,i,j,n=A->rmap.n,*ai=a->i,*aj=a->j;
  PetscInt       *ajtmp,nz,row,*ics;
  PetscInt       *diag = a->diag,nbdiag,*pj;
  PetscScalar    *rtmp,*pc,multiplier,d;
  MatScalar      *v,*pv;
  PetscReal      rs;
  LUShift_Ctx    sctx;
  PetscInt       newshift;

  PetscFunctionBegin;
  if (A != *B) SETERRQ(PETSC_ERR_ARG_INCOMP,"input and output matrix must have same address");
  ierr  = ISGetIndices(isrow,&r);CHKERRQ(ierr);
  ierr  = ISGetIndices(isicol,&ic);CHKERRQ(ierr);
  ierr  = PetscMalloc((n+1)*sizeof(PetscScalar),&rtmp);CHKERRQ(ierr);
  ierr  = PetscMemzero(rtmp,(n+1)*sizeof(PetscScalar));CHKERRQ(ierr);
  ics = ic;

  sctx.shift_top  = 0;
  sctx.nshift_max = 0;
  sctx.shift_lo   = 0;
  sctx.shift_hi   = 0;

  /* if both shift schemes are chosen by user, only use info->shiftpd */
  if (info->shiftpd && info->shiftnz) info->shiftnz = 0.0; 
  if (info->shiftpd) { /* set sctx.shift_top=max{rs} */
    sctx.shift_top = 0;
    for (i=0; i<n; i++) {
      /* calculate sum(|aij|)-RealPart(aii), amt of shift needed for this row */
      d  = (a->a)[diag[i]];
      rs = -PetscAbsScalar(d) - PetscRealPart(d);
      v  = a->a+ai[i];
      nz = ai[i+1] - ai[i];
      for (j=0; j<nz; j++) 
	rs += PetscAbsScalar(v[j]);
      if (rs>sctx.shift_top) sctx.shift_top = rs;
    }
    if (sctx.shift_top < info->zeropivot) sctx.shift_top = info->zeropivot;
    sctx.shift_top    *= 1.1;
    sctx.nshift_max   = 5;
    sctx.shift_lo     = 0.;
    sctx.shift_hi     = 1.;
  }

  sctx.shift_amount = 0;
  sctx.nshift       = 0;
  do {
    sctx.lushift = PETSC_FALSE;
    for (i=0; i<n; i++){
      /* load in initial unfactored row */
      nz    = ai[r[i]+1] - ai[r[i]];
      ajtmp = aj + ai[r[i]];
      v     = a->a + ai[r[i]];
      /* sort permuted ajtmp and values v accordingly */
      for (j=0; j<nz; j++) ajtmp[j] = ics[ajtmp[j]];
      ierr = PetscSortIntWithScalarArray(nz,ajtmp,v);CHKERRQ(ierr);

      diag[r[i]] = ai[r[i]]; 
      for (j=0; j<nz; j++) {
        rtmp[ajtmp[j]] = v[j];
        if (ajtmp[j] < i) diag[r[i]]++; /* update a->diag */
      }
      rtmp[r[i]] += sctx.shift_amount; /* shift the diagonal of the matrix */

      row = *ajtmp++;
      while  (row < i) {
        pc = rtmp + row;
        if (*pc != 0.0) {
          pv         = a->a + diag[r[row]];
          pj         = aj + diag[r[row]] + 1;

          multiplier = *pc / *pv++;
          *pc        = multiplier;
          nz         = ai[r[row]+1] - diag[r[row]] - 1;
          for (j=0; j<nz; j++) rtmp[pj[j]] -= multiplier * pv[j];
          ierr = PetscLogFlops(2*nz);CHKERRQ(ierr);
        }
        row = *ajtmp++;
      }
      /* finished row so overwrite it onto a->a */
      pv   = a->a + ai[r[i]] ;
      pj   = aj + ai[r[i]] ;
      nz   = ai[r[i]+1] - ai[r[i]];
      nbdiag = diag[r[i]] - ai[r[i]]; /* num of entries before the diagonal */
      
      rs   = 0.0;
      for (j=0; j<nz; j++) {
        pv[j] = rtmp[pj[j]];
        if (j != nbdiag) rs += PetscAbsScalar(pv[j]);
      }

      /* 9/13/02 Victor Eijkhout suggested scaling zeropivot by rs for matrices with funny scalings */
      sctx.rs  = rs;
      sctx.pv  = pv[nbdiag];
      ierr = MatLUCheckShift_inline(info,sctx,i,newshift);CHKERRQ(ierr);
      if (newshift == 1) break;
    } 

    if (info->shiftpd && !sctx.lushift && info->shift_fraction>0 && sctx.nshift<sctx.nshift_max) {
      /*
       * if no shift in this attempt & shifting & started shifting & can refine,
       * then try lower shift
       */
      sctx.shift_hi        = info->shift_fraction;
      info->shift_fraction = (sctx.shift_hi+sctx.shift_lo)/2.;
      sctx.shift_amount    = info->shift_fraction * sctx.shift_top;
      sctx.lushift         = PETSC_TRUE;
      sctx.nshift++;
    }
  } while (sctx.lushift);

  /* invert diagonal entries for simplier triangular solves */
  for (i=0; i<n; i++) {
    a->a[diag[r[i]]] = 1.0/a->a[diag[r[i]]];
  }

  ierr = PetscFree(rtmp);CHKERRQ(ierr);
  ierr = ISRestoreIndices(isicol,&ic);CHKERRQ(ierr);
  ierr = ISRestoreIndices(isrow,&r);CHKERRQ(ierr);
  A->factor     = MAT_FACTOR_LU;
  A->ops->solve = MatSolve_SeqAIJ_InplaceWithPerm;
  A->assembled = PETSC_TRUE;
  A->preallocated = PETSC_TRUE;
  ierr = PetscLogFlops(A->cmap.n);CHKERRQ(ierr);
  if (sctx.nshift){
    if (info->shiftnz) {
      ierr = PetscInfo2(A,"number of shift_nz tries %D, shift_amount %G\n",sctx.nshift,sctx.shift_amount);CHKERRQ(ierr);
    } else if (info->shiftpd) {
      ierr = PetscInfo4(A,"number of shift_pd tries %D, shift_amount %G, diagonal shifted up by %e fraction top_value %e\n",sctx.nshift,sctx.shift_amount,info->shift_fraction,sctx.shift_top);CHKERRQ(ierr);
    }
  }
  PetscFunctionReturn(0);
}

/* ----------------------------------------------------------- */
#undef __FUNCT__  
#define __FUNCT__ "MatLUFactor_SeqAIJ"
PetscErrorCode MatLUFactor_SeqAIJ(Mat A,IS row,IS col,MatFactorInfo *info)
{
  PetscErrorCode ierr;
  Mat            C;

  PetscFunctionBegin;
  ierr = MatGetFactor(A,"petsc",MAT_FACTOR_LU,&C);CHKERRQ(ierr);
  ierr = MatLUFactorSymbolic(A,row,col,info,&C);CHKERRQ(ierr);
  ierr = MatLUFactorNumeric(A,info,&C);CHKERRQ(ierr);
  ierr = MatHeaderCopy(A,C);CHKERRQ(ierr);
  ierr = PetscLogObjectParent(A,((Mat_SeqAIJ*)(A->data))->icol);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
/* ----------------------------------------------------------- */
#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqAIJ"
PetscErrorCode MatSolve_SeqAIJ(Mat A,Vec bb,Vec xx)
{
  Mat_SeqAIJ        *a = (Mat_SeqAIJ*)A->data;
  IS                iscol = a->col,isrow = a->row;
  PetscErrorCode    ierr;
  PetscInt          *r,*c,i, n = A->rmap.n,*vi,*ai = a->i,*aj = a->j;
  PetscInt          nz,*rout,*cout;
  PetscScalar       *x,*tmp,*tmps,sum;
  const PetscScalar *b;
  const MatScalar   *aa = a->a,*v;

  PetscFunctionBegin;
  if (!n) PetscFunctionReturn(0);

  ierr = VecGetArray(bb,(PetscScalar**)&b);CHKERRQ(ierr); 
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  tmp  = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout + (n-1);

  /* forward solve the lower triangular */
  tmp[0] = b[*r++];
  tmps   = tmp;
  for (i=1; i<n; i++) {
    v   = aa + ai[i] ;
    vi  = aj + ai[i] ;
    nz  = a->diag[i] - ai[i];
    sum = b[*r++];
    SPARSEDENSEMDOT(sum,tmps,v,vi,nz); 
    tmp[i] = sum;
  }

  /* backward solve the upper triangular */
  for (i=n-1; i>=0; i--){
    v   = aa + a->diag[i] + 1;
    vi  = aj + a->diag[i] + 1;
    nz  = ai[i+1] - a->diag[i] - 1;
    sum = tmp[i];
    SPARSEDENSEMDOT(sum,tmps,v,vi,nz); 
    x[*c--] = tmp[i] = sum*aa[a->diag[i]];
  }

  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,(PetscScalar**)&b);CHKERRQ(ierr); 
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2*a->nz - A->cmap.n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMatSolve_SeqAIJ"
PetscErrorCode MatMatSolve_SeqAIJ(Mat A,Mat B,Mat X)
{
  Mat_SeqAIJ      *a = (Mat_SeqAIJ*)A->data;
  IS              iscol = a->col,isrow = a->row;
  PetscErrorCode  ierr;
  PetscInt        *r,*c,i, n = A->rmap.n,*vi,*ai = a->i,*aj = a->j;
  PetscInt        nz,*rout,*cout,neq; 
  PetscScalar     *x,*b,*tmp,*tmps,sum;
  const MatScalar *aa = a->a,*v;

  PetscFunctionBegin;
  if (!n) PetscFunctionReturn(0);

  ierr = MatGetArray(B,&b);CHKERRQ(ierr); 
  ierr = MatGetArray(X,&x);CHKERRQ(ierr);
  
  tmp  = a->solve_work;
  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout;

  for (neq=0; neq<n; neq++){
    /* forward solve the lower triangular */
    tmp[0] = b[r[0]];
    tmps   = tmp;
    for (i=1; i<n; i++) {
      v   = aa + ai[i] ;
      vi  = aj + ai[i] ;
      nz  = a->diag[i] - ai[i];
      sum = b[r[i]];
      SPARSEDENSEMDOT(sum,tmps,v,vi,nz); 
      tmp[i] = sum;
    }
    /* backward solve the upper triangular */
    for (i=n-1; i>=0; i--){
      v   = aa + a->diag[i] + 1;
      vi  = aj + a->diag[i] + 1;
      nz  = ai[i+1] - a->diag[i] - 1;
      sum = tmp[i];
      SPARSEDENSEMDOT(sum,tmps,v,vi,nz); 
      x[c[i]] = tmp[i] = sum*aa[a->diag[i]];
    }

    b += n;
    x += n;
  }
  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = MatRestoreArray(B,&b);CHKERRQ(ierr); 
  ierr = MatRestoreArray(X,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(n*(2*a->nz - n));CHKERRQ(ierr);
  PetscFunctionReturn(0);
}  

#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqAIJ_InplaceWithPerm"
PetscErrorCode MatSolve_SeqAIJ_InplaceWithPerm(Mat A,Vec bb,Vec xx)
{
  Mat_SeqAIJ      *a = (Mat_SeqAIJ*)A->data;
  IS              iscol = a->col,isrow = a->row;
  PetscErrorCode  ierr;
  PetscInt        *r,*c,i, n = A->rmap.n,*vi,*ai = a->i,*aj = a->j;
  PetscInt        nz,*rout,*cout,row;
  PetscScalar     *x,*b,*tmp,*tmps,sum;
  const MatScalar *aa = a->a,*v;

  PetscFunctionBegin;
  if (!n) PetscFunctionReturn(0);

  ierr = VecGetArray(bb,&b);CHKERRQ(ierr); 
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  tmp  = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout + (n-1);

  /* forward solve the lower triangular */
  tmp[0] = b[*r++];
  tmps   = tmp;
  for (row=1; row<n; row++) {
    i   = rout[row]; /* permuted row */
    v   = aa + ai[i] ;
    vi  = aj + ai[i] ;
    nz  = a->diag[i] - ai[i];
    sum = b[*r++];
    SPARSEDENSEMDOT(sum,tmps,v,vi,nz); 
    tmp[row] = sum;
  }

  /* backward solve the upper triangular */
  for (row=n-1; row>=0; row--){
    i   = rout[row]; /* permuted row */
    v   = aa + a->diag[i] + 1;
    vi  = aj + a->diag[i] + 1;
    nz  = ai[i+1] - a->diag[i] - 1;
    sum = tmp[row];
    SPARSEDENSEMDOT(sum,tmps,v,vi,nz); 
    x[*c--] = tmp[row] = sum*aa[a->diag[i]];
  }

  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr); 
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2*a->nz - A->cmap.n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* ----------------------------------------------------------- */
#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqAIJ_NaturalOrdering"
PetscErrorCode MatSolve_SeqAIJ_NaturalOrdering(Mat A,Vec bb,Vec xx)
{
  Mat_SeqAIJ        *a = (Mat_SeqAIJ*)A->data;
  PetscErrorCode    ierr;
  PetscInt          n = A->rmap.n;
  const PetscInt    *ai = a->i,*aj = a->j,*adiag = a->diag,*vi;
  PetscScalar       *x;
  const PetscScalar *b;
  const MatScalar   *aa = a->a;
#if !defined(PETSC_USE_FORTRAN_KERNEL_SOLVEAIJ)
  PetscInt          adiag_i,i,nz,ai_i;
  const MatScalar   *v;
  PetscScalar       sum;
#endif

  PetscFunctionBegin;
  if (!n) PetscFunctionReturn(0);

  ierr = VecGetArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);

#if defined(PETSC_USE_FORTRAN_KERNEL_SOLVEAIJ)
  fortransolveaij_(&n,x,ai,aj,adiag,aa,b);
#else
  /* forward solve the lower triangular */
  x[0] = b[0];
  for (i=1; i<n; i++) {
    ai_i = ai[i];
    v    = aa + ai_i;
    vi   = aj + ai_i;
    nz   = adiag[i] - ai_i;
    sum  = b[i];
    while (nz--) sum -= *v++ * x[*vi++];
    x[i] = sum;
  }

  /* backward solve the upper triangular */
  for (i=n-1; i>=0; i--){
    adiag_i = adiag[i];
    v       = aa + adiag_i + 1;
    vi      = aj + adiag_i + 1;
    nz      = ai[i+1] - adiag_i - 1;
    sum     = x[i];
    while (nz--) sum -= *v++ * x[*vi++];
    x[i]    = sum*aa[adiag_i];
  }
#endif
  ierr = PetscLogFlops(2*a->nz - A->cmap.n);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,(PetscScalar**)&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolveAdd_SeqAIJ"
PetscErrorCode MatSolveAdd_SeqAIJ(Mat A,Vec bb,Vec yy,Vec xx)
{
  Mat_SeqAIJ      *a = (Mat_SeqAIJ*)A->data;
  IS              iscol = a->col,isrow = a->row;
  PetscErrorCode  ierr;
  PetscInt        *r,*c,i, n = A->rmap.n,*vi,*ai = a->i,*aj = a->j;
  PetscInt        nz,*rout,*cout;
  PetscScalar     *x,*b,*tmp,sum;
  const MatScalar *aa = a->a,*v;

  PetscFunctionBegin;
  if (yy != xx) {ierr = VecCopy(yy,xx);CHKERRQ(ierr);}

  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  tmp  = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout + (n-1);

  /* forward solve the lower triangular */
  tmp[0] = b[*r++];
  for (i=1; i<n; i++) {
    v   = aa + ai[i] ;
    vi  = aj + ai[i] ;
    nz  = a->diag[i] - ai[i];
    sum = b[*r++];
    while (nz--) sum -= *v++ * tmp[*vi++ ];
    tmp[i] = sum;
  }

  /* backward solve the upper triangular */
  for (i=n-1; i>=0; i--){
    v   = aa + a->diag[i] + 1;
    vi  = aj + a->diag[i] + 1;
    nz  = ai[i+1] - a->diag[i] - 1;
    sum = tmp[i];
    while (nz--) sum -= *v++ * tmp[*vi++ ];
    tmp[i] = sum*aa[a->diag[i]];
    x[*c--] += tmp[i];
  }

  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = PetscLogFlops(2*a->nz);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}
/* -------------------------------------------------------------------*/
#undef __FUNCT__  
#define __FUNCT__ "MatSolveTranspose_SeqAIJ"
PetscErrorCode MatSolveTranspose_SeqAIJ(Mat A,Vec bb,Vec xx)
{
  Mat_SeqAIJ      *a = (Mat_SeqAIJ*)A->data;
  IS              iscol = a->col,isrow = a->row;
  PetscErrorCode  ierr;
  PetscInt        *r,*c,i,n = A->rmap.n,*vi,*ai = a->i,*aj = a->j;
  PetscInt        nz,*rout,*cout,*diag = a->diag;
  PetscScalar     *x,*b,*tmp,s1;
  const MatScalar *aa = a->a,*v;

  PetscFunctionBegin;
  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  tmp  = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout;

  /* copy the b into temp work space according to permutation */
  for (i=0; i<n; i++) tmp[i] = b[c[i]]; 

  /* forward solve the U^T */
  for (i=0; i<n; i++) {
    v   = aa + diag[i] ;
    vi  = aj + diag[i] + 1;
    nz  = ai[i+1] - diag[i] - 1;
    s1  = tmp[i];
    s1 *= (*v++);  /* multiply by inverse of diagonal entry */
    while (nz--) {
      tmp[*vi++ ] -= (*v++)*s1;
    }
    tmp[i] = s1;
  }

  /* backward solve the L^T */
  for (i=n-1; i>=0; i--){
    v   = aa + diag[i] - 1 ;
    vi  = aj + diag[i] - 1 ;
    nz  = diag[i] - ai[i];
    s1  = tmp[i];
    while (nz--) {
      tmp[*vi-- ] -= (*v--)*s1;
    }
  }

  /* copy tmp into x according to permutation */
  for (i=0; i<n; i++) x[r[i]] = tmp[i];

  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);

  ierr = PetscLogFlops(2*a->nz-A->cmap.n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolveTransposeAdd_SeqAIJ"
PetscErrorCode MatSolveTransposeAdd_SeqAIJ(Mat A,Vec bb,Vec zz,Vec xx)
{
  Mat_SeqAIJ      *a = (Mat_SeqAIJ*)A->data;
  IS              iscol = a->col,isrow = a->row;
  PetscErrorCode  ierr;
  PetscInt        *r,*c,i,n = A->rmap.n,*vi,*ai = a->i,*aj = a->j;
  PetscInt        nz,*rout,*cout,*diag = a->diag;
  PetscScalar     *x,*b,*tmp;
  const MatScalar *aa = a->a,*v;

  PetscFunctionBegin;
  if (zz != xx) {ierr = VecCopy(zz,xx);CHKERRQ(ierr);}

  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  tmp = a->solve_work;

  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout;

  /* copy the b into temp work space according to permutation */
  for (i=0; i<n; i++) tmp[i] = b[c[i]]; 

  /* forward solve the U^T */
  for (i=0; i<n; i++) {
    v   = aa + diag[i] ;
    vi  = aj + diag[i] + 1;
    nz  = ai[i+1] - diag[i] - 1;
    tmp[i] *= *v++;
    while (nz--) {
      tmp[*vi++ ] -= (*v++)*tmp[i];
    }
  }

  /* backward solve the L^T */
  for (i=n-1; i>=0; i--){
    v   = aa + diag[i] - 1 ;
    vi  = aj + diag[i] - 1 ;
    nz  = diag[i] - ai[i];
    while (nz--) {
      tmp[*vi-- ] -= (*v--)*tmp[i];
    }
  }

  /* copy tmp into x according to permutation */
  for (i=0; i<n; i++) x[r[i]] += tmp[i]; 

  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);

  ierr = PetscLogFlops(2*a->nz);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
/* ----------------------------------------------------------------*/
EXTERN PetscErrorCode Mat_CheckInode(Mat,PetscTruth);
EXTERN PetscErrorCode MatDuplicateNoCreate_SeqAIJ(Mat,MatDuplicateOption,Mat*);

#undef __FUNCT__  
#define __FUNCT__ "MatILUFactorSymbolic_SeqAIJ"
PetscErrorCode MatILUFactorSymbolic_SeqAIJ(Mat A,IS isrow,IS iscol,MatFactorInfo *info,Mat *fact)
{
  Mat_SeqAIJ         *a = (Mat_SeqAIJ*)A->data,*b;
  IS                 isicol;
  PetscErrorCode     ierr;
  PetscInt           *r,*ic,n=A->rmap.n,*ai=a->i,*aj=a->j,d;
  PetscInt           *bi,*cols,nnz,*cols_lvl;
  PetscInt           *bdiag,prow,fm,nzbd,len, reallocs=0,dcount=0;
  PetscInt           i,levels,diagonal_fill;
  PetscTruth         col_identity,row_identity;
  PetscReal          f;
  PetscInt           nlnk,*lnk,*lnk_lvl=PETSC_NULL;
  PetscBT            lnkbt;
  PetscInt           nzi,*bj,**bj_ptr,**bjlvl_ptr; 
  PetscFreeSpaceList free_space=PETSC_NULL,current_space=PETSC_NULL; 
  PetscFreeSpaceList free_space_lvl=PETSC_NULL,current_space_lvl=PETSC_NULL; 
  PetscTruth         missing;

  PetscFunctionBegin;
  if (A->rmap.n != A->cmap.n) SETERRQ2(PETSC_ERR_ARG_WRONG,"Must be square matrix, rows %D columns %D",A->rmap.n,A->cmap.n);
  f             = info->fill;
  levels        = (PetscInt)info->levels;
  diagonal_fill = (PetscInt)info->diagonal_fill;
  ierr = ISInvertPermutation(iscol,PETSC_DECIDE,&isicol);CHKERRQ(ierr);

  /* special case that simply copies fill pattern */
  ierr = ISIdentity(isrow,&row_identity);CHKERRQ(ierr);
  ierr = ISIdentity(iscol,&col_identity);CHKERRQ(ierr);
  if (!levels && row_identity && col_identity) {
    ierr = MatDuplicateNoCreate_SeqAIJ(A,MAT_DO_NOT_COPY_VALUES,fact);CHKERRQ(ierr);
    (*fact)->factor                 = MAT_FACTOR_LU;
    (*fact)->info.factor_mallocs    = 0;
    (*fact)->info.fill_ratio_given  = info->fill;
    (*fact)->info.fill_ratio_needed = 1.0;
    b               = (Mat_SeqAIJ*)(*fact)->data;
    ierr = MatMissingDiagonal(A,&missing,&d);CHKERRQ(ierr);
    if (missing) SETERRQ1(PETSC_ERR_ARG_WRONGSTATE,"Matrix is missing diagonal entry %D",d);
    b->row              = isrow;
    b->col              = iscol;
    b->icol             = isicol;
    ierr                = PetscMalloc(((*fact)->rmap.n+1)*sizeof(PetscScalar),&b->solve_work);CHKERRQ(ierr);
    (*fact)->ops->solve = MatSolve_SeqAIJ_NaturalOrdering;
    ierr                = PetscObjectReference((PetscObject)isrow);CHKERRQ(ierr);
    ierr                = PetscObjectReference((PetscObject)iscol);CHKERRQ(ierr);
    ierr = Mat_CheckInode(*fact,PETSC_FALSE);CHKERRQ(ierr);
    PetscFunctionReturn(0);
  }

  ierr = ISGetIndices(isrow,&r);CHKERRQ(ierr);
  ierr = ISGetIndices(isicol,&ic);CHKERRQ(ierr);

  /* get new row pointers */
  ierr = PetscMalloc((n+1)*sizeof(PetscInt),&bi);CHKERRQ(ierr);
  bi[0] = 0;
  /* bdiag is location of diagonal in factor */
  ierr = PetscMalloc((n+1)*sizeof(PetscInt),&bdiag);CHKERRQ(ierr);
  bdiag[0]  = 0;

  ierr = PetscMalloc((2*n+1)*sizeof(PetscInt**),&bj_ptr);CHKERRQ(ierr); 
  bjlvl_ptr = (PetscInt**)(bj_ptr + n);

  /* create a linked list for storing column indices of the active row */
  nlnk = n + 1;
  ierr = PetscIncompleteLLCreate(n,n,nlnk,lnk,lnk_lvl,lnkbt);CHKERRQ(ierr);

  /* initial FreeSpace size is f*(ai[n]+1) */
  ierr = PetscFreeSpaceGet((PetscInt)(f*(ai[n]+1)),&free_space);CHKERRQ(ierr);
  current_space = free_space;
  ierr = PetscFreeSpaceGet((PetscInt)(f*(ai[n]+1)),&free_space_lvl);CHKERRQ(ierr);
  current_space_lvl = free_space_lvl;
 
  for (i=0; i<n; i++) {
    nzi = 0;
    /* copy current row into linked list */
    nnz  = ai[r[i]+1] - ai[r[i]];
    if (!nnz) SETERRQ(PETSC_ERR_MAT_LU_ZRPVT,"Empty row in matrix");
    cols = aj + ai[r[i]];
    lnk[i] = -1; /* marker to indicate if diagonal exists */
    ierr = PetscIncompleteLLInit(nnz,cols,n,ic,nlnk,lnk,lnk_lvl,lnkbt);CHKERRQ(ierr);
    nzi += nlnk;

    /* make sure diagonal entry is included */
    if (diagonal_fill && lnk[i] == -1) {
      fm = n;
      while (lnk[fm] < i) fm = lnk[fm];
      lnk[i]     = lnk[fm]; /* insert diagonal into linked list */
      lnk[fm]    = i;
      lnk_lvl[i] = 0;
      nzi++; dcount++; 
    }

    /* add pivot rows into the active row */
    nzbd = 0;
    prow = lnk[n];
    while (prow < i) {
      nnz      = bdiag[prow];
      cols     = bj_ptr[prow] + nnz + 1;
      cols_lvl = bjlvl_ptr[prow] + nnz + 1; 
      nnz      = bi[prow+1] - bi[prow] - nnz - 1;
      ierr = PetscILULLAddSorted(nnz,cols,levels,cols_lvl,prow,nlnk,lnk,lnk_lvl,lnkbt,prow);CHKERRQ(ierr);
      nzi += nlnk;
      prow = lnk[prow];
      nzbd++;
    }
    bdiag[i] = nzbd;
    bi[i+1]  = bi[i] + nzi;

    /* if free space is not available, make more free space */
    if (current_space->local_remaining<nzi) {
      nnz = nzi*(n - i); /* estimated and max additional space needed */
      ierr = PetscFreeSpaceGet(nnz,&current_space);CHKERRQ(ierr);
      ierr = PetscFreeSpaceGet(nnz,&current_space_lvl);CHKERRQ(ierr);
      reallocs++;
    }

    /* copy data into free_space and free_space_lvl, then initialize lnk */
    ierr = PetscIncompleteLLClean(n,n,nzi,lnk,lnk_lvl,current_space->array,current_space_lvl->array,lnkbt);CHKERRQ(ierr);
    bj_ptr[i]    = current_space->array;
    bjlvl_ptr[i] = current_space_lvl->array;

    /* make sure the active row i has diagonal entry */
    if (*(bj_ptr[i]+bdiag[i]) != i) {
      SETERRQ1(PETSC_ERR_MAT_LU_ZRPVT,"Row %D has missing diagonal in factored matrix\n\
    try running with -pc_factor_nonzeros_along_diagonal or -pc_factor_diagonal_fill",i);
    }

    current_space->array           += nzi;
    current_space->local_used      += nzi;
    current_space->local_remaining -= nzi;
    current_space_lvl->array           += nzi;
    current_space_lvl->local_used      += nzi;
    current_space_lvl->local_remaining -= nzi;
  } 

  ierr = ISRestoreIndices(isrow,&r);CHKERRQ(ierr);
  ierr = ISRestoreIndices(isicol,&ic);CHKERRQ(ierr);

  /* destroy list of free space and other temporary arrays */
  ierr = PetscMalloc((bi[n]+1)*sizeof(PetscInt),&bj);CHKERRQ(ierr);
  ierr = PetscFreeSpaceContiguous(&free_space,bj);CHKERRQ(ierr); 
  ierr = PetscIncompleteLLDestroy(lnk,lnkbt);CHKERRQ(ierr);
  ierr = PetscFreeSpaceDestroy(free_space_lvl);CHKERRQ(ierr); 
  ierr = PetscFree(bj_ptr);CHKERRQ(ierr);

#if defined(PETSC_USE_INFO)
  {
    PetscReal af = ((PetscReal)bi[n])/((PetscReal)ai[n]);
    ierr = PetscInfo3(A,"Reallocs %D Fill ratio:given %G needed %G\n",reallocs,f,af);CHKERRQ(ierr);
    ierr = PetscInfo1(A,"Run with -[sub_]pc_factor_fill %G or use \n",af);CHKERRQ(ierr);
    ierr = PetscInfo1(A,"PCFactorSetFill([sub]pc,%G);\n",af);CHKERRQ(ierr);
    ierr = PetscInfo(A,"for best performance.\n");CHKERRQ(ierr);
    if (diagonal_fill) {
      ierr = PetscInfo1(A,"Detected and replaced %D missing diagonals",dcount);CHKERRQ(ierr);
    }
  }
#endif

  /* put together the new matrix */
  ierr = PetscLogObjectParent(*fact,isicol);CHKERRQ(ierr);
  b = (Mat_SeqAIJ*)(*fact)->data;
  b->free_a       = PETSC_TRUE;
  b->free_ij      = PETSC_TRUE;
  b->singlemalloc = PETSC_FALSE;
  len = (bi[n] )*sizeof(PetscScalar);
  ierr = PetscMalloc(len+1,&b->a);CHKERRQ(ierr);
  b->j          = bj;
  b->i          = bi;
  for (i=0; i<n; i++) bdiag[i] += bi[i];
  b->diag       = bdiag;
  b->ilen       = 0;
  b->imax       = 0;
  b->row        = isrow;
  b->col        = iscol;
  ierr          = PetscObjectReference((PetscObject)isrow);CHKERRQ(ierr);
  ierr          = PetscObjectReference((PetscObject)iscol);CHKERRQ(ierr);
  b->icol       = isicol;
  ierr = PetscMalloc((n+1)*sizeof(PetscScalar),&b->solve_work);CHKERRQ(ierr);
  /* In b structure:  Free imax, ilen, old a, old j.  
     Allocate bdiag, solve_work, new a, new j */
  ierr = PetscLogObjectMemory(*fact,(bi[n]-n) * (sizeof(PetscInt)+sizeof(PetscScalar)));CHKERRQ(ierr);
  b->maxnz             = b->nz = bi[n] ;
  (*fact)->factor = MAT_FACTOR_LU;
  (*fact)->info.factor_mallocs    = reallocs;
  (*fact)->info.fill_ratio_given  = f;
  (*fact)->info.fill_ratio_needed = ((PetscReal)bi[n])/((PetscReal)ai[n]);

  ierr = MatILUFactorSymbolic_Inode(A,isrow,iscol,info,fact);CHKERRQ(ierr); 
  (*fact)->ops->lufactornumeric =  A->ops->lufactornumeric; /* Use Inode variant ONLY if A has inodes */

  PetscFunctionReturn(0); 
}

#include "src/mat/impls/sbaij/seq/sbaij.h"
#undef __FUNCT__  
#define __FUNCT__ "MatCholeskyFactorNumeric_SeqAIJ"
PetscErrorCode MatCholeskyFactorNumeric_SeqAIJ(Mat A,MatFactorInfo *info,Mat *B)
{
  Mat            C = *B;
  Mat_SeqAIJ     *a=(Mat_SeqAIJ*)A->data;
  Mat_SeqSBAIJ   *b=(Mat_SeqSBAIJ*)C->data;
  IS             ip=b->row,iip = b->icol;
  PetscErrorCode ierr;
  PetscInt       *rip,*riip,i,j,mbs=A->rmap.n,*bi=b->i,*bj=b->j,*bcol;
  PetscInt       *ai=a->i,*aj=a->j;
  PetscInt       k,jmin,jmax,*jl,*il,col,nexti,ili,nz;
  MatScalar      *rtmp,*ba=b->a,*bval,*aa=a->a,dk,uikdi;
  PetscReal      zeropivot,rs,shiftnz;
  PetscReal      shiftpd;
  ChShift_Ctx    sctx;
  PetscInt       newshift;

  PetscFunctionBegin;

  shiftnz   = info->shiftnz;
  shiftpd   = info->shiftpd;
  zeropivot = info->zeropivot; 

  ierr  = ISGetIndices(ip,&rip);CHKERRQ(ierr);
  ierr  = ISGetIndices(iip,&riip);CHKERRQ(ierr);
  
  /* initialization */
  nz   = (2*mbs+1)*sizeof(PetscInt)+mbs*sizeof(MatScalar);
  ierr = PetscMalloc(nz,&il);CHKERRQ(ierr);
  jl   = il + mbs;
  rtmp = (MatScalar*)(jl + mbs);

  sctx.shift_amount = 0;
  sctx.nshift       = 0;
  do {
    sctx.chshift = PETSC_FALSE;
    for (i=0; i<mbs; i++) {
      rtmp[i] = 0.0; jl[i] = mbs; il[0] = 0;
    } 
 
    for (k = 0; k<mbs; k++){
      bval = ba + bi[k];
      /* initialize k-th row by the perm[k]-th row of A */
      jmin = ai[rip[k]]; jmax = ai[rip[k]+1];
      for (j = jmin; j < jmax; j++){
        col = riip[aj[j]];
        if (col >= k){ /* only take upper triangular entry */
          rtmp[col] = aa[j];
          *bval++  = 0.0; /* for in-place factorization */
        }
      } 
      /* shift the diagonal of the matrix */
      if (sctx.nshift) rtmp[k] += sctx.shift_amount; 

      /* modify k-th row by adding in those rows i with U(i,k)!=0 */
      dk = rtmp[k];
      i = jl[k]; /* first row to be added to k_th row  */  

      while (i < k){
        nexti = jl[i]; /* next row to be added to k_th row */

        /* compute multiplier, update diag(k) and U(i,k) */
        ili = il[i];  /* index of first nonzero element in U(i,k:bms-1) */
        uikdi = - ba[ili]*ba[bi[i]];  /* diagonal(k) */ 
        dk += uikdi*ba[ili];
        ba[ili] = uikdi; /* -U(i,k) */

        /* add multiple of row i to k-th row */
        jmin = ili + 1; jmax = bi[i+1];
        if (jmin < jmax){
          for (j=jmin; j<jmax; j++) rtmp[bj[j]] += uikdi*ba[j];         
          /* update il and jl for row i */
          il[i] = jmin;             
          j = bj[jmin]; jl[i] = jl[j]; jl[j] = i; 
        }      
        i = nexti;         
      }

      /* shift the diagonals when zero pivot is detected */
      /* compute rs=sum of abs(off-diagonal) */
      rs   = 0.0;
      jmin = bi[k]+1; 
      nz   = bi[k+1] - jmin; 
      bcol = bj + jmin;
      while (nz--){
        rs += PetscAbsScalar(rtmp[*bcol]);
        bcol++;
      }

      sctx.rs = rs;
      sctx.pv = dk;
      ierr = MatCholeskyCheckShift_inline(info,sctx,k,newshift);CHKERRQ(ierr); 

      if (newshift == 1) {
        if (!sctx.shift_amount) {
          sctx.shift_amount = 1e-5;
        }
        break;
      }
   
      /* copy data into U(k,:) */
      ba[bi[k]] = 1.0/dk; /* U(k,k) */
      jmin = bi[k]+1; jmax = bi[k+1];
      if (jmin < jmax) {
        for (j=jmin; j<jmax; j++){
          col = bj[j]; ba[j] = rtmp[col]; rtmp[col] = 0.0;
        }       
        /* add the k-th row into il and jl */
        il[k] = jmin;
        i = bj[jmin]; jl[k] = jl[i]; jl[i] = k;
      }        
    } 
  } while (sctx.chshift);
  ierr = PetscFree(il);CHKERRQ(ierr);

  ierr = ISRestoreIndices(ip,&rip);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iip,&riip);CHKERRQ(ierr);
  C->factor       = MAT_FACTOR_CHOLESKY; 
  C->assembled    = PETSC_TRUE; 
  C->preallocated = PETSC_TRUE;
  ierr = PetscLogFlops(C->rmap.n);CHKERRQ(ierr);
  if (sctx.nshift){
    if (shiftnz) {
      ierr = PetscInfo2(A,"number of shiftnz tries %D, shift_amount %G\n",sctx.nshift,sctx.shift_amount);CHKERRQ(ierr);
    } else if (shiftpd) {
      ierr = PetscInfo2(A,"number of shiftpd tries %D, shift_amount %G\n",sctx.nshift,sctx.shift_amount);CHKERRQ(ierr);
    }
  }
  PetscFunctionReturn(0); 
}

#undef __FUNCT__  
#define __FUNCT__ "MatICCFactorSymbolic_SeqAIJ"
PetscErrorCode MatICCFactorSymbolic_SeqAIJ(Mat A,IS perm,MatFactorInfo *info,Mat *fact)
{
  Mat_SeqAIJ         *a = (Mat_SeqAIJ*)A->data;
  Mat_SeqSBAIJ       *b;
  PetscErrorCode     ierr;
  PetscTruth         perm_identity,missing;
  PetscInt           reallocs=0,*rip,*riip,i,*ai=a->i,*aj=a->j,am=A->rmap.n,*ui;
  PetscInt           jmin,jmax,nzk,k,j,*jl,prow,*il,nextprow;
  PetscInt           nlnk,*lnk,*lnk_lvl=PETSC_NULL,d;
  PetscInt           ncols,ncols_upper,*cols,*ajtmp,*uj,**uj_ptr,**uj_lvl_ptr;
  PetscReal          fill=info->fill,levels=info->levels;
  PetscFreeSpaceList free_space=PETSC_NULL,current_space=PETSC_NULL;
  PetscFreeSpaceList free_space_lvl=PETSC_NULL,current_space_lvl=PETSC_NULL;
  PetscBT            lnkbt;
  IS                 iperm;  
  
  PetscFunctionBegin;   
  if (A->rmap.n != A->cmap.n) SETERRQ2(PETSC_ERR_ARG_WRONG,"Must be square matrix, rows %D columns %D",A->rmap.n,A->cmap.n);
  ierr = MatMissingDiagonal(A,&missing,&d);CHKERRQ(ierr);
  if (missing) SETERRQ1(PETSC_ERR_ARG_WRONGSTATE,"Matrix is missing diagonal entry %D",d);
  ierr = ISIdentity(perm,&perm_identity);CHKERRQ(ierr);
  ierr = ISInvertPermutation(perm,PETSC_DECIDE,&iperm);CHKERRQ(ierr);

  ierr = PetscMalloc((am+1)*sizeof(PetscInt),&ui);CHKERRQ(ierr); 
  ui[0] = 0;

  /* ICC(0) without matrix ordering: simply copies fill pattern */
  if (!levels && perm_identity) { 

    for (i=0; i<am; i++) {
      ui[i+1] = ui[i] + ai[i+1] - a->diag[i]; 
    }
    ierr = PetscMalloc((ui[am]+1)*sizeof(PetscInt),&uj);CHKERRQ(ierr); 
    cols = uj;
    for (i=0; i<am; i++) {
      aj    = a->j + a->diag[i];  
      ncols = ui[i+1] - ui[i];
      for (j=0; j<ncols; j++) *cols++ = *aj++; 
    }
  } else { /* case: levels>0 || (levels=0 && !perm_identity) */
    ierr = ISGetIndices(iperm,&riip);CHKERRQ(ierr);
    ierr = ISGetIndices(perm,&rip);CHKERRQ(ierr);

    /* initialization */
    ierr  = PetscMalloc((am+1)*sizeof(PetscInt),&ajtmp);CHKERRQ(ierr); 

    /* jl: linked list for storing indices of the pivot rows 
       il: il[i] points to the 1st nonzero entry of U(i,k:am-1) */
    ierr = PetscMalloc((2*am+1)*sizeof(PetscInt)+2*am*sizeof(PetscInt**),&jl);CHKERRQ(ierr); 
    il         = jl + am;
    uj_ptr     = (PetscInt**)(il + am);
    uj_lvl_ptr = (PetscInt**)(uj_ptr + am);
    for (i=0; i<am; i++){
      jl[i] = am; il[i] = 0;
    }

    /* create and initialize a linked list for storing column indices of the active row k */
    nlnk = am + 1;
    ierr = PetscIncompleteLLCreate(am,am,nlnk,lnk,lnk_lvl,lnkbt);CHKERRQ(ierr);

    /* initial FreeSpace size is fill*(ai[am]+1) */
    ierr = PetscFreeSpaceGet((PetscInt)(fill*(ai[am]+1)),&free_space);CHKERRQ(ierr);
    current_space = free_space;
    ierr = PetscFreeSpaceGet((PetscInt)(fill*(ai[am]+1)),&free_space_lvl);CHKERRQ(ierr);
    current_space_lvl = free_space_lvl;

    for (k=0; k<am; k++){  /* for each active row k */
      /* initialize lnk by the column indices of row rip[k] of A */
      nzk   = 0;
      ncols = ai[rip[k]+1] - ai[rip[k]]; 
      if (!ncols) SETERRQ(PETSC_ERR_MAT_CH_ZRPVT,"Empty row in matrix");
      ncols_upper = 0;
      for (j=0; j<ncols; j++){
        i = *(aj + ai[rip[k]] + j); /* unpermuted column index */
        if (riip[i] >= k){ /* only take upper triangular entry */
          ajtmp[ncols_upper] = i; 
          ncols_upper++;
        }
      }
      ierr = PetscIncompleteLLInit(ncols_upper,ajtmp,am,riip,nlnk,lnk,lnk_lvl,lnkbt);CHKERRQ(ierr);
      nzk += nlnk;

      /* update lnk by computing fill-in for each pivot row to be merged in */
      prow = jl[k]; /* 1st pivot row */
   
      while (prow < k){
        nextprow = jl[prow];
      
        /* merge prow into k-th row */
        jmin = il[prow] + 1;  /* index of the 2nd nzero entry in U(prow,k:am-1) */
        jmax = ui[prow+1]; 
        ncols = jmax-jmin;
        i     = jmin - ui[prow];
        cols  = uj_ptr[prow] + i; /* points to the 2nd nzero entry in U(prow,k:am-1) */
        uj    = uj_lvl_ptr[prow] + i; /* levels of cols */
        j     = *(uj - 1); 
        ierr = PetscICCLLAddSorted(ncols,cols,levels,uj,am,nlnk,lnk,lnk_lvl,lnkbt,j);CHKERRQ(ierr); 
        nzk += nlnk;

        /* update il and jl for prow */
        if (jmin < jmax){
          il[prow] = jmin;
          j = *cols; jl[prow] = jl[j]; jl[j] = prow;  
        } 
        prow = nextprow; 
      }  

      /* if free space is not available, make more free space */
      if (current_space->local_remaining<nzk) {
        i = am - k + 1; /* num of unfactored rows */
        i = PetscMin(i*nzk, i*(i-1)); /* i*nzk, i*(i-1): estimated and max additional space needed */
        ierr = PetscFreeSpaceGet(i,&current_space);CHKERRQ(ierr);
        ierr = PetscFreeSpaceGet(i,&current_space_lvl);CHKERRQ(ierr);
        reallocs++;
      }

      /* copy data into free_space and free_space_lvl, then initialize lnk */
      if (nzk == 0) SETERRQ1(PETSC_ERR_ARG_WRONG,"Empty row %D in ICC matrix factor",k);
      ierr = PetscIncompleteLLClean(am,am,nzk,lnk,lnk_lvl,current_space->array,current_space_lvl->array,lnkbt);CHKERRQ(ierr);

      /* add the k-th row into il and jl */
      if (nzk > 1){
        i = current_space->array[1]; /* col value of the first nonzero element in U(k, k+1:am-1) */    
        jl[k] = jl[i]; jl[i] = k;
        il[k] = ui[k] + 1;
      } 
      uj_ptr[k]     = current_space->array;
      uj_lvl_ptr[k] = current_space_lvl->array; 

      current_space->array           += nzk;
      current_space->local_used      += nzk;
      current_space->local_remaining -= nzk;

      current_space_lvl->array           += nzk;
      current_space_lvl->local_used      += nzk;
      current_space_lvl->local_remaining -= nzk;

      ui[k+1] = ui[k] + nzk;  
    } 

#if defined(PETSC_USE_INFO)
    if (ai[am] != 0) {
      PetscReal af = (PetscReal)ui[am]/((PetscReal)ai[am]);
      ierr = PetscInfo3(A,"Reallocs %D Fill ratio:given %G needed %G\n",reallocs,fill,af);CHKERRQ(ierr);
      ierr = PetscInfo1(A,"Run with -pc_factor_fill %G or use \n",af);CHKERRQ(ierr);
      ierr = PetscInfo1(A,"PCFactorSetFill(pc,%G) for best performance.\n",af);CHKERRQ(ierr);
    } else {
      ierr = PetscInfo(A,"Empty matrix.\n");CHKERRQ(ierr);
    }
#endif

    ierr = ISRestoreIndices(perm,&rip);CHKERRQ(ierr);
    ierr = ISRestoreIndices(iperm,&riip);CHKERRQ(ierr);
    ierr = PetscFree(jl);CHKERRQ(ierr);
    ierr = PetscFree(ajtmp);CHKERRQ(ierr);

    /* destroy list of free space and other temporary array(s) */
    ierr = PetscMalloc((ui[am]+1)*sizeof(PetscInt),&uj);CHKERRQ(ierr);
    ierr = PetscFreeSpaceContiguous(&free_space,uj);CHKERRQ(ierr);
    ierr = PetscIncompleteLLDestroy(lnk,lnkbt);CHKERRQ(ierr);
    ierr = PetscFreeSpaceDestroy(free_space_lvl);CHKERRQ(ierr);

  } /* end of case: levels>0 || (levels=0 && !perm_identity) */

  /* put together the new matrix in MATSEQSBAIJ format */

  b    = (Mat_SeqSBAIJ*)(*fact)->data;
  b->singlemalloc = PETSC_FALSE;
  ierr = PetscMalloc((ui[am]+1)*sizeof(MatScalar),&b->a);CHKERRQ(ierr);
  b->j    = uj;
  b->i    = ui;
  b->diag = 0;
  b->ilen = 0;
  b->imax = 0;
  b->row  = perm;
  b->col  = perm;
  ierr    = PetscObjectReference((PetscObject)perm);CHKERRQ(ierr); 
  ierr    = PetscObjectReference((PetscObject)perm);CHKERRQ(ierr); 
  b->icol = iperm;
  b->pivotinblocks = PETSC_FALSE; /* need to get from MatFactorInfo */
  ierr    = PetscMalloc((am+1)*sizeof(PetscScalar),&b->solve_work);CHKERRQ(ierr);
  ierr = PetscLogObjectMemory((*fact),(ui[am]-am)*(sizeof(PetscInt)+sizeof(MatScalar)));CHKERRQ(ierr);
  b->maxnz   = b->nz = ui[am];
  b->free_a  = PETSC_TRUE; 
  b->free_ij = PETSC_TRUE; 
  
  (*fact)->factor                 = MAT_FACTOR_CHOLESKY;
  (*fact)->info.factor_mallocs    = reallocs;
  (*fact)->info.fill_ratio_given  = fill;
  if (ai[am] != 0) {
    (*fact)->info.fill_ratio_needed = ((PetscReal)ui[am])/((PetscReal)ai[am]);
  } else {
    (*fact)->info.fill_ratio_needed = 0.0;
  }
  (*fact)->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqAIJ;
  if (perm_identity){
    (*fact)->ops->solve          = MatSolve_SeqSBAIJ_1_NaturalOrdering;
    (*fact)->ops->solvetranspose = MatSolve_SeqSBAIJ_1_NaturalOrdering;
  } 
  PetscFunctionReturn(0); 
}

#undef __FUNCT__  
#define __FUNCT__ "MatCholeskyFactorSymbolic_SeqAIJ"
PetscErrorCode MatCholeskyFactorSymbolic_SeqAIJ(Mat A,IS perm,MatFactorInfo *info,Mat *fact)
{
  Mat_SeqAIJ         *a = (Mat_SeqAIJ*)A->data;
  Mat_SeqSBAIJ       *b;
  PetscErrorCode     ierr;
  PetscTruth         perm_identity;
  PetscReal          fill = info->fill;
  PetscInt           *rip,*riip,i,am=A->rmap.n,*ai=a->i,*aj=a->j,reallocs=0,prow;
  PetscInt           *jl,jmin,jmax,nzk,*ui,k,j,*il,nextprow;
  PetscInt           nlnk,*lnk,ncols,ncols_upper,*cols,*uj,**ui_ptr,*uj_ptr;
  PetscFreeSpaceList free_space=PETSC_NULL,current_space=PETSC_NULL;
  PetscBT            lnkbt;
  IS                 iperm;  

  PetscFunctionBegin;
  if (A->rmap.n != A->cmap.n) SETERRQ2(PETSC_ERR_ARG_WRONG,"Must be square matrix, rows %D columns %D",A->rmap.n,A->cmap.n);
  /* check whether perm is the identity mapping */
  ierr = ISIdentity(perm,&perm_identity);CHKERRQ(ierr);  
  ierr = ISInvertPermutation(perm,PETSC_DECIDE,&iperm);CHKERRQ(ierr);
  ierr = ISGetIndices(iperm,&riip);CHKERRQ(ierr);  
  ierr = ISGetIndices(perm,&rip);CHKERRQ(ierr);

  /* initialization */
  ierr  = PetscMalloc((am+1)*sizeof(PetscInt),&ui);CHKERRQ(ierr);
  ui[0] = 0; 

  /* jl: linked list for storing indices of the pivot rows 
     il: il[i] points to the 1st nonzero entry of U(i,k:am-1) */
  ierr = PetscMalloc((3*am+1)*sizeof(PetscInt)+am*sizeof(PetscInt**),&jl);CHKERRQ(ierr); 
  il     = jl + am;
  cols   = il + am;
  ui_ptr = (PetscInt**)(cols + am);
  for (i=0; i<am; i++){
    jl[i] = am; il[i] = 0;
  }

  /* create and initialize a linked list for storing column indices of the active row k */
  nlnk = am + 1;
  ierr = PetscLLCreate(am,am,nlnk,lnk,lnkbt);CHKERRQ(ierr);

  /* initial FreeSpace size is fill*(ai[am]+1) */
  ierr = PetscFreeSpaceGet((PetscInt)(fill*(ai[am]+1)),&free_space);CHKERRQ(ierr);
  current_space = free_space;

  for (k=0; k<am; k++){  /* for each active row k */
    /* initialize lnk by the column indices of row rip[k] of A */
    nzk   = 0;
    ncols = ai[rip[k]+1] - ai[rip[k]]; 
    if (!ncols) SETERRQ(PETSC_ERR_MAT_CH_ZRPVT,"Empty row in matrix");
    ncols_upper = 0;
    for (j=0; j<ncols; j++){
      i = riip[*(aj + ai[rip[k]] + j)];  
      if (i >= k){ /* only take upper triangular entry */
        cols[ncols_upper] = i;
        ncols_upper++;
      }
    }
    ierr = PetscLLAdd(ncols_upper,cols,am,nlnk,lnk,lnkbt);CHKERRQ(ierr);
    nzk += nlnk;

    /* update lnk by computing fill-in for each pivot row to be merged in */
    prow = jl[k]; /* 1st pivot row */
   
    while (prow < k){
      nextprow = jl[prow];
      /* merge prow into k-th row */
      jmin = il[prow] + 1;  /* index of the 2nd nzero entry in U(prow,k:am-1) */
      jmax = ui[prow+1]; 
      ncols = jmax-jmin;
      uj_ptr = ui_ptr[prow] + jmin - ui[prow]; /* points to the 2nd nzero entry in U(prow,k:am-1) */
      ierr = PetscLLAddSorted(ncols,uj_ptr,am,nlnk,lnk,lnkbt);CHKERRQ(ierr); 
      nzk += nlnk;

      /* update il and jl for prow */
      if (jmin < jmax){
        il[prow] = jmin;
        j = *uj_ptr; jl[prow] = jl[j]; jl[j] = prow;  
      } 
      prow = nextprow; 
    }  

    /* if free space is not available, make more free space */
    if (current_space->local_remaining<nzk) {
      i = am - k + 1; /* num of unfactored rows */
      i = PetscMin(i*nzk, i*(i-1)); /* i*nzk, i*(i-1): estimated and max additional space needed */
      ierr = PetscFreeSpaceGet(i,&current_space);CHKERRQ(ierr);
      reallocs++;
    }

    /* copy data into free space, then initialize lnk */
    ierr = PetscLLClean(am,am,nzk,lnk,current_space->array,lnkbt);CHKERRQ(ierr); 

    /* add the k-th row into il and jl */
    if (nzk-1 > 0){
      i = current_space->array[1]; /* col value of the first nonzero element in U(k, k+1:am-1) */    
      jl[k] = jl[i]; jl[i] = k;
      il[k] = ui[k] + 1;
    } 
    ui_ptr[k] = current_space->array;
    current_space->array           += nzk;
    current_space->local_used      += nzk;
    current_space->local_remaining -= nzk;

    ui[k+1] = ui[k] + nzk;  
  } 

#if defined(PETSC_USE_INFO)
  if (ai[am] != 0) {
    PetscReal af = (PetscReal)(ui[am])/((PetscReal)ai[am]);
    ierr = PetscInfo3(A,"Reallocs %D Fill ratio:given %G needed %G\n",reallocs,fill,af);CHKERRQ(ierr);
    ierr = PetscInfo1(A,"Run with -pc_factor_fill %G or use \n",af);CHKERRQ(ierr);
    ierr = PetscInfo1(A,"PCFactorSetFill(pc,%G) for best performance.\n",af);CHKERRQ(ierr);
  } else {
     ierr = PetscInfo(A,"Empty matrix.\n");CHKERRQ(ierr);
  }
#endif

  ierr = ISRestoreIndices(perm,&rip);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iperm,&riip);CHKERRQ(ierr);
  ierr = PetscFree(jl);CHKERRQ(ierr);

  /* destroy list of free space and other temporary array(s) */
  ierr = PetscMalloc((ui[am]+1)*sizeof(PetscInt),&uj);CHKERRQ(ierr);
  ierr = PetscFreeSpaceContiguous(&free_space,uj);CHKERRQ(ierr);
  ierr = PetscLLDestroy(lnk,lnkbt);CHKERRQ(ierr);

  /* put together the new matrix in MATSEQSBAIJ format */

  b = (Mat_SeqSBAIJ*)(*fact)->data;
  b->singlemalloc = PETSC_FALSE;
  b->free_a       = PETSC_TRUE;
  b->free_ij      = PETSC_TRUE;
  ierr = PetscMalloc((ui[am]+1)*sizeof(MatScalar),&b->a);CHKERRQ(ierr);
  b->j    = uj;
  b->i    = ui;
  b->diag = 0;
  b->ilen = 0;
  b->imax = 0;
  b->row  = perm;
  b->col  = perm;
  ierr    = PetscObjectReference((PetscObject)perm);CHKERRQ(ierr); 
  ierr    = PetscObjectReference((PetscObject)perm);CHKERRQ(ierr); 
  b->icol = iperm;
  b->pivotinblocks = PETSC_FALSE; /* need to get from MatFactorInfo */
  ierr    = PetscMalloc((am+1)*sizeof(PetscScalar),&b->solve_work);CHKERRQ(ierr);
  ierr    = PetscLogObjectMemory(*fact,(ui[am]-am)*(sizeof(PetscInt)+sizeof(MatScalar)));CHKERRQ(ierr);
  b->maxnz = b->nz = ui[am];
  
  (*fact)->factor                 = MAT_FACTOR_CHOLESKY;
  (*fact)->info.factor_mallocs    = reallocs;
  (*fact)->info.fill_ratio_given  = fill;
  if (ai[am] != 0) {
    (*fact)->info.fill_ratio_needed = ((PetscReal)ui[am])/((PetscReal)ai[am]);
  } else {
    (*fact)->info.fill_ratio_needed = 0.0;
  }
  (*fact)->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqAIJ;
  if (perm_identity){
    (*fact)->ops->solve           = MatSolve_SeqSBAIJ_1_NaturalOrdering;
    (*fact)->ops->solvetranspose  = MatSolve_SeqSBAIJ_1_NaturalOrdering;
    (*fact)->ops->forwardsolve    = MatForwardSolve_SeqSBAIJ_1_NaturalOrdering;
    (*fact)->ops->backwardsolve   = MatBackwardSolve_SeqSBAIJ_1_NaturalOrdering;
  } else {
    (*fact)->ops->solve           = MatSolve_SeqSBAIJ_1;
    (*fact)->ops->solvetranspose  = MatSolve_SeqSBAIJ_1;
    (*fact)->ops->forwardsolve    = MatForwardSolve_SeqSBAIJ_1;
    (*fact)->ops->backwardsolve   = MatBackwardSolve_SeqSBAIJ_1;
  }
  PetscFunctionReturn(0); 
}
