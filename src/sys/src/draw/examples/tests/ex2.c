/*$Id: ex2.c,v 1.35 2001/08/07 21:28:44 bsmith Exp $*/

static char help[] = "Demonstrates us of color map\n";

#include "petsc.h"

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **argv)
{
  PetscDraw draw;
  int     ierr,x = 0,y = 0,width = 256,height = 256,i; 

  ierr = PetscInitialize(&argc,&argv,(char*)0,help);CHKERRQ(ierr); 

  /* ierr = PetscDrawOpenX(PETSC_COMM_SELF,0,"Title",x,y,width,height,&draw);CHKERRQ(ierr);*/
  ierr = PetscDrawCreate(PETSC_COMM_SELF,0,"Title",x,y,width,height,&draw);CHKERRQ(ierr);
  ierr = PetscDrawSetFromOptions(draw);CHKERRQ(ierr);
  for (i=0; i<256; i++) {
    ierr = PetscDrawLine(draw,0.0,((PetscReal)i)/256.,1.0,((PetscReal)i)/256.,i);
  }
  ierr = PetscDrawFlush(draw);CHKERRQ(ierr);
  ierr = PetscSleep(2);CHKERRQ(ierr);
  ierr = PetscDrawDestroy(draw);CHKERRQ(ierr);
  ierr = PetscFinalize();CHKERRQ(ierr);
  return 0;
}
 
