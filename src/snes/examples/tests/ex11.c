/*$Id: ex11.c,v 1.38 1999/11/05 14:47:16 bsmith Exp bsmith $*/

static char help[] =
"This program demonstrates use of the SNES package to solve systems of\n\
nonlinear equations in parallel, using 2-dimensional distributed arrays.\n\
The 2-dim Bratu (SFI - solid fuel ignition) test problem is used, where\n\
analytic formation of the Jacobian is the default.  \n\
\n\
  Solves the linear systems via 2 level additive Schwarz \n\
\n\
The command line\n\
options are:\n\
  -par <parameter>, where <parameter> indicates the problem's nonlinearity\n\
     problem SFI:  <parameter> = Bratu parameter (0 <= par <= 6.81)\n\
  -Mx <xg>, where <xg> = number of grid points in the x-direction on coarse grid\n\
  -My <yg>, where <yg> = number of grid points in the y-direction on coarse grid\n\n";

/*  
    1) Solid Fuel Ignition (SFI) problem.  This problem is modeled by
    the partial differential equation
  
            -Laplacian u - lambda*exp(u) = 0,  0 < x,y < 1 ,
  
    with boundary conditions
   
             u = 0  for  x = 0, x = 1, y = 0, y = 1.
  
    A finite difference approximation with the usual 5-point stencil
    is used to discretize the boundary value problem to obtain a nonlinear 
    system of equations.

   The code has two cases for multilevel solver
     I. the coarse grid Jacobian is computed in parallel 
     II. the coarse grid Jacobian is computed sequentially on each processor
   in both cases the coarse problem is SOLVED redundantly.

*/

#include "snes.h"
#include "da.h"
#include "mg.h"

/* User-defined application contexts */

typedef struct {
   int        mx,my;            /* number grid points in x and y direction */
   Vec        localX,localF;    /* local vectors with ghost region */
   DA         da;
   Vec        x,b,r;            /* global vectors */
   Mat        J;                /* Jacobian on grid */
} GridCtx;

typedef struct {
   double      param;           /* test problem parameter */
   GridCtx     fine;
   GridCtx     coarse;
   SLES        sles_coarse;
   SLES        sles_fine;
   int         ratio;
   Mat         R;               /* restriction fine to coarse */
   Vec         Rscale;
   PetscTruth  redundant_build; /* build coarse matrix redundantly */
   Vec         localall;        /* contains entire coarse vector on each processor in NATURAL order*/
   VecScatter  tolocalall;      /* maps from parallel "global" coarse vector to localall */
   VecScatter  fromlocalall;    /* maps from localall vector back to global coarse vector */
} AppCtx;

#define COARSE_LEVEL 0
#define FINE_LEVEL   1

extern int FormFunction(SNES,Vec,Vec,void*), FormInitialGuess1(AppCtx*,Vec);
extern int FormJacobian(SNES,Vec,Mat*,Mat*,MatStructure*,void*);
extern int FormInterpolation(AppCtx *);

