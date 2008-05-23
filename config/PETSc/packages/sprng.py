#!/usr/bin/env python
from __future__ import generators
import user
import config.base
import os
import PETSc.package

class Configure(PETSc.package.Package):
  def __init__(self, framework):
    PETSc.package.Package.__init__(self, framework)
    self.download  = ['ftp://ftp.mcs.anl.gov/pub/petsc/externalpackages/sprng-1.0.tar.gz']
    self.functions = ['make_new_seed_mpi'] 
    self.includes  = ['sprng.h'] 
    self.liblist   = [['libcmrg.a','liblcg64.a','liblcg.a','liblfg.a','libmlfg.a']]
    return

  def setupDependencies(self, framework):
    PETSc.package.Package.setupDependencies(self, framework)
    self.mpi        = framework.require('config.packages.MPI',self)
    self.deps       = [self.mpi]
    return

  def Install(self):    

    g = open(os.path.join(self.packageDir,'SRC','make.PETSC'),'w')

    g.write('AR             = '+self.setCompilers.AR+'\n')
    g.write('ARFLAGS        = '+self.setCompilers.AR_FLAGS+'\n')
    g.write('AR_LIB_SUFFIX  = '+self.setCompilers.AR_LIB_SUFFIX+'\n')
    g.write('RANLIB         = '+self.setCompilers.RANLIB+'\n')

    self.setCompilers.pushLanguage('C')
    cflags = self.setCompilers.getCompilerFlags().replace('-Wall','').replace('-Wshadow','')
    cflags += ' ' + self.headers.toString(self.mpi.include)+' '+self.headers.toString('.')
    cflags += ' ' + '-DSPRNG_MPI' # either using MPI or MPIUNI

    g.write('CC             = '+self.setCompilers.getCompiler()+'\n')
    g.write('CFLAGS         = '+cflags+'\n')
    g.write('CLD            = $(CC)\n')
    g.write('MPICC          = $(CC)\n')
    g.write('CPP            ='+self.framework.getPreprocessor()+'\n')
    self.setCompilers.popLanguage()

    # extra unused options
    g.write('CLDFLAGS       = \n')
    g.write('F77            = echo\n')
    g.write('F77LD          = $(F77)\n')
    g.write('FFXN 	    = -DAdd_\n')
    g.write('FSUFFIX 	    = F\n')
    g.write('MPIF77 	    = echo\n')
    g.write('FFLAGS 	    = \n')
    g.write('F77LDFLAGS     = \n')
    g.close()

    if self.installNeeded(os.path.join('SRC','make.PETSC')):
      try:
        self.logPrintBox('Compiling SPRNG; this may take several minutes')
        output  = config.base.Configure.executeShellCommand('cd '+self.packageDir+';SPRNG_INSTALL_DIR='+self.installDir+';export SPRNG_INSTALL_DIR; make realclean; cd SRC; make; cd ..;  cp lib/*.a '+os.path.join(self.installDir,self.libdir)+'; cp include/*.h '+os.path.join(self.installDir,self.includedir)+'/.', timeout=2500, log = self.framework.log)[0]
      except RuntimeError, e:
        raise RuntimeError('Error running make on SPRNG: '+str(e))
      self.checkInstall(output,os.path.join('SRC','make.PETSC'))
    return self.installDir

if __name__ == '__main__':
  import config.framework
  import sys
  framework = config.framework.Framework(sys.argv[1:])
  framework.setupLogging(framework.clArgs)
  framework.children.append(Configure(framework))
  framework.configure()
  framework.dumpSubstitutions()
