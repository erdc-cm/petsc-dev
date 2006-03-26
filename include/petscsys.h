/*
    Provides access to system related and general utility routines.
*/
#if !defined(__PETSCSYS_H)
#define __PETSCSYS_H
#include "petsc.h"
PETSC_EXTERN_CXX_BEGIN

EXTERN PetscErrorCode PETSC_DLLEXPORT PetscGetArchType(char[],size_t);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscGetHostName(char[],size_t);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscGetUserName(char[],size_t);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscGetProgramName(char[],size_t);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscSetProgramName(const char[]);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscGetDate(char[],size_t);

EXTERN PetscErrorCode PETSC_DLLEXPORT PetscSortInt(PetscInt,PetscInt[]);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscSortIntWithPermutation(PetscInt,const PetscInt[],PetscInt[]);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscSortStrWithPermutation(PetscInt,const char*[],PetscInt[]);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscSortIntWithArray(PetscInt,PetscInt[],PetscInt[]);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscSortIntWithScalarArray(PetscInt,PetscInt[],PetscScalar[]);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscSortReal(PetscInt,PetscReal[]);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscSortRealWithPermutation(PetscInt,const PetscReal[],PetscInt[]);

EXTERN PetscErrorCode PETSC_DLLEXPORT PetscSetDisplay(void);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscGetDisplay(char[],size_t);

extern PetscCookie PETSC_DLLEXPORT PETSC_RANDOM_COOKIE;

#define RANDOM_DEFAULT           "random_default"
#define RANDOM_DEFAULT_REAL      "random_default_real"
#define RANDOM_DEFAULT_IMAGINARY "random_default_imaginary"
#define PETSC_RAND               "petsc_rand"
#define PETSC_RAND48             "petsc_rand48"
#define PetscRandomType const char*

/* Logging support */
extern PETSCVEC_DLLEXPORT PetscCookie PETSC_RANDOM_COOKIE;

EXTERN PetscErrorCode PETSCVEC_DLLEXPORT PetscRandomInitializePackage(char *);

/*S
     PetscRandom - Abstract PETSc object that manages generating random numbers

   Level: intermediate

  Concepts: random numbers

.seealso:  PetscRandomCreate(), PetscRandomGetValue()
S*/
typedef struct _p_PetscRandom*   PetscRandom;

/* Dynamic creation and loading functions */
extern PetscFList PetscRandomList;
extern PetscTruth PetscRandomRegisterAllCalled;

EXTERN PetscErrorCode PETSC_DLLEXPORT PetscRandomRegisterAll(const char []);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscRandomRegister(const char[],const char[],const char[],PetscErrorCode (*)(PetscRandom*));
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscRandomRegisterDestroy(void);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscRandomSetType(PetscRandom, PetscRandomType);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscRandomGetType(PetscRandom, PetscRandomType *);

/*MC
  PetscRandomRegisterDynamic - Adds a new PetscRandom component implementation

  Synopsis:
  PetscErrorCode PetscRandomRegisterDynamic(char *name, char *path, char *func_name, PetscErrorCode (*create_func)(PetscRandom))

  Not Collective

  Input Parameters:
+ name        - The name of a new user-defined creation routine
. path        - The path (either absolute or relative) of the library containing this routine
. func_name   - The name of routine to create method context
- create_func - The creation routine itself

  Notes:
  PetscRandomRegisterDynamic() may be called multiple times to add several user-defined vectors

  If dynamic libraries are used, then the fourth input argument (routine_create) is ignored.

  Sample usage:
.vb
    PetscRandomRegisterDynamic("my_rand","/home/username/my_lib/lib/libO/solaris/libmy.a", "MyPetscRandomtorCreate", MyPetscRandomtorCreate);
.ve

  Then, your random type can be chosen with the procedural interface via
.vb
    PetscRandomCreate(MPI_Comm, PetscRandom *);
    PetscRandomSetType(PetscRandom,"my_random_name");
.ve
   or at runtime via the option
.vb
    -random_type my_random_name
.ve

  Notes: $PETSC_ARCH occuring in pathname will be replaced with appropriate values.
         If your function is not being put into a shared library then use PetscRandomRegister() instead
        
  Level: advanced

.keywords: PetscRandom, register
.seealso: PetscRandomRegisterAll(), PetscRandomRegisterDestroy(), PetscRandomRegister()
M*/
#if defined(PETSC_USE_DYNAMIC_LIBRARIES)
#define PetscRandomRegisterDynamic(a,b,c,d) PetscRandomRegister(a,b,c,0)
#else
#define PetscRandomRegisterDynamic(a,b,c,d) PetscRandomRegister(a,b,c,d)
#endif

