/*$Id: vinv.c,v 1.71 2001/09/11 16:31:37 bsmith Exp $*/
/*
     Some useful vector utility functions.
*/
#include "src/vec/vecimpl.h"          /*I "petscvec.h" I*/

#undef __FUNCT__  
#define __FUNCT__ "VecStrideScale"
/*@C
   VecStrideScale - Scales a subvector of a vector defined 
   by a starting point and a stride.

   Collective on Vec

   Input Parameter:
+  v - the vector 
.  start - starting point of the subvector (defined by a stride)
-  scale - value to multiply each subvector entry by

   Notes:
   One must call VecSetBlockSize() before this routine to set the stride 
   information, or use a vector created from a multicomponent DA.

   This will only work if the desire subvector is a stride subvector

   Level: advanced

   Concepts: scale^on stride of vector
   Concepts: stride^scale

.seealso: VecNorm(), VecStrideGather(), VecStrideScatter(), VecStrideMin(), VecStrideMax(), VecStrideScale()
@*/
int VecStrideScale(Vec v,int start,PetscScalar *scale)
{
  int         i,n,ierr,bs;
  PetscScalar *x,xscale = *scale;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(v,VEC_COOKIE);
  ierr = VecGetLocalSize(v,&n);CHKERRQ(ierr);
  ierr = VecGetArrayFast(v,&x);CHKERRQ(ierr);

  bs   = v->bs;
  if (start >= bs) {
    SETERRQ2(1,"Start of stride subvector (%d) is too large for stride\n\
            Have you set the vector blocksize (%d) correctly with VecSetBlockSize()?",start,bs);
  }
  x += start;

  for (i=0; i<n; i+=bs) {
    x[i] *= xscale;
  }
  x -= start;
  ierr = VecRestoreArrayFast(v,&x);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecStrideNorm"
/*@C
   VecStrideNorm - Computes the norm of subvector of a vector defined 
   by a starting point and a stride.

   Collective on Vec

   Input Parameter:
+  v - the vector 
.  start - starting point of the subvector (defined by a stride)
-  ntype - type of norm, one of NORM_1, NORM_2, NORM_INFINITY

   Output Parameter:
.  norm - the norm

   Notes:
   One must call VecSetBlockSize() before this routine to set the stride 
   information, or use a vector created from a multicomponent DA.

   If x is the array representing the vector x then this computes the norm 
   of the array (x[start],x[start+stride],x[start+2*stride], ....)

   This is useful for computing, say the norm of the pressure variable when
   the pressure is stored (interlaced) with other variables, say density etc.

   This will only work if the desire subvector is a stride subvector

   Level: advanced

   Concepts: norm^on stride of vector
   Concepts: stride^norm

.seealso: VecNorm(), VecStrideGather(), VecStrideScatter(), VecStrideMin(), VecStrideMax()
@*/
int VecStrideNorm(Vec v,int start,NormType ntype,PetscReal *nrm)
{
  int         i,n,ierr,bs;
  PetscScalar *x;
  PetscReal   tnorm;
  MPI_Comm    comm;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(v,VEC_COOKIE);
  ierr = VecGetLocalSize(v,&n);CHKERRQ(ierr);
  ierr = VecGetArrayFast(v,&x);CHKERRQ(ierr);
  ierr = PetscObjectGetComm((PetscObject)v,&comm);CHKERRQ(ierr);

  bs   = v->bs;
  if (start >= bs) {
    SETERRQ2(1,"Start of stride subvector (%d) is too large for stride\n\
            Have you set the vector blocksize (%d) correctly with VecSetBlockSize()?",start,bs);
  }
  x += start;

  if (ntype == NORM_2) {
    PetscScalar sum = 0.0;
    for (i=0; i<n; i+=bs) {
      sum += x[i]*(PetscConj(x[i]));
    }
    tnorm  = PetscRealPart(sum);
    ierr   = MPI_Allreduce(&tnorm,nrm,1,MPIU_REAL,MPI_SUM,comm);CHKERRQ(ierr);
    *nrm = sqrt(*nrm);
  } else if (ntype == NORM_1) {
    tnorm = 0.0;
    for (i=0; i<n; i+=bs) {
      tnorm += PetscAbsScalar(x[i]);
    }
    ierr   = MPI_Allreduce(&tnorm,nrm,1,MPIU_REAL,MPI_SUM,comm);CHKERRQ(ierr);
  } else if (ntype == NORM_INFINITY) {
    PetscReal tmp;
    tnorm = 0.0;

    for (i=0; i<n; i+=bs) {
      if ((tmp = PetscAbsScalar(x[i])) > tnorm) tnorm = tmp;
      /* check special case of tmp == NaN */
      if (tmp != tmp) {tnorm = tmp; break;}
    } 
    ierr   = MPI_Allreduce(&tnorm,nrm,1,MPIU_REAL,MPI_MAX,comm);CHKERRQ(ierr);
  } else {
    SETERRQ(1,"Unknown norm type");
  }

  ierr = VecRestoreArrayFast(v,&x);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecStrideMax"
/*@
   VecStrideMax - Computes the maximum of subvector of a vector defined 
   by a starting point and a stride and optionally its location.

   Collective on Vec

   Input Parameter:
+  v - the vector 
-  start - starting point of the subvector (defined by a stride)

   Output Parameter:
+  index - the location where the maximum occurred (not supported, pass PETSC_NULL,
           if you need this, send mail to petsc-maint@mcs.anl.gov to request it)
-  nrm - the max

   Notes:
   One must call VecSetBlockSize() before this routine to set the stride 
   information, or use a vector created from a multicomponent DA.

   If xa is the array representing the vector x, then this computes the max
   of the array (xa[start],xa[start+stride],xa[start+2*stride], ....)

   This is useful for computing, say the maximum of the pressure variable when
   the pressure is stored (interlaced) with other variables, e.g., density, etc.
   This will only work if the desire subvector is a stride subvector.

   Level: advanced

   Concepts: maximum^on stride of vector
   Concepts: stride^maximum

.seealso: VecMax(), VecStrideNorm(), VecStrideGather(), VecStrideScatter(), VecStrideMin()
@*/
int VecStrideMax(Vec v,int start,int *idex,PetscReal *nrm)
{
  int         i,n,ierr,bs;
  PetscScalar *x;
  PetscReal   max,tmp;
  MPI_Comm    comm;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(v,VEC_COOKIE);
  if (idex) {
    SETERRQ(1,"No support yet for returning index; send mail to petsc-maint@mcs.anl.gov asking for it");
  }
  ierr = VecGetLocalSize(v,&n);CHKERRQ(ierr);
  ierr = VecGetArrayFast(v,&x);CHKERRQ(ierr);
  ierr = PetscObjectGetComm((PetscObject)v,&comm);CHKERRQ(ierr);

  bs   = v->bs;
  if (start >= bs) {
    SETERRQ2(1,"Start of stride subvector (%d) is too large for stride\n\
            Have you set the vector blocksize (%d) correctly with VecSetBlockSize()?",start,bs);
  }
  x += start;

  if (!n) {
    max = PETSC_MIN;
  } else {
#if defined(PETSC_USE_COMPLEX)
    max = PetscRealPart(x[0]);
#else
    max = x[0];
#endif
    for (i=bs; i<n; i+=bs) {
#if defined(PETSC_USE_COMPLEX)
      if ((tmp = PetscRealPart(x[i])) > max) { max = tmp;}
#else
      if ((tmp = x[i]) > max) { max = tmp; } 
#endif
    }
  }
  ierr   = MPI_Allreduce(&max,nrm,1,MPIU_REAL,MPI_MAX,comm);CHKERRQ(ierr);

  ierr = VecRestoreArrayFast(v,&x);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecStrideMin"
/*@C
   VecStrideMin - Computes the minimum of subvector of a vector defined 
   by a starting point and a stride and optionally its location.

   Collective on Vec

   Input Parameter:
+  v - the vector 
-  start - starting point of the subvector (defined by a stride)

   Output Parameter:
+  idex - the location where the minimum occurred (not supported, pass PETSC_NULL,
           if you need this, send mail to petsc-maint@mcs.anl.gov to request it)
-  nrm - the min

   Level: advanced

   Notes:
   One must call VecSetBlockSize() before this routine to set the stride 
   information, or use a vector created from a multicomponent DA.

   If xa is the array representing the vector x, then this computes the min
   of the array (xa[start],xa[start+stride],xa[start+2*stride], ....)

   This is useful for computing, say the minimum of the pressure variable when
   the pressure is stored (interlaced) with other variables, e.g., density, etc.
   This will only work if the desire subvector is a stride subvector.

   Concepts: minimum^on stride of vector
   Concepts: stride^minimum

.seealso: VecMin(), VecStrideNorm(), VecStrideGather(), VecStrideScatter(), VecStrideMax()
@*/
int VecStrideMin(Vec v,int start,int *idex,PetscReal *nrm)
{
  int         i,n,ierr,bs;
  PetscScalar *x;
  PetscReal   min,tmp;
  MPI_Comm    comm;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(v,VEC_COOKIE);
  if (idex) {
    SETERRQ(1,"No support yet for returning index; send mail to petsc-maint@mcs.anl.gov asking for it");
  }
  ierr = VecGetLocalSize(v,&n);CHKERRQ(ierr);
  ierr = VecGetArrayFast(v,&x);CHKERRQ(ierr);
  ierr = PetscObjectGetComm((PetscObject)v,&comm);CHKERRQ(ierr);

  bs   = v->bs;
  if (start >= bs) {
    SETERRQ2(1,"Start of stride subvector (%d) is too large for stride\n\
            Have you set the vector blocksize (%d) correctly with VecSetBlockSize()?",start,bs);
  }
  x += start;

  if (!n) {
    min = PETSC_MAX;
  } else {
#if defined(PETSC_USE_COMPLEX)
    min = PetscRealPart(x[0]);
#else
    min = x[0];
#endif
    for (i=bs; i<n; i+=bs) {
#if defined(PETSC_USE_COMPLEX)
      if ((tmp = PetscRealPart(x[i])) < min) { min = tmp;}
#else
      if ((tmp = x[i]) < min) { min = tmp; } 
#endif
    }
  }
  ierr   = MPI_Allreduce(&min,nrm,1,MPIU_REAL,MPI_MIN,comm);CHKERRQ(ierr);

  ierr = VecRestoreArrayFast(v,&x);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecStrideScaleAll"
/*@C
   VecStrideScaleAll - Scales the subvectors of a vector defined 
   by a starting point and a stride.

   Collective on Vec

   Input Parameter:
+  v - the vector 
-  scales - values to multiply each subvector entry by

   Notes:
   One must call VecSetBlockSize() before this routine to set the stride 
   information, or use a vector created from a multicomponent DA.


   Level: advanced

   Concepts: scale^on stride of vector
   Concepts: stride^scale

.seealso: VecNorm(), VecStrideScale(), VecScale(), VecStrideGather(), VecStrideScatter(), VecStrideMin(), VecStrideMax()
@*/
int VecStrideScaleAll(Vec v,PetscScalar *scales)
{
  int         i,j,n,ierr,bs;
  PetscScalar *x;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(v,VEC_COOKIE);
  ierr = VecGetLocalSize(v,&n);CHKERRQ(ierr);
  ierr = VecGetArrayFast(v,&x);CHKERRQ(ierr);

  bs   = v->bs;

  /* need to provide optimized code for each bs */
  for (i=0; i<n; i+=bs) {
    for (j=0; j<bs; j++) {
      x[i+j] *= scales[j];
    }
  }
  ierr = VecRestoreArrayFast(v,&x);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecStrideNormAll"
/*@C
   VecStrideNormAll - Computes the norms  subvectors of a vector defined 
   by a starting point and a stride.

   Collective on Vec

   Input Parameter:
+  v - the vector 
-  ntype - type of norm, one of NORM_1, NORM_2, NORM_INFINITY

   Output Parameter:
.  nrm - the norms

   Notes:
   One must call VecSetBlockSize() before this routine to set the stride 
   information, or use a vector created from a multicomponent DA.

   If x is the array representing the vector x then this computes the norm 
   of the array (x[start],x[start+stride],x[start+2*stride], ....)

   This is useful for computing, say the norm of the pressure variable when
   the pressure is stored (interlaced) with other variables, say density etc.

   This will only work if the desire subvector is a stride subvector

   Level: advanced

   Concepts: norm^on stride of vector
   Concepts: stride^norm

.seealso: VecNorm(), VecStrideGather(), VecStrideScatter(), VecStrideMin(), VecStrideMax()
@*/
int VecStrideNormAll(Vec v,NormType ntype,PetscReal *nrm)
{
  int         i,j,n,ierr,bs;
  PetscScalar *x;
  PetscReal   tnorm[128];
  MPI_Comm    comm;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(v,VEC_COOKIE);
  ierr = VecGetLocalSize(v,&n);CHKERRQ(ierr);
  ierr = VecGetArrayFast(v,&x);CHKERRQ(ierr);
  ierr = PetscObjectGetComm((PetscObject)v,&comm);CHKERRQ(ierr);

  bs   = v->bs;
  if (bs > 128) SETERRQ(1,"Currently supports only blocksize up to 128");

  if (ntype == NORM_2) {
    PetscScalar sum[128];
    for (j=0; j<bs; j++) sum[j] = 0.0;
    for (i=0; i<n; i+=bs) {
      for (j=0; j<bs; j++) {
	sum[j] += x[i+j]*(PetscConj(x[i+j]));
      }
    }
    for (j=0; j<bs; j++) {
      tnorm[j]  = PetscRealPart(sum[j]);
    }
    ierr   = MPI_Allreduce(tnorm,nrm,bs,MPIU_REAL,MPI_SUM,comm);CHKERRQ(ierr);
    for (j=0; j<bs; j++) {
      nrm[j] = sqrt(nrm[j]);
    }
  } else if (ntype == NORM_1) {
    for (j=0; j<bs; j++) {
      tnorm[j] = 0.0;
    }
    for (i=0; i<n; i+=bs) {
      for (j=0; j<bs; j++) {
	tnorm[j] += PetscAbsScalar(x[i+j]);
      }
    }
    ierr   = MPI_Allreduce(tnorm,nrm,bs,MPIU_REAL,MPI_SUM,comm);CHKERRQ(ierr);
  } else if (ntype == NORM_INFINITY) {
    PetscReal tmp;
    for (j=0; j<bs; j++) {
      tnorm[j] = 0.0;
    }

    for (i=0; i<n; i+=bs) {
      for (j=0; j<bs; j++) {
	if ((tmp = PetscAbsScalar(x[i+j])) > tnorm[j]) tnorm[j] = tmp;
	/* check special case of tmp == NaN */
	if (tmp != tmp) {tnorm[j] = tmp; break;}
      }
    } 
    ierr   = MPI_Allreduce(tnorm,nrm,bs,MPIU_REAL,MPI_MAX,comm);CHKERRQ(ierr);
  } else {
    SETERRQ(1,"Unknown norm type");
  }

  ierr = VecRestoreArrayFast(v,&x);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecStrideMaxAll"
/*@C
   VecStrideMaxAll - Computes the maximums of subvectors of a vector defined 
   by a starting point and a stride and optionally its location.

   Collective on Vec

   Input Parameter:
.  v - the vector 

   Output Parameter:
+  index - the location where the maximum occurred (not supported, pass PETSC_NULL,
           if you need this, send mail to petsc-maint@mcs.anl.gov to request it)
-  nrm - the maximums

   Notes:
   One must call VecSetBlockSize() before this routine to set the stride 
   information, or use a vector created from a multicomponent DA.

   This is useful for computing, say the maximum of the pressure variable when
   the pressure is stored (interlaced) with other variables, e.g., density, etc.
   This will only work if the desire subvector is a stride subvector.

   Level: advanced

   Concepts: maximum^on stride of vector
   Concepts: stride^maximum

.seealso: VecMax(), VecStrideNorm(), VecStrideGather(), VecStrideScatter(), VecStrideMin()
@*/
int VecStrideMaxAll(Vec v,int *idex,PetscReal *nrm)
{
  int         i,j,n,ierr,bs;
  PetscScalar *x;
  PetscReal   max[128],tmp;
  MPI_Comm    comm;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(v,VEC_COOKIE);
  if (idex) {
    SETERRQ(1,"No support yet for returning index; send mail to petsc-maint@mcs.anl.gov asking for it");
  }
  ierr = VecGetLocalSize(v,&n);CHKERRQ(ierr);
  ierr = VecGetArrayFast(v,&x);CHKERRQ(ierr);
  ierr = PetscObjectGetComm((PetscObject)v,&comm);CHKERRQ(ierr);

  bs   = v->bs;
  if (bs > 128) SETERRQ(1,"Currently supports only blocksize up to 128");

  if (!n) {
    for (j=0; j<bs; j++) {
      max[j] = PETSC_MIN;
    }
  } else {
    for (j=0; j<bs; j++) {
#if defined(PETSC_USE_COMPLEX)
      max[j] = PetscRealPart(x[j]);
#else
      max[j] = x[j];
#endif
    }
    for (i=bs; i<n; i+=bs) {
      for (j=0; j<bs; j++) {
#if defined(PETSC_USE_COMPLEX)
	if ((tmp = PetscRealPart(x[i+j])) > max[j]) { max[j] = tmp;}
#else
	if ((tmp = x[i+j]) > max[j]) { max[j] = tmp; } 
#endif
      }
    }
  }
  ierr   = MPI_Allreduce(max,nrm,bs,MPIU_REAL,MPI_MAX,comm);CHKERRQ(ierr);

  ierr = VecRestoreArrayFast(v,&x);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecStrideMinAll"
/*@C
   VecStrideMinAll - Computes the minimum of subvector of a vector defined 
   by a starting point and a stride and optionally its location.

   Collective on Vec

   Input Parameter:
.  v - the vector 

   Output Parameter:
+  idex - the location where the minimum occurred (not supported, pass PETSC_NULL,
           if you need this, send mail to petsc-maint@mcs.anl.gov to request it)
-  nrm - the minimums

   Level: advanced

   Notes:
   One must call VecSetBlockSize() before this routine to set the stride 
   information, or use a vector created from a multicomponent DA.

   This is useful for computing, say the minimum of the pressure variable when
   the pressure is stored (interlaced) with other variables, e.g., density, etc.
   This will only work if the desire subvector is a stride subvector.

   Concepts: minimum^on stride of vector
   Concepts: stride^minimum

.seealso: VecMin(), VecStrideNorm(), VecStrideGather(), VecStrideScatter(), VecStrideMax()
@*/
int VecStrideMinAll(Vec v,int *idex,PetscReal *nrm)
{
  int         i,n,ierr,bs,j;
  PetscScalar *x;
  PetscReal   min[128],tmp;
  MPI_Comm    comm;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(v,VEC_COOKIE);
  if (idex) {
    SETERRQ(1,"No support yet for returning index; send mail to petsc-maint@mcs.anl.gov asking for it");
  }
  ierr = VecGetLocalSize(v,&n);CHKERRQ(ierr);
  ierr = VecGetArrayFast(v,&x);CHKERRQ(ierr);
  ierr = PetscObjectGetComm((PetscObject)v,&comm);CHKERRQ(ierr);

  bs   = v->bs;
  if (bs > 128) SETERRQ(1,"Currently supports only blocksize up to 128");

  if (!n) {
    for (j=0; j<bs; j++) {
      min[j] = PETSC_MAX;
    }
  } else {
    for (j=0; j<bs; j++) {
#if defined(PETSC_USE_COMPLEX)
      min[j] = PetscRealPart(x[j]);
#else
      min[j] = x[j];
#endif
    }
    for (i=bs; i<n; i+=bs) {
      for (j=0; j<bs; j++) {
#if defined(PETSC_USE_COMPLEX)
	if ((tmp = PetscRealPart(x[i+j])) < min[j]) { min[j] = tmp;}
#else
	if ((tmp = x[i+j]) < min[j]) { min[j] = tmp; } 
#endif
      }
    }
  }
  ierr   = MPI_Allreduce(min,nrm,bs,MPIU_REAL,MPI_MIN,comm);CHKERRQ(ierr);

  ierr = VecRestoreArrayFast(v,&x);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*----------------------------------------------------------------------------------------------*/
#undef __FUNCT__  
#define __FUNCT__ "VecStrideGatherAll"
/*@
   VecStrideGatherAll - Gathers all the single components from a multi-component vector into
   seperate vectors.

   Collective on Vec

   Input Parameter:
+  v - the vector 
-  addv - one of ADD_VALUES,INSERT_VALUES,MAX_VALUES

   Output Parameter:
.  s - the location where the subvectors are stored

   Notes:
   One must call VecSetBlockSize() before this routine to set the stride 
   information, or use a vector created from a multicomponent DA.

   If x is the array representing the vector x then this gathers
   the arrays (x[start],x[start+stride],x[start+2*stride], ....)
   for start=0,1,2,...bs-1

   The parallel layout of the vector and the subvector must be the same;
   i.e., nlocal of v = stride*(nlocal of s) 

   Not optimized; could be easily

   Level: advanced

   Concepts: gather^into strided vector

.seealso: VecStrideNorm(), VecStrideScatter(), VecStrideMin(), VecStrideMax(), VecStrideGather(),
          VecStrideScatterAll()
@*/
int VecStrideGatherAll(Vec v,Vec *s,InsertMode addv)
{
  int          i,n,ierr,bs,j,k,*bss,nv,jj,nvc;
  PetscScalar  *x,**y;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(v,VEC_COOKIE);
  PetscValidHeaderSpecific(*s,VEC_COOKIE);
  ierr = VecGetLocalSize(v,&n);CHKERRQ(ierr);
  ierr = VecGetArrayFast(v,&x);CHKERRQ(ierr);
  bs   = v->bs;

  ierr = PetscMalloc(bs*sizeof(PetscReal*),&y);CHKERRQ(ierr);
  ierr = PetscMalloc(bs*sizeof(int),&bss);CHKERRQ(ierr);
  nv   = 0;
  nvc  = 0;
  for (i=0; i<bs; i++) {
    ierr = VecGetBlockSize(s[i],&bss[i]);CHKERRQ(ierr);
    ierr = VecGetArrayFast(s[i],&y[i]);CHKERRQ(ierr);
    nvc  += bss[i];
    nv++;
    if (nvc > bs)  SETERRQ(1,"Number of subvectors in subvectors > number of vectors in main vector");
    if (nvc == bs) break;
  }

  n =  n/bs;

  jj = 0;
  if (addv == INSERT_VALUES) {
    for (j=0; j<nv; j++) {
      for (k=0; k<bss[j]; k++) {
	for (i=0; i<n; i++) {
	  y[j][i*bss[j] + k] = x[bs*i+jj+k];
        }
      }
      jj += bss[j];
    }
  } else if (addv == ADD_VALUES) {
    for (j=0; j<nv; j++) {
      for (k=0; k<bss[j]; k++) {
	for (i=0; i<n; i++) {
	  y[j][i*bss[j] + k] += x[bs*i+jj+k];
        }
      }
      jj += bss[j];
    }
#if !defined(PETSC_USE_COMPLEX)
  } else if (addv == MAX_VALUES) {
    for (j=0; j<nv; j++) {
      for (k=0; k<bss[j]; k++) {
	for (i=0; i<n; i++) {
	  y[j][i*bss[j] + k] = PetscMax(y[j][i*bss[j] + k],x[bs*i+jj+k]);
        }
      }
      jj += bss[j];
    }
#endif
  } else {
    SETERRQ(1,"Unknown insert type");
  }

  ierr = VecRestoreArrayFast(v,&x);CHKERRQ(ierr);
  for (i=0; i<nv; i++) {
    ierr = VecRestoreArrayFast(s[i],&y[i]);CHKERRQ(ierr);
  }
  ierr = PetscFree(y);CHKERRQ(ierr);
  ierr = PetscFree(bss);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecStrideScatterAll"
/*@
   VecStrideScatterAll - Scatters all the single components from seperate vectors into 
     a multi-component vector.

   Collective on Vec

   Input Parameter:
+  s - the location where the subvectors are stored
-  addv - one of ADD_VALUES,INSERT_VALUES,MAX_VALUES

   Output Parameter:
.  v - the multicomponent vector 

   Notes:
   One must call VecSetBlockSize() before this routine to set the stride 
   information, or use a vector created from a multicomponent DA.

   The parallel layout of the vector and the subvector must be the same;
   i.e., nlocal of v = stride*(nlocal of s) 

   Not optimized; could be easily

   Level: advanced

   Concepts:  scatter^into strided vector

.seealso: VecStrideNorm(), VecStrideScatter(), VecStrideMin(), VecStrideMax(), VecStrideGather(),
          VecStrideScatterAll()
@*/
int VecStrideScatterAll(Vec *s,Vec v,InsertMode addv)
{
  int          i,n,ierr,bs,j,jj,k,*bss,nv,nvc;
  PetscScalar  *x,**y;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(v,VEC_COOKIE);
  PetscValidHeaderSpecific(*s,VEC_COOKIE);
  ierr = VecGetLocalSize(v,&n);CHKERRQ(ierr);
  ierr = VecGetArrayFast(v,&x);CHKERRQ(ierr);
  bs   = v->bs;

  ierr = PetscMalloc(bs*sizeof(PetscReal*),&y);CHKERRQ(ierr);
  ierr = PetscMalloc(bs*sizeof(int*),&bss);CHKERRQ(ierr);
  nv  = 0;
  nvc = 0;
  for (i=0; i<bs; i++) {
    ierr = VecGetBlockSize(s[i],&bss[i]);CHKERRQ(ierr);
    ierr = VecGetArrayFast(s[i],&y[i]);CHKERRQ(ierr);
    nvc  += bss[i];
    nv++;
    if (nvc > bs)  SETERRQ(1,"Number of subvectors in subvectors > number of vectors in main vector");
    if (nvc == bs) break;
  }

  n =  n/bs;

  jj = 0;
  if (addv == INSERT_VALUES) {
    for (j=0; j<nv; j++) {
      for (k=0; k<bss[j]; k++) {
	for (i=0; i<n; i++) {
	  x[bs*i+jj+k] = y[j][i*bss[j] + k];
        }
      }
      jj += bss[j];
    }
  } else if (addv == ADD_VALUES) {
    for (j=0; j<nv; j++) {
      for (k=0; k<bss[j]; k++) {
	for (i=0; i<n; i++) {
	  x[bs*i+jj+k] += y[j][i*bss[j] + k];
        }
      }
      jj += bss[j];
    }
#if !defined(PETSC_USE_COMPLEX)
  } else if (addv == MAX_VALUES) {
    for (j=0; j<nv; j++) {
      for (k=0; k<bss[j]; k++) {
	for (i=0; i<n; i++) {
	  x[bs*i+jj+k] = PetscMax(x[bs*i+jj+k],y[j][i*bss[j] + k]);
        }
      }
      jj += bss[j];
    }
#endif
  } else {
    SETERRQ(1,"Unknown insert type");
  }

  ierr = VecRestoreArrayFast(v,&x);CHKERRQ(ierr);
  for (i=0; i<nv; i++) {
    ierr = VecRestoreArrayFast(s[i],&y[i]);CHKERRQ(ierr);
  }
  ierr = PetscFree(y);CHKERRQ(ierr);
  ierr = PetscFree(bss);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecStrideGather"
/*@
   VecStrideGather - Gathers a single component from a multi-component vector into
   another vector.

   Collective on Vec

   Input Parameter:
+  v - the vector 
.  start - starting point of the subvector (defined by a stride)
-  addv - one of ADD_VALUES,INSERT_VALUES,MAX_VALUES

   Output Parameter:
.  s - the location where the subvector is stored

   Notes:
   One must call VecSetBlockSize() before this routine to set the stride 
   information, or use a vector created from a multicomponent DA.

   If x is the array representing the vector x then this gathers
   the array (x[start],x[start+stride],x[start+2*stride], ....)

   The parallel layout of the vector and the subvector must be the same;
   i.e., nlocal of v = stride*(nlocal of s) 

   Not optimized; could be easily

   Level: advanced

   Concepts: gather^into strided vector

.seealso: VecStrideNorm(), VecStrideScatter(), VecStrideMin(), VecStrideMax(), VecStrideGatherAll(),
          VecStrideScatterAll()
@*/
int VecStrideGather(Vec v,int start,Vec s,InsertMode addv)
{
  int          i,n,ierr,bs,ns;
  PetscScalar  *x,*y;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(v,VEC_COOKIE);
  PetscValidHeaderSpecific(s,VEC_COOKIE);
  ierr = VecGetLocalSize(v,&n);CHKERRQ(ierr);
  ierr = VecGetLocalSize(s,&ns);CHKERRQ(ierr);
  ierr = VecGetArrayFast(v,&x);CHKERRQ(ierr);
  ierr = VecGetArrayFast(s,&y);CHKERRQ(ierr);

  bs   = v->bs;
  if (start >= bs) {
    SETERRQ2(1,"Start of stride subvector (%d) is too large for stride\n\
            Have you set the vector blocksize (%d) correctly with VecSetBlockSize()?",start,bs);
  }
  if (n != ns*bs) {
    SETERRQ2(1,"Subvector length * blocksize %d not correct for gather from original vector %d",ns*bs,n);
  }
  x += start;
  n =  n/bs;

  if (addv == INSERT_VALUES) {
    for (i=0; i<n; i++) {
      y[i] = x[bs*i];
    }
  } else if (addv == ADD_VALUES) {
    for (i=0; i<n; i++) {
      y[i] += x[bs*i];
    }
#if !defined(PETSC_USE_COMPLEX)
  } else if (addv == MAX_VALUES) {
    for (i=0; i<n; i++) {
      y[i] = PetscMax(y[i],x[bs*i]);
    }
#endif
  } else {
    SETERRQ(1,"Unknown insert type");
  }

  ierr = VecRestoreArrayFast(v,&x);CHKERRQ(ierr);
  ierr = VecRestoreArrayFast(s,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecStrideScatter"
/*@
   VecStrideScatter - Scatters a single component from a vector into a multi-component vector.

   Collective on Vec

   Input Parameter:
+  s - the single-component vector 
.  start - starting point of the subvector (defined by a stride)
-  addv - one of ADD_VALUES,INSERT_VALUES,MAX_VALUES

   Output Parameter:
.  v - the location where the subvector is scattered (the multi-component vector)

   Notes:
   One must call VecSetBlockSize() on the multi-component vector before this
   routine to set the stride  information, or use a vector created from a multicomponent DA.

   The parallel layout of the vector and the subvector must be the same;
   i.e., nlocal of v = stride*(nlocal of s) 

   Not optimized; could be easily

   Level: advanced

   Concepts: scatter^into strided vector

.seealso: VecStrideNorm(), VecStrideGather(), VecStrideMin(), VecStrideMax(), VecStrideGatherAll(),
          VecStrideScatterAll()
@*/
int VecStrideScatter(Vec s,int start,Vec v,InsertMode addv)
{
  int          i,n,ierr,bs,ns;
  PetscScalar  *x,*y;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(v,VEC_COOKIE);
  PetscValidHeaderSpecific(s,VEC_COOKIE);
  ierr = VecGetLocalSize(v,&n);CHKERRQ(ierr);
  ierr = VecGetLocalSize(s,&ns);CHKERRQ(ierr);
  ierr = VecGetArrayFast(v,&x);CHKERRQ(ierr);
  ierr = VecGetArrayFast(s,&y);CHKERRQ(ierr);

  bs   = v->bs;
  if (start >= bs) {
    SETERRQ2(1,"Start of stride subvector (%d) is too large for stride\n\
            Have you set the vector blocksize (%d) correctly with VecSetBlockSize()?",start,bs);
  }
  if (n != ns*bs) {
    SETERRQ2(1,"Subvector length * blocksize %d not correct for scatter to multicomponent vector %d",ns*bs,n);
  }
  x += start;
  n =  n/bs;


  if (addv == INSERT_VALUES) {
    for (i=0; i<n; i++) {
      x[bs*i] = y[i];
    }
  } else if (addv == ADD_VALUES) {
    for (i=0; i<n; i++) {
      x[bs*i] += y[i];
    }
#if !defined(PETSC_USE_COMPLEX)
  } else if (addv == MAX_VALUES) {
    for (i=0; i<n; i++) {
      x[bs*i] = PetscMax(y[i],x[bs*i]);
    }
#endif
  } else {
    SETERRQ(1,"Unknown insert type");
  }


  ierr = VecRestoreArrayFast(v,&x);CHKERRQ(ierr);
  ierr = VecRestoreArrayFast(s,&y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecReciprocal_Default"
int VecReciprocal_Default(Vec v)
{
  int         i,n,ierr;
  PetscScalar *x;

  PetscFunctionBegin;
  ierr = VecGetLocalSize(v,&n);CHKERRQ(ierr);
  ierr = VecGetArrayFast(v,&x);CHKERRQ(ierr);
  for (i=0; i<n; i++) {
    if (x[i] != 0.0) x[i] = 1.0/x[i];
  }
  ierr = VecRestoreArrayFast(v,&x);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "VecSqrt"
/*@
  VecSqrt - Replaces each component of a vector by the square root of its magnitude.

  Not collective

  Input Parameter:
. v - The vector

  Output Parameter:
. v - The vector square root

  Level: beginner

  Note: The actual function is sqrt(|x_i|)

.keywords: vector, sqrt, square root
@*/
int VecSqrt(Vec v)
{
  PetscScalar *x;
  int         i, n;
  int         ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(v, VEC_COOKIE);
  ierr = VecGetLocalSize(v, &n);                                                                          CHKERRQ(ierr);
  ierr = VecGetArrayFast(v, &x);                                                                              CHKERRQ(ierr);
  for(i = 0; i < n; i++) {
    x[i] = sqrt(PetscAbsScalar(x[i]));
  }
  ierr = VecRestoreArrayFast(v, &x);                                                                          CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecSum"
/*@
   VecSum - Computes the sum of all the components of a vector.

   Collective on Vec

   Input Parameter:
.  v - the vector 

   Output Parameter:
.  sum - the result

   Level: beginner

   Concepts: sum^of vector entries

.seealso: VecNorm()
@*/
int VecSum(Vec v,PetscScalar *sum)
{
  int         i,n,ierr;
  PetscScalar *x,lsum = 0.0;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(v,VEC_COOKIE);
  ierr = VecGetLocalSize(v,&n);CHKERRQ(ierr);
  ierr = VecGetArrayFast(v,&x);CHKERRQ(ierr);
  for (i=0; i<n; i++) {
    lsum += x[i];
  }
  ierr = MPI_Allreduce(&lsum,sum,1,MPIU_SCALAR,PetscSum_Op,v->comm);CHKERRQ(ierr);
  ierr = VecRestoreArrayFast(v,&x);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecShift"
/*@
   VecShift - Shifts all of the components of a vector by computing
   x[i] = x[i] + shift.

   Collective on Vec

   Input Parameters:
+  v - the vector 
-  shift - the shift

   Output Parameter:
.  v - the shifted vector 

   Level: intermediate

   Concepts: vector^adding constant

@*/
int VecShift(const PetscScalar *shift,Vec v)
{
  int         i,n,ierr;
  PetscScalar *x,lsum = *shift;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(v,VEC_COOKIE);
  ierr = VecGetLocalSize(v,&n);CHKERRQ(ierr); 
  ierr = VecGetArrayFast(v,&x);CHKERRQ(ierr);
  for (i=0; i<n; i++) {
    x[i] += lsum;
  }
  ierr = VecRestoreArrayFast(v,&x);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecAbs"
/*@
   VecAbs - Replaces every element in a vector with its absolute value.

   Collective on Vec

   Input Parameters:
.  v - the vector 

   Level: intermediate

   Concepts: vector^absolute value

@*/
int VecAbs(Vec v)
{
  int         i,n,ierr;
  PetscScalar *x;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(v,VEC_COOKIE);
  ierr = VecGetLocalSize(v,&n);CHKERRQ(ierr);
  ierr = VecGetArrayFast(v,&x);CHKERRQ(ierr);
  for (i=0; i<n; i++) {
    x[i] = PetscAbsScalar(x[i]);
  }
  ierr = VecRestoreArrayFast(v,&x);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "VecPermute"
/*@
  VecPermute - Permutes a vector in place using the given ordering.

  Input Parameters:
+ vec   - The vector
. order - The ordering
- inv   - The flag for inverting the permutation

  Level: beginner

  Note: This function does not yet support parallel Index Sets

.seealso: MatPermute()
.keywords: vec, permute
@*/
int VecPermute(Vec x, IS row, PetscTruth inv)
{
  PetscScalar *array, *newArray;
  int         *idx;
  int          i;
  int          ierr;

  PetscFunctionBegin;
  ierr = ISGetIndices(row, &idx);                                                                         CHKERRQ(ierr);
  ierr = VecGetArrayFast(x, &array);                                                                          CHKERRQ(ierr);
  ierr = PetscMalloc((x->n+1) * sizeof(PetscScalar), &newArray);                                          CHKERRQ(ierr);
#ifdef PETSC_USE_BOPT_g
  for(i = 0; i < x->n; i++) {
    if ((idx[i] < 0) || (idx[i] >= x->n)) {
      SETERRQ2(PETSC_ERR_ARG_CORRUPT, "Permutation index %d is out of bounds: %d", i, idx[i]);
    }
  }
#endif
  if (inv == PETSC_FALSE) {
    for(i = 0; i < x->n; i++) newArray[i]      = array[idx[i]];
  } else {
    for(i = 0; i < x->n; i++) newArray[idx[i]] = array[i];
  }
  ierr = VecRestoreArrayFast(x, &array);                                                                      CHKERRQ(ierr);
  ierr = ISRestoreIndices(row, &idx);                                                                     CHKERRQ(ierr);
  ierr = VecReplaceArray(x, newArray);                                                                    CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecEqual"
/*@
   VecEqual - Compares two vectors.

   Collective on Vec

   Input Parameters:
+  vec1 - the first matrix
-  vec2 - the second matrix

   Output Parameter:
.  flg - PETSC_TRUE if the vectors are equal; PETSC_FALSE otherwise.

   Level: intermediate

   Concepts: equal^two vectors
   Concepts: vector^equality

@*/
int VecEqual(Vec vec1,Vec vec2,PetscTruth *flg)
{
  PetscScalar  *v1,*v2;
  int          n1,n2,ierr;
  PetscTruth   flg1;

  PetscFunctionBegin;
  ierr = VecGetSize(vec1,&n1);CHKERRQ(ierr);
  ierr = VecGetSize(vec2,&n2);CHKERRQ(ierr);
  if (vec1 == vec2) {
    flg1 = PETSC_TRUE;
  } else if (n1 != n2) {
    flg1 = PETSC_FALSE;
  } else {
    ierr = VecGetArrayFast(vec1,&v1);CHKERRQ(ierr);
    ierr = VecGetArrayFast(vec2,&v2);CHKERRQ(ierr);
    ierr = PetscMemcmp(v1,v2,n1*sizeof(PetscScalar),&flg1);CHKERRQ(ierr);
    ierr = VecRestoreArrayFast(vec1,&v1);CHKERRQ(ierr);
    ierr = VecRestoreArrayFast(vec2,&v2);CHKERRQ(ierr);
  }

  /* combine results from all processors */
  ierr = MPI_Allreduce(&flg1,flg,1,MPI_INT,MPI_MIN,vec1->comm);CHKERRQ(ierr);
  

  PetscFunctionReturn(0);
}



