/*$Id: sysio.c,v 1.81 2001/08/10 17:39:07 balay Exp $*/

/* 
   This file contains simple binary read/write routines.
 */

#include "petsc.h"     /*I          "petsc.h"    I*/
#include "petscsys.h"

#include <errno.h>
#include <fcntl.h>
#if defined(PETSC_HAVE_UNISTD_H)
#include <unistd.h>
#endif
#if defined (PARCH_win32)
#include <io.h>
#endif
#include "petscbt.h"


#if !defined(PETSC_WORDS_BIGENDIAN)
#undef __FUNCT__  
#define __FUNCT__ "PetscByteSwapInt"
/*
  PetscByteSwapInt - Swap bytes in an integer
*/
int PetscByteSwapInt(int *buff,int n)
{
  int  i,j,tmp =0;
  int  *tptr = &tmp;                /* Need to access tmp indirectly to get */
  char *ptr1,*ptr2 = (char*)&tmp; /* arround the bug in DEC-ALPHA g++ */
                                   
  PetscFunctionBegin;
  for (j=0; j<n; j++) {
    ptr1 = (char*)(buff + j);
    for (i=0; i<(int) sizeof(int); i++) {
      ptr2[i] = ptr1[sizeof(int)-1-i];
    }
    buff[j] = *tptr;
  }
  PetscFunctionReturn(0);
}
/* --------------------------------------------------------- */
#undef __FUNCT__  
#define __FUNCT__ "PetscByteSwapShort"
/*
  PetscByteSwapShort - Swap bytes in a short
*/
int PetscByteSwapShort(short *buff,int n)
{
  int   i,j;
  short tmp;
  short *tptr = &tmp;           /* take care pf bug in DEC-ALPHA g++ */
  char  *ptr1,*ptr2 = (char*)&tmp;

  PetscFunctionBegin;
  for (j=0; j<n; j++) {
    ptr1 = (char*)(buff + j);
    for (i=0; i<(int) sizeof(short); i++) {
      ptr2[i] = ptr1[sizeof(int)-1-i];
    }
    buff[j] = *tptr;
  }
  PetscFunctionReturn(0);
}
/* --------------------------------------------------------- */
#undef __FUNCT__  
#define __FUNCT__ "PetscByteSwapScalar"
/*
  PetscByteSwapScalar - Swap bytes in a double
  Complex is dealt with as if array of double twice as long.
*/
int PetscByteSwapScalar(PetscScalar *buff,int n)
{
  int       i,j;
  PetscReal tmp,*buff1 = (PetscReal*)buff;
  PetscReal *tptr = &tmp;          /* take care pf bug in DEC-ALPHA g++ */
  char      *ptr1,*ptr2 = (char*)&tmp;

  PetscFunctionBegin;
#if defined(PETSC_USE_COMPLEX)
  n *= 2;
#endif
  for (j=0; j<n; j++) {
    ptr1 = (char*)(buff1 + j);
    for (i=0; i<(int) sizeof(PetscReal); i++) {
      ptr2[i] = ptr1[sizeof(PetscReal)-1-i];
    }
    buff1[j] = *tptr;
  }
  PetscFunctionReturn(0);
}
/* --------------------------------------------------------- */
#undef __FUNCT__  
#define __FUNCT__ "PetscByteSwapDouble"
/*
  PetscByteSwapDouble - Swap bytes in a double
*/
int PetscByteSwapDouble(double *buff,int n)
{
  int    i,j;
  double tmp,*buff1 = (double*)buff;
  double *tptr = &tmp;          /* take care pf bug in DEC-ALPHA g++ */
  char   *ptr1,*ptr2 = (char*)&tmp;

  PetscFunctionBegin;
  for (j=0; j<n; j++) {
    ptr1 = (char*)(buff1 + j);
    for (i=0; i<(int) sizeof(double); i++) {
      ptr2[i] = ptr1[sizeof(double)-1-i];
    }
    buff1[j] = *tptr;
  }
  PetscFunctionReturn(0);
}
#endif
/* --------------------------------------------------------- */
#undef __FUNCT__  
#define __FUNCT__ "PetscBinaryRead"
/*@C
   PetscBinaryRead - Reads from a binary file.

   Not Collective

   Input Parameters:
+  fd - the file
.  n  - the number of items to read 
-  type - the type of items to read (PETSC_INT, PETSC_DOUBLE or PETSC_SCALAR)

   Output Parameters:
.  p - the buffer

   Options Database Key:
.   -binary_longints - indicates the file was generated on a Cray vector 
         machine (not the T3E/D) and the ints are stored as 64 bit 
         quantities, otherwise they are stored as 32 bit

   Level: developer

   Notes: 
   PetscBinaryRead() uses byte swapping to work on all machines.
   Integers are stored on the file as 32 long, regardless of whether
   they are stored in the machine as 32 or 64, this means the same
   binary file may be read on any machine.

   Note that Cray C90 and similar machines cannot generate files with 
   32 bit integers; use the flag -binary_longints to read files from the 
   C90 on non-C90 machines. Cray T3E/T3D are the same as other Unix
   machines, not the same as the C90.

   Concepts: files^reading binary
   Concepts: binary files^reading

.seealso: PetscBinaryWrite(), PetscBinaryOpen(), PetscBinaryClose()
@*/
int PetscBinaryRead(int fd,void *p,int n,PetscDataType type)
{
  int               maxblock = 65536,wsize,err,m = n,ierr;
  static PetscTruth longintset = PETSC_FALSE,longintfile = PETSC_FALSE;
  PetscTruth        flg;
  char              *pp = (char*)p;
#if (PETSC_SIZEOF_SHORT != 8)
  void              *ptmp = p; 
#endif

  PetscFunctionBegin;
  if (!n) PetscFunctionReturn(0);

  if (!longintset) {
    ierr = PetscOptionsHasName(PETSC_NULL,"-binary_longints",&longintfile);CHKERRQ(ierr);
    ierr = PetscOptionsHasName(PETSC_NULL,"-help",&flg);CHKERRQ(ierr);
    if (flg) {
      ierr = (*PetscHelpPrintf)(PETSC_COMM_SELF,"-binary_longints - for binary file generated\n\
   on a Cray vector machine (not T3E/T3D)\n");CHKERRQ(ierr);
    }
    longintset = PETSC_TRUE;
  }

#if (PETSC_SIZEOF_INT == 8 && PETSC_SIZEOF_SHORT == 4)
  if (type == PETSC_INT){
    if (longintfile) {
      m *= sizeof(int);
    } else {
      /* read them in as shorts, later stretch into ints */
      m   *= sizeof(short);
      ierr = PetscMalloc(m,&pp);CHKERRQ(ierr);
      ptmp = (void*)pp;
    }
  }
#elif (PETSC_SIZEOF_INT == 8 && PETSC_SIZEOF_SHORT == 8)
  if (type == PETSC_INT){
    if (longintfile) {
      m *= sizeof(int);
    } else {
      SETERRQ(1,"Can only process data file generated on Cray vector machine;\n\
      if this data WAS then run program with -binary_longints option");
    }
  }
#else
  if (type == PETSC_INT) {
    if (longintfile) {
       /* read in twice as many ints and later discard every other one */
       m    *= 2*sizeof(int);
       ierr = PetscMalloc(m,&pp);CHKERRQ(ierr);
       ptmp =  (void*)pp;
    } else {
       m *= sizeof(int);
    }
  }
#endif
  else if (type == PETSC_SCALAR)  m *= sizeof(PetscScalar);
  else if (type == PETSC_DOUBLE)  m *= sizeof(double);
  else if (type == PETSC_SHORT)   m *= sizeof(short);
  else if (type == PETSC_CHAR)    m *= sizeof(char);
  else if (type == PETSC_LOGICAL) m = PetscBTLength(m)*sizeof(char);
  else SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"Unknown type");
  
  while (m) {
    wsize = (m < maxblock) ? m : maxblock;
    err = read(fd,pp,wsize);
    if (err < 0 && errno == EINTR) continue;
    if (err == 0 && wsize > 0) SETERRQ(PETSC_ERR_FILE_READ,"Read past end of file");
    if (err < 0) SETERRQ(PETSC_ERR_FILE_READ,"Error reading from file");
    m  -= err;
    pp += err;
  }
