
#include "petsc.h"  /*I     "petsc.h"   I*/

/* ---------------------------------------------------------------------*/
/*
    The variable Petsc_Viewer_Stdout_keyval is used to indicate an MPI attribute that
  is attached to a communicator, in this case the attribute is a PetscViewer.
*/
static int Petsc_Viewer_Stdout_keyval = MPI_KEYVAL_INVALID;


#undef __FUNCT__  
#define __FUNCT__ "PETSC_VIEWER_STDOUT_"  
/*@C
   PETSC_VIEWER_STDOUT_ - Creates a ASCII PetscViewer shared by all processors 
                    in a communicator.

   Collective on MPI_Comm

   Input Parameter:
.  comm - the MPI communicator to share the PetscViewer

   Level: beginner

   Notes: 
   Unlike almost all other PETSc routines, this does not return 
   an error code. Usually used in the form
$      XXXView(XXX object,PETSC_VIEWER_STDOUT_(comm));

.seealso: PETSC_VIEWER_DRAW_(), PetscViewerASCIIOpen(), PETSC_VIEWER_STDERR_, PETSC_VIEWER_STDOUT_WORLD,
          PETSC_VIEWER_STDOUT_SELF

@*/
PetscViewer PETSC_VIEWER_STDOUT_(MPI_Comm comm)
{
  int         ierr;
  PetscTruth  flg;
  PetscViewer viewer;

  PetscFunctionBegin;
  if (Petsc_Viewer_Stdout_keyval == MPI_KEYVAL_INVALID) {
    ierr = MPI_Keyval_create(MPI_NULL_COPY_FN,MPI_NULL_DELETE_FN,&Petsc_Viewer_Stdout_keyval,0);
    if (ierr) {PetscError(__LINE__,"PETSC_VIEWER_STDOUT_",__FILE__,__SDIR__,1,1," "); PetscFunctionReturn(0);}
  }
  ierr = MPI_Attr_get(comm,Petsc_Viewer_Stdout_keyval,(void **)&viewer,(int*)&flg);
  if (ierr) {PetscError(__LINE__,"PETSC_VIEWER_STDOUT_",__FILE__,__SDIR__,1,1," "); PetscFunctionReturn(0);}
  if (!flg) { /* PetscViewer not yet created */
    ierr = PetscViewerASCIIOpen(comm,"stdout",&viewer);
    if (ierr) {PetscError(__LINE__,"PETSC_VIEWER_STDOUT_",__FILE__,__SDIR__,1,1," "); PetscFunctionReturn(0);}
    ierr = PetscObjectRegisterDestroy((PetscObject)viewer);
    if (ierr) {PetscError(__LINE__,"PETSC_VIEWER_STDOUT_",__FILE__,__SDIR__,1,1," "); PetscFunctionReturn(0);}
    ierr = MPI_Attr_put(comm,Petsc_Viewer_Stdout_keyval,(void*)viewer);
    if (ierr) {PetscError(__LINE__,"PETSC_VIEWER_STDOUT_",__FILE__,__SDIR__,1,1," "); PetscFunctionReturn(0);}
  } 
  PetscFunctionReturn(viewer);
}

/* ---------------------------------------------------------------------*/
/*
    The variable Petsc_Viewer_Stderr_keyval is used to indicate an MPI attribute that
  is attached to a communicator, in this case the attribute is a PetscViewer.
*/
static int Petsc_Viewer_Stderr_keyval = MPI_KEYVAL_INVALID;

