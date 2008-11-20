
!
!  Include file for Fortran use of the DA (distributed array) package in PETSc
!
#include "finclude/petscdadef.h"

#if defined(PETSC_USE_FORTRAN_MODULES)
      type DM
        PetscFortranAddr:: v
      end type DM
      type DA
        PetscFortranAddr:: v
      end type DA
#endif
!
!  Types of stencils
!
      PetscEnum DA_STENCIL_STAR
      PetscEnum DA_STENCIL_BOX

      parameter (DA_STENCIL_STAR = 0,DA_STENCIL_BOX = 1)
!
!  Types of periodicity
!
      PetscEnum DA_NONPERIODIC
      PetscEnum DA_XPERIODIC
      PetscEnum DA_YPERIODIC
      PetscEnum DA_XYPERIODIC
      PetscEnum DA_XYZPERIODIC
      PetscEnum DA_XZPERIODIC
      PetscEnum DA_YZPERIODIC
      PetscEnum DA_ZPERIODIC
      PetscEnum DA_XYZGHOSTED

      parameter (DA_NONPERIODIC = 0,DA_XPERIODIC = 1,DA_YPERIODIC = 2)
      parameter (DA_XYPERIODIC = 3,DA_XYZPERIODIC = 4)
      parameter (DA_XZPERIODIC = 5,DA_YZPERIODIC = 6,DA_ZPERIODIC = 7)
      parameter (DA_XYZGHOSTED = 8)
!
! DA Directions
!
      PetscEnum DA_X
      PetscEnum DA_Y
      PetscEnum DA_Z

      parameter (DA_X = 0,DA_Y = 1,DA_Z = 2)
!
!  End of Fortran include file for the DA package in PETSc

