#!/usr/bin/env python
from __future__ import generators
import user
import config.base
import os
import PETSc.package

class Configure(PETSc.package.Package):
  def __init__(self, framework):
    PETSc.package.Package.__init__(self, framework)
    self.download   = ['ftp://ftp.mcs.anl.gov/pub/petsc/externalpackages/blopex_abstract_Aug_2006.tar.gz']
    self.functions  = ['lobpcg_solve']
    self.includes   = ['interpreter.h']
    self.liblist    = [['libBLOPEX.a']]
    self.complex    = 0
    return

  def setupDependencies(self, framework):
    PETSc.package.Package.setupDependencies(self, framework)
    self.mpi        = framework.require('config.packages.MPI',self)
    self.blasLapack = framework.require('config.packages.BlasLapack',self)
    if self.framework.argDB.has_key('download-hypre') and not self.framework.argDB['download-hypre'] == 0:
      self.hypre      = framework.require('PETSc.packages.hypre',self)
      self.deps       = [self.mpi,self.blasLapack,self.hypre]
    elif self.framework.argDB.has_key('with-hypre-dir') or self.framework.argDB.has_key('with-hypre-include') or self.framework.argDB.has_key('with-hypre-lib'):   
      self.hypre      = framework.require('PETSc.packages.hypre',self)
      self.deps       = [self.mpi,self.blasLapack,self.hypre]
    else:
      self.deps       = [self.mpi,self.blasLapack]
    return

  def Install(self):
    # Get the BLOPEX directories
    blopexDir = self.getDir()
    
    # Configure and Build BLOPEX
    g = open(os.path.join(blopexDir,'Makefile.inc'),'w')
    self.setCompilers.pushLanguage('C')
    g.write('CC          = '+self.setCompilers.getCompiler()+'\n') 
    g.write('CFLAGS      = ' + self.setCompilers.getCompilerFlags().replace('-Wall','').replace('-Wshadow','') +'\n')
    self.setCompilers.popLanguage()
    g.write('AR          = '+self.setCompilers.AR+' '+self.setCompilers.AR_FLAGS+'\n')
    g.write('RANLIB      = '+self.setCompilers.RANLIB+'\n')
    g.close()
    if not os.path.isfile(os.path.join(self.confDir,'blopex')) or not (self.getChecksum(os.path.join(self.confDir,'blopex')) == self.getChecksum(os.path.join(blopexDir,'Makefile.inc'))):
      self.framework.log.write('Have to rebuild BLOPEX, Makefile.inc != '+self.confDir+'/blopex\n')
      try:
        self.logPrintBox('Compiling blopex; this may take several minutes')
        output  = config.base.Configure.executeShellCommand('cd '+blopexDir+';BLOPEX_INSTALL_DIR='+self.installDir+';export BLOPEX_INSTALL_DIR; make clean; make; mv -f lib/* '+os.path.join(self.installDir,self.libdir)+'; cp -fp multivector/temp_multivector.h include/.; mv -f include/* '+os.path.join(self.installDir,self.includedir)+'', timeout=2500, log = self.framework.log)[0]
      except RuntimeError, e:
        raise RuntimeError('Error running make on BLOPEX: '+str(e))
      self.checkInstall(output)
      output  = config.base.Configure.executeShellCommand('cp -f '+os.path.join(blopexDir,'Makefile.inc')+' '+self.confDir+'/blopex', timeout=5, log = self.framework.log)[0]
      self.framework.actions.addArgument(self.PACKAGE, 'Install', 'Installed BLOPEX into '+self.installDir)
    else:
      self.framework.log.write('Do NOT need to compile BLOPEX downloaded libraries\n')  
    return installDir

if __name__ == '__main__':
  import config.framework
  import sys
  framework = config.framework.Framework(sys.argv[1:])
  framework.setup()
  framework.addChild(Configure(framework))
  framework.configure()
  framework.dumpSubstitutions()
