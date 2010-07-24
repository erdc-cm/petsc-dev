#include <petscsnes.h>
#include <petscda.h>

int main(int argc, char *argv[]) {
  DA              da, daX, daY;
  DALocalInfo     info;
  MPI_Comm        commX, commY;
  Vec             basisX, basisY;
  PetscScalar   **arrayX, **arrayY;
  const PetscInt *lx, *ly;
  PetscInt        M = 3, N = 3;
  PetscInt        p = 1;
  PetscInt        numGP = 3;
  PetscInt        dof = 2*(p+1)*numGP;
  PetscMPIInt     rank, subsize, subrank;
  PetscErrorCode  ierr;

  ierr = PetscInitialize(&argc,&argv,0,0);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(PETSC_COMM_WORLD, &rank);CHKERRQ(ierr);
  /* Create 2D DA */
  ierr = DACreate2d(PETSC_COMM_WORLD, DA_NONPERIODIC, DA_STENCIL_STAR, M, N, PETSC_DECIDE, PETSC_DECIDE, 1, 1, PETSC_NULL, PETSC_NULL, &da);CHKERRQ(ierr);
  /* Create 1D DAs along two directions */
  ierr = DAGetOwnershipRanges(da, &lx, &ly, PETSC_NULL);CHKERRQ(ierr);
  ierr = DAGetLocalInfo(da, &info);CHKERRQ(ierr);
  ierr = DAGetProcessorSubsets(da, DA_X, &commX);CHKERRQ(ierr);
  ierr = DAGetProcessorSubsets(da, DA_Y, &commY);CHKERRQ(ierr);
  ierr = MPI_Comm_size(commX, &subsize);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(commX, &subrank);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_SELF, "[%d]X subrank: %d subsize: %d\n", rank, subrank, subsize);
  ierr = MPI_Comm_size(commY, &subsize);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(commY, &subrank);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_SELF, "[%d]Y subrank: %d subsize: %d\n", rank, subrank, subsize);
  ierr = DACreate1d(commX, DA_NONPERIODIC, M, dof, 1, lx, &daX);CHKERRQ(ierr);
  ierr = DACreate1d(commY, DA_NONPERIODIC, N, dof, 1, ly, &daY);CHKERRQ(ierr);
  /* Create 1D vectors for basis functions */
  ierr = DAGetGlobalVector(daX, &basisX);CHKERRQ(ierr);
  ierr = DAGetGlobalVector(daY, &basisY);CHKERRQ(ierr);
  /* Extract basis functions */
  ierr = DAVecGetArrayDOF(daX, basisX, &arrayX);CHKERRQ(ierr);
  ierr = DAVecGetArrayDOF(daY, basisY, &arrayY);CHKERRQ(ierr);
  //arrayX[i][ndof];
  //arrayY[j][ndof];
  ierr = DAVecRestoreArrayDOF(daX, basisX, &arrayX);CHKERRQ(ierr);
  ierr = DAVecRestoreArrayDOF(daY, basisY, &arrayY);CHKERRQ(ierr);
  /* Return basis vectors */
  ierr = DARestoreGlobalVector(daX, &basisX);CHKERRQ(ierr);
  ierr = DARestoreGlobalVector(daY, &basisY);CHKERRQ(ierr);
  /* Cleanup */
  ierr = DADestroy(daX);CHKERRQ(ierr);
  ierr = DADestroy(daY);CHKERRQ(ierr);
  ierr = DADestroy(da);CHKERRQ(ierr);
  ierr = PetscFinalize();CHKERRQ(ierr);
}
