/*
    Index sets for scatter-gather type operations in vectors
and matrices. 

*/

#if !defined(_IS_H)
#define _IS_H

#include "petscis.h"

struct _ISOps {
  PetscErrorCode (*getsize)(IS,int*),
       (*getlocalsize)(IS,int*),
       (*getindices)(IS,int**),
       (*restoreindices)(IS,int**),
       (*invertpermutation)(IS,int,IS*),
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
    int getsize(int*) {return 0;};
};
#endif

struct _p_IS {
  PETSCHEADER(struct _ISOps)
#if defined(__cplusplus)
  ISOps        *cops;
#endif
  PetscTruth   isperm;          /* if is a permutation */
  int          max,min;         /* range of possible values */
  void         *data;
  PetscTruth   isidentity;
};


#endif
