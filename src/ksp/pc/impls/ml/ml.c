/* 
   Provides an interface to the ML 3.0 smoothed Aggregation 
*/
#include "src/ksp/pc/pcimpl.h"   /*I "petscpc.h" I*/
#include "src/ksp/pc/impls/mg/mgimpl.h"                    /*I "petscmg.h" I*/
#include "src/mat/impls/aij/seq/aij.h"
#include "src/mat/impls/aij/mpi/mpiaij.h"
EXTERN_C_BEGIN
#include <math.h> 
#include "ml_include.h"
EXTERN_C_END

/* The context (data structure) at each grid level */
typedef struct {
  Vec        x,b,r;            /* global vectors */
  Mat        A,P,R;
  KSP        ksp;
} GridCtx;

/* The context used to input PETSc matrix into ML at fine grid */
typedef struct {
  Mat          A,Aloc;
  ML_Operator  *mlmat; 
  PetscScalar  *pwork; /* tmp array used by PetscML_comm() */
  PetscInt     rlen_max,*cols; /* used by MatConvert_ML_SeqAIJ() */
  PetscScalar  *vals;          /* used by MatConvert_ML_SeqAIJ() */
} FineGridCtx;

/* The context associates a ML matrix with a PETSc shell matrix */
typedef struct {
  Mat          A;       /* PETSc shell matrix associated with mlmat */
  ML_Operator  *mlmat;  /* ML matrix assorciated with A */
  Vec          y;
} Mat_MLShell;

/* Private context for the ML preconditioner */
typedef struct {
  ML           *ml_object;
  ML_Aggregate *agg_object;
  GridCtx      *gridctx;
  FineGridCtx  *PetscMLdata;
  PetscInt     fine_level,MaxNlevels,MaxCoarseSize,CoarsenScheme;
  PetscReal    Threshold,DampingFactor; 
  PetscTruth   SpectralNormScheme_Anorm;
  PetscMPIInt  size;

  PetscErrorCode (*PCSetUp)(PC);
  PetscErrorCode (*PCDestroy)(PC);
 
} PC_ML;
extern int PetscML_getrow(void *ML_data,int N_requested_rows,int requested_rows[],
            int allocated_space,int columns[],double values[],int row_lengths[]);
extern int PetscML_matvec(void *ML_data, int in_length, double p[], int out_length,double ap[]);
extern int PetscML_comm(double x[], void *ML_data);
extern PetscErrorCode MatMult_ML(Mat,Vec,Vec);
extern PetscErrorCode MatMultAdd_ML(Mat,Vec,Vec,Vec);
extern PetscErrorCode MatConvert_MPIAIJ_ML(Mat,const MatType,Mat*);
extern PetscErrorCode MatDestroy_ML(Mat);
extern PetscErrorCode MatConvert_ML_SeqAIJ(FineGridCtx*,Mat*);
extern PetscErrorCode MatConvert_ML_SHELL(ML_Operator*,Mat*);

/* -------------------------------------------------------------------------- */
/*
   PCSetUp_ML - Prepares for the use of the ML preconditioner
                    by setting data structures and options.   

   Input Parameter:
.  pc - the preconditioner context

   Application Interface Routine: PCSetUp()

   Notes:
   The interface routine PCSetUp() is not usually called directly by
   the user, but instead is called by PCApply() if necessary.
*/

