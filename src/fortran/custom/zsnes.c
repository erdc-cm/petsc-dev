/*$Id: zsnes.c,v 1.63 2001/08/31 16:15:30 bsmith Exp $*/

#include "src/fortran/custom/zpetsc.h"
#include "petscsnes.h"
#include "petscda.h"

#ifdef PETSC_HAVE_FORTRAN_UNDERSCORE_UNDERSCORE
#define snesconverged_eq_tr_         snesconverged_eq_tr__
#define snesconverged_eq_ls_         snesconverged_eq_ls__
#define snesconverged_um_tr_         snesconverged_um_tr__
#define snesconverged_um_ls_         snesconverged_um_ls__
#endif

#ifdef PETSC_HAVE_FORTRAN_CAPS
#define matcreatedaad_                   MATCREATEDAAD
#define matregisterdaad_                   MATREGISTERDAAD
#define matdaadsetsnes_                  MATDAADSETSNES
#define snesdacomputejacobian_           SNESDACOMPUTEJACOBIAN
#define snesdacomputejacobianwithadifor_ SNESDACOMPUTEJACOBIANWITHADIFOR
#define snesdaformfunction_          SNESDAFORMFUNCTION          
#define matsnesmfsetbase_            MATSNESMFSETBASE
#define snesconverged_eq_tr_         SNESCONVERGED_EQ_TR
#define snesconverged_eq_ls_         SNESCONVERGED_EQ_LS
#define snesconverged_um_tr_         SNESCONVERGED_UM_TR
#define snesconverged_um_ls_         SNESCONVERGED_UM_LS
#define snesgetconvergedreason_      SNESGETCONVERGEDREASON
#define snesdefaultmonitor_          SNESDEFAULTMONITOR
#define snesvecviewmonitor_          SNESVECVIEWMONITOR
#define sneslgmonitor_               SNESLGMONITOR
#define snesvecviewupdatemonitor_    SNESVECVIEWUPDATEMONITOR
#define snesregisterdestroy_         SNESREGISTERDESTROY
#define snessetjacobian_             SNESSETJACOBIAN
#define snescreate_                  SNESCREATE
#define snessetfunction_             SNESSETFUNCTION
#define snessetminimizationfunction_ SNESSETMINIMIZATIONFUNCTION
#define snesgetsles_                 SNESGETSLES
#define snessetgradient_             SNESSETGRADIENT
#define snessethessian_              SNESSETHESSIAN
#define snessetmonitor_              SNESSETMONITOR
#define snessetconvergencetest_      SNESSETCONVERGENCETEST
#define snesregisterdestroy_         SNESREGISTERDESTROY
#define snesgetsolution_             SNESGETSOLUTION
#define snesgetsolutionupdate_       SNESGETSOLUTIONUPDATE
#define snesgetfunction_             SNESGETFUNCTION
#define snesgetminimizationfunction_ SNESGETMINIMIZATIONFUNCTION
#define snesgetgradient_             SNESGETGRADIENT
#define snesdestroy_                 SNESDESTROY
#define snesgettype_                 SNESGETTYPE
#define snessetoptionsprefix_        SNESSETOPTIONSPREFIX 
#define snesappendoptionsprefix_     SNESAPPENDOPTIONSPREFIX 
#define matcreatesnesmf_             MATCREATESNESMF
#define matcreatemf_                 MATCREATEMF
#define snessettype_                 SNESSETTYPE
#define snesgetconvergencehistory_   SNESGETCONVERGENCEHISTORY
#define snesdefaultcomputejacobian_  SNESDEFAULTCOMPUTEJACOBIAN
#define snesdefaultcomputejacobiancolor_ SNESDEFAULTCOMPUTEJACOBIANCOLOR
#define matsnesmfsettype_                MATSNESMFSETTYPE
#define snesgetoptionsprefix_            SNESGETOPTIONSPREFIX
#define snesgetjacobian_                 SNESGETJACOBIAN
#define matsnesmfsetfunction_            MATSNESMFSETFUNCTION
#define snessetlinesearchparams_         SNESSETLINESEARCHPARAMS
#define snesgetlinesearchparams_         SNESGETLINESEARCHPARAMS
#define snessetlinesearch_               SNESSETLINESEARCH
#define snescubiclinesearch_             SNESCUBICLINESEARCH
#define snesquadraticlinesearch_         SNESQUADRATICLINESEARCH
#define snesnolinesearch_                SNESNOLINESEARCH
#define snesnolinesearchnonorms_         SNESNOLINESEARCHNONORMS
#define snesview_                        SNESVIEW
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE)
#define matcreatedaad_                   matcreatedaad
#define matregisterdaad_                   matregisterdaad
#define matdaadsetsnes_                  matdaadsetsnes
#define snesdacomputejacobian_           snesdacomputejacobian
#define snesdacomputejacobianwithadifor_ snesdacomputejacobianwithadifor
#define snesdaformfunction_          snesdaformfunction
#define matsnesmfsetbase_            matsnesmfsetbase
#define snescubiclinesearch_         snescubiclinesearch     
#define snesquadraticlinesearch_     snesquadraticlinesearch    
#define snesnolinesearch_            snesnolinesearch    
#define snesnolinesearchnonorms_     snesnolinesearchnonorms    
#define snessetlinesearch_           snessetlinesearch
#define snesconverged_eq_tr_         snesconverged_eq_tr
#define snesconverged_eq_ls_         snesconverged_eq_ls
#define snesconverged_um_tr_         snesconverged_um_tr
#define snesconverged_um_ls_         snesconverged_um_ls
#define snesgetconvergedreason_      snesgetconvergedreason
#define sneslgmonitor_               sneslgmonitor
#define snesdefaultmonitor_          snesdefaultmonitor
#define snesvecviewmonitor_          snesvecviewmonitor
#define snesvecviewupdatemonitor_    snesvecviewupdatemonitor
#define matsnesmfsetfunction_        matsnesmfsetfunction
#define snesregisterdestroy_         snesregisterdestroy
#define snessetjacobian_             snessetjacobian
#define snescreate_                  snescreate
#define snessetfunction_             snessetfunction
#define snessethessian_              snessethessian
#define snessetgradient_             snessetgradient
#define snesgetsles_                 snesgetsles
#define snessetminimizationfunction_ snessetminimizationfunction
#define snesdestroy_                 snesdestroy
#define snessetmonitor_              snessetmonitor
#define snessetconvergencetest_      snessetconvergencetest
#define snesregisterdestroy_         snesregisterdestroy
#define snesgetsolution_             snesgetsolution
#define snesgetsolutionupdate_       snesgetsolutionupdate
#define snesgetfunction_             snesgetfunction
#define snesgetminimizationfunction_ snesgetminimizationfunction
#define snesgetgradient_             snesgetgradient
#define snesgettype_                 snesgettype
#define snessetoptionsprefix_        snessetoptionsprefix 
#define snesappendoptionsprefix_     snesappendoptionsprefix
#define matcreatesnesmf_             matcreatesnesmf
#define matcreatemf_                 matcreatemf
#define snessettype_                 snessettype
#define snesgetconvergencehistory_   snesgetconvergencehistory
#define snesdefaultcomputejacobian_  snesdefaultcomputejacobian
#define snesdefaultcomputejacobiancolor_ snesdefaultcomputejacobiancolor
#define matsnesmfsettype_                matsnesmfsettype
#define snesgetoptionsprefix_            snesgetoptionsprefix
#define snesgetjacobian_                 snesgetjacobian
#define snessetlinesearchparams_         snessetlinesearchparams
#define snesgetlinesearchparams_         snesgetlinesearchparams
#define snesview_                        snesview
#endif

