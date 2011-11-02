static char help[] = "Stokes Problem in 2d.\n\
We solve the  Stokes problem in a 2D rectangular\n\
domain, using a parallel unstructured mesh (DMMESH) to discretize it.\n\
The command line options include:\n\
  -visc_model <name>, the viscosity model\n\n\n";

/*
 The variable-viscosity Stokes problem, which we discretize using the finite
element method on an unstructured mesh. The weak form equations are

  < \nabla v, \nabla u + {\nabla u}^T > + < \nabla\cdot v, p> + <v, f> = 0
  < q, \nabla\cdot v>                                                  = 0

We start with homogeneous Dirichlet conditions. We will expand this as the set
of test problems is developed.

Organization of Integration Routine:

Field Data:

  Sieve data is organized by point, and the closure operation just stacks up the
data from each sieve point in the closure. Thus, for a P_2-P_1 Stokes element, we
have

  cl{e} = {f e_0 e_1 e_2 v_0 v_1 v_2}
  x     = [u_{e_0} v_{e_0} u_{e_1} v_{e_1} u_{e_2} v_{e_2} u_{v_0} v_{v_0} p_{v_0} u_{v_1} v_{v_1} p_{v_1} u_{v_2} v_{v_2} p_{v_2}]

The problem here is that we would like to loop over each field separately for
integration. Therefore, the closure visitor in DMMeshVecGetClosure() reorders
the data so that each field is contiguous

  x'    = [u_{e_0} v_{e_0} u_{e_1} v_{e_1} u_{e_2} v_{e_2} u_{v_0} v_{v_0} u_{v_1} v_{v_1} u_{v_2} v_{v_2} p_{v_0} p_{v_1} p_{v_2}]

Likewise, DMMeshVecSetClosure() takes data partitioned by field, and correctly
puts it into the Sieve ordering.

Next Steps:

- Fix pressure BC
  - Input BC by field
  - Make constant vector on the pressure space (get a local P vector?)
  - Check that it is in the operator null space
- Create Jacobian
- Solve the system
  - Check error in the result
  - Refine and show convergence of correct order automatically (use femTest.py)
- Reply to Marc Hesse
  - Plan next steps with he and Avi
- Use quadratic elements for velocity
  - Check that disc error vanishes for exact solution
  - Redo slides from GUCASTutorial for these new examples
- Run in parallel
  - Check scaling with Mark
- Optimize closure operations
  - The visitor should not be created every time
  - The sizeWithBC operations can be precomputed (for a regular mesh)

- Function to get list (IS) of dofs constrained by Dirichlet conditions
- Flag for removing Dirichlet constrained unknowns
- Make an interface for PetscSection+IS to represent a partition, then you can sue this to distributed dependent objects
  - In general, we want IS+PetscSection to replace SectionInt
- Make new SNES F90 example that solves two-domain Laplace with different coefficient, reads from Exodus file

- Improve DMDA conversion to get edges
- Setup FV problem
*/

#include <petscdmmesh.h>
#include <petscsnes.h>

#ifndef PETSC_HAVE_SIEVE
#error This example requires Sieve. Reconfigure using --with-sieve
#endif

const int numFields     = 2;
const int numComponents = 3;

typedef struct {
  DM            dm;                /* REQUIRED in order to use SNES evaluation functions */
  PetscInt      debug;             /* The debugging level */
  PetscMPIInt   rank;              /* The process rank */
  PetscMPIInt   numProcs;          /* The number of processes */
  PetscLogEvent createMeshEvent, residualEvent, jacobianEvent, integrateCPUEvent;
  PetscBool     showResidual, showJacobian;
  /* Domain and mesh definition */
  PetscInt      dim;               /* The topological mesh dimension */
  PetscBool     interpolate;       /* Generate intermediate mesh elements */
  PetscReal     refinementLimit;   /* The largest allowable cell volume */
  char          partitioner[2048]; /* The graph partitioner */
  /* Element quadrature */
  PetscQuadrature q[numFields];
  /* GPU partitioning */
  PetscInt      numBatches;        /* The number of cell batches per kernel */
  PetscInt      numBlocks;         /* The number of concurrent blocks per kernel */
  /* Problem definition */
  void        (*f0Funcs[numFields])(PetscScalar u[], const PetscScalar gradU[], PetscScalar f0[]); /* The f_0 functions f0_u(x,y,z), and f0_p(x,y,z) */
  void        (*f1Funcs[numFields])(PetscScalar u[], const PetscScalar gradU[], PetscScalar f1[]); /* The f_1 functions f1_u(x,y,z), and f1_p(x,y,z) */
  PetscScalar (*exactFuncs[numComponents])(const PetscReal x[]);                                   /* The exact solution function u(x,y,z), v(x,y,z), and p(x,y,z) */
} AppCtx;

/*------------------------------------------------------------------------------
  This code can be generated using 'bin/pythonscripts/PetscGenerateFEMQuadrature.py 2 1 2 numComp src/snes/examples/tutorials/ex56.h'
 -----------------------------------------------------------------------------*/
#include "ex56.h"

PetscScalar zero(const PetscReal coords[]) {
  return 0.0;
}

/*
  Suppose we use exact solution:

    u = x^2 + y^2
    v = 2 x^2 - 2xy
    p = x + y - 1

  so that

    -\Delta u + \nabla p = <-4, -4> + <1, 1> = <-3, -3>
    \nabla \cdot u       = 2x - 2x = 0
*/
PetscScalar quadratic_u_2d(const PetscReal x[]) {
  return x[0]*x[0] + x[1]*x[1];
};

PetscScalar quadratic_v_2d(const PetscReal x[]) {
  return 2.0*x[0]*x[0] - 2.0*x[0]*x[1];
};

PetscScalar linear_p_2d(const PetscReal x[]) {
  return x[0] + x[1] - 1.0;
};

// gradP[d] = {p_x, p_y}
void f0_u(PetscScalar u[], const PetscScalar gradU[], PetscScalar f0[]) {
  const int dim   = 2;
  const int Ncomp = dim;

  for(int comp = 0; comp < Ncomp; ++comp) {
    f0[comp] = gradU[2*Ncomp+comp] + 3.0;
  }
}

