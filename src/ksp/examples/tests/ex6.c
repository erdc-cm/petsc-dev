
static char help[] = "Reads a PETSc matrix and vector from a file and solves a linear system.\n\
Input arguments are:\n\
  -f <input_file> : file to load.  For a 5X5 example of the 5-pt. stencil,\n\
                    use the file petsc/src/mat/examples/matbinary.ex\n\n";

#include "petscksp.h"
#include "petsclog.h"

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **args)
{
#if !defined(PETSC_USE_COMPLEX)
  int            ierr,its,stage1,stage2;
  PetscReal      norm;
  PetscLogDouble tsetup1,tsetup2,tsetup,tsolve1,tsolve2,tsolve;
  PetscScalar    zero = 0.0,none = -1.0;
  Vec            x,b,u;
  Mat            A;
  char           file[PETSC_MAX_PATH_LEN];
  PetscViewer    fd;
  PetscTruth     table,flg;
  KSP            ksp;
#endif

  PetscInitialize(&argc,&args,(char *)0,help);

#if defined(PETSC_USE_COMPLEX)
  SETERRQ(1,"This example does not work with complex numbers");
#else
  ierr = PetscOptionsHasName(PETSC_NULL,"-table",&table);CHKERRQ(ierr);


  /* Read matrix and RHS */
  ierr = PetscOptionsGetString(PETSC_NULL,"-f",file,PETSC_MAX_PATH_LEN-1,&flg);CHKERRQ(ierr);
  if (!flg) SETERRQ(1,"Must indicate binary file with the -f option");
  ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,file,PETSC_FILE_RDONLY,&fd);CHKERRQ(ierr);

  ierr = MatLoad(fd,MATSEQAIJ,&A);CHKERRQ(ierr);
  ierr = VecLoad(fd,PETSC_NULL,&b);CHKERRQ(ierr);
  ierr = PetscViewerDestroy(fd);CHKERRQ(ierr);

  /* 
   If the load matrix is larger then the vector, due to being padded 
   to match the blocksize then create a new padded vector
  */
  { 
    int    m,n,j,mvec,start,end,indx;
    Vec    tmp;
    PetscScalar *bold;

    ierr = MatGetLocalSize(A,&m,&n);CHKERRQ(ierr);
    ierr = VecCreate(PETSC_COMM_WORLD,&tmp);CHKERRQ(ierr);
    ierr = VecSetSizes(tmp,m,PETSC_DECIDE);CHKERRQ(ierr);
    ierr = VecSetFromOptions(tmp);CHKERRQ(ierr);
    ierr = VecGetOwnershipRange(b,&start,&end);CHKERRQ(ierr);
    ierr = VecGetLocalSize(b,&mvec);CHKERRQ(ierr);
    ierr = VecGetArray(b,&bold);CHKERRQ(ierr);
    for (j=0; j<mvec; j++) {
      indx = start+j;
      ierr  = VecSetValues(tmp,1,&indx,bold+j,INSERT_VALUES);CHKERRQ(ierr);
    }
    ierr = VecRestoreArray(b,&bold);CHKERRQ(ierr);
    ierr = VecDestroy(b);CHKERRQ(ierr);
    ierr = VecAssemblyBegin(tmp);CHKERRQ(ierr);
    ierr = VecAssemblyEnd(tmp);CHKERRQ(ierr);
    b = tmp;
  }
  ierr = VecDuplicate(b,&x);CHKERRQ(ierr);
  ierr = VecDuplicate(b,&u);CHKERRQ(ierr);

  ierr = VecSet(&zero,x);CHKERRQ(ierr);
  ierr = PetscBarrier((PetscObject)A);CHKERRQ(ierr);

  PetscLogStageRegister(&stage1,"mystage 1");
  PetscLogStagePush(stage1);
  ierr = PetscGetTime(&tsetup1);CHKERRQ(ierr);
  ierr = KSPCreate(PETSC_COMM_WORLD,&ksp);CHKERRQ(ierr);
  ierr = KSPSetOperators(ksp,A,A,DIFFERENT_NONZERO_PATTERN);CHKERRQ(ierr);
  ierr = KSPSetFromOptions(ksp);CHKERRQ(ierr);
  ierr = KSPSetUp(ksp);CHKERRQ(ierr);
  ierr = KSPSetUpOnBlocks(ksp);CHKERRQ(ierr);
  ierr = PetscGetTime(&tsetup2);CHKERRQ(ierr);
  tsetup = tsetup2 -tsetup1;
  PetscLogStagePop();
  ierr = PetscBarrier((PetscObject)A);CHKERRQ(ierr);

  PetscLogStageRegister(&stage2,"mystage 2");
  PetscLogStagePush(stage2);
  ierr = PetscGetTime(&tsolve1);CHKERRQ(ierr);
  ierr = KSPSolve(ksp,b,x);CHKERRQ(ierr);
  ierr = PetscGetTime(&tsolve2);CHKERRQ(ierr);
  tsolve = tsolve2 - tsolve1;
  PetscLogStagePop();

  /* Show result */
  ierr = MatMult(A,x,u);
  ierr = VecAXPY(&none,b,u);CHKERRQ(ierr);
  ierr = VecNorm(u,NORM_2,&norm);CHKERRQ(ierr);
  ierr = KSPGetIterationNumber(ksp,&its);CHKERRQ(ierr);
  /*  matrix PC   KSP   Options       its    residual setuptime solvetime  */
  if (table) {
    char   *matrixname,kspinfo[120];
    PetscViewer viewer;
    ierr = PetscViewerStringOpen(PETSC_COMM_WORLD,kspinfo,120,&viewer);CHKERRQ(ierr);
    ierr = KSPView(ksp,viewer);CHKERRQ(ierr);
    ierr = PetscStrrchr(file,'/',&matrixname);CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_WORLD,"%-8.8s %3d %2.0e %2.1e %2.1e %2.1e %s \n",
                       matrixname,its,norm,tsetup+tsolve,tsetup,tsolve,kspinfo);CHKERRQ(ierr);
    ierr = PetscViewerDestroy(viewer);CHKERRQ(ierr);
  } else {
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Number of iterations = %3d\n",its);CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Residual norm = %A\n",norm);CHKERRQ(ierr);
  }

  /* Cleanup */
  ierr = KSPDestroy(ksp);CHKERRQ(ierr);
  ierr = VecDestroy(x);CHKERRQ(ierr);
  ierr = VecDestroy(b);CHKERRQ(ierr);
  ierr = VecDestroy(u);CHKERRQ(ierr);
  ierr = MatDestroy(A);CHKERRQ(ierr);

  ierr = PetscFinalize();CHKERRQ(ierr);
#endif
  return 0;
}

