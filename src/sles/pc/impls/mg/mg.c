/*$Id: mg.c,v 1.128 2001/10/03 22:12:40 balay Exp $*/
/*
    Defines the multigrid preconditioner interface.
*/
#include "src/sles/pc/impls/mg/mgimpl.h"                    /*I "petscmg.h" I*/


/*
       MGMCycle_Private - Given an MG structure created with MGCreate() runs 
                  one multiplicative cycle down through the levels and
                  back up.

    Input Parameter:
.   mg - structure created with  MGCreate().
*/
#undef __FUNCT__  
#define __FUNCT__ "MGMCycle_Private"
int MGMCycle_Private(MG *mglevels,PetscTruth *converged)
{
  MG          mg = *mglevels,mgc;
  int         cycles = mg->cycles,ierr,its;
  PetscScalar zero = 0.0;

  PetscFunctionBegin;
  if (converged) *converged = PETSC_FALSE;

  if (!mg->level) {  /* coarse grid */
    ierr = SLESSolve(mg->smoothd,mg->b,mg->x,&its);CHKERRQ(ierr);
  } else {
    ierr = SLESSolve(mg->smoothd,mg->b,mg->x,&its);CHKERRQ(ierr);
    ierr = (*mg->residual)(mg->A,mg->b,mg->x,mg->r);CHKERRQ(ierr);

    /* if on finest level and have convergence criteria set */
    if (mg->level == mg->levels-1 && mg->ttol) {
      PetscReal rnorm;
      ierr = VecNorm(mg->r,NORM_2,&rnorm);CHKERRQ(ierr);
      if (rnorm <= mg->ttol) {
        *converged = PETSC_TRUE;
        if (rnorm < mg->atol) {
          PetscLogInfo(0,"Linear solver has converged. Residual norm %g is less than absolute tolerance %g\n",rnorm,mg->atol);
        } else {
          PetscLogInfo(0,"Linear solver has converged. Residual norm %g is less than relative tolerance times initial residual norm %g\n",rnorm,mg->ttol);
        }
        PetscFunctionReturn(0);
      }
    }

    mgc = *(mglevels - 1);
    ierr = MatRestrict(mg->restrct,mg->r,mgc->b);CHKERRQ(ierr);
    ierr = VecSet(&zero,mgc->x);CHKERRQ(ierr);
    while (cycles--) {
      ierr = MGMCycle_Private(mglevels-1,converged);CHKERRQ(ierr); 
    }
    ierr = MatInterpolateAdd(mg->interpolate,mgc->x,mg->x,mg->x);CHKERRQ(ierr);
    ierr = SLESSolve(mg->smoothu,mg->b,mg->x,&its);CHKERRQ(ierr); 
  }
  PetscFunctionReturn(0);
}

