#include <petsc-private/fortranimpl.h>
#include <petscksp.h>

#if defined(PETSC_HAVE_FORTRAN_CAPS)
#define kspmonitorset_             KSPMONITORSET
#define kspsetconvergencetest_     KSPSETCONVERGENCETEST
#define kspgetresidualhistory_     KSPGETRESIDUALHISTORY

#define kspdefaultconverged_       KSPDEFAULTCONVERGED
#define kspdefaultconvergedcreate_  KSPDEFAULTCONVERGEDCREATE
#define kspdefaultconvergeddestroy_  KSPDEFAULTCONVERGEDDESTROY
#define kspskipconverged_          KSPSKIPCONVERGED
#define kspgmresmonitorkrylov_     KSPGMRESMONITORKRYLOV
#define kspmonitordefault_         KSPMONITORDEFAULT
#define kspmonitortrueresidualnorm_    KSPMONITORTRUERESIDUALNORM
#define kspmonitorsolution_        KSPMONITORSOLUTION
#define kspmonitorlgresidualnorm_      KSPMONITORLGRESIDUALNORM
#define kspmonitorlgtrueresidualnorm_  KSPMONITORLGTRUERESIDUALNORM
#define kspmonitorsingularvalue_   KSPMONITORSINGULARVALUE
#define kspfgmresmodifypcksp_      KSPFGMRESMODIFYPCKSP
#define kspfgmresmodifypcnochange_ KSPFGMRESMODIFYPCNOCHANGE
#define kspsetcomputerhs_              KSPSETCOMPUTERHS
#define kspsetcomputeinitialguess_     KSPSETCOMPUTEINITIALGUESS
#define kspsetcomputeoperators_        KSPSETCOMPUTEOPERATORS
#define dmkspsetcomputerhs_            DMKSPSETCOMPUTERHS          /* zdmkspf.c */
#define dmkspsetcomputeinitialguess_   DMKSPSETCOMPUTEINITIALGUESS /* zdmkspf.c */
#define dmkspsetcomputeoperators_      DMKSPSETCOMPUTEOPERATORS    /* zdmkspf.c */
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE)
#define kspmonitorset_                 kspmonitorset
#define kspsetconvergencetest_         kspsetconvergencetest
#define kspgetresidualhistory_         kspgetresidualhistory
#define kspdefaultconverged_           kspdefaultconverged
#define kspdefaultconvergedcreate_     kspdefaultconvergedcreate
#define kspdefaultconvergeddestroy_    kspdefaultconvergeddestroy
#define kspskipconverged_              kspskipconverged
#define kspmonitorsingularvalue_       kspmonitorsingularvalue
#define kspgmresmonitorkrylov_         kspgmresmonitorkrylov
#define kspmonitordefault_             kspmonitordefault
#define kspmonitortrueresidualnorm_    kspmonitortrueresidualnorm
#define kspmonitorsolution_            kspmonitorsolution
#define kspmonitorlgresidualnorm_      kspmonitorlgresidualnorm
#define kspmonitorlgtrueresidualnorm_  kspmonitorlgtrueresidualnorm
#define kspfgmresmodifypcksp_          kspfgmresmodifypcksp
#define kspfgmresmodifypcnochange_     kspfgmresmodifypcnochange
#define kspsetcomputerhs_              kspsetcomputerhs
#define kspsetcomputeinitialguess_     kspsetcomputeinitialguess
#define kspsetcomputeoperators_        kspsetcomputeoperators
#define dmkspsetcomputerhs_            dmkspsetcomputerhs          /* zdmkspf.c */
#define dmkspsetcomputeinitialguess_   dmkspsetcomputeinitialguess /* zdmkspf.c */
#define dmkspsetcomputeoperators_      dmkspsetcomputeoperators    /* zdmkspf.c */
#endif

/* These are defined in zdmkspf.c */
PETSC_EXTERN_C void PETSC_STDCALL dmkspsetcomputerhs_(DM *dm,void (PETSC_STDCALL *func)(KSP*,Vec*,void*,PetscErrorCode*),void *ctx,PetscErrorCode *ierr);
PETSC_EXTERN_C void PETSC_STDCALL dmkspsetcomputeinitialguess_(DM *dm,void (PETSC_STDCALL *func)(KSP*,Vec*,void*,PetscErrorCode*),void *ctx,PetscErrorCode *ierr);
PETSC_EXTERN_C void PETSC_STDCALL dmkspsetcomputeoperators_(DM *dm,void (PETSC_STDCALL *func)(KSP*,Vec*,void*,PetscErrorCode*),void *ctx,PetscErrorCode *ierr);

EXTERN_C_BEGIN

/*
        These are not usually called from Fortran but allow Fortran users
   to transparently set these monitors from .F code

   functions, hence no STDCALL
*/

void kspdefaultconverged_(KSP *ksp,PetscInt *n,PetscReal *rnorm,KSPConvergedReason *flag,void *dummy,PetscErrorCode *ierr)
{
  CHKFORTRANNULLOBJECT(dummy);
  *ierr = KSPDefaultConverged(*ksp,*n,*rnorm,flag,dummy);
}

