/* $Id: filev.c,v 1.118 2001/04/10 19:34:05 bsmith Exp $ */

#include "src/sys/src/viewer/viewerimpl.h"  /*I     "petsc.h"   I*/
#include "petscfix.h"
#include <stdarg.h>

typedef struct {
  FILE          *fd;
  PetscFileMode mode;           /* The mode in which to open the file */
  int           tab;            /* how many times text is tabbed in from left */
  int           tab_store;      /* store tabs value while tabs are turned off */
  PetscViewer   bviewer;        /* if PetscViewer is a singleton, this points to mother */
  PetscViewer   sviewer;        /* if PetscViewer has a singleton, this points to singleton */
  char          *filename;
  PetscTruth    storecompressed; 
} PetscViewer_ASCII;

/* ----------------------------------------------------------------------*/
#undef __FUNCT__  
#define __FUNCT__ "PetscViewerDestroy_ASCII" 
int PetscViewerDestroy_ASCII(PetscViewer viewer)
{
  int               rank,ierr;
  PetscViewer_ASCII *vascii = (PetscViewer_ASCII *)viewer->data;

  PetscFunctionBegin;
  if (vascii->sviewer) {
    SETERRQ(1,"ASCII PetscViewer destroyed before restoring singleton PetscViewer");
  }
  ierr = MPI_Comm_rank(viewer->comm,&rank);CHKERRQ(ierr);
  if (!rank && vascii->fd != stderr && vascii->fd != stdout) {
    fclose(vascii->fd);
    if (vascii->storecompressed) {
      char par[1024],buf[1024];
      FILE *fp;
      ierr = PetscStrcpy(par,"gzip ");CHKERRQ(ierr);
      ierr = PetscStrcat(par,vascii->filename);CHKERRQ(ierr);
      ierr = PetscPOpen(PETSC_COMM_SELF,PETSC_NULL,par,"r",&fp);CHKERRQ(ierr);
      if (fgets(buf,1024,fp)) {
        SETERRQ2(1,"Error from compression command %s\n%s",par,buf);
      }
    }
  }
  ierr = PetscStrfree(vascii->filename);CHKERRQ(ierr);
  ierr = PetscFree(vascii);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerDestroy_ASCII_Singleton" 
int PetscViewerDestroy_ASCII_Singleton(PetscViewer viewer)
{
  PetscViewer_ASCII *vascii = (PetscViewer_ASCII *)viewer->data;
  int               ierr;
  PetscFunctionBegin;
  ierr = PetscViewerRestoreSingleton(vascii->bviewer,&viewer);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerFlush_ASCII_Singleton_0" 
int PetscViewerFlush_ASCII_Singleton_0(PetscViewer viewer)
{
  PetscViewer_ASCII *vascii = (PetscViewer_ASCII *)viewer->data;

  PetscFunctionBegin;
  fflush(vascii->fd);
  PetscFunctionReturn(0);  
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerFlush_ASCII" 
int PetscViewerFlush_ASCII(PetscViewer viewer)
{
  int               rank,ierr;
  PetscViewer_ASCII *vascii = (PetscViewer_ASCII *)viewer->data;

  PetscFunctionBegin;
  ierr = MPI_Comm_rank(viewer->comm,&rank);CHKERRQ(ierr);
  if (!rank) {
    fflush(vascii->fd);
  }

  /*
     Also flush anything printed with PetscViewerASCIISynchronizedPrintf()
  */
  ierr = PetscSynchronizedFlush(viewer->comm);CHKERRQ(ierr);
  PetscFunctionReturn(0);  
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerASCIIGetPointer" 
/*@C
    PetscViewerASCIIGetPointer - Extracts the file pointer from an ASCII PetscViewer.

    Not Collective

+   viewer - PetscViewer context, obtained from PetscViewerASCIIOpen()
-   fd - file pointer

    Level: intermediate

    Fortran Note:
    This routine is not supported in Fortran.

  Concepts: PetscViewer^file pointer
  Concepts: file pointer^getting from PetscViewer

.seealso: PetscViewerASCIIOpen(), PetscViewerDestroy(), PetscViewerSetType(), PetscViewerCreate(), PetscViewerASCIIPrintf(),
          PetscViewerASCIISynchronizedPrintf(), PetscViewerFlush()
@*/
int PetscViewerASCIIGetPointer(PetscViewer viewer,FILE **fd)
{
  PetscViewer_ASCII *vascii = (PetscViewer_ASCII *)viewer->data;

  PetscFunctionBegin;
  *fd = vascii->fd;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerASCIISetMode"
/*@C
    PetscViewerASCIISetMode - Sets the mode in which to open the file.

    Not Collective

+   viewer - viewer context, obtained from PetscViewerASCIIOpen()
-   mode   - The file mode

    Level: intermediate

    Fortran Note:
    This routine is not supported in Fortran.

.keywords: Viewer, file, get, pointer

.seealso: PetscViewerASCIIOpen()
@*/
int PetscViewerASCIISetMode(PetscViewer viewer, PetscFileMode mode)
{
  PetscViewer_ASCII *vascii = (PetscViewer_ASCII *)viewer->data;

  PetscFunctionBegin;
  vascii->mode = mode;
  PetscFunctionReturn(0);
}

/*
   If petsc_history is on, then all Petsc*Printf() results are saved
   if the appropriate (usually .petschistory) file.
*/
extern FILE *petsc_history;

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerASCIISetTab" 
/*@C
    PetscViewerASCIISetTab - Causes PetscViewer to tab in a number of times

    Not Collective, but only first processor in set has any effect

    Input Parameters:
+    viewer - optained with PetscViewerASCIIOpen()
-    tabs - number of tabs

    Level: developer

    Fortran Note:
    This routine is not supported in Fortran.

  Concepts: PetscViewerASCII^formating
  Concepts: tab^setting

.seealso: PetscPrintf(), PetscSynchronizedPrintf(), PetscViewerASCIIPrintf(),
          PetscViewerASCIIPopTab(), PetscViewerASCIISynchronizedPrintf(), PetscViewerASCIIOpen(),
          PetscViewerCreate(), PetscViewerDestroy(), PetscViewerSetType(), PetscViewerASCIIGetPointer(), PetscViewerASCIIPushTab()
@*/
int PetscViewerASCIISetTab(PetscViewer viewer,int tabs)
{
  PetscViewer_ASCII *ascii = (PetscViewer_ASCII*)viewer->data;
  PetscTruth        isascii;
  int               ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer,PETSC_VIEWER_COOKIE);
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_ASCII,&isascii);CHKERRQ(ierr);
  if (isascii) {
    ascii->tab = tabs;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerASCIIPushTab" 
/*@C
    PetscViewerASCIIPushTab - Adds one more tab to the amount that PetscViewerASCIIPrintf()
     lines are tabbed.

    Not Collective, but only first processor in set has any effect

    Input Parameters:
.    viewer - optained with PetscViewerASCIIOpen()

    Level: developer

    Fortran Note:
    This routine is not supported in Fortran.

  Concepts: PetscViewerASCII^formating
  Concepts: tab^setting

.seealso: PetscPrintf(), PetscSynchronizedPrintf(), PetscViewerASCIIPrintf(),
          PetscViewerASCIIPopTab(), PetscViewerASCIISynchronizedPrintf(), PetscViewerASCIIOpen(),
          PetscViewerCreate(), PetscViewerDestroy(), PetscViewerSetType(), PetscViewerASCIIGetPointer()
@*/
int PetscViewerASCIIPushTab(PetscViewer viewer)
{
  PetscViewer_ASCII *ascii = (PetscViewer_ASCII*)viewer->data;
  PetscTruth        isascii;
  int               ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer,PETSC_VIEWER_COOKIE);
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_ASCII,&isascii);CHKERRQ(ierr);
  if (isascii) {
    ascii->tab++;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerASCIIPopTab" 
/*@C
    PetscViewerASCIIPopTab - Removes one tab from the amount that PetscViewerASCIIPrintf()
     lines are tabbed.

    Not Collective, but only first processor in set has any effect

    Input Parameters:
.    viewer - optained with PetscViewerASCIIOpen()

    Level: developer

    Fortran Note:
    This routine is not supported in Fortran.

  Concepts: PetscViewerASCII^formating
  Concepts: tab^setting

.seealso: PetscPrintf(), PetscSynchronizedPrintf(), PetscViewerASCIIPrintf(),
          PetscViewerASCIIPushTab(), PetscViewerASCIISynchronizedPrintf(), PetscViewerASCIIOpen(),
          PetscViewerCreate(), PetscViewerDestroy(), PetscViewerSetType(), PetscViewerASCIIGetPointer()
@*/
int PetscViewerASCIIPopTab(PetscViewer viewer)
{
  PetscViewer_ASCII *ascii = (PetscViewer_ASCII*)viewer->data;
  int               ierr;
  PetscTruth        isascii;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer,PETSC_VIEWER_COOKIE);
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_ASCII,&isascii);CHKERRQ(ierr);
  if (isascii) {
    if (ascii->tab <= 0) SETERRQ(1,"More tabs popped than pushed");
    ascii->tab--;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerASCIIUseTabs" 
/*@C
    PetscViewerASCIIUseTabs - Turns on or off the use of tabs with the ASCII PetscViewer

    Not Collective, but only first processor in set has any effect

    Input Parameters:
+    viewer - optained with PetscViewerASCIIOpen()
-    flg - PETSC_YES or PETSC_NO

    Level: developer

    Fortran Note:
    This routine is not supported in Fortran.

  Concepts: PetscViewerASCII^formating
  Concepts: tab^setting

.seealso: PetscPrintf(), PetscSynchronizedPrintf(), PetscViewerASCIIPrintf(),
          PetscViewerASCIIPopTab(), PetscViewerASCIISynchronizedPrintf(), PetscViewerASCIIPushTab(), PetscViewerASCIIOpen(),
          PetscViewerCreate(), PetscViewerDestroy(), PetscViewerSetType(), PetscViewerASCIIGetPointer()
@*/
int PetscViewerASCIIUseTabs(PetscViewer viewer,PetscTruth flg)
{
  PetscViewer_ASCII *ascii = (PetscViewer_ASCII*)viewer->data;
  PetscTruth        isascii;
  int               ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer,PETSC_VIEWER_COOKIE);
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_ASCII,&isascii);CHKERRQ(ierr);
  if (isascii) {
    if (flg) {
      ascii->tab       = ascii->tab_store;
    } else {
      ascii->tab_store = ascii->tab;
      ascii->tab       = 0;
    }
  }
  PetscFunctionReturn(0);
}

/* ----------------------------------------------------------------------- */

#include "src/sys/src/fileio/mprint.h" /* defines the queue datastructures and variables */

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerASCIIPrintf" 
/*@C
    PetscViewerASCIIPrintf - Prints to a file, only from the first
    processor in the PetscViewer

    Not Collective, but only first processor in set has any effect

    Input Parameters:
+    viewer - optained with PetscViewerASCIIOpen()
-    format - the usual printf() format string 

    Level: developer

    Fortran Note:
    This routine is not supported in Fortran.

  Concepts: PetscViewerASCII^printing
  Concepts: printing^to file
  Concepts: printf

.seealso: PetscPrintf(), PetscSynchronizedPrintf(), PetscViewerASCIIOpen(),
          PetscViewerASCIIPushTab(), PetscViewerASCIIPopTab(), PetscViewerASCIISynchronizedPrintf(),
          PetscViewerCreate(), PetscViewerDestroy(), PetscViewerSetType(), PetscViewerASCIIGetPointer()
@*/
int PetscViewerASCIIPrintf(PetscViewer viewer,const char format[],...)
{
  PetscViewer_ASCII *ascii = (PetscViewer_ASCII*)viewer->data;
  int               rank,tab,ierr;
  FILE              *fd = ascii->fd;
  PetscTruth        isascii;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer,PETSC_VIEWER_COOKIE);
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_ASCII,&isascii);CHKERRQ(ierr);
  if (!isascii) SETERRQ(1,"Not ASCII PetscViewer");

  ierr = MPI_Comm_rank(viewer->comm,&rank);CHKERRQ(ierr);
  if (ascii->bviewer) {ierr = MPI_Comm_rank(ascii->bviewer->comm,&rank);CHKERRQ(ierr);}
  if (!rank) {
    va_list Argp;
    if (ascii->bviewer) {
      queuefile = fd;
    }

    tab = ascii->tab;
    while (tab--) fprintf(fd,"  ");

    va_start(Argp,format);
#if defined(PETSC_HAVE_VPRINTF_CHAR)
    vfprintf(fd,format,(char*)Argp);
#else
    vfprintf(fd,format,Argp);
#endif
    fflush(fd);
    if (petsc_history) {
      tab = ascii->tab;
      while (tab--) fprintf(fd,"  ");
#if defined(PETSC_HAVE_VPRINTF_CHAR)
      vfprintf(petsc_history,format,(char *)Argp);
#else
      vfprintf(petsc_history,format,Argp);
#endif
      fflush(petsc_history);
    }
    va_end(Argp);
  } else if (ascii->bviewer) { /* this is a singleton PetscViewer that is not on process 0 */
    int         len;
    va_list     Argp;

    PrintfQueue next;
    ierr = PetscNew(struct _PrintfQueue,&next);CHKERRQ(ierr);
    if (queue) {queue->next = next; queue = next;}
    else       {queuebase   = queue = next;}
    queuelength++;
    va_start(Argp,format);
#if defined(PETSC_HAVE_VPRINTF_CHAR)
    vsprintf(next->string,format,(char *)Argp);
#else
    vsprintf(next->string,format,Argp);
#endif
    va_end(Argp);
    ierr = PetscStrlen(next->string,&len);CHKERRQ(ierr);
    if (len > QUEUESTRINGSIZE) SETERRQ1(PETSC_ERR_ARG_OUTOFRANGE,"Formatted string longer then %d bytes",QUEUESTRINGSIZE);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerSetFilename" 
/*@C
     PetscViewerSetFilename - Sets the name of the file the PetscViewer uses.

    Collective on PetscViewer

  Input Parameters:
+  viewer - the PetscViewer; either ASCII or binary
-  name - the name of the file it should use

    Level: advanced

.seealso: PetscViewerCreate(), PetscViewerSetType(), PetscViewerASCIIOpen(), PetscViewerBinaryOpen(), PetscViewerDestroy(),
          PetscViewerASCIIGetPointer(), PetscViewerASCIIPrintf(), PetscViewerASCIISynchronizedPrintf()

@*/
int PetscViewerSetFilename(PetscViewer viewer,const char name[])
{
  int ierr,(*f)(PetscViewer,const char[]);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer,PETSC_VIEWER_COOKIE);
  if (!name) SETERRQ(1,"You must pass in non-null string");
  ierr = PetscObjectQueryFunction((PetscObject)viewer,"PetscViewerSetFilename_C",(void (**)(void))&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(viewer,name);CHKERRQ(ierr);
  }

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerGetFilename" 
/*@C
     PetscViewerGetFilename - Gets the name of the file the PetscViewer uses.

    Not Collective

  Input Parameter:
.  viewer - the PetscViewer; either ASCII or binary

  Output Parameter:
.  name - the name of the file it is using

    Level: advanced

.seealso: PetscViewerCreate(), PetscViewerSetType(), PetscViewerASCIIOpen(), PetscViewerBinaryOpen(), PetscViewerSetFilename()

@*/
int PetscViewerGetFilename(PetscViewer viewer,char **name)
{
  int ierr,(*f)(PetscViewer,char **);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer,PETSC_VIEWER_COOKIE);
  ierr = PetscObjectQueryFunction((PetscObject)viewer,"PetscViewerGetFilename_C",(void (**)(void))&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(viewer,name);CHKERRQ(ierr);
  }

  PetscFunctionReturn(0);
}

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "PetscViewerGetFilename_ASCII" 
int PetscViewerGetFilename_ASCII(PetscViewer viewer,char **name)
{
  PetscViewer_ASCII *vascii = (PetscViewer_ASCII*)viewer->data;

  PetscFunctionBegin;
  *name = vascii->filename;
  PetscFunctionReturn(0);
}
EXTERN_C_END

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "PetscViewerSetFilename_ASCII" 
int PetscViewerSetFilename_ASCII(PetscViewer viewer,const char name[])
{
  int               ierr,len;
  char              fname[256],*gz;
  PetscViewer_ASCII *vascii = (PetscViewer_ASCII*)viewer->data;
  PetscTruth        isstderr,isstdout;

  PetscFunctionBegin;
  if (!name) PetscFunctionReturn(0);

  ierr = PetscStrallocpy(name,&vascii->filename);CHKERRQ(ierr);

  /* Is this file to be compressed */
  vascii->storecompressed = PETSC_FALSE;
  ierr = PetscStrstr(vascii->filename,".gz",&gz);CHKERRQ(ierr);
  if (gz) {
    ierr = PetscStrlen(gz,&len);CHKERRQ(ierr);
    if (len == 3) {
      *gz = 0;
      vascii->storecompressed = PETSC_TRUE;
    } 
  }
  ierr = PetscStrcmp(name,"stderr",&isstderr);CHKERRQ(ierr);
  ierr = PetscStrcmp(name,"stdout",&isstdout);CHKERRQ(ierr);
  if (isstderr)      vascii->fd = stderr;
  else if (isstdout) vascii->fd = stdout;
  else {
    ierr = PetscFixFilename(name,fname);CHKERRQ(ierr);
    switch(vascii->mode) {
    case FILE_MODE_READ:
      vascii->fd = fopen(fname,"r");
      break;
    case FILE_MODE_WRITE:
      vascii->fd = fopen(fname,"w");
      break;
    case FILE_MODE_APPEND:
      vascii->fd = fopen(fname,"a");
      break;
    case FILE_MODE_UPDATE:
      vascii->fd = fopen(fname,"r+");
      if (vascii->fd == PETSC_NULL) {
        vascii->fd = fopen(fname,"w+");
      }
      break;
    case FILE_MODE_APPEND_UPDATE:
      /* I really want a file which is opened at the end for updating,
         not a+, which opens at the beginning, but makes writes at the end.
      */
      vascii->fd = fopen(fname,"r+");
      if (vascii->fd == PETSC_NULL) {
        vascii->fd = fopen(fname,"w+");
      } else {
        ierr     = fseek(vascii->fd, 0, SEEK_END);                                                        CHKERRQ(ierr);
      }
      break;
    default:
      SETERRQ1(PETSC_ERR_ARG_WRONG, "Invalid file mode %d", vascii->mode);
    }

    if (!vascii->fd) SETERRQ1(PETSC_ERR_FILE_OPEN,"Cannot open PetscViewer file: %s",fname);
  }
#if defined(PETSC_USE_LOG)
  PetscLogObjectState((PetscObject)viewer,"File: %s",name);
#endif

  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerGetSingleton_ASCII" 
int PetscViewerGetSingleton_ASCII(PetscViewer viewer,PetscViewer *outviewer)
{
  int               rank,ierr;
  PetscViewer_ASCII *vascii = (PetscViewer_ASCII *)viewer->data,*ovascii;
  char              *name;

  PetscFunctionBegin;
  if (vascii->sviewer) {
    SETERRQ(1,"Singleton already obtained from PetscViewer and not restored");
  }
  ierr         = PetscViewerCreate(PETSC_COMM_SELF,outviewer);CHKERRQ(ierr);
  ierr         = PetscViewerSetType(*outviewer,PETSC_VIEWER_ASCII);CHKERRQ(ierr);
  ovascii      = (PetscViewer_ASCII*)(*outviewer)->data;
  ovascii->fd  = vascii->fd;
  ovascii->tab = vascii->tab;

  vascii->sviewer = *outviewer;

  (*outviewer)->format     = viewer->format;
  (*outviewer)->iformat    = viewer->iformat;

  ierr = PetscObjectGetName((PetscObject)viewer,&name);CHKERRQ(ierr);
  ierr = PetscObjectSetName((PetscObject)(*outviewer),name);CHKERRQ(ierr);

  ierr = MPI_Comm_rank(viewer->comm,&rank);CHKERRQ(ierr);
  ((PetscViewer_ASCII*)((*outviewer)->data))->bviewer = viewer;
  (*outviewer)->ops->destroy = PetscViewerDestroy_ASCII_Singleton;
  if (rank) {
    (*outviewer)->ops->flush = 0;
  } else {
    (*outviewer)->ops->flush = PetscViewerFlush_ASCII_Singleton_0;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerRestoreSingleton_ASCII" 
int PetscViewerRestoreSingleton_ASCII(PetscViewer viewer,PetscViewer *outviewer)
{
  int               ierr;
  PetscViewer_ASCII *vascii = (PetscViewer_ASCII *)(*outviewer)->data;
  PetscViewer_ASCII *ascii  = (PetscViewer_ASCII *)viewer->data;

  PetscFunctionBegin;
  if (!ascii->sviewer) {
    SETERRQ(1,"Singleton never obtained from PetscViewer");
  }
  if (ascii->sviewer != *outviewer) {
    SETERRQ(1,"This PetscViewer did not generate singleton");
  }

  ascii->sviewer             = 0;
  vascii->fd                 = stdout;
  (*outviewer)->ops->destroy = PetscViewerDestroy_ASCII;
  ierr                       = PetscViewerDestroy(*outviewer);CHKERRQ(ierr);
  ierr = PetscViewerFlush(viewer);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "PetscViewerCreate_ASCII" 
int PetscViewerCreate_ASCII(PetscViewer viewer)
{
  PetscViewer_ASCII *vascii;
  int               ierr;

  PetscFunctionBegin;
  ierr         = PetscNew(PetscViewer_ASCII,&vascii);CHKERRQ(ierr);
  viewer->data = (void*)vascii;

  viewer->ops->destroy          = PetscViewerDestroy_ASCII;
  viewer->ops->flush            = PetscViewerFlush_ASCII;
  viewer->ops->getsingleton     = PetscViewerGetSingleton_ASCII;
  viewer->ops->restoresingleton = PetscViewerRestoreSingleton_ASCII;

  /* defaults to stdout unless set with PetscViewerSetFilename() */
  vascii->fd             = stdout;
  vascii->mode           = FILE_MODE_WRITE;
  vascii->bviewer        = 0;
  vascii->sviewer        = 0;
  viewer->format         = PETSC_VIEWER_ASCII_DEFAULT;
  viewer->iformat        = 0;
  vascii->tab            = 0;
  vascii->tab_store      = 0;
  vascii->filename       = 0;

  ierr = PetscObjectComposeFunctionDynamic((PetscObject)viewer,"PetscViewerSetFilename_C","PetscViewerSetFilename_ASCII",
                                     PetscViewerSetFilename_ASCII);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)viewer,"PetscViewerGetFilename_C","PetscViewerGetFilename_ASCII",
                                     PetscViewerGetFilename_ASCII);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}
EXTERN_C_END


#undef __FUNCT__  
#define __FUNCT__ "PetscViewerASCIISynchronizedPrintf" 
/*@C
    PetscViewerASCIISynchronizedFPrintf - Prints synchronized output to the specified file from
    several processors.  Output of the first processor is followed by that of the 
    second, etc.

    Not Collective, must call collective PetscViewerFlush() to get the results out

    Input Parameters:
+   viewer - the ASCII PetscViewer
-   format - the usual printf() format string 

    Level: intermediate

.seealso: PetscSynchronizedPrintf(), PetscSynchronizedFlush(), PetscFPrintf(),
          PetscFOpen(), PetscViewerFlush(), PetscViewerASCIIGetPointer(), PetscViewerDestroy(), PetscViewerASCIIOpen(),
          PetscViewerASCIIPrintf()

@*/
int PetscViewerASCIISynchronizedPrintf(PetscViewer viewer,const char format[],...)
{
  PetscViewer_ASCII *vascii = (PetscViewer_ASCII *)viewer->data;
  int               ierr,rank,tab = vascii->tab;
  MPI_Comm          comm;
  FILE              *fp;
  PetscTruth        isascii;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer,PETSC_VIEWER_COOKIE);
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_ASCII,&isascii);CHKERRQ(ierr);
  if (!isascii) SETERRQ(1,"Not ASCII PetscViewer");

  comm = viewer->comm;
  fp   = vascii->fd;
  ierr = MPI_Comm_rank(comm,&rank);CHKERRQ(ierr);
  if (vascii->bviewer) {ierr = MPI_Comm_rank(vascii->bviewer->comm,&rank);CHKERRQ(ierr);}
  

  /* First processor prints immediately to fp */
  if (!rank) {
    va_list Argp;

    while (tab--) fprintf(fp,"  ");

    va_start(Argp,format);
#if defined(PETSC_HAVE_VPRINTF_CHAR)
    vfprintf(fp,format,(char*)Argp);
#else
    vfprintf(fp,format,Argp);
#endif
    fflush(fp);
    queuefile = fp;
    if (petsc_history) {
#if defined(PETSC_HAVE_VPRINTF_CHAR)
      vfprintf(petsc_history,format,(char *)Argp);
#else
      vfprintf(petsc_history,format,Argp);
#endif
      fflush(petsc_history);
    }
    va_end(Argp);
  } else { /* other processors add to local queue */
    int         len;
    char        *string;
    va_list     Argp;
    PrintfQueue next;

    ierr = PetscNew(struct _PrintfQueue,&next);CHKERRQ(ierr);
    if (queue) {queue->next = next; queue = next;}
    else       {queuebase   = queue = next;}
    queuelength++;
    string = next->string;
    while (tab--) {*string++ = ' ';}
    va_start(Argp,format);
#if defined(PETSC_HAVE_VPRINTF_CHAR)
    vsprintf(string,format,(char *)Argp);
#else
    vsprintf(string,format,Argp);
#endif
    va_end(Argp);
    ierr = PetscStrlen(next->string,&len);CHKERRQ(ierr);
    if (len > QUEUESTRINGSIZE) SETERRQ1(PETSC_ERR_ARG_OUTOFRANGE,"Formatted string longer then %d bytes",QUEUESTRINGSIZE);
  }
  PetscFunctionReturn(0);
}


