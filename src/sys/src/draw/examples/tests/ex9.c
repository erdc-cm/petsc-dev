/*$Id: ex9.c,v 1.15 2001/08/07 21:28:44 bsmith Exp $*/

static char help[] = "Makes a simple histogram.\n";

#include "petsc.h"

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **argv)
{
  PetscDraw       draw;
  PetscDrawHG     hist;
  PetscDrawAxis   axis;
  int             n = 20,i,ierr,x = 0,y = 0,width = 300,height = 300,bins = 8;
  int             color = PETSC_DRAW_GREEN;
  char            *xlabel,*ylabel,*toplabel;
  PetscReal       xd;
  PetscTruth      flg;

  xlabel = "X-axis Label";toplabel = "Top Label";ylabel = "Y-axis Label";

  ierr = PetscInitialize(&argc,&argv,(char*)0,help);CHKERRQ(ierr); 
  ierr = PetscOptionsGetInt(PETSC_NULL,"-width",&width,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-height",&height,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-n",&n,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-bins",&bins,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-color",&color,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsHasName(PETSC_NULL,"-nolabels",&flg);CHKERRQ(ierr);
  if (flg) {
    xlabel = (char *)0; toplabel = (char *)0;
  }
  /* ierr = PetscDrawOpenX(PETSC_COMM_SELF,0,"Title",x,y,width,height,&draw);CHKERRQ(ierr);*/
  ierr = PetscDrawCreate(PETSC_COMM_SELF,0,"Title",x,y,width,height,&draw);CHKERRQ(ierr);
  ierr = PetscDrawSetType(draw,PETSC_DRAW_X);CHKERRQ(ierr);
  ierr = PetscDrawHGCreate(draw,bins,&hist);CHKERRQ(ierr);
  ierr = PetscDrawHGGetAxis(hist,&axis);CHKERRQ(ierr);
  ierr = PetscDrawAxisSetColors(axis,PETSC_DRAW_BLACK,PETSC_DRAW_RED,PETSC_DRAW_BLUE);CHKERRQ(ierr);
  ierr = PetscDrawAxisSetLabels(axis,toplabel,xlabel,ylabel);CHKERRQ(ierr);

  for (i=0; i<n ; i++) {
    xd = (PetscReal)(i - 5);
    ierr = PetscDrawHGAddValue(hist,xd*xd);CHKERRQ(ierr);
  }
  ierr = PetscDrawHGSetColor(hist,color);CHKERRQ(ierr);
  ierr = PetscDrawHGDraw(hist);CHKERRQ(ierr);
  ierr = PetscDrawFlush(draw);CHKERRQ(ierr);

  ierr = PetscDrawHGDestroy(hist);CHKERRQ(ierr);
  ierr = PetscDrawDestroy(draw);CHKERRQ(ierr);
  ierr = PetscFinalize();CHKERRQ(ierr);
  return 0;
}
 
