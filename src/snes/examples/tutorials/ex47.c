
static char help[] = "Solves -Laplacian u - lambda*exp(u) = 0,  0 < x < 1\n\n";

#include "petscda.h"
#include "petscsnes.h"

extern PetscErrorCode ComputeFunction(SNES,Vec,Vec,void*);
extern PetscErrorCode ComputeJacobian(SNES,Vec,Mat*,Mat*,MatStructure*,void*);

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **argv)
{
  SNES snes; Vec x,f; Mat J; DA da;

  PetscInitialize(&argc,&argv,(char *)0,help);

  DACreate1d(PETSC_COMM_WORLD,DA_NONPERIODIC,8,1,1,PETSC_NULL,&da);
  DACreateGlobalVector(da,&x); VecDuplicate(x,&f);
  DAGetMatrix(da,MATAIJ,&J);

  SNESCreate(PETSC_COMM_WORLD,&snes);
  SNESSetFunction(snes,f,ComputeFunction,da);
  SNESSetJacobian(snes,J,J,ComputeJacobian,da);
  SNESSetFromOptions(snes);
  SNESSolve(snes,PETSC_NULL,x);

  MatDestroy(J); VecDestroy(x); VecDestroy(f); SNESDestroy(snes); DADestroy(da);
  PetscFinalize();
  return 0;
}

PetscErrorCode ComputeFunction(SNES snes,Vec x,Vec f,void *ctx)
{
  PetscInt i,Mx,xs,xm; PetscScalar *xx,*ff,hx; DA da = (DA) ctx;

  DAGetInfo(da,PETSC_IGNORE,&Mx,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE);
  hx     = 1.0/(PetscReal)(Mx-1);
  DAVecGetArray(da,x,&xx);
  DAVecGetArray(da,f,&ff);
  DAGetCorners(da,&xs,PETSC_NULL,PETSC_NULL,&xm,PETSC_NULL,PETSC_NULL);

  for (i=xs; i<xs+xm; i++) {
    if (i == 0 || i == Mx-1) {
      ff[i] = xx[i]; 
    } else {
      ff[i] =  (2.0*xx[i] - xx[i-1] - xx[i+1])*dhx - sc*PetscExpScalar(xx[i]); 
    }
  }
  DAVecRestoreArray(da,x,&xx);
  DAVecRestoreArray(da,f,&ff);
  return 0;
} 

PetscErrorCode ComputeJacobian(SNES snes,Vec x,Mat *J,Mat *B,MatStructure *flag,void *ctx)
{
  DA da = (DA) ctx; PetscInt i,Mx,xm,xs; PetscScalar hx,*xx;

  DAGetInfo(da,PETSC_IGNORE,&Mx,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE);
  hx = 1.0/(PetscReal)(Mx-1);
  DAVecGetArray(da,x,&xx);
  DAGetCorners(da,&xs,PETSC_NULL,PETSC_NULL,&xm,PETSC_NULL,PETSC_NULL);

  for (i=xs; i<xs+xm; i++) {
    if (i == 0 || i == Mx-1) {
      MatSetValue(*J,i,i,1.0,INSERT_VALUES);
    } else {
      MatSetValue(*J,i,i-1,-hydhx,INSERT_VALUES);
      MatSetValue(*J,i,i,2.0*(hxdhy) - sc*PetscExpScalar(xx[i]),INSERT_VALUES);
      MatSetValue(*J,i,i+1,-hydhx,INSERT_VALUES);
    }
  }
  MatAssemblyBegin(*J,MAT_FINAL_ASSEMBLY);
  MatAssemblyEnd(*J,MAT_FINAL_ASSEMBLY);
  DAVecRestoreArray(da,x,&xx);
  *flag = SAME_NONZERO_PATTERN;
  return 0;
}
