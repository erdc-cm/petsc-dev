/*$Id: sor.c,v 1.103 2001/08/21 21:03:17 bsmith Exp $*/

/*
   Defines a  (S)SOR  preconditioner for any Mat implementation
*/
#include "src/sles/pc/pcimpl.h"               /*I "petscpc.h" I*/

typedef struct {
  int        its;        /* inner iterations, number of sweeps */
  int        lits;       /* local inner iterations, number of sweeps applied by the local matrix mat->A */
  MatSORType sym;        /* forward, reverse, symmetric etc. */
  PetscReal  omega;
} PC_SOR;

#undef __FUNCT__  
#define __FUNCT__ "PCDestroy_SOR"
static int PCDestroy_SOR(PC pc)
{
  PC_SOR *jac = (PC_SOR*)pc->data;
  int    ierr;

  PetscFunctionBegin;
  ierr = PetscFree(jac);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PCApply_SOR"
static int PCApply_SOR(PC pc,Vec x,Vec y)
{
  PC_SOR *jac = (PC_SOR*)pc->data;
  int    ierr,flag = jac->sym | SOR_ZERO_INITIAL_GUESS;

  PetscFunctionBegin;
  ierr = MatRelax(pc->pmat,x,jac->omega,(MatSORType)flag,0.0,jac->its,jac->lits,y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PCApplyRichardson_SOR"
static int PCApplyRichardson_SOR(PC pc,Vec b,Vec y,Vec w,PetscReal rtol,PetscReal atol, PetscReal dtol,int its)
{
  PC_SOR *jac = (PC_SOR*)pc->data;
  int    ierr;

  PetscFunctionBegin;
  PetscLogInfo(pc,"PCApplyRichardson_SOR: Warning, convergence critera ignored, using %d iterations\n",its);
  its  = its*jac->its;
  ierr = MatRelax(pc->pmat,b,jac->omega,(MatSORType)jac->sym,0.0,its,jac->lits,y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PCSetFromOptions_SOR"
static int PCSetFromOptions_SOR(PC pc)
{
  PC_SOR     *jac = (PC_SOR*)pc->data;
  int        ierr;
  PetscTruth flg;

  PetscFunctionBegin;
  ierr = PetscOptionsHead("(S)SOR options");CHKERRQ(ierr);
    ierr = PetscOptionsReal("-pc_sor_omega","relaxation factor (0 < omega < 2)","PCSORSetOmega",jac->omega,&jac->omega,0);CHKERRQ(ierr);
    ierr = PetscOptionsInt("-pc_sor_its","number of inner SOR iterations","PCSORSetIterations",jac->its,&jac->its,0);CHKERRQ(ierr);
    ierr = PetscOptionsInt("-pc_sor_lits","number of local inner SOR iterations","PCSORSetIterations",jac->lits,&jac->lits,0);CHKERRQ(ierr);
    ierr = PetscOptionsLogicalGroupBegin("-pc_sor_symmetric","SSOR, not SOR","PCSORSetSymmetric",&flg);CHKERRQ(ierr);
    if (flg) {ierr = PCSORSetSymmetric(pc,SOR_SYMMETRIC_SWEEP);CHKERRQ(ierr);}
    ierr = PetscOptionsLogicalGroup("-pc_sor_backward","use backward sweep instead of forward","PCSORSetSymmetric",&flg);CHKERRQ(ierr);
    if (flg) {ierr = PCSORSetSymmetric(pc,SOR_BACKWARD_SWEEP);CHKERRQ(ierr);}
    ierr = PetscOptionsLogicalGroup("-pc_sor_local_symmetric","use SSOR seperately on each processor","PCSORSetSymmetric",&flg);CHKERRQ(ierr);
    if (flg) {ierr = PCSORSetSymmetric(pc,SOR_LOCAL_SYMMETRIC_SWEEP);CHKERRQ(ierr);}
    ierr = PetscOptionsLogicalGroup("-pc_sor_local_backward","use backward sweep locally","PCSORSetSymmetric",&flg);CHKERRQ(ierr);
    if (flg) {ierr = PCSORSetSymmetric(pc,SOR_LOCAL_BACKWARD_SWEEP);CHKERRQ(ierr);}
    ierr = PetscOptionsLogicalGroupEnd("-pc_sor_local_forward","use forward sweep locally","PCSORSetSymmetric",&flg);CHKERRQ(ierr);
    if (flg) {ierr = PCSORSetSymmetric(pc,SOR_LOCAL_FORWARD_SWEEP);CHKERRQ(ierr);}
  ierr = PetscOptionsTail();CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PCView_SOR"
static int PCView_SOR(PC pc,PetscViewer viewer)
{
  PC_SOR     *jac = (PC_SOR*)pc->data;
  MatSORType sym = jac->sym;
  char       *sortype;
  int        ierr;
  PetscTruth isascii;

  PetscFunctionBegin;
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_ASCII,&isascii);CHKERRQ(ierr);
  if (isascii) {
    if (sym & SOR_ZERO_INITIAL_GUESS) {ierr = PetscViewerASCIIPrintf(viewer,"  SOR:  zero initial guess\n");CHKERRQ(ierr);}
    if (sym == SOR_APPLY_UPPER)              sortype = "apply_upper";
    else if (sym == SOR_APPLY_LOWER)         sortype = "apply_lower";
    else if (sym & SOR_EISENSTAT)            sortype = "Eisenstat";
    else if ((sym & SOR_SYMMETRIC_SWEEP) == SOR_SYMMETRIC_SWEEP)
                                             sortype = "symmetric";
    else if (sym & SOR_BACKWARD_SWEEP)       sortype = "backward";
    else if (sym & SOR_FORWARD_SWEEP)        sortype = "forward";
    else if ((sym & SOR_LOCAL_SYMMETRIC_SWEEP) == SOR_LOCAL_SYMMETRIC_SWEEP)
                                             sortype = "local_symmetric";
    else if (sym & SOR_LOCAL_FORWARD_SWEEP)  sortype = "local_forward";
    else if (sym & SOR_LOCAL_BACKWARD_SWEEP) sortype = "local_backward"; 
    else                                     sortype = "unknown";
    ierr = PetscViewerASCIIPrintf(viewer,"  SOR: type = %s, iterations = %d, omega = %g\n",sortype,jac->its,jac->omega);CHKERRQ(ierr);
  } else {
    SETERRQ1(1,"Viewer type %s not supported for PCSOR",((PetscObject)viewer)->type_name);
  }
  PetscFunctionReturn(0);
}


/* ------------------------------------------------------------------------------*/
EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "PCSORSetSymmetric_SOR"
int PCSORSetSymmetric_SOR(PC pc,MatSORType flag)
{
  PC_SOR *jac;

  PetscFunctionBegin;
  jac = (PC_SOR*)pc->data; 
  jac->sym = flag;
  PetscFunctionReturn(0);
}
EXTERN_C_END

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "PCSORSetOmega_SOR"
int PCSORSetOmega_SOR(PC pc,PetscReal omega)
{
  PC_SOR *jac;

  PetscFunctionBegin;
  if (omega >= 2.0 || omega <= 0.0) SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"Relaxation out of range");
  jac        = (PC_SOR*)pc->data; 
  jac->omega = omega;
  PetscFunctionReturn(0);
}
EXTERN_C_END

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "PCSORSetIterations_SOR"
int PCSORSetIterations_SOR(PC pc,int its,int lits)
{
  PC_SOR *jac;

  PetscFunctionBegin;
  jac      = (PC_SOR*)pc->data; 
  jac->its = its;
  jac->lits = lits;
  PetscFunctionReturn(0);
}
EXTERN_C_END

/* ------------------------------------------------------------------------------*/
#undef __FUNCT__  
#define __FUNCT__ "PCSORSetSymmetric"
/*@
   PCSORSetSymmetric - Sets the SOR preconditioner to use symmetric (SSOR), 
   backward, or forward relaxation.  The local variants perform SOR on
   each processor.  By default forward relaxation is used.

   Collective on PC

   Input Parameters:
+  pc - the preconditioner context
-  flag - one of the following
.vb
    SOR_FORWARD_SWEEP
    SOR_BACKWARD_SWEEP
    SOR_SYMMETRIC_SWEEP
    SOR_LOCAL_FORWARD_SWEEP
    SOR_LOCAL_BACKWARD_SWEEP
    SOR_LOCAL_SYMMETRIC_SWEEP
.ve

   Options Database Keys:
+  -pc_sor_symmetric - Activates symmetric version
.  -pc_sor_backward - Activates backward version
.  -pc_sor_local_forward - Activates local forward version
.  -pc_sor_local_symmetric - Activates local symmetric version
-  -pc_sor_local_backward - Activates local backward version

   Notes: 
   To use the Eisenstat trick with SSOR, employ the PCEISENSTAT preconditioner,
   which can be chosen with the option 
.  -pc_type eisenstat - Activates Eisenstat trick

   Level: intermediate

.keywords: PC, SOR, SSOR, set, relaxation, sweep, forward, backward, symmetric

.seealso: PCEisenstatSetOmega(), PCSORSetIterations(), PCSORSetOmega()
@*/
int PCSORSetSymmetric(PC pc,MatSORType flag)
{
  int ierr,(*f)(PC,MatSORType);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  ierr = PetscObjectQueryFunction((PetscObject)pc,"PCSORSetSymmetric_C",(void (**)(void))&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(pc,flag);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PCSORSetOmega"
/*@
   PCSORSetOmega - Sets the SOR relaxation coefficient, omega
   (where omega = 1.0 by default).

   Collective on PC

   Input Parameters:
+  pc - the preconditioner context
-  omega - relaxation coefficient (0 < omega < 2). 

   Options Database Key:
.  -pc_sor_omega <omega> - Sets omega

   Level: intermediate

.keywords: PC, SOR, SSOR, set, relaxation, omega

.seealso: PCSORSetSymmetric(), PCSORSetIterations(), PCEisenstatSetOmega()
@*/
int PCSORSetOmega(PC pc,PetscReal omega)
{
  int ierr,(*f)(PC,PetscReal);

  PetscFunctionBegin;
  ierr = PetscObjectQueryFunction((PetscObject)pc,"PCSORSetOmega_C",(void (**)(void))&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(pc,omega);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PCSORSetIterations"
/*@
   PCSORSetIterations - Sets the number of inner iterations to 
   be used by the SOR preconditioner. The default is 1.

   Collective on PC

   Input Parameters:
+  pc - the preconditioner context
.  lits - number of local iterations, smoothings over just variables on processor
-  its - number of parallel iterations to use; each parallel iteration has lits local iterations

   Options Database Key:
+  -pc_sor_its <its> - Sets number of iterations
-  -pc_sor_lits <lits> - Sets number of local iterations

   Level: intermediate

   Notes: When run on one processor the number of smoothings is lits*its

.keywords: PC, SOR, SSOR, set, iterations

.seealso: PCSORSetOmega(), PCSORSetSymmetric()
@*/
int PCSORSetIterations(PC pc,int its,int lits)
{
  int ierr,(*f)(PC,int,int);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  ierr = PetscObjectQueryFunction((PetscObject)pc,"PCSORSetIterations_C",(void (**)(void))&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(pc,its,lits);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

/*MC
     PCSOR - (S)SOR (successive over relaxation, Gauss-Seidel) preconditioning

   Options Database Keys:
+  -pc_sor_symmetric - Activates symmetric version
.  -pc_sor_backward - Activates backward version
.  -pc_sor_local_forward - Activates local forward version
.  -pc_sor_local_symmetric - Activates local symmetric version
.  -pc_sor_local_backward - Activates local backward version
.  -pc_sor_omega <omega> - Sets omega
.  -pc_sor_its <its> - Sets number of iterations
-  -pc_sor_lits <lits> - Sets number of local iterations

   Level: beginner

  Concepts: SOR, preconditioners, Gauss-Seidel

   Notes: Only implemented for the AIJ matrix format.
          Not a true parallel SOR, in parallel this implementation corresponds to block
          Jacobi with SOR on each block.

.seealso:  PCCreate(), PCSetType(), PCType (for list of available types), PC,
           PCSORSetIterations(), PCSORSetSymmetric(), PCSORSetOmega(), PCEISENSTAT
M*/

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "PCCreate_SOR"
int PCCreate_SOR(PC pc)
{
  int    ierr;
  PC_SOR *jac;

  PetscFunctionBegin;
  ierr = PetscNew(PC_SOR,&jac);CHKERRQ(ierr);
  PetscLogObjectMemory(pc,sizeof(PC_SOR));

  pc->ops->apply           = PCApply_SOR;
  pc->ops->applyrichardson = PCApplyRichardson_SOR;
  pc->ops->setfromoptions  = PCSetFromOptions_SOR;
  pc->ops->setup           = 0;
  pc->ops->view            = PCView_SOR;
  pc->ops->destroy         = PCDestroy_SOR;
  pc->data           = (void*)jac;
  jac->sym           = SOR_FORWARD_SWEEP;
  jac->omega         = 1.0;
  jac->its           = 1;
  jac->lits          = 1;

  ierr = PetscObjectComposeFunctionDynamic((PetscObject)pc,"PCSORSetSymmetric_C","PCSORSetSymmetric_SOR",
                    PCSORSetSymmetric_SOR);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)pc,"PCSORSetOmega_C","PCSORSetOmega_SOR",
                    PCSORSetOmega_SOR);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)pc,"PCSORSetIterations_C","PCSORSetIterations_SOR",
                    PCSORSetIterations_SOR);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}
EXTERN_C_END





