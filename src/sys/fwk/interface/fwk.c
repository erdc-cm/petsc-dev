#define PETSC_DLL

#include "petscsys.h"
#include "petscfwk.h"

/* FIX: is it okay to include this directly? */
#include <ctype.h>

PETSCSYS_DLLEXPORT PetscClassId PETSC_FWK_CLASSID;
static char PETSC_FWK_CLASS_NAME[] = "PetscFwk";
static PetscTruth PetscFwkPackageInitialized = PETSC_FALSE;


typedef PetscErrorCode (*PetscFwkPythonLoadVTableFunction)(PetscFwk fwk, const char* path, const char* name, void **vtable);
typedef PetscErrorCode (*PetscFwkPythonClearVTableFunction)(PetscFwk fwk, void **vtable);
typedef PetscErrorCode (*PetscFwkPythonCallFunction)(PetscFwk fwk, const char* message, void *vtable);

EXTERN_C_BEGIN
PetscFwkPythonLoadVTableFunction      PetscFwkPythonLoadVTable      = PETSC_NULL;
PetscFwkPythonClearVTableFunction     PetscFwkPythonClearVTable     = PETSC_NULL;
PetscFwkPythonCallFunction            PetscFwkPythonCall            = PETSC_NULL;
EXTERN_C_END

#define PETSC_FWK_CHECKINIT_PYTHON()					\
  if(PetscFwkPythonLoadVTable == PETSC_NULL) {		        	\
    PetscErrorCode ierr;						\
    ierr = PetscPythonInitialize(PETSC_NULL,PETSC_NULL);CHKERRQ(ierr);	\
    if(PetscFwkPythonLoadVTable == PETSC_NULL) {			        \
      SETERRQ(PETSC_COMM_SELF,PETSC_ERR_LIB,				\
	      "Couldn't initialize Python support for PetscFwk");	\
    }									\
  }									
  
#define PETSC_FWK_LOAD_VTABLE_PYTHON(fwk, path, name)                   \
  PETSC_FWK_CHECKINIT_PYTHON();						\
  {									\
    PetscErrorCode ierr;                                                \
    ierr = PetscFwkPythonLoadVTable(fwk, path, name, &(fwk->vtable));   \
    if (ierr) { PetscPythonPrintError(); SETERRQ(PETSC_COMM_SELF,PETSC_ERR_LIB, "Python error"); } \
  }

#define PETSC_FWK_CLEAR_VTABLE_PYTHON(fwk)                              \
  PETSC_FWK_CHECKINIT_PYTHON();						\
  {									\
    PetscErrorCode ierr;                                                \
    ierr = PetscFwkPythonClearVTable(fwk, &(fwk->vtable));              \
    if (ierr) { PetscPythonPrintError(); SETERRQ(PETSC_COMM_SELF,PETSC_ERR_LIB, "Python error"); } \
  }
  
#define PETSC_FWK_CALL_PYTHON(fwk, message)                             \
  PETSC_FWK_CHECKINIT_PYTHON();                                         \
  {									\
    PetscErrorCode ierr;                                                \
    ierr = PetscFwkPythonCall(fwk, message, fwk->vtable);                           \
    if (ierr) { PetscPythonPrintError(); SETERRQ(PETSC_COMM_SELF,PETSC_ERR_LIB, "Python error"); } \
  }
  

/* ---------------------------------------------------------------------------------------------- */
struct _n_PetscFwkGraph {
  PetscInt vcount, vmax; /* actual and allocated number of vertices */
  PetscInt *i, *j, *outdegree; /* (A)IJ structure for the underlying matrix: 
                                  i[row]             the row offset into j, 
                                  i[row+1] - i[row]  allocated number of entries for row,
                                  outdegree[row]     actual number of entries in row
                               */
  PetscInt *indegree;
  PetscInt nz, maxnz;
  PetscInt rowreallocs, colreallocs;
};

typedef struct _n_PetscFwkGraph *PetscFwkGraph;

#define CHUNKSIZE 5
/*
    Inserts the (row,col) entry, allocating larger arrays, if necessary. 
    Does NOT check whether row and col are within range (< graph->vcount).
    Does NOT check whether the entry is already present.
*/
#undef  __FUNCT__
#define __FUNCT__ "PetscFwkGraphExpandRow_Private"
PetscErrorCode PetscFwkGraphExpandRow_Private(PetscFwkGraph graph, PetscInt row) {
  PetscErrorCode ierr;
  PetscInt rowlen, rowmax, rowoffset;
  PetscInt ii;
  PetscFunctionBegin;
  rowlen = graph->outdegree[row]; 
  rowmax = graph->i[row+1] - graph->i[row]; 
  rowoffset = graph->i[row];
  if (rowlen >= rowmax) {
    /* there is no extra room in row, therefore enlarge */              
    PetscInt   new_nz = graph->i[graph->vcount] + CHUNKSIZE;  
    PetscInt   *new_i=0,*new_j=0;                            
    
    /* malloc new storage space */ 
    ierr = PetscMalloc(new_nz*sizeof(PetscInt),&new_j); CHKERRQ(ierr);
    ierr = PetscMalloc((graph->vmax+1)*sizeof(PetscInt),&new_i);CHKERRQ(ierr);
    
    /* copy over old data into new slots */ 
    for (ii=0; ii<row+1; ii++) {new_i[ii] = graph->i[ii];} 
    for (ii=row+1; ii<graph->vmax+1; ii++) {new_i[ii] = graph->i[ii]+CHUNKSIZE;} 
    ierr = PetscMemcpy(new_j,graph->j,(rowoffset+rowlen)*sizeof(PetscInt));CHKERRQ(ierr); 
    ierr = PetscMemcpy(new_j+rowoffset+rowlen+CHUNKSIZE,graph->j+rowoffset+rowlen,(new_nz - CHUNKSIZE - rowoffset - rowlen)*sizeof(PetscInt));CHKERRQ(ierr); 
    /* free up old matrix storage */ 
    ierr = PetscFree(graph->j);CHKERRQ(ierr);  
    ierr = PetscFree(graph->i);CHKERRQ(ierr);    
    graph->i = new_i; graph->j = new_j;  
    graph->maxnz     += CHUNKSIZE; 
    graph->colreallocs++; 
  } 
  PetscFunctionReturn(0);
}/* PetscFwkGraphExpandRow_Private() */

