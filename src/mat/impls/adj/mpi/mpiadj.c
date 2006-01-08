#define PETSCMAT_DLL

/*
    Defines the basic matrix operations for the ADJ adjacency list matrix data-structure. 
*/
#include "src/mat/impls/adj/mpi/mpiadj.h"
#include "petscsys.h"

#undef __FUNCT__  
#define __FUNCT__ "MatView_MPIAdj_ASCII"
PetscErrorCode MatView_MPIAdj_ASCII(Mat A,PetscViewer viewer)
{
  Mat_MPIAdj        *a = (Mat_MPIAdj*)A->data;
  PetscErrorCode    ierr;
  PetscInt          i,j,m = A->m;
  const char        *name;
  PetscViewerFormat format;

  PetscFunctionBegin;
  ierr = PetscObjectGetName((PetscObject)A,&name);CHKERRQ(ierr);
  ierr = PetscViewerGetFormat(viewer,&format);CHKERRQ(ierr);
  if (format == PETSC_VIEWER_ASCII_INFO) {
    PetscFunctionReturn(0);
  } else if (format == PETSC_VIEWER_ASCII_MATLAB) {
    SETERRQ(PETSC_ERR_SUP,"Matlab format not supported");
  } else {
    ierr = PetscViewerASCIIUseTabs(viewer,PETSC_NO);CHKERRQ(ierr);
    for (i=0; i<m; i++) {
      ierr = PetscViewerASCIISynchronizedPrintf(viewer,"row %D:",i+a->rstart);CHKERRQ(ierr);
      for (j=a->i[i]; j<a->i[i+1]; j++) {
        ierr = PetscViewerASCIISynchronizedPrintf(viewer," %D ",a->j[j]);CHKERRQ(ierr);
      }
      ierr = PetscViewerASCIISynchronizedPrintf(viewer,"\n");CHKERRQ(ierr);
    }
    ierr = PetscViewerASCIIUseTabs(viewer,PETSC_YES);CHKERRQ(ierr);
  } 
  ierr = PetscViewerFlush(viewer);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatView_MPIAdj"
PetscErrorCode MatView_MPIAdj(Mat A,PetscViewer viewer)
{
  PetscErrorCode ierr;
  PetscTruth     iascii;

  PetscFunctionBegin;
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_ASCII,&iascii);CHKERRQ(ierr);
  if (iascii) {
    ierr = MatView_MPIAdj_ASCII(A,viewer);CHKERRQ(ierr);
  } else {
    SETERRQ1(PETSC_ERR_SUP,"Viewer type %s not supported by MPIAdj",((PetscObject)viewer)->type_name);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatDestroy_MPIAdj"
PetscErrorCode MatDestroy_MPIAdj(Mat mat)
{
  Mat_MPIAdj     *a = (Mat_MPIAdj*)mat->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
#if defined(PETSC_USE_LOG)
  PetscLogObjectState((PetscObject)mat,"Rows=%D, Cols=%D, NZ=%D",mat->m,mat->n,a->nz);
#endif
  if (a->diag) {ierr = PetscFree(a->diag);CHKERRQ(ierr);}
  if (a->freeaij) {
    ierr = PetscFree(a->i);CHKERRQ(ierr);
    ierr = PetscFree(a->j);CHKERRQ(ierr);
    if (a->values) {ierr = PetscFree(a->values);CHKERRQ(ierr);}
  }
  ierr = PetscFree(a->rowners);CHKERRQ(ierr);
  ierr = PetscFree(a);CHKERRQ(ierr);

  ierr = PetscObjectComposeFunction((PetscObject)mat,"MatMPIAdjSetPreallocation_C","",PETSC_NULL);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSetOption_MPIAdj"
PetscErrorCode MatSetOption_MPIAdj(Mat A,MatOption op)
{
  Mat_MPIAdj     *a = (Mat_MPIAdj*)A->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  switch (op) {
  case MAT_SYMMETRIC:
  case MAT_STRUCTURALLY_SYMMETRIC:
  case MAT_HERMITIAN:
    a->symmetric = PETSC_TRUE;
    break;
  case MAT_NOT_SYMMETRIC:
  case MAT_NOT_STRUCTURALLY_SYMMETRIC:
  case MAT_NOT_HERMITIAN:
    a->symmetric = PETSC_FALSE;
    break;
  case MAT_SYMMETRY_ETERNAL:
  case MAT_NOT_SYMMETRY_ETERNAL:
    break;
  default:
    ierr = PetscInfo((A,"MatSetOption_MPIAdj:Option ignored\n"));CHKERRQ(ierr);
    break;
  }
  PetscFunctionReturn(0);
}


/*
     Adds diagonal pointers to sparse matrix structure.
*/

#undef __FUNCT__  
#define __FUNCT__ "MatMarkDiagonal_MPIAdj"
PetscErrorCode MatMarkDiagonal_MPIAdj(Mat A)
{
  Mat_MPIAdj     *a = (Mat_MPIAdj*)A->data; 
  PetscErrorCode ierr;
  PetscInt       i,j,*diag,m = A->m;

  PetscFunctionBegin;
  ierr = PetscMalloc((m+1)*sizeof(PetscInt),&diag);CHKERRQ(ierr);
  ierr = PetscLogObjectMemory(A,(m+1)*sizeof(PetscInt));CHKERRQ(ierr);
  for (i=0; i<A->m; i++) {
    for (j=a->i[i]; j<a->i[i+1]; j++) {
      if (a->j[j] == i) {
        diag[i] = j;
        break;
      }
    }
  }
  a->diag = diag;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatGetRow_MPIAdj"
PetscErrorCode MatGetRow_MPIAdj(Mat A,PetscInt row,PetscInt *nz,PetscInt **idx,PetscScalar **v)
{
  Mat_MPIAdj *a = (Mat_MPIAdj*)A->data;
  PetscInt   *itmp;

  PetscFunctionBegin;
  row -= a->rstart;

  if (row < 0 || row >= A->m) SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"Row out of range");

  *nz = a->i[row+1] - a->i[row];
  if (v) *v = PETSC_NULL;
  if (idx) {
    itmp = a->j + a->i[row];
    if (*nz) {
      *idx = itmp;
    }
    else *idx = 0;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatRestoreRow_MPIAdj"
PetscErrorCode MatRestoreRow_MPIAdj(Mat A,PetscInt row,PetscInt *nz,PetscInt **idx,PetscScalar **v)
{
  PetscFunctionBegin;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatEqual_MPIAdj"
PetscErrorCode MatEqual_MPIAdj(Mat A,Mat B,PetscTruth* flg)
{
  Mat_MPIAdj     *a = (Mat_MPIAdj *)A->data,*b = (Mat_MPIAdj *)B->data;
  PetscErrorCode ierr;
  PetscTruth     flag;

  PetscFunctionBegin;
  /* If the  matrix dimensions are not equal,or no of nonzeros */
  if ((A->m != B->m) ||(a->nz != b->nz)) {
    flag = PETSC_FALSE;
  }
  
  /* if the a->i are the same */
  ierr = PetscMemcmp(a->i,b->i,(A->m+1)*sizeof(PetscInt),&flag);CHKERRQ(ierr);
  
  /* if a->j are the same */
  ierr = PetscMemcmp(a->j,b->j,(a->nz)*sizeof(PetscInt),&flag);CHKERRQ(ierr);

  ierr = MPI_Allreduce(&flag,flg,1,MPI_INT,MPI_LAND,A->comm);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatGetRowIJ_MPIAdj"
PetscErrorCode MatGetRowIJ_MPIAdj(Mat A,PetscInt oshift,PetscTruth symmetric,PetscInt *m,PetscInt *ia[],PetscInt *ja[],PetscTruth *done)
{
  PetscErrorCode ierr;
  PetscMPIInt    size;
  PetscInt       i;
  Mat_MPIAdj     *a = (Mat_MPIAdj *)A->data;

  PetscFunctionBegin;
  ierr = MPI_Comm_size(A->comm,&size);CHKERRQ(ierr);
  if (size > 1) {*done = PETSC_FALSE; PetscFunctionReturn(0);}
  *m    = A->m;
  *ia   = a->i;
  *ja   = a->j;
  *done = PETSC_TRUE;
  if (oshift) {
    for (i=0; i<(*ia)[*m]; i++) {
      (*ja)[i]++;
    }
    for (i=0; i<=(*m); i++) (*ia)[i]++;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatRestoreRowIJ_MPIAdj"
PetscErrorCode MatRestoreRowIJ_MPIAdj(Mat A,PetscInt oshift,PetscTruth symmetric,PetscInt *m,PetscInt *ia[],PetscInt *ja[],PetscTruth *done)
{
  PetscInt   i;
  Mat_MPIAdj *a = (Mat_MPIAdj *)A->data;

  PetscFunctionBegin;
  if (ia && a->i != *ia) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"ia passed back is not one obtained with MatGetRowIJ()");
  if (ja && a->j != *ja) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"ja passed back is not one obtained with MatGetRowIJ()");
  if (oshift) {
    for (i=0; i<=(*m); i++) (*ia)[i]--;
    for (i=0; i<(*ia)[*m]; i++) {
      (*ja)[i]--;
    }
  }
  PetscFunctionReturn(0);
}

/* -------------------------------------------------------------------*/
static struct _MatOps MatOps_Values = {0,
       MatGetRow_MPIAdj,
       MatRestoreRow_MPIAdj,
       0,
/* 4*/ 0,
       0,
       0,
       0,
       0,
       0,
/*10*/ 0,
       0,
       0,
       0,
       0,
/*15*/ 0,
       MatEqual_MPIAdj,
       0,
       0,
       0,
/*20*/ 0,
       0,
       0,
       MatSetOption_MPIAdj,
       0,
/*25*/ 0,
       0,
       0,
       0,
       0,
/*30*/ 0,
       0,
       0,
       0,
       0,
/*35*/ 0,
       0,
       0,
       0,
       0,
/*40*/ 0,
       0,
       0,
       0,
       0,
/*45*/ 0,
       0,
       0,
       0,
       0,
/*50*/ 0,
       MatGetRowIJ_MPIAdj,
       MatRestoreRowIJ_MPIAdj,
       0,
       0,
/*55*/ 0,
       0,
       0,
       0,
       0,
/*60*/ 0,
       MatDestroy_MPIAdj,
       MatView_MPIAdj,
       MatGetPetscMaps_Petsc,
       0,
/*65*/ 0,
       0,
       0,
       0,
       0,
/*70*/ 0,
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
       0,
       0,
/*85*/ 0,
       0,
       0,
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
       0};

EXTERN_C_BEGIN
#undef __FUNCT__
#define __FUNCT__ "MatMPIAdjSetPreallocation_MPIAdj"
PetscErrorCode PETSCMAT_DLLEXPORT MatMPIAdjSetPreallocation_MPIAdj(Mat B,PetscInt *i,PetscInt *j,PetscInt *values)
{
  Mat_MPIAdj     *b = (Mat_MPIAdj *)B->data;
  PetscErrorCode ierr;
#if defined(PETSC_USE_DEBUG)
  PetscInt       ii;
#endif

  PetscFunctionBegin;
  B->preallocated = PETSC_TRUE;
#if defined(PETSC_USE_DEBUG)
  if (i[0] != 0) SETERRQ1(PETSC_ERR_ARG_OUTOFRANGE,"First i[] index must be zero, instead it is %D\n",i[0]);
  for (ii=1; ii<B->m; ii++) {
    if (i[ii] < 0 || i[ii] < i[ii-1]) {
      SETERRQ4(PETSC_ERR_ARG_OUTOFRANGE,"i[%D]=%D index is out of range: i[%D]=%D",ii,i[ii],ii-1,i[ii-1]);
    }
  }
  for (ii=0; ii<i[B->m]; ii++) {
    if (j[ii] < 0 || j[ii] >= B->N) {
      SETERRQ2(PETSC_ERR_ARG_OUTOFRANGE,"Column index %D out of range %D\n",ii,j[ii]);
    }
  } 
#endif

  b->j      = j;
  b->i      = i;
  b->values = values;

  b->nz               = i[B->m];
  b->diag             = 0;
  b->symmetric        = PETSC_FALSE;
  b->freeaij          = PETSC_TRUE;

  ierr = MatAssemblyBegin(B,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(B,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END

/*MC
   MATMPIADJ - MATMPIADJ = "mpiadj" - A matrix type to be used for distributed adjacency matrices,
   intended for use constructing orderings and partitionings.

  Level: beginner

.seealso: MatCreateMPIAdj
M*/

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "MatCreate_MPIAdj"
PetscErrorCode PETSCMAT_DLLEXPORT MatCreate_MPIAdj(Mat B)
{
  Mat_MPIAdj     *b;
  PetscErrorCode ierr;
  PetscInt       ii;
  PetscMPIInt    size,rank;

  PetscFunctionBegin;
  ierr = MPI_Comm_size(B->comm,&size);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(B->comm,&rank);CHKERRQ(ierr);

  ierr                = PetscNew(Mat_MPIAdj,&b);CHKERRQ(ierr);
  B->data             = (void*)b;
  ierr                = PetscMemcpy(B->ops,&MatOps_Values,sizeof(struct _MatOps));CHKERRQ(ierr);
  B->factor           = 0;
  B->mapping          = 0;
  B->assembled        = PETSC_FALSE;
  
  ierr = PetscSplitOwnership(B->comm,&B->m,&B->M);CHKERRQ(ierr);
  B->n = B->N = PetscMax(B->N,B->n);CHKERRQ(ierr);

  /* the information in the maps duplicates the information computed below, eventually 
     we should remove the duplicate information that is not contained in the maps */
  ierr = PetscMapCreateMPI(B->comm,B->m,B->M,&B->rmap);CHKERRQ(ierr);
  /* we don't know the "local columns" so just use the row information :-(*/
  ierr = PetscMapCreateMPI(B->comm,B->m,B->M,&B->cmap);CHKERRQ(ierr);

  ierr = PetscMalloc((size+1)*sizeof(PetscInt),&b->rowners);CHKERRQ(ierr);
  ierr = PetscLogObjectMemory(B,(size+2)*sizeof(PetscInt)+sizeof(struct _p_Mat)+sizeof(Mat_MPIAdj));CHKERRQ(ierr);
  ierr = MPI_Allgather(&B->m,1,MPI_INT,b->rowners+1,1,MPI_INT,B->comm);CHKERRQ(ierr);
  b->rowners[0] = 0;
  for (ii=2; ii<=size; ii++) {
    b->rowners[ii] += b->rowners[ii-1];
  }
  b->rstart = b->rowners[rank]; 
  b->rend   = b->rowners[rank+1]; 
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)B,"MatMPIAdjSetPreallocation_C",
                                    "MatMPIAdjSetPreallocation_MPIAdj",
                                     MatMPIAdjSetPreallocation_MPIAdj);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "MatMPIAdjSetPreallocation"
/*@C
   MatMPIAdjSetPreallocation - Sets the array used for storing the matrix elements

   Collective on MPI_Comm

   Input Parameters:
+  A - the matrix
.  i - the indices into j for the start of each row
.  j - the column indices for each row (sorted for each row).
       The indices in i and j start with zero (NOT with one).
-  values - [optional] edge weights

   Level: intermediate

.seealso: MatCreate(), MatCreateMPIAdj(), MatSetValues()
@*/
PetscErrorCode PETSCMAT_DLLEXPORT MatMPIAdjSetPreallocation(Mat B,PetscInt *i,PetscInt *j,PetscInt *values)
{
  PetscErrorCode ierr,(*f)(Mat,PetscInt*,PetscInt*,PetscInt*);

  PetscFunctionBegin;
  ierr = PetscObjectQueryFunction((PetscObject)B,"MatMPIAdjSetPreallocation_C",(void (**)(void))&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(B,i,j,values);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatCreateMPIAdj"
/*@C
   MatCreateMPIAdj - Creates a sparse matrix representing an adjacency list.
   The matrix does not have numerical values associated with it, but is
   intended for ordering (to reduce bandwidth etc) and partitioning.

   Collective on MPI_Comm

   Input Parameters:
+  comm - MPI communicator
.  m - number of local rows
.  n - number of columns
.  i - the indices into j for the start of each row
.  j - the column indices for each row (sorted for each row).
       The indices in i and j start with zero (NOT with one).
-  values -[optional] edge weights

   Output Parameter:
.  A - the matrix 

   Level: intermediate

   Notes: This matrix object does not support most matrix operations, include
   MatSetValues().
   You must NOT free the ii, values and jj arrays yourself. PETSc will free them
   when the matrix is destroyed; you must allocate them with PetscMalloc(). If you 
    call from Fortran you need not create the arrays with PetscMalloc().
   Should not include the matrix diagonals.

   If you already have a matrix, you can create its adjacency matrix by a call
   to MatConvert, specifying a type of MATMPIADJ.

   Possible values for MatSetOption() - MAT_STRUCTURALLY_SYMMETRIC

.seealso: MatCreate(), MatConvert(), MatGetOrdering()
@*/
PetscErrorCode PETSCMAT_DLLEXPORT MatCreateMPIAdj(MPI_Comm comm,PetscInt m,PetscInt n,PetscInt *i,PetscInt *j,PetscInt *values,Mat *A)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = MatCreate(comm,A);CHKERRQ(ierr);
  ierr = MatSetSizes(*A,m,n,PETSC_DETERMINE,n);CHKERRQ(ierr);
  ierr = MatSetType(*A,MATMPIADJ);CHKERRQ(ierr);
  ierr = MatMPIAdjSetPreallocation(*A,i,j,values);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "MatConvertTo_MPIAdj"
PetscErrorCode PETSCMAT_DLLEXPORT MatConvertTo_MPIAdj(Mat A,MatType type,MatReuse reuse,Mat *newmat)
{
  Mat               B;
  PetscErrorCode    ierr;
  PetscInt          i,m,N,nzeros = 0,*ia,*ja,len,rstart,cnt,j,*a;
  const PetscInt    *rj;
  const PetscScalar *ra;
  MPI_Comm          comm;

  PetscFunctionBegin;
  ierr = MatGetSize(A,PETSC_NULL,&N);CHKERRQ(ierr);
  ierr = MatGetLocalSize(A,&m,PETSC_NULL);CHKERRQ(ierr);
  ierr = MatGetOwnershipRange(A,&rstart,PETSC_NULL);CHKERRQ(ierr);
  
  /* count the number of nonzeros per row */
  for (i=0; i<m; i++) {
    ierr   = MatGetRow(A,i+rstart,&len,&rj,PETSC_NULL);CHKERRQ(ierr);
    for (j=0; j<len; j++) {
      if (rj[j] == i+rstart) {len--; break;}    /* don't count diagonal */
    }
    ierr   = MatRestoreRow(A,i+rstart,&len,&rj,PETSC_NULL);CHKERRQ(ierr);
    nzeros += len;
  }

  /* malloc space for nonzeros */
  ierr = PetscMalloc((nzeros+1)*sizeof(PetscInt),&a);CHKERRQ(ierr);
  ierr = PetscMalloc((N+1)*sizeof(PetscInt),&ia);CHKERRQ(ierr);
  ierr = PetscMalloc((nzeros+1)*sizeof(PetscInt),&ja);CHKERRQ(ierr);

  nzeros = 0;
  ia[0]  = 0;
  for (i=0; i<m; i++) {
    ierr    = MatGetRow(A,i+rstart,&len,&rj,&ra);CHKERRQ(ierr);
    cnt     = 0;
    for (j=0; j<len; j++) {
      if (rj[j] != i+rstart) { /* if not diagonal */
        a[nzeros+cnt]    = (PetscInt) PetscAbsScalar(ra[j]);
        ja[nzeros+cnt++] = rj[j];
      } 
    }
    ierr    = MatRestoreRow(A,i+rstart,&len,&rj,&ra);CHKERRQ(ierr);
    nzeros += cnt;
    ia[i+1] = nzeros; 
  }

  ierr = PetscObjectGetComm((PetscObject)A,&comm);CHKERRQ(ierr);
  ierr = MatCreate(comm,&B);CHKERRQ(ierr);
  ierr = MatSetSizes(B,m,N,PETSC_DETERMINE,N);CHKERRQ(ierr);
  ierr = MatSetType(B,type);CHKERRQ(ierr);
  ierr = MatMPIAdjSetPreallocation(B,ia,ja,a);CHKERRQ(ierr);

  if (reuse == MAT_REUSE_MATRIX) {
    ierr = MatHeaderReplace(A,B);CHKERRQ(ierr);
  } else {
    *newmat = B;
  }
  PetscFunctionReturn(0);
}
EXTERN_C_END





