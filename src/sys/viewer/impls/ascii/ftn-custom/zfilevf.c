#include "private/zpetsc.h"
#include "petsc.h"

#if defined(PETSC_HAVE_FORTRAN_CAPS)
#define petscviewerfilesetname_                PETSCVIEWERFILESETNAME
#define petscviewerasciiprintf_                PETSCVIEWERASCIIPRINTF
#define petscviewerasciisynchronizedprintf_    PETSCVIEWERASCIISYNCHRONIZEDPRINTF
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE)
#define petscviewerfilesetname_                petscviewerfilesetname
#define petscviewerasciiprintf_                petscviewerasciiprintf
#define petscviewerasciisynchronizedprintf_    petscviewerasciisynchronizedprintf
#endif

EXTERN_C_BEGIN

void PETSC_STDCALL petscviewerfilesetname_(PetscViewer *viewer,CHAR name PETSC_MIXED_LEN(len),
                                      PetscErrorCode *ierr PETSC_END_LEN(len))
{
  char   *c1;
  PetscViewer v;
  PetscPatchDefaultViewers_Fortran(viewer,v);
  FIXCHAR(name,len,c1);
  *ierr = PetscViewerFileSetName(v,c1);
  FREECHAR(name,c1);
}

void PETSC_STDCALL petscviewerasciiprintf_(PetscViewer *viewer,CHAR str PETSC_MIXED_LEN(len1),PetscErrorCode *ierr PETSC_END_LEN(len1))
{
  char *c1;

  FIXCHAR(str,len1,c1);
  *ierr = PetscViewerASCIIPrintf(*viewer,c1);
  FREECHAR(str,c1);
}

void PETSC_STDCALL petscviewerasciisynchronizedprintf_(PetscViewer *viewer,CHAR str PETSC_MIXED_LEN(len1),PetscErrorCode *ierr PETSC_END_LEN(len1))
{
  char *c1;

  FIXCHAR(str,len1,c1);
  *ierr = PetscViewerASCIISynchronizedPrintf(*viewer,c1);
  FREECHAR(str,c1);
}


EXTERN_C_END
