/*$Id: partition.c,v 1.60 2001/06/21 21:17:23 bsmith Exp $*/
 
#include "src/mat/matimpl.h"               /*I "petscmat.h" I*/

/* Logging support */
int MAT_PARTITIONING_COOKIE;

/*
   Simplest partitioning, keeps the current partitioning.
*/
#undef __FUNCT__  
#define __FUNCT__ "MatPartitioningApply_Current" 
static int MatPartitioningApply_Current(MatPartitioning part,IS *partitioning)
{
  int   ierr,m,rank,size;

  PetscFunctionBegin;
  ierr = MPI_Comm_size(part->comm,&size);CHKERRQ(ierr);
  if (part->n != size) {
    SETERRQ(PETSC_ERR_SUP,"Currently only supports one domain per processor");
  }
  ierr = MPI_Comm_rank(part->comm,&rank);CHKERRQ(ierr);

  ierr = MatGetLocalSize(part->adj,&m,PETSC_NULL);CHKERRQ(ierr);
  ierr = ISCreateStride(part->comm,m,rank,0,partitioning);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatPartitioningApply_Square" 
static int MatPartitioningApply_Square(MatPartitioning part,IS *partitioning)
{
  int   cell,ierr,n,N,p,rstart,rend,*color,size;

  PetscFunctionBegin;
  ierr = MPI_Comm_size(part->comm,&size);CHKERRQ(ierr);
  if (part->n != size) {
    SETERRQ(PETSC_ERR_SUP,"Currently only supports one domain per processor");
  }
  p = (int)sqrt((double)part->n);
  if (p*p != part->n) {
    SETERRQ(PETSC_ERR_SUP,"Square partitioning requires \"perfect square\" number of domains");
  }
  ierr = MatGetSize(part->adj,&N,PETSC_NULL);CHKERRQ(ierr);
  n = (int)sqrt((double)N);
  if (n*n != N) {  /* This condition is NECESSARY, but NOT SUFFICIENT in order to the domain be square */
    SETERRQ(PETSC_ERR_SUP,"Square partitioning requires square domain");
  }
  if (n%p != 0) {
    SETERRQ(PETSC_ERR_SUP,"Square partitioning requires p to divide n"); 
  }
  ierr = MatGetOwnershipRange(part->adj,&rstart,&rend);CHKERRQ(ierr);
  ierr = PetscMalloc((rend-rstart)*sizeof(int),&color);CHKERRQ(ierr);
  /* for (int cell=rstart; cell<rend; cell++) { color[cell-rstart] = ((cell%n) < (n/2)) + 2 * ((cell/n) < (n/2)); } */
  for (cell=rstart; cell<rend; cell++) {
    color[cell-rstart] = ((cell%n) / (n/p)) + p * ((cell/n) / (n/p));
  }
  ierr = ISCreateGeneral(part->comm,rend-rstart,color,partitioning);CHKERRQ(ierr);
  ierr = PetscFree(color);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}

EXTERN_C_BEGIN  
#undef __FUNCT__  
#define __FUNCT__ "MatPartitioningCreate_Current" 
int MatPartitioningCreate_Current(MatPartitioning part)
{
  PetscFunctionBegin;
  part->ops->apply   = MatPartitioningApply_Current;
  part->ops->view    = 0;
  part->ops->destroy = 0;
  PetscFunctionReturn(0);
}
EXTERN_C_END

EXTERN_C_BEGIN  
#undef __FUNCT__  
#define __FUNCT__ "MatPartitioningCreate_Square" 
int MatPartitioningCreate_Square(MatPartitioning part)
{
  PetscFunctionBegin;
  part->ops->apply   = MatPartitioningApply_Square;
  part->ops->view    = 0;
  part->ops->destroy = 0;
  PetscFunctionReturn(0);
}
EXTERN_C_END

/* ===========================================================================================*/

#include "petscsys.h"

PetscFList      MatPartitioningList = 0;
PetscTruth MatPartitioningRegisterAllCalled = PETSC_FALSE;


#undef __FUNCT__  
#define __FUNCT__ "MatPartitioningRegister" 
int MatPartitioningRegister(char *sname,char *path,char *name,int (*function)(MatPartitioning))
{
  int  ierr;
  char fullname[256];

  PetscFunctionBegin;
  ierr = PetscFListConcat(path,name,fullname);CHKERRQ(ierr);
  ierr = PetscFListAdd(&MatPartitioningList,sname,fullname,(void (*)(void))function);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatPartitioningRegisterDestroy" 
/*@C
   MatPartitioningRegisterDestroy - Frees the list of partitioning routines.

  Not Collective

  Level: developer

.keywords: matrix, register, destroy

.seealso: MatPartitioningRegisterDynamic(), MatPartitioningRegisterAll()
@*/
int MatPartitioningRegisterDestroy(void)
{
  int ierr;

  PetscFunctionBegin;
  if (MatPartitioningList) {
    ierr = PetscFListDestroy(&MatPartitioningList);CHKERRQ(ierr);
    MatPartitioningList = 0;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatPartitioningGetType"
/*@C
   MatPartitioningGetType - Gets the Partitioning method type and name (as a string) 
        from the partitioning context.

   Not collective

   Input Parameter:
.  partitioning - the partitioning context

   Output Parameter:
.  type - partitioner type

   Level: intermediate

   Not Collective

.keywords: Partitioning, get, method, name, type
@*/
int MatPartitioningGetType(MatPartitioning partitioning,MatPartitioningType *type)
{
  PetscFunctionBegin;
  *type = partitioning->type_name;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatPartitioningSetNParts"
/*@C
   MatPartitioningSetNParts - Set how many partitions need to be created;
        by default this is one per processor. Certain partitioning schemes may
        in fact only support that option.

   Not collective

   Input Parameter:
.  partitioning - the partitioning context
.  n - the number of partitions

   Level: intermediate

   Not Collective

.keywords: Partitioning, set

.seealso: MatPartitioningCreate(), MatPartitioningApply()
@*/
int MatPartitioningSetNParts(MatPartitioning part,int n)
{
  PetscFunctionBegin;
  part->n = n;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatPartitioningApply" 
/*@C
   MatPartitioningApply - Gets a partitioning for a matrix.

   Collective on Mat

   Input Parameters:
.  matp - the matrix partitioning object

   Output Parameters:
.   partitioning - the partitioning. For each local node this tells the processor
                   number that that node is assigned to.

   Options Database Keys:
   To specify the partitioning through the options database, use one of
   the following 
$    -mat_partitioning_type parmetis, -mat_partitioning current
   To see the partitioning result
$    -mat_partitioning_view

   Level: beginner

   The user can define additional partitionings; see MatPartitioningRegisterDynamic().

.keywords: matrix, get, partitioning

.seealso:  MatPartitioningRegisterDynamic(), MatPartitioningCreate(),
           MatPartitioningDestroy(), MatPartitioningSetAdjacency(), ISPartitioningToNumbering(),
           ISPartitioningCount()
@*/
int MatPartitioningApply(MatPartitioning matp,IS *partitioning)
{
  int        ierr;
  PetscTruth flag;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(matp,MAT_PARTITIONING_COOKIE);
  if (!matp->adj->assembled) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Not for unassembled matrix");
  if (matp->adj->factor) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Not for factored matrix"); 
  if (!matp->ops->apply) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Must set type with MatPartitioningSetFromOptions() or MatPartitioningSetType()");
  ierr = PetscLogEventBegin(MAT_Partitioning,matp,0,0,0);CHKERRQ(ierr);
  ierr = (*matp->ops->apply)(matp,partitioning);CHKERRQ(ierr);
  ierr = PetscLogEventEnd(MAT_Partitioning,matp,0,0,0);CHKERRQ(ierr);

  ierr = PetscOptionsHasName(PETSC_NULL,"-mat_partitioning_view",&flag);CHKERRQ(ierr);
  if (flag) {
    ierr = MatPartitioningView(matp,PETSC_VIEWER_STDOUT_(matp->comm));CHKERRQ(ierr);
    ierr = ISView(*partitioning,PETSC_VIEWER_STDOUT_(matp->comm));CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}
 
#undef __FUNCT__  
#define __FUNCT__ "MatPartitioningSetAdjacency"
/*@C
   MatPartitioningSetAdjacency - Sets the adjacency graph (matrix) of the thing to be
      partitioned.

   Collective on MatPartitioning and Mat

   Input Parameters:
+  part - the partitioning context
-  adj - the adjacency matrix

   Level: beginner

.keywords: Partitioning, adjacency

.seealso: MatPartitioningCreate()
@*/
int MatPartitioningSetAdjacency(MatPartitioning part,Mat adj)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(part,MAT_PARTITIONING_COOKIE);
  PetscValidHeaderSpecific(adj,MAT_COOKIE);
  part->adj = adj;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatPartitioningDestroy"
/*@C
   MatPartitioningDestroy - Destroys the partitioning context.

   Collective on Partitioning

   Input Parameters:
.  part - the partitioning context

   Level: beginner

.keywords: Partitioning, destroy, context

.seealso: MatPartitioningCreate()
@*/
int MatPartitioningDestroy(MatPartitioning part)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(part,MAT_PARTITIONING_COOKIE);
  if (--part->refct > 0) PetscFunctionReturn(0);

  if (part->ops->destroy) {
    ierr = (*part->ops->destroy)(part);CHKERRQ(ierr);
  }
  if (part->vertex_weights){
    ierr = PetscFree(part->vertex_weights);CHKERRQ(ierr);
  }
  if (part->part_weights){
    ierr = PetscFree(part->part_weights);CHKERRQ(ierr);
  }
  PetscLogObjectDestroy(part);
  PetscHeaderDestroy(part);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatPartitioningSetVertexWeights"
/*@C
   MatPartitioningSetVertexWeights - Sets the weights for vertices for a partitioning.

   Collective on Partitioning

   Input Parameters:
+  part - the partitioning context
-  weights - the weights

   Level: beginner

   Notes:
      The array weights is freed by PETSc so the user should not free the array. In C/C++
   the array must be obtained with a call to PetscMalloc(), not malloc().

.keywords: Partitioning, destroy, context

.seealso: MatPartitioningCreate(), MatPartitioningSetType(), MatPartitioningSetPartitionWeights()
@*/
int MatPartitioningSetVertexWeights(MatPartitioning part,const int weights[])
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(part,MAT_PARTITIONING_COOKIE);

  if (part->vertex_weights){
    ierr = PetscFree(part->vertex_weights);CHKERRQ(ierr);
  }
  part->vertex_weights = (int *)weights;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatPartitioningSetPartitionWeights"
/*@C
   MatPartitioningSetPartitionWeights - Sets the weights for each partition.

   Collective on Partitioning

   Input Parameters:
+  part - the partitioning context
-  weights - the weights

   Level: beginner

   Notes:
      The array weights is freed by PETSc so the user should not free the array. In C/C++
   the array must be obtained with a call to PetscMalloc(), not malloc().

.keywords: Partitioning, destroy, context

.seealso: MatPartitioningCreate(), MatPartitioningSetType(), MatPartitioningSetVertexWeights()
@*/
int MatPartitioningSetPartitionWeights(MatPartitioning part,const PetscReal weights[])
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(part,MAT_PARTITIONING_COOKIE);

  if (part->part_weights){
    ierr = PetscFree(part->part_weights);CHKERRQ(ierr);
  }
  part->part_weights = (PetscReal*)weights;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatPartitioningCreate"
/*@C
   MatPartitioningCreate - Creates a partitioning context.

   Collective on MPI_Comm

   Input Parameter:
.   comm - MPI communicator 

   Output Parameter:
.  newp - location to put the context

   Level: beginner

.keywords: Partitioning, create, context

.seealso: MatPartitioningSetType(), MatPartitioningApply(), MatPartitioningDestroy(),
          MatPartitioningSetAdjacency()

@*/
int MatPartitioningCreate(MPI_Comm comm,MatPartitioning *newp)
{
  MatPartitioning part;
  int             ierr;

  PetscFunctionBegin;
  *newp          = 0;

  PetscHeaderCreate(part,_p_MatPartitioning,struct _MatPartitioningOps,MAT_PARTITIONING_COOKIE,-1,"MatPartitioning",comm,MatPartitioningDestroy,
                    MatPartitioningView);
  PetscLogObjectCreate(part);
  part->type           = -1;
  part->vertex_weights = PETSC_NULL;
  part->part_weights   = PETSC_NULL;
  ierr = MPI_Comm_size(comm,&part->n);CHKERRQ(ierr);

  *newp = part;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatPartitioningView"
/*@C 
   MatPartitioningView - Prints the partitioning data structure.

   Collective on MatPartitioning

   Input Parameters:
.  part - the partitioning context
.  viewer - optional visualization context

   Level: intermediate

   Note:
   The available visualization contexts include
+     PETSC_VIEWER_STDOUT_SELF - standard output (default)
-     PETSC_VIEWER_STDOUT_WORLD - synchronized standard
         output where only the first processor opens
         the file.  All other processors send their 
         data to the first processor to print. 

   The user can open alternative visualization contexts with
.     PetscViewerASCIIOpen() - output to a specified file

.keywords: Partitioning, view

.seealso: PetscViewerASCIIOpen()
@*/
int MatPartitioningView(MatPartitioning part,PetscViewer viewer)
{
  int                 ierr;
  PetscTruth          isascii;
  MatPartitioningType name;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(part,MAT_PARTITIONING_COOKIE);
  if (!viewer) viewer = PETSC_VIEWER_STDOUT_(part->comm);
  PetscValidHeaderSpecific(viewer,PETSC_VIEWER_COOKIE);
  PetscCheckSameComm(part,viewer);

  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_ASCII,&isascii);CHKERRQ(ierr);
  if (isascii) {
    ierr = MatPartitioningGetType(part,&name);CHKERRQ(ierr);
    ierr = PetscViewerASCIIPrintf(viewer,"MatPartitioning Object: %s\n",name);CHKERRQ(ierr);
    if (part->vertex_weights) {
      ierr = PetscViewerASCIIPrintf(viewer,"  Using vertex weights\n");CHKERRQ(ierr);
    }
  } else {
    SETERRQ1(1,"Viewer type %s not supported for this MatParitioning",((PetscObject)viewer)->type_name);
  }

  if (part->ops->view) {
    ierr = PetscViewerASCIIPushTab(viewer);CHKERRQ(ierr);
    ierr = (*part->ops->view)(part,viewer);CHKERRQ(ierr);
    ierr = PetscViewerASCIIPopTab(viewer);CHKERRQ(ierr);
  }

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatPartitioningSetType"
/*@C
   MatPartitioningSetType - Sets the type of partitioner to use

   Collective on MatPartitioning

   Input Parameter:
.  part - the partitioning context.
.  type - a known method

   Options Database Command:
$  -mat_partitioning_type  <type>
$      Use -help for a list of available methods
$      (for instance, parmetis)

   Level: intermediate

.keywords: partitioning, set, method, type

.seealso: MatPartitioningCreate(), MatPartitioningApply()

@*/
int MatPartitioningSetType(MatPartitioning part,MatPartitioningType type)
{
  int        ierr,(*r)(MatPartitioning);
  PetscTruth match;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(part,MAT_PARTITIONING_COOKIE);
  PetscValidCharPointer(type);

  ierr = PetscTypeCompare((PetscObject)part,type,&match);CHKERRQ(ierr);
  if (match) PetscFunctionReturn(0);

  if (part->setupcalled) {
    ierr =  (*part->ops->destroy)(part);CHKERRQ(ierr);
    part->data        = 0;
    part->setupcalled = 0;
  }

  /* Get the function pointers for the method requested */
  if (!MatPartitioningRegisterAllCalled){ ierr = MatPartitioningRegisterAll(0);CHKERRQ(ierr);}
  ierr =  PetscFListFind(part->comm,MatPartitioningList,type,(void (**)(void)) &r);CHKERRQ(ierr);

  if (!r) {SETERRQ1(PETSC_ERR_ARG_OUTOFRANGE,"Unknown partitioning type %s",type);}

  part->ops->destroy      = (int (*)(MatPartitioning)) 0;
  part->ops->view         = (int (*)(MatPartitioning,PetscViewer)) 0;
  ierr = (*r)(part);CHKERRQ(ierr);

  ierr = PetscStrfree(part->type_name);CHKERRQ(ierr);
  ierr = PetscStrallocpy(type,&part->type_name);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatPartitioningSetFromOptions"
/*@
   MatPartitioningSetFromOptions - Sets various partitioning options from the 
        options database.

   Collective on MatPartitioning

   Input Parameter:
.  part - the partitioning context.

   Options Database Command:
$  -mat_partitioning_type  <type>
$      Use -help for a list of available methods
$      (for instance, parmetis)

   Level: beginner

.keywords: partitioning, set, method, type
@*/
int MatPartitioningSetFromOptions(MatPartitioning part)
{
  int        ierr;
  PetscTruth flag;
  char       type[256],*def;

  PetscFunctionBegin;
  if (!MatPartitioningRegisterAllCalled){ ierr = MatPartitioningRegisterAll(0);CHKERRQ(ierr);}
  ierr = PetscOptionsBegin(part->comm,part->prefix,"Partitioning options","MatOrderings");CHKERRQ(ierr);
    if (!part->type_name) {
#if defined(PETSC_HAVE_PARMETIS)
      def = MAT_PARTITIONING_PARMETIS;
#else
      def = MAT_PARTITIONING_CURRENT;
#endif
    } else {
      def = part->type_name;
    }
    ierr = PetscOptionsList("-mat_partitioning_type","Type of partitioner","MatPartitioningSetType",MatPartitioningList,def,type,256,&flag);CHKERRQ(ierr);
    if (flag) {
      ierr = MatPartitioningSetType(part,type);CHKERRQ(ierr);
    }
    /*
      Set the type if it was never set.
    */
    if (!part->type_name) {
      ierr = MatPartitioningSetType(part,def);CHKERRQ(ierr);
    }

    if (part->ops->setfromoptions) {
      ierr = (*part->ops->setfromoptions)(part);CHKERRQ(ierr);
    }
  ierr = PetscOptionsEnd();CHKERRQ(ierr);
  PetscFunctionReturn(0);
}






