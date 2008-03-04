
#include "src/mat/impls/dense/seq/dense.h"

  /*  Data stuctures for basic parallel dense matrix  */

typedef struct {
  PetscInt      nvec;                   /* this is the n size for the vector one multiplies with */
  Mat           A;                      /* local submatrix */
  PetscMPIInt   size;                   /* size of communicator */
  PetscMPIInt   rank;                   /* rank of proc in communicator */ 
  /* The following variables are used for matrix assembly */
  PetscTruth    donotstash;             /* Flag indicationg if values should be stashed */
  MPI_Request   *send_waits;            /* array of send requests */
  MPI_Request   *recv_waits;            /* array of receive requests */
  PetscInt      nsends,nrecvs;         /* numbers of sends and receives */
  PetscScalar   *svalues,*rvalues;     /* sending and receiving data */
  PetscInt      rmax;                   /* maximum message length */

  /* The following variables are used for matrix-vector products */

  Vec           lvec;                   /* local vector */
  VecScatter    Mvctx;                  /* scatter context for vector */

  PetscTruth    roworiented;            /* if true, row oriented input (default) */
} Mat_MPIDense;

EXTERN PetscErrorCode MatLoad_MPIDense(PetscViewer, MatType,Mat*);
EXTERN PetscErrorCode MatSetUpMultiply_MPIDense(Mat);
EXTERN PetscErrorCode MatGetSubMatrices_MPIDense(Mat,PetscInt,const IS[],const IS[],MatReuse,Mat *[]);
EXTERN PetscErrorCode MatEqual_MPIDense(Mat,Mat,PetscTruth*);
EXTERN PetscErrorCode MatMatMultSymbolic_MPIDense_MPIDense(Mat,Mat,PetscReal,Mat*);
EXTERN PetscErrorCode MatMatMult_MPIAIJ_MPIDense(Mat,Mat,MatReuse,PetscReal,Mat*);
EXTERN PetscErrorCode MatMatMultSymbolic_MPIAIJ_MPIDense(Mat,Mat,PetscReal,Mat*); 
EXTERN PetscErrorCode MatMatMultNumeric_MPIAIJ_MPIDense(Mat,Mat,Mat);

#if defined(PETSC_HAVE_PLAPACK)
EXTERN_C_BEGIN 
#include "PLA.h"
EXTERN_C_END 

typedef struct {
  MPI_Comm       comm_2d;
  PLA_Obj        A,pivots;
  PLA_Template   templ;
  MPI_Datatype   datatype;
  PetscInt       nb,nb_alg,ierror,rstart;
  VecScatter     ctx;
  IS             is_pla,is_petsc;
  PetscTruth     pla_solved;
  MatStructure   mstruct;
  PetscMPIInt    nprows,npcols;

  /* Flag to clean up (non-global) Plapack objects during Destroy */
  PetscTruth CleanUpPlapack;
} Mat_Plapack;

#endif
