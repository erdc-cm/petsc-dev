/*
     Code for some of the parallel vector primatives.
*/
#include "src/vec/impls/mpi/pvecimpl.h"   /*I  "petscvec.h"   I*/
#if defined(PETSC_HAVE_PNETCDF)
EXTERN_C_BEGIN
#include "pnetcdf.h"
EXTERN_C_END
#endif

#undef __FUNCT__  
#define __FUNCT__ "VecDestroy_MPI"
PetscErrorCode VecDestroy_MPI(Vec v)
{
  Vec_MPI *x = (Vec_MPI*)v->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;

  /* if memory was published with AMS then destroy it */
  ierr = PetscObjectDepublish(v);CHKERRQ(ierr);

#if defined(PETSC_USE_LOG)
  PetscLogObjectState((PetscObject)v,"Length=%d",v->N);
#endif  
  if (x->array_allocated) {ierr = PetscFree(x->array_allocated);CHKERRQ(ierr);}

  /* Destroy local representation of vector if it exists */
  if (x->localrep) {
    ierr = VecDestroy(x->localrep);CHKERRQ(ierr);
    if (x->localupdate) {ierr = VecScatterDestroy(x->localupdate);CHKERRQ(ierr);}
  }
  /* Destroy the stashes: note the order - so that the tags are freed properly */
  ierr = VecStashDestroy_Private(&v->bstash);CHKERRQ(ierr);
  ierr = VecStashDestroy_Private(&v->stash);CHKERRQ(ierr);
  ierr = PetscFree(x);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecView_MPI_ASCII"
PetscErrorCode VecView_MPI_ASCII(Vec xin,PetscViewer viewer)
{
  PetscErrorCode    ierr;
  PetscInt               i,len,work = xin->n,n,j,cnt;
  PetscMPIInt       size,rank,tag = ((PetscObject)viewer)->tag;
  MPI_Status        status;
  PetscScalar       *values,*xarray;
  char              *name;
  PetscViewerFormat format;

  PetscFunctionBegin;
  ierr = VecGetArray(xin,&xarray);CHKERRQ(ierr);
  /* determine maximum message to arrive */
  ierr = MPI_Comm_rank(xin->comm,&rank);CHKERRQ(ierr);
  ierr = MPI_Reduce(&work,&len,1,MPI_INT,MPI_MAX,0,xin->comm);CHKERRQ(ierr);
  ierr = MPI_Comm_size(xin->comm,&size);CHKERRQ(ierr);

  if (!rank) {
    ierr = PetscMalloc((len+1)*sizeof(PetscScalar),&values);CHKERRQ(ierr);
    ierr = PetscViewerGetFormat(viewer,&format);CHKERRQ(ierr);
    /*
        Matlab format and ASCII format are very similar except 
        Matlab uses %18.16e format while ASCII uses %g
    */
    if (format == PETSC_VIEWER_ASCII_MATLAB) {
      ierr = PetscObjectGetName((PetscObject)xin,&name);CHKERRQ(ierr);
      ierr = PetscViewerASCIIPrintf(viewer,"%s = [\n",name);CHKERRQ(ierr);
      for (i=0; i<xin->n; i++) {
#if defined(PETSC_USE_COMPLEX)
        if (PetscImaginaryPart(xarray[i]) > 0.0) {
          ierr = PetscViewerASCIIPrintf(viewer,"%18.16e + %18.16ei\n",PetscRealPart(xarray[i]),PetscImaginaryPart(xarray[i]));CHKERRQ(ierr);
        } else if (PetscImaginaryPart(xarray[i]) < 0.0) {
          ierr = PetscViewerASCIIPrintf(viewer,"%18.16e - %18.16ei\n",PetscRealPart(xarray[i]),-PetscImaginaryPart(xarray[i]));CHKERRQ(ierr);
        } else {
          ierr = PetscViewerASCIIPrintf(viewer,"%18.16e\n",PetscRealPart(xarray[i]));CHKERRQ(ierr);
        }
#else
        ierr = PetscViewerASCIIPrintf(viewer,"%18.16e\n",xarray[i]);CHKERRQ(ierr);
#endif
      }
      /* receive and print messages */
      for (j=1; j<size; j++) {
        ierr = MPI_Recv(values,len,MPIU_SCALAR,j,tag,xin->comm,&status);CHKERRQ(ierr);
        ierr = MPI_Get_count(&status,MPIU_SCALAR,&n);CHKERRQ(ierr);         
        for (i=0; i<n; i++) {
#if defined(PETSC_USE_COMPLEX)
          if (PetscImaginaryPart(values[i]) > 0.0) {
            ierr = PetscViewerASCIIPrintf(viewer,"%18.16e + %18.16e i\n",PetscRealPart(values[i]),PetscImaginaryPart(values[i]));CHKERRQ(ierr);
          } else if (PetscImaginaryPart(values[i]) < 0.0) {
            ierr = PetscViewerASCIIPrintf(viewer,"%18.16e - %18.16e i\n",PetscRealPart(values[i]),-PetscImaginaryPart(values[i]));CHKERRQ(ierr);
          } else {
            ierr = PetscViewerASCIIPrintf(viewer,"%18.16e\n",PetscRealPart(values[i]));CHKERRQ(ierr);
          }
#else
          ierr = PetscViewerASCIIPrintf(viewer,"%18.16e\n",values[i]);CHKERRQ(ierr);
#endif
        }
      }          
      ierr = PetscViewerASCIIPrintf(viewer,"];\n");CHKERRQ(ierr);

    } else if (format == PETSC_VIEWER_ASCII_SYMMODU) {
      for (i=0; i<xin->n; i++) {
#if defined(PETSC_USE_COMPLEX)
        ierr = PetscViewerASCIIPrintf(viewer,"%18.16e %18.16e\n",PetscRealPart(xarray[i]),PetscImaginaryPart(xarray[i]));CHKERRQ(ierr);
#else
        ierr = PetscViewerASCIIPrintf(viewer,"%18.16e\n",xarray[i]);CHKERRQ(ierr);
#endif
      }
      /* receive and print messages */
      for (j=1; j<size; j++) {
        ierr = MPI_Recv(values,len,MPIU_SCALAR,j,tag,xin->comm,&status);CHKERRQ(ierr);
        ierr = MPI_Get_count(&status,MPIU_SCALAR,&n);CHKERRQ(ierr);         
        for (i=0; i<n; i++) {
#if defined(PETSC_USE_COMPLEX)
          ierr = PetscViewerASCIIPrintf(viewer,"%18.16e %18.16e\n",PetscRealPart(values[i]),PetscImaginaryPart(values[i]));CHKERRQ(ierr);
#else
          ierr = PetscViewerASCIIPrintf(viewer,"%18.16e\n",values[i]);CHKERRQ(ierr);
#endif
        }
      }          

    } else {
      if (format != PETSC_VIEWER_ASCII_COMMON) {ierr = PetscViewerASCIIPrintf(viewer,"Process [%d]\n",rank);CHKERRQ(ierr);}
      cnt = 0;
      for (i=0; i<xin->n; i++) {
        if (format == PETSC_VIEWER_ASCII_INDEX) {
          ierr = PetscViewerASCIIPrintf(viewer,"%d: ",cnt++);CHKERRQ(ierr);
        }
#if defined(PETSC_USE_COMPLEX)
        if (PetscImaginaryPart(xarray[i]) > 0.0) {
          ierr = PetscViewerASCIIPrintf(viewer,"%g + %g i\n",PetscRealPart(xarray[i]),PetscImaginaryPart(xarray[i]));CHKERRQ(ierr);
        } else if (PetscImaginaryPart(xarray[i]) < 0.0) {
          ierr = PetscViewerASCIIPrintf(viewer,"%g - %g i\n",PetscRealPart(xarray[i]),-PetscImaginaryPart(xarray[i]));CHKERRQ(ierr);
        } else {
          ierr = PetscViewerASCIIPrintf(viewer,"%g\n",PetscRealPart(xarray[i]));CHKERRQ(ierr);
        }
#else
        ierr = PetscViewerASCIIPrintf(viewer,"%g\n",xarray[i]);CHKERRQ(ierr);
#endif
      }
      /* receive and print messages */
      for (j=1; j<size; j++) {
        ierr = MPI_Recv(values,len,MPIU_SCALAR,j,tag,xin->comm,&status);CHKERRQ(ierr);
        ierr = MPI_Get_count(&status,MPIU_SCALAR,&n);CHKERRQ(ierr);        
        if (format != PETSC_VIEWER_ASCII_COMMON) {
          ierr = PetscViewerASCIIPrintf(viewer,"Process [%d]\n",j);CHKERRQ(ierr);
        }
        for (i=0; i<n; i++) {
          if (format == PETSC_VIEWER_ASCII_INDEX) {
            ierr = PetscViewerASCIIPrintf(viewer,"%d: ",cnt++);CHKERRQ(ierr);
          }
#if defined(PETSC_USE_COMPLEX)
          if (PetscImaginaryPart(values[i]) > 0.0) {
            ierr = PetscViewerASCIIPrintf(viewer,"%g + %g i\n",PetscRealPart(values[i]),PetscImaginaryPart(values[i]));CHKERRQ(ierr);
          } else if (PetscImaginaryPart(values[i]) < 0.0) {
            ierr = PetscViewerASCIIPrintf(viewer,"%g - %g i\n",PetscRealPart(values[i]),-PetscImaginaryPart(values[i]));CHKERRQ(ierr);
          } else {
            ierr = PetscViewerASCIIPrintf(viewer,"%g\n",PetscRealPart(values[i]));CHKERRQ(ierr);
          }
#else
          ierr = PetscViewerASCIIPrintf(viewer,"%g\n",values[i]);CHKERRQ(ierr);
#endif
        }          
      }
    }
    ierr = PetscFree(values);CHKERRQ(ierr);
  } else {
    /* send values */
    ierr = MPI_Send(xarray,xin->n,MPIU_SCALAR,0,tag,xin->comm);CHKERRQ(ierr);
  }
  ierr = PetscViewerFlush(viewer);CHKERRQ(ierr);
  ierr = VecRestoreArray(xin,&xarray);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecView_MPI_Binary"
PetscErrorCode VecView_MPI_Binary(Vec xin,PetscViewer viewer)
{
  PetscErrorCode ierr;
  PetscMPIInt    rank,size,tag = ((PetscObject)viewer)->tag;
  PetscInt       len,work = xin->n,n,j;
  int            fdes;
  MPI_Status     status;
  PetscScalar    *values,*xarray;
  FILE           *file;

  PetscFunctionBegin;
  ierr = VecGetArray(xin,&xarray);CHKERRQ(ierr);
  ierr = PetscViewerBinaryGetDescriptor(viewer,&fdes);CHKERRQ(ierr);

  /* determine maximum message to arrive */
  ierr = MPI_Comm_rank(xin->comm,&rank);CHKERRQ(ierr);
  ierr = MPI_Reduce(&work,&len,1,MPI_INT,MPI_MAX,0,xin->comm);CHKERRQ(ierr);
  ierr = MPI_Comm_size(xin->comm,&size);CHKERRQ(ierr);

  if (!rank) {
    int cookie = VEC_FILE_COOKIE;
    ierr = PetscBinaryWrite(fdes,&cookie,1,PETSC_INT,0);CHKERRQ(ierr);
    ierr = PetscBinaryWrite(fdes,&xin->N,1,PETSC_INT,0);CHKERRQ(ierr);
    ierr = PetscBinaryWrite(fdes,xarray,xin->n,PETSC_SCALAR,0);CHKERRQ(ierr);

    ierr = PetscMalloc((len+1)*sizeof(PetscScalar),&values);CHKERRQ(ierr);
    /* receive and print messages */
    for (j=1; j<size; j++) {
      ierr = MPI_Recv(values,len,MPIU_SCALAR,j,tag,xin->comm,&status);CHKERRQ(ierr);
      ierr = MPI_Get_count(&status,MPIU_SCALAR,&n);CHKERRQ(ierr);         
      ierr = PetscBinaryWrite(fdes,values,n,PETSC_SCALAR,0);CHKERRQ(ierr);
    }
    ierr = PetscFree(values);CHKERRQ(ierr);
    ierr = PetscViewerBinaryGetInfoPointer(viewer,&file);CHKERRQ(ierr);
    if (file && xin->bs > 1) {
      if (xin->prefix) {
	fprintf(file,"-%svecload_block_size %d\n",xin->prefix,xin->bs);
      } else {
	fprintf(file,"-vecload_block_size %d\n",xin->bs);
      }
    }
  } else {
    /* send values */
    ierr = MPI_Send(xarray,xin->n,MPIU_SCALAR,0,tag,xin->comm);CHKERRQ(ierr);
  }
  ierr = VecRestoreArray(xin,&xarray);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecView_MPI_Draw_LG"
PetscErrorCode VecView_MPI_Draw_LG(Vec xin,PetscViewer viewer)
{
  PetscErrorCode ierr;
  PetscMPIInt    size,rank;
  int            i,N = xin->N,*lens;
  PetscDraw      draw;
  PetscReal      *xx,*yy;
  PetscDrawLG    lg;
  PetscTruth     isnull;
  PetscScalar    *xarray;

  PetscFunctionBegin;
  ierr = PetscViewerDrawGetDraw(viewer,0,&draw);CHKERRQ(ierr);
  ierr = PetscDrawIsNull(draw,&isnull);CHKERRQ(ierr);
  if (isnull) PetscFunctionReturn(0);

  ierr = VecGetArray(xin,&xarray);CHKERRQ(ierr);
  ierr = PetscViewerDrawGetDrawLG(viewer,0,&lg);CHKERRQ(ierr);
  ierr = PetscDrawCheckResizedWindow(draw);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(xin->comm,&rank);CHKERRQ(ierr);
  ierr = MPI_Comm_size(xin->comm,&size);CHKERRQ(ierr);
  if (!rank) {
    ierr = PetscDrawLGReset(lg);CHKERRQ(ierr);
    ierr = PetscMalloc(2*(N+1)*sizeof(PetscReal),&xx);CHKERRQ(ierr);
    for (i=0; i<N; i++) {xx[i] = (PetscReal) i;}
    yy   = xx + N;
    ierr = PetscMalloc(size*sizeof(int),&lens);CHKERRQ(ierr);
    for (i=0; i<size; i++) {
      lens[i] = xin->map->range[i+1] - xin->map->range[i];
    }
#if !defined(PETSC_USE_COMPLEX)
    ierr = MPI_Gatherv(xarray,xin->n,MPIU_REAL,yy,lens,xin->map->range,MPIU_REAL,0,xin->comm);CHKERRQ(ierr);
#else
    {
      PetscReal *xr;
      ierr = PetscMalloc((xin->n+1)*sizeof(PetscReal),&xr);CHKERRQ(ierr);
      for (i=0; i<xin->n; i++) {
        xr[i] = PetscRealPart(xarray[i]);
      }
      ierr = MPI_Gatherv(xr,xin->n,MPIU_REAL,yy,lens,xin->map->range,MPIU_REAL,0,xin->comm);CHKERRQ(ierr);
      ierr = PetscFree(xr);CHKERRQ(ierr);
    }
#endif
    ierr = PetscFree(lens);CHKERRQ(ierr);
    ierr = PetscDrawLGAddPoints(lg,N,&xx,&yy);CHKERRQ(ierr);
    ierr = PetscFree(xx);CHKERRQ(ierr);
  } else {
#if !defined(PETSC_USE_COMPLEX)
    ierr = MPI_Gatherv(xarray,xin->n,MPIU_REAL,0,0,0,MPIU_REAL,0,xin->comm);CHKERRQ(ierr);
#else
    {
      PetscReal *xr;
      ierr = PetscMalloc((xin->n+1)*sizeof(PetscReal),&xr);CHKERRQ(ierr);
      for (i=0; i<xin->n; i++) {
        xr[i] = PetscRealPart(xarray[i]);
      }
      ierr = MPI_Gatherv(xr,xin->n,MPIU_REAL,0,0,0,MPIU_REAL,0,xin->comm);CHKERRQ(ierr);
      ierr = PetscFree(xr);CHKERRQ(ierr);
    }
#endif
  }
  ierr = PetscDrawLGDraw(lg);CHKERRQ(ierr);
  ierr = PetscDrawSynchronizedFlush(draw);CHKERRQ(ierr);
  ierr = VecRestoreArray(xin,&xarray);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "VecView_MPI_Draw"
PetscErrorCode VecView_MPI_Draw(Vec xin,PetscViewer viewer)
{
  PetscErrorCode ierr;
  PetscMPIInt    rank,size,tag = ((PetscObject)viewer)->tag;
  PetscInt       i,start,end;
  MPI_Status     status;
  PetscReal      coors[4],ymin,ymax,xmin,xmax,tmp;
  PetscDraw      draw;
  PetscTruth     isnull;
  PetscDrawAxis  axis;
  PetscScalar    *xarray;

  PetscFunctionBegin;
  ierr = PetscViewerDrawGetDraw(viewer,0,&draw);CHKERRQ(ierr);
  ierr = PetscDrawIsNull(draw,&isnull);CHKERRQ(ierr); if (isnull) PetscFunctionReturn(0);

  ierr = VecGetArray(xin,&xarray);CHKERRQ(ierr);
  ierr = PetscDrawCheckResizedWindow(draw);CHKERRQ(ierr);
  xmin = 1.e20; xmax = -1.e20;
  for (i=0; i<xin->n; i++) {
#if defined(PETSC_USE_COMPLEX)
    if (PetscRealPart(xarray[i]) < xmin) xmin = PetscRealPart(xarray[i]);
    if (PetscRealPart(xarray[i]) > xmax) xmax = PetscRealPart(xarray[i]);
#else
    if (xarray[i] < xmin) xmin = xarray[i];
    if (xarray[i] > xmax) xmax = xarray[i];
#endif
  }
  if (xmin + 1.e-10 > xmax) {
    xmin -= 1.e-5;
    xmax += 1.e-5;
  }
  ierr = MPI_Reduce(&xmin,&ymin,1,MPIU_REAL,MPI_MIN,0,xin->comm);CHKERRQ(ierr);
  ierr = MPI_Reduce(&xmax,&ymax,1,MPIU_REAL,MPI_MAX,0,xin->comm);CHKERRQ(ierr);
  ierr = MPI_Comm_size(xin->comm,&size);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(xin->comm,&rank);CHKERRQ(ierr);
  ierr = PetscDrawAxisCreate(draw,&axis);CHKERRQ(ierr);
  PetscLogObjectParent(draw,axis);
  if (!rank) {
    ierr = PetscDrawClear(draw);CHKERRQ(ierr);
    ierr = PetscDrawFlush(draw);CHKERRQ(ierr);
    ierr = PetscDrawAxisSetLimits(axis,0.0,(double)xin->N,ymin,ymax);CHKERRQ(ierr);
    ierr = PetscDrawAxisDraw(axis);CHKERRQ(ierr);
    ierr = PetscDrawGetCoordinates(draw,coors,coors+1,coors+2,coors+3);CHKERRQ(ierr);
  }
  ierr = PetscDrawAxisDestroy(axis);CHKERRQ(ierr);
  ierr = MPI_Bcast(coors,4,MPIU_REAL,0,xin->comm);CHKERRQ(ierr);
  if (rank) {ierr = PetscDrawSetCoordinates(draw,coors[0],coors[1],coors[2],coors[3]);CHKERRQ(ierr);}
  /* draw local part of vector */
  ierr = VecGetOwnershipRange(xin,&start,&end);CHKERRQ(ierr);
  if (rank < size-1) { /*send value to right */
    ierr = MPI_Send(&xarray[xin->n-1],1,MPIU_REAL,rank+1,tag,xin->comm);CHKERRQ(ierr);
  }
  for (i=1; i<xin->n; i++) {
#if !defined(PETSC_USE_COMPLEX)
    ierr = PetscDrawLine(draw,(PetscReal)(i-1+start),xarray[i-1],(PetscReal)(i+start),
                   xarray[i],PETSC_DRAW_RED);CHKERRQ(ierr);
#else
    ierr = PetscDrawLine(draw,(PetscReal)(i-1+start),PetscRealPart(xarray[i-1]),(PetscReal)(i+start),
                   PetscRealPart(xarray[i]),PETSC_DRAW_RED);CHKERRQ(ierr);
#endif
  }
  if (rank) { /* receive value from right */
    ierr = MPI_Recv(&tmp,1,MPIU_REAL,rank-1,tag,xin->comm,&status);CHKERRQ(ierr);
#if !defined(PETSC_USE_COMPLEX)
    ierr = PetscDrawLine(draw,(PetscReal)start-1,tmp,(PetscReal)start,xarray[0],PETSC_DRAW_RED);CHKERRQ(ierr);
#else
    ierr = PetscDrawLine(draw,(PetscReal)start-1,tmp,(PetscReal)start,PetscRealPart(xarray[0]),PETSC_DRAW_RED);CHKERRQ(ierr);
#endif
  }
  ierr = PetscDrawSynchronizedFlush(draw);CHKERRQ(ierr);
  ierr = PetscDrawPause(draw);CHKERRQ(ierr);
  ierr = VecRestoreArray(xin,&xarray);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "VecView_MPI_Socket"
PetscErrorCode VecView_MPI_Socket(Vec xin,PetscViewer viewer)
{
  PetscErrorCode ierr;
  PetscMPIInt    rank,size;
  int            i,N = xin->N,*lens;
  PetscScalar    *xx,*xarray;

  PetscFunctionBegin;
  ierr = VecGetArray(xin,&xarray);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(xin->comm,&rank);CHKERRQ(ierr);
  ierr = MPI_Comm_size(xin->comm,&size);CHKERRQ(ierr);
  if (!rank) {
    ierr = PetscMalloc((N+1)*sizeof(PetscScalar),&xx);CHKERRQ(ierr);
    ierr = PetscMalloc(size*sizeof(int),&lens);CHKERRQ(ierr);
    for (i=0; i<size; i++) {
      lens[i] = xin->map->range[i+1] - xin->map->range[i];
    }
    ierr = MPI_Gatherv(xarray,xin->n,MPIU_SCALAR,xx,lens,xin->map->range,MPIU_SCALAR,0,xin->comm);CHKERRQ(ierr);
    ierr = PetscFree(lens);CHKERRQ(ierr);
    ierr = PetscViewerSocketPutScalar(viewer,N,1,xx);CHKERRQ(ierr);
    ierr = PetscFree(xx);CHKERRQ(ierr);
  } else {
    ierr = MPI_Gatherv(xarray,xin->n,MPIU_REAL,0,0,0,MPIU_REAL,0,xin->comm);CHKERRQ(ierr);
  }
  ierr = VecRestoreArray(xin,&xarray);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecView_MPI_Matlab"
PetscErrorCode VecView_MPI_Matlab(Vec xin,PetscViewer viewer)
{
#if defined(PETSC_HAVE_MATLAB) && !defined(PETSC_USE_COMPLEX) && !defined(PETSC_USE_SINGLE)
  PetscErrorCode ierr;
  PetscMPIInt    rank,size;
  int            i,N = xin->N,*lens;
  PetscScalar    *xx,*xarray;

  PetscFunctionBegin;
  ierr = VecGetArray(xin,&xarray);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(xin->comm,&rank);CHKERRQ(ierr);
  ierr = MPI_Comm_size(xin->comm,&size);CHKERRQ(ierr);
  if (!rank) {
    ierr = PetscMalloc((N+1)*sizeof(PetscScalar),&xx);CHKERRQ(ierr);
    ierr = PetscMalloc(size*sizeof(int),&lens);CHKERRQ(ierr);
    for (i=0; i<size; i++) {
      lens[i] = xin->map->range[i+1] - xin->map->range[i];
    }
    ierr = MPI_Gatherv(xarray,xin->n,MPIU_SCALAR,xx,lens,xin->map->range,MPIU_SCALAR,0,xin->comm);CHKERRQ(ierr);
    ierr = PetscFree(lens);CHKERRQ(ierr);

    ierr = PetscObjectName((PetscObject)xin);CHKERRQ(ierr);
    ierr = PetscViewerMatlabPutArray(viewer,N,1,xx,xin->name);CHKERRQ(ierr);

    ierr = PetscFree(xx);CHKERRQ(ierr);
  } else {
    ierr = MPI_Gatherv(xarray,xin->n,MPIU_REAL,0,0,0,MPIU_REAL,0,xin->comm);CHKERRQ(ierr);
  }
  ierr = VecRestoreArray(xin,&xarray);CHKERRQ(ierr);
  PetscFunctionReturn(0);
#else
  PetscFunctionBegin;
  SETERRQ(PETSC_ERR_SUP_SYS,"Build PETSc with Matlab to use this viewer");
#endif
}

#undef __FUNCT__  
#define __FUNCT__ "VecView_MPI_Netcdf"
PetscErrorCode VecView_MPI_Netcdf(Vec xin,PetscViewer v)
{
#if defined(PETSC_HAVE_PNETCDF)
  PetscErrorCode ierr;
  int         n = xin->n,ncid,xdim,xdim_num=1,xin_id,xstart;
  MPI_Comm    comm = xin->comm;  
  PetscScalar *xarray;

  PetscFunctionBegin;
#if !defined(PETSC_USE_COMPLEX)
  ierr = VecGetArray(xin,&xarray);CHKERRQ(ierr);
  ierr = PetscViewerNetcdfGetID(v,&ncid);CHKERRQ(ierr);
  if (ncid < 0) SETERRQ(PETSC_ERR_ORDER,"First call PetscViewerNetcdfOpen to create NetCDF dataset");
  /* define dimensions */
  ierr = ncmpi_def_dim(ncid,"PETSc_Vector_Global_Size",xin->N,&xdim);CHKERRQ(ierr);
  /* define variables */
  ierr = ncmpi_def_var(ncid,"PETSc_Vector_MPI",NC_DOUBLE,xdim_num,&xdim,&xin_id);CHKERRQ(ierr);
  /* leave define mode */
  ierr = ncmpi_enddef(ncid);CHKERRQ(ierr);
  /* store the vector */
  ierr = VecGetOwnershipRange(xin,&xstart,PETSC_NULL);CHKERRQ(ierr);
  ierr = ncmpi_put_vara_double_all(ncid,xin_id,(const size_t*)&xstart,(const size_t*)&n,xarray);CHKERRQ(ierr);
  ierr = VecRestoreArray(xin,&xarray);CHKERRQ(ierr);
#else 
    PetscPrintf(PETSC_COMM_WORLD,"NetCDF viewer not supported for complex numbers\n");
#endif
  PetscFunctionReturn(0);
#else /* !defined(PETSC_HAVE_PNETCDF) */
  PetscFunctionBegin;
  SETERRQ(PETSC_ERR_SUP_SYS,"Build PETSc with NetCDF to use this viewer");
#endif
}

#undef __FUNCT__  
#define __FUNCT__ "VecView_MPI_HDF4_Ex"
PetscErrorCode VecView_MPI_HDF4_Ex(Vec X, PetscViewer viewer, int d, int *dims)
{
#if defined(PETSC_HAVE_HDF4) && !defined(PETSC_USE_COMPLEX)
  PetscErrorCode ierr;
  PetscMPIInt    rank,size,tag = ((PetscObject)viewer)->tag;
  int            len, i, j, k, cur, bs, n, N;
  MPI_Status     status;
  PetscScalar    *x;
  float          *xlf, *xf;

  PetscFunctionBegin;

  bs = X->bs > 0 ? X->bs : 1;
  N  = X->N / bs;
  n  = X->n / bs;

  // For now, always convert to float
  ierr = PetscMalloc(N * sizeof(float), &xf);CHKERRQ(ierr);
  ierr = PetscMalloc(n * sizeof(float), &xlf);CHKERRQ(ierr);

  ierr = MPI_Comm_rank(X->comm, &rank);CHKERRQ(ierr);
  ierr = MPI_Comm_size(X->comm, &size);CHKERRQ(ierr);

  ierr = VecGetArray(X, &x);CHKERRQ(ierr);

  for (k = 0; k < bs; k++) {
    for (i = 0; i < n; i++) {
      xlf[i] = (float) x[i*bs + k];
    }
    if (!rank) {
      cur = 0;
      ierr = PetscMemcpy(xf + cur, xlf, n * sizeof(float));CHKERRQ(ierr);
      cur += n;
      for (j = 1; j < size; j++) {
        ierr = MPI_Recv(xf + cur, N - cur, MPI_FLOAT, j, tag, X->comm,&status);CHKERRQ(ierr);
        ierr = MPI_Get_count(&status, MPI_FLOAT, &len);CHKERRQ(ierr);cur += len;
      }
      if (cur != N) {
        SETERRQ2(1, "? %d %d", cur, N);
      }
      ierr = PetscViewerHDF4WriteSDS(viewer, xf, 2, dims, bs);CHKERRQ(ierr); 
    } else {
      ierr = MPI_Send(xlf, n, MPI_FLOAT, 0, tag, X->comm);CHKERRQ(ierr);
    }
  }
  ierr = VecRestoreArray(X, &x);CHKERRQ(ierr);
  ierr = PetscFree(xlf);CHKERRQ(ierr);
  ierr = PetscFree(xf);CHKERRQ(ierr);
  PetscFunctionReturn(0);
#else /* !defined(PETSC_HAVE_HDF4) */
  PetscFunctionBegin;
  SETERRQ(PETSC_ERR_SUP_SYS,"Build PETSc with HDF4 to use this viewer");
 
#endif
}

#undef __FUNCT__  
#define __FUNCT__ "VecView_MPI_HDF4"
PetscErrorCode VecView_MPI_HDF4(Vec xin,PetscViewer viewer)
{
#if defined(PETSC_HAVE_HDF4) && !defined(PETSC_USE_COMPLEX)
  PetscErrorCode ierr;
  PetscErrorCode  bs, dims[1];

  bs = xin->bs > 0 ? xin->bs : 1;
  dims[0] = xin->N / bs;
  ierr = VecView_MPI_HDF4_Ex(xin, viewer, 1, dims);CHKERRQ(ierr);
  PetscFunctionReturn(0);
#else /* !defined(PETSC_HAVE_HDF4) */
  PetscFunctionBegin;
  SETERRQ(PETSC_ERR_SUP_SYS,"Build PETSc with HDF4 to use this viewer");
#endif
}

#undef __FUNCT__  
#define __FUNCT__ "VecView_MPI"
PetscErrorCode VecView_MPI(Vec xin,PetscViewer viewer)
{
  PetscErrorCode ierr;
  PetscTruth iascii,issocket,isbinary,isdraw,ismathematica,isnetcdf,ishdf4,ismatlab;

  PetscFunctionBegin;
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_ASCII,&iascii);CHKERRQ(ierr);
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_SOCKET,&issocket);CHKERRQ(ierr);
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_BINARY,&isbinary);CHKERRQ(ierr);
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_DRAW,&isdraw);CHKERRQ(ierr);
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_MATHEMATICA,&ismathematica);CHKERRQ(ierr);
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_NETCDF,&isnetcdf);CHKERRQ(ierr);
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_HDF4,&ishdf4);CHKERRQ(ierr);
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_MATLAB,&ismatlab);CHKERRQ(ierr);
  if (iascii){
    ierr = VecView_MPI_ASCII(xin,viewer);CHKERRQ(ierr);
  } else if (issocket) {
    ierr = VecView_MPI_Socket(xin,viewer);CHKERRQ(ierr);
  } else if (isbinary) {
    ierr = VecView_MPI_Binary(xin,viewer);CHKERRQ(ierr);
  } else if (isdraw) {
    PetscViewerFormat format;

    ierr = PetscViewerGetFormat(viewer,&format);CHKERRQ(ierr);
    if (format == PETSC_VIEWER_DRAW_LG) {
      ierr = VecView_MPI_Draw_LG(xin,viewer);CHKERRQ(ierr);
    } else {
      ierr = VecView_MPI_Draw(xin,viewer);CHKERRQ(ierr);
    }
  } else if (ismathematica) {
    ierr = PetscViewerMathematicaPutVector(viewer,xin);CHKERRQ(ierr);
  } else if (isnetcdf) {
    ierr = VecView_MPI_Netcdf(xin,viewer);CHKERRQ(ierr);
  } else if (ishdf4) {
    ierr = VecView_MPI_HDF4(xin,viewer);CHKERRQ(ierr);
  } else if (ismatlab) {
    ierr = VecView_MPI_Matlab(xin,viewer);CHKERRQ(ierr);
  } else {
    SETERRQ1(PETSC_ERR_SUP,"Viewer type %s not supported for this object",((PetscObject)viewer)->type_name);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecGetSize_MPI"
PetscErrorCode VecGetSize_MPI(Vec xin,PetscInt *N)
{
  PetscFunctionBegin;
  *N = xin->N;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecSetValues_MPI"
PetscErrorCode VecSetValues_MPI(Vec xin,PetscInt ni,const PetscInt ix[],const PetscScalar y[],InsertMode addv)
{
  PetscMPIInt    rank = xin->stash.rank;
  PetscInt            *owners = xin->map->range,start = owners[rank];
  PetscErrorCode ierr;
  PetscInt            end = owners[rank+1],i,row;
  PetscScalar    *xx;

  PetscFunctionBegin;
#if defined(PETSC_USE_BOPT_g)
  if (xin->stash.insertmode == INSERT_VALUES && addv == ADD_VALUES) { 
   SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"You have already inserted values; you cannot now add");
  } else if (xin->stash.insertmode == ADD_VALUES && addv == INSERT_VALUES) { 
   SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"You have already added values; you cannot now insert");
  }
#endif
  ierr = VecGetArray(xin,&xx);CHKERRQ(ierr);
  xin->stash.insertmode = addv;

  if (addv == INSERT_VALUES) {
    for (i=0; i<ni; i++) {
      if ((row = ix[i]) >= start && row < end) {
        xx[row-start] = y[i];
      } else if (!xin->stash.donotstash) {
        if (ix[i] < 0) continue;
#if defined(PETSC_USE_BOPT_g)
        if (ix[i] >= xin->N) SETERRQ2(PETSC_ERR_ARG_OUTOFRANGE,"Out of range index value %d maximum %d",ix[i],xin->N);
#endif
        VecStashValue_Private(&xin->stash,row,y[i]);
      }
    }
  } else {
    for (i=0; i<ni; i++) {
      if ((row = ix[i]) >= start && row < end) {
        xx[row-start] += y[i];
      } else if (!xin->stash.donotstash) {
        if (ix[i] < 0) continue;
#if defined(PETSC_USE_BOPT_g)
        if (ix[i] > xin->N) SETERRQ2(PETSC_ERR_ARG_OUTOFRANGE,"Out of range index value %d maximum %d",ix[i],xin->N);
#endif        
        VecStashValue_Private(&xin->stash,row,y[i]);
      }
    }
  }
  ierr = VecRestoreArray(xin,&xx);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecSetValuesBlocked_MPI"
PetscErrorCode VecSetValuesBlocked_MPI(Vec xin,PetscInt ni,const PetscInt ix[],const PetscScalar yin[],InsertMode addv)
{
  PetscMPIInt    rank = xin->stash.rank;
  PetscInt            *owners = xin->map->range,start = owners[rank];
  PetscErrorCode ierr;
  PetscInt            end = owners[rank+1],i,row,bs = xin->bs,j;
  PetscScalar    *xx,*y = (PetscScalar*)yin;

  PetscFunctionBegin;
  ierr = VecGetArray(xin,&xx);CHKERRQ(ierr);
#if defined(PETSC_USE_BOPT_g)
  if (xin->stash.insertmode == INSERT_VALUES && addv == ADD_VALUES) { 
   SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"You have already inserted values; you cannot now add");
  }
  else if (xin->stash.insertmode == ADD_VALUES && addv == INSERT_VALUES) { 
   SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"You have already added values; you cannot now insert");
  }
