#ifdef PETSC_RCS_HEADER
static char vcid[] = "$Id: vsilo.c,v 1.4 2000/01/10 03:27:03 knepley Exp $";
#endif

/* 
        Written by Matt Dorbin, mrdorbin@cs.purdue.edu 3/1/99
        For database format and API from LLNL
        Updated by Matt Knepley, knepley@cs.purdue.edu 11/16/99
*/
#include "vsilo.h"
#ifdef GVEC_IS_WORKING
#include "gvec.h"
#else
#include "mesh.h"
#endif

#ifdef PETSC_HAVE_SILO

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerDestroy_Silo"
static int PetscViewerDestroy_Silo(PetscViewer viewer)
{
  Viewer_Silo *silo = (Viewer_Silo *) viewer->data;
  int          rank;
  int          ierr;

  PetscFunctionBegin;
  ierr = MPI_Comm_rank(viewer->comm, &rank);                                                             CHKERRQ(ierr);
  if(!rank) {
    DBClose(silo->file_pointer);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerFlush_Silo"
int PetscViewerFlush_Silo(PetscViewer viewer)
{
  int rank;
  PetscFunctionBegin;
  MPI_Comm_rank(viewer->comm, &rank);
  if (rank)
    PetscFunctionReturn(0);
  PetscFunctionReturn(0);
}

/*-----------------------------------------Public Functions-----------------------------------------------------------*/
#ifdef HAVE_SILO
#undef __FUNCT__  
#define __FUNCT__ "PetscViewerSiloGetFilePointer"
/*@C
  PetscViewerSiloGetFilePointer - Extracts the file pointer from a Silo viewer.

  Input Parameter:
. viewer - viewer context, obtained from PetscViewerSiloOpen()

  Output Parameter:
. fd     - file pointer

.keywords: PetscViewer, file, get, pointer

.seealso: PetscViewerSiloOpen()
@*/
int PetscViewerSiloGetFilePointer(PetscViewer viewer, DBfile **fd)
{
  Viewer_Silo *silo = (Viewer_Silo *) viewer->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_COOKIE);
  PetscValidPointer(fd);
  *fd = silo->file_pointer;
  PetscFunctionReturn(0);
}
#endif

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerSiloOpen"
/*@C
  PetscViewerSiloOpen - This routine writes the mesh and the partition in the 
  SILO format used by MeshTv, which can be used to create plots and
  MPEG movies.

  Collectiveon MPI_Comm

  Input Parameters:
+ comm - The MPI communicator
- name - The name for the Silo files

  Output Parameter:
. viewer  - A viewer

  Notes:
  This viewer is intended to work with the SILO portable database format.
  Details on SILO, MeshTv, and companion software can be obtained by sending
  mail to meshtv@viper.llnl.gov

  Options Database Keys:

  Level: beginner

.keywords: PetscViewer, Silo, open
@*/
int PetscViewerSiloOpen(MPI_Comm comm, const char name[], PetscViewer *viewer)
{
  PetscViewer       v;
  Viewer_Silo *silo;
  char         filename[256];
  char         filetemp[256];
  int          ierr;

  PetscFunctionBegin;
  PetscHeaderCreate(v, _p_PetscViewer, struct _PetscViewerOps, PETSC_VIEWER_COOKIE, -1, PETSC_VIEWER_SILO, comm, PetscViewerDestroy, 0);
  PLogObjectCreate(v);
  silo            = PetscNew(Viewer_Silo); CHKPTRQ(silo);
  v->data         = silo;
  v->ops->destroy = PetscViewerDestroy_Silo;
  v->ops->flush   = PetscViewerFlush_Silo;
  ierr            = PetscStrallocpy(PETSC_VIEWER_SILO, &v->type_name);                                    CHKERRQ(ierr);

  ierr = PetscStrncpy(filetemp, name, 251);                                                               CHKERRQ(ierr);
  ierr = PetscStrcat(filetemp, ".pdb");                                                                   CHKERRQ(ierr);
  ierr = PetscFixFilename(filetemp, filename);                                                            CHKERRQ(ierr);

  silo->file_pointer = DBCreate(filename, DB_CLOBBER, DB_LOCAL, NULL, DB_PDB);
  if (!silo->file_pointer) SETERRQ(PETSC_ERR_FILE_OPEN,"Cannot open Silo viewer file");
#if defined(PETSC_USE_LOG)
  PLogObjectState((PetscObject) v, "Silo File: %s", name);
#endif
  silo->meshName = PETSC_NULL;
  silo->objName  = PETSC_NULL;

  *viewer = v;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__  "PetscViewerSiloCheckMesh"
/*@C
  PetscViewerSiloCheckMesh - This routine checks a Silo viewer to determine whether the
                        mesh has already been put in the .silo file. It also checks for type,
                        and at the moment accepts only UCD_MESH meshes.

  Not collective

  Input Parameters:
+ mesh - The mesh that should be in place
. viewer - The viewer that should contain the mesh
- fp - The pointer to the DBFile that should contain the mesh 

  Level: intermediate

.keywords: viewer, Silo, mesh
.seealso: PetscViewerSiloOpen()
@*/
int PetscViewerSiloCheckMesh(PetscViewer viewer, Mesh mesh)
{
  Viewer_Silo *vsilo = (Viewer_Silo *) viewer->data;
  DBfile      *fp;
  int          mesh_type;
  int          ierr;

  PetscFunctionBegin;
  ierr = PetscViewerSiloGetFilePointer(viewer, &fp);                                                          CHKERRQ(ierr);
  if (vsilo->meshName == PETSC_NULL) {
    mesh_type = DBInqMeshtype(fp, "PetscMesh");
  } else {
    mesh_type = DBInqMeshtype(fp, vsilo->meshName);
  }
  if(mesh_type != DB_UCDMESH) { 
    /* DBInqMeshType returns -1 if the mesh is not found*/
    ierr = MeshView(mesh, viewer);                                                                       CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerSiloGetName"
/*@C
  PetscViewerSiloGetName - Retrieve the default name for objects communicated to Silo

  Input Parameter:
. viewer - The Silo viewer

  Output Parameter:
. name   - The name for new objects created in Silo

.keywords PetscViewer, Silo, name
.seealso PetscViewerSiloSetName(), PetscViewerSiloClearName()
@*/
int PetscViewerSiloGetName(PetscViewer viewer, char **name)
{
  PetscViewer_Silo *vsilo = (PetscViewer_Silo *) viewer->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_COOKIE);
  PetscValidPointer(name);
  *name = vsilo->objName;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerSiloSetName"
/*@C
  PetscViewerSiloSetName - Override the default name for objects communicated to Silo

  Input Parameters:
. viewer - The Silo viewer
. name   - The name for new objects created in Silo

.keywords PetscViewer, Silo, name
.seealso PetscViewerSiloSetName(), PetscViewerSiloClearName()
@*/
int PetscViewerSiloSetName(PetscViewer viewer, char *name)
{
  Viewer_Silo *vsilo = (Viewer_Silo *) viewer->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_COOKIE);
  PetscValidPointer(name);
  vsilo->objName = name;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerSiloClearName"
/*@C
  PetscViewerSiloClearName - Use the default name for objects communicated to Silo

  Input Parameter:
. viewer - The Silo viewer

.keywords PetscViewer, Silo, name
.seealso PetscViewerSiloGetName(), PetscViewerSiloSetName()
@*/
int PetscViewerSiloClearName(PetscViewer viewer)
{
  Viewer_Silo *vsilo = (Viewer_Silo *) viewer->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_COOKIE);
  vsilo->objName = PETSC_NULL;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerSiloGetMeshName"
/*@C
  PetscViewerSiloGetMeshName - Retrieve the default name for the mesh in Silo

  Input Parameter:
. viewer - The Silo viewer

  Output Parameter:
. name   - The name for new objects created in Silo

.keywords PetscViewer, Silo, name, mesh
.seealso PetscViewerSiloSetMeshName(), PetscViewerSiloClearMeshName()
@*/
int PetscViewerSiloGetMeshName(PetscViewer viewer, char **name)
{
  Viewer_Silo *vsilo = (Viewer_Silo *) viewer->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_COOKIE);
  PetscValidPointer(name);
  *name = vsilo->meshName;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerSiloSetMeshName"
/*@C
  PetscViewerSiloSetMeshName - Override the default name for the mesh in Silo

  Input Parameters:
+ viewer - The Silo viewer
- name   - The name for new objects created in Silo

.keywords PetscViewer, Silo, name, mesh
.seealso PetscViewerSiloSetMeshName(), PetscViewerSiloClearMeshName()
@*/
int PetscViewerSiloSetMeshName(PetscViewer viewer, char *name)
{
  Viewer_Silo *vsilo = (Viewer_Silo *) viewer->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_COOKIE);
  PetscValidPointer(name);
  vsilo->meshName = name;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscViewerSiloClearMeshName"
/*@C
  PetscViewerSiloClearMeshName - Use the default name for the mesh in Silo

  Input Parameter:
. viewer - The Silo viewer

.keywords PetscViewer, Silo, name, mesh
.seealso PetscViewerSiloGetMeshName(), PetscViewerSiloSetMeshName()
@*/
int PetscViewerSiloClearMeshName(PetscViewer viewer)
{
  Viewer_Silo *vsilo = (Viewer_Silo *) viewer->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_COOKIE);
  vsilo->meshName = PETSC_NULL;
  PetscFunctionReturn(0);
}

#else

int PetscViewerSiloOpen(MPI_Comm comm, const char name[], PetscViewer *viewer)
{
  SETERRQ(PETSC_ERR_SUP, "You must install the SILO package from LLNL");
}

int PetscViewerSiloCheckMesh(PetscViewer viewer, Mesh mesh)
{
  SETERRQ(PETSC_ERR_SUP, "You must install the SILO package from LLNL");
}

int PetscViewerSiloGetName(PetscViewer viewer, char **name)
{
  SETERRQ(PETSC_ERR_SUP, "You must install the SILO package from LLNL");
}

int PetscViewerSiloSetName(PetscViewer viewer, const char name[])
{
  SETERRQ(PETSC_ERR_SUP, "You must install the SILO package from LLNL");
}

int PetscViewerSiloClearName(PetscViewer viewer)
{
  SETERRQ(PETSC_ERR_SUP, "You must install the SILO package from LLNL");
}

int PetscViewerSiloGetMeshName(PetscViewer viewer, char **name)
{
  SETERRQ(PETSC_ERR_SUP, "You must install the SILO package from LLNL");
}

int PetscViewerSiloSetMeshName(PetscViewer viewer, const char name[])
{
  SETERRQ(PETSC_ERR_SUP, "You must install the SILO package from LLNL");
}

int PetscViewerSiloClearMeshName(PetscViewer viewer)
{
  SETERRQ(PETSC_ERR_SUP, "You must install the SILO package from LLNL");
}

#endif /* PETSC_HAVE_SILO */
