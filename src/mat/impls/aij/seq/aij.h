
#if !defined(__AIJ_H)
#define __AIJ_H
#include "src/mat/matimpl.h"

/* Info about i-nodes (identical nodes) */
typedef struct {
  PetscTruth use;
  int        node_count;                    /* number of inodes */
  int        *size;                         /* size of each inode */
  int        limit;                         /* inode limit */
  int        max_limit;                     /* maximum supported inode limit */
  PetscTruth checked;                       /* if inodes have been checked for */
} Mat_SeqAIJ_Inode;

/*  
  MATSEQAIJ format - Compressed row storage (also called Yale sparse matrix
  format).  The i[] and j[] arrays start at 0. For example,
  j[i[k]+p] is the pth column in row k.  Note that the diagonal
  matrix elements are stored with the rest of the nonzeros (not separately).
*/

typedef struct {
  PetscTruth       sorted;           /* if true, rows are sorted by increasing columns */
  PetscTruth       roworiented;      /* if true, row-oriented input, default */
  int              nonew;            /* 1 don't add new nonzeros, -1 generate error on new */
  PetscTruth       singlemalloc;     /* if true a, i, and j have been obtained with
                                          one big malloc */
  PetscTruth       freedata;        /* free the i,j,a data when the matrix is destroyed; true by default */
  int              nz,maxnz;        /* nonzeros, allocated nonzeros */
  int              *diag;            /* pointers to diagonal elements */
  int              *i;               /* pointer to beginning of each row */
  int              *imax;            /* maximum space allocated for each row */
  int              *ilen;            /* actual length of each row */
  int              *j;               /* column values: j + i[k] - 1 is start of row k */
  PetscScalar      *a;               /* nonzero elements */
  IS               row,col,icol;   /* index sets, used for reorderings */
  PetscScalar      *solve_work;      /* work space used in MatSolve */
  Mat_SeqAIJ_Inode inode;            /* identical node informaton */
  int              reallocs;         /* number of mallocs done during MatSetValues() 
                                        as more values are set than were prealloced */
  int              rmax;             /* max nonzeros in any row */
  PetscTruth       ilu_preserve_row_sums;
  PetscReal        lu_dtcol;
  PetscReal        lu_damping;
  PetscReal        lu_shift;         /* Manteuffel shift switch, fraction */
  PetscReal        lu_shift_fraction;
  PetscReal        lu_zeropivot;
  PetscScalar      *saved_values;    /* location for stashing nonzero values of matrix */
  PetscScalar      *idiag,*ssor;     /* inverse of diagonal entries; space for eisen */

  PetscTruth       keepzeroedrows;   /* keeps matrix structure same in calls to MatZeroRows()*/
  PetscTruth       ignorezeroentries;
  ISColoring       coloring;         /* set with MatADSetColoring() used by MatADSetValues() */
  Mat              sbaijMat;         /* mat in sbaij format */

  int              *xtoy,*xtoyB;     /* map nonzero pattern of X into Y's, used by MatAXPY() */
  Mat              XtoY;             /* used by MatAXPY() */
} Mat_SeqAIJ;

EXTERN int MatILUFactorSymbolic_SeqAIJ(Mat,IS,IS,MatFactorInfo*,Mat *);
EXTERN int MatICCFactorSymbolic_SeqAIJ(Mat,IS,MatFactorInfo*,Mat *);
EXTERN int MatCholeskyFactorSymbolic_SeqAIJ(Mat,IS,MatFactorInfo*,Mat*);
EXTERN int MatCholeskyFactorNumeric_SeqAIJ(Mat,Mat *);
EXTERN int MatDuplicate_SeqAIJ(Mat,MatDuplicateOption,Mat*);
EXTERN int MatMissingDiagonal_SeqAIJ(Mat);
EXTERN int MatMarkDiagonal_SeqAIJ(Mat);

EXTERN int MatMult_SeqAIJ(Mat A,Vec,Vec);
EXTERN int MatMultAdd_SeqAIJ(Mat A,Vec,Vec,Vec);
EXTERN int MatMultTranspose_SeqAIJ(Mat A,Vec,Vec);
EXTERN int MatMultTransposeAdd_SeqAIJ(Mat A,Vec,Vec,Vec);
EXTERN int MatRelax_SeqAIJ(Mat,Vec,PetscReal,MatSORType,PetscReal,int,int,Vec);

EXTERN int MatSetColoring_SeqAIJ(Mat,ISColoring);
EXTERN int MatSetValuesAdic_SeqAIJ(Mat,void*);
EXTERN int MatSetValuesAdifor_SeqAIJ(Mat,int,void*);

EXTERN int MatGetSymbolicTranspose_SeqAIJ(Mat,int *[],int *[]);
EXTERN int MatRestoreSymbolicTranspose_SeqAIJ(Mat,int *[],int *[]);
EXTERN int MatToSymmetricIJ_SeqAIJ(int,int*,int*,int,int,int**,int**);
EXTERN int Mat_AIJ_CheckInode(Mat,PetscTruth);
EXTERN int MatLUFactorSymbolic_SeqAIJ(Mat,IS,IS,MatFactorInfo*,Mat*);
EXTERN int MatLUFactorNumeric_SeqAIJ(Mat,Mat*);
EXTERN int MatLUFactor_SeqAIJ(Mat,IS,IS,MatFactorInfo*);
EXTERN int MatSolve_SeqAIJ(Mat,Vec,Vec);
EXTERN int MatSolveAdd_SeqAIJ(Mat,Vec,Vec,Vec);
EXTERN int MatSolveTranspose_SeqAIJ(Mat,Vec,Vec);
EXTERN int MatSolveTransposeAdd_SeqAIJ(Mat,Vec,Vec,Vec);
EXTERN int MatEqual_SeqAIJ(Mat A,Mat B,PetscTruth* flg);
EXTERN int MatFDColoringCreate_SeqAIJ(Mat,ISColoring,MatFDColoring);
EXTERN int MatILUDTFactor_SeqAIJ(Mat,MatFactorInfo*,IS,IS,Mat*);
EXTERN int MatLoad_SeqAIJ(PetscViewer,const MatType,Mat*);
EXTERN int RegisterApplyPtAPRoutines_Private(Mat);
EXTERN int MatMatMult_SeqAIJ_SeqAIJ(Mat,Mat,MatReuse,PetscReal,Mat*);
EXTERN_C_BEGIN
EXTERN int MatConvert_SeqAIJ_SeqSBAIJ(Mat,const MatType,Mat*);
EXTERN int MatConvert_SeqAIJ_SeqBAIJ(Mat,const MatType,Mat*);
EXTERN int MatReorderForNonzeroDiagonal_SeqAIJ(Mat,PetscReal,IS,IS);
EXTERN int MatAdjustForInodes_SeqAIJ(Mat,IS*,IS*);
EXTERN int MatSeqAIJGetInodeSizes_SeqAIJ(Mat,int*,int*[],int*);
EXTERN_C_END

#endif
