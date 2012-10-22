
static char help[] ="Model Equations for Advection-Diffusion\n";


/*
    Page 9, Section 1.2 Model Equations for Advection-Diffusion

          u_t = a u_x + d u_xx

   The initial conditions used here different then in the book.

*/

/*
   Include "petscts.h" so that we can use TS solvers.  Note that this file
   automatically includes:
     petscsys.h       - base PETSc routines   petscvec.h  - vectors
     petscmat.h  - matrices
     petscis.h     - index sets            petscksp.h  - Krylov subspace methods
     petscviewer.h - viewers               petscpc.h   - preconditioners
     petscksp.h   - linear solvers        petscsnes.h - nonlinear solvers
*/

#include <petscts.h>

/*
   User-defined application context - contains data needed by the
   application-provided call-back routines.
*/
typedef struct {
  PetscReal   h;     /* mesh width */
  PetscScalar a,d;   /* advection and diffusion strength */
  PetscBool   upwind; 
} AppCtx;

/*
   User-defined routines
*/
extern PetscErrorCode InitialConditions(TS,Vec,AppCtx*);
extern PetscErrorCode RHSMatrixHeat(TS,PetscReal,Vec,Mat*,Mat*,MatStructure*,void*);
extern PetscErrorCode ExactSolution(TS,PetscReal,Vec,AppCtx*);

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **argv)
{
  AppCtx         appctx;                 /* user-defined application context */
  TS             ts;                     /* timestepping context */
  Mat            A;                      /* matrix data structure */
  Vec            u;                      /* approximate solution vector */
  PetscReal      time_total_max = 100.0; /* default max total time */
  PetscInt       time_steps_max = 100;   /* default max timesteps */
  PetscErrorCode ierr;
  PetscInt       steps,m;
  PetscReal      dt;
  DM             da;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Initialize program and set problem parameters
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  ierr = PetscInitialize(&argc,&argv,(char*)0,help);CHKERRQ(ierr);

  m    = 60;
  ierr = PetscOptionsGetInt(PETSC_NULL,"-m",&m,PETSC_NULL);CHKERRQ(ierr);
  appctx.h        = 1.0/m;
  appctx.a        = 1.0;
  appctx.d        = 0.0;
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-a",&appctx.a,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-d",&appctx.d,PETSC_NULL);CHKERRQ(ierr);
  appctx.upwind   = PETSC_TRUE;
  ierr = PetscOptionsGetBool(PETSC_NULL,"-upwind",&appctx.upwind,PETSC_NULL);CHKERRQ(ierr);
  
  ierr = DMDACreate1d(PETSC_COMM_WORLD,DMDA_BOUNDARY_PERIODIC, m, 1, 1,PETSC_NULL,&da);CHKERRQ(ierr);
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Create vector data structures
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*
     Create vector data structures for approximate and exact solutions
  */
  ierr = DMCreateGlobalVector(da,&u);CHKERRQ(ierr);
  
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Create timestepping solver context
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  ierr = TSCreate(PETSC_COMM_WORLD,&ts);CHKERRQ(ierr);
  ierr = TSSetDM(ts,da);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

     Create matrix data structure; set matrix evaluation routine.
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  ierr = DMCreateMatrix(da,MATAIJ,&A);CHKERRQ(ierr);

  /*
      For linear problems with a time-dependent f(u,t) in the equation
     u_t = f(u,t), the user provides the discretized right-hand-side
      as a time-dependent matrix.
  */
  ierr = TSSetRHSFunction(ts,PETSC_NULL,TSComputeRHSFunctionLinear,&appctx);CHKERRQ(ierr);
  ierr = TSSetRHSJacobian(ts,A,A,RHSMatrixHeat,&appctx);CHKERRQ(ierr);
  ierr = TSSetSolutionFunction(ts,(PetscErrorCode (*)(TS,PetscReal,Vec,void*))ExactSolution,&appctx);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Set solution vector and initial timestep
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  dt = appctx.h*appctx.h/2.0;
  ierr = TSSetInitialTimeStep(ts,0.0,dt);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Customize timestepping solver:
       - Set timestepping duration info
     Then set runtime options, which can override these defaults.
     For example,
          -ts_max_steps <maxsteps> -ts_final_time <maxtime>
     to override the defaults set by TSSetDuration().
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  ierr = TSSetDuration(ts,time_steps_max,time_total_max);CHKERRQ(ierr);
  ierr = TSSetType(ts,TSARKIMEX);CHKERRQ(ierr);
  ierr = TSSetFromOptions(ts);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Solve the problem
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*
     Evaluate initial conditions
  */
  ierr = InitialConditions(ts,u,&appctx);CHKERRQ(ierr);

  /*
     Run the timestepping solver
  */
  ierr = TSSolve(ts,u,PETSC_NULL);CHKERRQ(ierr);
  ierr = TSGetTimeStepNumber(ts,&steps);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Free work space.  All PETSc objects should be destroyed when they
     are no longer needed.
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  ierr = TSDestroy(&ts);CHKERRQ(ierr);
  ierr = MatDestroy(&A);CHKERRQ(ierr);
  ierr = VecDestroy(&u);CHKERRQ(ierr);
  ierr = DMDestroy(&da);CHKERRQ(ierr);

  /*
     Always call PetscFinalize() before exiting a program.  This routine
       - finalizes the PETSc libraries as well as MPI
       - provides summary and diagnostic information if certain runtime
         options are chosen (e.g., -log_summary).
  */
  ierr = PetscFinalize();
  return 0;
}
/* --------------------------------------------------------------------- */
#undef __FUNCT__
#define __FUNCT__ "InitialConditions"
/*
   InitialConditions - Computes the solution at the initial time.

   Input Parameter:
   u - uninitialized solution vector (global)
   appctx - user-defined application context

   Output Parameter:
   u - vector with solution at initial time (global)
*/
PetscErrorCode InitialConditions(TS ts,Vec u,AppCtx *appctx)
{
  PetscScalar    *u_localptr,h = appctx->h;
  PetscErrorCode ierr;
  PetscInt       i,mstart,mend,xm;
  DM             da;

  ierr = TSGetDM(ts,&da);CHKERRQ(ierr);
  ierr = DMDAGetCorners(da,&mstart,0,0,&xm,0,0);CHKERRQ(ierr);
  mend = mstart + xm;
  /*
    Get a pointer to vector data.
    - For default PETSc vectors, VecGetArray() returns a pointer to
      the data array.  Otherwise, the routine is implementation dependent.
    - You MUST call VecRestoreArray() when you no longer need access to
      the array.
    - Note that the Fortran interface to VecGetArray() differs from the
      C version.  See the users manual for details.
  */
  ierr = DMDAVecGetArray(da,u,&u_localptr);CHKERRQ(ierr);

  /*
     We initialize the solution array by simply writing the solution
     directly into the array locations.  Alternatively, we could use
     VecSetValues() or VecSetValuesLocal().
  */
  for (i=mstart; i<mend; i++) {
    u_localptr[i] = PetscSinScalar(PETSC_PI*i*6.*h) + 3.*PetscSinScalar(PETSC_PI*i*2.*h);
  }

  /*
     Restore vector
  */
  ierr = DMDAVecRestoreArray(da,u,&u_localptr);CHKERRQ(ierr);

  return 0;
}
/* --------------------------------------------------------------------- */
#undef __FUNCT__
#define __FUNCT__ "ExactSolution"
/*
   ExactSolution - Computes the exact solution at a given time.

   Input Parameters:
   t - current time
   solution - vector in which exact solution will be computed
   appctx - user-defined application context

   Output Parameter:
   solution - vector with the newly computed exact solution
*/
PetscErrorCode ExactSolution(TS ts,PetscReal t,Vec solution,AppCtx *appctx)
{
  PetscScalar    *s_localptr,h = appctx->h,ex1,ex2,sc1,sc2;
  PetscErrorCode ierr;
  PetscInt       i,mstart,mend,xm;
  DM             da;

  ierr = TSGetDM(ts,&da);CHKERRQ(ierr);
  ierr = DMDAGetCorners(da,&mstart,0,0,&xm,0,0);CHKERRQ(ierr);
  mend = mstart + xm;
  /*
     Get a pointer to vector data.
  */
  ierr = DMDAVecGetArray(da,solution,&s_localptr);CHKERRQ(ierr);

  /*
     Simply write the solution directly into the array locations.
     Alternatively, we culd use VecSetValues() or VecSetValuesLocal().
  */
  ex1 = PetscExpScalar(-36.*PETSC_PI*PETSC_PI*appctx->d*t);
  ex2 = PetscExpScalar(-4.*PETSC_PI*PETSC_PI*appctx->d*t);
  sc1 = PETSC_PI*6.*h;                 sc2 = PETSC_PI*2.*h;
  for (i=mstart; i<mend; i++) {
    s_localptr[i] = PetscSinScalar(sc1*(PetscReal)i + appctx->a*PETSC_PI*6.*t)*ex1 + 3.*PetscSinScalar(sc2*(PetscReal)i + appctx->a*PETSC_PI*2.*t)*ex2;
  }

  /*
     Restore vector
  */
  ierr = DMDAVecRestoreArray(da,solution,&s_localptr);CHKERRQ(ierr);
  return 0;
}


