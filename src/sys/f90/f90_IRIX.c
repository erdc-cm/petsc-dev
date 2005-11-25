
/*-------------------------------------------------------------*/

#undef __FUNCT__  
#define __FUNCT__ "F90GetID"
PetscErrorCode F90GetID(PetscDataType type,PetscInt *id)
{
  PetscFunctionBegin;
  if (type == PETSC_INT) {
    *id = F90_INT_ID;
  } else if (type == PETSC_DOUBLE) {
    *id = F90_DOUBLE_ID;
#if defined(PETSC_USE_COMPLEX)
  } else if (type == PETSC_COMPLEX) {
    *id = F90_COMPLEX_ID;
#endif
  } else if (type == PETSC_LONG) {
    *id = F90_LONG_ID;
  } else {
    SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"Unknown PETSc datatype");
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "F90Array1dCreate"
PetscErrorCode F90Array1dCreate(void *array,PetscDataType type,PetscInt start,PetscInt len,F90Array1d *ptr)
{
  PetscInt       size,size_int,id;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidPointer(array,1);
  PetscValidPointer(ptr,5);  
  ierr               = PetscDataTypeGetSize(type,&size);CHKERRQ(ierr);
  ierr               = PetscDataTypeGetSize(PETSC_INT,&size_int);CHKERRQ(ierr);
  ierr               = F90GetID(type,&id);

  ptr->addr          = array;
  ptr->sd            = size*8;
  ptr->cookie        = F90_COOKIE;
  ptr->ndim          = 1;
  ptr->id            = id;
  ptr->a             = 0;
  ptr->addr_r        = ptr->addr;
  ptr->size          = ptr->sd * len;
  ptr->dim[0].extent = len;
  ptr->dim[0].mult   = size/size_int;
  ptr->dim[0].lower  = start;

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "F90Array2dCreate"
PetscErrorCode F90Array2dCreate(void *array,PetscDataType type,PetscInt start1,PetscInt len1,PetscInt start2,PetscInt len2,F90Array2d *ptr)
{
  PetscInt size,size_int,id;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidPointer(array,1);
  PetscValidPointer(ptr,7);  
  ierr               = PetscDataTypeGetSize(type,&size);CHKERRQ(ierr);
  ierr               = PetscDataTypeGetSize(PETSC_INT,&size_int);CHKERRQ(ierr);
  ierr               = F90GetID(type,&id);

  ptr->addr          = array;
  ptr->sd            = size*8;
  ptr->cookie        = F90_COOKIE;
  ptr->ndim          = 2;
  ptr->id            = id;
  ptr->a             = 0;
  ptr->addr_r        = ptr->addr;
  ptr->size          = ptr->sd*len1*len2;
  ptr->dim[0].extent = len1;
  ptr->dim[0].mult   = size/size_int;
  ptr->dim[0].lower  = start1;
  ptr->dim[1].extent = len2;
  ptr->dim[1].mult   = len1*size/size_int;
  ptr->dim[1].lower  = len2;
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "F90Array3dCreate"
PetscErrorCode F90Array3dCreate(void *array,PetscDataType type,PetscInt start1,PetscInt len1,PetscInt start2,PetscInt len2,PetscInt start3,PetscInt len3,F90Array3d *ptr)
{
  PetscInt size,size_int,id;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidPointer(array,1);
  PetscValidPointer(ptr,9);  
  ierr               = PetscDataTypeGetSize(type,&size);CHKERRQ(ierr);
  ierr               = PetscDataTypeGetSize(PETSC_INT,&size_int);CHKERRQ(ierr);
  ierr               = F90GetID(type,&id);

  ptr->addr          = array;
  ptr->sd            = size*8;
  ptr->cookie        = F90_COOKIE;
  ptr->ndim          = 3;
  ptr->id            = id;
  ptr->a             = 0;
  ptr->addr_r        = ptr->addr;
  ptr->size          = ptr->sd*len1*len2*len3;
  ptr->dim[0].extent = len1;
  ptr->dim[0].mult   = size/size_int;
  ptr->dim[0].lower  = start1;
  ptr->dim[1].extent = len2;
  ptr->dim[1].mult   = len1*size/size_int;
  ptr->dim[1].lower  = len2;
  ptr->dim[2].extent = len3;
  ptr->dim[2].mult   = len2*len1*size/size_int;
  ptr->dim[2].lower  = len3;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "F90Array34dCreate"
PetscErrorCode F90Array4dCreate(void *array,PetscDataType type,PetscInt start1,PetscInt len1,PetscInt start2,PetscInt len2,PetscInt start3,PetscInt len3,PetscInt start4,PetscInt len4,F90Array4d *ptr)
{
  PetscInt size,size_int,id;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidPointer(array,1);
  PetscValidPointer(ptr,11);  
  ierr               = PetscDataTypeGetSize(type,&size);CHKERRQ(ierr);
  ierr               = PetscDataTypeGetSize(PETSC_INT,&size_int);CHKERRQ(ierr);
  ierr               = F90GetID(type,&id);

  ptr->addr          = array;
  ptr->sd            = size*8;
  ptr->cookie        = F90_COOKIE;
  ptr->ndim          = 4;
  ptr->id            = id;
  ptr->a             = 0;
  ptr->addr_r        = ptr->addr;
  ptr->size          = ptr->sd*len1*len2*len3*len4;
  ptr->dim[0].extent = len1;
  ptr->dim[0].mult   = size/size_int;
  ptr->dim[0].lower  = start1;
  ptr->dim[1].extent = len2;
  ptr->dim[1].mult   = len1*size/size_int;
  ptr->dim[1].lower  = len2;
  ptr->dim[2].extent = len3;
  ptr->dim[2].mult   = len2*len1*size/size_int;
  ptr->dim[2].lower  = len3;
  ptr->dim[3].extent = len4;
  ptr->dim[3].mult   = len3*len2*len1*size/size_int;
  ptr->dim[3].lower  = len4;
  PetscFunctionReturn(0);
}

/*-------------------------------------------------------------*/
