/*$Id: sorder.c,v 1.90 2001/06/21 21:17:30 bsmith Exp $*/
/*
     Provides the code that allows PETSc users to register their own
  sequential matrix Ordering routines.
*/
#include "src/mat/matimpl.h"
#include "petscsys.h"

PetscFList      MatOrderingList = 0;
PetscTruth MatOrderingRegisterAllCalled = PETSC_FALSE;

EXTERN int MatOrdering_Flow_SeqAIJ(Mat,MatOrderingType,IS *,IS *);

#undef __FUNCT__  
#define __FUNCT__ "MatOrdering_Flow"
int MatOrdering_Flow(Mat mat,MatOrderingType type,IS *irow,IS *icol)
{
  PetscFunctionBegin;
  SETERRQ(PETSC_ERR_SUP,"Cannot do default flow ordering for matrix type");
#if !defined(PETSC_USE_DEBUG)
  PetscFunctionReturn(0);
#endif
}

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "MatOrdering_Natural"
int MatOrdering_Natural(Mat mat,MatOrderingType type,IS *irow,IS *icol)
{
  int        n,ierr,i,*ii;
  PetscTruth done;
  MPI_Comm   comm;

  PetscFunctionBegin;
  ierr = PetscObjectGetComm((PetscObject)mat,&comm);CHKERRQ(ierr);
  ierr = MatGetRowIJ(mat,0,PETSC_FALSE,&n,PETSC_NULL,PETSC_NULL,&done);CHKERRQ(ierr);
  ierr = MatRestoreRowIJ(mat,0,PETSC_FALSE,&n,PETSC_NULL,PETSC_NULL,&done);CHKERRQ(ierr);
  if (done) { /* matrix may be "compressed" in symbolic factorization, due to i-nodes or block storage */
    /*
      We actually create general index sets because this avoids mallocs to
      to obtain the indices in the MatSolve() routines.
      ierr = ISCreateStride(PETSC_COMM_SELF,n,0,1,irow);CHKERRQ(ierr);
      ierr = ISCreateStride(PETSC_COMM_SELF,n,0,1,icol);CHKERRQ(ierr);
    */
    ierr = PetscMalloc(n*sizeof(int),&ii);CHKERRQ(ierr);
    for (i=0; i<n; i++) ii[i] = i;
    ierr = ISCreateGeneral(PETSC_COMM_SELF,n,ii,irow);CHKERRQ(ierr);
    ierr = ISCreateGeneral(PETSC_COMM_SELF,n,ii,icol);CHKERRQ(ierr);
    ierr = PetscFree(ii);CHKERRQ(ierr);
  } else {
    int start,end;

    ierr = MatGetOwnershipRange(mat,&start,&end);CHKERRQ(ierr);
    ierr = ISCreateStride(comm,end-start,start,1,irow);CHKERRQ(ierr);
    ierr = ISCreateStride(comm,end-start,start,1,icol);CHKERRQ(ierr);
  }
  ierr = ISSetIdentity(*irow);CHKERRQ(ierr);
  ierr = ISSetIdentity(*icol);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END

EXTERN_C_BEGIN
/*
     Orders the rows (and columns) by the lengths of the rows. 
   This produces a symmetric Ordering but does not require a 
   matrix with symmetric non-zero structure.
*/
#undef __FUNCT__  
#define __FUNCT__ "MatOrdering_RowLength"
int MatOrdering_RowLength(Mat mat,MatOrderingType type,IS *irow,IS *icol)
{
  int        ierr,n,*ia,*ja,*permr,*lens,i;
  PetscTruth done;

  PetscFunctionBegin;
  ierr = MatGetRowIJ(mat,0,PETSC_FALSE,&n,&ia,&ja,&done);CHKERRQ(ierr);
  if (!done) SETERRQ(PETSC_ERR_SUP,"Cannot get rows for matrix");

  ierr  = PetscMalloc(2*n*sizeof(int),&lens);CHKERRQ(ierr);
  permr = lens + n;
  for (i=0; i<n; i++) { 
    lens[i]  = ia[i+1] - ia[i];
    permr[i] = i;
  }
  ierr = MatRestoreRowIJ(mat,0,PETSC_FALSE,&n,&ia,&ja,&done);CHKERRQ(ierr);

  ierr = PetscSortIntWithPermutation(n,lens,permr);CHKERRQ(ierr);

  ierr = ISCreateGeneral(PETSC_COMM_SELF,n,permr,irow);CHKERRQ(ierr);
  ierr = ISCreateGeneral(PETSC_COMM_SELF,n,permr,icol);CHKERRQ(ierr);
  ierr = PetscFree(lens);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "MatOrderingRegister" 
int MatOrderingRegister(const char sname[],const char path[],const char name[],int (*function)(Mat,MatOrderingType,IS*,IS*))
{
  int  ierr;
  char fullname[256];

  PetscFunctionBegin;
  ierr = PetscFListConcat(path,name,fullname);CHKERRQ(ierr);
  ierr = PetscFListAdd(&MatOrderingList,sname,fullname,(void (*)(void))function);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatOrderingRegisterDestroy"
/*@C
   MatOrderingRegisterDestroy - Frees the list of ordering routines.

   Not collective

   Level: developer
   
.keywords: matrix, register, destroy

.seealso: MatOrderingRegisterDynamic(), MatOrderingRegisterAll()
@*/
int MatOrderingRegisterDestroy(void)
{
  int ierr;

  PetscFunctionBegin;
  if (MatOrderingList) {
    ierr = PetscFListDestroy(&MatOrderingList);CHKERRQ(ierr);
    MatOrderingList = 0;
  }
  PetscFunctionReturn(0);
}

EXTERN int MatAdjustForInodes(Mat,IS *,IS *);

#include "src/mat/impls/aij/mpi/mpiaij.h"
#undef __FUNCT__  
#define __FUNCT__ "MatGetOrdering"
/*@C
   MatGetOrdering - Gets a reordering for a matrix to reduce fill or to
   improve numerical stability of LU factorization.

   Collective on Mat

   Input Parameters:
+  mat - the matrix
-  type - type of reordering, one of the following:
$      MATORDERING_NATURAL - Natural
$      MATORDERING_ND - Nested Dissection
$      MATORDERING_1WD - One-way Dissection
$      MATORDERING_RCM - Reverse Cuthill-McKee
$      MATORDERING_QMD - Quotient Minimum Degree

   Output Parameters:
+  rperm - row permutation indices
-  cperm - column permutation indices


   Options Database Key:
. -mat_view_ordering_draw - plots matrix nonzero structure in new ordering

   Level: intermediate
   
   Notes:
      This DOES NOT actually reorder the matrix; it merely returns two index sets
   that define a reordering. This is usually not used directly, rather use the 
   options PCLUSetMatOrdering() or PCILUSetMatOrdering().

   The user can define additional orderings; see MatOrderingRegisterDynamic().

.keywords: matrix, set, ordering, factorization, direct, ILU, LU,
           fill, reordering, natural, Nested Dissection,
           One-way Dissection, Cholesky, Reverse Cuthill-McKee, 
           Quotient Minimum Degree

.seealso:   MatOrderingRegisterDynamic(), PCLUSetMatOrdering(), PCILUSetMatOrdering()
@*/
int MatGetOrdering(Mat mat,const MatOrderingType type,IS *rperm,IS *cperm)
{
  int         ierr,mmat,nmat,mis,m;
  int         (*r)(Mat,MatOrderingType,IS*,IS*);
  PetscTruth  flg,isseqdense,ismpidense;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(mat,MAT_COOKIE);
  if (!mat->assembled) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Not for unassembled matrix");
  if (mat->factor) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Not for factored matrix"); 

  ierr = PetscTypeCompare((PetscObject)mat,MATSEQDENSE,&isseqdense);CHKERRQ(ierr);
  ierr = PetscTypeCompare((PetscObject)mat,MATMPIDENSE,&ismpidense);CHKERRQ(ierr);
  if (isseqdense || ismpidense) {
    ierr = MatGetLocalSize(mat,&m,PETSC_NULL);CHKERRQ(ierr);
    /*
       Dense matrices only give natural ordering
    */
    ierr = ISCreateStride(PETSC_COMM_SELF,0,m,1,cperm);CHKERRQ(ierr);
    ierr = ISCreateStride(PETSC_COMM_SELF,0,m,1,rperm);CHKERRQ(ierr);
    ierr = ISSetIdentity(*cperm);CHKERRQ(ierr);
    ierr = ISSetIdentity(*rperm);CHKERRQ(ierr);
    ierr = ISSetPermutation(*rperm);CHKERRQ(ierr);
    ierr = ISSetPermutation(*cperm);CHKERRQ(ierr);
    PetscFunctionReturn(0);
  }

  if (!mat->M) { /* matrix has zero rows */
    ierr = ISCreateStride(PETSC_COMM_SELF,0,0,1,cperm);CHKERRQ(ierr);
    ierr = ISCreateStride(PETSC_COMM_SELF,0,0,1,rperm);CHKERRQ(ierr);
    ierr = ISSetIdentity(*cperm);CHKERRQ(ierr);
    ierr = ISSetIdentity(*rperm);CHKERRQ(ierr);
    ierr = ISSetPermutation(*rperm);CHKERRQ(ierr);
    ierr = ISSetPermutation(*cperm);CHKERRQ(ierr);
    PetscFunctionReturn(0);
  }

  if (!MatOrderingRegisterAllCalled) {
    ierr = MatOrderingRegisterAll(PETSC_NULL);CHKERRQ(ierr);
  }

  ierr = PetscLogEventBegin(MAT_GetOrdering,mat,0,0,0);CHKERRQ(ierr);
  ierr =  PetscFListFind(mat->comm,MatOrderingList,type,(void (**)(void)) &r);CHKERRQ(ierr);
  if (!r) {SETERRQ1(PETSC_ERR_ARG_OUTOFRANGE,"Unknown or unregistered type: %s",type);}

  ierr = (*r)(mat,type,rperm,cperm);CHKERRQ(ierr);
  ierr = ISSetPermutation(*rperm);CHKERRQ(ierr);
  ierr = ISSetPermutation(*cperm);CHKERRQ(ierr);

  /*
      Adjust for inode (reduced matrix ordering) only if row permutation
    is smaller then matrix size
  */
  ierr = MatGetLocalSize(mat,&mmat,&nmat);CHKERRQ(ierr);
  ierr = ISGetLocalSize(*rperm,&mis);CHKERRQ(ierr);
  if (mmat > mis) {  
    ierr = MatAdjustForInodes(mat,rperm,cperm);CHKERRQ(ierr);
  }

  ierr = PetscLogEventEnd(MAT_GetOrdering,mat,0,0,0);CHKERRQ(ierr);

  ierr = PetscOptionsHasName(PETSC_NULL,"-mat_view_ordering_draw",&flg);CHKERRQ(ierr);
  if (flg) {
    Mat tmat;
    ierr = PetscOptionsHasName(PETSC_NULL,"-mat_view_contour",&flg);CHKERRQ(ierr);
    if (flg) {
      ierr = PetscViewerPushFormat(PETSC_VIEWER_DRAW_(mat->comm),PETSC_VIEWER_DRAW_CONTOUR);CHKERRQ(ierr);
    }
    ierr = MatPermute(mat,*rperm,*cperm,&tmat);CHKERRQ(ierr);
    ierr = MatView(tmat,PETSC_VIEWER_DRAW_(mat->comm));CHKERRQ(ierr);
    if (flg) {
      ierr = PetscViewerPopFormat(PETSC_VIEWER_DRAW_(mat->comm));CHKERRQ(ierr);
    }
    ierr = MatDestroy(tmat);CHKERRQ(ierr);
  }

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatGetOrderingList"
int MatGetOrderingList(PetscFList *list)
{
  PetscFunctionBegin;
  *list = MatOrderingList;
  PetscFunctionReturn(0);
}