#undef  __FUNCT__
#define __FUNCT__ "PetscFwkGraphAddVertex"
PetscErrorCode PetscFwkGraphAddVertex(PetscFwkGraph graph, PetscInt *v) {
  PetscInt ii;
  PetscErrorCode ierr;
  PetscFunctionBegin;
  if(graph->vcount >= graph->vmax) {
    /* Add rows */
    PetscInt   *new_i=0, *new_outdegree=0, *new_indegree;                            
    
    /* malloc new storage space */ 
    ierr = PetscMalloc((graph->vmax+CHUNKSIZE+1)*sizeof(PetscInt),&new_i);CHKERRQ(ierr);
    ierr = PetscMalloc((graph->vmax+CHUNKSIZE)*sizeof(PetscInt),&new_outdegree); CHKERRQ(ierr);
    ierr = PetscMalloc((graph->vmax+CHUNKSIZE)*sizeof(PetscInt),&new_indegree); CHKERRQ(ierr);
    ierr = PetscMemzero(new_outdegree, (graph->vmax+CHUNKSIZE)*sizeof(PetscInt)); CHKERRQ(ierr);
    ierr = PetscMemzero(new_indegree, (graph->vmax+CHUNKSIZE)*sizeof(PetscInt)); CHKERRQ(ierr);


    /* copy over old data into new slots */ 
    ierr = PetscMemcpy(new_i,graph->i,(graph->vmax+1)*sizeof(PetscInt));CHKERRQ(ierr); 
    ierr = PetscMemcpy(new_outdegree,graph->outdegree,(graph->vmax)*sizeof(PetscInt));CHKERRQ(ierr); 
    ierr = PetscMemcpy(new_indegree,graph->indegree,(graph->vmax)*sizeof(PetscInt));CHKERRQ(ierr); 
    for (ii=graph->vmax+1; ii<=graph->vmax+CHUNKSIZE; ++ii) {
      new_i[ii] = graph->i[graph->vmax];
    }

    /* free up old matrix storage */ 
    ierr = PetscFree(graph->i);CHKERRQ(ierr);  
    ierr = PetscFree(graph->outdegree);CHKERRQ(ierr);    
    ierr = PetscFree(graph->indegree);CHKERRQ(ierr);    
    graph->i = new_i; graph->outdegree = new_outdegree; graph->indegree = new_indegree;
    
    graph->vmax += CHUNKSIZE; 
    graph->rowreallocs++; 
  }
  if(v) {*v = graph->vcount;}
  ++(graph->vcount);
  PetscFunctionReturn(0);
}/* PetscFwkGraphAddVertex() */

#undef  __FUNCT__
#define __FUNCT__ "PetscFwkGraphAddEdge"
PetscErrorCode PetscFwkGraphAddEdge(PetscFwkGraph graph, PetscInt row, PetscInt col) {
  PetscErrorCode        ierr;
  PetscInt              *rp,low,high,t,ii,i;
  PetscFunctionBegin;

  if(row < 0 || row >= graph->vcount) {
    SETERRQ3(PETSC_COMM_SELF,PETSC_ERR_ARG_OUTOFRANGE,"Source vertex %D out of range: min %D max %D",row, 0, graph->vcount);
  }
  if(col < 0 || col >= graph->vcount) {
    SETERRQ3(PETSC_COMM_SELF,PETSC_ERR_ARG_OUTOFRANGE,"Target vertex %D out of range: min %D max %D",col, 0, graph->vcount);
  }
  rp   = graph->j + graph->i[row];
  low  = 0;
  high = graph->outdegree[row];
  while (high-low > 5) {
    t = (low+high)/2;
    if (rp[t] > col) high = t;
    else             low  = t;
  }
  for (i=low; i<high; ++i) {
    if (rp[i] > col) break;
    if (rp[i] == col) {
      goto we_are_done;
    }
  } 
  ierr = PetscFwkGraphExpandRow_Private(graph, row); CHKERRQ(ierr);
  /* 
     If the graph was empty before, graph->j was NULL and rp was NULL as well.  
     Now that the row has been expanded, rp needs to be reset. 
  */
  rp = graph->j + graph->i[row];
  /* shift up all the later entries in this row */
  for (ii=graph->outdegree[row]; ii>=i; --ii) {
    rp[ii+1] = rp[ii];
  }
  rp[i] = col; 
  ++(graph->outdegree[row]);
  ++(graph->indegree[col]);
  ++(graph->nz);
  
 we_are_done:
  PetscFunctionReturn(0);
}/* PetscFwkGraphAddEdge() */


#undef  __FUNCT__
#define __FUNCT__ "PetscFwkGraphTopologicalSort"
PetscErrorCode PetscFwkGraphTopologicalSort(PetscFwkGraph graph, PetscInt *n, PetscInt **queue) {
  PetscTruth *queued;
  PetscInt   *indegree;
  PetscInt ii, k, jj, Nqueued = 0;
  PetscTruth progress;
  PetscErrorCode ierr;
  PetscFunctionBegin;
  if(!n || !queue) {
    SETERRQ(PETSC_COMM_SELF,PETSC_ERR_ARG_WRONG, "Invalid return argument pointers n or vertices");
  }
  *n = graph->vcount;
  ierr = PetscMalloc(sizeof(PetscInt)*graph->vcount, queue); CHKERRQ(ierr);
  ierr = PetscMalloc(sizeof(PetscTruth)*graph->vcount, &queued); CHKERRQ(ierr);
  ierr = PetscMalloc(sizeof(PetscInt)*graph->vcount, &indegree); CHKERRQ(ierr);
  for(ii = 0; ii < graph->vcount; ++ii) {
    queued[ii]   = PETSC_FALSE;
    indegree[ii] = graph->indegree[ii];
  }
  while(Nqueued < graph->vcount) {
    progress = PETSC_FALSE;
    for(ii = 0; ii < graph->vcount; ++ii) {
      /* If ii is not queued yet, and the indegree is 0, queue it. */ 
      if(!queued[ii] && !indegree[ii]) {
        (*queue)[Nqueued] = ii;
        queued[ii] = PETSC_TRUE;
        ++Nqueued;
        progress = PETSC_TRUE;
        /* Reduce the indegree of all vertices in row ii */
        for(k = 0; k < graph->outdegree[ii]; ++k) {
          jj = graph->j[graph->i[ii]+k];
          --(indegree[jj]);
          /* 
             It probably would be more efficient to make a recursive call to the body of the for loop 
             with the jj in place of ii, but we use a simple-minded algorithm instead, since the graphs
             we anticipate encountering are tiny. 
          */
        }/*for(k)*/
      }/* if(!queued) */
    }/* for(ii) */
    /* If no progress was made during this iteration, the graph must have a cycle */
    if(!progress) {
      SETERRQ(PETSC_COMM_SELF,PETSC_ERR_ARG_WRONGSTATE, "Cycle detected in the dependency graph");
    }
  }/* while(Nqueued) */
  ierr = PetscFree(queued); CHKERRQ(ierr);
  ierr = PetscFree(indegree); CHKERRQ(ierr);
  PetscFunctionReturn(0);
}/* PetscFwkGraphTopologicalSort() */

