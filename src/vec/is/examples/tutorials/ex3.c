
/*      "$Id: ex3.c,v 1.21 2001/03/23 23:21:14 balay Exp $"; */

static char help[] = "Demonstrates creating a blocked index set.\n\n";

/*T
    Concepts: index sets^creating a block index set;
    Concepts: IS^creating a block index set;

    Comment:  Creates an index set based on blocks of integers. Views that index set
    and then destroys it.
T*/

#include "petscis.h"

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **argv)
{
  int        i,n = 4,ierr, inputindices[] = {0,3,9,12},bs = 3,issize,*indices;
  IS         set;
  PetscTruth isblock;

  ierr = PetscInitialize(&argc,&argv,(char*)0,help);CHKERRQ(ierr);
      
  /*
    Create a block index set. The index set has 4 blocks each of size 3.
    The indices are {0,1,2,3,4,5,9,10,11,12,13,14}
    Note each processor is generating its own index set 
    (in this case they are all identical)
  */
  ierr = ISCreateBlock(PETSC_COMM_SELF,bs,n,inputindices,&set);CHKERRQ(ierr);
  ierr = ISView(set,PETSC_VIEWER_STDOUT_SELF);CHKERRQ(ierr);

  /*
    Extract indices from set.
  */
  ierr = ISGetLocalSize(set,&issize);CHKERRQ(ierr);
  ierr = ISGetIndices(set,&indices);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_SELF,"Printing indices directly\n");CHKERRQ(ierr);
  for (i=0; i<issize; i++) {
    ierr = PetscPrintf(PETSC_COMM_SELF,"%d\n",indices[i]);CHKERRQ(ierr);
  }
  ierr = ISRestoreIndices(set,&indices);CHKERRQ(ierr);

  /*
    Extract the block indices. This returns one index per block.
  */
  ierr = ISBlockGetIndices(set,&indices);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_SELF,"Printing block indices directly\n");CHKERRQ(ierr);
  for (i=0; i<n; i++) {
    ierr = PetscPrintf(PETSC_COMM_SELF,"%d\n",indices[i]);CHKERRQ(ierr);
  }
  ierr = ISBlockRestoreIndices(set,&indices);CHKERRQ(ierr);

  /*
    Check if this is really a block index set
  */
  ierr = ISBlock(set,&isblock);CHKERRQ(ierr);
  if (isblock != PETSC_TRUE) SETERRQ(1,"Index set is not blocked!");

  /*
    Determine the block size of the index set
  */
  ierr = ISBlockGetBlockSize(set,&bs);CHKERRQ(ierr);
  if (bs != 3) SETERRQ(1,"Block size is not 3!");

  /*
    Get the number of blocks
  */
  ierr = ISBlockGetSize(set,&n);CHKERRQ(ierr);
  if (n != 4) SETERRQ(1,"Number of blocks not 4!");

  ierr = ISDestroy(set);CHKERRQ(ierr);
  ierr = PetscFinalize();CHKERRQ(ierr);
  return 0;
}


