/*$Id: vecstash.c,v 1.29 2001/09/07 20:08:55 bsmith Exp $*/

#include "src/vec/vecimpl.h"

#define DEFAULT_STASH_SIZE   100

/*
  VecStashCreate_Private - Creates a stash,currently used for all the parallel 
  matrix implementations. The stash is where elements of a matrix destined 
  to be stored on other processors are kept until matrix assembly is done.

  This is a simple minded stash. Simply adds entries to end of stash.

  Input Parameters:
  comm - communicator, required for scatters.
  bs   - stash block size. used when stashing blocks of values

  Output Parameters:
  stash    - the newly created stash
*/
#undef __FUNCT__  
#define __FUNCT__ "VecStashCreate_Private"
int VecStashCreate_Private(MPI_Comm comm,int bs,VecStash *stash)
{
  int        ierr,max,*opt,nopt;
  PetscTruth flg;

  PetscFunctionBegin;
  /* Require 2 tags, get the second using PetscCommGetNewTag() */
  stash->comm = comm;
  ierr = PetscCommGetNewTag(stash->comm,&stash->tag1);CHKERRQ(ierr);
  ierr = PetscCommGetNewTag(stash->comm,&stash->tag2);CHKERRQ(ierr);
  ierr = MPI_Comm_size(stash->comm,&stash->size);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(stash->comm,&stash->rank);CHKERRQ(ierr);

  nopt = stash->size;
  ierr = PetscMalloc(nopt*sizeof(int),&opt);CHKERRQ(ierr);
  ierr = PetscOptionsGetIntArray(PETSC_NULL,"-vecstash_initial_size",opt,&nopt,&flg);CHKERRQ(ierr);
  if (flg) {
    if (nopt == 1)                max = opt[0];
    else if (nopt == stash->size) max = opt[stash->rank];
    else if (stash->rank < nopt)  max = opt[stash->rank];
    else                          max = 0; /* use default */
    stash->umax = max;
  } else {
    stash->umax = 0;
  }
  ierr = PetscFree(opt);CHKERRQ(ierr);

  if (bs <= 0) bs = 1;

  stash->bs       = bs;
  stash->nmax     = 0;
  stash->oldnmax  = 0;
  stash->n        = 0;
  stash->reallocs = -1;
  stash->idx      = 0;
  stash->array    = 0;

  stash->send_waits  = 0;
  stash->recv_waits  = 0;
  stash->send_status = 0;
  stash->nsends      = 0;
  stash->nrecvs      = 0;
  stash->svalues     = 0;
  stash->rvalues     = 0;
  stash->rmax        = 0;
  stash->nprocs      = 0;
  stash->nprocessed  = 0;
  stash->donotstash  = PETSC_FALSE;
  PetscFunctionReturn(0);
}