#if !defined(PETSC_WORDS_BIGENDIAN)
  if      (type == PETSC_INT)    {ierr = PetscByteSwapInt((int*)ptmp,n);CHKERRQ(ierr);}
  else if (type == PETSC_SCALAR) {ierr = PetscByteSwapScalar((PetscScalar*)ptmp,n);CHKERRQ(ierr);}
  else if (type == PETSC_DOUBLE) {ierr = PetscByteSwapDouble((double*)ptmp,n);CHKERRQ(ierr);}
  else if (type == PETSC_SHORT)  {ierr = PetscByteSwapShort((short*)ptmp,n);CHKERRQ(ierr);}
#endif

#if (PETSC_SIZEOF_INT == 8 && PETSC_SIZEOF_SHORT == 4)
  if (type == PETSC_INT){
    if (!longintfile) {
      int   *p_int = (int*)p,i;
      short *p_short = (short *)ptmp;
      for (i=0; i<n; i++) {
        p_int[i] = (int)p_short[i];
      }
      ierr = PetscFree(ptmp);CHKERRQ(ierr);
    }
  }
#elif (PETSC_SIZEOF_INT == 8 && PETSC_SIZEOF_SHORT == 8)
#else
  if (type == PETSC_INT){
    if (longintfile) {
    /* 
       take the longs (treated as pair of ints) and convert them to ints
    */
      int   *p_int  = (int*)p,i;
      int   *p_intl = (int *)ptmp;
      for (i=0; i<n; i++) {
        p_int[i] = (int)p_intl[2*i+1];
      }
      ierr = PetscFree(ptmp);CHKERRQ(ierr);
    }
  }
