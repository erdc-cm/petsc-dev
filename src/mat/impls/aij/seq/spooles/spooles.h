
#if !defined(__SPOOLES_H)
#define __SPOOLES_H
#include "src/mat/matimpl.h"


EXTERN_C_BEGIN
#include "misc.h"
#include "FrontMtx.h"
#include "SymbFac.h"
#include "MPI/spoolesMPI.h" 
EXTERN_C_END

typedef struct {
  int             msglvl,pivotingflag,symflag,seed,FrontMtxInfo,typeflag;
  int             ordering,maxdomainsize,maxzeros,maxsize,
                  patchAndGoFlag,storeids,storevalues;
  PetscTruth      useQR;
  double          tau,toosmall,fudge;
  FILE            *msgFile ;
} Spooles_options;

typedef struct {
  /* Followings are used for seq and MPI Spooles */
  InpMtx          *mtxA ;        /* coefficient matrix */
  ETree           *frontETree ;  /* defines numeric and symbolic factorizations */
  FrontMtx        *frontmtx ;    /* numeric L, D, U factor matrices */
  IV              *newToOldIV, *oldToNewIV ; /* permutation vectors */
  IVL             *symbfacIVL ;              /* symbolic factorization */
  SubMtxManager   *mtxmanager  ;  /* working array */
  MatStructure    flg;
  double          cpus[20] ;
  int             *oldToNew,stats[20];
  Spooles_options options;
  Graph           *graph;

  /* Followings are used for MPI Spooles */
  MPI_Comm        comm_spooles;          /* communicator to be passed to spooles */
  IV              *ownersIV,*ownedColumnsIV,*vtxmapIV;
  SolveMap        *solvemap;
  DenseMtx        *mtxY, *mtxX;
  double          *entX;
  int             *rowindX,rstart,firsttag,nmycol;
  Vec             vec_spooles;
  IS              iden,is_petsc;
  VecScatter      scat;
  
  /* A few function pointers for inheritance */
  PetscErrorCode (*MatDuplicate)(Mat,MatDuplicateOption,Mat*);
  PetscErrorCode (*MatCholeskyFactorSymbolic)(Mat,IS,MatFactorInfo*,Mat*);
  PetscErrorCode (*MatLUFactorSymbolic)(Mat,IS,IS,MatFactorInfo*,Mat*);
  PetscErrorCode (*MatView)(Mat,PetscViewer);
  PetscErrorCode (*MatAssemblyEnd)(Mat,MatAssemblyType);
  PetscErrorCode (*MatDestroy)(Mat);
  PetscErrorCode (*MatPreallocate)(Mat,int,int,int*,int,int*);

  MatType    basetype;
  PetscTruth CleanUpSpooles,useQR;
} Mat_Spooles;

EXTERN PetscErrorCode SetSpoolesOptions(Mat, Spooles_options *);
EXTERN PetscErrorCode MatFactorInfo_Spooles(Mat,PetscViewer);

EXTERN PetscErrorCode MatDestroy_SeqAIJSpooles(Mat);
EXTERN PetscErrorCode MatSolve_SeqAIJSpooles(Mat,Vec,Vec);
EXTERN PetscErrorCode MatFactorNumeric_SeqAIJSpooles(Mat,Mat*); 
EXTERN PetscErrorCode MatView_SeqAIJSpooles(Mat,PetscViewer);
EXTERN PetscErrorCode MatAssemblyEnd_SeqAIJSpooles(Mat,MatAssemblyType);
EXTERN PetscErrorCode MatQRFactorSymbolic_SeqAIJSpooles(Mat,IS,IS,MatFactorInfo*,Mat*);
EXTERN PetscErrorCode MatLUFactorSymbolic_SeqAIJSpooles(Mat,IS,IS,MatFactorInfo*,Mat*);
EXTERN PetscErrorCode MatCholeskyFactorSymbolic_SeqAIJSpooles(Mat,IS,MatFactorInfo*,Mat*);
EXTERN PetscErrorCode MatDuplicate_Spooles(Mat,MatDuplicateOption,Mat*);

EXTERN PetscErrorCode MatDestroy_MPIAIJSpooles(Mat);
EXTERN PetscErrorCode MatSolve_MPIAIJSpooles(Mat,Vec,Vec);
EXTERN PetscErrorCode MatFactorNumeric_MPIAIJSpooles(Mat,Mat*); 
EXTERN PetscErrorCode MatAssemblyEnd_MPIAIJSpooles(Mat,MatAssemblyType);
EXTERN PetscErrorCode MatLUFactorSymbolic_MPIAIJSpooles(Mat,IS,IS,MatFactorInfo*,Mat*);

EXTERN PetscErrorCode MatDestroy_SeqSBAIJSpooles(Mat);
EXTERN PetscErrorCode MatGetInertia_SeqSBAIJSpooles(Mat,int*,int*,int*);
EXTERN PetscErrorCode MatCholeskyFactorSymbolic_SeqSBAIJSpooles(Mat,IS,MatFactorInfo*,Mat*);

EXTERN PetscErrorCode MatCholeskyFactorSymbolic_MPISBAIJSpooles(Mat,IS,MatFactorInfo*,Mat*);
EXTERN_C_BEGIN
EXTERN PetscErrorCode MatConvert_Spooles_Base(Mat,const MatType,Mat*);
EXTERN PetscErrorCode MatConvert_SeqAIJ_SeqAIJSpooles(Mat,const MatType,Mat*);
EXTERN PetscErrorCode MatConvert_SeqSBAIJ_SeqSBAIJSpooles(Mat,const MatType,Mat*);
EXTERN PetscErrorCode MatConvert_MPIAIJ_MPIAIJSpooles(Mat,const MatType,Mat*);
EXTERN PetscErrorCode MatConvert_MPISBAIJ_MPISBAIJSpooles(Mat,const MatType,Mat*);
EXTERN_C_END
#endif
