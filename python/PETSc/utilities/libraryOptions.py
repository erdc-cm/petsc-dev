#!/usr/bin/env python
from __future__ import generators
import user
import config.base
import os

class Configure(config.base.Configure):
  def __init__(self, framework):
    config.base.Configure.__init__(self, framework)
    self.headerPrefix = ''
    self.substPrefix  = ''
    self.arch         = self.framework.require('PETSc.utilities.arch', self)
    self.framework.require('config.setCompilers',self)
    self.debugging    = self.framework.require('PETSc.utilities.debugging', self)
    return

  def __str__(self):
    return ''
    
  def setupHelp(self, help):
    import nargs
    help.addArgument('PETSc', '-with-log=<bool>',              nargs.ArgBool(None, 1, 'Activate logging code in PETSc'))
    help.addArgument('PETSc', '-with-ctable=<bool>',           nargs.ArgBool(None, 1, 'Use CTABLE hashing for certain search functions - to conserve memory'))
    help.addArgument('PETSc', '-with-fortran-kernels=<bool>',  nargs.ArgBool(None, 0, 'Use Fortran for linear algebra kernels'))
    help.addArgument('PETSc', '-with-64-bit-ints=<bool>',      nargs.ArgBool(None, 0, 'Use 64 bit integers (long long) for indexing in vectors and matrices'))
    return

  def configureLibraryOptions(self):
    '''Sets PETSC_USE_DEBUG, PETSC_USE_LOG, PETSC_USE_CTABLE and PETSC_USE_FORTRAN_KERNELS'''
    self.useLog   = self.framework.argDB['with-log']
    self.addDefine('USE_LOG',   self.useLog)

    if self.debugging.debugging:
      self.addDefine('USE_DEBUG', '1')

    self.useCtable = self.framework.argDB['with-ctable']
    if self.useCtable:
      self.addDefine('USE_CTABLE', '1')
    
    if not 'FC' in self.framework.argDB and self.framework.argDB['with-fortran-kernels']:
      raise RuntimeError('Cannot use fortran kernels without a Fortran compiler')
    self.useFortranKernels = self.framework.argDB['with-fortran-kernels']
    self.addDefine('USE_FORTRAN_KERNELS', self.useFortranKernels)

    if self.framework.argDB['with-64-bit-ints']:
      self.addDefine('USE_64BIT_INT', 1)
    else:
      self.addDefine('USE_32BIT_INT', 1)
    return


  def configure(self):
    self.executeTest(self.configureLibraryOptions)
    return
