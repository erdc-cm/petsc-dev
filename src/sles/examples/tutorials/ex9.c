/*$Id: ex9.c,v 1.53 2001/08/07 21:30:54 bsmith Exp $*/

static char help[] = "The solution of 2 different linear systems with different linear solvers.\n\
Also, this example illustrates the repeated\n\
solution of linear systems, while reusing matrix, vector, and solver data\n\
structures throughout the process.  Note the various stages of event logging.\n\n";

/*T
   Concepts: SLES^repeatedly solving linear systems;
   Concepts: PetscLog^profiling multiple stages of code;
   Concepts: PetscLog^user-defined event profiling;
   Processors: n
T*/

/* 
  Include "petscsles.h" so that we can use SLES solvers.  Note that this file
  automatically includes:
     petsc.h       - base PETSc routines   petscvec.h - vectors
     petscsys.h    - system routines       petscmat.h - matrices
     petscis.h     - index sets            petscksp.h - Krylov subspace methods
     petscviewer.h - viewers               petscpc.h  - preconditioners
*/
#include "petscsles.h"

/* 
   Declare user-defined routines
*/
extern int CheckError(Vec,Vec,Vec,int,int);
extern int MyKSPMonitor(KSP,int,PetscReal,void*);

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **args)
{
  Vec          x1,b1,x2,b2; /* solution and RHS vectors for systems #1 and #2 */
  Vec          u;              /* exact solution vector */
  Mat          C1,C2;         /* matrices for systems #1 and #2 */
  SLES         sles1,sles2;   /* SLES contexts for systems #1 and #2 */
  KSP          ksp1;           /* KSP context for system #1 */
  int          ntimes = 3;     /* number of times to solve the linear systems */
  int          CHECK_ERROR;    /* event number for error checking */
  int          ldim,ierr,low,high,iglobal,Istart,Iend,Istart2,Iend2;
  int          I,J,i,j,m = 3,n = 2,rank,size,its,t;
  int          stages[3];
  PetscTruth   flg;
  PetscScalar  v;

  PetscInitialize(&argc,&args,(char *)0,help);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-m",&m,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-t",&ntimes,PETSC_NULL);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(PETSC_COMM_WORLD,&rank);CHKERRQ(ierr);
  ierr = MPI_Comm_size(PETSC_COMM_WORLD,&size);CHKERRQ(ierr);
  n = 2*size;

  /* 
     Register various stages for profiling
  */
  ierr = PetscLogStageRegister(&stages[0],"Prelim setup");CHKERRQ(ierr);
  ierr = PetscLogStageRegister(&stages[1],"Linear System 1");CHKERRQ(ierr);
  ierr = PetscLogStageRegister(&stages[2],"Linear System 2");CHKERRQ(ierr);

  /* 
     Register a user-defined event for profiling (error checking).
  */
  CHECK_ERROR = 0;
  ierr = PetscLogEventRegister(&CHECK_ERROR,"Check Error","Red:",SLES_COOKIE);CHKERRQ(ierr);

  /* - - - - - - - - - - - - Stage 0: - - - - - - - - - - - - - -
                        Preliminary Setup
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  ierr = PetscLogStagePush(stages[0]);CHKERRQ(ierr);

  /* 
     Create data structures for first linear system.
      - Create parallel matrix, specifying only its global dimensions.
        When using MatCreate(), the matrix format can be specified at
        runtime. Also, the parallel partitioning of the matrix is
        determined by PETSc at runtime.
      - Create parallel vectors.
        - When using VecSetSizes(), we specify only the vector's global
          dimension; the parallel partitioning is determined at runtime. 
        - Note: We form 1 vector from scratch and then duplicate as needed.
  */
  ierr = MatCreate(PETSC_COMM_WORLD,PETSC_DECIDE,PETSC_DECIDE,m*n,m*n,&C1);CHKERRQ(ierr);
  ierr = MatSetFromOptions(C1);CHKERRQ(ierr);
  ierr = MatGetOwnershipRange(C1,&Istart,&Iend);CHKERRQ(ierr);
  ierr = VecCreate(PETSC_COMM_WORLD,&u);CHKERRQ(ierr);
  ierr = VecSetSizes(u,PETSC_DECIDE,m*n);CHKERRQ(ierr);
  ierr = VecSetFromOptions(u);CHKERRQ(ierr);
  ierr = VecDuplicate(u,&b1);CHKERRQ(ierr);
  ierr = VecDuplicate(u,&x1);CHKERRQ(ierr);

  /*
     Create first linear solver context.
     Set runtime options (e.g., -pc_type <type>).
     Note that the first linear system uses the default option
     names, while the second linear systme uses a different
     options prefix.
  */
  ierr = SLESCreate(PETSC_COMM_WORLD,&sles1);CHKERRQ(ierr);
  ierr = SLESSetFromOptions(sles1);CHKERRQ(ierr);

  /* 
     Set user-defined monitoring routine for first linear system.
  */
  ierr = SLESGetKSP(sles1,&ksp1);CHKERRQ(ierr);
  ierr = PetscOptionsHasName(PETSC_NULL,"-my_ksp_monitor",&flg);CHKERRQ(ierr);
  if (flg) {ierr = KSPSetMonitor(ksp1,MyKSPMonitor,PETSC_NULL,0);CHKERRQ(ierr);}

  /*
     Create data structures for second linear system.
  */
  ierr = MatCreate(PETSC_COMM_WORLD,PETSC_DECIDE,PETSC_DECIDE,m*n,m*n,&C2);CHKERRQ(ierr);
  ierr = MatSetFromOptions(C2);CHKERRQ(ierr);
  ierr = MatGetOwnershipRange(C2,&Istart2,&Iend2);CHKERRQ(ierr);
  ierr = VecDuplicate(u,&b2);CHKERRQ(ierr);
  ierr = VecDuplicate(u,&x2);CHKERRQ(ierr);

  /*
     Create second linear solver context
  */
  ierr = SLESCreate(PETSC_COMM_WORLD,&sles2);CHKERRQ(ierr);

  /* 
     Set different options prefix for second linear system.
     Set runtime options (e.g., -s2_pc_type <type>)
  */
  ierr = SLESAppendOptionsPrefix(sles2,"s2_");CHKERRQ(ierr);
  ierr = SLESSetFromOptions(sles2);CHKERRQ(ierr);

  /* 
     Assemble exact solution vector in parallel.  Note that each
     processor needs to set only its local part of the vector.
  */
  ierr = VecGetLocalSize(u,&ldim);CHKERRQ(ierr);
  ierr = VecGetOwnershipRange(u,&low,&high);CHKERRQ(ierr);
  for (i=0; i<ldim; i++) {
    iglobal = i + low;
    v = (PetscScalar)(i + 100*rank);
    ierr = VecSetValues(u,1,&iglobal,&v,ADD_VALUES);CHKERRQ(ierr);
  }
  ierr = VecAssemblyBegin(u);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(u);CHKERRQ(ierr);

  /* 
     Log the number of flops for computing vector entries
  */
  ierr = PetscLogFlops(2*ldim);CHKERRQ(ierr);

  /*
     End curent profiling stage
  */
  ierr = PetscLogStagePop();CHKERRQ(ierr);

  /* -------------------------------------------------------------- 
                        Linear solver loop:
      Solve 2 different linear systems several times in succession 
     -------------------------------------------------------------- */

  for (t=0; t<ntimes; t++) {

    /* - - - - - - - - - - - - Stage 1: - - - - - - - - - - - - - -
                 Assemble and solve first linear system            
       - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

    /*
       Begin profiling stage #1
    */
    ierr = PetscLogStagePush(stages[1]);CHKERRQ(ierr);

    /* 
       Initialize all matrix entries to zero.  MatZeroEntries() retains
       the nonzero structure of the matrix for sparse formats.
    */
    ierr = MatZeroEntries(C1);CHKERRQ(ierr);

    /* 
       Set matrix entries in parallel.  Also, log the number of flops
       for computing matrix entries.
        - Each processor needs to insert only elements that it owns
          locally (but any non-local elements will be sent to the
          appropriate processor during matrix assembly). 
        - Always specify global row and columns of matrix entries.
    */
    for (I=Istart; I<Iend; I++) { 
      v = -1.0; i = I/n; j = I - i*n;  
      if (i>0)   {J = I - n; ierr = MatSetValues(C1,1,&I,1,&J,&v,ADD_VALUES);CHKERRQ(ierr);}
      if (i<m-1) {J = I + n; ierr = MatSetValues(C1,1,&I,1,&J,&v,ADD_VALUES);CHKERRQ(ierr);}
      if (j>0)   {J = I - 1; ierr = MatSetValues(C1,1,&I,1,&J,&v,ADD_VALUES);CHKERRQ(ierr);}
      if (j<n-1) {J = I + 1; ierr = MatSetValues(C1,1,&I,1,&J,&v,ADD_VALUES);CHKERRQ(ierr);}
      v = 4.0; ierr = MatSetValues(C1,1,&I,1,&I,&v,ADD_VALUES);CHKERRQ(ierr);
    }
    for (I=Istart; I<Iend; I++) { /* Make matrix nonsymmetric */
      v = -1.0*(t+0.5); i = I/n;
      if (i>0)   {J = I - n; ierr = MatSetValues(C1,1,&I,1,&J,&v,ADD_VALUES);CHKERRQ(ierr);}
    }
    ierr = PetscLogFlops(2*(Istart-Iend));CHKERRQ(ierr);

    /* 
       Assemble matrix, using the 2-step process:
         MatAssemblyBegin(), MatAssemblyEnd()
       Computations can be done while messages are in transition
       by placing code between these two statements.
    */
    ierr = MatAssemblyBegin(C1,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
    ierr = MatAssemblyEnd(C1,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);

    /* 
       Indicate same nonzero structure of successive linear system matrices
    */
    ierr = MatSetOption(C1,MAT_NO_NEW_NONZERO_LOCATIONS);CHKERRQ(ierr);

    /* 
       Compute right-hand-side vector
    */
    ierr = MatMult(C1,u,b1);CHKERRQ(ierr);

    /* 
       Set operators. Here the matrix that defines the linear system
       also serves as the preconditioning matrix.
        - The flag SAME_NONZERO_PATTERN indicates that the
          preconditioning matrix has identical nonzero structure
          as during the last linear solve (although the values of
          the entries have changed). Thus, we can save some
          work in setting up the preconditioner (e.g., no need to
          redo symbolic factorization for ILU/ICC preconditioners).
        - If the nonzero structure of the matrix is different during
          the second linear solve, then the flag DIFFERENT_NONZERO_PATTERN
          must be used instead.  If you are unsure whether the
          matrix structure has changed or not, use the flag
          DIFFERENT_NONZERO_PATTERN.
        - Caution:  If you specify SAME_NONZERO_PATTERN, PETSc
          believes your assertion and does not check the structure
          of the matrix.  If you erroneously claim that the structure
          is the same when it actually is not, the new preconditioner
          will not function correctly.  Thus, use this optimization
          feature with caution!
    */
    ierr = SLESSetOperators(sles1,C1,C1,SAME_NONZERO_PATTERN);CHKERRQ(ierr);

    /* 
       Use the previous solution of linear system #1 as the initial
       guess for the next solve of linear system #1.  The user MUST
       call KSPSetInitialGuessNonzero() in indicate use of an initial
       guess vector; otherwise, an initial guess of zero is used.
    */
    if (t>0) {
      ierr = KSPSetInitialGuessNonzero(ksp1,PETSC_TRUE);CHKERRQ(ierr);
    }

    /* 
       Solve the first linear system.  Here we explicitly call
       SLESSetUp() for more detailed performance monitoring of
       certain preconditioners, such as ICC and ILU.  This call
       is optional, ase SLESSetUp() will automatically be called
       within SLESSolve() if it hasn't been called already.
    */
    ierr = SLESSetUp(sles1,b1,x1);CHKERRQ(ierr);
    ierr = SLESSolve(sles1,b1,x1,&its);CHKERRQ(ierr);

    /*
       Check error of solution to first linear system
    */
    ierr = CheckError(u,x1,b1,its,CHECK_ERROR);CHKERRQ(ierr); 

    /* - - - - - - - - - - - - Stage 2: - - - - - - - - - - - - - -
                 Assemble and solve second linear system            
       - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

    /*
       Conclude profiling stage #1; begin profiling stage #2
    */
    ierr = PetscLogStagePop();CHKERRQ(ierr);
    ierr = PetscLogStagePush(stages[2]);CHKERRQ(ierr);

    /*
       Initialize all matrix entries to zero
    */
    ierr = MatZeroEntries(C2);CHKERRQ(ierr);

   /* 
      Assemble matrix in parallel. Also, log the number of flops
      for computing matrix entries.
       - To illustrate the features of parallel matrix assembly, we
         intentionally set the values differently from the way in
         which the matrix is distributed across the processors.  Each
         entry that is not owned locally will be sent to the appropriate
         processor during MatAssemblyBegin() and MatAssemblyEnd().
       - For best efficiency the user should strive to set as many
         entries locally as possible.
    */
    for (i=0; i<m; i++) { 
      for (j=2*rank; j<2*rank+2; j++) {
        v = -1.0;  I = j + n*i;
        if (i>0)   {J = I - n; ierr = MatSetValues(C2,1,&I,1,&J,&v,ADD_VALUES);CHKERRQ(ierr);}
        if (i<m-1) {J = I + n; ierr = MatSetValues(C2,1,&I,1,&J,&v,ADD_VALUES);CHKERRQ(ierr);}
        if (j>0)   {J = I - 1; ierr = MatSetValues(C2,1,&I,1,&J,&v,ADD_VALUES);CHKERRQ(ierr);}
        if (j<n-1) {J = I + 1; ierr = MatSetValues(C2,1,&I,1,&J,&v,ADD_VALUES);CHKERRQ(ierr);}
        v = 6.0 + t*0.5; ierr = MatSetValues(C2,1,&I,1,&I,&v,ADD_VALUES);CHKERRQ(ierr);
      }
    } 
    for (I=Istart2; I<Iend2; I++) { /* Make matrix nonsymmetric */
      v = -1.0*(t+0.5); i = I/n;
      if (i>0)   {J = I - n; ierr = MatSetValues(C2,1,&I,1,&J,&v,ADD_VALUES);CHKERRQ(ierr);}
    }
    ierr = MatAssemblyBegin(C2,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
    ierr = MatAssemblyEnd(C2,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr); 
    ierr = PetscLogFlops(2*(Istart-Iend));CHKERRQ(ierr);

    /* 
       Indicate same nonzero structure of successive linear system matrices
    */
    ierr = MatSetOption(C2,MAT_NO_NEW_NONZERO_LOCATIONS);CHKERRQ(ierr);

    /*
       Compute right-hand-side vector 
    */
    ierr = MatMult(C2,u,b2);CHKERRQ(ierr);

    /*
       Set operators. Here the matrix that defines the linear system
       also serves as the preconditioning matrix.  Indicate same nonzero
       structure of successive preconditioner matrices by setting flag
       SAME_NONZERO_PATTERN.
    */
    ierr = SLESSetOperators(sles2,C2,C2,SAME_NONZERO_PATTERN);CHKERRQ(ierr);

    /* 
       Solve the second linear system
    */
    ierr = SLESSetUp(sles2,b2,x2);CHKERRQ(ierr);
    ierr = SLESSolve(sles2,b2,x2,&its);CHKERRQ(ierr);

    /*
       Check error of solution to first linear system
    */
    ierr = CheckError(u,x2,b2,its,CHECK_ERROR);CHKERRQ(ierr); 

    /* 
       Conclude profiling stage #2
    */
    PetscLogStagePop();
  }
  /* -------------------------------------------------------------- 
                       End of linear solver loop
     -------------------------------------------------------------- */

  /* 
     Free work space.  All PETSc objects should be destroyed when they
     are no longer needed.
  */
  ierr = SLESDestroy(sles1);CHKERRQ(ierr); ierr = SLESDestroy(sles2);CHKERRQ(ierr);
  ierr = VecDestroy(x1);CHKERRQ(ierr);     ierr = VecDestroy(x2);CHKERRQ(ierr);
  ierr = VecDestroy(b1);CHKERRQ(ierr);     ierr = VecDestroy(b2);CHKERRQ(ierr);
  ierr = MatDestroy(C1);CHKERRQ(ierr);     ierr = MatDestroy(C2);CHKERRQ(ierr);
  ierr = VecDestroy(u);CHKERRQ(ierr);

  ierr = PetscFinalize();CHKERRQ(ierr);
  return 0;
}
#undef __FUNCT__
#define __FUNCT__ "CheckError"
/* ------------------------------------------------------------- */
/*
    CheckError - Checks the error of the solution.

    Input Parameters:
    u - exact solution
    x - approximate solution
    b - work vector
    its - number of iterations for convergence
    CHECK_ERROR - the event number for error checking
                  (for use with profiling)

    Notes:
    In order to profile this section of code separately from the
    rest of the program, we register it as an "event" with
    PetscLogEventRegister() in the main program.  Then, we indicate
    the start and end of this event by respectively calling
        PetscLogEventBegin(CHECK_ERROR,u,x,b,0);
        PetscLogEventEnd(CHECK_ERROR,u,x,b,0);
    Here, we specify the objects most closely associated with
    the event (the vectors u,x,b).  Such information is optional;
    we could instead just use 0 instead for all objects.
*/
int CheckError(Vec u,Vec x,Vec b,int its,int CHECK_ERROR)
{
  PetscScalar none = -1.0;
  PetscReal   norm;
  int         ierr;

  ierr = PetscLogEventBegin(CHECK_ERROR,u,x,b,0);CHKERRQ(ierr);

  /*
     Compute error of the solution, using b as a work vector.
  */
  ierr = VecCopy(x,b);CHKERRQ(ierr);
  ierr = VecAXPY(&none,u,b);CHKERRQ(ierr);
  ierr = VecNorm(b,NORM_2,&norm);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Norm of error %A, Iterations %d\n",norm,its);CHKERRQ(ierr);
  ierr = PetscLogEventEnd(CHECK_ERROR,u,x,b,0);CHKERRQ(ierr);
  return 0;
}
/* ------------------------------------------------------------- */
#undef __FUNCT__
#define __FUNCT__ "MyKSPMonitor"
/*
   MyKSPMonitor - This is a user-defined routine for monitoring
   the SLES iterative solvers.

   Input Parameters:
     ksp   - iterative context
     n     - iteration number
     rnorm - 2-norm (preconditioned) residual value (may be estimated)
     dummy - optional user-defined monitor context (unused here)
*/
int MyKSPMonitor(KSP ksp,int n,PetscReal rnorm,void *dummy)
{
  Vec      x;
  int      ierr;

  /* 
     Build the solution vector
  */
  ierr = KSPBuildSolution(ksp,PETSC_NULL,&x);CHKERRQ(ierr);

  /*
     Write the solution vector and residual norm to stdout.
      - PetscPrintf() handles output for multiprocessor jobs 
        by printing from only one processor in the communicator.
      - The parallel viewer PETSC_VIEWER_STDOUT_WORLD handles
        data from multiple processors so that the output
        is not jumbled.
  */
  ierr = PetscPrintf(PETSC_COMM_WORLD,"iteration %d solution vector:\n",n);CHKERRQ(ierr);
  ierr = VecView(x,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"iteration %d KSP Residual norm %14.12e \n",n,rnorm);CHKERRQ(ierr);
  return 0;
}

