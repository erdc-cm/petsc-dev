/*$Id: readinvecs.c,v 1.8 2001/03/23 23:19:53 balay Exp $*/

/*    Reads in PETSc vectors from a PETSc binary file into matlab

  Since this is called from Matlab it cannot be compiled with C++.
*/


#include "petscsys.h"
#include "petscvec.h"
#include "mex.h"
#include <fcntl.h>
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
#if defined (HAVE_IO_H)
#include <io.h>
#endif

#define ERROR(a) {fprintf(stdout,"ReadInVecs %s \n",a); return -1;}
/*-----------------------------------------------------------------*/
/*
       Reads in a single vector
*/
#undef __FUNCT__  
#define __FUNCT__ "ReadInVecs"
int ReadInVecs(Matrix *plhs[],int t,int dim,int *dims)
{
  int    cookie = 0,M,compx = 0,i;
  
  /* get size of matrix */
  if (PetscBinaryRead(t,&cookie,1,PETSC_INT))   return -1;  /* finished reading file */
  if (cookie != VEC_COOKIE) ERROR("could not read vector cookie");
  if (PetscBinaryRead(t,&M,1,PETSC_INT))        ERROR("reading number rows"); 
  
  if (dim == 1) {
    plhs[0]  = mxCreateFull(M,1,compx);
  } else if (dim == 2) {
    if (dims[0]*dims[1] != M) {
      printf("ERROR: m %d * n %d != M %d\n",dims[0],dims[1],M);
      return -1;
    }
    plhs[0]  = mxCreateFull(dims[0],dims[1],compx);
  } else {
    plhs[0] = mxCreateNumericArray(dim,dims,mxDOUBLE_CLASS,mxREAL);
  }

  /* read in matrix */
  if (!compx) {
    if (PetscBinaryRead(t,mxGetPr(plhs[0]),M,PETSC_DOUBLE)) ERROR("read dense matrix");
  } else {
    for (i=0; i<M; i++) {
      if (PetscBinaryRead(t,mxGetPr(plhs[0])+i,1,PETSC_DOUBLE)) ERROR("read dense matrix");
      if (PetscBinaryRead(t,mxGetPi(plhs[0])+i,1,PETSC_DOUBLE)) ERROR("read dense matrix");
    }
  }
  return 0;
}

#undef ERROR
#define ERROR(a) {fprintf(stdout,"ReadInVecs %s \n",a); return;}
/*-----------------------------------------------------------------*/

#undef __FUNCT__  
#define __FUNCT__ "mexFunction"
void mexFunction(int nlhs,Matrix *plhs[],int nrhs,Matrix *prhs[])
{
  static int fd = -1,dims[4],dim = 1,dof;
  char       filename[256],buffer[1024];
  int        err,d2,d3,d4;
  FILE       *file;

  /* check output parameters */
  if (nlhs != 1) ERROR("Receive requires one output argument.");
  if (fd == -1) {
    if (!mxIsString(prhs[0])) ERROR("First arg must be string.");
  
    /* open the file */
    mxGetString(prhs[0],filename,256);
    fd = open(filename,O_RDONLY,0);

    strcat(filename,".info");
    file = fopen(filename,"r");
    if (file) {
      fgets(buffer,1024,file);
      if (!strncmp(buffer,"-daload_info",12)) {
        sscanf(buffer,"-daload_info %d,%d,%d,%d,%d,%d,%d,%d\n",&dim,&dims[0],&dims[1],&dims[2],&dof,&d2,&d3,&d4);
        if (dof > 1) {
          dim++;
          dims[3] = dims[2];
          dims[2] = dims[1];
          dims[1] = dims[0];
          dims[0] = dof;
        }
      }
      fclose(file);
    }
  }

  /* read in the next vector */
  err = ReadInVecs(plhs,fd,dim,dims);

  if (err) {  /* file is finished so close and allow a restart */
    close(fd);
    fd = -1; 
  }
  return;
}


 
