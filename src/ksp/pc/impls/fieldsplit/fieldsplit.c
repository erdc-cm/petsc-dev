/*

*/
#include "src/ksp/pc/pcimpl.h"     /*I "petscpc.h" I*/

typedef struct _PC_FieldSplitLink *PC_FieldSplitLink;
struct _PC_FieldSplitLink {
  KSP               ksp;
  Vec               x,y;
  PetscInt          nfields;
  PetscInt          *fields;
  VecScatter        sctx;
  PC_FieldSplitLink next;
};

typedef struct {
  PCCompositeType   type;              /* additive or multiplicative */
  PetscTruth        defaultsplit;
  PetscInt          bs;
  PetscInt          nsplits;
  Vec               *x,*y,w1,w2;
  Mat               *pmat;
  IS                *is;
  PC_FieldSplitLink head;
} PC_FieldSplit;

#undef __FUNCT__  
#define __FUNCT__ "PCView_FieldSplit"
static PetscErrorCode PCView_FieldSplit(PC pc,PetscViewer viewer)
{
  PC_FieldSplit     *jac = (PC_FieldSplit*)pc->data;
  PetscErrorCode    ierr;
  PetscTruth        iascii;
  PetscInt          i,j;
  PC_FieldSplitLink link = jac->head;
  const char        *types[] = {"additive","multiplicative"};

  PetscFunctionBegin;
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_ASCII,&iascii);CHKERRQ(ierr);
  if (iascii) {
    ierr = PetscViewerASCIIPrintf(viewer,"  FieldSplit with %s composition: total splits = %D",types[jac->type],jac->nsplits);CHKERRQ(ierr);
    ierr = PetscViewerASCIIPrintf(viewer,"  Solver info for each split is in the following KSP objects:\n");CHKERRQ(ierr);
    ierr = PetscViewerASCIIPushTab(viewer);CHKERRQ(ierr);
    for (i=0; i<jac->nsplits; i++) {
      ierr = PetscViewerASCIIPrintf(viewer,"Split number %D Fields ",i);CHKERRQ(ierr);
      ierr = PetscViewerASCIIUseTabs(viewer,PETSC_FALSE);CHKERRQ(ierr);
      for (j=0; j<link->nfields; j++) {
        if (j > 0) {
          ierr = PetscViewerASCIIPrintf(viewer,",");CHKERRQ(ierr);
        }
        ierr = PetscViewerASCIIPrintf(viewer," %D",link->fields[j]);CHKERRQ(ierr);
      }
      ierr = PetscViewerASCIIPrintf(viewer,"\n");CHKERRQ(ierr);
      ierr = PetscViewerASCIIUseTabs(viewer,PETSC_TRUE);CHKERRQ(ierr);
      ierr = KSPView(link->ksp,viewer);CHKERRQ(ierr);
      link = link->next;
    }
    ierr = PetscViewerASCIIPopTab(viewer);CHKERRQ(ierr);
  } else {
    SETERRQ1(PETSC_ERR_SUP,"Viewer type %s not supported for PCFieldSplit",((PetscObject)viewer)->type_name);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PCFieldSplitSetDefaults"
static PetscErrorCode PCFieldSplitSetDefaults(PC pc)
{
  PC_FieldSplit     *jac  = (PC_FieldSplit*)pc->data;
  PetscErrorCode    ierr;
  PC_FieldSplitLink link = jac->head;
  PetscInt          i;
  PetscTruth        flg = PETSC_FALSE,*fields;

  PetscFunctionBegin;
  ierr = PetscOptionsGetLogical(pc->prefix,"-pc_fieldsplit_default",&flg,PETSC_NULL);CHKERRQ(ierr);
  if (!link || flg) { 
    ierr = PetscLogInfo(pc,"PCFieldSplitSetDefaults: Using default splitting of fields");CHKERRQ(ierr);
    if (jac->bs <= 0) {
      ierr   = MatGetBlockSize(pc->pmat,&jac->bs);CHKERRQ(ierr);
    }
    ierr = PetscMalloc(jac->bs*sizeof(PetscTruth),&fields);CHKERRQ(ierr);
    ierr = PetscMemzero(fields,jac->bs*sizeof(PetscTruth));CHKERRQ(ierr);
    while (link) {
      for (i=0; i<link->nfields; i++) {
        fields[link->fields[i]] = PETSC_TRUE;
      }
      link = link->next;
    }
    jac->defaultsplit = PETSC_TRUE;
    for (i=0; i<jac->bs; i++) {
      if (!fields[i]) {
	ierr = PCFieldSplitSetFields(pc,1,&i);CHKERRQ(ierr);
      } else {
        jac->defaultsplit = PETSC_FALSE;
      }
    }
  }
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "PCSetUp_FieldSplit"
static PetscErrorCode PCSetUp_FieldSplit(PC pc)
{
  PC_FieldSplit     *jac = (PC_FieldSplit*)pc->data;
  PetscErrorCode    ierr;
  PC_FieldSplitLink link;
  PetscInt          i,nsplit;
  MatStructure      flag = pc->flag;

  PetscFunctionBegin;
  ierr   = PCFieldSplitSetDefaults(pc);CHKERRQ(ierr);
  nsplit = jac->nsplits;
  link   = jac->head;

  /* get the matrices for each split */
  if (!jac->is) {
    PetscInt rstart,rend,nslots,bs;

    ierr   = MatGetBlockSize(pc->pmat,&bs);CHKERRQ(ierr);
    ierr   = MatGetOwnershipRange(pc->pmat,&rstart,&rend);CHKERRQ(ierr);
    nslots = (rend - rstart)/bs;
    ierr   = PetscMalloc(nsplit*sizeof(IS),&jac->is);CHKERRQ(ierr);
    for (i=0; i<nsplit; i++) {
      if (jac->defaultsplit) {
	ierr = ISCreateStride(pc->comm,nslots,rstart+i,nsplit,&jac->is[i]);CHKERRQ(ierr);
      } else {
        PetscInt   *ii,j,k,nfields = link->nfields,*fields = link->fields;
        PetscTruth sorted;
        ierr = PetscMalloc(link->nfields*nslots*sizeof(PetscInt),&ii);CHKERRQ(ierr);
        for (j=0; j<nslots; j++) {
          for (k=0; k<nfields; k++) {
            ii[nfields*j + k] = rstart + bs*j + fields[k];
          }
        }
	ierr = ISCreateGeneral(pc->comm,nslots*nfields,ii,&jac->is[i]);CHKERRQ(ierr);       
        ierr = ISSorted(jac->is[i],&sorted);CHKERRQ(ierr);
        if (!sorted) SETERRQ(PETSC_ERR_USER,"Fields must be sorted when creating split");
        ierr = PetscFree(ii);CHKERRQ(ierr);
        link = link->next;
      }
    }
  }
  
  if (!jac->pmat) {
    ierr = MatGetSubMatrices(pc->pmat,nsplit,jac->is,jac->is,MAT_INITIAL_MATRIX,&jac->pmat);CHKERRQ(ierr);
  } else {
    ierr = MatGetSubMatrices(pc->pmat,nsplit,jac->is,jac->is,MAT_REUSE_MATRIX,&jac->pmat);CHKERRQ(ierr);
  }

  /* set up the individual PCs */
  i    = 0;
  link = jac->head;
  while (link) {
    ierr = KSPSetOperators(link->ksp,jac->pmat[i],jac->pmat[i],flag);CHKERRQ(ierr);
    ierr = KSPSetFromOptions(link->ksp);CHKERRQ(ierr);
    ierr = KSPSetUp(link->ksp);CHKERRQ(ierr);
    i++;
    link = link->next;
  }
  
  /* create work vectors for each split */
  if (!jac->x) {
    Vec xtmp;
    ierr = PetscMalloc2(nsplit,Vec,&jac->x,nsplit,Vec,&jac->y);CHKERRQ(ierr);
    link = jac->head;
    for (i=0; i<nsplit; i++) {
      Mat A;
      ierr      = KSPGetOperators(link->ksp,PETSC_NULL,&A,PETSC_NULL);CHKERRQ(ierr);
      ierr      = MatGetVecs(A,&link->x,&link->y);CHKERRQ(ierr);
      jac->x[i] = link->x;
      jac->y[i] = link->y;
      link      = link->next;
    }
    /* compute scatter contexts needed by multiplicative versions and non-default splits */
    
    link = jac->head;
    ierr = MatGetVecs(pc->pmat,&xtmp,PETSC_NULL);CHKERRQ(ierr);
    for (i=0; i<nsplit; i++) {
      ierr = VecScatterCreate(xtmp,jac->is[i],jac->x[i],PETSC_NULL,&link->sctx);CHKERRQ(ierr);
      link = link->next;
    }
    ierr = VecDestroy(xtmp);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#define FieldSplitSplitSolveAdd(link,xx,yy) \
    (VecScatterBegin(xx,link->x,INSERT_VALUES,SCATTER_FORWARD,link->sctx) || \
     VecScatterEnd(xx,link->x,INSERT_VALUES,SCATTER_FORWARD,link->sctx) || \
     KSPSolve(link->ksp,link->x,link->y) || \
     VecScatterBegin(link->y,yy,ADD_VALUES,SCATTER_REVERSE,link->sctx) || \
     VecScatterEnd(link->y,yy,ADD_VALUES,SCATTER_REVERSE,link->sctx))

#undef __FUNCT__  
#define __FUNCT__ "PCApply_FieldSplit"
static PetscErrorCode PCApply_FieldSplit(PC pc,Vec x,Vec y)
{
  PC_FieldSplit     *jac = (PC_FieldSplit*)pc->data;
  PetscErrorCode    ierr;
  PC_FieldSplitLink link = jac->head;
  PetscScalar       zero = 0.0,mone = -1.0;

  PetscFunctionBegin;
  if (jac->type == PC_COMPOSITE_ADDITIVE) {
    if (jac->defaultsplit) {
      ierr = VecStrideGatherAll(x,jac->x,INSERT_VALUES);CHKERRQ(ierr);
      while (link) {
	ierr = KSPSolve(link->ksp,link->x,link->y);CHKERRQ(ierr);
	link = link->next;
      }
      ierr = VecStrideScatterAll(jac->y,y,INSERT_VALUES);CHKERRQ(ierr);
    } else {
      PetscInt    i = 0;

      ierr = VecSet(&zero,y);CHKERRQ(ierr);
      while (link) {
        ierr = FieldSplitSplitSolveAdd(link,x,y);CHKERRQ(ierr);
	link = link->next;
	i++;
      }
    }
  } else {
    if (!jac->w1) {
      ierr = VecDuplicate(x,&jac->w1);CHKERRQ(ierr);
      ierr = VecDuplicate(x,&jac->w2);CHKERRQ(ierr);
    }
    ierr = VecSet(&zero,y);CHKERRQ(ierr);
    ierr = FieldSplitSplitSolveAdd(link,x,y);CHKERRQ(ierr);
    while (link->next) {
      link = link->next;
      ierr = MatMult(pc->pmat,y,jac->w1);CHKERRQ(ierr);
      ierr = VecWAXPY(&mone,jac->w1,x,jac->w2);CHKERRQ(ierr);
      ierr = FieldSplitSplitSolveAdd(link,jac->w2,y);CHKERRQ(ierr);
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PCDestroy_FieldSplit"
static PetscErrorCode PCDestroy_FieldSplit(PC pc)
{
  PC_FieldSplit     *jac = (PC_FieldSplit*)pc->data;
  PetscErrorCode    ierr;
  PC_FieldSplitLink link = jac->head,next;

  PetscFunctionBegin;
  while (link) {
    ierr = KSPDestroy(link->ksp);CHKERRQ(ierr);
    if (link->x) {ierr = VecDestroy(link->x);CHKERRQ(ierr);}
    if (link->y) {ierr = VecDestroy(link->y);CHKERRQ(ierr);}
    if (link->sctx) {ierr = VecScatterDestroy(link->sctx);CHKERRQ(ierr);}
    next = link->next;
    ierr = PetscFree2(link,link->fields);CHKERRQ(ierr);
    link = next;
  }
  if (jac->x) {ierr = PetscFree2(jac->x,jac->y);CHKERRQ(ierr);}
  if (jac->pmat) {ierr = MatDestroyMatrices(jac->nsplits,&jac->pmat);CHKERRQ(ierr);}
  if (jac->is) {
    PetscInt i;
    for (i=0; i<jac->nsplits; i++) {ierr = ISDestroy(jac->is[i]);CHKERRQ(ierr);}
    ierr = PetscFree(jac->is);CHKERRQ(ierr);
  }
  if (jac->w1) {ierr = VecDestroy(jac->w1);CHKERRQ(ierr);}
  if (jac->w2) {ierr = VecDestroy(jac->w2);CHKERRQ(ierr);}
  ierr = PetscFree(jac);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PCSetFromOptions_FieldSplit"
static PetscErrorCode PCSetFromOptions_FieldSplit(PC pc)
/*   This does not call KSPSetFromOptions() on the subksp's, see PCSetFromOptionsBJacobi/ASM() */
{
  PetscErrorCode ierr;
  PetscInt       i = 0,nfields,fields[12],indx;
  PetscTruth     flg;
  char           optionname[128];
  const char     *types[] = {"additive","multiplicative"};

  PetscFunctionBegin;
  ierr = PetscOptionsHead("FieldSplit options");CHKERRQ(ierr);
  ierr = PetscOptionsEList("-pc_fieldsplit_type","Type of composition","PCFieldSplitSetType",types,2,types[0],&indx,&flg);CHKERRQ(ierr);
  if (flg) {
    ierr = PCFieldSplitSetType(pc,(PCCompositeType)indx);CHKERRQ(ierr);
  }
  while (PETSC_TRUE) {
    sprintf(optionname,"-pc_fieldsplit_%d_fields",i++);
    nfields = 12;
    ierr    = PetscOptionsIntArray(optionname,"Fields in this split","PCFieldSplitSetFields",fields,&nfields,&flg);CHKERRQ(ierr);
    if (!flg) break;
    if (!nfields) SETERRQ(PETSC_ERR_USER,"Cannot list zero fields");
    ierr = PCFieldSplitSetFields(pc,nfields,fields);CHKERRQ(ierr);
  }
  ierr = PetscOptionsTail();CHKERRQ(ierr);  
  PetscFunctionReturn(0);
}

/*------------------------------------------------------------------------------------*/

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "PCFieldSplitSetFields_FieldSplit"
PetscErrorCode PCFieldSplitSetFields_FieldSplit(PC pc,PetscInt n,PetscInt *fields)
{
  PC_FieldSplit     *jac = (PC_FieldSplit*)pc->data;
  PetscErrorCode    ierr;
  PC_FieldSplitLink link,next = jac->head;
  char              prefix[128];

  PetscFunctionBegin;
  if (n <= 0) SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"Negative number of fields requested");
  ierr = PetscMalloc2(1,struct _PC_FieldSplitLink,&link,n,PetscInt,&link->fields);CHKERRQ(ierr);
  ierr = PetscMemcpy(link->fields,fields,n*sizeof(PetscInt));CHKERRQ(ierr);
  link->nfields = n;
  link->next    = PETSC_NULL;
  ierr          = KSPCreate(pc->comm,&link->ksp);CHKERRQ(ierr);
  ierr          = KSPSetType(link->ksp,KSPPREONLY);CHKERRQ(ierr);

  if (pc->prefix) {
    sprintf(prefix,"%s_fieldsplit_%d_",pc->prefix,jac->nsplits);
  } else {
    sprintf(prefix,"fieldsplit_%d_",jac->nsplits);
  }
  ierr = KSPSetOptionsPrefix(link->ksp,prefix);CHKERRQ(ierr);

  if (!next) {
    jac->head = link;
  } else {
    while (next->next) {
      next = next->next;
    }
    next->next = link;
  }
  jac->nsplits++;
  PetscFunctionReturn(0);
}
EXTERN_C_END


EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "PCFieldSplitGetSubKSP_FieldSplit"
PetscErrorCode PCFieldSplitGetSubKSP_FieldSplit(PC pc,PetscInt *n,KSP **subksp)
{
  PC_FieldSplit     *jac = (PC_FieldSplit*)pc->data;
  PetscErrorCode    ierr;
  PetscInt          cnt = 0;
  PC_FieldSplitLink link;

  PetscFunctionBegin;
  ierr = PetscMalloc(jac->nsplits*sizeof(KSP*),subksp);CHKERRQ(ierr);
  while (link) {
    (*subksp)[cnt++] = link->ksp;
    link = link->next;
  }
  if (cnt != jac->nsplits) SETERRQ2(PETSC_ERR_PLIB,"Corrupt PCFIELDSPLIT object: number splits in linked list %D in object %D",cnt,jac->nsplits);
  *n = jac->nsplits;
  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "PCFieldSplitSetFields"
/*@
    PCFieldSplitSetFields - Sets the fields for one particular split in the field split preconditioner

    Collective on PC

    Input Parameters:
+   pc  - the preconditioner context
.   n - the number of fields in this split
.   fields - the fields in this split

    Level: intermediate

.seealso: PCFieldSplitGetSubKSP(), PCFIELDSPLIT

@*/
PetscErrorCode PCFieldSplitSetFields(PC pc,PetscInt n, PetscInt *fields)
{
  PetscErrorCode ierr,(*f)(PC,PetscInt,PetscInt *);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE,1);
  ierr = PetscObjectQueryFunction((PetscObject)pc,"PCFieldSplitSetFields_C",(void (**)(void))&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(pc,n,fields);CHKERRQ(ierr);
  } 
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PCFieldSplitGetSubKSP"
/*@C
   PCFieldSplitGetSubKSP - Gets the KSP contexts for all splits
   
   Collective on KSP

   Input Parameter:
.  pc - the preconditioner context

   Output Parameters:
+  n - the number of split
-  pc - the array of KSP contexts

   Note:  
   After PCFieldSplitGetSubKSP() the array of KSPs IS to be freed

   You must call KSPSetUp() before calling PCFieldSplitGetSubKSP().

   Level: advanced

.seealso: PCFIELDSPLIT
@*/
PetscErrorCode PCFieldSplitGetSubKSP(PC pc,PetscInt *n,KSP *subksp[])
{
  PetscErrorCode ierr,(*f)(PC,PetscInt*,KSP **);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE,1);
  PetscValidIntPointer(n,2);
  ierr = PetscObjectQueryFunction((PetscObject)pc,"PCFieldSplitGetSubKSP_C",(void (**)(void))&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(pc,n,subksp);CHKERRQ(ierr);
  } else {
    SETERRQ(PETSC_ERR_ARG_WRONG,"Cannot get subksp for this type of PC");
  }
  PetscFunctionReturn(0);
}

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "PCFieldSplitSetType_FieldSplit"
PetscErrorCode PCFieldSplitSetType_FieldSplit(PC pc,PCCompositeType type)
{
  PC_FieldSplit  *jac = (PC_FieldSplit*)pc->data;

  PetscFunctionBegin;
  jac->type = type;
  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "PCFieldSplitSetType"
/*@C
   PCFieldSplitSetType - Sets the type of fieldsplit preconditioner.
   
   Collective on PC

   Input Parameter:
.  pc - the preconditioner context
.  type - PC_COMPOSITE_ADDITIVE (default), PC_COMPOSITE_MULTIPLICATIVE

   Options Database Key:
.  -pc_fieldsplit_type <type: one of multiplicative, additive, special> - Sets fieldsplit preconditioner type

   Level: Developer

.keywords: PC, set, type, composite preconditioner, additive, multiplicative

.seealso: PCCompositeSetType()

@*/
PetscErrorCode PCFieldSplitSetType(PC pc,PCCompositeType type)
{
  PetscErrorCode ierr,(*f)(PC,PCCompositeType);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE,1);
  ierr = PetscObjectQueryFunction((PetscObject)pc,"PCFieldSplitSetType_C",(void (**)(void))&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(pc,type);CHKERRQ(ierr);
  } 
  PetscFunctionReturn(0);
}

/* -------------------------------------------------------------------------------------*/
/*MC
   PCFieldSplit - Preconditioner created by combining seperate preconditioners for individual
                  fields or groups of fields


     To set options on the solvers for each block append -sub_ to all the PC
        options database keys. For example, -sub_pc_type ilu -sub_pc_ilu_levels 1
        
     To set the options on the solvers seperate for each block call PCFieldSplitGetSubKSP()
         and set the options directly on the resulting KSP object

   Level: intermediate

   Options Database Keys:
+   -pc_splitfields_xxx_fields <a,b,..> - indicates the fields to be used in the xxx'th split
.   -pc_splitfields_default - automatically add any fields to additional splits that have not
                              been supplied explicitly by -pc_splitfields_xxx_fields
-   -pc_splitfields_type <additive,multiplicative>

   Concepts: physics based preconditioners

.seealso:  PCCreate(), PCSetType(), PCType (for list of available types), PC,
           PCFieldSplitGetSubKSP(), PCFieldSplitSetFields()
M*/

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "PCCreate_FieldSplit"
PetscErrorCode PCCreate_FieldSplit(PC pc)
{
  PetscErrorCode ierr;
  PC_FieldSplit  *jac;

  PetscFunctionBegin;
  ierr = PetscNew(PC_FieldSplit,&jac);CHKERRQ(ierr);
  PetscLogObjectMemory(pc,sizeof(PC_FieldSplit));
  ierr = PetscMemzero(jac,sizeof(PC_FieldSplit));CHKERRQ(ierr);
  jac->bs      = -1;
  jac->nsplits = 0;
  jac->type    = PC_COMPOSITE_ADDITIVE;
  pc->data     = (void*)jac;

  pc->ops->apply             = PCApply_FieldSplit;
  pc->ops->setup             = PCSetUp_FieldSplit;
  pc->ops->destroy           = PCDestroy_FieldSplit;
  pc->ops->setfromoptions    = PCSetFromOptions_FieldSplit;
  pc->ops->view              = PCView_FieldSplit;
  pc->ops->applyrichardson   = 0;

  ierr = PetscObjectComposeFunctionDynamic((PetscObject)pc,"PCFieldSplitGetSubKSP_C","PCFieldSplitGetSubKSP_FieldSplit",
                    PCFieldSplitGetSubKSP_FieldSplit);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)pc,"PCFieldSplitSetFields_C","PCFieldSplitSetFields_FieldSplit",
                    PCFieldSplitSetFields_FieldSplit);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)pc,"PCFieldSplitSetType_C","PCFieldSplitSetType_FieldSplit",
                    PCFieldSplitSetType_FieldSplit);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END


