/*$Id: redundant.c,v 1.29 2001/04/10 19:36:17 bsmith Exp $*/
/*
  This file defines a "solve the problem redundantly on each processor" preconditioner.

*/
#include "src/ksp/pc/pcimpl.h"     /*I "petscpc.h" I*/
#include "petscksp.h"

typedef struct {
  PC         pc;                    /* actual preconditioner used on each processor */
  Vec        x,b;                   /* sequential vectors to hold parallel vectors */
  Mat        *pmats;                /* matrix and optional preconditioner matrix */
  VecScatter scatterin,scatterout;  /* scatter used to move all values to each processor */
  PetscTruth useparallelmat;
} PC_Redundant;

#undef __FUNCT__  
#define __FUNCT__ "PCView_Redundant"
static int PCView_Redundant(PC pc,PetscViewer viewer)
{
  PC_Redundant *red = (PC_Redundant*)pc->data;
  int          ierr,rank;
  PetscTruth   isascii,isstring;
  PetscViewer  sviewer;

  PetscFunctionBegin;
  ierr = MPI_Comm_rank(pc->comm,&rank);CHKERRQ(ierr);
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_ASCII,&isascii);CHKERRQ(ierr);
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_STRING,&isstring);CHKERRQ(ierr);
  if (isascii) {
    ierr = PetscViewerASCIIPrintf(viewer,"  Redundant solver preconditioner: Actual PC follows\n");CHKERRQ(ierr);
    ierr = PetscViewerGetSingleton(viewer,&sviewer);CHKERRQ(ierr);
    if (!rank) {
      ierr = PetscViewerASCIIPushTab(viewer);CHKERRQ(ierr);
      ierr = PCView(red->pc,sviewer);CHKERRQ(ierr);
      ierr = PetscViewerASCIIPopTab(viewer);CHKERRQ(ierr);
    }
    ierr = PetscViewerRestoreSingleton(viewer,&sviewer);CHKERRQ(ierr);
  } else if (isstring) {
    ierr = PetscViewerStringSPrintf(viewer," Redundant solver preconditioner");CHKERRQ(ierr);
    ierr = PetscViewerGetSingleton(viewer,&sviewer);CHKERRQ(ierr);
    if (!rank) {
      ierr = PCView(red->pc,sviewer);CHKERRQ(ierr);
    }
    ierr = PetscViewerRestoreSingleton(viewer,&sviewer);CHKERRQ(ierr);
  } else {
    SETERRQ1(1,"Viewer type %s not supported for PC redundant",((PetscObject)viewer)->type_name);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PCSetUp_Redundant"
static int PCSetUp_Redundant(PC pc)
{
  PC_Redundant   *red  = (PC_Redundant*)pc->data;
  int            ierr,mstart,mlocal,m,size;
  IS             isl;
  MatReuse       reuse = MAT_INITIAL_MATRIX;
  MatStructure   str   = DIFFERENT_NONZERO_PATTERN;
  MPI_Comm       comm;

  PetscFunctionBegin;
  ierr = PCSetFromOptions(red->pc);CHKERRQ(ierr);
  ierr = VecGetSize(pc->vec,&m);CHKERRQ(ierr);
  if (!pc->setupcalled) {
    ierr = VecGetLocalSize(pc->vec,&mlocal);CHKERRQ(ierr);
    ierr = VecCreateSeq(PETSC_COMM_SELF,m,&red->x);CHKERRQ(ierr);
    ierr = VecDuplicate(red->x,&red->b);CHKERRQ(ierr);
    ierr = PCSetVector(red->pc,red->x);CHKERRQ(ierr);
    if (!red->scatterin) {

      /*
         Create the vectors and vector scatter to get the entire vector onto each processor
      */
      ierr = VecGetOwnershipRange(pc->vec,&mstart,PETSC_NULL);CHKERRQ(ierr);
      ierr = VecScatterCreate(pc->vec,0,red->x,0,&red->scatterin);CHKERRQ(ierr);
      ierr = ISCreateStride(pc->comm,mlocal,mstart,1,&isl);CHKERRQ(ierr);
      ierr = VecScatterCreate(red->x,isl,pc->vec,isl,&red->scatterout);CHKERRQ(ierr);
      ierr = ISDestroy(isl);CHKERRQ(ierr);
    }
  }

  /* if pmatrix set by user is sequential then we do not need to gather the parallel matrix*/

  ierr = PetscObjectGetComm((PetscObject)pc->pmat,&comm);CHKERRQ(ierr);
  ierr = MPI_Comm_size(comm,&size);CHKERRQ(ierr);CHKERRQ(ierr);
  if (size == 1) {
    red->useparallelmat = PETSC_FALSE;
  }

  if (red->useparallelmat) {
    if (pc->setupcalled == 1 && pc->flag == DIFFERENT_NONZERO_PATTERN) {
      /* destroy old matrices */
      if (red->pmats) {
        ierr = MatDestroyMatrices(1,&red->pmats);CHKERRQ(ierr);
      }
    } else if (pc->setupcalled == 1) {
      reuse = MAT_REUSE_MATRIX;
      str   = SAME_NONZERO_PATTERN;
    }
        
    /* 
       grab the parallel matrix and put it on each processor
    */
    ierr = ISCreateStride(PETSC_COMM_SELF,m,0,1,&isl);CHKERRQ(ierr);
    ierr = MatGetSubMatrices(pc->pmat,1,&isl,&isl,reuse,&red->pmats);CHKERRQ(ierr);
    ierr = ISDestroy(isl);CHKERRQ(ierr);

    /* tell sequential PC its operators */
    ierr = PCSetOperators(red->pc,red->pmats[0],red->pmats[0],str);CHKERRQ(ierr);
  } else {
    ierr = PCSetOperators(red->pc,pc->mat,pc->pmat,pc->flag);CHKERRQ(ierr);
  }
  ierr = PCSetFromOptions(red->pc);CHKERRQ(ierr);
  ierr = PCSetVector(red->pc,red->b);CHKERRQ(ierr);
  ierr = PCSetUp(red->pc);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "PCApply_Redundant"
static int PCApply_Redundant(PC pc,Vec x,Vec y)
{
  PC_Redundant      *red = (PC_Redundant*)pc->data;
  int               ierr;

  PetscFunctionBegin;
  /* move all values to each processor */
  ierr = VecScatterBegin(x,red->b,INSERT_VALUES,SCATTER_FORWARD,red->scatterin);CHKERRQ(ierr);
  ierr = VecScatterEnd(x,red->b,INSERT_VALUES,SCATTER_FORWARD,red->scatterin);CHKERRQ(ierr);

  /* apply preconditioner on each processor */
  ierr = PCApply(red->pc,red->b,red->x,PC_LEFT);CHKERRQ(ierr);

  /* move local part of values into y vector */
  ierr = VecScatterBegin(red->x,y,INSERT_VALUES,SCATTER_FORWARD,red->scatterout);CHKERRQ(ierr);
  ierr = VecScatterEnd(red->x,y,INSERT_VALUES,SCATTER_FORWARD,red->scatterout);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "PCDestroy_Redundant"
static int PCDestroy_Redundant(PC pc)
{
  PC_Redundant *red = (PC_Redundant*)pc->data;
  int          ierr;

  PetscFunctionBegin;
  if (red->scatterin)  {ierr = VecScatterDestroy(red->scatterin);CHKERRQ(ierr);}
  if (red->scatterout) {ierr = VecScatterDestroy(red->scatterout);CHKERRQ(ierr);}
  if (red->x)          {ierr = VecDestroy(red->x);CHKERRQ(ierr);}
  if (red->b)          {ierr = VecDestroy(red->b);CHKERRQ(ierr);}
  if (red->pmats) {
    ierr = MatDestroyMatrices(1,&red->pmats);CHKERRQ(ierr);
  }
  ierr = PCDestroy(red->pc);CHKERRQ(ierr);
  ierr = PetscFree(red);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PCSetFromOptions_Redundant"
static int PCSetFromOptions_Redundant(PC pc)
{
  PetscFunctionBegin;
  PetscFunctionReturn(0);
}

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "PCRedundantSetScatter_Redundant"
int PCRedundantSetScatter_Redundant(PC pc,VecScatter in,VecScatter out)
{
  PC_Redundant *red = (PC_Redundant*)pc->data;
  int          ierr;

  PetscFunctionBegin;
  red->scatterin  = in; 
  red->scatterout = out;
  ierr = PetscObjectReference((PetscObject)in);CHKERRQ(ierr);
  ierr = PetscObjectReference((PetscObject)out);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "PCRedundantSetScatter"
/*@
   PCRedundantSetScatter - Sets the scatter used to copy values into the
     redundant local solve and the scatter to move them back into the global
     vector.

   Collective on PC

   Input Parameters:
+  pc - the preconditioner context
.  in - the scatter to move the values in
-  out - the scatter to move them out

   Level: advanced

.keywords: PC, redundant solve
@*/
int PCRedundantSetScatter(PC pc,VecScatter in,VecScatter out)
{
  int ierr,(*f)(PC,VecScatter,VecScatter);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  ierr = PetscObjectQueryFunction((PetscObject)pc,"PCRedundantSetScatter_C",(void (**)(void))&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(pc,in,out);CHKERRQ(ierr);
  } 
  PetscFunctionReturn(0);
}  

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "PCRedundantGetPC_Redundant"
int PCRedundantGetPC_Redundant(PC pc,PC *innerpc)
{
  PC_Redundant *red = (PC_Redundant*)pc->data;

  PetscFunctionBegin;
  *innerpc = red->pc;
  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "PCRedundantGetPC"
/*@
   PCRedundantGetPC - Gets the sequential PC created by the redundant PC.

   Not Collective

   Input Parameter:
.  pc - the preconditioner context

   Output Parameter:
.  innerpc - the sequential PC 

   Level: advanced

.keywords: PC, redundant solve
@*/
int PCRedundantGetPC(PC pc,PC *innerpc)
{
  int ierr,(*f)(PC,PC*);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  ierr = PetscObjectQueryFunction((PetscObject)pc,"PCRedundantGetPC_C",(void (**)(void))&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(pc,innerpc);CHKERRQ(ierr);
  } 
  PetscFunctionReturn(0);
}  

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "PCRedundantGetOperators_Redundant"
int PCRedundantGetOperators_Redundant(PC pc,Mat *mat,Mat *pmat)
{
  PC_Redundant *red = (PC_Redundant*)pc->data;

  PetscFunctionBegin;
  if (mat)  *mat  = red->pmats[0];
  if (pmat) *pmat = red->pmats[0];
  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "PCRedundantGetOperators"
/*@
   PCRedundantGetOperators - gets the sequential matrix and preconditioner matrix

   Not Collective

   Input Parameter:
.  pc - the preconditioner context

   Output Parameters:
+  mat - the matrix
-  pmat - the (possibly different) preconditioner matrix

   Level: advanced

.keywords: PC, redundant solve
@*/
int PCRedundantGetOperators(PC pc,Mat *mat,Mat *pmat)
{
  int ierr,(*f)(PC,Mat*,Mat*);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  ierr = PetscObjectQueryFunction((PetscObject)pc,"PCRedundantGetOperators_C",(void (**)(void))&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(pc,mat,pmat);CHKERRQ(ierr);
  } 
  PetscFunctionReturn(0);
}  

/* -------------------------------------------------------------------------------------*/
EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "PCCreate_Redundant"
int PCCreate_Redundant(PC pc)
{
  int          ierr;
  PC_Redundant *red;
  char         *prefix;

  PetscFunctionBegin;
  ierr = PetscNew(PC_Redundant,&red);CHKERRQ(ierr);
  PetscLogObjectMemory(pc,sizeof(PC_Redundant));
  ierr = PetscMemzero(red,sizeof(PC_Redundant));CHKERRQ(ierr);
  red->useparallelmat   = PETSC_TRUE;

  /* create the sequential PC that each processor has copy of */
  ierr = PCCreate(PETSC_COMM_SELF,&red->pc);CHKERRQ(ierr);
  ierr = PCSetType(red->pc,PCLU);CHKERRQ(ierr);
  ierr = PCGetOptionsPrefix(pc,&prefix);CHKERRQ(ierr);
  ierr = PCSetOptionsPrefix(red->pc,prefix);CHKERRQ(ierr);
  ierr = PCAppendOptionsPrefix(red->pc,"redundant_");CHKERRQ(ierr);

  pc->ops->apply             = PCApply_Redundant;
  pc->ops->applytranspose    = 0;
  pc->ops->setup             = PCSetUp_Redundant;
  pc->ops->destroy           = PCDestroy_Redundant;
  pc->ops->setfromoptions    = PCSetFromOptions_Redundant;
  pc->ops->setuponblocks     = 0;
  pc->ops->view              = PCView_Redundant;
  pc->ops->applyrichardson   = 0;

  pc->data              = (void*)red;

  ierr = PetscObjectComposeFunctionDynamic((PetscObject)pc,"PCRedundantSetScatter_C","PCRedundantSetScatter_Redundant",
                    PCRedundantSetScatter_Redundant);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)pc,"PCRedundantGetPC_C","PCRedundantGetPC_Redundant",
                    PCRedundantGetPC_Redundant);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)pc,"PCRedundantGetOperators_C","PCRedundantGetOperators_Redundant",
                    PCRedundantGetOperators_Redundant);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}
EXTERN_C_END