#endif
  xin->stash.insertmode = addv;

  if (addv == INSERT_VALUES) {
    for (i=0; i<ni; i++) {
      if ((row = bs*ix[i]) >= start && row < end) {
        for (j=0; j<bs; j++) {
          xx[row-start+j] = y[j];
        }
      } else if (!xin->stash.donotstash) {
        if (ix[i] < 0) continue;
#if defined(PETSC_USE_BOPT_g)
        if (ix[i] >= xin->N) SETERRQ2(PETSC_ERR_ARG_OUTOFRANGE,"Out of range index value %d max %d",ix[i],xin->N);
#endif
        VecStashValuesBlocked_Private(&xin->bstash,ix[i],y);
      }
      y += bs;
    }
  } else {
    for (i=0; i<ni; i++) {
      if ((row = bs*ix[i]) >= start && row < end) {
        for (j=0; j<bs; j++) {
          xx[row-start+j] += y[j];
        }
      } else if (!xin->stash.donotstash) {
        if (ix[i] < 0) continue;
#if defined(PETSC_USE_BOPT_g)
        if (ix[i] > xin->N) SETERRQ2(PETSC_ERR_ARG_OUTOFRANGE,"Out of range index value %d max %d",ix[i],xin->N);
#endif
        VecStashValuesBlocked_Private(&xin->bstash,ix[i],y);
      }
      y += bs;
    }
  }
  ierr = VecRestoreArray(xin,&xx);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*
   Since nsends or nreceives may be zero we add 1 in certain mallocs