EXTERN_C_BEGIN

#if defined (PETSC_HAVE_ADIC) && !defined(PETSC_USE_COMPLEX)
void PETSC_STDCALL matregisterdaad_(int *ierr)
{
  *ierr = MatRegisterDAAD();
}

void PETSC_STDCALL matcreatedaad_(DA *da,Mat *mat,int *ierr)
{
  *ierr = MatCreateDAAD(*da,mat);
}

void PETSC_STDCALL matdaadsetsnes_(Mat *mat,SNES *snes,int *ierr)
{
  *ierr = MatDAADSetSNES(*mat,*snes);
}
#endif

void PETSC_STDCALL snesview_(SNES *snes,PetscViewer *viewer, int *ierr)
{
  PetscViewer v;
  PetscPatchDefaultViewers_Fortran(viewer,v);
  *ierr = SNESView(*snes,v);
}

void PETSC_STDCALL snesgetconvergedreason(SNES *snes,SNESConvergedReason *r,int *ierr)
{
  *ierr = SNESGetConvergedReason(*snes,r);
}

void PETSC_STDCALL snessetlinesearchparams_(SNES *snes,PetscReal *alpha,PetscReal *maxstep,PetscReal *steptol,int *ierr)
{
  *ierr = SNESSetLineSearchParams(*snes,*alpha,*maxstep,*steptol);
}

