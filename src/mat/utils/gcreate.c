
#include "src/mat/matimpl.h"       /*I "petscmat.h"  I*/
#include "petscsys.h"

#undef __FUNCT__  
#define __FUNCT__ "MatPublish_Base"
static PetscErrorCode MatPublish_Base(PetscObject obj)
{
  PetscFunctionBegin;
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "MatCreate"
/*@C
   MatCreate - Creates a matrix where the type is determined
   from either a call to MatSetType() or from the options database
   with a call to MatSetFromOptions(). The default matrix type is
   AIJ, using the routines MatCreateSeqAIJ() or MatCreateMPIAIJ()
   if you do not set a type in the options database. If you never
   call MatSetType() or MatSetFromOptions() it will generate an 
   error when you try to use the matrix.

   Collective on MPI_Comm

   Input Parameters:
+  m - number of local rows (or PETSC_DECIDE)
.  n - number of local columns (or PETSC_DECIDE)
.  M - number of global rows (or PETSC_DETERMINE)
.  N - number of global columns (or PETSC_DETERMINE)
-  comm - MPI communicator
 
   Output Parameter:
.  A - the matrix

   Options Database Keys:
+    -mat_type seqaij   - AIJ type, uses MatCreateSeqAIJ()
.    -mat_type mpiaij   - AIJ type, uses MatCreateMPIAIJ()
.    -mat_type seqbdiag - block diagonal type, uses MatCreateSeqBDiag()
.    -mat_type mpibdiag - block diagonal type, uses MatCreateMPIBDiag()
.    -mat_type mpirowbs - rowbs type, uses MatCreateMPIRowbs()
.    -mat_type seqdense - dense type, uses MatCreateSeqDense()
.    -mat_type mpidense - dense type, uses MatCreateMPIDense()
.    -mat_type seqbaij  - block AIJ type, uses MatCreateSeqBAIJ()
-    -mat_type mpibaij  - block AIJ type, uses MatCreateMPIBAIJ()

   Even More Options Database Keys:
   See the manpages for particular formats (e.g., MatCreateSeqAIJ())
   for additional format-specific options.

   Notes:
   If PETSC_DECIDE is not used for the arguments 'm' and 'n', then the
   user must ensure that they are chosen to be compatible with the
   vectors. To do this, one first considers the matrix-vector product 
   'y = A x'. The 'm' that is used in the above routine must match the 
   local size used in the vector creation routine VecCreateMPI() for 'y'.
   Likewise, the 'n' used must match that used as the local size in
   VecCreateMPI() for 'x'.

   Level: beginner

   User manual sections:
+   sec_matcreate
-   chapter_matrices

.keywords: matrix, create

.seealso: MatCreateSeqAIJ((), MatCreateMPIAIJ(), 
          MatCreateSeqBDiag(),MatCreateMPIBDiag(),
          MatCreateSeqDense(), MatCreateMPIDense(), 
          MatCreateMPIRowbs(), MatCreateSeqBAIJ(), MatCreateMPIBAIJ(),
          MatCreateSeqSBAIJ(), MatCreateMPISBAIJ(),
          MatConvert()
@*/
PetscErrorCode MatCreate(MPI_Comm comm,PetscInt m,PetscInt n,PetscInt M,PetscInt N,Mat *A)
{
  Mat B;
#ifndef PETSC_USE_DYNAMIC_LIBRARIES
  PetscErrorCode ierr;
#endif

  PetscFunctionBegin;
  PetscValidPointer(A,6);
  if (M > 0 && m > M) SETERRQ2(PETSC_ERR_ARG_INCOMP,"Local column size %D cannot be larger than global column size %D",m,M);
  if (N > 0 && n > N) SETERRQ2(PETSC_ERR_ARG_INCOMP,"Local row size %D cannot be larger than global row size %D",n,N);

  *A = PETSC_NULL;
#ifndef PETSC_USE_DYNAMIC_LIBRARIES
  ierr = MatInitializePackage(PETSC_NULL);CHKERRQ(ierr);
#endif

  PetscHeaderCreate(B,_p_Mat,struct _MatOps,MAT_COOKIE,0,"Mat",comm,MatDestroy,MatView);
  PetscLogObjectCreate(B);

  B->m = m;
  B->n = n;
  B->M = M;
  B->N = N;

  B->preallocated  = PETSC_FALSE;
  B->bops->publish = MatPublish_Base;
  *A = B;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSetFromOptions"
/*@C
   MatSetFromOptions - Creates a matrix where the type is determined
   from the options database. Generates a parallel MPI matrix if the
   communicator has more than one processor.  The default matrix type is
   AIJ, using the routines MatCreateSeqAIJ() and MatCreateMPIAIJ() if
   you do not select a type in the options database.

   Collective on Mat

   Input Parameter:
.  A - the matrix

   Options Database Keys:
+    -mat_type seqaij   - AIJ type, uses MatCreateSeqAIJ()
.    -mat_type mpiaij   - AIJ type, uses MatCreateMPIAIJ()
.    -mat_type seqbdiag - block diagonal type, uses MatCreateSeqBDiag()
.    -mat_type mpibdiag - block diagonal type, uses MatCreateMPIBDiag()
.    -mat_type mpirowbs - rowbs type, uses MatCreateMPIRowbs()
.    -mat_type seqdense - dense type, uses MatCreateSeqDense()
.    -mat_type mpidense - dense type, uses MatCreateMPIDense()
.    -mat_type seqbaij  - block AIJ type, uses MatCreateSeqBAIJ()
-    -mat_type mpibaij  - block AIJ type, uses MatCreateMPIBAIJ()

   Even More Options Database Keys:
   See the manpages for particular formats (e.g., MatCreateSeqAIJ())
   for additional format-specific options.

   Level: beginner

.keywords: matrix, create

.seealso: MatCreateSeqAIJ((), MatCreateMPIAIJ(), 
          MatCreateSeqBDiag(),MatCreateMPIBDiag(),
          MatCreateSeqDense(), MatCreateMPIDense(), 
          MatCreateMPIRowbs(), MatCreateSeqBAIJ(), MatCreateMPIBAIJ(),
          MatCreateSeqSBAIJ(), MatCreateMPISBAIJ(),
          MatConvert()
@*/
PetscErrorCode MatSetFromOptions(Mat B)
{
  PetscErrorCode ierr;
  PetscMPIInt    size;
  char           mtype[256];
  PetscTruth     flg;

  PetscFunctionBegin;
  ierr = PetscOptionsGetString(B->prefix,"-mat_type",mtype,256,&flg);CHKERRQ(ierr);
  if (flg) {
    ierr = MatSetType(B,mtype);CHKERRQ(ierr);
  }
  if (!B->type_name) {
    ierr = MPI_Comm_size(B->comm,&size);CHKERRQ(ierr);
    ierr = MatSetType(B,MATAIJ);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSetUpPreallocation"
/*@C
   MatSetUpPreallocation

   Collective on Mat

   Input Parameter:
.  A - the matrix

   Level: beginner

.keywords: matrix, create

.seealso: MatCreateSeqAIJ((), MatCreateMPIAIJ(), 
          MatCreateSeqBDiag(),MatCreateMPIBDiag(),
          MatCreateSeqDense(), MatCreateMPIDense(), 
          MatCreateMPIRowbs(), MatCreateSeqBAIJ(), MatCreateMPIBAIJ(),
          MatCreateSeqSBAIJ(), MatCreateMPISBAIJ(),
          MatConvert()
@*/
PetscErrorCode MatSetUpPreallocation(Mat B)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (B->ops->setuppreallocation) {
    PetscLogInfo(B,"MatSetUpPreallocation: Warning not preallocating matrix storage");
    ierr = (*B->ops->setuppreallocation)(B);CHKERRQ(ierr);
    B->ops->setuppreallocation = 0;
    B->preallocated            = PETSC_TRUE;
  }
  PetscFunctionReturn(0);
}

/*
        Copies from Cs header to A
*/
#undef __FUNCT__  
#define __FUNCT__ "MatHeaderCopy"
PetscErrorCode MatHeaderCopy(Mat A,Mat C)
{
  PetscErrorCode ierr;
  int refct;
  PetscOps    *Abops;
  MatOps      Aops;
  char        *mtype,*mname;

  PetscFunctionBegin;
  /* free all the interior data structures from mat */
  ierr = (*A->ops->destroy)(A);CHKERRQ(ierr);

  ierr = PetscMapDestroy(A->rmap);CHKERRQ(ierr);
  ierr = PetscMapDestroy(A->cmap);CHKERRQ(ierr);

  /* save the parts of A we need */
  Abops = A->bops;
  Aops  = A->ops;
  refct = A->refct;
  mtype = A->type_name;
  mname = A->name;

  /* copy C over to A */
  ierr  = PetscMemcpy(A,C,sizeof(struct _p_Mat));CHKERRQ(ierr);

  /* return the parts of A we saved */
  A->bops      = Abops;
  A->ops       = Aops;
  A->qlist     = 0;
  A->refct     = refct;
  A->type_name = mtype;
  A->name      = mname;

  PetscLogObjectDestroy(C);
  PetscHeaderDestroy(C);
  PetscFunctionReturn(0);
}