to make sure we never malloc an empty one.      
*/
#undef __FUNCT__  
#define __FUNCT__ "VecAssemblyBegin_MPI"
PetscErrorCode VecAssemblyBegin_MPI(Vec xin)
{
  PetscErrorCode ierr;
  PetscInt         *owners = xin->map->range,*bowners,size,i,bs,nstash,reallocs;
  InsertMode  addv;
  MPI_Comm    comm = xin->comm;

  PetscFunctionBegin;
  if (xin->stash.donotstash) {
    PetscFunctionReturn(0);
  }

  ierr = MPI_Allreduce(&xin->stash.insertmode,&addv,1,MPI_INT,MPI_BOR,comm);CHKERRQ(ierr);
  if (addv == (ADD_VALUES|INSERT_VALUES)) { 
    SETERRQ(PETSC_ERR_ARG_NOTSAMETYPE,"Some processors inserted values while others added");
  }
  xin->stash.insertmode = addv; /* in case this processor had no cache */
  
  bs = xin->bs;
  ierr = MPI_Comm_size(xin->comm,&size);CHKERRQ(ierr);
  if (!xin->bstash.bowners && xin->bs != -1) {
    ierr = PetscMalloc((size+1)*sizeof(PetscInt),&bowners);CHKERRQ(ierr);
    for (i=0; i<size+1; i++){ bowners[i] = owners[i]/bs;}
    xin->bstash.bowners = bowners;
  } else { 
    bowners = xin->bstash.bowners; 
  }
  ierr = VecStashScatterBegin_Private(&xin->stash,owners);CHKERRQ(ierr);
  ierr = VecStashScatterBegin_Private(&xin->bstash,bowners);CHKERRQ(ierr);
  ierr = VecStashGetInfo_Private(&xin->stash,&nstash,&reallocs);CHKERRQ(ierr);
  PetscLogInfo(0,"VecAssemblyBegin_MPI:Stash has %d entries, uses %d mallocs.\n",nstash,reallocs);
  ierr = VecStashGetInfo_Private(&xin->bstash,&nstash,&reallocs);CHKERRQ(ierr);
  PetscLogInfo(0,"VecAssemblyBegin_MPI:Block-Stash has %d entries, uses %d mallocs.\n",nstash,reallocs);

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecAssemblyEnd_MPI"
PetscErrorCode VecAssemblyEnd_MPI(Vec vec)
{
  PetscErrorCode ierr;
  PetscInt         base,i,j,n,*row,flg,bs;
  PetscScalar *val,*vv,*array,*xarray;

  PetscFunctionBegin;
  if (!vec->stash.donotstash) {
    ierr = VecGetArray(vec,&xarray);CHKERRQ(ierr); 
    base = vec->map->range[vec->stash.rank];
    bs   = vec->bs;

    /* Process the stash */
    while (1) {
      ierr = VecStashScatterGetMesg_Private(&vec->stash,&n,&row,&val,&flg);CHKERRQ(ierr);
      if (!flg) break;
      if (vec->stash.insertmode == ADD_VALUES) {
        for (i=0; i<n; i++) { xarray[row[i] - base] += val[i]; }
      } else if (vec->stash.insertmode == INSERT_VALUES) {
        for (i=0; i<n; i++) { xarray[row[i] - base] = val[i]; }
      } else {
        SETERRQ(PETSC_ERR_ARG_CORRUPT,"Insert mode is not set correctly; corrupted vector");
      }
    }
    ierr = VecStashScatterEnd_Private(&vec->stash);CHKERRQ(ierr);

    /* now process the block-stash */
    while (1) {
      ierr = VecStashScatterGetMesg_Private(&vec->bstash,&n,&row,&val,&flg);CHKERRQ(ierr);
      if (!flg) break;
      for (i=0; i<n; i++) { 
        array = xarray+row[i]*bs-base;
        vv    = val+i*bs;
        if (vec->stash.insertmode == ADD_VALUES) {
          for (j=0; j<bs; j++) { array[j] += vv[j];}
        } else if (vec->stash.insertmode == INSERT_VALUES) {
          for (j=0; j<bs; j++) { array[j] = vv[j]; }
        } else {
          SETERRQ(PETSC_ERR_ARG_CORRUPT,"Insert mode is not set correctly; corrupted vector");
        }
      }
    }
    ierr = VecStashScatterEnd_Private(&vec->bstash);CHKERRQ(ierr);
    ierr = VecRestoreArray(vec,&xarray);CHKERRQ(ierr); 
  }
  vec->stash.insertmode = NOT_SET_VALUES;
  PetscFunctionReturn(0);
}

