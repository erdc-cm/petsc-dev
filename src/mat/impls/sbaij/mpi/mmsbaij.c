/*$Id: mmsbaij.c,v 1.10 2001/08/07 03:03:05 balay Exp $*/

/*
   Support for the parallel SBAIJ matrix vector multiply
*/
#include "src/mat/impls/sbaij/mpi/mpisbaij.h"

extern int MatSetValues_SeqSBAIJ(Mat,int,const int [],int,const int [],const PetscScalar [],InsertMode);


#undef __FUNCT__  
#define __FUNCT__ "MatSetUpMultiply_MPISBAIJ"
int MatSetUpMultiply_MPISBAIJ(Mat mat)
{
  Mat_MPISBAIJ       *sbaij = (Mat_MPISBAIJ*)mat->data;
  Mat_SeqBAIJ        *B = (Mat_SeqBAIJ*)(sbaij->B->data);  
  int                Nbs = sbaij->Nbs,i,j,*indices,*aj = B->j,ierr,ec = 0,*garray,*sgarray;
  int                bs = sbaij->bs,*stmp,mbs=sbaij->mbs, vec_size,nt;
  IS                 from,to;
  Vec                gvec;
  int                rank=sbaij->rank,lsize,size=sbaij->size; 
  int                *owners=sbaij->rowners,*sowners,*ec_owner,k; 
  PetscMap           vecmap;
  PetscScalar        *ptr;

  PetscFunctionBegin;
  if (sbaij->lvec) {
    ierr = VecDestroy(sbaij->lvec);CHKERRQ(ierr);
    sbaij->lvec = 0;
  }
  if (sbaij->Mvctx) {
    ierr = VecScatterDestroy(sbaij->Mvctx);CHKERRQ(ierr);
    sbaij->Mvctx = 0;
  }

  /* For the first stab we make an array as long as the number of columns */
  /* mark those columns that are in sbaij->B */
  ierr = PetscMalloc((Nbs+1)*sizeof(int),&indices);CHKERRQ(ierr);
  ierr = PetscMemzero(indices,Nbs*sizeof(int));CHKERRQ(ierr);
  for (i=0; i<mbs; i++) {
    for (j=0; j<B->ilen[i]; j++) {
      if (!indices[aj[B->i[i] + j]]) ec++; 
      indices[aj[B->i[i] + j] ] = 1;
    }
  }

  /* form arrays of columns we need */
  ierr = PetscMalloc((ec+1)*sizeof(int),&garray);CHKERRQ(ierr);
  ierr = PetscMalloc((3*ec+1)*sizeof(int),&sgarray);CHKERRQ(ierr);
  ec_owner = sgarray + 2*ec;
  
  ec = 0;
  for (j=0; j<size; j++){
    for (i=owners[j]; i<owners[j+1]; i++){
      if (indices[i]) {
        garray[ec]   = i;
        ec_owner[ec] = j;
        ec++;
      }
    }
  }

  /* make indices now point into garray */
  for (i=0; i<ec; i++) indices[garray[i]] = i;

  /* compact out the extra columns in B */
  for (i=0; i<mbs; i++) {
    for (j=0; j<B->ilen[i]; j++) aj[B->i[i] + j] = indices[aj[B->i[i] + j]];
  }
  B->nbs      = ec;
  sbaij->B->n = ec*B->bs;
  ierr = PetscFree(indices);CHKERRQ(ierr);

  /* create local vector that is used to scatter into */
  ierr = VecCreateSeq(PETSC_COMM_SELF,ec*bs,&sbaij->lvec);CHKERRQ(ierr);

  /* create two temporary index sets for building scatter-gather */
  ierr = PetscMalloc((2*ec+1)*sizeof(int),&stmp);CHKERRQ(ierr);
  for (i=0; i<ec; i++) stmp[i] = bs*garray[i];  
  ierr = ISCreateBlock(PETSC_COMM_SELF,bs,ec,stmp,&from);CHKERRQ(ierr);   
  
  for (i=0; i<ec; i++) { stmp[i] = bs*i; } 
  ierr = ISCreateBlock(PETSC_COMM_SELF,bs,ec,stmp,&to);CHKERRQ(ierr);

  /* generate the scatter context */
  ierr = VecCreateMPI(mat->comm,mat->n,mat->N,&gvec);CHKERRQ(ierr);
  ierr = VecScatterCreate(gvec,from,sbaij->lvec,to,&sbaij->Mvctx);CHKERRQ(ierr); 
  ierr = VecScatterPostRecvs(gvec,sbaij->lvec,INSERT_VALUES,SCATTER_FORWARD,sbaij->Mvctx);CHKERRQ(ierr); 

  sbaij->garray = garray;
  PetscLogObjectParent(mat,sbaij->Mvctx);
  PetscLogObjectParent(mat,sbaij->lvec);
  PetscLogObjectParent(mat,from);
  PetscLogObjectParent(mat,to);

  ierr = ISDestroy(from);CHKERRQ(ierr);
  ierr = ISDestroy(to);CHKERRQ(ierr);

  /* create parallel vector that is used by SBAIJ matrix to scatter from/into */
  lsize = (mbs + ec)*bs;
  ierr = VecCreateMPI(mat->comm,lsize,PETSC_DETERMINE,&sbaij->slvec0);CHKERRQ(ierr);
  ierr = VecDuplicate(sbaij->slvec0,&sbaij->slvec1);CHKERRQ(ierr);
  ierr = VecGetSize(sbaij->slvec0,&vec_size);CHKERRQ(ierr);

  ierr = VecGetPetscMap(sbaij->slvec0,&vecmap);CHKERRQ(ierr);   
  ierr = PetscMapGetGlobalRange(vecmap,&sowners);CHKERRQ(ierr); 
 
  /* x index in the IS sfrom */
  for (i=0; i<ec; i++) { 
    j = ec_owner[i];
    sgarray[i]  = garray[i] + (sowners[j]/bs - owners[j]);  
  } 
  /* b index in the IS sfrom */
  k = sowners[rank]/bs + mbs;
  for (i=ec,j=0; i< 2*ec; i++,j++) sgarray[i] = k + j;
  
  for (i=0; i<2*ec; i++) stmp[i] = bs*sgarray[i];  
  ierr = ISCreateBlock(PETSC_COMM_SELF,bs,2*ec,stmp,&from);CHKERRQ(ierr);   
 
  /* x index in the IS sto */
  k = sowners[rank]/bs + mbs;
  for (i=0; i<ec; i++) stmp[i] = bs*(k + i);  
  /* b index in the IS sto */
  for (i=ec; i<2*ec; i++) stmp[i] = bs*sgarray[i-ec]; 

  ierr = ISCreateBlock(PETSC_COMM_SELF,bs,2*ec,stmp,&to);CHKERRQ(ierr); 

  /* gnerate the SBAIJ scatter context */
  ierr = VecScatterCreate(sbaij->slvec0,from,sbaij->slvec1,to,&sbaij->sMvctx);CHKERRQ(ierr);  
 
   /*
      Post the receives for the first matrix vector product. We sync-chronize after
    this on the chance that the user immediately calls MatMult() after assemblying 
    the matrix.
  */
  ierr = VecScatterPostRecvs(sbaij->slvec0,sbaij->slvec1,INSERT_VALUES,SCATTER_FORWARD,sbaij->sMvctx);CHKERRQ(ierr); 

  ierr = VecGetLocalSize(sbaij->slvec1,&nt);CHKERRQ(ierr);
  ierr = VecGetArrayFast(sbaij->slvec1,&ptr);CHKERRQ(ierr);
  ierr = VecCreateSeqWithArray(PETSC_COMM_SELF,bs*mbs,ptr,&sbaij->slvec1a);CHKERRQ(ierr); 
  ierr = VecCreateSeqWithArray(PETSC_COMM_SELF,nt-bs*mbs,ptr+bs*mbs,&sbaij->slvec1b);CHKERRQ(ierr);
  ierr = VecRestoreArrayFast(sbaij->slvec1,&ptr);CHKERRQ(ierr);

  ierr = VecGetArrayFast(sbaij->slvec0,&ptr);CHKERRQ(ierr);
  ierr = VecCreateSeqWithArray(PETSC_COMM_SELF,nt-bs*mbs,ptr+bs*mbs,&sbaij->slvec0b);CHKERRQ(ierr);
  ierr = VecRestoreArrayFast(sbaij->slvec0,&ptr);CHKERRQ(ierr); 

  ierr = PetscFree(stmp);CHKERRQ(ierr);
  ierr = MPI_Barrier(mat->comm);CHKERRQ(ierr);
  
  PetscLogObjectParent(mat,sbaij->sMvctx); 
  PetscLogObjectParent(mat,sbaij->slvec0);
  PetscLogObjectParent(mat,sbaij->slvec1);
  PetscLogObjectParent(mat,sbaij->slvec0b);
  PetscLogObjectParent(mat,sbaij->slvec1a);
  PetscLogObjectParent(mat,sbaij->slvec1b);
  PetscLogObjectParent(mat,from);
  PetscLogObjectParent(mat,to); 
  
  PetscLogObjectMemory(mat,(ec+1)*sizeof(int)); 
  ierr = ISDestroy(from);CHKERRQ(ierr);  
  ierr = ISDestroy(to);CHKERRQ(ierr);
  ierr = VecDestroy(gvec);CHKERRQ(ierr);
  ierr = PetscFree(sgarray);CHKERRQ(ierr); 

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSetUpMultiply_MPISBAIJ_2comm"
int MatSetUpMultiply_MPISBAIJ_2comm(Mat mat)
{
  Mat_MPISBAIJ       *baij = (Mat_MPISBAIJ*)mat->data;
  Mat_SeqBAIJ        *B = (Mat_SeqBAIJ*)(baij->B->data);  
  int                Nbs = baij->Nbs,i,j,*indices,*aj = B->j,ierr,ec = 0,*garray;
  int                bs = baij->bs,*stmp;
  IS                 from,to;
  Vec                gvec;
#if defined (PETSC_USE_CTABLE)
  PetscTable         gid1_lid1;
  PetscTablePosition tpos;
  int                gid,lid; 
#endif  

  PetscFunctionBegin;

#if defined (PETSC_USE_CTABLE)
  /* use a table - Mark Adams */
  PetscTableCreate(B->mbs,&gid1_lid1); 
  for (i=0; i<B->mbs; i++) {
    for (j=0; j<B->ilen[i]; j++) {
      int data,gid1 = aj[B->i[i]+j] + 1;
      ierr = PetscTableFind(gid1_lid1,gid1,&data) ;CHKERRQ(ierr);
      if (!data) {
        /* one based table */ 
        ierr = PetscTableAdd(gid1_lid1,gid1,++ec);CHKERRQ(ierr); 
      }
    }
  } 
  /* form array of columns we need */
  ierr = PetscMalloc((ec+1)*sizeof(int),&garray);CHKERRQ(ierr);
  ierr = PetscTableGetHeadPosition(gid1_lid1,&tpos);CHKERRQ(ierr); 
  while (tpos) {  
    ierr = PetscTableGetNext(gid1_lid1,&tpos,&gid,&lid);CHKERRQ(ierr); 
    gid--; lid--;
    garray[lid] = gid; 
  }
  ierr = PetscSortInt(ec,garray);CHKERRQ(ierr);
  ierr = PetscTableRemoveAll(gid1_lid1);CHKERRQ(ierr);
  for (i=0; i<ec; i++) {
    ierr = PetscTableAdd(gid1_lid1,garray[i]+1,i+1);CHKERRQ(ierr); 
  }
  /* compact out the extra columns in B */
  for (i=0; i<B->mbs; i++) {
    for (j=0; j<B->ilen[i]; j++) {
      int gid1 = aj[B->i[i] + j] + 1;
      ierr = PetscTableFind(gid1_lid1,gid1,&lid);CHKERRQ(ierr);
      lid --;
      aj[B->i[i]+j] = lid;
    }
  }
  B->nbs     = ec;
  baij->B->n = ec*B->bs;
  ierr = PetscTableDelete(gid1_lid1);CHKERRQ(ierr);
  /* Mark Adams */
#else
  /* For the first stab we make an array as long as the number of columns */
  /* mark those columns that are in baij->B */
  ierr = PetscMalloc((Nbs+1)*sizeof(int),&indices);CHKERRQ(ierr);
  ierr = PetscMemzero(indices,Nbs*sizeof(int));CHKERRQ(ierr);
  for (i=0; i<B->mbs; i++) {
    for (j=0; j<B->ilen[i]; j++) {
      if (!indices[aj[B->i[i] + j]]) ec++; 
      indices[aj[B->i[i] + j] ] = 1;
    }
  }

  /* form array of columns we need */
  ierr = PetscMalloc((ec+1)*sizeof(int),&garray);CHKERRQ(ierr);
  ec = 0;
  for (i=0; i<Nbs; i++) {
    if (indices[i]) {
      garray[ec++] = i;
    }
  }

  /* make indices now point into garray */
  for (i=0; i<ec; i++) {
    indices[garray[i]] = i;
  }

  /* compact out the extra columns in B */
  for (i=0; i<B->mbs; i++) {
    for (j=0; j<B->ilen[i]; j++) {
      aj[B->i[i] + j] = indices[aj[B->i[i] + j]];
    }
  }
  B->nbs       = ec;
  baij->B->n   = ec*B->bs;
  ierr = PetscFree(indices);CHKERRQ(ierr);
#endif  
  
  /* create local vector that is used to scatter into */
  ierr = VecCreateSeq(PETSC_COMM_SELF,ec*bs,&baij->lvec);CHKERRQ(ierr);

  /* create two temporary index sets for building scatter-gather */
  for (i=0; i<ec; i++) {
    garray[i] = bs*garray[i];
  }
  ierr = ISCreateBlock(PETSC_COMM_SELF,bs,ec,garray,&from);CHKERRQ(ierr);   
  for (i=0; i<ec; i++) {
    garray[i] = garray[i]/bs;
  }

  ierr = PetscMalloc((ec+1)*sizeof(int),&stmp);CHKERRQ(ierr);
  for (i=0; i<ec; i++) { stmp[i] = bs*i; } 
  ierr = ISCreateBlock(PETSC_COMM_SELF,bs,ec,stmp,&to);CHKERRQ(ierr);
  ierr = PetscFree(stmp);CHKERRQ(ierr);

  /* create temporary global vector to generate scatter context */
  /* this is inefficient, but otherwise we must do either 
     1) save garray until the first actual scatter when the vector is known or
     2) have another way of generating a scatter context without a vector.*/
  ierr = VecCreateMPI(mat->comm,mat->n,mat->N,&gvec);CHKERRQ(ierr);

  /* gnerate the scatter context */
  ierr = VecScatterCreate(gvec,from,baij->lvec,to,&baij->Mvctx);CHKERRQ(ierr);

  /*
      Post the receives for the first matrix vector product. We sync-chronize after
    this on the chance that the user immediately calls MatMult() after assemblying 
    the matrix.
  */
  ierr = VecScatterPostRecvs(gvec,baij->lvec,INSERT_VALUES,SCATTER_FORWARD,baij->Mvctx);CHKERRQ(ierr);
  ierr = MPI_Barrier(mat->comm);CHKERRQ(ierr);

  PetscLogObjectParent(mat,baij->Mvctx);
  PetscLogObjectParent(mat,baij->lvec);
  PetscLogObjectParent(mat,from);
  PetscLogObjectParent(mat,to);
  baij->garray = garray;
  PetscLogObjectMemory(mat,(ec+1)*sizeof(int));
  ierr = ISDestroy(from);CHKERRQ(ierr);
  ierr = ISDestroy(to);CHKERRQ(ierr);
  ierr = VecDestroy(gvec);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


/*
     Takes the local part of an already assembled MPIBAIJ matrix
   and disassembles it. This is to allow new nonzeros into the matrix
   that require more communication in the matrix vector multiply. 
   Thus certain data-structures must be rebuilt.

   Kind of slow! But that's what application programmers get when 
   they are sloppy.
*/
#undef __FUNCT__  
#define __FUNCT__ "DisAssemble_MPISBAIJ"
int DisAssemble_MPISBAIJ(Mat A)
{
  Mat_MPISBAIJ   *baij = (Mat_MPISBAIJ*)A->data;
  Mat           B = baij->B,Bnew;
  Mat_SeqBAIJ   *Bbaij = (Mat_SeqBAIJ*)B->data;
  int           ierr,i,j,mbs=Bbaij->mbs,n = A->N,col,*garray=baij->garray;
  int           k,bs=baij->bs,bs2=baij->bs2,*rvals,*nz,ec,m=A->m;
  MatScalar     *a = Bbaij->a;
  PetscScalar   *atmp;
#if defined(PETSC_USE_MAT_SINGLE)
  int           l;
#endif

  PetscFunctionBegin;
#if defined(PETSC_USE_MAT_SINGLE)
  ierr = PetscMalloc(baij->bs*sizeof(PetscScalar),&atmp);
#endif
  /* free stuff related to matrix-vec multiply */
  ierr = VecGetSize(baij->lvec,&ec);CHKERRQ(ierr); /* needed for PetscLogObjectMemory below */
  ierr = VecDestroy(baij->lvec);CHKERRQ(ierr); baij->lvec = 0;
  ierr = VecScatterDestroy(baij->Mvctx);CHKERRQ(ierr); baij->Mvctx = 0;
  if (baij->colmap) {
#if defined (PETSC_USE_CTABLE)
    ierr = PetscTableDelete(baij->colmap); baij->colmap = 0;CHKERRQ(ierr);
#else
    ierr = PetscFree(baij->colmap);CHKERRQ(ierr);
    baij->colmap = 0;
    PetscLogObjectMemory(A,-Bbaij->nbs*sizeof(int));
#endif
  }

  /* make sure that B is assembled so we can access its values */
  ierr = MatAssemblyBegin(B,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(B,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);

  /* invent new B and copy stuff over */
  ierr = PetscMalloc(mbs*sizeof(int),&nz);CHKERRQ(ierr);
  for (i=0; i<mbs; i++) {
    nz[i] = Bbaij->i[i+1]-Bbaij->i[i];
  }
  ierr = MatCreateSeqBAIJ(PETSC_COMM_SELF,baij->bs,m,n,0,nz,&Bnew);CHKERRQ(ierr);
  ierr = PetscFree(nz);CHKERRQ(ierr);
  
  ierr = PetscMalloc(bs*sizeof(int),&rvals);CHKERRQ(ierr);
  for (i=0; i<mbs; i++) {
    rvals[0] = bs*i;
    for (j=1; j<bs; j++) { rvals[j] = rvals[j-1] + 1; }
    for (j=Bbaij->i[i]; j<Bbaij->i[i+1]; j++) {
      col = garray[Bbaij->j[j]]*bs;
      for (k=0; k<bs; k++) {
#if defined(PETSC_USE_MAT_SINGLE)
        for (l=0; l<bs; l++) atmp[l] = a[j*bs2+l];
#else
        atmp = a+j*bs2 + k*bs;
#endif
        ierr = MatSetValues_SeqSBAIJ(Bnew,bs,rvals,1,&col,atmp,B->insertmode);CHKERRQ(ierr);
        col++;
      }
    }
  }
#if defined(PETSC_USE_MAT_SINGLE)
  ierr = PetscFree(atmp);CHKERRQ(ierr);
#endif
  ierr = PetscFree(baij->garray);CHKERRQ(ierr);
  baij->garray = 0;
  ierr = PetscFree(rvals);CHKERRQ(ierr);
  PetscLogObjectMemory(A,-ec*sizeof(int));
  ierr = MatDestroy(B);CHKERRQ(ierr);
  PetscLogObjectParent(A,Bnew);
  baij->B = Bnew;
  A->was_assembled = PETSC_FALSE;
  PetscFunctionReturn(0);
}



