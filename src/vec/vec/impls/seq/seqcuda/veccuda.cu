#define PETSCVEC_DLL
/*
   Implements the sequential vectors.
*/

#include "petscconf.h"
PETSC_CUDA_EXTERN_C_BEGIN
#include "private/vecimpl.h"          /*I "petscvec.h" I*/
#include "../src/vec/vec/impls/dvecimpl.h"
PETSC_CUDA_EXTERN_C_END
#include "../src/vec/vec/impls/seq/seqcuda/cudavecimpl.h"
/* these following 2 public versions are necessary because we use CUSP in the regular version and these need to be called from plain C code. */
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
  PetscScalar    *array;
  Vec_Seq        *s;
  PetscInt       n = v->map->n;

  PetscFunctionBegin;
  s = (Vec_Seq*)v->data;
  if (s->array == 0){
    ierr               = PetscMalloc(n*sizeof(PetscScalar),&array);CHKERRQ(ierr);
    ierr               = PetscLogObjectMemory(v,n*sizeof(PetscScalar));CHKERRQ(ierr);
    s->array           = array;
    s->array_allocated = array;
  }
  if (v->valid_GPU_array == PETSC_CUDA_GPU){
    ierr = PetscLogEventBegin(VEC_CUDACopyFromGPU,v,0,0,0);CHKERRQ(ierr);
    thrust::copy(GPUvector->begin(),GPUvector->end(),*(PetscScalar**)v->data);
    ierr = PetscLogEventEnd(VEC_CUDACopyFromGPU,v,0,0,0);CHKERRQ(ierr);
    v->valid_GPU_array = PETSC_CUDA_BOTH;
  }
  PetscFunctionReturn(0);
}





/*MC
   VECSEQCUDA - VECSEQCUDA = "seqcuda" - The basic sequential vector, modified to use CUDA

   Options Database Keys:
. -vec_type seqcuda - sets the vector type to VECSEQCUDA during a call to VecSetFromOptions()

  Level: beginner

.seealso: VecCreate(), VecSetType(), VecSetFromOptions(), VecCreateSeqWithArray(), VECMPI, VecType, VecCreateMPI(), VecCreateSeq()
M*/

/* for VecAYPX_SeqCUDA*/
namespace cusp
{
namespace blas
{
namespace detail
{
  template <typename T>
    struct AYPX : public thrust::binary_function<T,T,T>
    {
      T alpha;
      
      AYPX(T _alpha) : alpha(_alpha) {}

      __host__ __device__
	T operator()(T x, T y)
      {
	return alpha * y + x;
      }
    };
}

 template <typename ForwardIterator1,
           typename ForwardIterator2,
           typename ScalarType>
void aypx(ForwardIterator1 first1,ForwardIterator1 last1,ForwardIterator2 first2,ScalarType alpha)
	   {
	     thrust::transform(first1,last1,first2,first2,detail::AYPX<ScalarType>(alpha));
	   }
 template <typename Array1, typename Array2, typename ScalarType>
   void aypx(const Array1& x, Array2& y, ScalarType alpha)
 {
   detail::assert_same_dimensions(x,y);
   aypx(x.begin(),x.end(),y.begin(),alpha);
 }
}
}

#undef __FUNCT__
#define __FUNCT__ "VecAYPX_SeqCUDA"
PetscErrorCode VecAYPX_SeqCUDA(Vec yin, PetscScalar alpha, Vec xin)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (alpha != 0.0) {
    ierr = VecCUDACopyToGPU(xin);CHKERRQ(ierr);
    ierr = VecCUDACopyToGPU(yin);CHKERRQ(ierr);
    cusp::blas::aypx(*(CUSPARRAY *)(xin->spptr),*(CUSPARRAY *)(yin->spptr),alpha);
    yin->valid_GPU_array = PETSC_CUDA_GPU;
    ierr = PetscLogFlops(2.0*yin->map->n);CHKERRQ(ierr);
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

struct VecCUDAPointwiseDivide
{
  template <typename Tuple>
  __host__ __device__
  void operator()(Tuple t)
  {
    thrust::get<0>(t) = thrust::get<1>(t) / thrust::get<2>(t);
  }
};

#undef __FUNCT__
#define __FUNCT__ "VecPointwiseDivide_SeqCUDA"
PetscErrorCode VecPointwiseDivide_SeqCUDA(Vec win, Vec xin, Vec yin)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = VecCUDAAllocateCheck(win);CHKERRQ(ierr);
  ierr = VecCUDACopyToGPU(xin);CHKERRQ(ierr);
  ierr = VecCUDACopyToGPU(yin);CHKERRQ(ierr);
  thrust::for_each(
	thrust::make_zip_iterator(
	    thrust::make_tuple(
		((CUSPARRAY*)win->spptr)->begin(),
		((CUSPARRAY*)xin->spptr)->begin(),
		((CUSPARRAY*)yin->spptr)->begin())),
	thrust::make_zip_iterator(
	    thrust::make_tuple(
		((CUSPARRAY*)win->spptr)->end(),  
		((CUSPARRAY*)xin->spptr)->end(),
		((CUSPARRAY*)yin->spptr)->end())),
	VecCUDAPointwiseDivide());
  ierr = PetscLogFlops(win->map->n);CHKERRQ(ierr);
  win->valid_GPU_array = PETSC_CUDA_GPU;
  PetscFunctionReturn(0);
}


