/*$Id: hists.c,v 1.26 2001/04/10 19:34:23 bsmith Exp $*/

/*
  Contains the data structure for plotting a histogram in a window with an axis.
*/

#include "petsc.h"         /*I "petsc.h" I*/

struct _p_DrawHG {
  PETSCHEADER(int) 
  int           (*destroy)(PetscDrawSP);
  int           (*view)(PetscDrawSP,PetscViewer);
  PetscDraw     win;
  PetscDrawAxis axis;
  PetscReal     xmin,xmax;
  PetscReal     ymin,ymax;
  int           numBins;
  int           maxBins;
  PetscReal     *bins;
  int           numValues;
  int           maxValues;
  PetscReal     *values;
  int           color;
  PetscTruth    calcStats;
  PetscTruth    integerBins;
};

#define CHUNKSIZE 100

#undef __FUNCT__  
#define __FUNCT__ "PetscDrawHGCreate" 
/*@C
   PetscDrawHGCreate - Creates a histogram data structure.

   Collective over PetscDraw

   Input Parameters:
+  draw  - The window where the graph will be made
-  bins - The number of bins to use

   Output Parameters:
.  hist - The histogram context

   Level: intermediate

   Contributed by: Matthew Knepley

   Concepts: histogram^creating

.seealso: PetscDrawHGDestroy()

@*/
int PetscDrawHGCreate(PetscDraw draw,int bins,PetscDrawHG *hist)
{
  int         ierr;
  PetscTruth  isnull;
  PetscObject obj = (PetscObject)draw;
  PetscDrawHG h;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(draw,PETSC_DRAW_COOKIE);
  PetscValidPointer(hist);
  ierr = PetscTypeCompare(obj,PETSC_DRAW_NULL,&isnull);CHKERRQ(ierr);
  if (isnull) {
    ierr = PetscDrawOpenNull(obj->comm,(PetscDraw*)hist);CHKERRQ(ierr);
    PetscFunctionReturn(0);
  }
  PetscHeaderCreate(h,_p_DrawHG,int,DRAWHG_COOKIE,0,"PetscDrawHG",obj->comm,PetscDrawHGDestroy,0);
  h->view        = 0;
  h->destroy     = 0;
  h->win         = draw;
  h->color       = PETSC_DRAW_GREEN;
  h->xmin        = PETSC_MAX;
  h->xmax        = PETSC_MIN;
  h->ymin        = 0.;
  h->ymax        = 1.;
  h->numBins     = bins;
  h->maxBins     = bins;
  ierr = PetscMalloc(h->maxBins * sizeof(PetscReal), &h->bins);CHKERRQ(ierr);
  h->numValues   = 0;
  h->maxValues   = CHUNKSIZE;
  h->calcStats   = PETSC_FALSE;
  h->integerBins = PETSC_FALSE;
  ierr = PetscMalloc(h->maxValues * sizeof(PetscReal), &h->values);CHKERRQ(ierr);
  PetscLogObjectMemory(h,bins*sizeof(PetscReal) + h->maxValues*sizeof(PetscReal));
  ierr = PetscDrawAxisCreate(draw, &h->axis);CHKERRQ(ierr);
  PetscLogObjectParent(h, h->axis);
  *hist = h;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscDrawHGSetNumberBins" 
/*@
   PetscDrawHGSetNumberBins - Change the number of bins that are to be drawn.

   Not Collective (ignored except on processor 0 of PetscDrawHG)

   Input Parameter:
+  hist - The histogram context.
-  dim  - The number of curves.

   Level: intermediate

  Contributed by: Matthew Knepley

   Concepts: histogram^setting number of bins

@*/
int PetscDrawHGSetNumberBins(PetscDrawHG hist,int bins)
{
  int ierr;

  PetscFunctionBegin;
  if (hist && hist->cookie == PETSC_DRAW_COOKIE) PetscFunctionReturn(0);

  PetscValidHeaderSpecific(hist,DRAWHG_COOKIE);
  if (hist->maxBins < bins) {
    ierr = PetscFree(hist->bins);                                                                         CHKERRQ(ierr);
    ierr = PetscMalloc(bins * sizeof(PetscReal), &hist->bins);                                            CHKERRQ(ierr);
    PetscLogObjectMemory(hist, (bins - hist->maxBins) * sizeof(PetscReal));
    hist->maxBins = bins;
  }
  hist->numBins = bins;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscDrawHGReset" 
/*@
  PetscDrawHGReset - Clears histogram to allow for reuse with new data.

  Not Collective (ignored except on processor 0 of PetscDrawHG)

  Input Parameter:
. hist - The histogram context.

   Level: intermediate

  Contributed by: Matthew Knepley

   Concepts: histogram^resetting

@*/
int PetscDrawHGReset(PetscDrawHG hist)
{
  PetscFunctionBegin;
  if (hist && hist->cookie == PETSC_DRAW_COOKIE) PetscFunctionReturn(0);
  PetscValidHeaderSpecific(hist,DRAWHG_COOKIE);
  hist->xmin      = PETSC_MAX;
  hist->xmax      = PETSC_MIN;
  hist->ymin      = 0;
  hist->ymax      = 0;
  hist->numValues = 0;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscDrawHGDestroy" 
/*@C
  PetscDrawHGDestroy - Frees all space taken up by histogram data structure.

  Collective over PetscDrawHG

  Input Parameter:
. hist - The histogram context

   Level: intermediate

  Contributed by: Matthew Knepley

.seealso:  PetscDrawHGCreate()
@*/
int PetscDrawHGDestroy(PetscDrawHG hist)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeader(hist);

  if (--hist->refct > 0) PetscFunctionReturn(0);
  if (hist->cookie == PETSC_DRAW_COOKIE){
    ierr = PetscDrawDestroy((PetscDraw) hist);CHKERRQ(ierr);
    PetscFunctionReturn(0);
  }

  ierr = PetscDrawAxisDestroy(hist->axis);CHKERRQ(ierr);
  ierr = PetscFree(hist->bins);CHKERRQ(ierr);
  ierr = PetscFree(hist->values);CHKERRQ(ierr);
  PetscLogObjectDestroy(hist);
  PetscHeaderDestroy(hist);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscDrawHGAddValue" 
/*@
  PetscDrawHGAddValue - Adds another value to the histogram.

  Not Collective (ignored except on processor 0 of PetscDrawHG)

  Input Parameters:
+ hist  - The histogram
- value - The value 

   Level: intermediate

  Contributed by: Matthew Knepley

  Concepts: histogram^adding values

.seealso: PetscDrawHGAddValues()
@*/
int PetscDrawHGAddValue(PetscDrawHG hist,PetscReal value)
{
  PetscFunctionBegin;
  if (hist && hist->cookie == PETSC_DRAW_COOKIE) PetscFunctionReturn(0);

  PetscValidHeaderSpecific(hist,DRAWHG_COOKIE);
  /* Allocate more memory if necessary */
  if (hist->numValues >= hist->maxValues) {
    PetscReal *tmp;
    int        ierr;

    ierr = PetscMalloc((hist->maxValues+CHUNKSIZE) * sizeof(PetscReal), &tmp);                            CHKERRQ(ierr);
    PetscLogObjectMemory(hist, CHUNKSIZE * sizeof(PetscReal));
    ierr = PetscMemcpy(tmp, hist->values, hist->maxValues * sizeof(PetscReal));                           CHKERRQ(ierr);
    ierr = PetscFree(hist->values);                                                                       CHKERRQ(ierr);
    hist->values     = tmp;
    hist->maxValues += CHUNKSIZE;
  }
  if (!hist->numValues) {
    hist->xmin = value;
    hist->xmax = value;
#if 1
  } else { 
    /* Update limits */
    if (value > hist->xmax)
      hist->xmax = value;
    if (value < hist->xmin)
      hist->xmin = value;
#else
  } else if (hist->numValues == 1) {
    /* Update limits -- We need to overshoot the largest value somewhat */
    if (value > hist->xmax) {
      hist->xmax = value + 0.001*(value - hist->xmin)/hist->numBins;
    }
    if (value < hist->xmin) {
      hist->xmin = value;
      hist->xmax = hist->xmax + 0.001*(hist->xmax - hist->xmin)/hist->numBins;
    }
  } else {
    /* Update limits -- We need to overshoot the largest value somewhat */
    if (value > hist->xmax) {
      hist->xmax = value + 0.001*(hist->xmax - hist->xmin)/hist->numBins;
    }
    if (value < hist->xmin) {
      hist->xmin = value;
    }
#endif
  }

  hist->values[hist->numValues++] = value;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscDrawHGDraw" 
/*@
  PetscDrawHGDraw - Redraws a histogram.

  Not Collective (ignored except on processor 0 of PetscDrawHG)

  Input Parameter:
. hist - The histogram context

   Level: intermediate

  Contributed by: Matthew Knepley

@*/
int PetscDrawHGDraw(PetscDrawHG hist)
{
  PetscDraw draw;
  PetscReal xmin,xmax,ymin,ymax,*bins,*values,binSize,binLeft,binRight,maxHeight,mean,var;
  char      title[256];
  char      xlabel[256];
  int       numBins,numBinsOld,numValues,initSize,i,p,ierr,bcolor,color,rank;

  PetscFunctionBegin;
  if (hist && hist->cookie == PETSC_DRAW_COOKIE) PetscFunctionReturn(0);
  PetscValidHeaderSpecific(hist,DRAWHG_COOKIE);
  if ((hist->xmin >= hist->xmax) || (hist->ymin >= hist->ymax)) PetscFunctionReturn(0);
  if (hist->numValues < 1) PetscFunctionReturn(0);

#if 0
  ierr = MPI_Comm_rank(hist->comm,&rank);CHKERRQ(ierr);
  if (rank) PetscFunctionReturn(0);
#endif

  color = hist->color; 
  if (color == PETSC_DRAW_ROTATE) {bcolor = 2;} else {bcolor = color;}
  draw      = hist->win;
  xmin      = hist->xmin;
  xmax      = hist->xmax;
  ymin      = hist->ymin;
  ymax      = hist->ymax;
  numValues = hist->numValues;
  values    = hist->values;
  mean      = 0.0;
  var       = 0.0;
 
  ierr = PetscDrawClear(draw);CHKERRQ(ierr);
  if (xmin == xmax) {
    /* Calculate number of points in each bin */
    bins    = hist->bins;
    bins[0] = 0;
    for(p = 0; p < numValues; p++) {
      if (values[p] == xmin) bins[0]++;
      mean += values[p];
      var  += values[p]*values[p];
    }
    maxHeight = bins[0];
    if (maxHeight > ymax) ymax = hist->ymax = maxHeight;
    xmax = xmin + 1;
    ierr = PetscDrawAxisSetLimits(hist->axis, xmin, xmax, ymin, ymax);                                    CHKERRQ(ierr);
    if (hist->calcStats) {
      mean /= numValues;
      if (numValues > 1) {
        var = (var - numValues*mean*mean) / (numValues-1);
      } else {
        var = 0.0;
      }
      sprintf(title,  "Mean: %g  Var: %g", mean, var);
      sprintf(xlabel, "Total: %d", numValues);
      ierr  = PetscDrawAxisSetLabels(hist->axis, title, xlabel, PETSC_NULL);                              CHKERRQ(ierr);
    }
    ierr = PetscDrawAxisDraw(hist->axis);                                                                 CHKERRQ(ierr);
    /* Draw bins */
    binLeft   = xmin;
    binRight  = xmax;
    ierr = PetscDrawRectangle(draw,binLeft,ymin,binRight,bins[0],bcolor,bcolor,bcolor,bcolor);            CHKERRQ(ierr);
    if (color == PETSC_DRAW_ROTATE && bins[0]) bcolor++; if (bcolor > 31) bcolor = 2;
    ierr = PetscDrawLine(draw,binLeft,ymin,binLeft,bins[0],PETSC_DRAW_BLACK);                             CHKERRQ(ierr);
    ierr = PetscDrawLine(draw,binRight,ymin,binRight,bins[0],PETSC_DRAW_BLACK);                           CHKERRQ(ierr);
    ierr = PetscDrawLine(draw,binLeft,bins[0],binRight,bins[0],PETSC_DRAW_BLACK);                         CHKERRQ(ierr);
  } else {
    numBins    = hist->numBins;
    numBinsOld = hist->numBins;
    if ((hist->integerBins == PETSC_TRUE) && (((int) xmax - xmin) + 1.0e-05 > xmax - xmin)) {
      initSize = ((int) xmax - xmin)/numBins;
      while (initSize*numBins != (int) xmax - xmin) {
        initSize = PetscMax(initSize - 1, 1);
        numBins  = ((int) xmax - xmin)/initSize;
        ierr     = PetscDrawHGSetNumberBins(hist, numBins);                                               CHKERRQ(ierr);
      }
    }
    binSize = (xmax - xmin)/numBins;
    bins    = hist->bins;

    ierr = PetscMemzero(bins, numBins * sizeof(PetscReal));                                               CHKERRQ(ierr);
    maxHeight = 0;
    for (i = 0; i < numBins; i++) {
      binLeft   = xmin + binSize*i;
      binRight  = xmin + binSize*(i+1);
      for(p = 0; p < numValues; p++) {
        if ((values[p] >= binLeft) && (values[p] < binRight)) bins[i]++;
        /* Handle last bin separately */
        if ((i == numBins-1) && (values[p] == binRight)) bins[i]++;
        if (i == 0) {
          mean += values[p];
          var  += values[p]*values[p];
        }
      }
      maxHeight = PetscMax(maxHeight, bins[i]);
    }
    if (maxHeight > ymax) ymax = hist->ymax = maxHeight;

    ierr = PetscDrawAxisSetLimits(hist->axis, xmin, xmax, ymin, ymax);                                    CHKERRQ(ierr);
    if (hist->calcStats) {
      mean /= numValues;
      if (numValues > 1) {
        var = (var - numValues*mean*mean) / (numValues-1);
      } else {
        var = 0.0;
      }
      sprintf(title, "Mean: %g  Var: %g", mean, var);
      sprintf(xlabel, "Total: %d", numValues);
      ierr  = PetscDrawAxisSetLabels(hist->axis, title, xlabel, PETSC_NULL);                              CHKERRQ(ierr);
    }
    ierr = PetscDrawAxisDraw(hist->axis);                                                                 CHKERRQ(ierr);
    /* Draw bins */
    for (i = 0; i < numBins; i++) {
      binLeft   = xmin + binSize*i;
      binRight  = xmin + binSize*(i+1);
      ierr = PetscDrawRectangle(draw,binLeft,ymin,binRight,bins[i],bcolor,bcolor,bcolor,bcolor);          CHKERRQ(ierr);
      if (color == PETSC_DRAW_ROTATE && bins[i]) bcolor++; if (bcolor > 31) bcolor = 2;
      ierr = PetscDrawLine(draw,binLeft,ymin,binLeft,bins[i],PETSC_DRAW_BLACK);                           CHKERRQ(ierr);
      ierr = PetscDrawLine(draw,binRight,ymin,binRight,bins[i],PETSC_DRAW_BLACK);                         CHKERRQ(ierr);
      ierr = PetscDrawLine(draw,binLeft,bins[i],binRight,bins[i],PETSC_DRAW_BLACK);                       CHKERRQ(ierr);
    }
    ierr = PetscDrawHGSetNumberBins(hist, numBinsOld);                                                    CHKERRQ(ierr);
  }
  ierr = PetscDrawSynchronizedFlush(draw);                                                                CHKERRQ(ierr);
  ierr = PetscDrawPause(draw);                                                                            CHKERRQ(ierr);
  PetscFunctionReturn(0);
} 

#undef __FUNC__  
#define __FUNC__ "PetscDrawHGPrint"
/*@
  PetscDrawHGPrint - Prints the histogram information.

  Not collective

  Input Parameter:
. hist - The histogram context

  Level: beginner

  Contributed by: Matthew Knepley

.keywords:  draw, histogram
@*/
int PetscDrawHGPrint(PetscDrawHG hist)
{
  double   xmax,xmin,*bins,*values,binSize,binLeft,binRight,mean,var;
  int      numBins,numBinsOld,numValues,initSize,i,p,ierr;

  PetscFunctionBegin;
  if (hist && hist->cookie == PETSC_DRAW_COOKIE) PetscFunctionReturn(0);
  PetscValidHeaderSpecific(hist,DRAWHG_COOKIE);
  if ((hist->xmin > hist->xmax) || (hist->ymin >= hist->ymax)) PetscFunctionReturn(0);
  if (hist->numValues < 1) PetscFunctionReturn(0);

  xmax      = hist->xmax;
  xmin      = hist->xmin;
  numValues = hist->numValues;
  values    = hist->values;
  mean      = 0.0;
  var       = 0.0;
  if (xmax == xmin) {
    /* Calculate number of points in the bin */
    bins    = hist->bins;
    bins[0] = 0;
    for(p = 0; p < numValues; p++) {
      if (values[p] == xmin) bins[0]++;
      mean += values[p];
      var  += values[p]*values[p];
    }
    /* Draw bins */
    PetscPrintf(hist->comm, "Bin %2d (%6.2lf - %6.2lf): %.0lf\n", 0, xmin, xmax, bins[0]);
  } else {
    numBins    = hist->numBins;
    numBinsOld = hist->numBins;
    if ((hist->integerBins == PETSC_TRUE) && (((int) xmax - xmin) + 1.0e-05 > xmax - xmin)) {
      initSize = ((int) xmax - xmin)/numBins;
      while (initSize*numBins != (int) xmax - xmin) {
        initSize = PetscMax(initSize - 1, 1);
        numBins  = ((int) xmax - xmin)/initSize;
        ierr     = PetscDrawHGSetNumberBins(hist, numBins);                                               CHKERRQ(ierr);
      }
    }
    binSize = (xmax - xmin)/numBins;
    bins    = hist->bins;

    /* Calculate number of points in each bin */
    ierr = PetscMemzero(bins, numBins * sizeof(PetscReal));                                               CHKERRQ(ierr);
    for (i = 0; i < numBins; i++) {
      binLeft   = xmin + binSize*i;
      binRight  = xmin + binSize*(i+1);
      for(p = 0; p < numValues; p++) {
        if ((values[p] >= binLeft) && (values[p] < binRight)) bins[i]++;
        /* Handle last bin separately */
        if ((i == numBins-1) && (values[p] == binRight)) bins[i]++;
        if (i == 0) {
          mean += values[p];
          var  += values[p]*values[p];
        }
      }
    }
    /* Draw bins */
    for (i = 0; i < numBins; i++) {
      binLeft   = xmin + binSize*i;
      binRight  = xmin + binSize*(i+1);
      PetscPrintf(hist->comm, "Bin %2d (%6.2lf - %6.2lf): %.0lf\n", i, binLeft, binRight, bins[i]);
    }
    ierr = PetscDrawHGSetNumberBins(hist, numBinsOld);                                                    CHKERRQ(ierr);
  }

  if (hist->calcStats) {
    mean /= numValues;
    if (numValues > 1) {
      var = (var - numValues*mean*mean) / (numValues-1);
    } else {
      var = 0.0;
    }
    PetscPrintf(hist->comm, "Mean: %g  Var: %g\n", mean, var);
    PetscPrintf(hist->comm, "Total: %d\n", numValues);
  }
  PetscFunctionReturn(0);
} 
 
#undef __FUNCT__  
#define __FUNCT__ "PetscDrawHGSetColor" 
/*@
  PetscDrawHGSetColor - Sets the color the bars will be drawn with.

  Not Collective (ignored except on processor 0 of PetscDrawHG)

  Input Parameters:
+ hist - The histogram context
- color - one of the colors defined in petscdraw.h or PETSC_DRAW_ROTATE to make each bar a 
          different color

  Level: intermediate

@*/
int PetscDrawHGSetColor(PetscDrawHG hist,int color)
{
  PetscFunctionBegin;
  if (hist && hist->cookie == PETSC_DRAW_COOKIE) PetscFunctionReturn(0);
  PetscValidHeaderSpecific(hist,DRAWHG_COOKIE);
  hist->color = color;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscDrawHGSetLimits" 
/*@
  PetscDrawHGSetLimits - Sets the axis limits for a histogram. If more
  points are added after this call, the limits will be adjusted to
  include those additional points.

  Not Collective (ignored except on processor 0 of PetscDrawHG)

  Input Parameters:
+ hist - The histogram context
- x_min,x_max,y_min,y_max - The limits

  Level: intermediate

  Contributed by: Matthew Knepley

  Concepts: histogram^setting axis

@*/
int PetscDrawHGSetLimits(PetscDrawHG hist,PetscReal x_min,PetscReal x_max,int y_min,int y_max) 
{
  PetscFunctionBegin;
  if (hist && hist->cookie == PETSC_DRAW_COOKIE) PetscFunctionReturn(0);
  PetscValidHeaderSpecific(hist,DRAWHG_COOKIE);
  hist->xmin = x_min; 
  hist->xmax = x_max; 
  hist->ymin = y_min; 
  hist->ymax = y_max;
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PetscDrawHGCalcStats"
/*@
  PetscDrawHGCalcStats - Turns on calculation of descriptive statistics

  Not collective

  Input Parameters:
+ hist - The histogram context
- calc - Flag for calculation

  Level: intermediate

  Contributed by: Matthew Knepley

.keywords:  draw, histogram, statistics

@*/
int PetscDrawHGCalcStats(PetscDrawHG hist, PetscTruth calc)
{
  PetscFunctionBegin;
  if (hist && hist->cookie == PETSC_DRAW_COOKIE) PetscFunctionReturn(0);
  PetscValidHeaderSpecific(hist,DRAWHG_COOKIE);
  hist->calcStats = calc;
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PetscDrawHGIntegerBins"
/*@
  PetscDrawHGIntegerBins - Turns on integer width bins

  Not collective

  Input Parameters:
+ hist - The histogram context
- ints - Flag for integer width bins

  Level: intermediate

  Contributed by: Matthew Knepley

.keywords:  draw, histogram, statistics

@*/
int PetscDrawHGIntegerBins(PetscDrawHG hist, PetscTruth ints)
{
  PetscFunctionBegin;
  if (hist && hist->cookie == PETSC_DRAW_COOKIE) PetscFunctionReturn(0);
  PetscValidHeaderSpecific(hist,DRAWHG_COOKIE);
  hist->integerBins = ints;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscDrawHGGetAxis" 
/*@C
  PetscDrawHGGetAxis - Gets the axis context associated with a histogram.
  This is useful if one wants to change some axis property, such as
  labels, color, etc. The axis context should not be destroyed by the
  application code.

  Not Collective (ignored except on processor 0 of PetscDrawHG)

  Input Parameter:
. hist - The histogram context

  Output Parameter:
. axis - The axis context

  Level: intermediate

  Contributed by: Matthew Knepley

@*/
int PetscDrawHGGetAxis(PetscDrawHG hist,PetscDrawAxis *axis)
{
  PetscFunctionBegin;
  if (hist && hist->cookie == PETSC_DRAW_COOKIE) {
    *axis = 0;
    PetscFunctionReturn(0);
  }
  PetscValidHeaderSpecific(hist,DRAWHG_COOKIE);
  *axis = hist->axis;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscDrawHGGetDraw" 
/*@C
  PetscDrawHGGetDraw - Gets the draw context associated with a histogram.

  Not Collective, PetscDraw is parallel if PetscDrawHG is parallel

  Input Parameter:
. hist - The histogram context

  Output Parameter:
. win  - The draw context

  Level: intermediate

  Contributed by: Matthew Knepley

@*/
int PetscDrawHGGetDraw(PetscDrawHG hist,PetscDraw *win)
{
  PetscFunctionBegin;
  PetscValidHeader(hist);
  if (hist && hist->cookie == PETSC_DRAW_COOKIE) {
    *win = (PetscDraw)hist;
  } else {
    *win = hist->win;
  }
  PetscFunctionReturn(0);
}

