/*$Id: dlinegw.c,v 1.34 2001/03/23 23:20:08 balay Exp $*/
/*
       Provides the calling sequences for all the basic PetscDraw routines.
*/
#include "src/sys/src/draw/drawimpl.h"  /*I "petscdraw.h" I*/

#undef __FUNCT__  
#define __FUNCT__ "PetscDrawLineGetWidth" 
/*@
   PetscDrawLineGetWidth - Gets the line width for future draws.  The width is
   relative to the user coordinates of the window; 0.0 denotes the natural
   width; 1.0 denotes the interior viewport. 

   Not collective

   Input Parameter:
.  draw - the drawing context

   Output Parameter:
.  width - the width in user coordinates

   Level: advanced

   Notes:
   Not currently implemented.

   Concepts: line^width

.seealso:  PetscDrawLineSetWidth()
@*/
int PetscDrawLineGetWidth(PetscDraw draw,PetscReal *width)
{
  int        ierr;
  PetscTruth isdrawnull;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(draw,PETSC_DRAW_COOKIE);
  ierr = PetscTypeCompare((PetscObject)draw,PETSC_DRAW_NULL,&isdrawnull);CHKERRQ(ierr);
  if (isdrawnull) PetscFunctionReturn(0);
  if (!draw->ops->linegetwidth) SETERRQ(PETSC_ERR_SUP,0);
  ierr = (*draw->ops->linegetwidth)(draw,width);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

