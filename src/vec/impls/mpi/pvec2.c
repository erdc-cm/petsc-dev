/*$Id: pvec2.c,v 1.57 2001/09/11 16:32:01 bsmith Exp $*/

/*
     Code for some of the parallel vector primatives.
*/
#include "src/vec/impls/mpi/pvecimpl.h" 
#include "src/inline/dot.h"
#include "petscblaslapack.h"

#define do_not_use_ethernet
int Ethernet_Allreduce(PetscReal *in,PetscReal *out,int n,MPI_Datatype type,MPI_Op op,MPI_Comm comm)
{
  int        i,rank,size,ierr;
  MPI_Status status;


  ierr = MPI_Comm_size(comm,&size);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(comm,&rank);CHKERRQ(ierr);

  if (rank) {
    ierr = MPI_Recv(out,n,MPIU_REAL,rank-1,837,comm,&status);CHKERRQ(ierr);
    for (i =0; i<n; i++) in[i] += out[i];
  }
  if (rank != size - 1) {
    ierr = MPI_Send(in,n,MPIU_REAL,rank+1,837,comm);CHKERRQ(ierr);
  }
  if (rank == size-1) {
    for (i=0; i<n; i++) out[i] = in[i];    
  } else {
    ierr = MPI_Recv(out,n,MPIU_REAL,rank+1,838,comm,&status);CHKERRQ(ierr);
  }
  if (rank) {
    ierr = MPI_Send(out,n,MPIU_REAL,rank-1,838,comm);CHKERRQ(ierr);
  }
  return 0;
}