void kspskipconverged_(KSP *ksp,PetscInt *n,PetscReal *rnorm,KSPConvergedReason *flag,void *dummy,PetscErrorCode *ierr)
{
  CHKFORTRANNULLOBJECT(dummy);
  *ierr = KSPSkipConverged(*ksp,*n,*rnorm,flag,dummy);
}

void kspgmresmonitorkrylov_(KSP *ksp,PetscInt *it,PetscReal *norm,void *ctx,PetscErrorCode *ierr)
{
  *ierr = KSPGMRESMonitorKrylov(*ksp,*it,*norm,ctx);
}

void  kspmonitordefault_(KSP *ksp,PetscInt *it,PetscReal *norm,void *ctx,PetscErrorCode *ierr)
{
  *ierr = KSPMonitorDefault(*ksp,*it,*norm,ctx);
}

void  kspmonitorsingularvalue_(KSP *ksp,PetscInt *it,PetscReal *norm,void *ctx,PetscErrorCode *ierr)
{
  *ierr = KSPMonitorSingularValue(*ksp,*it,*norm,ctx);
}

void  kspmonitorlgresidualnorm_(KSP *ksp,PetscInt *it,PetscReal *norm,void *ctx,PetscErrorCode *ierr)
{
  *ierr = KSPMonitorLGResidualNorm(*ksp,*it,*norm,ctx);
}

void  kspmonitorlgtrueresidualnorm_(KSP *ksp,PetscInt *it,PetscReal *norm,void *ctx,PetscErrorCode *ierr)
{
  *ierr = KSPMonitorLGTrueResidualNorm(*ksp,*it,*norm,ctx);
}

void  kspmonitortrueresidualnorm_(KSP *ksp,PetscInt *it,PetscReal *norm,void *ctx,PetscErrorCode *ierr)
{
  *ierr = KSPMonitorTrueResidualNorm(*ksp,*it,*norm,ctx);
}

void  kspmonitorsolution_(KSP *ksp,PetscInt *it,PetscReal *norm,void *ctx,PetscErrorCode *ierr)
{
  *ierr = KSPMonitorSolution(*ksp,*it,*norm,ctx);
}

EXTERN_C_END

static struct {
  PetscFortranCallbackId monitor;
  PetscFortranCallbackId monitordestroy;
  PetscFortranCallbackId test;
  PetscFortranCallbackId testdestroy;
} _cb;

static PetscErrorCode ourmonitor(KSP ksp,PetscInt i,PetscReal d,void* ctx)
{
  PetscObjectUseFortranCallback(ksp,_cb.monitor,(KSP*,PetscInt*,PetscReal*,void*,PetscErrorCode*),(&ksp,&i,&d,_ctx,&ierr));
  return 0;
}

static PetscErrorCode ourdestroy(void** ctx)
{
  KSP ksp = (KSP)*ctx;
  PetscObjectUseFortranCallback(ksp,_cb.monitordestroy,(void*,PetscErrorCode*),(_ctx,&ierr));
  return 0;
}

/* These are not extern C because they are passed into non-extern C user level functions */
static PetscErrorCode ourtest(KSP ksp,PetscInt i,PetscReal d,KSPConvergedReason *reason,void* ctx)
{
  PetscObjectUseFortranCallback(ksp,_cb.test,(KSP*,PetscInt*,PetscReal*,KSPConvergedReason*,void*,PetscErrorCode*),(&ksp,&i,&d,reason,_ctx,&ierr));
  return 0;
}
static PetscErrorCode ourtestdestroy(void* ctx)
{
  KSP ksp = (KSP)ctx;
  PetscObjectUseFortranCallback(ksp,_cb.testdestroy,(void*,PetscErrorCode*),(_ctx,&ierr));
  return 0;
}