void PETSC_STDCALL snesgetlinesearchparams_(SNES *snes,PetscReal *alpha,PetscReal *maxstep,PetscReal *steptol,int *ierr)
{
  if (FORTRANNULLREAL(alpha)) alpha = PETSC_NULL;
  if (FORTRANNULLREAL(maxstep)) maxstep = PETSC_NULL;
  if (FORTRANNULLREAL(steptol)) steptol = PETSC_NULL;
  *ierr = SNESGetLineSearchParams(*snes,alpha,maxstep,steptol);
}

/*  func is currently ignored from Fortran */
void PETSC_STDCALL snesgetjacobian_(SNES *snes,Mat *A,Mat *B,void **ctx,int *func,int *ierr)
{
  if (FORTRANNULLINTEGER(ctx)) ctx = PETSC_NULL;
  if (FORTRANNULLOBJECT(A))    A = PETSC_NULL;
  if (FORTRANNULLOBJECT(B))    B = PETSC_NULL;
  *ierr = SNESGetJacobian(*snes,A,B,ctx,0);
}

void PETSC_STDCALL matsnesmfsettype_(Mat *mat,CHAR ftype PETSC_MIXED_LEN(len),
                                     int *ierr PETSC_END_LEN(len))
{
  char *t;
  FIXCHAR(ftype,len,t);
  *ierr = MatSNESMFSetType(*mat,t);
  FREECHAR(ftype,t);
}

void PETSC_STDCALL snesgetconvergencehistory_(SNES *snes,int *na,int *ierr)
{
  *ierr = SNESGetConvergenceHistory(*snes,PETSC_NULL,PETSC_NULL,na);
}

void PETSC_STDCALL snessettype_(SNES *snes,CHAR type PETSC_MIXED_LEN(len),
                                int *ierr PETSC_END_LEN(len))
{
  char *t;

  FIXCHAR(type,len,t);
  *ierr = SNESSetType(*snes,t);
  FREECHAR(type,t);
}

void PETSC_STDCALL snesappendoptionsprefix_(SNES *snes,CHAR prefix PETSC_MIXED_LEN(len),
                                            int *ierr PETSC_END_LEN(len))
{
  char *t;

  FIXCHAR(prefix,len,t);
  *ierr = SNESAppendOptionsPrefix(*snes,t);
  FREECHAR(prefix,t);
}

void PETSC_STDCALL matsnesmfsetbase_(Mat *m,Vec *x,int *ierr)
{
  *ierr = MatSNESMFSetBase(*m,*x);
}

void PETSC_STDCALL matcreatesnesmf_(SNES *snes,Vec *x,Mat *J,int *ierr)
{
  *ierr = MatCreateSNESMF(*snes,*x,J);
}

void PETSC_STDCALL matcreatemf_(Vec *x,Mat *J,int *ierr)
{
  *ierr = MatCreateMF(*x,J);
}

/* functions, hence no STDCALL */

void sneslgmonitor_(SNES *snes,int *its,PetscReal *fgnorm,void *dummy,int *ierr)
{
  *ierr = SNESLGMonitor(*snes,*its,*fgnorm,dummy);
}

