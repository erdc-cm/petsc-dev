/* $Id: smatlab.c,v 1.12 2001/03/23 23:20:30 balay Exp $ */

#include "petsc.h"
#include "petscsys.h"

#undef __FUNCT__
#define __FUNCT__ "PetscStartMatlab"	
/*@C
    PetscStartMatlab - starts up Matlab with a Matlab script

    Collective on MPI_Comm, but only processor zero in the communicator does anything

    Input Parameters:
+     comm - MPI communicator
.     machine - optional machine to run Matlab on
-     script - name of script (without the .m)

    Output Parameter:
.     fp - a file pointer returned from PetscPOpen()

    Level: intermediate

    Notes: 
     This overwrites your matlab/startup.m file

     The script must be in your Matlab path or current directory

     Assumes that all machines share a common file system

.seealso: PetscPOpen(), PetscPClose()
@*/
int PetscStartMatlab(MPI_Comm comm,char *machine,char *script,FILE **fp)
{
  int  ierr;
  FILE *fd;
  char command[512];
#if defined(HAVE_UCBPS)
  char buf[1024],*found;
  int  rank;
#endif

  PetscFunctionBegin;

#if defined(HAVE_UCBPS)
  /* check if Matlab is not already running */
  ierr = PetscPOpen(comm,machine,"/usr/ucb/ps -ugxww | grep matlab | grep -v grep","r",&fd);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(comm,&rank);CHKERRQ(ierr);
  if (!rank) {
    found = fgets(buf,1024,fd);
  }
  ierr = MPI_Bcast(&found,1,MPI_CHAR,0,comm);CHKERRQ(ierr);
  ierr = PetscFClose(comm,fd);CHKERRQ(ierr);
  if (found) PetscFunctionReturn(0);
#endif

  if (script) {
    /* the remote machine won't know about current directory, so add it to Matlab path */
    /* the extra \" are to protect possible () in the script command from the shell */
    sprintf(command,"echo \"delete ${HOMEDIRECTORY}/matlab/startup.m ; path(path,'${WORKINGDIRECTORY}'); %s  \" > ${HOMEDIRECTORY}/matlab/startup.m",script);
    ierr = PetscPOpen(comm,machine,command,"r",&fd);CHKERRQ(ierr);
    ierr = PetscFClose(comm,fd);CHKERRQ(ierr);
  }

  ierr = PetscPOpen(comm,machine,"xterm -display ${DISPLAY} -e matlab -nosplash","r",fp);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}

