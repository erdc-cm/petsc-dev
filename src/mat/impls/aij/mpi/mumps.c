/*$Id: mumps.c,v 1.10 2001/08/15 15:56:50 bsmith Exp $*/
/* 
    Provides an interface to the MUMPS_4.2_beta sparse solver
*/

#include "src/mat/impls/aij/seq/aij.h"
#include "src/mat/impls/aij/mpi/mpiaij.h"

#if defined(PETSC_HAVE_MUMPS) && !defined(PETSC_USE_SINGLE) && !defined(PETSC_USE_COMPLEX)
EXTERN_C_BEGIN 
#include "dmumps_c.h" 
EXTERN_C_END 
#define JOB_INIT -1
#define JOB_END -2
/* macro s.t. indices match MUMPS documentation */
#define ICNTL(I) icntl[(I)-1] 
#define INFOG(I) infog[(I)-1]
#define RINFOG(I) rinfog[(I)-1]

typedef struct {
  DMUMPS_STRUC_C id;
  MatStructure   matstruc;
  int            myid,size,*irn,*jcn;
  PetscScalar    *val;
} Mat_MPIAIJ_MUMPS;

/* convert Petsc mpiaij matrix to triples: row[nz], col[nz], val[nz] */
/*
  input: 
    A       - matrix in mpiaij format
    shift   - 0: C style output triple; 1: Fortran style output triple.
    valOnly - FALSE: spaces are allocated and values are set for the triple  
              TRUE:  only the values in v array are updated
  output:
    nnz     - dim of r, c, and v (number of local nonzero entries of A)
    r, c, v - row and col index, matrix values (matrix triples) 
 */
int MatConvertToTriples(Mat A,int shift,PetscTruth valOnly,int *nnz,int **r, int **c, PetscScalar **v)
{
  Mat_MPIAIJ   *mat =  (Mat_MPIAIJ*)A->data;  
  Mat_SeqAIJ   *aa=(Mat_SeqAIJ*)(mat->A)->data;
  Mat_SeqAIJ   *bb=(Mat_SeqAIJ*)(mat->B)->data;
  int          *ai=aa->i, *aj=aa->j, *bi=bb->i, *bj=bb->j, rstart= mat->rstart,
               nz = aa->nz + bb->nz, *garray = mat->garray;
  int          ierr,i,j,jj,jB,irow,m=A->m,*ajj,*bjj,countA,countB,colA_start,jcol;
  int          *row,*col;
  PetscScalar  *av=aa->a, *bv=bb->a,*val;

  PetscFunctionBegin;
  if (!valOnly){ 
    ierr = PetscMalloc(nz*sizeof(int),&row);CHKERRQ(ierr);
    ierr = PetscMalloc(nz*sizeof(int),&col);CHKERRQ(ierr);
    ierr = PetscMalloc(nz*sizeof(PetscScalar),&val);CHKERRQ(ierr);
    *r = row; *c = col; *v = val;
  } else {
    row = *r; col = *c; val = *v; 
  }
  *nnz = nz; 

  jj = 0; jB = 0; irow = rstart;   
  for ( i=0; i<m; i++ ) {
    ajj = aj + ai[i];                 /* ptr to the beginning of this row */      
    countA = ai[i+1] - ai[i];
    countB = bi[i+1] - bi[i];
    bjj = bj + bi[i];  
  
    /* B part, smaller col index */   
    colA_start = rstart + ajj[0]; /* the smallest col index for A */  
    for (j=0; j<countB; j++){
      jcol = garray[bjj[j]];
      if (jcol > colA_start) {
        jB = j;
        break;
      }
      if (!valOnly){ 
        row[jj] = irow + shift; col[jj] = jcol + shift; 
      }
      val[jj++] = *bv++;
      if (j==countB-1) jB = countB; 
    }
    /* A part */
    for (j=0; j<countA; j++){
      if (!valOnly){
        row[jj] = irow + shift; col[jj] = rstart + ajj[j] + shift; 
      }
      val[jj++] = *av++;
    }
    /* B part, larger col index */      
    for (j=jB; j<countB; j++){
      if (!valOnly){
        row[jj] = irow + shift; col[jj] = garray[bjj[j]] + shift;
      }
      val[jj++] = *bv++;
    }
    irow++;
  } 
  
  PetscFunctionReturn(0);
}

