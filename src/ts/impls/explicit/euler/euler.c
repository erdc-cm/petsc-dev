/*
       Code for Timestepping with explicit Euler.
*/
#include "src/ts/tsimpl.h"                /*I   "petscts.h"   I*/

typedef struct {
  Vec update;     /* work vector where F(t[i],u[i]) is stored */
} TS_Euler;

#undef __FUNCT__  
#define __FUNCT__ "TSSetUp_Euler"
static int TSSetUp_Euler(TS ts)
{
  TS_Euler *euler = (TS_Euler*)ts->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = VecDuplicate(ts->vec_sol,&euler->update);CHKERRQ(ierr);  
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSStep_Euler"
static int TSStep_Euler(TS ts,int *steps,PetscReal *ptime)
{
  TS_Euler *euler = (TS_Euler*)ts->data;
  Vec      sol = ts->vec_sol,update = euler->update;
  PetscErrorCode ierr;
  int i,max_steps = ts->max_steps;
  PetscScalar   dt = ts->time_step;
  
  PetscFunctionBegin;
  *steps = -ts->steps;
  ierr = TSMonitor(ts,ts->steps,ts->ptime,sol);CHKERRQ(ierr);

  for (i=0; i<max_steps; i++) {
    ts->ptime += ts->time_step;
    ierr = TSComputeRHSFunction(ts,ts->ptime,sol,update);CHKERRQ(ierr);
    ierr = VecAXPY(&dt,update,sol);CHKERRQ(ierr);
    ts->steps++;
    ierr = TSMonitor(ts,ts->steps,ts->ptime,sol);CHKERRQ(ierr);
    if (ts->ptime > ts->max_time) break;
  }

  *steps += ts->steps;
  *ptime  = ts->ptime;
  PetscFunctionReturn(0);
}
/*------------------------------------------------------------*/
#undef __FUNCT__  
#define __FUNCT__ "TSDestroy_Euler"
static int TSDestroy_Euler(TS ts)
{
  TS_Euler *euler = (TS_Euler*)ts->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (euler->update) {ierr = VecDestroy(euler->update);CHKERRQ(ierr);}
  ierr = PetscFree(euler);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
/*------------------------------------------------------------*/

#undef __FUNCT__  
#define __FUNCT__ "TSSetFromOptions_Euler"
static int TSSetFromOptions_Euler(TS ts)
{
  PetscFunctionBegin;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSView_Euler"
static int TSView_Euler(TS ts,PetscViewer viewer)
{
  PetscFunctionBegin;
  PetscFunctionReturn(0);
}

/* ------------------------------------------------------------ */

/*MC
      TS_EULER - ODE solver using the explicit forward Euler method

  Level: beginner

.seealso:  TSCreate(), TS, TSSetType(), TS_BEULER

M*/
EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "TSCreate_Euler"
PetscErrorCode TSCreate_Euler(TS ts)
{
  TS_Euler *euler;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ts->ops->setup           = TSSetUp_Euler;
  ts->ops->step            = TSStep_Euler;
  ts->ops->destroy         = TSDestroy_Euler;
  ts->ops->setfromoptions  = TSSetFromOptions_Euler;
  ts->ops->view            = TSView_Euler;

  ierr = PetscNew(TS_Euler,&euler);CHKERRQ(ierr);
  PetscLogObjectMemory(ts,sizeof(TS_Euler));
  ts->data = (void*)euler;

  PetscFunctionReturn(0);
}
EXTERN_C_END




