/*$Id: mhas.c,v 1.24 2001/03/23 23:21:44 balay Exp $*/


#include "petsc.h"
#include "src/mat/matimpl.h"        /*I "petscmat.h" I*/
       
#undef __FUNCT__  
#define __FUNCT__ "MatHasOperation"
/*@
    MatHasOperation - Determines whether the given matrix supports the particular
    operation.

   Collective on Mat

   Input Parameters:
+  mat - the matrix
-  op - the operation, for example, MATOP_GET_DIAGONAL

   Output Parameter:
.  has - either PETSC_TRUE or PETSC_FALSE

   Level: advanced

   Notes:
   See the file include/petscmat.h for a complete list of matrix
   operations, which all have the form MATOP_<OPERATION>, where
   <OPERATION> is the name (in all capital letters) of the
   user-level routine.  E.g., MatNorm() -> MATOP_NORM.

.keywords: matrix, has, operation

.seealso: MatCreateShell()
@*/
int MatHasOperation(Mat mat,MatOperation op,PetscTruth *has)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(mat,MAT_COOKIE);
  PetscValidType(mat);
  MatPreallocated(mat);
  if (((void **)mat->ops)[op]) {*has =  PETSC_TRUE;}
  else {*has = PETSC_FALSE;}
  PetscFunctionReturn(0);
}
