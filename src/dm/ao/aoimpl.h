/* 
   This private file should not be included in users' code.
*/

#ifndef __AOIMPL 
#define __AOIMPL

#include "petscao.h"

/*
    Defines the abstract AO operations
*/
struct _AOOps {
      /* Generic Operations */
  PetscErrorCode (*view)(AO, PetscViewer),
      (*destroy)(AO),
      /* AO-Specific Operations */
      (*petsctoapplication)(AO, int, int[]),
      (*applicationtopetsc)(AO, int, int[]),
      (*petsctoapplicationpermuteint)(AO, int, int[]),
      (*applicationtopetscpermuteint)(AO, int, int[]),
      (*petsctoapplicationpermutereal)(AO, int, double[]),
      (*applicationtopetscpermutereal)(AO, int, double[]);
};

struct _p_AO {
  PETSCHEADER(struct _AOOps)
  void          *data;                   /* implementation-specific data */
  int           N,n;                    /* global, local vector size */
};

/*
    Defines the abstract AOData operations
*/
struct _AODataOps {
  PetscErrorCode (*segmentadd)(AOData,const char[],const char[],int,int,int*,void*,PetscDataType);
  PetscErrorCode (*segmentget)(AOData,const char[],const char[],int,int*,void**);
  PetscErrorCode (*segmentrestore)(AOData,const char[],const char[],int,int*,void**);
  PetscErrorCode (*segmentgetlocal)(AOData,const char[],const char[],int,int*,void**);
  PetscErrorCode (*segmentrestorelocal)(AOData,const char[],const char[],int,int*,void**);
  PetscErrorCode (*segmentgetreduced)(AOData,const char[],const char[],int,int*,IS *);
  PetscErrorCode (*segmentgetextrema)(AOData,const char[],const char[],void *,void *);
  PetscErrorCode (*keyremap)(AOData,const char[],AO);
  PetscErrorCode (*keygetadjacency)(AOData,const char[],Mat*);
  PetscErrorCode (*keygetactive)(AOData,const char[],const char[],int,int*,int,IS*);
  PetscErrorCode (*keygetactivelocal)(AOData,const char[],const char[],int,int*,int,IS*);
  PetscErrorCode (*segmentpartition)(AOData,const char[],const char[]);
  PetscErrorCode (*keyremove)(AOData,const char[]);
  PetscErrorCode (*segmentremove)(AOData,const char[],const char[]);
  PetscErrorCode (*destroy)(AOData);
  PetscErrorCode (*view)(AOData,PetscViewer);
};

/*
      A AODate object consists of 

           - key1 
	       * name      = name of first key
               * N         = number of key entries
               * nlocal    = number of local key entries
               * nsegments = number of segments in first key  
               * ltog      = local to global mapping 
               - segment1 
                  * name      = name of first segment in first key
                  * bs        = blocksize of first segment in first key
                  * datatype  = datatype of first segment in first key

               - segment2

                  ....

            - key2

                ....
*/       
typedef struct __AODataSegment AODataSegment; 
struct __AODataSegment {
  void              *data;                   /* implementation-specific data */
  char              *name;
  int               bs;                      /* block size of basic chunk */
  PetscDataType     datatype;                /* type of data item, int, double etc */
  AODataSegment     *next;  
};

typedef struct __AODataKey AODataKey;
struct __AODataKey {
  void                   *data;                   /* implementation-specific data */
  char                   *name;
  int                    N;                       /* number of key entries */
  int                    nsegments;               /* number of segments in key */
  AODataSegment          *segments;
  ISLocalToGlobalMapping ltog;

  /* should the following be so public? */

  int                    nlocal;                  /* number of key entries owned locally */
  int                    *rowners;                /* ownership range of each processor */
  int                    rstart,rend;             /* first and 1 + last owned locally */
  AODataKey              *next;
};

typedef struct __AODataAlias AODataAlias;         /* allows a field or key to have several names */
struct __AODataAlias {
  char        *alias;
  char        *name;
  AODataAlias *next;
};

struct _p_AOData {
  PETSCHEADER(struct _AODataOps)
  int               nkeys;                   /* current number of keys */
  AODataKey         *keys;
  void              *data;
  int               datacomplete;            /* indicates all AOData object is fully set */
  AODataAlias       *aliases;
};

EXTERN PetscErrorCode AODataKeyFind_Private(AOData,const char[],PetscTruth *,AODataKey **);
EXTERN PetscErrorCode AODataSegmentFind_Private(AOData,const char[],const char[],PetscTruth *,AODataKey **,AODataSegment **);


#include "petscbt.h"

struct _p_AOData2dGrid {
   int       cell_n, vertex_n, edge_n;
   int       cell_max, vertex_max, edge_max;
   int       *cell_vertex,*cell_edge,*cell_cell;
   PetscReal *vertex;
   PetscReal xmin,xmax,ymin,ymax;
   int       *edge_vertex,*edge_cell;
   PetscBT   vertex_boundary;
};


#endif
