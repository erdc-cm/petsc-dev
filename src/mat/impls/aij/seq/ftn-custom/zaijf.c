#include "private/fortranimpl.h"
#include "petscmat.h"

#if defined(PETSC_HAVE_FORTRAN_CAPS)
#define matcreateseqaij_                 MATCREATESEQAIJ
#define matseqaijsetpreallocation_       MATSEQAIJSETPREALLOCATION
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE)
#define matcreateseqaij_                 matcreateseqaij
#define matseqaijsetpreallocation_       matseqaijsetpreallocation
#endif

EXTERN_C_BEGIN
void PETSC_STDCALL matcreateseqaij_(MPI_Comm *comm,PetscInt *m,PetscInt *n,PetscInt *nz,
                           PetscInt *nnz,Mat *newmat,PetscErrorCode *ierr)
{
  CHKFORTRANNULLINTEGER(nnz);
  *ierr = MatCreateSeqAIJ((MPI_Comm)PetscToPointerComm(*comm),*m,*n,*nz,nnz,newmat);
}

void PETSC_STDCALL matseqaijsetpreallocation_(Mat *mat,PetscInt *nz,PetscInt *nnz,PetscErrorCode *ierr)
{
  CHKFORTRANNULLINTEGER(nnz);
  *ierr = MatSeqAIJSetPreallocation(*mat,*nz,nnz);
}

EXTERN_C_END