#undef  __FUNCT__
#define __FUNCT__ "PetscFwkGraphDestroy"
PetscErrorCode PetscFwkGraphDestroy(PetscFwkGraph graph) {
  PetscErrorCode ierr;
  PetscFunctionBegin;
  ierr = PetscFree(graph->i);        CHKERRQ(ierr);
  ierr = PetscFree(graph->j);        CHKERRQ(ierr);
  ierr = PetscFree(graph->outdegree);     CHKERRQ(ierr);
  ierr = PetscFree(graph->indegree); CHKERRQ(ierr);
  ierr = PetscFree(graph);           CHKERRQ(ierr);
  graph = PETSC_NULL;
  PetscFunctionReturn(0);
}/* PetscFwkGraphDestroy() */

#undef  __FUNCT__
#define __FUNCT__ "PetscFwkGraphCreate"
PetscErrorCode PetscFwkGraphCreate(PetscFwkGraph *graph_p) {
  PetscFwkGraph graph;
  PetscErrorCode ierr;
  PetscFunctionBegin;
  ierr = PetscNew(struct _n_PetscFwkGraph, graph_p);   CHKERRQ(ierr);
  graph = *graph_p;
  graph->vcount = graph->vmax = graph->nz = graph->maxnz = graph->rowreallocs = graph->colreallocs = 0;
  ierr = PetscMalloc(sizeof(PetscInt), &(graph->i)); CHKERRQ(ierr);
  graph->j = graph->outdegree = graph->indegree = PETSC_NULL;
  PetscFunctionReturn(0);
}/* PetscFwkGraphCreate() */



/* ------------------------------------------------------------------------------------------------------- */

typedef enum{PETSC_FWK_VTABLE_NONE, PETSC_FWK_VTABLE_SO, PETSC_FWK_VTABLE_PY} PetscFwkVTableType;

struct _n_PetscFwkVTable_SO {
  char           *path, *name;
};


struct _p_PetscFwk {
  PETSCHEADER(int);
  PetscFwkVTableType        vtable_type;
  void                      *vtable;
  char *                    url;
  PetscInt                  N, maxN;
  PetscFwk                  *component;
  PetscFwkGraph             dep_graph;
};

static PetscFwk defaultFwk = PETSC_NULL;



/* ------------------------------------------------------------------------------------------------------- */

typedef PetscErrorCode (*PetscFwkCallFunction)(PetscFwk, const char*);
typedef PetscErrorCode (*PetscFwkMessageFunction)(PetscFwk);
typedef void (*QueryFunction)(void);

/* 
   Had to essentially duplicate a bunch of code from dl.c to override PetscDLLibrarySym,
   because it fails when it can't find a symbol.
   Not so cool, because we want to try two different symbols.
   Maybe PetscDLLibrarySym can be fixed instead.
*/
#include "../src/sys/dll/dlimpl.h"
struct _n_PetscDLLibrary {
  PetscDLLibrary next;
  PetscDLHandle  handle;
  char           libname[PETSC_MAX_PATH_LEN];
};
static PetscDLLibrary PetscFwkDLLibrariesLoaded = 0;

#undef  __FUNCT__
#define __FUNCT__ "PetscFwkDLLibrarySym"
PetscErrorCode PETSCSYS_DLLEXPORT PetscFwkDLLibrarySym(MPI_Comm comm, PetscDLLibrary *outlist, const char path[],const char symbol[],void **value)
{
  char           libname[PETSC_MAX_PATH_LEN],suffix[16],*s;
  PetscDLLibrary nlist,prev,list;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidPointer(outlist,2);
  if (path) PetscValidCharPointer(path,3);
  PetscValidCharPointer(symbol,4);
  PetscValidPointer(value,5);

  list   = *outlist;
  *value = 0;

  /*
       Function name does include library 
       -------------------------------------
  */
  if (path && path[0] != '\0') {
    /* copy path and remove suffix from libname */
    ierr = PetscStrncpy(libname,path,PETSC_MAX_PATH_LEN);CHKERRQ(ierr);
    ierr = PetscStrcpy(suffix,".");CHKERRQ(ierr);
    ierr = PetscStrcat(suffix,PETSC_SLSUFFIX);CHKERRQ(ierr);
    ierr = PetscStrrstr(libname,suffix,&s);CHKERRQ(ierr);
    if (s) s[0] = 0;
    /* Look if library is already opened and in path */
    prev  = 0;
    nlist = list;
    while (nlist) {
      PetscTruth match;
      ierr = PetscStrcmp(nlist->libname,libname,&match);CHKERRQ(ierr);
      if (match) goto done;
      prev  = nlist;
      nlist = nlist->next;
    }
    /* open the library and append it to path */
    ierr = PetscDLLibraryOpen(comm,path,&nlist);CHKERRQ(ierr);
    ierr = PetscInfo1(0,"Appending %s to dynamic library search path\n",path);CHKERRQ(ierr);
    if (prev) { prev->next = nlist; }
    else      { *outlist   = nlist; }

  done:;
    ierr = PetscDLSym(nlist->handle,symbol,value);CHKERRQ(ierr);
    if (*value) {
      ierr = PetscInfo2(0,"Loading function %s from dynamic library %s\n",symbol,path);CHKERRQ(ierr);
    }

  /*
       Function name does not include library so search path
       -----------------------------------------------------
  */
  } else {
    while (list) {
      ierr = PetscDLSym(list->handle,symbol,value);CHKERRQ(ierr);
      if (*value) {
        ierr = PetscInfo2(0,"Loading symbol %s from dynamic library %s\n",symbol,list->libname);CHKERRQ(ierr);
        break;
      }
      list = list->next;
    }
    if (!*value) {
      ierr = PetscDLSym(PETSC_NULL,symbol,value);CHKERRQ(ierr);
      if (*value) {
        ierr = PetscInfo1(0,"Loading symbol %s from object code\n",symbol);CHKERRQ(ierr);
      }
    }
  }
  PetscFunctionReturn(0);
}/* PetscFwkDLLibrarySym() */

