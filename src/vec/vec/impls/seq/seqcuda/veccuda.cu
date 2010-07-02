#define PETSCVEC_DLL
/*
   Implements the sequential vectors.
*/

#include "private/vecimpl.h"          /*I "petscvec.h" I*/
#include "../src/vec/vec/impls/dvecimpl.h"
#include "petscblaslapack.h"

/*MC
   VECSEQCUDA - VECSEQCUDA = "seqcuda" - The basic sequential vector, modified to use CUDA

   Options Database Keys:
. -vec_type seqcuda - sets the vector type to VECSEQCUDA during a call to VecSetFromOptions()

  Level: beginner

.seealso: VecCreate(), VecSetType(), VecSetFromOptions(), VecCreateSeqWithArray(), VECMPI, VecType, VecCreateMPI(), VecCreateSeq()
M*/

#define CHKERRCUDA(err) if (err != CUBLAS_STATUS_SUCCESS) SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_LIB,"CUDA error %d",err)

#define VecCUDACastToRawPtr(x) thrust::raw_pointer_cast(&(x)[0])
#define CUSPARRAY cusp::array1d<PetscScalar,cusp::device_memory>

#undef __FUNCT__
#define __FUNCT__ "VecCUDAAllocateCheck"
PETSC_STATIC_INLINE PetscErrorCode VecCUDAAllocateCheck(Vec v)
{
  PetscFunctionBegin;
  if (v->valid_GPU_array == PETSC_CUDA_UNALLOCATED){
    v->spptr= new CUSPARRAY;
    ((CUSPARRAY *)(v->spptr))->resize((PetscBLASInt)v->map->n);
    v->valid_GPU_array = PETSC_CUDA_CPU;
  }
  PetscFunctionReturn(0);
}


