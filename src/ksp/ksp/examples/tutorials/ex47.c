
/*
Laplacian in 3D. Modeled by the partial differential equation

   - Laplacian u = 1,0 < x,y,z < 1,

with boundary conditions

   u = 1 for x = 0, x = 1, y = 0, y = 1, z = 0, z = 1.

   This uses multigrid to solve the linear system

*/

static char help[] = "Solves 3D Laplacian using multigrid.\n\n";

#include "petscda.h"
#include "petscksp.h"
#include "petscdmmg.h"

extern PetscErrorCode ComputeMatrix(DMMG,Mat,Mat);
extern PetscErrorCode ComputeRHS(DMMG,Vec, PetscTruth);
extern PetscErrorCode Solve_FFT(DA, Vec, Vec);
extern PetscErrorCode CalculateXYStdDev(DA, Vec, Vec *);
extern PetscErrorCode VecViewCenterSingle(DA da, Vec v, PetscViewer viewer, const char name[], PetscInt i, PetscInt j);

PetscReal L[3] = {1.0, 1.0, 1.0};

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **argv)
{
  PetscErrorCode ierr;
  DMMG           *dmmg;
  PetscReal      norm, normTotal;
  DA             da;
  Vec            phi, phiRhs;

  ierr = PetscInitialize(&argc,&argv,(char *)0,help);CHKERRQ(ierr);

  ierr = DMMGCreate(PETSC_COMM_WORLD,3,PETSC_NULL,&dmmg);CHKERRQ(ierr);
  ierr = DACreate3d(PETSC_COMM_WORLD,DA_XYZPERIODIC,DA_STENCIL_STAR,-3,-3,-3,PETSC_DECIDE,PETSC_DECIDE,PETSC_DECIDE,1,1,0,0,0,&da);CHKERRQ(ierr);  
  ierr = DMMGSetDM(dmmg,(DM)da);CHKERRQ(ierr);
  ierr = DADestroy(da);CHKERRQ(ierr);

  ierr = DMMGSetKSP(dmmg,(PetscErrorCode (*)(DMMG, Vec)) ComputeRHS,ComputeMatrix);CHKERRQ(ierr);
  //ierr = DMMGSetNullSpace(dmmg, PETSC_TRUE, 0, PETSC_NULL);CHKERRQ(ierr);

  ierr = DMMGSetUp(dmmg);CHKERRQ(ierr);
  ierr = DMMGSolve(dmmg);CHKERRQ(ierr);

  ierr = VecDuplicate(DMMGGetx(dmmg), &phi);CHKERRQ(ierr);
  ierr = VecDuplicate(DMMGGetx(dmmg), &phiRhs);CHKERRQ(ierr);
  ierr = ComputeRHS(dmmg[0], phiRhs, PETSC_TRUE);CHKERRQ(ierr);
  ierr = Solve_FFT(DMMGGetDA(dmmg), phiRhs, phi);CHKERRQ(ierr);

  Vec       stddev;
  PetscReal s;
  PetscInt  p;

  ierr = CalculateXYStdDev(da, DMMGGetRHS(dmmg), &stddev);CHKERRQ(ierr);
  ierr = VecMax(stddev, &p, &s);CHKERRQ(ierr);
  if (s > 1.0e-10) {
    SETERRQ2(PETSC_COMM_SELF,PETSC_ERR_PLIB, "RHS Homogeneity violation, std deviation %g z %d", s, p);
  } else {
    PetscPrintf(PETSC_COMM_WORLD, "FD RHS Homogeneity verification\n");
    PetscPrintf(PETSC_COMM_WORLD, "    std deviation   %g\n", s);
  }
  ierr = VecDestroy(stddev);CHKERRQ(ierr);
  ierr = CalculateXYStdDev(da, DMMGGetx(dmmg), &stddev);CHKERRQ(ierr);
  ierr = VecMax(stddev, &p, &s);CHKERRQ(ierr);
  if (s > 1.0e-5) {
    SETERRQ2(PETSC_COMM_SELF,PETSC_ERR_PLIB, "FD Homogeneity violation, std deviation %g z %d", s, p);
  } else {
    PetscPrintf(PETSC_COMM_WORLD, "FD Solution Homogeneity verification\n");
    PetscPrintf(PETSC_COMM_WORLD, "    std deviation   %g\n", s);
  }
  ierr = VecDestroy(stddev);CHKERRQ(ierr);
  ierr = CalculateXYStdDev(da, phi, &stddev);CHKERRQ(ierr);
  ierr = VecMax(stddev, &p, &s);CHKERRQ(ierr);
  if (s > 1.0e-10) {
    SETERRQ2(PETSC_COMM_SELF,PETSC_ERR_PLIB, "FFT Homogeneity violation, std deviation %g z %d", s, p);
  } else {
    PetscPrintf(PETSC_COMM_WORLD, "FFT Solution Homogeneity verification\n");
    PetscPrintf(PETSC_COMM_WORLD, "    std deviation   %g\n", s);
  }
  ierr = VecDestroy(stddev);CHKERRQ(ierr);

  PetscInt N;
  Vec      tmp;
  ierr = VecGetSize(phi, &N);CHKERRQ(ierr);
  ierr = VecCreate(PETSC_COMM_WORLD, &tmp);CHKERRQ(ierr);
  ierr = VecSetSizes(tmp, PETSC_DECIDE, N);CHKERRQ(ierr);
  ierr = VecSetFromOptions(tmp);CHKERRQ(ierr);

  ierr = VecCopy(DMMGGetx(dmmg), tmp);CHKERRQ(ierr);
  ierr = VecView(tmp, PETSC_VIEWER_DRAW_WORLD);CHKERRQ(ierr);
  ierr = VecViewCenterSingle(da, DMMGGetx(dmmg), PETSC_VIEWER_DRAW_WORLD, "FD Solution", -1, -1);CHKERRQ(ierr);
  ierr = VecViewCenterSingle(da, DMMGGetx(dmmg), PETSC_VIEWER_STDOUT_WORLD, "FD Solution", -1, -1);CHKERRQ(ierr);
  ierr = MatMult(DMMGGetJ(dmmg),DMMGGetx(dmmg),DMMGGetr(dmmg));CHKERRQ(ierr);
  ierr = VecAXPY(DMMGGetr(dmmg),-1.0,DMMGGetRHS(dmmg));CHKERRQ(ierr);
  ierr = VecNorm(DMMGGetr(dmmg),NORM_2,&norm);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Residual norm %G\n",norm);CHKERRQ(ierr);

  ierr = VecCopy(phi, tmp);CHKERRQ(ierr);
  ierr = VecView(tmp, PETSC_VIEWER_DRAW_WORLD);CHKERRQ(ierr);
  ierr = VecViewCenterSingle(da, phi, PETSC_VIEWER_DRAW_WORLD, "FFT Solution", -1, -1);CHKERRQ(ierr);
  ierr = VecViewCenterSingle(da, phi, PETSC_VIEWER_STDOUT_WORLD, "FFT Solution", -1, -1);CHKERRQ(ierr);
  ierr = VecAXPY(phi,-1.0,DMMGGetx(dmmg));CHKERRQ(ierr);
  ierr = VecNorm(phi,NORM_2,&norm);CHKERRQ(ierr);
  ierr = VecNorm(DMMGGetx(dmmg),NORM_2,&normTotal);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Error norm (FFT vs. FD) %G %G\n",norm, norm/normTotal);CHKERRQ(ierr);

  ierr = VecCopy(phi, tmp);CHKERRQ(ierr);
  ierr = VecView(tmp, PETSC_VIEWER_DRAW_WORLD);CHKERRQ(ierr);
  ierr = VecViewCenterSingle(da, phi, PETSC_VIEWER_DRAW_WORLD, "Error", -1, -1);CHKERRQ(ierr);
  ierr = VecViewCenterSingle(da, phi, PETSC_VIEWER_STDOUT_WORLD, "Error", -1, -1);CHKERRQ(ierr);
  ierr = VecDestroy(tmp);CHKERRQ(ierr);

  ierr = DMMGDestroy(dmmg);CHKERRQ(ierr);
  ierr = PetscFinalize();CHKERRQ(ierr);
  return 0;
}

