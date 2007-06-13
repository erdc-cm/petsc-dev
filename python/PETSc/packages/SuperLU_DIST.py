#!/usr/bin/env python
from __future__ import generators
import user
import config.base
import os
import PETSc.package

class Configure(PETSc.package.Package):
  def __init__(self, framework):
    PETSc.package.Package.__init__(self, framework)
    self.download   = ['ftp://ftp.mcs.anl.gov/pub/petsc/externalpackages/SuperLU_DIST_2.0-Jan_5_2006.tar.gz']
    self.functions  = ['set_default_options_dist']
    self.includes   = ['superlu_ddefs.h']
    self.liblist    = [['libsuperlu_dist_2.0.a']]
    self.complex    = 1
    return

  def setupDependencies(self, framework):
    PETSc.package.Package.setupDependencies(self, framework)
    self.mpi        = framework.require('config.packages.MPI',self)
    self.blasLapack = framework.require('config.packages.BlasLapack',self)
    self.deps       = [self.mpi,self.blasLapack]
    return

  def Install(self):
    # Get the SUPERLU_DIST directories
    superluDir = self.getDir()
    installDir = os.path.join(self.petscdir.dir,self.arch.arch)
    confDir = os.path.join(self.petscdir.dir,self.arch.arch,'conf')
    
    # Configure and Build SUPERLU_DIST
    if os.path.isfile(os.path.join(superluDir,'make.inc')):
      output  = config.base.Configure.executeShellCommand('cd '+superluDir+'; rm -f make.inc', timeout=2500, log = self.framework.log)[0]
    g = open(os.path.join(superluDir,'make.inc'),'w')
    g.write('DSuperLUroot = '+superluDir+'\n')
    g.write('DSUPERLULIB  = $(DSuperLUroot)/libsuperlu_dist_2.0.a\n')
    g.write('BLASDEF      = -DUSE_VENDOR_BLAS\n')
    g.write('BLASLIB      = '+self.libraries.toString(self.blasLapack.dlib)+'\n')
    g.write('IMPI         = '+self.headers.toString(self.mpi.include)+'\n')
    g.write('MPILIB       = '+self.libraries.toString(self.mpi.lib)+'\n')
    g.write('SYS_LIB      = \n')
    g.write('LIBS         = $(DSUPERLULIB) $(BLASLIB) $(PERFLIB) $(MPILIB) $(SYS_LIB)\n')
    g.write('ARCH         = '+self.setCompilers.AR+'\n')
    g.write('ARCHFLAGS    = '+self.setCompilers.AR_FLAGS+'\n')
    g.write('RANLIB       = '+self.setCompilers.RANLIB+'\n')
    self.setCompilers.pushLanguage('C')
    g.write('CC           = '+self.setCompilers.getCompiler()+' $(IMPI)\n') #build fails without $(IMPI)
    g.write('CFLAGS       = '+self.setCompilers.getCompilerFlags()+'\n')
    g.write('LOADER       = '+self.setCompilers.getLinker()+'\n') 
    g.write('LOADOPTS     = \n')
    self.setCompilers.popLanguage()
    if hasattr(self.compilers, 'FC'):
      self.setCompilers.pushLanguage('FC')
      g.write('FORTRAN      = '+self.setCompilers.getCompiler()+'\n')
      g.write('FFLAGS       = '+self.setCompilers.getCompilerFlags().replace('-Mfree','')+'\n')
      # set fortran name mangling
      if self.compilers.fortranMangling == 'underscore':
        g.write('CDEFS   = -DAdd_\n')
      elif self.compilers.fortranMangling == 'capitalize':
        g.write('CDEFS   = -DUpCase\n')
      else:
        g.write('CDEFS   = -DNoChange\n')
      self.setCompilers.popLanguage()
    else:
      raise RuntimeError('SuperLU_DIST requires a fortran compiler! No fortran compiler configured!')
    g.write('NOOPTS       =  -O0\n')
    g.close()
    if not os.path.isdir(installDir):
      os.mkdir(installDir)
    if not os.path.isfile(os.path.join(confDir,'SuperLU_DIST')) or not (self.getChecksum(os.path.join(confDir,'SuperLU_DIST')) == self.getChecksum(os.path.join(superluDir,'make.inc'))):  
      self.framework.log.write('Have to rebuild SUPERLU_DIST, make.inc != '+confDir+'/SuperLU_DIST\n')
      try:
        self.logPrintBox('Compiling superlu_dist; this may take several minutes')
        output  = config.base.Configure.executeShellCommand('cd '+superluDir+';SUPERLU_DIST_INSTALL_DIR='+installDir+'/lib;export SUPERLU_DIST_INSTALL_DIR; make clean; make lib LAAUX=""; mv *.a '+os.path.join(installDir,'lib')+'; cp SRC/*.h '+os.path.join(installDir,'include')+'/.', timeout=2500, log = self.framework.log)[0]
      except RuntimeError, e:
        raise RuntimeError('Error running make on SUPERLU_DIST: '+str(e))
      if not os.path.isfile(os.path.join(installDir,'lib','libsuperlu_dist_2.0.a')):
        self.framework.log.write('Error running make on SUPERLU_DIST   ******(libraries not installed)*******\n')
        self.framework.log.write('********Output of running make on SUPERLU_DIST follows *******\n')        
        self.framework.log.write(output)
        self.framework.log.write('********End of Output of running make on SUPERLU_DIST *******\n')
        raise RuntimeError('Error running make on SUPERLU_DIST, libraries not installed')
      
      output  = config.base.Configure.executeShellCommand('cp -f '+os.path.join(superluDir,'make.inc')+' '+confDir+'/SuperLU_DIST', timeout=5, log = self.framework.log)[0]
      self.framework.actions.addArgument(self.PACKAGE, 'Install', 'Installed SUPERLU_DIST into '+installDir)
    return installDir

  def configureLibrary(self):
    '''Calls the regular package configureLibrary and then does an additional test needed by SuperLU_DIST'''
    '''Normally you do not need to provide this method'''
    PETSc.package.Package.configureLibrary(self)
    if self.blasLapack.f2c:
      raise RuntimeError('SuperLU_DIST requires a COMPLETE BLAS and LAPACK, it cannot be used with --download-c-blas-lapack=1 \nUse --download-f-blas-lapack option instead.')

    errormsg = ', the current Lapack libraries '+str(self.blasLapack.lib)+' does not have it\nTry using --download-f-blas-lapack=1 option or see \nhttp://www-unix.mcs.anl.gov/petsc/petsc-as/documentation/installation.html#BLAS/LAPACK'
    if not self.blasLapack.checkForRoutine('slamch'): 
      raise RuntimeError('SuperLU_DIST requires the BLAS routine slamch()'+errormsg)
    self.framework.log.write('Found slamch() in BLAS library as needed by SuperLU_DIST\n')

    if not self.blasLapack.checkForRoutine('dlamch'): 
      raise RuntimeError('SuperLU_DIST requires the BLAS routine dlamch()'+errormsg)
    self.framework.log.write('Found dlamch() in BLAS library as needed by SuperLU_DIST\n')
    if not self.blasLapack.checkForRoutine('xerbla'): 
      raise RuntimeError('SuperLU_DIST requires the BLAS routine xerbla()'+errormsg)
    self.framework.log.write('Found xerbla() in BLAS library as needed by SuperLU_DIST\n')
    return
  
if __name__ == '__main__':
  import config.framework
  import sys
  framework = config.framework.Framework(sys.argv[1:])
  framework.setupLogging(framework.clArgs)
  framework.children.append(Configure(framework))
  framework.configure()
  framework.dumpSubstitutions()
