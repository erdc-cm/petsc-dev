
static char help[] ="Tests sequential and parallel MatMatMult() and MatPtAP()\n\
  -Mx <xg>, where <xg> = number of coarse grid points in the x-direction\n\
  -My <yg>, where <yg> = number of coarse grid points in the y-direction\n\
  -Npx <npx>, where <npx> = number of processors in the x-direction\n\
  -Npy <npy>, where <npy> = number of processors in the y-direction\n\n";

/*  This test is modified from ~src/ksp/examples/tests/ex19.c.
    This problem is modeled by
    the partial differential equation
  
            -Laplacian u  = g,  0 < x,y < 1,
  
    with boundary conditions
   
             u = 0  for  x = 0, x = 1, y = 0, y = 1.
  
    A finite difference approximation with the usual 5-point stencil
    is used to discretize the boundary value problem to obtain a nonlinear 
    system of equations.
*/

#include "petscksp.h"
#include "petscda.h"
#include "petscmg.h"
#include "src/mat/impls/aij/seq/aij.h"
#include "src/mat/impls/aij/mpi/mpiaij.h"

/* User-defined application contexts */
typedef struct {
   int        mx,my;            /* number grid points in x and y direction */
   Vec        localX,localF;    /* local vectors with ghost region */
   DA         da;
   Vec        x,b,r;            /* global vectors */
   Mat        J;                /* Jacobian on grid */
} GridCtx;
typedef struct {
   GridCtx     fine;
   GridCtx     coarse;
   KSP         ksp_coarse;
   int         ratio;
   Mat         I;               /* interpolation from coarse to fine */
} AppCtx;

#define COARSE_LEVEL 0
#define FINE_LEVEL   1