#undef __FUNCT__
#define __FUNCT__ "ComputeRHS"
PetscErrorCode ComputeRHS(DMMG dmmg,Vec b, PetscTruth withBC= PETSC_TRUE)
{
  DA             da = (DA) dmmg->dm;
  PetscInt       bathIndex;
  PetscScalar ***a;
  PetscScalar    sc;
  PetscInt       mx, my, mz, xm, ym, zm, xs, ys, zs, wallPos, i, j, k;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = DAGetInfo(da,0,&mx,&my,&mz,0,0,0,0,0,0,0);CHKERRQ(ierr);
  ierr = DAGetCorners(da,&xs,&ys,&zs,&xm,&ym,&zm);CHKERRQ(ierr);
  sc   = 1.0/((mx-1)*(my-1)*(mz-1));
  //wallPos = (mz-1)/20;
  wallPos = -1;
  bathIndex = mz/2;
  ierr = DAVecGetArray(da, b, &a);CHKERRQ(ierr);
  for(k = zs; k < zs+zm; ++k) {
    for(j = ys; j < ys+ym; ++j) {
      for(i = xs; i < xs+xm; ++i) {
        if (k == bathIndex && withBC) {
          a[k][j][i] = 0.0;
        } else {
          if (k > wallPos) {
            a[k][j][i] = sc;
          } else {
            a[k][j][i] = 0.0;
          }
        }
      }
    }
  }
  ierr = DAVecRestoreArray(da, b, &a);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
    
#undef __FUNCT__
#define __FUNCT__ "ComputeMatrix"
PetscErrorCode ComputeMatrix(DMMG dmmg,Mat jac,Mat B)
{
  DA             da = (DA)dmmg->dm;
  PetscInt       bathIndex;
  PetscErrorCode ierr;
  PetscInt       i,j,k,mx,my,mz,xm,ym,zm,xs,ys,zs;
  PetscScalar    v[7],Hx,Hy,Hz,HxHydHz,HyHzdHx,HxHzdHy;
  MatStencil     row,col[7];

  ierr = DAGetInfo(da,0,&mx,&my,&mz,0,0,0,0,0,0,0);CHKERRQ(ierr);  
  Hx = 1.0 / (PetscReal)(mx-1); Hy = 1.0 / (PetscReal)(my-1); Hz = 1.0 / (PetscReal)(mz-1);
  //Hx = L[0] / (PetscReal)(mx-1); Hy = L[1] / (PetscReal)(my-1); Hz = L[2] / (PetscReal)(mz-1);
  //HxHydHz = Hx*Hy/Hz; HxHzdHy = Hx*Hz/Hy; HyHzdHx = Hy*Hz/Hx;
  HxHydHz = 1.0/PetscSqr(Hz); HxHzdHy = 1.0/PetscSqr(Hy); HyHzdHx = 1.0/PetscSqr(Hx);
  ierr = DAGetCorners(da,&xs,&ys,&zs,&xm,&ym,&zm);CHKERRQ(ierr);
  bathIndex = mz/2;
  PetscFunctionBegin;
  for (k=zs; k<zs+zm; k++){
    for (j=ys; j<ys+ym; j++){
      for(i=xs; i<xs+xm; i++){
        row.i = i; row.j = j; row.k = k;
        if (k == bathIndex) {
          v[0] = 2.0*(HxHydHz + HxHzdHy + HyHzdHx);
          ierr = MatSetValuesStencil(B,1,&row,1,&row,v,INSERT_VALUES);CHKERRQ(ierr);
        } else {
          v[0] = -HxHydHz;col[0].i = i; col[0].j = j; col[0].k = k-1;
          v[1] = -HxHzdHy;col[1].i = i; col[1].j = j-1; col[1].k = k;
          v[2] = -HyHzdHx;col[2].i = i-1; col[2].j = j; col[2].k = k;
          v[3] = 2.0*(HxHydHz + HxHzdHy + HyHzdHx);col[3].i = row.i; col[3].j = row.j; col[3].k = row.k;
          v[4] = -HyHzdHx;col[4].i = i+1; col[4].j = j; col[4].k = k;
          v[5] = -HxHzdHy;col[5].i = i; col[5].j = j+1; col[5].k = k;
          v[6] = -HxHydHz;col[6].i = i; col[6].j = j; col[6].k = k+1;
          ierr = MatSetValuesStencil(B,1,&row,7,col,v,INSERT_VALUES);CHKERRQ(ierr);
        }
      }
    }
  }
  ierr = MatAssemblyBegin(B, MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(B, MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


#undef __FUNCT__
#define __FUNCT__ "Solve_FFT"
PetscErrorCode Solve_FFT(DA da, Vec rhs, Vec phi)
{
  PetscReal      h[3];
  PetscInt       dim[3];
  PetscReal      scale, sc;
  PetscScalar ***rhsHatArray;
  PetscScalar ***phiHatArray;
  Mat            F;
  Vec            rhsHat, phiHat;
  PetscInt       M, N, P, xm, ym, zm, xs, ys, zs;
  PetscErrorCode ierr;
	
  PetscFunctionBegin;
  ierr = DAGetInfo(da, 0, &M, &N, &P, 0, 0, 0, 0, 0, 0, 0);CHKERRQ(ierr);
  ierr = DAGetInfo(da, 0, &dim[2], &dim[1], &dim[0], 0, 0, 0, 0, 0, 0, 0);CHKERRQ(ierr);
  ierr = MatCreateSeqFFTW(PETSC_COMM_WORLD, 3, dim, &F);CHKERRQ(ierr);
  ierr = DAGetCorners(da, &xs, &ys, &zs, &xm, &ym, &zm);CHKERRQ(ierr);
  h[0] = L[0]/(M - 1);
  h[1] = L[1]/(N - 1);
  h[2] = L[2]/(P - 1);
  scale = 1.0/((PetscReal) M*N*P);
  sc    = (M-1)*(N-1)*(P-1);
  ierr = DAGetGlobalVector(da, &rhsHat);CHKERRQ(ierr);
  ierr = MatMult(F, rhs, rhsHat);CHKERRQ(ierr);
  ierr = DAGetGlobalVector(da, &phiHat);CHKERRQ(ierr);
  ierr = DAVecGetArray(da, rhsHat, &rhsHatArray);CHKERRQ(ierr);
  ierr = DAVecGetArray(da, phiHat, &phiHatArray);CHKERRQ(ierr);
  for(PetscInt k = zs; k < zs+zm; ++k) {
    PetscReal kz = k <= P/2 ? 2.0*M_PI*k/(P) : -2.0*M_PI*(P-k)/(P);

    for(PetscInt j = ys; j < ys+ym; ++j) {
      PetscReal ky = j <= N/2 ? 2.0*M_PI*j/(N) : -2.0*M_PI*(N-j)/(N);

      for(PetscInt i = xs; i < xs+xm; ++i) {
        PetscReal kx = i <= M/2 ? 2.0*M_PI*i/(M) : -2.0*M_PI*(M-i)/(M);
        PetscScalar charge = 0.0;

#if 0
        for(PetscInt sp = 0; sp < geomOptions->numSpecies; ++sp) {
          charge += (e*e/epsilon)*z[sp]*rhoHatArray[k][j][i].v[sp];
        }
#else
        charge = rhsHatArray[k][j][i];
#endif
        PetscReal denom = 2.0*((1.0-cos(kx))/PetscSqr(h[0]) + (1.0-cos(ky))/PetscSqr(h[1]) + (1.0-cos(kz))/PetscSqr(h[2]));
        // Omit the zeroth moment
        if (kx == 0 && ky == 0 && kz == 0) {
          phiHatArray[k][j][i] = 0.0;
        } else {
          phiHatArray[k][j][i] = charge/denom; // Changed units by scaling by e
        }
        if (PetscIsInfOrNanScalar(phiHatArray[k][j][i])) {
          SETERRQ4(PETSC_COMM_SELF,PETSC_ERR_FP, "Nan or inf at phiHat[%d][%d][%d]: %g ", k, j, i, PetscRealPart(phiHatArray[k][j][i]));
        }
      }
    }
  }
  ierr = DAVecRestoreArray(da, phiHat, &phiHatArray);CHKERRQ(ierr);
  ierr = MatMultTranspose(F, phiHat, phi);CHKERRQ(ierr);
  ierr = VecScale(phi, scale);CHKERRQ(ierr);
  ierr = DARestoreGlobalVector(da, &phiHat);CHKERRQ(ierr);
  ierr = DARestoreGlobalVector(da, &rhsHat);CHKERRQ(ierr);
  ierr = MatDestroy(F);CHKERRQ(ierr);

  // Force potential in the bath to be 0
  PetscInt       bathIndex[3] = {0, 0, P/2};
  PetscScalar ***phiArray;
  PetscScalar    bathPotential;

  ierr = DAVecGetArray(da, phi, &phiArray);CHKERRQ(ierr);
  bathPotential = phiArray[bathIndex[2]][bathIndex[1]][bathIndex[0]];
  ierr = DAVecRestoreArray(da, phi, &phiArray);CHKERRQ(ierr);
  ierr = VecShift(phi, -bathPotential);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "CalculateXYStdDev"
PetscErrorCode CalculateXYStdDev(DA da, Vec v, Vec *std) {
  DALocalInfo    info;
  MPI_Comm       comm;
  PetscScalar ***a;
  PetscScalar   *r;
  PetscErrorCode ierr;
  
  PetscFunctionBegin;
  ierr = PetscObjectGetComm((PetscObject) da, &comm);CHKERRQ(ierr);
  ierr = DAGetLocalInfo(da, &info);CHKERRQ(ierr);
  ierr = DAVecGetArray(da, v, &a);CHKERRQ(ierr);
    ierr = VecCreate(comm, std);CHKERRQ(ierr);
    ierr = VecSetSizes(*std, info.zm - info.zs, info.mz);CHKERRQ(ierr);
    ierr = VecSetFromOptions(*std);CHKERRQ(ierr);
    ierr = VecGetArray(*std, &r);CHKERRQ(ierr);
    for(PetscInt k = 0; k < info.mz; ++k) {
      PetscScalar avg = 0.0, var = 0.0;

      for(PetscInt j = 0; j < info.my; ++j) {
        for(PetscInt i = 0; i < info.mx; ++i) {
          avg += a[k][j][i];
        }
      }
      avg /= (info.mx*info.my);
      for(PetscInt j = 0; j < info.my; ++j) {
        for(PetscInt i = 0; i < info.mx; ++i) {
          var += PetscSqr(a[k][j][i] - avg);
        }
      }
      r[k] = sqrt(var);
	}
    ierr = VecRestoreArray(*std, &r);CHKERRQ(ierr);
  ierr = DAVecRestoreArray(da, v, &a);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "VecViewCenterSingle"
PetscErrorCode VecViewCenterSingle(DA da, Vec v, PetscViewer viewer, const char name[], PetscInt i, PetscInt j)
{
  DALocalInfo    info;
  MPI_Comm       comm;
  Vec            c;
  PetscScalar ***a;
  PetscScalar   *b;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscObjectGetComm((PetscObject) da, &comm);CHKERRQ(ierr);
  if (i < 0) {
    ierr = PetscPrintf(comm, "Viewing %s\n", name);
  } else {
    if (j < 0) {
      ierr = PetscPrintf(comm, "Viewing %s[%d]\n", name, i);
    } else {
      ierr = PetscPrintf(comm, "Viewing %s[%d,%d]\n", name, i, j);
    }
  }
  ierr = DAGetLocalInfo(da, &info);CHKERRQ(ierr);
  ierr = VecCreate(comm, &c);CHKERRQ(ierr);
  ierr = VecSetSizes(c, info.zm - info.zs, info.mz);CHKERRQ(ierr);
  ierr = VecSetFromOptions(c);CHKERRQ(ierr);
  ierr = DAVecGetArray(da, v, &a);CHKERRQ(ierr);
  ierr = VecGetArray(c, &b);CHKERRQ(ierr);
  for(PetscInt k = 0, i = info.mx/2, j = info.my/2; k < info.mz; ++k) {
    b[k] = a[k][j][i];
  }
  ierr = DAVecRestoreArray(da, v, &a);CHKERRQ(ierr);
  ierr = VecRestoreArray(c, &b);CHKERRQ(ierr);
  ierr = VecView(c, viewer);
  ierr = VecDestroy(c);
  PetscFunctionReturn(0);
}