#undef  __FUNCT__
#define __FUNCT__ "PetscFwkCall_SO"
PetscErrorCode PetscFwkCall_SO(PetscFwk fwk, const char* path, const char* name, const char* message) {
  size_t    namelen, messagelen, msgfunclen, callfunclen;
  char *msgfunc  = PETSC_NULL, *callfunc = PETSC_NULL;
  PetscFwkCallFunction call = PETSC_NULL;
  PetscFwkMessageFunction msg = PETSC_NULL;
  PetscErrorCode ierr;
  PetscFunctionBegin;
  ierr = PetscStrlen(name, &namelen); CHKERRQ(ierr);
  ierr = PetscStrlen(message, &messagelen); CHKERRQ(ierr);
  msgfunclen = namelen + messagelen;
  ierr = PetscMalloc(sizeof(char)*(msgfunclen+1), &msgfunc); CHKERRQ(ierr);
  msgfunc[0] = '\0';
  if(namelen){
    ierr = PetscStrcat(msgfunc, name); CHKERRQ(ierr);
  }
  ierr = PetscStrcat(msgfunc, message); CHKERRQ(ierr);
  if(namelen) {
    /* HACK: is 'toupper' part of the C standard? Looks like starting with C89. */
    msgfunc[namelen] = toupper(msgfunc[namelen]);
  }
  ierr = PetscFwkDLLibrarySym(((PetscObject)fwk)->comm, &PetscFwkDLLibrariesLoaded, path, msgfunc, (void **)(&msg)); CHKERRQ(ierr);
  ierr = PetscFree(msgfunc); CHKERRQ(ierr);
  if(msg) {
    ierr = (*msg)(fwk); CHKERRQ(ierr);
    PetscFunctionReturn(0);
  }
  callfunclen        = namelen+4;
  ierr = PetscMalloc(sizeof(char)*(callfunclen+1), &callfunc); CHKERRQ(ierr);
  if(namelen) {
    ierr = PetscStrcpy(callfunc, name); CHKERRQ(ierr);
  }
  if(namelen){
    ierr = PetscStrcat(callfunc, "Call"); CHKERRQ(ierr);
  }
  else {
    ierr = PetscStrcat(callfunc, "call"); CHKERRQ(ierr);
  }
  ierr = PetscFwkDLLibrarySym(((PetscObject)fwk)->comm, &PetscFwkDLLibrariesLoaded, path, callfunc, (void**)(&call)); CHKERRQ(ierr);
  ierr = PetscFree(callfunc); CHKERRQ(ierr);
  if(call) {
    ierr = (*call)(fwk, message); CHKERRQ(ierr);
    PetscFunctionReturn(0);
  }  
  SETERRQ2(PETSC_COMM_SELF, PETSC_ERR_ARG_WRONGSTATE, "PetscFwk '%s' cannot execute '%s'", ((PetscObject)fwk)->name, message);
  PetscFunctionReturn(0);
}/* PetscFwkCall_SO() */

/*
  Again, here we have to duplicate most of PetscFListFind, since it ultimately calls
  to PetscDLLibrarySym, which will raise an error when a symbol isn't found.
  And I don't think handling it using an error handler is efficient.
  Here we compress the code somewhat, since we know that path is NULL.
*/
EXTERN PetscDLLibrary DLLibrariesLoaded;
struct _n_PetscFList {
  void        (*routine)(void);   /* the routine */
  char        *path;              /* path of link library containing routine */
  char        *name;              /* string to identify routine */
  char        *rname;             /* routine name in dynamic library */
  PetscFList  next;               /* next pointer */
  PetscFList  next_list;          /* used to maintain list of all lists for freeing */
};

#undef  __FUNCT__
#define __FUNCT__ "PetscFwkFListFind"
PetscErrorCode PetscFwkFListFind(PetscFList fl,MPI_Comm comm,const char function[],QueryFunction *r) {
  PetscFList     entry = fl;
  PetscErrorCode ierr;
  PetscTruth     flg,f1,f2;
 
  PetscFunctionBegin;
  if (!function) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_ARG_NULL,"Trying to find routine with null name");

  *r = 0;

  while (entry) {
    flg = PETSC_FALSE;
    ierr = PetscStrcmp(function,entry->name,&f1);CHKERRQ(ierr);
    ierr = PetscStrcmp(function,entry->rname,&f2);CHKERRQ(ierr);
    flg =  (PetscTruth) (f1 || f2);

    if (flg) {
      if (entry->routine) {
        *r   = entry->routine; 
        PetscFunctionReturn(0);
      }
      /* it is not yet in memory so load from dynamic library */
#if defined(PETSC_HAVE_DYNAMIC_LIBRARIES)
      ierr = PetscFwkDLLibrarySym(comm,&DLLibrariesLoaded,entry->path,entry->rname,(void **)r);CHKERRQ(ierr);
      if (*r) {
        entry->routine = *r;
        PetscFunctionReturn(0);
      } else {
        PetscErrorPrintf("Unable to find function. Search path:\n");
        ierr = PetscDLLibraryPrintPath(DLLibrariesLoaded);CHKERRQ(ierr);
        SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_PLIB,"Unable to find function:%s: either it is mis-spelled or dynamic library is not in path",entry->rname);
      }
#endif
    }
    entry = entry->next;
  }

#if defined(PETSC_HAVE_DYNAMIC_LIBRARIES)
  /* Function never registered; try for it anyway; use PETSC_NULL for path */
  ierr = PetscFwkDLLibrarySym(comm,&DLLibrariesLoaded,PETSC_NULL,function,(void **)r);CHKERRQ(ierr);
  if (*r) {
    ierr = PetscFListAdd(&fl,function,function,*r);CHKERRQ(ierr);
  }
#endif
  PetscFunctionReturn(0);
}/* PetscFwkFListFind() */


