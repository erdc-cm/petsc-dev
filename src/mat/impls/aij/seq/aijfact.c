
#include "src/mat/impls/aij/seq/aij.h"
#include "src/inline/dot.h"
#include "src/inline/spops.h"

#undef __FUNCT__  
#define __FUNCT__ "MatOrdering_Flow_SeqAIJ"
int MatOrdering_Flow_SeqAIJ(Mat mat,const MatOrderingType type,IS *irow,IS *icol)
{
  PetscFunctionBegin;

  SETERRQ(PETSC_ERR_SUP,"Code not written");
#if !defined(PETSC_USE_DEBUG)
  PetscFunctionReturn(0);
#endif
}


EXTERN int MatMarkDiagonal_SeqAIJ(Mat);
EXTERN int Mat_AIJ_CheckInode(Mat,PetscTruth);

EXTERN int SPARSEKIT2dperm(int*,PetscScalar*,int*,int*,PetscScalar*,int*,int*,int*,int*,int*);
EXTERN int SPARSEKIT2ilutp(int*,PetscScalar*,int*,int*,int*,PetscReal,PetscReal*,int*,PetscScalar*,int*,int*,int*,PetscScalar*,int*,int*,int*);
EXTERN int SPARSEKIT2msrcsr(int*,PetscScalar*,int*,PetscScalar*,int*,int*,PetscScalar*,int*);

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
int MatILUDTFactor_SeqAIJ(Mat A,MatFactorInfo *info,IS isrow,IS iscol,Mat *fact)
{
#if defined(PETSC_AVOID_GNUCOPYRIGHT_CODE)
  PetscFunctionBegin;
  SETERRQ(1,"This distribution does not include GNU Copyright code\n\
  You can obtain the drop tolerance routines by installing PETSc from\n\
  www.mcs.anl.gov/petsc\n");
#else
  Mat_SeqAIJ   *a = (Mat_SeqAIJ*)A->data,*b;
  IS           iscolf,isicol,isirow;
  PetscTruth   reorder;
  int          *c,*r,*ic,ierr,i,n = A->m;
  int          *old_i = a->i,*old_j = a->j,*new_i,*old_i2 = 0,*old_j2 = 0,*new_j;
  int          *ordcol,*iwk,*iperm,*jw;
  int          jmax,lfill,job,*o_i,*o_j;
  PetscScalar  *old_a = a->a,*w,*new_a,*old_a2 = 0,*wk,*o_a;
  PetscReal    permtol,af;

  PetscFunctionBegin;

  if (info->dt == PETSC_DEFAULT)      info->dt      = .005;
  if (info->dtcount == PETSC_DEFAULT) info->dtcount = (int)(1.5*a->rmax); 
  if (info->dtcol == PETSC_DEFAULT)   info->dtcol   = .01;
  if (info->fill == PETSC_DEFAULT)    info->fill    = ((double)(n*(info->dtcount+1)))/a->nz;
  lfill   = (int)(info->dtcount/2.0);
  jmax    = (int)(info->fill*a->nz);
  permtol = info->dtcol;


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
  ierr = PetscMalloc((n+1)*sizeof(int),&new_i);CHKERRQ(ierr);
  ierr = PetscMalloc(jmax*sizeof(int),&new_j);CHKERRQ(ierr);
  ierr = PetscMalloc(jmax*sizeof(PetscScalar),&new_a);CHKERRQ(ierr);
  ierr = PetscMalloc(n*sizeof(int),&ordcol);CHKERRQ(ierr);

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
    ierr = PetscMalloc((n+1)*sizeof(int),&old_i2);CHKERRQ(ierr);
    ierr = PetscMalloc((old_i[n]-old_i[0]+1)*sizeof(int),&old_j2);CHKERRQ(ierr);
    ierr = PetscMalloc((old_i[n]-old_i[0]+1)*sizeof(PetscScalar),&old_a2);CHKERRQ(ierr);
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

  ierr = PetscMalloc(2*n*sizeof(int),&iperm);CHKERRQ(ierr);
  ierr = PetscMalloc(2*n*sizeof(int),&jw);CHKERRQ(ierr);
  ierr = PetscMalloc(n*sizeof(PetscScalar),&w);CHKERRQ(ierr);

  SPARSEKIT2ilutp(&n,o_a,o_j,o_i,&lfill,(PetscReal)info->dt,&permtol,&n,new_a,new_j,new_i,&jmax,w,jw,iperm,&ierr); 
  if (ierr) {
    switch (ierr) {
      case -3: SETERRQ2(1,"ilutp(), matrix U overflows, need larger info->fill current fill %g space allocated %d",info->fill,jmax);
      case -2: SETERRQ2(1,"ilutp(), matrix L overflows, need larger info->fill current fill %g space allocated %d",info->fill,jmax);
      case -5: SETERRQ(1,"ilutp(), zero row encountered");
      case -1: SETERRQ(1,"ilutp(), input matrix may be wrong");
      case -4: SETERRQ1(1,"ilutp(), illegal info->fill value %d",jmax);
      default: SETERRQ1(1,"ilutp(), zero pivot detected on row %d",ierr);
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
  ierr = PetscMalloc((n+1)*sizeof(int),&iwk);CHKERRQ(ierr);

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

  ierr = MatCreate(A->comm,n,n,n,n,fact);CHKERRQ(ierr);
  ierr = MatSetType(*fact,A->type_name);CHKERRQ(ierr);
  ierr = MatSeqAIJSetPreallocation(*fact,0,PETSC_NULL);CHKERRQ(ierr);
  (*fact)->factor    = FACTOR_LU;
  (*fact)->assembled = PETSC_TRUE;

  b = (Mat_SeqAIJ*)(*fact)->data;
  ierr = PetscFree(b->imax);CHKERRQ(ierr);
  b->sorted        = PETSC_FALSE;
  b->singlemalloc  = PETSC_FALSE;
  /* the next line frees the default space generated by the MatCreate() */
  ierr             = PetscFree(b->a);CHKERRQ(ierr);
  ierr             = PetscFree(b->ilen);CHKERRQ(ierr);
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

  ierr = MatMarkDiagonal_SeqAIJ(A);CHKERRQ(ierr);

  /* check out for identical nodes. If found, use inode functions */
  ierr = Mat_AIJ_CheckInode(*fact,PETSC_FALSE);CHKERRQ(ierr);

  af = ((double)b->nz)/((double)a->nz) + .001;
  PetscLogInfo(A,"MatILUDTFactor_SeqAIJ:Fill ratio:given %g needed %g\n",info->fill,af);
  PetscLogInfo(A,"MatILUDTFactor_SeqAIJ:Run with -pc_ilu_fill %g or use \n",af);
  PetscLogInfo(A,"MatILUDTFactor_SeqAIJ:PCILUSetFill(pc,%g);\n",af);
  PetscLogInfo(A,"MatILUDTFactor_SeqAIJ:for best performance.\n");

  PetscFunctionReturn(0);
#endif
}

/*
    Factorization code for AIJ format. 
*/
#undef __FUNCT__  
#define __FUNCT__ "MatLUFactorSymbolic_SeqAIJ"
int MatLUFactorSymbolic_SeqAIJ(Mat A,IS isrow,IS iscol,MatFactorInfo *info,Mat *B)
{
  Mat_SeqAIJ *a = (Mat_SeqAIJ*)A->data,*b;
  IS         isicol;
  int        *r,*ic,ierr,i,n = A->m,*ai = a->i,*aj = a->j;
  int        *ainew,*ajnew,jmax,*fill,*ajtmp,nz;
  int        *idnew,idx,row,m,fm,nnz,nzi,realloc = 0,nzbd,*im;
  PetscReal  f;

  PetscFunctionBegin;
  if (A->M != A->N) SETERRQ(PETSC_ERR_ARG_WRONG,"matrix must be square");
  ierr = ISInvertPermutation(iscol,PETSC_DECIDE,&isicol);CHKERRQ(ierr);
  ierr = ISGetIndices(isrow,&r);CHKERRQ(ierr);
  ierr = ISGetIndices(isicol,&ic);CHKERRQ(ierr);

  /* get new row pointers */
  ierr = PetscMalloc((n+1)*sizeof(int),&ainew);CHKERRQ(ierr);
  ainew[0] = 0;
  /* don't know how many column pointers are needed so estimate */
  f = info->fill;
  jmax  = (int)(f*ai[n]+1);
  ierr = PetscMalloc((jmax)*sizeof(int),&ajnew);CHKERRQ(ierr);
  /* fill is a linked list of nonzeros in active row */
  ierr = PetscMalloc((2*n+1)*sizeof(int),&fill);CHKERRQ(ierr);
  im   = fill + n + 1;
  /* idnew is location of diagonal in factor */
  ierr = PetscMalloc((n+1)*sizeof(int),&idnew);CHKERRQ(ierr);
  idnew[0] = 0;

  for (i=0; i<n; i++) {
    /* first copy previous fill into linked list */
    nnz     = nz    = ai[r[i]+1] - ai[r[i]];
    if (!nz) SETERRQ(PETSC_ERR_MAT_LU_ZRPVT,"Empty row in matrix");
    ajtmp   = aj + ai[r[i]];
    fill[n] = n;
    while (nz--) {
      fm  = n;
      idx = ic[*ajtmp++];
      do {
        m  = fm;
        fm = fill[m];
      } while (fm < idx);
      fill[m]   = idx;
      fill[idx] = fm;
    }
    row = fill[n];
    while (row < i) {
      ajtmp = ajnew + idnew[row] + 1;
      nzbd  = 1 + idnew[row] - ainew[row];
      nz    = im[row] - nzbd;
      fm    = row;
      while (nz-- > 0) {
        idx = *ajtmp++ ;
        nzbd++;
        if (idx == i) im[row] = nzbd;
        do {
          m  = fm;
          fm = fill[m];
        } while (fm < idx);
        if (fm != idx) {
          fill[m]   = idx;
          fill[idx] = fm;
          fm        = idx;
          nnz++;
        }
      }
      row = fill[row];
    }
    /* copy new filled row into permanent storage */
    ainew[i+1] = ainew[i] + nnz;
    if (ainew[i+1] > jmax) {

      /* estimate how much additional space we will need */
      /* use the strategy suggested by David Hysom <hysom@perch-t.icase.edu> */
      /* just double the memory each time */
      int maxadd = jmax;
      /* maxadd = (int)((f*(ai[n]+(!shift))*(n-i+5))/n); */
      if (maxadd < nnz) maxadd = (n-i)*(nnz+1);
      jmax += maxadd;

      /* allocate a longer ajnew */
      ierr = PetscMalloc(jmax*sizeof(int),&ajtmp);CHKERRQ(ierr);
      ierr  = PetscMemcpy(ajtmp,ajnew,(ainew[i])*sizeof(int));CHKERRQ(ierr);
      ierr  = PetscFree(ajnew);CHKERRQ(ierr);
      ajnew = ajtmp;
      realloc++; /* count how many times we realloc */
    }
    ajtmp = ajnew + ainew[i];
    fm    = fill[n];
    nzi   = 0;
    im[i] = nnz;
    while (nnz--) {
      if (fm < i) nzi++;
      *ajtmp++ = fm ;
      fm       = fill[fm];
    }
    idnew[i] = ainew[i] + nzi;
  }
  if (ai[n] != 0) {
    PetscReal af = ((PetscReal)ainew[n])/((PetscReal)ai[n]);
    PetscLogInfo(A,"MatLUFactorSymbolic_SeqAIJ:Reallocs %d Fill ratio:given %g needed %g\n",realloc,f,af);
    PetscLogInfo(A,"MatLUFactorSymbolic_SeqAIJ:Run with -pc_lu_fill %g or use \n",af);
    PetscLogInfo(A,"MatLUFactorSymbolic_SeqAIJ:PCLUSetFill(pc,%g);\n",af);
    PetscLogInfo(A,"MatLUFactorSymbolic_SeqAIJ:for best performance.\n");
  } else {
    PetscLogInfo(A,"MatLUFactorSymbolic_SeqAIJ: Empty matrix\n");
  }

  ierr = ISRestoreIndices(isrow,&r);CHKERRQ(ierr);
  ierr = ISRestoreIndices(isicol,&ic);CHKERRQ(ierr);

  ierr = PetscFree(fill);CHKERRQ(ierr);

  /* put together the new matrix */
  ierr = MatCreate(A->comm,n,n,n,n,B);CHKERRQ(ierr);
  ierr = MatSetType(*B,A->type_name);CHKERRQ(ierr);
  ierr = MatSeqAIJSetPreallocation(*B,0,PETSC_NULL);CHKERRQ(ierr);
  PetscLogObjectParent(*B,isicol); 
  b = (Mat_SeqAIJ*)(*B)->data;
  ierr = PetscFree(b->imax);CHKERRQ(ierr);
  b->singlemalloc = PETSC_FALSE;
  /* the next line frees the default space generated by the Create() */
  ierr = PetscFree(b->a);CHKERRQ(ierr);
  ierr = PetscFree(b->ilen);CHKERRQ(ierr);
  ierr          = PetscMalloc((ainew[n]+1)*sizeof(PetscScalar),&b->a);CHKERRQ(ierr);
  b->j          = ajnew;
  b->i          = ainew;
  b->diag       = idnew;
  b->ilen       = 0;
  b->imax       = 0;
  b->row        = isrow;
  b->col        = iscol;
  b->lu_damping        = info->damping;
  b->lu_zeropivot      = info->zeropivot;
  b->lu_shift          = info->shift;
  b->lu_shift_fraction = info->shift_fraction;
  ierr          = PetscObjectReference((PetscObject)isrow);CHKERRQ(ierr);
  ierr          = PetscObjectReference((PetscObject)iscol);CHKERRQ(ierr);
  b->icol       = isicol;
  ierr          = PetscMalloc((n+1)*sizeof(PetscScalar),&b->solve_work);CHKERRQ(ierr);
  /* In b structure:  Free imax, ilen, old a, old j.  
     Allocate idnew, solve_work, new a, new j */
  PetscLogObjectMemory(*B,(ainew[n]-n)*(sizeof(int)+sizeof(PetscScalar)));
  b->maxnz = b->nz = ainew[n] ;

  (*B)->factor                 =  FACTOR_LU;
  (*B)->info.factor_mallocs    = realloc;
  (*B)->info.fill_ratio_given  = f;
  ierr = Mat_AIJ_CheckInode(*B,PETSC_FALSE);CHKERRQ(ierr);
  (*B)->ops->lufactornumeric   =  A->ops->lufactornumeric; /* Use Inode variant ONLY if A has inodes */

  if (ai[n] != 0) {
    (*B)->info.fill_ratio_needed = ((PetscReal)ainew[n])/((PetscReal)ai[n]);
  } else {
    (*B)->info.fill_ratio_needed = 0.0;
  }
  PetscFunctionReturn(0); 
}
/* ----------------------------------------------------------- */
EXTERN int Mat_AIJ_CheckInode(Mat,PetscTruth);

#undef __FUNCT__  
#define __FUNCT__ "MatLUFactorNumeric_SeqAIJ"
int MatLUFactorNumeric_SeqAIJ(Mat A,Mat *B)
{
  Mat          C = *B;
  Mat_SeqAIJ   *a = (Mat_SeqAIJ*)A->data,*b = (Mat_SeqAIJ *)C->data;
  IS           isrow = b->row,isicol = b->icol;
  int          *r,*ic,ierr,i,j,n = A->m,*ai = b->i,*aj = b->j;
  int          *ajtmpold,*ajtmp,nz,row,*ics;
  int          *diag_offset = b->diag,diag,*pj,ndamp = 0, nshift=0;
  PetscScalar  *rtmp,*v,*pc,multiplier,*pv,*rtmps;
  PetscReal    damping = b->lu_damping, zeropivot = b->lu_zeropivot,rs,d;
  PetscReal    row_shift,shift_fraction,shift_amount,shift_lo=0., shift_hi=1., shift_top=0.;
  PetscTruth   damp,lushift;

  PetscFunctionBegin;
  ierr  = ISGetIndices(isrow,&r);CHKERRQ(ierr);
  ierr  = ISGetIndices(isicol,&ic);CHKERRQ(ierr);
  ierr  = PetscMalloc((n+1)*sizeof(PetscScalar),&rtmp);CHKERRQ(ierr);
  ierr  = PetscMemzero(rtmp,(n+1)*sizeof(PetscScalar));CHKERRQ(ierr);
  rtmps = rtmp; ics = ic;

  if (!a->diag) {
    ierr = MatMarkDiagonal_SeqAIJ(A);CHKERRQ(ierr);
  }

  if (b->lu_shift) { /* set max shift */
    int *aai = a->i,*ddiag = a->diag;
    shift_top = 0;
    for (i=0; i<n; i++) {
      d = PetscAbsScalar((a->a)[ddiag[i]]);
      /* calculate amt of shift needed for this row */
      if (d<=0) {
        row_shift = 0; 
      } else {
        row_shift = -2*d;
      }
      v = a->a+aai[i];
      for (j=0; j<aai[i+1]-aai[i]; j++) 
	row_shift += PetscAbsScalar(v[j]);
      if (row_shift>shift_top) shift_top = row_shift;
    }
  }

  shift_fraction = 0; shift_amount = 0;
  do {
    damp    = PETSC_FALSE;
    lushift = PETSC_FALSE;
    for (i=0; i<n; i++) {
      nz    = ai[i+1] - ai[i];
      ajtmp = aj + ai[i];
      for  (j=0; j<nz; j++) rtmps[ajtmp[j]] = 0.0;

      /* load in initial (unfactored row) */
      nz       = a->i[r[i]+1] - a->i[r[i]];
      ajtmpold = a->j + a->i[r[i]];
      v        = a->a + a->i[r[i]];
      for (j=0; j<nz; j++) {
        rtmp[ics[ajtmpold[j]]] = v[j];
      }
      rtmp[ics[r[i]]] += damping + shift_amount; /* damp the diagonal of the matrix */

      row = *ajtmp++ ;
      while  (row < i) {
        pc = rtmp + row;
        if (*pc != 0.0) {
          pv         = b->a + diag_offset[row] ;
          pj         = b->j + diag_offset[row] + 1;
          multiplier = *pc / *pv++;
          *pc        = multiplier;
          nz         = ai[row+1] - diag_offset[row] - 1;
          for (j=0; j<nz; j++) rtmps[pj[j]] -= multiplier * pv[j];
          PetscLogFlops(2*nz);
        }
        row = *ajtmp++;
      }
      /* finished row so stick it into b->a */
      pv   = b->a + ai[i] ;
      pj   = b->j + ai[i] ;
      nz   = ai[i+1] - ai[i];
      diag = diag_offset[i] - ai[i];
      /* 9/13/02 Victor Eijkhout suggested scaling zeropivot by rs for matrices with funny scalings */
      rs   = 0.0;
      for (j=0; j<nz; j++) {
        pv[j] = rtmps[pj[j]];
        if (j != diag) rs += PetscAbsScalar(pv[j]);
      }
#define MAX_NSHIFT 5
      if (PetscRealPart(pv[diag]) <= zeropivot*rs && b->lu_shift) {
	if (nshift>MAX_NSHIFT) {
	  SETERRQ(1,"Unable to determine shift to enforce positive definite preconditioner");
	} else if (nshift==MAX_NSHIFT) {
	  shift_fraction = shift_hi;
	  lushift      = PETSC_FALSE;
	} else {
	  shift_lo = shift_fraction; shift_fraction = (shift_hi+shift_lo)/2.;
	  lushift      = PETSC_TRUE;
	}
	shift_amount = shift_fraction * shift_top;
        nshift++; 
        break;
      }
      if (PetscAbsScalar(pv[diag]) <= zeropivot*rs) {
        if (damping) {
          if (ndamp) damping *= 2.0;
          damp = PETSC_TRUE;
          ndamp++;
          break;
        } else {
          SETERRQ4(PETSC_ERR_MAT_LU_ZRPVT,"Zero pivot row %d value %g tolerance %g * rs %g",i,PetscAbsScalar(pv[diag]),zeropivot,rs);
        }
      }
    }
    if (!lushift && b->lu_shift && shift_fraction>0 && nshift<MAX_NSHIFT) {
      /*
       * if no shift in this attempt & shifting & started shifting & can refine,
       * then try lower shift
       */
      shift_hi       = shift_fraction;
      shift_fraction = (shift_hi+shift_lo)/2.;
      shift_amount   = shift_fraction * shift_top;
      lushift        = PETSC_TRUE;
      nshift++;
    }
  } while (damp || lushift);

  /* invert diagonal entries for simplier triangular solves */
  for (i=0; i<n; i++) {
    b->a[diag_offset[i]] = 1.0/b->a[diag_offset[i]];
  }

  ierr = PetscFree(rtmp);CHKERRQ(ierr);
  ierr = ISRestoreIndices(isicol,&ic);CHKERRQ(ierr);
  ierr = ISRestoreIndices(isrow,&r);CHKERRQ(ierr);
  C->factor = FACTOR_LU;
  (*B)->ops->lufactornumeric   =  A->ops->lufactornumeric; /* Use Inode variant ONLY if A has inodes */
  C->assembled = PETSC_TRUE;
  PetscLogFlops(C->n);
  if (ndamp) {
    PetscLogInfo(0,"MatLUFactorNumerical_SeqAIJ: number of damping tries %d damping value %g\n",ndamp,damping);
  }
  if (nshift) {
    b->lu_shift_fraction = shift_fraction;
    PetscLogInfo(0,"MatLUFactorNumerical_SeqAIJ: diagonal shifted up by %e fraction top_value %e number shifts %d\n",shift_fraction,shift_top,nshift);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatUsePETSc_SeqAIJ"
int MatUsePETSc_SeqAIJ(Mat A)
{
  PetscFunctionBegin;
  A->ops->lufactorsymbolic = MatLUFactorSymbolic_SeqAIJ;
  A->ops->lufactornumeric  = MatLUFactorNumeric_SeqAIJ;
  PetscFunctionReturn(0);
}


/* ----------------------------------------------------------- */
#undef __FUNCT__  
#define __FUNCT__ "MatLUFactor_SeqAIJ"
int MatLUFactor_SeqAIJ(Mat A,IS row,IS col,MatFactorInfo *info)
{
  int ierr;
  Mat C;

  PetscFunctionBegin;
  ierr = MatLUFactorSymbolic(A,row,col,info,&C);CHKERRQ(ierr);
  ierr = MatLUFactorNumeric(A,&C);CHKERRQ(ierr);
  ierr = MatHeaderCopy(A,C);CHKERRQ(ierr);
  PetscLogObjectParent(A,((Mat_SeqAIJ*)(A->data))->icol); 
  PetscFunctionReturn(0);
}
/* ----------------------------------------------------------- */
#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqAIJ"
int MatSolve_SeqAIJ(Mat A,Vec bb,Vec xx)
{
  Mat_SeqAIJ   *a = (Mat_SeqAIJ*)A->data;
  IS           iscol = a->col,isrow = a->row;
  int          *r,*c,ierr,i, n = A->m,*vi,*ai = a->i,*aj = a->j;
  int          nz,*rout,*cout;
  PetscScalar  *x,*b,*tmp,*tmps,*aa = a->a,sum,*v;

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
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr); 
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  PetscLogFlops(2*a->nz - A->n);
  PetscFunctionReturn(0);
}

/* ----------------------------------------------------------- */
#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqAIJ_NaturalOrdering"
int MatSolve_SeqAIJ_NaturalOrdering(Mat A,Vec bb,Vec xx)
{
  Mat_SeqAIJ   *a = (Mat_SeqAIJ*)A->data;
  int          n = A->m,*ai = a->i,*aj = a->j,*adiag = a->diag,ierr;
  PetscScalar  *x,*b,*aa = a->a;
#if !defined(PETSC_USE_FORTRAN_KERNEL_SOLVEAIJ)
  int          adiag_i,i,*vi,nz,ai_i;
  PetscScalar  *v,sum;
#endif

  PetscFunctionBegin;
  if (!n) PetscFunctionReturn(0);

  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
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
  PetscLogFlops(2*a->nz - A->n);
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolveAdd_SeqAIJ"
int MatSolveAdd_SeqAIJ(Mat A,Vec bb,Vec yy,Vec xx)
{
  Mat_SeqAIJ   *a = (Mat_SeqAIJ*)A->data;
  IS           iscol = a->col,isrow = a->row;
  int          *r,*c,ierr,i, n = A->m,*vi,*ai = a->i,*aj = a->j;
  int          nz,*rout,*cout;
  PetscScalar  *x,*b,*tmp,*aa = a->a,sum,*v;

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
  PetscLogFlops(2*a->nz);

  PetscFunctionReturn(0);
}
/* -------------------------------------------------------------------*/
#undef __FUNCT__  
#define __FUNCT__ "MatSolveTranspose_SeqAIJ"
int MatSolveTranspose_SeqAIJ(Mat A,Vec bb,Vec xx)
{
  Mat_SeqAIJ   *a = (Mat_SeqAIJ*)A->data;
  IS           iscol = a->col,isrow = a->row;
  int          *r,*c,ierr,i,n = A->m,*vi,*ai = a->i,*aj = a->j;
  int          nz,*rout,*cout,*diag = a->diag;
  PetscScalar  *x,*b,*tmp,*aa = a->a,*v,s1;

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

  PetscLogFlops(2*a->nz-A->n);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolveTransposeAdd_SeqAIJ"
int MatSolveTransposeAdd_SeqAIJ(Mat A,Vec bb,Vec zz,Vec xx)
{
  Mat_SeqAIJ   *a = (Mat_SeqAIJ*)A->data;
  IS           iscol = a->col,isrow = a->row;
  int          *r,*c,ierr,i,n = A->m,*vi,*ai = a->i,*aj = a->j;
  int          nz,*rout,*cout,*diag = a->diag;
  PetscScalar  *x,*b,*tmp,*aa = a->a,*v;

  PetscFunctionBegin;
  if (zz != xx) VecCopy(zz,xx);

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

  PetscLogFlops(2*a->nz);
  PetscFunctionReturn(0);
}
/* ----------------------------------------------------------------*/
EXTERN int MatMissingDiagonal_SeqAIJ(Mat);

#undef __FUNCT__  
#define __FUNCT__ "MatILUFactorSymbolic_SeqAIJ"
int MatILUFactorSymbolic_SeqAIJ(Mat A,IS isrow,IS iscol,MatFactorInfo *info,Mat *fact)
{
  Mat_SeqAIJ *a = (Mat_SeqAIJ*)A->data,*b;
  IS         isicol;
  int        *r,*ic,ierr,prow,n = A->m,*ai = a->i,*aj = a->j;
  int        *ainew,*ajnew,jmax,*fill,*xi,nz,*im,*ajfill,*flev;
  int        *dloc,idx,row,m,fm,nzf,nzi,len, realloc = 0,dcount = 0;
  int        incrlev,nnz,i,levels,diagonal_fill;
  PetscTruth col_identity,row_identity;
  PetscReal  f;
 
  PetscFunctionBegin;
  f             = info->fill;
  levels        = (int)info->levels;
  diagonal_fill = (int)info->diagonal_fill;
  ierr = ISInvertPermutation(iscol,PETSC_DECIDE,&isicol);CHKERRQ(ierr);

  /* special case that simply copies fill pattern */
  ierr = ISIdentity(isrow,&row_identity);CHKERRQ(ierr);
  ierr = ISIdentity(iscol,&col_identity);CHKERRQ(ierr);
  if (!levels && row_identity && col_identity) {
    ierr = MatDuplicate_SeqAIJ(A,MAT_DO_NOT_COPY_VALUES,fact);CHKERRQ(ierr);
    (*fact)->factor = FACTOR_LU;
    b               = (Mat_SeqAIJ*)(*fact)->data;
    if (!b->diag) {
      ierr = MatMarkDiagonal_SeqAIJ(*fact);CHKERRQ(ierr);
    }
    ierr = MatMissingDiagonal_SeqAIJ(*fact);CHKERRQ(ierr);
    b->row              = isrow;
    b->col              = iscol;
    b->icol             = isicol;
    b->lu_damping       = info->damping;
    b->lu_zeropivot     = info->zeropivot;
    b->lu_shift         = info->shift;
    b->lu_shift_fraction= info->shift_fraction;
    ierr                = PetscMalloc(((*fact)->m+1)*sizeof(PetscScalar),&b->solve_work);CHKERRQ(ierr);
    (*fact)->ops->solve = MatSolve_SeqAIJ_NaturalOrdering;
    ierr                = PetscObjectReference((PetscObject)isrow);CHKERRQ(ierr);
    ierr                = PetscObjectReference((PetscObject)iscol);CHKERRQ(ierr);
    PetscFunctionReturn(0);
  }

  ierr = ISGetIndices(isrow,&r);CHKERRQ(ierr);
  ierr = ISGetIndices(isicol,&ic);CHKERRQ(ierr);

  /* get new row pointers */
  ierr = PetscMalloc((n+1)*sizeof(int),&ainew);CHKERRQ(ierr);
  ainew[0] = 0;
  /* don't know how many column pointers are needed so estimate */
  jmax = (int)(f*(ai[n]+1));
  ierr = PetscMalloc((jmax)*sizeof(int),&ajnew);CHKERRQ(ierr);
  /* ajfill is level of fill for each fill entry */
  ierr = PetscMalloc((jmax)*sizeof(int),&ajfill);CHKERRQ(ierr);
  /* fill is a linked list of nonzeros in active row */
  ierr = PetscMalloc((n+1)*sizeof(int),&fill);CHKERRQ(ierr);
  /* im is level for each filled value */
  ierr = PetscMalloc((n+1)*sizeof(int),&im);CHKERRQ(ierr);
  /* dloc is location of diagonal in factor */
  ierr = PetscMalloc((n+1)*sizeof(int),&dloc);CHKERRQ(ierr);
  dloc[0]  = 0;
  for (prow=0; prow<n; prow++) {

    /* copy current row into linked list */
    nzf     = nz  = ai[r[prow]+1] - ai[r[prow]];
    if (!nz) SETERRQ(PETSC_ERR_MAT_LU_ZRPVT,"Empty row in matrix");
    xi      = aj + ai[r[prow]] ;
    fill[n]    = n;
    fill[prow] = -1; /* marker to indicate if diagonal exists */
    while (nz--) {
      fm  = n;
      idx = ic[*xi++ ];
      do {
        m  = fm;
        fm = fill[m];
      } while (fm < idx);
      fill[m]   = idx;
      fill[idx] = fm;
      im[idx]   = 0;
    }

    /* make sure diagonal entry is included */
    if (diagonal_fill && fill[prow] == -1) {
      fm = n;
      while (fill[fm] < prow) fm = fill[fm];
      fill[prow] = fill[fm]; /* insert diagonal into linked list */
      fill[fm]   = prow;
      im[prow]   = 0;
      nzf++;
      dcount++;
    }

    nzi = 0;
    row = fill[n];
    while (row < prow) {
      incrlev = im[row] + 1;
      nz      = dloc[row];
      xi      = ajnew  + ainew[row]  + nz + 1;
      flev    = ajfill + ainew[row]  + nz + 1;
      nnz     = ainew[row+1] - ainew[row] - nz - 1;
      fm      = row;
      while (nnz-- > 0) {
        idx = *xi++ ;
        if (*flev + incrlev > levels) {
          flev++;
          continue;
        }
        do {
          m  = fm;
          fm = fill[m];
        } while (fm < idx);
        if (fm != idx) {
          im[idx]   = *flev + incrlev;
          fill[m]   = idx;
          fill[idx] = fm;
          fm        = idx;
          nzf++;
        } else {
          if (im[idx] > *flev + incrlev) im[idx] = *flev+incrlev;
        }
        flev++;
      }
      row = fill[row];
      nzi++;
    }
    /* copy new filled row into permanent storage */
    ainew[prow+1] = ainew[prow] + nzf;
    if (ainew[prow+1] > jmax) {

      /* estimate how much additional space we will need */
      /* use the strategy suggested by David Hysom <hysom@perch-t.icase.edu> */
      /* just double the memory each time */
      /*  maxadd = (int)((f*(ai[n]+!shift)*(n-prow+5))/n); */
      int maxadd = jmax;
      if (maxadd < nzf) maxadd = (n-prow)*(nzf+1);
      jmax += maxadd;

      /* allocate a longer ajnew and ajfill */
      ierr   = PetscMalloc(jmax*sizeof(int),&xi);CHKERRQ(ierr);
      ierr   = PetscMemcpy(xi,ajnew,(ainew[prow])*sizeof(int));CHKERRQ(ierr);
      ierr   = PetscFree(ajnew);CHKERRQ(ierr);
      ajnew  = xi;
      ierr   = PetscMalloc(jmax*sizeof(int),&xi);CHKERRQ(ierr);
      ierr   = PetscMemcpy(xi,ajfill,(ainew[prow])*sizeof(int));CHKERRQ(ierr);
      ierr   = PetscFree(ajfill);CHKERRQ(ierr);
      ajfill = xi;
      realloc++; /* count how many times we realloc */
    }
    xi          = ajnew + ainew[prow] ;
    flev        = ajfill + ainew[prow] ;
    dloc[prow]  = nzi;
    fm          = fill[n];
    while (nzf--) {
      *xi++   = fm ;
      *flev++ = im[fm];
      fm      = fill[fm];
    }
    /* make sure row has diagonal entry */
    if (ajnew[ainew[prow]+dloc[prow]] != prow) {
      SETERRQ1(PETSC_ERR_MAT_LU_ZRPVT,"Row %d has missing diagonal in factored matrix\n\
    try running with -pc_ilu_nonzeros_along_diagonal or -pc_ilu_diagonal_fill",prow);
    }
  }
  ierr = PetscFree(ajfill);CHKERRQ(ierr);
  ierr = ISRestoreIndices(isrow,&r);CHKERRQ(ierr);
  ierr = ISRestoreIndices(isicol,&ic);CHKERRQ(ierr);
  ierr = PetscFree(fill);CHKERRQ(ierr);
  ierr = PetscFree(im);CHKERRQ(ierr);

  {
    PetscReal af = ((PetscReal)ainew[n])/((PetscReal)ai[n]);
    PetscLogInfo(A,"MatILUFactorSymbolic_SeqAIJ:Reallocs %d Fill ratio:given %g needed %g\n",realloc,f,af);
    PetscLogInfo(A,"MatILUFactorSymbolic_SeqAIJ:Run with -[sub_]pc_ilu_fill %g or use \n",af);
    PetscLogInfo(A,"MatILUFactorSymbolic_SeqAIJ:PCILUSetFill([sub]pc,%g);\n",af);
    PetscLogInfo(A,"MatILUFactorSymbolic_SeqAIJ:for best performance.\n");
    if (diagonal_fill) {
      PetscLogInfo(A,"MatILUFactorSymbolic_SeqAIJ:Detected and replaced %d missing diagonals",dcount);
    }
  }

  /* put together the new matrix */
  ierr = MatCreate(A->comm,n,n,n,n,fact);CHKERRQ(ierr);
  ierr = MatSetType(*fact,A->type_name);CHKERRQ(ierr);
  ierr = MatSeqAIJSetPreallocation(*fact,0,PETSC_NULL);CHKERRQ(ierr);
  PetscLogObjectParent(*fact,isicol);
  b = (Mat_SeqAIJ*)(*fact)->data;
  ierr = PetscFree(b->imax);CHKERRQ(ierr);
  b->singlemalloc = PETSC_FALSE;
  len = (ainew[n] )*sizeof(PetscScalar);
  /* the next line frees the default space generated by the Create() */
  ierr = PetscFree(b->a);CHKERRQ(ierr);
  ierr = PetscFree(b->ilen);CHKERRQ(ierr);
  ierr = PetscMalloc(len+1,&b->a);CHKERRQ(ierr);
  b->j          = ajnew;
  b->i          = ainew;
  for (i=0; i<n; i++) dloc[i] += ainew[i];
  b->diag       = dloc;
  b->ilen       = 0;
  b->imax       = 0;
  b->row        = isrow;
  b->col        = iscol;
  ierr          = PetscObjectReference((PetscObject)isrow);CHKERRQ(ierr);
  ierr          = PetscObjectReference((PetscObject)iscol);CHKERRQ(ierr);
  b->icol       = isicol;
  ierr = PetscMalloc((n+1)*sizeof(PetscScalar),&b->solve_work);CHKERRQ(ierr);
  /* In b structure:  Free imax, ilen, old a, old j.  
     Allocate dloc, solve_work, new a, new j */
  PetscLogObjectMemory(*fact,(ainew[n]-n) * (sizeof(int)+sizeof(PetscScalar)));
  b->maxnz             = b->nz = ainew[n] ;
  b->lu_damping        = info->damping;
  b->lu_shift          = info->shift;
  b->lu_shift_fraction = info->shift_fraction;
  b->lu_zeropivot = info->zeropivot;
  (*fact)->factor = FACTOR_LU;
  ierr = Mat_AIJ_CheckInode(*fact,PETSC_FALSE);CHKERRQ(ierr);
  (*fact)->ops->lufactornumeric =  A->ops->lufactornumeric; /* Use Inode variant ONLY if A has inodes */

  (*fact)->info.factor_mallocs    = realloc;
  (*fact)->info.fill_ratio_given  = f;
  (*fact)->info.fill_ratio_needed = ((PetscReal)ainew[n])/((PetscReal)ai[prow]);
  PetscFunctionReturn(0); 
}

#include "src/mat/impls/sbaij/seq/sbaij.h"
#undef __FUNCT__  
#define __FUNCT__ "MatCholeskyFactorNumeric_SeqAIJ"
int MatCholeskyFactorNumeric_SeqAIJ(Mat A,Mat *fact)
{
  Mat_SeqAIJ          *a = (Mat_SeqAIJ*)A->data;
  int                 ierr;

  PetscFunctionBegin; 
  if (!a->sbaijMat){
    ierr = MatConvert(A,MATSEQSBAIJ,&a->sbaijMat);CHKERRQ(ierr); 
  } 
  
  ierr = MatCholeskyFactorNumeric_SeqSBAIJ_1_NaturalOrdering(a->sbaijMat,fact);CHKERRQ(ierr);
  ierr = MatDestroy(a->sbaijMat);CHKERRQ(ierr);
  a->sbaijMat = PETSC_NULL; 
  
  PetscFunctionReturn(0); 
}

#undef __FUNCT__  
#define __FUNCT__ "MatICCFactorSymbolic_SeqAIJ"
int MatICCFactorSymbolic_SeqAIJ(Mat A,IS perm,MatFactorInfo *info,Mat *fact)
{
  Mat_SeqAIJ          *a = (Mat_SeqAIJ*)A->data;
  int                 ierr;
  PetscTruth          perm_identity;
 
  PetscFunctionBegin;   
  ierr = ISIdentity(perm,&perm_identity);CHKERRQ(ierr);
  if (!perm_identity){
    SETERRQ(1,"Non-identity permutation is not supported yet");
  }
  if (!a->sbaijMat){
    ierr = MatConvert(A,MATSEQSBAIJ,&a->sbaijMat);CHKERRQ(ierr);
  }

  ierr = MatICCFactorSymbolic(a->sbaijMat,perm,info,fact);CHKERRQ(ierr);
  (*fact)->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqAIJ;
 
  PetscFunctionReturn(0); 
}

#undef __FUNCT__  
#define __FUNCT__ "MatCholeskyFactorSymbolic_SeqAIJ"
int MatCholeskyFactorSymbolic_SeqAIJ(Mat A,IS perm,MatFactorInfo *info,Mat *fact)
{
  Mat_SeqAIJ          *a = (Mat_SeqAIJ*)A->data;
  int                 ierr;
  PetscTruth          perm_identity;
 
  PetscFunctionBegin;   
  ierr = ISIdentity(perm,&perm_identity);CHKERRQ(ierr);
  if (!perm_identity){
    SETERRQ(1,"Non-identity permutation is not supported yet");
  }
  if (!a->sbaijMat){
    ierr = MatConvert(A,MATSEQSBAIJ,&a->sbaijMat);CHKERRQ(ierr);
  }

  ierr = MatCholeskyFactorSymbolic(a->sbaijMat,perm,info,fact);CHKERRQ(ierr);   
  (*fact)->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqAIJ;
 
  PetscFunctionReturn(0); 
}