void snesdefaultmonitor_(SNES *snes,int *its,PetscReal *fgnorm,void *dummy,int *ierr)
{
  *ierr = SNESDefaultMonitor(*snes,*its,*fgnorm,dummy);
}

void snesvecviewmonitor_(SNES *snes,int *its,PetscReal *fgnorm,void *dummy,int *ierr)
{
  *ierr = SNESVecViewMonitor(*snes,*its,*fgnorm,dummy);
}

void snesvecviewupdatemonitor_(SNES *snes,int *its,PetscReal *fgnorm,void *dummy,int *ierr)
{
  *ierr = SNESVecViewUpdateMonitor(*snes,*its,*fgnorm,dummy);
}

static void (PETSC_STDCALL *f7)(SNES*,int*,PetscReal*,void*,int*);
static int oursnesmonitor(SNES snes,int i,PetscReal d,void*ctx)
{
  int              ierr = 0;

  (*f7)(&snes,&i,&d,ctx,&ierr);CHKERRQ(ierr);
  return 0;
}
static void (PETSC_STDCALL *f71)(void*,int*);
static int ourmondestroy(void* ctx)
{
  int              ierr = 0;

  (*f71)(ctx,&ierr);CHKERRQ(ierr);
  return 0;
}

void PETSC_STDCALL snessetmonitor_(SNES *snes,void (PETSC_STDCALL *func)(SNES*,int*,PetscReal*,void*,int*),
                    void *mctx,void (PETSC_STDCALL *mondestroy)(void *,int *),int *ierr)
{
  if (FORTRANNULLOBJECT(mctx)) mctx = PETSC_NULL;
  if ((void(*)(void))func == (void(*)(void))snesdefaultmonitor_) {
    *ierr = SNESSetMonitor(*snes,SNESDefaultMonitor,0,0);
  } else if ((void(*)(void))func == (void(*)(void))snesvecviewmonitor_) {
    *ierr = SNESSetMonitor(*snes,SNESVecViewMonitor,0,0);
  } else if ((void(*)(void))func == (void(*)(void))snesvecviewupdatemonitor_) {
    *ierr = SNESSetMonitor(*snes,SNESVecViewUpdateMonitor,0,0);
  } else if ((void(*)(void))func == (void(*)(void))sneslgmonitor_) {
    *ierr = SNESSetMonitor(*snes,SNESLGMonitor,0,0);
  } else {
    f7 = func;
    if (FORTRANNULLFUNCTION(mondestroy)){
      *ierr = SNESSetMonitor(*snes,oursnesmonitor,mctx,0);
    } else {
      f71 = mondestroy;
      *ierr = SNESSetMonitor(*snes,oursnesmonitor,mctx,ourmondestroy);
    }
  }
}

/* -----------------------------------------------------------------------------------------------------*/
void snescubiclinesearch_(SNES *snes,void *lsctx,Vec *x,Vec *f,Vec *g,Vec *y,Vec *w,PetscReal*fnorm,
                                        PetscReal *ynorm,PetscReal *gnorm,int *flag,int *ierr)
{
  *ierr = SNESCubicLineSearch(*snes,lsctx,*x,*f,*g,*y,*w,*fnorm,ynorm,gnorm,flag);
}
void snesquadraticlinesearch_(SNES *snes,void *lsctx,Vec *x,Vec *f,Vec *g,Vec *y,Vec *w,PetscReal*fnorm,
                                        PetscReal *ynorm,PetscReal *gnorm,int *flag,int *ierr)
{
  *ierr = SNESQuadraticLineSearch(*snes,lsctx,*x,*f,*g,*y,*w,*fnorm,ynorm,gnorm,flag);
}
void snesnolinesearch_(SNES *snes,void *lsctx,Vec *x,Vec *f,Vec *g,Vec *y,Vec *w,PetscReal*fnorm,
                                        PetscReal *ynorm,PetscReal *gnorm,int *flag,int *ierr)
{
  *ierr = SNESNoLineSearch(*snes,lsctx,*x,*f,*g,*y,*w,*fnorm,ynorm,gnorm,flag);
}
void snesnolinesearchnonorms_(SNES *snes,void *lsctx,Vec *x,Vec *f,Vec *g,Vec *y,Vec *w,PetscReal*fnorm,
                                        PetscReal *ynorm,PetscReal *gnorm,int *flag,int *ierr)
{
  *ierr = SNESNoLineSearchNoNorms(*snes,lsctx,*x,*f,*g,*y,*w,*fnorm,ynorm,gnorm,flag);
}

