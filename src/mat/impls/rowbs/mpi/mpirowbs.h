/* $Id: mpirowbs.h,v 1.50 2001/08/07 03:02:51 balay Exp $ */

#if !defined(__MPIROWBS_H)
#define __MPIROWBS_H

#include "src/mat/matimpl.h"

EXTERN_C_BEGIN
#include "BSsparse.h"
#include "BSprivate.h"
EXTERN_C_END

/*
   Mat_MPIRowbs - Parallel, compressed row storage format that's the
   interface to BlockSolve.
 */

typedef struct {
  int         *rowners;           /* range of rows owned by each proc */
  int         rstart,rend;       /* starting and ending owned rows */
  int         size;               /* size of communicator */
  int         rank;               /* rank of proc in communicator */ 
  int         sorted;             /* if true, rows sorted by increasing cols */
  PetscTruth  roworiented;        /* if true, row-oriented storage */
  int         nonew;              /* if true, no new elements allowed */
  int         nz,maxnz;          /* total nonzeros stored, allocated */
  int         *imax;              /* allocated matrix space per row */

  /*  The following variables are used in matrix assembly */
  PetscTruth  donotstash;         /* 1 if off processor entries dropped */
  MPI_Request *send_waits;        /* array of send requests */
  MPI_Request *recv_waits;        /* array of receive requests */
  int         nsends,nrecvs;     /* numbers of sends and receives */
  PetscScalar *svalues,*rvalues; /* sending and receiving data */
  int         rmax;               /* maximum message length */
  PetscTruth  vecs_permscale;     /* flag indicating permuted and scaled vectors */
  int         factor;
  int         bs_color_single;    /* Indicates blocksolve should bypass cliques in coloring */
  int         reallocs;           /* number of mallocs during MatSetValues() */

  /* BlockSolve data */
  BSprocinfo *procinfo;         /* BlockSolve processor context */
  BSmapping  *bsmap;            /* BlockSolve mapping context */
  BSspmat    *A;                /* initial matrix */
  BSpar_mat  *pA;               /* permuted matrix */
  BScomm     *comm_pA;          /* communication info for triangular solves */
  BSpar_mat  *fpA;              /* factored permuted matrix */
  BScomm     *comm_fpA;         /* communication info for factorization */
  Vec        diag;              /* scaling vector (stores inverse of square
                                   root of permuted diagonal of original matrix) */
  Vec        xwork;             /* work space for mat-vec mult */

  /* Cholesky factorization data */
  double     alpha;             /* restart for failed factorization */
  int        ierr;              /* BS factorization error */
  int        failures;          /* number of BS factorization failures */
  int        blocksolveassembly;/* Indicates the matrix has been assembled 
                                   for block solve */
} Mat_MPIRowbs;


EXTERN int MatAssemblyEnd_MPIRowbs_ForBlockSolve(Mat);

#define CHKERRBS(a) {if (__BSERROR_STATUS) {(*PetscErrorPrintf)(\
        "BlockSolve95 Error Code %d\n",__BSERROR_STATUS);CHKERRQ(1);}}

#if defined(PETSC_USE_LOG)  /* turn on BlockSolve logging */
#define MAINLOG
#endif

#endif