#undef __FUNCT__
#define __FUNCT__ "VecCUDACopyToGPU"
/* Copies a vector from the CPU to the GPU unless we already have an up-to-date copy on the GPU */
PETSC_STATIC_INLINE PetscErrorCode VecCUDACopyToGPU(Vec v)
{
  PetscBLASInt   cn = v->map->n;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = VecCUDAAllocateCheck(v);CHKERRQ(ierr);
  if (v->valid_GPU_array == PETSC_CUDA_CPU){
    ierr = PetscLogEventBegin(VEC_CUDACopyToGPU,v,0,0,0);CHKERRQ(ierr);
    ((CUSPARRAY *)(v->spptr))->assign(*(PetscScalar**)v->data,*(PetscScalar**)v->data + cn);
    ierr = PetscLogEventEnd(VEC_CUDACopyToGPU,v,0,0,0);CHKERRQ(ierr);
    v->valid_GPU_array = PETSC_CUDA_BOTH;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "VecCUDAAllocateCheck_Public"
PetscErrorCode VecCUDAAllocateCheck_Public(Vec v)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = VecCUDAAllocateCheck(v);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "VecCUDACopyToGPU_Public"
PetscErrorCode VecCUDACopyToGPU_Public(Vec v)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = VecCUDACopyToGPU(v);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "VecCUDACopyFromGPU"
/* Copies a vector from the GPU to the CPU unless we already have an up-to-date copy on the CPU */
PetscErrorCode VecCUDACopyFromGPU(Vec v)
{
  PetscErrorCode ierr;
  CUSPARRAY      *GPUvector = (CUSPARRAY *)(v->spptr);

  PetscFunctionBegin;
  if (v->valid_GPU_array == PETSC_CUDA_GPU){
    ierr = PetscLogEventBegin(VEC_CUDACopyFromGPU,v,0,0,0);CHKERRQ(ierr);
    thrust::copy(GPUvector->begin(),GPUvector->end(),*(PetscScalar**)v->data);
    ierr = PetscLogEventEnd(VEC_CUDACopyFromGPU,v,0,0,0);CHKERRQ(ierr);
    v->valid_GPU_array = PETSC_CUDA_BOTH;
  }
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "VecSet_SeqCUDA"
PetscErrorCode VecSet_SeqCUDA(Vec xin,PetscScalar alpha)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  /* if there's a faster way to do the case alpha=0.0 on the GPU we should do that*/
  ierr = VecCUDAAllocateCheck(xin);CHKERRQ(ierr);
  cusp::blas::fill(*(CUSPARRAY *)(xin->spptr),alpha);
  xin->valid_GPU_array = PETSC_CUDA_GPU;
  PetscFunctionReturn(0);
}

#undef __FUNCT__ 
#define __FUNCT__ "VecScale_SeqCUDA"
PetscErrorCode VecScale_SeqCUDA(Vec xin, PetscScalar alpha)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (alpha == 0.0) {
    ierr = VecSet_SeqCUDA(xin,alpha);CHKERRQ(ierr);
  } else if (alpha != 1.0) {
    ierr = VecCUDACopyToGPU(xin);CHKERRQ(ierr);
    cusp::blas::scal(*(CUSPARRAY *)(xin->spptr),alpha);
    xin->valid_GPU_array = PETSC_CUDA_GPU;
  }
  ierr = PetscLogFlops(xin->map->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
#undef __FUNCT__
#define __FUNCT__ "VecDot_SeqCUDA"
PetscErrorCode VecDot_SeqCUDA(Vec xin,Vec yin,PetscScalar *z)
{
#if defined(PETSC_USE_COMPLEX)
  PetscScalar    *ya,*xa;
#endif
  PetscErrorCode ierr;

  PetscFunctionBegin;
#if defined(PETSC_USE_COMPLEX)
  /* cannot use BLAS dot for complex because compiler/linker is 
     not happy about returning a double complex */
  {
    ierr = VecGetArrayPrivate2(xin,&xa,yin,&ya);CHKERRQ(ierr);

    PetscInt    i;
    PetscScalar sum = 0.0;
    for (i=0; i<xin->map->n; i++) {
      sum += xa[i]*PetscConj(ya[i]);
    }
    *z = sum;
    ierr = VecRestoreArrayPrivate2(xin,&xa,yin,&ya);CHKERRQ(ierr);
  }
#else
  {
    ierr = VecCUDACopyToGPU(xin);CHKERRQ(ierr);
    ierr = VecCUDACopyToGPU(yin);CHKERRQ(ierr);
    *z = cusp::blas::dot(*(CUSPARRAY *)(xin->spptr),*(CUSPARRAY *)(yin->spptr));
  }
#endif
  if (xin->map->n >0) {
    ierr = PetscLogFlops(2.0*xin->map->n-1);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "VecTDot_SeqCUDA"
PetscErrorCode VecTDot_SeqCUDA(Vec xin,Vec yin,PetscScalar *z)
{
#if defined(PETSC_USE_COMPLEX)
  PetscScalar    *ya,*xa;
#endif
  PetscErrorCode ierr;

  PetscFunctionBegin;
#if defined(PETSC_USE_COMPLEX)
  /* cannot use BLAS dot for complex because compiler/linker is 
     not happy about returning a double complex */
 ierr = VecGetArrayPrivate2(xin,&xa,yin,&ya);CHKERRQ(ierr);
 {
   PetscInt    i;
   PetscScalar sum = 0.0;
   for (i=0; i<xin->map->n; i++) {
     sum += xa[i]*ya[i];
   }
   *z = sum;
   ierr = VecRestoreArrayPrivate2(xin,&xa,yin,&ya);CHKERRQ(ierr);
 }
#else
 ierr = VecCUDACopyToGPU(xin);CHKERRQ(ierr);
 ierr = VecCUDACopyToGPU(yin);CHKERRQ(ierr);
 *z = cusp::blas::dot(*(CUSPARRAY *)(xin->spptr),*(CUSPARRAY *)(yin->spptr));
#endif
  if (xin->map->n > 0) {
    ierr = PetscLogFlops(2.0*xin->map->n-1);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}
#undef __FUNCT__  
#define __FUNCT__ "VecCopy_SeqCUDA"
PetscErrorCode VecCopy_SeqCUDA(Vec xin,Vec yin)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (xin != yin) {
    ierr = VecCUDACopyToGPU(xin);CHKERRQ(ierr);
    ierr = VecCUDAAllocateCheck(yin);CHKERRQ(ierr);
    cusp::blas::copy(*(CUSPARRAY *)(xin->spptr),*(CUSPARRAY *)(yin->spptr));
    yin->valid_GPU_array = PETSC_CUDA_GPU;
  }
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "VecSwap_SeqCUDA"
PetscErrorCode VecSwap_SeqCUDA(Vec xin,Vec yin)
{
  PetscErrorCode ierr;
  PetscBLASInt   one = 1,bn = PetscBLASIntCast(xin->map->n);

  PetscFunctionBegin;
  if (xin != yin) {
    ierr = VecCUDACopyToGPU(xin);CHKERRQ(ierr);
    ierr = VecCUDACopyToGPU(yin);CHKERRQ(ierr);
    cublasSswap(bn,VecCUDACastToRawPtr(*(CUSPARRAY *)(xin->spptr)),one,VecCUDACastToRawPtr(*(CUSPARRAY *)(yin->spptr)),one);
    ierr = cublasGetError();CHKERRCUDA(ierr);
    xin->valid_GPU_array = PETSC_CUDA_GPU;
    yin->valid_GPU_array = PETSC_CUDA_GPU;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecAXPY_SeqCUDA"
PetscErrorCode VecAXPY_SeqCUDA(Vec yin,PetscScalar alpha,Vec xin)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  /* assume that the BLAS handles alpha == 1.0 efficiently since we have no fast code for it */
  if (alpha != 0.0) {
    ierr = VecCUDACopyToGPU(xin);CHKERRQ(ierr);
    ierr = VecCUDACopyToGPU(yin);CHKERRQ(ierr);
    cusp::blas::axpy(*(CUSPARRAY *)(xin->spptr),*(CUSPARRAY *)(yin->spptr),alpha);
    yin->valid_GPU_array = PETSC_CUDA_GPU;
    ierr = PetscLogFlops(2.0*yin->map->n);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecAXPBY_SeqCUDA"
PetscErrorCode VecAXPBY_SeqCUDA(Vec yin,PetscScalar alpha,PetscScalar beta,Vec xin)
{
  PetscErrorCode    ierr;
  PetscInt          n = yin->map->n,i;
  const PetscScalar *xx;
  PetscScalar       *yy,a = alpha,b = beta;
 
  PetscFunctionBegin;
  if (a == 0.0) {
    ierr = VecScale_SeqCUDA(yin,beta);CHKERRQ(ierr);
  } else if (b == 1.0) {
    ierr = VecAXPY_SeqCUDA(yin,alpha,xin);CHKERRQ(ierr);
  } else if (a == 1.0) {
    ierr = VecAYPX_Seq(yin,beta,xin);CHKERRQ(ierr);
  } else if (b == 0.0) {
    ierr = VecGetArrayPrivate2(xin,(PetscScalar**)&xx,yin,&yy);CHKERRQ(ierr);
    for (i=0; i<n; i++) {
      yy[i] = a*xx[i];
    }
    ierr = VecRestoreArrayPrivate2(xin,(PetscScalar**)&xx,yin,&yy);CHKERRQ(ierr);
    ierr = PetscLogFlops(xin->map->n);CHKERRQ(ierr);
  } else {
    ierr = VecCUDACopyToGPU(xin);CHKERRQ(ierr);
    ierr = VecCUDACopyToGPU(yin);CHKERRQ(ierr);
    cusp::blas::axpby(*(CUSPARRAY *)(xin->spptr),*(CUSPARRAY *)(yin->spptr),*(CUSPARRAY *)(yin->spptr),a,b);
    yin->valid_GPU_array = PETSC_CUDA_GPU;
    ierr = PetscLogFlops(3.0*xin->map->n);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecAXPBYPCZ_SeqCUDA"
PetscErrorCode VecAXPBYPCZ_SeqCUDA(Vec zin,PetscScalar alpha,PetscScalar beta,PetscScalar gamma,Vec xin,Vec yin)
{
  PetscErrorCode     ierr;
  PetscInt           n = zin->map->n,i;
  const PetscScalar  *yy,*xx;
  PetscScalar        *zz;

  PetscFunctionBegin;
  if (alpha == 1.0) {
    ierr = VecGetArrayPrivate3(xin,(PetscScalar**)&xx,yin,(PetscScalar**)&yy,zin,&zz);CHKERRQ(ierr);
    for (i=0; i<n; i++) {
      zz[i] = xx[i] + beta*yy[i] + gamma*zz[i];
    }
    ierr = PetscLogFlops(4.0*n);CHKERRQ(ierr);
    ierr = VecRestoreArrayPrivate3(xin,(PetscScalar**)&xx,yin,(PetscScalar**)&yy,zin,&zz);CHKERRQ(ierr);
  } else if (gamma == 1.0) {
    ierr = VecGetArrayPrivate3(xin,(PetscScalar**)&xx,yin,(PetscScalar**)&yy,zin,&zz);CHKERRQ(ierr);
    for (i=0; i<n; i++) {
      zz[i] = alpha*xx[i] + beta*yy[i] + zz[i];
    }
    ierr = PetscLogFlops(4.0*n);CHKERRQ(ierr);
    ierr = VecRestoreArrayPrivate3(xin,(PetscScalar**)&xx,yin,(PetscScalar**)&yy,zin,&zz);CHKERRQ(ierr);
  } else {
    ierr = VecCUDACopyToGPU(xin);
    ierr = VecCUDACopyToGPU(yin);
    ierr = VecCUDACopyToGPU(zin);
    cusp::blas::axpbypcz(*(CUSPARRAY *)(xin->spptr),*(CUSPARRAY *)(yin->spptr),*(CUSPARRAY *)(zin->spptr),*(CUSPARRAY *)(zin->spptr),alpha,beta,gamma);
    zin->valid_GPU_array = PETSC_CUDA_GPU;
    ierr = PetscLogFlops(5.0*n);CHKERRQ(ierr);    
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecPointwiseMult_SeqCUDA"
PetscErrorCode VecPointwiseMult_SeqCUDA(Vec win,Vec xin,Vec yin)
{
  PetscErrorCode ierr;
  PetscInt       n = win->map->n;

  PetscFunctionBegin;
  ierr = VecCUDACopyToGPU(xin);CHKERRQ(ierr);
  ierr = VecCUDACopyToGPU(yin);CHKERRQ(ierr);
  ierr = VecCUDAAllocateCheck(win);CHKERRQ(ierr);
  cusp::blas::xmy(*(CUSPARRAY *)(xin->spptr),*(CUSPARRAY *)(yin->spptr),*(CUSPARRAY *)(win->spptr));
  win->valid_GPU_array = PETSC_CUDA_GPU;
  ierr = PetscLogFlops(n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecView_SeqCUDA"
PetscErrorCode VecView_SeqCUDA(Vec xin,PetscViewer viewer)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = VecCUDACopyFromGPU(xin);CHKERRQ(ierr);
  ierr = VecView_Seq(xin,viewer);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "VecNorm_SeqCUDA"
PetscErrorCode VecNorm_SeqCUDA(Vec xin,NormType type,PetscReal* z)
{
  PetscScalar    *xx;
  PetscErrorCode ierr;
  PetscInt       n = xin->map->n;
  PetscBLASInt   one = 1, bn = PetscBLASIntCast(n);

  PetscFunctionBegin;
  if (type == NORM_2 || type == NORM_FROBENIUS) {
    ierr = VecCUDACopyToGPU(xin);CHKERRQ(ierr);
    *z = cusp::blas::nrm2(*(CUSPARRAY *)(xin->spptr));
    ierr = PetscLogFlops(PetscMax(2.0*n-1,0.0));CHKERRQ(ierr);
  } else if (type == NORM_INFINITY) {
    PetscInt     i;
    PetscReal    max = 0.0,tmp;

    ierr = VecGetArrayPrivate(xin,&xx);CHKERRQ(ierr);
    for (i=0; i<n; i++) {
      if ((tmp = PetscAbsScalar(*xx)) > max) max = tmp;
      /* check special case of tmp == NaN */
      if (tmp != tmp) {max = tmp; break;}
      xx++;
    }
    ierr = VecRestoreArrayPrivate(xin,&xx);CHKERRQ(ierr);
    *z   = max;
  } else if (type == NORM_1) {
    ierr = VecCUDACopyToGPU(xin);CHKERRQ(ierr);
    *z = cublasSasum(bn,VecCUDACastToRawPtr(*(CUSPARRAY *)(xin->spptr)),one);
    ierr = cublasGetError();CHKERRCUDA(ierr);
    ierr = PetscLogFlops(PetscMax(n-1.0,0.0));CHKERRQ(ierr);
  } else if (type == NORM_1_AND_2) {
    ierr = VecNorm_SeqCUDA(xin,NORM_1,z);CHKERRQ(ierr);
    ierr = VecNorm_SeqCUDA(xin,NORM_2,z+1);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}



#undef __FUNCT__  
#define __FUNCT__ "VecSetRandom_SeqCUDA"
PetscErrorCode VecSetRandom_SeqCUDA(Vec xin,PetscRandom r)
{
  PetscErrorCode ierr;
  PetscFunctionBegin;
  if (xin->valid_GPU_array != PETSC_CUDA_UNALLOCATED){
    xin->valid_GPU_array = PETSC_CUDA_CPU;
  }
  ierr = VecSetRandom_Seq(xin,r);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecResetArray_SeqCUDA"
PetscErrorCode VecResetArray_SeqCUDA(Vec vin)
{
  PetscErrorCode ierr;
  PetscFunctionBegin;
  ierr = VecResetArray_Seq(vin);CHKERRQ(ierr);
  if (vin->valid_GPU_array != PETSC_CUDA_UNALLOCATED){
    vin->valid_GPU_array = PETSC_CUDA_CPU;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecPlaceArray_SeqCUDA"
PetscErrorCode VecPlaceArray_SeqCUDA(Vec vin,const PetscScalar *a)
{
  PetscErrorCode ierr;
  PetscFunctionBegin;
  ierr = VecPlaceArray_Seq(vin,a);CHKERRQ(ierr);
  if (vin->valid_GPU_array != PETSC_CUDA_UNALLOCATED){
    vin->valid_GPU_array = PETSC_CUDA_CPU;
  }
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "VecReplaceArray_SeqCUDA"
PetscErrorCode VecReplaceArray_SeqCUDA(Vec vin,const PetscScalar *a)
{
  PetscErrorCode ierr;
  PetscFunctionBegin;
  ierr = VecReplaceArray_Seq(vin,a);CHKERRQ(ierr);
  if (vin->valid_GPU_array != PETSC_CUDA_UNALLOCATED){
    vin->valid_GPU_array = PETSC_CUDA_CPU;
  }
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "VecCreateSeqCUDA"
/*@
   VecCreateSeqCUDA - Creates a standard, sequential array-style vector.

   Collective on MPI_Comm

   Input Parameter:
+  comm - the communicator, should be PETSC_COMM_SELF
-  n - the vector length 

   Output Parameter:
.  V - the vector

   Notes:
   Use VecDuplicate() or VecDuplicateVecs() to form additional vectors of the
   same type as an existing vector.

   Level: intermediate

   Concepts: vectors^creating sequential

.seealso: VecCreateMPI(), VecCreate(), VecDuplicate(), VecDuplicateVecs(), VecCreateGhost()
@*/
PetscErrorCode PETSCVEC_DLLEXPORT VecCreateSeqCUDA(MPI_Comm comm,PetscInt n,Vec *v)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = VecCreate(comm,v);CHKERRQ(ierr);
  ierr = VecSetSizes(*v,n,n);CHKERRQ(ierr);
  ierr = VecSetType(*v,VECSEQCUDA);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "VecDuplicate_SeqCUDA"
PetscErrorCode VecDuplicate_SeqCUDA(Vec win,Vec *V)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = VecCreateSeqCUDA(((PetscObject)win)->comm,win->map->n,V);CHKERRQ(ierr);
  if (win->mapping) {
    ierr = PetscObjectReference((PetscObject)win->mapping);CHKERRQ(ierr);
    (*V)->mapping = win->mapping;
  }
  if (win->bmapping) {
    ierr = PetscObjectReference((PetscObject)win->bmapping);CHKERRQ(ierr);
    (*V)->bmapping = win->bmapping;
  }
  (*V)->map->bs = win->map->bs;
  ierr = PetscOListDuplicate(((PetscObject)win)->olist,&((PetscObject)(*V))->olist);CHKERRQ(ierr);
  ierr = PetscFListDuplicate(((PetscObject)win)->qlist,&((PetscObject)(*V))->qlist);CHKERRQ(ierr);

  (*V)->stash.ignorenegidx = win->stash.ignorenegidx;

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecDestroy_SeqCUDA"
PetscErrorCode VecDestroy_SeqCUDA(Vec v)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  delete (CUSPARRAY *)(v->spptr);
  ierr = VecDestroy_Seq(v);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "VecCreate_SeqCUDA"
PetscErrorCode PETSCVEC_DLLEXPORT VecCreate_SeqCUDA(Vec V)
{
  PetscErrorCode ierr;
 
  PetscFunctionBegin;
  ierr = VecCreate_Seq(V);CHKERRQ(ierr);
  ierr = PetscObjectChangeTypeName((PetscObject)V,VECSEQCUDA);CHKERRQ(ierr);
  V->ops->duplicate     = VecDuplicate_SeqCUDA;
  V->ops->dot           = VecDot_SeqCUDA;
  V->ops->norm          = VecNorm_SeqCUDA;
  V->ops->tdot          = VecTDot_SeqCUDA;
  V->ops->scale         = VecScale_SeqCUDA;
  V->ops->copy          = VecCopy_SeqCUDA;
  V->ops->set           = VecSet_SeqCUDA;
  V->ops->swap          = VecSwap_SeqCUDA;
  V->ops->axpy          = VecAXPY_SeqCUDA;
  V->ops->axpby         = VecAXPBY_SeqCUDA;
  V->ops->axpbypcz      = VecAXPBYPCZ_SeqCUDA;
  V->ops->pointwisemult = VecPointwiseMult_SeqCUDA;
  V->ops->setrandom     = VecSetRandom_SeqCUDA;
  V->ops->view          = VecView_SeqCUDA;
  V->ops->placearray    = VecPlaceArray_SeqCUDA;
  V->ops->replacearray  = VecReplaceArray_SeqCUDA;
  V->ops->dot_local     = VecDot_SeqCUDA;
  V->ops->tdot_local    = VecTDot_SeqCUDA;
  V->ops->norm_local    = VecNorm_SeqCUDA;
  V->ops->resetarray    = VecResetArray_SeqCUDA;
  V->ops->destroy       = VecDestroy_SeqCUDA;
  PetscFunctionReturn(0);
}
EXTERN_C_END