void (PETSC_STDCALL *f73)(SNES*,void *,Vec*,Vec*,Vec*,Vec*,Vec*,PetscReal*,PetscReal*,PetscReal*,int*,int*);
int OurSNESLineSearch(SNES snes,void *ctx,Vec x,Vec f,Vec g,Vec y,Vec w,PetscReal fnorm,PetscReal*ynorm,PetscReal*gnorm,int *flag)
{
  int ierr = 0;
  (*f73)(&snes,(void*)&ctx,&x,&f,&g,&y,&w,&fnorm,ynorm,gnorm,flag,&ierr);CHKERRQ(ierr);
  return 0;
}

void PETSC_STDCALL snessetlinesearch_(SNES *snes,void (PETSC_STDCALL *f)(SNES*,void *,Vec*,Vec*,Vec*,Vec*,Vec*,PetscReal*,PetscReal*,PetscReal*,int*,int*),void *ctx,int *ierr)
{
  if ((void(*)(void))f == (void(*)(void))snescubiclinesearch_) {
    *ierr = SNESSetLineSearch(*snes,SNESCubicLineSearch,0);
  } else if ((void(*)(void))f == (void(*)(void))snesquadraticlinesearch_) {
    *ierr = SNESSetLineSearch(*snes,SNESQuadraticLineSearch,0);
  } else if ((void(*)(void))f == (void(*)(void))snesnolinesearch_) {
    *ierr = SNESSetLineSearch(*snes,SNESNoLineSearch,0);
  } else if ((void(*)(void))f == (void(*)(void))snesnolinesearchnonorms_) {
    *ierr = SNESSetLineSearch(*snes,SNESNoLineSearchNoNorms,0);
  } else {
    f73 = f;
    *ierr = SNESSetLineSearch(*snes,OurSNESLineSearch,0);
  }
}
  

/*--------------------------------------------------------------------------------------------*/
void snesconverged_um_ls_(SNES *snes,PetscReal *a,PetscReal *b,PetscReal *c,SNESConvergedReason *r,
                                       void *ct,int *ierr)
{
  *ierr = SNESConverged_UM_LS(*snes,*a,*b,*c,r,ct);
}

void snesconverged_um_tr_(SNES *snes,PetscReal *a,PetscReal *b,PetscReal *c,SNESConvergedReason *r,
                                       void *ct,int *ierr)
{
  *ierr = SNESConverged_UM_TR(*snes,*a,*b,*c,r,ct);
}

void snesconverged_eq_tr_(SNES *snes,PetscReal *a,PetscReal *b,PetscReal *c,SNESConvergedReason *r,
                                       void *ct,int *ierr)
{
  *ierr = SNESConverged_EQ_TR(*snes,*a,*b,*c,r,ct);
}

void snesconverged_eq_ls_(SNES *snes,PetscReal *a,PetscReal *b,PetscReal *c,SNESConvergedReason *r,
                                       void *ct,int *ierr)
{
  *ierr = SNESConverged_EQ_LS(*snes,*a,*b,*c,r,ct);
}

static void (PETSC_STDCALL *f8)(SNES*,PetscReal*,PetscReal*,PetscReal*,SNESConvergedReason*,void*,int*);
static int oursnestest(SNES snes,PetscReal a,PetscReal d,PetscReal c,SNESConvergedReason*reason,void*ctx)
{
  int              ierr = 0;

  (*f8)(&snes,&a,&d,&c,reason,ctx,&ierr);CHKERRQ(ierr);
  return 0;
}