/*
      Mm_ratio - ration of grid lines between fine and coarse grids.
*/
#undef __FUNC__
#define __FUNC__ "main"
int main( int argc, char **argv )
{
  SNES          snes;                      
  AppCtx        user;                      
  int           ierr, its, N, n, Nx = PETSC_DECIDE, Ny = PETSC_DECIDE;
  int           size, nlocal,Nlocal;
  double        bratu_lambda_max = 6.81, bratu_lambda_min = 0.;
  SLES          sles;
  PC            pc;
  char          ctype[80];

  /*
      Initialize PETSc, note that default options in ex11options can be 
      overridden at the command line
  */
  PetscInitialize( &argc, &argv,"ex11options",help );

  user.ratio = 2;
  user.coarse.mx = 5; user.coarse.my = 5; user.param = 6.0;
  ierr = OptionsGetInt(PETSC_NULL,"-Mx",&user.coarse.mx,PETSC_NULL);CHKERRA(ierr);
  ierr = OptionsGetInt(PETSC_NULL,"-My",&user.coarse.my,PETSC_NULL);CHKERRA(ierr);
  ierr = OptionsGetInt(PETSC_NULL,"-ratio",&user.ratio,PETSC_NULL);CHKERRA(ierr);
  user.fine.mx = user.ratio*(user.coarse.mx-1)+1; user.fine.my = user.ratio*(user.coarse.my-1)+1;

  ierr = OptionsHasName(PETSC_NULL,"-redundant_build",&user.redundant_build);CHKERRA(ierr);
  if (user.redundant_build) {
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Building coarse Jacobian redundantly\n");CHKERRA(ierr);
  }

  ierr = PetscPrintf(PETSC_COMM_WORLD,"Coarse grid size %d by %d\n",user.coarse.mx,user.coarse.my);CHKERRA(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Fine grid size %d by %d\n",user.fine.mx,user.fine.my);CHKERRA(ierr);

  ierr = OptionsGetDouble(PETSC_NULL,"-par",&user.param,PETSC_NULL);CHKERRA(ierr);
  if (user.param >= bratu_lambda_max || user.param < bratu_lambda_min) {
    SETERRA(1,0,"Lambda is out of range");
  }
  n = user.fine.mx*user.fine.my; N = user.coarse.mx*user.coarse.my;

  ierr = MPI_Comm_size(PETSC_COMM_WORLD,&size);CHKERRA(ierr);
  ierr = OptionsGetInt(PETSC_NULL,"-Nx",&Nx,PETSC_NULL);CHKERRA(ierr);
  ierr = OptionsGetInt(PETSC_NULL,"-Ny",&Ny,PETSC_NULL);CHKERRA(ierr);

  /* Set up distributed array for fine grid */
  ierr = DACreate2d(PETSC_COMM_WORLD,DA_NONPERIODIC,DA_STENCIL_STAR,user.fine.mx,
                    user.fine.my,Nx,Ny,1,1,PETSC_NULL,PETSC_NULL,&user.fine.da);CHKERRA(ierr);
  ierr = DACreateGlobalVector(user.fine.da,&user.fine.x);CHKERRA(ierr);
  ierr = VecDuplicate(user.fine.x,&user.fine.r);CHKERRA(ierr);
  ierr = VecDuplicate(user.fine.x,&user.fine.b);CHKERRA(ierr);
  ierr = VecGetLocalSize(user.fine.x,&nlocal);CHKERRA(ierr);
  ierr = DACreateLocalVector(user.fine.da,&user.fine.localX);CHKERRA(ierr);
  ierr = VecDuplicate(user.fine.localX,&user.fine.localF);CHKERRA(ierr);
  ierr = MatCreateMPIAIJ(PETSC_COMM_WORLD,nlocal,nlocal,n,n,5,PETSC_NULL,3,PETSC_NULL,&user.fine.J);CHKERRA(ierr);

  /* Set up distributed array for coarse grid */
  ierr = DACreate2d(PETSC_COMM_WORLD,DA_NONPERIODIC,DA_STENCIL_STAR,user.coarse.mx,
                    user.coarse.my,Nx,Ny,1,1,PETSC_NULL,PETSC_NULL,&user.coarse.da);CHKERRA(ierr);
  ierr = DACreateGlobalVector(user.coarse.da,&user.coarse.x);CHKERRA(ierr);
  ierr = VecDuplicate(user.coarse.x,&user.coarse.b);CHKERRA(ierr);
  if (user.redundant_build) {
    /* Create scatter from parallel global numbering to redundant with natural ordering */
    ierr = DAGlobalToNaturalAllCreate(user.coarse.da,&user.tolocalall);CHKERRA(ierr);
    ierr = DANaturalAllToGlobalCreate(user.coarse.da,&user.fromlocalall);CHKERRA(ierr);
    ierr = VecCreateSeq(PETSC_COMM_SELF,N,&user.localall);CHKERRA(ierr);
    /* Create sequential matrix to hold entire coarse grid Jacobian on each processor */
    ierr = MatCreateSeqAIJ(PETSC_COMM_SELF,N,N,5,PETSC_NULL,&user.coarse.J);CHKERRA(ierr);
  } else {
    ierr = VecGetLocalSize(user.coarse.x,&Nlocal);CHKERRA(ierr);
    ierr = DACreateLocalVector(user.coarse.da,&user.coarse.localX);CHKERRA(ierr);
    ierr = VecDuplicate(user.coarse.localX,&user.coarse.localF);CHKERRA(ierr);
    /* We will compute the coarse Jacobian in parallel */
    ierr = MatCreateMPIAIJ(PETSC_COMM_WORLD,Nlocal,Nlocal,N,N,5,PETSC_NULL,3,PETSC_NULL,&user.coarse.J);CHKERRA(ierr);
  }

  /* Create nonlinear solver */
  ierr = SNESCreate(PETSC_COMM_WORLD,SNES_NONLINEAR_EQUATIONS,&snes);CHKERRA(ierr);

  /* provide user function and Jacobian */
  ierr = SNESSetFunction(snes,user.fine.b,FormFunction,&user);CHKERRA(ierr);
  ierr = SNESSetJacobian(snes,user.fine.J,user.fine.J,FormJacobian,&user);CHKERRA(ierr);

  /* set two level additive Schwarz preconditioner */
  ierr = SNESGetSLES(snes,&sles);CHKERRA(ierr);
  ierr = SLESGetPC(sles,&pc);CHKERRA(ierr);
  ierr = PCSetType(pc,PCMG);CHKERRA(ierr);
  ierr = MGSetLevels(pc,2);CHKERRA(ierr);
  ierr = MGSetType(pc,MGADDITIVE);CHKERRA(ierr);

  /* always solve the coarse problem redundantly with direct LU solver */
  ierr = OptionsSetValue("-coarse_pc_type","redundant");CHKERRA(ierr);
  ierr = OptionsSetValue("-coarse_redundant_pc_type","lu");CHKERRA(ierr);

  /* Create coarse level */
  ierr = MGGetCoarseSolve(pc,&user.sles_coarse);CHKERRA(ierr);
  ierr = SLESSetOptionsPrefix(user.sles_coarse,"coarse_");CHKERRA(ierr);
  ierr = SLESSetFromOptions(user.sles_coarse);CHKERRA(ierr);
  ierr = SLESSetOperators(user.sles_coarse,user.coarse.J,user.coarse.J,DIFFERENT_NONZERO_PATTERN);CHKERRA(ierr);
  ierr = MGSetX(pc,COARSE_LEVEL,user.coarse.x);CHKERRA(ierr); 
  ierr = MGSetRhs(pc,COARSE_LEVEL,user.coarse.b);CHKERRA(ierr); 
  if (user.redundant_build) {
    PC rpc;
    ierr = SLESGetPC(user.sles_coarse,&rpc);CHKERRA(ierr);
    ierr = PCRedundantSetScatter(rpc,user.tolocalall,user.fromlocalall);CHKERRA(ierr);
  }

  /* Create fine level */
  ierr = MGGetSmoother(pc,FINE_LEVEL,&user.sles_fine);CHKERRA(ierr);
  ierr = SLESSetOptionsPrefix(user.sles_fine,"fine_");CHKERRA(ierr);
  ierr = SLESSetFromOptions(user.sles_fine);CHKERRA(ierr);
  ierr = SLESSetOperators(user.sles_fine,user.fine.J,user.fine.J,DIFFERENT_NONZERO_PATTERN);CHKERRA(ierr);
  ierr = MGSetR(pc,FINE_LEVEL,user.fine.r);CHKERRA(ierr); 
  ierr = MGSetResidual(pc,FINE_LEVEL,MGDefaultResidual,user.fine.J);CHKERRA(ierr);

  /* Create interpolation between the levels */
  ierr = FormInterpolation(&user);CHKERRA(ierr);
  ierr = MGSetInterpolate(pc,FINE_LEVEL,user.R);CHKERRA(ierr);
  ierr = MGSetRestriction(pc,FINE_LEVEL,user.R);CHKERRA(ierr);

  /* Set options, then solve nonlinear system */
  ierr = SNESSetFromOptions(snes);CHKERRA(ierr);
  ierr = FormInitialGuess1(&user,user.fine.x);CHKERRA(ierr);
  ierr = SNESSolve(snes,user.fine.x,&its);CHKERRA(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Number of Newton iterations = %d\n", its );CHKERRA(ierr);

  /* Free data structures */
  if (user.redundant_build) {
    ierr = VecScatterDestroy(user.tolocalall);CHKERRA(ierr);
    ierr = VecScatterDestroy(user.fromlocalall);CHKERRA(ierr);
    ierr = VecDestroy(user.localall);CHKERRA(ierr);
  } else {
    ierr = VecDestroy(user.coarse.localX);CHKERRA(ierr);
    ierr = VecDestroy(user.coarse.localF);CHKERRA(ierr);
  }

  ierr = MatDestroy(user.fine.J);CHKERRA(ierr);
  ierr = VecDestroy(user.fine.x);CHKERRA(ierr);
  ierr = VecDestroy(user.fine.r);CHKERRA(ierr);
  ierr = VecDestroy(user.fine.b);CHKERRA(ierr);
  ierr = DADestroy(user.fine.da);CHKERRA(ierr);
  ierr = VecDestroy(user.fine.localX);CHKERRA(ierr);
  ierr = VecDestroy(user.fine.localF);CHKERRA(ierr);

  ierr = MatDestroy(user.coarse.J);CHKERRA(ierr);
  ierr = VecDestroy(user.coarse.x);CHKERRA(ierr);
  ierr = VecDestroy(user.coarse.b);CHKERRA(ierr);
  ierr = DADestroy(user.coarse.da);CHKERRA(ierr);

  ierr = SNESDestroy(snes);CHKERRA(ierr);
  ierr = MatDestroy(user.R);CHKERRA(ierr); 
  ierr = VecDestroy(user.Rscale);CHKERRA(ierr); 
  PetscFinalize();

  return 0;
}/* --------------------  Form initial approximation ----------------- */
#undef __FUNC__
#define __FUNC__ "FormInitialGuess1"
int FormInitialGuess1(AppCtx *user,Vec X)
{
  int     i, j, row, mx, my, ierr, xs, ys, xm, ym, Xm, Ym, Xs, Ys;
  double  one = 1.0, lambda, temp1, temp, hx, hy, hxdhy, hydhx,sc;
  Scalar  *x;
  Vec     localX = user->fine.localX;

  mx = user->fine.mx;       my = user->fine.my;            lambda = user->param;
  hx = one/(double)(mx-1);  hy = one/(double)(my-1);
  sc = hx*hy*lambda;        hxdhy = hx/hy;            hydhx = hy/hx;

  temp1 = lambda/(lambda + one);

  /* Get ghost points */
  ierr = DAGetCorners(user->fine.da,&xs,&ys,0,&xm,&ym,0);CHKERRQ(ierr);
  ierr = DAGetGhostCorners(user->fine.da,&Xs,&Ys,0,&Xm,&Ym,0);CHKERRQ(ierr);
  ierr = VecGetArray(localX,&x);CHKERRQ(ierr);

  /* Compute initial guess */
  for (j=ys; j<ys+ym; j++) {
    temp = (double)(PetscMin(j,my-j-1))*hy;
    for (i=xs; i<xs+xm; i++) {
      row = i - Xs + (j - Ys)*Xm; 
      if (i == 0 || j == 0 || i == mx-1 || j == my-1 ) {
        x[row] = 0.0; 
        continue;
      }
      x[row] = temp1*sqrt( PetscMin( (double)(PetscMin(i,mx-i-1))*hx,temp) ); 
    }
  }
  ierr = VecRestoreArray(localX,&x);CHKERRQ(ierr);

  /* Insert values into global vector */
  ierr = DALocalToGlobal(user->fine.da,localX,INSERT_VALUES,X);CHKERRQ(ierr);
  return 0;
}

 /* --------------------  Evaluate Function F(x) --------------------- */
#undef __FUNC__
#define __FUNC__ "FormFunction"
int FormFunction(SNES snes,Vec X,Vec F,void *ptr)
{
  AppCtx  *user = (AppCtx *) ptr;
  int     ierr, i, j, row, mx, my, xs, ys, xm, ym, Xs, Ys, Xm, Ym;
  double  two = 2.0, one = 1.0, lambda,hx, hy, hxdhy, hydhx,sc;
  Scalar  u, uxx, uyy, *x,*f;
  Vec     localX = user->fine.localX, localF = user->fine.localF; 

  mx = user->fine.mx;       my = user->fine.my;       lambda = user->param;
  hx = one/(double)(mx-1);  hy = one/(double)(my-1);
  sc = hx*hy*lambda;        hxdhy = hx/hy;            hydhx = hy/hx;

  /* Get ghost points */
  ierr = DAGlobalToLocalBegin(user->fine.da,X,INSERT_VALUES,localX);CHKERRQ(ierr);
  ierr = DAGlobalToLocalEnd(user->fine.da,X,INSERT_VALUES,localX);CHKERRQ(ierr);
  ierr = DAGetCorners(user->fine.da,&xs,&ys,0,&xm,&ym,0);CHKERRQ(ierr);
  ierr = DAGetGhostCorners(user->fine.da,&Xs,&Ys,0,&Xm,&Ym,0);CHKERRQ(ierr);
  ierr = VecGetArray(localX,&x);CHKERRQ(ierr);
  ierr = VecGetArray(localF,&f);CHKERRQ(ierr);

  /* Evaluate function */
  for (j=ys; j<ys+ym; j++) {
    row = (j - Ys)*Xm + xs - Xs - 1; 
    for (i=xs; i<xs+xm; i++) {
      row++;
      if (i > 0 && i < mx-1 && j > 0 && j < my-1) {
        u = x[row];
        uxx = (two*u - x[row-1] - x[row+1])*hydhx;
        uyy = (two*u - x[row-Xm] - x[row+Xm])*hxdhy;
        f[row] = uxx + uyy - sc*exp(u);
      } else if ((i > 0 && i < mx-1) || (j > 0 && j < my-1)){
        f[row] = .5*two*(hydhx + hxdhy)*x[row];
      } else {
        f[row] = .25*two*(hydhx + hxdhy)*x[row];
      }
    }
  }
  ierr = VecRestoreArray(localX,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(localF,&f);CHKERRQ(ierr);

  /* Insert values into global vector */
  ierr = DALocalToGlobal(user->fine.da,localF,INSERT_VALUES,F);CHKERRQ(ierr);
  PLogFlops(11*ym*xm);
  return 0; 
} 

/*
        Computes the part of the Jacobian associated with this processor 
*/
#undef __FUNC__
#define __FUNC__ "FormJacobian_Grid"
int FormJacobian_Grid(AppCtx *user,GridCtx *grid,Vec X, Mat *J,Mat *B)
{
  Mat     jac = *J;
  int     ierr, i, j, row, mx, my, xs, ys, xm, ym, Xs, Ys, Xm, Ym, col[5];
  int     nloc, *ltog, grow;
  Scalar  two = 2.0, one = 1.0, lambda, v[5], hx, hy, hxdhy, hydhx, sc, *x, value;
  Vec     localX = grid->localX;

  mx = grid->mx;            my = grid->my;            lambda = user->param;
  hx = one/(double)(mx-1);  hy = one/(double)(my-1);
  sc = hx*hy;               hxdhy = hx/hy;            hydhx = hy/hx;

  /* Get ghost points */
  ierr = DAGlobalToLocalBegin(grid->da,X,INSERT_VALUES,localX);CHKERRQ(ierr);
  ierr = DAGlobalToLocalEnd(grid->da,X,INSERT_VALUES,localX);CHKERRQ(ierr);
  ierr = DAGetCorners(grid->da,&xs,&ys,0,&xm,&ym,0);CHKERRQ(ierr);
  ierr = DAGetGhostCorners(grid->da,&Xs,&Ys,0,&Xm,&Ym,0);CHKERRQ(ierr);
  ierr = DAGetGlobalIndices(grid->da,&nloc,&ltog);CHKERRQ(ierr);
  ierr = VecGetArray(localX,&x);CHKERRQ(ierr);

  /* Evaluate Jacobian of function */
  for (j=ys; j<ys+ym; j++) {
    row = (j - Ys)*Xm + xs - Xs - 1; 
    for (i=xs; i<xs+xm; i++) {
      row++;
      grow = ltog[row];
      if (i > 0 && i < mx-1 && j > 0 && j < my-1) {
        v[0] = -hxdhy; col[0] = ltog[row - Xm];
        v[1] = -hydhx; col[1] = ltog[row - 1];
        v[2] = two*(hydhx + hxdhy) - sc*lambda*exp(x[row]); col[2] = grow;
        v[3] = -hydhx; col[3] = ltog[row + 1];
        v[4] = -hxdhy; col[4] = ltog[row + Xm];
        ierr = MatSetValues(jac,1,&grow,5,col,v,INSERT_VALUES);CHKERRQ(ierr);
      } else if ((i > 0 && i < mx-1) || (j > 0 && j < my-1)){
        value = .5*two*(hydhx + hxdhy);
        ierr = MatSetValues(jac,1,&grow,1,&grow,&value,INSERT_VALUES);CHKERRQ(ierr);
      } else {
        value = .25*two*(hydhx + hxdhy);
        ierr = MatSetValues(jac,1,&grow,1,&grow,&value,INSERT_VALUES);CHKERRQ(ierr);
      }
    }
  }
  ierr = MatAssemblyBegin(jac,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = VecRestoreArray(localX,&x);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(jac,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);

  return 0;
}

/*
        Computes the ENTIRE Jacobian associated with the ENTIRE grid sequentially
    This is for generating the coarse grid redundantly.

          This is BAD code duplication, since the bulk of this routine is the
    same as the routine above

       Note the numbering of the rows/columns is the NATURAL numbering
*/
#undef __FUNC__
#define __FUNC__ "FormJacobian_Coarse"
int FormJacobian_Coarse(AppCtx *user,GridCtx *grid,Vec X, Mat *J,Mat *B)
{
  Mat     jac = *J;
  int     ierr, i, j, row, mx, my, col[5];
  Scalar  two = 2.0, one = 1.0, lambda, v[5], hx, hy, hxdhy, hydhx, sc, *x, value;

  mx = grid->mx;            my = grid->my;            lambda = user->param;
  hx = one/(double)(mx-1);  hy = one/(double)(my-1);
  sc = hx*hy;               hxdhy = hx/hy;            hydhx = hy/hx;

  ierr = VecGetArray(X,&x);CHKERRQ(ierr);

  /* Evaluate Jacobian of function */
  for (j=0; j<my; j++) {
    row = j*mx - 1;
    for (i=0; i<mx; i++) {
      row++;
      if (i > 0 && i < mx-1 && j > 0 && j < my-1) {
        v[0] = -hxdhy; col[0] = row - mx;
        v[1] = -hydhx; col[1] = row - 1;
        v[2] = two*(hydhx + hxdhy) - sc*lambda*exp(x[row]); col[2] = row;
        v[3] = -hydhx; col[3] = row + 1;
        v[4] = -hxdhy; col[4] = row + mx;
        ierr = MatSetValues(jac,1,&row,5,col,v,INSERT_VALUES);CHKERRQ(ierr);
      } else if ((i > 0 && i < mx-1) || (j > 0 && j < my-1)){
        value = .5*two*(hydhx + hxdhy);
        ierr = MatSetValues(jac,1,&row,1,&row,&value,INSERT_VALUES);CHKERRQ(ierr);
      } else {
        value = .25*two*(hydhx + hxdhy);
        ierr = MatSetValues(jac,1,&row,1,&row,&value,INSERT_VALUES);CHKERRQ(ierr);
      }
    }
  }
  ierr = MatAssemblyBegin(jac,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = VecRestoreArray(X,&x);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(jac,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);

  return 0;
}

/* --------------------  Evaluate Jacobian F'(x) --------------------- */
#undef __FUNC__
#define __FUNC__ "FormJacobian"
int FormJacobian(SNES snes,Vec X,Mat *J,Mat *B,MatStructure *flag,void *ptr)
{
  AppCtx     *user = (AppCtx *) ptr;
  int        ierr;
  SLES       sles;
  PC         pc;
  PetscTruth ismg;

  *flag = SAME_NONZERO_PATTERN;
  ierr = FormJacobian_Grid(user,&user->fine,X,J,B);CHKERRQ(ierr);

  /* create coarse grid jacobian for preconditioner */
  ierr = SNESGetSLES(snes,&sles);CHKERRQ(ierr);
  ierr = SLESGetPC(sles,&pc);CHKERRQ(ierr);
  
  ierr = PetscTypeCompare((PetscObject)pc,PCMG,&ismg);CHKERRA(ierr);
  if (ismg) {

    ierr = SLESSetOperators(user->sles_fine,user->fine.J,user->fine.J,SAME_NONZERO_PATTERN);CHKERRA(ierr);

    /* restrict X to coarse grid */
    ierr = MatMult(user->R,X,user->coarse.x);CHKERRQ(ierr);
    ierr = VecPointwiseMult(user->Rscale,user->coarse.x,user->coarse.x);CHKERRQ(ierr);

    /* form Jacobian on coarse grid */
    if (user->redundant_build) {
      /* get copy of coarse X onto each processor */
      ierr = VecScatterBegin(user->coarse.x,user->localall,INSERT_VALUES,SCATTER_FORWARD,user->tolocalall);CHKERRQ(ierr);
      ierr = VecScatterEnd(user->coarse.x,user->localall,INSERT_VALUES,SCATTER_FORWARD,user->tolocalall);CHKERRQ(ierr);
      ierr = FormJacobian_Coarse(user,&user->coarse,user->localall,&user->coarse.J,&user->coarse.J);CHKERRQ(ierr);

    } else {
      /* coarse grid Jacobian computed in parallel */
      ierr = FormJacobian_Grid(user,&user->coarse,user->coarse.x,&user->coarse.J,&user->coarse.J);CHKERRQ(ierr);
    }
    ierr = SLESSetOperators(user->sles_coarse,user->coarse.J,user->coarse.J,SAME_NONZERO_PATTERN);CHKERRA(ierr);
  }

  return 0;
}


#undef __FUNC__
#define __FUNC__ "FormInterpolation"
/*
      Forms the interpolation (and restriction) operator from 
coarse grid to fine.
*/
int FormInterpolation(AppCtx *user)
{
  int      ierr,i,j,i_start,m_fine,j_start,m,n,M,Mx = user->coarse.mx,My = user->coarse.my,*idx;
  int      m_ghost,n_ghost,*idx_c,m_ghost_c,n_ghost_c,m_coarse;
  int      row,i_start_ghost,j_start_ghost,cols[4],mx = user->fine.mx, m_c,my = user->fine.my;
  int      c0,c1,c2,c3,nc,ratio = user->ratio,i_end,i_end_ghost,m_c_local,m_fine_local;
  int      i_c,j_c,i_start_c,j_start_c,n_c,i_start_ghost_c,j_start_ghost_c,col;
  Scalar   v[4],x,y, one = 1.0;
  Mat      mat;
  Vec      Rscale;
  
  ierr = DAGetCorners(user->fine.da,&i_start,&j_start,0,&m,&n,0);CHKERRQ(ierr);
  ierr = DAGetGhostCorners(user->fine.da,&i_start_ghost,&j_start_ghost,0,&m_ghost,&n_ghost,0);CHKERRQ(ierr);
  ierr = DAGetGlobalIndices(user->fine.da,PETSC_NULL,&idx);CHKERRQ(ierr);

  ierr = DAGetCorners(user->coarse.da,&i_start_c,&j_start_c,0,&m_c,&n_c,0);CHKERRQ(ierr);
  ierr = DAGetGhostCorners(user->coarse.da,&i_start_ghost_c,&j_start_ghost_c,0,&m_ghost_c,&n_ghost_c,0);CHKERRQ(ierr);
  ierr = DAGetGlobalIndices(user->coarse.da,PETSC_NULL,&idx_c);CHKERRQ(ierr);

  /* create interpolation matrix */
  ierr = VecGetLocalSize(user->fine.x,&m_fine_local);CHKERRQ(ierr);
  ierr = VecGetLocalSize(user->coarse.x,&m_c_local);CHKERRQ(ierr);
  ierr = VecGetSize(user->fine.x,&m_fine);CHKERRQ(ierr);
  ierr = VecGetSize(user->coarse.x,&m_coarse);CHKERRQ(ierr);
  ierr = MatCreateMPIAIJ(PETSC_COMM_WORLD,m_fine_local,m_c_local,m_fine,m_coarse,
                         5,0,3,0,&mat);CHKERRQ(ierr);

  /* loop over local fine grid nodes setting interpolation for those*/
  for ( j=j_start; j<j_start+n; j++ ) {
    for ( i=i_start; i<i_start+m; i++ ) {
      /* convert to local "natural" numbering and then to PETSc global numbering */
      row    = idx[m_ghost*(j-j_start_ghost) + (i-i_start_ghost)];

      i_c = (i/ratio);    /* coarse grid node to left of fine grid node */
      j_c = (j/ratio);    /* coarse grid node below fine grid node */

      /* 
         Only include those interpolation points that are truly 
         nonzero. Note this is very important for final grid lines
         in x and y directions; since they have no right/top neighbors
      */
      x  = ((double)(i - i_c*ratio))/((double)ratio);
      y  = ((double)(j - j_c*ratio))/((double)ratio);
      /* printf("i j %d %d %g %g\n",i,j,x,y); */
      nc = 0;
      /* one left and below; or we are right on it */
      if (j_c < j_start_ghost_c || j_c > j_start_ghost_c+n_ghost_c) {
        SETERRQ3(1,1,"Sorry j %d %d %d",j_c,j_start_ghost_c,j_start_ghost_c+n_ghost_c);
      }
      if (i_c < i_start_ghost_c || i_c > i_start_ghost_c+m_ghost_c) {
        SETERRQ3(1,1,"Sorry i %d %d %d",i_c,i_start_ghost_c,i_start_ghost_c+m_ghost_c);
      }
      col      = m_ghost_c*(j_c-j_start_ghost_c) + (i_c-i_start_ghost_c);
      cols[nc] = idx_c[col];
      v[nc++]  = x*y - x - y + 1.0;
      /* one right and below */
      if (i_c*ratio != i) { 
        cols[nc] = idx_c[col+1];
        v[nc++]  = -x*y + x;
      }
      /* one left and above */
      if (j_c*ratio != j) { 
        cols[nc] = idx_c[col+m_ghost_c];
        v[nc++]  = -x*y + y;
      }
      /* one right and above */
      if (j_c*ratio != j && i_c*ratio != i) { 
        cols[nc] = idx_c[col+m_ghost_c+1];
        v[nc++]  = x*y;
      }
      ierr = MatSetValues(mat,1,&row,nc,cols,v,INSERT_VALUES);CHKERRQ(ierr); 
    }
  }
  ierr = MatAssemblyBegin(mat,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(mat,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);

  ierr = VecDuplicate(user->coarse.x,&Rscale);CHKERRQ(ierr);
  ierr = VecSet(&one,user->fine.x);CHKERRQ(ierr);
  ierr = MatMultTrans(mat,user->fine.x,Rscale);CHKERRQ(ierr);
  ierr = VecReciprocal(Rscale);CHKERRQ(ierr);
  user->Rscale = Rscale;

  /*
     this is merely so we provide a restriction "matrix" to the multigrid code
     so we flip the roles of MatMult() and MatMultTrans() on the "interpolation/restriction"
     matrix
  */
  ierr = MatCreateShell(PETSC_COMM_WORLD,m_c_local,m_fine_local,m_coarse,m_fine,
                        mat,&user->R);CHKERRQ(ierr);
  ierr = MatShellSetOperation(user->R,MATOP_MULT,(void*)MatMult_Ours);CHKERRQ(ierr);
  ierr = MatShellSetOperation(user->R,MATOP_MULT_TRANS_ADD,(void*)MatMultTransAdd_Ours);CHKERRQ(ierr);
  ierr = MatShellSetOperation(user->R,MATOP_DESTROY,(void*)MatDestroy_Ours);CHKERRQ(ierr);
  return 0;
}