#undef  __FUNCT__
#define __FUNCT__ "PetscFwkCall_NONE"
PetscErrorCode PetscFwkCall_NONE(PetscFwk fwk, const char* message) {
  PetscFwkCallFunction call = PETSC_NULL;
  PetscFwkMessageFunction msg = PETSC_NULL;
  PetscErrorCode ierr;
  PetscFunctionBegin;
  ierr = PetscFwkFListFind(((PetscObject)fwk)->qlist, ((PetscObject)fwk)->comm, message, (QueryFunction*)(&msg)); CHKERRQ(ierr);
  if(msg) {
    ierr = (*msg)(fwk); CHKERRQ(ierr);
    PetscFunctionReturn(0);
  }
  ierr = PetscFwkFListFind(((PetscObject)fwk)->qlist, ((PetscObject)fwk)->comm, "call", (QueryFunction*)(&call)); CHKERRQ(ierr);
  if(call) {
    ierr = (*call)(fwk, message); CHKERRQ(ierr);
    PetscFunctionReturn(0);
  }
  SETERRQ2(PETSC_COMM_SELF, PETSC_ERR_ARG_WRONGSTATE, "PetscFwk '%s' cannot execute '%s'", ((PetscObject)fwk)->name, message);
  PetscFunctionReturn(0);
}/* PetscFwkCall_NONE() */

#undef  __FUNCT__
#define __FUNCT__ "PetscFwkCall"
PetscErrorCode PetscFwkCall(PetscFwk fwk, const char* message) {
  PetscErrorCode ierr;
  PetscFunctionBegin;
  if(!message || !message[0]) {
    SETERRQ(PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Null or empty message string");
  }
  switch(fwk->vtable_type) {
  case PETSC_FWK_VTABLE_NONE:
    ierr = PetscFwkCall_NONE(fwk, message); CHKERRQ(ierr);
    break;
  case PETSC_FWK_VTABLE_SO:
    ierr = PetscFwkCall_SO(fwk, 
                           ((struct _n_PetscFwkVTable_SO*)fwk->vtable)->path, 
                           ((struct _n_PetscFwkVTable_SO*)fwk->vtable)->name, 
                           message); 
    CHKERRQ(ierr);
    break;
  case PETSC_FWK_VTABLE_PY:
    PETSC_FWK_CALL_PYTHON(fwk, message);
    break;
  default:
    SETERRQ(PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Unknown PetscFwkVTableType value");
    break;
  }
  PetscFunctionReturn(0);
}/* PetscFwkCall() */


#define PETSC_FWK_MAX_URL_LENGTH 1024
/* 
   Normalize the url (by truncating to PETSC_FWK_MAX_URL_LENGTH) and parse it to find out the component type and location.
   Warning: the returned char pointers are borrowed and their contents must be copied elsewhere to be preserved.
*/
#undef  __FUNCT__
#define __FUNCT__ "PetscFwkParseURL_Private"
PetscErrorCode PETSCSYS_DLLEXPORT PetscFwkParseURL_Private(const char inurl[], char **outpath, char **outname, PetscFwkVTableType *outtype){
  char *n, *s;
  static PetscInt nlen = PETSC_FWK_MAX_URL_LENGTH;
  static char path[PETSC_FWK_MAX_URL_LENGTH+1], name[PETSC_FWK_MAX_URL_LENGTH+1];
  PetscFwkVTableType type = PETSC_FWK_VTABLE_NONE;
  PetscErrorCode ierr;
  PetscFunctionBegin;
  /* FIX: this routine should replace the filesystem path by an abolute path for real normalization */
  /* Copy the inurl so we can manipulate it inplace and also truncate to the max allowable length */
  ierr = PetscStrncpy(path, inurl, nlen); CHKERRQ(ierr);  
  /* Split url <path>:<name> into <path> and <name> */
  ierr = PetscStrrchr(path,':',&n); CHKERRQ(ierr);
  /* Make sure it's not the ":/" of the "://" separator */
  if(!n[0] || n[0] == '/') {
    SETERRQ2(PETSC_COMM_SELF,PETSC_ERR_ARG_WRONG, 
           "Could not locate component name within the URL.\n"
           "Must have url = [<path/><library>:]<name>.\n"
           "Instead got %s\n"
           "Remember that URL is always truncated to the max allowed length of %d", 
           inurl, nlen);
  }
  /* Copy n to name */
  ierr = PetscStrcpy(name, n); CHKERRQ(ierr);
  /* If n isn't the whole path (i.e., there is a ':' separator), end 'path' right before the located ':' */
  if(n == path) {
    /* 
       No library is provided, so the component is assumed to be "local", that is
       defined in an already loaded executable. So we set type to .so, path to "",
       and look for the configure symbol among already loaded symbols 
       (or count on PetscDLXXX to do that.
    */
    type = PETSC_FWK_VTABLE_SO;
    path[0] = '\0';
  }
  else {
    n[-1] = '\0';
    /* Find the library suffix and determine the component library type: .so or .py */
    ierr = PetscStrrchr(path,'.',&s);CHKERRQ(ierr);
    /* FIX: we should really be using PETSc's internally defined suffices */
    if(s != path && s[-1] == '.') {
      if((s[0] == 'a' && s[1] == '\0') || (s[0] == 's' && s[1] == 'o' && s[2] == '\0')){
        type = PETSC_FWK_VTABLE_SO;
      }
      else if (s[0] == 'p' && s[1] == 'y' && s[2] == '\0'){
        type = PETSC_FWK_VTABLE_PY;
      }
      else {
        SETERRQ3(PETSC_COMM_SELF,PETSC_ERR_ARG_WRONG, 
                 "Unknown library suffix within the URL.\n"
                 "Must have url = [<path/><library>:]<name>,\n"
                 "where library = <libname>.<suffix>, suffix = .a || .so || .py.\n"
                 "Instead got url %s\n"
                 "Remember that URL is always truncated to the max allowed length of %d", 
                 inurl, s,nlen);     
      }
    }
  }
  *outpath = path;
  *outname = name;
  *outtype = type;
  PetscFunctionReturn(0);
}/* PetscFwkParseURL_Private() */

#undef  __FUNCT__
#define __FUNCT__ "PetscFwkClearURL_Private"
PetscErrorCode PETSCSYS_DLLEXPORT PetscFwkClearURL_Private(PetscFwk fwk) {
  PetscErrorCode ierr;
  PetscFunctionBegin;
  switch(fwk->vtable_type) {
  case PETSC_FWK_VTABLE_SO:
    {
      struct _n_PetscFwkVTable_SO *vt = (struct _n_PetscFwkVTable_SO*)(fwk->vtable);
      ierr = PetscFree(vt->path); CHKERRQ(ierr);
      ierr = PetscFree(vt->name); CHKERRQ(ierr);
      ierr = PetscFree(vt);       CHKERRQ(ierr);
      fwk->vtable = PETSC_NULL;
      fwk->vtable_type = PETSC_FWK_VTABLE_NONE;
    }
    break;
  case PETSC_FWK_VTABLE_PY:
    PETSC_FWK_CLEAR_VTABLE_PYTHON(fwk);
    break;
  case PETSC_FWK_VTABLE_NONE:
    break;
  default:
    SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_ARG_WRONGSTATE, 
             "Unknown PetscFwk vtable type: %d", fwk->vtable_type);
  }
  ierr = PetscFree(fwk->url);  CHKERRQ(ierr);
  fwk->url = PETSC_NULL;
  PetscFunctionReturn(0);
}/* PetscFwkClearURL_Private() */

#undef  __FUNCT__
#define __FUNCT__ "PetscFwkSetURL"
PetscErrorCode PETSCSYS_DLLEXPORT PetscFwkSetURL(PetscFwk fwk, const char url[]) {
  PetscErrorCode ierr;
  char *path, *name;
  PetscFunctionBegin;
  if(fwk->vtable) {
    ierr = PetscFwkClearURL_Private(fwk); CHKERRQ(ierr);
  }
  ierr = PetscStrallocpy(url,  &(fwk->url));  CHKERRQ(ierr);
  ierr = PetscFwkParseURL_Private(url, &path, &name, &fwk->vtable_type); CHKERRQ(ierr);
  switch(fwk->vtable_type) {
  case PETSC_FWK_VTABLE_SO:
    {
      struct _n_PetscFwkVTable_SO *vt;
      ierr = PetscMalloc(sizeof(struct _n_PetscFwkVTable_SO), &(vt));
      fwk->vtable = (void*)vt;
      ierr = PetscStrallocpy(path, &vt->path); CHKERRQ(ierr);
      ierr = PetscStrallocpy(name, &vt->name); CHKERRQ(ierr);
    }
    break;
  case PETSC_FWK_VTABLE_PY:
    PETSC_FWK_LOAD_VTABLE_PYTHON(fwk, path, name);
    break;
  default:
    SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_ARG_WRONGSTATE, 
             "Unknown PetscFwk vtable type: %d", fwk->vtable_type);
  }
  PetscFunctionReturn(0);
}/* PetscFwkSetURL() */

