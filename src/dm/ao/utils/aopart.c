/*$Id: aopart.c,v 1.17 2001/03/23 23:24:57 balay Exp $*/

#include "petscao.h"       /*I  "petscao.h"  I*/

#undef __FUNCT__
#define __FUNCT__ "AODataKeyPartition"
/*@C
    AODataKeyPartition - Partitions a key across the processors to reduce
    communication costs.

    Collective on AOData

    Input Parameters:
+   aodata - the database
-   key - the key you wish partitioned and renumbered

   Level: advanced

.seealso: AODataSegmentPartition()
@*/
int AODataKeyPartition(AOData aodata,char *key)
{
  AO              ao;
  Mat             adj;
  MatPartitioning part;
  IS              is,isg;
  int             ierr;
  MPI_Comm        comm;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(aodata,AODATA_COOKIE);
  ierr = PetscObjectGetComm((PetscObject)aodata,&comm);CHKERRQ(ierr);

  ierr = AODataKeyGetAdjacency(aodata,key,&adj);CHKERRQ(ierr);
  ierr = MatPartitioningCreate(comm,&part);CHKERRQ(ierr);
  ierr = MatPartitioningSetAdjacency(part,adj);CHKERRQ(ierr);
  ierr = MatPartitioningSetFromOptions(part);CHKERRQ(ierr);
  ierr = MatPartitioningApply(part,&is);CHKERRQ(ierr);
  ierr = MatPartitioningDestroy(part);CHKERRQ(ierr);
  ierr = MatDestroy(adj);CHKERRQ(ierr);
  ierr = ISPartitioningToNumbering(is,&isg);CHKERRQ(ierr);
  ierr = ISDestroy(is);CHKERRQ(ierr);

  ierr = AOCreateBasicIS(isg,PETSC_NULL,&ao);CHKERRQ(ierr);
  ierr = ISDestroy(isg);CHKERRQ(ierr);

  ierr = AODataKeyRemap(aodata,key,ao);CHKERRQ(ierr);
  ierr = AODestroy(ao);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
