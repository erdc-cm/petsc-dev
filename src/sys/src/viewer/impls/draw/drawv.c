
#include "src/sys/src/viewer/impls/draw/vdraw.h" /*I "petscdraw.h" I*/

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerDestroy_Draw" 
int PetscViewerDestroy_Draw(PetscViewer v)
{
  int              ierr,i;
  PetscViewer_Draw *vdraw = (PetscViewer_Draw*)v->data;

  PetscFunctionBegin;
  if (vdraw->singleton_made) {
    SETERRQ(PETSC_ERR_ORDER,"Destroying PetscViewer without first restoring singleton");
  }
  for (i=0; i<vdraw->draw_max; i++) {
    if (vdraw->drawaxis[i]) {ierr = PetscDrawAxisDestroy(vdraw->drawaxis[i]);CHKERRQ(ierr);}
    if (vdraw->drawlg[i])   {ierr = PetscDrawLGDestroy(vdraw->drawlg[i]);CHKERRQ(ierr);}
    if (vdraw->draw[i])     {ierr = PetscDrawDestroy(vdraw->draw[i]);CHKERRQ(ierr);}
  }
  ierr = PetscFree(vdraw->drawaxis);CHKERRQ(ierr);
  ierr = PetscFree(vdraw->drawlg);CHKERRQ(ierr);
  ierr = PetscFree(vdraw->draw);CHKERRQ(ierr);
  ierr = PetscFree(vdraw);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerFlush_Draw" 
int PetscViewerFlush_Draw(PetscViewer v)
{
  int              ierr,i;
  PetscViewer_Draw *vdraw = (PetscViewer_Draw*)v->data;

  PetscFunctionBegin;
  for (i=0; i<vdraw->draw_max; i++) {
    if (vdraw->draw[i]) {ierr = PetscDrawSynchronizedFlush(vdraw->draw[i]);CHKERRQ(ierr);}
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerDrawGetDraw" 
/*@C
    PetscViewerDrawGetDraw - Returns PetscDraw object from PetscViewer object.
    This PetscDraw object may then be used to perform graphics using 
    PetscDrawXXX() commands.

    Not collective (but PetscDraw returned will be parallel object if PetscViewer is)

    Input Parameters:
+  viewer - the PetscViewer (created with PetscViewerDrawOpen()
-   windownumber - indicates which subwindow (usually 0)

    Ouput Parameter:
.   draw - the draw object

    Level: intermediate

   Concepts: drawing^accessing PetscDraw context from PetscViewer
   Concepts: graphics

.seealso: PetscViewerDrawGetLG(), PetscViewerDrawGetAxis(), PetscViewerDrawOpen()
@*/
int PetscViewerDrawGetDraw(PetscViewer viewer,int windownumber,PetscDraw *draw)
{
  PetscViewer_Draw *vdraw = (PetscViewer_Draw*)viewer->data;
  int              ierr;
  PetscTruth       isdraw;
  char             *title;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer,PETSC_VIEWER_COOKIE,1);
  PetscValidPointer(draw,3);
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_DRAW,&isdraw);CHKERRQ(ierr);
  if (!isdraw) {
    SETERRQ(PETSC_ERR_ARG_WRONG,"Must be draw type PetscViewer");
  }
  if (windownumber < 0) {
    SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"Window number cannot be negative");
  }
  if (windownumber >= vdraw->draw_max) {
     /* allocate twice as many slots as needed */
     int           draw_max = vdraw->draw_max;
     PetscDraw     *tdraw = vdraw->draw;
     PetscDrawLG   *drawlg = vdraw->drawlg;
     PetscDrawAxis *drawaxis = vdraw->drawaxis;

     vdraw->draw_max = 2*windownumber;
     ierr = PetscMalloc(vdraw->draw_max*sizeof(PetscDraw),&vdraw->draw);CHKERRQ(ierr);
     ierr = PetscMemzero(vdraw->draw,vdraw->draw_max*sizeof(PetscDraw));CHKERRQ(ierr);
     ierr = PetscMalloc(vdraw->draw_max*sizeof(PetscDrawLG),&vdraw->drawlg);CHKERRQ(ierr);
     ierr = PetscMemzero(vdraw->drawlg,vdraw->draw_max*sizeof(PetscDrawLG));CHKERRQ(ierr);
     ierr = PetscMalloc(vdraw->draw_max*sizeof(PetscDrawAxis),&vdraw->drawaxis);CHKERRQ(ierr);
     ierr = PetscMemzero(vdraw->drawaxis,vdraw->draw_max*sizeof(PetscDrawAxis));CHKERRQ(ierr);

     ierr = PetscMemcpy(vdraw->draw,tdraw,draw_max*sizeof(PetscDraw));CHKERRQ(ierr);
     ierr = PetscMemcpy(vdraw->drawlg,drawlg,draw_max*sizeof(PetscDrawLG));CHKERRQ(ierr);
     ierr = PetscMemcpy(vdraw->drawaxis,drawaxis,draw_max*sizeof(PetscDrawAxis));CHKERRQ(ierr);

     ierr = PetscFree(tdraw);CHKERRQ(ierr);
     ierr = PetscFree(drawlg);CHKERRQ(ierr);
     ierr = PetscFree(drawaxis);CHKERRQ(ierr);
  }

  if (!vdraw->draw[windownumber]) {
    if (vdraw->draw[0]) {
      ierr = PetscDrawGetTitle(vdraw->draw[0],&title);CHKERRQ(ierr);
    } else title = 0;
    ierr = PetscDrawCreate(viewer->comm,vdraw->display,title,PETSC_DECIDE,PETSC_DECIDE,vdraw->w,vdraw->h,
                     &vdraw->draw[windownumber]);CHKERRQ(ierr);
    ierr = PetscDrawSetFromOptions(vdraw->draw[windownumber]);CHKERRQ(ierr);
  }
  *draw = vdraw->draw[windownumber];
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerDrawGetDrawLG" 
/*@C
    PetscViewerDrawGetDrawLG - Returns PetscDrawLG object from PetscViewer object.
    This PetscDrawLG object may then be used to perform graphics using 
    PetscDrawLGXXX() commands.

    Not Collective (but PetscDrawLG object will be parallel if PetscViewer is)

    Input Parameter:
+   PetscViewer - the PetscViewer (created with PetscViewerDrawOpen())
-   windownumber - indicates which subwindow (usually 0)

    Ouput Parameter:
.   draw - the draw line graph object

    Level: intermediate

  Concepts: line graph^accessing context

.seealso: PetscViewerDrawGetDraw(), PetscViewerDrawGetAxis(), PetscViewerDrawOpen()
@*/
int PetscViewerDrawGetDrawLG(PetscViewer viewer,int windownumber,PetscDrawLG *drawlg)
{
  int              ierr;
  PetscTruth       isdraw;
  PetscViewer_Draw *vdraw = (PetscViewer_Draw*)viewer->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer,PETSC_VIEWER_COOKIE,1);
  PetscValidPointer(drawlg,3);
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_DRAW,&isdraw);CHKERRQ(ierr);
  if (!isdraw) {
    SETERRQ(PETSC_ERR_ARG_WRONG,"Must be draw type PetscViewer");
  }
  if (windownumber < 0) {
    SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"Window number cannot be negative");
  }
  if (windownumber >= vdraw->draw_max || !vdraw->draw[windownumber]) {
    SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"No window with that number created yet");
  }

  if (!vdraw->drawlg[windownumber]) {
    ierr = PetscDrawLGCreate(vdraw->draw[windownumber],1,&vdraw->drawlg[windownumber]);CHKERRQ(ierr);
    PetscLogObjectParent(viewer,vdraw->drawlg[windownumber]);
  }
  *drawlg = vdraw->drawlg[windownumber];
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerDrawGetDrawAxis" 
/*@C
    PetscViewerDrawGetDrawAxis - Returns PetscDrawAxis object from PetscViewer object.
    This PetscDrawAxis object may then be used to perform graphics using 
    PetscDrawAxisXXX() commands.

    Not Collective (but PetscDrawAxis object will be parallel if PetscViewer is)

    Input Parameter:
+   viewer - the PetscViewer (created with PetscViewerDrawOpen()
-   windownumber - indicates which subwindow (usually 0)

    Ouput Parameter:
.   drawaxis - the draw axis object

    Level: advanced

  Concepts: line graph^accessing context

.seealso: PetscViewerDrawGetDraw(), PetscViewerDrawGetLG(), PetscViewerDrawOpen()
@*/
int PetscViewerDrawGetDrawAxis(PetscViewer viewer,int windownumber,PetscDrawAxis *drawaxis)
{
  int              ierr;
  PetscTruth       isdraw;
  PetscViewer_Draw *vdraw = (PetscViewer_Draw*)viewer->data;;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer,PETSC_VIEWER_COOKIE,1);
  PetscValidPointer(drawaxis,3);
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_DRAW,&isdraw);CHKERRQ(ierr);
  if (!isdraw) {
    SETERRQ(PETSC_ERR_ARG_WRONG,"Must be draw type PetscViewer");
  }
  if (windownumber < 0) {
    SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"Window number cannot be negative");
  }
  if (windownumber >= vdraw->draw_max || !vdraw->draw[windownumber]) {
    SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"No window with that number created yet");
  }

  if (!vdraw->drawaxis[windownumber]) {
    ierr = PetscDrawAxisCreate(vdraw->draw[windownumber],&vdraw->drawaxis[windownumber]);CHKERRQ(ierr);
    PetscLogObjectParent(viewer,vdraw->drawaxis[windownumber]);
  }
  *drawaxis = vdraw->drawaxis[windownumber];
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerDrawSetInfo" 
int PetscViewerDrawSetInfo(PetscViewer v,const char display[],const char title[],int x,int y,int w,int h)
{
  int              ierr;
  PetscViewer_Draw *vdraw = (PetscViewer_Draw*)v->data;

  PetscFunctionBegin;
  vdraw->h  = h;
  vdraw->w  = w;
  ierr      = PetscStrallocpy(display,&vdraw->display);CHKERRQ(ierr);
  ierr      = PetscDrawCreate(v->comm,display,title,x,y,w,h,&vdraw->draw[0]);CHKERRQ(ierr);
  ierr      = PetscDrawSetFromOptions(vdraw->draw[0]);CHKERRQ(ierr);
  PetscLogObjectParent(v,vdraw->draw[0]);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerDrawOpen" 
/*@C
   PetscViewerDrawOpen - Opens an X window for use as a PetscViewer. If you want to 
   do graphics in this window, you must call PetscViewerDrawGetDraw() and
   perform the graphics on the PetscDraw object.

   Collective on MPI_Comm

   Input Parameters:
+  comm - communicator that will share window
.  display - the X display on which to open, or null for the local machine
.  title - the title to put in the title bar, or null for no title
.  x, y - the screen coordinates of the upper left corner of window, or use PETSC_DECIDE
-  w, h - window width and height in pixels, or may use PETSC_DECIDE or PETSC_DRAW_FULL_SIZE, PETSC_DRAW_HALF_SIZE,
          PETSC_DRAW_THIRD_SIZE, PETSC_DRAW_QUARTER_SIZE

   Output Parameters:
. viewer - the PetscViewer

   Format Options:
+  PETSC_VIEWER_DRAW_BASIC - displays with basic format
-  PETSC_VIEWER_DRAW_LG    - displays using a line graph

   Options Database Keys:
   PetscViewerDrawOpen() calls PetscDrawCreate(), so see the manual page for
   PetscDrawCreate() for runtime options, including
+  -draw_type x or null
.  -nox - Disables all x-windows output
.  -display <name> - Specifies name of machine for the X display
-  -draw_pause <pause> - Sets time (in seconds) that the
     program pauses after PetscDrawPause() has been called
     (0 is default, -1 implies until user input).

   Level: beginner

   Note for Fortran Programmers:
   Whenever indicating null character data in a Fortran code,
   PETSC_NULL_CHARACTER must be employed; using PETSC_NULL is not
   correct for character data!  Thus, PETSC_NULL_CHARACTER can be
   used for the display and title input parameters.

  Concepts: graphics^opening PetscViewer
  Concepts: drawing^opening PetscViewer


.seealso: PetscDrawCreate(), PetscViewerDestroy(), PetscViewerDrawGetDraw(), PetscViewerCreate(), PETSC_VIEWER_DRAW_,
          PETSC_VIEWER_DRAW_WORLD, PETSC_VIEWER_DRAW_SELF
@*/
int PetscViewerDrawOpen(MPI_Comm comm,const char display[],const char title[],int x,int y,int w,int h,PetscViewer *viewer)
{
  int ierr;

  PetscFunctionBegin;
  ierr = PetscViewerCreate(comm,viewer);CHKERRQ(ierr);
  ierr = PetscViewerSetType(*viewer,PETSC_VIEWER_DRAW);CHKERRQ(ierr);
  ierr = PetscViewerDrawSetInfo(*viewer,display,title,x,y,w,h);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerGetSingleton_Draw" 
int PetscViewerGetSingleton_Draw(PetscViewer viewer,PetscViewer *sviewer)
{
  int              ierr,rank,i;
  PetscViewer_Draw *vdraw = (PetscViewer_Draw *)viewer->data,*vsdraw;

  PetscFunctionBegin;
  if (vdraw->singleton_made) {
    SETERRQ(PETSC_ERR_ORDER,"Trying to get singleton without first restoring previous");
  }

  /* only processor zero can use the PetscViewer draw singleton */
  ierr = MPI_Comm_rank(viewer->comm,&rank);CHKERRQ(ierr);
  if (!rank) {
    ierr   = PetscViewerCreate(PETSC_COMM_SELF,sviewer);CHKERRQ(ierr);
    ierr   = PetscViewerSetType(*sviewer,PETSC_VIEWER_DRAW);CHKERRQ(ierr);
    vsdraw = (PetscViewer_Draw *)(*sviewer)->data;
    for (i=0; i<vdraw->draw_max; i++) {
      if (vdraw->draw[i]) {
        ierr = PetscDrawGetSingleton(vdraw->draw[i],&vsdraw->draw[i]);CHKERRQ(ierr);
      }
    }
  }
  vdraw->singleton_made = PETSC_TRUE;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerRestoreSingleton_Draw" 
int PetscViewerRestoreSingleton_Draw(PetscViewer viewer,PetscViewer *sviewer)
{
  int              ierr,rank,i;
  PetscViewer_Draw *vdraw = (PetscViewer_Draw *)viewer->data,*vsdraw;

  PetscFunctionBegin;
  if (!vdraw->singleton_made) {
    SETERRQ(PETSC_ERR_ORDER,"Trying to restore a singleton that was not gotten");
  }
  ierr = MPI_Comm_rank(viewer->comm,&rank);CHKERRQ(ierr);
  if (!rank) {
    vsdraw = (PetscViewer_Draw *)(*sviewer)->data;
    for (i=0; i<vdraw->draw_max; i++) {
      if (vdraw->draw[i] && vsdraw->draw[i]) {
         ierr = PetscDrawRestoreSingleton(vdraw->draw[i],&vsdraw->draw[i]);CHKERRQ(ierr);
      }
    }
    ierr = PetscFree(vsdraw->drawaxis);CHKERRQ(ierr);
    ierr = PetscFree(vsdraw->drawlg);CHKERRQ(ierr);
    ierr = PetscFree(vsdraw->draw);CHKERRQ(ierr);
    ierr = PetscFree((*sviewer)->data);CHKERRQ(ierr);
    PetscLogObjectDestroy((PetscObject)*sviewer);
    PetscHeaderDestroy((PetscObject)*sviewer);
  }
  vdraw->singleton_made = PETSC_FALSE;
  PetscFunctionReturn(0);
}

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "PetscViewerCreate_Draw" 
int PetscViewerCreate_Draw(PetscViewer viewer)
{
  int              i,ierr;
  PetscViewer_Draw *vdraw;

  PetscFunctionBegin;
  ierr         = PetscNew(PetscViewer_Draw,&vdraw);CHKERRQ(ierr);
  viewer->data = (void*)vdraw;

  viewer->ops->flush            = PetscViewerFlush_Draw;
  viewer->ops->destroy          = PetscViewerDestroy_Draw;
  viewer->ops->getsingleton     = PetscViewerGetSingleton_Draw;
  viewer->ops->restoresingleton = PetscViewerRestoreSingleton_Draw;
  viewer->format                = PETSC_VIEWER_NOFORMAT;

  /* these are created on the fly if requested */
  vdraw->draw_max = 5;
  ierr = PetscMalloc(vdraw->draw_max*sizeof(PetscDraw),&vdraw->draw);CHKERRQ(ierr);
  ierr = PetscMemzero(vdraw->draw,vdraw->draw_max*sizeof(PetscDraw));CHKERRQ(ierr);
  ierr = PetscMalloc(vdraw->draw_max*sizeof(PetscDrawLG),&vdraw->drawlg);CHKERRQ(ierr);
  ierr = PetscMemzero(vdraw->drawlg,vdraw->draw_max*sizeof(PetscDrawLG));CHKERRQ(ierr);
  ierr = PetscMalloc(vdraw->draw_max*sizeof(PetscDrawAxis),&vdraw->drawaxis);CHKERRQ(ierr);
  ierr = PetscMemzero(vdraw->drawaxis,vdraw->draw_max*sizeof(PetscDrawAxis));CHKERRQ(ierr);
  for (i=0; i<vdraw->draw_max; i++) {
    vdraw->draw[i]     = 0; 
    vdraw->drawlg[i]   = 0; 
    vdraw->drawaxis[i] = 0;
  }
  vdraw->singleton_made = PETSC_FALSE;
  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerDrawClear" 
/*@
    PetscViewerDrawClear - Clears a PetscDraw graphic associated with a PetscViewer.

    Not Collective

    Input Parameter:
.  viewer - the PetscViewer 

    Level: intermediate

.seealso: PetscViewerDrawOpen(), PetscViewerDrawGetDraw(), 

@*/
int PetscViewerDrawClear(PetscViewer viewer)
{
  int              ierr,i;
  PetscTruth       isdraw;
  PetscViewer_Draw *vdraw;

  PetscFunctionBegin;
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_DRAW,&isdraw);CHKERRQ(ierr);
  if (isdraw) {
    vdraw = (PetscViewer_Draw*)viewer->data;
    for (i=0; i<vdraw->draw_max; i++) {
      if (vdraw->draw[i]) {ierr = PetscDrawClear(vdraw->draw[i]);CHKERRQ(ierr);}
    }
  }
  PetscFunctionReturn(0);
}

/* ---------------------------------------------------------------------*/
/*
    The variable Petsc_Viewer_Draw_keyval is used to indicate an MPI attribute that
  is attached to a communicator, in this case the attribute is a PetscViewer.
*/
static int Petsc_Viewer_Draw_keyval = MPI_KEYVAL_INVALID;

#undef __FUNCT__  
#define __FUNCT__ "PETSC_VIEWER_DRAW_" 
/*@C
    PETSC_VIEWER_DRAW_ - Creates a window PetscViewer shared by all processors 
                     in a communicator.

     Collective on MPI_Comm

     Input Parameter:
.    comm - the MPI communicator to share the window PetscViewer

     Level: intermediate

     Notes:
     Unlike almost all other PETSc routines, PETSC_VIEWER_DRAW_ does not return 
     an error code.  The window is usually used in the form
$       XXXView(XXX object,PETSC_VIEWER_DRAW_(comm));

.seealso: PETSC_VIEWER_DRAW_WORLD, PETSC_VIEWER_DRAW_SELF, PetscViewerDrawOpen(), 
@*/
PetscViewer PETSC_VIEWER_DRAW_(MPI_Comm comm)
{
  int         ierr,flag;
  PetscViewer viewer;

  PetscFunctionBegin;
  if (Petsc_Viewer_Draw_keyval == MPI_KEYVAL_INVALID) {
    ierr = MPI_Keyval_create(MPI_NULL_COPY_FN,MPI_NULL_DELETE_FN,&Petsc_Viewer_Draw_keyval,0);
    if (ierr) {PetscError(__LINE__,"PETSC_VIEWER_DRAW_",__FILE__,__SDIR__,1,1," "); viewer = 0;}
  }
  ierr = MPI_Attr_get(comm,Petsc_Viewer_Draw_keyval,(void **)&viewer,&flag);
  if (ierr) {PetscError(__LINE__,"PETSC_VIEWER_DRAW_",__FILE__,__SDIR__,1,1," "); viewer = 0;}
  if (!flag) { /* PetscViewer not yet created */
    ierr = PetscViewerDrawOpen(comm,0,0,PETSC_DECIDE,PETSC_DECIDE,300,300,&viewer); 
    if (ierr) {PetscError(__LINE__,"PETSC_VIEWER_DRAW_",__FILE__,__SDIR__,1,1," "); viewer = 0;}
    ierr = PetscObjectRegisterDestroy((PetscObject)viewer);
    if (ierr) {PetscError(__LINE__,"PETSC_VIEWER_STDOUT_",__FILE__,__SDIR__,1,1," "); viewer = 0;}
    ierr = MPI_Attr_put(comm,Petsc_Viewer_Draw_keyval,(void*)viewer);
    if (ierr) {PetscError(__LINE__,"PETSC_VIEWER_DRAW_",__FILE__,__SDIR__,1,1," "); viewer = 0;}
  } 
  PetscFunctionReturn(viewer);
}


