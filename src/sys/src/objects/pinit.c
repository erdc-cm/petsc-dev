/*$Id: pinit.c,v 1.58 2001/08/10 03:28:54 bsmith Exp $*/
/*
   This file defines the initialization of PETSc, including PetscInitialize()
*/

#include "petsc.h"        /*I  "petsc.h"   I*/
#include "petscsys.h"

EXTERN int PetscLogBegin_Private(void);

/* -----------------------------------------------------------------------------------------*/

extern FILE *petsc_history;

EXTERN int PetscInitialize_DynamicLibraries(void);
EXTERN int PetscFinalize_DynamicLibraries(void);
EXTERN int PetscFListDestroyAll(void);
EXTERN int PetscSequentialPhaseBegin_Private(MPI_Comm,int);
EXTERN int PetscSequentialPhaseEnd_Private(MPI_Comm,int);
EXTERN int PetscLogCloseHistoryFile(FILE **);

/* this is used by the _, __, and ___ macros (see include/petscerror.h) */
int __gierr = 0;

/*
       Checks the options database for initializations related to the 
    PETSc components
*/
#undef __FUNCT__  
#define __FUNCT__ "PetscOptionsCheckInitial_Components"
int PetscOptionsCheckInitial_Components(void)
{
  MPI_Comm   comm = PETSC_COMM_WORLD;
  PetscTruth flg1;
  int        ierr;

  PetscFunctionBegin;
  /*
     Publishing to the AMS
  */
#if defined(PETSC_HAVE_AMS)
  ierr = PetscOptionsHasName(PETSC_NULL,"-ams_publish_objects",&flg1);CHKERRQ(ierr);
  if (flg1) {
    PetscAMSPublishAll = PETSC_TRUE;
  }
  ierr = PetscOptionsHasName(PETSC_NULL,"-ams_publish_stack",&flg1);CHKERRQ(ierr);
  if (flg1) {
    ierr = PetscStackPublish();CHKERRQ(ierr);
  }
#endif

  ierr = PetscOptionsHasName(PETSC_NULL,"-help",&flg1);CHKERRQ(ierr);
  if (flg1) {
#if defined (PETSC_USE_LOG)
    ierr = (*PetscHelpPrintf)(comm,"------Additional PETSc component options--------\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -log_summary_exclude: <vec,mat,sles,snes>\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -log_info_exclude: <null,vec,mat,sles,snes,ts>\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm,"-----------------------------------------------\n");CHKERRQ(ierr);
#endif
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscInitializeNoArguments"
/*@C
      PetscInitializeNoArguments - Calls PetscInitialize() from C/C++ without
        the command line arguments.

   Collective
  
   Level: advanced

.seealso: PetscInitialize(), PetscInitializeFortran()
@*/
int PetscInitializeNoArguments(void)
{
  int ierr,argc = 0;
  char **args = 0;

  PetscFunctionBegin;
  ierr = PetscInitialize(&argc,&args,PETSC_NULL,PETSC_NULL);
  PetscFunctionReturn(ierr);
}

EXTERN int        PetscOptionsCheckInitial_Private(void);
extern PetscTruth PetscBeganMPI;

/*
       This function is the MPI reduction operation used to compute the sum of the 
   first half of the datatype and the max of the second half.
*/
MPI_Op PetscMaxSum_Op = 0;

EXTERN_C_BEGIN
#undef __FUNCT__
#define __FUNCT__ "PetscMaxSum_Local"
void PetscMaxSum_Local(void *in,void *out,int *cnt,MPI_Datatype *datatype)
{
  int *xin = (int *)in,*xout = (int*)out,i,count = *cnt;

  PetscFunctionBegin;
  if (*datatype != MPI_2INT) {
    (*PetscErrorPrintf)("Can only handle MPI_2INT data types");
    MPI_Abort(MPI_COMM_WORLD,1);
  }

  for (i=0; i<count; i++) {
    xout[2*i]    = PetscMax(xout[2*i],xin[2*i]); 
    xout[2*i+1] += xin[2*i+1]; 
  }
  PetscStackPop;
  return;
}
EXTERN_C_END

/*
    Returns the max of the first entry owned by this processor and the
sum of the second entry.
*/
#undef __FUNCT__
#define __FUNCT__ "PetscMaxSum"
int PetscMaxSum(MPI_Comm comm,const int nprocs[],int *max,int *sum)
{
  int size,rank,ierr,*work;
  
  PetscFunctionBegin;
  ierr   = MPI_Comm_size(comm,&size);CHKERRQ(ierr);
  ierr   = MPI_Comm_rank(comm,&rank);CHKERRQ(ierr);
  ierr   = PetscMalloc(2*size*sizeof(int),&work);CHKERRQ(ierr);
  ierr   = MPI_Allreduce((void*)nprocs,work,size,MPI_2INT,PetscMaxSum_Op,comm);CHKERRQ(ierr);
  *max   = work[2*rank];
  *sum   = work[2*rank+1]; 
  ierr   = PetscFree(work);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* ----------------------------------------------------------------------------*/
MPI_Op PetscADMax_Op = 0;

EXTERN_C_BEGIN
#undef __FUNCT__
#define __FUNCT__ "PetscADMax_Local"
void PetscADMax_Local(void *in,void *out,int *cnt,MPI_Datatype *datatype)
{
  PetscScalar *xin = (PetscScalar *)in,*xout = (PetscScalar*)out;
  int         i,count = *cnt;

  PetscFunctionBegin;
  if (*datatype != MPIU_2SCALAR) {
    (*PetscErrorPrintf)("Can only handle MPIU_2SCALAR data (i.e. double or complex) types");
    MPI_Abort(MPI_COMM_WORLD,1);
  }

  for (i=0; i<count; i++) {
    if (PetscRealPart(xout[2*i]) < PetscRealPart(xin[2*i])) {
      xout[2*i]   = xin[2*i];
      xout[2*i+1] = xin[2*i+1];
    }
  }

  PetscStackPop;
  return;
}
EXTERN_C_END

MPI_Op PetscADMin_Op = 0;

EXTERN_C_BEGIN
#undef __FUNCT__
#define __FUNCT__ "PetscADMin_Local"
void PetscADMin_Local(void *in,void *out,int *cnt,MPI_Datatype *datatype)
{
  PetscScalar *xin = (PetscScalar *)in,*xout = (PetscScalar*)out;
  int         i,count = *cnt;

  PetscFunctionBegin;
  if (*datatype != MPIU_2SCALAR) {
    (*PetscErrorPrintf)("Can only handle MPIU_2SCALAR data (i.e. double or complex) types");
    MPI_Abort(MPI_COMM_WORLD,1);
  }

  for (i=0; i<count; i++) {
    if (PetscRealPart(xout[2*i]) > PetscRealPart(xin[2*i])) {
      xout[2*i]   = xin[2*i];
      xout[2*i+1] = xin[2*i+1];
    }
  }

  PetscStackPop;
  return;
}
EXTERN_C_END
/* ---------------------------------------------------------------------------------------*/

#if defined(PETSC_USE_COMPLEX)
MPI_Op PetscSum_Op = 0;

EXTERN_C_BEGIN
#undef __FUNCT__
#define __FUNCT__ "PetscSum_Local"
void PetscSum_Local(void *in,void *out,int *cnt,MPI_Datatype *datatype)
{
  PetscScalar *xin = (PetscScalar *)in,*xout = (PetscScalar*)out;
  int         i,count = *cnt;

  PetscFunctionBegin;
  if (*datatype != MPIU_SCALAR) {
    (*PetscErrorPrintf)("Can only handle MPIU_SCALAR data (i.e. double or complex) types");
    MPI_Abort(MPI_COMM_WORLD,1);
  }

  for (i=0; i<count; i++) {
    xout[i] += xin[i]; 
  }

  PetscStackPop;
  return;
}
EXTERN_C_END
#endif

static int  PetscGlobalArgc   = 0;
static char **PetscGlobalArgs = 0;

#undef __FUNCT__  
#define __FUNCT__ "PetscGetArgs"
/*@C
   PetscGetArgs - Allows you to access the raw command line arguments anywhere
     after PetscInitialize() is called but before PetscFinalize().

   Not Collective

   Output Parameters:
+  argc - count of number of command line arguments
-  args - the command line arguments

   Level: intermediate

   Notes:
      This is usually used to pass the command line arguments into other libraries
   that are called internally deep in PETSc or the application.

   Concepts: command line arguments
   
.seealso: PetscFinalize(), PetscInitializeFortran()

@*/
int PetscGetArgs(int *argc,char ***args)
{
  PetscFunctionBegin;
  if (!PetscGlobalArgs) {
    SETERRQ(1,"You must call after PetscInitialize() but before PetscFinalize()");
  }
  *argc = PetscGlobalArgc;
  *args = PetscGlobalArgs;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscInitialize"
/*@C
   PetscInitialize - Initializes the PETSc database and MPI. 
   PetscInitialize() calls MPI_Init() if that has yet to be called,
   so this routine should always be called near the beginning of 
   your program -- usually the very first line! 

   Collective on MPI_COMM_WORLD or PETSC_COMM_WORLD if it has been set

   Input Parameters:
+  argc - count of number of command line arguments
.  args - the command line arguments
.  file - [optional] PETSc database file, defaults to ~username/.petscrc
          (use PETSC_NULL for default)
-  help - [optional] Help message to print, use PETSC_NULL for no message

   Options Database Keys:
+  -start_in_debugger [noxterm,dbx,xdb,gdb,...] - Starts program in debugger
.  -on_error_attach_debugger [noxterm,dbx,xdb,gdb,...] - Starts debugger when error detected
.  -on_error_emacs <machinename> causes emacsclient to jump to error file
.  -debugger_nodes [node1,node2,...] - Indicates nodes to start in debugger
.  -debugger_pause [sleeptime] (in seconds) - Pauses debugger
.  -stop_for_debugger - Print message on how to attach debugger manually to 
                        process and wait (-debugger_pause) seconds for attachment
.  -trmalloc - Indicates use of PETSc error-checking malloc
.  -trmalloc no - Indicates not to use error-checking malloc
.  -fp_trap - Stops on floating point exceptions (Note that on the
              IBM RS6000 this slows code by at least a factor of 10.)
.  -no_signal_handler - Indicates not to trap error signals
.  -shared_tmp - indicates /tmp directory is shared by all processors
.  -not_shared_tmp - each processor has own /tmp
.  -tmp - alternative name of /tmp directory
.  -get_total_flops - returns total flops done by all processors
-  -get_resident_set_size - Print memory usage at end of run

   Options Database Keys for Profiling:
   See the Profiling chapter of the users manual for details.
+  -log_trace [filename] - Print traces of all PETSc calls
        to the screen (useful to determine where a program
        hangs without running in the debugger).  See PetscLogTraceBegin().
.  -log_info <optional filename> - Prints verbose information to the screen
-  -log_info_exclude <null,vec,mat,sles,snes,ts> - Excludes some of the verbose messages

   Environmental Variables:
+   PETSC_TMP - alternative tmp directory
.   PETSC_SHARED_TMP - tmp is shared by all processes
.   PETSC_NOT_SHARED_TMP - each process has its own private tmp
.   PETSC_VIEWER_SOCKET_PORT - socket number to use for socket viewer
-   PETSC_VIEWER_SOCKET_MACHINE - machine to use for socket viewer to connect to


   Level: beginner

   Notes:
   If for some reason you must call MPI_Init() separately, call
   it before PetscInitialize().

   Fortran Version:
   In Fortran this routine has the format
$       call PetscInitialize(file,ierr)

+   ierr - error return code
-   file - [optional] PETSc database file name, defaults to 
           ~username/.petscrc (use PETSC_NULL_CHARACTER for default)
           
   Important Fortran Note:
   In Fortran, you MUST use PETSC_NULL_CHARACTER to indicate a
   null character string; you CANNOT just use PETSC_NULL as 
   in the C version.  See the users manual for details.


   Concepts: initializing PETSc
   
.seealso: PetscFinalize(), PetscInitializeFortran(), PetescGetArgs()

@*/
int PetscInitialize(int *argc,char ***args,char file[],const char help[])
{
  int        ierr,flag,dummy_tag,size;
  PetscTruth flg;
  char       hostname[256];

  PetscFunctionBegin;
  if (PetscInitializeCalled) PetscFunctionReturn(0);

  ierr = PetscOptionsCreate();CHKERRQ(ierr);

  /*
     We initialize the program name here (before MPI_Init()) because MPICH has a bug in 
     it that it sets args[0] on all processors to be args[0] on the first processor.
  */
  if (argc && *argc) {
    ierr = PetscSetProgramName(**args);CHKERRQ(ierr);
  } else {
    ierr = PetscSetProgramName("Unknown Name");CHKERRQ(ierr);
  }


  ierr = MPI_Initialized(&flag);CHKERRQ(ierr);
  if (!flag) {
    ierr          = MPI_Init(argc,args);CHKERRQ(ierr);
    PetscBeganMPI = PETSC_TRUE;
  }
  if (argc && args) {
    PetscGlobalArgc = *argc;
    PetscGlobalArgs = *args;
  }
  PetscInitializeCalled = PETSC_TRUE;

  /* Also initialize the initial datestamp. Done after init due to a bug in MPICH-GM? */
  ierr = PetscSetInitialDate();CHKERRQ(ierr);

  if (!PETSC_COMM_WORLD) {
    PETSC_COMM_WORLD = MPI_COMM_WORLD;
  }

  ierr = MPI_Comm_rank(MPI_COMM_WORLD,&PetscGlobalRank);CHKERRQ(ierr);
  ierr = MPI_Comm_size(MPI_COMM_WORLD,&PetscGlobalSize);CHKERRQ(ierr);

#if defined(PETSC_USE_COMPLEX)
  /* 
     Initialized the global complex variable; this is because with 
     shared libraries the constructors for global variables
     are not called; at least on IRIX.
  */
  {
    PetscScalar ic(0.0,1.0);
    PETSC_i = ic; 
  }
  ierr = MPI_Type_contiguous(2,MPIU_REAL,&MPIU_COMPLEX);CHKERRQ(ierr);
  ierr = MPI_Type_commit(&MPIU_COMPLEX);CHKERRQ(ierr);
  ierr = MPI_Op_create(PetscSum_Local,1,&PetscSum_Op);CHKERRQ(ierr);
#endif

  /*
     Create the PETSc MPI reduction operator that sums of the first
     half of the entries and maxes the second half.
  */
  ierr = MPI_Op_create(PetscMaxSum_Local,1,&PetscMaxSum_Op);CHKERRQ(ierr);

  ierr = MPI_Type_contiguous(2,MPIU_SCALAR,&MPIU_2SCALAR);CHKERRQ(ierr);
  ierr = MPI_Type_commit(&MPIU_2SCALAR);CHKERRQ(ierr);
  ierr = MPI_Op_create(PetscADMax_Local,1,&PetscADMax_Op);CHKERRQ(ierr);
  ierr = MPI_Op_create(PetscADMin_Local,1,&PetscADMin_Op);CHKERRQ(ierr);

  /*
     Build the options database and check for user setup requests
  */
  ierr = PetscOptionsInsert(argc,args,file);CHKERRQ(ierr);

  /*
     Print main application help message
  */
  ierr = PetscOptionsHasName(PETSC_NULL,"-help",&flg);CHKERRQ(ierr);
  if (help && flg) {
    ierr = PetscPrintf(PETSC_COMM_WORLD,help);CHKERRQ(ierr);
  }
  ierr = PetscOptionsCheckInitial_Private();CHKERRQ(ierr); 

  /* SHOULD PUT IN GUARDS: Make sure logging is initialized, even if we od not print it out */
  ierr = PetscLogBegin_Private();CHKERRQ(ierr);

  /*
     Initialize PETSC_COMM_SELF and WORLD as a MPI_Comm with the PETSc attribute.
    
     We delay until here to do it, since PetscMalloc() may not have been
     setup before this.
  */
  ierr = PetscCommDuplicate_Private(MPI_COMM_SELF,&PETSC_COMM_SELF,&dummy_tag);CHKERRQ(ierr);
  ierr = PetscCommDuplicate_Private(PETSC_COMM_WORLD,&PETSC_COMM_WORLD,&dummy_tag);CHKERRQ(ierr);

  /*
     Load the dynamic libraries (on machines that support them), this registers all
     the solvers etc. (On non-dynamic machines this initializes the PetscDraw and PetscViewer classes)
  */
  ierr = PetscInitialize_DynamicLibraries();CHKERRQ(ierr);

  /*
     Initialize all the default viewers
  */
  ierr = MPI_Comm_size(PETSC_COMM_WORLD,&size);CHKERRQ(ierr);
  PetscLogInfo(0,"PetscInitialize:PETSc successfully started: number of processors = %d\n",size);
  ierr = PetscGetHostName(hostname,256);CHKERRQ(ierr);
  PetscLogInfo(0,"PetscInitialize:Running on machine: %s\n",hostname);

  ierr = PetscOptionsCheckInitial_Components();CHKERRQ(ierr);

  PetscFunctionReturn(ierr);
}


#undef __FUNCT__  
#define __FUNCT__ "PetscFinalize"
/*@C 
   PetscFinalize - Checks for options to be called at the conclusion
   of the program and calls MPI_Finalize().

   Collective on PETSC_COMM_WORLD

   Options Database Keys:
+  -options_table - Calls OptionsPrint()
.  -options_left - Prints unused options that remain in the database
.  -options_left no - Does not print unused options that remain in the database
.  -mpidump - Calls PetscMPIDump()
.  -trdump - Calls PetscTrDump()
.  -trinfo - Prints total memory usage
.  -trdebug - Calls malloc_debug(2) to activate memory
        allocation diagnostics (used by PETSC_ARCH=sun4, 
        BOPT=[g,g_c++,g_complex] only!)
-  -trmalloc_log - Prints summary of memory usage

   Options Database Keys for Profiling:
   See the Profiling chapter of the users manual for details.
+  -log_summary [filename] - Prints summary of flop and timing
        information to screen. If the filename is specified the
        summary is written to the file. (for code compiled with 
        PETSC_USE_LOG).  See PetscLogPrintSummary().
.  -log_all [filename] - Logs extensive profiling information
        (for code compiled with PETSC_USE_LOG). See PetscLogDump(). 
.  -log [filename] - Logs basic profiline information (for
        code compiled with PETSC_USE_LOG).  See PetscLogDump().
.  -log_sync - Log the synchronization in scatters, inner products
        and norms
-  -log_mpe [filename] - Creates a logfile viewable by the 
      utility Upshot/Nupshot (in MPICH distribution)

   Level: beginner

   Note:
   See PetscInitialize() for more general runtime options.

.seealso: PetscInitialize(), PetscOptionsPrint(), PetscTrDump(), PetscMPIDump(), PetscEnd()
@*/
int PetscFinalize(void)
{
  int            ierr,rank,nopt;
  PetscLogDouble rss;
  PetscTruth     flg1,flg2,flg3;
  
  PetscFunctionBegin;

  if (!PetscInitializeCalled) {
    (*PetscErrorPrintf)("PETSc ERROR: PetscInitialize() must be called before PetscFinalize()\n");
    PetscFunctionReturn(0);
  }
  /* Destroy auxiliary packages */
  ierr = PetscViewerMathematicaFinalizePackage();CHKERRQ(ierr);
  ierr = PetscPLAPACKFinalizePackage();CHKERRQ(ierr);

  /*
     Destroy all the function registration lists created
  */
  ierr = PetscFinalize_DynamicLibraries();CHKERRQ(ierr);


  ierr = PetscOptionsHasName(PETSC_NULL,"-get_resident_set_size",&flg1);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(PETSC_COMM_WORLD,&rank);CHKERRQ(ierr);
  if (flg1) {
    ierr = PetscGetResidentSetSize(&rss);CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_SELF,"[%d] Size of entire process memory %d\n",rank,(int)rss);CHKERRQ(ierr);
  }

#if defined(PETSC_USE_LOG)
  ierr = PetscOptionsHasName(PETSC_NULL,"-get_total_flops",&flg1);CHKERRQ(ierr);
  if (flg1) {
    PetscLogDouble flops = 0;
    ierr = MPI_Reduce(&_TotalFlops,&flops,1,MPI_DOUBLE,MPI_SUM,0,PETSC_COMM_WORLD);CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Total flops over all processors %g\n",flops);CHKERRQ(ierr);
  }
#endif

  /*
     Free all objects registered with PetscObjectRegisterDestroy() such ast
    PETSC_VIEWER_XXX_().
  */
  ierr = PetscObjectRegisterDestroyAll();CHKERRQ(ierr);  

#if defined(PETSC_USE_STACK)
  if (PetscStackActive) {
    ierr = PetscStackDestroy();CHKERRQ(ierr);
  }
#endif

#if defined(PETSC_USE_LOG)
  {
    char mname[PETSC_MAX_PATH_LEN];
#if defined(PETSC_HAVE_MPE)
    mname[0] = 0;
    ierr = PetscOptionsGetString(PETSC_NULL,"-log_mpe",mname,PETSC_MAX_PATH_LEN,&flg1);CHKERRQ(ierr);
    if (flg1){
      if (mname[0]) {ierr = PetscLogMPEDump(mname);CHKERRQ(ierr);}
      else          {ierr = PetscLogMPEDump(0);CHKERRQ(ierr);}
    }
#endif
    mname[0] = 0;
    ierr = PetscOptionsGetString(PETSC_NULL,"-log_summary",mname,PETSC_MAX_PATH_LEN,&flg1);CHKERRQ(ierr);
    if (flg1) { 
      if (mname[0])  {ierr = PetscLogPrintSummary(PETSC_COMM_WORLD,mname);CHKERRQ(ierr);}
      else           {ierr = PetscLogPrintSummary(PETSC_COMM_WORLD,0);CHKERRQ(ierr);}
    }

    mname[0] = 0;
    ierr = PetscOptionsGetString(PETSC_NULL,"-log_all",mname,PETSC_MAX_PATH_LEN,&flg1);CHKERRQ(ierr);
    ierr = PetscOptionsGetString(PETSC_NULL,"-log",mname,PETSC_MAX_PATH_LEN,&flg2);CHKERRQ(ierr);
    if (flg1 || flg2){
      if (mname[0]) PetscLogDump(mname); 
      else          PetscLogDump(0);
    }
    ierr = PetscLogDestroy();CHKERRQ(ierr);
  }
#endif
  ierr = PetscOptionsHasName(PETSC_NULL,"-no_signal_handler",&flg1);CHKERRQ(ierr);
  if (!flg1) { ierr = PetscPopSignalHandler();CHKERRQ(ierr);}
  ierr = PetscOptionsHasName(PETSC_NULL,"-mpidump",&flg1);CHKERRQ(ierr);
  if (flg1) {
    ierr = PetscMPIDump(stdout);CHKERRQ(ierr);
  }
  ierr = PetscOptionsHasName(PETSC_NULL,"-trdump",&flg1);CHKERRQ(ierr);
  ierr = PetscOptionsHasName(PETSC_NULL,"-optionstable",&flg1);CHKERRQ(ierr);
  ierr = PetscOptionsHasName(PETSC_NULL,"-options_table",&flg2);CHKERRQ(ierr);
  if (flg1 && flg2) {
    if (!rank) {ierr = PetscOptionsPrint(stdout);CHKERRQ(ierr);}
  }

  /* to prevent PETSc -options_left from warning */
  ierr = PetscOptionsHasName(PETSC_NULL,"-nox_warning",&flg1);CHKERRQ(ierr)
  ierr = PetscOptionsHasName(PETSC_NULL,"-error_output_stderr",&flg1);CHKERRQ(ierr);

  ierr = PetscOptionsGetLogical(PETSC_NULL,"-options_left",&flg2,&flg1);CHKERRQ(ierr);
  ierr = PetscOptionsAllUsed(&nopt);CHKERRQ(ierr);
  if (flg2) {
    ierr = PetscOptionsPrint(stdout);CHKERRQ(ierr);
    if (!nopt) { 
      ierr = PetscPrintf(PETSC_COMM_WORLD,"There are no unused options.\n");CHKERRQ(ierr);
    } else if (nopt == 1) {
      ierr = PetscPrintf(PETSC_COMM_WORLD,"There is one unused database option. It is:\n");CHKERRQ(ierr);
    } else {
      ierr = PetscPrintf(PETSC_COMM_WORLD,"There are %d unused database options. They are:\n",nopt);CHKERRQ(ierr);
    }
  } 
#if defined(PETSC_USE_BOPT_g)
  if (nopt && !flg1 && !flg2) {
    ierr = PetscPrintf(PETSC_COMM_WORLD,"WARNING! There are options you set that were not used!\n");CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_WORLD,"WARNING! could be spelling mistake, etc!\n");CHKERRQ(ierr);
    ierr = PetscOptionsLeft();CHKERRQ(ierr);
  } else if (nopt && flg2) {
#else 
  if (nopt && flg2) {
#endif
    ierr = PetscOptionsLeft();CHKERRQ(ierr);
  }

  ierr = PetscOptionsHasName(PETSC_NULL,"-log_history",&flg1);CHKERRQ(ierr);
  if (flg1) {
    ierr = PetscLogCloseHistoryFile(&petsc_history);CHKERRQ(ierr);
    petsc_history = 0;
  }


  /*
       Destroy PETSC_COMM_SELF/WORLD as a MPI_Comm with the PETSc 
     attribute.
  */
  ierr = PetscCommDestroy_Private(&PETSC_COMM_SELF);CHKERRQ(ierr);
  ierr = PetscCommDestroy_Private(&PETSC_COMM_WORLD);CHKERRQ(ierr);

  /*
       Free all the registered create functions, such as KSPList, VecList, SNESList, etc
  */
  ierr = PetscFListDestroyAll();CHKERRQ(ierr); 

  ierr = PetscOptionsHasName(PETSC_NULL,"-trdump",&flg1);CHKERRQ(ierr);
  ierr = PetscOptionsHasName(PETSC_NULL,"-trinfo",&flg2);CHKERRQ(ierr);
  ierr = PetscOptionsHasName(PETSC_NULL,"-trmalloc_log",&flg3);CHKERRQ(ierr);
  if (flg1) {
    char fname[256];
    FILE *fd;
    
    fname[0] = 0;
    ierr = PetscOptionsGetString(PETSC_NULL,"-trdump",fname,250,&flg1);CHKERRQ(ierr);
    if (flg1 && fname[0]) {
      char sname[256];

      sprintf(sname,"%s_%d",fname,rank);
      fd   = fopen(sname,"w"); if (!fd) SETERRQ1(1,"Cannot open log file: %s",sname);
      ierr = PetscTrDump(fd);CHKERRQ(ierr);
      fclose(fd);
    } else {
      MPI_Comm local_comm;

      ierr = MPI_Comm_dup(MPI_COMM_WORLD,&local_comm);CHKERRQ(ierr);
      ierr = PetscSequentialPhaseBegin_Private(local_comm,1);CHKERRQ(ierr);
        ierr = PetscTrDump(stdout);CHKERRQ(ierr);
      ierr = PetscSequentialPhaseEnd_Private(local_comm,1);CHKERRQ(ierr);
      ierr = MPI_Comm_free(&local_comm);CHKERRQ(ierr);
    }
  } else if (flg2) {
    MPI_Comm       local_comm;
    PetscLogDouble maxm;

    ierr = MPI_Comm_dup(MPI_COMM_WORLD,&local_comm);CHKERRQ(ierr);
    ierr = PetscTrSpace(PETSC_NULL,PETSC_NULL,&maxm);CHKERRQ(ierr);
    ierr = PetscSequentialPhaseBegin_Private(local_comm,1);CHKERRQ(ierr);
      printf("[%d] Maximum memory used %g\n",rank,maxm);
    ierr = PetscSequentialPhaseEnd_Private(local_comm,1);CHKERRQ(ierr);
    ierr = MPI_Comm_free(&local_comm);CHKERRQ(ierr);
  }
  if (flg3) {
    char fname[256];
    FILE *fd;
    
    fname[0] = 0;
    ierr = PetscOptionsGetString(PETSC_NULL,"-trmalloc_log",fname,250,&flg1);CHKERRQ(ierr);
    if (flg1 && fname[0]) {
      char sname[256];

      sprintf(sname,"%s_%d",fname,rank);
      fd   = fopen(sname,"w"); if (!fd) SETERRQ1(1,"Cannot open log file: %s",sname);
      ierr = PetscTrLogDump(fd);CHKERRQ(ierr); 
      fclose(fd);
    } else {
      ierr = PetscTrLogDump(stdout);CHKERRQ(ierr); 
    }
  }
  /* Can be destroyed only after all the options are used */
  ierr = PetscOptionsDestroy();CHKERRQ(ierr);

  PetscGlobalArgc = 0;
  PetscGlobalArgs = 0;

  PetscLogInfo(0,"PetscFinalize:PETSc successfully ended!\n");
  if (PetscBeganMPI) {
    ierr = MPI_Finalize();CHKERRQ(ierr);
  }

/*

     Note: In certain cases PETSC_COMM_WORLD is never MPI_Comm_free()ed because 
   the communicator has some outstanding requests on it. Specifically if the 
   flag PETSC_HAVE_BROKEN_REQUEST_FREE is set (for IBM MPI implementation). See 
   src/vec/utils/vpscat.c. Due to this the memory allocated in PetscCommDuplicate_Private()
   is never freed as it should be. Thus one may obtain messages of the form
   [ 1] 8 bytes PetscCommDuplicate_Private() line 645 in src/sys/src/mpiu.c indicating the
   memory was not freed.

*/
  ierr = PetscClearMalloc();CHKERRQ(ierr);
  PetscInitializeCalled = PETSC_FALSE;
  PetscFunctionReturn(ierr);
}

/*
     These may be used in code that ADIC is to be used on
*/

#undef __FUNCT__  
#define __FUNCT__ "PetscGlobalMax"
/*@C
      PetscGlobalMax - Computes the maximum value over several processors

     Collective on MPI_Comm

   Input Parameters:
+   local - the local value
-   comm - the processors that find the maximum

   Output Parameter:
.   result - the maximum value
  
   Level: intermediate

   Notes:
     These functions are to be used inside user functions that are to be processed with 
   ADIC. PETSc will automatically provide differentiated versions of these functions

.seealso: PetscGlobalMin(), PetscGlobalSum()
@*/
int PetscGlobalMax(PetscReal* local,PetscReal* result,MPI_Comm comm)
{
  return MPI_Allreduce(local,result,1,MPIU_REAL,MPI_MAX,comm);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscGlobalMin"
/*@C
      PetscGlobalMin - Computes the minimum value over several processors

     Collective on MPI_Comm

   Input Parameters:
+   local - the local value
-   comm - the processors that find the minimum

   Output Parameter:
.   result - the minimum value
  
   Level: intermediate

   Notes:
     These functions are to be used inside user functions that are to be processed with 
   ADIC. PETSc will automatically provide differentiated versions of these functions

.seealso: PetscGlobalMax(), PetscGlobalSum()
@*/
int PetscGlobalMin(PetscReal* local,PetscReal* result,MPI_Comm comm)
{
  return MPI_Allreduce(local,result,1,MPIU_REAL,MPI_MIN,comm);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscGlobalSum"
/*@C
      PetscGlobalSum - Computes the sum over sever processors

     Collective on MPI_Comm

   Input Parameters:
+   local - the local value
-   comm - the processors that find the sum

   Output Parameter:
.   result - the sum
  
   Level: intermediate

   Notes:
     These functions are to be used inside user functions that are to be processed with 
   ADIC. PETSc will automatically provide differentiated versions of these functions

.seealso: PetscGlobalMin(), PetscGlobalMax()
@*/
int PetscGlobalSum(PetscScalar* local,PetscScalar* result,MPI_Comm comm)
{
  return MPI_Allreduce(local,result,1,MPIU_SCALAR,PetscSum_Op,comm);
}

/*MC
   PetscFunctionBegin - First executable line of each PETSc function
        used for error handling.

   Synopsis:
   void PetscFunctionBegin;

   Usage:
.vb
     int something;

     PetscFunctionBegin;
.ve

   Notes:
     Not available in Fortran

   Level: developer

.seealso: PetscFunctionReturn()

.keywords: traceback, error handling
M*/

/*MC
   PetscFunctionEnd - Last executable line of each PETSc function
        used for error handling. Replaces return()

   Synopsis:
   void PetscFunctionReturn(0);

   Usage:
.vb
    ....
     PetscFunctionReturn(0);
   }
.ve

   Notes:
     Not available in Fortran

   Level: developer

.seealso: PetscFunctionBegin()

.keywords: traceback, error handling
M*/
