/*$Id: eige.c,v 1.35 2001/08/07 03:03:45 balay Exp $*/

#include "src/sles/ksp/kspimpl.h"   /*I "petscksp.h" I*/

#undef __FUNCT__  
#define __FUNCT__ "KSPComputeExplicitOperator"
/*@
    KSPComputeExplicitOperator - Computes the explicit preconditioned operator.  

    Collective on KSP

    Input Parameter:
.   ksp - the Krylov subspace context

    Output Parameter:
.   mat - the explict preconditioned operator

    Notes:
    This computation is done by applying the operators to columns of the 
    identity matrix.

    Currently, this routine uses a dense matrix format when 1 processor
    is used and a sparse format otherwise.  This routine is costly in general,
    and is recommended for use only with relatively small systems.

    Level: advanced
   
.keywords: KSP, compute, explicit, operator

.seealso: KSPComputeEigenvaluesExplicitly()
@*/
int KSPComputeExplicitOperator(KSP ksp,Mat *mat)
{
  Vec      in,out;
  int      ierr,i,M,m,size,*rows,start,end;
  Mat      A;
  MPI_Comm comm;
  PetscScalar   *array,zero = 0.0,one = 1.0;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ksp,KSP_COOKIE);
  PetscValidPointer(mat);
  comm = ksp->comm;

  ierr = MPI_Comm_size(comm,&size);CHKERRQ(ierr);

  ierr = VecDuplicate(ksp->vec_sol,&in);CHKERRQ(ierr);
  ierr = VecDuplicate(ksp->vec_sol,&out);CHKERRQ(ierr);
  ierr = VecGetSize(in,&M);CHKERRQ(ierr);
  ierr = VecGetLocalSize(in,&m);CHKERRQ(ierr);
  ierr = VecGetOwnershipRange(in,&start,&end);CHKERRQ(ierr);
  ierr = PetscMalloc((m+1)*sizeof(int),&rows);CHKERRQ(ierr);
  for (i=0; i<m; i++) {rows[i] = start + i;}

  if (size == 1) {
    ierr = MatCreateSeqDense(comm,M,M,PETSC_NULL,mat);CHKERRQ(ierr);
  } else {
    /*    ierr = MatCreateMPIDense(comm,m,M,M,M,PETSC_NULL,mat);CHKERRQ(ierr); */
    ierr = MatCreateMPIAIJ(comm,m,m,M,M,0,0,0,0,mat);CHKERRQ(ierr);
  }
  
  ierr = PCGetOperators(ksp->B,&A,PETSC_NULL,PETSC_NULL);CHKERRQ(ierr);

  for (i=0; i<M; i++) {

    ierr = VecSet(&zero,in);CHKERRQ(ierr);
    ierr = VecSetValues(in,1,&i,&one,INSERT_VALUES);CHKERRQ(ierr);
    ierr = VecAssemblyBegin(in);CHKERRQ(ierr);
    ierr = VecAssemblyEnd(in);CHKERRQ(ierr);

    ierr = KSP_MatMult(ksp,A,in,out);CHKERRQ(ierr);
    ierr = KSP_PCApply(ksp,ksp->B,out,in);CHKERRQ(ierr);
    
    ierr = VecGetArray(in,&array);CHKERRQ(ierr);
    ierr = MatSetValues(*mat,m,rows,1,&i,array,INSERT_VALUES);CHKERRQ(ierr); 
    ierr = VecRestoreArray(in,&array);CHKERRQ(ierr);

  }
  ierr = PetscFree(rows);CHKERRQ(ierr);
  ierr = VecDestroy(in);CHKERRQ(ierr);
  ierr = VecDestroy(out);CHKERRQ(ierr);
  ierr = MatAssemblyBegin(*mat,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(*mat,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#include "petscblaslapack.h"

#undef __FUNCT__  
#define __FUNCT__ "KSPComputeEigenvaluesExplicitly"
/*@
   KSPComputeEigenvaluesExplicitly - Computes all of the eigenvalues of the 
   preconditioned operator using LAPACK.  

   Collective on KSP

   Input Parameter:
+  ksp - iterative context obtained from KSPCreate()
-  n - size of arrays r and c

   Output Parameters:
+  r - real part of computed eigenvalues
-  c - complex part of computed eigenvalues

   Notes:
   This approach is very slow but will generally provide accurate eigenvalue
   estimates.  This routine explicitly forms a dense matrix representing 
   the preconditioned operator, and thus will run only for relatively small
   problems, say n < 500.

   Many users may just want to use the monitoring routine
   KSPSingularValueMonitor() (which can be set with option -ksp_singmonitor)
   to print the singular values at each iteration of the linear solve.

   Level: advanced

.keywords: KSP, compute, eigenvalues, explicitly

.seealso: KSPComputeEigenvalues(), KSPSingularValueMonitor(), KSPComputeExtremeSingularValues()
@*/
int KSPComputeEigenvaluesExplicitly(KSP ksp,int nmax,PetscReal *r,PetscReal *c) 
{
  Mat          BA;
  int          i,n,ierr,size,rank,dummy;
  MPI_Comm     comm = ksp->comm;
  PetscScalar  *array;
  Mat          A;
  int          m,row,nz,*cols;
  PetscScalar  *vals;

  PetscFunctionBegin;
  ierr =  KSPComputeExplicitOperator(ksp,&BA);CHKERRQ(ierr);
  ierr = MPI_Comm_size(comm,&size);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(comm,&rank);CHKERRQ(ierr);

  ierr     = MatGetSize(BA,&n,&n);CHKERRQ(ierr);
  if (size > 1) { /* assemble matrix on first processor */
    if (!rank) {
      ierr = MatCreateMPIDense(ksp->comm,n,n,n,n,PETSC_NULL,&A);CHKERRQ(ierr);
    }
    else {
      ierr = MatCreateMPIDense(ksp->comm,0,n,n,n,PETSC_NULL,&A);CHKERRQ(ierr);
    }
    PetscLogObjectParent(BA,A);

    ierr = MatGetOwnershipRange(BA,&row,&dummy);CHKERRQ(ierr);
    ierr = MatGetLocalSize(BA,&m,&dummy);CHKERRQ(ierr);
    for (i=0; i<m; i++) {
      ierr = MatGetRow(BA,row,&nz,&cols,&vals);CHKERRQ(ierr);
      ierr = MatSetValues(A,1,&row,nz,cols,vals,INSERT_VALUES);CHKERRQ(ierr);
      ierr = MatRestoreRow(BA,row,&nz,&cols,&vals);CHKERRQ(ierr);
      row++;
    } 

    ierr = MatAssemblyBegin(A,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
    ierr = MatAssemblyEnd(A,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
    ierr = MatGetArray(A,&array);CHKERRQ(ierr);
  } else {
    ierr = MatGetArray(BA,&array);CHKERRQ(ierr);
  }

#if defined(PETSC_HAVE_ESSL)
  /* ESSL has a different calling sequence for dgeev() and zgeev() than standard LAPACK */
  if (!rank) {
    PetscScalar    sdummy,*cwork;
    PetscReal *work,*realpart;
    int       clen,idummy,lwork,*perm,zero;

#if !defined(PETSC_USE_COMPLEX)
    clen = n;
#else
    clen = 2*n;
#endif
    ierr   = PetscMalloc(clen*sizeof(PetscScalar),&cwork);CHKERRQ(ierr);
    idummy = n;
    lwork  = 5*n;
    ierr   = PetscMalloc(lwork*sizeof(PetscReal),&work);CHKERRQ(ierr);
    ierr   = PetscMalloc(n*sizeof(PetscReal),&realpart);CHKERRQ(ierr);
    zero   = 0;
    LAgeev_(&zero,array,&n,cwork,&sdummy,&idummy,&idummy,&n,work,&lwork);
    ierr = PetscFree(work);CHKERRQ(ierr);

    /* For now we stick with the convention of storing the real and imaginary
       components of evalues separately.  But is this what we really want? */
    ierr = PetscMalloc(n*sizeof(int),&perm);CHKERRQ(ierr);

#if !defined(PETSC_USE_COMPLEX)
    for (i=0; i<n; i++) {
      realpart[i] = cwork[2*i];
      perm[i]     = i;
    }
    ierr = PetscSortRealWithPermutation(n,realpart,perm);CHKERRQ(ierr);
    for (i=0; i<n; i++) {
      r[i] = cwork[2*perm[i]];
      c[i] = cwork[2*perm[i]+1];
    }
#else
    for (i=0; i<n; i++) {
      realpart[i] = PetscRealPart(cwork[i]);
      perm[i]     = i;
    }
    ierr = PetscSortRealWithPermutation(n,realpart,perm);CHKERRQ(ierr);
    for (i=0; i<n; i++) {
      r[i] = PetscRealPart(cwork[perm[i]]);
      c[i] = PetscImaginaryPart(cwork[perm[i]]);
    }
#endif
    ierr = PetscFree(perm);CHKERRQ(ierr);
    ierr = PetscFree(realpart);CHKERRQ(ierr);
    ierr = PetscFree(cwork);CHKERRQ(ierr);
  }
#elif !defined(PETSC_USE_COMPLEX)
  if (!rank) {
    PetscScalar    *work,sdummy;
    PetscReal *realpart,*imagpart;
    int       idummy,lwork,*perm;

    idummy   = n;
    lwork    = 5*n;
    ierr     = PetscMalloc(2*n*sizeof(PetscReal),&realpart);CHKERRQ(ierr);
    imagpart = realpart + n;
    ierr     = PetscMalloc(5*n*sizeof(PetscReal),&work);CHKERRQ(ierr);
#if defined(PETSC_MISSING_LAPACK_GEEV) 
  SETERRQ(PETSC_ERR_SUP,"GEEV - Lapack routine is unavilable\nNot able to provide eigen values.");
#else
    LAgeev_("N","N",&n,array,&n,realpart,imagpart,&sdummy,&idummy,&sdummy,&idummy,work,&lwork,&ierr);
#endif
    if (ierr) SETERRQ1(PETSC_ERR_LIB,"Error in LAPACK routine %d",ierr);
    ierr = PetscFree(work);CHKERRQ(ierr);
    ierr = PetscMalloc(n*sizeof(int),&perm);CHKERRQ(ierr);
    for (i=0; i<n; i++) { perm[i] = i;}
    ierr = PetscSortRealWithPermutation(n,realpart,perm);CHKERRQ(ierr);
    for (i=0; i<n; i++) {
      r[i] = realpart[perm[i]];
      c[i] = imagpart[perm[i]];
    }
    ierr = PetscFree(perm);CHKERRQ(ierr);
    ierr = PetscFree(realpart);CHKERRQ(ierr);
  }
#else
  if (!rank) {
    PetscScalar *work,sdummy,*eigs;
    PetscReal *rwork;
    int    idummy,lwork,*perm;

    idummy   = n;
    lwork    = 5*n;
    ierr = PetscMalloc(5*n*sizeof(PetscScalar),&work);CHKERRQ(ierr);
    ierr = PetscMalloc(2*n*sizeof(PetscReal),&rwork);CHKERRQ(ierr);
    ierr = PetscMalloc(n*sizeof(PetscScalar),&eigs);CHKERRQ(ierr);
#if defined(PETSC_MISSING_LAPACK_GEEV) 
  SETERRQ(PETSC_ERR_SUP,"GEEV - Lapack routine is unavilable\nNot able to provide eigen values.");
#else
    LAgeev_("N","N",&n,array,&n,eigs,&sdummy,&idummy,&sdummy,&idummy,work,&lwork,rwork,&ierr);
#endif
    if (ierr) SETERRQ1(PETSC_ERR_LIB,"Error in LAPACK routine %d",ierr);
    ierr = PetscFree(work);CHKERRQ(ierr);
    ierr = PetscFree(rwork);CHKERRQ(ierr);
    ierr = PetscMalloc(n*sizeof(int),&perm);CHKERRQ(ierr);
    for (i=0; i<n; i++) { perm[i] = i;}
    for (i=0; i<n; i++) { r[i]    = PetscRealPart(eigs[i]);}
    ierr = PetscSortRealWithPermutation(n,r,perm);CHKERRQ(ierr);
    for (i=0; i<n; i++) {
      r[i] = PetscRealPart(eigs[perm[i]]);
      c[i] = PetscImaginaryPart(eigs[perm[i]]);
    }
    ierr = PetscFree(perm);CHKERRQ(ierr);
    ierr = PetscFree(eigs);CHKERRQ(ierr);
  }
#endif  
  if (size > 1) {
    ierr = MatRestoreArray(A,&array);CHKERRQ(ierr);
    ierr = MatDestroy(A);CHKERRQ(ierr);
  } else {
    ierr = MatRestoreArray(BA,&array);CHKERRQ(ierr);
  }
  ierr = MatDestroy(BA);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
