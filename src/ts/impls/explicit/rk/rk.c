/*$Id: rk.c,v 0.1 2003/06/03 Asbjorn Hoiland Aarrestad$*/
/*
 * Code for Timestepping with Runge Kutta
 *
 * Written by
 * Asbjorn Hoiland Aarrestad
 * asbjorn@aarrestad.com
 * http://asbjorn.aarrestad.com/
 * 
 */
#include "src/ts/tsimpl.h"                /*I   "petscts.h"   I*/
#include "time.h"

typedef struct {
   Vec		y1,y2;  /* work wectors for the two rk permuations */
   int		nok,nnok; /* counters for ok and not ok steps */
   PetscReal	maxerror; /* variable to tell the maxerror allowed */
   PetscReal    ferror; /* variable to tell (global maxerror)/(total time) */
   Vec		tmp,tmp_y,*k; /* two temp vectors and the k vectors for rk */
   PetscScalar	a[7][6]; /* rk scalars */
   PetscScalar	b1[7],b2[7]; /* rk scalars */
   PetscReal	c[7]; /* rk scalars */
   int          p,s; /* variables to tell the size of the runge-kutta solver */
   clock_t      start,end; /* variables to mesure cpu time */
} TS_Rk;



#undef __FUNCT__  
#define __FUNCT__ "TSSetUp_Rk"
static int TSSetUp_Rk(TS ts)
{
  TS_Rk	*rk = (TS_Rk*)ts->data;
  int	ierr;

  PetscFunctionBegin;
  rk->nok = 0;
  rk->nnok = 0;
  rk->maxerror = ts->time_step;

  /* fixing maxerror: global vs local */
  rk->ferror = rk->maxerror / (ts->max_time - ts->ptime);

  /* 34.0/45.0 gives double precision division */
  /* defining variables needed for Runge-Kutta computing */
  /* when changing below, please remember to change a, b1, b2 and c above! */
  /* Found in table on page 171: Dormand-Prince 5(4) */

  /* are these right? */
  rk->p=6;
  rk->s=7;

  rk->a[1][0]=1.0/5.0;
  rk->a[2][0]=3.0/40.0;
  rk->a[2][1]=9.0/40.0;
  rk->a[3][0]=44.0/45.0;
  rk->a[3][1]=-56.0/15.0;
  rk->a[3][2]=32.0/9.0;
  rk->a[4][0]=19372.0/6561.0;
  rk->a[4][1]=-25360.0/2187.0;
  rk->a[4][2]=64448.0/6561.0;
  rk->a[4][3]=-212.0/729.0;
  rk->a[5][0]=9017.0/3168.0;
  rk->a[5][1]=-355.0/33.0;
  rk->a[5][2]=46732.0/5247.0;
  rk->a[5][3]=49.0/176.0;
  rk->a[5][4]=-5103.0/18656.0;
  rk->a[6][0]=35.0/384.0;
  rk->a[6][1]=0.0;
  rk->a[6][2]=500.0/1113.0;
  rk->a[6][3]=125.0/192.0;
  rk->a[6][4]=-2187.0/6784.0;
  rk->a[6][5]=11.0/84.0;


  rk->c[0]=0.0;
  rk->c[1]=1.0/5.0;
  rk->c[2]=3.0/10.0;
  rk->c[3]=4.0/5.0;
  rk->c[4]=8.0/9.0;
  rk->c[5]=1.0;
  rk->c[6]=1.0;
  
  rk->b1[0]=35.0/384.0;
  rk->b1[1]=0.0;
  rk->b1[2]=500.0/1113.0;
  rk->b1[3]=125.0/192.0;
  rk->b1[4]=-2187.0/6784.0;
  rk->b1[5]=11.0/84.0;
  rk->b1[6]=0.0;

  rk->b2[0]=5179.0/57600.0;
  rk->b2[1]=0.0;
  rk->b2[2]=7571.0/16695.0;
  rk->b2[3]=393.0/640.0;
  rk->b2[4]=-92097.0/339200.0;
  rk->b2[5]=187.0/2100.0;
  rk->b2[6]=1.0/40.0;
  
  
  /* Found in table on page 170: Fehlberg 4(5) */
  /*  
  rk->p=5;
  rk->s=6;

  rk->a[1][0]=1.0/4.0;
  rk->a[2][0]=3.0/32.0;
  rk->a[2][1]=9.0/32.0;
  rk->a[3][0]=1932.0/2197.0;
  rk->a[3][1]=-7200.0/2197.0;
  rk->a[3][2]=7296.0/2197.0;
  rk->a[4][0]=439.0/216.0;
  rk->a[4][1]=-8.0;
  rk->a[4][2]=3680.0/513.0;
  rk->a[4][3]=-845.0/4104.0;
  rk->a[5][0]=-8.0/27.0;
  rk->a[5][1]=2.0;
  rk->a[5][2]=-3544.0/2565.0;
  rk->a[5][3]=1859.0/4104.0;
  rk->a[5][4]=-11.0/40.0;

  rk->c[0]=0.0;
  rk->c[1]=1.0/4.0;
  rk->c[2]=3.0/8.0;
  rk->c[3]=12.0/13.0;
  rk->c[4]=1.0;
  rk->c[5]=1.0/2.0;

  rk->b1[0]=25.0/216.0;
  rk->b1[1]=0.0;
  rk->b1[2]=1408.0/2565.0;
  rk->b1[3]=2197.0/4104.0;
  rk->b1[4]=-1.0/5.0;
  rk->b1[5]=0.0;
  
  rk->b2[0]=16.0/135.0;
  rk->b2[1]=0.0;
  rk->b2[2]=6656.0/12825.0;
  rk->b2[3]=28561.0/56430.0;
  rk->b2[4]=-9.0/50.0;
  rk->b2[5]=2.0/55.0;
  */
  /* Found in table on page 169: Merson 4("5") */
  /*
  rk->p=4;
  rk->s=5;
  rk->a[1][0] = 1.0/3.0;
  rk->a[2][0] = 1.0/6.0;
  rk->a[2][1] = 1.0/6.0;
  rk->a[3][0] = 1.0/8.0;
  rk->a[3][1] = 0.0;
  rk->a[3][2] = 3.0/8.0;
  rk->a[4][0] = 1.0/2.0;
  rk->a[4][1] = 0.0;
  rk->a[4][2] = -3.0/2.0;
  rk->a[4][3] = 2.0;

  rk->c[0] = 0.0;
  rk->c[1] = 1.0/3.0;
  rk->c[2] = 1.0/3.0;
  rk->c[3] = 0.5;
  rk->c[4] = 1.0;

  rk->b1[0] = 1.0/2.0;
  rk->b1[1] = 0.0;
  rk->b1[2] = -3.0/2.0;
  rk->b1[3] = 2.0;
  rk->b1[4] = 0.0;

  rk->b2[0] = 1.0/6.0;
  rk->b2[1] = 0.0;
  rk->b2[2] = 0.0;
  rk->b2[3] = 2.0/3.0;
  rk->b2[4] = 1.0/6.0;
  */

  /* making b2 -> e=b1-b2 */
  /*
    for(i=0;i<rk->s;i++){
     rk->b2[i] = (rk->b1[i]) - (rk->b2[i]);
  }
  */
  rk->b2[0]=71.0/57600.0;
  rk->b2[1]=0.0;
  rk->b2[2]=-71.0/16695.0;
  rk->b2[3]=71.0/1920.0;
  rk->b2[4]=-17253.0/339200.0;
  rk->b2[5]=22.0/525.0;
  rk->b2[6]=-1.0/40.0;

  /* initializing vectors */
  ierr = VecDuplicate(ts->vec_sol,&rk->y1);CHKERRQ(ierr);
  ierr = VecDuplicate(ts->vec_sol,&rk->y2);CHKERRQ(ierr);
  ierr = VecDuplicate(rk->y1,&rk->tmp);CHKERRQ(ierr);
  ierr = VecDuplicate(rk->y1,&rk->tmp_y);CHKERRQ(ierr);
  ierr = VecDuplicateVecs(rk->y1,rk->s,&rk->k);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSStep_Rk"
static int TSStep_Rk(TS ts,int *steps,PetscReal *ptime)
{
  TS_Rk		*rk = (TS_Rk*)ts->data;
  int		ierr;
  PetscReal	dt = 0.001; /* fixed first step guess */
  PetscReal	norm=0.0,dt_fac=0.0,fac = 0.0,ttmp=0.0;

  PetscFunctionBegin;
  rk->start=clock();
  ierr=VecCopy(ts->vec_sol,rk->y1);CHKERRQ(ierr);
  *steps = -ts->steps;
  /* trying to save the vector */
  ierr = TSMonitor(ts,ts->steps,ts->ptime,ts->vec_sol);CHKERRQ(ierr);
  /* while loop to get from start to stop */
  while (ts->ptime < ts->max_time){
   /* calling rkqs */
     /*
       -- input
       ts        - pointer to ts
       ts->ptime - current time
       dt        - try this timestep
       y1        - solution for this step

       --output
       y1        - suggested solution
       y2        - check solution (runge - kutta second permutation)
     */
     ierr = TSRkqs(ts,ts->ptime,dt);CHKERRQ(ierr);
   /* checking for maxerror */
     /* comparing difference to maxerror */
     ierr = VecNorm(rk->y2,NORM_2,&norm);CHKERRQ(ierr);
     /* modifying maxerror to satisfy this timestep */
     rk->maxerror = rk->ferror * dt;
     /* ierr = PetscPrintf(PETSC_COMM_WORLD,"norm err: %f maxerror: %f dt: %f",norm,rk->maxerror,dt);CHKERRQ(ierr); */

   /* handling ok and not ok */
     if(norm < rk->maxerror){
	/* if ok: */
        ierr=VecCopy(rk->y1,ts->vec_sol);CHKERRQ(ierr); /* saves the suggested solution to current solution */
        ts->ptime += dt; /* storing the new current time */
	rk->nok++;
	fac=5.0;
	/* trying to save the vector */
       /* calling monitor */
       ierr = TSMonitor(ts,ts->steps,ts->ptime,ts->vec_sol);CHKERRQ(ierr);  
     }else{
	/* if not OK */
	rk->nnok++;
	fac=1.0;
	ierr=VecCopy(ts->vec_sol,rk->y1);CHKERRQ(ierr);  /* restores old solution */
     }     

     /*Computing next stepsize. See page 167 in Solving ODE 1
      *
      * h_new = h * min( facmax , max( facmin , fac * (tol/err)^(1/(p+1)) ) )
      * facmax set above
      * facmin
      */
     dt_fac = exp(log((rk->maxerror) / norm) / ((rk->p) + 1) ) * 0.9 ;

     if(dt_fac > fac){
        /*ierr = PetscPrintf(PETSC_COMM_WORLD,"changing fac %f\n",fac);*/
	dt_fac = fac;
     }

     /* computing new dt */
     dt = dt * dt_fac;

     if(ts->ptime+dt > ts->max_time){
	dt = ts->max_time - ts->ptime;
     }

     if(dt < 1e-14){
	ierr = PetscPrintf(PETSC_COMM_WORLD,"Very small steps: %f\n",dt);CHKERRQ(ierr);
	dt = 1e-14;
     }

     /* trying to purify h */
     /* (did not give any visible result) */
     /* ttmp = ts->ptime + dt;
	dt = ttmp - ts->ptime; */
     
     /* counting steps */
     ts->steps++;
  }
  
  ierr=VecCopy(rk->y1,ts->vec_sol);CHKERRQ(ierr);
  *steps += ts->steps;
  *ptime  = ts->ptime;
  rk->end=clock();
  PetscFunctionReturn(0);
}
/*------------------------------------------------------------*/
#undef __FUNCT__  
#define __FUNCT__ "TSRkqs"
int TSRkqs(TS ts,PetscReal t,PetscReal h)
{
  TS_Rk		*rk = (TS_Rk*)ts->data;
  int		ierr,j,l;
  PetscReal	tmp_t=t;
  PetscScalar	null=0.0,hh=h;

  /*  printf("h: %f, hh: %f",h,hh); */
  
  PetscFunctionBegin;
   
  /* k[0]=0  */
  ierr = VecSet(&null,rk->k[0]);CHKERRQ(ierr);
     
  /* k[0] = derivs(t,y1) */
  ierr = TSComputeRHSFunction(ts,t,rk->y1,rk->k[0]);CHKERRQ(ierr);
  /* looping over runge-kutta variables */
  /* building the k - array of vectors */
  for(j = 1 ; j < rk->s ; j++){

     /* rk->tmp = 0 */
     ierr = VecSet(&null,rk->tmp);CHKERRQ(ierr);     

     for(l=0;l<j;l++){
	/* tmp += a(j,l)*k[l] */
	/* ierr = PetscPrintf(PETSC_COMM_WORLD,"a(%i,%i)=%f \n",j,l,rk->a[j][l]);CHKERRQ(ierr); */
	ierr = VecAXPY(&rk->a[j][l],rk->k[l],rk->tmp);CHKERRQ(ierr);
     }     

     /* ierr = VecView(rk->tmp,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr); */
     
     /* k[j] = derivs(t+c(j)*h,y1+h*tmp,k(j)) */
     /* I need the following helpers:
	PetscScalar  tmp_t=t+c(j)*h
	Vec          tmp_y=h*tmp+y1
     */

     tmp_t = t + rk->c[j] * h;

     /* tmp_y = h * tmp + y1 */
     ierr = VecWAXPY(&hh,rk->tmp,rk->y1,rk->tmp_y);CHKERRQ(ierr);

     /* rk->k[j]=0 */
     ierr = VecSet(&null,rk->k[j]);CHKERRQ(ierr);
     ierr = TSComputeRHSFunction(ts,tmp_t,rk->tmp_y,rk->k[j]);CHKERRQ(ierr);
  }	

  /* tmp=0 and tmp_y=0 */
  ierr = VecSet(&null,rk->tmp);CHKERRQ(ierr);
  ierr = VecSet(&null,rk->tmp_y);CHKERRQ(ierr);
  
  for(j = 0 ; j < rk->s ; j++){
     /* tmp=b1[j]*k[j]+tmp  */
     ierr = VecAXPY(&rk->b1[j],rk->k[j],rk->tmp);CHKERRQ(ierr);
     /* tmp_y=b2[j]*k[j]+tmp_y */
     ierr = VecAXPY(&rk->b2[j],rk->k[j],rk->tmp_y);CHKERRQ(ierr);
  }

  /* y2 = hh * tmp_y */
  ierr = VecSet(&null,rk->y2);CHKERRQ(ierr);  
  ierr = VecAXPY(&hh,rk->tmp_y,rk->y2);CHKERRQ(ierr);
  /* y1 = hh*tmp + y1 */
  ierr = VecAXPY(&hh,rk->tmp,rk->y1);CHKERRQ(ierr);
  /* Finding difference between y1 and y2 */

  PetscFunctionReturn(0);
}

/*------------------------------------------------------------*/
#undef __FUNCT__  
#define __FUNCT__ "TSDestroy_Rk"
static int TSDestroy_Rk(TS ts)
{
  TS_Rk *rk = (TS_Rk*)ts->data;
  int      i,ierr;

  /* REMEMBER TO DESTROY ALL */
  
  PetscFunctionBegin;
  if (rk->y1) {ierr = VecDestroy(rk->y1);CHKERRQ(ierr);}
  if (rk->y2) {ierr = VecDestroy(rk->y2);CHKERRQ(ierr);}
  if (rk->tmp) {ierr = VecDestroy(rk->tmp);CHKERRQ(ierr);}
  if (rk->tmp_y) {ierr = VecDestroy(rk->tmp_y);CHKERRQ(ierr);}
  for(i=0;i<rk->s;i++){
     if (rk->k[i]) {ierr = VecDestroy(rk->k[i]);CHKERRQ(ierr);}
  }
  ierr = PetscFree(rk);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
/*------------------------------------------------------------*/

#undef __FUNCT__  
#define __FUNCT__ "TSSetFromOptions_Rk"
static int TSSetFromOptions_Rk(TS ts)
{
  PetscFunctionBegin;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSView_Rk"
static int TSView_Rk(TS ts,PetscViewer viewer)
{
   TS_Rk *rk = (TS_Rk*)ts->data;
   int ierr;
   double elapsed;
   
   PetscFunctionBegin;
   ierr = PetscPrintf(PETSC_COMM_WORLD,"  number of ok steps: %d\n",rk->nok);CHKERRQ(ierr);
   ierr = PetscPrintf(PETSC_COMM_WORLD,"  number of rejected steps: %d\n",rk->nnok);CHKERRQ(ierr);
   /*   elapsed = ((double) (rk->end - rk->start)) / CLOCKS_PER_SEC;
   
   ierr = PetscPrintf(PETSC_COMM_WORLD,"  CPU time used (in seconds): %f\n",elapsed);CHKERRQ(ierr); */
   PetscFunctionReturn(0);
}

/* ------------------------------------------------------------ */
EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "TSCreate_Rk"
int TSCreate_Rk(TS ts)
{
  TS_Rk    *rk;
  int      ierr;

  PetscFunctionBegin;
  ts->ops->setup           = TSSetUp_Rk;
  ts->ops->step            = TSStep_Rk;
  ts->ops->destroy         = TSDestroy_Rk;
  ts->ops->setfromoptions  = TSSetFromOptions_Rk;
  ts->ops->view            = TSView_Rk;

  ierr = PetscNew(TS_Rk,&rk);CHKERRQ(ierr);
  PetscLogObjectMemory(ts,sizeof(TS_Rk));
  ts->data = (void*)rk;

  PetscFunctionReturn(0);
}
EXTERN_C_END