void PETSC_STDCALL snessetconvergencetest_(SNES *snes,
       void (PETSC_STDCALL *func)(SNES*,PetscReal*,PetscReal*,PetscReal*,SNESConvergedReason*,void*,int*),
       void *cctx,int *ierr)
{
  if (FORTRANNULLOBJECT(cctx)) cctx = PETSC_NULL;
  if ((void(*)(void))func == (void(*)(void))snesconverged_eq_ls_){
    *ierr = SNESSetConvergenceTest(*snes,SNESConverged_EQ_LS,0);
  } else if ((void(*)(void))func == (void(*)(void))snesconverged_eq_tr_){
    *ierr = SNESSetConvergenceTest(*snes,SNESConverged_EQ_TR,0);
  } else if ((void(*)(void))func == (void(*)(void))snesconverged_um_tr_){
    *ierr = SNESSetConvergenceTest(*snes,SNESConverged_UM_TR,0);
  } else if ((void(*)(void))func == (void(*)(void))snesconverged_um_ls_){
    *ierr = SNESSetConvergenceTest(*snes,SNESConverged_UM_LS,0);
  } else {
    f8 = func;
    *ierr = SNESSetConvergenceTest(*snes,oursnestest,cctx);
  }
}

/*--------------------------------------------------------------------------------------------*/

void PETSC_STDCALL snesgetsolution_(SNES *snes,Vec *x,int *ierr)
{
  *ierr = SNESGetSolution(*snes,x);
}

void PETSC_STDCALL snesgetsolutionupdate_(SNES *snes,Vec *x,int *ierr)
{
  *ierr = SNESGetSolutionUpdate(*snes,x);
}

/* the func argument is ignored */
void PETSC_STDCALL snesgetfunction_(SNES *snes,Vec *r,void **ctx,void *func,int *ierr)
{
  if (FORTRANNULLINTEGER(ctx)) ctx = PETSC_NULL;
  if (FORTRANNULLINTEGER(r))   r   = PETSC_NULL;
  *ierr = SNESGetFunction(*snes,r,ctx,PETSC_NULL);
}

void PETSC_STDCALL snesgetminimizationfunction_(SNES *snes,PetscReal *r,void **ctx,int *ierr)
{
  if (FORTRANNULLINTEGER(ctx)) ctx = PETSC_NULL;
  if (FORTRANNULLREAL(r))    r   = PETSC_NULL;
  *ierr = SNESGetMinimizationFunction(*snes,r,ctx);
}

void PETSC_STDCALL snesgetgradient_(SNES *snes,Vec *r,void **ctx,int *ierr)
{
  if (FORTRANNULLINTEGER(ctx)) ctx = PETSC_NULL;
  if (FORTRANNULLINTEGER(r))   r   = PETSC_NULL;
  *ierr = SNESGetGradient(*snes,r,ctx);
}

void PETSC_STDCALL snesdestroy_(SNES *snes,int *ierr)
{
  *ierr = SNESDestroy(*snes);
}

void PETSC_STDCALL snesgetsles_(SNES *snes,SLES *sles,int *ierr)
{
  *ierr = SNESGetSLES(*snes,sles);
}

static void (PETSC_STDCALL *f6)(SNES *,Vec *,Mat *,Mat *,int*,void*,int*);
static int oursneshessianfunction(SNES snes,Vec x,Mat* mat,Mat* pmat,
                                  MatStructure* st,void *ctx)
{
  int              ierr = 0;

  (*f6)(&snes,&x,mat,pmat,(int*)st,ctx,&ierr);CHKERRQ(ierr);

  return 0;
}

void PETSC_STDCALL snessethessian_(SNES *snes,Mat *A,Mat *B,void (PETSC_STDCALL *func)(SNES*,Vec*,Mat*,Mat*,int*,void*,int*),
                     void *ctx,int *ierr)
{
  f6 = func;
  if (FORTRANNULLOBJECT(ctx)) ctx = PETSC_NULL;
  *ierr = SNESSetHessian(*snes,*A,*B,oursneshessianfunction,ctx);
}

static void (PETSC_STDCALL *f5)(SNES*,Vec*,Vec *,void*,int*);
static int oursnesgradientfunction(SNES snes,Vec x,Vec d,void *ctx)
{
  int ierr = 0;
  (*f5)(&snes,&x,&d,ctx,&ierr);CHKERRQ(ierr);
  return 0;
}

