#include "private/fortranimpl.h"
#include "petscmesh.h"
#include "src/sys/f90/f90impl.h"

#ifdef PETSC_HAVE_FORTRAN_CAPS
#define meshgetcoordinatesf90_     MESHGETCOORDINATESF90
#define meshrestorecoordinatesf90_ MESHRESTORECOORDINATESF90
#define meshgetelementsf90_        MESHGETELEMENTSF90
#define meshrestoreelementsf90_    MESHRESTOREELEMENTSF90
#define sectiongetarrayf90_        SECTIONGETARRAYF90
#define sectiongetarray1df90_      SECTIONGETARRAY1DF90
#define bcsectiongetarrayf90_      BCSECTIONGETARRAYF90
#define bcsectiongetarray1df90_    BCSECTIONGETARRAY1DF90
#define bcsectionrealgetarrayf90_  BCSECTIONREALGETARRAYF90
#define bcfuncgetarrayf90_         BCFUNCGETARRAYF90
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE)
#define meshgetcoordinatesf90_     meshgetcoordinatesf90
#define meshrestorecoordinatesf90_ meshrestorecoordinatesf90
#define meshgetelementsf90_        meshgetelementsf90
#define meshrestoreelementsf90_    meshrestoreelementsf90
#define sectiongetarrayf90_        sectiongetarrayf90
#define sectiongetarray1df90_      sectiongetarray1df90
#define bcsectiongetarrayf90_      bcsectiongetarrayf90
#define bcsectiongetarray1df90_    bcsectiongetarray1df90
#define bcsectionrealgetarrayf90_  bcsectionrealgetarrayf90
#define bcfuncgetarrayf90_         bcfuncgetarrayf90
#endif

EXTERN_C_BEGIN

