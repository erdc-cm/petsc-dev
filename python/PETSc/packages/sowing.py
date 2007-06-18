from __future__ import generators
import config.base
import os
import re
import PETSc.package

class Configure(PETSc.package.Package):
  def __init__(self, framework):
    PETSc.package.Package.__init__(self, framework)
    self.download     = ['ftp://ftp.mcs.anl.gov/pub/petsc/tmp/sowing.tar.gz']
    return

  def Install(self):
    sowingDir = self.getDir()
    # Configure and Build sowing
    args = ['--prefix='+self.installDir]
    args = ' '.join(args)
    try:
      fd      = file(os.path.join(self.confDir,'sowing'))
      oldargs = fd.readline()
      fd.close()
    except:
      oldargs = ''
    if not oldargs == args:
      self.framework.log.write('Need to configure and compile Sowing: old args = '+oldargs+'\n new args ='+args+'\n')
      try:
        output  = config.base.Configure.executeShellCommand('cd '+sowingDir+';./configure '+args, timeout=900, log = self.framework.log)[0]
      except RuntimeError, e:
        if self.framework.argDB['with-batch']:
          return
        else:
          raise RuntimeError('Error running configure on Sowing: '+str(e))
      try:
        output  = config.base.Configure.executeShellCommand('cd '+sowingDir+';make; make install; make clean', timeout=2500, log = self.framework.log)[0]
      except RuntimeError, e:
        if self.framework.argDB['with-batch']:
          return
        else:
          raise RuntimeError('Error running make; make install on Sowing: '+str(e))
      fd = file(os.path.join(self.confDir,'sowing'), 'w')
      fd.write(args)
      fd.close()
      self.framework.actions.addArgument('Sowing', 'Install', 'Installed Sowing into '+self.installDir)
    self.binDir   = os.path.join(self.installDir, 'bin')
    self.bfort    = os.path.join(self.binDir, 'bfort')
    self.doctext  = os.path.join(self.binDir, 'doctext')
    self.mapnames = os.path.join(self.binDir, 'mapnames')
    # Bill's bug he does not install bib2html so use original location if needed
    if os.path.isfile(os.path.join(self.binDir, 'bib2html')):
      self.bib2html = os.path.join(self.binDir, 'bib2html')
    else:
      self.bib2html = os.path.join(sowingDir,'bin', 'bib2html')
    for prog in [self.bfort, self.doctext, self.mapnames]:
      if not (os.path.isfile(prog) and os.access(prog, os.X_OK)):
        raise RuntimeError('Error in Sowing installation: Could not find '+prog)
    self.addMakeMacro('BFORT ', self.bfort)
    self.addMakeMacro('DOCTEXT ', self.doctext)
    self.addMakeMacro('MAPNAMES ', self.mapnames)
    self.addMakeMacro('BIB2HTML ', self.bib2html)    
    self.getExecutable('pdflatex', getFullPath = 1)
    return

  def buildFortranStubs(self):
    if hasattr(self.compilers, 'FC'):
      if self.framework.argDB['with-batch'] and not hasattr(self,'bfort'):
        self.logPrintBox('Batch build that could not generate bfort, skipping generating Fortran stubs\n \
                          you will need to copy them from some other system (src/fortran/auto)')
      else:     
        self.framework.log.write('           Running '+self.bfort+' to generate fortran stubs\n')
        try:
          import sys
          sys.path.insert(0, os.path.abspath('maint'))
          import generatefortranstubs
          del sys.path[0]
          generatefortranstubs.main(self.bfort)
          self.framework.actions.addArgument('PETSc', 'File creation', 'Generated Fortran stubs and made makefile.src')
        except RuntimeError, e:
          raise RuntimeError('*******Error generating Fortran stubs: '+str(e)+'*******\n')
    return

  def configure(self):
    '''Determine whether the Sowing exist or not'''
    if self.petscdir.isClone:
      self.framework.logPrint('PETSc clone, checking for Sowing\n')
      self.installDir  = os.path.join(self.petscdir.dir,self.arch.arch)
      self.confDir     = os.path.join(self.petscdir.dir,self.arch.arch,'conf')
      self.Install()
      self.buildFortranStubs()
    else:
      self.framework.logPrint("Not a clone of PETSc, don't need Sowing\n")
    return