/* 
   VecStashDestroy_Private - Destroy the stash
*/
#undef __FUNCT__  
#define __FUNCT__ "VecStashDestroy_Private"
int VecStashDestroy_Private(VecStash *stash)
{
  int ierr;

  PetscFunctionBegin;
  if (stash->array) {
    ierr = PetscFree(stash->array);CHKERRQ(ierr);
    stash->array = 0;
  }
  if (stash->bowners) {
    ierr = PetscFree(stash->bowners);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

/* 
   VecStashScatterEnd_Private - This is called as the fial stage of
   scatter. The final stages of message passing is done here, and
   all the memory used for message passing is cleanedu up. This
   routine also resets the stash, and deallocates the memory used
   for the stash. It also keeps track of the current memory usage
   so that the same value can be used the next time through.
*/
#undef __FUNCT__  
#define __FUNCT__ "VecStashScatterEnd_Private"
int VecStashScatterEnd_Private(VecStash *stash)
{ 
  int         nsends=stash->nsends,ierr,oldnmax;
  MPI_Status  *send_status;

  PetscFunctionBegin;
  /* wait on sends */
  if (nsends) {
    ierr = PetscMalloc(2*nsends*sizeof(MPI_Status),&send_status);CHKERRQ(ierr);
    ierr = MPI_Waitall(2*nsends,stash->send_waits,send_status);CHKERRQ(ierr);
    ierr = PetscFree(send_status);CHKERRQ(ierr);
  }

  /* Now update nmaxold to be app 10% more than max n, this way the
     wastage of space is reduced the next time this stash is used.
     Also update the oldmax, only if it increases */
  if (stash->n) {
    oldnmax  = ((int)(stash->n * 1.1) + 5)*stash->bs;
    if (oldnmax > stash->oldnmax) stash->oldnmax = oldnmax;
  }

  stash->nmax       = 0;
  stash->n          = 0;
  stash->reallocs   = -1;
  stash->rmax       = 0;
  stash->nprocessed = 0;

  if (stash->array) {
    ierr = PetscFree(stash->array);CHKERRQ(ierr);
    stash->array = 0;
    stash->idx   = 0;
  }
  if (stash->send_waits) {
    ierr = PetscFree(stash->send_waits);CHKERRQ(ierr);
    stash->send_waits = 0;
  }
  if (stash->recv_waits) {
    ierr = PetscFree(stash->recv_waits);CHKERRQ(ierr);
    stash->recv_waits = 0;
  } 
  if (stash->svalues) {
    ierr = PetscFree(stash->svalues);CHKERRQ(ierr);
  stash->svalues = 0;
  }
  if (stash->rvalues) {
    ierr = PetscFree(stash->rvalues);CHKERRQ(ierr);
    stash->rvalues = 0;
  }
  if (stash->nprocs) {
    ierr = PetscFree(stash->nprocs);CHKERRQ(ierr);
    stash->nprocs = 0;
  }

  PetscFunctionReturn(0);
}

/* 
   VecStashGetInfo_Private - Gets the relavant statistics of the stash

   Input Parameters:
   stash    - the stash
   nstash   - the size of the stash
   reallocs - the number of additional mallocs incurred.
   
*/
#undef __FUNCT__  
#define __FUNCT__ "VecStashGetInfo_Private"
int VecStashGetInfo_Private(VecStash *stash,int *nstash,int *reallocs)
{
  PetscFunctionBegin;

  *nstash   = stash->n*stash->bs;
  if (stash->reallocs < 0) *reallocs = 0;
  else                     *reallocs = stash->reallocs;

  PetscFunctionReturn(0);
}


/* 
   VecStashSetInitialSize_Private - Sets the initial size of the stash

   Input Parameters:
   stash  - the stash
   max    - the value that is used as the max size of the stash. 
            this value is used while allocating memory. It specifies
            the number of vals stored, even with the block-stash
*/
#undef __FUNCT__  
#define __FUNCT__ "VecStashSetInitialSize_Private"
int VecStashSetInitialSize_Private(VecStash *stash,int max)
{
  PetscFunctionBegin;
  stash->umax = max;
  PetscFunctionReturn(0);
}

/* VecStashExpand_Private - Expand the stash. This function is called
   when the space in the stash is not sufficient to add the new values
   being inserted into the stash.
   
   Input Parameters:
   stash - the stash
   incr  - the minimum increase requested
   
   Notes: 
   This routine doubles the currently used memory. 
*/
#undef __FUNCT__  
#define __FUNCT__ "VecStashExpand_Private"
int VecStashExpand_Private(VecStash *stash,int incr)
{ 
  int         *n_idx,newnmax,bs=stash->bs,ierr;
  PetscScalar *n_array;

  PetscFunctionBegin;
  /* allocate a larger stash. */
  if (!stash->oldnmax && !stash->nmax) { /* new stash */
    if (stash->umax)                  newnmax = stash->umax/bs;             
    else                              newnmax = DEFAULT_STASH_SIZE/bs;
  } else if (!stash->nmax) { /* resuing stash */ 
    if (stash->umax > stash->oldnmax) newnmax = stash->umax/bs;
    else                              newnmax = stash->oldnmax/bs;
  } else                              newnmax = stash->nmax*2;

  if (newnmax  < (stash->nmax + incr)) newnmax += 2*incr;

  ierr  = PetscMalloc((newnmax)*(sizeof(int)+bs*sizeof(PetscScalar)),&n_array);CHKERRQ(ierr);
  n_idx = (int*)(n_array + bs*newnmax);
  ierr  = PetscMemcpy(n_array,stash->array,bs*stash->nmax*sizeof(PetscScalar));CHKERRQ(ierr);
  ierr  = PetscMemcpy(n_idx,stash->idx,stash->nmax*sizeof(int));CHKERRQ(ierr);
  if (stash->array) {ierr = PetscFree(stash->array);CHKERRQ(ierr);}
  stash->array   = n_array; 
  stash->idx     = n_idx; 
  stash->nmax    = newnmax;
  stash->reallocs++;
  PetscFunctionReturn(0);
}
/*
  VecStashScatterBegin_Private - Initiates the transfer of values to the
  correct owners. This function goes through the stash, and check the
  owners of each stashed value, and sends the values off to the owner
  processors.

  Input Parameters:
  stash  - the stash
  owners - an array of size 'no-of-procs' which gives the ownership range
           for each node.

  Notes: The 'owners' array in the cased of the blocked-stash has the 
  ranges specified blocked global indices, and for the regular stash in
  the proper global indices.
*/
#undef __FUNCT__  
#define __FUNCT__ "VecStashScatterBegin_Private"
int VecStashScatterBegin_Private(VecStash *stash,int *owners)
{ 
  int         *owner,*start,tag1=stash->tag1,tag2=stash->tag2;
  int         rank=stash->rank,size=stash->size,*nprocs,*procs,nsends,nreceives;
  int         nmax,*work,count,ierr,*sindices,*rindices,i,j,idx,bs=stash->bs;
  PetscScalar *rvalues,*svalues;
  MPI_Comm    comm = stash->comm;
  MPI_Request *send_waits,*recv_waits;

  PetscFunctionBegin;

  /*  first count number of contributors to each processor */
  ierr = PetscMalloc(2*size*sizeof(int),&nprocs);CHKERRQ(ierr);
  procs  = nprocs + size;
  ierr   = PetscMemzero(nprocs,2*size*sizeof(int));CHKERRQ(ierr);
  ierr   = PetscMalloc((stash->n+1)*sizeof(int),&owner);CHKERRQ(ierr);

  for (i=0; i<stash->n; i++) {
    idx = stash->idx[i];
    for (j=0; j<size; j++) {
      if (idx >= owners[j] && idx < owners[j+1]) {
        nprocs[j]++; procs[j] = 1; owner[i] = j; break;
      }
    }
  }
  nsends = 0;  for (i=0; i<size; i++) { nsends += procs[i];} 
  
  /* inform other processors of number of messages and max length*/
  ierr      = PetscMalloc(2*size*sizeof(int),&work);CHKERRQ(ierr);
  ierr      = MPI_Allreduce(nprocs,work,2*size,MPI_INT,PetscMaxSum_Op,comm);CHKERRQ(ierr);
  nmax      = work[rank];
  nreceives = work[size+rank]; 
  ierr      = PetscFree(work);CHKERRQ(ierr);
  /* post receives: 
     since we don't know how long each individual message is we 
     allocate the largest needed buffer for each receive. Potentially 
     this is a lot of wasted space.
  */
  ierr     = PetscMalloc((nreceives+1)*(nmax+1)*(bs*sizeof(PetscScalar)+sizeof(int)),&rvalues);CHKERRQ(ierr);
  rindices = (int*)(rvalues + bs*nreceives*nmax);
  ierr     = PetscMalloc((nreceives+1)*2*sizeof(MPI_Request),&recv_waits);CHKERRQ(ierr);
  for (i=0,count=0; i<nreceives; i++) {
    ierr = MPI_Irecv(rvalues+bs*nmax*i,bs*nmax,MPIU_SCALAR,MPI_ANY_SOURCE,tag1,comm,recv_waits+count++);CHKERRQ(ierr);
    ierr = MPI_Irecv(rindices+nmax*i,nmax,MPI_INT,MPI_ANY_SOURCE,tag2,comm,recv_waits+count++);CHKERRQ(ierr);
  }

  /* do sends:
      1) starts[i] gives the starting index in svalues for stuff going to 
         the ith processor
  */
  ierr       = PetscMalloc((stash->n+1)*(bs*sizeof(PetscScalar)+sizeof(int)),&svalues);CHKERRQ(ierr);
  sindices   = (int*)(svalues + bs*stash->n);
  ierr = PetscMalloc(2*(nsends+1)*sizeof(MPI_Request),&send_waits);CHKERRQ(ierr);
  ierr = PetscMalloc(size*sizeof(int),&start);CHKERRQ(ierr);
  /* use 2 sends the first with all_v, the next with all_i */
  start[0] = 0;
  for (i=1; i<size; i++) { 
    start[i] = start[i-1] + nprocs[i-1];
  } 
  for (i=0; i<stash->n; i++) {
    j = owner[i];
    if (bs == 1) {
      svalues[start[j]] = stash->array[i];
    } else {
      ierr = PetscMemcpy(svalues+bs*start[j],stash->array+bs*i,bs*sizeof(PetscScalar));CHKERRQ(ierr);
    }
    sindices[start[j]]  = stash->idx[i];
    start[j]++;
  }
  start[0] = 0;
  for (i=1; i<size; i++) { start[i] = start[i-1] + nprocs[i-1];} 
  for (i=0,count=0; i<size; i++) {
    if (procs[i]) {
      ierr = MPI_Isend(svalues+bs*start[i],bs*nprocs[i],MPIU_SCALAR,i,tag1,comm,send_waits+count++);CHKERRQ(ierr);
      ierr = MPI_Isend(sindices+start[i],nprocs[i],MPI_INT,i,tag2,comm,send_waits+count++);CHKERRQ(ierr);
    }
  }
  ierr = PetscFree(owner);CHKERRQ(ierr);
  ierr = PetscFree(start);CHKERRQ(ierr);
  /* This memory is reused in scatter end  for a different purpose*/
  for (i=0; i<2*size; i++) nprocs[i] = -1;
  stash->nprocs      = nprocs;

  stash->svalues    = svalues;    stash->rvalues    = rvalues;
  stash->nsends     = nsends;     stash->nrecvs     = nreceives;
  stash->send_waits = send_waits; stash->recv_waits = recv_waits;
  stash->rmax       = nmax;
  PetscFunctionReturn(0);
}

/* 
   VecStashScatterGetMesg_Private - This function waits on the receives posted 
   in the function VecStashScatterBegin_Private() and returns one message at 
   a time to the calling function. If no messages are left, it indicates this
   by setting flg = 0, else it sets flg = 1.

   Input Parameters:
   stash - the stash

   Output Parameters:
   nvals - the number of entries in the current message.
   rows  - an array of row indices (or blocked indices) corresponding to the values
   cols  - an array of columnindices (or blocked indices) corresponding to the values
   vals  - the values
   flg   - 0 indicates no more message left, and the current call has no values associated.
           1 indicates that the current call successfully received a message, and the
             other output parameters nvals,rows,cols,vals are set appropriately.
*/
#undef __FUNCT__  
#define __FUNCT__ "VecStashScatterGetMesg_Private"
int VecStashScatterGetMesg_Private(VecStash *stash,int *nvals,int **rows,PetscScalar **vals,int *flg)
{
  int         i,ierr,size=stash->size,*flg_v,*flg_i;
  int         i1,i2,*rindices,bs=stash->bs;
  MPI_Status  recv_status;
  PetscTruth  match_found = PETSC_FALSE;

  PetscFunctionBegin;

  *flg = 0; /* When a message is discovered this is reset to 1 */
  /* Return if no more messages to process */
  if (stash->nprocessed == stash->nrecvs) { PetscFunctionReturn(0); } 

  flg_v = stash->nprocs;
  flg_i = flg_v + size;
  /* If a matching pair of receieves are found, process them, and return the data to
     the calling function. Until then keep receiving messages */
  while (!match_found) {
    ierr = MPI_Waitany(2*stash->nrecvs,stash->recv_waits,&i,&recv_status);CHKERRQ(ierr);
    /* Now pack the received message into a structure which is useable by others */
    if (i % 2) { 
      ierr = MPI_Get_count(&recv_status,MPI_INT,nvals);CHKERRQ(ierr);
      flg_i[recv_status.MPI_SOURCE] = i/2; 
    } else { 
      ierr = MPI_Get_count(&recv_status,MPIU_SCALAR,nvals);CHKERRQ(ierr);
      flg_v[recv_status.MPI_SOURCE] = i/2; 
      *nvals = *nvals/bs; 
    }
    
    /* Check if we have both the messages from this proc */
    i1 = flg_v[recv_status.MPI_SOURCE];
    i2 = flg_i[recv_status.MPI_SOURCE];
    if (i1 != -1 && i2 != -1) {
      rindices    = (int*)(stash->rvalues + bs*stash->rmax*stash->nrecvs);
      *rows       = rindices + i2*stash->rmax;
      *vals       = stash->rvalues + i1*bs*stash->rmax;
      *flg        = 1;
      stash->nprocessed ++;
      match_found = PETSC_TRUE;
    }
  }
  PetscFunctionReturn(0);
}