#undef  __FUNCT__
#define __FUNCT__ "PetscFwkGetURL"
PetscErrorCode PETSCSYS_DLLEXPORT PetscFwkGetURL(PetscFwk fwk, const char **url) {
  PetscFunctionBegin;
  *url = fwk->url;
  PetscFunctionReturn(0);
}/* PetscFwkGetURL() */
 

/* ------------------------------------------------------------------------------------------------------- */
#undef  __FUNCT__
#define __FUNCT__ "PetscFwkView"
PetscErrorCode PetscFwkView(PetscFwk fwk, PetscViewer viewer) {
  PetscInt *vertices, N;
  PetscInt i, id;
  PetscTruth        iascii;
  PetscErrorCode    ierr;
  PetscFunctionBegin;
  PetscValidHeaderSpecific(fwk,PETSC_FWK_CLASSID,1);
  PetscValidType(fwk,1);
  if (!viewer) {
    ierr = PetscViewerASCIIGetStdout(((PetscObject)fwk)->comm,&viewer);CHKERRQ(ierr);
  }
  PetscValidHeaderSpecific(viewer,PETSC_VIEWER_CLASSID,2);
  PetscCheckSameComm(fwk,1,viewer,2);
  ierr = PetscTypeCompare((PetscObject)viewer,PETSCVIEWERASCII,&iascii);CHKERRQ(ierr);
  if (iascii) {
    ierr = PetscViewerASCIIPrintf(viewer, "Framework url: %s\n", fwk->url); CHKERRQ(ierr);
    ierr = PetscViewerASCIIPrintf(viewer, "Components are visited in this order:\n");
    ierr = PetscFwkGraphTopologicalSort(fwk->dep_graph, &N, &vertices); CHKERRQ(ierr);
    for(i = 0; i < N; ++i) {
      if(i) {
        ierr = PetscViewerASCIIPrintf(viewer, "\n"); CHKERRQ(ierr);
      }
      id = vertices[i];
      ierr = PetscViewerASCIIPrintf(viewer, "%d: name:%s, url:%s", id, ((PetscObject)(fwk->component[id]))->name, fwk->component[id]->url); CHKERRQ(ierr);
    }
    ierr = PetscViewerASCIIPrintf(viewer, "\n"); CHKERRQ(ierr);
    ierr = PetscFree(vertices); CHKERRQ(ierr);
  }
  else {
    SETERRQ(PETSC_COMM_SELF,PETSC_ERR_ARG_WRONG, "PetscFwk can only be viewed with an ASCII viewer");
  }
  PetscFunctionReturn(0);
}/* PetscFwkView() */


#undef  __FUNCT__
#define __FUNCT__ "PetscFwkVisit"
PetscErrorCode PetscFwkVisit(PetscFwk fwk, const char* message){
  PetscInt i, id, N, *vertices;
  PetscFwk component, visitor;
  PetscErrorCode ierr;
  PetscFunctionBegin;
  ierr = PetscFwkGraphTopologicalSort(fwk->dep_graph, &N, &vertices); CHKERRQ(ierr);
  for(i = 0; i < N; ++i) {
    id = vertices[i];
    component = fwk->component[id];
    /* Save the component's visitor and set fwk as the current visitor. */
    ierr = PetscObjectQuery((PetscObject)component, "visitor", (PetscObject*)(&visitor)); CHKERRQ(ierr);
    ierr = PetscObjectCompose((PetscObject)component, "visitor", (PetscObject) fwk);      CHKERRQ(ierr);
    /* Call "configure" */
    ierr = PetscFwkCall(component, message);    CHKERRQ(ierr);
    /* Restore visitor */
    ierr = PetscObjectCompose((PetscObject)component, "visitor", (PetscObject) visitor);  CHKERRQ(ierr);
  }
  ierr = PetscFree(vertices); CHKERRQ(ierr);
  PetscFunctionReturn(0);
}/* PetscFwkVisit() */

