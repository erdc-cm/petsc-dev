/* 
 GAMG geometric-algebric multiogrid PC - Mark Adams 2011
 */

#include <../src/ksp/pc/impls/gamg/gamg.h>
#include <private/kspimpl.h>

#if defined(PETSC_HAVE_TRIANGLE) 
#define REAL PetscReal
#include <triangle.h>
#endif

#include <assert.h>
#include <petscblaslapack.h>

/* -------------------------------------------------------------------------- */
/*
   PCSetCoordinates_GEO

   Input Parameter:
   .  pc - the preconditioner context
*/
EXTERN_C_BEGIN
#undef __FUNCT__
#define __FUNCT__ "PCSetCoordinates_GEO"
PetscErrorCode PCSetCoordinates_GEO( PC pc, PetscInt ndm, PetscReal *coords )
{
  PC_MG          *mg = (PC_MG*)pc->data;
  PC_GAMG        *pc_gamg = (PC_GAMG*)mg->innerctx;
  PetscErrorCode ierr;
  PetscInt       arrsz,bs,my0,kk,ii,nloc,Iend;
  Mat            Amat = pc->pmat;

  PetscFunctionBegin;
  PetscValidHeaderSpecific( Amat, MAT_CLASSID, 1 );
  ierr  = MatGetBlockSize( Amat, &bs );               CHKERRQ( ierr );
  ierr  = MatGetOwnershipRange( Amat, &my0, &Iend ); CHKERRQ(ierr);
  nloc = (Iend-my0)/bs; 
  if((Iend-my0)%bs!=0) SETERRQ1(((PetscObject)Amat)->comm,PETSC_ERR_ARG_WRONG, "Bad local size %d.",nloc);

  pc_gamg->data_rows = 1;
  if( coords==0 && nloc > 0 ) {
    SETERRQ(((PetscObject)Amat)->comm,PETSC_ERR_ARG_WRONG, "Need coordinates for pc_gamg_type 'geo'.");
  }
  pc_gamg->data_cols = ndm; /* coordinates */

  arrsz = nloc*pc_gamg->data_rows*pc_gamg->data_cols;
  
  /* create data - syntactic sugar that should be refactored at some point */
  if (pc_gamg->data==0 || (pc_gamg->data_sz != arrsz)) {
    ierr = PetscFree( pc_gamg->data );  CHKERRQ(ierr);
    ierr = PetscMalloc((arrsz+1)*sizeof(PetscReal), &pc_gamg->data ); CHKERRQ(ierr);
  }
  for(kk=0;kk<arrsz;kk++)pc_gamg->data[kk] = -999.;
  pc_gamg->data[arrsz] = -99.;
  /* copy data in - column oriented */
  for( kk = 0 ; kk < nloc ; kk++ ){
    for( ii = 0 ; ii < ndm ; ii++ ) {
      pc_gamg->data[ii*nloc + kk] =  coords[kk*ndm + ii];
    }
  }
  assert(pc_gamg->data[arrsz] == -99.);
    
  pc_gamg->data_sz = arrsz;

  PetscFunctionReturn(0);
}
EXTERN_C_END

/* -------------------------------------------------------------------------- */
/*
   PCSetData_GEO

  Input Parameter:
   . pc - 
*/
#undef __FUNCT__
#define __FUNCT__ "PCSetData_GEO"
PetscErrorCode PCSetData_GEO( PC pc )
{
  PetscFunctionBegin;
  SETERRQ(((PetscObject)pc)->comm,PETSC_ERR_LIB,"GEO MG needs coordinates");
}

/* -------------------------------------------------------------------------- */
/*
   PCSetFromOptions_GEO

  Input Parameter:
   . pc - 
*/
#undef __FUNCT__
#define __FUNCT__ "PCSetFromOptions_GEO"
PetscErrorCode PCSetFromOptions_GEO( PC pc )
{
  PetscErrorCode  ierr;
  PC_MG           *mg = (PC_MG*)pc->data;
  PC_GAMG         *pc_gamg = (PC_GAMG*)mg->innerctx;
  
  PetscFunctionBegin;
  ierr = PetscOptionsHead("GAMG-GEO options"); CHKERRQ(ierr);
  {
    /* -pc_gamg_sa_nsmooths */
    /* pc_gamg_sa->smooths = 0; */
    /* ierr = PetscOptionsInt("-pc_gamg_agg_nsmooths", */
    /*                        "smoothing steps for smoothed aggregation, usually 1 (0)", */
    /*                        "PCGAMGSetNSmooths_AGG", */
    /*                        pc_gamg_sa->smooths, */
    /*                        &pc_gamg_sa->smooths, */
    /*                        &flag);  */
    /* CHKERRQ(ierr); */
  }
  ierr = PetscOptionsTail();CHKERRQ(ierr);
  
  /* call base class */
  ierr = PCSetFromOptions_GAMG( pc ); CHKERRQ(ierr);

  if( pc_gamg->verbose ) {
    PetscPrintf(PETSC_COMM_WORLD,"[%d]%s done\n",0,__FUNCT__);
  }

  PetscFunctionReturn(0);
}

