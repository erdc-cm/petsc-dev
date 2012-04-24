#include <petsc-private/threadcommimpl.h>      /*I "petscthreadcomm.h" I*/

PetscClassId PETSCTHREADCOMM_CLASSID;

PetscInt N_CORES;

PetscBool  PetscThreadCommRegisterAllCalled = PETSC_FALSE;
PetscBool  PetscGetNCoresCalled             = PETSC_FALSE;
PetscFList PetscThreadCommList              = PETSC_NULL;

PetscThreadComm PETSC_THREAD_COMM_WORLD;

#undef __FUNCT__
#define __FUNCT__ "PetscGetNCores"
/*
  PetscGetNCores - Gets the number of availalbe cores
                   on the system
		   
  Level: developer

  Notes
  Defaults to 1 if the available cores cannot be found
*/
PetscErrorCode PetscGetNCores(void)
{
  PetscFunctionBegin;
  N_CORES=1; /* Default value if N_CORES cannot be found out */
  /* Find the number of cores */
#if defined(PETSC_HAVE_SCHED_CPU_SET_T) /* Linux */
  N_CORES = get_nprocs();
#elif defined(PETSC_HAVE_SYS_SYSCTL_H) /* MacOS, BSD */
  {
    PetscErrorCode ierr;
    size_t   len = sizeof(N_CORES);
    ierr = sysctlbyname("hw.activecpu",&N_CORES,&len,NULL,0);CHKERRQ(ierr);
  }
#elif defined(PETSC_HAVE_WINDOWS_H)   /* Windows */
  {
    SYSTEM_INFO sysinfo;
    GetSystemInfo( &sysinfo );
    N_CORES = sysinfo.dwNumberOfProcessors;
  }
#endif
  PetscFunctionReturn(0);
}
			    
