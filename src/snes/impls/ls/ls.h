/* 
   Private context for a Newton line search method for solving
   systems of nonlinear equations
 */

#ifndef __SNES_LS_H
#define __SNES_LS_H
#include "src/snes/snesimpl.h"

typedef struct {
  PetscErrorCode (*LineSearch)(SNES,void*,Vec,Vec,Vec,Vec,Vec,PetscReal,PetscReal*,PetscReal*,PetscTruth*);
  void           *lsP;                              /* user-defined line-search context (optional) */
  /* --------------- Parameters used by line search method ----------------- */
  PetscReal      alpha;		                    /* used to determine sufficient reduction */
  PetscReal      maxstep;                           /* maximum step size */
  PetscReal      steptol;                           /* step convergence tolerance */
  PetscErrorCode (*CheckStep)(SNES,void*,Vec,PetscTruth*); /* step-checking routine (optional) */
  void           *checkP;                           /* user-defined step-checking context (optional) */
} SNES_LS;

#endif