#undef __FUNCT__  
#define __FUNCT__ "PETSC_VIEWER_STDERR_" 
/*@C
   PETSC_VIEWER_STDERR_ - Creates a ASCII PetscViewer shared by all processors 
                    in a communicator.

   Collective on MPI_Comm

   Input Parameter:
.  comm - the MPI communicator to share the PetscViewer

   Level: beginner

   Note: 
   Unlike almost all other PETSc routines, this does not return 
   an error code. Usually used in the form
$      XXXView(XXX object,PETSC_VIEWER_STDERR_(comm));

.seealso: PETSC_VIEWER_DRAW_, PetscViewerASCIIOpen(), PETSC_VIEWER_STDOUT_, PETSC_VIEWER_STDOUT_WORLD,
          PETSC_VIEWER_STDOUT_SELF, PETSC_VIEWER_STDERR_WORLD, PETSC_VIEWER_STDERR_SELF
@*/
PetscViewer PETSC_VIEWER_STDERR_(MPI_Comm comm)
{
  int         ierr;
  PetscTruth  flg;
  PetscViewer viewer;

  PetscFunctionBegin;
  if (Petsc_Viewer_Stderr_keyval == MPI_KEYVAL_INVALID) {
    ierr = MPI_Keyval_create(MPI_NULL_COPY_FN,MPI_NULL_DELETE_FN,&Petsc_Viewer_Stderr_keyval,0);
    if (ierr) {PetscError(__LINE__,"PETSC_VIEWER_STDERR_",__FILE__,__SDIR__,1,1," "); PetscFunctionReturn(0);}
  }
  ierr = MPI_Attr_get(comm,Petsc_Viewer_Stderr_keyval,(void **)&viewer,(int*)&flg);
  if (ierr) {PetscError(__LINE__,"PETSC_VIEWER_STDERR_",__FILE__,__SDIR__,1,1," "); PetscFunctionReturn(0);}
  if (!flg) { /* PetscViewer not yet created */
    ierr = PetscViewerASCIIOpen(comm,"stderr",&viewer);
    if (ierr) {PetscError(__LINE__,"PETSC_VIEWER_STDERR_",__FILE__,__SDIR__,1,1," "); PetscFunctionReturn(0);}
    ierr = PetscObjectRegisterDestroy((PetscObject)viewer);
    if (ierr) {PetscError(__LINE__,"PETSC_VIEWER_STDOUT_",__FILE__,__SDIR__,1,1," "); PetscFunctionReturn(0);}
    ierr = MPI_Attr_put(comm,Petsc_Viewer_Stderr_keyval,(void*)viewer);
    if (ierr) {PetscError(__LINE__,"PETSC_VIEWER_STDERR_",__FILE__,__SDIR__,1,1," "); PetscFunctionReturn(0);}
  } 
  PetscFunctionReturn(viewer);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerASCIIOpen" 
/*@C
   PetscViewerASCIIOpen - Opens an ASCII file as a PetscViewer.

   Collective on MPI_Comm

   Input Parameters:
+  comm - the communicator
-  name - the file name

   Output Parameter:
.  lab - the PetscViewer to use with the specified file

   Level: beginner

   Notes:
   This PetscViewer can be destroyed with PetscViewerDestroy().

   If a multiprocessor communicator is used (such as PETSC_COMM_WORLD), 
   then only the first processor in the group opens the file.  All other 
   processors send their data to the first processor to print. 

   Each processor can instead write its own independent output by
   specifying the communicator PETSC_COMM_SELF.

   As shown below, PetscViewerASCIIOpen() is useful in conjunction with 
   MatView() and VecView()
.vb
     PetscViewerASCIIOpen(PETSC_COMM_WORLD,"mat.output",&viewer);
     MatView(matrix,viewer);
.ve

  Concepts: PetscViewerASCII^creating
  Concepts: printf
  Concepts: printing
  Concepts: accessing remote file
  Concepts: remote file

.seealso: MatView(), VecView(), PetscViewerDestroy(), PetscViewerBinaryOpen(),
          PetscViewerASCIIGetPointer(), PetscViewerSetFormat(), PETSC_VIEWER_STDOUT_, PETSC_VIEWER_STDERR_,
          PETSC_VIEWER_STDOUT_WORLD, PETSC_VIEWER_STDOUT_SELF, 
@*/
int PetscViewerASCIIOpen(MPI_Comm comm,const char name[],PetscViewer *lab)
{
  int ierr;

  PetscFunctionBegin;
  ierr = PetscViewerCreate(comm,lab);CHKERRQ(ierr);
  ierr = PetscViewerSetType(*lab,PETSC_VIEWER_ASCII);CHKERRQ(ierr);
  if (name) {
    ierr = PetscViewerSetFilename(*lab,name);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}