void PETSC_STDCALL snessetgradient_(SNES *snes,Vec *r,void (PETSC_STDCALL *func)(SNES*,Vec*,Vec*,void*,int*),void *ctx,int *ierr)
{
  if (FORTRANNULLOBJECT(ctx)) ctx = PETSC_NULL;
  f5 = func;
  *ierr = SNESSetGradient(*snes,*r,oursnesgradientfunction,ctx);
}

static void (PETSC_STDCALL *f4)(SNES*,Vec*,PetscReal*,void*,int*);
static int oursnesminfunction(SNES snes,Vec x,PetscReal* d,void *ctx)
{
  int ierr = 0;
  (*f4)(&snes,&x,d,ctx,&ierr);CHKERRQ(ierr);
  return 0;
}

void PETSC_STDCALL snessetminimizationfunction_(SNES *snes,
          void (PETSC_STDCALL *func)(SNES*,Vec*,PetscReal*,void*,int*),void *ctx,int *ierr){
  f4 = func;
  *ierr = SNESSetMinimizationFunction(*snes,oursnesminfunction,ctx);
}

/* ---------------------------------------------------------*/

static void (PETSC_STDCALL *f2)(SNES*,Vec*,Vec*,void*,int*);
static int oursnesfunction(SNES snes,Vec x,Vec f,void *ctx)
{
  int ierr = 0;
  (*f2)(&snes,&x,&f,ctx,&ierr);CHKERRQ(ierr);
  return 0;
}

/*
        These are not usually called from Fortran but allow Fortran users 
   to transparently set these monitors from .F code
   
   functions, hence no STDCALL
*/
void  snesdaformfunction_(SNES *snes,Vec *X, Vec *F,void *ptr,int *ierr)
{
  *ierr = SNESDAFormFunction(*snes,*X,*F,ptr);
}


void PETSC_STDCALL snessetfunction_(SNES *snes,Vec *r,void (PETSC_STDCALL *func)(SNES*,Vec*,Vec*,void*,int*),
                      void *ctx,int *ierr)
{
  if (FORTRANNULLOBJECT(ctx)) ctx = PETSC_NULL;
   f2 = func;
   if ((void(*)(void))func == (void(*)(void))snesdaformfunction_) {
     *ierr = SNESSetFunction(*snes,*r,SNESDAFormFunction,ctx);
   } else {
     *ierr = SNESSetFunction(*snes,*r,oursnesfunction,ctx);
   }
}

/* ---------------------------------------------------------*/

static void (PETSC_STDCALL *f11)(SNES*,Vec*,Vec*,void*,int*);
static int ourmatsnesmffunction(SNES snes,Vec x,Vec f,void *ctx)
{
  int ierr = 0;
  (*f11)(&snes,&x,&f,ctx,&ierr);CHKERRQ(ierr);
  return 0;
}
void PETSC_STDCALL matsnesmfsetfunction_(Mat *mat,Vec *r,void (PETSC_STDCALL *func)(SNES*,Vec*,Vec*,void*,int*),
                      void *ctx,int *ierr){
   f11 = func;
   if (FORTRANNULLOBJECT(ctx)) ctx = PETSC_NULL;
   *ierr = MatSNESMFSetFunction(*mat,*r,ourmatsnesmffunction,ctx);
}
/* ---------------------------------------------------------*/

void PETSC_STDCALL snescreate_(MPI_Comm *comm,SNESProblemType *type,SNES *outsnes,int *ierr){

*ierr = SNESCreate((MPI_Comm)PetscToPointerComm(*comm),*type,outsnes);
}

/* ---------------------------------------------------------*/
/*
     snesdefaultcomputejacobian() and snesdefaultcomputejacobiancolor()
  These can be used directly from Fortran but are mostly so that 
  Fortran SNESSetJacobian() will properly handle the defaults being passed in.

  functions, hence no STDCALL
*/
void snesdefaultcomputejacobian_(SNES *snes,Vec *x,Mat *m,Mat *p,MatStructure* type,void *ctx,int *ierr)
{
  *ierr = SNESDefaultComputeJacobian(*snes,*x,m,p,type,ctx);
}
void  snesdefaultcomputejacobiancolor_(SNES *snes,Vec *x,Mat *m,Mat *p,MatStructure* type,void *ctx,int *ierr)
{
  *ierr = SNESDefaultComputeJacobianColor(*snes,*x,m,p,type,*(MatFDColoring*)ctx);
}

