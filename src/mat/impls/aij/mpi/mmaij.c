/*$Id: mmaij.c,v 1.59 2001/08/07 03:02:49 balay Exp $*/

/*
   Support for the parallel AIJ matrix vector multiply
*/
#include "src/mat/impls/aij/mpi/mpiaij.h"
#include "src/vec/vecimpl.h"

#undef __FUNCT__  
#define __FUNCT__ "MatSetUpMultiply_MPIAIJ"
int MatSetUpMultiply_MPIAIJ(Mat mat)
{
  Mat_MPIAIJ         *aij = (Mat_MPIAIJ*)mat->data;
  Mat_SeqAIJ         *B = (Mat_SeqAIJ*)(aij->B->data);  
  int                N = mat->N,i,j,*indices,*aj = B->j,ierr,ec = 0,*garray;
  int                shift = B->indexshift;
  IS                 from,to;
  Vec                gvec;
#if defined (PETSC_USE_CTABLE)
  PetscTable         gid1_lid1;
  PetscTablePosition tpos;
  int                gid,lid; 
#endif

  PetscFunctionBegin;

#if defined (PETSC_USE_CTABLE)
  /* use a table - Mark Adams (this has not been tested with "shift") */
  ierr = PetscTableCreate(aij->B->m,&gid1_lid1);CHKERRQ(ierr);
  for (i=0; i<aij->B->m; i++) {
    for (j=0; j<B->ilen[i]; j++) {
      int data,gid1 = aj[B->i[i] + shift + j] + 1 + shift;
      ierr = PetscTableFind(gid1_lid1,gid1,&data);CHKERRQ(ierr);
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
    gid--;
    lid--;
    garray[lid] = gid; 
  }
  ierr = PetscSortInt(ec,garray);CHKERRQ(ierr); /* sort, and rebuild */
  ierr = PetscTableRemoveAll(gid1_lid1);CHKERRQ(ierr);
  for (i=0; i<ec; i++) {
    ierr = PetscTableAdd(gid1_lid1,garray[i]+1,i+1);CHKERRQ(ierr); 
  }
  /* compact out the extra columns in B */
  for (i=0; i<aij->B->m; i++) {
    for (j=0; j<B->ilen[i]; j++) {
      int gid1 = aj[B->i[i] + shift + j] + 1 + shift;
      ierr = PetscTableFind(gid1_lid1,gid1,&lid);CHKERRQ(ierr);
      lid --;
      aj[B->i[i] + shift + j]  = lid - shift;
    }
  }
  aij->B->n = aij->B->N = ec;
  ierr = PetscTableDelete(gid1_lid1);CHKERRQ(ierr);
  /* Mark Adams */
#else
  /* For the first stab we make an array as long as the number of columns */
  /* mark those columns that are in aij->B */
  ierr = PetscMalloc((N+1)*sizeof(int),&indices);CHKERRQ(ierr);
  ierr = PetscMemzero(indices,N*sizeof(int));CHKERRQ(ierr);
  for (i=0; i<aij->B->m; i++) {
    for (j=0; j<B->ilen[i]; j++) {
      if (!indices[aj[B->i[i] +shift + j] + shift]) ec++; 
      indices[aj[B->i[i] + shift + j] + shift] = 1;
    }
  }

  /* form array of columns we need */
  ierr = PetscMalloc((ec+1)*sizeof(int),&garray);CHKERRQ(ierr);
  ec = 0;
  for (i=0; i<N; i++) {
    if (indices[i]) garray[ec++] = i;
  }

  /* make indices now point into garray */
  for (i=0; i<ec; i++) {
    indices[garray[i]] = i-shift;
  }

  /* compact out the extra columns in B */
  for (i=0; i<aij->B->m; i++) {
    for (j=0; j<B->ilen[i]; j++) {
      aj[B->i[i] + shift + j] = indices[aj[B->i[i] + shift + j]+shift];
    }
  }
  aij->B->n = aij->B->N = ec;
  ierr = PetscFree(indices);CHKERRQ(ierr);
#endif  
  /* create local vector that is used to scatter into */
  ierr = VecCreateSeq(PETSC_COMM_SELF,ec,&aij->lvec);CHKERRQ(ierr);

  /* create two temporary Index sets for build scatter gather */
  ierr = ISCreateGeneral(mat->comm,ec,garray,&from);CHKERRQ(ierr);
  ierr = ISCreateStride(PETSC_COMM_SELF,ec,0,1,&to);CHKERRQ(ierr);

  /* create temporary global vector to generate scatter context */
  /* this is inefficient, but otherwise we must do either 
     1) save garray until the first actual scatter when the vector is known or
     2) have another way of generating a scatter context without a vector.*/
  ierr = VecCreateMPI(mat->comm,mat->n,mat->N,&gvec);CHKERRQ(ierr);

  /* generate the scatter context */
  ierr = VecScatterCreate(gvec,from,aij->lvec,to,&aij->Mvctx);CHKERRQ(ierr);
  PetscLogObjectParent(mat,aij->Mvctx);
  PetscLogObjectParent(mat,aij->lvec);
  PetscLogObjectParent(mat,from);
  PetscLogObjectParent(mat,to);
  aij->garray = garray;
  PetscLogObjectMemory(mat,(ec+1)*sizeof(int));
  ierr = ISDestroy(from);CHKERRQ(ierr);
  ierr = ISDestroy(to);CHKERRQ(ierr);
  ierr = VecDestroy(gvec);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "DisAssemble_MPIAIJ"
/*
     Takes the local part of an already assembled MPIAIJ matrix
   and disassembles it. This is to allow new nonzeros into the matrix
   that require more communication in the matrix vector multiply. 
   Thus certain data-structures must be rebuilt.

   Kind of slow! But that's what application programmers get when 
   they are sloppy.
*/
int DisAssemble_MPIAIJ(Mat A)
{
  Mat_MPIAIJ   *aij = (Mat_MPIAIJ*)A->data;
  Mat          B = aij->B,Bnew;
  Mat_SeqAIJ   *Baij = (Mat_SeqAIJ*)B->data;
  int          ierr,i,j,m = B->m,n = A->N,col,ct = 0,*garray = aij->garray;
  int          *nz,ec,shift = Baij->indexshift;
  PetscScalar  v;

  PetscFunctionBegin;
  /* free stuff related to matrix-vec multiply */
  ierr = VecGetSize(aij->lvec,&ec);CHKERRQ(ierr); /* needed for PetscLogObjectMemory below */
  ierr = VecDestroy(aij->lvec);CHKERRQ(ierr); aij->lvec = 0;
  ierr = VecScatterDestroy(aij->Mvctx);CHKERRQ(ierr); aij->Mvctx = 0;
  if (aij->colmap) {
#if defined (PETSC_USE_CTABLE)
    ierr = PetscTableDelete(aij->colmap);CHKERRQ(ierr);
    aij->colmap = 0;
#else
    ierr = PetscFree(aij->colmap);CHKERRQ(ierr);
    aij->colmap = 0;
    PetscLogObjectMemory(A,-aij->B->n*sizeof(int));
#endif
  }

  /* make sure that B is assembled so we can access its values */
  ierr = MatAssemblyBegin(B,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(B,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);

  /* invent new B and copy stuff over */
  ierr = PetscMalloc((m+1)*sizeof(int),&nz);CHKERRQ(ierr);
  for (i=0; i<m; i++) {
    nz[i] = Baij->i[i+1] - Baij->i[i];
  }
  ierr = MatCreateSeqAIJ(PETSC_COMM_SELF,m,n,0,nz,&Bnew);CHKERRQ(ierr);
  ierr = PetscFree(nz);CHKERRQ(ierr);
  for (i=0; i<m; i++) {
    for (j=Baij->i[i]+shift; j<Baij->i[i+1]+shift; j++) {
      col  = garray[Baij->j[ct]+shift];
      v    = Baij->a[ct++];
      ierr = MatSetValues(Bnew,1,&i,1,&col,&v,B->insertmode);CHKERRQ(ierr);
    }
  }
  ierr = PetscFree(aij->garray);CHKERRQ(ierr);
  aij->garray = 0;
  PetscLogObjectMemory(A,-ec*sizeof(int));
  ierr = MatDestroy(B);CHKERRQ(ierr);
  PetscLogObjectParent(A,Bnew);
  aij->B = Bnew;
  A->was_assembled = PETSC_FALSE;
  PetscFunctionReturn(0);
}

/*      ugly stuff added for Glenn someday we should fix this up */

static int *auglyrmapd = 0,*auglyrmapo = 0;  /* mapping from the local ordering to the "diagonal" and "off-diagonal"
                                      parts of the local matrix */
static Vec auglydd = 0,auglyoo = 0;   /* work vectors used to scale the two parts of the local matrix */


#undef __FUNCT__
#define __FUNCT__ "MatMPIAIJDiagonalScaleLocalSetUp"
int MatMPIAIJDiagonalScaleLocalSetUp(Mat inA,Vec scale)
{
  Mat_MPIAIJ  *ina = (Mat_MPIAIJ*) inA->data; /*access private part of matrix */
  int          ierr,i,n,nt,cstart,cend,no,*garray = ina->garray,*lindices;
  int          *r_rmapd,*r_rmapo;
  
  PetscFunctionBegin;
  ierr = MatGetOwnershipRange(inA,&cstart,&cend);CHKERRQ(ierr);
  ierr = MatGetSize(ina->A,PETSC_NULL,&n);CHKERRQ(ierr);
  ierr = PetscMalloc((inA->mapping->n+1)*sizeof(int),&r_rmapd);CHKERRQ(ierr);
  ierr = PetscMemzero(r_rmapd,inA->mapping->n*sizeof(int));CHKERRQ(ierr);
  nt   = 0;
  for (i=0; i<inA->mapping->n; i++) {
    if (inA->mapping->indices[i] >= cstart && inA->mapping->indices[i] < cend) {
      nt++;
      r_rmapd[i] = inA->mapping->indices[i] + 1;
    }
  }
  if (nt != n) SETERRQ2(1,"Hmm nt %d n %d",nt,n);
  ierr = PetscMalloc((n+1)*sizeof(int),&auglyrmapd);CHKERRQ(ierr);
  for (i=0; i<inA->mapping->n; i++) {
    if (r_rmapd[i]){
      auglyrmapd[(r_rmapd[i]-1)-cstart] = i;
    }
  }
  ierr = PetscFree(r_rmapd);CHKERRQ(ierr);
  ierr = VecCreateSeq(PETSC_COMM_SELF,n,&auglydd);CHKERRQ(ierr);

  ierr = PetscMalloc((inA->N+1)*sizeof(int),&lindices);CHKERRQ(ierr);
  ierr = PetscMemzero(lindices,inA->N*sizeof(int));CHKERRQ(ierr);
  for (i=0; i<ina->B->n; i++) {
    lindices[garray[i]] = i+1;
  }
  no   = inA->mapping->n - nt;
  ierr = PetscMalloc((inA->mapping->n+1)*sizeof(int),&r_rmapo);CHKERRQ(ierr);
  ierr = PetscMemzero(r_rmapo,inA->mapping->n*sizeof(int));CHKERRQ(ierr);
  nt   = 0;
  for (i=0; i<inA->mapping->n; i++) {
    if (lindices[inA->mapping->indices[i]]) {
      nt++;
      r_rmapo[i] = lindices[inA->mapping->indices[i]];
    }
  }
  if (nt > no) SETERRQ2(1,"Hmm nt %d no %d",nt,n);
  ierr = PetscFree(lindices);CHKERRQ(ierr);
  ierr = PetscMalloc((nt+1)*sizeof(int),&auglyrmapo);CHKERRQ(ierr);
  for (i=0; i<inA->mapping->n; i++) {
    if (r_rmapo[i]){
      auglyrmapo[(r_rmapo[i]-1)] = i;
    }
  }
  ierr = PetscFree(r_rmapo);CHKERRQ(ierr);
  ierr = VecCreateSeq(PETSC_COMM_SELF,nt,&auglyoo);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "MatMPIAIJDiagonalScaleLocal"
int MatMPIAIJDiagonalScaleLocal(Mat A,Vec scale)
{
  Mat_MPIAIJ  *a = (Mat_MPIAIJ*) A->data; /*access private part of matrix */
  int         ierr,n,i;
  PetscScalar *d,*o,*s;
  
  PetscFunctionBegin;
  if (!auglyrmapd) {
    ierr = MatMPIAIJDiagonalScaleLocalSetUp(A,scale);CHKERRQ(ierr);
  }

  ierr = VecGetArray(scale,&s);CHKERRQ(ierr);
  
  ierr = VecGetLocalSize(auglydd,&n);CHKERRQ(ierr);
  ierr = VecGetArray(auglydd,&d);CHKERRQ(ierr);
  for (i=0; i<n; i++) {
    d[i] = s[auglyrmapd[i]]; /* copy "diagonal" (true local) portion of scale into dd vector */
  }
  ierr = VecRestoreArray(auglydd,&d);CHKERRQ(ierr);
  /* column scale "diagonal" portion of local matrix */
  ierr = MatDiagonalScale(a->A,PETSC_NULL,auglydd);CHKERRQ(ierr);

  ierr = VecGetLocalSize(auglyoo,&n);CHKERRQ(ierr);
  ierr = VecGetArray(auglyoo,&o);CHKERRQ(ierr);
  for (i=0; i<n; i++) {
    o[i] = s[auglyrmapo[i]]; /* copy "off-diagonal" portion of scale into oo vector */
  }
  ierr = VecRestoreArray(scale,&s);CHKERRQ(ierr);
  ierr = VecRestoreArray(auglyoo,&o);CHKERRQ(ierr);
  /* column scale "off-diagonal" portion of local matrix */
  ierr = MatDiagonalScale(a->B,PETSC_NULL,auglyoo);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}