/* --------------------------------------------------------------------- */
#undef __FUNCT__
#define __FUNCT__ "RHSMatrixHeat"
/*
   RHSMatrixHeat - User-provided routine to compute the right-hand-side
   matrix for the heat equation.

   Input Parameters:
   ts - the TS context
   t - current time
   global_in - global input vector
   dummy - optional user-defined context, as set by TSetRHSJacobian()

   Output Parameters:
   AA - Jacobian matrix
   BB - optionally different preconditioning matrix
   str - flag indicating matrix structure

   Notes:
   Recall that MatSetValues() uses 0-based row and column numbers
   in Fortran as well as in C.
*/
PetscErrorCode RHSMatrixHeat(TS ts,PetscReal t,Vec X,Mat *AA,Mat *BB,MatStructure *str,void *ctx)
{
  Mat            A = *AA;                      /* Jacobian matrix */
  AppCtx         *appctx = (AppCtx*)ctx;     /* user-defined application context */
  PetscInt       mstart, mend;
  PetscErrorCode ierr;
  PetscInt       i,idx[3],M,xm;
  PetscScalar    v[3];
  DM             da;

  ierr = TSGetDM(ts,&da);CHKERRQ(ierr);
  ierr = DMDAGetInfo(da,0,&M,0,0,0,0,0,0,0,0,0,0,0);CHKERRQ(ierr);
  ierr = DMDAGetCorners(da,&mstart,0,0,&xm,0,0);CHKERRQ(ierr);
  mend = mstart + xm;
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Compute entries for the locally owned part of the matrix
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  /*
     Set matrix rows corresponding to boundary data
  */

  /* diffusion */
  v[0] = appctx->d/(appctx->h*appctx->h); 
  v[1] = -2.0*appctx->d/(appctx->h*appctx->h);
  v[2] = appctx->d/(appctx->h*appctx->h);
  if (!mstart) {
    idx[0] = M-1; idx[1] = 0; idx[2] = 1;
    ierr = MatSetValues(A,1,&mstart,3,idx,v,INSERT_VALUES);CHKERRQ(ierr);
    mstart++;
  }

  if (mend == M){ 
    mend--;
    idx[0] = M-2; idx[1] = M-1; idx[2] = 0;
    ierr = MatSetValues(A,1,&mend,3,idx,v,INSERT_VALUES);CHKERRQ(ierr);
  }

  /*
     Set matrix rows corresponding to interior data.  We construct the
     matrix one row at a time.
  */
  for (i=mstart; i<mend; i++) {
    idx[0] = i-1; idx[1] = i; idx[2] = i+1;
    ierr = MatSetValues(A,1,&i,3,idx,v,INSERT_VALUES);CHKERRQ(ierr);
  }
  ierr = MatAssemblyBegin(A,MAT_FLUSH_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(A,MAT_FLUSH_ASSEMBLY);CHKERRQ(ierr);

  ierr = DMDAGetCorners(da,&mstart,0,0,&xm,0,0);CHKERRQ(ierr);
  mend = mstart + xm;
  if (!appctx->upwind) {
    /* advection -- centered differencing */
    v[0] = -.5*appctx->a/(appctx->h);
    v[1] = .5*appctx->a/(appctx->h);
    if (!mstart) {
      idx[0] = M-1; idx[1] = 1;
      ierr = MatSetValues(A,1,&mstart,2,idx,v,ADD_VALUES);CHKERRQ(ierr);
      mstart++;
    }

    if (mend == M){
      mend--;
      idx[0] = M-2; idx[1] = 0;
      ierr = MatSetValues(A,1,&mend,2,idx,v,ADD_VALUES);CHKERRQ(ierr);
    }

    for (i=mstart; i<mend; i++) {
      idx[0] = i-1; idx[1] = i+1;
      ierr = MatSetValues(A,1,&i,2,idx,v,ADD_VALUES);CHKERRQ(ierr);
    }
  } else {
    /* advection -- upwinding */
    v[0] = -appctx->a/(appctx->h);
    v[1] = appctx->a/(appctx->h);
    if (!mstart) {
      idx[0] = 0; idx[1] = 1;
      ierr = MatSetValues(A,1,&mstart,2,idx,v,ADD_VALUES);CHKERRQ(ierr);
      mstart++;
    }

    if (mend == M){
      mend--;
      idx[0] = M-1; idx[1] = 0;
      ierr = MatSetValues(A,1,&mend,2,idx,v,ADD_VALUES);CHKERRQ(ierr);
    }

    for (i=mstart; i<mend; i++) {
      idx[0] = i; idx[1] = i+1;
      ierr = MatSetValues(A,1,&i,2,idx,v,ADD_VALUES);CHKERRQ(ierr);
    }
  }


  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Complete the matrix assembly process and set some options
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  /*
     Assemble matrix, using the 2-step process:
       MatAssemblyBegin(), MatAssemblyEnd()
     Computations can be done while messages are in transition
     by placing code between these two statements.
  */
  ierr = MatAssemblyBegin(A,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(A,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);

  /*
     Set flag to indicate that the Jacobian matrix retains an identical
     nonzero structure throughout all timestepping iterations (although the
     values of the entries change). Thus, we can save some work in setting
     up the preconditioner (e.g., no need to redo symbolic factorization for
     ILU/ICC preconditioners).
      - If the nonzero structure of the matrix is different during
        successive linear solves, then the flag DIFFERENT_NONZERO_PATTERN
        must be used instead.  If you are unsure whether the matrix
        structure has changed or not, use the flag DIFFERENT_NONZERO_PATTERN.
      - Caution:  If you specify SAME_NONZERO_PATTERN, PETSc
        believes your assertion and does not check the structure
        of the matrix.  If you erroneously claim that the structure
        is the same when it actually is not, the new preconditioner
        will not function correctly.  Thus, use this optimization
        feature with caution!
  */
  *str = SAME_NONZERO_PATTERN;

  /*
     Set and option to indicate that we will never add a new nonzero location
     to the matrix. If we do, it will generate an error.
  */
  ierr = MatSetOption(A,MAT_NEW_NONZERO_LOCATION_ERR,PETSC_TRUE);CHKERRQ(ierr);

  return 0;
}