void  snesdacomputejacobianwithadifor_(SNES *snes,Vec *X,Mat *m,Mat *p,MatStructure* type,void *ctx,int *ierr) 
{
  (*PetscErrorPrintf)("Cannot call this function from Fortran");
  *ierr = 1;
}

void  snesdacomputejacobian_(SNES *snes,Vec *X,Mat *m,Mat *p,MatStructure* type,void *ctx,int *ierr) 
{
  (*PetscErrorPrintf)("Cannot call this function from Fortran");
  *ierr = 1;
}

static void (PETSC_STDCALL *f3)(SNES*,Vec*,Mat*,Mat*,MatStructure*,void*,int*);
static int oursnesjacobian(SNES snes,Vec x,Mat* m,Mat* p,MatStructure* type,void*ctx)
{
  int              ierr = 0;
  (*f3)(&snes,&x,m,p,type,ctx,&ierr);CHKERRQ(ierr);
  return 0;
}

void PETSC_STDCALL snessetjacobian_(SNES *snes,Mat *A,Mat *B,void (PETSC_STDCALL *func)(SNES*,Vec*,Mat*,Mat*,
            MatStructure*,void*,int*),void *ctx,int *ierr)
{
  if (FORTRANNULLOBJECT(ctx)) ctx = PETSC_NULL;
  if ((void(*)(void))func == (void(*)(void))snesdefaultcomputejacobian_) {
    *ierr = SNESSetJacobian(*snes,*A,*B,SNESDefaultComputeJacobian,ctx);
  } else if ((void(*)(void))func == (void(*)(void))snesdefaultcomputejacobiancolor_) {
    *ierr = SNESSetJacobian(*snes,*A,*B,SNESDefaultComputeJacobianColor,*(MatFDColoring*)ctx);
  } else if ((void(*)(void))func == (void(*)(void))snesdacomputejacobianwithadifor_) {
    *ierr = SNESSetJacobian(*snes,*A,*B,SNESDAComputeJacobianWithAdifor,ctx);
  } else if ((void(*)(void))func == (void(*)(void))snesdacomputejacobian_) {
    *ierr = SNESSetJacobian(*snes,*A,*B,SNESDAComputeJacobian,ctx);
  } else {
    f3 = func;
    *ierr = SNESSetJacobian(*snes,*A,*B,oursnesjacobian,ctx);
  }
}

/* -------------------------------------------------------------*/

void PETSC_STDCALL snesregisterdestroy_(int *ierr)
{
  *ierr = SNESRegisterDestroy();
}

void PETSC_STDCALL snesgettype_(SNES *snes,CHAR name PETSC_MIXED_LEN(len),
                                int *ierr PETSC_END_LEN(len))
{
  char *tname;

  *ierr = SNESGetType(*snes,&tname);
#if defined(PETSC_USES_CPTOFCD)
  {
    char *t = _fcdtocp(name); int len1 = _fcdlen(name);
    *ierr = PetscStrncpy(t,tname,len1);if (*ierr) return;
  }
#else
  *ierr = PetscStrncpy(name,tname,len);if (*ierr) return;
#endif
}

void PETSC_STDCALL snesgetoptionsprefix_(SNES *snes,CHAR prefix PETSC_MIXED_LEN(len),
                                         int *ierr PETSC_END_LEN(len))
{
  char *tname;

  *ierr = SNESGetOptionsPrefix(*snes,&tname);
#if defined(PETSC_USES_CPTOFCD)
  {
    char *t = _fcdtocp(prefix); int len1 = _fcdlen(prefix);
    *ierr = PetscStrncpy(t,tname,len1);if (*ierr) return;
  }
#else
  *ierr = PetscStrncpy(prefix,tname,len);if (*ierr) return;
#endif
}

EXTERN_C_END


