/* 
   This file contains simple binary input routines for vectors.  The
   analogous output routines are within each vector implementation's 
   VecView (with viewer types PETSC_VIEWER_BINARY)
 */

#include "petsc.h"
#include "petscsys.h"
#include "petscvec.h"         /*I  "petscvec.h"  I*/
#include "vecimpl.h"
#if defined(PETSC_HAVE_PNETCDF)
EXTERN_C_BEGIN
#include "pnetcdf.h"
EXTERN_C_END
#endif
int VecLoad_Binary(PetscViewer,const VecType, Vec*);
int VecLoad_Netcdf(PetscViewer, Vec*);
int VecLoadIntoVector_Binary(PetscViewer, Vec);
int VecLoadIntoVector_Netcdf(PetscViewer, Vec);

#undef __FUNCT__  
#define __FUNCT__ "VecLoad"
/*@C 
  VecLoad - Loads a vector that has been stored in binary format
  with VecView().

  Collective on PetscViewer 

  Input Parameters:
+ viewer - binary file viewer, obtained from PetscViewerBinaryOpen() or
           NetCDF file viewer, obtained from PetscViewerNetcdfOpen()
- outtype - the type of vector VECSEQ or VECMPI or PETSC_NULL

  Output Parameter:
. newvec - the newly loaded vector

   Level: intermediate

  Notes:
  The input file must contain the full global vector, as
  written by the routine VecView().

  Notes for advanced users:
  Most users should not need to know the details of the binary storage
  format, since VecLoad() and VecView() completely hide these details.
  But for anyone who's interested, the standard binary matrix storage
  format is
.vb
     int    VEC_FILE_COOKIE
     int    number of rows
     PetscScalar *values of all nonzeros
.ve

   Note for Cray users, the int's stored in the binary file are 32 bit
integers; not 64 as they are represented in the memory, so if you
write your own routines to read/write these binary files from the Cray
you need to adjust the integer sizes that you read in, see
PetscReadBinary() and PetscWriteBinary() to see how this may be
done.

   In addition, PETSc automatically does the byte swapping for
machines that store the bytes reversed, e.g.  DEC alpha, freebsd,
linux, Windows and the paragon; thus if you write your own binary
read/write routines you have to swap the bytes; see PetscReadBinary()
and PetscWriteBinary() to see how this may be done.

  Concepts: vector^loading from file

.seealso: PetscViewerBinaryOpen(), VecView(), MatLoad(), VecLoadIntoVector() 
@*/  
int VecLoad(PetscViewer viewer,const VecType outtype,Vec *newvec)
{
  int         ierr;
  PetscTruth  isbinary,isnetcdf,flg;
  char        vtype[256],*prefix;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer,PETSC_VIEWER_COOKIE,1);
  PetscValidPointer(newvec,3);
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_BINARY,&isbinary);CHKERRQ(ierr);
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_NETCDF,&isnetcdf);CHKERRQ(ierr);
  if ((!isbinary) && (!isnetcdf)) SETERRQ(PETSC_ERR_ARG_WRONG,"Must be binary or NetCDF viewer");

#ifndef PETSC_USE_DYNAMIC_LIBRARIES
  ierr = VecInitializePackage(PETSC_NULL);CHKERRQ(ierr);
