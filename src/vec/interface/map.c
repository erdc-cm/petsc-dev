/*$Id: map.c,v 1.15 2001/07/20 21:18:10 bsmith Exp $*/
/*
     Provides the interface functions for all map operations.
   These are the map functions the user calls.
*/
#include "vecimpl.h"    /*I "petscvec.h" I*/

/* Logging support */
int MAP_COOKIE = 0;

#undef __FUNCT__  
#define __FUNCT__ "PetscMapSetTypeFromOptions_Private"
/*
  PetscMapSetTypeFromOptions_Private - Sets the type of map from user options. Defaults to a PETSc sequential map on one
  processor and a PETSc MPI map on more than one processor.

  Collective on PetscMap

  Input Parameter:
. map - The map

  Level: intermediate

.keywords: PetscMap, set, options, database, type
.seealso: PetscMapSetFromOptions(), PetscMapSetType()
*/
static int PetscMapSetTypeFromOptions_Private(PetscMap map)
{
  PetscTruth opt;
  const char *defaultType;
  char       typeName[256];
  int        size;
  int        ierr;

  PetscFunctionBegin;
  if (map->type_name != PETSC_NULL) {
    defaultType = map->type_name;
  } else {
    ierr = MPI_Comm_size(map->comm, &size);                                                           CHKERRQ(ierr);
    if (size > 1) {
      defaultType = MAP_MPI;
    } else {
      defaultType = MAP_MPI;
    }
  }

  if (!PetscMapRegisterAllCalled) {
    ierr = PetscMapRegisterAll(PETSC_NULL);                                                               CHKERRQ(ierr);
  }
  ierr = PetscOptionsList("-map_type", "PetscMap type"," PetscMapSetType", PetscMapList, defaultType, typeName, 256, &opt);
  CHKERRQ(ierr);
  if (opt == PETSC_TRUE) {
    ierr = PetscMapSetType(map, typeName);                                                                CHKERRQ(ierr);
  } else {
    ierr = PetscMapSetType(map, defaultType);                                                             CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscMapSetFromOptions"
/*@C
  PetscMapSetFromOptions - Configures the map from the options database.

  Collective on PetscMap

  Input Parameter:
. map - The map

  Notes:  To see all options, run your program with the -help option, or consult the users manual.
          Must be called after PetscMapCreate() but before the map is used.

  Level: intermediate

  Concepts: maptors^setting options
  Concepts: maptors^setting type

.keywords: PetscMap, set, options, database
.seealso: PetscMapCreate(), PetscMapPrintHelp(), PetscMaphSetOptionsPrefix()
@*/
int PetscMapSetFromOptions(PetscMap map)
{
  PetscTruth opt;
  int        ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(map,MAP_COOKIE,1);

  ierr = PetscOptionsBegin(map->comm, map->prefix, "PetscMap options", "PetscMap");                       CHKERRQ(ierr);

  /* Handle generic maptor options */
  ierr = PetscOptionsHasName(PETSC_NULL, "-help", &opt);                                                  CHKERRQ(ierr);
  if (opt == PETSC_TRUE) {
    ierr = PetscMapPrintHelp(map);                                                                        CHKERRQ(ierr);
  }

  /* Handle map type options */
  ierr = PetscMapSetTypeFromOptions_Private(map);                                                         CHKERRQ(ierr);

  /* Handle specific maptor options */
  if (map->ops->setfromoptions != PETSC_NULL) {
    ierr = (*map->ops->setfromoptions)(map);                                                              CHKERRQ(ierr);
  }

  ierr = PetscOptionsEnd();                                                                               CHKERRQ(ierr);

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscMapPrintHelp"
/*@
  PetscMapPrintHelp - Prints all options for the PetscMap.

  Input Parameter:
. map - The map

  Options Database Keys:
$  -help, -h

  Level: intermediate

.keywords: PetscMap, help
.seealso: PetscMapSetFromOptions()
@*/
int PetscMapPrintHelp(PetscMap map)
{
  char p[64];
  int  ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(map, MAP_COOKIE,1);

  ierr = PetscStrcpy(p, "-");                                                                             CHKERRQ(ierr);
  if (map->prefix != PETSC_NULL) {
    ierr = PetscStrcat(p, map->prefix);                                                                   CHKERRQ(ierr);
  }

  (*PetscHelpPrintf)(map->comm, "PetscMap options ------------------------------------------------\n");
  (*PetscHelpPrintf)(map->comm,"   %smap_type <typename> : Sets the map type\n", p);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscMapDestroy"
/*@C
   PetscMapDestroy - Destroys a map object.

   Not Collective

   Input Parameter:
.  m - the map object

   Level: developer

.seealso: PetscMapCreateMPI()

@*/
int PetscMapDestroy(PetscMap map)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(map, MAP_COOKIE,1); 
  if (--map->refct > 0) PetscFunctionReturn(0);
  if (map->range != PETSC_NULL) {
    ierr = PetscFree(map->range);                                                                         CHKERRQ(ierr);
  }
  if (map->ops->destroy != PETSC_NULL) {
    ierr = (*map->ops->destroy)(map);                                                                     CHKERRQ(ierr);
  }
  PetscLogObjectDestroy(map);
  PetscHeaderDestroy(map);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscMapSetLocalSize"
/*@C
  PetscMapSetLocalSize - Sets the number of elements associated with this processor.

  Not Collective

  Input Parameters:
+ m - the map object
- n - the local size

  Level: developer

.seealso: PetscMapSetSize(), PetscMapGetLocalRange(), PetscMapGetGlobalRange()
Concepts: PetscMap^local size
@*/
int PetscMapSetLocalSize(PetscMap m,int n)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(m,MAP_COOKIE,1); 
  m->n = n;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscMapGetLocalSize"
/*@C
   PetscMapGetLocalSize - Gets the number of elements associated with this processor.

   Not Collective

   Input Parameter:
.  m - the map object

   Output Parameter:
.  n - the local size

   Level: developer

.seealso: PetscMapGetSize(), PetscMapGetLocalRange(), PetscMapGetGlobalRange()

   Concepts: PetscMap^local size

@*/
int PetscMapGetLocalSize(PetscMap m,int *n)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(m,MAP_COOKIE,1); 
  PetscValidIntPointer(n,2);
  *n = m->n;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscMapSetSize"
/*@C
  PetscMapSetSize - Sets the total number of elements associated with this map.

  Not Collective

  Input Parameters:
+ m - the map object
- N - the global size

  Level: developer

.seealso: PetscMapSetLocalSize(), PetscMapGetLocalRange(), PetscMapGetGlobalRange()
 Concepts: PetscMap^size
@*/
int PetscMapSetSize(PetscMap m,int N)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(m,MAP_COOKIE,1); 
  m->N = N;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscMapGetSize"
/*@C
   PetscMapGetSize - Gets the total number of elements associated with this map.

   Not Collective

   Input Parameter:
.  m - the map object

   Output Parameter:
.  N - the global size

   Level: developer

.seealso: PetscMapGetLocalSize(), PetscMapGetLocalRange(), PetscMapGetGlobalRange()

   Concepts: PetscMap^size
@*/
int PetscMapGetSize(PetscMap m,int *N)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(m,MAP_COOKIE,1); 
  PetscValidIntPointer(N,2);
  *N = m->N;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscMapGetLocalRange"
/*@C
   PetscMapGetLocalRange - Gets the local ownership range for this procesor.

   Not Collective

   Input Parameter:
.  m - the map object

   Output Parameter:
+  rstart - the first local index, pass in PETSC_NULL if not interested 
-  rend   - the last local index + 1, pass in PETSC_NULL if not interested

   Level: developer

.seealso: PetscMapGetLocalSize(), PetscMapGetGlobalRange()
@*/
int PetscMapGetLocalRange(PetscMap m,int *rstart,int *rend)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(m,MAP_COOKIE,1); 
  if (rstart)  PetscValidIntPointer(rstart,2);
  if (rend) PetscValidIntPointer(rend,3);
  if (rstart) *rstart = m->rstart;
  if (rend)   *rend   = m->rend;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscMapGetGlobalRange"
/*@C
   PetscMapGetGlobalRange - Gets the ownership ranges for all processors.

   Not Collective

   Input Parameter:
.  m - the map object

   Output Parameter:
.  range - array of size + 1 where size is the size of the communicator 
           associated with the map. range[rank], range[rank+1] is the 
           range for processor 

   Level: developer

.seealso: PetscMapGetSize(), PetscMapGetLocalRange()

@*/
int PetscMapGetGlobalRange(PetscMap m,int *range[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(m,MAP_COOKIE,1); 
  PetscValidPointer(range,2);
  *range = m->range;
  PetscFunctionReturn(0);
}
