
#include "petsc.h"        /*I  "petsc.h"  I*/


#undef __FUNCT__  
#define __FUNCT__ "PetscGatherNumberOfMessages"
/*@C
  PetscGatherNumberOfMessages -  Computes the number of messages a node expects to receive

  Collective on MPI_Comm

  Input Parameters:
+ comm     - Communicator
. iflags   - an array of integers of length sizeof(comm). A '1' in ilengths[i] represent a 
             message from current node to ith node. Optionally PETSC_NULL
- ilengths - Non zero ilengths[i] represent a message to i of length ilengths[i].
             Optionally PETSC_NULL.

  Output Parameters:
. nrecvs    - number of messages received

  Level: developer

  Concepts: mpi utility

  Notes:
  With this info, the correct message lengths can be determined using
  PetscGatherMessageLengths()

  Either iflags or ilengths should be provided.  If iflags is not
  provided (PETSC_NULL) it can be computed from ilengths. If iflags is
  provided, ilengths is not required.

.seealso: PetscGatherMessageLengths()
@*/
PetscErrorCode PetscGatherNumberOfMessages(MPI_Comm comm,PetscMPIInt *iflags,PetscMPIInt *ilengths,PetscMPIInt *nrecvs)
{
  PetscMPIInt    size,rank,*recv_buf,i,*iflags_local;
  PetscErrorCode ierr;

  PetscFunctionBegin;

  ierr = MPI_Comm_size(comm,&size);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(comm,&rank);CHKERRQ(ierr);

  /* If iflags not provided, compute iflags from ilengths */
  if (!iflags) {
    if (!ilengths) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Either iflags or ilengths should be provided");
    ierr = PetscMalloc(size*sizeof(PetscMPIInt),&iflags_local);CHKERRQ(ierr);
    for (i=0; i<size; i++) { 
      if (ilengths[i])  iflags_local[i] = 1;
      else iflags_local[i] = 0;
    }
  } else {
    iflags_local = iflags;
  }

  ierr = PetscMalloc(size*sizeof(PetscMPIInt),&recv_buf);CHKERRQ(ierr);

  /* Post an allreduce to determine the numer of messages the current node will receive */
  ierr    = MPI_Allreduce(iflags_local,recv_buf,size,MPI_INT,MPI_SUM,comm);CHKERRQ(ierr);
  *nrecvs = recv_buf[rank];

  if (!iflags) {
    ierr = PetscFree(iflags_local);CHKERRQ(ierr);
  }
  ierr = PetscFree(recv_buf);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "PetscGatherMessageLengths"
/*@C
  PetscGatherMessageLengths - Computes info about messages that a MPI-node will receive, 
  including (from-id,length) pairs for each message.

  Collective on MPI_Comm

  Input Parameters:
+ comm      - Communicator
. nsends    - number of messages that are to be sent.
. nrecvs    - number of messages being received
- ilengths  - an array of integers of length sizeof(comm)
              a non zero ilengths[i] represent a message to i of length ilengths[i] 


  Output Parameters:
+ onodes    - list of node-ids from which messages are expected
- olengths  - corresponding message lengths

  Level: developer

  Concepts: mpi utility

  Notes:
  With this info, the correct MPI_Irecv() can be posted with the correct
  from-id, with a buffer with the right amount of memory required.

  The calling function deallocates the memory in onodes and olengths

  To determine nrecevs, one can use PetscGatherNumberOfMessages()

.seealso: PetscGatherNumberOfMessages()
@*/
PetscErrorCode PetscGatherMessageLengths(MPI_Comm comm,PetscMPIInt nsends,PetscMPIInt nrecvs,PetscMPIInt *ilengths,PetscMPIInt **onodes,PetscMPIInt **olengths)
{
  PetscErrorCode ierr;
  PetscMPIInt    size,tag,i,j;
  MPI_Request    *s_waits,*r_waits;
  MPI_Status     *w_status;

  PetscFunctionBegin;
  ierr = MPI_Comm_size(comm,&size);CHKERRQ(ierr);
  ierr = PetscCommGetNewTag(comm,&tag);CHKERRQ(ierr);

  ierr = PetscMalloc((nrecvs+nsends+1)*sizeof(MPI_Request),&r_waits);CHKERRQ(ierr);
  s_waits = r_waits + nrecvs;

  /* Post the Irecv to get the message length-info */
  ierr = PetscMalloc((nrecvs+1)*sizeof(PetscMPIInt),olengths);CHKERRQ(ierr);
  for (i=0; i<nrecvs; i++) {
    ierr = MPI_Irecv((*olengths)+i,1,MPI_INT,MPI_ANY_SOURCE,tag,comm,r_waits+i);CHKERRQ(ierr);
  }

  /* Post the Isends with the message length-info */
  for (i=0,j=0; i<size; ++i) {
    if (ilengths[i]) {
      ierr = MPI_Isend(ilengths+i,1,MPI_INT,i,tag,comm,s_waits+j);CHKERRQ(ierr);
      j++;
    }
  }
  
  /* Post waits on sends and receivs */
  ierr = PetscMalloc((nrecvs+nsends+1)*sizeof(MPI_Status),&w_status);CHKERRQ(ierr);
  ierr = MPI_Waitall(nrecvs+nsends,r_waits,w_status);CHKERRQ(ierr);

  
  /* Pack up the received data */
  ierr = PetscMalloc((nrecvs+1)*sizeof(PetscMPIInt),onodes);CHKERRQ(ierr);
  for (i=0; i<nrecvs; ++i) {
    (*onodes)[i] = w_status[i].MPI_SOURCE;
  }

  ierr = PetscFree(r_waits);CHKERRQ(ierr);
  ierr = PetscFree(w_status);CHKERRQ(ierr);
  
  PetscFunctionReturn(0);
}

/*

  Allocate a bufffer sufficient to hold messages of size specified in olengths.
  And post Irecvs on these buffers using node info from onodes
  
 */
#undef __FUNCT__  
#define __FUNCT__ "PetscPostIrecvInt"
PetscErrorCode PetscPostIrecvInt(MPI_Comm comm,PetscMPIInt tag,PetscMPIInt nrecvs,PetscMPIInt *onodes,PetscMPIInt *olengths,PetscMPIInt ***rbuf,MPI_Request **r_waits)
{
  PetscErrorCode ierr;
  PetscMPIInt    len=0,**rbuf_t,i;
  MPI_Request    *r_waits_t;

  PetscFunctionBegin;

  /* compute memory required for recv buffers */
  for (i=0; i<nrecvs; i++) len += olengths[i];  /* each message length */
  len *= sizeof(PetscMPIInt);
  len += (nrecvs+1)*sizeof(PetscMPIInt*); /* Array of pointers for each message */

  /* allocate memory for recv buffers */
  ierr    = PetscMalloc(len,&rbuf_t);CHKERRQ(ierr);
  rbuf_t[0] = (PetscMPIInt*)(rbuf_t + nrecvs);
  for (i=1; i<nrecvs; ++i) rbuf_t[i] = rbuf_t[i-1] + olengths[i-1];

  /* Post the receives */
  ierr = PetscMalloc((nrecvs+1)*sizeof(MPI_Request),&r_waits_t);CHKERRQ(ierr);
  for (i=0; i<nrecvs; ++i) {
    ierr = MPI_Irecv(rbuf_t[i],olengths[i],MPI_INT,onodes[i],tag,comm,r_waits_t+i);CHKERRQ(ierr);
  }

  *rbuf    = rbuf_t;
  *r_waits = r_waits_t;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscPostIrecvScalar"
PetscErrorCode PetscPostIrecvScalar(MPI_Comm comm,PetscMPIInt tag,PetscMPIInt nrecvs,PetscMPIInt *onodes,PetscMPIInt *olengths,PetscScalar ***rbuf,MPI_Request **r_waits)
{
  PetscErrorCode ierr;
  PetscMPIInt    len=0,i;
  PetscScalar    **rbuf_t;
  MPI_Request    *r_waits_t;

  PetscFunctionBegin;

  /* compute memory required for recv buffers */
  for (i=0; i<nrecvs; i++) len += olengths[i];  /* each message length */
  len *= sizeof(PetscScalar);
  len += (nrecvs+1)*sizeof(PetscScalar*); /* Array of pointers for each message */


  /* allocate memory for recv buffers */
  ierr    = PetscMalloc(len,&rbuf_t);CHKERRQ(ierr);
  rbuf_t[0] = (PetscScalar*)(rbuf_t + nrecvs);
  for (i=1; i<nrecvs; ++i) rbuf_t[i] = rbuf_t[i-1] + olengths[i-1];

  /* Post the receives */
  ierr = PetscMalloc((nrecvs+1)*sizeof(MPI_Request),&r_waits_t);CHKERRQ(ierr);
  for (i=0; i<nrecvs; ++i) {
    ierr = MPI_Irecv(rbuf_t[i],olengths[i],MPIU_SCALAR,onodes[i],tag,comm,r_waits_t+i);CHKERRQ(ierr);
  }

  *rbuf    = rbuf_t;
  *r_waits = r_waits_t;
  PetscFunctionReturn(0);
}