extern int MatDestroy_MPIAIJ(Mat);
extern int MatDestroy_SeqAIJ(Mat);

#undef __FUNCT__  
#define __FUNCT__ "MatDestroy_MPIAIJ_MUMPS"
int MatDestroy_MPIAIJ_MUMPS(Mat A)
{
  Mat_MPIAIJ_MUMPS *lu = (Mat_MPIAIJ_MUMPS*)A->spptr; 
  int              ierr,size=lu->size;

  PetscFunctionBegin; 
  /* Terminate instance, deallocate memories */
  lu->id.job=JOB_END; dmumps_c(&lu->id); 
  if (lu->irn) { ierr = PetscFree(lu->irn);CHKERRQ(ierr);}
  if (lu->jcn) { ierr = PetscFree(lu->jcn);CHKERRQ(ierr);}
  if (size>1 && lu->val) { ierr = PetscFree(lu->val);CHKERRQ(ierr);} 
  
  ierr = PetscFree(lu);CHKERRQ(ierr); 
  if (size == 1){
    ierr = MatDestroy_SeqAIJ(A);CHKERRQ(ierr);
  } else {
    ierr = MatDestroy_MPIAIJ(A);CHKERRQ(ierr);
  } 
  
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolve_MPIAIJ_MUMPS"
int MatSolve_MPIAIJ_MUMPS(Mat A,Vec b,Vec x)
{
  Mat_MPIAIJ_MUMPS *lu = (Mat_MPIAIJ_MUMPS*)A->spptr; 
  PetscScalar      *rhs,*array;
  Vec              x_seq;
  IS               iden;
  VecScatter       scat;
  int              ierr;

  PetscFunctionBegin; 
  PetscPrintf(A->comm," ... Solve_\n");
  if (lu->size > 1){
    if (!lu->myid){
      ierr = VecCreateSeq(PETSC_COMM_SELF,A->N,&x_seq);CHKERRQ(ierr);
      ierr = ISCreateStride(PETSC_COMM_SELF,A->N,0,1,&iden);CHKERRQ(ierr);
    } else {
      ierr = VecCreateSeq(PETSC_COMM_SELF,0,&x_seq);CHKERRQ(ierr);
      ierr = ISCreateStride(PETSC_COMM_SELF,0,0,1,&iden);CHKERRQ(ierr);
    }
    ierr = VecScatterCreate(b,iden,x_seq,iden,&scat);CHKERRQ(ierr);
    ierr = ISDestroy(iden);CHKERRQ(ierr);

    ierr = VecScatterBegin(b,x_seq,INSERT_VALUES,SCATTER_FORWARD,scat);CHKERRQ(ierr);
    ierr = VecScatterEnd(b,x_seq,INSERT_VALUES,SCATTER_FORWARD,scat);CHKERRQ(ierr);
    if (!lu->myid) {ierr = VecGetArray(x_seq,&array);CHKERRQ(ierr);}
  } else {  /* size == 1 */
    ierr = VecCopy(b,x);CHKERRQ(ierr);
    ierr = VecGetArray(x,&array);CHKERRQ(ierr);
  }
  if (!lu->myid) { /* define rhs on the host */
    lu->id.rhs = array;
  }

  /* solve phase */
  lu->id.job=3;
  dmumps_c(&lu->id);
  if (lu->id.INFOG(1) < 0) {   
    SETERRQ1(1,"Error reported by MUMPS in solve phase: INFOG(1)=%d\n",lu->id.INFOG(1));
  }

  /* convert mumps solution x_seq to petsc mpi x */
  if (lu->size > 1) {
    if (!lu->myid){
      ierr = VecRestoreArray(x_seq,&array);CHKERRQ(ierr);
    }
    ierr = VecScatterBegin(x_seq,x,INSERT_VALUES,SCATTER_REVERSE,scat);CHKERRQ(ierr);
    ierr = VecScatterEnd(x_seq,x,INSERT_VALUES,SCATTER_REVERSE,scat);CHKERRQ(ierr);
    ierr = VecScatterDestroy(scat);CHKERRQ(ierr);
    ierr = VecDestroy(x_seq);CHKERRQ(ierr);
  } else {
    ierr = VecRestoreArray(x,&array);CHKERRQ(ierr);
  } 
   
  PetscFunctionReturn(0);
}

#undef __FUNCT__   
#define __FUNCT__ "MatLUFactorNumeric_MPIAIJ_MUMPS"
int MatLUFactorNumeric_MPIAIJ_MUMPS(Mat A,Mat *F)
{
  Mat_MPIAIJ_MUMPS *lu = (Mat_MPIAIJ_MUMPS*)(*F)->spptr; 
  int              rnz,*aj,nnz,ierr,nz,i,M=A->M;
  Mat_SeqAIJ       *aa; 
  PetscTruth       valOnly;

  PetscFunctionBegin; 	
  PetscPrintf(A->comm," ... Numeric_, par: %d, ICNTL(18): %d, ICNTL(7): %d\n", lu->id.par,lu->id.ICNTL(18),lu->id.ICNTL(7));
  /* define matrix A */
  switch (lu->id.ICNTL(18)){
  case 0:  /* centralized assembled matrix input (size=1) */
    if (!lu->myid) {
      aa      = (Mat_SeqAIJ*)(A)->data;
      nz      = aa->nz;
      lu->val = aa->a;
      if (lu->matstruc == DIFFERENT_NONZERO_PATTERN){ /* first numeric factorization, get irn and jcn */
        ierr = PetscMalloc(nz*sizeof(int),&lu->irn);CHKERRQ(ierr);
        ierr = PetscMalloc(nz*sizeof(int),&lu->jcn);CHKERRQ(ierr);
        aj   = aa->j;
        nz = 0;
        for (i=0; i<M; i++){
          rnz = aa->i[i+1] - aa->i[i];
          while (rnz--) {  /* Fortran row/col index! */
            lu->irn[nz] = i+1; lu->jcn[nz] = (*aj)+1; aj++; nz++; 
          }
        }
      }
    }
    break;
  case 3:  /* distributed assembled matrix input (size>1) */
    if (lu->matstruc == DIFFERENT_NONZERO_PATTERN){
      valOnly = PETSC_FALSE; 
    } else {
      valOnly = PETSC_TRUE; /* only update mat values, not row and col index */
    }
    ierr = MatConvertToTriples(A, 1, valOnly, &nnz, &lu->irn, &lu->jcn, &lu->val);CHKERRQ(ierr);
    break;
  default: SETERRQ(PETSC_ERR_SUP,"Matrix input format is not supported by MUMPS.");
  }

  /* analysis phase */
  if (lu->matstruc == DIFFERENT_NONZERO_PATTERN){ 
     lu->id.n = M;
    switch (lu->id.ICNTL(18)){
    case 0:  /* centralized assembled matrix input */
      if (!lu->myid) {
        lu->id.nz =nz; lu->id.irn=lu->irn; lu->id.jcn=lu->jcn;
        if (lu->id.ICNTL(6)>1) lu->id.a = lu->val; 
      }
      break;
    case 3:  /* distributed assembled matrix input (size>1) */ 
      lu->id.nz_loc = nnz; 
      lu->id.irn_loc=lu->irn; lu->id.jcn_loc=lu->jcn;
      if (lu->id.ICNTL(6)>1) lu->id.a_loc = lu->val;
      break;
    }    
    lu->id.job=1;
    dmumps_c(&lu->id);
    if (lu->id.INFOG(1) < 0) { 
      SETERRQ1(1,"Error reported by MUMPS in analysis phase: INFOG(1)=%d\n",lu->id.INFOG(1)); 
    }
  }

  /* numerical factorization phase */
  if(lu->id.ICNTL(18) == 0) { 
    if (lu->myid == 0) lu->id.a = lu->val; 
  } else {
    lu->id.a_loc = lu->val; 
  }
  lu->id.job=2;
  dmumps_c(&lu->id);
  if (lu->id.INFOG(1) < 0) {
    SETERRQ1(1,"1, Error reported by MUMPS in numerical factorization phase: INFOG(1)=%d\n",lu->id.INFOG(1)); 
  }
  
  (*F)->assembled = PETSC_TRUE;
  lu->matstruc    = SAME_NONZERO_PATTERN;
  PetscFunctionReturn(0);
}

/* Note the Petsc r and c permutations are ignored */
#undef __FUNCT__  
#define __FUNCT__ "MatLUFactorSymbolic_MPIAIJ_MUMPS"
int MatLUFactorSymbolic_MPIAIJ_MUMPS(Mat A,IS r,IS c,MatFactorInfo *info,Mat *F)
{
  Mat_MPIAIJ              *fac;
  Mat_MPIAIJ_MUMPS        *lu;   
  int                     ierr,icntl;
  PetscTruth              flg;

  PetscFunctionBegin; 	
  ierr = PetscNew(Mat_MPIAIJ_MUMPS,&lu);CHKERRQ(ierr); 

  /* Create the factorization matrix F */ 
  ierr = MatCreateMPIAIJ(A->comm,A->m,A->n,A->M,A->N,0,PETSC_NULL,0,PETSC_NULL,F);CHKERRQ(ierr);

  (*F)->ops->lufactornumeric  = MatLUFactorNumeric_MPIAIJ_MUMPS;
  (*F)->ops->solve            = MatSolve_MPIAIJ_MUMPS;
  (*F)->ops->destroy          = MatDestroy_MPIAIJ_MUMPS;  
  (*F)->factor                = FACTOR_LU;  
  (*F)->spptr                  = (void*)lu;
  fac                         = (Mat_MPIAIJ*)(*F)->data; 
  
  /* Initialize a MUMPS instance */
  ierr = MPI_Comm_rank(A->comm, &lu->myid);
  ierr = MPI_Comm_size(A->comm,&lu->size);CHKERRQ(ierr);
  lu->id.job = JOB_INIT; 
  lu->id.comm_fortran = A->comm;

  /* Set default options */
  lu->id.par=1;  /* host participates factorizaton and solve */
  lu->id.sym=0;  /* matrix symmetry - 0: unsymmetric; 1: spd; 2: general symmetric. */
  dmumps_c(&lu->id);

  if (lu->size == 1){
    lu->id.ICNTL(18) = 0;   /* centralized assembled matrix input */
  } else {
    lu->id.ICNTL(18) = 3;   /* distributed assembled matrix input */
  }

  /* Get runtime options */
  ierr = PetscOptionsBegin(A->comm,A->prefix,"MUMPS Options","Mat");CHKERRQ(ierr);
  icntl=-1;
  ierr = PetscOptionsInt("-mat_mumps_icntl_4","ICNTL(4): level of printing (0 to 4)","None",lu->id.ICNTL(4),&icntl,&flg);CHKERRQ(ierr);
  if (flg && icntl > 0) {
    lu->id.ICNTL(4)=icntl; /* and use mumps default icntl(i), i=1,2,3 */
  } else { /* no output */
    lu->id.ICNTL(1) = 0;  /* error message, default= 6 */
    lu->id.ICNTL(2) = -1; /* output stream for diagnostic printing, statistics, and warning. default=0 */
    lu->id.ICNTL(3) = -1; /* output stream for global information, default=6 */
    lu->id.ICNTL(4) = 0;  /* level of printing, 0,1,2,3,4, default=2 */
  }
  ierr = PetscOptionsInt("-mat_mumps_icntl_6","ICNTL(6): matrix prescaling (0 to 7)","None",lu->id.ICNTL(6),&lu->id.ICNTL(6),PETSC_NULL);CHKERRQ(ierr);
  icntl=-1;
  ierr = PetscOptionsInt("-mat_mumps_icntl_7","ICNTL(7): matrix ordering (0 to 7)","None",lu->id.ICNTL(7),&icntl,&flg);CHKERRQ(ierr);
  if (flg) {
    if (icntl== 1){
      SETERRQ(PETSC_ERR_SUP,"pivot order be set by the user in PERM_IN -- not supported by the PETSc/MUMPS interface\n");
    } else {
      lu->id.ICNTL(7) = icntl;
    }
  } 
  ierr = PetscOptionsInt("-mat_mumps_icntl_9","ICNTL(9): A or A^T x=b to be solved. 1: A; otherwise: A^T","None",lu->id.ICNTL(9),&lu->id.ICNTL(9),PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsInt("-mat_mumps_icntl_10","ICNTL(10): max num of refinements","None",lu->id.ICNTL(10),&lu->id.ICNTL(10),PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsInt("-mat_mumps_icntl_11","ICNTL(11): error analysis, a positive value returns statistics","None",lu->id.ICNTL(11),&lu->id.ICNTL(11),PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsInt("-mat_mumps_icntl_12","ICNTL(12): efficiency control","None",lu->id.ICNTL(12),&lu->id.ICNTL(12),PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsInt("-mat_mumps_icntl_13","ICNTL(13): efficiency control","None",lu->id.ICNTL(13),&lu->id.ICNTL(13),PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsInt("-mat_mumps_icntl_14","ICNTL(14): efficiency control","None",lu->id.ICNTL(14),&lu->id.ICNTL(14),PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsInt("-mat_mumps_icntl_15","ICNTL(15): efficiency control","None",lu->id.ICNTL(15),&lu->id.ICNTL(15),PETSC_NULL);CHKERRQ(ierr);
  PetscOptionsEnd();

  lu->matstruc = DIFFERENT_NONZERO_PATTERN; 
  PetscFunctionReturn(0); 
}

#undef __FUNCT__  
#define __FUNCT__ "MatUseMUMPS_MPIAIJ"
int MatUseMUMPS_MPIAIJ(Mat A)
{
  PetscFunctionBegin;
  A->ops->lufactorsymbolic = MatLUFactorSymbolic_MPIAIJ_MUMPS;
  A->ops->lufactornumeric  = MatLUFactorNumeric_MPIAIJ_MUMPS; 
  PetscFunctionReturn(0);
}

int MatFactorInfo_MUMPS(Mat A,PetscViewer viewer)
{
  Mat_MPIAIJ_MUMPS *lu= (Mat_MPIAIJ_MUMPS*)A->spptr;
  int              ierr;

  PetscFunctionBegin;
  /* check if matrix is mumps type */
  if (A->ops->solve != MatSolve_MPIAIJ_MUMPS) PetscFunctionReturn(0); 

  ierr = PetscViewerASCIIPrintf(viewer,"MUMPS run parameters:\n");CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer,"  SYM (matrix type):                  %d \n",lu->id.sym);CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer,"  PAR (host participation):           %d \n",lu->id.par);CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer,"  ICNTL(4) (level of printing):       %d \n",lu->id.ICNTL(4));CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer,"  ICNTL(5) (input mat struct):        %d \n",lu->id.ICNTL(5));CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer,"  ICNTL(6) (matrix prescaling):       %d \n",lu->id.ICNTL(6));CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer,"  ICNTL(7) (matrix ordering):         %d \n",lu->id.ICNTL(7));CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer,"  ICNTL(9) (A/A^T x=b is solved):     %d \n",lu->id.ICNTL(9));CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer,"  ICNTL(10) (max num of refinements): %d \n",lu->id.ICNTL(10));CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer,"  ICNTL(11) (error analysis):         %d \n",lu->id.ICNTL(11));CHKERRQ(ierr);  
  if (lu->myid == 0 && lu->id.ICNTL(11)>0) {
    ierr = PetscPrintf(PETSC_COMM_SELF,"        RINFOG(4) (inf norm of input mat):        %g\n",lu->id.RINFOG(4));CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_SELF,"        RINFOG(5) (inf norm of solution):         %g\n",lu->id.RINFOG(5));CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_SELF,"        RINFOG(6) (inf norm of residual):         %g\n",lu->id.RINFOG(6));CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_SELF,"        RINFOG(7),RINFOG(8) (backward error est): %g\n",lu->id.RINFOG(7),lu->id.RINFOG(8));CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_SELF,"        RINFOG(9) (error estimate):               %g \n",lu->id.RINFOG(9));CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_SELF,"        RINFOG(10),RINFOG(11)(condition numbers): %g, %g\n",lu->id.RINFOG(10),lu->id.RINFOG(11));CHKERRQ(ierr);
  
  }
  ierr = PetscViewerASCIIPrintf(viewer,"  ICNTL(18) (input mat struct):       %d \n",lu->id.ICNTL(18));CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#else

#undef __FUNCT__  
#define __FUNCT__ "MatUseMUMPS_MPIAIJ"
int MatUseMUMPS_MPIAIJ(Mat A)
{
  PetscFunctionBegin;
  PetscFunctionReturn(0);
}

#endif