#undef __FUNCT__
#define __FUNCT__ "PetscThreadCommCreate"
/*
   PetscThreadCommCreate - Allocates a thread communicator object
  
   Output Parameters:
   tcomm - pointer to the thread communicator object

   Level: developer

.seealso: PetscThreadCommDestroy()
*/
PetscErrorCode PetscThreadCommCreate(PetscThreadComm *tcomm)
{
  PetscErrorCode  ierr;
  PetscThreadComm tcommout;

  PetscFunctionBegin;
  PetscValidPointer(tcomm,1);
  *tcomm = PETSC_NULL;
#ifndef PETSC_USE_DYNAMIC_LIBRARIES
  ierr = PetscThreadCommInitializePackage(PETSC_NULL);CHKERRQ(ierr);
#endif

  ierr = PetscHeaderCreate(tcommout,_p_PetscThreadComm,struct _PetscThreadCommOps,PETSCTHREADCOMM_CLASSID,0,"PetscThreadComm","Thread communicator","PetscThreadComm",PETSC_COMM_WORLD,PetscThreadCommDestroy,PetscThreadCommView);CHKERRQ(ierr);
  tcommout->nworkThreads =  -1;
  tcommout->affinities = PETSC_NULL;
  ierr = PetscNew(struct _p_PetscThreadCommJobCtx,&tcommout->jobctx);CHKERRQ(ierr);
  *tcomm = tcommout;

  if(!PetscGetNCoresCalled) {     
    /* Set the number of available cores */
    ierr = PetscGetNCores();CHKERRQ(ierr);
    PetscGetNCoresCalled = PETSC_TRUE;
  }

  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscThreadCommDestroy"
/*
  PetscThreadCommDestroy - Frees a thread communicator object

  Input Parameters:
. tcomm - the PetscThreadComm object

  Level: developer

.seealso: PetscThreadCommCreate()
*/
PetscErrorCode PetscThreadCommDestroy(PetscThreadComm *tcomm)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if(!*tcomm) PetscFunctionReturn(0);
  PetscValidHeaderSpecific((*tcomm),PETSCTHREADCOMM_CLASSID,1);
  if(--((PetscObject)(*tcomm))->refct > 0) {*tcomm = 0;PetscFunctionReturn(0); }

  /* Destroy the implementation specific data struct */
  if((*tcomm)->ops->destroy) {
    (*((*tcomm))->ops->destroy)((*tcomm));
  } 

  ierr = PetscFree((*tcomm)->affinities);CHKERRQ(ierr);
  ierr = PetscFree((*tcomm)->jobctx);CHKERRQ(ierr);
  ierr = PetscHeaderDestroy(tcomm);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscThreadCommView"
/*@C
   PetscThreadCommView - view a thread communicator

   Collective

   Input Parameters:
+  tcomm - thread communicator
-  viewer - viewer to display, for example PETSC_VIEWER_STDOUT_WORLD

   Level: developer

.seealso: PetscThreadCommCreate()
@*/
PetscErrorCode PetscThreadCommView(PetscThreadComm tcomm,PetscViewer viewer)
{
  PetscErrorCode ierr;
  PetscBool      iascii;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(tcomm,PETSCTHREADCOMM_CLASSID,1);
  if(!viewer) {ierr = PetscViewerASCIIGetStdout(((PetscObject)tcomm)->comm,&viewer);CHKERRQ(ierr);}
  PetscCheckSameComm(tcomm,1,viewer,2);
  ierr = PetscTypeCompare((PetscObject)viewer,PETSCVIEWERASCII,&iascii);CHKERRQ(ierr);
  if (iascii) {
    ierr = PetscObjectPrintClassNamePrefixType((PetscObject)tcomm,viewer,"Thread Communicator Object");CHKERRQ(ierr);
    ierr = PetscViewerASCIIPushTab(viewer);CHKERRQ(ierr);
    ierr = PetscViewerASCIIPrintf(viewer,"Number of threads = %D\n",tcomm->nworkThreads);CHKERRQ(ierr);
    ierr = PetscViewerASCIIPopTab(viewer);CHKERRQ(ierr);
    if(tcomm->ops->view) {
      ierr = PetscViewerASCIIPushTab(viewer);CHKERRQ(ierr);
      ierr = (*tcomm->ops->view)(tcomm,viewer);CHKERRQ(ierr);
      ierr = PetscViewerASCIIPopTab(viewer);CHKERRQ(ierr);
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscThreadCommSetNThreads"
/*C
   PetscThreadCommSetNThreads - Set the thread count for the thread communicator

   Not collective

   Input Parameters:
+  tcomm - the thread communicator
-  nthreads - Number of threads

   Options Database keys:
   -threadcomm_nthreads <nthreads> Number of threads to use

   Level: developer

   Notes:
   Defaults to using 1 thread.

   Use nthreads = PETSC_DECIDE or -threadcomm_nthreads PETSC_DECIDE for PETSc to decide the number of threads.


.seealso: PetscThreadCommGetNThreads()
*/
PetscErrorCode PetscThreadCommSetNThreads(PetscThreadComm tcomm,PetscInt nthreads)
{
  PetscErrorCode ierr;
  PetscBool      flg;
  PetscInt        nthr;
  PetscFunctionBegin;
  if(nthreads == PETSC_DECIDE) {
    tcomm->nworkThreads = 1;
    ierr = PetscObjectOptionsBegin((PetscObject)tcomm);CHKERRQ(ierr);
    ierr = PetscOptionsInt("-threadcomm_nthreads","number of threads to use in the thread communicator","PetscThreadCommSetNThreads",1,&nthr,&flg);CHKERRQ(ierr);
    ierr = PetscOptionsEnd();CHKERRQ(ierr);
    if(flg){ 
      if(nthr == PETSC_DECIDE) {
      tcomm->nworkThreads = N_CORES;
      } else tcomm->nworkThreads = nthr;
    }
  } else tcomm->nworkThreads = nthreads;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscThreadCommGetNThreads"
/*@C
   PetscThreadCommGetNThreads - Gets the thread count from the thread communicator

   Not collective

   Input Parameters:
.  tcomm - the thread communicator
 
   Output Parameters:
.  nthreads - thread count of the communicator

   Level: developer

.seealso: PetscThreadCommSetNThreads()
@*/
PetscErrorCode PetscThreadCommGetNThreads(PetscThreadComm tcomm,PetscInt *nthreads)
{
  PetscFunctionBegin;
  *nthreads = tcomm->nworkThreads;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscThreadCommSetAffinities"
/*
   PetscThreadCommSetAffinities - Sets the core affinity for threads
                                  (which threads run on which cores)
   
   Not collective

   Input Parameters:
+  tcomm - the thread communicator
.  affinities - array of core affinity for threads

   Options Database keys:
   -thread_affinities <list of thread affinities> 

   Level: developer

   Notes:
   Use affinities = PETSC_NULL for PETSc to decide the affinities.
   If PETSc decides affinities, then each thread has affinity to 
   a unique core with the main thread on Core 0, thread0 on core 1,
   and so on. If the thread count is more the number of available
   cores then multiple threads share a core.

   The first value is the affinity for the main thread

   The affinity list can be passed as
   a comma seperated list:                                 0,1,2,3,4,5,6,7
   a range (start-end+1):                                  0-8
   a range with given increment (start-end+1:inc):         0-7:2
   a combination of values and ranges seperated by commas: 0,1-8,8-15:2

   There must be no intervening spaces between the values.

.seealso: PetscThreadCommGetAffinities(), PetscThreadCommSetNThreads()
*/				  
PetscErrorCode PetscThreadCommSetAffinities(PetscThreadComm tcomm,const PetscInt affinities[])
{
  PetscErrorCode ierr;
  PetscBool      flg;
  PetscInt       nmax=tcomm->nworkThreads;

  PetscFunctionBegin;
  /* Free if affinities set already */
  ierr = PetscFree(tcomm->affinities);CHKERRQ(ierr);

  ierr = PetscMalloc(tcomm->nworkThreads*sizeof(PetscInt),&tcomm->affinities);CHKERRQ(ierr);

  if(affinities == PETSC_NULL) {
    /* Check if option is present in the options database */
    ierr = PetscObjectOptionsBegin((PetscObject)tcomm);CHKERRQ(ierr);
    ierr = PetscOptionsIntArray("-threadcomm_affinities","Set core affinities of threads","PetscThreadCommSetAffinities",tcomm->affinities,&nmax,&flg);CHKERRQ(ierr);
    ierr = PetscOptionsEnd();CHKERRQ(ierr);
    if(flg) {
      if(nmax != tcomm->nworkThreads) {
	SETERRQ2(PETSC_COMM_SELF,PETSC_ERR_ARG_SIZ,"Must set affinities for all threads, Threads = %D, Core affinities set = %D",tcomm->nworkThreads,nmax);
      }
    } else {
      /* PETSc default affinities */
      PetscInt i;
      for(i=0;i<tcomm->nworkThreads;i++) tcomm->affinities[i] = i%N_CORES;
    }
  } else {
    ierr = PetscMemcpy(tcomm->affinities,affinities,tcomm->nworkThreads*sizeof(PetscInt));
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscThreadCommGetAffinities"
/*@C
   PetscThreadCommGetAffinities - Returns the core affinities set for the
                                  thread communicator

    Not collective

    Input Parameters:
.   tcomm - thread communicator

    Output Parameters:
.   affinities - thread affinities

    Level: developer

    Notes:
    The user must allocate space (nthreads PetscInts) for the
    affinities. Must call PetscThreadCommSetAffinities before.

*/
PetscErrorCode PetscThreadCommGetAffinities(PetscThreadComm tcomm,PetscInt affinities[])
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(tcomm,PETSCTHREADCOMM_CLASSID,1);
  PetscValidIntPointer(affinities,2);
  ierr = PetscMemcpy(affinities,tcomm->affinities,tcomm->nworkThreads*sizeof(PetscInt));CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
  
#undef __FUNCT__
#define __FUNCT__ "PetscThreadCommSetType"
/*
   PetscThreadCommSetType - Sets the threading model for the communicator

   Logically collective

   Input Parameters:
+  tcomm - the thread communicator
-  type  - the type of thread model needed


   Options Database keys:
   -threadcomm_type <type>

   Available types
   See "petsc/include/petscthreadcomm.h" for available types

.seealso: PetscThreadCommGetType()   
*/
PetscErrorCode PetscThreadCommSetType(PetscThreadComm tcomm,const PetscThreadCommType type)
{
  PetscErrorCode ierr,(*r)(PetscThreadComm);
  PetscBool      match;
  char           ttype[256];
  PetscBool      flg;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(tcomm,PETSCTHREADCOMM_CLASSID,1);
  PetscValidCharPointer(type,2);

  if(!PetscThreadCommRegisterAllCalled) { ierr = PetscThreadCommRegisterAll(PETSC_NULL);CHKERRQ(ierr);}

  ierr = PetscObjectOptionsBegin((PetscObject)tcomm);CHKERRQ(ierr);
  ierr = PetscOptionsList("-threadcomm_type","Thread communicator model","PetscThreadCommSetType",PetscThreadCommList,PTHREAD,ttype,256,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsEnd();CHKERRQ(ierr);
  if (!flg) {
    ierr = PetscStrcpy(ttype,type);CHKERRQ(ierr); 
  }
  ierr = PetscTypeCompare((PetscObject)tcomm,ttype,&match);CHKERRQ(ierr);
  if(match) PetscFunctionReturn(0);
  
  ierr = PetscFListFind(PetscThreadCommList,PETSC_COMM_WORLD,type,PETSC_TRUE,(void (**)(void)) &r);CHKERRQ(ierr);
  if (!r) SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_ARG_UNKNOWN_TYPE,"Unable to find requested PetscThreadComm type %s",ttype);
  ierr = PetscObjectChangeTypeName((PetscObject)tcomm,ttype);CHKERRQ(ierr);
  ierr = (*r)(tcomm);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscThreadCommRegisterDestroy"
/*@C
   PetscThreadCommRegisterDestroy - Frees the list of thread communicator models that were
   registered by PetscThreadCommRegisterDynamic().

   Not Collective

   Level: advanced

.keywords: PetscThreadComm, register, destroy

.seealso: PetscThreadCommRegisterAll()
@*/
PetscErrorCode  PetscThreadCommRegisterDestroy(void)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscFListDestroy(&PetscThreadCommList);CHKERRQ(ierr);
  PetscThreadCommRegisterAllCalled = PETSC_FALSE;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscThreadCommRegister"
/*@C
  PetscThreadCommRegister - See PetscThreadCommRegisterDynamic()

  Level: advanced
@*/
PetscErrorCode  PetscThreadCommRegister(const char sname[],const char path[],const char name[],PetscErrorCode (*function)(PetscThreadComm))
{
  char           fullname[PETSC_MAX_PATH_LEN];
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscFListConcat(path,name,fullname);CHKERRQ(ierr);
  ierr = PetscFListAdd(&PetscThreadCommList,sname,fullname,(void (*)(void))function);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscThreadCommRunKernel"
/*@C
   PetscThreadCommRunKernel - Runs the kernel using the given PetscThreadComm

   Input Parameters:
+  tcomm - the thread communicator
.  func  - the kernel (needs to be cast to PetscThreadKernel)
.  nargs - Number of input arguments for the kernel
-  ...   - variable list of input arguments

   Level: developer

   Notes:
   All input arguments to the kernel must be passed by reference, Petsc objects are
   inherrently passed by reference so you don't need to additionally & them.

   Example usage - PetscThreadCommRunKernel(tcomm,(PetscThreadKernel)kernel_func,3,x,y,z);
   with kernel_func declared as
   PetscErrorCode kernel_func(PetscInt thread_id,PetscInt* x, PetscScalar* y, PetscReal* z)

   The first input argument of kernel_func, thread_id, is the thread rank. This is passed implicitly
   by PETSc.

.seealso: PetscThreadCommCreate(), PetscThreadCommSetNThreads()
@*/
PetscErrorCode PetscThreadCommRunKernel(PetscThreadComm tcomm,PetscErrorCode (*func)(PetscInt,...),PetscInt nargs,...)
{
  PetscErrorCode ierr;
  va_list        argptr;
  PetscInt       i;
  PetscThreadCommJobCtx job=tcomm->jobctx;

  PetscFunctionBegin;
  if(nargs > PETSC_KERNEL_NARGS_MAX) SETERRQ2(PETSC_COMM_SELF,PETSC_ERR_ARG_WRONG,"Requested %D input arguments for kernel, max. limit %D",nargs,PETSC_KERNEL_NARGS_MAX);
  PetscValidHeaderSpecific(tcomm,PETSCTHREADCOMM_CLASSID,1);
  job->nargs = nargs;
  job->pfunc = func;
  va_start(argptr,nargs);
  for(i=0; i < nargs; i++) {
    job->args[i] = va_arg(argptr,void*);
  }
  va_end(argptr);
  
  ierr = (*tcomm->ops->runkernel)(tcomm,job);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscThreadComm_Init"
/*
  PetscThreadComm_Init - Initializes the global thread communicator

  PetscThreadComm_Init() defaults to 1 thread and PTHREAD type.
*/
PetscErrorCode PetscThreadComm_Init(void)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscThreadCommCreate(&PETSC_THREAD_COMM_WORLD);CHKERRQ(ierr);
  ierr = PetscThreadCommSetNThreads(PETSC_THREAD_COMM_WORLD,PETSC_DECIDE);CHKERRQ(ierr);
  ierr = PetscThreadCommSetAffinities(PETSC_THREAD_COMM_WORLD,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscThreadCommSetType(PETSC_THREAD_COMM_WORLD,PTHREAD);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
