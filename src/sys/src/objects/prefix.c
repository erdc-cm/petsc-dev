/*$Id: prefix.c,v 1.30 2001/03/23 23:20:38 balay Exp $*/
/*
     Provides utility routines for manulating any type of PETSc object.
*/
#include "petsc.h"  /*I   "petsc.h"    I*/

#undef __FUNCT__  
#define __FUNCT__ "PetscObjectSetOptionsPrefix"
/*
   PetscObjectSetOptionsPrefix - Sets the prefix used for searching for all 
   options of PetscObjectType in the database. 

   Input Parameters:
.  obj - any PETSc object, for example a Vec, Mat or KSP.
.  prefix - the prefix string to prepend to option requests of the object.

   Notes: 
   A hyphen (-) must NOT be given at the beginning of the prefix name.
   The first character of all runtime options is AUTOMATICALLY the
   hyphen.

   Concepts: prefix^setting

*/
int PetscObjectSetOptionsPrefix(PetscObject obj,const char prefix[])
{
  int ierr;

  PetscFunctionBegin;
  ierr = PetscStrfree(obj->prefix);CHKERRQ(ierr);
  if (!prefix) {
    obj->prefix = PETSC_NULL;
  } else {
     if (prefix[0] == '-') SETERRQ(PETSC_ERR_ARG_WRONG,"Options prefix should not begin with a hypen");
    ierr  = PetscStrallocpy(prefix,&obj->prefix);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscObjectAppendOptionsPrefix"
/*
   PetscObjectAppendOptionsPrefix - Sets the prefix used for searching for all 
   options of PetscObjectType in the database. 

   Input Parameters:
.  obj - any PETSc object, for example a Vec, Mat or KSP.
.  prefix - the prefix string to prepend to option requests of the object.

   Notes: 
   A hyphen (-) must NOT be given at the beginning of the prefix name.
   The first character of all runtime options is AUTOMATICALLY the
   hyphen.

   Concepts: prefix^setting

*/
int PetscObjectAppendOptionsPrefix(PetscObject obj,const char prefix[])
{
  char *buf = obj->prefix ;
  int  ierr,len1,len2;

  PetscFunctionBegin;
  if (!prefix) {PetscFunctionReturn(0);}
  if (!buf) {
    ierr = PetscObjectSetOptionsPrefix(obj,prefix);CHKERRQ(ierr);
    PetscFunctionReturn(0);
  }
  if (prefix[0] == '-') SETERRQ(PETSC_ERR_ARG_WRONG,"Options prefix should not begin with a hypen");

  ierr  = PetscStrlen(prefix,&len1);CHKERRQ(ierr);
  ierr  = PetscStrlen(buf,&len2);CHKERRQ(ierr);
  ierr  = PetscMalloc((1+len1+len2)*sizeof(char),&obj->prefix);CHKERRQ(ierr);
  ierr  = PetscStrcpy(obj->prefix,buf);CHKERRQ(ierr);
  ierr  = PetscStrcat(obj->prefix,prefix);CHKERRQ(ierr);
  ierr  = PetscFree(buf);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscObjectGetOptionsPrefix"
/*
   PetscObjectGetOptionsPrefix - Gets the prefix of the PetscObject.

   Input Parameters:
.  obj - any PETSc object, for example a Vec, Mat or KSP.

   Output Parameters:
.  prefix - pointer to the prefix string used is returned

   Concepts: prefix^getting

*/
int PetscObjectGetOptionsPrefix(PetscObject obj,char *prefix[])
{
  PetscFunctionBegin;
  *prefix = obj->prefix;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PetscObjectPrependOptionsPrefix"
/*
   PetscObjectPrependOptionsPrefix - Sets the prefix used for searching for all 
   options of PetscObjectType in the database. 

   Input Parameters:
.  obj - any PETSc object, for example a Vec, Mat or KSP.
.  prefix - the prefix string to prepend to option requests of the object.

   Notes: 
   A hyphen (-) must NOT be given at the beginning of the prefix name.
   The first character of all runtime options is AUTOMATICALLY the
   hyphen.

   Concepts: prefix^setting

*/
int PetscObjectPrependOptionsPrefix(PetscObject obj,const char prefix[])
{
  char *buf = obj->prefix ;
  int  ierr,len1,len2;

  PetscFunctionBegin;
  if (!prefix) {PetscFunctionReturn(0);}
  if (!buf) {
    ierr = PetscObjectSetOptionsPrefix(obj,prefix);CHKERRQ(ierr);
    PetscFunctionReturn(0);
  }
  if (prefix[0] == '-') SETERRQ(PETSC_ERR_ARG_WRONG,"Options prefix should not begin with a hypen");

  ierr = PetscStrlen(prefix,&len1);CHKERRQ(ierr);
  ierr = PetscStrlen(buf,&len2);CHKERRQ(ierr);
  ierr = PetscMalloc((1+len1+len2)*sizeof(char),&obj->prefix);CHKERRQ(ierr);
  ierr = PetscStrcpy(obj->prefix,prefix);CHKERRQ(ierr);
  ierr = PetscStrcat(obj->prefix,buf);CHKERRQ(ierr);
  ierr = PetscFree(buf);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

