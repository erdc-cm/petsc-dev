
#include "src/fortran/custom/zpetsc.h"
#include "petscsys.h"

#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petsclogprintsummary_     PETSCLOGPRINTSUMMARY
#define petsclogeventbegin_       PETSCLOGEVENTBEGIN
#define petsclogeventend_         PETSCLOGEVENTEND
#define petsclogflops_            PETSCLOGFLOPS
#define petsclogallbegin_         PETSCLOGALLBEGIN
#define petsclogdestroy_          PETSCLOGDESTROY
#define petsclogbegin_            PETSCLOGBEGIN
#define petsclogdump_             PETSCLOGDUMP
#define petsclogeventregister_    PETSCLOGEVENTREGISTER
#define petsclogstagepop_         PETSCLOGSTAGEPOP
#define petsclogstageregister_    PETSCLOGSTAGEREGISTER
#define petsclogclassregister_    PETSCLOGCLASSREGISTER
#define petsclogstagepush_        PETSCLOGSTAGEPUSH
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE)
#define petsclogprintsummary_     petsclogprintsummary
#define petsclogeventbegin_       petsclogeventbegin
#define petsclogeventend_         petsclogeventend
#define petsclogflops_            petsclogflops
#define petsclogallbegin_         petsclogallbegin
#define petsclogdestroy_          petsclogdestroy
#define petsclogbegin_            petsclogbegin
#define petsclogeventregister_    petsclogeventregister
#define petsclogdump_             petsclogdump
#define petsclogstagepop_         petsclogstagepop  
#define petsclogstageregister_    petsclogstageregister
#define petsclogclassregister_    petsclogclassregister
#define petsclogstagepush_        petsclogstagepush
#endif

EXTERN_C_BEGIN

void PETSC_STDCALL petsclogprintsummary_(MPI_Comm *comm,CHAR filename PETSC_MIXED_LEN(len),int *ierr PETSC_END_LEN(len))
{
#if defined(PETSC_USE_LOG)
  char *t;
  FIXCHAR(filename,len,t);
  *ierr = PetscLogPrintSummary((MPI_Comm)PetscToPointerComm(*comm),t);
#endif
}



void PETSC_STDCALL petsclogdump_(CHAR name PETSC_MIXED_LEN(len),int *ierr PETSC_END_LEN(len))
{
#if defined(PETSC_USE_LOG)
  char *t1;
  FIXCHAR(name,len,t1);
  *ierr = PetscLogDump(t1);
  FREECHAR(name,t1);
#endif
}
void PETSC_STDCALL petsclogeventregister_(PetscEvent *e,CHAR string PETSC_MIXED_LEN(len),int *cookie,int *ierr PETSC_END_LEN(len))
{
#if defined(PETSC_USE_LOG)
  char *t1;
  FIXCHAR(string,len,t1);
  *ierr = PetscLogEventRegister(e,t1,*cookie);
  FREECHAR(string,t1);
#endif
}
void PETSC_STDCALL petsclogclassregister_(PetscCookie *e,CHAR string PETSC_MIXED_LEN(len),int *ierr PETSC_END_LEN(len))
{
#if defined(PETSC_USE_LOG)
  char *t1;
  FIXCHAR(string,len,t1);

  *ierr = PetscLogClassRegister(e,t1);
  FREECHAR(string,t1);
#endif
}

void PETSC_STDCALL petsclogallbegin_(int *ierr){
#if defined(PETSC_USE_LOG)
  *ierr = PetscLogAllBegin();
#endif
}

void PETSC_STDCALL petsclogdestroy_(int *ierr){
#if defined(PETSC_USE_LOG)
  *ierr = PetscLogDestroy();
#endif
}

void PETSC_STDCALL petsclogbegin_(int *ierr){
#if defined(PETSC_USE_LOG)
  *ierr = PetscLogBegin();
#endif
}

void PETSC_STDCALL petsclogeventbegin_(PetscEvent *e,int *_ierr){
  *_ierr = PetscLogEventBegin(*e,0,0,0,0);
}

void PETSC_STDCALL petsclogeventend_(PetscEvent *e,int *_ierr){
  *_ierr = PetscLogEventEnd(*e,0,0,0,0);
}

void PETSC_STDCALL petsclogflops_(int *f,int *_ierr) {
  *_ierr = PetscLogFlops(*f);
}

void PETSC_STDCALL petsclogstagepop_(int *ierr)
{
#if defined(PETSC_USE_LOG)
  *ierr = PetscLogStagePop();
#endif
}

void PETSC_STDCALL petsclogstageregister_(int *stage,CHAR sname PETSC_MIXED_LEN(len),int *ierr PETSC_END_LEN(len))
{
#if defined(PETSC_USE_LOG)
  char *t;
  FIXCHAR(sname,len,t);
  *ierr = PetscLogStageRegister(stage,t);
#endif
}

void PETSC_STDCALL petsclogstagepush_(int *stage,int *ierr){
#if defined(PETSC_USE_LOG)
  *ierr = PetscLogStagePush(*stage);
#endif
}

EXTERN_C_END