#undef __FUNCT__  
#define __FUNCT__ "VecMDot_MPI"
int VecMDot_MPI(int nv,Vec xin,const Vec y[],PetscScalar *z)
{
  PetscScalar awork[128],*work = awork;
  int         ierr;

  PetscFunctionBegin;
  if (nv > 128) {
    ierr = PetscMalloc(nv*sizeof(PetscScalar),&work);CHKERRQ(ierr);
  }
  ierr = VecMDot_Seq(nv,xin,y,work);CHKERRQ(ierr);
  ierr = MPI_Allreduce(work,z,nv,MPIU_SCALAR,PetscSum_Op,xin->comm);CHKERRQ(ierr);
  if (nv > 128) {
    ierr = PetscFree(work);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecMTDot_MPI"
int VecMTDot_MPI(int nv,Vec xin,const Vec y[],PetscScalar *z)
{
  PetscScalar awork[128],*work = awork;
  int         ierr;

  PetscFunctionBegin;
  if (nv > 128) {
    ierr = PetscMalloc(nv*sizeof(PetscScalar),&work);CHKERRQ(ierr);
  }
  ierr = VecMTDot_Seq(nv,xin,y,work);CHKERRQ(ierr);
  ierr = MPI_Allreduce(work,z,nv,MPIU_SCALAR,PetscSum_Op,xin->comm);CHKERRQ(ierr);
  if (nv > 128) {
    ierr = PetscFree(work);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecNorm_MPI"
int VecNorm_MPI(Vec xin,NormType type,PetscReal *z)
{
  Vec_MPI      *x = (Vec_MPI*)xin->data;
  PetscReal    sum,work = 0.0;
  PetscScalar  *xx = x->array;
  int          n = xin->n,ierr;

  PetscFunctionBegin;
  if (type == NORM_2 || type == NORM_FROBENIUS) {

#if defined(PETSC_HAVE_SLOW_NRM2)
#if defined(PETSC_USE_FORTRAN_KERNEL_NORM)
    fortrannormsqr_(xx,&n,&work);
#elif defined(PETSC_USE_UNROLLED_NORM)
    switch (n & 0x3) {
      case 3: work += PetscRealPart(xx[0]*PetscConj(xx[0])); xx++;
      case 2: work += PetscRealPart(xx[0]*PetscConj(xx[0])); xx++;
      case 1: work += PetscRealPart(xx[0]*PetscConj(xx[0])); xx++; n -= 4;
    }
    while (n>0) {
      work += PetscRealPart(xx[0]*PetscConj(xx[0])+xx[1]*PetscConj(xx[1])+
                        xx[2]*PetscConj(xx[2])+xx[3]*PetscConj(xx[3]));
      xx += 4; n -= 4;
    } 
#else
    {int i; for (i=0; i<n; i++) work += PetscRealPart((xx[i])*(PetscConj(xx[i])));}
#endif
#else
    {int one = 1;
      work  = BLnrm2_(&n,xx,&one);
      work *= work;
    }
#endif
    ierr = MPI_Allreduce(&work,&sum,1,MPIU_REAL,MPI_SUM,xin->comm);CHKERRQ(ierr);
    *z = sqrt(sum);
    PetscLogFlops(2*xin->n);
  } else if (type == NORM_1) {
    /* Find the local part */
    ierr = VecNorm_Seq(xin,NORM_1,&work);CHKERRQ(ierr);
    /* Find the global max */
    ierr = MPI_Allreduce(&work,z,1,MPIU_REAL,MPI_SUM,xin->comm);CHKERRQ(ierr);
  } else if (type == NORM_INFINITY) {
    /* Find the local max */
    ierr = VecNorm_Seq(xin,NORM_INFINITY,&work);CHKERRQ(ierr);
    /* Find the global max */
    ierr = MPI_Allreduce(&work,z,1,MPIU_REAL,MPI_MAX,xin->comm);CHKERRQ(ierr);
  } else if (type == NORM_1_AND_2) {
    PetscReal temp[2];
    ierr = VecNorm_Seq(xin,NORM_1,temp);CHKERRQ(ierr);
    ierr = VecNorm_Seq(xin,NORM_2,temp+1);CHKERRQ(ierr);
    temp[1] = temp[1]*temp[1];
    ierr = MPI_Allreduce(temp,z,2,MPIU_REAL,MPI_SUM,xin->comm);CHKERRQ(ierr);
    z[1] = sqrt(z[1]);
  }
  PetscFunctionReturn(0);
}

/*
       These two functions are the MPI reduction operation used for max and min with index
   The call below to MPI_Op_create() converts the function Vec[Max,Min]_Local() to the 
   MPI operator Vec[Max,Min]_Local_Op.
*/
MPI_Op VecMax_Local_Op = 0;
MPI_Op VecMin_Local_Op = 0;

EXTERN_C_BEGIN
#undef __FUNCT__
#define __FUNCT__ "VecMax_Local"
void VecMax_Local(void *in,void *out,int *cnt,MPI_Datatype *datatype)
{
  PetscReal *xin = (PetscReal *)in,*xout = (PetscReal*)out;

  PetscFunctionBegin;
  if (*datatype != MPIU_REAL) {
    (*PetscErrorPrintf)("Can only handle MPIU_REAL data types");
    MPI_Abort(MPI_COMM_WORLD,1);
  }
  if (xin[0] > xout[0]) {
    xout[0] = xin[0];
    xout[1] = xin[1];
  }
  PetscStackPop;
  return; /* cannot return a value */
}
EXTERN_C_END

EXTERN_C_BEGIN
#undef __FUNCT__
#define __FUNCT__ "VecMin_Local"
void VecMin_Local(void *in,void *out,int *cnt,MPI_Datatype *datatype)
{
  PetscReal *xin = (PetscReal *)in,*xout = (PetscReal*)out;

  PetscFunctionBegin;
  if (*datatype != MPIU_REAL) {
    (*PetscErrorPrintf)("Can only handle MPIU_REAL data types");
    MPI_Abort(MPI_COMM_WORLD,1);
  }
  if (xin[0] < xout[0]) {
    xout[0] = xin[0];
    xout[1] = xin[1];
  }
  PetscStackPop;
  return;
}
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "VecMax_MPI"
int VecMax_MPI(Vec xin,int *idx,PetscReal *z)
{
  int       ierr;
  PetscReal work;

  PetscFunctionBegin;
  /* Find the local max */
  ierr = VecMax_Seq(xin,idx,&work);CHKERRQ(ierr);

  /* Find the global max */
  if (!idx) {
    ierr = MPI_Allreduce(&work,z,1,MPIU_REAL,MPI_MAX,xin->comm);CHKERRQ(ierr);
  } else {
    PetscReal work2[2],z2[2];
    int       rstart;

    if (!VecMax_Local_Op) {
      ierr = MPI_Op_create(VecMax_Local,1,&VecMax_Local_Op);CHKERRQ(ierr);
    }
     
    ierr = VecGetOwnershipRange(xin,&rstart,PETSC_NULL);CHKERRQ(ierr);
    work2[0] = work;
    work2[1] = *idx + rstart;
    ierr = MPI_Allreduce(work2,z2,2,MPIU_REAL,VecMax_Local_Op,xin->comm);CHKERRQ(ierr);
    *z   = z2[0];
    *idx = (int)z2[1];
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecMin_MPI"
int VecMin_MPI(Vec xin,int *idx,PetscReal *z)
{
  int       ierr;
  PetscReal work;

  PetscFunctionBegin;
  /* Find the local Min */
  ierr = VecMin_Seq(xin,idx,&work);CHKERRQ(ierr);

  /* Find the global Min */
  if (!idx) {
    ierr = MPI_Allreduce(&work,z,1,MPIU_REAL,MPI_MIN,xin->comm);CHKERRQ(ierr);
  } else {
    PetscReal work2[2],z2[2];
    int       rstart;

    if (!VecMin_Local_Op) {
      ierr = MPI_Op_create(VecMin_Local,1,&VecMin_Local_Op);CHKERRQ(ierr);
    }
     
    ierr = VecGetOwnershipRange(xin,&rstart,PETSC_NULL);CHKERRQ(ierr);
    work2[0] = work;
    work2[1] = *idx + rstart;
    ierr = MPI_Allreduce(work2,z2,2,MPIU_REAL,VecMin_Local_Op,xin->comm);CHKERRQ(ierr);
    *z   = z2[0];
    *idx = (int)z2[1];
  }
  PetscFunctionReturn(0);
}