void PETSC_STDCALL meshgetcoordinatesf90_(Mesh *mesh,F90Array2d *ptr,int *__ierr PETSC_F90_2PTR_PROTO(ptrd))
{
  PetscReal *c;
  PetscInt   n, d;
  *__ierr = MeshGetCoordinates(*mesh,PETSC_TRUE,&n,&d,&c); if (*__ierr) return;
  *__ierr = F90Array2dCreate(c,PETSC_REAL,1,n,1,d,ptr PETSC_F90_2PTR_PARAM(ptrd));
}
void PETSC_STDCALL meshrestorecoordinatesf90_(Mesh *x,F90Array2d *ptr,int *__ierr PETSC_F90_2PTR_PROTO(ptrd))
{
  PetscReal *c;
  *__ierr = F90Array2dAccess(ptr,PETSC_REAL,(void**)&c PETSC_F90_2PTR_PARAM(ptrd));if (*__ierr) return;
  *__ierr = F90Array2dDestroy(ptr,PETSC_REAL PETSC_F90_2PTR_PARAM(ptrd));if (*__ierr) return;
  *__ierr = PetscFree(c);
}
void PETSC_STDCALL meshgetelementsf90_(Mesh *mesh,F90Array2d *ptr,int *__ierr PETSC_F90_2PTR_PROTO(ptrd))
{
  PetscInt   *v;
  PetscInt   n, c;
  *__ierr = MeshGetElements(*mesh,PETSC_FALSE,&n,&c,&v); if (*__ierr) return;
  *__ierr = F90Array2dCreate(v,PETSC_INT,1,c,1,n,ptr PETSC_F90_2PTR_PARAM(ptrd));
}
void PETSC_STDCALL meshrestoreelementsf90_(Mesh *x,F90Array2d *ptr,int *__ierr PETSC_F90_2PTR_PROTO(ptrd))
{
  PetscInt   *v;
  *__ierr = F90Array2dAccess(ptr,PETSC_INT,(void**)&v PETSC_F90_2PTR_PARAM(ptrd));if (*__ierr) return;
  *__ierr = F90Array2dDestroy(ptr,PETSC_INT PETSC_F90_2PTR_PARAM(ptrd));if (*__ierr) return;
  *__ierr = PetscFree(v);
}
void PETSC_STDCALL sectiongetarrayf90_(Mesh *mesh,CHAR name PETSC_MIXED_LEN(len),F90Array2d *ptr,int *ierr PETSC_END_LEN(len) PETSC_F90_2PTR_PROTO(ptrd))
{
  PetscScalar *a;
  PetscInt     n, d;
  char        *nF;
  FIXCHAR(name,len,nF);
  *ierr = SectionGetArray(*mesh,nF,&n,&d,&a); if (*ierr) return;
  *ierr = F90Array2dCreate(a,PETSC_SCALAR,1,d,1,n,ptr PETSC_F90_2PTR_PARAM(ptrd));
  FREECHAR(name,nF);
}
void PETSC_STDCALL sectiongetarray1df90_(Mesh *mesh,CHAR name PETSC_MIXED_LEN(len),F90Array1d *ptr,int *ierr PETSC_END_LEN(len) PETSC_F90_2PTR_PROTO(ptrd))
{
  PetscScalar *a;
  PetscInt     n, d;
  char        *nF;
  FIXCHAR(name,len,nF);
  *ierr = SectionGetArray(*mesh,nF,&n,&d,&a); if (*ierr) return;
  *ierr = F90Array1dCreate(a,PETSC_SCALAR,1,n*d,ptr PETSC_F90_2PTR_PARAM(ptrd));
  FREECHAR(name,nF);
}
void PETSC_STDCALL bcsectiongetarrayf90_(Mesh *mesh,CHAR name PETSC_MIXED_LEN(len),F90Array2d *ptr,int *ierr PETSC_END_LEN(len) PETSC_F90_2PTR_PROTO(ptrd))
{
  PetscInt *a;
  PetscInt  n, d;
  char     *nF;
  FIXCHAR(name,len,nF);
  *ierr = BCSectionGetArray(*mesh,nF,&n,&d,&a); if (*ierr) return;
  *ierr = F90Array2dCreate(a,PETSC_INT,1,d,1,n,ptr PETSC_F90_2PTR_PARAM(ptrd));
  FREECHAR(name,nF);
}
void PETSC_STDCALL bcsectiongetarray1df90_(Mesh *mesh,CHAR name PETSC_MIXED_LEN(len),F90Array1d *ptr,int *ierr PETSC_END_LEN(len) PETSC_F90_2PTR_PROTO(ptrd))
{
  PetscInt *a;
  PetscInt  n, d;
  char     *nF;
  FIXCHAR(name,len,nF);
  *ierr = BCSectionGetArray(*mesh,nF,&n,&d,&a); if (*ierr) return;
  *ierr = F90Array1dCreate(a,PETSC_INT,1,n*d,ptr PETSC_F90_2PTR_PARAM(ptrd));
  FREECHAR(name,nF);
}
void PETSC_STDCALL bcsectionrealgetarrayf90_(Mesh *mesh,CHAR name PETSC_MIXED_LEN(len),F90Array2d *ptr,int *ierr PETSC_END_LEN(len) PETSC_F90_2PTR_PROTO(ptrd))
{
  PetscReal *a;
  PetscInt   n, d;
  char      *nF;
  FIXCHAR(name,len,nF);
  *ierr = BCSectionRealGetArray(*mesh,nF,&n,&d,&a); if (*ierr) return;
  *ierr = F90Array2dCreate(a,PETSC_SCALAR,1,d,1,n,ptr PETSC_F90_2PTR_PARAM(ptrd));
  FREECHAR(name,nF);
}
void PETSC_STDCALL bcfuncgetarrayf90_(Mesh *mesh,F90Array2d *ptr,int *ierr PETSC_F90_2PTR_PROTO(ptrd))
{
  PetscScalar *a;
  PetscInt     n, d;
  *ierr = BCFUNCGetArray(*mesh,&n,&d,&a); if (*ierr) return;
  *ierr = F90Array2dCreate(a,PETSC_SCALAR,1,d,1,n,ptr PETSC_F90_2PTR_PARAM(ptrd));
}

EXTERN_C_END