struct VecCUDAWAXPY
{
  template <typename Tuple>
  __host__ __device__
  void operator()(Tuple t)
  {
    thrust::get<0>(t) = thrust::get<1>(t) + thrust::get<2>(t)*thrust::get<3>(t);
  }
};

struct VecCUDASum
{
  template <typename Tuple>
  __host__ __device__
  void operator()(Tuple t)
  {
    thrust::get<0>(t) = thrust::get<1>(t) + thrust::get<2>(t);
  }
};

struct VecCUDADiff
{
  template <typename Tuple>
  __host__ __device__
  void operator()(Tuple t)
  {
    thrust::get<0>(t) = thrust::get<1>(t) - thrust::get<2>(t);
  }
};

#undef __FUNCT__
#define __FUNCT__ "VecWAXPY_SeqCUDA"
PetscErrorCode VecWAXPY_SeqCUDA(Vec win,PetscScalar alpha,Vec xin, Vec yin)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = VecCUDACopyToGPU(xin);CHKERRQ(ierr);
  ierr = VecCUDACopyToGPU(yin);CHKERRQ(ierr);
  ierr = VecCUDAAllocateCheck(win);CHKERRQ(ierr);
  if (alpha == 1.0) {
    thrust::for_each(
	thrust::make_zip_iterator(
	    thrust::make_tuple(
		((CUSPARRAY*)win->spptr)->begin(),
		((CUSPARRAY*)yin->spptr)->begin(),
		((CUSPARRAY*)xin->spptr)->begin())),
	thrust::make_zip_iterator(
	    thrust::make_tuple(
		((CUSPARRAY*)win->spptr)->end(),  
		((CUSPARRAY*)yin->spptr)->end(),
		((CUSPARRAY*)xin->spptr)->end())),
	VecCUDASum());
    ierr = PetscLogFlops(win->map->n);CHKERRQ(ierr);
  } else if (alpha == -1.0) {
     thrust::for_each(
	thrust::make_zip_iterator(
	    thrust::make_tuple(
		((CUSPARRAY*)win->spptr)->begin(),
		((CUSPARRAY*)yin->spptr)->begin(),
		((CUSPARRAY*)xin->spptr)->begin())),
	thrust::make_zip_iterator(
	    thrust::make_tuple(
		((CUSPARRAY*)win->spptr)->end(),  
		((CUSPARRAY*)yin->spptr)->end(),
		((CUSPARRAY*)xin->spptr)->end())),
	VecCUDADiff());
     ierr = PetscLogFlops(win->map->n);CHKERRQ(ierr);
  } else if (alpha == 0.0) {
    ierr = VecCopy_SeqCUDA(yin,win);CHKERRQ(ierr);
  } else {
     thrust::for_each(
	thrust::make_zip_iterator(
	    thrust::make_tuple(
		((CUSPARRAY*)win->spptr)->begin(),
		((CUSPARRAY*)yin->spptr)->begin(),
		thrust::make_constant_iterator(alpha,0),
		((CUSPARRAY*)xin->spptr)->begin())),
	thrust::make_zip_iterator(
	    thrust::make_tuple(
		((CUSPARRAY*)win->spptr)->end(),  
		((CUSPARRAY*)yin->spptr)->end(),
		thrust::make_constant_iterator(alpha,win->map->n),
		((CUSPARRAY*)xin->spptr)->end())),
	VecCUDAWAXPY());
     ierr = PetscLogFlops(2*win->map->n);CHKERRQ(ierr);
  }
  win->valid_GPU_array = PETSC_CUDA_GPU;
  PetscFunctionReturn(0);
}

