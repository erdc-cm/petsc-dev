
/**********************************types.h*************************************
SPARSE GATHER-SCATTER PACKAGE: bss_malloc bss_malloc ivec error comm gs queue

Author: Henry M. Tufo III

e-mail: hmt@cs.brown.edu

snail-mail:
Division of Applied Mathematics
Brown University
Providence, RI 02912

Last Modification: 
6.21.97
***********************************types.h************************************/

/**********************************types.h*************************************
File Description:
-----------------

***********************************types.h************************************/
typedef void (*vfp)(void*,void*,int,...);
typedef void (*rbfp)(REAL *, REAL *, int len);
#define vbfp MPI_User_function *
typedef int (*bfp)(void*, void *, int *len, DATA_TYPE *dt); 
/* typedef REAL (*bfp)(REAL, REAL); */
