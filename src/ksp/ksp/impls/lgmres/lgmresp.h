/* A. Baker */
/*
   Private data structure used by the LGMRES method.
*/

#if !defined(__LGMRES)
#define __LGMRES

#include "src/ksp/ksp/kspimpl.h" /*includes petscksp.h */

  typedef struct {
    /* Hessenberg matrix and orthogonalization information. */ 
    PetscScalar *hh_origin;   /* holds hessenburg matrix that has been
                            multiplied by plane rotations (upper tri) */
    PetscScalar *hes_origin;  /* holds the original (unmodified) hessenberg matrix 
                            which may be used to estimate the Singular Values
                            of the matrix */
    PetscScalar *cc_origin;   /* holds cosines for rotation matrices */
    PetscScalar *ss_origin;   /* holds sines for rotation matrices */
    PetscScalar *rs_origin;   /* holds the right-hand-side of the Hessenberg system */

    /* Work space for computing eigenvalues/singular values */
    PetscReal *Dsvd;
    PetscScalar *Rsvd;
      
    /* parameters */
    PetscReal haptol;            /* tolerance used for the "HAPPY BREAK DOWN"  */
    int    max_k;             /* maximum size of the approximation space  
                                 before restarting */

    int                       (*orthog)(KSP,int); /* orthogonalization function to use */
    KSPGMRESCGSRefinementType cgstype;
    
    Vec   *vecs;              /* holds the work vectors */
   
    int    q_preallocate;     /* 0 = don't pre-allocate space for work vectors */
    int    delta_allocate;    /* the number of vectors to allocate in each block 
                                 if not pre-allocated */
    int    vv_allocated;      /* vv_allocated is the number of allocated lgmres 
                                 direction vectors */
    
    int    vecs_allocated;    /* vecs_allocated is the total number of vecs 
                                 available - used to simplify the dynamic
                                 allocation of vectors */
   
    Vec    **user_work;       /* Since we may call the user "obtain_work_vectors" 
                                 several times, we have to keep track of the pointers
                                 that it has returned (so that we may free the 
                                 storage) */

    int    *mwork_alloc;      /* Number of work vectors allocated as part of
                                 a work-vector chunck */
    int    nwork_alloc;       /* Number of work-vector chunks allocated */


    /* In order to allow the solution to be constructed during the solution
       process, we need some additional information: */

    int    it;              /* Current iteration */
    PetscScalar *nrs;            /* temp that holds the coefficients of the 
                               Krylov vectors that form the minimum residual
                               solution */
    Vec    sol_temp;        /* used to hold temporary solution */


    /* LGMRES_MOD - make these for the z vectors - new storage for lgmres */
    Vec  *augvecs;            /* holds the error approximation vectors for lgmres. */
    Vec  **augvecs_user_work; /* same purpose as user_work above, but this one is
                                for our error approx vectors */
    int    aug_vv_allocated;      /* aug_vv_allocated is the number of allocated lgmres 
                                 augmentation vectors */
    int    aug_vecs_allocated;    /* aug_vecs_allocated is the total number of augmentation vecs 
                                 available - used to simplify the dynamic
                                 allocation of vectors */

    int   augwork_alloc;       /*size of chunk allocated for augmentation vectors */ 

    int   aug_dim;             /* max number of augmented directions to add */

    int   aug_ct;              /* number of aug. vectors available */ 

    int   *aug_order;          /*keeps track of order to use aug. vectors*/

    int   approx_constant;   /* = 1 then the approx space at each restart will 
                                  be  size max_k .  Therefore, more than (max_k - aug_dim) 
                                  krylov vectors may be used if less than aug_dim error 
                                  approximations are available (in the first few restarts,
                                  for example) to keep the space a constant size. */ 
  
    int   matvecs;            /*keep track of matvecs */

} KSP_LGMRES;


#define HH(a,b)  (lgmres->hh_origin + (b)*(lgmres->max_k+2)+(a)) 
                 /* HH will be size (max_k+2)*(max_k+1)  -  think of HH as 
                    being stored columnwise (inc. zeros) for access purposes. */
#define HES(a,b) (lgmres->hes_origin + (b)*(lgmres->max_k+1)+(a)) 
                  /* HES will be size (max_k + 1) * (max_k + 1) - 
                     again, think of HES as being stored columnwise */
#define CC(a)    (lgmres->cc_origin + (a)) /* CC will be length (max_k+1) - cosines */
#define SS(a)    (lgmres->ss_origin + (a)) /* SS will be length (max_k+1) - sines */
#define GRS(a)    (lgmres->rs_origin + (a)) /* GRS will be length (max_k+2) - rt side */

/* vector names */
#define VEC_OFFSET     2
#define VEC_SOLN       ksp->vec_sol                  /* solution */ 
#define VEC_RHS        ksp->vec_rhs                  /* right-hand side */
#define VEC_TEMP       lgmres->vecs[0]               /* work space */  
#define VEC_TEMP_MATOP lgmres->vecs[1]               /* work space */
#define VEC_VV(i)      lgmres->vecs[VEC_OFFSET+i]    /* use to access
                                                        othog basis vectors */
/*LGMRES_MOD */
#define AUG_OFFSET     1
#define AUGVEC(i)      lgmres->augvecs[AUG_OFFSET+i]   /*error approx vecors */
#define AUG_ORDER(i)   lgmres->aug_order[i]            /*order in which to augment */ 
#define A_AUGVEC(i)    lgmres->augvecs[AUG_OFFSET+i+lgmres->aug_dim] /*A times error vector */
#define AUG_TEMP       lgmres->augvecs[0]              /* work vector */ 
#endif