EXTERN_C_BEGIN
void PETSC_STDCALL kspmonitorset_(KSP *ksp,void (PETSC_STDCALL *monitor)(KSP*,PetscInt*,PetscReal*,void*,PetscErrorCode*),
                    void *mctx,void (PETSC_STDCALL *monitordestroy)(void*,PetscErrorCode*),PetscErrorCode *ierr)
{
  CHKFORTRANNULLOBJECT(mctx);
  CHKFORTRANNULLFUNCTION(monitordestroy);

  if ((PetscVoidFunction)monitor == (PetscVoidFunction)kspmonitordefault_) {
    *ierr = KSPMonitorSet(*ksp,KSPMonitorDefault,0,0);
  } else if ((PetscVoidFunction)monitor == (PetscVoidFunction)kspmonitorlgresidualnorm_) {
    *ierr = KSPMonitorSet(*ksp,KSPMonitorLGResidualNorm,0,0);
  } else if ((PetscVoidFunction)monitor == (PetscVoidFunction)kspmonitorlgtrueresidualnorm_) {
    *ierr = KSPMonitorSet(*ksp,KSPMonitorLGTrueResidualNorm,0,0);
  } else if ((PetscVoidFunction)monitor == (PetscVoidFunction)kspmonitorsolution_) {
    *ierr = KSPMonitorSet(*ksp,KSPMonitorSolution,0,0);
  } else if ((PetscVoidFunction)monitor == (PetscVoidFunction)kspmonitortrueresidualnorm_) {
    *ierr = KSPMonitorSet(*ksp,KSPMonitorTrueResidualNorm,0,0);
  } else if ((PetscVoidFunction)monitor == (PetscVoidFunction)kspmonitorsingularvalue_) {
    *ierr = KSPMonitorSet(*ksp,KSPMonitorSingularValue,0,0);
  } else if ((PetscVoidFunction)monitor == (PetscVoidFunction)kspgmresmonitorkrylov_) {
    *ierr = KSPMonitorSet(*ksp,KSPGMRESMonitorKrylov,0,0);
  } else {
    *ierr = PetscObjectSetFortranCallback((PetscObject)*ksp,PETSC_FORTRAN_CALLBACK_CLASS,&_cb.monitor,(PetscVoidFunction)monitor,mctx); if (*ierr) return;
    if (!monitordestroy) {
      *ierr = KSPMonitorSet(*ksp,ourmonitor,*ksp,0);
    } else {
      *ierr = PetscObjectSetFortranCallback((PetscObject)*ksp,PETSC_FORTRAN_CALLBACK_CLASS,&_cb.monitordestroy,(PetscVoidFunction)monitordestroy,mctx); if (*ierr) return;
      *ierr = KSPMonitorSet(*ksp,ourmonitor,*ksp,ourdestroy);
    }
  }
}

void PETSC_STDCALL kspsetconvergencetest_(KSP *ksp,
      void (PETSC_STDCALL *converge)(KSP*,PetscInt*,PetscReal*,KSPConvergedReason*,void*,PetscErrorCode*),void **cctx,
      void (PETSC_STDCALL *destroy)(void*,PetscErrorCode*),PetscErrorCode *ierr)
{
  CHKFORTRANNULLOBJECT(cctx);
  CHKFORTRANNULLFUNCTION(destroy);

  if ((PetscVoidFunction)converge == (PetscVoidFunction)kspdefaultconverged_) {
    *ierr = KSPSetConvergenceTest(*ksp,KSPDefaultConverged,*cctx,KSPDefaultConvergedDestroy);
  } else if ((PetscVoidFunction)converge == (PetscVoidFunction)kspskipconverged_) {
    *ierr = KSPSetConvergenceTest(*ksp,KSPSkipConverged,0,0);
  } else {
    *ierr = PetscObjectSetFortranCallback((PetscObject)*ksp,PETSC_FORTRAN_CALLBACK_CLASS,&_cb.test,(PetscVoidFunction)converge,cctx); if (*ierr) return;
    if (!destroy) {
      *ierr = KSPSetConvergenceTest(*ksp,ourtest,*ksp,0);
    } else {
      *ierr = PetscObjectSetFortranCallback((PetscObject)*ksp,PETSC_FORTRAN_CALLBACK_CLASS,&_cb.testdestroy,(PetscVoidFunction)destroy,cctx); if (*ierr) return;
      *ierr = KSPSetConvergenceTest(*ksp,ourtest,*ksp,ourtestdestroy);
    }
  }
}

void PETSC_STDCALL kspdefaultconvergedcreate_(PetscFortranAddr *ctx,PetscErrorCode *ierr)
{
  *ierr = KSPDefaultConvergedCreate((void**)ctx);
}

void PETSC_STDCALL kspdefaultconvergeddestroy_(PetscFortranAddr *ctx,PetscErrorCode *ierr)
{
  *ierr = KSPDefaultConvergedDestroy(*(void**)ctx);
}

void PETSC_STDCALL kspgetresidualhistory_(KSP *ksp,PetscInt *na,PetscErrorCode *ierr)
{
  *ierr = KSPGetResidualHistory(*ksp,PETSC_NULL,na);
}

void PETSC_STDCALL kspsetcomputerhs_(KSP *ksp,void (PETSC_STDCALL *func)(KSP*,Vec*,void*,PetscErrorCode*),void *ctx,PetscErrorCode *ierr)
{
  DM dm;
  *ierr = KSPGetDM(*ksp,&dm);
  if (!*ierr) dmkspsetcomputerhs_(&dm,func,ctx,ierr);
}

void PETSC_STDCALL kspsetcomputeinitialguess_(KSP *ksp,void (PETSC_STDCALL *func)(KSP*,Vec*,void*,PetscErrorCode*),void *ctx,PetscErrorCode *ierr)
{
  DM dm;
  *ierr = KSPGetDM(*ksp,&dm);
  if (!*ierr) dmkspsetcomputeinitialguess_(&dm,func,ctx,ierr);
}

void PETSC_STDCALL kspsetcomputeoperators_(KSP *ksp,void (PETSC_STDCALL *func)(KSP*,Vec*,void*,PetscErrorCode*),void *ctx,PetscErrorCode *ierr)
{
  DM dm;
  *ierr = KSPGetDM(*ksp,&dm);
  if (!*ierr) dmkspsetcomputeoperators_(&dm,func,ctx,ierr);
}

EXTERN_C_END