#endif
  if (isnetcdf) {
    ierr = VecLoad_Netcdf(viewer,newvec);CHKERRQ(ierr);
  } else {
    Vec      factory;
    MPI_Comm comm;
    int      (*r)(PetscViewer,const VecType,Vec*),size;

    ierr = PetscObjectGetOptionsPrefix((PetscObject)viewer,&prefix);CHKERRQ(ierr);
    ierr = PetscOptionsGetString(prefix,"-vec_type",vtype,256,&flg);CHKERRQ(ierr);
    if (flg) {
      outtype = vtype;
    }
    ierr = PetscOptionsGetString(prefix,"-vecload_type",vtype,256,&flg);CHKERRQ(ierr);
    if (flg) {
      outtype = vtype;
    }
    ierr = PetscObjectGetComm((PetscObject)viewer,&comm);CHKERRQ(ierr);  
    if (!outtype) {
      ierr = MPI_Comm_size(comm,&size);CHKERRQ(ierr);
      outtype = (size > 1) ? VECMPI : VECSEQ;
    }

    ierr = VecCreate(comm,&factory);CHKERRQ(ierr);
    ierr = VecSetSizes(factory,1,PETSC_DETERMINE);CHKERRQ(ierr);
    ierr = VecSetType(factory,outtype);CHKERRQ(ierr);
    r = factory->ops->load;
    ierr = VecDestroy(factory);
    if (!r) SETERRQ1(PETSC_ERR_SUP,"VecLoad is not supported for type: %s",outtype);
    ierr = (*r)(viewer,outtype,newvec);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecLoad_Netcdf"
int VecLoad_Netcdf(PetscViewer viewer,Vec *newvec)
{
#if defined(PETSC_HAVE_PNETCDF)
  int         i,N,ierr,n,rank,bs;
  int         ncid,start;
  Vec         vec;
  PetscScalar *avec;
  MPI_Comm    comm;
  MPI_Request request;
  MPI_Status  status;
  PetscMap    map;
  PetscTruth  isnetcdf,flag;
  char        name[NC_MAX_NAME];

  PetscFunctionBegin;
  ierr = PetscLogEventBegin(VEC_Load,viewer,0,0,0);CHKERRQ(ierr);
  ierr = PetscObjectGetComm((PetscObject)viewer,&comm);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(comm,&rank);CHKERRQ(ierr);
  ierr = PetscViewerNetcdfGetID(viewer,&ncid);CHKERRQ(ierr);
  ierr = ncmpi_inq_dim(ncid,0,name,(size_t*)&N);CHKERRQ(ierr); /* N gets the global vector size */
  ierr = VecCreate(comm,&vec);CHKERRQ(ierr);
  ierr = VecSetSizes(vec,PETSC_DECIDE,N);CHKERRQ(ierr);
  if (!rank) {
    ierr = PetscOptionsGetInt(PETSC_NULL,"-vecload_block_size",&bs,&flag);CHKERRQ(ierr);
    if (flag) {
      ierr = VecSetBlockSize(vec,bs);CHKERRQ(ierr);
    }
  }
  ierr = VecSetFromOptions(vec);CHKERRQ(ierr);
  ierr = VecGetLocalSize(vec,&n);CHKERRQ(ierr);
  ierr = VecGetOwnershipRange(vec,&start,PETSC_NULL);CHKERRQ(ierr);
  ierr = VecGetArray(vec,&avec);CHKERRQ(ierr);
  ierr = ncmpi_get_vara_double_all(ncid,0,(const size_t*)&start,(const size_t*)&n,(double *)avec);CHKERRQ(ierr);
  ierr = VecRestoreArray(vec,&avec);CHKERRQ(ierr);
  *newvec = vec;
  ierr = VecAssemblyBegin(vec);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(vec);CHKERRQ(ierr);
  ierr = PetscLogEventEnd(VEC_Load,viewer,0,0,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
#else
  PetscFunctionBegin;
  SETERRQ(PETSC_ERR_SUP_SYS,"Build PETSc with NetCDF to use this viewer");
#endif
}

#undef __FUNCT__  
#define __FUNCT__ "VecLoad_Binary"
int VecLoad_Binary(PetscViewer viewer,const VecType itype,Vec *newvec)
{
  int         i,rows,ierr,type,fd,rank,size,n,*range,tag,bs,nierr;
  Vec         vec;
  PetscScalar *avec;
  MPI_Comm    comm;
  MPI_Request request;
  MPI_Status  status;
  PetscMap    map;
  PetscTruth  flag;

  PetscFunctionBegin;
  ierr = PetscLogEventBegin(VEC_Load,viewer,0,0,0);CHKERRQ(ierr);
  ierr = PetscViewerBinaryGetDescriptor(viewer,&fd);CHKERRQ(ierr);
  ierr = PetscObjectGetComm((PetscObject)viewer,&comm);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(comm,&rank);CHKERRQ(ierr);
  ierr = MPI_Comm_size(comm,&size);CHKERRQ(ierr);

  if (!rank) {
    /* Read vector header. */
    ierr = PetscBinaryRead(fd,&type,1,PETSC_INT);if (ierr) goto handleerror;
    if (type != VEC_FILE_COOKIE) {ierr = PETSC_ERR_ARG_WRONG; goto handleerror;}
    ierr = PetscBinaryRead(fd,&rows,1,PETSC_INT);if (ierr) goto handleerror;
    ierr = MPI_Bcast(&rows,1,MPI_INT,0,comm);CHKERRQ(ierr);
    ierr = VecCreate(comm,&vec);CHKERRQ(ierr);
    ierr = VecSetSizes(vec,PETSC_DECIDE,rows);CHKERRQ(ierr);
    ierr = PetscOptionsGetInt(PETSC_NULL,"-vecload_block_size",&bs,&flag);CHKERRQ(ierr);
    if (flag) {
      ierr = VecSetBlockSize(vec,bs);CHKERRQ(ierr);
    }
    ierr = VecSetFromOptions(vec);CHKERRQ(ierr);
    ierr = VecGetLocalSize(vec,&n);CHKERRQ(ierr);
    ierr = VecGetArray(vec,&avec);CHKERRQ(ierr);
    ierr = PetscBinaryRead(fd,avec,n,PETSC_SCALAR);CHKERRQ(ierr);
    ierr = VecRestoreArray(vec,&avec);CHKERRQ(ierr);

    if (size > 1) {
      /* read in other chuncks and send to other processors */
      /* determine maximum chunck owned by other */
      ierr = VecGetPetscMap(vec,&map);CHKERRQ(ierr);
      ierr = PetscMapGetGlobalRange(map,&range);CHKERRQ(ierr);
      n = 1;
      for (i=1; i<size; i++) {
        n = PetscMax(n,range[i] - range[i-1]);
      }
      ierr = PetscMalloc(n*sizeof(PetscScalar),&avec);CHKERRQ(ierr);
      ierr = PetscObjectGetNewTag((PetscObject)viewer,&tag);CHKERRQ(ierr);
      for (i=1; i<size; i++) {
        n    = range[i+1] - range[i];
        ierr = PetscBinaryRead(fd,avec,n,PETSC_SCALAR);CHKERRQ(ierr);
        ierr = MPI_Isend(avec,n,MPIU_SCALAR,i,tag,comm,&request);CHKERRQ(ierr);
        ierr = MPI_Wait(&request,&status);CHKERRQ(ierr);
      }
      ierr = PetscFree(avec);CHKERRQ(ierr);
    }
  } else {
    ierr = MPI_Bcast(&rows,1,MPI_INT,0,comm);CHKERRQ(ierr);
    /* this is a marker sent to indicate that the file does not have a vector at this location */
    if (rows == -1)  SETERRQ(PETSC_ERR_FILE_UNEXPECTED,"Error loading vector");
    ierr = VecCreate(comm,&vec);CHKERRQ(ierr);
    ierr = VecSetSizes(vec,PETSC_DECIDE,rows);CHKERRQ(ierr);
    ierr = VecSetFromOptions(vec);CHKERRQ(ierr);
    ierr = VecGetLocalSize(vec,&n);CHKERRQ(ierr); 
    ierr = PetscObjectGetNewTag((PetscObject)viewer,&tag);CHKERRQ(ierr);
    ierr = VecGetArray(vec,&avec);CHKERRQ(ierr);
    ierr = MPI_Recv(avec,n,MPIU_SCALAR,0,tag,comm,&status);CHKERRQ(ierr);
    ierr = VecRestoreArray(vec,&avec);CHKERRQ(ierr);
  }
  *newvec = vec;
  ierr = VecAssemblyBegin(vec);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(vec);CHKERRQ(ierr);
  PetscFunctionReturn(0);
  /* tell the other processors we've had an error */
  handleerror:
    nierr = PetscLogEventEnd(VEC_Load,viewer,0,0,0);CHKERRQ(nierr);
    nierr = -1; MPI_Bcast(&nierr,1,MPI_INT,0,comm);
    SETERRQ(ierr,"Error loading vector");
}

#undef __FUNCT__  
#define __FUNCT__ "VecLoadIntoVector_Default"
int VecLoadIntoVector_Default(PetscViewer viewer,Vec vec)
{
  PetscTruth isbinary,isnetcdf;
  int        ierr;

  PetscFunctionBegin;

  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_BINARY,&isbinary);CHKERRQ(ierr);
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_NETCDF,&isnetcdf);CHKERRQ(ierr);
  if ((!isbinary) && (!isnetcdf)) SETERRQ(PETSC_ERR_ARG_WRONG,"Must be binary or NetCDF viewer");

  if (isnetcdf) {
    ierr = VecLoadIntoVector_Netcdf(viewer,vec);CHKERRQ(ierr);
  } else {
    ierr = VecLoadIntoVector_Binary(viewer,vec);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecLoadIntoVector_Netcdf"
int VecLoadIntoVector_Netcdf(PetscViewer viewer,Vec vec)
{
#if defined(PETSC_HAVE_PNETCDF)
  int         i,N,rows,ierr,n,rank,bs;
  int         ncid,start;
  PetscScalar *avec;
  MPI_Comm    comm;
  MPI_Request request;
  MPI_Status  status;
  PetscMap    map;
  PetscTruth  isnetcdf,flag;
  char        name[NC_MAX_NAME];

  PetscFunctionBegin;
  ierr = PetscLogEventBegin(VEC_Load,viewer,vec,0,0);CHKERRQ(ierr);
  ierr = PetscObjectGetComm((PetscObject)viewer,&comm);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(comm,&rank);CHKERRQ(ierr);
  ierr = PetscViewerNetcdfGetID(viewer,&ncid);CHKERRQ(ierr);
  ierr = ncmpi_inq_dim(ncid,0,name,(size_t*)&N);CHKERRQ(ierr); /* N gets the global vector size */
  if (!rank) {
    ierr = VecGetSize(vec,&rows);CHKERRQ(ierr);
    if (N != rows) SETERRQ(PETSC_ERR_FILE_UNEXPECTED,"Vector in file different length then input vector");
    ierr = PetscOptionsGetInt(PETSC_NULL,"-vecload_block_size",&bs,&flag);CHKERRQ(ierr);
    if (flag) {
      ierr = VecSetBlockSize(vec,bs);CHKERRQ(ierr);
    }
  }
  ierr = VecSetFromOptions(vec);CHKERRQ(ierr);
  ierr = VecGetLocalSize(vec,&n);CHKERRQ(ierr);
  ierr = VecGetOwnershipRange(vec,&start,PETSC_NULL);CHKERRQ(ierr);
  ierr = VecGetArray(vec,&avec);CHKERRQ(ierr);
  ierr = ncmpi_get_vara_double_all(ncid,0,(const size_t*)&start,(const size_t*)&n,(double *)avec);CHKERRQ(ierr);
  ierr = VecRestoreArray(vec,&avec);CHKERRQ(ierr);
  ierr = VecAssemblyBegin(vec);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(vec);CHKERRQ(ierr);
  ierr = PetscLogEventEnd(VEC_Load,viewer,vec,0,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
#else
  PetscFunctionBegin;
  SETERRQ(PETSC_ERR_SUP_SYS,"Build PETSc with NetCDF to use this viewer");
#endif
}

#undef __FUNCT__  
#define __FUNCT__ "VecLoadIntoVector_Binary"
int VecLoadIntoVector_Binary(PetscViewer viewer,Vec vec)
{
  int         i,rows,ierr,type,fd,rank,size,n,*range,tag,bs;
  PetscScalar *avec;
  MPI_Comm    comm;
  MPI_Request request;
  MPI_Status  status;
  PetscMap    map;
  PetscTruth  flag;
  char        *prefix;

  PetscFunctionBegin;
  ierr = PetscLogEventBegin(VEC_Load,viewer,vec,0,0);CHKERRQ(ierr);

  ierr = PetscViewerBinaryGetDescriptor(viewer,&fd);CHKERRQ(ierr);
  ierr = PetscObjectGetComm((PetscObject)viewer,&comm);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(comm,&rank);CHKERRQ(ierr);
  ierr = MPI_Comm_size(comm,&size);CHKERRQ(ierr);

  if (!rank) {
    /* Read vector header. */
    ierr = PetscBinaryRead(fd,&type,1,PETSC_INT);CHKERRQ(ierr);
    if (type != VEC_FILE_COOKIE) SETERRQ(PETSC_ERR_ARG_WRONG,"Non-vector object");
    ierr = PetscBinaryRead(fd,&rows,1,PETSC_INT);CHKERRQ(ierr);
    ierr = VecGetSize(vec,&n);CHKERRQ(ierr);
    if (n != rows) SETERRQ(PETSC_ERR_FILE_UNEXPECTED,"Vector in file different length then input vector");
    ierr = MPI_Bcast(&rows,1,MPI_INT,0,comm);CHKERRQ(ierr);

    ierr = PetscObjectGetOptionsPrefix((PetscObject)vec,&prefix);CHKERRQ(ierr);
    ierr = PetscOptionsGetInt(prefix,"-vecload_block_size",&bs,&flag);CHKERRQ(ierr);
    if (flag) {
      ierr = VecSetBlockSize(vec,bs);CHKERRQ(ierr);
    }
    ierr = VecSetFromOptions(vec);CHKERRQ(ierr);
    ierr = VecGetLocalSize(vec,&n);CHKERRQ(ierr);
    ierr = VecGetArray(vec,&avec);CHKERRQ(ierr);
    ierr = PetscBinaryRead(fd,avec,n,PETSC_SCALAR);CHKERRQ(ierr);
    ierr = VecRestoreArray(vec,&avec);CHKERRQ(ierr);

    if (size > 1) {
      /* read in other chuncks and send to other processors */
      /* determine maximum chunck owned by other */
      ierr = VecGetPetscMap(vec,&map);CHKERRQ(ierr);
      ierr = PetscMapGetGlobalRange(map,&range);CHKERRQ(ierr);
      n = 1;
      for (i=1; i<size; i++) {
        n = PetscMax(n,range[i] - range[i-1]);
      }
      ierr = PetscMalloc(n*sizeof(PetscScalar),&avec);CHKERRQ(ierr);
      ierr = PetscObjectGetNewTag((PetscObject)viewer,&tag);CHKERRQ(ierr);
      for (i=1; i<size; i++) {
        n    = range[i+1] - range[i];
        ierr = PetscBinaryRead(fd,avec,n,PETSC_SCALAR);CHKERRQ(ierr);
        ierr = MPI_Isend(avec,n,MPIU_SCALAR,i,tag,comm,&request);CHKERRQ(ierr);
        ierr = MPI_Wait(&request,&status);CHKERRQ(ierr);
      }
      ierr = PetscFree(avec);CHKERRQ(ierr);
    }
  } else {
    ierr = MPI_Bcast(&rows,1,MPI_INT,0,comm);CHKERRQ(ierr);
    ierr = VecSetFromOptions(vec);CHKERRQ(ierr);
    ierr = VecGetLocalSize(vec,&n);CHKERRQ(ierr); 
    ierr = PetscObjectGetNewTag((PetscObject)viewer,&tag);CHKERRQ(ierr);
    ierr = VecGetArray(vec,&avec);CHKERRQ(ierr);
    ierr = MPI_Recv(avec,n,MPIU_SCALAR,0,tag,comm,&status);CHKERRQ(ierr);
    ierr = VecRestoreArray(vec,&avec);CHKERRQ(ierr);
  }
  ierr = VecAssemblyBegin(vec);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(vec);CHKERRQ(ierr);
  ierr = PetscLogEventEnd(VEC_Load,viewer,vec,0,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