#undef  __FUNCT__
#define __FUNCT__ "PetscFwkGetKeyID_Private"
PetscErrorCode PETSCSYS_DLLEXPORT PetscFwkGetKeyID_Private(PetscFwk fwk, const char key[], PetscInt *_id, PetscTruth *_found){
  PetscInt i;
  PetscTruth eq;
  PetscErrorCode ierr;
  PetscFunctionBegin;
  /* Check whether a component with the given key has already been registered. */
  if(_found){*_found = PETSC_FALSE;}
  for(i = 0; i < fwk->N; ++i) {
    ierr = PetscStrcmp(key, ((PetscObject)fwk->component[i])->name, &eq); CHKERRQ(ierr);
    if(eq) {
      if(_id) {*_id = i;}
      if(_found){*_found = PETSC_TRUE;}
    }
  }
  PetscFunctionReturn(0);
}/* PetscFwkGetKeyID_Private() */

#undef  __FUNCT__
#define __FUNCT__ "PetscFwkRegisterKey_Private"
PetscErrorCode PETSCSYS_DLLEXPORT PetscFwkRegisterKey_Private(PetscFwk fwk, const char key[], PetscInt *_id) {
  PetscInt v, id;
  PetscTruth found;
  PetscErrorCode ierr;
  PetscFunctionBegin;
  /* Check whether a component with the given url has already been registered.  If so, return its id, if it has been requested. */
  ierr = PetscFwkGetKeyID_Private(fwk, key, &id, &found); CHKERRQ(ierr);
  if(found) {
    goto alldone;
  }
  /***/
  /* No such key found. */
  /* Create a new component for this key. */
  if(fwk->N >= fwk->maxN) {
    /* No more empty component slots, therefore, expand the component array */
    PetscFwk *new_components;
    ierr = PetscMalloc(sizeof(PetscFwk)*(fwk->maxN+CHUNKSIZE), &new_components);   CHKERRQ(ierr);
    ierr = PetscMemcpy(new_components, fwk->component, sizeof(PetscFwk)*(fwk->maxN)); CHKERRQ(ierr);
    ierr = PetscMemzero(new_components+fwk->maxN,sizeof(PetscFwk)*(CHUNKSIZE));    CHKERRQ(ierr);
    ierr = PetscFree(fwk->component);                                              CHKERRQ(ierr);
    fwk->component = new_components;
    fwk->maxN += CHUNKSIZE;
  }
  id = fwk->N;
  ++(fwk->N);
  /* Store key */
  /* Create the corresponding component. */
  ierr = PetscFwkCreate(((PetscObject)fwk)->comm, fwk->component+id); CHKERRQ(ierr);
  ierr = PetscObjectSetName((PetscObject)fwk->component[id], key);    CHKERRQ(ierr);
  /* Add a new vertex to the dependence graph.  This vertex will correspond to the newly registered component. */
  ierr = PetscFwkGraphAddVertex(fwk->dep_graph, &v); CHKERRQ(ierr);
  /* v must equal id */
  if(v != id) {
    SETERRQ3(PETSC_COMM_SELF,PETSC_ERR_ARG_CORRUPT, "New dependence graph vertex %d for key %s not the same as component id %d", v, key, id); 
  }
 alldone:
  if(_id) {
    *_id = id;
  }
  PetscFunctionReturn(0);
}/* PetscFwkRegisterKey_Private() */


#undef  __FUNCT__
#define __FUNCT__ "PetscFwkRegisterComponent"
PetscErrorCode PETSCSYS_DLLEXPORT PetscFwkRegisterComponent(PetscFwk fwk, const char key[]){
  PetscErrorCode ierr;
  PetscFunctionBegin;
  ierr = PetscFwkRegisterKey_Private(fwk, key, PETSC_NULL); CHKERRQ(ierr);
  PetscFunctionReturn(0);
}/* PetscFwkRegisterComponent() */

#undef  __FUNCT__
#define __FUNCT__ "PetscFwkRegisterComponentURL"
PetscErrorCode PETSCSYS_DLLEXPORT PetscFwkRegisterComponentURL(PetscFwk fwk, const char key[], const char url[]){
  PetscErrorCode ierr;
  PetscInt id;
  PetscFunctionBegin;
  ierr = PetscFwkRegisterKey_Private(fwk, key, &id); CHKERRQ(ierr);
  ierr = PetscFwkSetURL(fwk->component[id], url); CHKERRQ(ierr);
  PetscFunctionReturn(0);
}/* PetscFwkRegisterComponentURL() */


#undef  __FUNCT__
#define __FUNCT__ "PetscFwkRegisterDependence"
PetscErrorCode PETSCSYS_DLLEXPORT PetscFwkRegisterDependence(PetscFwk fwk, const char clientkey[], const char serverkey[])
{
  PetscInt clientid, serverid;
  PetscErrorCode ierr; 
  PetscFunctionBegin; 
  PetscValidCharPointer(clientkey,2);
  PetscValidCharPointer(serverkey,3);
  /* Register keys */
  ierr = PetscFwkRegisterKey_Private(fwk, clientkey, &clientid); CHKERRQ(ierr);
  ierr = PetscFwkRegisterKey_Private(fwk, serverkey, &serverid); CHKERRQ(ierr);
  /*
    Add the dependency edge to the dependence_graph as follows (serverurl, clienturl): 
     this means "server preceeds client", so server should be configured first.
  */
  ierr = PetscFwkGraphAddEdge(fwk->dep_graph, clientid, serverid); CHKERRQ(ierr);
  PetscFunctionReturn(0);
}/*PetscFwkRegisterDependence()*/



#undef  __FUNCT__
#define __FUNCT__ "PetscFwkDestroy"
PetscErrorCode PETSCSYS_DLLEXPORT PetscFwkDestroy(PetscFwk fwk)
{
  PetscInt i;
  PetscErrorCode ierr;
  if (--((PetscObject)fwk)->refct > 0) PetscFunctionReturn(0);
  for(i = 0; i < fwk->N; ++i){
    ierr = PetscObjectDestroy((PetscObject)fwk->component[i]); CHKERRQ(ierr);
  }
  ierr = PetscFree(fwk->component); CHKERRQ(ierr);
  ierr = PetscFwkGraphDestroy(fwk->dep_graph); CHKERRQ(ierr);
  ierr = PetscHeaderDestroy(fwk);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}/* PetscFwkDestroy()*/

