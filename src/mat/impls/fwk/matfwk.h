#if !defined(__MATFWK_H)
#define __MATFWK_H

#include "private/matimpl.h"
#include "../src/vec/vec/impls/dvecimpl.h"


typedef struct {
  Mat mat;
  Vec invec, outvec;
} Mat_FwkBlock;

#define Mat_FwkBlockGetMat(A,rowblock,colblock,_block, _mat) 0; *_mat = _block->mat

typedef struct {
  PetscInt          nonew;            /* 1 don't add new nonzero blocks, -1 generate error on new */
  PetscInt          nounused;         /* -1 generate error on unused space */
  PetscTruth        singlemalloc;     /* if true a, i, and j have been obtained with one big malloc */
  PetscInt          maxnz;            /* allocated nonzeros */
  PetscInt          *imax;            /* maximum space allocated for each row */
  PetscInt          *ilen;            /* actual length of each row */
  PetscTruth        free_imax_ilen;  
  PetscInt          reallocs;         /* number of mallocs done during MatFwkAddBlock()
                                        as more blocks are set than were prealloced */
  PetscInt          rmax;             /* max nonzeros in any row */\
  PetscTruth        free_ij;          /* free the column indices j and row offsets i when the matrix is destroyed */ \
  PetscTruth        free_a;           /* free the numerical values when matrix is destroy */ 
  PetscInt          nz;               /* nonzero blocks */                                       
  PetscInt          *i;               /* pointer to beginning of each row */               
  PetscInt          *j;               /* column values: j + i[k] - 1 is start of row k */  
  Mat_FwkBlock      *a;               /* nonzero blocks */                               
} Mat_FwkAIJ;

typedef struct {
  const MatType    default_block_type;
  Mat *scatters; /* FIX: merge into a single scatter to ammortize communication setup costs */
  Mat *gathers;  /* FIX: merge into a single gather to ammortize communication setup costs */
  PetscInt colblockcount;
  PetscInt rowblockcount;
  Vec      *invecs, *outvecs;     /* vectors to scatter into/gather from; represent the direct sum of all vectors scattered/gathered; FIX: will be obsolete as soon as scatters/gathers are merged */
  Vec      invec, outvec;
  PetscInt *lcolblockoffset, *gcolblockoffset; /* Cumulative local and global row dimensions of scatters. */
  PetscInt *lrowblockoffset, *growblockoffset; /* Cumulative local and global column dimensions of gathers.  */                          
  void     *data;
} Mat_Fwk;


/*
    Frees the a, i, and j arrays from the FwkAIJ matrix type
*/
#undef __FUNCT__  
#define __FUNCT__ "MatSeqXAIJFreeAIJ"
PETSC_STATIC_INLINE PetscErrorCode MatFwkXAIJFreeAIJ(Mat AAA, Mat_FwkBlock **a,PetscInt **j,PetscInt **i) 
{
  PetscErrorCode ierr;
                                     Mat_Fwk     *AA = (Mat_Fwk*) AAA->data;
                                     Mat_FwkAIJ  *A  = (Mat_FwkAIJ*) AA->data;
                                     if (A->singlemalloc) {
                                       ierr = PetscFree3(*a,*j,*i);CHKERRQ(ierr);
                                     } else {
                                       if (A->free_a  && *a) {ierr = PetscFree(*a);CHKERRQ(ierr);}
                                       if (A->free_ij && *j) {ierr = PetscFree(*j);CHKERRQ(ierr);}
                                       if (A->free_ij && *i) {ierr = PetscFree(*i);CHKERRQ(ierr);}
                                     }
                                     *a = 0; *j = 0; *i = 0;
                                     return 0;
}

#define CHUNKSIZE 15
/*
    Allocates larger a, i, and j arrays for the FwkAIJ matrix type
*/
#define MatFwkXAIJReallocateAIJ(matrix,rowcount,current_row_length,row,col,allocated_row_length,aa,ai,aj,j_row_pointer,a_row_pointer,allocated_row_lengths,no_new_block_flag) \
{\
  Mat_Fwk *fwk = (Mat_Fwk*)matrix->data;\
  Mat_FwkAIJ *A = (Mat_FwkAIJ*)fwk->data;\
  PetscInt AM = rowcount, NROW = current_row_length, ROW = row, COL = col, RMAX = allocated_row_length;\
  Mat_FwkBlock *AA = aa;\
  PetscInt *AI = ai, *AJ = aj, *RP = j_row_pointer;\
  Mat_FwkBlock *AP = a_row_pointer;\
  PetscInt *AIMAX = allocated_row_lengths;\
  PetscTruth NONEW=(no_new_block_flag) ? PETSC_TRUE: PETSC_FALSE;	\
  if (NROW >= RMAX) {\
        /* there is no extra room in row, therefore enlarge */ \
        PetscInt   new_nz = AI[AM] + CHUNKSIZE,len,*new_i=0,*new_j=0; \
        Mat_FwkBlock *new_a; \
 \
        if (NONEW == -2) SETERRQ2(PETSC_ERR_ARG_OUTOFRANGE,"New nonzero at (%D,%D) caused a malloc",ROW,COL); \
        /* malloc new storage space */ \
        ierr = PetscMalloc3(new_nz,Mat_FwkBlock,&new_a,new_nz,PetscInt,&new_j,AM+1,PetscInt,&new_i);CHKERRQ(ierr);\
 \
        /* copy over old data into new slots */ \
        for (ii=0; ii<ROW+1; ii++) {new_i[ii] = AI[ii];} \
        for (ii=ROW+1; ii<AM+1; ii++) {new_i[ii] = AI[ii]+CHUNKSIZE;} \
        ierr = PetscMemcpy(new_j,AJ,(AI[ROW]+NROW)*sizeof(PetscInt));CHKERRQ(ierr); \
        len = (new_nz - CHUNKSIZE - AI[ROW] - NROW); \
        ierr = PetscMemcpy(new_j+AI[ROW]+NROW+CHUNKSIZE,AJ+AI[ROW]+NROW,len*sizeof(PetscInt));CHKERRQ(ierr); \
        ierr = PetscMemcpy(new_a,AA,(AI[ROW]+NROW)*sizeof(Mat_FwkBlock));CHKERRQ(ierr); \
        ierr = PetscMemzero(new_a+(AI[ROW]+NROW),CHUNKSIZE*sizeof(Mat_FwkBlock));CHKERRQ(ierr);\
        ierr = PetscMemcpy(new_a+(AI[ROW]+NROW+CHUNKSIZE),AA+(AI[ROW]+NROW),len*sizeof(Mat_FwkBlock));CHKERRQ(ierr);  \
        /* free up old matrix storage */ \
        ierr = MatFwkXAIJFreeAIJ(matrix,&A->a,&A->j,&A->i);CHKERRQ(ierr);\
        AA = new_a; \
        A->a = (Mat_FwkBlock*) new_a;		   \
        AI = A->i = new_i; AJ = A->j = new_j;  \
        A->singlemalloc = PETSC_TRUE; \
 \
        RP          = AJ + AI[ROW]; AP = AA + AI[ROW]; \
        RMAX        = AIMAX[ROW] = AIMAX[ROW] + CHUNKSIZE; \
        A->maxnz += CHUNKSIZE; \
        A->reallocs++; \
      } \
} \


#endif