/*
      Mm_ratio - ration of grid lines between fine and coarse grids.
*/
#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **argv)
{
  PetscErrorCode ierr;
  AppCtx         user;                      
  int            Nx = PETSC_DECIDE,Ny = PETSC_DECIDE;
  int            size,rank,m,n,M,N,i,nrows,*ia,*ja; 
  PetscScalar   one = 1.0;
  PetscReal     fill=2.0;
  Mat           A,A_tmp,P,C;
  PetscScalar   *array,none = -1.0,alpha;
  PetscTruth    flg;
  Vec          x,v1,v2;
  PetscReal    norm,norm_tmp,norm_tmp1,tol=1.e-12;
  PetscRandom  rand;
  PetscTruth   Test_MatMatMult=PETSC_TRUE,Test_MatPtAP=PETSC_TRUE;

  PetscInitialize(&argc,&argv,PETSC_NULL,help);
  ierr = PetscOptionsGetReal(PETSC_NULL,"-tol",&tol,PETSC_NULL);CHKERRQ(ierr);

  user.ratio = 2;
  user.coarse.mx = 2; user.coarse.my = 2; 
  ierr = PetscOptionsGetInt(PETSC_NULL,"-Mx",&user.coarse.mx,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-My",&user.coarse.my,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-ratio",&user.ratio,PETSC_NULL);CHKERRQ(ierr);
  user.fine.mx = user.ratio*(user.coarse.mx-1)+1; user.fine.my = user.ratio*(user.coarse.my-1)+1;
  /*
  PetscPrintf(PETSC_COMM_WORLD,"Coarse grid size %d by %d\n",user.coarse.mx,user.coarse.my);
  PetscPrintf(PETSC_COMM_WORLD,"Fine grid size %d by %d\n",user.fine.mx,user.fine.my);
  */
  MPI_Comm_size(PETSC_COMM_WORLD,&size);
  MPI_Comm_rank(PETSC_COMM_WORLD,&rank);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-Npx",&Nx,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-Npy",&Ny,PETSC_NULL);CHKERRQ(ierr);

  /* Set up distributed array for fine grid */
  ierr = DACreate2d(PETSC_COMM_WORLD,DA_NONPERIODIC,DA_STENCIL_STAR,user.fine.mx,
                    user.fine.my,Nx,Ny,1,1,PETSC_NULL,PETSC_NULL,&user.fine.da);CHKERRQ(ierr);

  ierr = DAGetMatrix(user.fine.da,MATAIJ,&A);CHKERRQ(ierr);
  ierr = MatGetLocalSize(A,&m,&n);CHKERRQ(ierr);
  ierr = MatGetSize(A,&M,&N);CHKERRQ(ierr);
  /* ierr = PetscPrintf(PETSC_COMM_SELF," [%d] A - loc and gl dim: %d, %d; %d, %d\n",rank,m,n,M,N);*/
  /* set val=one to A */
  if (size == 1){
    ierr = MatGetRowIJ(A,0,PETSC_FALSE,&nrows,&ia,&ja,&flg);
    if (flg){
      ierr = MatGetArray(A,&array);CHKERRQ(ierr);
      for (i=0; i<ia[nrows]; i++) array[i] = one;
      ierr = MatRestoreArray(A,&array);CHKERRQ(ierr);
    }
    ierr = MatRestoreRowIJ(A,0,PETSC_FALSE,&nrows,&ia,&ja,&flg);
  } else {
    Mat_MPIAIJ *aij = (Mat_MPIAIJ*)A->data;
    Mat_SeqAIJ *a=(Mat_SeqAIJ*)(aij->A)->data, *b=(Mat_SeqAIJ*)(aij->B)->data;
    /* A_part */
    for (i=0; i<a->i[m]; i++) a->a[i] = one;
    /* B_part */
    for (i=0; i<b->i[m]; i++) b->a[i] = one;
    
  }
  /* ierr = MatView(A, PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr); */

  /* Set up distributed array for coarse grid */
  ierr = DACreate2d(PETSC_COMM_WORLD,DA_NONPERIODIC,DA_STENCIL_STAR,user.coarse.mx,
                    user.coarse.my,Nx,Ny,1,1,PETSC_NULL,PETSC_NULL,&user.coarse.da);CHKERRQ(ierr);

  /* Create interpolation between the levels */
  ierr = DAGetInterpolation(user.coarse.da,user.fine.da,&P,PETSC_NULL);CHKERRQ(ierr);
  
  ierr = MatGetLocalSize(P,&m,&n);CHKERRQ(ierr);
  ierr = MatGetSize(P,&M,&N);CHKERRQ(ierr);
  /* ierr = PetscPrintf(PETSC_COMM_SELF," [%d] P - loc and gl dim: %d, %d; %d, %d\n",rank,m,n,M,N);*/
  /* ierr = MatView(P, PETSC_VIEWER_DRAW_WORLD);CHKERRQ(ierr); */

  /* Create vectors v1 and v2 that are compatible with A */
  ierr = VecCreate(PETSC_COMM_WORLD,&v1);CHKERRQ(ierr);
  ierr = MatGetLocalSize(A,&m,PETSC_NULL);CHKERRQ(ierr);
  ierr = VecSetSizes(v1,m,PETSC_DECIDE);CHKERRQ(ierr);
  ierr = VecSetFromOptions(v1);CHKERRQ(ierr);
  ierr = VecDuplicate(v1,&v2);CHKERRQ(ierr);
  ierr = PetscRandomCreate(PETSC_COMM_WORLD,RANDOM_DEFAULT,&rand);CHKERRQ(ierr);

  /* Test MatMatMult(): C = A*P */
  /*----------------------------*/
  if (Test_MatMatMult){
    ierr = MatDuplicate(A,MAT_COPY_VALUES,&A_tmp);CHKERRQ(ierr);
    ierr = MatMatMult(A_tmp,P,MAT_INITIAL_MATRIX,fill,&C);CHKERRQ(ierr);
    
    /* Test MAT_REUSE_MATRIX - reuse symbolic C */
    alpha=1.0;
    for (i=0; i<2; i++){
      alpha -=0.1;
      ierr = MatScale(&alpha,A_tmp);CHKERRQ(ierr);
      ierr = MatMatMult(A_tmp,P,MAT_REUSE_MATRIX,fill,&C);CHKERRQ(ierr);
    }
    /*
    if (rank == 0) ierr = PetscPrintf(PETSC_COMM_SELF, " \nA*P: \n");
    ierr = MatView(C, PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
    */

    /* Create vector x that is compatible with P */
    ierr = VecCreate(PETSC_COMM_WORLD,&x);CHKERRQ(ierr);
    ierr = MatGetLocalSize(P,PETSC_NULL,&n);CHKERRQ(ierr);
    ierr = VecSetSizes(x,n,PETSC_DECIDE);CHKERRQ(ierr);
    ierr = VecSetFromOptions(x);CHKERRQ(ierr);

    norm = 0.0;
    for (i=0; i<10; i++) {
      ierr = VecSetRandom(rand,x);CHKERRQ(ierr);
      ierr = MatMult(P,x,v1);CHKERRQ(ierr);  
      ierr = MatMult(A_tmp,v1,v2);CHKERRQ(ierr);  /* v2 = A*P*x */
      ierr = MatMult(C,x,v1);CHKERRQ(ierr);       /* v1 = C*x   */
      ierr = VecAXPY(&none,v2,v1);CHKERRQ(ierr);
      ierr = VecNorm(v1,NORM_1,&norm_tmp);CHKERRQ(ierr);
      ierr = VecNorm(v2,NORM_1,&norm_tmp1);CHKERRQ(ierr);
      norm_tmp /= norm_tmp1;
      if (norm_tmp > norm) norm = norm_tmp;
    }
    if (norm >= tol && !rank) {
      ierr = PetscPrintf(PETSC_COMM_SELF,"Error: MatMatMult(), |v1 - v2|/|v2|: %g\n",norm);CHKERRQ(ierr);
    }
    
    ierr = VecDestroy(x);CHKERRQ(ierr);
    ierr = MatDestroy(C);CHKERRQ(ierr);
    ierr = MatDestroy(A_tmp);CHKERRQ(ierr);
  }

  /* Test P^T * A * P - MatPtAP() */ 
  /*------------------------------*/
  if (Test_MatPtAP){
    ierr = MatPtAP(A,P,MAT_INITIAL_MATRIX,fill,&C);CHKERRQ(ierr); 
    ierr = MatGetLocalSize(C,&m,&n);CHKERRQ(ierr);
    
    /* ierr = PetscPrintf(PETSC_COMM_SELF, " \n[%d] C=P^T*A*P, dim %d, %d: \n",rank,m,n); */
    /* ierr = MatView(C, PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr); */

    /* Test MAT_REUSE_MATRIX - reuse symbolic C */
    alpha=1.0;
    for (i=0; i<1; i++){
      alpha -=0.1;
      ierr = MatScale(&alpha,A);CHKERRQ(ierr);
      ierr = MatPtAP(A,P,MAT_REUSE_MATRIX,fill,&C);CHKERRQ(ierr);
    }

    /* Create vector x that is compatible with P */
    ierr = VecCreate(PETSC_COMM_WORLD,&x);CHKERRQ(ierr);
    ierr = MatGetLocalSize(P,&m,&n);CHKERRQ(ierr);
    ierr = VecSetSizes(x,n,PETSC_DECIDE);CHKERRQ(ierr);
    ierr = VecSetFromOptions(x);CHKERRQ(ierr);
  
    Vec v3,v4;
    ierr = VecCreate(PETSC_COMM_WORLD,&v3);CHKERRQ(ierr);
    ierr = VecSetSizes(v3,n,PETSC_DECIDE);CHKERRQ(ierr);
    ierr = VecSetFromOptions(v3);CHKERRQ(ierr);
    ierr = VecDuplicate(v3,&v4);CHKERRQ(ierr);

    norm = 0.0;
    for (i=0; i<10; i++) {
      ierr = VecSetRandom(rand,x);CHKERRQ(ierr);
      ierr = MatMult(P,x,v1);CHKERRQ(ierr);  
      ierr = MatMult(A,v1,v2);CHKERRQ(ierr);  /* v2 = A*P*x */

      ierr = MatMultTranspose(P,v2,v3);CHKERRQ(ierr); /* v3 = Pt*A*P*x */
      ierr = MatMult(C,x,v4);CHKERRQ(ierr);           /* v3 = C*x   */
      ierr = VecAXPY(&none,v3,v4);CHKERRQ(ierr);
      ierr = VecNorm(v4,NORM_1,&norm_tmp);CHKERRQ(ierr);
      ierr = VecNorm(v3,NORM_1,&norm_tmp1);CHKERRQ(ierr);
      norm_tmp /= norm_tmp1;
      if (norm_tmp > norm) norm = norm_tmp;
    }
    if (norm >= tol && !rank) {
      ierr = PetscPrintf(PETSC_COMM_SELF,"Error: MatPtAP(), |v3 - v4|/|v3|: %g\n",norm);CHKERRQ(ierr);
    }
  
    ierr = MatDestroy(C);CHKERRQ(ierr);
    ierr = VecDestroy(v3);CHKERRQ(ierr);
    ierr = VecDestroy(v4);CHKERRQ(ierr);
    ierr = VecDestroy(x);CHKERRQ(ierr);
 
  }

  /* Clean up */
   ierr = MatDestroy(A);CHKERRQ(ierr);
  ierr = PetscRandomDestroy(rand);CHKERRQ(ierr);
  ierr = VecDestroy(v1);CHKERRQ(ierr);
  ierr = VecDestroy(v2);CHKERRQ(ierr);
  ierr = DADestroy(user.fine.da);CHKERRQ(ierr);
  ierr = DADestroy(user.coarse.da);CHKERRQ(ierr);
  ierr = MatDestroy(P);CHKERRQ(ierr); 

  ierr = PetscFinalize();CHKERRQ(ierr);

  return 0;
}
