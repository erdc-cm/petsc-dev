
#include "src/sys/src/viewer/viewerimpl.h"    /*I   "petsc.h"   I*/
#include "petscsys.h"
#include <fcntl.h>
#if defined(PETSC_HAVE_UNISTD_H)
#include <unistd.h>
#endif
#if defined (PETSC_HAVE_IO_H)
#include <io.h>
#endif

typedef struct  {
  int                   fdes;            /* file descriptor */
  PetscViewerFileType btype;           /* read or write? */
  FILE                  *fdes_info;      /* optional file containing info on binary file*/
  PetscTruth            storecompressed; /* gzip the write binary file when closing it*/
  char                  *filename;
  PetscTruth            skipinfo;        /* Don't create info file for writing */
} PetscViewer_Binary;

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerGetSingleton_Binary" 
PetscErrorCode PetscViewerGetSingleton_Binary(PetscViewer viewer,PetscViewer *outviewer)
{
  int                rank;
  PetscErrorCode     ierr;
  PetscViewer_Binary *vbinary = (PetscViewer_Binary*)viewer->data,*obinary;

  PetscFunctionBegin;
  ierr = MPI_Comm_rank(viewer->comm,&rank);CHKERRQ(ierr);
  if (!rank) {
    ierr    = PetscViewerCreate(PETSC_COMM_SELF,outviewer);CHKERRQ(ierr);
    ierr    = PetscViewerSetType(*outviewer,PETSC_VIEWER_BINARY);CHKERRQ(ierr);
    obinary = (PetscViewer_Binary*)(*outviewer)->data;
    ierr    = PetscMemcpy(obinary,vbinary,sizeof(PetscViewer_Binary));CHKERRQ(ierr);
  } else {
    *outviewer = 0;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerRestoreSingleton_Binary" 
PetscErrorCode PetscViewerRestoreSingleton_Binary(PetscViewer viewer,PetscViewer *outviewer)
{
  PetscErrorCode ierr;
  PetscErrorCode rank;

  PetscFunctionBegin;
  ierr = MPI_Comm_rank(viewer->comm,&rank);CHKERRQ(ierr);
  if (!rank) {
    ierr = PetscFree((*outviewer)->data);CHKERRQ(ierr);
    PetscLogObjectDestroy((PetscObject)*outviewer);
    PetscHeaderDestroy((PetscObject)*outviewer); 
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerBinaryGetDescriptor" 
/*@C
    PetscViewerBinaryGetDescriptor - Extracts the file descriptor from a PetscViewer.

    Not Collective

+   viewer - PetscViewer context, obtained from PetscViewerBinaryOpen()
-   fdes - file descriptor

    Level: advanced

    Notes:
      For writable binary PetscViewers, the descriptor will only be valid for the 
    first processor in the communicator that shares the PetscViewer. For readable 
    files it will only be valid on nodes that have the file. If node 0 does not
    have the file it generates an error even if another node does have the file.
 
    Fortran Note:
    This routine is not supported in Fortran.

  Concepts: file descriptor^getting
  Concepts: PetscViewerBinary^accessing file descriptor

.seealso: PetscViewerBinaryOpen(),PetscViewerBinaryGetInfoPointer()
@*/
PetscErrorCode PetscViewerBinaryGetDescriptor(PetscViewer viewer,int *fdes)
{
  PetscViewer_Binary *vbinary = (PetscViewer_Binary*)viewer->data;

  PetscFunctionBegin;
  *fdes = vbinary->fdes;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerBinarySkipInfo" 
/*@
    PetscViewerBinarySkipInfo - Binary file will not have .info file created with it

    Not Collective

    Input Paramter:
.   viewer - PetscViewer context, obtained from PetscViewerBinaryOpen()

    Options Database Key:
.   -viewer_binary_skip_info

    Level: advanced

    Notes: This must be called after PetscViewerSetType() but before PetscViewerBinarySetFilename()

   Concepts: PetscViewerBinary^accessing info file

.seealso: PetscViewerBinaryOpen(),PetscViewerBinaryGetDescriptor()
@*/
PetscErrorCode PetscViewerBinarySkipInfo(PetscViewer viewer)
{
  PetscViewer_Binary *vbinary = (PetscViewer_Binary*)viewer->data;

  PetscFunctionBegin;
  vbinary->skipinfo = PETSC_TRUE;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerBinaryGetInfoPointer" 
/*@C
    PetscViewerBinaryGetInfoPointer - Extracts the file pointer for the ASCII
          info file associated with a binary file.

    Not Collective

+   viewer - PetscViewer context, obtained from PetscViewerBinaryOpen()
-   file - file pointer

    Level: advanced

    Notes:
      For writable binary PetscViewers, the descriptor will only be valid for the 
    first processor in the communicator that shares the PetscViewer.
 
    Fortran Note:
    This routine is not supported in Fortran.

  Concepts: PetscViewerBinary^accessing info file

.seealso: PetscViewerBinaryOpen(),PetscViewerBinaryGetDescriptor()
@*/
PetscErrorCode PetscViewerBinaryGetInfoPointer(PetscViewer viewer,FILE **file)
{
  PetscViewer_Binary *vbinary = (PetscViewer_Binary*)viewer->data;

  PetscFunctionBegin;
  *file = vbinary->fdes_info;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerDestroy_Binary" 
PetscErrorCode PetscViewerDestroy_Binary(PetscViewer v)
{
  PetscViewer_Binary *vbinary = (PetscViewer_Binary*)v->data;
  PetscErrorCode     ierr;
  int                rank;

  PetscFunctionBegin;
  ierr = MPI_Comm_rank(v->comm,&rank);CHKERRQ(ierr);
  if (!rank && vbinary->fdes) {
    close(vbinary->fdes);
    if (vbinary->storecompressed) {
      char par[PETSC_MAX_PATH_LEN],buf[PETSC_MAX_PATH_LEN];
      FILE *fp;
      /* compress the file */
      ierr = PetscStrcpy(par,"gzip ");CHKERRQ(ierr);
      ierr = PetscStrcat(par,vbinary->filename);CHKERRQ(ierr);
      ierr = PetscPOpen(PETSC_COMM_SELF,PETSC_NULL,par,"r",&fp);CHKERRQ(ierr);
      if (fgets(buf,1024,fp)) {
        SETERRQ2(1,"Error from command %s\n%s",par,buf);
      }
      ierr = PetscPClose(PETSC_COMM_SELF,fp);CHKERRQ(ierr);
    }
  }
  if (vbinary->fdes_info) fclose(vbinary->fdes_info);
  ierr = PetscStrfree(vbinary->filename);CHKERRQ(ierr);
  ierr = PetscFree(vbinary);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerBinaryOpen" 
/*@C
   PetscViewerBinaryOpen - Opens a file for binary input/output.

   Collective on MPI_Comm

   Input Parameters:
+  comm - MPI communicator
.  name - name of file 
-  type - type of file
$    PETSC_FILE_CREATE - create new file for binary output
$    PETSC_FILE_RDONLY - open existing file for binary input
$    PETSC_FILE_WRONLY - open existing file for binary output

   Output Parameter:
.  binv - PetscViewer for binary input/output to use with the specified file

   Level: beginner

   Note:
   This PetscViewer should be destroyed with PetscViewerDestroy().

    For reading files, the filename may begin with ftp:// or http:// and/or
    end with .gz; in this case file is brought over and uncompressed.

    For creating files, if the file name ends with .gz it is automatically 
    compressed when closed.

    For writing files it only opens the file on processor 0 in the communicator.
    For readable files it opens the file on all nodes that have the file. If 
    node 0 does not have the file it generates an error even if other nodes
    do have the file.

   Concepts: binary files
   Concepts: PetscViewerBinary^creating
   Concepts: gzip
   Concepts: accessing remote file
   Concepts: remote file

.seealso: PetscViewerASCIIOpen(), PetscViewerSetFormat(), PetscViewerDestroy(),
          VecView(), MatView(), VecLoad(), MatLoad(), PetscViewerBinaryGetDescriptor(),
          PetscViewerBinaryGetInfoPointer(), PetscViewerFileType, PetscViewer
@*/
PetscErrorCode PetscViewerBinaryOpen(MPI_Comm comm,const char name[],PetscViewerFileType type,PetscViewer *binv)
{
  PetscErrorCode ierr;
  
  PetscFunctionBegin;
  ierr = PetscViewerCreate(comm,binv);CHKERRQ(ierr);
  ierr = PetscViewerSetType(*binv,PETSC_VIEWER_BINARY);CHKERRQ(ierr);
  ierr = PetscViewerSetFileType(*binv,type);CHKERRQ(ierr);
  ierr = PetscViewerSetFilename(*binv,name);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerSetFileType" 
/*@C
     PetscViewerSetFileType - Sets the type of file to be open

    Collective on PetscViewer

  Input Parameters:
+  viewer - the PetscViewer; must be a binary, Matlab, hdf, or netcdf PetscViewer
-  type - type of file
$    PETSC_FILE_CREATE - create new file for binary output
$    PETSC_FILE_RDONLY - open existing file for binary input
$    PETSC_FILE_WRONLY - open existing file for binary output

  Level: advanced

.seealso: PetscViewerCreate(), PetscViewerSetType(), PetscViewerBinaryOpen()

@*/
PetscErrorCode PetscViewerSetFileType(PetscViewer viewer,PetscViewerFileType type)
{
  PetscErrorCode ierr,(*f)(PetscViewer,PetscViewerFileType);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer,PETSC_VIEWER_COOKIE,1);
  ierr = PetscObjectQueryFunction((PetscObject)viewer,"PetscViewerSetFileType_C",(void (**)(void))&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(viewer,type);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "PetscViewerSetFileType_Binary" 
PetscErrorCode PetscViewerSetFileType_Binary(PetscViewer viewer,PetscViewerFileType type)
{
  PetscViewer_Binary *vbinary = (PetscViewer_Binary*)viewer->data;

  PetscFunctionBegin;
  vbinary->btype = type;
  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerBinaryLoadInfo" 
/*@
    PetscViewerBinaryLoadInfo - Loads options from the name.info file
       if it exists.

   Collective on PetscViewer

  Input Parameter:
.    viewer - the binary viewer whose options you wish to load

   Level: developer

@*/
PetscErrorCode PetscViewerBinaryLoadInfo(PetscViewer viewer)
{
  FILE           *file;
  char           string[256],*first,*second,*final;
  size_t         len;
  PetscErrorCode ierr;
  PetscTruth     flg;
  PetscToken     *token;  

  PetscFunctionBegin;
  ierr = PetscOptionsHasName(PETSC_NULL,"-load_ignore_info",&flg);CHKERRQ(ierr);
  if (flg) PetscFunctionReturn(0);

  ierr = PetscViewerBinaryGetInfoPointer(viewer,&file);CHKERRQ(ierr);
  if (!file) PetscFunctionReturn(0);

  /* read rows of the file adding them to options database */
  while (fgets(string,256,file)) {
    /* Comments are indicated by #, ! or % in the first column */
    if (string[0] == '#') continue;
    if (string[0] == '!') continue;
    if (string[0] == '%') continue;
    ierr = PetscTokenCreate(string,' ',&token);CHKERRQ(ierr);
    ierr = PetscTokenFind(token,&first);CHKERRQ(ierr);
    ierr = PetscTokenFind(token,&second);CHKERRQ(ierr);
    if (first && first[0] == '-') {
      PetscTruth wrongtype;
      /*
         Check for -mat_complex or -mat_double
      */
#if defined(PETSC_USE_COMPLEX)
      ierr = PetscStrncmp(first,"-mat_double",11,&wrongtype);CHKERRQ(ierr);
      if (wrongtype) {
        SETERRQ(PETSC_ERR_FILE_UNEXPECTED,"Loading double number matrix with complex number code");
      }
#else
      ierr = PetscStrncmp(first,"-mat_complex",12,&wrongtype);CHKERRQ(ierr);
      if (wrongtype) {
        SETERRQ(PETSC_ERR_FILE_UNEXPECTED,"Loading complex number matrix with double number code");
      }
#endif

      if (second) {final = second;} else {final = first;}
      ierr = PetscStrlen(final,&len);CHKERRQ(ierr);
      while (len > 0 && (final[len-1] == ' ' || final[len-1] == '\n')) {
        len--; final[len] = 0;
      }
      ierr = PetscOptionsSetValue(first,second);CHKERRQ(ierr);
    }
    ierr = PetscTokenDestroy(token);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

/*
        Actually opens the file 
*/
EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "PetscViewerSetFilename_Binary" 
PetscErrorCode PetscViewerSetFilename_Binary(PetscViewer viewer,const char name[])
{
  int                 rank;
  PetscErrorCode      ierr;
  size_t              len;
  PetscViewer_Binary  *vbinary = (PetscViewer_Binary*)viewer->data;
  const char          *fname;
  char                bname[PETSC_MAX_PATH_LEN],*gz;
  PetscTruth          found;
  PetscViewerFileType type = vbinary->btype;

  PetscFunctionBegin;
  if (type == (PetscViewerFileType) -1) {
    SETERRQ(PETSC_ERR_ORDER,"Must call PetscViewerBinarySetType() before PetscViewerSetFilename()");
  }
  ierr = PetscOptionsHasName(viewer->prefix,"-viewer_binary_skip_info",&vbinary->skipinfo);CHKERRQ(ierr);

  ierr = MPI_Comm_rank(viewer->comm,&rank);CHKERRQ(ierr);

  /* copy name so we can edit it */
  ierr = PetscStrallocpy(name,&vbinary->filename);CHKERRQ(ierr);

  /* if ends in .gz strip that off and note user wants file compressed */
  vbinary->storecompressed = PETSC_FALSE;
  if (!rank && type == PETSC_FILE_CREATE) {
    /* remove .gz if it ends library name */
    ierr = PetscStrstr(vbinary->filename,".gz",&gz);CHKERRQ(ierr);
    if (gz) {
      ierr = PetscStrlen(gz,&len);CHKERRQ(ierr);
      if (len == 3) {
        *gz = 0;
        vbinary->storecompressed = PETSC_TRUE;
      } 
    }
  }

  /* only first processor opens file if writeable */
  if (!rank || type == PETSC_FILE_RDONLY) {

    if (type == PETSC_FILE_RDONLY){
      /* possibly get the file from remote site or compressed file */
      ierr  = PetscFileRetrieve(viewer->comm,vbinary->filename,bname,PETSC_MAX_PATH_LEN,&found);CHKERRQ(ierr);
      fname = bname;
      if (!rank && !found) {
        SETERRQ1(PETSC_ERR_FILE_OPEN,"Cannot locate file: %s on node zero",vbinary->filename);
      } else if (!found) {
        PetscLogInfo(viewer,"Nonzero processor did not locate readonly file");
        fname = 0;
      }
    } else {
      fname = vbinary->filename;
    }

#if defined(PARCH_win32_gnu) || defined(PARCH_win32) 
    if (type == PETSC_FILE_CREATE) {
      if ((vbinary->fdes = open(fname,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0666)) == -1) {
        SETERRQ1(PETSC_ERR_FILE_OPEN,"Cannot create file %s for writing",fname);
      }
    } else if (type == PETSC_FILE_RDONLY && fname) {
      if ((vbinary->fdes = open(fname,O_RDONLY|O_BINARY,0)) == -1) {
        SETERRQ1(PETSC_ERR_FILE_OPEN,"Cannot open file %s for reading",fname);
      }
    } else if (type == PETSC_FILE_WRONLY) {
      if ((vbinary->fdes = open(fname,O_WRONLY|O_BINARY,0)) == -1) {
        SETERRQ1(PETSC_ERR_FILE_OPEN,"Cannot open file %s for writing",fname);
      }
    } else if (fname) {
      SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"Unknown file type");
    }
#else
    if (type == PETSC_FILE_CREATE) {
      if ((vbinary->fdes = creat(fname,0666)) == -1) {
        SETERRQ1(PETSC_ERR_FILE_OPEN,"Cannot create file %s for writing",fname);
      }
    } else if (type == PETSC_FILE_RDONLY && fname) {
      if ((vbinary->fdes = open(fname,O_RDONLY,0)) == -1) {
        SETERRQ1(PETSC_ERR_FILE_OPEN,"Cannot open file %s for reading",fname);
      }
    } else if (type == PETSC_FILE_WRONLY) {
      if ((vbinary->fdes = open(fname,O_WRONLY|O_APPEND,0)) == -1) {
        SETERRQ1(PETSC_ERR_FILE_OPEN,"Cannot open file %s for writing",fname);
      }
    } else if (fname) {
      SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"Unknown file type");
    }
#endif
  } else vbinary->fdes = -1;
  viewer->format = PETSC_VIEWER_NOFORMAT;

  /* 
      try to open info file: all processors open this file if read only
  */
  if (!rank || type == PETSC_FILE_RDONLY) {
    char infoname[PETSC_MAX_PATH_LEN],iname[PETSC_MAX_PATH_LEN];
  
    ierr = PetscStrcpy(infoname,name);CHKERRQ(ierr);
    /* remove .gz if it ends library name */
    ierr = PetscStrstr(infoname,".gz",&gz);CHKERRQ(ierr);
    if (gz) {
      ierr = PetscStrlen(gz,&len);CHKERRQ(ierr);
      if (len == 3) {
        *gz = 0;
      } 
    }
    
    ierr = PetscStrcat(infoname,".info");CHKERRQ(ierr);
    ierr = PetscFixFilename(infoname,iname);CHKERRQ(ierr);
    if (type == PETSC_FILE_RDONLY) {
      ierr = PetscFileRetrieve(viewer->comm,iname,infoname,PETSC_MAX_PATH_LEN,&found);CHKERRQ(ierr);
      if (found) {
        vbinary->fdes_info = fopen(infoname,"r");
        if (vbinary->fdes_info) {
          ierr = PetscViewerBinaryLoadInfo(viewer);CHKERRQ(ierr);
          fclose(vbinary->fdes_info);
        }
        vbinary->fdes_info = fopen(infoname,"r");
      }
    } else if (!vbinary->skipinfo) {
      vbinary->fdes_info = fopen(infoname,"w");
      if (!vbinary->fdes_info) {
        SETERRQ1(PETSC_ERR_FILE_OPEN,"Cannot open .info file %s for writing",infoname);
      }
    }
  }

#if defined(PETSC_USE_LOG)
  PetscLogObjectState((PetscObject)viewer,"File: %s",name);
#endif
  PetscFunctionReturn(0);
}
EXTERN_C_END

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "PetscViewerCreate_Binary" 
PetscErrorCode PetscViewerCreate_Binary(PetscViewer v)
{  
  PetscErrorCode     ierr;
  PetscViewer_Binary *vbinary;

  PetscFunctionBegin;
  ierr               = PetscNew(PetscViewer_Binary,&vbinary);CHKERRQ(ierr);
  v->data            = (void*)vbinary;
  v->ops->destroy    = PetscViewerDestroy_Binary;
  v->ops->flush      = 0;
  v->iformat         = 0;
  vbinary->fdes_info = 0;
  vbinary->fdes      = 0;
  vbinary->skipinfo  = PETSC_FALSE;
  v->ops->getsingleton     = PetscViewerGetSingleton_Binary;
  v->ops->restoresingleton = PetscViewerRestoreSingleton_Binary;
  vbinary->btype           = (PetscViewerFileType) -1; 
  vbinary->storecompressed = PETSC_FALSE;
  vbinary->filename        = 0;

  ierr = PetscObjectComposeFunctionDynamic((PetscObject)v,"PetscViewerSetFilename_C",
                                    "PetscViewerSetFilename_Binary",
                                     PetscViewerSetFilename_Binary);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)v,"PetscViewerSetFileType_C",
                                    "PetscViewerSetFileType_Binary",
                                     PetscViewerSetFileType_Binary);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END


/* ---------------------------------------------------------------------*/
/*
    The variable Petsc_Viewer_Binary_keyval is used to indicate an MPI attribute that
  is attached to a communicator, in this case the attribute is a PetscViewer.
*/
static int Petsc_Viewer_Binary_keyval = MPI_KEYVAL_INVALID;

#undef __FUNCT__  
#define __FUNCT__ "PETSC_VIEWER_BINARY_"  
/*@C
     PETSC_VIEWER_BINARY_ - Creates a binary PetscViewer shared by all processors 
                     in a communicator.

     Collective on MPI_Comm

     Input Parameter:
.    comm - the MPI communicator to share the binary PetscViewer
    
     Level: intermediate

   Options Database Keys:
$    -viewer_binary_filename <name>

   Environmental variables:
-   PETSC_VIEWER_BINARY_FILENAME

     Notes:
     Unlike almost all other PETSc routines, PETSC_VIEWER_BINARY_ does not return 
     an error code.  The binary PetscViewer is usually used in the form
$       XXXView(XXX object,PETSC_VIEWER_BINARY_(comm));

.seealso: PETSC_VIEWER_BINARY_WORLD, PETSC_VIEWER_BINARY_SELF, PetscViewerBinaryOpen(), PetscViewerCreate(),
          PetscViewerDestroy()
@*/
PetscViewer PETSC_VIEWER_BINARY_(MPI_Comm comm)
{
  PetscErrorCode ierr;
  PetscTruth     flg;
  PetscViewer    viewer;
  char           fname[PETSC_MAX_PATH_LEN];

  PetscFunctionBegin;
  if (Petsc_Viewer_Binary_keyval == MPI_KEYVAL_INVALID) {
    ierr = MPI_Keyval_create(MPI_NULL_COPY_FN,MPI_NULL_DELETE_FN,&Petsc_Viewer_Binary_keyval,0);
    if (ierr) {PetscError(__LINE__,"PETSC_VIEWER_BINARY_",__FILE__,__SDIR__,1,1," "); viewer = 0;}
  }
  ierr = MPI_Attr_get(comm,Petsc_Viewer_Binary_keyval,(void **)&viewer,(int*)&flg);
  if (ierr) {PetscError(__LINE__,"PETSC_VIEWER_BINARY_",__FILE__,__SDIR__,1,1," "); viewer = 0;}
  if (!flg) { /* PetscViewer not yet created */
    ierr = PetscOptionsGetenv(comm,"PETSC_VIEWER_BINARY_FILENAME",fname,PETSC_MAX_PATH_LEN,&flg);
    if (ierr) {PetscError(__LINE__,"PETSC_VIEWER_BINARY_",__FILE__,__SDIR__,1,1," "); viewer = 0;}
    if (!flg) {
      ierr = PetscStrcpy(fname,"binaryoutput");
      if (ierr) {PetscError(__LINE__,"PETSC_VIEWER_BINARY_",__FILE__,__SDIR__,1,1," "); viewer = 0;}
    }
    ierr = PetscViewerBinaryOpen(comm,fname,PETSC_FILE_CREATE,&viewer); 
    if (ierr) {PetscError(__LINE__,"PETSC_VIEWER_BINARY_",__FILE__,__SDIR__,1,1," "); viewer = 0;}
    ierr = PetscObjectRegisterDestroy((PetscObject)viewer);
    if (ierr) {PetscError(__LINE__,"PETSC_VIEWER_BINARY_",__FILE__,__SDIR__,1,1," "); viewer = 0;}
    ierr = MPI_Attr_put(comm,Petsc_Viewer_Binary_keyval,(void*)viewer);
    if (ierr) {PetscError(__LINE__,"PETSC_VIEWER_BINARY_",__FILE__,__SDIR__,1,1," "); viewer = 0;}
  } 
  PetscFunctionReturn(viewer);
}







