/* $Id: ilu.h,v 1.13 2001/07/11 20:06:23 bsmith Exp $ */

/* 
   Private data structure for ILU preconditioner.
*/
#if !defined(__ILU_H)
#define __ILU_H

#include "petscmat.h"

typedef struct {
  Mat               fact;             /* factored matrix */
  MatOrderingType   ordering;         /* matrix reordering */
  IS                row,col;         /* row and column permutations for reordering */
  void              *implctx;         /* private implementation context */
  PetscTruth        inplace;          /* in-place ILU factorization */
  PetscTruth        reuseordering;    /* reuses previous reordering computed */

  PetscTruth        usedt;            /* use drop tolerance form of ILU */
  PetscTruth        reusefill;        /* reuse fill from previous ILUDT */
  double            actualfill;       /* expected fill in factorization */
  MatILUInfo        info;
  PetscTruth        single_precision_solve; 
} PC_ILU;

#endif
