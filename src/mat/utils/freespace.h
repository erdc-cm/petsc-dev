#if !defined(_FreeSpace_h_)
#define _FreeSpace_h_

#include "petsc.h"

typedef struct _Space *FreeSpaceList;

typedef struct _Space {
  FreeSpaceList more_space;
  int           *array;
  int           *array_head;
  int           total_array_size;
  int           local_used;
  int           local_remaining;
} FreeSpace;  

PetscErrorCode GetMoreSpace(int size,FreeSpaceList *list);
PetscErrorCode MakeSpaceContiguous(FreeSpaceList *head,int *space);

#endif
