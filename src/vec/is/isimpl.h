/*
    Index sets for scatter-gather type operations in vectors
and matrices. 

*/

#if !defined(_IS_H)
#define _IS_H

#include "petscis.h"

struct _ISOps {
  PetscErrorCode (*getsize)(IS,PetscInt*),
                 (*getlocalsize)(IS,PetscInt*),
                 (*getindices)(IS,PetscInt**),
                 (*restoreindices)(IS,PetscInt**),
                 (*invertpermutation)(IS,PetscInt,IS*),
                 (*sortindices)(IS),
                 (*sorted)(IS,PetscTruth *),
                 (*duplicate)(IS,IS *),
                 (*destroy)(IS),
                 (*view)(IS,PetscViewer),
                 (*identity)(IS,PetscTruth*);
};

#if defined(__cplusplus)
class ISOps {
  public:
    int getsize(PetscInt*) {return 0;};
};
#endif

struct _p_IS {
  PETSCHEADER(struct _ISOps)
#if defined(__cplusplus)
  ISOps        *cops;
#endif
  PetscTruth   isperm;          /* if is a permutation */
  PetscInt     max,min;         /* range of possible values */
  void         *data;
  PetscTruth   isidentity;
};


#endif