// gradU[comp*dim+d] = {u_x, u_y, v_x, v_y}
void f1_u(PetscScalar u[], const PetscScalar gradU[], PetscScalar f1[]) {
  const int dim   = 2;
  const int Ncomp = dim;

  for(int comp = 0; comp < Ncomp; ++comp) {
    for(int d = 0; d < dim; ++d) {
      //f1[comp*dim+d] = 0.5*(gradU[comp*dim+d] + gradU[d*dim+comp]);
      f1[comp*dim+d] = gradU[comp*dim+d];
    }
  }
}

void f0_p(PetscScalar u[], const PetscScalar gradU[], PetscScalar f0[]) {
  const int dim = 2;

  f0[0] = 0.0;
  for(int d = 0; d < dim; ++d) {
    f0[0] += gradU[d*dim+d];
  }
}

void f1_p(PetscScalar u[], const PetscScalar gradU[], PetscScalar f1[]) {
  const int dim = 2;

  for(int d = 0; d < dim; ++d) {
    f1[d] = 0.0;
  }
}

#undef __FUNCT__
#define __FUNCT__ "ProcessOptions"
PetscErrorCode ProcessOptions(MPI_Comm comm, AppCtx *options) {
  PetscErrorCode ierr;

  PetscFunctionBegin;
  options->debug           = 0;
  options->dim             = 2;
  options->interpolate     = PETSC_FALSE;
  options->refinementLimit = 0.0;
  options->numBatches      = 1;
  options->numBlocks       = 1;
  options->showResidual    = PETSC_TRUE;
  options->showJacobian    = PETSC_TRUE;

  ierr = MPI_Comm_size(comm, &options->numProcs);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(comm, &options->rank);CHKERRQ(ierr);
  ierr = PetscOptionsBegin(comm, "", "Bratu Problem Options", "DMMESH");CHKERRQ(ierr);
  ierr = PetscOptionsInt("-debug", "The debugging level", "ex52.c", options->debug, &options->debug, PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsInt("-dim", "The topological mesh dimension", "ex52.c", options->dim, &options->dim, PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsBool("-interpolate", "Generate intermediate mesh elements", "ex52.c", options->interpolate, &options->interpolate, PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsReal("-refinement_limit", "The largest allowable cell volume", "ex52.c", options->refinementLimit, &options->refinementLimit, PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscStrcpy(options->partitioner, "chaco");CHKERRQ(ierr);
  ierr = PetscOptionsString("-partitioner", "The graph partitioner", "pflotran.cxx", options->partitioner, options->partitioner, 2048, PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsInt("-gpu_batches", "The number of cell batches per kernel", "ex52.c", options->numBatches, &options->numBatches, PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsInt("-gpu_blocks", "The number of concurrent blocks per kernel", "ex52.c", options->numBlocks, &options->numBlocks, PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsBool("-show_residual", "Output the residual for verification", "ex52.c", options->showResidual, &options->showResidual, PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsBool("-show_jacobian", "Output the Jacobian for verification", "ex52.c", options->showJacobian, &options->showJacobian, PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsEnd();

  ierr = PetscLogEventRegister("CreateMesh",    DM_CLASSID,   &options->createMeshEvent);CHKERRQ(ierr);
  ierr = PetscLogEventRegister("Residual",      SNES_CLASSID, &options->residualEvent);CHKERRQ(ierr);
  ierr = PetscLogEventRegister("IntegBatchCPU", SNES_CLASSID, &options->integrateCPUEvent);CHKERRQ(ierr);
  ierr = PetscLogEventRegister("Jacobian",      SNES_CLASSID, &options->jacobianEvent);CHKERRQ(ierr);
  PetscFunctionReturn(0);
};

#undef __FUNCT__
#define __FUNCT__ "CreateMesh"
PetscErrorCode CreateMesh(MPI_Comm comm, AppCtx *user, DM *dm)
{
  PetscInt       dim             = user->dim;
  PetscBool      interpolate     = user->interpolate;
  PetscReal      refinementLimit = user->refinementLimit;
  const char    *partitioner     = user->partitioner;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscLogEventBegin(user->createMeshEvent,0,0,0,0);CHKERRQ(ierr);
  ierr = DMMeshCreateBoxMesh(comm, dim, interpolate, dm);CHKERRQ(ierr);
  {
    DM refinedMesh     = PETSC_NULL;
    DM distributedMesh = PETSC_NULL;

    /* Refine mesh using a volume constraint */
    ierr = DMMeshRefine(*dm, refinementLimit, interpolate, &refinedMesh);CHKERRQ(ierr);
    if (refinedMesh) {
      ierr = DMDestroy(dm);CHKERRQ(ierr);
      *dm  = refinedMesh;
    }
    /* Distribute mesh over processes */
    ierr = DMMeshDistribute(*dm, partitioner, &distributedMesh);CHKERRQ(ierr);
    if (distributedMesh) {
      ierr = DMDestroy(dm);CHKERRQ(ierr);
      *dm  = distributedMesh;
    }
  }
  ierr = DMSetFromOptions(*dm);CHKERRQ(ierr);
  ierr = PetscLogEventEnd(user->createMeshEvent,0,0,0,0);CHKERRQ(ierr);
  user->dm = *dm;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "SetupQuadrature"
PetscErrorCode SetupQuadrature(AppCtx *user) {
  PetscFunctionBegin;
  switch(user->dim) {
  case 2:
    user->q[0].numQuadPoints = NUM_QUADRATURE_POINTS_0;
    user->q[0].quadPoints    = points_0;
    user->q[0].quadWeights   = weights_0;
    user->q[0].numBasisFuncs = NUM_BASIS_FUNCTIONS_0;
    user->q[0].numComponents = NUM_BASIS_COMPONENTS_0;
    user->q[0].basis         = Basis_0;
    user->q[0].basisDer      = BasisDerivatives_0;
    user->q[1].numQuadPoints = NUM_QUADRATURE_POINTS_1;
    user->q[1].quadPoints    = points_1;
    user->q[1].quadWeights   = weights_1;
    user->q[1].numBasisFuncs = NUM_BASIS_FUNCTIONS_1;
    user->q[1].numComponents = NUM_BASIS_COMPONENTS_1;
    user->q[1].basis         = Basis_1;
    user->q[1].basisDer      = BasisDerivatives_1;
    break;
  default:
    SETERRQ1(PETSC_COMM_WORLD, PETSC_ERR_ARG_OUTOFRANGE, "Invalid dimension %d", user->dim);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "SetupSection"
PetscErrorCode SetupSection(DM dm, AppCtx *user) {
  PetscSection   section;
  /* These can be generated using config/PETSc/FEM.py */
  PetscInt       dim          = user->dim;
  PetscInt       numDof_0[4]  = {dim, 0, 1, 0};
  PetscInt       numDof_1[6]  = {dim, 0, 0, 1, 0, 0};
  PetscInt       numDof_2[8]  = {dim, 0, 0, 0, 1, 0, 0, 0};
  PetscInt      *numDof       = PETSC_NULL;
  PetscInt       bcFields[1]  = {0};
  IS             bcPoints[1]  = {PETSC_NULL};
  PetscErrorCode ierr;

  PetscFunctionBegin;
  switch(user->dim) {
  case 1:
    numDof = numDof_0;break;
  case 2:
    numDof = numDof_1;break;
  case 3:
    numDof = numDof_2;break;
  default:
    SETERRQ1(PETSC_COMM_WORLD, PETSC_ERR_ARG_OUTOFRANGE, "Invalid spatial dimension %d", user->dim);
  }
  ierr = DMMeshGetStratumIS(dm, "marker", 1, &bcPoints[0]);CHKERRQ(ierr);
  ierr = DMMeshCreateSection(dm, dim, 2, numDof, 1, bcFields, bcPoints, &section);CHKERRQ(ierr);
  ierr = DMMeshSetSection(dm, "default", section);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "SetupExactSolution"
PetscErrorCode SetupExactSolution(AppCtx *user) {
  PetscFunctionBegin;
  switch(user->dim) {
  case 2:
    user->f0Funcs[0]    = f0_u;
    user->f0Funcs[1]    = f0_p;
    user->f1Funcs[0]    = f1_u;
    user->f1Funcs[1]    = f1_p;
    user->exactFuncs[0] = quadratic_u_2d;
    user->exactFuncs[1] = quadratic_v_2d;
    user->exactFuncs[2] = linear_p_2d;
    break;
  default:
    SETERRQ1(PETSC_COMM_WORLD, PETSC_ERR_ARG_OUTOFRANGE, "Invalid dimension %d", user->dim);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "ComputeError"
PetscErrorCode ComputeError(Vec X, PetscReal *error, AppCtx *user) {
  PetscScalar   (**exactFuncs)(const PetscReal []) = user->exactFuncs;
  const PetscInt   debug         = user->debug;
  const PetscInt   dim           = user->dim;
  Vec              localX;
  PetscReal       *coords, *v0, *J, *invJ, detJ;
  PetscReal        localError;
  PetscInt         cStart, cEnd;
  PetscErrorCode   ierr;

  PetscFunctionBegin;
  ierr = DMGetLocalVector(user->dm, &localX);CHKERRQ(ierr);
  ierr = DMGlobalToLocalBegin(user->dm, X, INSERT_VALUES, localX);CHKERRQ(ierr);
  ierr = DMGlobalToLocalEnd(user->dm, X, INSERT_VALUES, localX);CHKERRQ(ierr);
  ierr = PetscMalloc4(dim,PetscReal,&coords,dim,PetscReal,&v0,dim*dim,PetscReal,&J,dim*dim,PetscReal,&invJ);CHKERRQ(ierr);
  ierr = DMMeshGetHeightStratum(user->dm, 0, &cStart, &cEnd);CHKERRQ(ierr);
  for(PetscInt c = cStart; c < cEnd; ++c) {
    const PetscScalar *x;
    PetscReal          elemError = 0.0;

    ierr = DMMeshComputeCellGeometry(user->dm, c, v0, J, invJ, &detJ);CHKERRQ(ierr);
    if (detJ <= 0.0) {SETERRQ2(PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Invalid determinant %g for element %d", detJ, c);}
    ierr = DMMeshVecGetClosure(user->dm, localX, c, &x);CHKERRQ(ierr);

    for(PetscInt field = 0, comp = 0, fieldOffset = 0; field < numFields; ++field) {
      const PetscInt   numQuadPoints = user->q[field].numQuadPoints;
      const PetscReal *quadPoints    = user->q[field].quadPoints;
      const PetscReal *quadWeights   = user->q[field].quadWeights;
      const PetscInt   numBasisFuncs = user->q[field].numBasisFuncs;
      const PetscInt   numBasisComps = user->q[field].numComponents;
      const PetscReal *basis         = user->q[field].basis;

      if (debug) {
        char title[1024];
        ierr = PetscSNPrintf(title, 1023, "Solution for Field %d", field);CHKERRQ(ierr);
        ierr = DMMeshPrintCellVector(c, title, numBasisFuncs*numBasisComps, &x[fieldOffset]);CHKERRQ(ierr);
      }
      for(PetscInt q = 0; q < numQuadPoints; ++q) {
        for(PetscInt d = 0; d < dim; d++) {
          coords[d] = v0[d];
          for(PetscInt e = 0; e < dim; e++) {
            coords[d] += J[d*dim+e]*(quadPoints[q*dim+e] + 1.0);
          }
        }
        for(PetscInt fc = 0; fc < numBasisComps; ++fc) {
          const PetscScalar funcVal     = (*exactFuncs[comp+fc])(coords);
          PetscReal         interpolant = 0.0;
          for(int f = 0; f < numBasisFuncs; ++f) {
            const PetscInt fidx = f*numBasisComps+fc;
            interpolant += x[fieldOffset+fidx]*basis[q*numBasisFuncs+fidx];
          }
          if (debug) {ierr = PetscPrintf(PETSC_COMM_SELF, "    elem %d field %d error %g\n", c, field, PetscSqr(interpolant - funcVal)*quadWeights[q]*detJ);CHKERRQ(ierr);}
          elemError += PetscSqr(interpolant - funcVal)*quadWeights[q]*detJ;
        }
      }
      comp        += numBasisComps;
      fieldOffset += numBasisFuncs*numBasisComps;
    }
    if (debug) {ierr = PetscPrintf(PETSC_COMM_SELF, "  elem %d error %g\n", c, elemError);CHKERRQ(ierr);}
    localError += elemError;
  }
  ierr = PetscFree4(coords,v0,J,invJ);CHKERRQ(ierr);
  ierr = DMRestoreLocalVector(user->dm, &localX);CHKERRQ(ierr);
  ierr = MPI_Allreduce(&localError, error, 1, MPIU_REAL, MPI_SUM, PETSC_COMM_WORLD);CHKERRQ(ierr);
  *error = PetscSqrtReal(*error);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMComputeVertexFunction"
/*
  DMComputeVertexFunction - This calls a function with the coordinates of each vertex, and stores the result in a vector.

  Input Parameters:
+ dm - The DM
. mode - The insertion mode for values
. numComp - The number of components (functions)
- func - The coordinate functions to evaluate

  Output Parameter:
. X - vector
*/
PetscErrorCode DMComputeVertexFunction(DM dm, InsertMode mode, Vec X, PetscInt numComp, ...)
{
  Vec            localX, coordinates;
  PetscSection   section, cSection;
  PetscInt       vStart, vEnd;
  PetscScalar (**funcs)(const PetscReal []);
  PetscScalar   *values;
  va_list        ap;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = DMGetLocalVector(dm, &localX);CHKERRQ(ierr);
  ierr = DMMeshGetDepthStratum(dm, 0, &vStart, &vEnd);CHKERRQ(ierr);
  ierr = DMMeshGetDefaultSection(dm, &section);CHKERRQ(ierr);
  ierr = DMMeshGetCoordinateSection(dm, &cSection);CHKERRQ(ierr);
  ierr = DMMeshGetCoordinateVec(dm, &coordinates);CHKERRQ(ierr);
  ierr = PetscMalloc2(numComp,PetscScalar (*)(const PetscReal *),&funcs,numComp,PetscScalar,&values);CHKERRQ(ierr);
  va_start(ap, numComp);
  for(PetscInt c = 0; c < numComp; ++c) {
    funcs[c] = va_arg(ap, PetscScalar (*)(const PetscReal []));
  }
  va_end(ap);
  for(PetscInt v = vStart; v < vEnd; ++v) {
    PetscScalar *coords;

    ierr = VecGetValuesSection(coordinates, cSection, v, &coords);CHKERRQ(ierr);
    for(PetscInt c = 0; c < numComp; ++c) {
      values[c] = (*funcs[c])(coords);
    }
    ierr = VecSetValuesSection(localX, section, v, values, mode);CHKERRQ(ierr);
  }
  ierr = PetscFree2(funcs,values);CHKERRQ(ierr);
  ierr = VecDestroy(&coordinates);CHKERRQ(ierr);
  ierr = PetscSectionDestroy(&section);CHKERRQ(ierr);
  ierr = PetscSectionDestroy(&cSection);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD, "Local vertex function\n");CHKERRQ(ierr);
  ierr = VecView(localX, PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);

  ierr = DMLocalToGlobalBegin(dm, localX, mode, X);CHKERRQ(ierr);
  ierr = DMLocalToGlobalEnd(dm, localX, mode, X);CHKERRQ(ierr);
  ierr = DMRestoreLocalVector(dm, &localX);CHKERRQ(ierr);
#if 0
  /* This is necessary for higher order elements */
  ierr = MeshGetSectionReal(mesh, "exactSolution", &this->_options.exactSol.section);CHKERRQ(ierr);
  const Obj<PETSC_MESH_TYPE::real_section_type>& s = this->_mesh->getRealSection("exactSolution");
  this->_mesh->setupField(s);
  const Obj<PETSC_MESH_TYPE::label_sequence>&     cells       = this->_mesh->heightStratum(0);
  const Obj<PETSC_MESH_TYPE::real_section_type>&  coordinates = this->_mesh->getRealSection("coordinates");
  const int                                       localDof    = this->_mesh->sizeWithBC(s, *cells->begin());
  PETSC_MESH_TYPE::real_section_type::value_type *values      = new PETSC_MESH_TYPE::real_section_type::value_type[localDof];
  PetscReal                                      *v0          = new PetscReal[dim()];
  PetscReal                                      *J           = new PetscReal[dim()*dim()];
  PetscReal                                       detJ;
  ALE::ISieveVisitor::PointRetriever<PETSC_MESH_TYPE::sieve_type> pV((int) pow(this->_mesh->getSieve()->getMaxConeSize(), this->_mesh->depth())+1, true);

  for(PETSC_MESH_TYPE::label_sequence::iterator c_iter = cells->begin(); c_iter != cells->end(); ++c_iter) {
    ALE::ISieveTraversal<PETSC_MESH_TYPE::sieve_type>::orientedClosure(*this->_mesh->getSieve(), *c_iter, pV);
    const PETSC_MESH_TYPE::point_type *oPoints = pV.getPoints();
    const int                          oSize   = pV.getSize();
    int                                v       = 0;

    this->_mesh->computeElementGeometry(coordinates, *c_iter, v0, J, PETSC_NULL, detJ);
    for(int cl = 0; cl < oSize; ++cl) {
      const int pointDim = s->getFiberDimension(oPoints[cl]);

      if (pointDim) {
        for(int d = 0; d < pointDim; ++d, ++v) {
          values[v] = (*this->_options.integrate)(v0, J, v, initFunc);
        }
      }
    }
    this->_mesh->update(s, *c_iter, values);
    pV.clear();
  }
  delete [] values;
  delete [] v0;
  delete [] J;
#endif
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "IntegrateResidualBatchCPU"
PetscErrorCode IntegrateResidualBatchCPU(PetscInt Ne, PetscInt numFields, PetscInt field, const PetscScalar coefficients[], const PetscReal jacobianInverses[], const PetscReal jacobianDeterminants[], PetscQuadrature quad[], void (*f0_func)(PetscScalar u[], const PetscScalar gradU[], PetscScalar f0[]), void (*f1_func)(PetscScalar u[], const PetscScalar gradU[], PetscScalar f1[]), PetscScalar elemVec[], AppCtx *user) {
  const PetscInt debug   = user->debug;
  const PetscInt dim     = 2;
  PetscInt       cOffset = 0;
  PetscInt       eOffset = 0;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscLogEventBegin(user->integrateCPUEvent,0,0,0,0);CHKERRQ(ierr);
  for(PetscInt e = 0; e < Ne; ++e) {
    const PetscReal  detJ = jacobianDeterminants[e];
    const PetscReal *invJ = &jacobianInverses[e*dim*dim];
    const PetscInt   Nq   = quad[field].numQuadPoints;
    PetscScalar      f0[1*dim];     // Nq = 1
    PetscScalar      f1[1*dim*dim]; // Nq = 1

    assert(Nq == 1);
    if (debug > 1) {
      ierr = PetscPrintf(PETSC_COMM_SELF, "  detJ: %g\n", detJ);CHKERRQ(ierr);
      ierr = PetscPrintf(PETSC_COMM_SELF, "  invJ: %g %g %g %g\n", invJ[0], invJ[1], invJ[2], invJ[3]);CHKERRQ(ierr);
    }
    for(PetscInt q = 0; q < Nq; ++q) {
      if (debug) {ierr = PetscPrintf(PETSC_COMM_SELF, "  quad point %d\n", q);CHKERRQ(ierr);}
      PetscScalar u[dim+1]           = {0.0, 0.0, 0.0};
      PetscScalar gradU[dim*(dim+1)] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
      PetscInt    fOffset            = 0;
      PetscInt    dOffset            = cOffset;

      for(PetscInt f = 0; f < numFields; ++f) {
        const PetscInt   Nb       = quad[f].numBasisFuncs;
        const PetscInt   Ncomp    = quad[f].numComponents;
        const PetscReal *basis    = quad[f].basis;
        const PetscReal *basisDer = quad[f].basisDer;

        for(PetscInt b = 0; b < Nb; ++b) {
          for(int comp = 0; comp < Ncomp; ++comp) {
            const PetscInt cidx = b*Ncomp+comp;
            PetscScalar    realSpaceDer[dim];

            u[fOffset+comp] += coefficients[dOffset+cidx]*basis[q*Nb*Ncomp+cidx];
            for(PetscInt d = 0; d < dim; ++d) {
              realSpaceDer[d] = 0.0;
              for(PetscInt g = 0; g < dim; ++g) {
                realSpaceDer[d] += invJ[g*dim+d]*basisDer[(q*Nb*Ncomp+cidx)*dim+g];
              }
              gradU[(fOffset+comp)*dim+d] += coefficients[dOffset+cidx]*realSpaceDer[d];
            }
          }
        }
        if (debug > 1) {
          for(int comp = 0; comp < Ncomp; ++comp) {
            ierr = PetscPrintf(PETSC_COMM_SELF, "    u[%d,%d]: %g\n", f, comp, u[fOffset+comp]);CHKERRQ(ierr);
            for(PetscInt d = 0; d < dim; ++d) {
              ierr = PetscPrintf(PETSC_COMM_SELF, "    gradU[%d,%d]_%c: %g\n", f, comp, 'x'+d, gradU[(fOffset+comp)*dim+d]);CHKERRQ(ierr);
            }
          }
        }
        fOffset += Ncomp;
        dOffset += Nb*Ncomp;
      }
      const PetscInt   Ncomp       = quad[field].numComponents;
      const PetscReal *quadWeights = quad[field].quadWeights;

      f0_func(u, gradU, &f0[q*Ncomp]);
      for(int i = 0; i < Ncomp; ++i) {
        f0[q*Ncomp*dim+i] *= detJ*quadWeights[q];
      }
      f1_func(u, gradU, &f1[q*Ncomp*dim]);
      for(int i = 0; i < Ncomp*dim; ++i) {
        f1[q*Ncomp*dim+i] *= detJ*quadWeights[q];
      }
      if (debug > 1) {
        for(int c = 0; c < Ncomp; ++c) {
          ierr = PetscPrintf(PETSC_COMM_SELF, "    f0[%d]: %g\n", c, f0[q*Ncomp+c]);CHKERRQ(ierr);
          for(int d = 0; d < dim; ++d) {
            ierr = PetscPrintf(PETSC_COMM_SELF, "    f1[%d]_%c: %g\n", c, 'x'+d, f1[(q*Ncomp + c)*dim+d]);CHKERRQ(ierr);
          }
        }
      }
      if (q == Nq-1) {cOffset = dOffset;}
    }
    for(PetscInt f = 0; f < numFields; ++f) {
      const PetscInt   Nq       = quad[f].numQuadPoints;
      const PetscInt   Nb       = quad[f].numBasisFuncs;
      const PetscInt   Ncomp    = quad[f].numComponents;
      const PetscReal *basis    = quad[f].basis;
      const PetscReal *basisDer = quad[f].basisDer;

      if (f == field) {
      for(PetscInt b = 0; b < Nb; ++b) {
        for(int comp = 0; comp < Ncomp; ++comp) {
          const PetscInt cidx = b*Ncomp+comp;

          elemVec[eOffset+cidx] = 0.0;
          for(PetscInt q = 0; q < Nq; ++q) {
            PetscScalar realSpaceDer[dim];

            elemVec[eOffset+cidx] += basis[q*Nb*Ncomp+cidx]*f0[q*Ncomp+comp];
            for(PetscInt d = 0; d < dim; ++d) {
              realSpaceDer[d] = 0.0;
              for(PetscInt g = 0; g < dim; ++g) {
                realSpaceDer[d] += invJ[g*dim+d]*basisDer[(q*Nb*Ncomp+cidx)*dim+g];
              }
              elemVec[eOffset+cidx] += realSpaceDer[d]*f1[(q*Ncomp+comp)*dim+d];
            }
          }
        }
      }
      if (debug > 1) {
        for(PetscInt b = 0; b < Nb; ++b) {
          for(int comp = 0; comp < Ncomp; ++comp) {
            ierr = PetscPrintf(PETSC_COMM_SELF, "    elemVec[%d,%d]: %g\n", b, comp, elemVec[eOffset+b*Ncomp+comp]);CHKERRQ(ierr);
          }
        }
      }
      }
      eOffset += Nb*Ncomp;
    }
  }
  //ierr = PetscLogFlops((((2+(2+2*dim)*dim)*Ncomp*Nb+(2+2)*dim*Ncomp)*Nq + (2+2*dim)*dim*Nq*Ncomp*Nb)*Ne);CHKERRQ(ierr);
  ierr = PetscLogEventEnd(user->integrateCPUEvent,0,0,0,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
};

#undef __FUNCT__
#define __FUNCT__ "FormFunctionLocalBatch"
/*
 dm - The mesh
 X  - Local intput vector
 F  - Local output vector
 */
PetscErrorCode FormFunctionLocalBatch(DM dm, Vec X, Vec F, AppCtx *user)
{
  const PetscInt   debug     = user->debug;
  const PetscInt   dim       = user->dim;
  const PetscInt   numFields = 2;
  PetscReal       *coords, *v0, *J, *invJ, *detJ;
  PetscScalar     *elemVec;
  PetscInt         cStart, cEnd;
  PetscErrorCode   ierr;

  PetscFunctionBegin;
  ierr = PetscLogEventBegin(user->residualEvent,0,0,0,0);CHKERRQ(ierr);
  ierr = VecSet(F, 0.0);CHKERRQ(ierr);
  ierr = PetscMalloc3(dim,PetscReal,&coords,dim,PetscReal,&v0,dim*dim,PetscReal,&J);CHKERRQ(ierr);
  ierr = DMMeshGetHeightStratum(dm, 0, &cStart, &cEnd);CHKERRQ(ierr);
  const PetscInt numCells = cEnd - cStart;
  PetscInt       cellDof  = 0;
  PetscScalar   *u;

  for(PetscInt field = 0; field < numFields; ++field) {
    cellDof += user->q[field].numBasisFuncs*user->q[field].numComponents;
  }
  ierr = PetscMalloc4(numCells*cellDof,PetscScalar,&u,numCells*dim*dim,PetscReal,&invJ,numCells,PetscReal,&detJ,numCells*cellDof,PetscScalar,&elemVec);CHKERRQ(ierr);
  for(PetscInt c = cStart; c < cEnd; ++c) {
    const PetscScalar *x;

    ierr = DMMeshComputeCellGeometry(dm, c, v0, J, &invJ[c*dim*dim], &detJ[c]);CHKERRQ(ierr);
    ierr = DMMeshVecGetClosure(dm, X, c, &x);CHKERRQ(ierr);

    for(PetscInt i = 0; i < cellDof; ++i) {
      u[c*cellDof+i] = x[i];
    }
  }
  for(PetscInt field = 0; field < numFields; ++field) {
    const PetscInt numQuadPoints = user->q[field].numQuadPoints;
    const PetscInt numBasisFuncs = user->q[field].numBasisFuncs;
    void (*f0)(PetscScalar u[], const PetscScalar gradU[], PetscScalar f0[]) = user->f0Funcs[field];
    void (*f1)(PetscScalar u[], const PetscScalar gradU[], PetscScalar f1[]) = user->f1Funcs[field];
    // Conforming batches
    PetscInt blockSize  = numBasisFuncs*numQuadPoints;
    PetscInt numBlocks  = 1;
    PetscInt batchSize  = numBlocks * blockSize;
    PetscInt numBatches = user->numBatches;
    PetscInt numChunks  = numCells / (numBatches*batchSize);
    ierr = IntegrateResidualBatchCPU(numChunks*numBatches*batchSize, numFields, field, u, invJ, detJ, user->q, f0, f1, elemVec, user);CHKERRQ(ierr);
    // Remainder
    PetscInt numRemainder = numCells % (numBatches * batchSize);
    PetscInt offset       = numCells - numRemainder;
    ierr = IntegrateResidualBatchCPU(numRemainder, numFields, field, &u[offset*cellDof], &invJ[offset*dim*dim], &detJ[offset],
                                     user->q, f0, f1, &elemVec[offset*cellDof], user);CHKERRQ(ierr);
  }
  for(PetscInt c = cStart; c < cEnd; ++c) {
    if (debug) {ierr = DMMeshPrintCellVector(c, "Residual", cellDof, &elemVec[c*cellDof]);CHKERRQ(ierr);}
    ierr = DMMeshVecSetClosure(dm, F, c, &elemVec[c*cellDof], ADD_VALUES);CHKERRQ(ierr);
  }
  ierr = PetscFree4(u,invJ,detJ,elemVec);CHKERRQ(ierr);
  ierr = PetscFree3(coords,v0,J);CHKERRQ(ierr);
  if (user->showResidual) {
    ierr = PetscPrintf(PETSC_COMM_WORLD, "Residual:\n");CHKERRQ(ierr);
    for(int p = 0; p < user->numProcs; ++p) {
      if (p == user->rank) {ierr = VecView(F, PETSC_VIEWER_STDOUT_SELF);CHKERRQ(ierr);}
      ierr = PetscBarrier((PetscObject) dm);CHKERRQ(ierr);
    }
  }
  ierr = PetscLogEventEnd(user->residualEvent,0,0,0,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


#undef __FUNCT__
#define __FUNCT__ "IntegrateJacobianBatchCPU"
PetscErrorCode IntegrateJacobianBatchCPU(PetscInt Ne, PetscInt Nb, PetscInt Ncomp, const PetscScalar coefficients[], const PetscReal jacobianInverses[], const PetscReal jacobianDeterminants[], PetscInt Nq, const PetscReal quadPoints[], const PetscReal quadWeights[], const PetscReal basisTabulation[], const PetscReal basisDerTabulation[], PetscScalar elemMat[], AppCtx *user) {
}

#undef __FUNCT__
#define __FUNCT__ "FormJacobianLocalBatch"
/*
   FormJacobianLocal - Evaluates Jacobian matrix.
*/
PetscErrorCode FormJacobianLocalBatch(DM dm, Vec X, Mat Jac, AppCtx *user)
{
  const PetscInt   debug         = user->debug;
  const PetscInt   dim           = user->dim;
  const PetscInt   numQuadPoints = user->q[0].numQuadPoints;
  const PetscReal *quadPoints    = user->q[0].quadPoints;
  const PetscReal *quadWeights   = user->q[0].quadWeights;
  const PetscInt   numBasisFuncs = user->q[0].numBasisFuncs;
  const PetscInt   numBasisComps = user->q[0].numComponents;
  const PetscReal *basis         = user->q[0].basis;
  const PetscReal *basisDer      = user->q[0].basisDer;
  PetscReal       *v0, *J, *invJ, *detJ;
  PetscScalar     *elemMat;
  PetscInt         cStart, cEnd;
  PetscErrorCode   ierr;

  PetscFunctionBegin;
  ierr = MatZeroEntries(Jac);CHKERRQ(ierr);
  ierr = PetscMalloc2(dim,PetscReal,&v0,dim*dim,PetscReal,&J);CHKERRQ(ierr);
  ierr = DMMeshGetHeightStratum(user->dm, 0, &cStart, &cEnd);CHKERRQ(ierr);
  const PetscInt numCells = cEnd - cStart;
  PetscScalar   *u;

  ierr = PetscMalloc4(numCells*numBasisFuncs*numBasisComps,PetscScalar,&u,numCells*dim*dim,PetscReal,&invJ,numCells,PetscReal,&detJ,numCells*numBasisFuncs*numBasisComps*numBasisFuncs*numBasisComps,PetscScalar,&elemMat);CHKERRQ(ierr);
  ierr = PetscMemzero(elemMat, numBasisFuncs*numBasisComps*numBasisFuncs*numBasisComps * sizeof(PetscScalar));CHKERRQ(ierr);
  for(PetscInt c = cStart; c < cEnd; ++c) {
    const PetscScalar *x;

    ierr = DMMeshComputeCellGeometry(dm, c, v0, J, &invJ[c*dim*dim], &detJ[c]);CHKERRQ(ierr);
    if (detJ[c] <= 0.0) {SETERRQ2(PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Invalid determinant %g for element %d", detJ[c], c);}
    ierr = DMMeshVecGetClosure(dm, X, c, &x);CHKERRQ(ierr);

    for(int f = 0; f < numBasisFuncs*numBasisComps; ++f) {
      u[c*numBasisFuncs*numBasisComps+f] = x[f];
    }
  }
#if 0
    for(int q = 0; q < numQuadPoints; ++q) {
      PetscScalar fieldVal = 0.0;

      for(int f = 0; f < numBasisFuncs; ++f) {
        fieldVal += x[f]*basis[q*numBasisFuncs+f];
      }
      for(int f = 0; f < numBasisFuncs; ++f) {
        for(int d = 0; d < dim; ++d) {
          realSpaceTestDer[d] = 0.0;
          for(int e = 0; e < dim; ++e) {
            realSpaceTestDer[d] += invJ[e*dim+d]*basisDer[(q*numBasisFuncs+f)*dim+e];
          }
        }
        for(int g = 0; g < numBasisFuncs; ++g) {
          for(int d = 0; d < dim; ++d) {
            realSpaceBasisDer[d] = 0.0;
            for(int e = 0; e < dim; ++e) {
              realSpaceBasisDer[d] += invJ[e*dim+d]*basisDer[(q*numBasisFuncs+g)*dim+e];
            }
          }
          /* Linear term: -\Delta u */
          PetscScalar product = 0.0;
          for(int d = 0; d < dim; ++d) product += realSpaceTestDer[d]*realSpaceBasisDer[d];
          elemMat[f*numBasisFuncs+g] += product*quadWeights[q]*detJ[c];
        }
      }
    }
#endif
  // Conforming batches
  PetscInt blockSize  = numBasisFuncs*numQuadPoints;
  PetscInt numBlocks  = 1;
  PetscInt batchSize  = numBlocks * blockSize;
  PetscInt numBatches = user->numBatches;
  PetscInt numChunks  = numCells / (numBatches*batchSize);
  ierr = IntegrateJacobianBatchCPU(numChunks*numBatches*batchSize, numBasisFuncs, numBasisComps, u, invJ, detJ, numQuadPoints, quadPoints, quadWeights, basis, basisDer, elemMat, user);CHKERRQ(ierr);
  // Remainder
  PetscInt numRemainder = numCells % (numBatches * batchSize);
  PetscInt offset       = numCells - numRemainder;
  ierr = IntegrateJacobianBatchCPU(numRemainder, numBasisFuncs, numBasisComps, &u[offset*numBasisFuncs*numBasisComps], &invJ[offset*dim*dim], &detJ[offset],
                                   numQuadPoints, quadPoints, quadWeights, basis, basisDer, &elemMat[offset*numBasisFuncs*numBasisComps*numBasisFuncs*numBasisComps], user);CHKERRQ(ierr);
  for(PetscInt c = cStart; c < cEnd; ++c) {
    if (debug) {ierr = DMMeshPrintCellMatrix(c, "Jacobian", numBasisFuncs, numBasisFuncs, elemMat);CHKERRQ(ierr);}
    ierr = DMMeshMatSetClosure(user->dm, Jac, c, &elemMat[c*numBasisFuncs*numBasisComps*numBasisFuncs*numBasisComps], ADD_VALUES);CHKERRQ(ierr);
  }
  ierr = PetscFree4(u,invJ,detJ,elemMat);CHKERRQ(ierr);
  ierr = PetscFree2(v0,J);CHKERRQ(ierr);

  /* Assemble matrix, using the 2-step process:
       MatAssemblyBegin(), MatAssemblyEnd(). */
  ierr = MatAssemblyBegin(Jac, MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(Jac, MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  /* Tell the matrix we will never add a new nonzero location to the
     matrix. If we do, it will generate an error. */
  ierr = MatSetOption(Jac, MAT_NEW_NONZERO_LOCATION_ERR, PETSC_TRUE);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc, char **argv)
{
  SNES           snes;                 /* nonlinear solver */
  Vec            u,r;                  /* solution, residual vectors */
  Mat            A,J;                  /* Jacobian matrix */
  AppCtx         user;                 /* user-defined work context */
  PetscInt       its;                  /* iterations for convergence */
  PetscReal      error;                /* L_2 error in the solution */
  PetscErrorCode ierr;

  ierr = PetscInitialize(&argc, &argv, PETSC_NULL, help);CHKERRQ(ierr);
  ierr = ProcessOptions(PETSC_COMM_WORLD, &user);CHKERRQ(ierr);
  ierr = SNESCreate(PETSC_COMM_WORLD, &snes);CHKERRQ(ierr);
  ierr = CreateMesh(PETSC_COMM_WORLD, &user, &user.dm);CHKERRQ(ierr);
  ierr = SNESSetDM(snes, user.dm);CHKERRQ(ierr);

  ierr = SetupExactSolution(&user);CHKERRQ(ierr);
  ierr = SetupQuadrature(&user);CHKERRQ(ierr);
  ierr = SetupSection(user.dm, &user);CHKERRQ(ierr);

  ierr = DMCreateGlobalVector(user.dm, &u);CHKERRQ(ierr);
  ierr = VecDuplicate(u, &r);CHKERRQ(ierr);

  ierr = DMGetMatrix(user.dm, MATAIJ, &J);CHKERRQ(ierr);
  A    = J;
  ierr = SNESSetJacobian(snes, A, J, SNESMeshFormJacobian, &user);CHKERRQ(ierr);

  ierr = DMMeshSetLocalFunction(user.dm, (DMMeshLocalFunction1) FormFunctionLocalBatch);CHKERRQ(ierr);
  ierr = DMMeshSetLocalJacobian(user.dm, (DMMeshLocalJacobian1) FormJacobianLocalBatch);CHKERRQ(ierr);
  ierr = SNESSetFunction(snes, r, SNESMeshFormFunction, &user);CHKERRQ(ierr);
  ierr = SNESSetFromOptions(snes);CHKERRQ(ierr);

  ierr = DMComputeVertexFunction(user.dm, INSERT_ALL_VALUES, u, numComponents, user.exactFuncs[0], user.exactFuncs[1], user.exactFuncs[2]);CHKERRQ(ierr);
  //if (user.run == RUN_FULL)
  if (0) {
    ierr = DMComputeVertexFunction(user.dm, INSERT_VALUES, u, numComponents, zero, zero, zero);CHKERRQ(ierr);
    if (user.debug) {
      ierr = PetscPrintf(PETSC_COMM_WORLD, "Initial guess\n");CHKERRQ(ierr);
      ierr = VecView(u, PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
    }
    ierr = SNESSolve(snes, PETSC_NULL, u);CHKERRQ(ierr);
    ierr = SNESGetIterationNumber(snes, &its);CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_WORLD, "Number of SNES iterations = %D\n", its);CHKERRQ(ierr);
    ierr = ComputeError(u, &error, &user);CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_WORLD, "L_2 Error: %g\n", error);CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_WORLD, "Solution\n");CHKERRQ(ierr);
    ierr = VecView(u, PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
  } else {
    PetscReal res;

    /* Check discretization error */
    ierr = PetscPrintf(PETSC_COMM_WORLD, "Initial guess\n");CHKERRQ(ierr);
    ierr = VecView(u, PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
    ierr = ComputeError(u, &error, &user);CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_WORLD, "L_2 Error: %g\n", error);CHKERRQ(ierr);
    /* Check residual */
    ierr = SNESMeshFormFunction(snes, u, r, &user);CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_WORLD, "Initial Residual\n");CHKERRQ(ierr);
    ierr = VecView(r, PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
    ierr = VecNorm(r, NORM_2, &res);CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_WORLD, "L_2 Residual: %g\n", res);CHKERRQ(ierr);
  }

  if (0) {
    PetscViewer viewer;

    ierr = PetscViewerCreate(PETSC_COMM_WORLD, &viewer);CHKERRQ(ierr);
    /*ierr = PetscViewerSetType(viewer, PETSCVIEWERDRAW);CHKERRQ(ierr);
      ierr = PetscViewerDrawSetInfo(viewer, PETSC_NULL, "Solution", PETSC_DECIDE, PETSC_DECIDE, PETSC_DECIDE, PETSC_DECIDE);CHKERRQ(ierr); */
    ierr = PetscViewerSetType(viewer, PETSCVIEWERASCII);CHKERRQ(ierr);
    ierr = PetscViewerFileSetName(viewer, "ex12_sol.vtk");CHKERRQ(ierr);
    ierr = PetscViewerSetFormat(viewer, PETSC_VIEWER_ASCII_VTK);CHKERRQ(ierr);
    ierr = DMView(user.dm, viewer);CHKERRQ(ierr);
    ierr = VecView(u, viewer);CHKERRQ(ierr);
    ierr = PetscViewerDestroy(&viewer);CHKERRQ(ierr);
  }

  if (A != J) {
    ierr = MatDestroy(&A);CHKERRQ(ierr);
  }
  ierr = MatDestroy(&J);CHKERRQ(ierr);
  ierr = VecDestroy(&u);CHKERRQ(ierr);
  ierr = VecDestroy(&r);CHKERRQ(ierr);
  ierr = SNESDestroy(&snes);CHKERRQ(ierr);
  ierr = DMDestroy(&user.dm);CHKERRQ(ierr);
  ierr = PetscFinalize();
  return 0;
}
