/*$Id: sorti.c,v 1.30 2001/08/06 21:14:16 bsmith Exp $*/
/*
   This file contains routines for sorting integers. Values are sorted in place.


   The word "register"  in this code is used to identify data that is not
   aliased.  For some compilers, marking variables as register can improve 
   the compiler optimizations.
 */
#include "petscconfig.h"
#include "petsc.h"                /*I  "petsc.h"  I*/
#include "petscsys.h"             /*I  "petscsys.h"    I*/

#define SWAP(a,b,t) {t=a;a=b;b=t;}

/* -----------------------------------------------------------------------*/

#undef __FUNCT__  
#define __FUNCT__ "PetscSortInt_Private"
/* 
   A simple version of quicksort; taken from Kernighan and Ritchie, page 87.
   Assumes 0 origin for v, number of elements = right+1 (right is index of
   right-most member). 
*/
static int PetscSortInt_Private(int *v,int right)
{
  int i,vl,last,tmp,ierr;

  PetscFunctionBegin;
  if (right <= 1) {
    if (right == 1) {
      if (v[0] > v[1]) SWAP(v[0],v[1],tmp);
    }
    PetscFunctionReturn(0);
  }
  SWAP(v[0],v[right/2],tmp);
  vl   = v[0];
  last = 0;
  for (i=1; i<=right; i++) {
    if (v[i] < vl) {last++; SWAP(v[last],v[i],tmp);}
  }
  SWAP(v[0],v[last],tmp);
  ierr = PetscSortInt_Private(v,last-1);CHKERRQ(ierr);
  ierr = PetscSortInt_Private(v+last+1,right-(last+1));CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscSortInt" 
/*@
   PetscSortInt - Sorts an array of integers in place in increasing order.

   Not Collective

   Input Parameters:
+  n  - number of values
-  i  - array of integers

   Level: intermediate

   Concepts: sorting^ints

.seealso: PetscSortReal(), PetscSortIntWithPermutation()
@*/
int PetscSortInt(int n,int i[])
{
  int ierr,j,k,tmp,ik;

  PetscFunctionBegin;
  if (n<8) {
    for (k=0; k<n; k++) {
      ik = i[k];
      for (j=k+1; j<n; j++) {
	if (ik > i[j]) {
	  SWAP(i[k],i[j],tmp);
	  ik = i[k];
	}
      }
    }
  } else {
    ierr = PetscSortInt_Private(i,n-1);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#define SWAP2(a,b,c,d,t) {t=a;a=b;b=t;t=c;c=d;d=t;}

/* -----------------------------------------------------------------------*/

#undef __FUNCT__  
#define __FUNCT__ "PetscSortIntWithArray_Private"
/* 
   A simple version of quicksort; taken from Kernighan and Ritchie, page 87.
   Assumes 0 origin for v, number of elements = right+1 (right is index of
   right-most member). 
*/
static int PetscSortIntWithArray_Private(int *v,int *V,int right)
{
  int i,vl,last,tmp,ierr;

  PetscFunctionBegin;
  if (right <= 1) {
    if (right == 1) {
      if (v[0] > v[1]) SWAP2(v[0],v[1],V[0],V[1],tmp);
    }
    PetscFunctionReturn(0);
  }
  SWAP2(v[0],v[right/2],V[0],V[right/2],tmp);
  vl   = v[0];
  last = 0;
  for (i=1; i<=right; i++) {
    if (v[i] < vl) {last++; SWAP2(v[last],v[i],V[last],V[i],tmp);}
  }
  SWAP2(v[0],v[last],V[0],V[last],tmp);
  ierr = PetscSortIntWithArray_Private(v,V,last-1);CHKERRQ(ierr);
  ierr = PetscSortIntWithArray_Private(v+last+1,V+last+1,right-(last+1));CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscSortIntWithArray" 
/*@
   PetscSortIntWithArray - Sorts an array of integers in place in increasing order;
       changes a second array to match the sorted first array.

   Not Collective

   Input Parameters:
+  n  - number of values
.  i  - array of integers
-  I - second array if integers

   Level: intermediate

   Concepts: sorting^ints with array

.seealso: PetscSortReal(), PetscSortIntPermutation(), PetscSortInt()
@*/
int PetscSortIntWithArray(int n,int i[],int I[])
{
  int ierr,j,k,tmp,ik;

  PetscFunctionBegin;
  if (n<8) {
    for (k=0; k<n; k++) {
      ik = i[k];
      for (j=k+1; j<n; j++) {
	if (ik > i[j]) {
	  SWAP2(i[k],i[j],I[k],I[j],tmp);
	  ik = i[k];
	}
      }
    }
  } else {
    ierr = PetscSortIntWithArray_Private(i,I,n-1);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}