/*
       MGCreate_Private - Creates a MG structure for use with the
               multigrid code. Level 0 is the coarsest. (But the 
               finest level is stored first in the array).

*/
#undef __FUNCT__  
#define __FUNCT__ "MGCreate_Private"
static int MGCreate_Private(MPI_Comm comm,int levels,PC pc,MPI_Comm *comms,MG **result)
{
  MG   *mg;
  int  i,ierr,size;
  char *prefix;
  KSP  ksp;
  PC   ipc;

  PetscFunctionBegin;
  ierr = PetscMalloc(levels*sizeof(MG),&mg);CHKERRQ(ierr);
  PetscLogObjectMemory(pc,levels*(sizeof(MG)+sizeof(struct _MG)));

  ierr = PCGetOptionsPrefix(pc,&prefix);CHKERRQ(ierr);

  for (i=0; i<levels; i++) {
    ierr = PetscNew(struct _MG,&mg[i]);CHKERRQ(ierr);
    ierr = PetscMemzero(mg[i],sizeof(struct _MG));CHKERRQ(ierr);
    mg[i]->level  = i;
    mg[i]->levels = levels;
    mg[i]->cycles = 1;

    if (comms) comm = comms[i];
    ierr = SLESCreate(comm,&mg[i]->smoothd);CHKERRQ(ierr);
    ierr = SLESGetKSP(mg[i]->smoothd,&ksp);CHKERRQ(ierr);
    ierr = KSPSetTolerances(ksp,PETSC_DEFAULT,PETSC_DEFAULT,PETSC_DEFAULT,1);CHKERRQ(ierr);
    ierr = SLESSetOptionsPrefix(mg[i]->smoothd,prefix);CHKERRQ(ierr);

    /* do special stuff for coarse grid */
    if (!i && levels > 1) {
      ierr = SLESAppendOptionsPrefix(mg[0]->smoothd,"mg_coarse_");CHKERRQ(ierr);

      /* coarse solve is (redundant) LU by default */
      ierr = SLESGetKSP(mg[0]->smoothd,&ksp);CHKERRQ(ierr);
      ierr = KSPSetType(ksp,KSPPREONLY);CHKERRQ(ierr);
      ierr = SLESGetPC(mg[0]->smoothd,&ipc);CHKERRQ(ierr);
      ierr = MPI_Comm_size(comm,&size);CHKERRQ(ierr);
      if (size > 1) {
        ierr = PCSetType(ipc,PCREDUNDANT);CHKERRQ(ierr);
        ierr = PCRedundantGetPC(ipc,&ipc);CHKERRQ(ierr);
      }
      ierr = PCSetType(ipc,PCLU);CHKERRQ(ierr);

    } else {
      ierr = SLESAppendOptionsPrefix(mg[i]->smoothd,"mg_levels_");CHKERRQ(ierr);
    }
    PetscLogObjectParent(pc,mg[i]->smoothd);
    mg[i]->smoothu         = mg[i]->smoothd;
    mg[i]->default_smoothu = 10000;
    mg[i]->default_smoothd = 10000;
    mg[i]->rtol = 0.0;
    mg[i]->atol = 0.0;
    mg[i]->dtol = 0.0;
    mg[i]->ttol = 0.0;
  }
  *result = mg;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PCDestroy_MG"
static int PCDestroy_MG(PC pc)
{
  MG  *mg = (MG*)pc->data;
  int i,n = mg[0]->levels,ierr;

  PetscFunctionBegin;
  for (i=0; i<n; i++) {
    if (mg[i]->smoothd != mg[i]->smoothu) {
      ierr = SLESDestroy(mg[i]->smoothd);CHKERRQ(ierr);
    }
    ierr = SLESDestroy(mg[i]->smoothu);CHKERRQ(ierr);
    ierr = PetscFree(mg[i]);CHKERRQ(ierr);
  }
  ierr = PetscFree(mg);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}



EXTERN int MGACycle_Private(MG*);
EXTERN int MGFCycle_Private(MG*);
EXTERN int MGKCycle_Private(MG*);

/*
   PCApply_MG - Runs either an additive, multiplicative, Kaskadic
             or full cycle of multigrid. 

  Note: 
  A simple wrapper which calls MGMCycle(),MGACycle(), or MGFCycle(). 
*/ 
#undef __FUNCT__  
#define __FUNCT__ "PCApply_MG"
static int PCApply_MG(PC pc,Vec b,Vec x)
{
  MG     *mg = (MG*)pc->data;
  PetscScalar zero = 0.0;
  int    levels = mg[0]->levels,ierr;

  PetscFunctionBegin;
  mg[levels-1]->b = b; 
  mg[levels-1]->x = x;
  if (mg[0]->am == MGMULTIPLICATIVE) {
    ierr = VecSet(&zero,x);CHKERRQ(ierr);
    ierr = MGMCycle_Private(mg+levels-1,PETSC_NULL);CHKERRQ(ierr);
  } 
  else if (mg[0]->am == MGADDITIVE) {
    ierr = MGACycle_Private(mg);CHKERRQ(ierr);
  }
  else if (mg[0]->am == MGKASKADE) {
    ierr = MGKCycle_Private(mg);CHKERRQ(ierr);
  }
  else {
    ierr = MGFCycle_Private(mg);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PCApplyRichardson_MG"
static int PCApplyRichardson_MG(PC pc,Vec b,Vec x,Vec w,PetscReal rtol,PetscReal atol, PetscReal dtol,int its)
{
  MG         *mg = (MG*)pc->data;
  int        ierr,levels = mg[0]->levels;
  PetscTruth converged = PETSC_FALSE;

  PetscFunctionBegin;
  mg[levels-1]->b    = b; 
  mg[levels-1]->x    = x;

  mg[levels-1]->rtol = rtol;
  mg[levels-1]->atol = atol;
  mg[levels-1]->dtol = dtol;
  if (rtol) {
    /* compute initial residual norm for relative convergence test */
    PetscReal rnorm;
    ierr               = (*mg[levels-1]->residual)(mg[levels-1]->A,b,x,w);CHKERRQ(ierr);
    ierr               = VecNorm(w,NORM_2,&rnorm);CHKERRQ(ierr);
    mg[levels-1]->ttol = PetscMax(rtol*rnorm,atol);
  } else if (atol) {
    mg[levels-1]->ttol = atol;
  } else {
    mg[levels-1]->ttol = 0.0;
  }

  while (its-- && !converged) {
    ierr = MGMCycle_Private(mg+levels-1,&converged);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PCSetFromOptions_MG"
static int PCSetFromOptions_MG(PC pc)
{
  int        ierr,m,levels = 1;
  PetscTruth flg;
  char       buff[16],*type[] = {"additive","multiplicative","full","cascade"};

  PetscFunctionBegin;

  ierr = PetscOptionsHead("Multigrid options");CHKERRQ(ierr);
    if (!pc->data) {
      ierr = PetscOptionsInt("-pc_mg_levels","Number of Levels","MGSetLevels",levels,&levels,&flg);CHKERRQ(ierr);
      ierr = MGSetLevels(pc,levels,PETSC_NULL);CHKERRQ(ierr);
    }
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
    ierr = PetscOptionsEList("-pc_mg_type","Multigrid type","MGSetType",type,4,"multiplicative",buff,15,&flg);CHKERRQ(ierr);
    if (flg) {
      MGType     mg;
      PetscTruth isadd,ismult,isfull,iskask,iscasc;

      ierr = PetscStrcmp(buff,type[0],&isadd);CHKERRQ(ierr);
      ierr = PetscStrcmp(buff,type[1],&ismult);CHKERRQ(ierr);
      ierr = PetscStrcmp(buff,type[2],&isfull);CHKERRQ(ierr);
      ierr = PetscStrcmp(buff,type[3],&iscasc);CHKERRQ(ierr);
      ierr = PetscStrcmp(buff,"kaskade",&iskask);CHKERRQ(ierr);

      if      (isadd)  mg = MGADDITIVE;
      else if (ismult) mg = MGMULTIPLICATIVE;
      else if (isfull) mg = MGFULL;
      else if (iskask) mg = MGKASKADE;
      else if (iscasc) mg = MGKASKADE;
      else SETERRQ1(PETSC_ERR_ARG_OUTOFRANGE,"Unknown type: %s",buff);
      ierr = MGSetType(pc,mg);CHKERRQ(ierr);
    }
  ierr = PetscOptionsTail();CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PCView_MG"
static int PCView_MG(PC pc,PetscViewer viewer)
{
  MG         *mg = (MG*)pc->data;
  KSP        kspu,kspd;
  int        itu,itd,ierr,levels = mg[0]->levels,i;
  PetscReal  dtol,atol,rtol;
  char       *cstring;
  PetscTruth isascii;

  PetscFunctionBegin;
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_ASCII,&isascii);CHKERRQ(ierr);
  if (isascii) {
    ierr = SLESGetKSP(mg[0]->smoothu,&kspu);CHKERRQ(ierr);
    ierr = SLESGetKSP(mg[0]->smoothd,&kspd);CHKERRQ(ierr);
    ierr = KSPGetTolerances(kspu,&dtol,&atol,&rtol,&itu);CHKERRQ(ierr);
    ierr = KSPGetTolerances(kspd,&dtol,&atol,&rtol,&itd);CHKERRQ(ierr);
    if (mg[0]->am == MGMULTIPLICATIVE) cstring = "multiplicative";
    else if (mg[0]->am == MGADDITIVE)  cstring = "additive";
    else if (mg[0]->am == MGFULL)      cstring = "full";
    else if (mg[0]->am == MGKASKADE)   cstring = "Kaskade";
    else cstring = "unknown";
    ierr = PetscViewerASCIIPrintf(viewer,"  MG: type is %s, levels=%d cycles=%d, pre-smooths=%d, post-smooths=%d\n",
                      cstring,levels,mg[0]->cycles,mg[0]->default_smoothu,mg[0]->default_smoothd);CHKERRQ(ierr);
    for (i=0; i<levels; i++) {
      ierr = PetscViewerASCIIPrintf(viewer,"Down solver (pre-smoother) on level %d -------------------------------\n",i);CHKERRQ(ierr);
      ierr = PetscViewerASCIIPushTab(viewer);CHKERRQ(ierr);
      ierr = SLESView(mg[i]->smoothd,viewer);CHKERRQ(ierr);
      ierr = PetscViewerASCIIPopTab(viewer);CHKERRQ(ierr);
      if (mg[i]->smoothd == mg[i]->smoothu) {
        ierr = PetscViewerASCIIPrintf(viewer,"Up solver same as down solver\n");CHKERRQ(ierr);
      } else {
        ierr = PetscViewerASCIIPrintf(viewer,"Up solver (post-smoother) on level %d -------------------------------\n",i);CHKERRQ(ierr);
        ierr = PetscViewerASCIIPushTab(viewer);CHKERRQ(ierr);
        ierr = SLESView(mg[i]->smoothu,viewer);CHKERRQ(ierr);
        ierr = PetscViewerASCIIPopTab(viewer);CHKERRQ(ierr);
      }
    }
  } else {
    SETERRQ1(1,"Viewer type %s not supported for PCMG",((PetscObject)viewer)->type_name);
  }
  PetscFunctionReturn(0);
}

/*
    Calls setup for the SLES on each level
*/
#undef __FUNCT__  
#define __FUNCT__ "PCSetUp_MG"
static int PCSetUp_MG(PC pc)
{
  MG          *mg = (MG*)pc->data;
  int         ierr,i,n = mg[0]->levels;
  KSP         ksp;
  PC          cpc;
  PetscTruth  preonly,lu,redundant,monitor = PETSC_FALSE,dump;
  PetscViewer ascii;
  MPI_Comm    comm;

  PetscFunctionBegin;
  /*
     temporarily stick pc->vec into mg[0]->b and x so that 
   SLESSetUp is happy. Since currently those slots are empty.
  */
  mg[n-1]->x = pc->vec;
  mg[n-1]->b = pc->vec;

  if (pc->setupcalled == 0) {
    ierr = PetscOptionsHasName(0,"-pc_mg_monitor",&monitor);CHKERRQ(ierr);
     
    for (i=1; i<n; i++) {
      if (mg[i]->smoothd) {
        if (monitor) {
          ierr = SLESGetKSP(mg[i]->smoothd,&ksp);CHKERRQ(ierr);
          ierr = PetscObjectGetComm((PetscObject)ksp,&comm);CHKERRQ(ierr);
          ierr = PetscViewerASCIIOpen(comm,"stdout",&ascii);CHKERRQ(ierr);
          ierr = PetscViewerASCIISetTab(ascii,n-i);CHKERRQ(ierr);
          ierr = KSPSetMonitor(ksp,KSPDefaultMonitor,ascii,(int(*)(void*))PetscViewerDestroy);CHKERRQ(ierr);
        }
        ierr = SLESSetFromOptions(mg[i]->smoothd);CHKERRQ(ierr);
      }
    }
    for (i=0; i<n; i++) {
      if (mg[i]->smoothu && mg[i]->smoothu != mg[i]->smoothd) {
        if (monitor) {
          ierr = SLESGetKSP(mg[i]->smoothu,&ksp);CHKERRQ(ierr);
          ierr = PetscObjectGetComm((PetscObject)ksp,&comm);CHKERRQ(ierr);
          ierr = PetscViewerASCIIOpen(comm,"stdout",&ascii);CHKERRQ(ierr);
          ierr = PetscViewerASCIISetTab(ascii,n-i);CHKERRQ(ierr);
          ierr = KSPSetMonitor(ksp,KSPDefaultMonitor,ascii,(int(*)(void*))PetscViewerDestroy);CHKERRQ(ierr);
        }
        ierr = SLESSetFromOptions(mg[i]->smoothu);CHKERRQ(ierr);
      }
    }
  }

  for (i=1; i<n; i++) {
    if (mg[i]->smoothd) {
      ierr = SLESGetKSP(mg[i]->smoothd,&ksp);CHKERRQ(ierr);
      ierr = KSPSetInitialGuessNonzero(ksp,PETSC_TRUE);CHKERRQ(ierr);
      ierr = SLESSetUp(mg[i]->smoothd,mg[i]->b,mg[i]->x);CHKERRQ(ierr);
    }
  }
  for (i=0; i<n; i++) {
    if (mg[i]->smoothu && mg[i]->smoothu != mg[i]->smoothd) {
      ierr = SLESGetKSP(mg[i]->smoothu,&ksp);CHKERRQ(ierr);
      ierr = KSPSetInitialGuessNonzero(ksp,PETSC_TRUE);CHKERRQ(ierr);
      ierr = SLESSetUp(mg[i]->smoothu,mg[i]->b,mg[i]->x);CHKERRQ(ierr);
    }
  }

  /*
      If coarse solver is not direct method then DO NOT USE preonly 
  */
  ierr = SLESGetKSP(mg[0]->smoothd,&ksp);CHKERRQ(ierr);
  ierr = PetscTypeCompare((PetscObject)ksp,KSPPREONLY,&preonly);CHKERRQ(ierr);
  if (preonly) {
    ierr = SLESGetPC(mg[0]->smoothd,&cpc);CHKERRQ(ierr);
    ierr = PetscTypeCompare((PetscObject)cpc,PCLU,&lu);CHKERRQ(ierr);
    ierr = PetscTypeCompare((PetscObject)cpc,PCREDUNDANT,&redundant);CHKERRQ(ierr);
    if (!lu && !redundant) {
      ierr = KSPSetType(ksp,KSPGMRES);CHKERRQ(ierr);
    }
  }

  if (pc->setupcalled == 0) {
    if (monitor) {
      ierr = SLESGetKSP(mg[0]->smoothd,&ksp);CHKERRQ(ierr);
      ierr = PetscObjectGetComm((PetscObject)ksp,&comm);CHKERRQ(ierr);
      ierr = PetscViewerASCIIOpen(comm,"stdout",&ascii);CHKERRQ(ierr);
      ierr = PetscViewerASCIISetTab(ascii,n);CHKERRQ(ierr);
      ierr = KSPSetMonitor(ksp,KSPDefaultMonitor,ascii,(int(*)(void*))PetscViewerDestroy);CHKERRQ(ierr);
    }
    ierr = SLESSetFromOptions(mg[0]->smoothd);CHKERRQ(ierr);
  }

  ierr = SLESSetUp(mg[0]->smoothd,mg[0]->b,mg[0]->x);CHKERRQ(ierr);

  /*
     Dump the interpolation/restriction matrices to matlab plus the 
   Jacobian/stiffness on each level. This allows Matlab users to 
   easily check if the Galerkin condition A_c = R A_f R^T is satisfied */
  ierr = PetscOptionsHasName(pc->prefix,"-pc_mg_dump_matlab",&dump);CHKERRQ(ierr);
  if (dump) {
    for (i=1; i<n; i++) {
      ierr = MatView(mg[i]->restrct,PETSC_VIEWER_SOCKET_(pc->comm));CHKERRQ(ierr);
    }
    for (i=0; i<n; i++) {
      ierr = SLESGetPC(mg[i]->smoothd,&pc);CHKERRQ(ierr);
      ierr = MatView(pc->mat,PETSC_VIEWER_SOCKET_(pc->comm));CHKERRQ(ierr);
    }
  }

  PetscFunctionReturn(0);
}

/* -------------------------------------------------------------------------------------*/

#undef __FUNCT__  
#define __FUNCT__ "MGSetLevels"
/*@C
   MGSetLevels - Sets the number of levels to use with MG.
   Must be called before any other MG routine.

   Collective on PC

   Input Parameters:
+  pc - the preconditioner context
.  levels - the number of levels
-  comms - optional communicators for each level; this is to allow solving the coarser problems
           on smaller sets of processors. Use PETSC_NULL_OBJECT for default in Fortran

   Level: intermediate

   Notes:
     If the number of levels is one then the multigrid uses the -mg_levels prefix
  for setting the level options rather than the -mg_coarse prefix.

.keywords: MG, set, levels, multigrid

.seealso: MGSetType(), MGGetLevels()
@*/
int MGSetLevels(PC pc,int levels,MPI_Comm *comms)
{
  int ierr;
  MG  *mg;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);

  if (pc->data) {
    SETERRQ(1,"Number levels already set for MG\n\
    make sure that you call MGSetLevels() before SLESSetFromOptions()");
  }
  ierr                     = MGCreate_Private(pc->comm,levels,pc,comms,&mg);CHKERRQ(ierr);
  mg[0]->am                = MGMULTIPLICATIVE;
  pc->data                 = (void*)mg;
  pc->ops->applyrichardson = PCApplyRichardson_MG;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MGGetLevels"
/*@
   MGGetLevels - Gets the number of levels to use with MG.

   Not Collective

   Input Parameter:
.  pc - the preconditioner context

   Output parameter:
.  levels - the number of levels

   Level: advanced

.keywords: MG, get, levels, multigrid

.seealso: MGSetLevels()
@*/
int MGGetLevels(PC pc,int *levels)
{
  MG  *mg;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);

  mg      = (MG*)pc->data;
  *levels = mg[0]->levels;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MGSetType"
/*@
   MGSetType - Determines the form of multigrid to use:
   multiplicative, additive, full, or the Kaskade algorithm.

   Collective on PC

   Input Parameters:
+  pc - the preconditioner context
-  form - multigrid form, one of MGMULTIPLICATIVE, MGADDITIVE,
   MGFULL, MGKASKADE

   Options Database Key:
.  -pc_mg_type <form> - Sets <form>, one of multiplicative,
   additive, full, kaskade   

   Level: advanced

.keywords: MG, set, method, multiplicative, additive, full, Kaskade, multigrid

.seealso: MGSetLevels()
@*/
int MGSetType(PC pc,MGType form)
{
  MG *mg;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  mg = (MG*)pc->data;

  if (!mg) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Must set MG levels before calling");
  mg[0]->am = form;
  if (form == MGMULTIPLICATIVE) pc->ops->applyrichardson = PCApplyRichardson_MG;
  else pc->ops->applyrichardson = 0;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MGSetCycles"
/*@
   MGSetCycles - Sets the type cycles to use.  Use MGSetCyclesOnLevel() for more 
   complicated cycling.

   Collective on PC

   Input Parameters:
+  mg - the multigrid context 
-  n - the number of cycles

   Options Database Key:
$  -pc_mg_cycles n - 1 denotes a V-cycle; 2 denotes a W-cycle.

   Level: advanced

.keywords: MG, set, cycles, V-cycle, W-cycle, multigrid

.seealso: MGSetCyclesOnLevel()
@*/
int MGSetCycles(PC pc,int n)
{ 
  MG  *mg;
  int i,levels;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  mg     = (MG*)pc->data;
  if (!mg) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Must set MG levels before calling");
  levels = mg[0]->levels;

  for (i=0; i<levels; i++) {  
    mg[i]->cycles  = n; 
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MGCheck"
/*@
   MGCheck - Checks that all components of the MG structure have 
   been set.

   Collective on PC

   Input Parameters:
.  mg - the MG structure

   Level: advanced

.keywords: MG, check, set, multigrid
@*/
int MGCheck(PC pc)
{
  MG  *mg;
  int i,n,count = 0;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  mg = (MG*)pc->data;

  if (!mg) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Must set MG levels before calling");

  n = mg[0]->levels;

  for (i=1; i<n; i++) {
    if (!mg[i]->restrct) {
      (*PetscErrorPrintf)("No restrict set level %d \n",n-i); count++;
    }    
    if (!mg[i]->interpolate) {
      (*PetscErrorPrintf)("No interpolate set level %d \n",n-i); count++;
    }
    if (!mg[i]->residual) {
      (*PetscErrorPrintf)("No residual set level %d \n",n-i); count++;
    }
    if (!mg[i]->smoothu) {
      (*PetscErrorPrintf)("No smoothup set level %d \n",n-i); count++;
    }  
    if (!mg[i]->smoothd) {
      (*PetscErrorPrintf)("No smoothdown set level %d \n",n-i); count++;
    }
    if (!mg[i]->r) {
      (*PetscErrorPrintf)("No r set level %d \n",n-i); count++;
    } 
    if (!mg[i-1]->x) {
      (*PetscErrorPrintf)("No x set level %d \n",n-i); count++;
    }
    if (!mg[i-1]->b) {
      (*PetscErrorPrintf)("No b set level %d \n",n-i); count++;
    }
  }
  PetscFunctionReturn(count);
}


#undef __FUNCT__  
#define __FUNCT__ "MGSetNumberSmoothDown"
/*@
   MGSetNumberSmoothDown - Sets the number of pre-smoothing steps to
   use on all levels. Use MGGetSmootherDown() to set different 
   pre-smoothing steps on different levels.

   Collective on PC

   Input Parameters:
+  mg - the multigrid context 
-  n - the number of smoothing steps

   Options Database Key:
.  -pc_mg_smoothdown <n> - Sets number of pre-smoothing steps

   Level: advanced

.keywords: MG, smooth, down, pre-smoothing, steps, multigrid

.seealso: MGSetNumberSmoothUp()
@*/
int MGSetNumberSmoothDown(PC pc,int n)
{ 
  MG  *mg;
  int i,levels,ierr;
  KSP ksp;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  mg     = (MG*)pc->data;
  if (!mg) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Must set MG levels before calling");
  levels = mg[0]->levels;

  for (i=0; i<levels; i++) {  
    ierr = SLESGetKSP(mg[i]->smoothd,&ksp);CHKERRQ(ierr);
    ierr = KSPSetTolerances(ksp,PETSC_DEFAULT,PETSC_DEFAULT,PETSC_DEFAULT,n);CHKERRQ(ierr);
    mg[i]->default_smoothd = n;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MGSetNumberSmoothUp"
/*@
   MGSetNumberSmoothUp - Sets the number of post-smoothing steps to use 
   on all levels. Use MGGetSmootherUp() to set different numbers of 
   post-smoothing steps on different levels.

   Collective on PC

   Input Parameters:
+  mg - the multigrid context 
-  n - the number of smoothing steps

   Options Database Key:
.  -pc_mg_smoothup <n> - Sets number of post-smoothing steps

   Level: advanced

.keywords: MG, smooth, up, post-smoothing, steps, multigrid

.seealso: MGSetNumberSmoothDown()
@*/
int  MGSetNumberSmoothUp(PC pc,int n)
{ 
  MG  *mg;
  int i,levels,ierr;
  KSP ksp;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  mg     = (MG*)pc->data;
  if (!mg) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Must set MG levels before calling");
  levels = mg[0]->levels;

  for (i=0; i<levels; i++) {  
    ierr = SLESGetKSP(mg[i]->smoothu,&ksp);CHKERRQ(ierr);
    ierr = KSPSetTolerances(ksp,PETSC_DEFAULT,PETSC_DEFAULT,PETSC_DEFAULT,n);CHKERRQ(ierr);
    mg[i]->default_smoothu = n;
  }
  PetscFunctionReturn(0);
}

/* ----------------------------------------------------------------------------------------*/

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "PCCreate_MG"
int PCCreate_MG(PC pc)
{
  PetscFunctionBegin;
  pc->ops->apply          = PCApply_MG;
  pc->ops->setup          = PCSetUp_MG;
  pc->ops->destroy        = PCDestroy_MG;
  pc->ops->setfromoptions = PCSetFromOptions_MG;
  pc->ops->view           = PCView_MG;

  pc->data                = (void*)0;
  PetscFunctionReturn(0);
}
EXTERN_C_END
