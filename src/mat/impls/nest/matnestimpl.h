
#ifndef MatNest_impl_h
#define MatNest_impl_h

#include <private/matimpl.h>

struct MatNestISPair {
  IS *row,*col;
};

typedef struct {
  PetscInt           nr,nc;        /* nr x nc blocks */
  Mat                **m;
  struct MatNestISPair isglobal;
  struct MatNestISPair islocal;
  Vec                *left,*right;
  PetscInt           *row_len,*col_len;
} Mat_Nest;

#endif
