/*
      Utilites routines to add simple ASCII IO capability.
*/
#include "src/sys/src/fileio/mprint.h"
/*
   If petsc_history is on, then all Petsc*Printf() results are saved
   if the appropriate (usually .petschistory) file.
*/
extern FILE *petsc_history;

#undef __FUNCT__  
#define __FUNCT__ "PetscVSNPrintf"
/* 
   No error handling because may be called by error handler
*/
PetscErrorCode PetscVSNPrintf(char *str,size_t len,const char *format,va_list Argp)
{
#if defined(PETSC_HAVE_VPRINTF_CHAR)
  vsprintf(str,format,(char *)Argp);
#else
  vsprintf(str,format,Argp);
#endif
  return 0;
}

#undef __FUNCT__  
#define __FUNCT__ "PetscVFPrintf"
/* 
   No error handling because may be called by error handler
*/
PetscErrorCode PetscVFPrintf(FILE *fd,const char *format,va_list Argp)
{
#if defined(PETSC_HAVE_VPRINTF_CHAR)
  vfprintf(fd,format,(char *)Argp);
#else
  vfprintf(fd,format,Argp);
#endif
  return 0;
}

/* ----------------------------------------------------------------------- */

PrintfQueue queue       = 0,queuebase = 0;
PetscErrorCode         queuelength = 0;
FILE        *queuefile  = PETSC_NULL;

#undef __FUNCT__  
#define __FUNCT__ "PetscSynchronizedPrintf" 
/*@C
    PetscSynchronizedPrintf - Prints synchronized output from several processors.
    Output of the first processor is followed by that of the second, etc.

    Not Collective

    Input Parameters:
+   comm - the communicator
-   format - the usual printf() format string 

   Level: intermediate

    Notes:
    REQUIRES a intervening call to PetscSynchronizedFlush() for the information 
    from all the processors to be printed.

    Fortran Note:
    The call sequence is PetscSynchronizedPrintf(PetscViewer, character(*), PetscErrorCode ierr) from Fortran. 
    That is, you can only pass a single character string from Fortran.

    The length of the formatted message cannot exceed QUEUESTRINGSIZE characters.

.seealso: PetscSynchronizedFlush(), PetscSynchronizedFPrintf(), PetscFPrintf(), 
          PetscPrintf(), PetscViewerASCIIPrintf(), PetscViewerASCIISynchronizedPrintf()
@*/
PetscErrorCode PetscSynchronizedPrintf(MPI_Comm comm,const char format[],...)
{
  PetscErrorCode ierr,rank;

  PetscFunctionBegin;
  ierr = MPI_Comm_rank(comm,&rank);CHKERRQ(ierr);
  
  /* First processor prints immediately to stdout */
  if (!rank) {
    va_list Argp;
    va_start(Argp,format);
    PetscVFPrintf(stdout,format,Argp);CHKERRQ(ierr);
    fflush(stdout);
    if (petsc_history) {
      ierr = PetscVFPrintf(petsc_history,format,Argp);CHKERRQ(ierr);
      fflush(petsc_history);
    }
    va_end(Argp);
  } else { /* other processors add to local queue */
    va_list     Argp;
    PrintfQueue next;

    ierr = PetscNew(struct _PrintfQueue,&next);CHKERRQ(ierr);
    if (queue) {queue->next = next; queue = next; queue->next = 0;}
    else       {queuebase   = queue = next;}
    queuelength++;
    va_start(Argp,format);
    ierr = PetscVSNPrintf(next->string,QUEUESTRINGSIZE,format,Argp);CHKERRQ(ierr);
    va_end(Argp);
  }
    
  PetscFunctionReturn(0);
}
 
