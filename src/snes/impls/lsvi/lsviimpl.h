#include "private/snesimpl.h"

#define PETSC_LSVI_INF   1.0e20
#define PETSC_LSVI_NINF -1.0e20
#define PETSC_LSVI_EPS  2.2204460492503131e-16

#define PetscScalarNorm(a,b) (PetscSqrtScalar((a)*(a)+(b)*(b)))
/* 
   Private context for semismooth newton method with line search for solving
   system of mixed complementarity equations
 */

#ifndef __SNES_LSVI_H
#define __SNES_LSVI_H

typedef struct {
  PetscErrorCode           (*LineSearch)(SNES,void*,Vec,Vec,Vec,Vec,Vec,PetscReal,PetscReal,PetscReal*,PetscReal*,PetscTruth*);
  void                     *lsP;                              /* user-defined line-search context (optional) */
  /* --------------- Parameters used by line search method ----------------- */
  PetscReal                alpha;		                                                   /* used to determine sufficient reduction */
  PetscReal                maxstep;                                                          /* maximum step size */
  PetscReal                minlambda;                                                        /* determines smallest line search lambda used */
  PetscErrorCode           (*precheckstep)(SNES,Vec,Vec,void*,PetscTruth*);                  /* step-checking routine (optional) */
  void                     *precheck;                                                        /* user-defined step-checking context (optional) */
  PetscErrorCode           (*postcheckstep)(SNES,Vec,Vec,Vec,void*,PetscTruth*,PetscTruth*); /* step-checking routine (optional) */
  void                     *postcheck;                                                       /* user-defined step-checking context (optional) */
  PetscViewerASCIIMonitor  monitor;

  /* ------------------ Semismooth algorithm stuff ------------------------------ */
  Vec                      phi;                      /* pointer to semismooth function */
  PetscReal                phinorm;                 /* 2-norm of the semismooth function */
  PetscErrorCode           (*computessfunction)(PetscScalar,PetscScalar,PetscScalar*); /* Semismooth function evaluation routine */
  PetscReal                merit;           /* Merit function */
  Vec                      dpsi;          /* Gradient of merit function */
  Vec                      Da;            /* B sub-differential work vector (diag perturbation) */
  Vec                      Db;            /* B sub-differential work vector (row scaling) */
  Vec                      z;    /* B subdifferential work vector */
  Vec                      t;    /* B subdifferential work vector */
  Vec                      xl;            /* lower bound on variables */
  Vec                      xu;            /* upper bound on variables */
  PetscTruth               usersetxbounds; /* flag to indicate whether the user has set bounds on variables */

  PetscScalar             norm_d;         /* two norm of the descent direction */
  /* Parameters for checking sufficient descent conditions satisfied */
  PetscReal             rho;
  PetscReal             delta;
} SNES_LSVI;

#endif

extern PetscErrorCode SNESLSVISetVariableBounds(SNES,Vec,Vec);