#endif

  PetscFunctionReturn(0);
}
/* --------------------------------------------------------- */
#undef __FUNCT__  
#define __FUNCT__ "PetscBinaryWrite"
/*@C
   PetscBinaryWrite - Writes to a binary file.

   Not Collective

   Input Parameters:
+  fd     - the file
.  p      - the buffer
.  n      - the number of items to write
.  type   - the type of items to read (PETSC_INT, PETSC_DOUBLE or PETSC_SCALAR)
-  istemp - 0 if buffer data should be preserved, 1 otherwise.

   Level: advanced

   Notes: 
   PetscBinaryWrite() uses byte swapping to work on all machines.
   Integers are stored on the file as 32 long, regardless of whether
   they are stored in the machine as 32 or 64, this means the same
   binary file may be read on any machine.

   The Buffer p should be read-write buffer, and not static data.
   This way, byte-swapping is done in-place, and then the buffer is
   written to the file.
   
   This routine restores the original contents of the buffer, after 
   it is written to the file. This is done by byte-swapping in-place 
   the second time. If the flag istemp is set to 1, the second
   byte-swapping operation is not done, thus saving some computation,
   but the buffer corrupted is corrupted.

   Concepts: files^writing binary
   Concepts: binary files^writing

.seealso: PetscBinaryRead(), PetscBinaryOpen(), PetscBinaryClose()
@*/
int PetscBinaryWrite(int fd,void *p,int n,PetscDataType type,int istemp)
{
  char *pp = (char*)p;
  int  err,maxblock,wsize,m = n;
#if !defined(PETSC_WORDS_BIGENDIAN) || (PETSC_SIZEOF_INT == 8)
  int  ierr;
  void *ptmp = p; 
#endif

  PetscFunctionBegin;
  if (n < 0) SETERRQ1(1,"Trying to write a negative amount of data %d",n);
  if (!n) PetscFunctionReturn(0);

  maxblock = 65536;

#if !defined(PETSC_WORDS_BIGENDIAN)
  if      (type == PETSC_INT)    {ierr = PetscByteSwapInt((int*)ptmp,n);CHKERRQ(ierr);}
  else if (type == PETSC_SCALAR) {ierr = PetscByteSwapScalar((PetscScalar*)ptmp,n);CHKERRQ(ierr);}
  else if (type == PETSC_DOUBLE) {ierr = PetscByteSwapDouble((double*)ptmp,n);CHKERRQ(ierr);}
  else if (type == PETSC_SHORT)  {ierr = PetscByteSwapShort((short*)ptmp,n);CHKERRQ(ierr);}
#endif

#if (PETSC_SIZEOF_INT == 8)
  if (type == PETSC_INT){
    /* 
      integers on the Cray T3d/e are 64 bits so we copy the big
      integers into a short array and write those out.
    */
    int   *p_int = (int*)p,i;
    short *p_short;
    m       *= sizeof(short);
    ierr    = PetscMalloc(m,&pp);CHKERRQ(ierr);
    ptmp    = (void*)pp;
    p_short = (short*)pp;

    for (i=0; i<n; i++) {
      p_short[i] = (short) p_int[i];
    }
  }
#else
  if (type == PETSC_INT)          m *= sizeof(int);
#endif
  else if (type == PETSC_SCALAR)  m *= sizeof(PetscScalar);
  else if (type == PETSC_DOUBLE)  m *= sizeof(double);
  else if (type == PETSC_SHORT)   m *= sizeof(short);
  else if (type == PETSC_CHAR)    m *= sizeof(char);
  else if (type == PETSC_LOGICAL) m = PetscBTLength(m)*sizeof(char);
  else SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"Unknown type");

  while (m) {
    wsize = (m < maxblock) ? m : maxblock;
    err = write(fd,pp,wsize);
    if (err < 0 && errno == EINTR) continue;
    if (err != wsize) SETERRQ(PETSC_ERR_FILE_WRITE,"Error writing to file.");
    m -= wsize;
    pp += wsize;
  }

