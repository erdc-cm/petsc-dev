/*
     Fortran interface for SDA routines.
*/
#include "src/fortran/custom/zpetsc.h"
#include "petscda.h"

#ifdef PETSC_HAVE_FORTRAN_CAPS
#define sdadestroy_           SDADESTROY
#define sdalocaltolocalbegin_ SDALOCALTOLOCALBEGIN
#define sdalocaltolocalend_   SDALOCALTOLOCALEND
#define sdacreate1d_          SDACREATE1D
#define sdacreate2d_          SDACREATE2D
#define sdacreate3d_          SDACREATE3D
#define sdagetghostcorners_   SDAGETGHOSTCORNERS
#define sdagetcorners_        SDAGETCORNERS
#define sdaarrayview_         SDAARRAYVIEW
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE)
#define sdadestroy_           sdadestroy
#define sdalocaltolocalbegin_ sdalocaltolocalbegin
#define sdalocaltolocalend_   sdalocaltolocalend
#define sdacreate1d_          sdacreate1d
#define sdacreate2d_          sdacreate2d
#define sdacreate3d_          sdacreate3d
#define sdagetghostcorners_   sdagetghostcorners
#define sdagetcorners_        sdagetcorners
#define sdaarrayview_         sdaarrayview
#endif

EXTERN PetscErrorCode SDAArrayView(SDA,PetscScalar*,PetscViewer);

EXTERN_C_BEGIN
void sdaarrayview_(SDA *da,PetscScalar *values,PetscViewer *vin,PetscErrorCode *ierr)
{
  PetscViewer v;
  PetscPatchDefaultViewers_Fortran(vin,v);
  *ierr = SDAArrayView(*da,values,v);
}

void sdagetghostcorners_(SDA *da,PetscInt *x,PetscInt *y,PetscInt *z,PetscInt *m,PetscInt *n,PetscInt *p,PetscErrorCode *ierr)
{
  CHKFORTRANNULLINTEGER(x);
  CHKFORTRANNULLINTEGER(y);
  CHKFORTRANNULLINTEGER(z);
  CHKFORTRANNULLINTEGER(m);
  CHKFORTRANNULLINTEGER(n);
  CHKFORTRANNULLINTEGER(p);
  *ierr = SDAGetGhostCorners(*da,x,y,z,m,n,p);
}

void sdagetcorners_(SDA *da,PetscInt *x,PetscInt *y,PetscInt *z,PetscInt *m,PetscInt *n,PetscInt *p,PetscErrorCode *ierr)
{
  CHKFORTRANNULLINTEGER(x);
  CHKFORTRANNULLINTEGER(y);
  CHKFORTRANNULLINTEGER(z);
  CHKFORTRANNULLINTEGER(m);
  CHKFORTRANNULLINTEGER(n);
  CHKFORTRANNULLINTEGER(p);
  *ierr = SDAGetCorners(*da,x,y,z,m,n,p);
}

void sdadestroy_(SDA *sda,PetscErrorCode *ierr)
{
  *ierr = SDADestroy((SDA)PetscToPointer(sda));
  PetscRmPointer(sda);
}

void sdalocaltolocalbegin_(SDA *sda,PetscScalar *g,InsertMode *mode,PetscScalar *l,
                           PetscErrorCode *ierr)
{
  *ierr = SDALocalToLocalBegin((SDA)PetscToPointer(sda),g,*mode,l);
}

void sdalocaltolocalend_(SDA *sda,PetscScalar *g,InsertMode *mode,PetscScalar *l,
                         PetscErrorCode *ierr){
  *ierr = SDALocalToLocalEnd((SDA)PetscToPointer(sda),g,*mode,l);
}

void sdacreate2d_(MPI_Comm *comm,DAPeriodicType *wrap,DAStencilType
                  *stencil_type,PetscInt *M,PetscInt *N,PetscInt *m,PetscInt *n,PetscInt *w,
                  PetscInt *s,PetscInt *lx,PetscInt *ly,SDA *inra,PetscErrorCode *ierr)
{
  CHKFORTRANNULLINTEGER(lx);
  CHKFORTRANNULLINTEGER(ly);
  *ierr = SDACreate2d(
	    (MPI_Comm)PetscToPointerComm(*comm),*wrap,
            *stencil_type,*M,*N,*m,*n,*w,*s,lx,ly,inra);
}

void sdacreate1d_(MPI_Comm *comm,DAPeriodicType *wrap,PetscInt *M,PetscInt *w,PetscInt *s,
                 PetscInt *lc,SDA *inra,PetscErrorCode *ierr)
{
  CHKFORTRANNULLINTEGER(lc);
  *ierr = SDACreate1d(
	   (MPI_Comm)PetscToPointerComm(*comm),*wrap,*M,*w,*s,lc,inra);
}

void sdacreate3d_(MPI_Comm *comm,DAPeriodicType *wrap,DAStencilType 
                 *stencil_type,PetscInt *M,PetscInt *N,PetscInt *P,PetscInt *m,PetscInt *n,PetscInt *p,
                 PetscInt *w,PetscInt *s,PetscInt *lx,PetscInt *ly,PetscInt *lz,SDA *inra,PetscErrorCode *ierr)
{
  CHKFORTRANNULLINTEGER(lx);
  CHKFORTRANNULLINTEGER(ly);
  CHKFORTRANNULLINTEGER(lz);
  *ierr = SDACreate3d(
	   (MPI_Comm)PetscToPointerComm(*comm),*wrap,*stencil_type,
           *M,*N,*P,*m,*n,*p,*w,*s,lx,ly,lz,inra);
}

EXTERN_C_END
