#include <private/fortranimpl.h>
#include <petscdm.h>

#if defined(PETSC_HAVE_FORTRAN_CAPS)
#define dmview_                      DMVIEW
#define dmgetcoloring_               DMGETCOLORING
#define dmsetinitialguess_           DMSETINITIALGUESS
#define dmsetfunction_               DMSETFUNCTION
#define dmsetjacobian_               DMSETJACOBIAN
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE)
#define dmview_                      dmview
#define dmgetcoloring_               dmgetcoloring
#define dmsetinitialguess_           dmsetinitialguess
#define dmsetfunction_               dmsetfunction
#define dmsetjacobian_               dmsetjacobian
#endif

static PetscErrorCode ourdminitialguess(DM dm,Vec x)
{
  PetscErrorCode ierr = 0;
  (*(void (PETSC_STDCALL *)(DM*,Vec*,PetscErrorCode*))(((PetscObject)dm)->fortran_func_pointers[0]))(&dm,&x,&ierr);CHKERRQ(ierr);
  return 0;
}

static PetscErrorCode ourdmfunction(DM dm,Vec x,Vec b)
{
  PetscErrorCode ierr = 0;
  (*(void (PETSC_STDCALL *)(DM*,Vec*,Vec*,PetscErrorCode*))(((PetscObject)dm)->fortran_func_pointers[1]))(&dm,&x,&b,&ierr);CHKERRQ(ierr);
  return 0;
}

static PetscErrorCode ourdmjacobian(DM dm,Vec x,Mat A,Mat B,MatStructure *str)
{
  PetscErrorCode ierr = 0;
  (*(void (PETSC_STDCALL *)(DM*,Vec*,Mat*,Mat*,MatStructure*,PetscErrorCode*))(((PetscObject)dm)->fortran_func_pointers[2]))(&dm,&x,&A,&B,str,&ierr);CHKERRQ(ierr);
  return 0;
}

EXTERN_C_BEGIN
void PETSC_STDCALL  dmsetinitialguess_(DM *dm,PetscErrorCode (*f)(DM*,Vec*,PetscErrorCode*), int *ierr )
{
  PetscObjectAllocateFortranPointers(*dm,12);
  ((PetscObject)*dm)->fortran_func_pointers[0] = (PetscVoidFunction)f;
  *ierr = DMSetInitialGuess(*dm,ourdminitialguess);
}
EXTERN_C_END

EXTERN_C_BEGIN
void PETSC_STDCALL  dmsetfunction_(DM *dm,PetscErrorCode (*f)(DM*,Vec*,Vec*,PetscErrorCode*), int *ierr )
{
  PetscObjectAllocateFortranPointers(*dm,12);
  ((PetscObject)*dm)->fortran_func_pointers[1] = (PetscVoidFunction)f;
  *ierr = DMSetFunction(*dm,ourdmfunction);
}
EXTERN_C_END

EXTERN_C_BEGIN
void PETSC_STDCALL  dmsetjacobian_(DM *dm,PetscErrorCode (*f)(DM*,Vec*,Mat*,Mat*,MatStructure*,PetscErrorCode*), int *ierr )
{
  PetscObjectAllocateFortranPointers(*dm,12);
  ((PetscObject)*dm)->fortran_func_pointers[2] = (PetscVoidFunction)f;
  *ierr = DMSetJacobian(*dm,ourdmjacobian);
}
EXTERN_C_END

EXTERN_C_BEGIN
void PETSC_STDCALL  dmgetcoloring_(DM *dm,ISColoringType *ctype, CHAR mtype PETSC_MIXED_LEN(len),ISColoring *coloring, int *ierr PETSC_END_LEN(len))
{
  char *t;

  FIXCHAR(mtype,len,t);
  *ierr = DMGetColoring(*dm,*ctype,t,coloring);
  FREECHAR(mtype,t);
}
EXTERN_C_END

EXTERN_C_BEGIN
void PETSC_STDCALL dmview_(DM *da,PetscViewer *vin,PetscErrorCode *ierr)
{
  PetscViewer v;
  PetscPatchDefaultViewers_Fortran(vin,v);
  *ierr = DMView(*da,v);
}
EXTERN_C_END
