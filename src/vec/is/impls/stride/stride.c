/*
       Index sets of evenly space integers, defined by a 
    start, stride and length.
*/
#include "src/vec/is/isimpl.h"             /*I   "petscis.h"   I*/

EXTERN PetscErrorCode VecInitializePackage(char *);

typedef struct {
  int N,n,first,step;
} IS_Stride;

#undef __FUNCT__  
#define __FUNCT__ "ISIdentity_Stride" 
PetscErrorCode ISIdentity_Stride(IS is,PetscTruth *ident)
{
  IS_Stride *is_stride = (IS_Stride*)is->data;

  PetscFunctionBegin;
  is->isidentity = PETSC_FALSE;
  *ident         = PETSC_FALSE;
  if (is_stride->first != 0) PetscFunctionReturn(0);
  if (is_stride->step  != 1) PetscFunctionReturn(0);
  *ident          = PETSC_TRUE;
  is->isidentity  = PETSC_TRUE;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "ISDuplicate_Stride" 
PetscErrorCode ISDuplicate_Stride(IS is,IS *newIS)
{
  PetscErrorCode ierr;
  IS_Stride *sub = (IS_Stride*)is->data;

  PetscFunctionBegin;
  ierr = ISCreateStride(is->comm,sub->n,sub->first,sub->step,newIS);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "ISInvertPermutation_Stride" 
PetscErrorCode ISInvertPermutation_Stride(IS is,int nlocal,IS *perm)
{
  IS_Stride *isstride = (IS_Stride*)is->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (is->isidentity) {
    ierr = ISCreateStride(PETSC_COMM_SELF,isstride->n,0,1,perm);CHKERRQ(ierr);
  } else {
    IS  tmp;
    int *indices,n = isstride->n;
    ierr = ISGetIndices(is,&indices);CHKERRQ(ierr);
    ierr = ISCreateGeneral(is->comm,n,indices,&tmp);CHKERRQ(ierr);
    ierr = ISRestoreIndices(is,&indices);CHKERRQ(ierr);
    ierr = ISInvertPermutation(tmp,nlocal,perm);CHKERRQ(ierr);
    ierr = ISDestroy(tmp);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}
    
#undef __FUNCT__  
#define __FUNCT__ "ISStrideGetInfo" 
/*@
   ISStrideGetInfo - Returns the first index in a stride index set and 
   the stride width.

   Not Collective

   Input Parameter:
.  is - the index set

   Output Parameters:
.  first - the first index
.  step - the stride width

   Level: intermediate

   Notes:
   Returns info on stride index set. This is a pseudo-public function that
   should not be needed by most users.

   Concepts: index sets^getting information
   Concepts: IS^getting information

.seealso: ISCreateStride(), ISGetSize()
@*/
PetscErrorCode ISStrideGetInfo(IS is,int *first,int *step)
{
  IS_Stride *sub;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(is,IS_COOKIE,1);
  if (first) PetscValidIntPointer(first,2);
  if (step) PetscValidIntPointer(step,3);

  sub = (IS_Stride*)is->data;
  if (is->type != IS_STRIDE) PetscFunctionReturn(0);
  if (first) *first = sub->first; 
  if (step)  *step  = sub->step;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "ISStride" 
/*@C
   ISStride - Determines if an IS is based on a stride.

   Not Collective

   Input Parameter:
.  is - the index set

   Output Parameters:
.  flag - either PETSC_TRUE or PETSC_FALSE

   Level: intermediate

   Concepts: index sets^is it stride
   Concepts: IS^is it stride

.seealso: ISCreateStride(), ISGetSize()
@*/
PetscErrorCode ISStride(IS is,PetscTruth *flag)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(is,IS_COOKIE,1);
  PetscValidIntPointer(flag,2);

  if (is->type != IS_STRIDE) *flag = PETSC_FALSE;
  else                       *flag = PETSC_TRUE;

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "ISDestroy_Stride" 
PetscErrorCode ISDestroy_Stride(IS is)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscFree(is->data);CHKERRQ(ierr);
  PetscLogObjectDestroy(is);
  PetscHeaderDestroy(is); PetscFunctionReturn(0);
}

/*
     Returns a legitimate index memory even if 
   the stride index set is empty.
*/
#undef __FUNCT__  
#define __FUNCT__ "ISGetIndices_Stride" 
PetscErrorCode ISGetIndices_Stride(IS in,int **idx)
{
  IS_Stride *sub = (IS_Stride*)in->data;
  PetscErrorCode ierr;
  int       i;

  PetscFunctionBegin;
  ierr      = PetscMalloc((sub->n+1)*sizeof(int),idx);CHKERRQ(ierr);
  (*idx)[0] = sub->first;
  for (i=1; i<sub->n; i++) (*idx)[i] = (*idx)[i-1] + sub->step;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "ISRestoreIndices_Stride" 
PetscErrorCode ISRestoreIndices_Stride(IS in,int **idx)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscFree(*idx);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "ISGetSize_Stride" 
PetscErrorCode ISGetSize_Stride(IS is,int *size)
{
  IS_Stride *sub = (IS_Stride *)is->data;

  PetscFunctionBegin;
  *size = sub->N; 
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "ISGetLocalSize_Stride" 
PetscErrorCode ISGetLocalSize_Stride(IS is,int *size)
{
  IS_Stride *sub = (IS_Stride *)is->data;

  PetscFunctionBegin;
  *size = sub->n; 
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "ISView_Stride" 
PetscErrorCode ISView_Stride(IS is,PetscViewer viewer)
{
  IS_Stride   *sub = (IS_Stride *)is->data;
  int         i,n = sub->n,rank,size;
  PetscTruth  iascii;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_ASCII,&iascii);CHKERRQ(ierr);
  if (iascii) { 
    ierr = MPI_Comm_rank(is->comm,&rank);CHKERRQ(ierr);
    ierr = MPI_Comm_size(is->comm,&size);CHKERRQ(ierr);
    if (size == 1) {
      if (is->isperm) {
        ierr = PetscViewerASCIISynchronizedPrintf(viewer,"Index set is permutation\n");CHKERRQ(ierr);
      }
      ierr = PetscViewerASCIISynchronizedPrintf(viewer,"Number of indices in (stride) set %d\n",n);CHKERRQ(ierr);
      for (i=0; i<n; i++) {
        ierr = PetscViewerASCIISynchronizedPrintf(viewer,"%d %d\n",i,sub->first + i*sub->step);CHKERRQ(ierr);
      }
    } else {
      if (is->isperm) {
        ierr = PetscViewerASCIISynchronizedPrintf(viewer,"[%d] Index set is permutation\n",rank);CHKERRQ(ierr);
      }
      ierr = PetscViewerASCIISynchronizedPrintf(viewer,"[%d] Number of indices in (stride) set %d\n",rank,n);CHKERRQ(ierr);
      for (i=0; i<n; i++) {
        ierr = PetscViewerASCIISynchronizedPrintf(viewer,"[%d] %d %d\n",rank,i,sub->first + i*sub->step);CHKERRQ(ierr);
      }
    }
    ierr = PetscViewerFlush(viewer);CHKERRQ(ierr);
  } else {
    SETERRQ1(PETSC_ERR_SUP,"Viewer type %s not supported for this object",((PetscObject)viewer)->type_name);
  }
  PetscFunctionReturn(0);
}
  
#undef __FUNCT__  
#define __FUNCT__ "ISSort_Stride" 
PetscErrorCode ISSort_Stride(IS is)
{
  IS_Stride *sub = (IS_Stride*)is->data;

  PetscFunctionBegin;
  if (sub->step >= 0) PetscFunctionReturn(0);
  sub->first += (sub->n - 1)*sub->step;
  sub->step *= -1;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "ISSorted_Stride" 
PetscErrorCode ISSorted_Stride(IS is,PetscTruth* flg)
{
  IS_Stride *sub = (IS_Stride*)is->data;

  PetscFunctionBegin;
  if (sub->step >= 0) *flg = PETSC_TRUE;
  else *flg = PETSC_FALSE;
  PetscFunctionReturn(0);
}

static struct _ISOps myops = { ISGetSize_Stride,
                               ISGetLocalSize_Stride,
                               ISGetIndices_Stride,
                               ISRestoreIndices_Stride,
                               ISInvertPermutation_Stride,
                               ISSort_Stride,
                               ISSorted_Stride,
                               ISDuplicate_Stride,
                               ISDestroy_Stride,
                               ISView_Stride,
                               ISIdentity_Stride };

#undef __FUNCT__  
#define __FUNCT__ "ISCreateStride" 
/*@C
   ISCreateStride - Creates a data structure for an index set 
   containing a list of evenly spaced integers.

   Collective on MPI_Comm

   Input Parameters:
+  comm - the MPI communicator
.  n - the length of the index set
.  first - the first element of the index set
-  step - the change to the next index

   Output Parameter:
.  is - the new index set

   Notes: 
   When the communicator is not MPI_COMM_SELF, the operations on IS are NOT
   conceptually the same as MPI_Group operations. The IS are the 
   distributed sets of indices and thus certain operations on them are collective. 

   Level: beginner

  Concepts: IS^stride
  Concepts: index sets^stride
  Concepts: stride^index set

.seealso: ISCreateGeneral(), ISCreateBlock(), ISAllGather()
@*/
PetscErrorCode ISCreateStride(MPI_Comm comm,int n,int first,int step,IS *is)
{
  PetscErrorCode ierr;
  int        min,max;
  IS         Nindex;
  IS_Stride  *sub;
  PetscTruth flg;

  PetscFunctionBegin;
  PetscValidPointer(is,5);
  *is = PETSC_NULL;
  if (n < 0) SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"Number of indices < 0");
#ifndef PETSC_USE_DYNAMIC_LIBRARIES
  ierr = VecInitializePackage(PETSC_NULL);CHKERRQ(ierr);
#endif

  PetscHeaderCreate(Nindex,_p_IS,struct _ISOps,IS_COOKIE,IS_STRIDE,"IS",comm,ISDestroy,ISView); 
  PetscLogObjectCreate(Nindex);
  PetscLogObjectMemory(Nindex,sizeof(IS_Stride) + sizeof(struct _p_IS));
  ierr           = PetscNew(IS_Stride,&sub);CHKERRQ(ierr);
  sub->n         = n;
  ierr = MPI_Allreduce(&n,&sub->N,1,MPI_INT,MPI_SUM,comm);CHKERRQ(ierr);
  sub->first     = first;
  sub->step      = step;
  if (step > 0) {min = first; max = first + step*(n-1);}
  else          {max = first; min = first + step*(n-1);}

  Nindex->min     = min;
  Nindex->max     = max;
  Nindex->data    = (void*)sub;
  ierr = PetscMemcpy(Nindex->ops,&myops,sizeof(myops));CHKERRQ(ierr);

  if ((!first && step == 1) || (first == max && step == -1 && !min)) {
    Nindex->isperm  = PETSC_TRUE;
  } else {
    Nindex->isperm  = PETSC_FALSE;
  }
  ierr = PetscOptionsHasName(PETSC_NULL,"-is_view",&flg);CHKERRQ(ierr);
  if (flg) {
    ierr = ISView(Nindex,PETSC_VIEWER_STDOUT_(Nindex->comm));CHKERRQ(ierr);
  }
  *is = Nindex; 
  PetscFunctionReturn(0);
}




