#ifdef PETSC_RCS_HEADER
static char vcid[] = "$Id: mapreg.c,v 1.2 2000/01/10 03:18:14 knepley Exp $";
#endif

#include "src/vec/vecimpl.h"  /*I "petscvec.h"  I*/

PetscFList PetscMapList                       = PETSC_NULL;
PetscTruth PetscMapRegisterAllCalled          = PETSC_FALSE;

#undef __FUNCT__  
#define __FUNCT__ "PetscMapSetType"
/*@C
  PetscMapSetType - Builds a map, for a particular map implementation.

  Collective on PetscMap

  Input Parameters:
+ map    - The PetscMap object
- method - The name of the map type

  Options Database Command:
. -map_type <method> - Sets the method; use -help for a list
                       of available methods (for instance, mpi)

  Notes:
  See "petsc/include/vec.h" for available vector types (for instance, MAP_MPI).

  Level: intermediate

.keywords: map, set, type
.seealso PetscMapGetType(), PetscMapCreate()
@*/
int PetscMapSetType(PetscMap map, const PetscMapType method)
{
  int      (*r)(PetscMap);
  PetscTruth match;
  int        ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(map, MAP_COOKIE);
  ierr = PetscTypeCompare((PetscObject) map, method, &match);                                             CHKERRQ(ierr);
  if (match == PETSC_TRUE) PetscFunctionReturn(0);

  /* Get the function pointers for the method requested */
  if (PetscMapRegisterAllCalled == PETSC_FALSE) {
    ierr = PetscMapRegisterAll(PETSC_NULL);                                                               CHKERRQ(ierr);
  }
  ierr = PetscFListFind(map->comm, PetscMapList, method, (void (**)(void)) &r);                           CHKERRQ(ierr);
  if (!r) SETERRQ1(PETSC_ERR_ARG_WRONG, "Unknown map type: %s", method);

  if (map->ops->destroy != PETSC_NULL) {
    ierr = (*map->ops->destroy)(map);                                                                     CHKERRQ(ierr);
  }
  ierr = (*r)(map);                                                                                       CHKERRQ(ierr);

  ierr = PetscObjectChangeTypeName((PetscObject) map, method);                                            CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscMapGetType"
/*@C
  PetscMapGetType - Gets the map type name (as a string) from the PetscMap.

  Not collective

  Input Parameter:
. map  - The map

  Output Parameter:
. type - The map type name

  Level: intermediate

.keywords: map, get, type, name
.seealso PetscMapSetType(), PetscMapCreate()
@*/
int PetscMapGetType(PetscMap map, PetscMapType *type)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(map, MAP_COOKIE);
  PetscValidCharPointer(type);
  if (PetscMapRegisterAllCalled == PETSC_FALSE) {
    ierr = PetscMapRegisterAll(PETSC_NULL);                                                               CHKERRQ(ierr);
  }
  *type = map->type_name;
  PetscFunctionReturn(0);
}

/*--------------------------------------------------------------------------------------------------------------------*/
/*MC
  PetscMapRegisterDynamic - Adds a new map component implementation

  Synopsis:
  PetscMapRegisterDynamic(char *name, char *path, char *func_name, int (*create_func)(PetscMap))

  Not Collective

  Input Parameters:
+ name        - The name of a new user-defined creation routine
. path        - The path (either absolute or relative) of the library containing this routine
. func_name   - The name of routine to create method context
- create_func - The creation routine itself

  Notes:
  PetscMapRegister() may be called multiple times to add several user-defined maptors

  If dynamic libraries are used, then the fourth input argument (routine_create) is ignored.

  Sample usage:
.vb
    PetscMapRegisterDynamic("my_map","/home/username/my_lib/lib/libO/solaris/libmy.a", "MyPetscMapCreate", MyPetscMapCreate);
.ve

  Then, your map type can be chosen with the procedural interface via
.vb
    PetscMapCreate(MPI_Comm, PetscMap *);
    PetscMapSetType(PetscMap,"my_map_name");
.ve
   or at runtime via the option
.vb
    -map_type my_map_name
.ve

  Note: $PETSC_ARCH and $BOPT occuring in pathname will be replaced with appropriate values.

  Level: advanced

.keywords: PetscMap, register
.seealso: PetscMapRegisterAll(), PetscMapRegisterDestroy()
M*/

#undef __FUNCT__  
#define __FUNCT__ "PetscMapRegister"
int PetscMapRegister(const char sname[], const char path[], const char name[], int (*function)(PetscMap))
{
  char fullname[PETSC_MAX_PATH_LEN];
  int  ierr;

  PetscFunctionBegin;
  ierr = PetscStrcpy(fullname, path);                                                                     CHKERRQ(ierr);
  ierr = PetscStrcat(fullname, ":");                                                                      CHKERRQ(ierr);
  ierr = PetscStrcat(fullname, name);                                                                     CHKERRQ(ierr);
  ierr = PetscFListAdd(&PetscMapList, sname, fullname, (void (*)(void)) function);                        CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*--------------------------------------------------------------------------------------------------------------------*/
#undef __FUNCT__  
#define __FUNCT__ "PetscMapRegisterDestroy"
/*@C
  PetscMapRegisterDestroy - Frees the list of PetscMap methods that were registered by PetscMapRegister().

  Not collective

  Level: advanced

.keywords: map, register, destroy
.seealso: PetscMapRegister(), PetscMapRegisterAll()
@*/
int PetscMapRegisterDestroy()
{
  int ierr;

  PetscFunctionBegin;
  if (PetscMapList != PETSC_NULL) {
    ierr = PetscFListDestroy(&PetscMapList);                                                              CHKERRQ(ierr);
    PetscMapList = PETSC_NULL;
  }
  PetscMapRegisterAllCalled = PETSC_FALSE;
  PetscFunctionReturn(0);
}