#undef __FUNCT__  
#define __FUNCT__ "PetscSynchronizedFPrintf" 
/*@C
    PetscSynchronizedFPrintf - Prints synchronized output to the specified file from
    several processors.  Output of the first processor is followed by that of the 
    second, etc.

    Not Collective

    Input Parameters:
+   comm - the communicator
.   fd - the file pointer
-   format - the usual printf() format string 

    Level: intermediate

    Notes:
    REQUIRES a intervening call to PetscSynchronizedFlush() for the information 
    from all the processors to be printed.

    The length of the formatted message cannot exceed QUEUESTRINGSIZE characters.

    Contributed by: Matthew Knepley

.seealso: PetscSynchronizedPrintf(), PetscSynchronizedFlush(), PetscFPrintf(),
          PetscFOpen(), PetscViewerASCIISynchronizedPrintf(), PetscViewerASCIIPrintf()

@*/
PetscErrorCode PetscSynchronizedFPrintf(MPI_Comm comm,FILE* fp,const char format[],...)
{
  PetscErrorCode ierr,rank;

  PetscFunctionBegin;
  ierr = MPI_Comm_rank(comm,&rank);CHKERRQ(ierr);
  
  /* First processor prints immediately to fp */
  if (!rank) {
    va_list Argp;
    va_start(Argp,format);
    ierr = PetscVFPrintf(fp,format,Argp);CHKERRQ(ierr);
    fflush(fp);
    queuefile = fp;
    if (petsc_history) {
      ierr = PetscVFPrintf(petsc_history,format,Argp);CHKERRQ(ierr);
      fflush(petsc_history);
    }
    va_end(Argp);
  } else { /* other processors add to local queue */
    va_list     Argp;
    PrintfQueue next;
    ierr = PetscNew(struct _PrintfQueue,&next);CHKERRQ(ierr);
    if (queue) {queue->next = next; queue = next; queue->next = 0;}
    else       {queuebase   = queue = next;}
    queuelength++;
    va_start(Argp,format);
    ierr = PetscVSNPrintf(next->string,QUEUESTRINGSIZE,format,Argp);CHKERRQ(ierr);
    va_end(Argp);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscSynchronizedFlush" 
/*@C
    PetscSynchronizedFlush - Flushes to the screen output from all processors 
    involved in previous PetscSynchronizedPrintf() calls.

    Collective on MPI_Comm

    Input Parameters:
.   comm - the communicator

    Level: intermediate

    Notes:
    Usage of PetscSynchronizedPrintf() and PetscSynchronizedFPrintf() with
    different MPI communicators REQUIRES an intervening call to PetscSynchronizedFlush().

.seealso: PetscSynchronizedPrintf(), PetscFPrintf(), PetscPrintf(), PetscViewerASCIIPrintf(),
          PetscViewerASCIISynchronizedPrintf()
@*/
PetscErrorCode PetscSynchronizedFlush(MPI_Comm comm)
{
  int        rank,size,i,j,n,tag,ierr;
  char       message[QUEUESTRINGSIZE];
  MPI_Status status;
  FILE       *fd;

  PetscFunctionBegin;
  ierr = MPI_Comm_rank(comm,&rank);CHKERRQ(ierr);
  ierr = MPI_Comm_size(comm,&size);CHKERRQ(ierr);

  ierr = PetscCommGetNewTag(comm,&tag);CHKERRQ(ierr);
  /* First processor waits for messages from all other processors */
  if (!rank) {
    if (queuefile) {
      fd = queuefile;
    } else {
      fd = stdout;
    }
    for (i=1; i<size; i++) {
      ierr = MPI_Recv(&n,1,MPI_INT,i,tag,comm,&status);CHKERRQ(ierr);
      for (j=0; j<n; j++) {
        ierr = MPI_Recv(message,QUEUESTRINGSIZE,MPI_CHAR,i,tag,comm,&status);CHKERRQ(ierr);
        fprintf(fd,"%s",message);
        if (petsc_history) {
          fprintf(petsc_history,"%s",message);
        }
      }
    }
    fflush(fd);
    if (petsc_history) fflush(petsc_history);
    queuefile = PETSC_NULL;
  } else { /* other processors send queue to processor 0 */
    PrintfQueue next = queuebase,previous;

    ierr = MPI_Send(&queuelength,1,MPI_INT,0,tag,comm);CHKERRQ(ierr);
    for (i=0; i<queuelength; i++) {
      ierr     = MPI_Send(next->string,QUEUESTRINGSIZE,MPI_CHAR,0,tag,comm);CHKERRQ(ierr);
      previous = next; 
      next     = next->next;
      ierr     = PetscFree(previous);CHKERRQ(ierr);
    }
    queue       = 0;
    queuelength = 0;
  }
  PetscFunctionReturn(0);
}

/* ---------------------------------------------------------------------------------------*/

#undef __FUNCT__  
#define __FUNCT__ "PetscFPrintf" 
/*@C
    PetscFPrintf - Prints to a file, only from the first
    processor in the communicator.

    Not Collective

    Input Parameters:
+   comm - the communicator
.   fd - the file pointer
-   format - the usual printf() format string 

    Level: intermediate

    Fortran Note:
    This routine is not supported in Fortran.

   Concepts: printing^in parallel
   Concepts: printf^in parallel

.seealso: PetscPrintf(), PetscSynchronizedPrintf(), PetscViewerASCIIPrintf(),
          PetscViewerASCIISynchronizedPrintf(), PetscSynchronizedFlush()
@*/
PetscErrorCode PetscFPrintf(MPI_Comm comm,FILE* fd,const char format[],...)
{
  int rank,ierr;

  PetscFunctionBegin;
  ierr = MPI_Comm_rank(comm,&rank);CHKERRQ(ierr);
  if (!rank) {
    va_list Argp;
    va_start(Argp,format);
    ierr = PetscVFPrintf(fd,format,Argp);CHKERRQ(ierr);
    fflush(fd);
    if (petsc_history) {
      ierr = PetscVFPrintf(petsc_history,format,Argp);CHKERRQ(ierr);
      fflush(petsc_history);
    }
    va_end(Argp);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscPrintf" 
/*@C
    PetscPrintf - Prints to standard out, only from the first
    processor in the communicator.

    Not Collective

    Input Parameters:
+   comm - the communicator
-   format - the usual printf() format string 

   Level: intermediate

    Fortran Note:
    The call sequence is PetscPrintf(PetscViewer, character(*), PetscErrorCode ierr) from Fortran. 
    That is, you can only pass a single character string from Fortran.

   Notes: %A is replace with %g unless the value is < 1.e-12 when it is 
          replaced with < 1.e-12

   Concepts: printing^in parallel
   Concepts: printf^in parallel

.seealso: PetscFPrintf(), PetscSynchronizedPrintf()
@*/
PetscErrorCode PetscPrintf(MPI_Comm comm,const char format[],...)
{
  int       rank,ierr;
  size_t    len;
  char      *nformat,*sub1,*sub2;
  PetscReal value;

  PetscFunctionBegin;
  if (!comm) comm = PETSC_COMM_WORLD;
  ierr = MPI_Comm_rank(comm,&rank);CHKERRQ(ierr);
  if (!rank) {
    va_list Argp;
    va_start(Argp,format);

    ierr = PetscStrstr(format,"%A",&sub1);CHKERRQ(ierr);
    if (sub1) {
      ierr = PetscStrstr(format,"%",&sub2);CHKERRQ(ierr);
      if (sub1 != sub2) SETERRQ(PETSC_ERR_ARG_WRONG,"%%A format must be first in format string");
      ierr    = PetscStrlen(format,&len);CHKERRQ(ierr);
      ierr    = PetscMalloc((len+16)*sizeof(char),&nformat);CHKERRQ(ierr);
      ierr    = PetscStrcpy(nformat,format);CHKERRQ(ierr);
      ierr    = PetscStrstr(nformat,"%",&sub2);CHKERRQ(ierr);
      sub2[0] = 0;
      value   = (double)va_arg(Argp,double);
      if (PetscAbsReal(value) < 1.e-12) {
        ierr    = PetscStrcat(nformat,"< 1.e-12");CHKERRQ(ierr);
      } else {
        ierr    = PetscStrcat(nformat,"%g");CHKERRQ(ierr);
        va_end(Argp);
        va_start(Argp,format);
      }
      ierr    = PetscStrcat(nformat,sub1+2);CHKERRQ(ierr);
    } else {
      nformat = (char*)format;
    }
    ierr = PetscVFPrintf(stdout,nformat,Argp);CHKERRQ(ierr);
    fflush(stdout);
    if (petsc_history) {
      ierr = PetscVFPrintf(petsc_history,nformat,Argp);CHKERRQ(ierr);
      fflush(petsc_history);
    }
    va_end(Argp);
    if (sub1) {ierr = PetscFree(nformat);CHKERRQ(ierr);}
  }
  PetscFunctionReturn(0);
}

/* ---------------------------------------------------------------------------------------*/
#undef __FUNCT__  
#define __FUNCT__ "PetscHelpPrintfDefault" 
PetscErrorCode PetscHelpPrintfDefault(MPI_Comm comm,const char format[],...)
{
  int rank,ierr;

  PetscFunctionBegin;
  if (!comm) comm = PETSC_COMM_WORLD;
  ierr = MPI_Comm_rank(comm,&rank);CHKERRQ(ierr);
  if (!rank) {
    va_list Argp;
    va_start(Argp,format);
    ierr = PetscVFPrintf(stdout,format,Argp);CHKERRQ(ierr);
    fflush(stdout);
    if (petsc_history) {
      PetscVFPrintf(petsc_history,format,Argp);CHKERRQ(ierr);
      fflush(petsc_history);
    }
    va_end(Argp);
  }
  PetscFunctionReturn(0);
}

/* ---------------------------------------------------------------------------------------*/

#undef __FUNCT__  
#define __FUNCT__ "PetscErrorPrintfDefault" 
PetscErrorCode PetscErrorPrintfDefault(const char format[],...)
{
  va_list            Argp;
  static  PetscTruth PetscErrorPrintfCalled    = PETSC_FALSE;
  static  PetscTruth InPetscErrorPrintfDefault = PETSC_FALSE;
  static  FILE       *fd;
  char               version[256];
  /*
      InPetscErrorPrintfDefault is used to prevent the error handler called (potentially)
     from PetscSleep(), PetscGetArchName(), ... below from printing its own error message.
  */

  /*
      This function does not call PetscFunctionBegin and PetscFunctionReturn() because
    it may be called by PetscStackView().

      This function does not do error checking because it is called by the error handlers.
  */

  if (!PetscErrorPrintfCalled) {
    char       arch[10],hostname[64],username[16],pname[PETSC_MAX_PATH_LEN],date[64];
    PetscTruth use_stderr;

    PetscErrorPrintfCalled    = PETSC_TRUE;
    InPetscErrorPrintfDefault = PETSC_TRUE;

    PetscOptionsHasName(PETSC_NULL,"-error_output_stderr",&use_stderr);
    if (use_stderr) {
      fd = stderr;
    } else {
      fd = stdout;
    }

    /*
        On the SGI machines and Cray T3E, if errors are generated  "simultaneously" by
      different processors, the messages are printed all jumbled up; to try to 
      prevent this we have each processor wait based on their rank
    */
#if defined(PETSC_CAN_SLEEP_AFTER_ERROR)
    {
      int rank;
      if (PetscGlobalRank > 8) rank = 8; else rank = PetscGlobalRank;
      PetscSleep(rank);
    }
#endif
    
    PetscGetVersion(&version);

    /* Cannot do error checking on these calls because we are called by error handler */
    PetscGetArchType(arch,10);
    PetscGetHostName(hostname,64);
    PetscGetUserName(username,16);
    PetscGetProgramName(pname,PETSC_MAX_PATH_LEN);
    PetscGetInitialDate(date,64);
    fprintf(fd,"--------------------------------------------\
------------------------------\n");
    fprintf(fd,"%s\n",version);
    fprintf(fd,"See docs/changes/index.html for recent updates.\n");
    fprintf(fd,"See docs/troubleshooting.html for hints about trouble shooting.\n");
    fprintf(fd,"See docs/index.html for manual pages.\n");
    fprintf(fd,"--------------------------------------------\
---------------------------\n");
    fprintf(fd,"%s on a %s named %s by %s %s\n",pname,arch,hostname,username,date);
#if !defined (PARCH_win32)
    fprintf(fd,"Libraries linked from %s\n",PETSC_LIB_DIR);
#endif
    fprintf(fd,"--------------------------------------------\
---------------------------\n");
    fflush(fd);
    InPetscErrorPrintfDefault = PETSC_FALSE;
  }

  if (!InPetscErrorPrintfDefault) {
    va_start(Argp,format);
    fprintf(fd,"[%d]PETSC ERROR: ",PetscGlobalRank);
    PetscVFPrintf(fd,format,Argp);
    fflush(fd);
    va_end(Argp);
  }
  return 0;
}

#undef __FUNCT__  
#define __FUNCT__ "PetscSynchronizedFGets" 
/*@C
    PetscSynchronizedFGets - Several processors all get the same line from a file.

    Collective on MPI_Comm

    Input Parameters:
+   comm - the communicator
.   fd - the file pointer
-   len - the lenght of the output buffer

    Output Parameter:
.   string - the line read from the file

    Level: intermediate

.seealso: PetscSynchronizedPrintf(), PetscSynchronizedFlush(), 
          PetscFOpen(), PetscViewerASCIISynchronizedPrintf(), PetscViewerASCIIPrintf()

@*/
PetscErrorCode PetscSynchronizedFGets(MPI_Comm comm,FILE* fp,int len,char string[])
{
  PetscErrorCode ierr,rank;

  PetscFunctionBegin;
  ierr = MPI_Comm_rank(comm,&rank);CHKERRQ(ierr);
  
  /* First processor prints immediately to fp */
  if (!rank) {
    fgets(string,len,fp);
  }
  ierr = MPI_Bcast(string,len,MPI_BYTE,0,comm);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
