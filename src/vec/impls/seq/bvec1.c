
/*
   Defines the BLAS based vector operations. Code shared by parallel
  and sequential vectors.
*/

#include "vecimpl.h" 
#include "src/vec/impls/dvecimpl.h" 
#include "petscblaslapack.h"

#undef __FUNCT__  
#define __FUNCT__ "VecDot_Seq"
PetscErrorCode VecDot_Seq(Vec xin,Vec yin,PetscScalar *z)
{
  PetscScalar *ya,*xa;
  PetscErrorCode ierr;
#if !defined(PETSC_USE_COMPLEX)
  PetscBLASInt bn = (PetscBLASInt)xin->n, one = 1;
#endif

  PetscFunctionBegin;
  ierr = VecGetArray(xin,&xa);CHKERRQ(ierr);
  if (xin != yin) {ierr = VecGetArray(yin,&ya);CHKERRQ(ierr);}
  else ya = xa;
#if defined(PETSC_USE_COMPLEX)
  /* cannot use BLAS dot for complex because compiler/linker is 
     not happy about returning a double complex */
  {
    int         i;
    PetscScalar sum = 0.0;
    for (i=0; i<xin->n; i++) {
      sum += xa[i]*PetscConj(ya[i]);
    }
    *z = sum;
  }
#else
  *z = BLASdot_(&bn,xa,&one,ya,&one);
#endif
  ierr = VecRestoreArray(xin,&xa);CHKERRQ(ierr);
  if (xin != yin) {ierr = VecRestoreArray(yin,&ya);CHKERRQ(ierr);}
  PetscLogFlops(2*xin->n-1);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecTDot_Seq"
PetscErrorCode VecTDot_Seq(Vec xin,Vec yin,PetscScalar *z)
{
  PetscScalar *ya,*xa;
  PetscErrorCode ierr;
#if !defined(PETSC_USE_COMPLEX)
 PetscBLASInt bn = (PetscBLASInt)xin->n, one = 1;
#endif

  PetscFunctionBegin;
  ierr = VecGetArray(xin,&xa);CHKERRQ(ierr);
  if (xin != yin) {ierr = VecGetArray(yin,&ya);CHKERRQ(ierr);}
  else ya = xa;
#if defined(PETSC_USE_COMPLEX)
  /* cannot use BLAS dot for complex because compiler/linker is 
     not happy about returning a double complex */
  int         i;
  PetscScalar sum = 0.0;
  for (i=0; i<xin->n; i++) {
    sum += xa[i]*ya[i];
  }
  *z = sum;
#else
  *z = BLASdot_(&bn,xa,&one,ya,&one);
#endif
  ierr = VecRestoreArray(xin,&xa);CHKERRQ(ierr);
  if (xin != yin) {ierr = VecRestoreArray(yin,&ya);CHKERRQ(ierr);}
  PetscLogFlops(2*xin->n-1);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecScale_Seq"
PetscErrorCode VecScale_Seq(const PetscScalar *alpha,Vec xin)
{
  Vec_Seq      *x = (Vec_Seq*)xin->data;
  PetscBLASInt bn = (PetscBLASInt)xin->n, one = 1;

  PetscFunctionBegin;
  BLASscal_(&bn,(PetscScalar *)alpha,x->array,&one);
  PetscLogFlops(xin->n);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecCopy_Seq"
PetscErrorCode VecCopy_Seq(Vec xin,Vec yin)
{
  Vec_Seq     *x = (Vec_Seq *)xin->data;
  PetscScalar *ya;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (xin != yin) {
    ierr = VecGetArray(yin,&ya);CHKERRQ(ierr);
    ierr = PetscMemcpy(ya,x->array,xin->n*sizeof(PetscScalar));CHKERRQ(ierr);
    ierr = VecRestoreArray(yin,&ya);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecSwap_Seq"
PetscErrorCode VecSwap_Seq(Vec xin,Vec yin)
{
  Vec_Seq        *x = (Vec_Seq *)xin->data;
  PetscScalar    *ya;
  PetscErrorCode ierr;
  PetscBLASInt   bn = (PetscBLASInt)xin->n, one = 1;

  PetscFunctionBegin;
  if (xin != yin) {
    ierr = VecGetArray(yin,&ya);CHKERRQ(ierr);
    BLASswap_(&bn,x->array,&one,ya,&one);
    ierr = VecRestoreArray(yin,&ya);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecAXPY_Seq"
PetscErrorCode VecAXPY_Seq(const PetscScalar *alpha,Vec xin,Vec yin)
{
  Vec_Seq        *x = (Vec_Seq *)xin->data;
  PetscErrorCode ierr;
  PetscBLASInt   bn = (PetscBLASInt)xin->n, one = 1;
  PetscScalar    *yarray;

  PetscFunctionBegin;
  ierr = VecGetArray(yin,&yarray);CHKERRQ(ierr);
  BLASaxpy_(&bn,(PetscScalar *)alpha,x->array,&one,yarray,&one);
  ierr = VecRestoreArray(yin,&yarray);CHKERRQ(ierr);
  PetscLogFlops(2*xin->n);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecAXPBY_Seq"
PetscErrorCode VecAXPBY_Seq(const PetscScalar *alpha,const PetscScalar *beta,Vec xin,Vec yin)
{
  Vec_Seq        *x = (Vec_Seq *)xin->data;
  PetscErrorCode ierr;
  int            n = xin->n,i;
  PetscScalar    *xx = x->array,*yy ,a = *alpha,b = *beta;

  PetscFunctionBegin;
  ierr = VecGetArray(yin,&yy);CHKERRQ(ierr);
  for (i=0; i<n; i++) {
    yy[i] = a*xx[i] + b*yy[i];
  }
  ierr = VecRestoreArray(yin,&yy);CHKERRQ(ierr);

  PetscLogFlops(3*xin->n);
  PetscFunctionReturn(0);
}

