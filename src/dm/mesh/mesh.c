 
#include "src/dm/mesh/meshimpl.h"   /*I      "petscmesh.h"   I*/
#include <Distribution.hh>
#include "src/dm/mesh/meshvtk.h"
#include "src/dm/mesh/meshpcice.h"
#include "src/dm/mesh/meshpylith.h"

/* Logging support */
PetscCookie PETSCDM_DLLEXPORT MESH_COOKIE = 0;
PetscEvent  Mesh_View = 0, Mesh_GetGlobalScatter = 0, Mesh_restrictVector = 0, Mesh_assembleVector = 0,
            Mesh_assembleVectorComplete = 0, Mesh_assembleMatrix = 0, Mesh_updateOperator = 0;

#undef __FUNCT__  
#define __FUNCT__ "MeshFinalize"
PetscErrorCode MeshFinalize()
{
  PetscFunctionBegin;
  ALE::Mesh::NumberingFactory::singleton(0, true);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MeshView_Sieve_Ascii"
PetscErrorCode MeshView_Sieve_Ascii(const ALE::Obj<ALE::Mesh>& mesh, PetscViewer viewer)
{
  PetscViewerFormat format;
  PetscErrorCode    ierr;

  PetscFunctionBegin;
  ierr = PetscViewerGetFormat(viewer, &format);CHKERRQ(ierr);
  if (format == PETSC_VIEWER_ASCII_VTK) {
    ierr = VTKViewer::writeHeader(viewer);CHKERRQ(ierr);
    ierr = VTKViewer::writeVertices(mesh, 0, viewer);CHKERRQ(ierr);
    ierr = VTKViewer::writeElements(mesh, 0, viewer);CHKERRQ(ierr);
  } else if (format == PETSC_VIEWER_ASCII_PYLITH) {
    char *filename;
    char  connectFilename[2048];
    char  coordFilename[2048];

    ierr = PetscViewerFileGetName(viewer, &filename);CHKERRQ(ierr);
    ierr = PetscViewerFileSetMode(viewer, FILE_MODE_WRITE);CHKERRQ(ierr);
    ierr = PetscStrcpy(connectFilename, filename);CHKERRQ(ierr);
    ierr = PetscStrcat(connectFilename, ".connect");CHKERRQ(ierr);
    ierr = PetscViewerFileSetName(viewer, connectFilename);CHKERRQ(ierr);
    ierr = ALE::PyLith::Viewer::writeElements(mesh, mesh->getIntSection("material"), viewer);CHKERRQ(ierr);
    ierr = PetscStrcpy(coordFilename, filename);CHKERRQ(ierr);
    ierr = PetscStrcat(coordFilename, ".coord");CHKERRQ(ierr);
    ierr = PetscViewerFileSetName(viewer, coordFilename);CHKERRQ(ierr);
    ierr = ALE::PyLith::Viewer::writeVertices(mesh, viewer);CHKERRQ(ierr);
    ierr = PetscViewerFileSetMode(viewer, FILE_MODE_READ);CHKERRQ(ierr);
    ierr = PetscExceptionTry1(PetscViewerFileSetName(viewer, filename), PETSC_ERR_FILE_OPEN);
    if (PetscExceptionValue(ierr)) {
      /* this means that a caller above me has also tryed this exception so I don't handle it here, pass it up */
    } else if (PetscExceptionCaught(ierr, PETSC_ERR_FILE_OPEN)) {
      ierr = 0;
    } 
    CHKERRQ(ierr);
  } else if (format == PETSC_VIEWER_ASCII_PYLITH_LOCAL) {
    PetscViewer connectViewer, coordViewer, splitViewer, tractionViewer;
    char       *filename;
    char        localFilename[2048];
    int         rank = mesh->commRank();

    ierr = PetscViewerFileGetName(viewer, &filename);CHKERRQ(ierr);

    sprintf(localFilename, "%s.%d.connect", filename, rank);
    ierr = PetscViewerCreate(PETSC_COMM_SELF, &connectViewer);CHKERRQ(ierr);
    ierr = PetscViewerSetType(connectViewer, PETSC_VIEWER_ASCII);CHKERRQ(ierr);
    ierr = PetscViewerSetFormat(connectViewer, PETSC_VIEWER_ASCII_PYLITH);CHKERRQ(ierr);
    ierr = PetscViewerFileSetName(connectViewer, localFilename);CHKERRQ(ierr);
    ierr = ALE::PyLith::Viewer::writeElementsLocal(mesh, mesh->getIntSection("material"), connectViewer);CHKERRQ(ierr);
    ierr = PetscViewerDestroy(connectViewer);CHKERRQ(ierr);

    sprintf(localFilename, "%s.%d.coord", filename, rank);
    ierr = PetscViewerCreate(PETSC_COMM_SELF, &coordViewer);CHKERRQ(ierr);
    ierr = PetscViewerSetType(coordViewer, PETSC_VIEWER_ASCII);CHKERRQ(ierr);
    ierr = PetscViewerSetFormat(coordViewer, PETSC_VIEWER_ASCII_PYLITH);CHKERRQ(ierr);
    ierr = PetscViewerFileSetName(coordViewer, localFilename);CHKERRQ(ierr);
    ierr = ALE::PyLith::Viewer::writeVerticesLocal(mesh, coordViewer);CHKERRQ(ierr);
    ierr = PetscViewerDestroy(coordViewer);CHKERRQ(ierr);

    if (mesh->hasPairSection("split")) {
      sprintf(localFilename, "%s.%d.split", filename, rank);
      ierr = PetscViewerCreate(PETSC_COMM_SELF, &splitViewer);CHKERRQ(ierr);
      ierr = PetscViewerSetType(splitViewer, PETSC_VIEWER_ASCII);CHKERRQ(ierr);
      ierr = PetscViewerSetFormat(splitViewer, PETSC_VIEWER_ASCII_PYLITH);CHKERRQ(ierr);
      ierr = PetscViewerFileSetName(splitViewer, localFilename);CHKERRQ(ierr);
      ierr = ALE::PyLith::Viewer::writeSplitLocal(mesh, mesh->getPairSection("split"), splitViewer);CHKERRQ(ierr);
      ierr = PetscViewerDestroy(splitViewer);CHKERRQ(ierr);
    }

    if (mesh->hasRealSection("traction")) {
      sprintf(localFilename, "%s.%d.traction", filename, rank);
      ierr = PetscViewerCreate(PETSC_COMM_SELF, &tractionViewer);CHKERRQ(ierr);
      ierr = PetscViewerSetType(tractionViewer, PETSC_VIEWER_ASCII);CHKERRQ(ierr);
      ierr = PetscViewerSetFormat(tractionViewer, PETSC_VIEWER_ASCII_PYLITH);CHKERRQ(ierr);
      ierr = PetscViewerFileSetName(tractionViewer, localFilename);CHKERRQ(ierr);
      ierr = ALE::PyLith::Viewer::writeTractionsLocal(mesh, mesh->getRealSection("traction"), tractionViewer);CHKERRQ(ierr);
      ierr = PetscViewerDestroy(tractionViewer);CHKERRQ(ierr);
    }
  } else if (format == PETSC_VIEWER_ASCII_PCICE) {
    char      *filename;
    char       coordFilename[2048];
    PetscTruth isConnect;
    size_t     len;

    ierr = PetscViewerFileGetName(viewer, &filename);CHKERRQ(ierr);
    ierr = PetscStrlen(filename, &len);CHKERRQ(ierr);
    ierr = PetscStrcmp(&(filename[len-5]), ".lcon", &isConnect);CHKERRQ(ierr);
    if (!isConnect) {
      SETERRQ1(PETSC_ERR_ARG_WRONG, "Invalid element connectivity filename: %s", filename);
    }
    ierr = ALE::PCICE::Viewer::writeElements(mesh, viewer);CHKERRQ(ierr);
    ierr = PetscStrncpy(coordFilename, filename, len-5);CHKERRQ(ierr);
    coordFilename[len-5] = '\0';
    ierr = PetscStrcat(coordFilename, ".nodes");CHKERRQ(ierr);
    ierr = PetscViewerFileSetName(viewer, coordFilename);CHKERRQ(ierr);
    ierr = ALE::PCICE::Viewer::writeVertices(mesh, viewer);CHKERRQ(ierr);
  } else {
    int dim = mesh->getDimension();

    ierr = PetscViewerASCIIPrintf(viewer, "Mesh in %d dimensions:\n", dim);CHKERRQ(ierr);
    for(int d = 0; d <= dim; d++) {
      // FIX: Need to globalize
      ierr = PetscViewerASCIIPrintf(viewer, "  %d %d-cells\n", mesh->getTopology()->depthStratum(0, d)->size(), d);CHKERRQ(ierr);
    }
  }
  ierr = PetscViewerFlush(viewer);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MeshView_Sieve"
PetscErrorCode MeshView_Sieve(const ALE::Obj<ALE::Mesh>& mesh, PetscViewer viewer)
{
  PetscTruth     iascii, isbinary, isdraw;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscTypeCompare((PetscObject) viewer, PETSC_VIEWER_ASCII, &iascii);CHKERRQ(ierr);
  ierr = PetscTypeCompare((PetscObject) viewer, PETSC_VIEWER_BINARY, &isbinary);CHKERRQ(ierr);
  ierr = PetscTypeCompare((PetscObject) viewer, PETSC_VIEWER_DRAW, &isdraw);CHKERRQ(ierr);

  if (iascii){
    ierr = MeshView_Sieve_Ascii(mesh, viewer);CHKERRQ(ierr);
  } else if (isbinary) {
    SETERRQ(PETSC_ERR_SUP, "Binary viewer not implemented for Mesh");
  } else if (isdraw){ 
    SETERRQ(PETSC_ERR_SUP, "Draw viewer not implemented for Mesh");
  } else {
    SETERRQ1(PETSC_ERR_SUP,"Viewer type %s not supported by this mesh object", ((PetscObject)viewer)->type_name);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MeshView"
/*@C
   MeshView - Views a Mesh object. 

   Collective on Mesh

   Input Parameters:
+  mesh - the mesh
-  viewer - an optional visualization context

   Notes:
   The available visualization contexts include
+     PETSC_VIEWER_STDOUT_SELF - standard output (default)
-     PETSC_VIEWER_STDOUT_WORLD - synchronized standard
         output where only the first processor opens
         the file.  All other processors send their 
         data to the first processor to print. 

   You can change the format the mesh is printed using the 
   option PetscViewerSetFormat().

   The user can open alternative visualization contexts with
+    PetscViewerASCIIOpen() - Outputs mesh to a specified file
.    PetscViewerBinaryOpen() - Outputs mesh in binary to a
         specified file; corresponding input uses MeshLoad()
.    PetscViewerDrawOpen() - Outputs mesh to an X window display

   The user can call PetscViewerSetFormat() to specify the output
   format of ASCII printed objects (when using PETSC_VIEWER_STDOUT_SELF,
   PETSC_VIEWER_STDOUT_WORLD and PetscViewerASCIIOpen).  Available formats include
+    PETSC_VIEWER_ASCII_DEFAULT - default, prints mesh information
-    PETSC_VIEWER_ASCII_VTK - outputs a VTK file describing the mesh

   Level: beginner

   Concepts: mesh^printing
   Concepts: mesh^saving to disk

.seealso: PetscViewerASCIIOpen(), PetscViewerDrawOpen(), PetscViewerBinaryOpen(),
          MeshLoad(), PetscViewerCreate()
@*/
PetscErrorCode PETSCDM_DLLEXPORT MeshView(Mesh mesh, PetscViewer viewer)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(mesh, MESH_COOKIE, 1);
  PetscValidType(mesh, 1);
  if (!viewer) viewer = PETSC_VIEWER_STDOUT_(mesh->comm);
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_COOKIE, 2);
  PetscCheckSameComm(mesh, 1, viewer, 2);

  ierr = PetscLogEventBegin(Mesh_View,0,0,0,0);CHKERRQ(ierr);
  ierr = (*mesh->ops->view)(mesh->m, viewer);CHKERRQ(ierr);
  ierr = PetscLogEventEnd(Mesh_View,0,0,0,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MeshLoad" 
/*@C
    MeshLoad - Create a mesh topology from the saved data in a viewer.

    Collective on Viewer

    Input Parameter:
.   viewer - The viewer containing the data

    Output Parameters:
.   mesh - the mesh object

    Level: advanced

.seealso MeshView()

@*/
PetscErrorCode PETSCDM_DLLEXPORT MeshLoad(PetscViewer viewer, Mesh *mesh)
{
  SETERRQ(PETSC_ERR_SUP, "");
}

#undef __FUNCT__  
#define __FUNCT__ "MeshGetMesh"
/*@C
    MeshGetMesh - Gets the internal mesh object

    Not collective

    Input Parameter:
.    mesh - the mesh object

    Output Parameter:
.    m - the internal mesh object
 
    Level: advanced

.seealso MeshCreate(), MeshSetMesh()

@*/
PetscErrorCode PETSCDM_DLLEXPORT MeshGetMesh(Mesh mesh, ALE::Obj<ALE::Mesh>& m)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(mesh, MESH_COOKIE, 1);
  m = mesh->m;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MeshSetMesh"
/*@C
    MeshSetMesh - Sets the internal mesh object

    Not collective

    Input Parameters:
+    mesh - the mesh object
-    m - the internal mesh object
 
    Level: advanced

.seealso MeshCreate(), MeshGetMesh()

@*/
PetscErrorCode PETSCDM_DLLEXPORT MeshSetMesh(Mesh mesh, const ALE::Obj<ALE::Mesh>& m)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(mesh, MESH_COOKIE, 1);
  mesh->m = m;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MeshGetMatrix" 
/*@C
    MeshGetMatrix - Creates a matrix with the correct parallel layout required for 
      computing the Jacobian on a function defined using the informatin in Mesh.

    Collective on Mesh

    Input Parameter:
+   mesh - the mesh object
-   mtype - Supported types are MATSEQAIJ, MATMPIAIJ, MATSEQBAIJ, MATMPIBAIJ, MATSEQSBAIJ, MATMPISBAIJ,
            or any type which inherits from one of these (such as MATAIJ, MATLUSOL, etc.).

    Output Parameters:
.   J  - matrix with the correct nonzero preallocation
        (obviously without the correct Jacobian values)

    Level: advanced

    Notes: This properly preallocates the number of nonzeros in the sparse matrix so you 
       do not need to do it yourself.

.seealso ISColoringView(), ISColoringGetIS(), MatFDColoringCreate(), DASetBlockFills()

@*/
PetscErrorCode PETSCDM_DLLEXPORT MeshGetMatrix(Mesh mesh, MatType mtype,Mat *J)
{
  ALE::Obj<ALE::Mesh> m;
#if 0
  ISLocalToGlobalMapping lmap;
  PetscInt              *globals,rstart,i;
#endif
  PetscInt               localSize;
  PetscErrorCode         ierr;

  PetscFunctionBegin;
  ierr = MeshGetMesh(mesh, m);CHKERRQ(ierr);
  //localSize = m->getRealSection("u")->getGlobalOrder()->getSize(ALE::Mesh::field_type::patch_type());

  ierr = MatCreate(mesh->comm,J);CHKERRQ(ierr);
  ierr = MatSetSizes(*J,localSize,localSize,PETSC_DETERMINE,PETSC_DETERMINE);CHKERRQ(ierr);
  ierr = MatSetType(*J,mtype);CHKERRQ(ierr);
  ierr = MatSetBlockSize(*J,1);CHKERRQ(ierr);
  ierr = MatSeqAIJSetPreallocation(*J,mesh->d_nz,mesh->d_nnz);CHKERRQ(ierr);
  ierr = MatMPIAIJSetPreallocation(*J,mesh->d_nz,mesh->d_nnz,mesh->o_nz,mesh->o_nnz);CHKERRQ(ierr);
  ierr = MatSeqBAIJSetPreallocation(*J,mesh->bs,mesh->d_nz,mesh->d_nnz);CHKERRQ(ierr);
  ierr = MatMPIBAIJSetPreallocation(*J,mesh->bs,mesh->d_nz,mesh->d_nnz,mesh->o_nz,mesh->o_nnz);CHKERRQ(ierr);

#if 0
  ierr = PetscMalloc((mesh->n+mesh->Nghosts+1)*sizeof(PetscInt),&globals);CHKERRQ(ierr);
  ierr = MatGetOwnershipRange(*J,&rstart,PETSC_NULL);CHKERRQ(ierr);
  for (i=0; i<mesh->n; i++) {
    globals[i] = rstart + i;
  }
  ierr = PetscMemcpy(globals+mesh->n,mesh->ghosts,mesh->Nghosts*sizeof(PetscInt));CHKERRQ(ierr);
  ierr = ISLocalToGlobalMappingCreate(PETSC_COMM_SELF,mesh->n+mesh->Nghosts,globals,&lmap);CHKERRQ(ierr);
  ierr = PetscFree(globals);CHKERRQ(ierr);
  ierr = MatSetLocalToGlobalMapping(*J,lmap);CHKERRQ(ierr);
  ierr = ISLocalToGlobalMappingDestroy(lmap);CHKERRQ(ierr);
#endif
  PetscFunctionReturn(0);
} 

#undef __FUNCT__  
#define __FUNCT__ "MeshSetGhosts"
/*@C
    MeshSetGhosts - Sets the global indices of other processes elements that will
      be ghosts on this process

    Not Collective

    Input Parameters:
+    mesh - the Mesh object
.    bs - block size
.    nlocal - number of local (non-ghost) entries
.    Nghosts - number of ghosts on this process
-    ghosts - indices of all the ghost points

    Level: advanced

.seealso MeshDestroy(), MeshCreateGlobalVector(), MeshGetGlobalIndices()

@*/
PetscErrorCode PETSCDM_DLLEXPORT MeshSetGhosts(Mesh mesh,PetscInt bs,PetscInt nlocal,PetscInt Nghosts,const PetscInt ghosts[])
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidPointer(mesh,1);
  ierr = PetscFree(mesh->ghosts);CHKERRQ(ierr);
  ierr = PetscMalloc((1+Nghosts)*sizeof(PetscInt),&mesh->ghosts);CHKERRQ(ierr);
  ierr = PetscMemcpy(mesh->ghosts,ghosts,Nghosts*sizeof(PetscInt));CHKERRQ(ierr);
  mesh->bs      = bs;
  mesh->n       = nlocal;
  mesh->Nghosts = Nghosts;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MeshSetPreallocation"
/*@C
    MeshSetPreallocation - sets the matrix memory preallocation for matrices computed by Mesh

    Not Collective

    Input Parameters:
+    mesh - the Mesh object
.    d_nz - maximum number of nonzeros in any row of diagonal block
.    d_nnz - number of nonzeros in each row of diagonal block
.    o_nz - maximum number of nonzeros in any row of off-diagonal block
.    o_nnz - number of nonzeros in each row of off-diagonal block


    Level: advanced

.seealso MeshDestroy(), MeshCreateGlobalVector(), MeshGetGlobalIndices(), MatMPIAIJSetPreallocation(),
         MatMPIBAIJSetPreallocation()

@*/
PetscErrorCode PETSCDM_DLLEXPORT MeshSetPreallocation(Mesh mesh,PetscInt d_nz,const PetscInt d_nnz[],PetscInt o_nz,const PetscInt o_nnz[])
{
  PetscFunctionBegin;
  PetscValidPointer(mesh,1);
  mesh->d_nz  = d_nz;
  mesh->d_nnz = (PetscInt*)d_nnz;
  mesh->o_nz  = o_nz;
  mesh->o_nnz = (PetscInt*)o_nnz;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MeshCreate"
/*@C
    MeshCreate - Creates a DM object, used to manage data for an unstructured problem
    described by a Sieve.

    Collective on MPI_Comm

    Input Parameter:
.   comm - the processors that will share the global vector

    Output Parameters:
.   mesh - the mesh object

    Level: advanced

.seealso MeshDestroy(), MeshCreateGlobalVector(), MeshGetGlobalIndices()

@*/
PetscErrorCode PETSCDM_DLLEXPORT MeshCreate(MPI_Comm comm,Mesh *mesh)
{
  PetscErrorCode ierr;
  Mesh         p;

  PetscFunctionBegin;
  PetscValidPointer(mesh,2);
  *mesh = PETSC_NULL;
#ifndef PETSC_USE_DYNAMIC_LIBRARIES
  ierr = DMInitializePackage(PETSC_NULL);CHKERRQ(ierr);
#endif

  ierr = PetscHeaderCreate(p,_p_Mesh,struct _MeshOps,MESH_COOKIE,0,"Mesh",comm,MeshDestroy,0);CHKERRQ(ierr);
  p->ops->view               = MeshView_Sieve;
  p->ops->createglobalvector = MeshCreateGlobalVector;
  p->ops->getmatrix          = MeshGetMatrix;

  ierr = PetscObjectChangeTypeName((PetscObject) p, "sieve");CHKERRQ(ierr);

  p->m            = PETSC_NULL;
  p->globalvector = PETSC_NULL;
  *mesh = p;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MeshDestroy"
/*@C
    MeshDestroy - Destroys a mesh.

    Collective on Mesh

    Input Parameter:
.   mesh - the mesh object

    Level: advanced

.seealso MeshCreate(), MeshCreateGlobalVector(), MeshGetGlobalIndices()

@*/
PetscErrorCode PETSCDM_DLLEXPORT MeshDestroy(Mesh mesh)
{
  PetscErrorCode     ierr;

  PetscFunctionBegin;
  if (--mesh->refct > 0) PetscFunctionReturn(0);
  if (mesh->globalvector) {ierr = VecDestroy(mesh->globalvector);CHKERRQ(ierr);}
  mesh->m = PETSC_NULL;
  ierr = PetscHeaderDestroy(mesh);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "ExpandInterval"
inline void ExpandInterval(const ALE::Point& interval, int indices[], int& indx)
{
  const int end = interval.prefix + interval.index;
  for(int i = interval.index; i < end; i++) {
    indices[indx++] = i;
  }
}

#undef __FUNCT__
#define __FUNCT__ "ExpandInterval_New"
inline void ExpandInterval_New(ALE::Point interval, PetscInt indices[], PetscInt *indx)
{
  for(int i = 0; i < interval.prefix; i++) {
    indices[(*indx)++] = interval.index + i;
  }
}

#undef __FUNCT__
#define __FUNCT__ "ExpandIntervals"
PetscErrorCode ExpandIntervals(ALE::Obj<ALE::Mesh::real_section_type::IndexArray> intervals, PetscInt *indices)
{
  int k = 0;

  PetscFunctionBegin;
  for(ALE::Mesh::real_section_type::IndexArray::iterator i_itor = intervals->begin(); i_itor != intervals->end(); i_itor++) {
    ExpandInterval_New(*i_itor, indices, &k);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MeshCreateVector"
/*
  Creates a ghosted vector based upon the global ordering in the bundle.
*/
PetscErrorCode MeshCreateVector(ALE::Obj<ALE::Mesh> m, Vec *v)
{
  // FIX: Must not include ghosts
  PetscInt       localSize = 0;
  MPI_Comm       comm = m->comm();
  PetscMPIInt    rank = m->commRank();
  PetscInt      *ghostIndices = NULL;
  PetscInt       ghostSize = 0;
  PetscErrorCode ierr;

  PetscFunctionBegin;
#ifdef PARALLEL
  ALE::Obj<ALE::PreSieve> globalIndices = bundle->getGlobalIndices();
  ALE::Obj<ALE::PreSieve> pointTypes = bundle->getPointTypes();
  ALE::Obj<ALE::Point_set> rentedPoints = pointTypes->cone(ALE::Point(rank, ALE::rentedPoint));

  for(ALE::Point_set::iterator e_itor = rentedPoints->begin(); e_itor != rentedPoints->end(); e_itor++) {
    ALE::Obj<ALE::Point_set> cone = globalIndices->cone(*e_itor);

    if (cone->size()) {
      ALE::Point interval = *cone->begin();

      ghostSize += interval.index;
    }
  }
#endif
  if (ghostSize) {
    ierr = PetscMalloc(ghostSize * sizeof(PetscInt), &ghostIndices);CHKERRQ(ierr);
  }
#ifdef PARALLEL
  PetscInt ghostIdx = 0;

  for(ALE::Point_set::iterator e_itor = rentedPoints->begin(); e_itor != rentedPoints->end(); e_itor++) {
    ALE::Obj<ALE::Point_set> cone = globalIndices->cone(*e_itor);

    if (cone->size()) {
      ALE::Point interval = *cone->begin();

      // Must insert into ghostIndices at the index given by localIndices
      //   However, I think right now its correct because rentedPoints iterates in the same way in both methods
      ExpandInterval(interval, ghostIndices, &ghostIdx);
    }
  }
#endif
  ierr = VecCreateGhost(comm, localSize, PETSC_DETERMINE, ghostSize, ghostIndices, v);CHKERRQ(ierr);
  if (m->debug()) {
    PetscInt globalSize, g;

    ierr = VecGetSize(*v, &globalSize);CHKERRQ(ierr);
    ierr = PetscPrintf(comm, "Making an ordering over the vertices\n===============================\n");
    ierr = PetscSynchronizedPrintf(comm, "[%d]  global size: %d localSize: %d ghostSize: %d\n", rank, globalSize, localSize, ghostSize);CHKERRQ(ierr);
    ierr = PetscSynchronizedPrintf(comm, "[%d]  ghostIndices:", rank);CHKERRQ(ierr);
    for(g = 0; g < ghostSize; g++) {
      ierr = PetscSynchronizedPrintf(comm, "[%d] %d\n", rank, ghostIndices[g]);CHKERRQ(ierr);
    }
    ierr = PetscSynchronizedPrintf(comm, "\n");CHKERRQ(ierr);
    ierr = PetscSynchronizedFlush(comm);CHKERRQ(ierr);
  }
  ierr = PetscFree(ghostIndices);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MeshCreateGlobalVector"
/*@C
    MeshCreateGlobalVector - Creates a vector of the correct size to be gathered into 
        by the mesh.

    Collective on Mesh

    Input Parameter:
.    mesh - the mesh object

    Output Parameters:
.   gvec - the global vector

    Level: advanced

    Notes: Once this has been created you cannot add additional arrays or vectors to be packed.

.seealso MeshDestroy(), MeshCreate(), MeshGetGlobalIndices()

@*/
PetscErrorCode PETSCDM_DLLEXPORT MeshCreateGlobalVector(Mesh mesh,Vec *gvec)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  /* Turned off caching for this method so that bundle can be reset to make different vectors */
#if 0
  if (mesh->globalvector) {
    ierr = VecDuplicate(mesh->globalvector, gvec);CHKERRQ(ierr);
    PetscFunctionReturn(0);
  }
#endif
#ifdef __cplusplus
  ALE::Obj<ALE::Mesh> m;

  ierr = MeshGetMesh(mesh, m);CHKERRQ(ierr);
  ierr = MeshCreateVector(m, gvec);CHKERRQ(ierr);
#endif
#if 0
  mesh->globalvector = *gvec;
  ierr = PetscObjectReference((PetscObject) mesh->globalvector);CHKERRQ(ierr); 
#endif
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MeshGetGlobalIndices"
/*@C
    MeshGetGlobalIndices - Gets the global indices for all the local entries

    Collective on Mesh

    Input Parameter:
.    mesh - the mesh object

    Output Parameters:
.    idx - the individual indices for each packed vector/array
 
    Level: advanced

    Notes:
       The idx parameters should be freed by the calling routine with PetscFree()

.seealso MeshDestroy(), MeshCreateGlobalVector(), MeshCreate()

@*/
PetscErrorCode PETSCDM_DLLEXPORT MeshGetGlobalIndices(Mesh mesh,PetscInt *idx[])
{
  SETERRQ(PETSC_ERR_SUP, "");
}

#undef __FUNCT__
#define __FUNCT__ "MeshGetGlobalScatter"
PetscErrorCode PETSCDM_DLLEXPORT MeshGetGlobalScatter(ALE::Mesh *mesh, const char fieldName[], Vec g, VecScatter *scatter)
{
  typedef ALE::Mesh::real_section_type::index_type index_type;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscLogEventBegin(Mesh_GetGlobalScatter,0,0,0,0);CHKERRQ(ierr);
  const ALE::Obj<ALE::Mesh::real_section_type>&  field       = mesh->getRealSection(std::string(fieldName));
  const ALE::Obj<ALE::Mesh::topology_type>&      topology    = field->getTopology();
  const ALE::Obj<ALE::Mesh::real_section_type::atlas_type>&         atlas       = field->getAtlas();
  const ALE::Mesh::real_section_type::patch_type patch       = 0;
  const ALE::Obj<ALE::Mesh::order_type>&     globalOrder = mesh->getFactory()->getGlobalOrder(topology, patch, fieldName, field->getAtlas());
  const ALE::Mesh::real_section_type::atlas_type::chart_type& chart = atlas->getPatch(patch);
  int                                        localSize   = field->size(patch);
  int *localIndices, *globalIndices;
  int  localIndx = 0, globalIndx = 0;
  Vec  localVec;
  IS   localIS, globalIS;

  // Loop over all local points
  ierr = PetscMalloc(localSize*sizeof(int), &localIndices); CHKERRQ(ierr);
  ierr = PetscMalloc(localSize*sizeof(int), &globalIndices); CHKERRQ(ierr);
  for(ALE::Mesh::real_section_type::atlas_type::chart_type::const_iterator p_iter = chart.begin(); p_iter != chart.end(); ++p_iter) {
    const ALE::Mesh::real_section_type::index_type& idx = atlas->restrictPoint(patch, *p_iter)[0];

    // Map local indices to global indices
    ExpandInterval(idx, localIndices, localIndx);
    ExpandInterval(index_type(idx.prefix, globalOrder->getIndex(*p_iter)), globalIndices, globalIndx);
  }
  if (localIndx  != localSize) SETERRQ2(PETSC_ERR_ARG_SIZ, "Invalid number of local indices %d, should be %d", localIndx, localSize);
  if (globalIndx != localSize) SETERRQ2(PETSC_ERR_ARG_SIZ, "Invalid number of global indices %d, should be %d", globalIndx, localSize);
  ierr = ISCreateGeneral(PETSC_COMM_SELF, localSize, localIndices,  &localIS);CHKERRQ(ierr);
  ierr = ISCreateGeneral(PETSC_COMM_SELF, localSize, globalIndices, &globalIS);CHKERRQ(ierr);
  ierr = PetscFree(localIndices);
  ierr = PetscFree(globalIndices);CHKERRQ(ierr);
  ierr = VecCreateSeqWithArray(PETSC_COMM_SELF, localSize, field->restrict(patch), &localVec);CHKERRQ(ierr);
  ierr = VecScatterCreate(localVec, localIS, g, globalIS, scatter);CHKERRQ(ierr);
  ierr = ISDestroy(globalIS);CHKERRQ(ierr);
  ierr = ISDestroy(localIS);CHKERRQ(ierr);
  ierr = PetscLogEventEnd(Mesh_GetGlobalScatter,0,0,0,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

EXTERN PetscErrorCode assembleFullField(VecScatter, Vec, Vec, InsertMode);

#undef __FUNCT__
#define __FUNCT__ "restrictVector"
/*@C
  restrictVector - Insert values from a global vector into a local ghosted vector

  Collective on g

  Input Parameters:
+ g - The global vector
. l - The local vector
- mode - either ADD_VALUES or INSERT_VALUES, where
   ADD_VALUES adds values to any existing entries, and
   INSERT_VALUES replaces existing entries with new values

   Level: beginner

.seealso: MatSetOption()
@*/
PetscErrorCode restrictVector(Vec g, Vec l, InsertMode mode)
{
  VecScatter     injection;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscLogEventBegin(Mesh_restrictVector,0,0,0,0);CHKERRQ(ierr);
  ierr = PetscObjectQuery((PetscObject) g, "injection", (PetscObject *) &injection);CHKERRQ(ierr);
  if (injection) {
    ierr = VecScatterBegin(g, l, mode, SCATTER_REVERSE, injection);
    ierr = VecScatterEnd(g, l, mode, SCATTER_REVERSE, injection);
  } else {
    if (mode == INSERT_VALUES) {
      ierr = VecCopy(g, l);CHKERRQ(ierr);
    } else {
      ierr = VecAXPY(l, 1.0, g);CHKERRQ(ierr);
    }
  }
  ierr = PetscLogEventEnd(Mesh_restrictVector,0,0,0,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "assembleVectorComplete"
/*@C
  assembleVectorComplete - Insert values from a local ghosted vector into a global vector

  Collective on g

  Input Parameters:
+ g - The global vector
. l - The local vector
- mode - either ADD_VALUES or INSERT_VALUES, where
   ADD_VALUES adds values to any existing entries, and
   INSERT_VALUES replaces existing entries with new values

   Level: beginner

.seealso: MatSetOption()
@*/
PetscErrorCode assembleVectorComplete(Vec g, Vec l, InsertMode mode)
{
  VecScatter     injection;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscLogEventBegin(Mesh_assembleVectorComplete,0,0,0,0);CHKERRQ(ierr);
  ierr = PetscObjectQuery((PetscObject) g, "injection", (PetscObject *) &injection);CHKERRQ(ierr);
  if (injection) {
    ierr = VecScatterBegin(l, g, mode, SCATTER_FORWARD, injection);CHKERRQ(ierr);
    ierr = VecScatterEnd(l, g, mode, SCATTER_FORWARD, injection);CHKERRQ(ierr);
  } else {
    if (mode == INSERT_VALUES) {
      ierr = VecCopy(l, g);CHKERRQ(ierr);
    } else {
      ierr = VecAXPY(g, 1.0, l);CHKERRQ(ierr);
    }
  }
  ierr = PetscLogEventEnd(Mesh_assembleVectorComplete,0,0,0,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "assembleVector"
/*@C
  assembleVector - Insert values into a vector

  Collective on A

  Input Parameters:
+ b - the vector
. e - The element number
. v - The values
- mode - either ADD_VALUES or INSERT_VALUES, where
   ADD_VALUES adds values to any existing entries, and
   INSERT_VALUES replaces existing entries with new values

   Level: beginner

.seealso: VecSetOption()
@*/
PetscErrorCode assembleVector(Vec b, PetscInt e, PetscScalar v[], InsertMode mode)
{
  Mesh                     mesh;
  ALE::Obj<ALE::Mesh> m;
  ALE::Mesh::real_section_type::patch_type patch;
  PetscInt                 firstElement;
  PetscErrorCode           ierr;

  PetscFunctionBegin;
  ierr = PetscLogEventBegin(Mesh_assembleVector,0,0,0,0);CHKERRQ(ierr);
  ierr = PetscObjectQuery((PetscObject) b, "mesh", (PetscObject *) &mesh);CHKERRQ(ierr);
  ierr = MeshGetMesh(mesh, m);CHKERRQ(ierr);
  //firstElement = elementBundle->getLocalSizes()[bundle->getCommRank()];
  firstElement = 0;
  // Must relate b to field
  if (mode == INSERT_VALUES) {
    m->getRealSection(std::string("x"))->update(patch, ALE::Mesh::point_type(e + firstElement), v);
  } else {
    m->getRealSection(std::string("x"))->updateAdd(patch, ALE::Mesh::point_type(e + firstElement), v);
  }
  ierr = PetscLogEventEnd(Mesh_assembleVector,0,0,0,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "updateOperator"
PetscErrorCode updateOperator(Mat A, const ALE::Obj<ALE::Mesh::real_section_type>& section, const ALE::Obj<ALE::Mesh::order_type>& globalOrder, const ALE::Mesh::point_type& e, PetscScalar array[], InsertMode mode)
{
  ALE::Mesh::real_section_type::patch_type patch = 0;
  static PetscInt  indicesSize = 0;
  static PetscInt *indices = NULL;
  PetscInt         numIndices = 0;
  PetscErrorCode   ierr;

  PetscFunctionBegin;
  const ALE::Obj<ALE::Mesh::real_section_type::IndexArray> intervals = section->getIndices(patch, e, globalOrder);

  ierr = PetscLogEventBegin(Mesh_updateOperator,0,0,0,0);CHKERRQ(ierr);
  if (section->debug()) {printf("[%d]mat for element %d\n", section->commRank(), e);}
  for(ALE::Mesh::real_section_type::IndexArray::iterator i_iter = intervals->begin(); i_iter != intervals->end(); ++i_iter) {
    numIndices += i_iter->prefix;
    if (section->debug()) {
      printf("[%d]mat interval (%d, %d)\n", section->commRank(), i_iter->prefix, i_iter->index);
    }
  }
  if (indicesSize && (indicesSize != numIndices)) {
    ierr = PetscFree(indices); CHKERRQ(ierr);
    indices = NULL;
  }
  if (!indices) {
    indicesSize = numIndices;
    ierr = PetscMalloc(indicesSize * sizeof(PetscInt), &indices); CHKERRQ(ierr);
  }
  ierr = ExpandIntervals(intervals, indices); CHKERRQ(ierr);
  if (section->debug()) {
    for(int i = 0; i < numIndices; i++) {
      printf("[%d]mat indices[%d] = %d\n", section->commRank(), i, indices[i]);
    }
    for(int i = 0; i < numIndices; i++) {
      printf("[%d]", section->commRank());
      for(int j = 0; j < numIndices; j++) {
        printf(" %g", array[i*numIndices+j]);
      }
      printf("\n");
    }
  }
  ierr = MatSetValues(A, numIndices, indices, numIndices, indices, array, mode);
  if (ierr) {
    printf("[%d]ERROR in updateOperator: point %d\n", section->commRank(), e);
    for(int i = 0; i < numIndices; i++) {
      printf("[%d]mat indices[%d] = %d\n", section->commRank(), i, indices[i]);
    }
    CHKERRQ(ierr);
  }
  ierr = PetscLogEventEnd(Mesh_updateOperator,0,0,0,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "assembleMatrix"
/*@C
  assembleMatrix - Insert values into a matrix

  Collective on A

  Input Parameters:
+ A - the matrix
. e - The element number
. v - The values
- mode - either ADD_VALUES or INSERT_VALUES, where
   ADD_VALUES adds values to any existing entries, and
   INSERT_VALUES replaces existing entries with new values

   Level: beginner

.seealso: MatSetOption()
@*/
PetscErrorCode assembleMatrix(Mat A, PetscInt e, PetscScalar v[], InsertMode mode)
{
  PetscObjectContainer c;
  ALE::Mesh           *mesh;
  PetscErrorCode       ierr;

  PetscFunctionBegin;
  ierr = PetscLogEventBegin(Mesh_assembleMatrix,0,0,0,0);CHKERRQ(ierr);
  ierr = PetscObjectQuery((PetscObject) A, "mesh", (PetscObject *) &c);CHKERRQ(ierr);
  ierr = PetscObjectContainerGetPointer(c, (void **) &mesh);CHKERRQ(ierr);
  try {
    const ALE::Obj<ALE::Mesh::topology_type>& topology = mesh->getTopology();
    const ALE::Obj<ALE::Mesh::numbering_type>& cNumbering = mesh->getFactory()->getLocalNumbering(topology, 0, topology->depth());
    const ALE::Obj<ALE::Mesh::order_type>& globalOrder = mesh->getFactory()->getGlobalOrder(topology, 0, "displacement", mesh->getRealSection("displacement")->getAtlas());

    ierr = updateOperator(A, mesh->getRealSection("displacement"), globalOrder, cNumbering->getPoint(e), v, mode);CHKERRQ(ierr);
  } catch (ALE::Exception e) {
    std::cout << e.msg() << std::endl;
  }
  ierr = PetscLogEventEnd(Mesh_assembleMatrix,0,0,0,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "preallocateMatrix"
PetscErrorCode preallocateMatrix(ALE::Mesh *mesh, const ALE::Obj<ALE::Mesh::real_section_type>& field, const ALE::Obj<ALE::Mesh::order_type>& globalOrder, Mat A)
{
  const ALE::Obj<ALE::Mesh::sieve_type>     adjGraph    = new ALE::Mesh::sieve_type(mesh->comm(), mesh->debug());
  const ALE::Obj<ALE::Mesh::topology_type>  adjTopology = new ALE::Mesh::topology_type(mesh->comm(), mesh->debug());
  const ALE::Mesh::real_section_type::patch_type patch       = 0;
  const ALE::Obj<ALE::Mesh::topology_type>& topology    = mesh->getTopology();
  const ALE::Obj<ALE::Mesh::sieve_type>&    sieve       = topology->getPatch(patch);
  PetscInt       numLocalRows, firstRow, lastRow;
  PetscInt      *dnz, *onz;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  adjTopology->setPatch(patch, adjGraph);
  ierr = MatGetLocalSize(A, &numLocalRows, PETSC_NULL);CHKERRQ(ierr);
  ierr = MatGetOwnershipRange(A, &firstRow, &lastRow);CHKERRQ(ierr);
  ierr = PetscMalloc2(numLocalRows, PetscInt, &dnz, numLocalRows, PetscInt, &onz);CHKERRQ(ierr);
  /* Create local adjacency graph */
  /*   In general, we need to get FIAT info that attaches dual basis vectors to sieve points */
  const ALE::Mesh::real_section_type::atlas_type::chart_type& chart = field->getAtlas()->getPatch(patch);

  for(ALE::Mesh::real_section_type::atlas_type::chart_type::const_iterator c_iter = chart.begin(); c_iter != chart.end(); ++c_iter) {
    const ALE::Mesh::real_section_type::atlas_type::point_type& point = *c_iter;

    adjGraph->addCone(sieve->cone(sieve->support(point)), point);
  }
  /* Distribute adjacency graph */
  topology->constructOverlap(patch);
  const Obj<ALE::Mesh::send_overlap_type>& vertexSendOverlap = topology->getSendOverlap();
  const Obj<ALE::Mesh::recv_overlap_type>& vertexRecvOverlap = topology->getRecvOverlap();
  const Obj<ALE::Mesh::send_overlap_type>  nbrSendOverlap    = new ALE::Mesh::send_overlap_type(mesh->comm(), mesh->debug());
  const Obj<ALE::Mesh::recv_overlap_type>  nbrRecvOverlap    = new ALE::Mesh::recv_overlap_type(mesh->comm(), mesh->debug());
  const Obj<ALE::Mesh::send_section_type>  sendSection       = new ALE::Mesh::send_section_type(mesh->comm(), mesh->debug());
  const Obj<ALE::Mesh::recv_section_type>  recvSection       = new ALE::Mesh::recv_section_type(mesh->comm(), sendSection->getTag(), mesh->debug());

  ALE::New::Distribution<ALE::Mesh::topology_type>::coneCompletion(vertexSendOverlap, vertexRecvOverlap, adjTopology, sendSection, recvSection);
  /* Distribute indices for new points */
  ALE::New::Distribution<ALE::Mesh::topology_type>::updateOverlap(sendSection, recvSection, nbrSendOverlap, nbrRecvOverlap);
  mesh->getFactory()->completeOrder(globalOrder, nbrSendOverlap, nbrRecvOverlap, patch, true);
  /* Read out adjacency graph */
  const ALE::Obj<ALE::Mesh::sieve_type> graph = adjTopology->getPatch(patch);

  ierr = PetscMemzero(dnz, numLocalRows * sizeof(PetscInt));CHKERRQ(ierr);
  ierr = PetscMemzero(onz, numLocalRows * sizeof(PetscInt));CHKERRQ(ierr);
  for(ALE::Mesh::real_section_type::atlas_type::chart_type::const_iterator c_iter = chart.begin(); c_iter != chart.end(); ++c_iter) {
    const ALE::Mesh::real_section_type::atlas_type::point_type& point = *c_iter;

    if (globalOrder->isLocal(point)) {
      const Obj<ALE::Mesh::sieve_type::traits::coneSequence>& adj   = graph->cone(point);
      const ALE::Mesh::order_type::value_type&                rIdx  = globalOrder->restrictPoint(patch, point)[0];
      const int                                               row   = rIdx.prefix;
      const int                                               rSize = rIdx.index;

      for(ALE::Mesh::sieve_type::traits::coneSequence::iterator v_iter = adj->begin(); v_iter != adj->end(); ++v_iter) {
        const ALE::Mesh::real_section_type::atlas_type::point_type& neighbor = *v_iter;
        const ALE::Mesh::order_type::value_type& cIdx     = globalOrder->restrictPoint(patch, neighbor)[0];
        const int&                               cSize    = cIdx.index;
        
        if (globalOrder->isLocal(neighbor)) {
          for(int r = 0; r < rSize; ++r) {dnz[row - firstRow + r] += cSize;}
        } else {
          for(int r = 0; r < rSize; ++r) {onz[row - firstRow + r] += cSize;}
        }
      }
    }
  }
  if (mesh->debug()) {
    int rank = mesh->commRank();
    for(int r = 0; r < numLocalRows; r++) {
      std::cout << "["<<rank<<"]: dnz["<<r<<"]: " << dnz[r] << " onz["<<r<<"]: " << onz[r] << std::endl;
    }
  }
  ierr = MatSeqAIJSetPreallocation(A, 0, dnz);CHKERRQ(ierr);
  ierr = MatMPIAIJSetPreallocation(A, 0, dnz, 0, onz);CHKERRQ(ierr);
  ierr = PetscFree2(dnz, onz);CHKERRQ(ierr);
  ierr = MatSetOption(A, MAT_NEW_NONZERO_ALLOCATION_ERR);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/******************************** C Wrappers **********************************/

#undef __FUNCT__  
#define __FUNCT__ "WriteVTKHeader"
PetscErrorCode WriteVTKHeader(PetscViewer viewer)
{
  return VTKViewer::writeHeader(viewer);
}

#undef __FUNCT__  
#define __FUNCT__ "WriteVTKVertices"
PetscErrorCode WriteVTKVertices(Mesh mesh, PetscViewer viewer)
{
  ALE::Obj<ALE::Mesh> m;
  PetscErrorCode ierr;

  ierr = MeshGetMesh(mesh, m);CHKERRQ(ierr);
  return VTKViewer::writeVertices(m, 0, viewer);
}

#undef __FUNCT__  
#define __FUNCT__ "WriteVTKElements"
PetscErrorCode WriteVTKElements(Mesh mesh, PetscViewer viewer)
{
  ALE::Obj<ALE::Mesh> m;
  PetscErrorCode ierr;

  ierr = MeshGetMesh(mesh, m);CHKERRQ(ierr);
  return VTKViewer::writeElements(m, 0, viewer);
}

#undef __FUNCT__  
#define __FUNCT__ "WritePCICEVertices"
PetscErrorCode WritePCICEVertices(Mesh mesh, PetscViewer viewer)
{
  ALE::Obj<ALE::Mesh> m;
  PetscErrorCode ierr;

  ierr = MeshGetMesh(mesh, m);CHKERRQ(ierr);
  return ALE::PCICE::Viewer::writeVertices(m, viewer);
}

#undef __FUNCT__  
#define __FUNCT__ "WritePCICEElements"
PetscErrorCode WritePCICEElements(Mesh mesh, PetscViewer viewer)
{
  ALE::Obj<ALE::Mesh> m;
  PetscErrorCode ierr;

  ierr = MeshGetMesh(mesh, m);CHKERRQ(ierr);
  return ALE::PCICE::Viewer::writeElements(m, viewer);
}

#undef __FUNCT__  
#define __FUNCT__ "WritePCICERestart"
PetscErrorCode WritePCICERestart(Mesh mesh, PetscViewer viewer)
{
  ALE::Obj<ALE::Mesh> m;
  PetscErrorCode ierr;

  ierr = MeshGetMesh(mesh, m);CHKERRQ(ierr);
  return ALE::PCICE::Viewer::writeRestart(m, viewer);
}

#undef __FUNCT__  
#define __FUNCT__ "MeshCreatePCICE"
/*@C
  MeshCreatePCICE - Create a Mesh from PCICE files.

  Not Collective

  Input Parameters:
+ dim - The topological mesh dimension
. coordFilename - The file containing vertex coordinates
. adjFilename - The file containing the vertices for each element
. bcFilename - The file containing the boundary topology and conditions
. numBdFaces - The number of boundary faces (or edges)
- numBdVertices - The number of boundary vertices

  Output Parameter:
. mesh - The Mesh object

  Level: beginner

.keywords: mesh, PCICE
.seealso: MeshCreate()
@*/
PetscErrorCode MeshCreatePCICE(MPI_Comm comm, const int dim, const char coordFilename[], const char adjFilename[], const char bcFilename[], const int numBdFaces, const int numBdVertices, Mesh *mesh)
{
  ALE::Obj<ALE::Mesh> m;
  PetscInt            debug = 0;
  PetscTruth          flag;
  PetscErrorCode      ierr;

  PetscFunctionBegin;
  ierr = MeshCreate(comm, mesh);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL, "-debug", &debug, &flag);CHKERRQ(ierr);
  try {
    m  = ALE::PCICE::Builder::readMesh(comm, dim, std::string(coordFilename), std::string(adjFilename), false, false, debug);
  } catch(ALE::Exception e) {
    SETERRQ(PETSC_ERR_FILE_OPEN, e.message());
  }
  if (bcFilename) {
    ALE::PCICE::Builder::readBoundary(m, std::string(bcFilename), numBdFaces, numBdVertices);
  }
  ierr = MeshSetMesh(*mesh, m);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MeshGetCoordinates"
/*@C
  MeshGetCoordinates - Creates an array holding the coordinates.

  Not Collective

  Input Parameter:
+ mesh - The Mesh object
- columnMajor - Flag for column major order

  Output Parameter:
+ numVertices - The number of vertices
. dim - The embedding dimension
- coords - The array holding local coordinates

  Level: intermediate

.keywords: mesh, coordinates
.seealso: MeshCreate()
@*/
PetscErrorCode MeshGetCoordinates(Mesh mesh, PetscTruth columnMajor, PetscInt *numVertices, PetscInt *dim, PetscReal *coords[])
{
  ALE::Obj<ALE::Mesh> m;
  PetscErrorCode      ierr;

  PetscFunctionBegin;
  ierr = MeshGetMesh(mesh, m);CHKERRQ(ierr);
  ALE::PCICE::Builder::outputVerticesLocal(m, numVertices, dim, coords, columnMajor);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MeshGetElements"
/*@C
  MeshGetElements - Creates an array holding the vertices on each element.

  Not Collective

  Input Parameters:
+ mesh - The Mesh object
- columnMajor - Flag for column major order

  Output Parameters:
+ numElements - The number of elements
. numCorners - The number of vertices per element
- vertices - The array holding vertices on each local element

  Level: intermediate

.keywords: mesh, elements
.seealso: MeshCreate()
@*/
PetscErrorCode MeshGetElements(Mesh mesh, PetscTruth columnMajor, PetscInt *numElements, PetscInt *numCorners, PetscInt *vertices[])
{
  ALE::Obj<ALE::Mesh> m;
  PetscErrorCode      ierr;

  PetscFunctionBegin;
  ierr = MeshGetMesh(mesh, m);CHKERRQ(ierr);
  ALE::PCICE::Builder::outputElementsLocal(m, numElements, numCorners, vertices, columnMajor);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MeshDistribute"
/*@C
  MeshDistribute - Distributes the mesh and any associated sections.

  Not Collective

  Input Parameter:
. serialMesh - The original Mesh object

  Output Parameter:
. parallelMesh - The distributed Mesh object

  Level: intermediate

.keywords: mesh, elements
.seealso: MeshCreate()
@*/
PetscErrorCode MeshDistribute(Mesh serialMesh, Mesh *parallelMesh)
{
  ALE::Obj<ALE::Mesh> oldMesh;
  PetscErrorCode      ierr;

  PetscFunctionBegin;
  ierr = MeshGetMesh(serialMesh, oldMesh);CHKERRQ(ierr);
  ierr = MeshCreate(oldMesh->comm(), parallelMesh);CHKERRQ(ierr);
  ALE::Obj<ALE::Mesh> newMesh = ALE::New::Distribution<ALE::Mesh::topology_type>::distributeMesh(oldMesh);
  ierr = MeshSetMesh(*parallelMesh, newMesh);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MeshHasSectionReal"
/*@C
  MeshHasSectionReal - Determines whether this mesh has a SectionReal with the given name.

  Not Collective

  Input Parameters:
+ mesh - The Mesh object
- name - The section name

  Output Parameter:
. flag - True if the SectionReal is present in the Mesh

  Level: intermediate

.keywords: mesh, elements
.seealso: MeshCreate()
@*/
PetscErrorCode MeshHasSectionReal(Mesh mesh, const char name[], PetscTruth *flag)
{
  ALE::Obj<ALE::Mesh> m;
  PetscErrorCode      ierr;

  PetscFunctionBegin;
  ierr = MeshGetMesh(mesh, m);CHKERRQ(ierr);
  *flag = (PetscTruth) m->hasRealSection(std::string(name));
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MeshGetSectionReal"
/*@C
  MeshGetSectionReal - Returns a SectionReal of the given name from the Mesh.

  Collective on Mesh

  Input Parameters:
+ mesh - The Mesh object
- name - The section name

  Output Parameter:
. section - The SectionReal

  Note: The section is a new object, and must be destroyed by the user

  Level: intermediate

.keywords: mesh, elements
.seealso: MeshCreate()
@*/
PetscErrorCode MeshGetSectionReal(Mesh mesh, const char name[], SectionReal *section)
{
  ALE::Obj<ALE::Mesh> m;
  PetscErrorCode      ierr;

  PetscFunctionBegin;
  ierr = MeshGetMesh(mesh, m);CHKERRQ(ierr);
  ierr = SectionRealCreate(m->comm(), section);CHKERRQ(ierr);
  ierr = PetscObjectSetName((PetscObject) *section, name);CHKERRQ(ierr);
  ierr = SectionRealSetSection(*section, m->getRealSection(std::string(name)));
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MeshHasSectionInt"
/*@C
  MeshHasSectionInt - Determines whether this mesh has a SectionInt with the given name.

  Not Collective

  Input Parameters:
+ mesh - The Mesh object
- name - The section name

  Output Parameter:
. flag - True if the SectionInt is present in the Mesh

  Level: intermediate

.keywords: mesh, elements
.seealso: MeshCreate()
@*/
PetscErrorCode MeshHasSectionInt(Mesh mesh, const char name[], PetscTruth *flag)
{
  ALE::Obj<ALE::Mesh> m;
  PetscErrorCode      ierr;

  PetscFunctionBegin;
  ierr = MeshGetMesh(mesh, m);CHKERRQ(ierr);
  *flag = (PetscTruth) m->hasIntSection(std::string(name));
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MeshGetSectionInt"
/*@C
  MeshGetSectionInt - Returns a SectionInt of the given name from the Mesh.

  Collective on Mesh

  Input Parameters:
+ mesh - The Mesh object
- name - The section name

  Output Parameter:
. section - The SectionInt

  Note: The section is a new object, and must be destroyed by the user

  Level: intermediate

.keywords: mesh, elements
.seealso: MeshCreate()
@*/
PetscErrorCode MeshGetSectionInt(Mesh mesh, const char name[], SectionInt *section)
{
  ALE::Obj<ALE::Mesh> m;
  PetscErrorCode      ierr;

  PetscFunctionBegin;
  ierr = MeshGetMesh(mesh, m);CHKERRQ(ierr);
  ierr = SectionIntCreate(m->comm(), section);CHKERRQ(ierr);
  ierr = PetscObjectSetName((PetscObject) *section, name);CHKERRQ(ierr);
  ierr = SectionIntSetSection(*section, m->getIntSection(std::string(name)));
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MeshHasSectionPair"
/*@C
  MeshHasSectionPair - Determines whether this mesh has a SectionPair with the given name.

  Not Collective

  Input Parameters:
+ mesh - The Mesh object
- name - The section name

  Output Parameter:
. flag - True if the SectionPair is present in the Mesh

  Level: intermediate

.keywords: mesh, elements
.seealso: MeshCreate()
@*/
PetscErrorCode MeshHasSectionPair(Mesh mesh, const char name[], PetscTruth *flag)
{
  ALE::Obj<ALE::Mesh> m;
  PetscErrorCode      ierr;

  PetscFunctionBegin;
  ierr = MeshGetMesh(mesh, m);CHKERRQ(ierr);
  *flag = (PetscTruth) m->hasPairSection(std::string(name));
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MeshGetSectionPair"
/*@C
  MeshGetSectionPair - Returns a SectionPair of the given name from the Mesh.

  Collective on Mesh

  Input Parameters:
+ mesh - The Mesh object
- name - The section name

  Output Parameter:
. section - The SectionPair

  Note: The section is a new object, and must be destroyed by the user

  Level: intermediate

.keywords: mesh, elements
.seealso: MeshCreate()
@*/
PetscErrorCode MeshGetSectionPair(Mesh mesh, const char name[], SectionPair *section)
{
  ALE::Obj<ALE::Mesh> m;
  PetscErrorCode      ierr;

  PetscFunctionBegin;
  ierr = MeshGetMesh(mesh, m);CHKERRQ(ierr);
  ierr = SectionPairCreate(m->comm(), section);CHKERRQ(ierr);
  ierr = PetscObjectSetName((PetscObject) *section, name);CHKERRQ(ierr);
  ierr = SectionPairSetSection(*section, m->getPairSection(std::string(name)));
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "SectionGetArray"
/*@C
  SectionGetArray - Returns the array underlying the Section.

  Not Collective

  Input Parameters:
+ mesh - The Mesh object
- name - The section name

  Output Parameters:
+ numElements - The number of mesh element with values
. fiberDim - The number of values per element
- array - The array

  Level: intermediate

.keywords: mesh, elements
.seealso: MeshCreate()
@*/
PetscErrorCode SectionGetArray(Mesh mesh, const char name[], PetscInt *numElements, PetscInt *fiberDim, PetscScalar *array[])
{
  ALE::Obj<ALE::Mesh> m;
  PetscErrorCode      ierr;

  PetscFunctionBegin;
  ierr = MeshGetMesh(mesh, m);CHKERRQ(ierr);
  const Obj<ALE::Mesh::real_section_type>&        section = m->getRealSection(std::string(name));
  const ALE::Mesh::real_section_type::patch_type  patch   = 0;
  if (!section->hasPatch(patch)) {
    *numElements = 0;
    *fiberDim    = 0;
    *array       = NULL;
    PetscFunctionReturn(0);
  }
  const ALE::Mesh::real_section_type::chart_type& chart   = section->getPatch(patch);
/*   const int                                  depth   = m->getTopology()->depth(patch, *chart.begin()); */
/*   *numElements = m->getTopology()->depthStratum(patch, depth)->size(); */
/*   *fiberDim    = section->getFiberDimension(patch, *chart.begin()); */
/*   *array       = (PetscScalar *) section->restrict(patch); */
  int fiberDimMin = section->getFiberDimension(patch, *chart.begin());
  int numElem     = 0;

  for(ALE::Mesh::real_section_type::chart_type::iterator c_iter = chart.begin(); c_iter != chart.end(); ++c_iter) {
    const int fiberDim = section->getFiberDimension(patch, *c_iter);

    if (fiberDim < fiberDimMin) fiberDimMin = fiberDim;
  }
  for(ALE::Mesh::real_section_type::chart_type::iterator c_iter = chart.begin(); c_iter != chart.end(); ++c_iter) {
    const int fiberDim = section->getFiberDimension(patch, *c_iter);

    numElem += fiberDim/fiberDimMin;
  }
  *numElements = numElem;
  *fiberDim    = fiberDimMin;
  *array       = (PetscScalar *) section->restrict(patch);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "BCSectionGetArray"
/*@C
  BCSectionGetArray - Returns the array underlying the BCSection.

  Not Collective

  Input Parameters:
+ mesh - The Mesh object
- name - The section name

  Output Parameters:
+ numElements - The number of mesh element with values
. fiberDim - The number of values per element
- array - The array

  Level: intermediate

.keywords: mesh, elements
.seealso: MeshCreate()
@*/
PetscErrorCode BCSectionGetArray(Mesh mesh, const char name[], PetscInt *numElements, PetscInt *fiberDim, PetscInt *array[])
{
  ALE::Obj<ALE::Mesh> m;
  PetscErrorCode      ierr;

  PetscFunctionBegin;
  ierr = MeshGetMesh(mesh, m);CHKERRQ(ierr);
  const Obj<ALE::Mesh::int_section_type>&       section = m->getIntSection(std::string(name));
  const ALE::Mesh::int_section_type::patch_type patch   = 0;
  if (!section->hasPatch(patch)) {
    *numElements = 0;
    *fiberDim    = 0;
    *array       = NULL;
    PetscFunctionReturn(0);
  }
  const ALE::Mesh::int_section_type::chart_type& chart = section->getPatch(patch);
  int fiberDimMin = section->getFiberDimension(patch, *chart.begin());
  int numElem     = 0;

  for(ALE::Mesh::int_section_type::chart_type::iterator c_iter = chart.begin(); c_iter != chart.end(); ++c_iter) {
    const int fiberDim = section->getFiberDimension(patch, *c_iter);

    if (fiberDim < fiberDimMin) fiberDimMin = fiberDim;
  }
  for(ALE::Mesh::int_section_type::chart_type::iterator c_iter = chart.begin(); c_iter != chart.end(); ++c_iter) {
    const int fiberDim = section->getFiberDimension(patch, *c_iter);

    numElem += fiberDim/fiberDimMin;
  }
  *numElements = numElem;
  *fiberDim    = fiberDimMin;
  *array       = (PetscInt *) section->restrict(patch);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "BCFUNCGetArray"
PetscErrorCode BCFUNCGetArray(Mesh mesh, PetscInt *numElements, PetscInt *fiberDim, PetscScalar *array[])
{
  ALE::Obj<ALE::Mesh> m;
  PetscErrorCode      ierr;

  PetscFunctionBegin;
  ierr = MeshGetMesh(mesh, m);CHKERRQ(ierr);
  ALE::Mesh::bc_values_type& bcValues = m->getBCValues();
  *numElements = bcValues.size();
  *fiberDim    = 4;
  *array       = new PetscScalar[(*numElements)*(*fiberDim)];
  for(int bcf = 1; bcf <= (int) bcValues.size(); ++bcf) {
    (*array)[(bcf-1)*4+0] = bcValues[bcf].rho;
    (*array)[(bcf-1)*4+1] = bcValues[bcf].u;
    (*array)[(bcf-1)*4+2] = bcValues[bcf].v;
    (*array)[(bcf-1)*4+3] = bcValues[bcf].p;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "WritePyLithVertices"
PetscErrorCode WritePyLithVertices(Mesh mesh, PetscViewer viewer)
{
  ALE::Obj<ALE::Mesh> m;
  PetscErrorCode ierr;

  ierr = MeshGetMesh(mesh, m);CHKERRQ(ierr);
  return ALE::PyLith::Viewer::writeVertices(m, viewer);
}

#undef __FUNCT__  
#define __FUNCT__ "WritePyLithElements"
PetscErrorCode WritePyLithElements(Mesh mesh, SectionInt material, PetscViewer viewer)
{
  ALE::Obj<ALE::Mesh> m;
  ALE::Obj<ALE::Mesh::int_section_type> s;
  PetscErrorCode ierr;

  ierr = MeshGetMesh(mesh, m);CHKERRQ(ierr);
  ierr = SectionIntGetSection(material, s);CHKERRQ(ierr);
  return ALE::PyLith::Viewer::writeElements(m, s, viewer);
}

#undef __FUNCT__  
#define __FUNCT__ "WritePyLithVerticesLocal"
PetscErrorCode WritePyLithVerticesLocal(Mesh mesh, PetscViewer viewer)
{
  ALE::Obj<ALE::Mesh> m;
  PetscErrorCode ierr;

  ierr = MeshGetMesh(mesh, m);CHKERRQ(ierr);
  return ALE::PyLith::Viewer::writeVerticesLocal(m, viewer);
}

#undef __FUNCT__  
#define __FUNCT__ "WritePyLithElementsLocal"
PetscErrorCode WritePyLithElementsLocal(Mesh mesh, SectionInt material, PetscViewer viewer)
{
  ALE::Obj<ALE::Mesh> m;
  ALE::Obj<ALE::Mesh::int_section_type> s;
  PetscErrorCode ierr;

  ierr = MeshGetMesh(mesh, m);CHKERRQ(ierr);
  ierr = SectionIntGetSection(material, s);CHKERRQ(ierr);
  return ALE::PyLith::Viewer::writeElementsLocal(m, s, viewer);
}

#undef __FUNCT__  
#define __FUNCT__ "WritePyLithTractionsLocal"
PetscErrorCode WritePyLithTractionsLocal(Mesh mesh, PetscViewer viewer)
{
  ALE::Obj<ALE::Mesh> m;
  PetscErrorCode ierr;

  ierr = MeshGetMesh(mesh, m);CHKERRQ(ierr);
  return ALE::PyLith::Viewer::writeTractionsLocal(m, m->getRealSection("tractions"), viewer);
}
