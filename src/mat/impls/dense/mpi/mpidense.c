/*
   Basic functions for basic parallel dense matrices.
*/
    
#include "src/mat/impls/dense/mpi/mpidense.h"

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "MatGetDiagonalBlock_MPIDense"
int MatGetDiagonalBlock_MPIDense(Mat A,PetscTruth *iscopy,MatReuse reuse,Mat *B)
{
  Mat_MPIDense *mdn = (Mat_MPIDense*)A->data;
  int          m = A->m,rstart = mdn->rstart,ierr;
  PetscScalar  *array;
  MPI_Comm     comm;

  PetscFunctionBegin;
  if (A->M != A->N) SETERRQ(PETSC_ERR_SUP,"Only square matrices supported.");

  /* The reuse aspect is not implemented efficiently */
  if (reuse) { ierr = MatDestroy(*B);CHKERRQ(ierr);}

  ierr = PetscObjectGetComm((PetscObject)(mdn->A),&comm);CHKERRQ(ierr);
  ierr = MatGetArray(mdn->A,&array);CHKERRQ(ierr);
  ierr = MatCreate(comm,m,m,m,m,B);CHKERRQ(ierr);
  ierr = MatSetType(*B,mdn->A->type_name);CHKERRQ(ierr);
  ierr = MatSeqDenseSetPreallocation(*B,array+m*rstart);CHKERRQ(ierr);
  ierr = MatRestoreArray(mdn->A,&array);CHKERRQ(ierr);
  ierr = MatAssemblyBegin(*B,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(*B,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
    
  *iscopy = PETSC_TRUE;
  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "MatSetValues_MPIDense"
int MatSetValues_MPIDense(Mat mat,int m,const int idxm[],int n,const int idxn[],const PetscScalar v[],InsertMode addv)
{
  Mat_MPIDense *A = (Mat_MPIDense*)mat->data;
  int          ierr,i,j,rstart = A->rstart,rend = A->rend,row;
  PetscTruth   roworiented = A->roworiented;

  PetscFunctionBegin;
  for (i=0; i<m; i++) {
    if (idxm[i] < 0) continue;
    if (idxm[i] >= mat->M) SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"Row too large");
    if (idxm[i] >= rstart && idxm[i] < rend) {
      row = idxm[i] - rstart;
      if (roworiented) {
        ierr = MatSetValues(A->A,1,&row,n,idxn,v+i*n,addv);CHKERRQ(ierr);
      } else {
        for (j=0; j<n; j++) {
          if (idxn[j] < 0) continue;
          if (idxn[j] >= mat->N) SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"Column too large");
          ierr = MatSetValues(A->A,1,&row,1,&idxn[j],v+i+j*m,addv);CHKERRQ(ierr);
        }
      }
    } else {
      if (!A->donotstash) {
        if (roworiented) {
          ierr = MatStashValuesRow_Private(&mat->stash,idxm[i],n,idxn,v+i*n);CHKERRQ(ierr);
        } else {
          ierr = MatStashValuesCol_Private(&mat->stash,idxm[i],n,idxn,v+i,m);CHKERRQ(ierr);
        }
      }
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatGetValues_MPIDense"
int MatGetValues_MPIDense(Mat mat,int m,const int idxm[],int n,const int idxn[],PetscScalar v[])
{
  Mat_MPIDense *mdn = (Mat_MPIDense*)mat->data;
  int          ierr,i,j,rstart = mdn->rstart,rend = mdn->rend,row;

  PetscFunctionBegin;
  for (i=0; i<m; i++) {
    if (idxm[i] < 0) SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"Negative row");
    if (idxm[i] >= mat->M) SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"Row too large");
    if (idxm[i] >= rstart && idxm[i] < rend) {
      row = idxm[i] - rstart;
      for (j=0; j<n; j++) {
        if (idxn[j] < 0) SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"Negative column");
        if (idxn[j] >= mat->N) {
          SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"Column too large");
        }
        ierr = MatGetValues(mdn->A,1,&row,1,&idxn[j],v+i*n+j);CHKERRQ(ierr);
      }
    } else {
      SETERRQ(PETSC_ERR_SUP,"Only local values currently supported");
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatGetArray_MPIDense"
int MatGetArray_MPIDense(Mat A,PetscScalar *array[])
{
  Mat_MPIDense *a = (Mat_MPIDense*)A->data;
  int          ierr;

  PetscFunctionBegin;
  ierr = MatGetArray(a->A,array);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatGetSubMatrix_MPIDense"
static int MatGetSubMatrix_MPIDense(Mat A,IS isrow,IS iscol,int cs,MatReuse scall,Mat *B)
{
  Mat_MPIDense *mat = (Mat_MPIDense*)A->data,*newmatd;
  Mat_SeqDense *lmat = (Mat_SeqDense*)mat->A->data;
  int          i,j,ierr,*irow,*icol,rstart,rend,nrows,ncols,nlrows,nlcols;
  PetscScalar  *av,*bv,*v = lmat->v;
  Mat          newmat;

  PetscFunctionBegin;
  ierr = ISGetIndices(isrow,&irow);CHKERRQ(ierr);
  ierr = ISGetIndices(iscol,&icol);CHKERRQ(ierr);
  ierr = ISGetLocalSize(isrow,&nrows);CHKERRQ(ierr);
  ierr = ISGetLocalSize(iscol,&ncols);CHKERRQ(ierr);

  /* No parallel redistribution currently supported! Should really check each index set
     to comfirm that it is OK.  ... Currently supports only submatrix same partitioning as
     original matrix! */

  ierr = MatGetLocalSize(A,&nlrows,&nlcols);CHKERRQ(ierr);
  ierr = MatGetOwnershipRange(A,&rstart,&rend);CHKERRQ(ierr);
  
  /* Check submatrix call */
  if (scall == MAT_REUSE_MATRIX) {
    /* SETERRQ(PETSC_ERR_ARG_SIZ,"Reused submatrix wrong size"); */
    /* Really need to test rows and column sizes! */
    newmat = *B;
  } else {
    /* Create and fill new matrix */
    ierr = MatCreate(A->comm,nrows,cs,PETSC_DECIDE,ncols,&newmat);CHKERRQ(ierr);
    ierr = MatSetType(newmat,A->type_name);CHKERRQ(ierr);
    ierr = MatMPIDenseSetPreallocation(newmat,PETSC_NULL);CHKERRQ(ierr);
  }

  /* Now extract the data pointers and do the copy, column at a time */
  newmatd = (Mat_MPIDense*)newmat->data;
  bv      = ((Mat_SeqDense *)newmatd->A->data)->v;
  
  for (i=0; i<ncols; i++) {
    av = v + nlrows*icol[i];
    for (j=0; j<nrows; j++) {
      *bv++ = av[irow[j] - rstart];
    }
  }

  /* Assemble the matrices so that the correct flags are set */
  ierr = MatAssemblyBegin(newmat,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(newmat,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);

  /* Free work space */
  ierr = ISRestoreIndices(isrow,&irow);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&icol);CHKERRQ(ierr);
  *B = newmat;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatRestoreArray_MPIDense"
int MatRestoreArray_MPIDense(Mat A,PetscScalar *array[])
{
  PetscFunctionBegin;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatAssemblyBegin_MPIDense"
int MatAssemblyBegin_MPIDense(Mat mat,MatAssemblyType mode)
{ 
  Mat_MPIDense *mdn = (Mat_MPIDense*)mat->data;
  MPI_Comm     comm = mat->comm;
  int          ierr,nstash,reallocs;
  InsertMode   addv;

  PetscFunctionBegin;
  /* make sure all processors are either in INSERTMODE or ADDMODE */
  ierr = MPI_Allreduce(&mat->insertmode,&addv,1,MPI_INT,MPI_BOR,comm);CHKERRQ(ierr);
  if (addv == (ADD_VALUES|INSERT_VALUES)) { 
    SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Cannot mix adds/inserts on different procs");
  }
  mat->insertmode = addv; /* in case this processor had no cache */

  ierr = MatStashScatterBegin_Private(&mat->stash,mdn->rowners);CHKERRQ(ierr);
  ierr = MatStashGetInfo_Private(&mat->stash,&nstash,&reallocs);CHKERRQ(ierr);
  PetscLogInfo(mdn->A,"MatAssemblyBegin_MPIDense:Stash has %d entries, uses %d mallocs.\n",nstash,reallocs);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatAssemblyEnd_MPIDense"
int MatAssemblyEnd_MPIDense(Mat mat,MatAssemblyType mode)
{ 
  Mat_MPIDense *mdn=(Mat_MPIDense*)mat->data;
  int          i,n,ierr,*row,*col,flg,j,rstart,ncols;
  PetscScalar  *val;
  InsertMode   addv=mat->insertmode;

  PetscFunctionBegin;
  /*  wait on receives */
  while (1) {
    ierr = MatStashScatterGetMesg_Private(&mat->stash,&n,&row,&col,&val,&flg);CHKERRQ(ierr);
    if (!flg) break;
    
    for (i=0; i<n;) {
      /* Now identify the consecutive vals belonging to the same row */
      for (j=i,rstart=row[j]; j<n; j++) { if (row[j] != rstart) break; }
      if (j < n) ncols = j-i;
      else       ncols = n-i;
      /* Now assemble all these values with a single function call */
      ierr = MatSetValues_MPIDense(mat,1,row+i,ncols,col+i,val+i,addv);CHKERRQ(ierr);
      i = j;
    }
  }
  ierr = MatStashScatterEnd_Private(&mat->stash);CHKERRQ(ierr);
  
  ierr = MatAssemblyBegin(mdn->A,mode);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(mdn->A,mode);CHKERRQ(ierr);

  if (!mat->was_assembled && mode == MAT_FINAL_ASSEMBLY) {
    ierr = MatSetUpMultiply_MPIDense(mat);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatZeroEntries_MPIDense"
int MatZeroEntries_MPIDense(Mat A)
{
  int          ierr;
  Mat_MPIDense *l = (Mat_MPIDense*)A->data;

  PetscFunctionBegin;
  ierr = MatZeroEntries(l->A);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatGetBlockSize_MPIDense"
int MatGetBlockSize_MPIDense(Mat A,int *bs)
{
  PetscFunctionBegin;
  *bs = 1;
  PetscFunctionReturn(0);
}

/* the code does not do the diagonal entries correctly unless the 
   matrix is square and the column and row owerships are identical.
   This is a BUG. The only way to fix it seems to be to access 
   mdn->A and mdn->B directly and not through the MatZeroRows() 
   routine. 
*/
#undef __FUNCT__  
#define __FUNCT__ "MatZeroRows_MPIDense"
int MatZeroRows_MPIDense(Mat A,IS is,const PetscScalar *diag)
{
  Mat_MPIDense   *l = (Mat_MPIDense*)A->data;
  int            i,ierr,N,*rows,*owners = l->rowners,size = l->size;
  int            *nprocs,j,idx,nsends;
  int            nmax,*svalues,*starts,*owner,nrecvs,rank = l->rank;
  int            *rvalues,tag = A->tag,count,base,slen,n,*source;
  int            *lens,imdex,*lrows,*values;
  MPI_Comm       comm = A->comm;
  MPI_Request    *send_waits,*recv_waits;
  MPI_Status     recv_status,*send_status;
  IS             istmp;
  PetscTruth     found;

  PetscFunctionBegin;
  ierr = ISGetLocalSize(is,&N);CHKERRQ(ierr);
  ierr = ISGetIndices(is,&rows);CHKERRQ(ierr);

  /*  first count number of contributors to each processor */
  ierr  = PetscMalloc(2*size*sizeof(int),&nprocs);CHKERRQ(ierr);
  ierr  = PetscMemzero(nprocs,2*size*sizeof(int));CHKERRQ(ierr);
  ierr  = PetscMalloc((N+1)*sizeof(int),&owner);CHKERRQ(ierr); /* see note*/
  for (i=0; i<N; i++) {
    idx = rows[i];
    found = PETSC_FALSE;
    for (j=0; j<size; j++) {
      if (idx >= owners[j] && idx < owners[j+1]) {
        nprocs[2*j]++; nprocs[2*j+1] = 1; owner[i] = j; found = PETSC_TRUE; break;
      }
    }
    if (!found) SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"Index out of range");
  }
  nsends = 0;  for (i=0; i<size; i++) { nsends += nprocs[2*i+1];} 

  /* inform other processors of number of messages and max length*/
  ierr = PetscMaxSum(comm,nprocs,&nmax,&nrecvs);CHKERRQ(ierr);

  /* post receives:   */
  ierr = PetscMalloc((nrecvs+1)*(nmax+1)*sizeof(int),&rvalues);CHKERRQ(ierr);
  ierr = PetscMalloc((nrecvs+1)*sizeof(MPI_Request),&recv_waits);CHKERRQ(ierr);
  for (i=0; i<nrecvs; i++) {
    ierr = MPI_Irecv(rvalues+nmax*i,nmax,MPI_INT,MPI_ANY_SOURCE,tag,comm,recv_waits+i);CHKERRQ(ierr);
  }

  /* do sends:
      1) starts[i] gives the starting index in svalues for stuff going to 
         the ith processor
  */
  ierr = PetscMalloc((N+1)*sizeof(int),&svalues);CHKERRQ(ierr);
  ierr = PetscMalloc((nsends+1)*sizeof(MPI_Request),&send_waits);CHKERRQ(ierr);
  ierr = PetscMalloc((size+1)*sizeof(int),&starts);CHKERRQ(ierr);
  starts[0]  = 0; 
  for (i=1; i<size; i++) { starts[i] = starts[i-1] + nprocs[2*i-2];} 
  for (i=0; i<N; i++) {
    svalues[starts[owner[i]]++] = rows[i];
  }
  ISRestoreIndices(is,&rows);

  starts[0] = 0;
  for (i=1; i<size+1; i++) { starts[i] = starts[i-1] + nprocs[2*i-2];} 
  count = 0;
  for (i=0; i<size; i++) {
    if (nprocs[2*i+1]) {
      ierr = MPI_Isend(svalues+starts[i],nprocs[2*i],MPI_INT,i,tag,comm,send_waits+count++);CHKERRQ(ierr);
    }
  }
  ierr = PetscFree(starts);CHKERRQ(ierr);

  base = owners[rank];

  /*  wait on receives */
  ierr   = PetscMalloc(2*(nrecvs+1)*sizeof(int),&lens);CHKERRQ(ierr);
  source = lens + nrecvs;
  count  = nrecvs; slen = 0;
  while (count) {
    ierr = MPI_Waitany(nrecvs,recv_waits,&imdex,&recv_status);CHKERRQ(ierr);
    /* unpack receives into our local space */
    ierr = MPI_Get_count(&recv_status,MPI_INT,&n);CHKERRQ(ierr);
    source[imdex]  = recv_status.MPI_SOURCE;
    lens[imdex]  = n;
    slen += n;
    count--;
  }
  ierr = PetscFree(recv_waits);CHKERRQ(ierr);
  
  /* move the data into the send scatter */
  ierr = PetscMalloc((slen+1)*sizeof(int),&lrows);CHKERRQ(ierr);
  count = 0;
  for (i=0; i<nrecvs; i++) {
    values = rvalues + i*nmax;
    for (j=0; j<lens[i]; j++) {
      lrows[count++] = values[j] - base;
    }
  }
  ierr = PetscFree(rvalues);CHKERRQ(ierr);
  ierr = PetscFree(lens);CHKERRQ(ierr);
  ierr = PetscFree(owner);CHKERRQ(ierr);
  ierr = PetscFree(nprocs);CHKERRQ(ierr);
    
  /* actually zap the local rows */
  ierr = ISCreateGeneral(PETSC_COMM_SELF,slen,lrows,&istmp);CHKERRQ(ierr);   
  PetscLogObjectParent(A,istmp);
  ierr = PetscFree(lrows);CHKERRQ(ierr);
  ierr = MatZeroRows(l->A,istmp,diag);CHKERRQ(ierr);
  ierr = ISDestroy(istmp);CHKERRQ(ierr);

  /* wait on sends */
  if (nsends) {
    ierr = PetscMalloc(nsends*sizeof(MPI_Status),&send_status);CHKERRQ(ierr);
    ierr = MPI_Waitall(nsends,send_waits,send_status);CHKERRQ(ierr);
    ierr = PetscFree(send_status);CHKERRQ(ierr);
  }
  ierr = PetscFree(send_waits);CHKERRQ(ierr);
  ierr = PetscFree(svalues);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMult_MPIDense"
int MatMult_MPIDense(Mat mat,Vec xx,Vec yy)
{
  Mat_MPIDense *mdn = (Mat_MPIDense*)mat->data;
  int          ierr;

  PetscFunctionBegin;
  ierr = VecScatterBegin(xx,mdn->lvec,INSERT_VALUES,SCATTER_FORWARD,mdn->Mvctx);CHKERRQ(ierr);
  ierr = VecScatterEnd(xx,mdn->lvec,INSERT_VALUES,SCATTER_FORWARD,mdn->Mvctx);CHKERRQ(ierr);
  ierr = MatMult_SeqDense(mdn->A,mdn->lvec,yy);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMultAdd_MPIDense"
int MatMultAdd_MPIDense(Mat mat,Vec xx,Vec yy,Vec zz)
{
  Mat_MPIDense *mdn = (Mat_MPIDense*)mat->data;
  int          ierr;

  PetscFunctionBegin;
  ierr = VecScatterBegin(xx,mdn->lvec,INSERT_VALUES,SCATTER_FORWARD,mdn->Mvctx);CHKERRQ(ierr);
  ierr = VecScatterEnd(xx,mdn->lvec,INSERT_VALUES,SCATTER_FORWARD,mdn->Mvctx);CHKERRQ(ierr);
  ierr = MatMultAdd_SeqDense(mdn->A,mdn->lvec,yy,zz);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMultTranspose_MPIDense"
int MatMultTranspose_MPIDense(Mat A,Vec xx,Vec yy)
{
  Mat_MPIDense *a = (Mat_MPIDense*)A->data;
  int          ierr;
  PetscScalar  zero = 0.0;

  PetscFunctionBegin;
  ierr = VecSet(&zero,yy);CHKERRQ(ierr);
  ierr = MatMultTranspose_SeqDense(a->A,xx,a->lvec);CHKERRQ(ierr);
  ierr = VecScatterBegin(a->lvec,yy,ADD_VALUES,SCATTER_REVERSE,a->Mvctx);CHKERRQ(ierr);
  ierr = VecScatterEnd(a->lvec,yy,ADD_VALUES,SCATTER_REVERSE,a->Mvctx);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMultTransposeAdd_MPIDense"
int MatMultTransposeAdd_MPIDense(Mat A,Vec xx,Vec yy,Vec zz)
{
  Mat_MPIDense *a = (Mat_MPIDense*)A->data;
  int          ierr;

  PetscFunctionBegin;
  ierr = VecCopy(yy,zz);CHKERRQ(ierr);
  ierr = MatMultTranspose_SeqDense(a->A,xx,a->lvec);CHKERRQ(ierr);
  ierr = VecScatterBegin(a->lvec,zz,ADD_VALUES,SCATTER_REVERSE,a->Mvctx);CHKERRQ(ierr);
  ierr = VecScatterEnd(a->lvec,zz,ADD_VALUES,SCATTER_REVERSE,a->Mvctx);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatGetDiagonal_MPIDense"
int MatGetDiagonal_MPIDense(Mat A,Vec v)
{
  Mat_MPIDense *a = (Mat_MPIDense*)A->data;
  Mat_SeqDense *aloc = (Mat_SeqDense*)a->A->data;
  int          ierr,len,i,n,m = A->m,radd;
  PetscScalar  *x,zero = 0.0;
  
  PetscFunctionBegin;
  ierr = VecSet(&zero,v);CHKERRQ(ierr);
  ierr = VecGetArray(v,&x);CHKERRQ(ierr);
  ierr = VecGetSize(v,&n);CHKERRQ(ierr);
  if (n != A->M) SETERRQ(PETSC_ERR_ARG_SIZ,"Nonconforming mat and vec");
  len  = PetscMin(a->A->m,a->A->n);
  radd = a->rstart*m;
  for (i=0; i<len; i++) {
    x[i] = aloc->v[radd + i*m + i];
  }
  ierr = VecRestoreArray(v,&x);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatDestroy_MPIDense"
int MatDestroy_MPIDense(Mat mat)
{
  Mat_MPIDense *mdn = (Mat_MPIDense*)mat->data;
  int          ierr;

  PetscFunctionBegin;

#if defined(PETSC_USE_LOG)
  PetscLogObjectState((PetscObject)mat,"Rows=%d, Cols=%d",mat->M,mat->N);
#endif
  ierr = MatStashDestroy_Private(&mat->stash);CHKERRQ(ierr);
  ierr = PetscFree(mdn->rowners);CHKERRQ(ierr);
  ierr = MatDestroy(mdn->A);CHKERRQ(ierr);
  if (mdn->lvec)   VecDestroy(mdn->lvec);
  if (mdn->Mvctx)  VecScatterDestroy(mdn->Mvctx);
  if (mdn->factor) {
    if (mdn->factor->temp)   {ierr = PetscFree(mdn->factor->temp);CHKERRQ(ierr);}
    if (mdn->factor->tag)    {ierr = PetscFree(mdn->factor->tag);CHKERRQ(ierr);}
    if (mdn->factor->pivots) {ierr = PetscFree(mdn->factor->pivots);CHKERRQ(ierr);}
    ierr = PetscFree(mdn->factor);CHKERRQ(ierr);
  }
  ierr = PetscFree(mdn);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatView_MPIDense_Binary"
static int MatView_MPIDense_Binary(Mat mat,PetscViewer viewer)
{
  Mat_MPIDense *mdn = (Mat_MPIDense*)mat->data;
  int          ierr;

  PetscFunctionBegin;
  if (mdn->size == 1) {
    ierr = MatView(mdn->A,viewer);CHKERRQ(ierr);
  }
  else SETERRQ(PETSC_ERR_SUP,"Only uniprocessor output supported");
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatView_MPIDense_ASCIIorDraworSocket"
static int MatView_MPIDense_ASCIIorDraworSocket(Mat mat,PetscViewer viewer)
{
  Mat_MPIDense      *mdn = (Mat_MPIDense*)mat->data;
  int               ierr,size = mdn->size,rank = mdn->rank; 
  PetscViewerType   vtype;
  PetscTruth        iascii,isdraw;
  PetscViewer       sviewer;
  PetscViewerFormat format;

  PetscFunctionBegin;
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_ASCII,&iascii);CHKERRQ(ierr);
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_DRAW,&isdraw);CHKERRQ(ierr);
  if (iascii) {
    ierr = PetscViewerGetType(viewer,&vtype);CHKERRQ(ierr);
    ierr = PetscViewerGetFormat(viewer,&format);CHKERRQ(ierr);
    if (format == PETSC_VIEWER_ASCII_INFO_DETAIL) {
      MatInfo info;
      ierr = MatGetInfo(mat,MAT_LOCAL,&info);CHKERRQ(ierr);
      ierr = PetscViewerASCIISynchronizedPrintf(viewer,"  [%d] local rows %d nz %d nz alloced %d mem %d \n",rank,mat->m,
                   (int)info.nz_used,(int)info.nz_allocated,(int)info.memory);CHKERRQ(ierr);       
      ierr = PetscViewerFlush(viewer);CHKERRQ(ierr);
      ierr = VecScatterView(mdn->Mvctx,viewer);CHKERRQ(ierr);
      PetscFunctionReturn(0); 
    } else if (format == PETSC_VIEWER_ASCII_INFO) {
      PetscFunctionReturn(0);
    }
  } else if (isdraw) {
    PetscDraw       draw;
    PetscTruth isnull;

    ierr = PetscViewerDrawGetDraw(viewer,0,&draw);CHKERRQ(ierr);
    ierr = PetscDrawIsNull(draw,&isnull);CHKERRQ(ierr);
    if (isnull) PetscFunctionReturn(0);
  }

  if (size == 1) { 
    ierr = MatView(mdn->A,viewer);CHKERRQ(ierr);
  } else {
    /* assemble the entire matrix onto first processor. */
    Mat               A;
    int               M = mat->M,N = mat->N,m,row,i,nz;
    const int         *cols;
    const PetscScalar *vals;

    if (!rank) {
      ierr = MatCreate(mat->comm,M,N,M,N,&A);CHKERRQ(ierr);
    } else {
      ierr = MatCreate(mat->comm,0,0,M,N,&A);CHKERRQ(ierr);
    }
    /* Since this is a temporary matrix, MATMPIDENSE instead of A->type_name here is probably acceptable. */
    ierr = MatSetType(A,MATMPIDENSE);CHKERRQ(ierr);
    ierr = MatMPIDenseSetPreallocation(A,PETSC_NULL);
    PetscLogObjectParent(mat,A);

    /* Copy the matrix ... This isn't the most efficient means,
       but it's quick for now */
    row = mdn->rstart; m = mdn->A->m;
    for (i=0; i<m; i++) {
      ierr = MatGetRow(mat,row,&nz,&cols,&vals);CHKERRQ(ierr);
      ierr = MatSetValues(A,1,&row,nz,cols,vals,INSERT_VALUES);CHKERRQ(ierr);
      ierr = MatRestoreRow(mat,row,&nz,&cols,&vals);CHKERRQ(ierr);
      row++;
    } 

    ierr = MatAssemblyBegin(A,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
    ierr = MatAssemblyEnd(A,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
    ierr = PetscViewerGetSingleton(viewer,&sviewer);CHKERRQ(ierr);
    if (!rank) {
      ierr = MatView(((Mat_MPIDense*)(A->data))->A,sviewer);CHKERRQ(ierr);
    }
    ierr = PetscViewerRestoreSingleton(viewer,&sviewer);CHKERRQ(ierr);
    ierr = PetscViewerFlush(viewer);CHKERRQ(ierr);
    ierr = MatDestroy(A);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatView_MPIDense"
int MatView_MPIDense(Mat mat,PetscViewer viewer)
{
  int        ierr;
  PetscTruth iascii,isbinary,isdraw,issocket;
 
  PetscFunctionBegin;
  
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_ASCII,&iascii);CHKERRQ(ierr);
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_BINARY,&isbinary);CHKERRQ(ierr);
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_SOCKET,&issocket);CHKERRQ(ierr);
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_DRAW,&isdraw);CHKERRQ(ierr);

  if (iascii || issocket || isdraw) {
    ierr = MatView_MPIDense_ASCIIorDraworSocket(mat,viewer);CHKERRQ(ierr);
  } else if (isbinary) {
    ierr = MatView_MPIDense_Binary(mat,viewer);CHKERRQ(ierr);
  } else {
    SETERRQ1(1,"Viewer type %s not supported by MPI dense matrix",((PetscObject)viewer)->type_name);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatGetInfo_MPIDense"
int MatGetInfo_MPIDense(Mat A,MatInfoType flag,MatInfo *info)
{
  Mat_MPIDense *mat = (Mat_MPIDense*)A->data;
  Mat          mdn = mat->A;
  int          ierr;
  PetscReal    isend[5],irecv[5];

  PetscFunctionBegin;
  info->rows_global    = (double)A->M;
  info->columns_global = (double)A->N;
  info->rows_local     = (double)A->m;
  info->columns_local  = (double)A->N;
  info->block_size     = 1.0;
  ierr = MatGetInfo(mdn,MAT_LOCAL,info);CHKERRQ(ierr);
  isend[0] = info->nz_used; isend[1] = info->nz_allocated; isend[2] = info->nz_unneeded;
  isend[3] = info->memory;  isend[4] = info->mallocs;
  if (flag == MAT_LOCAL) {
    info->nz_used      = isend[0];
    info->nz_allocated = isend[1];
    info->nz_unneeded  = isend[2];
    info->memory       = isend[3];
    info->mallocs      = isend[4];
  } else if (flag == MAT_GLOBAL_MAX) {
    ierr = MPI_Allreduce(isend,irecv,5,MPIU_REAL,MPI_MAX,A->comm);CHKERRQ(ierr);
    info->nz_used      = irecv[0];
    info->nz_allocated = irecv[1];
    info->nz_unneeded  = irecv[2];
    info->memory       = irecv[3];
    info->mallocs      = irecv[4];
  } else if (flag == MAT_GLOBAL_SUM) {
    ierr = MPI_Allreduce(isend,irecv,5,MPIU_REAL,MPI_SUM,A->comm);CHKERRQ(ierr);
    info->nz_used      = irecv[0];
    info->nz_allocated = irecv[1];
    info->nz_unneeded  = irecv[2];
    info->memory       = irecv[3];
    info->mallocs      = irecv[4];
  }
  info->fill_ratio_given  = 0; /* no parallel LU/ILU/Cholesky */
  info->fill_ratio_needed = 0;
  info->factor_mallocs    = 0;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSetOption_MPIDense"
int MatSetOption_MPIDense(Mat A,MatOption op)
{
  Mat_MPIDense *a = (Mat_MPIDense*)A->data;
  int          ierr;

  PetscFunctionBegin;
  switch (op) {
  case MAT_NO_NEW_NONZERO_LOCATIONS:
  case MAT_YES_NEW_NONZERO_LOCATIONS:
  case MAT_NEW_NONZERO_LOCATION_ERR:
  case MAT_NEW_NONZERO_ALLOCATION_ERR:
  case MAT_COLUMNS_SORTED:
  case MAT_COLUMNS_UNSORTED:
    ierr = MatSetOption(a->A,op);CHKERRQ(ierr);
    break;
  case MAT_ROW_ORIENTED:
    a->roworiented = PETSC_TRUE;
    ierr = MatSetOption(a->A,op);CHKERRQ(ierr);
    break;
  case MAT_ROWS_SORTED: 
  case MAT_ROWS_UNSORTED:
  case MAT_YES_NEW_DIAGONALS:
  case MAT_USE_HASH_TABLE:
    PetscLogInfo(A,"MatSetOption_MPIDense:Option ignored\n");
    break;
  case MAT_COLUMN_ORIENTED:
    a->roworiented = PETSC_FALSE;
    ierr = MatSetOption(a->A,op);CHKERRQ(ierr);
    break;
  case MAT_IGNORE_OFF_PROC_ENTRIES:
    a->donotstash = PETSC_TRUE;
    break;
  case MAT_NO_NEW_DIAGONALS:
    SETERRQ(PETSC_ERR_SUP,"MAT_NO_NEW_DIAGONALS");
  case MAT_SYMMETRIC:
  case MAT_STRUCTURALLY_SYMMETRIC:
  case MAT_NOT_SYMMETRIC:
  case MAT_NOT_STRUCTURALLY_SYMMETRIC:
  case MAT_HERMITIAN:
  case MAT_NOT_HERMITIAN:
  case MAT_SYMMETRY_ETERNAL:
  case MAT_NOT_SYMMETRY_ETERNAL:
    break;
  default:
    SETERRQ(PETSC_ERR_SUP,"unknown option");
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatGetRow_MPIDense"
int MatGetRow_MPIDense(Mat A,int row,int *nz,int **idx,PetscScalar **v)
{
  Mat_MPIDense *mat = (Mat_MPIDense*)A->data;
  int          lrow,rstart = mat->rstart,rend = mat->rend,ierr;

  PetscFunctionBegin;
  if (row < rstart || row >= rend) SETERRQ(PETSC_ERR_SUP,"only local rows")
  lrow = row - rstart;
  ierr = MatGetRow(mat->A,lrow,nz,(const int **)idx,(const PetscScalar **)v);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatRestoreRow_MPIDense"
int MatRestoreRow_MPIDense(Mat mat,int row,int *nz,int **idx,PetscScalar **v)
{
  int ierr;

  PetscFunctionBegin;
  if (idx) {ierr = PetscFree(*idx);CHKERRQ(ierr);}
  if (v) {ierr = PetscFree(*v);CHKERRQ(ierr);}
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatDiagonalScale_MPIDense"
int MatDiagonalScale_MPIDense(Mat A,Vec ll,Vec rr)
{
  Mat_MPIDense *mdn = (Mat_MPIDense*)A->data;
  Mat_SeqDense *mat = (Mat_SeqDense*)mdn->A->data;
  PetscScalar  *l,*r,x,*v;
  int          ierr,i,j,s2a,s3a,s2,s3,m=mdn->A->m,n=mdn->A->n;

  PetscFunctionBegin;
  ierr = MatGetLocalSize(A,&s2,&s3);CHKERRQ(ierr);
  if (ll) {
    ierr = VecGetLocalSize(ll,&s2a);CHKERRQ(ierr);
    if (s2a != s2) SETERRQ(PETSC_ERR_ARG_SIZ,"Left scaling vector non-conforming local size");
    ierr = VecGetArray(ll,&l);CHKERRQ(ierr);
    for (i=0; i<m; i++) {
      x = l[i];
      v = mat->v + i;
      for (j=0; j<n; j++) { (*v) *= x; v+= m;} 
    }
    ierr = VecRestoreArray(ll,&l);CHKERRQ(ierr);
    PetscLogFlops(n*m);
  }
  if (rr) {
    ierr = VecGetSize(rr,&s3a);CHKERRQ(ierr);
    if (s3a != s3) SETERRQ(PETSC_ERR_ARG_SIZ,"Right scaling vec non-conforming local size");
    ierr = VecScatterBegin(rr,mdn->lvec,INSERT_VALUES,SCATTER_FORWARD,mdn->Mvctx);CHKERRQ(ierr);
    ierr = VecScatterEnd(rr,mdn->lvec,INSERT_VALUES,SCATTER_FORWARD,mdn->Mvctx);CHKERRQ(ierr);
    ierr = VecGetArray(mdn->lvec,&r);CHKERRQ(ierr);
    for (i=0; i<n; i++) {
      x = r[i];
      v = mat->v + i*m;
      for (j=0; j<m; j++) { (*v++) *= x;} 
    }
    ierr = VecRestoreArray(mdn->lvec,&r);CHKERRQ(ierr);
    PetscLogFlops(n*m);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatNorm_MPIDense"
int MatNorm_MPIDense(Mat A,NormType type,PetscReal *nrm)
{
  Mat_MPIDense *mdn = (Mat_MPIDense*)A->data;
  Mat_SeqDense *mat = (Mat_SeqDense*)mdn->A->data;
  int          ierr,i,j;
  PetscReal    sum = 0.0;
  PetscScalar  *v = mat->v;

  PetscFunctionBegin;
  if (mdn->size == 1) {
    ierr =  MatNorm(mdn->A,type,nrm);CHKERRQ(ierr);
  } else {
    if (type == NORM_FROBENIUS) {
      for (i=0; i<mdn->A->n*mdn->A->m; i++) {
#if defined(PETSC_USE_COMPLEX)
        sum += PetscRealPart(PetscConj(*v)*(*v)); v++;
#else
        sum += (*v)*(*v); v++;
#endif
      }
      ierr = MPI_Allreduce(&sum,nrm,1,MPIU_REAL,MPI_SUM,A->comm);CHKERRQ(ierr);
      *nrm = sqrt(*nrm);
      PetscLogFlops(2*mdn->A->n*mdn->A->m);
    } else if (type == NORM_1) { 
      PetscReal *tmp,*tmp2;
      ierr = PetscMalloc(2*A->N*sizeof(PetscReal),&tmp);CHKERRQ(ierr);
      tmp2 = tmp + A->N;
      ierr = PetscMemzero(tmp,2*A->N*sizeof(PetscReal));CHKERRQ(ierr);
      *nrm = 0.0;
      v = mat->v;
      for (j=0; j<mdn->A->n; j++) {
        for (i=0; i<mdn->A->m; i++) {
          tmp[j] += PetscAbsScalar(*v);  v++;
        }
      }
      ierr = MPI_Allreduce(tmp,tmp2,A->N,MPIU_REAL,MPI_SUM,A->comm);CHKERRQ(ierr);
      for (j=0; j<A->N; j++) {
        if (tmp2[j] > *nrm) *nrm = tmp2[j];
      }
      ierr = PetscFree(tmp);CHKERRQ(ierr);
      PetscLogFlops(A->n*A->m);
    } else if (type == NORM_INFINITY) { /* max row norm */
      PetscReal ntemp;
      ierr = MatNorm(mdn->A,type,&ntemp);CHKERRQ(ierr);
      ierr = MPI_Allreduce(&ntemp,nrm,1,MPIU_REAL,MPI_MAX,A->comm);CHKERRQ(ierr);
    } else {
      SETERRQ(PETSC_ERR_SUP,"No support for two norm");
    }
  }
  PetscFunctionReturn(0); 
}

#undef __FUNCT__  
#define __FUNCT__ "MatTranspose_MPIDense"
int MatTranspose_MPIDense(Mat A,Mat *matout)
{ 
  Mat_MPIDense *a = (Mat_MPIDense*)A->data;
  Mat_SeqDense *Aloc = (Mat_SeqDense*)a->A->data;
  Mat          B;
  int          M = A->M,N = A->N,m,n,*rwork,rstart = a->rstart;
  int          j,i,ierr;
  PetscScalar  *v;

  PetscFunctionBegin;
  if (!matout && M != N) {
    SETERRQ(PETSC_ERR_SUP,"Supports square matrix only in-place");
  }
  ierr = MatCreate(A->comm,PETSC_DECIDE,PETSC_DECIDE,N,M,&B);CHKERRQ(ierr);
  ierr = MatSetType(B,A->type_name);CHKERRQ(ierr);
  ierr = MatMPIDenseSetPreallocation(B,PETSC_NULL);CHKERRQ(ierr);

  m = a->A->m; n = a->A->n; v = Aloc->v;
  ierr = PetscMalloc(n*sizeof(int),&rwork);CHKERRQ(ierr);
  for (j=0; j<n; j++) {
    for (i=0; i<m; i++) rwork[i] = rstart + i;
    ierr = MatSetValues(B,1,&j,m,rwork,v,INSERT_VALUES);CHKERRQ(ierr);
    v   += m;
  } 
  ierr = PetscFree(rwork);CHKERRQ(ierr);
  ierr = MatAssemblyBegin(B,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(B,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  if (matout) {
    *matout = B;
  } else {
    ierr = MatHeaderCopy(A,B);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#include "petscblaslapack.h"
#undef __FUNCT__  
#define __FUNCT__ "MatScale_MPIDense"
int MatScale_MPIDense(const PetscScalar *alpha,Mat inA)
{
  Mat_MPIDense *A = (Mat_MPIDense*)inA->data;
  Mat_SeqDense *a = (Mat_SeqDense*)A->A->data;
  int          one = 1,nz;

  PetscFunctionBegin;
  nz = inA->m*inA->N;
  BLscal_(&nz,(PetscScalar*)alpha,a->v,&one);
  PetscLogFlops(nz);
  PetscFunctionReturn(0);
}

static int MatDuplicate_MPIDense(Mat,MatDuplicateOption,Mat *);

#undef __FUNCT__  
#define __FUNCT__ "MatSetUpPreallocation_MPIDense"
int MatSetUpPreallocation_MPIDense(Mat A)
{
  int        ierr;

  PetscFunctionBegin;
  ierr =  MatMPIDenseSetPreallocation(A,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* -------------------------------------------------------------------*/
static struct _MatOps MatOps_Values = {MatSetValues_MPIDense,
       MatGetRow_MPIDense,
       MatRestoreRow_MPIDense,
       MatMult_MPIDense,
/* 4*/ MatMultAdd_MPIDense,
       MatMultTranspose_MPIDense,
       MatMultTransposeAdd_MPIDense,
       0,
       0,
       0,
/*10*/ 0,
       0,
       0,
       0,
       MatTranspose_MPIDense,
/*15*/ MatGetInfo_MPIDense,
       0,
       MatGetDiagonal_MPIDense,
       MatDiagonalScale_MPIDense,
       MatNorm_MPIDense,
/*20*/ MatAssemblyBegin_MPIDense,
       MatAssemblyEnd_MPIDense,
       0,
       MatSetOption_MPIDense,
       MatZeroEntries_MPIDense,
/*25*/ MatZeroRows_MPIDense,
       0,
       0,
       0,
       0,
/*30*/ MatSetUpPreallocation_MPIDense,
       0,
       0,
       MatGetArray_MPIDense,
       MatRestoreArray_MPIDense,
/*35*/ MatDuplicate_MPIDense,
       0,
       0,
       0,
       0,
/*40*/ 0,
       MatGetSubMatrices_MPIDense,
       0,
       MatGetValues_MPIDense,
       0,
/*45*/ 0,
       MatScale_MPIDense,
       0,
       0,
       0,
/*50*/ MatGetBlockSize_MPIDense,
       0,
       0,
       0,
       0,
/*55*/ 0,
       0,
       0,
       0,
       0,
/*60*/ MatGetSubMatrix_MPIDense,
       MatDestroy_MPIDense,
       MatView_MPIDense,
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
/*85*/ MatLoad_MPIDense};

EXTERN_C_BEGIN
#undef __FUNCT__
#define __FUNCT__ "MatMPIDenseSetPreallocation_MPIDense"
int MatMPIDenseSetPreallocation_MPIDense(Mat mat,PetscScalar *data)
{
  Mat_MPIDense *a;
  int          ierr;

  PetscFunctionBegin;
  mat->preallocated = PETSC_TRUE;
  /* Note:  For now, when data is specified above, this assumes the user correctly
   allocates the local dense storage space.  We should add error checking. */

  a    = (Mat_MPIDense*)mat->data;
  ierr = MatCreate(PETSC_COMM_SELF,mat->m,mat->N,mat->m,mat->N,&a->A);CHKERRQ(ierr);
  ierr = MatSetType(a->A,MATSEQDENSE);CHKERRQ(ierr);
  ierr = MatSeqDenseSetPreallocation(a->A,data);CHKERRQ(ierr);
  PetscLogObjectParent(mat,a->A);
  PetscFunctionReturn(0);
}
EXTERN_C_END

/*MC
   MATMPIDENSE - MATMPIDENSE = "mpidense" - A matrix type to be used for distributed dense matrices.

   Options Database Keys:
. -mat_type mpidense - sets the matrix type to "mpidense" during a call to MatSetFromOptions()

  Level: beginner

.seealso: MatCreateMPIDense
M*/

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "MatCreate_MPIDense"
int MatCreate_MPIDense(Mat mat)
{
  Mat_MPIDense *a;
  int          ierr,i;

  PetscFunctionBegin;
  ierr              = PetscNew(Mat_MPIDense,&a);CHKERRQ(ierr);
  mat->data         = (void*)a;
  ierr              = PetscMemcpy(mat->ops,&MatOps_Values,sizeof(struct _MatOps));CHKERRQ(ierr);
  mat->factor       = 0;
  mat->mapping      = 0;

  a->factor       = 0;
  mat->insertmode = NOT_SET_VALUES;
  ierr = MPI_Comm_rank(mat->comm,&a->rank);CHKERRQ(ierr);
  ierr = MPI_Comm_size(mat->comm,&a->size);CHKERRQ(ierr);

  ierr = PetscSplitOwnership(mat->comm,&mat->m,&mat->M);CHKERRQ(ierr);
  ierr = PetscSplitOwnership(mat->comm,&mat->n,&mat->N);CHKERRQ(ierr);
  a->nvec = mat->n;

  /* the information in the maps duplicates the information computed below, eventually 
     we should remove the duplicate information that is not contained in the maps */
  ierr = PetscMapCreateMPI(mat->comm,mat->m,mat->M,&mat->rmap);CHKERRQ(ierr);
  ierr = PetscMapCreateMPI(mat->comm,mat->n,mat->N,&mat->cmap);CHKERRQ(ierr);

  /* build local table of row and column ownerships */
  ierr       = PetscMalloc(2*(a->size+2)*sizeof(int),&a->rowners);CHKERRQ(ierr);
  a->cowners = a->rowners + a->size + 1;
  PetscLogObjectMemory(mat,2*(a->size+2)*sizeof(int)+sizeof(struct _p_Mat)+sizeof(Mat_MPIDense));
  ierr = MPI_Allgather(&mat->m,1,MPI_INT,a->rowners+1,1,MPI_INT,mat->comm);CHKERRQ(ierr);
  a->rowners[0] = 0;
  for (i=2; i<=a->size; i++) {
    a->rowners[i] += a->rowners[i-1];
  }
  a->rstart = a->rowners[a->rank]; 
  a->rend   = a->rowners[a->rank+1]; 
  ierr      = MPI_Allgather(&mat->n,1,MPI_INT,a->cowners+1,1,MPI_INT,mat->comm);CHKERRQ(ierr);
  a->cowners[0] = 0;
  for (i=2; i<=a->size; i++) {
    a->cowners[i] += a->cowners[i-1];
  }

  /* build cache for off array entries formed */
  a->donotstash = PETSC_FALSE;
  ierr = MatStashCreate_Private(mat->comm,1,&mat->stash);CHKERRQ(ierr);

  /* stuff used for matrix vector multiply */
  a->lvec        = 0;
  a->Mvctx       = 0;
  a->roworiented = PETSC_TRUE;

  ierr = PetscObjectComposeFunctionDynamic((PetscObject)mat,"MatGetDiagonalBlock_C",
                                     "MatGetDiagonalBlock_MPIDense",
                                     MatGetDiagonalBlock_MPIDense);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)mat,"MatMPIDenseSetPreallocation_C",
                                     "MatMPIDenseSetPreallocation_MPIDense",
                                     MatMPIDenseSetPreallocation_MPIDense);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END

/*MC
   MATDENSE - MATDENSE = "dense" - A matrix type to be used for dense matrices.

   This matrix type is identical to MATSEQDENSE when constructed with a single process communicator,
   and MATMPIDENSE otherwise.

   Options Database Keys:
. -mat_type dense - sets the matrix type to "dense" during a call to MatSetFromOptions()

  Level: beginner

.seealso: MatCreateMPIDense,MATSEQDENSE,MATMPIDENSE
M*/

EXTERN_C_BEGIN
#undef __FUNCT__
#define __FUNCT__ "MatCreate_Dense"
int MatCreate_Dense(Mat A) {
  int ierr,size;

  PetscFunctionBegin;
  ierr = PetscObjectChangeTypeName((PetscObject)A,MATDENSE);CHKERRQ(ierr);
  ierr = MPI_Comm_size(A->comm,&size);CHKERRQ(ierr);
  if (size == 1) {
    ierr = MatSetType(A,MATSEQDENSE);CHKERRQ(ierr);
  } else {
    ierr = MatSetType(A,MATMPIDENSE);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "MatMPIDenseSetPreallocation"
/*@C
   MatMPIDenseSetPreallocation - Sets the array used to store the matrix entries

   Not collective

   Input Parameters:
.  A - the matrix
-  data - optional location of matrix data.  Set data=PETSC_NULL for PETSc
   to control all matrix memory allocation.

   Notes:
   The dense format is fully compatible with standard Fortran 77
   storage by columns.

   The data input variable is intended primarily for Fortran programmers
   who wish to allocate their own matrix memory space.  Most users should
   set data=PETSC_NULL.

   Level: intermediate

.keywords: matrix,dense, parallel

.seealso: MatCreate(), MatCreateSeqDense(), MatSetValues()
@*/
int MatMPIDenseSetPreallocation(Mat mat,PetscScalar *data)
{
  int ierr,(*f)(Mat,PetscScalar *);

  PetscFunctionBegin;
  ierr = PetscObjectQueryFunction((PetscObject)mat,"MatMPIDenseSetPreallocation_C",(void (**)(void))&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(mat,data);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatCreateMPIDense"
/*@C
   MatCreateMPIDense - Creates a sparse parallel matrix in dense format.

   Collective on MPI_Comm

   Input Parameters:
+  comm - MPI communicator
.  m - number of local rows (or PETSC_DECIDE to have calculated if M is given)
.  n - number of local columns (or PETSC_DECIDE to have calculated if N is given)
.  M - number of global rows (or PETSC_DECIDE to have calculated if m is given)
.  N - number of global columns (or PETSC_DECIDE to have calculated if n is given)
-  data - optional location of matrix data.  Set data=PETSC_NULL (PETSC_NULL_SCALAR for Fortran users) for PETSc
   to control all matrix memory allocation.

   Output Parameter:
.  A - the matrix

   Notes:
   The dense format is fully compatible with standard Fortran 77
   storage by columns.

   The data input variable is intended primarily for Fortran programmers
   who wish to allocate their own matrix memory space.  Most users should
   set data=PETSC_NULL (PETSC_NULL_SCALAR for Fortran users).

   The user MUST specify either the local or global matrix dimensions
   (possibly both).

   Level: intermediate

.keywords: matrix,dense, parallel

.seealso: MatCreate(), MatCreateSeqDense(), MatSetValues()
@*/
int MatCreateMPIDense(MPI_Comm comm,int m,int n,int M,int N,PetscScalar *data,Mat *A)
{
  int ierr,size;

  PetscFunctionBegin;
  ierr = MatCreate(comm,m,n,M,N,A);CHKERRQ(ierr);
  ierr = MPI_Comm_size(comm,&size);CHKERRQ(ierr);
  if (size > 1) {
    ierr = MatSetType(*A,MATMPIDENSE);CHKERRQ(ierr);
    ierr = MatMPIDenseSetPreallocation(*A,data);CHKERRQ(ierr);
  } else {
    ierr = MatSetType(*A,MATSEQDENSE);CHKERRQ(ierr);
    ierr = MatSeqDenseSetPreallocation(*A,data);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatDuplicate_MPIDense"
static int MatDuplicate_MPIDense(Mat A,MatDuplicateOption cpvalues,Mat *newmat)
{
  Mat          mat;
  Mat_MPIDense *a,*oldmat = (Mat_MPIDense*)A->data;
  int          ierr;

  PetscFunctionBegin;
  *newmat       = 0;
  ierr = MatCreate(A->comm,A->m,A->n,A->M,A->N,&mat);CHKERRQ(ierr);
  ierr = MatSetType(mat,A->type_name);CHKERRQ(ierr);
  ierr              = PetscNew(Mat_MPIDense,&a);CHKERRQ(ierr);
  mat->data         = (void*)a;
  ierr              = PetscMemcpy(mat->ops,&MatOps_Values,sizeof(struct _MatOps));CHKERRQ(ierr);
  mat->factor       = A->factor;
  mat->assembled    = PETSC_TRUE;
  mat->preallocated = PETSC_TRUE;

  a->rstart       = oldmat->rstart;
  a->rend         = oldmat->rend;
  a->size         = oldmat->size;
  a->rank         = oldmat->rank;
  mat->insertmode = NOT_SET_VALUES;
  a->nvec         = oldmat->nvec;
  a->donotstash   = oldmat->donotstash;
  ierr            = PetscMalloc((a->size+1)*sizeof(int),&a->rowners);CHKERRQ(ierr);
  PetscLogObjectMemory(mat,(a->size+1)*sizeof(int)+sizeof(struct _p_Mat)+sizeof(Mat_MPIDense));
  ierr = PetscMemcpy(a->rowners,oldmat->rowners,(a->size+1)*sizeof(int));CHKERRQ(ierr);
  ierr = MatStashCreate_Private(A->comm,1,&mat->stash);CHKERRQ(ierr);

  ierr = MatSetUpMultiply_MPIDense(mat);CHKERRQ(ierr);
  ierr = MatDuplicate(oldmat->A,cpvalues,&a->A);CHKERRQ(ierr);
  PetscLogObjectParent(mat,a->A);
  *newmat = mat;
  PetscFunctionReturn(0);
}

#include "petscsys.h"

#undef __FUNCT__  
#define __FUNCT__ "MatLoad_MPIDense_DenseInFile"
int MatLoad_MPIDense_DenseInFile(MPI_Comm comm,int fd,int M,int N,const MatType type,Mat *newmat)
{
  int          *rowners,i,size,rank,m,ierr,nz,j;
  PetscScalar  *array,*vals,*vals_ptr;
  MPI_Status   status;

  PetscFunctionBegin;
  ierr = MPI_Comm_rank(comm,&rank);CHKERRQ(ierr);
  ierr = MPI_Comm_size(comm,&size);CHKERRQ(ierr);

  /* determine ownership of all rows */
  m          = M/size + ((M % size) > rank);
  ierr       = PetscMalloc((size+2)*sizeof(int),&rowners);CHKERRQ(ierr);
  ierr       = MPI_Allgather(&m,1,MPI_INT,rowners+1,1,MPI_INT,comm);CHKERRQ(ierr);
  rowners[0] = 0;
  for (i=2; i<=size; i++) {
    rowners[i] += rowners[i-1];
  }

  ierr = MatCreate(comm,m,PETSC_DECIDE,M,N,newmat);CHKERRQ(ierr);
  ierr = MatSetType(*newmat,type);CHKERRQ(ierr);
  ierr = MatMPIDenseSetPreallocation(*newmat,PETSC_NULL);CHKERRQ(ierr);
  ierr = MatGetArray(*newmat,&array);CHKERRQ(ierr);

  if (!rank) {
    ierr = PetscMalloc(m*N*sizeof(PetscScalar),&vals);CHKERRQ(ierr);

    /* read in my part of the matrix numerical values  */
    ierr = PetscBinaryRead(fd,vals,m*N,PETSC_SCALAR);CHKERRQ(ierr);
    
    /* insert into matrix-by row (this is why cannot directly read into array */
    vals_ptr = vals;
    for (i=0; i<m; i++) {
      for (j=0; j<N; j++) {
        array[i + j*m] = *vals_ptr++;
      }
    }

    /* read in other processors and ship out */
    for (i=1; i<size; i++) {
      nz   = (rowners[i+1] - rowners[i])*N;
      ierr = PetscBinaryRead(fd,vals,nz,PETSC_SCALAR);CHKERRQ(ierr);
      ierr = MPI_Send(vals,nz,MPIU_SCALAR,i,(*newmat)->tag,comm);CHKERRQ(ierr);
    }
  } else {
    /* receive numeric values */
    ierr = PetscMalloc(m*N*sizeof(PetscScalar),&vals);CHKERRQ(ierr);

    /* receive message of values*/
    ierr = MPI_Recv(vals,m*N,MPIU_SCALAR,0,(*newmat)->tag,comm,&status);CHKERRQ(ierr);

    /* insert into matrix-by row (this is why cannot directly read into array */
    vals_ptr = vals;
    for (i=0; i<m; i++) {
      for (j=0; j<N; j++) {
        array[i + j*m] = *vals_ptr++;
      }
    }
  }
  ierr = PetscFree(rowners);CHKERRQ(ierr);
  ierr = PetscFree(vals);CHKERRQ(ierr);
  ierr = MatAssemblyBegin(*newmat,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(*newmat,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatLoad_MPIDense"
int MatLoad_MPIDense(PetscViewer viewer,const MatType type,Mat *newmat)
{
  Mat          A;
  PetscScalar  *vals,*svals;
  MPI_Comm     comm = ((PetscObject)viewer)->comm;
  MPI_Status   status;
  int          header[4],rank,size,*rowlengths = 0,M,N,m,*rowners,maxnz,*cols;
  int          *ourlens,*sndcounts = 0,*procsnz = 0,*offlens,jj,*mycols,*smycols;
  int          tag = ((PetscObject)viewer)->tag;
  int          i,nz,ierr,j,rstart,rend,fd;

  PetscFunctionBegin;
  ierr = MPI_Comm_size(comm,&size);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(comm,&rank);CHKERRQ(ierr);
  if (!rank) {
    ierr = PetscViewerBinaryGetDescriptor(viewer,&fd);CHKERRQ(ierr);
    ierr = PetscBinaryRead(fd,(char *)header,4,PETSC_INT);CHKERRQ(ierr);
    if (header[0] != MAT_FILE_COOKIE) SETERRQ(PETSC_ERR_FILE_UNEXPECTED,"not matrix object");
  }

  ierr = MPI_Bcast(header+1,3,MPI_INT,0,comm);CHKERRQ(ierr);
  M = header[1]; N = header[2]; nz = header[3];

  /*
       Handle case where matrix is stored on disk as a dense matrix 
  */
  if (nz == MATRIX_BINARY_FORMAT_DENSE) {
    ierr = MatLoad_MPIDense_DenseInFile(comm,fd,M,N,type,newmat);CHKERRQ(ierr);
    PetscFunctionReturn(0);
  }

  /* determine ownership of all rows */
  m          = M/size + ((M % size) > rank);
  ierr       = PetscMalloc((size+2)*sizeof(int),&rowners);CHKERRQ(ierr);
  ierr       = MPI_Allgather(&m,1,MPI_INT,rowners+1,1,MPI_INT,comm);CHKERRQ(ierr);
  rowners[0] = 0;
  for (i=2; i<=size; i++) {
    rowners[i] += rowners[i-1];
  }
  rstart = rowners[rank]; 
  rend   = rowners[rank+1]; 

  /* distribute row lengths to all processors */
  ierr    = PetscMalloc(2*(rend-rstart+1)*sizeof(int),&ourlens);CHKERRQ(ierr);
  offlens = ourlens + (rend-rstart);
  if (!rank) {
    ierr = PetscMalloc(M*sizeof(int),&rowlengths);CHKERRQ(ierr);
    ierr = PetscBinaryRead(fd,rowlengths,M,PETSC_INT);CHKERRQ(ierr);
    ierr = PetscMalloc(size*sizeof(int),&sndcounts);CHKERRQ(ierr);
    for (i=0; i<size; i++) sndcounts[i] = rowners[i+1] - rowners[i];
    ierr = MPI_Scatterv(rowlengths,sndcounts,rowners,MPI_INT,ourlens,rend-rstart,MPI_INT,0,comm);CHKERRQ(ierr);
    ierr = PetscFree(sndcounts);CHKERRQ(ierr);
  } else {
    ierr = MPI_Scatterv(0,0,0,MPI_INT,ourlens,rend-rstart,MPI_INT,0,comm);CHKERRQ(ierr);
  }

  if (!rank) {
    /* calculate the number of nonzeros on each processor */
    ierr = PetscMalloc(size*sizeof(int),&procsnz);CHKERRQ(ierr);
    ierr = PetscMemzero(procsnz,size*sizeof(int));CHKERRQ(ierr);
    for (i=0; i<size; i++) {
      for (j=rowners[i]; j< rowners[i+1]; j++) {
        procsnz[i] += rowlengths[j];
      }
    }
    ierr = PetscFree(rowlengths);CHKERRQ(ierr);

    /* determine max buffer needed and allocate it */
    maxnz = 0;
    for (i=0; i<size; i++) {
      maxnz = PetscMax(maxnz,procsnz[i]);
    }
    ierr = PetscMalloc(maxnz*sizeof(int),&cols);CHKERRQ(ierr);

    /* read in my part of the matrix column indices  */
    nz = procsnz[0];
    ierr = PetscMalloc(nz*sizeof(int),&mycols);CHKERRQ(ierr);
    ierr = PetscBinaryRead(fd,mycols,nz,PETSC_INT);CHKERRQ(ierr);

    /* read in every one elses and ship off */
    for (i=1; i<size; i++) {
      nz   = procsnz[i];
      ierr = PetscBinaryRead(fd,cols,nz,PETSC_INT);CHKERRQ(ierr);
      ierr = MPI_Send(cols,nz,MPI_INT,i,tag,comm);CHKERRQ(ierr);
    }
    ierr = PetscFree(cols);CHKERRQ(ierr);
  } else {
    /* determine buffer space needed for message */
    nz = 0;
    for (i=0; i<m; i++) {
      nz += ourlens[i];
    }
    ierr = PetscMalloc((nz+1)*sizeof(int),&mycols);CHKERRQ(ierr);

    /* receive message of column indices*/
    ierr = MPI_Recv(mycols,nz,MPI_INT,0,tag,comm,&status);CHKERRQ(ierr);
    ierr = MPI_Get_count(&status,MPI_INT,&maxnz);CHKERRQ(ierr);
    if (maxnz != nz) SETERRQ(PETSC_ERR_FILE_UNEXPECTED,"something is wrong with file");
  }

  /* loop over local rows, determining number of off diagonal entries */
  ierr = PetscMemzero(offlens,m*sizeof(int));CHKERRQ(ierr);
  jj = 0;
  for (i=0; i<m; i++) {
    for (j=0; j<ourlens[i]; j++) {
      if (mycols[jj] < rstart || mycols[jj] >= rend) offlens[i]++;
      jj++;
    }
  }

  /* create our matrix */
  for (i=0; i<m; i++) {
    ourlens[i] -= offlens[i];
  }
  ierr = MatCreate(comm,m,PETSC_DECIDE,M,N,newmat);CHKERRQ(ierr);
  ierr = MatSetType(*newmat,type);CHKERRQ(ierr);
  ierr = MatMPIDenseSetPreallocation(*newmat,PETSC_NULL);CHKERRQ(ierr);
  A = *newmat;
  for (i=0; i<m; i++) {
    ourlens[i] += offlens[i];
  }

  if (!rank) {
    ierr = PetscMalloc(maxnz*sizeof(PetscScalar),&vals);CHKERRQ(ierr);

    /* read in my part of the matrix numerical values  */
    nz = procsnz[0];
    ierr = PetscBinaryRead(fd,vals,nz,PETSC_SCALAR);CHKERRQ(ierr);
    
    /* insert into matrix */
    jj      = rstart;
    smycols = mycols;
    svals   = vals;
    for (i=0; i<m; i++) {
      ierr = MatSetValues(A,1,&jj,ourlens[i],smycols,svals,INSERT_VALUES);CHKERRQ(ierr);
      smycols += ourlens[i];
      svals   += ourlens[i];
      jj++;
    }

    /* read in other processors and ship out */
    for (i=1; i<size; i++) {
      nz   = procsnz[i];
      ierr = PetscBinaryRead(fd,vals,nz,PETSC_SCALAR);CHKERRQ(ierr);
      ierr = MPI_Send(vals,nz,MPIU_SCALAR,i,A->tag,comm);CHKERRQ(ierr);
    }
    ierr = PetscFree(procsnz);CHKERRQ(ierr);
  } else {
    /* receive numeric values */
    ierr = PetscMalloc((nz+1)*sizeof(PetscScalar),&vals);CHKERRQ(ierr);

    /* receive message of values*/
    ierr = MPI_Recv(vals,nz,MPIU_SCALAR,0,A->tag,comm,&status);CHKERRQ(ierr);
    ierr = MPI_Get_count(&status,MPIU_SCALAR,&maxnz);CHKERRQ(ierr);
    if (maxnz != nz) SETERRQ(PETSC_ERR_FILE_UNEXPECTED,"something is wrong with file");

    /* insert into matrix */
    jj      = rstart;
    smycols = mycols;
    svals   = vals;
    for (i=0; i<m; i++) {
      ierr = MatSetValues(A,1,&jj,ourlens[i],smycols,svals,INSERT_VALUES);CHKERRQ(ierr);
      smycols += ourlens[i];
      svals   += ourlens[i];
      jj++;
    }
  }
  ierr = PetscFree(ourlens);CHKERRQ(ierr);
  ierr = PetscFree(vals);CHKERRQ(ierr);
  ierr = PetscFree(mycols);CHKERRQ(ierr);
  ierr = PetscFree(rowners);CHKERRQ(ierr);

  ierr = MatAssemblyBegin(A,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(A,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}





