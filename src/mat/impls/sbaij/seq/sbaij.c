#define PETSCMAT_DLL

/*
    Defines the basic matrix operations for the SBAIJ (compressed row)
  matrix storage format.
*/
#include "src/mat/impls/baij/seq/baij.h"         /*I "petscmat.h" I*/
#include "src/inline/spops.h"
#include "src/mat/impls/sbaij/seq/sbaij.h"

#define CHUNKSIZE  10

/*
     Checks for missing diagonals
*/
#undef __FUNCT__  
#define __FUNCT__ "MatMissingDiagonal_SeqSBAIJ"
PetscErrorCode MatMissingDiagonal_SeqSBAIJ(Mat A)
{
  Mat_SeqSBAIJ   *a = (Mat_SeqSBAIJ*)A->data; 
  PetscErrorCode ierr;
  PetscInt       *diag,*jj = a->j,i;

  PetscFunctionBegin;
  ierr = MatMarkDiagonal_SeqSBAIJ(A);CHKERRQ(ierr);
  diag = a->diag;
  for (i=0; i<a->mbs; i++) {
    if (jj[diag[i]] != i) SETERRQ1(PETSC_ERR_ARG_CORRUPT,"Matrix is missing diagonal number %D",i);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMarkDiagonal_SeqSBAIJ"
PetscErrorCode MatMarkDiagonal_SeqSBAIJ(Mat A)
{
  Mat_SeqSBAIJ   *a = (Mat_SeqSBAIJ*)A->data; 
  PetscErrorCode ierr;
  PetscInt       i;

  PetscFunctionBegin;
  if (!a->diag) {
    ierr = PetscMalloc(a->mbs*sizeof(PetscInt),&a->diag);CHKERRQ(ierr); 
  }
  for (i=0; i<a->mbs; i++) a->diag[i] = a->i[i];  
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatGetRowIJ_SeqSBAIJ"
static PetscErrorCode MatGetRowIJ_SeqSBAIJ(Mat A,PetscInt oshift,PetscTruth symmetric,PetscTruth blockcompressed,PetscInt *nn,PetscInt *ia[],PetscInt *ja[],PetscTruth *done)
{
  Mat_SeqSBAIJ *a = (Mat_SeqSBAIJ*)A->data;
  PetscInt     i,j,n = a->mbs,nz = a->i[n],bs = A->rmap.bs;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  *nn = n;
  if (!ia) PetscFunctionReturn(0);
  if (!blockcompressed) {
    /* malloc & create the natural set of indices */
    ierr = PetscMalloc2((n+1)*bs,PetscInt,ia,nz*bs,PetscInt,ja);CHKERRQ(ierr);
    for (i=0; i<n+1; i++) {
      for (j=0; j<bs; j++) {
        *ia[i*bs+j] = a->i[i]*bs+j+oshift;
      }
    }
    for (i=0; i<nz; i++) {
      for (j=0; j<bs; j++) {
        *ja[i*bs+j] = a->j[i]*bs+j+oshift;
      }
    }
  } else { /* blockcompressed */
    if (oshift == 1) {
      /* temporarily add 1 to i and j indices */
      for (i=0; i<nz; i++) a->j[i]++;
      for (i=0; i<n+1; i++) a->i[i]++;
    }
    *ia = a->i; *ja = a->j;
  }

  PetscFunctionReturn(0); 
}

#undef __FUNCT__  
#define __FUNCT__ "MatRestoreRowIJ_SeqSBAIJ" 
static PetscErrorCode MatRestoreRowIJ_SeqSBAIJ(Mat A,PetscInt oshift,PetscTruth symmetric,PetscTruth blockcompressed,PetscInt *nn,PetscInt *ia[],PetscInt *ja[],PetscTruth *done)
{
  Mat_SeqSBAIJ *a = (Mat_SeqSBAIJ*)A->data;
  PetscInt     i,n = a->mbs,nz = a->i[n];
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!ia) PetscFunctionReturn(0);

  if (!blockcompressed) {
    ierr = PetscFree2(*ia,*ja);CHKERRQ(ierr);
  } else if (oshift == 1) { /* blockcompressed */
    for (i=0; i<nz; i++) a->j[i]--;
    for (i=0; i<n+1; i++) a->i[i]--;
  }

  PetscFunctionReturn(0); 
}

#undef __FUNCT__  
#define __FUNCT__ "MatDestroy_SeqSBAIJ"
PetscErrorCode MatDestroy_SeqSBAIJ(Mat A)
{
  Mat_SeqSBAIJ   *a = (Mat_SeqSBAIJ*)A->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
#if defined(PETSC_USE_LOG)
  PetscLogObjectState((PetscObject)A,"Rows=%D, NZ=%D",A->rmap.N,a->nz);
#endif
  ierr = MatSeqXAIJFreeAIJ(A,&a->a,&a->j,&a->i);CHKERRQ(ierr);
  if (a->row) {ierr = ISDestroy(a->row);CHKERRQ(ierr);}
  if (a->col){ierr = ISDestroy(a->col);CHKERRQ(ierr);}
  if (a->icol) {ierr = ISDestroy(a->icol);CHKERRQ(ierr);}
  ierr = PetscFree(a->diag);CHKERRQ(ierr);
  ierr = PetscFree2(a->imax,a->ilen);CHKERRQ(ierr);
  ierr = PetscFree(a->solve_work);CHKERRQ(ierr);
  ierr = PetscFree(a->relax_work);CHKERRQ(ierr);
  ierr = PetscFree(a->solves_work);CHKERRQ(ierr);
  ierr = PetscFree(a->mult_work);CHKERRQ(ierr);
  ierr = PetscFree(a->saved_values);CHKERRQ(ierr);
  ierr = PetscFree(a->xtoy);CHKERRQ(ierr);

  ierr = PetscFree(a->inew);CHKERRQ(ierr);
  ierr = PetscFree(a);CHKERRQ(ierr);

  ierr = PetscObjectChangeTypeName((PetscObject)A,0);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunction((PetscObject)A,"MatStoreValues_C","",PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunction((PetscObject)A,"MatRetrieveValues_C","",PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunction((PetscObject)A,"MatSeqSBAIJSetColumnIndices_C","",PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunction((PetscObject)A,"MatConvert_seqsbaij_seqaij_C","",PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunction((PetscObject)A,"MatConvert_seqsbaij_seqbaij_C","",PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunction((PetscObject)A,"MatSeqSBAIJSetPreallocation_C","",PETSC_NULL);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSetOption_SeqSBAIJ"
PetscErrorCode MatSetOption_SeqSBAIJ(Mat A,MatOption op)
{
  Mat_SeqSBAIJ   *a = (Mat_SeqSBAIJ*)A->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  switch (op) {
  case MAT_ROW_ORIENTED:
    a->roworiented = PETSC_TRUE;
    break;
  case MAT_COLUMN_ORIENTED:
    a->roworiented = PETSC_FALSE;
    break;
  case MAT_COLUMNS_SORTED:
    a->sorted = PETSC_TRUE;
    break;
  case MAT_COLUMNS_UNSORTED:
    a->sorted = PETSC_FALSE;
    break;
  case MAT_KEEP_ZEROED_ROWS:
    a->keepzeroedrows = PETSC_TRUE;
    break;
  case MAT_NO_NEW_NONZERO_LOCATIONS:
    a->nonew = 1;
    break;
  case MAT_NEW_NONZERO_LOCATION_ERR:
    a->nonew = -1;
    break;
  case MAT_NEW_NONZERO_ALLOCATION_ERR:
    a->nonew = -2;
    break;
  case MAT_YES_NEW_NONZERO_LOCATIONS:
    a->nonew = 0;
    break;
  case MAT_ROWS_SORTED:
  case MAT_ROWS_UNSORTED:
  case MAT_YES_NEW_DIAGONALS:
  case MAT_IGNORE_OFF_PROC_ENTRIES:
  case MAT_USE_HASH_TABLE:
    ierr = PetscInfo1(A,"Option %s ignored\n",MatOptions[op]);CHKERRQ(ierr);
    break;
  case MAT_NO_NEW_DIAGONALS:
    SETERRQ(PETSC_ERR_SUP,"MAT_NO_NEW_DIAGONALS");
  case MAT_NOT_SYMMETRIC:
  case MAT_NOT_STRUCTURALLY_SYMMETRIC:
  case MAT_HERMITIAN:
    SETERRQ(PETSC_ERR_SUP,"Matrix must be symmetric");
  case MAT_SYMMETRIC:
  case MAT_STRUCTURALLY_SYMMETRIC:
  case MAT_NOT_HERMITIAN:
  case MAT_SYMMETRY_ETERNAL:
  case MAT_NOT_SYMMETRY_ETERNAL:
    ierr = PetscInfo1(A,"Option %s not relevent\n",MatOptions[op]);CHKERRQ(ierr);
    break;
  case MAT_IGNORE_LOWER_TRIANGULAR:
    a->ignore_ltriangular = PETSC_TRUE;
    break;
  case MAT_ERROR_LOWER_TRIANGULAR:
    a->ignore_ltriangular = PETSC_FALSE;
    break;
  case MAT_GETROW_UPPERTRIANGULAR:
    a->getrow_utriangular = PETSC_TRUE;
    break;
  default:
    SETERRQ1(PETSC_ERR_SUP,"unknown option %d",op);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatGetRow_SeqSBAIJ"
PetscErrorCode MatGetRow_SeqSBAIJ(Mat A,PetscInt row,PetscInt *ncols,PetscInt **cols,PetscScalar **v)
{
  Mat_SeqSBAIJ   *a = (Mat_SeqSBAIJ*)A->data;
  PetscErrorCode ierr;
  PetscInt       itmp,i,j,k,M,*ai,*aj,bs,bn,bp,*cols_i,bs2;
  MatScalar      *aa,*aa_i;
  PetscScalar    *v_i;

  PetscFunctionBegin;
  if (A && !a->getrow_utriangular) SETERRQ(PETSC_ERR_SUP,"MatGetRow is not supported for SBAIJ matrix format. Getting the upper triangular part of row, run with -mat_getrow_uppertriangular, call MatSetOption(mat,MAT_GETROW_UPPERTRIANGULAR) or MatGetRowUpperTriangular()");
  /* Get the upper triangular part of the row */
  bs  = A->rmap.bs;
  ai  = a->i;
  aj  = a->j;
  aa  = a->a;
  bs2 = a->bs2;
  
  if (row < 0 || row >= A->rmap.N) SETERRQ1(PETSC_ERR_ARG_OUTOFRANGE, "Row %D out of range", row);
  
  bn  = row/bs;   /* Block number */
  bp  = row % bs; /* Block position */
  M   = ai[bn+1] - ai[bn];
  *ncols = bs*M;
  
  if (v) {
    *v = 0;
    if (*ncols) {
      ierr = PetscMalloc((*ncols+row)*sizeof(PetscScalar),v);CHKERRQ(ierr);
      for (i=0; i<M; i++) { /* for each block in the block row */
        v_i  = *v + i*bs;
        aa_i = aa + bs2*(ai[bn] + i);
        for (j=bp,k=0; j<bs2; j+=bs,k++) {v_i[k] = aa_i[j];}
      }
    }
  }
  
  if (cols) {
    *cols = 0;
    if (*ncols) {
      ierr = PetscMalloc((*ncols+row)*sizeof(PetscInt),cols);CHKERRQ(ierr);
      for (i=0; i<M; i++) { /* for each block in the block row */
        cols_i = *cols + i*bs;
        itmp  = bs*aj[ai[bn] + i];
        for (j=0; j<bs; j++) {cols_i[j] = itmp++;}
      }
    }
  }
  
  /*search column A(0:row-1,row) (=A(row,0:row-1)). Could be expensive! */
  /* this segment is currently removed, so only entries in the upper triangle are obtained */
#ifdef column_search
  v_i    = *v    + M*bs;
  cols_i = *cols + M*bs;
  for (i=0; i<bn; i++){ /* for each block row */
    M = ai[i+1] - ai[i];
    for (j=0; j<M; j++){
      itmp = aj[ai[i] + j];    /* block column value */
      if (itmp == bn){ 
        aa_i   = aa    + bs2*(ai[i] + j) + bs*bp;
        for (k=0; k<bs; k++) {
          *cols_i++ = i*bs+k; 
          *v_i++    = aa_i[k]; 
        }
        *ncols += bs;
        break;
      }
    }
  }
#endif
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatRestoreRow_SeqSBAIJ"
PetscErrorCode MatRestoreRow_SeqSBAIJ(Mat A,PetscInt row,PetscInt *nz,PetscInt **idx,PetscScalar **v)
{
  PetscErrorCode ierr;
  
  PetscFunctionBegin;
  if (idx) {ierr = PetscFree(*idx);CHKERRQ(ierr);}
  if (v)   {ierr = PetscFree(*v);CHKERRQ(ierr);}
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatGetRowUpperTriangular_SeqSBAIJ"
PetscErrorCode MatGetRowUpperTriangular_SeqSBAIJ(Mat A)
{
  Mat_SeqSBAIJ   *a = (Mat_SeqSBAIJ*)A->data;

  PetscFunctionBegin;
  a->getrow_utriangular = PETSC_TRUE;
  PetscFunctionReturn(0);
}
#undef __FUNCT__  
#define __FUNCT__ "MatRestoreRowUpperTriangular_SeqSBAIJ"
PetscErrorCode MatRestoreRowUpperTriangular_SeqSBAIJ(Mat A)
{
  Mat_SeqSBAIJ   *a = (Mat_SeqSBAIJ*)A->data;

  PetscFunctionBegin;
  a->getrow_utriangular = PETSC_FALSE;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatTranspose_SeqSBAIJ"
PetscErrorCode MatTranspose_SeqSBAIJ(Mat A,Mat *B)
{ 
  PetscErrorCode ierr;
  PetscFunctionBegin;
  ierr = MatDuplicate(A,MAT_COPY_VALUES,B);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatView_SeqSBAIJ_ASCII"
static PetscErrorCode MatView_SeqSBAIJ_ASCII(Mat A,PetscViewer viewer)
{
  Mat_SeqSBAIJ      *a = (Mat_SeqSBAIJ*)A->data;
  PetscErrorCode    ierr;
  PetscInt          i,j,bs = A->rmap.bs,k,l,bs2=a->bs2;
  const char        *name;
  PetscViewerFormat format;
  
  PetscFunctionBegin;
  ierr = PetscObjectGetName((PetscObject)A,&name);CHKERRQ(ierr);
  ierr = PetscViewerGetFormat(viewer,&format);CHKERRQ(ierr);
  if (format == PETSC_VIEWER_ASCII_INFO || format == PETSC_VIEWER_ASCII_INFO_DETAIL) {
    ierr = PetscViewerASCIIPrintf(viewer,"  block size is %D\n",bs);CHKERRQ(ierr);
  } else if (format == PETSC_VIEWER_ASCII_MATLAB) {
    if (A->factor && bs>1){
      ierr = PetscPrintf(PETSC_COMM_SELF,"Warning: matrix is factored with bs>1. MatView() with PETSC_VIEWER_ASCII_MATLAB is not supported and ignored!\n");CHKERRQ(ierr);
      PetscFunctionReturn(0);
    }
    Mat aij;
    ierr = MatConvert(A,MATSEQAIJ,MAT_INITIAL_MATRIX,&aij);CHKERRQ(ierr);
    ierr = MatView(aij,viewer);CHKERRQ(ierr);
    ierr = MatDestroy(aij);CHKERRQ(ierr);
  } else if (format == PETSC_VIEWER_ASCII_COMMON) {
    ierr = PetscViewerASCIIUseTabs(viewer,PETSC_NO);CHKERRQ(ierr);
    for (i=0; i<a->mbs; i++) {
      for (j=0; j<bs; j++) {
        ierr = PetscViewerASCIIPrintf(viewer,"row %D:",i*bs+j);CHKERRQ(ierr);
        for (k=a->i[i]; k<a->i[i+1]; k++) {
          for (l=0; l<bs; l++) {
#if defined(PETSC_USE_COMPLEX)
            if (PetscImaginaryPart(a->a[bs2*k + l*bs + j]) > 0.0 && PetscRealPart(a->a[bs2*k + l*bs + j]) != 0.0) {
              ierr = PetscViewerASCIIPrintf(viewer," (%D, %G + %G i) ",bs*a->j[k]+l,
                                            PetscRealPart(a->a[bs2*k + l*bs + j]),PetscImaginaryPart(a->a[bs2*k + l*bs + j]));CHKERRQ(ierr);
            } else if (PetscImaginaryPart(a->a[bs2*k + l*bs + j]) < 0.0 && PetscRealPart(a->a[bs2*k + l*bs + j]) != 0.0) {
              ierr = PetscViewerASCIIPrintf(viewer," (%D, %G - %G i) ",bs*a->j[k]+l,
                                            PetscRealPart(a->a[bs2*k + l*bs + j]),-PetscImaginaryPart(a->a[bs2*k + l*bs + j]));CHKERRQ(ierr);
            } else if (PetscRealPart(a->a[bs2*k + l*bs + j]) != 0.0) {
              ierr = PetscViewerASCIIPrintf(viewer," (%D, %G) ",bs*a->j[k]+l,PetscRealPart(a->a[bs2*k + l*bs + j]));CHKERRQ(ierr);
            }
#else
            if (a->a[bs2*k + l*bs + j] != 0.0) {
              ierr = PetscViewerASCIIPrintf(viewer," (%D, %G) ",bs*a->j[k]+l,a->a[bs2*k + l*bs + j]);CHKERRQ(ierr);
            }
#endif
          }
        }
        ierr = PetscViewerASCIIPrintf(viewer,"\n");CHKERRQ(ierr);
      }
    } 
    ierr = PetscViewerASCIIUseTabs(viewer,PETSC_YES);CHKERRQ(ierr);
  } else if (format == PETSC_VIEWER_ASCII_FACTOR_INFO) {
     PetscFunctionReturn(0);
  } else {
    if (A->factor && bs>1){
      ierr = PetscPrintf(PETSC_COMM_SELF,"Warning: matrix is factored. MatView_SeqSBAIJ_ASCII() may not display complete or logically correct entries!\n");CHKERRQ(ierr);
    }
    ierr = PetscViewerASCIIUseTabs(viewer,PETSC_NO);CHKERRQ(ierr);
    for (i=0; i<a->mbs; i++) {
      for (j=0; j<bs; j++) {
        ierr = PetscViewerASCIIPrintf(viewer,"row %D:",i*bs+j);CHKERRQ(ierr);
        for (k=a->i[i]; k<a->i[i+1]; k++) {
          for (l=0; l<bs; l++) {
#if defined(PETSC_USE_COMPLEX)
            if (PetscImaginaryPart(a->a[bs2*k + l*bs + j]) > 0.0) {
              ierr = PetscViewerASCIIPrintf(viewer," (%D, %G + %G i) ",bs*a->j[k]+l,
                                            PetscRealPart(a->a[bs2*k + l*bs + j]),PetscImaginaryPart(a->a[bs2*k + l*bs + j]));CHKERRQ(ierr);
            } else if (PetscImaginaryPart(a->a[bs2*k + l*bs + j]) < 0.0) {
              ierr = PetscViewerASCIIPrintf(viewer," (%D, %G - %G i) ",bs*a->j[k]+l,
                                            PetscRealPart(a->a[bs2*k + l*bs + j]),-PetscImaginaryPart(a->a[bs2*k + l*bs + j]));CHKERRQ(ierr);
            } else {
              ierr = PetscViewerASCIIPrintf(viewer," (%D, %G) ",bs*a->j[k]+l,PetscRealPart(a->a[bs2*k + l*bs + j]));CHKERRQ(ierr);
            }
#else
            ierr = PetscViewerASCIIPrintf(viewer," (%D, %G) ",bs*a->j[k]+l,a->a[bs2*k + l*bs + j]);CHKERRQ(ierr);
#endif
          }
        }
        ierr = PetscViewerASCIIPrintf(viewer,"\n");CHKERRQ(ierr);
      }
    } 
    ierr = PetscViewerASCIIUseTabs(viewer,PETSC_YES);CHKERRQ(ierr);
  }
  ierr = PetscViewerFlush(viewer);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatView_SeqSBAIJ_Draw_Zoom"
static PetscErrorCode MatView_SeqSBAIJ_Draw_Zoom(PetscDraw draw,void *Aa)
{
  Mat            A = (Mat) Aa;
  Mat_SeqSBAIJ   *a=(Mat_SeqSBAIJ*)A->data;
  PetscErrorCode ierr;
  PetscInt       row,i,j,k,l,mbs=a->mbs,color,bs=A->rmap.bs,bs2=a->bs2;
  PetscMPIInt    rank;
  PetscReal      xl,yl,xr,yr,x_l,x_r,y_l,y_r;
  MatScalar      *aa;
  MPI_Comm       comm;
  PetscViewer    viewer;
  
  PetscFunctionBegin; 
  /*
    This is nasty. If this is called from an originally parallel matrix
    then all processes call this,but only the first has the matrix so the
    rest should return immediately.
  */
  ierr = PetscObjectGetComm((PetscObject)draw,&comm);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(comm,&rank);CHKERRQ(ierr);
  if (rank) PetscFunctionReturn(0);
  
  ierr = PetscObjectQuery((PetscObject)A,"Zoomviewer",(PetscObject*)&viewer);CHKERRQ(ierr); 
  
  ierr = PetscDrawGetCoordinates(draw,&xl,&yl,&xr,&yr);CHKERRQ(ierr);
  PetscDrawString(draw, .3*(xl+xr), .3*(yl+yr), PETSC_DRAW_BLACK, "symmetric");
  
  /* loop over matrix elements drawing boxes */
  color = PETSC_DRAW_BLUE;
  for (i=0,row=0; i<mbs; i++,row+=bs) {
    for (j=a->i[i]; j<a->i[i+1]; j++) {
      y_l = A->rmap.N - row - 1.0; y_r = y_l + 1.0;
      x_l = a->j[j]*bs; x_r = x_l + 1.0;
      aa = a->a + j*bs2;
      for (k=0; k<bs; k++) {
        for (l=0; l<bs; l++) {
          if (PetscRealPart(*aa++) >=  0.) continue;
          ierr = PetscDrawRectangle(draw,x_l+k,y_l-l,x_r+k,y_r-l,color,color,color,color);CHKERRQ(ierr);
        }
      }
    } 
  }
  color = PETSC_DRAW_CYAN;
  for (i=0,row=0; i<mbs; i++,row+=bs) {
    for (j=a->i[i]; j<a->i[i+1]; j++) {
      y_l = A->rmap.N - row - 1.0; y_r = y_l + 1.0;
      x_l = a->j[j]*bs; x_r = x_l + 1.0;
      aa = a->a + j*bs2;
      for (k=0; k<bs; k++) {
        for (l=0; l<bs; l++) {
          if (PetscRealPart(*aa++) != 0.) continue;
          ierr = PetscDrawRectangle(draw,x_l+k,y_l-l,x_r+k,y_r-l,color,color,color,color);CHKERRQ(ierr);
        }
      }
    } 
  }
  
  color = PETSC_DRAW_RED;
  for (i=0,row=0; i<mbs; i++,row+=bs) {
    for (j=a->i[i]; j<a->i[i+1]; j++) {
      y_l = A->rmap.N - row - 1.0; y_r = y_l + 1.0;
      x_l = a->j[j]*bs; x_r = x_l + 1.0;
      aa = a->a + j*bs2;
      for (k=0; k<bs; k++) {
        for (l=0; l<bs; l++) {
          if (PetscRealPart(*aa++) <= 0.) continue;
          ierr = PetscDrawRectangle(draw,x_l+k,y_l-l,x_r+k,y_r-l,color,color,color,color);CHKERRQ(ierr);
        }
      }
    } 
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatView_SeqSBAIJ_Draw"
static PetscErrorCode MatView_SeqSBAIJ_Draw(Mat A,PetscViewer viewer)
{
  PetscErrorCode ierr;
  PetscReal      xl,yl,xr,yr,w,h;
  PetscDraw      draw;
  PetscTruth     isnull;
  
  PetscFunctionBegin; 
  ierr = PetscViewerDrawGetDraw(viewer,0,&draw);CHKERRQ(ierr);
  ierr = PetscDrawIsNull(draw,&isnull);CHKERRQ(ierr); if (isnull) PetscFunctionReturn(0);
  
  ierr = PetscObjectCompose((PetscObject)A,"Zoomviewer",(PetscObject)viewer);CHKERRQ(ierr);
  xr  = A->rmap.N; yr = A->rmap.N; h = yr/10.0; w = xr/10.0; 
  xr += w;    yr += h;  xl = -w;     yl = -h;
  ierr = PetscDrawSetCoordinates(draw,xl,yl,xr,yr);CHKERRQ(ierr);
  ierr = PetscDrawZoom(draw,MatView_SeqSBAIJ_Draw_Zoom,A);CHKERRQ(ierr);
  ierr = PetscObjectCompose((PetscObject)A,"Zoomviewer",PETSC_NULL);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatView_SeqSBAIJ"
PetscErrorCode MatView_SeqSBAIJ(Mat A,PetscViewer viewer)
{
  PetscErrorCode ierr;
  PetscTruth     iascii,isdraw;
  
  PetscFunctionBegin;
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_ASCII,&iascii);CHKERRQ(ierr);
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_DRAW,&isdraw);CHKERRQ(ierr);
  if (iascii){
    ierr = MatView_SeqSBAIJ_ASCII(A,viewer);CHKERRQ(ierr);
  } else if (isdraw) {
    ierr = MatView_SeqSBAIJ_Draw(A,viewer);CHKERRQ(ierr);
  } else {
    Mat B;
    ierr = MatConvert(A,MATSEQAIJ,MAT_INITIAL_MATRIX,&B);CHKERRQ(ierr);
    ierr = MatView(B,viewer);CHKERRQ(ierr);
    ierr = MatDestroy(B);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "MatGetValues_SeqSBAIJ"
PetscErrorCode MatGetValues_SeqSBAIJ(Mat A,PetscInt m,const PetscInt im[],PetscInt n,const PetscInt in[],PetscScalar v[])
{
  Mat_SeqSBAIJ *a = (Mat_SeqSBAIJ*)A->data;
  PetscInt     *rp,k,low,high,t,row,nrow,i,col,l,*aj = a->j;
  PetscInt     *ai = a->i,*ailen = a->ilen;
  PetscInt     brow,bcol,ridx,cidx,bs=A->rmap.bs,bs2=a->bs2;
  MatScalar    *ap,*aa = a->a;
  
  PetscFunctionBegin;
  for (k=0; k<m; k++) { /* loop over rows */
    row  = im[k]; brow = row/bs;  
    if (row < 0) {v += n; continue;} /* SETERRQ1(PETSC_ERR_ARG_OUTOFRANGE,"Negative row: %D",row); */
    if (row >= A->rmap.N) SETERRQ2(PETSC_ERR_ARG_OUTOFRANGE,"Row too large: row %D max %D",row,A->rmap.N-1);
    rp   = aj + ai[brow] ; ap = aa + bs2*ai[brow] ;
    nrow = ailen[brow]; 
    for (l=0; l<n; l++) { /* loop over columns */
      if (in[l] < 0) {v++; continue;} /* SETERRQ1(PETSC_ERR_ARG_OUTOFRANGE,"Negative column: %D",in[l]); */
      if (in[l] >= A->cmap.n) SETERRQ2(PETSC_ERR_ARG_OUTOFRANGE,"Column too large: col %D max %D",in[l],A->cmap.n-1);
      col  = in[l] ; 
      bcol = col/bs;
      cidx = col%bs; 
      ridx = row%bs;
      high = nrow; 
      low  = 0; /* assume unsorted */
      while (high-low > 5) {
        t = (low+high)/2;
        if (rp[t] > bcol) high = t;
        else             low  = t;
      }
      for (i=low; i<high; i++) {
        if (rp[i] > bcol) break;
        if (rp[i] == bcol) {
          *v++ = ap[bs2*i+bs*cidx+ridx];
          goto finished;
        }
      } 
      *v++ = 0.0;
      finished:;
    }
  }
  PetscFunctionReturn(0);
} 


#undef __FUNCT__  
#define __FUNCT__ "MatSetValuesBlocked_SeqSBAIJ"
PetscErrorCode MatSetValuesBlocked_SeqSBAIJ(Mat A,PetscInt m,const PetscInt im[],PetscInt n,const PetscInt in[],const PetscScalar v[],InsertMode is)
{
  Mat_SeqSBAIJ    *a = (Mat_SeqSBAIJ*)A->data;
  PetscErrorCode  ierr;
  PetscInt        *rp,k,low,high,t,ii,jj,row,nrow,i,col,l,rmax,N,lastcol = -1;
  PetscInt        *imax=a->imax,*ai=a->i,*ailen=a->ilen;
  PetscInt        *aj=a->j,nonew=a->nonew,bs2=a->bs2,bs=A->rmap.bs,stepval;
  PetscTruth      roworiented=a->roworiented; 
  const MatScalar *value = v;
  MatScalar       *ap,*aa = a->a,*bap;
  
  PetscFunctionBegin;
  if (roworiented) { 
    stepval = (n-1)*bs;
  } else {
    stepval = (m-1)*bs;
  }
  for (k=0; k<m; k++) { /* loop over added rows */
    row  = im[k]; 
    if (row < 0) continue;
#if defined(PETSC_USE_DEBUG)  
    if (row >= a->mbs) SETERRQ2(PETSC_ERR_ARG_OUTOFRANGE,"Row too large: row %D max %D",row,a->mbs-1);
#endif
    rp   = aj + ai[row]; 
    ap   = aa + bs2*ai[row];
    rmax = imax[row]; 
    nrow = ailen[row]; 
    low  = 0;
    high = nrow;
    for (l=0; l<n; l++) { /* loop over added columns */
      if (in[l] < 0) continue;
      col = in[l]; 
#if defined(PETSC_USE_DEBUG)  
      if (col >= a->nbs) SETERRQ2(PETSC_ERR_ARG_OUTOFRANGE,"Column too large: col %D max %D",col,a->nbs-1);
#endif
      if (col < row) continue; /* ignore lower triangular block */
      if (roworiented) { 
        value = v + k*(stepval+bs)*bs + l*bs;
      } else {
        value = v + l*(stepval+bs)*bs + k*bs;
      }
      if (col <= lastcol) low = 0; else high = nrow;
      lastcol = col;
      while (high-low > 7) {
        t = (low+high)/2;
        if (rp[t] > col) high = t;
        else             low  = t;
      }
      for (i=low; i<high; i++) {
        if (rp[i] > col) break;
        if (rp[i] == col) {
          bap  = ap +  bs2*i;
          if (roworiented) { 
            if (is == ADD_VALUES) {
              for (ii=0; ii<bs; ii++,value+=stepval) {
                for (jj=ii; jj<bs2; jj+=bs) {
                  bap[jj] += *value++; 
                }
              }
            } else {
              for (ii=0; ii<bs; ii++,value+=stepval) {
                for (jj=ii; jj<bs2; jj+=bs) {
                  bap[jj] = *value++; 
                }
               }
            }
          } else {
            if (is == ADD_VALUES) {
              for (ii=0; ii<bs; ii++,value+=stepval) {
                for (jj=0; jj<bs; jj++) {
                  *bap++ += *value++; 
                }
              }
            } else {
              for (ii=0; ii<bs; ii++,value+=stepval) {
                for (jj=0; jj<bs; jj++) {
                  *bap++  = *value++; 
                }
              }
            }
          }
          goto noinsert2;
        }
      } 
      if (nonew == 1) goto noinsert2;
      if (nonew == -1) SETERRQ2(PETSC_ERR_ARG_OUTOFRANGE,"Inserting a new nonzero (%D, %D) in the matrix", row, col);
      MatSeqXAIJReallocateAIJ(A,a->mbs,bs2,nrow,row,col,rmax,aa,ai,aj,rp,ap,imax,nonew,MatScalar);
      N = nrow++ - 1; high++;
      /* shift up all the later entries in this row */
      for (ii=N; ii>=i; ii--) {
        rp[ii+1] = rp[ii];
        ierr = PetscMemcpy(ap+bs2*(ii+1),ap+bs2*(ii),bs2*sizeof(MatScalar));CHKERRQ(ierr);
      }
      if (N >= i) {
        ierr = PetscMemzero(ap+bs2*i,bs2*sizeof(MatScalar));CHKERRQ(ierr);
      }
      rp[i] = col; 
      bap   = ap +  bs2*i;
      if (roworiented) { 
        for (ii=0; ii<bs; ii++,value+=stepval) {
          for (jj=ii; jj<bs2; jj+=bs) {
            bap[jj] = *value++; 
          }
        }
      } else {
        for (ii=0; ii<bs; ii++,value+=stepval) {
          for (jj=0; jj<bs; jj++) {
            *bap++  = *value++; 
          }
        }
       }
    noinsert2:;
      low = i;
    }
    ailen[row] = nrow;
  }
   PetscFunctionReturn(0);
} 

#undef __FUNCT__  
#define __FUNCT__ "MatAssemblyEnd_SeqSBAIJ"
PetscErrorCode MatAssemblyEnd_SeqSBAIJ(Mat A,MatAssemblyType mode)
{
  Mat_SeqSBAIJ   *a = (Mat_SeqSBAIJ*)A->data;
  PetscErrorCode ierr;
  PetscInt       fshift = 0,i,j,*ai = a->i,*aj = a->j,*imax = a->imax;
  PetscInt       m = A->rmap.N,*ip,N,*ailen = a->ilen;
  PetscInt       mbs = a->mbs,bs2 = a->bs2,rmax = 0;
  MatScalar      *aa = a->a,*ap;
  
  PetscFunctionBegin;
  if (mode == MAT_FLUSH_ASSEMBLY) PetscFunctionReturn(0);
  
  if (m) rmax = ailen[0];
  for (i=1; i<mbs; i++) {
    /* move each row back by the amount of empty slots (fshift) before it*/
    fshift += imax[i-1] - ailen[i-1];
     rmax   = PetscMax(rmax,ailen[i]);
     if (fshift) {
       ip = aj + ai[i]; ap = aa + bs2*ai[i];
       N = ailen[i];
       for (j=0; j<N; j++) {
         ip[j-fshift] = ip[j];
         ierr = PetscMemcpy(ap+(j-fshift)*bs2,ap+j*bs2,bs2*sizeof(MatScalar));CHKERRQ(ierr);
       }
     } 
     ai[i] = ai[i-1] + ailen[i-1];
  }
  if (mbs) {
    fshift += imax[mbs-1] - ailen[mbs-1];
     ai[mbs] = ai[mbs-1] + ailen[mbs-1];
  }
  /* reset ilen and imax for each row */
  for (i=0; i<mbs; i++) {
    ailen[i] = imax[i] = ai[i+1] - ai[i];
  }
  a->nz = ai[mbs]; 
  
  /* diagonals may have moved, reset it */
  if (a->diag) {
    ierr = PetscMemcpy(a->diag,ai,(mbs+1)*sizeof(PetscInt));CHKERRQ(ierr);
  } 
  ierr = PetscInfo5(A,"Matrix size: %D X %D, block size %D; storage space: %D unneeded, %D used\n",m,A->rmap.N,A->rmap.bs,fshift*bs2,a->nz*bs2);CHKERRQ(ierr);
  ierr = PetscInfo1(A,"Number of mallocs during MatSetValues is %D\n",a->reallocs);CHKERRQ(ierr);
  ierr = PetscInfo1(A,"Most nonzeros blocks in any row is %D\n",rmax);CHKERRQ(ierr);
  a->reallocs          = 0;
  A->info.nz_unneeded  = (PetscReal)fshift*bs2;
  PetscFunctionReturn(0);
}

/* 
   This function returns an array of flags which indicate the locations of contiguous
   blocks that should be zeroed. for eg: if bs = 3  and is = [0,1,2,3,5,6,7,8,9]
   then the resulting sizes = [3,1,1,3,1] correspondig to sets [(0,1,2),(3),(5),(6,7,8),(9)]
   Assume: sizes should be long enough to hold all the values.
*/
#undef __FUNCT__  
#define __FUNCT__ "MatZeroRows_SeqSBAIJ_Check_Blocks"
PetscErrorCode MatZeroRows_SeqSBAIJ_Check_Blocks(PetscInt idx[],PetscInt n,PetscInt bs,PetscInt sizes[], PetscInt *bs_max)
{
  PetscInt   i,j,k,row;
  PetscTruth flg;
  
  PetscFunctionBegin;
   for (i=0,j=0; i<n; j++) {
     row = idx[i];
     if (row%bs!=0) { /* Not the begining of a block */
       sizes[j] = 1;
       i++; 
     } else if (i+bs > n) { /* Beginning of a block, but complete block doesn't exist (at idx end) */
       sizes[j] = 1;         /* Also makes sure atleast 'bs' values exist for next else */
       i++; 
     } else { /* Begining of the block, so check if the complete block exists */
       flg = PETSC_TRUE;
       for (k=1; k<bs; k++) {
         if (row+k != idx[i+k]) { /* break in the block */
           flg = PETSC_FALSE;
           break;
         }
       }
       if (flg) { /* No break in the bs */
         sizes[j] = bs;
         i+= bs;
       } else {
         sizes[j] = 1;
         i++;
       }
     }
   }
   *bs_max = j;
   PetscFunctionReturn(0);
}


/* Only add/insert a(i,j) with i<=j (blocks). 
   Any a(i,j) with i>j input by user is ingored. 
*/

#undef __FUNCT__  
#define __FUNCT__ "MatSetValues_SeqSBAIJ"
PetscErrorCode MatSetValues_SeqSBAIJ(Mat A,PetscInt m,const PetscInt im[],PetscInt n,const PetscInt in[],const PetscScalar v[],InsertMode is)
{
  Mat_SeqSBAIJ   *a = (Mat_SeqSBAIJ*)A->data;
  PetscErrorCode ierr;
  PetscInt       *rp,k,low,high,t,ii,row,nrow,i,col,l,rmax,N,lastcol = -1;
  PetscInt       *imax=a->imax,*ai=a->i,*ailen=a->ilen,roworiented=a->roworiented;
  PetscInt       *aj=a->j,nonew=a->nonew,bs=A->rmap.bs,brow,bcol;
  PetscInt       ridx,cidx,bs2=a->bs2;
  MatScalar      *ap,value,*aa=a->a,*bap;
  
  PetscFunctionBegin;
  for (k=0; k<m; k++) { /* loop over added rows */
    row  = im[k];       /* row number */ 
    brow = row/bs;      /* block row number */ 
    if (row < 0) continue;
#if defined(PETSC_USE_DEBUG)  
    if (row >= A->rmap.N) SETERRQ2(PETSC_ERR_ARG_OUTOFRANGE,"Row too large: row %D max %D",row,A->rmap.N-1);
#endif
    rp   = aj + ai[brow]; /*ptr to beginning of column value of the row block*/
    ap   = aa + bs2*ai[brow]; /*ptr to beginning of element value of the row block*/
    rmax = imax[brow];  /* maximum space allocated for this row */
    nrow = ailen[brow]; /* actual length of this row */
    low  = 0;
    
    for (l=0; l<n; l++) { /* loop over added columns */
      if (in[l] < 0) continue;
#if defined(PETSC_USE_DEBUG)  
      if (in[l] >= A->rmap.N) SETERRQ2(PETSC_ERR_ARG_OUTOFRANGE,"Column too large: col %D max %D",in[l],A->rmap.N-1);
#endif
      col = in[l]; 
      bcol = col/bs;              /* block col number */
      
      if (brow > bcol) {
        if (a->ignore_ltriangular){
          continue; /* ignore lower triangular values */
        } else {
          SETERRQ(PETSC_ERR_USER,"Lower triangular value cannot be set for sbaij format. Ignoring these values, run with -mat_ignore_lower_triangular or call MatSetOption(mat,MAT_IGNORE_LOWER_TRIANGULAR)");
        }
      }
      
      ridx = row % bs; cidx = col % bs; /*row and col index inside the block */
      if ((brow==bcol && ridx<=cidx) || (brow<bcol)){    
        /* element value a(k,l) */
        if (roworiented) {
          value = v[l + k*n];             
        } else {
          value = v[k + l*m];
        }
        
        /* move pointer bap to a(k,l) quickly and add/insert value */
        if (col <= lastcol) low = 0; high = nrow;
        lastcol = col;
        while (high-low > 7) {
          t = (low+high)/2;
          if (rp[t] > bcol) high = t;
          else              low  = t;
        }
        for (i=low; i<high; i++) {
          if (rp[i] > bcol) break;
          if (rp[i] == bcol) {
            bap  = ap +  bs2*i + bs*cidx + ridx;
            if (is == ADD_VALUES) *bap += value;  
            else                  *bap  = value; 
            /* for diag block, add/insert its symmetric element a(cidx,ridx) */
            if (brow == bcol && ridx < cidx){
              bap  = ap +  bs2*i + bs*ridx + cidx;
              if (is == ADD_VALUES) *bap += value;  
              else                  *bap  = value; 
            }
            goto noinsert1;
          }
        }      
        
        if (nonew == 1) goto noinsert1;
        if (nonew == -1) SETERRQ2(PETSC_ERR_ARG_OUTOFRANGE,"Inserting a new nonzero (%D, %D) in the matrix", row, col);
        MatSeqXAIJReallocateAIJ(A,a->mbs,bs2,nrow,brow,bcol,rmax,aa,ai,aj,rp,ap,imax,nonew,MatScalar);
        
        N = nrow++ - 1; high++;
        /* shift up all the later entries in this row */
        for (ii=N; ii>=i; ii--) {
          rp[ii+1] = rp[ii];
          ierr     = PetscMemcpy(ap+bs2*(ii+1),ap+bs2*(ii),bs2*sizeof(MatScalar));CHKERRQ(ierr);
        }
        if (N>=i) {
          ierr = PetscMemzero(ap+bs2*i,bs2*sizeof(MatScalar));CHKERRQ(ierr);
        }
        rp[i]                      = bcol; 
        ap[bs2*i + bs*cidx + ridx] = value; 
      noinsert1:;
        low = i;      
      }
    }   /* end of loop over added columns */
    ailen[brow] = nrow; 
  }   /* end of loop over added rows */
  PetscFunctionReturn(0);
} 

#undef __FUNCT__  
#define __FUNCT__ "MatICCFactor_SeqSBAIJ"
PetscErrorCode MatICCFactor_SeqSBAIJ(Mat inA,IS row,MatFactorInfo *info)
{
  Mat_SeqSBAIJ   *a = (Mat_SeqSBAIJ*)inA->data;
  Mat            outA;
  PetscErrorCode ierr;
  PetscTruth     row_identity;
  
  PetscFunctionBegin;
  if (info->levels != 0) SETERRQ(PETSC_ERR_SUP,"Only levels=0 is supported for in-place icc");
  ierr = ISIdentity(row,&row_identity);CHKERRQ(ierr);
  if (!row_identity) SETERRQ(PETSC_ERR_SUP,"Matrix reordering is not supported");
  if (inA->rmap.bs != 1) SETERRQ1(PETSC_ERR_SUP,"Matrix block size %D is not supported",inA->rmap.bs); /* Need to replace MatCholeskyFactorSymbolic_SeqSBAIJ_MSR()! */

  outA        = inA; 
  inA->factor = FACTOR_CHOLESKY;
  
  ierr = MatMarkDiagonal_SeqSBAIJ(inA);CHKERRQ(ierr);
  /*
    Blocksize < 8 have a special faster factorization/solver 
    for ICC(0) factorization with natural ordering
  */   
  switch (inA->rmap.bs){ /* Note: row_identity = PETSC_TRUE! */
  case 1:
    inA->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqSBAIJ_1_NaturalOrdering;
    inA->ops->solve            = MatSolve_SeqSBAIJ_1_NaturalOrdering;
    inA->ops->solvetranspose   = MatSolve_SeqSBAIJ_1_NaturalOrdering;
    inA->ops->solves           = MatSolves_SeqSBAIJ_1;
    inA->ops->forwardsolve     = MatForwardSolve_SeqSBAIJ_1_NaturalOrdering;
    inA->ops->backwardsolve    = MatBackwardSolve_SeqSBAIJ_1_NaturalOrdering;
    ierr = PetscInfo(inA,"Using special in-place natural ordering solvetrans BS=1\n");CHKERRQ(ierr);
    break;           
  case 2:
    inA->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqSBAIJ_2_NaturalOrdering;
    inA->ops->solve           = MatSolve_SeqSBAIJ_2_NaturalOrdering;
    inA->ops->solvetranspose  = MatSolve_SeqSBAIJ_2_NaturalOrdering;
    inA->ops->forwardsolve     = MatForwardSolve_SeqSBAIJ_2_NaturalOrdering;
    inA->ops->backwardsolve    = MatBackwardSolve_SeqSBAIJ_2_NaturalOrdering;
    ierr = PetscInfo(inA,"Using special in-place natural ordering factor and solve BS=2\n");CHKERRQ(ierr);
    break; 
  case 3:
     inA->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqSBAIJ_3_NaturalOrdering;
     inA->ops->solve           = MatSolve_SeqSBAIJ_3_NaturalOrdering;
     inA->ops->solvetranspose  = MatSolve_SeqSBAIJ_3_NaturalOrdering;
     inA->ops->forwardsolve    = MatForwardSolve_SeqSBAIJ_3_NaturalOrdering;
     inA->ops->backwardsolve   = MatBackwardSolve_SeqSBAIJ_3_NaturalOrdering;
     ierr = PetscInfo(inA,"Using special in-place natural ordering factor and solve BS=3\n");CHKERRQ(ierr);
     break; 
  case 4:
    inA->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqSBAIJ_4_NaturalOrdering;
    inA->ops->solve           = MatSolve_SeqSBAIJ_4_NaturalOrdering;
    inA->ops->solvetranspose  = MatSolve_SeqSBAIJ_4_NaturalOrdering;
    inA->ops->forwardsolve    = MatForwardSolve_SeqSBAIJ_4_NaturalOrdering;
    inA->ops->backwardsolve   = MatBackwardSolve_SeqSBAIJ_4_NaturalOrdering;
    ierr = PetscInfo(inA,"Using special in-place natural ordering factor and solve BS=4\n");CHKERRQ(ierr);
    break;
  case 5:
    inA->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqSBAIJ_5_NaturalOrdering;
    inA->ops->solve           = MatSolve_SeqSBAIJ_5_NaturalOrdering;
    inA->ops->solvetranspose  = MatSolve_SeqSBAIJ_5_NaturalOrdering;
    inA->ops->forwardsolve    = MatForwardSolve_SeqSBAIJ_5_NaturalOrdering;
    inA->ops->backwardsolve   = MatBackwardSolve_SeqSBAIJ_5_NaturalOrdering;
    ierr = PetscInfo(inA,"Using special in-place natural ordering factor and solve BS=5\n");CHKERRQ(ierr);
    break;
  case 6: 
    inA->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqSBAIJ_6_NaturalOrdering;
    inA->ops->solve           = MatSolve_SeqSBAIJ_6_NaturalOrdering;
    inA->ops->solvetranspose  = MatSolve_SeqSBAIJ_6_NaturalOrdering;
    inA->ops->forwardsolve    = MatForwardSolve_SeqSBAIJ_6_NaturalOrdering;
    inA->ops->backwardsolve   = MatBackwardSolve_SeqSBAIJ_6_NaturalOrdering;
    ierr = PetscInfo(inA,"Using special in-place natural ordering factor and solve BS=6\n");CHKERRQ(ierr);
    break;  
  case 7:
    inA->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqSBAIJ_7_NaturalOrdering;
    inA->ops->solvetranspose  = MatSolve_SeqSBAIJ_7_NaturalOrdering;
    inA->ops->solve           = MatSolve_SeqSBAIJ_7_NaturalOrdering;
    inA->ops->forwardsolve    = MatForwardSolve_SeqSBAIJ_7_NaturalOrdering;
    inA->ops->backwardsolve   = MatBackwardSolve_SeqSBAIJ_7_NaturalOrdering;
    ierr = PetscInfo(inA,"Using special in-place natural ordering factor and solve BS=7\n");CHKERRQ(ierr);
    break; 
  default:
    inA->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqSBAIJ_N_NaturalOrdering;
    inA->ops->solvetranspose  = MatSolve_SeqSBAIJ_N_NaturalOrdering;
    inA->ops->solve           = MatSolve_SeqSBAIJ_N_NaturalOrdering;
    inA->ops->forwardsolve    = MatForwardSolve_SeqSBAIJ_N_NaturalOrdering;
    inA->ops->backwardsolve   = MatBackwardSolve_SeqSBAIJ_N_NaturalOrdering;
    break;
  } 
           
  ierr   = PetscObjectReference((PetscObject)row);CHKERRQ(ierr);
  if (a->row) { ierr = ISDestroy(a->row);CHKERRQ(ierr); }
  a->row = row;
  ierr   = PetscObjectReference((PetscObject)row);CHKERRQ(ierr);
  if (a->col) { ierr = ISDestroy(a->col);CHKERRQ(ierr); }
  a->col = row;
    
  /* Create the invert permutation so that it can be used in MatCholeskyFactorNumeric() */
  if (a->icol) {ierr = ISInvertPermutation(row,PETSC_DECIDE, &a->icol);CHKERRQ(ierr);}
  ierr = PetscLogObjectParent(inA,a->icol);CHKERRQ(ierr);
    
  if (!a->solve_work) {
    ierr = PetscMalloc((inA->rmap.N+inA->rmap.bs)*sizeof(PetscScalar),&a->solve_work);CHKERRQ(ierr);
    ierr = PetscLogObjectMemory(inA,(inA->rmap.N+inA->rmap.bs)*sizeof(PetscScalar));CHKERRQ(ierr);
  }
   
  ierr = MatCholeskyFactorNumeric(inA,info,&outA);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "MatSeqSBAIJSetColumnIndices_SeqSBAIJ"
PetscErrorCode PETSCMAT_DLLEXPORT MatSeqSBAIJSetColumnIndices_SeqSBAIJ(Mat mat,PetscInt *indices)
{
  Mat_SeqSBAIJ *baij = (Mat_SeqSBAIJ *)mat->data;
  PetscInt     i,nz,n;
  
  PetscFunctionBegin;
  nz = baij->maxnz;
  n  = mat->cmap.n;
  for (i=0; i<nz; i++) {
    baij->j[i] = indices[i];
  }
   baij->nz = nz;
   for (i=0; i<n; i++) {
     baij->ilen[i] = baij->imax[i];
   }
   PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "MatSeqSBAIJSetColumnIndices"
/*@
  MatSeqSBAIJSetColumnIndices - Set the column indices for all the rows
  in the matrix.
  
  Input Parameters:
  +  mat     - the SeqSBAIJ matrix
  -  indices - the column indices
  
  Level: advanced
  
  Notes:
  This can be called if you have precomputed the nonzero structure of the 
  matrix and want to provide it to the matrix object to improve the performance
  of the MatSetValues() operation.
  
  You MUST have set the correct numbers of nonzeros per row in the call to 
  MatCreateSeqSBAIJ(), and the columns indices MUST be sorted.
  
  MUST be called before any calls to MatSetValues()
  
  .seealso: MatCreateSeqSBAIJ
@*/ 
PetscErrorCode PETSCMAT_DLLEXPORT MatSeqSBAIJSetColumnIndices(Mat mat,PetscInt *indices)
{
  PetscErrorCode ierr,(*f)(Mat,PetscInt *);
  
  PetscFunctionBegin;
  PetscValidHeaderSpecific(mat,MAT_COOKIE,1);
  PetscValidPointer(indices,2);
  ierr = PetscObjectQueryFunction((PetscObject)mat,"MatSeqSBAIJSetColumnIndices_C",(void (**)(void))&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(mat,indices);CHKERRQ(ierr);
  } else {
    SETERRQ(PETSC_ERR_SUP,"Wrong type of matrix to set column indices");
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatCopy_SeqSBAIJ"
PetscErrorCode MatCopy_SeqSBAIJ(Mat A,Mat B,MatStructure str)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  /* If the two matrices have the same copy implementation, use fast copy. */
  if (str == SAME_NONZERO_PATTERN && (A->ops->copy == B->ops->copy)) {
    Mat_SeqSBAIJ *a = (Mat_SeqSBAIJ*)A->data; 
    Mat_SeqSBAIJ *b = (Mat_SeqSBAIJ*)B->data; 

    if (a->i[A->rmap.N] != b->i[B->rmap.N]) {
      SETERRQ(PETSC_ERR_ARG_INCOMP,"Number of nonzeros in two matrices are different");
    }
    ierr = PetscMemcpy(b->a,a->a,(a->i[A->rmap.N])*sizeof(PetscScalar));CHKERRQ(ierr);
  } else {
    ierr = MatGetRowUpperTriangular(A);CHKERRQ(ierr); 
    ierr = MatCopy_Basic(A,B,str);CHKERRQ(ierr);
    ierr = MatRestoreRowUpperTriangular(A);CHKERRQ(ierr); 
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSetUpPreallocation_SeqSBAIJ"
PetscErrorCode MatSetUpPreallocation_SeqSBAIJ(Mat A)
{
  PetscErrorCode ierr;
  
  PetscFunctionBegin;
  ierr =  MatSeqSBAIJSetPreallocation_SeqSBAIJ(A,PetscMax(A->rmap.bs,1),PETSC_DEFAULT,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatGetArray_SeqSBAIJ"
PetscErrorCode MatGetArray_SeqSBAIJ(Mat A,PetscScalar *array[])
{
  Mat_SeqSBAIJ *a = (Mat_SeqSBAIJ*)A->data; 
  PetscFunctionBegin;
  *array = a->a;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatRestoreArray_SeqSBAIJ"
PetscErrorCode MatRestoreArray_SeqSBAIJ(Mat A,PetscScalar *array[])
{
  PetscFunctionBegin;
  PetscFunctionReturn(0);
 }

#include "petscblaslapack.h"
#undef __FUNCT__  
#define __FUNCT__ "MatAXPY_SeqSBAIJ"
PetscErrorCode MatAXPY_SeqSBAIJ(Mat Y,PetscScalar a,Mat X,MatStructure str)
{
  Mat_SeqSBAIJ   *x=(Mat_SeqSBAIJ *)X->data, *y=(Mat_SeqSBAIJ *)Y->data;
  PetscErrorCode ierr;
  PetscInt       i,bs=Y->rmap.bs,bs2,j;
  PetscBLASInt   bnz = (PetscBLASInt)x->nz,one = 1;
  
  PetscFunctionBegin;
  if (str == SAME_NONZERO_PATTERN) {
    PetscScalar alpha = a;
    BLASaxpy_(&bnz,&alpha,x->a,&one,y->a,&one);
  } else if (str == SUBSET_NONZERO_PATTERN) { /* nonzeros of X is a subset of Y's */
    if (y->xtoy && y->XtoY != X) {
      ierr = PetscFree(y->xtoy);CHKERRQ(ierr);
      ierr = MatDestroy(y->XtoY);CHKERRQ(ierr);
    }
    if (!y->xtoy) { /* get xtoy */
      ierr = MatAXPYGetxtoy_Private(x->mbs,x->i,x->j,PETSC_NULL, y->i,y->j,PETSC_NULL, &y->xtoy);CHKERRQ(ierr);
      y->XtoY = X;
    }
    bs2 = bs*bs;
    for (i=0; i<x->nz; i++) {
      j = 0;
      while (j < bs2){
        y->a[bs2*y->xtoy[i]+j] += a*(x->a[bs2*i+j]); 
        j++; 
      }
    }
    ierr = PetscInfo3(0,"ratio of nnz_s(X)/nnz_s(Y): %D/%D = %G\n",bs2*x->nz,bs2*y->nz,(PetscReal)(bs2*x->nz)/(bs2*y->nz));CHKERRQ(ierr);
  } else {
    ierr = MatGetRowUpperTriangular(X);CHKERRQ(ierr); 
    ierr = MatAXPY_Basic(Y,a,X,str);CHKERRQ(ierr);
    ierr = MatRestoreRowUpperTriangular(X);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatIsSymmetric_SeqSBAIJ"
PetscErrorCode MatIsSymmetric_SeqSBAIJ(Mat A,PetscReal tol,PetscTruth *flg)
{
  PetscFunctionBegin;
  *flg = PETSC_TRUE;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatIsStructurallySymmetric_SeqSBAIJ"
PetscErrorCode MatIsStructurallySymmetric_SeqSBAIJ(Mat A,PetscTruth *flg)
{
   PetscFunctionBegin;
   *flg = PETSC_TRUE;
   PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatIsHermitian_SeqSBAIJ"
PetscErrorCode MatIsHermitian_SeqSBAIJ(Mat A,PetscReal tol,PetscTruth *flg)
 {
   PetscFunctionBegin;
   *flg = PETSC_FALSE;
   PetscFunctionReturn(0);
 }

#undef __FUNCT__  
#define __FUNCT__ "MatRealPart_SeqSBAIJ"
PetscErrorCode MatRealPart_SeqSBAIJ(Mat A)
{
  Mat_SeqSBAIJ   *a = (Mat_SeqSBAIJ*)A->data; 
  PetscInt       i,nz = a->bs2*a->i[a->mbs];
  PetscScalar    *aa = a->a;

  PetscFunctionBegin;  
  for (i=0; i<nz; i++) aa[i] = PetscRealPart(aa[i]);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatImaginaryPart_SeqSBAIJ"
PetscErrorCode MatImaginaryPart_SeqSBAIJ(Mat A)
{
  Mat_SeqSBAIJ   *a = (Mat_SeqSBAIJ*)A->data; 
  PetscInt       i,nz = a->bs2*a->i[a->mbs];
  PetscScalar    *aa = a->a;

  PetscFunctionBegin;  
  for (i=0; i<nz; i++) aa[i] = PetscImaginaryPart(aa[i]);
  PetscFunctionReturn(0);
}

/* -------------------------------------------------------------------*/
static struct _MatOps MatOps_Values = {MatSetValues_SeqSBAIJ,
       MatGetRow_SeqSBAIJ,
       MatRestoreRow_SeqSBAIJ,
       MatMult_SeqSBAIJ_N,
/* 4*/ MatMultAdd_SeqSBAIJ_N,
       MatMult_SeqSBAIJ_N,       /* transpose versions are same as non-transpose versions */
       MatMultAdd_SeqSBAIJ_N,
       MatSolve_SeqSBAIJ_N,
       0,
       0,
/*10*/ 0,
       0,
       MatCholeskyFactor_SeqSBAIJ,
       MatRelax_SeqSBAIJ,
       MatTranspose_SeqSBAIJ,
/*15*/ MatGetInfo_SeqSBAIJ,
       MatEqual_SeqSBAIJ,
       MatGetDiagonal_SeqSBAIJ,
       MatDiagonalScale_SeqSBAIJ,
       MatNorm_SeqSBAIJ,
/*20*/ 0,
       MatAssemblyEnd_SeqSBAIJ,
       0,
       MatSetOption_SeqSBAIJ,
       MatZeroEntries_SeqSBAIJ,
/*25*/ 0,
       0,
       0,
       MatCholeskyFactorSymbolic_SeqSBAIJ,
       MatCholeskyFactorNumeric_SeqSBAIJ_N,
/*30*/ MatSetUpPreallocation_SeqSBAIJ,
       0,
       MatICCFactorSymbolic_SeqSBAIJ,
       MatGetArray_SeqSBAIJ,
       MatRestoreArray_SeqSBAIJ,
/*35*/ MatDuplicate_SeqSBAIJ,
       MatForwardSolve_SeqSBAIJ_N,
       MatBackwardSolve_SeqSBAIJ_N,
       0,
       MatICCFactor_SeqSBAIJ,
/*40*/ MatAXPY_SeqSBAIJ,
       MatGetSubMatrices_SeqSBAIJ,
       MatIncreaseOverlap_SeqSBAIJ,
       MatGetValues_SeqSBAIJ,
       MatCopy_SeqSBAIJ,
/*45*/ 0,
       MatScale_SeqSBAIJ,
       0,
       0,
       0,
/*50*/ 0,
       MatGetRowIJ_SeqSBAIJ,
       MatRestoreRowIJ_SeqSBAIJ,
       0,
       0,
/*55*/ 0,
       0,
       0,
       0,
       MatSetValuesBlocked_SeqSBAIJ,
/*60*/ MatGetSubMatrix_SeqSBAIJ,
       0,
       0,
       0,
       0,
/*65*/ 0,
       0,
       0,
       0,
       0,    
/*70*/ MatGetRowMaxAbs_SeqSBAIJ,
       0,
       0,
       0,
       0,
/*75*/ 0,
       0,
       0,
       0,
       0,
/*80*/ 0,
       0,
       0,
#if !defined(PETSC_USE_COMPLEX)
       MatGetInertia_SeqSBAIJ,
#else
       0,
#endif
       MatLoad_SeqSBAIJ,
/*85*/ MatIsSymmetric_SeqSBAIJ,
       MatIsHermitian_SeqSBAIJ,
       MatIsStructurallySymmetric_SeqSBAIJ,
       0,
       0,
/*90*/ 0,
       0,
       0,
       0,
       0,
/*95*/ 0,
       0,
       0,
       0,
       0,
/*100*/0,
       0,
       0,
       0,
       0,
/*105*/0,
       MatRealPart_SeqSBAIJ,
       MatImaginaryPart_SeqSBAIJ,
       MatGetRowUpperTriangular_SeqSBAIJ,
       MatRestoreRowUpperTriangular_SeqSBAIJ
};

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "MatStoreValues_SeqSBAIJ"
PetscErrorCode PETSCMAT_DLLEXPORT MatStoreValues_SeqSBAIJ(Mat mat)
{
  Mat_SeqSBAIJ   *aij = (Mat_SeqSBAIJ *)mat->data;
  PetscInt       nz = aij->i[mat->rmap.N]*mat->rmap.bs*aij->bs2;
  PetscErrorCode ierr;
  
  PetscFunctionBegin;
  if (aij->nonew != 1) {
    SETERRQ(PETSC_ERR_ORDER,"Must call MatSetOption(A,MAT_NO_NEW_NONZERO_LOCATIONS);first");
  }
  
  /* allocate space for values if not already there */
  if (!aij->saved_values) {
    ierr = PetscMalloc((nz+1)*sizeof(PetscScalar),&aij->saved_values);CHKERRQ(ierr);
  }
  
  /* copy values over */
  ierr = PetscMemcpy(aij->saved_values,aij->a,nz*sizeof(PetscScalar));CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "MatRetrieveValues_SeqSBAIJ"
PetscErrorCode PETSCMAT_DLLEXPORT MatRetrieveValues_SeqSBAIJ(Mat mat)
{
  Mat_SeqSBAIJ   *aij = (Mat_SeqSBAIJ *)mat->data;
  PetscErrorCode ierr;
  PetscInt       nz = aij->i[mat->rmap.N]*mat->rmap.bs*aij->bs2;
  
  PetscFunctionBegin;
  if (aij->nonew != 1) {
    SETERRQ(PETSC_ERR_ORDER,"Must call MatSetOption(A,MAT_NO_NEW_NONZERO_LOCATIONS);first");
  }
  if (!aij->saved_values) {
    SETERRQ(PETSC_ERR_ORDER,"Must call MatStoreValues(A);first");
  }
  
  /* copy values over */
  ierr = PetscMemcpy(aij->a,aij->saved_values,nz*sizeof(PetscScalar));CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END

EXTERN_C_BEGIN
#undef __FUNCT__
#define __FUNCT__ "MatSeqSBAIJSetPreallocation_SeqSBAIJ"
PetscErrorCode PETSCMAT_DLLEXPORT MatSeqSBAIJSetPreallocation_SeqSBAIJ(Mat B,PetscInt bs,PetscInt nz,PetscInt *nnz)
{
  Mat_SeqSBAIJ   *b = (Mat_SeqSBAIJ*)B->data;
  PetscErrorCode ierr;
  PetscInt       i,mbs,bs2;
  PetscTruth     skipallocation = PETSC_FALSE,flg;
  
  PetscFunctionBegin;
  B->preallocated = PETSC_TRUE;
  ierr = PetscOptionsGetInt(B->prefix,"-mat_block_size",&bs,PETSC_NULL);CHKERRQ(ierr);
  B->rmap.bs = B->cmap.bs = bs;
  ierr = PetscMapSetUp(&B->rmap);CHKERRQ(ierr);
  ierr = PetscMapSetUp(&B->cmap);CHKERRQ(ierr);

  mbs  = B->rmap.N/bs;
  bs2  = bs*bs;
  
  if (mbs*bs != B->rmap.N) {
    SETERRQ(PETSC_ERR_ARG_SIZ,"Number rows, cols must be divisible by blocksize");
  }
  
  if (nz == MAT_SKIP_ALLOCATION) {
    skipallocation = PETSC_TRUE;
    nz             = 0;
  }

  if (nz == PETSC_DEFAULT || nz == PETSC_DECIDE) nz = 3;
  if (nz < 0) SETERRQ1(PETSC_ERR_ARG_OUTOFRANGE,"nz cannot be less than 0: value %D",nz);
  if (nnz) {
    for (i=0; i<mbs; i++) {
      if (nnz[i] < 0) SETERRQ2(PETSC_ERR_ARG_OUTOFRANGE,"nnz cannot be less than 0: local row %D value %D",i,nnz[i]);
      if (nnz[i] > mbs) SETERRQ3(PETSC_ERR_ARG_OUTOFRANGE,"nnz cannot be greater than block row length: local row %D value %D rowlength %D",i,nnz[i],mbs);
    }
  }
  
  ierr    = PetscOptionsHasName(B->prefix,"-mat_no_unroll",&flg);CHKERRQ(ierr);
  if (!flg) {
    switch (bs) {
    case 1:
      B->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqSBAIJ_1;
      B->ops->solve            = MatSolve_SeqSBAIJ_1;
      B->ops->solves           = MatSolves_SeqSBAIJ_1;
      B->ops->solvetranspose   = MatSolve_SeqSBAIJ_1;
      B->ops->mult             = MatMult_SeqSBAIJ_1;
      B->ops->multadd          = MatMultAdd_SeqSBAIJ_1;
      B->ops->multtranspose    = MatMult_SeqSBAIJ_1;
      B->ops->multtransposeadd = MatMultAdd_SeqSBAIJ_1;
      break;
    case 2:
      B->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqSBAIJ_2;  
      B->ops->solve            = MatSolve_SeqSBAIJ_2;
      B->ops->solvetranspose   = MatSolve_SeqSBAIJ_2;
      B->ops->mult             = MatMult_SeqSBAIJ_2;
      B->ops->multadd          = MatMultAdd_SeqSBAIJ_2;
      B->ops->multtranspose    = MatMult_SeqSBAIJ_2;
      B->ops->multtransposeadd = MatMultAdd_SeqSBAIJ_2;
      break;
    case 3:
      B->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqSBAIJ_3;  
      B->ops->solve            = MatSolve_SeqSBAIJ_3;
      B->ops->solvetranspose   = MatSolve_SeqSBAIJ_3;
      B->ops->mult             = MatMult_SeqSBAIJ_3;
      B->ops->multadd          = MatMultAdd_SeqSBAIJ_3;
      B->ops->multtranspose    = MatMult_SeqSBAIJ_3;
      B->ops->multtransposeadd = MatMultAdd_SeqSBAIJ_3;
      break;
    case 4:
      B->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqSBAIJ_4;  
      B->ops->solve            = MatSolve_SeqSBAIJ_4;
      B->ops->solvetranspose   = MatSolve_SeqSBAIJ_4;
      B->ops->mult             = MatMult_SeqSBAIJ_4;
      B->ops->multadd          = MatMultAdd_SeqSBAIJ_4;
      B->ops->multtranspose    = MatMult_SeqSBAIJ_4;
      B->ops->multtransposeadd = MatMultAdd_SeqSBAIJ_4;
      break;
    case 5:
      B->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqSBAIJ_5;  
      B->ops->solve            = MatSolve_SeqSBAIJ_5; 
      B->ops->solvetranspose   = MatSolve_SeqSBAIJ_5;
      B->ops->mult             = MatMult_SeqSBAIJ_5;
      B->ops->multadd          = MatMultAdd_SeqSBAIJ_5;
      B->ops->multtranspose    = MatMult_SeqSBAIJ_5;
      B->ops->multtransposeadd = MatMultAdd_SeqSBAIJ_5;
      break;
    case 6:
      B->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqSBAIJ_6;  
      B->ops->solve            = MatSolve_SeqSBAIJ_6; 
      B->ops->solvetranspose   = MatSolve_SeqSBAIJ_6;
      B->ops->mult             = MatMult_SeqSBAIJ_6;
      B->ops->multadd          = MatMultAdd_SeqSBAIJ_6;
      B->ops->multtranspose    = MatMult_SeqSBAIJ_6;
      B->ops->multtransposeadd = MatMultAdd_SeqSBAIJ_6;
      break;
    case 7:
      B->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqSBAIJ_7;
      B->ops->solve            = MatSolve_SeqSBAIJ_7;
      B->ops->solvetranspose   = MatSolve_SeqSBAIJ_7;
      B->ops->mult             = MatMult_SeqSBAIJ_7; 
      B->ops->multadd          = MatMultAdd_SeqSBAIJ_7;
      B->ops->multtranspose    = MatMult_SeqSBAIJ_7;
      B->ops->multtransposeadd = MatMultAdd_SeqSBAIJ_7;
      break;
    }
  }
  
  b->mbs = mbs;
  b->nbs = mbs; 
  if (!skipallocation) {
    /* b->ilen will count nonzeros in each block row so far. */
    ierr   = PetscMalloc2(mbs,PetscInt,&b->imax,mbs,PetscInt,&b->ilen);CHKERRQ(ierr);
    for (i=0; i<mbs; i++) { b->ilen[i] = 0;}
    if (!nnz) {
      if (nz == PETSC_DEFAULT || nz == PETSC_DECIDE) nz = 5;
      else if (nz <= 0)        nz = 1;
      for (i=0; i<mbs; i++) {
        b->imax[i] = nz; 
      }
      nz = nz*mbs; /* total nz */
    } else {
      nz = 0;
      for (i=0; i<mbs; i++) {b->imax[i] = nnz[i]; nz += nnz[i];}
    }
    /* nz=(nz+mbs)/2; */ /* total diagonal and superdiagonal nonzero blocks */
  
    /* allocate the matrix space */
    ierr = PetscMalloc3(bs2*nz,PetscScalar,&b->a,nz,PetscInt,&b->j,B->rmap.N+1,PetscInt,&b->i);CHKERRQ(ierr);
    ierr = PetscMemzero(b->a,nz*bs2*sizeof(MatScalar));CHKERRQ(ierr);
    ierr = PetscMemzero(b->j,nz*sizeof(PetscInt));CHKERRQ(ierr);
    b->singlemalloc = PETSC_TRUE;
  
    /* pointer to beginning of each row */
    b->i[0] = 0;
    for (i=1; i<mbs+1; i++) {
      b->i[i] = b->i[i-1] + b->imax[i-1];
    }
    b->free_a     = PETSC_TRUE;
    b->free_ij    = PETSC_TRUE;
  } else {
    b->free_a     = PETSC_FALSE;
    b->free_ij    = PETSC_FALSE;
  }
  
  B->rmap.bs               = bs;
  b->bs2              = bs2;
  b->nz             = 0;
  b->maxnz          = nz*bs2;
  
  b->inew             = 0;
  b->jnew             = 0;
  b->anew             = 0;
  b->a2anew           = 0;
  b->permute          = PETSC_FALSE;
  PetscFunctionReturn(0);
}
EXTERN_C_END

EXTERN_C_BEGIN
EXTERN PetscErrorCode PETSCMAT_DLLEXPORT MatConvert_SeqSBAIJ_SeqAIJ(Mat, MatType,MatReuse,Mat*); 
EXTERN PetscErrorCode PETSCMAT_DLLEXPORT MatConvert_SeqSBAIJ_SeqBAIJ(Mat, MatType,MatReuse,Mat*); 
EXTERN_C_END

/*MC
  MATSEQSBAIJ - MATSEQSBAIJ = "seqsbaij" - A matrix type to be used for sequential symmetric block sparse matrices, 
  based on block compressed sparse row format.  Only the upper triangular portion of the matrix is stored.
  
  Options Database Keys:
  . -mat_type seqsbaij - sets the matrix type to "seqsbaij" during a call to MatSetFromOptions()
  
  Level: beginner
  
  .seealso: MatCreateSeqSBAIJ
M*/

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "MatCreate_SeqSBAIJ"
PetscErrorCode PETSCMAT_DLLEXPORT MatCreate_SeqSBAIJ(Mat B)
{
  Mat_SeqSBAIJ   *b;
  PetscErrorCode ierr;
  PetscMPIInt    size;
  PetscTruth     flg;
  
  PetscFunctionBegin;
  ierr = MPI_Comm_size(B->comm,&size);CHKERRQ(ierr);
  if (size > 1) SETERRQ(PETSC_ERR_ARG_WRONG,"Comm must be of size 1");
  
  ierr    = PetscNewLog(B,Mat_SeqSBAIJ,&b);CHKERRQ(ierr);
  B->data = (void*)b;
  ierr    = PetscMemcpy(B->ops,&MatOps_Values,sizeof(struct _MatOps));CHKERRQ(ierr);
  B->ops->destroy     = MatDestroy_SeqSBAIJ;
  B->ops->view        = MatView_SeqSBAIJ;
  B->factor           = 0;
  B->mapping          = 0;
  b->row              = 0;
  b->icol             = 0;
  b->reallocs         = 0;
  b->saved_values     = 0;
  
  
  b->sorted           = PETSC_FALSE;
  b->roworiented      = PETSC_TRUE;
  b->nonew            = 0;
  b->diag             = 0;
  b->solve_work       = 0;
  b->mult_work        = 0;
  B->spptr            = 0;
  b->keepzeroedrows   = PETSC_FALSE;
  b->xtoy             = 0;
  b->XtoY             = 0;
  
  b->inew             = 0;
  b->jnew             = 0;
  b->anew             = 0;
  b->a2anew           = 0;
  b->permute          = PETSC_FALSE;

  b->ignore_ltriangular = PETSC_FALSE;
  ierr = PetscOptionsHasName(PETSC_NULL,"-mat_ignore_lower_triangular",&flg);CHKERRQ(ierr);
  if (flg) b->ignore_ltriangular = PETSC_TRUE;

  b->getrow_utriangular = PETSC_FALSE;
  ierr = PetscOptionsHasName(PETSC_NULL,"-mat_getrow_uppertriangular",&flg);CHKERRQ(ierr);
  if (flg) b->getrow_utriangular = PETSC_TRUE;

  ierr = PetscObjectComposeFunctionDynamic((PetscObject)B,"MatStoreValues_C",
                                     "MatStoreValues_SeqSBAIJ",
                                     MatStoreValues_SeqSBAIJ);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)B,"MatRetrieveValues_C",
                                     "MatRetrieveValues_SeqSBAIJ",
                                     (void*)MatRetrieveValues_SeqSBAIJ);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)B,"MatSeqSBAIJSetColumnIndices_C",
                                     "MatSeqSBAIJSetColumnIndices_SeqSBAIJ",
                                     MatSeqSBAIJSetColumnIndices_SeqSBAIJ);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)B,"MatConvert_seqsbaij_seqaij_C",
                                     "MatConvert_SeqSBAIJ_SeqAIJ",
                                      MatConvert_SeqSBAIJ_SeqAIJ);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)B,"MatConvert_seqsbaij_seqbaij_C",
                                     "MatConvert_SeqSBAIJ_SeqBAIJ",
                                      MatConvert_SeqSBAIJ_SeqBAIJ);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)B,"MatSeqSBAIJSetPreallocation_C",
                                     "MatSeqSBAIJSetPreallocation_SeqSBAIJ",
                                     MatSeqSBAIJSetPreallocation_SeqSBAIJ);CHKERRQ(ierr);

  B->symmetric                  = PETSC_TRUE;
  B->structurally_symmetric     = PETSC_TRUE;
  B->symmetric_set              = PETSC_TRUE;
  B->structurally_symmetric_set = PETSC_TRUE;
  ierr = PetscObjectChangeTypeName((PetscObject)B,MATSEQSBAIJ);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "MatSeqSBAIJSetPreallocation"
/*@C
   MatSeqSBAIJSetPreallocation - Creates a sparse symmetric matrix in block AIJ (block
   compressed row) format.  For good matrix assembly performance the
   user should preallocate the matrix storage by setting the parameter nz
   (or the array nnz).  By setting these parameters accurately, performance
   during matrix assembly can be increased by more than a factor of 50.

   Collective on Mat

   Input Parameters:
+  A - the symmetric matrix 
.  bs - size of block
.  nz - number of block nonzeros per block row (same for all rows)
-  nnz - array containing the number of block nonzeros in the upper triangular plus
         diagonal portion of each block (possibly different for each block row) or PETSC_NULL

   Options Database Keys:
.   -mat_no_unroll - uses code that does not unroll the loops in the 
                     block calculations (much slower)
.    -mat_block_size - size of the blocks to use

   Level: intermediate

   Notes:
   Specify the preallocated storage with either nz or nnz (not both).
   Set nz=PETSC_DEFAULT and nnz=PETSC_NULL for PETSc to control dynamic memory 
   allocation.  For additional details, see the users manual chapter on
   matrices.

   If the nnz parameter is given then the nz parameter is ignored


.seealso: MatCreate(), MatCreateSeqAIJ(), MatSetValues(), MatCreateMPISBAIJ()
@*/
PetscErrorCode PETSCMAT_DLLEXPORT MatSeqSBAIJSetPreallocation(Mat B,PetscInt bs,PetscInt nz,const PetscInt nnz[]) 
{
  PetscErrorCode ierr,(*f)(Mat,PetscInt,PetscInt,const PetscInt[]);

  PetscFunctionBegin;
  ierr = PetscObjectQueryFunction((PetscObject)B,"MatSeqSBAIJSetPreallocation_C",(void (**)(void))&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(B,bs,nz,nnz);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatCreateSeqSBAIJ"
/*@C
   MatCreateSeqSBAIJ - Creates a sparse symmetric matrix in block AIJ (block
   compressed row) format.  For good matrix assembly performance the
   user should preallocate the matrix storage by setting the parameter nz
   (or the array nnz).  By setting these parameters accurately, performance
   during matrix assembly can be increased by more than a factor of 50.

   Collective on MPI_Comm

   Input Parameters:
+  comm - MPI communicator, set to PETSC_COMM_SELF
.  bs - size of block
.  m - number of rows, or number of columns
.  nz - number of block nonzeros per block row (same for all rows)
-  nnz - array containing the number of block nonzeros in the upper triangular plus
         diagonal portion of each block (possibly different for each block row) or PETSC_NULL

   Output Parameter:
.  A - the symmetric matrix 

   Options Database Keys:
.   -mat_no_unroll - uses code that does not unroll the loops in the 
                     block calculations (much slower)
.    -mat_block_size - size of the blocks to use

   Level: intermediate

   Notes:
   The number of rows and columns must be divisible by blocksize.
   This matrix type does not support complex Hermitian operation.

   Specify the preallocated storage with either nz or nnz (not both).
   Set nz=PETSC_DEFAULT and nnz=PETSC_NULL for PETSc to control dynamic memory 
   allocation.  For additional details, see the users manual chapter on
   matrices.

   If the nnz parameter is given then the nz parameter is ignored

.seealso: MatCreate(), MatCreateSeqAIJ(), MatSetValues(), MatCreateMPISBAIJ()
@*/
PetscErrorCode PETSCMAT_DLLEXPORT MatCreateSeqSBAIJ(MPI_Comm comm,PetscInt bs,PetscInt m,PetscInt n,PetscInt nz,const PetscInt nnz[],Mat *A)
{
  PetscErrorCode ierr;
 
  PetscFunctionBegin;
  ierr = MatCreate(comm,A);CHKERRQ(ierr);
  ierr = MatSetSizes(*A,m,n,m,n);CHKERRQ(ierr);
  ierr = MatSetType(*A,MATSEQSBAIJ);CHKERRQ(ierr);
  ierr = MatSeqSBAIJSetPreallocation_SeqSBAIJ(*A,bs,nz,(PetscInt*)nnz);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatDuplicate_SeqSBAIJ"
PetscErrorCode MatDuplicate_SeqSBAIJ(Mat A,MatDuplicateOption cpvalues,Mat *B)
{
  Mat            C;
  Mat_SeqSBAIJ   *c,*a = (Mat_SeqSBAIJ*)A->data;
  PetscErrorCode ierr;
  PetscInt       i,mbs = a->mbs,nz = a->nz,bs2 =a->bs2;

  PetscFunctionBegin;
  if (a->i[mbs] != nz) SETERRQ(PETSC_ERR_PLIB,"Corrupt matrix");

  *B = 0;
  ierr = MatCreate(A->comm,&C);CHKERRQ(ierr);
  ierr = MatSetSizes(C,A->rmap.N,A->cmap.n,A->rmap.N,A->cmap.n);CHKERRQ(ierr);
  ierr = MatSetType(C,A->type_name);CHKERRQ(ierr);
  ierr = PetscMemcpy(C->ops,A->ops,sizeof(struct _MatOps));CHKERRQ(ierr);
  c    = (Mat_SeqSBAIJ*)C->data;

  C->preallocated   = PETSC_TRUE;
  C->factor         = A->factor;
  c->row            = 0;
  c->icol           = 0;
  c->saved_values   = 0;
  c->keepzeroedrows = a->keepzeroedrows;
  C->assembled      = PETSC_TRUE;

  ierr = PetscMapCopy(A->comm,&A->rmap,&C->rmap);CHKERRQ(ierr);  
  ierr = PetscMapCopy(A->comm,&A->cmap,&C->cmap);CHKERRQ(ierr);  
  c->bs2  = a->bs2;
  c->mbs  = a->mbs;
  c->nbs  = a->nbs;

  ierr = PetscMalloc2((mbs+1),PetscInt,&c->imax,(mbs+1),PetscInt,&c->ilen);CHKERRQ(ierr);
  for (i=0; i<mbs; i++) {
    c->imax[i] = a->imax[i];
    c->ilen[i] = a->ilen[i]; 
  }

  /* allocate the matrix space */
  ierr = PetscMalloc3(bs2*nz,MatScalar,&c->a,nz,PetscInt,&c->j,mbs+1,PetscInt,&c->i);CHKERRQ(ierr);
  c->singlemalloc = PETSC_TRUE;
  ierr = PetscMemcpy(c->i,a->i,(mbs+1)*sizeof(PetscInt));CHKERRQ(ierr);
  ierr = PetscLogObjectMemory(C,(mbs+1)*sizeof(PetscInt) + nz*(bs2*sizeof(MatScalar) + sizeof(PetscInt)));CHKERRQ(ierr);
  if (mbs > 0) {
    ierr = PetscMemcpy(c->j,a->j,nz*sizeof(PetscInt));CHKERRQ(ierr);
    if (cpvalues == MAT_COPY_VALUES) {
      ierr = PetscMemcpy(c->a,a->a,bs2*nz*sizeof(MatScalar));CHKERRQ(ierr);
    } else {
      ierr = PetscMemzero(c->a,bs2*nz*sizeof(MatScalar));CHKERRQ(ierr);
    }
  }

  c->sorted      = a->sorted;
  c->roworiented = a->roworiented;
  c->nonew       = a->nonew;

  if (a->diag) {
    ierr = PetscMalloc((mbs+1)*sizeof(PetscInt),&c->diag);CHKERRQ(ierr);
    ierr = PetscLogObjectMemory(C,(mbs+1)*sizeof(PetscInt));CHKERRQ(ierr);
    for (i=0; i<mbs; i++) {
      c->diag[i] = a->diag[i];
    }
  } else c->diag  = 0;
  c->nz           = a->nz;
  c->maxnz        = a->maxnz;
  c->solve_work   = 0;
  c->mult_work    = 0;
  c->free_a       = PETSC_TRUE;
  c->free_ij      = PETSC_TRUE;
  *B = C;
  ierr = PetscFListDuplicate(A->qlist,&C->qlist);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatLoad_SeqSBAIJ"
PetscErrorCode MatLoad_SeqSBAIJ(PetscViewer viewer, MatType type,Mat *A)
{
  Mat_SeqSBAIJ   *a;
  Mat            B;
  PetscErrorCode ierr;
  int            fd;
  PetscMPIInt    size;
  PetscInt       i,nz,header[4],*rowlengths=0,M,N,bs=1;
  PetscInt       *mask,mbs,*jj,j,rowcount,nzcount,k,*s_browlengths,maskcount;
  PetscInt       kmax,jcount,block,idx,point,nzcountb,extra_rows;
  PetscInt       *masked,nmask,tmp,bs2,ishift;
  PetscScalar    *aa;
  MPI_Comm       comm = ((PetscObject)viewer)->comm;

  PetscFunctionBegin;
  ierr = PetscOptionsGetInt(PETSC_NULL,"-matload_block_size",&bs,PETSC_NULL);CHKERRQ(ierr);
  bs2  = bs*bs;

  ierr = MPI_Comm_size(comm,&size);CHKERRQ(ierr);
  if (size > 1) SETERRQ(PETSC_ERR_ARG_WRONG,"view must have one processor");
  ierr = PetscViewerBinaryGetDescriptor(viewer,&fd);CHKERRQ(ierr);
  ierr = PetscBinaryRead(fd,header,4,PETSC_INT);CHKERRQ(ierr);
  if (header[0] != MAT_FILE_COOKIE) SETERRQ(PETSC_ERR_FILE_UNEXPECTED,"not Mat object");
  M = header[1]; N = header[2]; nz = header[3];

  if (header[3] < 0) {
    SETERRQ(PETSC_ERR_FILE_UNEXPECTED,"Matrix stored in special format, cannot load as SeqSBAIJ");
  }

  if (M != N) SETERRQ(PETSC_ERR_SUP,"Can only do square matrices");

  /* 
     This code adds extra rows to make sure the number of rows is 
    divisible by the blocksize
  */
  mbs        = M/bs;
  extra_rows = bs - M + bs*(mbs);
  if (extra_rows == bs) extra_rows = 0;
  else                  mbs++;
  if (extra_rows) {
    ierr = PetscInfo(0,"Padding loaded matrix to match blocksize\n");CHKERRQ(ierr);
  }

  /* read in row lengths */
  ierr = PetscMalloc((M+extra_rows)*sizeof(PetscInt),&rowlengths);CHKERRQ(ierr);
  ierr = PetscBinaryRead(fd,rowlengths,M,PETSC_INT);CHKERRQ(ierr);
  for (i=0; i<extra_rows; i++) rowlengths[M+i] = 1;

  /* read in column indices */
  ierr = PetscMalloc((nz+extra_rows)*sizeof(PetscInt),&jj);CHKERRQ(ierr);
  ierr = PetscBinaryRead(fd,jj,nz,PETSC_INT);CHKERRQ(ierr);
  for (i=0; i<extra_rows; i++) jj[nz+i] = M+i;

  /* loop over row lengths determining block row lengths */
  ierr     = PetscMalloc(mbs*sizeof(PetscInt),&s_browlengths);CHKERRQ(ierr);
  ierr     = PetscMemzero(s_browlengths,mbs*sizeof(PetscInt));CHKERRQ(ierr);
  ierr     = PetscMalloc(2*mbs*sizeof(PetscInt),&mask);CHKERRQ(ierr);
  ierr     = PetscMemzero(mask,mbs*sizeof(PetscInt));CHKERRQ(ierr);
  masked   = mask + mbs;
  rowcount = 0; nzcount = 0;
  for (i=0; i<mbs; i++) {
    nmask = 0;
    for (j=0; j<bs; j++) {
      kmax = rowlengths[rowcount];
      for (k=0; k<kmax; k++) {
        tmp = jj[nzcount++]/bs;   /* block col. index */
        if (!mask[tmp] && tmp >= i) {masked[nmask++] = tmp; mask[tmp] = 1;} 
      }
      rowcount++;
    }
    s_browlengths[i] += nmask;
    
    /* zero out the mask elements we set */
    for (j=0; j<nmask; j++) mask[masked[j]] = 0;
  }

  /* create our matrix */
  ierr = MatCreate(comm,&B);CHKERRQ(ierr);
  ierr = MatSetSizes(B,M+extra_rows,N+extra_rows,M+extra_rows,N+extra_rows);CHKERRQ(ierr);
  ierr = MatSetType(B,type);CHKERRQ(ierr);
  ierr = MatSeqSBAIJSetPreallocation_SeqSBAIJ(B,bs,0,s_browlengths);CHKERRQ(ierr);
  a = (Mat_SeqSBAIJ*)B->data;

  /* set matrix "i" values */
  a->i[0] = 0;
  for (i=1; i<= mbs; i++) {
    a->i[i]      = a->i[i-1] + s_browlengths[i-1];
    a->ilen[i-1] = s_browlengths[i-1];
  }
  a->nz = a->i[mbs];

  /* read in nonzero values */
  ierr = PetscMalloc((nz+extra_rows)*sizeof(PetscScalar),&aa);CHKERRQ(ierr);
  ierr = PetscBinaryRead(fd,aa,nz,PETSC_SCALAR);CHKERRQ(ierr);
  for (i=0; i<extra_rows; i++) aa[nz+i] = 1.0;

  /* set "a" and "j" values into matrix */
  nzcount = 0; jcount = 0;
  for (i=0; i<mbs; i++) {
    nzcountb = nzcount;
    nmask    = 0;
    for (j=0; j<bs; j++) {
      kmax = rowlengths[i*bs+j];
      for (k=0; k<kmax; k++) {
        tmp = jj[nzcount++]/bs; /* block col. index */
        if (!mask[tmp] && tmp >= i) { masked[nmask++] = tmp; mask[tmp] = 1;} 
      }
    }
    /* sort the masked values */
    ierr = PetscSortInt(nmask,masked);CHKERRQ(ierr);

    /* set "j" values into matrix */
    maskcount = 1;
    for (j=0; j<nmask; j++) {
      a->j[jcount++]  = masked[j];
      mask[masked[j]] = maskcount++; 
    }

    /* set "a" values into matrix */
    ishift = bs2*a->i[i]; 
    for (j=0; j<bs; j++) {
      kmax = rowlengths[i*bs+j];
      for (k=0; k<kmax; k++) {
        tmp       = jj[nzcountb]/bs ; /* block col. index */
        if (tmp >= i){ 
          block     = mask[tmp] - 1;
          point     = jj[nzcountb] - bs*tmp;
          idx       = ishift + bs2*block + j + bs*point;
          a->a[idx] = aa[nzcountb];
        } 
        nzcountb++;
      }
    }
    /* zero out the mask elements we set */
    for (j=0; j<nmask; j++) mask[masked[j]] = 0;
  }
  if (jcount != a->nz) SETERRQ(PETSC_ERR_FILE_UNEXPECTED,"Bad binary matrix");

  ierr = PetscFree(rowlengths);CHKERRQ(ierr);
  ierr = PetscFree(s_browlengths);CHKERRQ(ierr);
  ierr = PetscFree(aa);CHKERRQ(ierr);
  ierr = PetscFree(jj);CHKERRQ(ierr);
  ierr = PetscFree(mask);CHKERRQ(ierr);

  ierr = MatAssemblyBegin(B,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(B,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatView_Private(B);CHKERRQ(ierr);
  *A = B;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatRelax_SeqSBAIJ"
PetscErrorCode MatRelax_SeqSBAIJ(Mat A,Vec bb,PetscReal omega,MatSORType flag,PetscReal fshift,PetscInt its,PetscInt lits,Vec xx)
{
  Mat_SeqSBAIJ   *a = (Mat_SeqSBAIJ*)A->data;
  MatScalar      *aa=a->a,*v,*v1;
  PetscScalar    *x,*b,*t,sum,d;
  PetscErrorCode ierr;
  PetscInt       m=a->mbs,bs=A->rmap.bs,*ai=a->i,*aj=a->j;
  PetscInt       nz,nz1,*vj,*vj1,i;

  PetscFunctionBegin;
  if (flag & SOR_EISENSTAT) SETERRQ(PETSC_ERR_SUP,"No support yet for Eisenstat");

  its = its*lits;
  if (its <= 0) SETERRQ2(PETSC_ERR_ARG_WRONG,"Relaxation requires global its %D and local its %D both positive",its,lits);

  if (bs > 1)
    SETERRQ(PETSC_ERR_SUP,"SSOR for block size > 1 is not yet implemented");

  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  if (xx != bb) { 
    ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  } else { 
    b = x;
  } 

  if (!a->relax_work) {
    ierr = PetscMalloc(m*sizeof(PetscScalar),&a->relax_work);CHKERRQ(ierr);
  }
  t = a->relax_work;
 
  if (flag & SOR_ZERO_INITIAL_GUESS) {
    if (flag & SOR_FORWARD_SWEEP || flag & SOR_LOCAL_FORWARD_SWEEP){ 
      ierr = PetscMemcpy(t,b,m*sizeof(PetscScalar));CHKERRQ(ierr);

      for (i=0; i<m; i++){
        d  = *(aa + ai[i]);  /* diag[i] */
        v  = aa + ai[i] + 1; 
        vj = aj + ai[i] + 1;    
        nz = ai[i+1] - ai[i] - 1;       
        ierr = PetscLogFlops(2*nz-1);CHKERRQ(ierr);
        x[i] = omega*t[i]/d;
        while (nz--) t[*vj++] -= x[i]*(*v++); /* update rhs */
      }
    } 

    if (flag & SOR_BACKWARD_SWEEP || flag & SOR_LOCAL_BACKWARD_SWEEP){ 
      if (!(flag & SOR_FORWARD_SWEEP || flag & SOR_LOCAL_FORWARD_SWEEP)){ 
        t = b;
      }
  
      for (i=m-1; i>=0; i--){
        d  = *(aa + ai[i]);  
        v  = aa + ai[i] + 1; 
        vj = aj + ai[i] + 1;    
        nz = ai[i+1] - ai[i] - 1;
        ierr = PetscLogFlops(2*nz-1);CHKERRQ(ierr);
        sum = t[i];
        while (nz--) sum -= x[*vj++]*(*v++);
        x[i] =   (1-omega)*x[i] + omega*sum/d;        
      }
      t = a->relax_work;
    }
    its--;
  } 

  while (its--) {
    /* 
       forward sweep:
       for i=0,...,m-1:
         sum[i] = (b[i] - U(i,:)x )/d[i];
         x[i]   = (1-omega)x[i] + omega*sum[i];
         b      = b - x[i]*U^T(i,:);
         
    */ 
    if (flag & SOR_FORWARD_SWEEP || flag & SOR_LOCAL_FORWARD_SWEEP){ 
      ierr = PetscMemcpy(t,b,m*sizeof(PetscScalar));CHKERRQ(ierr);

      for (i=0; i<m; i++){
        d  = *(aa + ai[i]);  /* diag[i] */
        v  = aa + ai[i] + 1; v1=v;
        vj = aj + ai[i] + 1; vj1=vj;   
        nz = ai[i+1] - ai[i] - 1; nz1=nz;
        sum = t[i];
        ierr = PetscLogFlops(4*nz-2);CHKERRQ(ierr);
        while (nz1--) sum -= (*v1++)*x[*vj1++]; 
        x[i] = (1-omega)*x[i] + omega*sum/d;
        while (nz--) t[*vj++] -= x[i]*(*v++); 
      }
    }
  
    if (flag & SOR_BACKWARD_SWEEP || flag & SOR_LOCAL_BACKWARD_SWEEP){ 
      /* 
       backward sweep:
       b = b - x[i]*U^T(i,:), i=0,...,n-2
       for i=m-1,...,0:
         sum[i] = (b[i] - U(i,:)x )/d[i];
         x[i]   = (1-omega)x[i] + omega*sum[i];
      */
      /* if there was a forward sweep done above then I thing the next two for loops are not needed */ 
      ierr = PetscMemcpy(t,b,m*sizeof(PetscScalar));CHKERRQ(ierr);
  
      for (i=0; i<m-1; i++){  /* update rhs */
        v  = aa + ai[i] + 1; 
        vj = aj + ai[i] + 1;    
        nz = ai[i+1] - ai[i] - 1;
        ierr = PetscLogFlops(2*nz-1);CHKERRQ(ierr);
        while (nz--) t[*vj++] -= x[i]*(*v++);
      }
      for (i=m-1; i>=0; i--){
        d  = *(aa + ai[i]);  
        v  = aa + ai[i] + 1; 
        vj = aj + ai[i] + 1;    
        nz = ai[i+1] - ai[i] - 1;
        ierr = PetscLogFlops(2*nz-1);CHKERRQ(ierr);
        sum = t[i];
        while (nz--) sum -= x[*vj++]*(*v++);
        x[i] =   (1-omega)*x[i] + omega*sum/d;        
      }
    }
  } 

  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  if (bb != xx) { 
    ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  } 
  PetscFunctionReturn(0);
} 

#undef __FUNCT__  
#define __FUNCT__ "MatCreateSeqSBAIJWithArrays"
/*@
     MatCreateSeqSBAIJWithArrays - Creates an sequential SBAIJ matrix using matrix elements 
              (upper triangular entries in CSR format) provided by the user.

     Collective on MPI_Comm

   Input Parameters:
+  comm - must be an MPI communicator of size 1
.  bs - size of block
.  m - number of rows
.  n - number of columns
.  i - row indices
.  j - column indices
-  a - matrix values

   Output Parameter:
.  mat - the matrix

   Level: intermediate

   Notes:
       The i, j, and a arrays are not copied by this routine, the user must free these arrays
    once the matrix is destroyed

       You cannot set new nonzero locations into this matrix, that will generate an error.

       The i and j indices are 0 based

.seealso: MatCreate(), MatCreateMPISBAIJ(), MatCreateSeqSBAIJ()

@*/
PetscErrorCode PETSCMAT_DLLEXPORT MatCreateSeqSBAIJWithArrays(MPI_Comm comm,PetscInt bs,PetscInt m,PetscInt n,PetscInt* i,PetscInt*j,PetscScalar *a,Mat *mat)
{
  PetscErrorCode ierr;
  PetscInt       ii;
  Mat_SeqSBAIJ   *sbaij;

  PetscFunctionBegin;
  if (bs != 1) SETERRQ1(PETSC_ERR_SUP,"block size %D > 1 is not supported yet",bs);
  if (i[0]) SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"i (row indices) must start with 0");
  
  ierr = MatCreate(comm,mat);CHKERRQ(ierr);
  ierr = MatSetSizes(*mat,m,n,m,n);CHKERRQ(ierr);
  ierr = MatSetType(*mat,MATSEQSBAIJ);CHKERRQ(ierr);
  ierr = MatSeqSBAIJSetPreallocation_SeqSBAIJ(*mat,bs,MAT_SKIP_ALLOCATION,0);CHKERRQ(ierr);
  sbaij = (Mat_SeqSBAIJ*)(*mat)->data;
  ierr = PetscMalloc2(m,PetscInt,&sbaij->imax,m,PetscInt,&sbaij->ilen);CHKERRQ(ierr);

  sbaij->i = i;
  sbaij->j = j;
  sbaij->a = a;
  sbaij->singlemalloc = PETSC_FALSE;
  sbaij->nonew        = -1;             /*this indicates that inserting a new value in the matrix that generates a new nonzero is an error*/
  sbaij->free_a       = PETSC_FALSE; 
  sbaij->free_ij      = PETSC_FALSE; 

  for (ii=0; ii<m; ii++) {
    sbaij->ilen[ii] = sbaij->imax[ii] = i[ii+1] - i[ii];
#if defined(PETSC_USE_DEBUG)
    if (i[ii+1] - i[ii] < 0) SETERRQ2(PETSC_ERR_ARG_OUTOFRANGE,"Negative row length in i (row indices) row = %d length = %d",ii,i[ii+1] - i[ii]);
#endif    
  }
#if defined(PETSC_USE_DEBUG)
  for (ii=0; ii<sbaij->i[m]; ii++) {
    if (j[ii] < 0) SETERRQ2(PETSC_ERR_ARG_OUTOFRANGE,"Negative column index at location = %d index = %d",ii,j[ii]);
    if (j[ii] > n - 1) SETERRQ2(PETSC_ERR_ARG_OUTOFRANGE,"Column index to large at location = %d index = %d",ii,j[ii]);
  }
#endif    

  ierr = MatAssemblyBegin(*mat,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(*mat,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}