EXTERN PetscErrorCode PETSC_DLLEXPORT PetscRandomCreate(MPI_Comm,PetscRandomType,PetscRandom*);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscRandomGetValue(PetscRandom,PetscScalar*);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscRandomGetInterval(PetscRandom,PetscScalar*,PetscScalar*);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscRandomSetInterval(PetscRandom,PetscScalar,PetscScalar);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscRandomSetSeed(PetscRandom,unsigned long);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscRandomGetSeed(PetscRandom,unsigned long *);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscRandomSeed(PetscRandom);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscRandomDestroy(PetscRandom);

EXTERN PetscErrorCode PETSC_DLLEXPORT PetscGetFullPath(const char[],char[],size_t);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscGetRelativePath(const char[],char[],size_t);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscGetWorkingDirectory(char[],size_t);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscGetRealPath(char[],char[]);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscGetHomeDirectory(char[],size_t);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscTestFile(const char[],char,PetscTruth*);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscTestDirectory(const char[],char,PetscTruth*);

EXTERN PetscErrorCode PETSC_DLLEXPORT PetscBinaryRead(int,void*,PetscInt,PetscDataType);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscSynchronizedBinaryRead(MPI_Comm,int,void*,PetscInt,PetscDataType);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscSynchronizedBinaryWrite(MPI_Comm,int,void*,PetscInt,PetscDataType,PetscTruth);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscBinaryWrite(int,void*,PetscInt,PetscDataType,PetscTruth);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscBinaryOpen(const char[],PetscFileMode,int *); 
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscBinaryClose(int);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscSharedTmp(MPI_Comm,PetscTruth *);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscSharedWorkingDirectory(MPI_Comm,PetscTruth *);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscGetTmp(MPI_Comm,char *,size_t);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscFileRetrieve(MPI_Comm,const char *,char *,size_t,PetscTruth*);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscLs(MPI_Comm,const char[],char*,size_t,PetscTruth*);

/*
   In binary files variables are stored using the following lengths,
  regardless of how they are stored in memory on any one particular
  machine. Use these rather then sizeof() in computing sizes for 
  PetscBinarySeek().
*/
#define PETSC_BINARY_INT_SIZE    (32/8)
#define PETSC_BINARY_FLOAT_SIZE  (32/8)
#define PETSC_BINARY_CHAR_SIZE    (8/8)
#define PETSC_BINARY_SHORT_SIZE  (16/8)
#define PETSC_BINARY_DOUBLE_SIZE (64/8)
#define PETSC_BINARY_SCALAR_SIZE sizeof(PetscScalar)

/*E
  PetscBinarySeekType - argument to PetscBinarySeek()

  Level: advanced

.seealso: PetscBinarySeek(), PetscSynchronizedBinarySeek()
E*/
typedef enum {PETSC_BINARY_SEEK_SET = 0,PETSC_BINARY_SEEK_CUR = 1,PETSC_BINARY_SEEK_END = 2} PetscBinarySeekType;
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscBinarySeek(int,off_t,PetscBinarySeekType,off_t*);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscSynchronizedBinarySeek(MPI_Comm,int,off_t,PetscBinarySeekType,off_t*);

EXTERN PetscErrorCode PETSC_DLLEXPORT PetscSetDebugger(const char[],PetscTruth);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscSetDefaultDebugger(void);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscSetDebuggerFromString(char*);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscAttachDebugger(void);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscStopForDebugger(void);

EXTERN PetscErrorCode PETSC_DLLEXPORT PetscGatherNumberOfMessages(MPI_Comm,PetscMPIInt*,PetscMPIInt*,PetscMPIInt*);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscGatherMessageLengths(MPI_Comm,PetscMPIInt,PetscMPIInt,PetscMPIInt*,PetscMPIInt**,PetscMPIInt**);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscGatherMessageLengths2(MPI_Comm,PetscMPIInt,PetscMPIInt,PetscMPIInt*,PetscMPIInt*,PetscMPIInt**,PetscMPIInt**,PetscMPIInt**);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscPostIrecvInt(MPI_Comm,PetscMPIInt,PetscMPIInt,PetscMPIInt*,PetscMPIInt*,PetscInt***,MPI_Request**);
EXTERN PetscErrorCode PETSC_DLLEXPORT PetscPostIrecvScalar(MPI_Comm,PetscMPIInt,PetscMPIInt,PetscMPIInt*,PetscMPIInt*,PetscScalar***,MPI_Request**);