#undef  __FUNCT__
#define __FUNCT__ "PetscFwkCreate"
PetscErrorCode PETSCSYS_DLLEXPORT PetscFwkCreate(MPI_Comm comm, PetscFwk *framework){
  PetscFwk fwk;
  PetscErrorCode ierr;
  PetscFunctionBegin;
#ifndef PETSC_USE_DYNAMIC_LIBRARIES
  ierr = PetscFwkInitializePackage(PETSC_NULL);CHKERRQ(ierr);
#endif
  PetscValidPointer(framework,2);
  ierr = PetscHeaderCreate(fwk,_p_PetscFwk,PetscInt,PETSC_FWK_CLASSID,0,"PetscFwk",comm,PetscFwkDestroy,PetscFwkView);CHKERRQ(ierr);
  fwk->component   = PETSC_NULL;
  fwk->vtable_type = PETSC_FWK_VTABLE_NONE;
  fwk->vtable      = PETSC_NULL;
  fwk->N = fwk->maxN = 0;
  /* FIX: should only create a graph on demand */
  ierr = PetscFwkGraphCreate(&fwk->dep_graph); CHKERRQ(ierr);
  ierr = PetscObjectChangeTypeName((PetscObject)fwk,PETSCFWK);CHKERRQ(ierr);
  *framework = fwk;
  PetscFunctionReturn(0);
}/* PetscFwkCreate() */


#undef  __FUNCT__
#define __FUNCT__ "PetscFwkGetComponent"
PetscErrorCode PETSCSYS_DLLEXPORT PetscFwkGetComponent(PetscFwk fwk, const char key[], PetscFwk *_component, PetscTruth *_found) {
  PetscInt id;
  PetscTruth found;
  PetscErrorCode ierr;
  PetscFunctionBegin;
  PetscValidCharPointer(key,2);
  ierr = PetscFwkGetKeyID_Private(fwk, key, &id, &found); CHKERRQ(ierr);
  if(found && _component) {
    *_component = fwk->component[id];
  }
  if(_found) {*_found = found;}
  PetscFunctionReturn(0);
}/* PetscFwkGetComponent() */



#undef  __FUNCT__
#define __FUNCT__ "PetscFwkFinalizePackage"
PetscErrorCode PetscFwkFinalizePackage(void){
  PetscErrorCode ierr;
  PetscFunctionBegin;
  if(defaultFwk) {
    ierr = PetscFwkDestroy(defaultFwk); CHKERRQ(ierr);
  }
  PetscFwkPackageInitialized = PETSC_FALSE;
  PetscFunctionReturn(0);
}/* PetscFwkFinalizePackage() */


#undef  __FUNCT__
#define __FUNCT__ "PetscFwkInitializePackage"
PetscErrorCode PetscFwkInitializePackage(const char path[]){
  PetscErrorCode ierr;
  PetscFunctionBegin;
  if(PetscFwkPackageInitialized) PetscFunctionReturn(0);
  PetscFwkPackageInitialized = PETSC_TRUE;
  /* Register classes */
  ierr = PetscClassIdRegister(PETSC_FWK_CLASS_NAME, &PETSC_FWK_CLASSID); CHKERRQ(ierr);
  /* Register finalization routine */
  ierr = PetscRegisterFinalize(PetscFwkFinalizePackage);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}/* PetscFwkInitializePackage() */

/* ---------------------------------------------------------------------*/
/*
    The variable Petsc_Fwk_default_keyval is used to indicate an MPI attribute that
  is attached to a communicator, in this case the attribute is a PetscFwk.
*/
static PetscMPIInt Petsc_Fwk_default_keyval = MPI_KEYVAL_INVALID;

#undef  __FUNCT__
#define __FUNCT__ "PETSC_FWK_DEFAULT_"
PetscFwk PETSCSYS_DLLEXPORT PETSC_FWK_DEFAULT_(MPI_Comm comm) {
  PetscErrorCode ierr;
  PetscTruth     flg;
  PetscFwk       fwk;

  PetscFunctionBegin;
  if (Petsc_Fwk_default_keyval == MPI_KEYVAL_INVALID) {
    ierr = MPI_Keyval_create(MPI_NULL_COPY_FN,MPI_NULL_DELETE_FN,&Petsc_Fwk_default_keyval,0);
    if (ierr) {PetscError(PETSC_COMM_SELF,__LINE__,"PETSC_FWK_DEFAULT_",__FILE__,__SDIR__,PETSC_ERR_PLIB,PETSC_ERROR_INITIAL," "); PetscFunctionReturn(0);}
  }
  ierr = MPI_Attr_get(comm,Petsc_Fwk_default_keyval,(void **)(&fwk),(PetscMPIInt*)&flg);
  if (ierr) {PetscError(PETSC_COMM_SELF,__LINE__,"PETSC_FWK_DEFAULT_",__FILE__,__SDIR__,PETSC_ERR_PLIB,PETSC_ERROR_INITIAL," "); PetscFunctionReturn(0);}
  if (!flg) { /* PetscFwk not yet created */
    ierr = PetscFwkCreate(comm, &fwk);
    if (ierr) {PetscError(PETSC_COMM_SELF,__LINE__,"PETSC_FWK_DEFAULT_",__FILE__,__SDIR__,PETSC_ERR_PLIB,PETSC_ERROR_INITIAL," "); PetscFunctionReturn(0);}
    ierr = PetscObjectRegisterDestroy((PetscObject)fwk);
    if (ierr) {PetscError(PETSC_COMM_SELF,__LINE__,"PETSC_FWK_DEFAULT_",__FILE__,__SDIR__,PETSC_ERR_PLIB,PETSC_ERROR_INITIAL," "); PetscFunctionReturn(0);}
    ierr = MPI_Attr_put(comm,Petsc_Fwk_default_keyval,(void*)fwk);
    if (ierr) {PetscError(PETSC_COMM_SELF,__LINE__,"PETSC_FWK_DEFAULT_",__FILE__,__SDIR__,PETSC_ERR_PLIB,PETSC_ERROR_INITIAL," "); PetscFunctionReturn(0);}
  } 
  PetscFunctionReturn(fwk);
}/* PETSC_FWK_DEFAULT_() */