/* -------------------------------------------------------------------------- */
/*
 PCCreateGAMG_GEO - nothing to do here now

  Input Parameter:
   . pc - 
*/
#undef __FUNCT__
#define __FUNCT__ "PCCreateGAMG_GEO"
PetscErrorCode  PCCreateGAMG_GEO( PC pc )
{
  PetscErrorCode  ierr;
  PC_MG           *mg = (PC_MG*)pc->data;
  PC_GAMG         *pc_gamg = (PC_GAMG*)mg->innerctx;

  PetscFunctionBegin;
  pc->ops->setfromoptions = PCSetFromOptions_GEO;
  /* pc->ops->destroy        = PCDestroy_GEO; */
  /* reset does not do anything; setup not virtual */

  /* set internal function pointers */
  pc_gamg->createprolongator = PCGAMGcreateProl_GEO;
  pc_gamg->createdefaultdata = PCSetData_GEO;
  
  ierr = PetscObjectComposeFunctionDynamic( (PetscObject)pc,
                                            "PCSetCoordinates_C",
                                            "PCSetCoordinates_GEO",
                                            PCSetCoordinates_GEO);

  PetscFunctionReturn(0);
}

/* -------------------------------------------------------------------------- */
/*
 triangulateAndFormProl

   Input Parameter:
   . selected_2 - list of selected local ID, includes selected ghosts
   . nnodes -
   . coords[2*nnodes] - column vector of local coordinates w/ ghosts
   . selected_1 - selected IDs that go with base (1) graph
   . locals_llist - linked list with (some) locality info of base graph
   . crsGID[selected.size()] - global index for prolongation operator
   . bs - block size
  Output Parameter:
   . a_Prol - prolongation operator
   . a_worst_best - measure of worst missed fine vertex, 0 is no misses
*/
#undef __FUNCT__
#define __FUNCT__ "triangulateAndFormProl"
PetscErrorCode triangulateAndFormProl( IS  selected_2, /* list of selected local ID, includes selected ghosts */
				       const PetscInt nnodes,
                                       const PetscReal coords[], /* column vector of local coordinates w/ ghosts */
                                       IS  selected_1, /* list of selected local ID, includes selected ghosts */
                                       IS  locals_llist, /* linked list from selected vertices of aggregate unselected vertices */
				       const PetscInt crsGID[],
                                       const PetscInt bs,
                                       Mat a_Prol, /* prolongation operator (output) */
                                       PetscReal *a_worst_best /* measure of worst missed fine vertex, 0 is no misses */
                                       )
{
#if defined(PETSC_HAVE_TRIANGLE) 
  PetscErrorCode       ierr;
  PetscInt             jj,tid,tt,idx,nselected_1,nselected_2;
  struct triangulateio in,mid;
  const PetscInt      *selected_idx_1,*selected_idx_2,*llist_idx;
  PetscMPIInt          mype,npe;
  PetscInt             Istart,Iend,nFineLoc,myFine0;
  int kk,nPlotPts,sid;

  PetscFunctionBegin;
  *a_worst_best = 0.0;
  ierr = MPI_Comm_rank(((PetscObject)a_Prol)->comm,&mype);    CHKERRQ(ierr);
  ierr = MPI_Comm_size(((PetscObject)a_Prol)->comm,&npe);     CHKERRQ(ierr);
  ierr = ISGetLocalSize( selected_1, &nselected_1 );        CHKERRQ(ierr);
  ierr = ISGetLocalSize( selected_2, &nselected_2 );        CHKERRQ(ierr);
  if(nselected_2 == 1 || nselected_2 == 2 ){ /* 0 happens on idle processors */
    /* SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_LIB,"Not enough points - error in stopping logic",nselected_2); */
    *a_worst_best = 100.0; /* this will cause a stop, but not globalized (should not happen) */
#ifdef VERBOSE
    PetscPrintf(PETSC_COMM_SELF,"[%d]%s %d selected point - bailing out\n",mype,__FUNCT__,nselected_2);
#endif
    PetscFunctionReturn(0);
  }
  ierr = MatGetOwnershipRange( a_Prol, &Istart, &Iend );  CHKERRQ(ierr);
  nFineLoc = (Iend-Istart)/bs; myFine0 = Istart/bs;
  nPlotPts = nFineLoc; /* locals */
  /* traingle */
  /* Define input points - in*/
  in.numberofpoints = nselected_2;
  in.numberofpointattributes = 0;
  /* get nselected points */
  ierr = PetscMalloc( 2*(nselected_2)*sizeof(REAL), &in.pointlist ); CHKERRQ(ierr);
  ierr = ISGetIndices( selected_2, &selected_idx_2 );     CHKERRQ(ierr);

  for(kk=0,sid=0;kk<nselected_2;kk++,sid += 2){
    PetscInt lid = selected_idx_2[kk];
    in.pointlist[sid] = coords[lid];
    in.pointlist[sid+1] = coords[nnodes + lid];
    if(lid>=nFineLoc) nPlotPts++;
  }
  assert(sid==2*nselected_2);

  in.numberofsegments = 0;
  in.numberofedges = 0;
  in.numberofholes = 0;
  in.numberofregions = 0;
  in.trianglelist = 0;
  in.segmentmarkerlist = 0;
  in.pointattributelist = 0;
  in.pointmarkerlist = 0;
  in.triangleattributelist = 0;
  in.trianglearealist = 0;
  in.segmentlist = 0;
  in.holelist = 0;
  in.regionlist = 0;
  in.edgelist = 0;
  in.edgemarkerlist = 0;
  in.normlist = 0;
  /* triangulate */
  mid.pointlist = 0;            /* Not needed if -N switch used. */
  /* Not needed if -N switch used or number of point attributes is zero: */
  mid.pointattributelist = 0;
  mid.pointmarkerlist = 0; /* Not needed if -N or -B switch used. */
  mid.trianglelist = 0;          /* Not needed if -E switch used. */
  /* Not needed if -E switch used or number of triangle attributes is zero: */
  mid.triangleattributelist = 0;
  mid.neighborlist = 0;         /* Needed only if -n switch used. */
  /* Needed only if segments are output (-p or -c) and -P not used: */
  mid.segmentlist = 0;
  /* Needed only if segments are output (-p or -c) and -P and -B not used: */
  mid.segmentmarkerlist = 0;
  mid.edgelist = 0;             /* Needed only if -e switch used. */
  mid.edgemarkerlist = 0;   /* Needed if -e used and -B not used. */
  mid.numberoftriangles = 0;

  /* Triangulate the points.  Switches are chosen to read and write a  */
  /*   PSLG (p), preserve the convex hull (c), number everything from  */
  /*   zero (z), assign a regional attribute to each element (A), and  */
  /*   produce an edge list (e), a Voronoi diagram (v), and a triangle */
  /*   neighbor list (n).                                            */
  if(nselected_2 != 0){ /* inactive processor */
    char args[] = "npczQ"; /* c is needed ? */
    triangulate(args, &in, &mid, (struct triangulateio *) NULL );
    /* output .poly files for 'showme' */
    if( !PETSC_TRUE ) {
      static int level = 1;
      FILE *file; char fname[32];

      sprintf(fname,"C%d_%d.poly",level,mype); file = fopen(fname, "w");
      /*First line: <# of vertices> <dimension (must be 2)> <# of attributes> <# of boundary markers (0 or 1)>*/
      fprintf(file, "%d  %d  %d  %d\n",in.numberofpoints,2,0,0);
      /*Following lines: <vertex #> <x> <y> */
      for(kk=0,sid=0;kk<in.numberofpoints;kk++,sid += 2){
        fprintf(file, "%d %e %e\n",kk,in.pointlist[sid],in.pointlist[sid+1]);
      }
      /*One line: <# of segments> <# of boundary markers (0 or 1)> */
      fprintf(file, "%d  %d\n",0,0);
      /*Following lines: <segment #> <endpoint> <endpoint> [boundary marker] */
      /* One line: <# of holes> */
      fprintf(file, "%d\n",0);
      /* Following lines: <hole #> <x> <y> */
      /* Optional line: <# of regional attributes and/or area constraints> */
      /* Optional following lines: <region #> <x> <y> <attribute> <maximum area> */
      fclose(file);

      /* elems */
      sprintf(fname,"C%d_%d.ele",level,mype); file = fopen(fname, "w");
      /* First line: <# of triangles> <nodes per triangle> <# of attributes> */
      fprintf(file, "%d %d %d\n",mid.numberoftriangles,3,0);
      /* Remaining lines: <triangle #> <node> <node> <node> ... [attributes] */
      for(kk=0,sid=0;kk<mid.numberoftriangles;kk++,sid += 3){
        fprintf(file, "%d %d %d %d\n",kk,mid.trianglelist[sid],mid.trianglelist[sid+1],mid.trianglelist[sid+2]);
      }
      fclose(file);

      sprintf(fname,"C%d_%d.node",level,mype); file = fopen(fname, "w");
      /* First line: <# of vertices> <dimension (must be 2)> <# of attributes> <# of boundary markers (0 or 1)> */
      /* fprintf(file, "%d  %d  %d  %d\n",in.numberofpoints,2,0,0); */
      fprintf(file, "%d  %d  %d  %d\n",nPlotPts,2,0,0);
      /*Following lines: <vertex #> <x> <y> */
      for(kk=0,sid=0;kk<in.numberofpoints;kk++,sid+=2){
        fprintf(file, "%d %e %e\n",kk,in.pointlist[sid],in.pointlist[sid+1]);
      }

      sid /= 2;
      for(jj=0;jj<nFineLoc;jj++){
        PetscBool sel = PETSC_TRUE;
        for( kk=0 ; kk<nselected_2 && sel ; kk++ ){
          PetscInt lid = selected_idx_2[kk];
          if( lid == jj ) sel = PETSC_FALSE;
        }
        if( sel ) {
          fprintf(file, "%d %e %e\n",sid++,coords[jj],coords[nnodes + jj]);
        }
      }
      fclose(file);
      assert(sid==nPlotPts);
      level++;
    }
  }
#if defined PETSC_USE_LOG
  ierr = PetscLogEventBegin(gamg_setup_events[FIND_V],0,0,0,0);CHKERRQ(ierr);
#endif
  { /* form P - setup some maps */
    PetscInt clid_iterator;
    PetscInt *nTri, *node_tri;
    
    ierr = PetscMalloc( nselected_2*sizeof(PetscInt), &node_tri ); CHKERRQ(ierr); 
    ierr = PetscMalloc( nselected_2*sizeof(PetscInt), &nTri ); CHKERRQ(ierr); 

    /* need list of triangles on node */
    for(kk=0;kk<nselected_2;kk++) nTri[kk] = 0;
    for(tid=0,kk=0;tid<mid.numberoftriangles;tid++){
      for(jj=0;jj<3;jj++) {
        PetscInt cid = mid.trianglelist[kk++];
        if( nTri[cid] == 0 ) node_tri[cid] = tid;
        nTri[cid]++;
      }
    } 
#define EPS 1.e-12
    /* find points and set prolongation */
    ierr = ISGetIndices( selected_1, &selected_idx_1 );     CHKERRQ(ierr); 
    ierr = ISGetIndices( locals_llist, &llist_idx );     CHKERRQ(ierr);
    for( clid_iterator = 0 ; clid_iterator < nselected_1 ; clid_iterator++ ){
      PetscInt flid = selected_idx_1[clid_iterator]; assert(flid != -1);
      PetscScalar AA[3][3];
      PetscBLASInt N=3,NRHS=1,LDA=3,IPIV[3],LDB=3,INFO;
      do{
        if( flid < nFineLoc ) {  /* could be a ghost */
          PetscInt bestTID = -1; PetscReal best_alpha = 1.e10; 
          const PetscInt fgid = flid + myFine0;
          /* compute shape function for gid */
          const PetscReal fcoord[3] = { coords[flid], coords[nnodes + flid], 1.0 };
          PetscBool haveit = PETSC_FALSE; PetscScalar alpha[3]; PetscInt clids[3];
          /* look for it */
          for( tid = node_tri[clid_iterator], jj=0;
               jj < 5 && !haveit && tid != -1;
               jj++ ){
            for(tt=0;tt<3;tt++){
              PetscInt cid2 = mid.trianglelist[3*tid + tt];
              PetscInt lid2 = selected_idx_2[cid2];
              AA[tt][0] = coords[lid2]; AA[tt][1] = coords[nnodes + lid2]; AA[tt][2] = 1.0;
              clids[tt] = cid2; /* store for interp */
            }

            for(tt=0;tt<3;tt++) alpha[tt] = (PetscScalar)fcoord[tt];

            /* SUBROUTINE DGESV( N, NRHS, A, LDA, IPIV, B, LDB, INFO ) */
            LAPACKgesv_(&N, &NRHS, (PetscScalar*)AA, &LDA, IPIV, alpha, &LDB, &INFO);
            {
              PetscBool have=PETSC_TRUE;  PetscReal lowest=1.e10;
              for( tt = 0, idx = 0 ; tt < 3 ; tt++ ) {
                if( PetscRealPart(alpha[tt]) > (1.0+EPS) || PetscRealPart(alpha[tt]) < -EPS ) have = PETSC_FALSE;
                if( PetscRealPart(alpha[tt]) < lowest ){
                  lowest = PetscRealPart(alpha[tt]);
                  idx = tt;
                }
              }
              haveit = have;
            }
            tid = mid.neighborlist[3*tid + idx];
          }

          if( !haveit ) {
            /* brute force */
            for(tid=0 ; tid<mid.numberoftriangles && !haveit ; tid++ ){
              for(tt=0;tt<3;tt++){
                PetscInt cid2 = mid.trianglelist[3*tid + tt];
                PetscInt lid2 = selected_idx_2[cid2];
                AA[tt][0] = coords[lid2]; AA[tt][1] = coords[nnodes + lid2]; AA[tt][2] = 1.0;
                clids[tt] = cid2; /* store for interp */
              }
              for(tt=0;tt<3;tt++) alpha[tt] = fcoord[tt];
              /* SUBROUTINE DGESV( N, NRHS, A, LDA, IPIV, B, LDB, INFO ) */
              LAPACKgesv_(&N, &NRHS, (PetscScalar*)AA, &LDA, IPIV, alpha, &LDB, &INFO);
              {
                PetscBool have=PETSC_TRUE;  PetscReal worst=0.0, v;
                for(tt=0; tt<3 && have ;tt++) {
                  if( PetscRealPart(alpha[tt]) > 1.0+EPS || PetscRealPart(alpha[tt]) < -EPS ) have=PETSC_FALSE;
                  if( (v=PetscAbs(PetscRealPart(alpha[tt])-0.5)) > worst ) worst = v;
                }
                if( worst < best_alpha ) {
                  best_alpha = worst; bestTID = tid;
                }
                haveit = have;
              }
            }
          }
          if( !haveit ) {
            if( best_alpha > *a_worst_best ) *a_worst_best = best_alpha;
            /* use best one */
            for(tt=0;tt<3;tt++){
              PetscInt cid2 = mid.trianglelist[3*bestTID + tt];
              PetscInt lid2 = selected_idx_2[cid2];
              AA[tt][0] = coords[lid2]; AA[tt][1] = coords[nnodes + lid2]; AA[tt][2] = 1.0;
              clids[tt] = cid2; /* store for interp */
            }
            for(tt=0;tt<3;tt++) alpha[tt] = fcoord[tt];
            /* SUBROUTINE DGESV( N, NRHS, A, LDA, IPIV, B, LDB, INFO ) */
            LAPACKgesv_(&N, &NRHS, (PetscScalar*)AA, &LDA, IPIV, alpha, &LDB, &INFO);
          }

          /* put in row of P */
          for(idx=0;idx<3;idx++){
            PetscScalar shp = alpha[idx];
            if( PetscAbs(PetscRealPart(shp)) > 1.e-6 ) {
              PetscInt cgid = crsGID[clids[idx]];
              PetscInt jj = cgid*bs, ii = fgid*bs; /* need to gloalize */
              for(tt=0 ; tt < bs ; tt++, ii++, jj++ ){
                ierr = MatSetValues(a_Prol,1,&ii,1,&jj,&shp,INSERT_VALUES); CHKERRQ(ierr);
              }
            }
          }
        } /* local vertex test */
      } while( (flid=llist_idx[flid]) != -1 );
    }
    ierr = ISRestoreIndices( selected_2, &selected_idx_2 );     CHKERRQ(ierr);
    ierr = ISRestoreIndices( selected_1, &selected_idx_1 );     CHKERRQ(ierr);
    ierr = ISRestoreIndices( locals_llist, &llist_idx );     CHKERRQ(ierr);
    ierr = MatAssemblyBegin(a_Prol,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
    ierr = MatAssemblyEnd(a_Prol,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);

    ierr = PetscFree( node_tri );  CHKERRQ(ierr);
    ierr = PetscFree( nTri );  CHKERRQ(ierr);
  }
#if defined PETSC_USE_LOG
  ierr = PetscLogEventEnd(gamg_setup_events[FIND_V],0,0,0,0);CHKERRQ(ierr);
#endif
  free( mid.trianglelist );
  free( mid.neighborlist );
  ierr = PetscFree( in.pointlist );  CHKERRQ(ierr);

  PetscFunctionReturn(0);
#else
    SETERRQ(((PetscObject)a_Prol)->comm,PETSC_ERR_LIB,"configure with TRIANGLE to use geometric MG");
#endif
}
/* -------------------------------------------------------------------------- */
/*
   getGIDsOnSquareGraph - square graph, get

   Input Parameter:
   . selected_1 - selected local indices (includes ghosts in input Gmat_1)
   . Gmat1 - graph that goes with 'selected_1'
   Output Parameter:
   . a_selected_2 - selected local indices (includes ghosts in output a_Gmat_2)
   . a_Gmat_2 - graph that is squared of 'Gmat_1'
   . a_crsGID[a_selected_2.size()] - map of global IDs of coarse grid nodes
*/
#undef __FUNCT__
#define __FUNCT__ "getGIDsOnSquareGraph"
PetscErrorCode getGIDsOnSquareGraph( const IS selected_1,
                                     const Mat Gmat1,
                                     IS *a_selected_2,
                                     Mat *a_Gmat_2,
                                     PetscInt **a_crsGID
                                     )
{
  PetscErrorCode ierr;
  PetscMPIInt    mype,npe;
  PetscInt       *crsGID, kk,my0,Iend,nloc,nSelected_1;
  const PetscInt *selected_idx;
  MPI_Comm       wcomm = ((PetscObject)Gmat1)->comm;

  PetscFunctionBegin;
  ierr = MPI_Comm_rank(wcomm,&mype);CHKERRQ(ierr);
  ierr = MPI_Comm_size(wcomm,&npe);CHKERRQ(ierr);
  ierr = MatGetOwnershipRange(Gmat1,&my0,&Iend); CHKERRQ(ierr); /* AIJ */
  nloc = Iend - my0; /* this does not change */
  ierr = ISGetLocalSize( selected_1, &nSelected_1 );        CHKERRQ(ierr);

  if (npe == 1) { /* not much to do in serial */
    ierr = PetscMalloc( nSelected_1*sizeof(PetscInt), &crsGID ); CHKERRQ(ierr);
    for(kk=0;kk<nSelected_1;kk++) crsGID[kk] = kk;
    *a_Gmat_2 = 0;
    *a_selected_2 = selected_1; /* needed? */
  }
  else {
    PetscInt      idx,num_fine_ghosts,num_crs_ghost,nLocalSelected,myCrs0;
    Mat_MPIAIJ   *mpimat2; 
    Mat           Gmat2;
    Vec           locState;
    PetscScalar   *cpcol_state;

    /* get 'nLocalSelected' */
    ierr = ISGetIndices( selected_1, &selected_idx );     CHKERRQ(ierr);
    for(kk=0,nLocalSelected=0;kk<nSelected_1;kk++){
      PetscInt lid = selected_idx[kk];
      if(lid<nloc) nLocalSelected++;
    }
    ierr = ISRestoreIndices( selected_1, &selected_idx );     CHKERRQ(ierr);
    /* scan my coarse zero gid, set 'lid_state' with coarse GID */
    MPI_Scan( &nLocalSelected, &myCrs0, 1, MPIU_INT, MPIU_SUM, wcomm );
    myCrs0 -= nLocalSelected;

    if( a_Gmat_2 != 0 ) { /* output */
      /* grow graph to get wider set of selected vertices to cover fine grid, invalidates 'llist' */
      ierr = MatTransposeMatMult(Gmat1, Gmat1, MAT_INITIAL_MATRIX, PETSC_DEFAULT, &Gmat2 );   CHKERRQ(ierr);
      *a_Gmat_2 = Gmat2; /* output */
    }
    else Gmat2 = Gmat1;  /* use local to get crsGIDs at least */
    /* get coarse grid GIDS for selected (locals and ghosts) */
    mpimat2 = (Mat_MPIAIJ*)Gmat2->data;
    ierr = MatGetVecs( Gmat2, &locState, 0 );         CHKERRQ(ierr);
    ierr = VecSet( locState, (PetscScalar)(PetscReal)(-1) );  CHKERRQ(ierr); /* set with UNKNOWN state */
    ierr = ISGetIndices( selected_1, &selected_idx );     CHKERRQ(ierr);
    for(kk=0;kk<nLocalSelected;kk++){
      PetscInt fgid = selected_idx[kk] + my0;
      PetscScalar v = (PetscScalar)(kk+myCrs0);
      ierr = VecSetValues( locState, 1, &fgid, &v, INSERT_VALUES );  CHKERRQ(ierr); /* set with PID */
    }
    ierr = ISRestoreIndices( selected_1, &selected_idx );     CHKERRQ(ierr);
    ierr = VecAssemblyBegin( locState ); CHKERRQ(ierr);
    ierr = VecAssemblyEnd( locState ); CHKERRQ(ierr);
    ierr = VecScatterBegin(mpimat2->Mvctx,locState,mpimat2->lvec,INSERT_VALUES,SCATTER_FORWARD);CHKERRQ(ierr);
    ierr =   VecScatterEnd(mpimat2->Mvctx,locState,mpimat2->lvec,INSERT_VALUES,SCATTER_FORWARD);CHKERRQ(ierr);
    ierr = VecGetLocalSize( mpimat2->lvec, &num_fine_ghosts ); CHKERRQ(ierr);
    ierr = VecGetArray( mpimat2->lvec, &cpcol_state ); CHKERRQ(ierr); 
    for(kk=0,num_crs_ghost=0;kk<num_fine_ghosts;kk++){
      if( (PetscInt)PetscRealPart(cpcol_state[kk]) != -1 ) num_crs_ghost++;
    }
    ierr = PetscMalloc( (nLocalSelected+num_crs_ghost)*sizeof(PetscInt), &crsGID ); CHKERRQ(ierr); /* output */
    {
      PetscInt *selected_set;
      ierr = PetscMalloc( (nLocalSelected+num_crs_ghost)*sizeof(PetscInt), &selected_set ); CHKERRQ(ierr);
      /* do ghost of 'crsGID' */
      for(kk=0,idx=nLocalSelected;kk<num_fine_ghosts;kk++){
        if( (PetscInt)PetscRealPart(cpcol_state[kk]) != -1 ){
          PetscInt cgid = (PetscInt)PetscRealPart(cpcol_state[kk]);
          selected_set[idx] = nloc + kk;
          crsGID[idx++] = cgid;
        }
      }
      assert(idx==(nLocalSelected+num_crs_ghost));
      ierr = VecRestoreArray( mpimat2->lvec, &cpcol_state ); CHKERRQ(ierr);
      /* do locals in 'crsGID' */
      ierr = VecGetArray( locState, &cpcol_state ); CHKERRQ(ierr);
      for(kk=0,idx=0;kk<nloc;kk++){
        if( (PetscInt)PetscRealPart(cpcol_state[kk]) != -1 ){
          PetscInt cgid = (PetscInt)PetscRealPart(cpcol_state[kk]);
          selected_set[idx] = kk;
          crsGID[idx++] = cgid;
        }
      }
      assert(idx==nLocalSelected);
      ierr = VecRestoreArray( locState, &cpcol_state ); CHKERRQ(ierr);

      if( a_selected_2 != 0 ) { /* output */
        ierr = ISCreateGeneral(PETSC_COMM_SELF,(nLocalSelected+num_crs_ghost),selected_set,PETSC_COPY_VALUES,a_selected_2);
        CHKERRQ(ierr);
      }
      ierr = PetscFree( selected_set );  CHKERRQ(ierr);
    }
    ierr = VecDestroy( &locState );                    CHKERRQ(ierr);
  }
  *a_crsGID = crsGID; /* output */

  PetscFunctionReturn(0);
}

/* -------------------------------------------------------------------------- */
/*
   PCGAMGcreateProl_GEO

  Input Parameter:
   . pc - this
   . Amat - matrix on this fine level
   . data[nloc*data_sz(in)]
  Output Parameter:
   . a_P_out - prolongation operator to the next level
   . a_data_out - data of coarse grid points (num local columns in 'a_P_out')
*/
#undef __FUNCT__
#define __FUNCT__ "PCGAMGcreateProl_GEO"
PetscErrorCode PCGAMGcreateProl_GEO( PC pc, 
                                     const Mat Amat,
                                     const PetscReal data[],
                                     Mat *a_P_out,
                                     PetscReal **a_data_out
                                     )
{
  PC_MG          *mg = (PC_MG*)pc->data;
  PC_GAMG        *pc_gamg = (PC_GAMG*)mg->innerctx;  
  const PetscInt verbose = pc_gamg->verbose;
  const PetscInt  dim = pc_gamg->data_cols, data_cols = pc_gamg->data_cols;
  PetscErrorCode ierr;
  PetscInt       Istart,Iend,nloc,jj,kk,my0,nLocalSelected,NN,MM,bs;
  Mat            Prol, Gmat;
  PetscMPIInt    mype, npe;
  MPI_Comm       wcomm = ((PetscObject)Amat)->comm;
  IS             permIS, llist_1, selected_1, selected_2;
  const PetscInt *selected_idx;

  PetscFunctionBegin;
  ierr = MPI_Comm_rank(wcomm,&mype);CHKERRQ(ierr);
  ierr = MPI_Comm_size(wcomm,&npe);CHKERRQ(ierr);
  ierr = MatGetOwnershipRange( Amat, &Istart, &Iend ); CHKERRQ(ierr);
  ierr = MatGetSize( Amat, &MM, &NN ); CHKERRQ(ierr);
  ierr  = MatGetBlockSize( Amat, &bs );               CHKERRQ( ierr );
  nloc = (Iend-Istart)/bs; my0 = Istart/bs; assert((Iend-Istart)%bs==0);

  ierr = createGraph( pc, Amat, &Gmat, PETSC_NULL, &permIS ); CHKERRQ(ierr);

  /* SELECT COARSE POINTS */
#if defined PETSC_USE_LOG
  ierr = PetscLogEventBegin(gamg_setup_events[SET4],0,0,0,0);CHKERRQ(ierr);
#endif

  ierr = maxIndSetAgg( permIS, Gmat, PETSC_NULL, PETSC_FALSE, &selected_1, &llist_1 );
  CHKERRQ(ierr);
#if defined PETSC_USE_LOG
  ierr = PetscLogEventEnd(gamg_setup_events[SET4],0,0,0,0);CHKERRQ(ierr);
#endif
  ierr = ISDestroy(&permIS); CHKERRQ(ierr);

  /* get 'nLocalSelected' */
  ierr = ISGetLocalSize( selected_1, &NN );        CHKERRQ(ierr);
  ierr = ISGetIndices( selected_1, &selected_idx );     CHKERRQ(ierr);
  for(kk=0,nLocalSelected=0;kk<NN;kk++){
    PetscInt lid = selected_idx[kk];
    if(lid<nloc) nLocalSelected++;
  }
  ierr = ISRestoreIndices( selected_1, &selected_idx );     CHKERRQ(ierr);
  
  /* create prolongator, create P matrix */
  ierr = MatCreateMPIAIJ(wcomm, 
                         nloc*bs, nLocalSelected*bs,
                         PETSC_DETERMINE, PETSC_DETERMINE,
                         3*data_cols, PETSC_NULL, /* don't have a good way to set this!!! */
                         3*data_cols, PETSC_NULL,
                         &Prol );
  CHKERRQ(ierr);
  
  /* can get all points "removed" */
  ierr =  MatGetSize( Prol, &kk, &jj ); CHKERRQ(ierr);
  if( jj==0 ) {
    if( verbose ) {
      PetscPrintf(PETSC_COMM_WORLD,"[%d]%s no selected points on coarse grid\n",mype,__FUNCT__);
    }
    ierr = MatDestroy( &Prol );  CHKERRQ(ierr);
    ierr = ISDestroy( &llist_1 ); CHKERRQ(ierr);
    ierr = ISDestroy( &selected_1 ); CHKERRQ(ierr);
    ierr = MatDestroy( &Gmat );  CHKERRQ(ierr);    
    *a_P_out = PETSC_NULL;  /* out */
    PetscFunctionReturn(0);
  }

  {
    PetscReal *coords; 
    PetscInt nnodes;
    PetscInt  *crsGID;
    Mat        Gmat2;

    assert(dim==data_cols); 
    /* grow ghost data for better coarse grid cover of fine grid */
#if defined PETSC_USE_LOG
    ierr = PetscLogEventBegin(gamg_setup_events[SET5],0,0,0,0);CHKERRQ(ierr);
#endif
    ierr = getGIDsOnSquareGraph( selected_1, Gmat, &selected_2, &Gmat2, &crsGID );
    CHKERRQ(ierr);
    ierr = MatDestroy( &Gmat );  CHKERRQ(ierr);
    /* llist is now not valid wrt squared graph, but will work as iterator in 'triangulateAndFormProl' */
#if defined PETSC_USE_LOG
    ierr = PetscLogEventEnd(gamg_setup_events[SET5],0,0,0,0);CHKERRQ(ierr);
#endif
    /* create global vector of coorindates in 'coords' */
    if (npe > 1) {
      ierr = getDataWithGhosts( Gmat2, dim, data, &nnodes, &coords );
      CHKERRQ(ierr);
    }
    else {
      coords = (PetscReal*)data;
      nnodes = nloc;
    }
    ierr = MatDestroy( &Gmat2 );  CHKERRQ(ierr);

    /* triangulate */
    if( dim == 2 ) {
      PetscReal metric=0.0;
#if defined PETSC_USE_LOG
      ierr = PetscLogEventBegin(gamg_setup_events[SET6],0,0,0,0);CHKERRQ(ierr);
#endif
      ierr = triangulateAndFormProl( selected_2, nnodes, coords,
                                     selected_1, llist_1, crsGID, bs, Prol, &metric );
      CHKERRQ(ierr);
#if defined PETSC_USE_LOG
      ierr = PetscLogEventEnd(gamg_setup_events[SET6],0,0,0,0); CHKERRQ(ierr);
#endif
      ierr = PetscFree( crsGID );  CHKERRQ(ierr);

      /* clean up and create coordinates for coarse grid (output) */
      if (npe > 1) ierr = PetscFree( coords ); CHKERRQ(ierr);
      
      if( metric > 1. ) { /* needs to be globalized - should not happen */
        if( verbose ) {
          PetscPrintf(PETSC_COMM_SELF,"[%d]%s failed metric for coarse grid %e\n",mype,__FUNCT__,metric);
        }
        ierr = MatDestroy( &Prol );  CHKERRQ(ierr);
        Prol = PETSC_NULL;
      }
      else if( metric > .0 ) {
        if( verbose ) {
          PetscPrintf(PETSC_COMM_SELF,"[%d]%s metric for coarse grid = %e\n",mype,__FUNCT__,metric);
        }
      }
    } else {
      SETERRQ(wcomm,PETSC_ERR_LIB,"3D not implemented for 'geo' AMG");
    }
    { /* create next coords - output */
      PetscReal *crs_crds;
      ierr = PetscMalloc( dim*nLocalSelected*sizeof(PetscReal), &crs_crds ); 
      CHKERRQ(ierr);
      ierr = ISGetIndices( selected_1, &selected_idx );     CHKERRQ(ierr);
      for(kk=0;kk<nLocalSelected;kk++){/* grab local select nodes to promote - output */
        PetscInt lid = selected_idx[kk];
        for(jj=0;jj<dim;jj++) crs_crds[jj*nLocalSelected + kk] = data[jj*nloc + lid];
      }
      ierr = ISRestoreIndices( selected_1, &selected_idx );     CHKERRQ(ierr);
      *a_data_out = crs_crds; /* out */
    }
    if (npe > 1) {
      ierr = ISDestroy( &selected_2 ); CHKERRQ(ierr); /* this is selected_1 in serial */
    }
  }
  *a_P_out = Prol;  /* out */

  ierr = ISDestroy( &llist_1 ); CHKERRQ(ierr);
  ierr = ISDestroy( &selected_1 ); CHKERRQ(ierr);

  PetscFunctionReturn(0);
}