#if !defined(PETSC_WORDS_BIGENDIAN)
  if (!istemp) {
    if      (type == PETSC_SCALAR) {ierr = PetscByteSwapScalar((PetscScalar*)ptmp,n);CHKERRQ(ierr);}
    else if (type == PETSC_SHORT)  {ierr = PetscByteSwapShort((short*)ptmp,n);CHKERRQ(ierr);}
    else if (type == PETSC_INT)    {ierr = PetscByteSwapInt((int*)ptmp,n);CHKERRQ(ierr);}
  }
#endif

#if (PETSC_SIZEOF_INT == 8)
  if (type == PETSC_INT){
    ierr = PetscFree(ptmp);CHKERRQ(ierr);
  }
#endif

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscBinaryOpen" 
/*@C
   PetscBinaryOpen - Opens a PETSc binary file.

   Not Collective

   Input Parameters:
+  name - filename
-  type - type of binary file, on of PETSC_FILE_RDONLY, PETSC_FILE_WRONLY, PETSC_FILE_CREATE

   Output Parameter:
.  fd - the file

   Level: advanced

  Concepts: files^opening binary
  Concepts: binary files^opening

.seealso: PetscBinaryRead(), PetscBinaryWrite()
@*/
int PetscBinaryOpen(const char name[],int type,int *fd)
{
  PetscFunctionBegin;
#if defined(PARCH_win32_gnu) || defined(PARCH_win32) 
  if (type == PETSC_FILE_CREATE) {
    if ((*fd = open(name,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0666)) == -1) {
      SETERRQ1(PETSC_ERR_FILE_OPEN,"Cannot create file for writing: %s",name);
    }
  } else if (type == PETSC_FILE_RDONLY) {
    if ((*fd = open(name,O_RDONLY|O_BINARY,0)) == -1) {
      SETERRQ1(PETSC_ERR_FILE_OPEN,"Cannot open file for reading: %s",name);
    }
  } else if (type == PETSC_FILE_WRONLY) {
    if ((*fd = open(name,O_WRONLY|O_BINARY,0)) == -1) {
      SETERRQ1(PETSC_ERR_FILE_OPEN,"Cannot open file for writing: %s",name);
    }
#else
  if (type == PETSC_FILE_CREATE) {
    if ((*fd = creat(name,0666)) == -1) {
      SETERRQ1(PETSC_ERR_FILE_OPEN,"Cannot create file for writing: %s",name);
    }
  } else if (type == PETSC_FILE_RDONLY) {
    if ((*fd = open(name,O_RDONLY,0)) == -1) {
      SETERRQ1(PETSC_ERR_FILE_OPEN,"Cannot open file for reading: %s",name);
    }
  }
  else if (type == PETSC_FILE_WRONLY) {
    if ((*fd = open(name,O_WRONLY,0)) == -1) {
      SETERRQ1(PETSC_ERR_FILE_OPEN,"Cannot open file for writing: %s",name);
    }
#endif
  } else SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"Unknown file type");
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscBinaryClose" 
/*@C
   PetscBinaryClose - Closes a PETSc binary file.

   Not Collective

   Output Parameter:
.  fd - the file

   Level: advanced

.seealso: PetscBinaryRead(), PetscBinaryWrite(), PetscBinaryOpen()
@*/
int PetscBinaryClose(int fd)
{
  PetscFunctionBegin;
  close(fd);
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "PetscBinarySeek" 
/*@C
   PetscBinarySeek - Moves the file pointer on a PETSc binary file.

   Not Collective

   Input Parameters:
+  fd - the file
.  whence - if PETSC_BINARY_SEEK_SET then size is an absolute location in the file
            if PETSC_BINARY_SEEK_CUR then size is offset from current location
            if PETSC_BINARY_SEEK_END then size is offset from end of file
-  size - number of bytes to move. Use PETSC_BINARY_INT_SIZE, PETSC_BINARY_SCALAR_SIZE,
            etc. in your calculation rather than sizeof() to compute byte lengths.

   Output Parameter:
.   offset - new offset in file

   Level: developer

   Notes: 
   Integers are stored on the file as 32 long, regardless of whether
   they are stored in the machine as 32 or 64, this means the same
   binary file may be read on any machine. Hence you CANNOT use sizeof()
   to determine the offset or location.

   Concepts: files^binary seeking
   Concepts: binary files^seeking

.seealso: PetscBinaryRead(), PetscBinaryWrite(), PetscBinaryOpen()
@*/
int PetscBinarySeek(int fd,int size,PetscBinarySeekType whence,int *offset)
{
  int iwhence=0;

  PetscFunctionBegin;
  if (whence == PETSC_BINARY_SEEK_SET) {
    iwhence = SEEK_SET;
  } else if (whence == PETSC_BINARY_SEEK_CUR) {
    iwhence = SEEK_CUR;
  } else if (whence == PETSC_BINARY_SEEK_END) {
    iwhence = SEEK_END;
  } else {
    SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"Unknown seek location");
  }
#if defined(PARCH_win32)
  *offset = _lseek(fd,(long)size,iwhence);
#else
  *offset = lseek(fd,(off_t)size,iwhence);
#endif

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscSynchronizedBinaryRead"
/*@C
   PetscSynchronizedBinaryRead - Reads from a binary file.

   Collective on MPI_Comm

   Input Parameters:
+  comm - the MPI communicator 
.  fd - the file
.  n  - the number of items to read 
-  type - the type of items to read (PETSC_INT, PETSC_DOUBLE or PETSC_SCALAR)

   Output Parameters:
.  p - the buffer

   Options Database Key:
.   -binary_longints - indicates the file was generated on a Cray vector 
         machine (not the T3E/D) and the ints are stored as 64 bit 
         quantities, otherwise they are stored as 32 bit

   Level: developer

   Notes: 
   Does a PetscBinaryRead() followed by an MPI_Bcast()

   PetscSynchronizedBinaryRead() uses byte swapping to work on all machines.
   Integers are stored on the file as 32 long, regardless of whether
   they are stored in the machine as 32 or 64, this means the same
   binary file may be read on any machine.

   Note that Cray C90 and similar machines cannot generate files with 
   32 bit integers; use the flag -binary_longints to read files from the 
   C90 on non-C90 machines. Cray T3E/T3D are the same as other Unix
   machines, not the same as the C90.

   Concepts: files^synchronized reading of binary files
   Concepts: binary files^reading, synchronized

.seealso: PetscBinaryWrite(), PetscBinaryOpen(), PetscBinaryClose(), PetscBinaryRead()
@*/
int PetscSynchronizedBinaryRead(MPI_Comm comm,int fd,void *p,int n,PetscDataType type)
{
  int          ierr,rank;
  MPI_Datatype mtype;

  PetscFunctionBegin;
  ierr = MPI_Comm_rank(comm,&rank);CHKERRQ(ierr);
  if (!rank) {
    ierr = PetscBinaryRead(fd,p,n,type);CHKERRQ(ierr);
  }
  ierr = PetscDataTypeToMPIDataType(type,&mtype);CHKERRQ(ierr);
  ierr = MPI_Bcast(p,n,mtype,0,comm);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscSynchronizedBinarySeek" 
/*@C
   PetscSynchronizedBinarySeek - Moves the file pointer on a PETSc binary file.

   Not Collective

   Input Parameters:
+  fd - the file
.  whence - if PETSC_BINARY_SEEK_SET then size is an absolute location in the file
            if PETSC_BINARY_SEEK_CUR then size is offset from current location
            if PETSC_BINARY_SEEK_END then size is offset from end of file
-  size - number of bytes to move. Use PETSC_BINARY_INT_SIZE, PETSC_BINARY_SCALAR_SIZE,
            etc. in your calculation rather than sizeof() to compute byte lengths.

   Output Parameter:
.   offset - new offset in file

   Level: developer

   Notes: 
   Integers are stored on the file as 32 long, regardless of whether
   they are stored in the machine as 32 or 64, this means the same
   binary file may be read on any machine. Hence you CANNOT use sizeof()
   to determine the offset or location.

   Concepts: binary files^seeking
   Concepts: files^seeking in binary 

.seealso: PetscBinaryRead(), PetscBinaryWrite(), PetscBinaryOpen()
@*/
int PetscSynchronizedBinarySeek(MPI_Comm comm,int fd,int size,PetscBinarySeekType whence,int *offset)
{
  int ierr,rank;

  PetscFunctionBegin;
  ierr = MPI_Comm_rank(comm,&rank);CHKERRQ(ierr);
  if (!rank) {
    ierr = PetscBinarySeek(fd,size,whence,offset);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