EXTERN PetscErrorCode PETSC_DLLEXPORT PetscSSEIsEnabled(MPI_Comm,PetscTruth *,PetscTruth *);

/*E
  InsertMode - Whether entries are inserted or added into vectors or matrices

  Level: beginner

.seealso: VecSetValues(), MatSetValues(), VecSetValue(), VecSetValuesBlocked(),
          VecSetValuesLocal(), VecSetValuesBlockedLocal(), MatSetValuesBlocked(),
          MatSetValuesBlockedLocal(), MatSetValuesLocal(), VecScatterBegin(), VecScatterEnd()
E*/
typedef enum {NOT_SET_VALUES, INSERT_VALUES, ADD_VALUES, MAX_VALUES} InsertMode;

/*MC
    INSERT_VALUES - Put a value into a vector or matrix, overwrites any previous value

    Level: beginner

.seealso: InsertMode, VecSetValues(), MatSetValues(), VecSetValue(), VecSetValuesBlocked(),
          VecSetValuesLocal(), VecSetValuesBlockedLocal(), MatSetValuesBlocked(), ADD_VALUES, INSERT_VALUES,
          MatSetValuesBlockedLocal(), MatSetValuesLocal(), VecScatterBegin(), VecScatterEnd()

M*/

/*MC
    ADD_VALUES - Adds a value into a vector or matrix, if there previously was no value, just puts the
                value into that location

    Level: beginner

.seealso: InsertMode, VecSetValues(), MatSetValues(), VecSetValue(), VecSetValuesBlocked(),
          VecSetValuesLocal(), VecSetValuesBlockedLocal(), MatSetValuesBlocked(), ADD_VALUES, INSERT_VALUES,
          MatSetValuesBlockedLocal(), MatSetValuesLocal(), VecScatterBegin(), VecScatterEnd()

M*/

/*MC
    MAX_VALUES - Puts the maximum of the scattered/gathered value and the current value into each location

    Level: beginner

.seealso: InsertMode, VecScatterBegin(), VecScatterEnd(), ADD_VALUES, INSERT_VALUES

M*/

/*E
  ScatterMode - Determines the direction of a scatter

  Level: beginner

.seealso: VecScatter, VecScatterBegin(), VecScatterEnd()
E*/
typedef enum {SCATTER_FORWARD=0, SCATTER_REVERSE=1, SCATTER_FORWARD_LOCAL=2, SCATTER_REVERSE_LOCAL=3, SCATTER_LOCAL=2} ScatterMode;

/*MC
    SCATTER_FORWARD - Scatters the values as dictated by the VecScatterCreate() call

    Level: beginner

.seealso: VecScatter, ScatterMode, VecScatterCreate(), VecScatterBegin(), VecScatterEnd(), SCATTER_REVERSE, SCATTER_FORWARD_LOCAL,
          SCATTER_REVERSE_LOCAL

M*/

/*MC
    SCATTER_REVERSE - Moves the values in the opposite direction then the directions indicated in
         in the VecScatterCreate()

    Level: beginner

.seealso: VecScatter, ScatterMode, VecScatterCreate(), VecScatterBegin(), VecScatterEnd(), SCATTER_FORWARD, SCATTER_FORWARD_LOCAL,
          SCATTER_REVERSE_LOCAL

M*/

/*MC
    SCATTER_FORWARD_LOCAL - Scatters the values as dictated by the VecScatterCreate() call except NO parallel communication
       is done. Any variables that have be moved between processes are ignored

    Level: developer

.seealso: VecScatter, ScatterMode, VecScatterCreate(), VecScatterBegin(), VecScatterEnd(), SCATTER_REVERSE, SCATTER_FORWARD,
          SCATTER_REVERSE_LOCAL

M*/

/*MC
    SCATTER_REVERSE_LOCAL - Moves the values in the opposite direction then the directions indicated in
         in the VecScatterCreate()  except NO parallel communication
       is done. Any variables that have be moved between processes are ignored

    Level: developer

.seealso: VecScatter, ScatterMode, VecScatterCreate(), VecScatterBegin(), VecScatterEnd(), SCATTER_FORWARD, SCATTER_FORWARD_LOCAL,
          SCATTER_REVERSE

M*/


PETSC_EXTERN_CXX_END
#endif /* __PETSCSYS_H */