#undef __FUNCT__  
#define __FUNCT__ "PCSetUp_ML"
static PetscErrorCode PCSetUp_ML(PC pc)
{
  PetscErrorCode       ierr;
  PetscMPIInt          size,rank;
  FineGridCtx          *PetscMLdata;
  ML                   *ml_object;
  ML_Aggregate         *agg_object;
  ML_Operator          *mlmat;
  PetscInt             nlocal_allcols,Nlevels,mllevel,level,m,fine_level;
  Mat                  A,Aloc; 
  GridCtx              *gridctx; 
  PC                   pc_coarse;
  PC_ML                *pc_ml=PETSC_NULL;
  PetscObjectContainer container;

  PetscFunctionBegin;
  ierr = PetscObjectQuery((PetscObject)pc,"PC_ML",(PetscObject *)&container);CHKERRQ(ierr);
  if (container) {
    ierr = PetscObjectContainerGetPointer(container,(void **)&pc_ml);CHKERRQ(ierr); 
  } else {
    SETERRQ(PETSC_ERR_ARG_NULL,"Container does not exit");
  }
  
  /* setup special features of PCML */
  /*--------------------------------*/
  /* covert A to Aloc to be used by ML at fine grid */
  A = pc->pmat;
  ierr = MPI_Comm_size(A->comm,&size);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(A->comm,&rank);CHKERRQ(ierr);
  pc_ml->size = size;
  if (size > 1){ 
    Aloc = PETSC_NULL;
    ierr = MatConvert_MPIAIJ_ML(A,0,&Aloc);CHKERRQ(ierr);
  } else {
    Aloc = A;
  } 

  /* create and initialize struct 'PetscMLdata' */
  ierr = PetscNew(FineGridCtx,&PetscMLdata);CHKERRQ(ierr);
  ierr = PetscMemzero(PetscMLdata,sizeof(FineGridCtx));CHKERRQ(ierr);
  PetscMLdata->A    = A;
  PetscMLdata->Aloc = Aloc;
  ierr = PetscMalloc((Aloc->n+1)*sizeof(PetscScalar),&PetscMLdata->pwork);CHKERRQ(ierr);
  pc_ml->PetscMLdata = PetscMLdata;
    
  /* create ML discretization matrix at fine grid */
  ierr = MatGetSize(Aloc,&m,&nlocal_allcols);CHKERRQ(ierr);
  ML_Create(&ml_object,pc_ml->MaxNlevels);
  ML_Init_Amatrix(ml_object,0,m,m,PetscMLdata);
  ML_Set_Amatrix_Getrow(ml_object,0,PetscML_getrow,PetscML_comm,nlocal_allcols); 
  ML_Set_Amatrix_Matvec(ml_object,0,PetscML_matvec);

  /* aggregation */
  ML_Aggregate_Create(&agg_object);
  ML_Aggregate_Set_MaxCoarseSize(agg_object,pc_ml->MaxCoarseSize);
  /* set options */
  switch (pc_ml->CoarsenScheme) {
  case 1:  
    ML_Aggregate_Set_CoarsenScheme_Coupled(agg_object);break;
  case 2:
    ML_Aggregate_Set_CoarsenScheme_MIS(agg_object);break;
  case 3:
    ML_Aggregate_Set_CoarsenScheme_METIS(agg_object);break;
  }
  ML_Aggregate_Set_Threshold(agg_object,pc_ml->Threshold); 
  ML_Aggregate_Set_DampingFactor(agg_object,pc_ml->DampingFactor); 
  if (pc_ml->SpectralNormScheme_Anorm){
    ML_Aggregate_Set_SpectralNormScheme_Anorm(agg_object); 
  }
  
  Nlevels = ML_Gen_MGHierarchy_UsingAggregation(ml_object,0,ML_INCREASING,agg_object);
  if (Nlevels<=0) SETERRQ1(PETSC_ERR_ARG_OUTOFRANGE,"Nlevels %d must > 0",Nlevels);
  ierr = MGSetLevels(pc,Nlevels,PETSC_NULL);CHKERRQ(ierr); 
  pc_ml->ml_object  = ml_object;
  pc_ml->agg_object = agg_object;

  ierr = PetscMalloc(Nlevels*sizeof(GridCtx),&gridctx);CHKERRQ(ierr);
  fine_level = Nlevels - 1;
  pc_ml->gridctx = gridctx;
  pc_ml->fine_level = fine_level;

  /* wrap ML matrices by PETSc shell matrices at coarsened grids. 
     Level 0 is the finest grid for ML, but coarsest for PETSc! */
  gridctx[fine_level].A = A;
  level = fine_level - 1;
  if (size == 1){ /* convert ML mat format into petsc seqaij format */
    PetscMLdata->rlen_max = A->N;
    ierr = PetscMalloc(PetscMLdata->rlen_max*(sizeof(PetscScalar)+sizeof(PetscInt)),&PetscMLdata->cols);CHKERRQ(ierr);
    PetscMLdata->vals = (PetscScalar*)(PetscMLdata->cols + PetscMLdata->rlen_max);
    for (mllevel=1; mllevel<Nlevels; mllevel++){ 
      PetscMLdata->mlmat  = &(ml_object->Pmat[mllevel]);
      ierr = MatConvert_ML_SeqAIJ(PetscMLdata,&gridctx[level].P);CHKERRQ(ierr);
      PetscMLdata->mlmat  = &(ml_object->Amat[mllevel]);
      ierr = MatConvert_ML_SeqAIJ(PetscMLdata,&gridctx[level].A);CHKERRQ(ierr);
      PetscMLdata->mlmat  = &(ml_object->Rmat[mllevel-1]);
      ierr = MatConvert_ML_SeqAIJ(PetscMLdata,&gridctx[level].R);CHKERRQ(ierr);
      level--;
    }
  } else { /* convert ML mat format into petsc shell format */
    for (mllevel=1; mllevel<Nlevels; mllevel++){ 
      mlmat  = &(ml_object->Pmat[mllevel]);
      ierr = MatConvert_ML_SHELL(mlmat,&gridctx[level].P);CHKERRQ(ierr);
      mlmat  = &(ml_object->Amat[mllevel]);
      ierr = MatConvert_ML_SHELL(mlmat,&gridctx[level].A);CHKERRQ(ierr);
      mlmat  = &(ml_object->Rmat[mllevel-1]);
      ierr = MatConvert_ML_SHELL(mlmat,&gridctx[level].R);CHKERRQ(ierr);
      level--;
    }
  }

  /* create coarse level and the interpolation between the levels */
  level = fine_level; 
  while ( level >= 0 ){  
    if (level != fine_level){
      ierr = VecCreate(gridctx[level].A->comm,&gridctx[level].x);CHKERRQ(ierr); 
      ierr = VecSetSizes(gridctx[level].x,gridctx[level].A->n,PETSC_DECIDE);CHKERRQ(ierr);
      ierr = VecSetType(gridctx[level].x,VECMPI);CHKERRQ(ierr);
      ierr = MGSetX(pc,level,gridctx[level].x);CHKERRQ(ierr); 
    
      ierr = VecCreate(gridctx[level].A->comm,&gridctx[level].b);CHKERRQ(ierr);
      ierr = VecSetSizes(gridctx[level].b,gridctx[level].A->m,PETSC_DECIDE);CHKERRQ(ierr);
      ierr = VecSetType(gridctx[level].b,VECMPI);CHKERRQ(ierr);
      ierr = MGSetRhs(pc,level,gridctx[level].b);CHKERRQ(ierr); 
    }
    ierr = VecCreate(gridctx[level].A->comm,&gridctx[level].r);CHKERRQ(ierr);
    ierr = VecSetSizes(gridctx[level].r,gridctx[level].A->m,PETSC_DECIDE);CHKERRQ(ierr);
    ierr = VecSetType(gridctx[level].r,VECMPI);CHKERRQ(ierr);
    ierr = MGSetR(pc,level,gridctx[level].r);CHKERRQ(ierr); 

    if (level == 0){
      ierr = MGGetCoarseSolve(pc,&gridctx[level].ksp);CHKERRQ(ierr); 
    } else {
      ierr = MGGetSmoother(pc,level,&gridctx[level].ksp);CHKERRQ(ierr);
      ierr = MGSetResidual(pc,level,MGDefaultResidual,gridctx[level].A);CHKERRQ(ierr);
      if (level == fine_level){
        ierr = KSPSetOptionsPrefix(gridctx[level].ksp,"mg_fine_");CHKERRQ(ierr);
        ierr = MGSetR(pc,level,gridctx[level].r);CHKERRQ(ierr);
      }
    }
    ierr = KSPSetOperators(gridctx[level].ksp,gridctx[level].A,gridctx[level].A,DIFFERENT_NONZERO_PATTERN);CHKERRQ(ierr);
    
    if (level < fine_level){
      if (size > 1){
        ierr = KSPGetPC(gridctx[level].ksp,&pc_coarse);CHKERRQ(ierr);
        ierr = PCSetType(pc_coarse,PCNONE);CHKERRQ(ierr); 
      }
      ierr = MGSetInterpolate(pc,level+1,gridctx[level].P);CHKERRQ(ierr);
      ierr = MGSetRestriction(pc,level+1,gridctx[level].R);CHKERRQ(ierr); 
    }
    level--;
  }

  /* now call PCSetUp_MG()         */
  /*--------------------------------*/
  ierr = (*pc_ml->PCSetUp)(pc);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscObjectContainerDestroy_PC_ML"
PetscErrorCode PetscObjectContainerDestroy_PC_ML(void *ptr)
{
  PetscErrorCode       ierr;
  PC_ML                *pc_ml = (PC_ML*)ptr;
  PetscInt             level;

  PetscFunctionBegin; 
  if (pc_ml->size > 1){ierr = MatDestroy(pc_ml->PetscMLdata->Aloc);CHKERRQ(ierr);} 
  ML_Aggregate_Destroy(&pc_ml->agg_object); 
  ML_Destroy(&pc_ml->ml_object);

  ierr = PetscFree(pc_ml->PetscMLdata->pwork);CHKERRQ(ierr);
  if (pc_ml->size == 1){ierr = PetscFree(pc_ml->PetscMLdata->cols);CHKERRQ(ierr);}
  ierr = PetscFree(pc_ml->PetscMLdata);CHKERRQ(ierr);

  level = pc_ml->fine_level;
  while ( level>= 0 ){ 
    if (level != pc_ml->fine_level){
      ierr = MatDestroy(pc_ml->gridctx[level].A);CHKERRQ(ierr);
      ierr = MatDestroy(pc_ml->gridctx[level].P);CHKERRQ(ierr);
      ierr = MatDestroy(pc_ml->gridctx[level].R);CHKERRQ(ierr);
      ierr = VecDestroy(pc_ml->gridctx[level].x);CHKERRQ(ierr);
      ierr = VecDestroy(pc_ml->gridctx[level].b);CHKERRQ(ierr);
    }
    ierr = VecDestroy(pc_ml->gridctx[level].r);CHKERRQ(ierr);
    level--;
  }
  ierr = PetscFree(pc_ml->gridctx);CHKERRQ(ierr);
  ierr = PetscFree(pc_ml);CHKERRQ(ierr); 
  PetscFunctionReturn(0);
}
/* -------------------------------------------------------------------------- */
extern PetscErrorCode PetscObjectContainerSetUserDestroy(PetscObjectContainer,PetscErrorCode (*)(void*));
/*
   PCDestroy_ML - Destroys the private context for the ML preconditioner
   that was created with PCCreate_ML().

   Input Parameter:
.  pc - the preconditioner context

   Application Interface Routine: PCDestroy()
*/
#undef __FUNCT__  
#define __FUNCT__ "PCDestroy_ML"
static PetscErrorCode PCDestroy_ML(PC pc)
{
  PetscErrorCode       ierr;
  PC_ML                *pc_ml=PETSC_NULL;
  PetscObjectContainer container;

  PetscFunctionBegin;
  ierr = PetscObjectQuery((PetscObject)pc,"PC_ML",(PetscObject *)&container);CHKERRQ(ierr);
  if (container) {
    ierr = PetscObjectContainerGetPointer(container,(void **)&pc_ml);CHKERRQ(ierr);
    pc->ops->destroy = pc_ml->PCDestroy;
  } else {
    SETERRQ(PETSC_ERR_ARG_NULL,"Container does not exit");
  }
  /* detach pc and PC_ML and dereference container ??? */
  ierr = PetscObjectCompose((PetscObject)pc,"PC_ML",0);CHKERRQ(ierr); 
  ierr = (*pc->ops->destroy)(pc);CHKERRQ(ierr);

  ierr = PetscObjectContainerSetUserDestroy(container,PetscObjectContainerDestroy_PC_ML);CHKERRQ(ierr); 
  ierr = PetscObjectContainerDestroy(container);CHKERRQ(ierr); 
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PCSetFromOptions_ML"
static PetscErrorCode PCSetFromOptions_ML(PC pc)
{
  PetscErrorCode ierr;
  PetscInt       indx,m,PrintLevel,MaxNlevels,MaxCoarseSize; 
  PetscReal      Threshold,DampingFactor; 
  PetscTruth     flg;
  const char     *type[] = {"additive","multiplicative","full","cascade","kascade"};
  const char     *scheme[] = {"Uncoupled","Coupled","MIS","METIS"};
  PC_ML          *pc_ml=PETSC_NULL;
  PetscObjectContainer container;

  PetscFunctionBegin;
  ierr = PetscObjectQuery((PetscObject)pc,"PC_ML",(PetscObject *)&container);CHKERRQ(ierr);
  if (container) {
    ierr = PetscObjectContainerGetPointer(container,(void **)&pc_ml);CHKERRQ(ierr);
  } else {
    SETERRQ(PETSC_ERR_ARG_NULL,"Container does not exit");
  }
  ierr = PetscOptionsHead("MG options");CHKERRQ(ierr); 
  /* inherited MG options */
  ierr = PetscOptionsInt("-pc_mg_cycles","1 for V cycle, 2 for W-cycle","MGSetCycles",1,&m,&flg);CHKERRQ(ierr);
  if (flg) {
    ierr = MGSetCycles(pc,m);CHKERRQ(ierr);
  } 
  ierr = PetscOptionsInt("-pc_mg_smoothup","Number of post-smoothing steps","MGSetNumberSmoothUp",1,&m,&flg);CHKERRQ(ierr);
  if (flg) {
    ierr = MGSetNumberSmoothUp(pc,m);CHKERRQ(ierr);
  }
  ierr = PetscOptionsInt("-pc_mg_smoothdown","Number of pre-smoothing steps","MGSetNumberSmoothDown",1,&m,&flg);CHKERRQ(ierr);
  if (flg) {
    ierr = MGSetNumberSmoothDown(pc,m);CHKERRQ(ierr);
  }
  ierr = PetscOptionsEList("-pc_mg_type","Multigrid type","MGSetType",type,5,type[1],&indx,&flg);CHKERRQ(ierr);
  if (flg) {
    MGType mg = (MGType) 0;
    switch (indx) {
    case 0:
      mg = MGADDITIVE;
      break;
    case 1:
      mg = MGMULTIPLICATIVE;
      break;
    case 2:
      mg = MGFULL;
      break;
    case 3:
      mg = MGKASKADE;
      break;
    case 4:
      mg = MGKASKADE;
      break;
    }
    ierr = MGSetType(pc,mg);CHKERRQ(ierr);
  }
  ierr = PetscOptionsTail();CHKERRQ(ierr);

  /* ML options */
  ierr = PetscOptionsHead("ML options");CHKERRQ(ierr); 
  /* set defaults */
  PrintLevel    = 0;
  MaxNlevels    = 10;
  MaxCoarseSize = 1;
  indx          = 0;
  Threshold     = 0.0;
  DampingFactor = 4.0/3.0;
  
  ierr = PetscOptionsInt("-pc_ml_PrintLevel","Print level","ML_Set_PrintLevel",PrintLevel,&PrintLevel,PETSC_NULL);CHKERRQ(ierr);
  ML_Set_PrintLevel(PrintLevel);

  ierr = PetscOptionsInt("-pc_ml_maxNlevels","Maximum number of levels","None",MaxNlevels,&MaxNlevels,PETSC_NULL);CHKERRQ(ierr);
  pc_ml->MaxNlevels = MaxNlevels;

  ierr = PetscOptionsInt("-pc_ml_maxCoarseSize","Maximum coarsest mesh size","ML_Aggregate_Set_MaxCoarseSize",MaxCoarseSize,&MaxCoarseSize,PETSC_NULL);CHKERRQ(ierr);
  pc_ml->MaxCoarseSize = MaxCoarseSize;

  ierr = PetscOptionsEList("-pc_ml_CoarsenScheme","Aggregate Coarsen Scheme","ML_Aggregate_Set_CoarsenScheme_*",scheme,4,scheme[0],&indx,PETSC_NULL);CHKERRQ(ierr);
  pc_ml->CoarsenScheme = indx;

  ierr = PetscOptionsReal("-pc_ml_DampingFactor","P damping factor","ML_Aggregate_Set_DampingFactor",DampingFactor,&DampingFactor,PETSC_NULL);CHKERRQ(ierr);
  pc_ml->DampingFactor = DampingFactor;
  
  ierr = PetscOptionsReal("-pc_ml_Threshold","Smoother drop tol","ML_Aggregate_Set_Threshold",Threshold,&Threshold,PETSC_NULL);CHKERRQ(ierr);
  pc_ml->Threshold = Threshold;

  ierr = PetscOptionsLogical("-pc_ml_SpectralNormScheme_Anorm","Method used for estimating spectral radius","ML_Aggregate_Set_SpectralNormScheme_Anorm",PETSC_FALSE,&pc_ml->SpectralNormScheme_Anorm,PETSC_FALSE);
  
  ierr = PetscOptionsTail();CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* -------------------------------------------------------------------------- */
/*
   PCCreate_ML - Creates a ML preconditioner context, PC_ML, 
   and sets this as the private data within the generic preconditioning 
   context, PC, that was created within PCCreate().

   Input Parameter:
.  pc - the preconditioner context

   Application Interface Routine: PCCreate()
*/

/*MC
     PCML - Use geometric multigrid preconditioning. This preconditioner requires you provide 
       fine grid discretization matrix. The coarser grid matrices and restriction/interpolation 
       operators are computed by ML and wrapped as PETSc shell matrices.

   Options Database Key: (not done yet!)
+  -pc_mg_maxlevels <nlevels> - maximum number of levels including finest
.  -pc_mg_cycles 1 or 2 - for V or W-cycle
.  -pc_mg_smoothup <n> - number of smoothing steps after interpolation
.  -pc_mg_smoothdown <n> - number of smoothing steps before applying restriction operator
.  -pc_mg_type <additive,multiplicative,full,cascade> - multiplicative is the default
.  -pc_mg_monitor - print information on the multigrid convergence
-  -pc_mg_dump_matlab - dumps the matrices for each level and the restriction/interpolation matrices
                        to the Socket viewer for reading from Matlab.

   Level: intermediate

  Concepts: multigrid
 
.seealso:  PCCreate(), PCSetType(), PCType (for list of available types), PC, PCMGType, 
           MGSetLevels(), MGGetLevels(), MGSetType(), MPSetCycles(), MGSetNumberSmoothDown(),
           MGSetNumberSmoothUp(), MGGetCoarseSolve(), MGSetResidual(), MGSetInterpolation(),
           MGSetRestriction(), MGGetSmoother(), MGGetSmootherUp(), MGGetSmootherDown(),
           MGSetCyclesOnLevel(), MGSetRhs(), MGSetX(), MGSetR()      
M*/

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "PCCreate_ML"
PetscErrorCode PCCreate_ML(PC pc)
{
  PetscErrorCode       ierr;
  PC_ML                *pc_ml;
  PetscObjectContainer container;

  PetscFunctionBegin;
  /* initialize pc as PCMG */
  ierr = PCSetType(pc,PCMG);CHKERRQ(ierr); /* calls PCCreate_MG() and MGCreate_Private() */

  /* create a supporting struct and attach it to pc */
  ierr = PetscNew(PC_ML,&pc_ml);CHKERRQ(ierr);
  /* ierr = PetscMemzero(pc_ml,sizeof(PC_ML));CHKERRQ(ierr); */
  ierr = PetscObjectContainerCreate(PETSC_COMM_SELF,&container);CHKERRQ(ierr);
  ierr = PetscObjectContainerSetPointer(container,pc_ml);CHKERRQ(ierr);
  ierr = PetscObjectCompose((PetscObject)pc,"PC_ML",(PetscObject)container);CHKERRQ(ierr);
  
  pc_ml->PCSetUp   = pc->ops->setup;
  pc_ml->PCDestroy = pc->ops->destroy;

  /* overwrite the pointers of PCMG by the functions of PCML */
  pc->ops->setfromoptions = PCSetFromOptions_ML;
  pc->ops->setup          = PCSetUp_ML;
  pc->ops->destroy        = PCDestroy_ML;
  PetscFunctionReturn(0);
}
EXTERN_C_END

int PetscML_getrow(void *ML_data, int N_requested_rows, int requested_rows[],
   int allocated_space, int columns[], double values[], int row_lengths[])
{
  PetscErrorCode ierr;
  Mat            Aloc; 
  Mat_SeqAIJ     *a;
  PetscInt       m,i,j,k=0,row,*aj;
  PetscScalar    *aa;
  FineGridCtx    *ml=(FineGridCtx*)ML_data;

  Aloc = ml->Aloc;
  a    = (Mat_SeqAIJ*)Aloc->data;
  ierr = MatGetSize(Aloc,&m,PETSC_NULL);CHKERRQ(ierr);

  for (i = 0; i<N_requested_rows; i++) {
    row   = requested_rows[i];
    row_lengths[i] = a->ilen[row]; 
    if (allocated_space < k+row_lengths[i]) return(0);
    if ( (row >= 0) || (row <= (m-1)) ) {
      aj = a->j + a->i[row];
      aa = a->a + a->i[row];
      for (j=0; j<row_lengths[i]; j++){
        columns[k]  = aj[j];
        values[k++] = aa[j];
      }
    }
  }
  return(1);
}

int PetscML_matvec(void *ML_data,int in_length,double p[],int out_length,double ap[])
{
  PetscErrorCode ierr;
  FineGridCtx    *ml=(FineGridCtx*)ML_data;
  Mat            A=ml->A, Aloc=ml->Aloc; 
  Vec            x,y;
  PetscMPIInt    size;
  PetscScalar    *pwork=ml->pwork; 
  PetscInt       i;

  ierr = MPI_Comm_size(A->comm,&size);CHKERRQ(ierr);
  ierr = VecCreateSeqWithArray(PETSC_COMM_SELF,out_length,ap,&y);CHKERRQ(ierr);
  if (size == 1){
    ierr = VecCreateSeqWithArray(PETSC_COMM_SELF,in_length,p,&x);CHKERRQ(ierr);
  } else {
    for (i=0; i<in_length; i++) pwork[i] = p[i]; 
    PetscML_comm(pwork,ml);
    ierr = VecCreateSeqWithArray(PETSC_COMM_SELF,Aloc->n,pwork,&x);CHKERRQ(ierr);
  }
  ierr = MatMult(Aloc,x,y);CHKERRQ(ierr);
  ierr = VecDestroy(x);CHKERRQ(ierr);
  ierr = VecDestroy(y);CHKERRQ(ierr);
  return 0;
}

int PetscML_comm(double p[],void *ML_data)
{
  PetscErrorCode ierr;
  FineGridCtx    *ml=(FineGridCtx*)ML_data;
  Mat            A=ml->A;
  Mat_MPIAIJ     *a = (Mat_MPIAIJ*)A->data;
  Vec            x;
  PetscMPIInt    size;
  PetscInt       i,in_length=A->m,out_length=ml->Aloc->n;
  PetscScalar    *array;

  ierr = MPI_Comm_size(A->comm,&size);CHKERRQ(ierr);
  if (size == 1) return 0;
  ierr = VecCreateSeqWithArray(PETSC_COMM_SELF,in_length,p,&x);CHKERRQ(ierr);
  ierr = VecScatterBegin(x,a->lvec,INSERT_VALUES,SCATTER_FORWARD,a->Mvctx);CHKERRQ(ierr);
  ierr = VecScatterEnd(x,a->lvec,INSERT_VALUES,SCATTER_FORWARD,a->Mvctx);CHKERRQ(ierr);
  ierr = VecGetArray(a->lvec,&array);CHKERRQ(ierr);
  for (i=in_length; i<out_length; i++){
    p[i] = array[i-in_length];
  }
  ierr = VecDestroy(x);CHKERRQ(ierr);
  return 0;
}
#undef __FUNCT__  
#define __FUNCT__ "MatMult_ML"
PetscErrorCode MatMult_ML(Mat A,Vec x,Vec y)
{
  PetscErrorCode   ierr;
  Mat_MLShell      *shell;
  PetscScalar      *xarray,*yarray;
  PetscInt         x_length,y_length;
  
  PetscFunctionBegin;
  ierr = MatShellGetContext(A,(void *)&shell);CHKERRQ(ierr);
  ierr = VecGetArray(x,&xarray);CHKERRQ(ierr);
  ierr = VecGetArray(y,&yarray);CHKERRQ(ierr);
  x_length = shell->mlmat->invec_leng;
  y_length = shell->mlmat->outvec_leng;

  ML_Operator_Apply(shell->mlmat,x_length,xarray,y_length,yarray); 

  ierr = VecRestoreArray(x,&xarray);CHKERRQ(ierr); 
  ierr = VecRestoreArray(y,&yarray);CHKERRQ(ierr); 
  PetscFunctionReturn(0);
}
/* MatMultAdd_ML -  Compute y = w + A*x */
#undef __FUNCT__  
#define __FUNCT__ "MatMultAdd_ML"
PetscErrorCode MatMultAdd_ML(Mat A,Vec x,Vec w,Vec y)
{
  PetscErrorCode    ierr;
  Mat_MLShell       *shell;
  PetscScalar       *xarray,*yarray;
  const PetscScalar one=1.0;
  PetscInt          x_length,y_length;
  
  PetscFunctionBegin;
  ierr = MatShellGetContext(A,(void *)&shell);CHKERRQ(ierr);
  ierr = VecGetArray(x,&xarray);CHKERRQ(ierr);
  ierr = VecGetArray(y,&yarray);CHKERRQ(ierr);

  x_length = shell->mlmat->invec_leng;
  y_length = shell->mlmat->outvec_leng;

  ML_Operator_Apply(shell->mlmat,x_length,xarray,y_length,yarray); 

  ierr = VecRestoreArray(x,&xarray);CHKERRQ(ierr); 
  ierr = VecRestoreArray(y,&yarray);CHKERRQ(ierr); 
  ierr = VecAXPY(&one,w,y);CHKERRQ(ierr); 

  PetscFunctionReturn(0);
}

/* newtype is ignored because "ml" is not listed under Petsc MatType yet */
#undef __FUNCT__  
#define __FUNCT__ "MatConvert_MPIAIJ_ML"
PetscErrorCode MatConvert_MPIAIJ_ML(Mat A,const MatType newtype,Mat *Aloc) 
{
  PetscErrorCode  ierr;
  Mat_MPIAIJ      *mpimat=(Mat_MPIAIJ*)A->data; 
  Mat_SeqAIJ      *mat,*a=(Mat_SeqAIJ*)(mpimat->A)->data,*b=(Mat_SeqAIJ*)(mpimat->B)->data;
  PetscInt        *ai=a->i,*aj=a->j,*bi=b->i,*bj=b->j;
  PetscScalar     *aa=a->a,*ba=b->a,*ca;
  PetscInt        am=A->m,an=A->n,i,j,k;
  PetscInt        *ci,*cj,ncols;
  MatReuse        scall=MAT_INITIAL_MATRIX;

  PetscFunctionBegin;
  if (am != an) SETERRQ2(PETSC_ERR_ARG_WRONG,"A must have a square diagonal portion, am: %d != an: %d",am,an);

  if (*Aloc) scall = MAT_REUSE_MATRIX;
  if (scall == MAT_INITIAL_MATRIX){
    ierr = PetscMalloc((1+am)*sizeof(PetscInt),&ci);CHKERRQ(ierr);
    ci[0] = 0;
    for (i=0; i<am; i++){
      ci[i+1] = ci[i] + (ai[i+1] - ai[i]) + (bi[i+1] - bi[i]);
    }
    ierr = PetscMalloc((1+ci[am])*sizeof(PetscInt),&cj);CHKERRQ(ierr);
    ierr = PetscMalloc((1+ci[am])*sizeof(PetscScalar),&ca);CHKERRQ(ierr);

    k = 0;
    for (i=0; i<am; i++){
      /* diagonal portion of A */
      ncols = ai[i+1] - ai[i];
      for (j=0; j<ncols; j++) {
        cj[k]   = *aj++; 
        ca[k++] = *aa++; 
      }
      /* off-diagonal portion of A */
      ncols = bi[i+1] - bi[i];
      for (j=0; j<ncols; j++) {
        cj[k]   = an + (*bj); bj++;
        ca[k++] = *ba++; 
      }
    }
    if (k != ci[am]) SETERRQ2(PETSC_ERR_ARG_WRONG,"k: %d != ci[am]: %d",k,ci[am]);

    /* put together the new matrix */
    an = mpimat->A->n+mpimat->B->n;
    ierr = MatCreateSeqAIJWithArrays(PETSC_COMM_SELF,am,an,ci,cj,ca,Aloc);CHKERRQ(ierr);

    /* MatCreateSeqAIJWithArrays flags matrix so PETSc doesn't free the user's arrays. */
    /* Since these are PETSc arrays, change flags to free them as necessary. */
    mat = (Mat_SeqAIJ*)(*Aloc)->data;
    mat->freedata = PETSC_TRUE;
    mat->nonew    = 0;
  } else if (scall == MAT_REUSE_MATRIX){
    mat=(Mat_SeqAIJ*)(*Aloc)->data; 
    ci = mat->i; cj = mat->j; ca = mat->a;
    for (i=0; i<am; i++) {
      /* diagonal portion of A */
      ncols = ai[i+1] - ai[i];
      for (j=0; j<ncols; j++) *ca++ = *aa++; 
      /* off-diagonal portion of A */
      ncols = bi[i+1] - bi[i];
      for (j=0; j<ncols; j++) *ca++ = *ba++; 
    }
  } else {
    SETERRQ1(PETSC_ERR_ARG_WRONG,"Invalid MatReuse %d",(int)scall);
  }
  PetscFunctionReturn(0);
}
extern PetscErrorCode MatDestroy_Shell(Mat);
#undef __FUNCT__  
#define __FUNCT__ "MatDestroy_ML"
PetscErrorCode MatDestroy_ML(Mat A)
{
  PetscErrorCode ierr;
  Mat_MLShell    *shell;

  PetscFunctionBegin;
  ierr = MatShellGetContext(A,(void *)&shell);CHKERRQ(ierr);
  ierr = VecDestroy(shell->y);CHKERRQ(ierr);
  ierr = PetscFree(shell);CHKERRQ(ierr); 
  ierr = MatDestroy_Shell(A);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatConvert_ML_SeqAIJ"
PetscErrorCode MatConvert_ML_SeqAIJ(FineGridCtx *ml,Mat *newmat) 
{
  PetscErrorCode  ierr; 
  PetscInt        i;
  ML_Operator     *mat=ml->mlmat;
  PetscInt        m=mat->outvec_leng,n= mat->invec_leng,nnz[m];
  
  PetscFunctionBegin;
  if ( mat->getrow == NULL) SETERRQ(PETSC_ERR_ARG_NULL,"mat->getrow = NULL");

  ierr = MatCreate(PETSC_COMM_SELF,m,n,PETSC_DECIDE,PETSC_DECIDE,newmat);CHKERRQ(ierr);
  ierr = MatSetType(*newmat,MATSEQAIJ);CHKERRQ(ierr);
  for (i=0; i<m; i++){
    ML_get_matrix_row(mat,1,&i,&ml->rlen_max,&ml->cols,&ml->vals,&nnz[i],0);
  }
  ierr = MatSeqAIJSetPreallocation(*newmat,0,nnz);CHKERRQ(ierr);

  for (i=0; i<m; i++){
    ML_get_matrix_row(mat,1,&i,&ml->rlen_max,&ml->cols,&ml->vals,&nnz[i],0); 
    ierr = MatSetValues(*newmat,1,&i,nnz[i],ml->cols,ml->vals,INSERT_VALUES);CHKERRQ(ierr);
  }
  ierr = MatAssemblyBegin(*newmat,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(*newmat,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatConvert_ML_SHELL"
PetscErrorCode MatConvert_ML_SHELL(ML_Operator *mlmat,Mat *newmat) 
{
  PetscErrorCode ierr;
  PetscInt       m,n;
  ML_Comm        *MLcomm;
  Mat_MLShell    *shellctx;

  PetscFunctionBegin;
  m = mlmat->outvec_leng; 
  n = mlmat->invec_leng;
  if (!m || !n){
    newmat = PETSC_NULL;
  } else {
    MLcomm = mlmat->comm;
    ierr = PetscNew(Mat_MLShell,&shellctx);CHKERRQ(ierr);
    ierr = PetscMemzero(shellctx,sizeof(Mat_MLShell));CHKERRQ(ierr);
    ierr = MatCreateShell(MLcomm->USR_comm,m,n,PETSC_DETERMINE,PETSC_DETERMINE,shellctx,newmat);CHKERRQ(ierr);
    ierr = MatShellSetOperation(*newmat,MATOP_MULT,(void *)MatMult_ML);CHKERRQ(ierr); 
    ierr = MatShellSetOperation(*newmat,MATOP_MULT_ADD,(void *)MatMultAdd_ML);CHKERRQ(ierr); 
    shellctx->A         = *newmat;
    shellctx->mlmat     = mlmat;
    ierr = VecCreate(PETSC_COMM_WORLD,&shellctx->y);CHKERRQ(ierr);
    ierr = VecSetSizes(shellctx->y,m,PETSC_DECIDE);CHKERRQ(ierr);
    ierr = VecSetFromOptions(shellctx->y);CHKERRQ(ierr);
    (*newmat)->ops->destroy = MatDestroy_ML;
  }
  PetscFunctionReturn(0);
}
