/*$Id: ex24.c,v 1.13 2001/08/07 21:30:50 bsmith Exp $*/

static char help[] = "Tests CG, MINRES and SYMMLQ on symmetric matrices with SBAIJ format. The preconditioner ICC only works on sequential SBAIJ format. \n\n";

#include "petscsles.h"


#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **args)
{
  Mat         C;
  PetscScalar v,none = -1.0;
  int         i,j,I,J,ierr,Istart,Iend,N,m = 4,n = 4,rank,size,its,k;
  PetscReal   err_norm,res_norm;
  Vec         x,b,u,u_tmp;
  PetscRandom r;
  SLES        sles;
  PC          pc;          
  KSP         ksp;  

  PetscInitialize(&argc,&args,(char *)0,help);
  ierr = MPI_Comm_rank(PETSC_COMM_WORLD,&rank);CHKERRQ(ierr);
  ierr = MPI_Comm_size(PETSC_COMM_WORLD,&size);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-m",&m,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-n",&n,PETSC_NULL);CHKERRQ(ierr);
  N = m*n;


  /* Generate matrix */
  ierr = MatCreate(PETSC_COMM_WORLD,PETSC_DECIDE,PETSC_DECIDE,N,N,&C);CHKERRQ(ierr);
  ierr = MatSetFromOptions(C);CHKERRQ(ierr);
  ierr = MatGetOwnershipRange(C,&Istart,&Iend);CHKERRQ(ierr);
  for (I=Istart; I<Iend; I++) { 
    v = -1.0; i = I/n; j = I - i*n;  
    if (i>0)   {J = I - n; ierr = MatSetValues(C,1,&I,1,&J,&v,ADD_VALUES);CHKERRQ(ierr);}
    if (i<m-1) {J = I + n; ierr = MatSetValues(C,1,&I,1,&J,&v,ADD_VALUES);CHKERRQ(ierr);}
    if (j>0)   {J = I - 1; ierr = MatSetValues(C,1,&I,1,&J,&v,ADD_VALUES);CHKERRQ(ierr);}
    if (j<n-1) {J = I + 1; ierr = MatSetValues(C,1,&I,1,&J,&v,ADD_VALUES);CHKERRQ(ierr);}
    v = 4.0; ierr = MatSetValues(C,1,&I,1,&I,&v,ADD_VALUES);CHKERRQ(ierr);
  }
  ierr = MatAssemblyBegin(C,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(C,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);

  /* a shift can make C indefinite. Preconditioners LU, ILU (for BAIJ format) and ICC may fail */
  /* ierr = MatShift(&alpha, C);CHKERRQ(ierr); */
  /* ierr = MatView(C,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr); */

  /* Setup and solve for system */
    
  /* Create vectors.  */
  ierr = VecCreate(PETSC_COMM_WORLD,&x);CHKERRQ(ierr);
  ierr = VecSetSizes(x,PETSC_DECIDE,N);CHKERRQ(ierr);
  ierr = VecSetFromOptions(x);CHKERRQ(ierr);
  ierr = VecDuplicate(x,&b);CHKERRQ(ierr);
  ierr = VecDuplicate(x,&u);CHKERRQ(ierr);
  ierr = VecDuplicate(x,&u_tmp);CHKERRQ(ierr);

  /* Set exact solution u; then compute right-hand-side vector b. */   
  ierr = PetscRandomCreate(PETSC_COMM_SELF,RANDOM_DEFAULT,&r);CHKERRQ(ierr);
  ierr = VecSetRandom(r,u);CHKERRQ(ierr);
  ierr = PetscRandomDestroy(r);CHKERRQ(ierr); 
  
  ierr = MatMult(C,u,b);CHKERRQ(ierr); 

  for (k=0; k<3; k++){
    if (k == 0){                              /* CG  */
      ierr = SLESCreate(PETSC_COMM_WORLD,&sles);CHKERRQ(ierr);
      ierr = SLESSetOperators(sles,C,C,DIFFERENT_NONZERO_PATTERN);CHKERRQ(ierr);
      ierr = SLESGetKSP(sles,&ksp);CHKERRQ(ierr);
      ierr = PetscPrintf(PETSC_COMM_WORLD,"\n CG: \n");CHKERRQ(ierr);
      ierr = KSPSetType(ksp,KSPCG);CHKERRQ(ierr); 
    } else if (k == 1){                       /* MINRES */
      ierr = SLESCreate(PETSC_COMM_WORLD,&sles);CHKERRQ(ierr);
      ierr = SLESSetOperators(sles,C,C,DIFFERENT_NONZERO_PATTERN);CHKERRQ(ierr);
      ierr = SLESGetKSP(sles,&ksp);CHKERRQ(ierr);
      ierr = PetscPrintf(PETSC_COMM_WORLD,"\n MINRES: \n");CHKERRQ(ierr);
      ierr = KSPSetType(ksp,KSPMINRES);CHKERRQ(ierr); 
    } else {                                 /* SYMMLQ */
      ierr = SLESCreate(PETSC_COMM_WORLD,&sles);CHKERRQ(ierr);
      ierr = SLESSetOperators(sles,C,C,DIFFERENT_NONZERO_PATTERN);CHKERRQ(ierr);
      ierr = SLESGetKSP(sles,&ksp);CHKERRQ(ierr);
      ierr = PetscPrintf(PETSC_COMM_WORLD,"\n SYMMLQ: \n");CHKERRQ(ierr);
      ierr = KSPSetType(ksp,KSPSYMMLQ);CHKERRQ(ierr); 
    }

    ierr = SLESGetPC(sles,&pc);CHKERRQ(ierr);
    /* ierr = PCSetType(pc,PCICC);CHKERRQ(ierr); */
    ierr = PCSetType(pc,PCJACOBI);CHKERRQ(ierr); 
    ierr = KSPSetTolerances(ksp,1.e-7,PETSC_DEFAULT,PETSC_DEFAULT,PETSC_DEFAULT);CHKERRQ(ierr);

    /*
    Set runtime options, e.g.,
        -ksp_type <type> -pc_type <type> -ksp_monitor -ksp_rtol <rtol>
    These options will override those specified above as long as
    SLESSetFromOptions() is called _after_ any other customization
    routines.
    */
    ierr = SLESSetFromOptions(sles);CHKERRQ(ierr);   

    /* Solve linear system; */ 
    ierr = SLESSolve(sles,b,x,&its);CHKERRQ(ierr);
   
  /* Check error */
    ierr = VecCopy(u,u_tmp);CHKERRQ(ierr); 
    ierr = VecAXPY(&none,x,u_tmp);CHKERRQ(ierr);
    ierr = VecNorm(u_tmp,NORM_2,&err_norm);CHKERRQ(ierr);
    ierr = MatMult(C,x,u_tmp);CHKERRQ(ierr);  
    ierr = VecAXPY(&none,b,u_tmp);CHKERRQ(ierr);
    ierr = VecNorm(u_tmp,NORM_2,&res_norm);CHKERRQ(ierr);
  
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Number of iterations = %3d\n",its);CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Residual norm %A;",res_norm);CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_WORLD,"  Error norm %A.\n",err_norm);CHKERRQ(ierr);

    ierr = SLESDestroy(sles);CHKERRQ(ierr);
  }
   
  /* 
       Free work space.  All PETSc objects should be destroyed when they
       are no longer needed.
  */
  ierr = VecDestroy(b);CHKERRQ(ierr);
  ierr = VecDestroy(u);CHKERRQ(ierr); 
  ierr = VecDestroy(x);CHKERRQ(ierr);
  ierr = VecDestroy(u_tmp);CHKERRQ(ierr);  
  ierr = MatDestroy(C);CHKERRQ(ierr);

  ierr = PetscFinalize();CHKERRQ(ierr);
  return 0;
}


