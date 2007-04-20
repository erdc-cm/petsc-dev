
#include "zpetsc.h"
#include "petscda.h"

#if defined(PETSC_HAVE_FORTRAN_CAPS)
#define dacreate1d_                  DACREATE1D
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE)
#define dacreate1d_                  dacreate1d
#endif

EXTERN_C_BEGIN

void PETSC_STDCALL dacreate1d_(MPI_Comm *comm,DAPeriodicType *wrap,PetscInt *M,PetscInt *w,PetscInt *s,
                 PetscInt *lc,DA *inra,PetscErrorCode *ierr)
{
  CHKFORTRANNULLINTEGER(lc);
  *ierr = DACreate1d((MPI_Comm)PetscToPointerComm(*comm),*wrap,*M,*w,*s,lc,inra);
}
EXTERN_C_END

#if defined(PETSC_HAVE_F90_C)
#include "src/sys/f90/f90impl.h"
EXTERN_C_BEGIN
void PETSC_STDCALL davecgetarrayf901_(DA *da,Vec *v,F90Array1d *a,PetscErrorCode *ierr)
{
  PetscInt    xs,ys,zs,xm,ym,zm,gxs,gys,gzs,gxm,gym,gzm,N,dim,dof;
  PetscScalar *aa;
  
  PetscFunctionBegin;
  *ierr = DAGetCorners(*da,&xs,&ys,&zs,&xm,&ym,&zm);
  *ierr = DAGetGhostCorners(*da,&gxs,&gys,&gzs,&gxm,&gym,&gzm);
  *ierr = DAGetInfo(*da,&dim,0,0,0,0,0,0,&dof,0,0,0);

  /* Handle case where user passes in global vector as opposed to local */
  *ierr = VecGetLocalSize(*v,&N);
  if (N == xm*ym*zm*dof) {
    gxm = xm;
    gym = ym;
    gzm = zm;
    gxs = xs;
    gys = ys;
    gzs = zs;
  } else if (N != gxm*gym*gzm*dof) {
    *ierr = PETSC_ERR_ARG_INCOMP;
  }
  *ierr = VecGetArray(*v,&aa);
  *ierr = F90Array1dCreate(aa,PETSC_SCALAR,gxs,gxs+gxm-1,a);
}

void PETSC_STDCALL davecrestorearrayf901_(DA *da,Vec *v,F90Array1d *a,PetscErrorCode *ierr)
{
  *ierr = VecRestoreArray(*v,0);
  *ierr = F90Array1dDestroy(a);
}

void PETSC_STDCALL davecgetarrayf90user1_(DA *da,Vec *v,F90Array1d *a,PetscErrorCode *ierr)
{
  PetscInt    xs,ys,zs,xm,ym,zm,gxs,gys,gzs,gxm,gym,gzm,N,dim,dof;
  PetscScalar *aa;
  
  PetscFunctionBegin;
  *ierr = DAGetCorners(*da,&xs,&ys,&zs,&xm,&ym,&zm);
  *ierr = DAGetGhostCorners(*da,&gxs,&gys,&gzs,&gxm,&gym,&gzm);
<  *ierr = DAGetInfo(*da,&dim,0,0,0,0,0,0,&dof,0,0,0);

  /* Handle case where user passes in global vector as opposed to local */
  *ierr = VecGetLocalSize(*v,&N);
  if (N == xm*ym*zm*dof) {
    gxm = xm;
    gym = ym;
    gzm = zm;
    gxs = xs;
    gys = ys;
    gzs = zs;
  } else if (N != gxm*gym*gzm*dof) {
    *ierr = PETSC_ERR_ARG_INCOMP;
  }
  *ierr = VecGetArray(*v,&aa);
  *ierr = F90Array1dCreate(aa,(PetscDataType)(-dof*sizeof(PetscScalar)),gxs,gxs+gxm-1,a);
}

void PETSC_STDCALL davecrestorearrayf90user1_(DA *da,Vec *v,F90Array1d *a,PetscErrorCode *ierr)
{
  *ierr = VecRestoreArray(*v,0);
  *ierr = F90Array1dDestroy(a);
}

EXTERN_C_END
#endif