/* These functions are for the CUDA implementation of MAXPY with the loop unrolled on the CPU */
struct VecCUDAMAXPY4
{
  template <typename Tuple>
  __host__ __device__
  void operator()(Tuple t)
  {
    /*y += a1*x1 +a2*x2 + 13*x3 +a4*x4 */
    thrust::get<0>(t) += thrust::get<1>(t)*thrust::get<2>(t)+thrust::get<3>(t)*thrust::get<4>(t)+thrust::get<5>(t)*thrust::get<6>(t)+thrust::get<7>(t)*thrust::get<8>(t);
  }
};


struct VecCUDAMAXPY3
{
  template <typename Tuple>
  __host__ __device__
  void operator()(Tuple t)
  {
    /*y += a1*x1 +a2*x2 + 13*x3 */
    thrust::get<0>(t) += thrust::get<1>(t)*thrust::get<2>(t)+thrust::get<3>(t)*thrust::get<4>(t)+thrust::get<5>(t)*thrust::get<6>(t);
  }
};

struct VecCUDAMAXPY2
{
  template <typename Tuple>
  __host__ __device__
  void operator()(Tuple t)
  {
    /*y += a1*x1 +a2*x2*/
    thrust::get<0>(t) += thrust::get<1>(t)*thrust::get<2>(t)+thrust::get<3>(t)*thrust::get<4>(t);
  }
};
#undef __FUNCT__  
#define __FUNCT__ "VecMAXPY_SeqCUDA"
PetscErrorCode VecMAXPY_SeqCUDA(Vec xin, PetscInt nv,const PetscScalar *alpha,Vec *y)
{
  PetscErrorCode    ierr;
  PetscInt          n = xin->map->n,j,j_rem;
  Vec               yy0,yy1,yy2,yy3;
  PetscScalar       alpha0,alpha1,alpha2,alpha3;

  PetscFunctionBegin;
  ierr = PetscLogFlops(nv*2.0*n);CHKERRQ(ierr);
  ierr = VecCUDACopyToGPU(xin);CHKERRQ(ierr);
  switch (j_rem=nv&0x3) {
  case 3: 
    alpha0 = alpha[0]; 
    alpha1 = alpha[1]; 
    alpha2 = alpha[2]; 
    alpha += 3;
    yy0    = y[0];
    yy1    = y[1];
    yy2    = y[2];
    ierr   = VecCUDACopyToGPU(yy0);CHKERRQ(ierr);
    ierr   = VecCUDACopyToGPU(yy1);CHKERRQ(ierr);
    ierr   = VecCUDACopyToGPU(yy2);CHKERRQ(ierr);
    thrust::for_each(
	thrust::make_zip_iterator(
	    thrust::make_tuple(
		((CUSPARRAY*)xin->spptr)->begin(),
		thrust::make_constant_iterator(alpha0,0),
		((CUSPARRAY*)yy0->spptr)->begin(),
		thrust::make_constant_iterator(alpha1,0),
		((CUSPARRAY*)yy1->spptr)->begin(),
		thrust::make_constant_iterator(alpha2,0),
		((CUSPARRAY*)yy2->spptr)->begin())),
	thrust::make_zip_iterator(
	    thrust::make_tuple(
		((CUSPARRAY*)xin->spptr)->end(),  
		thrust::make_constant_iterator(alpha0,n),
		((CUSPARRAY*)yy0->spptr)->end(),
		thrust::make_constant_iterator(alpha1,n),
		((CUSPARRAY*)yy1->spptr)->end(),
		thrust::make_constant_iterator(alpha2,n),
		((CUSPARRAY*)yy2->spptr)->end())),
	VecCUDAMAXPY3());
    y     += 3;
    break;
  case 2: 
    alpha0 = alpha[0]; 
    alpha1 = alpha[1]; 
    alpha +=2;
    yy0    = y[0];
    yy1    = y[1];
    ierr   = VecCUDACopyToGPU(yy0);CHKERRQ(ierr);
    ierr   = VecCUDACopyToGPU(yy1);CHKERRQ(ierr);
    thrust::for_each(
	thrust::make_zip_iterator(
	    thrust::make_tuple(
		((CUSPARRAY*)xin->spptr)->begin(),
		thrust::make_constant_iterator(alpha0,0),
		((CUSPARRAY*)yy0->spptr)->begin(),
		thrust::make_constant_iterator(alpha1,0),
		((CUSPARRAY*)yy1->spptr)->begin())),
	thrust::make_zip_iterator(
	    thrust::make_tuple(
		((CUSPARRAY*)xin->spptr)->end(),  
		thrust::make_constant_iterator(alpha0,n),
		((CUSPARRAY*)yy0->spptr)->end(),
		thrust::make_constant_iterator(alpha1,n),
		((CUSPARRAY*)yy1->spptr)->end())),
	VecCUDAMAXPY2());
    y     +=2;
    break;
  case 1: 
    alpha0 = *alpha++; 
    yy0 = y[0];
    ierr =  VecAXPY_SeqCUDA(xin,alpha0,yy0);
    y     +=1;
    break;
  }
  for (j=j_rem; j<nv; j+=4) {
    alpha0 = alpha[0];
    alpha1 = alpha[1];
    alpha2 = alpha[2];
    alpha3 = alpha[3];
    alpha  += 4;
    yy0    = y[0];
    yy1    = y[1];
    yy2    = y[2];
    yy3    = y[3];
    ierr   = VecCUDACopyToGPU(yy0);CHKERRQ(ierr);
    ierr   = VecCUDACopyToGPU(yy1);CHKERRQ(ierr);
    ierr   = VecCUDACopyToGPU(yy2);CHKERRQ(ierr);
    ierr   = VecCUDACopyToGPU(yy3);CHKERRQ(ierr);
    thrust::for_each(
	thrust::make_zip_iterator(
	    thrust::make_tuple(
		((CUSPARRAY*)xin->spptr)->begin(),
		thrust::make_constant_iterator(alpha0,0),
		((CUSPARRAY*)yy0->spptr)->begin(),
		thrust::make_constant_iterator(alpha1,0),
		((CUSPARRAY*)yy1->spptr)->begin(),
		thrust::make_constant_iterator(alpha2,0),
		((CUSPARRAY*)yy2->spptr)->begin(),
		thrust::make_constant_iterator(alpha3,0),
		((CUSPARRAY*)yy3->spptr)->begin())),
	thrust::make_zip_iterator(
	    thrust::make_tuple(
		((CUSPARRAY*)xin->spptr)->end(),  
		thrust::make_constant_iterator(alpha0,n),
		((CUSPARRAY*)yy0->spptr)->end(),
		thrust::make_constant_iterator(alpha1,n),
		((CUSPARRAY*)yy1->spptr)->end(),
		thrust::make_constant_iterator(alpha2,n),
		((CUSPARRAY*)yy2->spptr)->end(),
		thrust::make_constant_iterator(alpha3,n),
		((CUSPARRAY*)yy3->spptr)->end())),
	VecCUDAMAXPY4());
    y      += 4;
  }
  xin->valid_GPU_array = PETSC_CUDA_GPU;
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

/*The following few template functions are for VecMDot_SeqCUDA*/

template <typename T1,typename T2>
struct cudamult2 : thrust::unary_function<T1,T2>
{
	__host__ __device__
	T2 operator()(T1 x)
	{
		return thrust::make_tuple(thrust::get<0>(x)*thrust::get<1>(x),thrust::get<0>(x)*thrust::get<2>(x));
	}
};

template <typename T>
struct cudaadd2 : thrust::binary_function<T,T,T>
{
	__host__ __device__
	T operator()(T x,T y)
	{
		return thrust::make_tuple(thrust::get<0>(x)+thrust::get<0>(y),thrust::get<1>(x)+thrust::get<1>(y));
	}
};
	
template <typename T1,typename T2>
struct cudamult3 : thrust::unary_function<T1,T2>
{
	__host__ __device__
	T2 operator()(T1 x)
	{
	  return thrust::make_tuple(thrust::get<0>(x)*thrust::get<1>(x),thrust::get<0>(x)*thrust::get<2>(x),thrust::get<0>(x)*thrust::get<3>(x));
	}
};

template <typename T>
struct cudaadd3 : thrust::binary_function<T,T,T>
{
	__host__ __device__
	T operator()(T x,T y)
	{
	  return thrust::make_tuple(thrust::get<0>(x)+thrust::get<0>(y),thrust::get<1>(x)+thrust::get<1>(y),thrust::get<2>(x)+thrust::get<2>(y));
	}
};
	template <typename T1,typename T2>
struct cudamult4 : thrust::unary_function<T1,T2>
{
	__host__ __device__
	T2 operator()(T1 x)
	{
	  return thrust::make_tuple(thrust::get<0>(x)*thrust::get<1>(x),thrust::get<0>(x)*thrust::get<2>(x),thrust::get<0>(x)*thrust::get<3>(x),thrust::get<0>(x)*thrust::get<4>(x));
	}
};

template <typename T>
struct cudaadd4 : thrust::binary_function<T,T,T>
{
	__host__ __device__
	T operator()(T x,T y)
	{
	  return thrust::make_tuple(thrust::get<0>(x)+thrust::get<0>(y),thrust::get<1>(x)+thrust::get<1>(y),thrust::get<2>(x)+thrust::get<2>(y),thrust::get<3>(x)+thrust::get<3>(y));
	}
};


#undef __FUNCT__  
#define __FUNCT__ "VecMDot_SeqCUDA"
PetscErrorCode VecMDot_SeqCUDA(Vec xin,PetscInt nv,const Vec yin[],PetscScalar *z)
{
  PetscErrorCode    ierr;
  PetscInt          n = xin->map->n,j,j_rem;
  Vec               yy0,yy1,yy2,yy3;
  PetscScalar       zero=0.0;
  thrust::tuple<PetscScalar,PetscScalar> result2;
  thrust::tuple<PetscScalar,PetscScalar,PetscScalar> result3;
  thrust::tuple<PetscScalar,PetscScalar,PetscScalar,PetscScalar>result4;

  PetscFunctionBegin;
  ierr = VecCUDACopyToGPU(xin);CHKERRQ(ierr);
  switch(j_rem=nv&0x3) {
  case 3: 
    yy0  =  yin[0];
    yy1  =  yin[1];
    yy2  =  yin[2];
    ierr =  VecCUDACopyToGPU(yy0);CHKERRQ(ierr);
    ierr =  VecCUDACopyToGPU(yy1);CHKERRQ(ierr);
    ierr =  VecCUDACopyToGPU(yy2);CHKERRQ(ierr);
    result3 = thrust::transform_reduce(thrust::make_zip_iterator(thrust::make_tuple(((CUSPARRAY *)xin->spptr)->begin(),((CUSPARRAY *)yy0->spptr)->begin(),((CUSPARRAY *)yy1->spptr)->begin(), ((CUSPARRAY *)yy2->spptr)->begin())),
				       thrust::make_zip_iterator(thrust::make_tuple(((CUSPARRAY *)xin->spptr)->end(),((CUSPARRAY *)yy0->spptr)->end(),((CUSPARRAY *)yy1->spptr)->end(),((CUSPARRAY *)yy2->spptr)->end())),
				       cudamult3<thrust::tuple<PetscScalar,PetscScalar,PetscScalar,PetscScalar>, thrust::tuple<PetscScalar,PetscScalar,PetscScalar> >(),
				       thrust::make_tuple(zero,zero,zero), /*init */
				       cudaadd3<thrust::tuple<PetscScalar,PetscScalar,PetscScalar> >()); /* binary function */
    z[0] = thrust::get<0>(result3);
    z[1] = thrust::get<1>(result3);
    z[2] = thrust::get<2>(result3);
    
    z    += 3;
    yin  += 3;
    break;
  case 2:
    yy0  =  yin[0];
    yy1  =  yin[1];
    ierr =  VecCUDACopyToGPU(yy0);CHKERRQ(ierr);
    ierr =  VecCUDACopyToGPU(yy1);CHKERRQ(ierr);
    result2 = thrust::transform_reduce(thrust::make_zip_iterator(thrust::make_tuple(((CUSPARRAY *)xin->spptr)->begin(),((CUSPARRAY *)yy0->spptr)->begin(),((CUSPARRAY *)yy1->spptr)->begin())),
				    thrust::make_zip_iterator(thrust::make_tuple(((CUSPARRAY *)xin->spptr)->end(),((CUSPARRAY *)yy0->spptr)->end(),((CUSPARRAY *)yy1->spptr)->end())),
				    cudamult2<thrust::tuple<PetscScalar,PetscScalar,PetscScalar>, thrust::tuple<PetscScalar,PetscScalar> >(),
				    thrust::make_tuple(zero,zero), /*init */
				    cudaadd2<thrust::tuple<PetscScalar, PetscScalar> >()); /* binary function */
    z[0] = thrust::get<0>(result2);
    z[1] = thrust::get<1>(result2);
    

    z    += 2;
    yin  += 2;
    break;
  case 1: 
    yy0  =  yin[0];
    ierr =  VecCUDACopyToGPU(yy0);CHKERRQ(ierr);
    ierr =  VecDot_SeqCUDA(xin,yy0,&z[0]);CHKERRQ(ierr);
    z    += 1;
    yin  += 1;
    break;
  }
  for (j=j_rem; j<nv; j+=4) {
    yy0  =  yin[0];
    yy1  =  yin[1];
    yy2  =  yin[2];
    yy3  =  yin[3];
    ierr =  VecCUDACopyToGPU(yy0);CHKERRQ(ierr);
    ierr =  VecCUDACopyToGPU(yy1);CHKERRQ(ierr);
    ierr =  VecCUDACopyToGPU(yy2);CHKERRQ(ierr);
    ierr =  VecCUDACopyToGPU(yy3);CHKERRQ(ierr);
    result4 = thrust::transform_reduce(thrust::make_zip_iterator(thrust::make_tuple(((CUSPARRAY *)xin->spptr)->begin(),((CUSPARRAY *)yy0->spptr)->begin(),((CUSPARRAY *)yy1->spptr)->begin(), ((CUSPARRAY *)yy2->spptr)->begin(),((CUSPARRAY *)yy3->spptr)->begin())),
				       thrust::make_zip_iterator(thrust::make_tuple(((CUSPARRAY *)xin->spptr)->end(),((CUSPARRAY *)yy0->spptr)->end(),((CUSPARRAY *)yy1->spptr)->end(),((CUSPARRAY *)yy2->spptr)->end(),((CUSPARRAY *)yy3->spptr)->end())),
				       cudamult4<thrust::tuple<PetscScalar,PetscScalar,PetscScalar,PetscScalar,PetscScalar>, thrust::tuple<PetscScalar,PetscScalar,PetscScalar,PetscScalar> >(),
				       thrust::make_tuple(zero,zero,zero,zero), /*init */
				       cudaadd4<thrust::tuple<PetscScalar,PetscScalar,PetscScalar,PetscScalar> >()); /* binary function */
    z[0] = thrust::get<0>(result4);
    z[1] = thrust::get<1>(result4);
    z[2] = thrust::get<2>(result4);
    z[3] = thrust::get<3>(result4);

    z    += 4;
    yin  += 4;
  }  
  ierr = PetscLogFlops(PetscMax(nv*(2.0*n-1),0.0));CHKERRQ(ierr);
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
    if (xin->valid_GPU_array == PETSC_CUDA_GPU) {
      /* copy in GPU */
       ierr = VecCUDAAllocateCheck(yin);CHKERRQ(ierr);
      cusp::blas::copy(*(CUSPARRAY *)(xin->spptr),*(CUSPARRAY *)(yin->spptr));
      yin->valid_GPU_array = PETSC_CUDA_GPU;

    } else if (xin->valid_GPU_array == PETSC_CUDA_CPU || xin->valid_GPU_array == PETSC_CUDA_UNALLOCATED) {
      /* copy in CPU if we are on the CPU*/
      ierr = VecCopy_Seq(xin,yin);CHKERRQ(ierr);
    
    } else if (xin->valid_GPU_array == PETSC_CUDA_BOTH) {
      /* if xin is valid in both places, see where yin is and copy there (because it's probably where we'll want to next use it) */
      if (yin->valid_GPU_array == PETSC_CUDA_CPU) {
	/* copy in CPU */
	ierr = VecCopy_Seq(xin,yin);CHKERRQ(ierr);

      } else if (yin->valid_GPU_array == PETSC_CUDA_GPU) {
	/* copy in GPU */
	ierr = VecCUDACopyToGPU(xin);CHKERRQ(ierr);
	cusp::blas::copy(*(CUSPARRAY *)(xin->spptr),*(CUSPARRAY *)(yin->spptr));
	yin->valid_GPU_array = PETSC_CUDA_GPU;
      } else if (yin->valid_GPU_array == PETSC_CUDA_BOTH) {
	/* xin and yin are both valid in both places (or yin was unallocated before the earlier call to allocatecheck
	   default to copy in GPU (this is an arbitrary choice) */
	cusp::blas::copy(*(CUSPARRAY *)(xin->spptr),*(CUSPARRAY *)(yin->spptr));
	yin->valid_GPU_array = PETSC_CUDA_GPU;
      } else {
	ierr = VecCopy_Seq(xin,yin);CHKERRQ(ierr);
      }
    }
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
#if defined(PETSC_USE_SCALAR_SINGLE)
    cublasSswap(bn,VecCUDACastToRawPtr(*(CUSPARRAY *)(xin->spptr)),one,VecCUDACastToRawPtr(*(CUSPARRAY *)(yin->spptr)),one);
#else
    cublasDswap(bn,VecCUDACastToRawPtr(*(CUSPARRAY *)(xin->spptr)),one,VecCUDACastToRawPtr(*(CUSPARRAY *)(yin->spptr)),one);
#endif
    ierr = cublasGetError();CHKERRCUDA(ierr);
    xin->valid_GPU_array = PETSC_CUDA_GPU;
    yin->valid_GPU_array = PETSC_CUDA_GPU;
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
    ierr = VecAYPX_SeqCUDA(yin,beta,xin);CHKERRQ(ierr);
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

/* structs below are for special cases of VecAXPBYPCZ_SeqCUDA */
struct VecCUDAXPBYPCZ
{
  /* z = x + b*y + c*z */
  template <typename Tuple>
  __host__ __device__
  void operator()(Tuple t)
  {
    thrust::get<0>(t) = thrust::get<1>(t)*thrust::get<0>(t)+thrust::get<2>(t)+thrust::get<4>(t)*thrust::get<3>(t);
  }
};
struct VecCUDAAXPBYPZ
{
  /* z = ax + b*y + z */
  template <typename Tuple>
  __host__ __device__
  void operator()(Tuple t)
  {
    thrust::get<0>(t) += thrust::get<2>(t)*thrust::get<1>(t)+thrust::get<4>(t)*thrust::get<3>(t);
  }
};

#undef __FUNCT__  
#define __FUNCT__ "VecAXPBYPCZ_SeqCUDA"
PetscErrorCode VecAXPBYPCZ_SeqCUDA(Vec zin,PetscScalar alpha,PetscScalar beta,PetscScalar gamma,Vec xin,Vec yin)
{
  PetscErrorCode     ierr;
  PetscInt           n = zin->map->n;

  PetscFunctionBegin;
    ierr = VecCUDACopyToGPU(xin);CHKERRQ(ierr);
    ierr = VecCUDACopyToGPU(yin);CHKERRQ(ierr);
    ierr = VecCUDACopyToGPU(zin);CHKERRQ(ierr);
    /*
  if (alpha == 1.0) {
    thrust::for_each(
	thrust::make_zip_iterator(
	    thrust::make_tuple(
		((CUSPARRAY*)zin->spptr)->begin(),
		thrust::make_constant_iterator(gamma,0),
		((CUSPARRAY*)xin->spptr)->begin(),
		((CUSPARRAY*)yin->spptr)->begin(),
		thrust::make_constant_iterator(beta,0))),
	thrust::make_zip_iterator(
	    thrust::make_tuple(
		((CUSPARRAY*)zin->spptr)->end(),  
		thrust::make_constant_iterator(gamma,n),
		((CUSPARRAY*)xin->spptr)->end(),
		((CUSPARRAY*)yin->spptr)->end(),
		thrust::make_constant_iterator(beta,n))),
	VecCUDAXPBYPCZ());
    ierr = PetscLogFlops(4.0*n);CHKERRQ(ierr);
  } else if (gamma == 1.0) {
    thrust::for_each(
	thrust::make_zip_iterator(
	    thrust::make_tuple(
		((CUSPARRAY*)zin->spptr)->begin(),
		((CUSPARRAY*)xin->spptr)->begin(),
		thrust::make_constant_iterator(alpha,0),
		((CUSPARRAY*)yin->spptr)->begin(),
		thrust::make_constant_iterator(beta,0))),
	thrust::make_zip_iterator(
	    thrust::make_tuple(
		((CUSPARRAY*)zin->spptr)->end(),  
		((CUSPARRAY*)xin->spptr)->end(),
		thrust::make_constant_iterator(alpha,n),	
		((CUSPARRAY*)yin->spptr)->end(),
		thrust::make_constant_iterator(beta,n))),
	VecCUDAAXPBYPZ());
    ierr = PetscLogFlops(4.0*n);CHKERRQ(ierr); 
  } else {
    */ 
     const PetscScalar  *yy,*xx;
  PetscScalar        *zz;
  PetscInt i;
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
#if defined(PETSC_USE_SCALAR_SINGLE)
    *z = cublasSasum(bn,VecCUDACastToRawPtr(*(CUSPARRAY *)(xin->spptr)),one);
#else
    *z = cublasDasum(bn,VecCUDACastToRawPtr(*(CUSPARRAY *)(xin->spptr)),one);
#endif
    ierr = cublasGetError();CHKERRCUDA(ierr);
    ierr = PetscLogFlops(PetscMax(n-1.0,0.0));CHKERRQ(ierr);
  } else if (type == NORM_1_AND_2) {
    ierr = VecNorm_SeqCUDA(xin,NORM_1,z);CHKERRQ(ierr);
    ierr = VecNorm_SeqCUDA(xin,NORM_2,z+1);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}


/*the following few functions should be modified to actually work with the GPU so they don't force unneccesary allocation of CPU memory */

#undef __FUNCT__  
#define __FUNCT__ "VecSetRandom_SeqCUDA"
PetscErrorCode VecSetRandom_SeqCUDA(Vec xin,PetscRandom r)
{
  PetscErrorCode ierr;
  PetscFunctionBegin;
  ierr = VecSetRandom_Seq(xin,r);CHKERRQ(ierr);
  if (xin->valid_GPU_array != PETSC_CUDA_UNALLOCATED){
    xin->valid_GPU_array = PETSC_CUDA_CPU;
  }
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

/*The following template functions are for VecDotNorm2_SeqCUDA.  Note that there is no complex support as currently written*/
template <typename T>
struct cudadotnormcalculate : thrust::unary_function<T,T>
{
	__host__ __device__
	T operator()(T x)
	{
		return thrust::make_tuple(thrust::get<0>(x)*thrust::get<1>(x),thrust::get<1>(x)*thrust::get<1>(x));
	}
};

template <typename T>
struct cudadotnormreduce : thrust::binary_function<T,T,T>
{
	__host__ __device__
	T operator()(T x,T y)
	{
		return thrust::make_tuple(thrust::get<0>(x)+thrust::get<0>(y),thrust::get<1>(x)+thrust::get<1>(y));
	}
};
	
#undef __FUNCT__
#define __FUNCT__ "VecDotNorm2_SeqCUDA"
PetscErrorCode VecDotNorm2_SeqCUDA(Vec s, Vec t, PetscScalar *dp, PetscScalar *nm)
{
  PetscErrorCode                         ierr;
  PetscScalar                            zero = 0.0,n=s->map->n;
  thrust::tuple<PetscScalar,PetscScalar> result;

  PetscFunctionBegin;
  ierr = VecCUDACopyToGPU(s);CHKERRQ(ierr);
  ierr = VecCUDACopyToGPU(t);CHKERRQ(ierr);
  result = thrust::transform_reduce(thrust::make_zip_iterator(thrust::make_tuple(((CUSPARRAY *)s->spptr)->begin(),((CUSPARRAY *)t->spptr)->begin())),
				    thrust::make_zip_iterator(thrust::make_tuple(((CUSPARRAY *)s->spptr)->end(),((CUSPARRAY *)t->spptr)->end())),
				    cudadotnormcalculate<thrust::tuple<PetscScalar,PetscScalar> >(),
				    thrust::make_tuple(zero,zero), /*init */
				    cudadotnormreduce<thrust::tuple<PetscScalar, PetscScalar> >()); /* binary function */
  *dp = thrust::get<0>(result);
  *nm = thrust::get<1>(result);
  ierr = PetscLogFlops(4.0*n);CHKERRQ(ierr);
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
  PetscMPIInt    size;
 
  PetscFunctionBegin;
  ierr = MPI_Comm_size(((PetscObject)V)->comm,&size);CHKERRQ(ierr);
  if  (size > 1) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_ARG_WRONG,"Cannot create VECSEQCUDA on more than one process");
  ierr = VecCreate_Seq_Private(V,0);CHKERRQ(ierr);
  ierr = PetscObjectChangeTypeName((PetscObject)V,VECSEQCUDA);CHKERRQ(ierr);
  V->ops->duplicate       = VecDuplicate_SeqCUDA;
  V->ops->dot             = VecDot_SeqCUDA;
  V->ops->norm            = VecNorm_SeqCUDA;
  V->ops->tdot            = VecTDot_SeqCUDA;
  V->ops->scale           = VecScale_SeqCUDA;
  V->ops->copy            = VecCopy_SeqCUDA;
  V->ops->set             = VecSet_SeqCUDA;
  V->ops->swap            = VecSwap_SeqCUDA;
  V->ops->axpy            = VecAXPY_SeqCUDA;
  V->ops->axpby           = VecAXPBY_SeqCUDA;
  V->ops->axpbypcz        = VecAXPBYPCZ_SeqCUDA;
  V->ops->pointwisemult   = VecPointwiseMult_SeqCUDA;
  V->ops->pointwisedivide = VecPointwiseDivide_SeqCUDA;
  V->ops->setrandom       = VecSetRandom_SeqCUDA;
  V->ops->view            = VecView_SeqCUDA;
  V->ops->placearray      = VecPlaceArray_SeqCUDA;
  V->ops->replacearray    = VecReplaceArray_SeqCUDA;
  V->ops->dot_local       = VecDot_SeqCUDA;
  V->ops->tdot_local      = VecTDot_SeqCUDA;
  V->ops->norm_local      = VecNorm_SeqCUDA;
  V->ops->resetarray      = VecResetArray_SeqCUDA;
  V->ops->destroy         = VecDestroy_SeqCUDA;
  V->ops->maxpy           = VecMAXPY_SeqCUDA;
  V->ops->mdot            = VecMDot_SeqCUDA;
  V->ops->aypx            = VecAYPX_SeqCUDA;
  V->ops->waxpy           = VecWAXPY_SeqCUDA;
  /*V->ops->dotnorm2        = VecDotNorm2_SeqCUDA;*/
  V->valid_GPU_array      = PETSC_CUDA_UNALLOCATED;
  PetscFunctionReturn(0);
}
EXTERN_C_END
