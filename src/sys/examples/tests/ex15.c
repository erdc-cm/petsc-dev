/*$Id: ex15.c,v 1.4 2001/03/23 23:20:59 balay Exp $*/

/* 
   Demonstrates PetscPopUpSelect()
 */

#include "petsc.h"


#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **argv)
{
  int  ierr,choice;
  char *choices[] = {"Say hello","Say goodbye"};

  PetscInitialize(&argc,&argv,(char *)0,0);
  ierr = PetscPopUpSelect(PETSC_COMM_WORLD,PETSC_NULL,"Select one of ",2,choices,&choice);CHKERRQ(ierr);
  ierr = PetscSynchronizedPrintf(PETSC_COMM_WORLD,"You selected %s\n",choices[choice]);CHKERRQ(ierr);
  ierr = PetscSynchronizedFlush(PETSC_COMM_WORLD);CHKERRQ(ierr);
  ierr = PetscFinalize();CHKERRQ(ierr);
  return 0;
}
 
