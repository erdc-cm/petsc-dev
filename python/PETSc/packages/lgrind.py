#!/usr/bin/env python
from __future__ import generators
import config.base
import os
import re
    
class Configure(config.base.Configure):
  def __init__(self, framework):
    config.base.Configure.__init__(self, framework)
    self.headerPrefix = 'PETSC'
    self.substPrefix  = 'PETSC'
    self.argDB        = framework.argDB
    self.compilers    = self.framework.require('config.compilers', self)
    return

  def getDir(self):
    '''Find the directory containing lgrind'''
    packages  = os.path.join(self.framework.argDB['PETSC_DIR'], 'packages')
    if not os.path.isdir(packages):
      os.mkdir(packages)
      self.framework.actions.addArgument('PETSc', 'Directory creation', 'Created the packages directory: '+packages)
    lgrindDir = None
    for dir in os.listdir(packages):
      if dir == 'lgrind-dev' and os.path.isdir(os.path.join(packages, dir)):
        lgrindDir = dir
    if lgrindDir is None:
      raise RuntimeError('Error locating lgrind directory')
    return os.path.join(packages, lgrindDir)

  def downLoadLgrind(self):
    import commands
    self.framework.log.write('Downloading LGRIND\n')
    try:
      lgrindDir = self.getDir()
    except RuntimeError:
      import urllib

      packages = os.path.join(self.framework.argDB['PETSC_DIR'], 'packages')
      try:
        self.framework.log.write('Downloading it using "bk clone bk://petsc.bkbits.net/lgrind-dev $PETSC_DIR/packages/lgrind-dev"''\n')
        (status,output) = commands.getstatusoutput('bk clone bk://petsc.bkbits.net/lgrind-dev packages/lgrind-dev')
        if status:
          if output.find('ommand not found') >= 0:
            print '''******** Unable to locate bk (Bitkeeper) to download BuildSystem; make sure bk is in your path'''
          elif output.find('Cannot resolve host') >= 0:
            print '''******** Unable to download lgrind. You must be off the network. Connect to the internet and run config/configure.py again******** '''
          else:
            print '''******** Unable to download lgrind. Please send this message to petsc-maint@mcs.anl.gov******** '''
            print output
            sys.exit(3)
      except RuntimeError, e:
        raise RuntimeError('Error bk cloneing lgrind '+str(e))        
      self.framework.actions.addArgument('LGRIND', 'Download', 'Downloaded lgrind into '+self.getDir())
      
    # Get the LGRIND directories
    lgrindDir  = self.getDir()
    installDir = os.path.join(lgrindDir, self.framework.argDB['PETSC_ARCH'])
    if not os.path.isdir(installDir):
      os.mkdir(installDir)
    try:
      output  = config.base.Configure.executeShellCommand('cd '+os.path.join(lgrindDir,'source')+';make', timeout=2500, log = self.framework.log)[0]
    except RuntimeError, e:
      raise RuntimeError('Error running make on lgrind: '+str(e))
    try:
      output  = config.base.Configure.executeShellCommand('cp '+os.path.join(lgrindDir,'source','lgrind')+' '+installDir, timeout=2500, log = self.framework.log)[0]
    except RuntimeError, e:
      raise RuntimeError('Error copying lgrind executable: '+str(e))
    self.framework.actions.addArgument('lgrind', 'Install', 'Installed lgrind into '+installDir)
    self.lgrind = lgrindDir
    return

  def configureLgrind(self):
    '''Determine whether the Lgrind exist or not'''
    if os.path.exists(os.path.join(self.framework.argDB['PETSC_DIR'], 'BitKeeper')):
      self.framework.log.write('BitKeeper clone of PETSc, checking for Lgrind\n')
      self.downLoadLgrind()
        
      if hasattr(self, 'lgrind'):
        self.framework.addSubstitution('LGRIND', self.lgrind)
      else:
        raise RuntimeError('Could not install Lgrind\n')
    else:
      self.framework.log.write("Not BitKeeper clone of PETSc, don't need Lgrind\n")
    return

  def configure(self):
    self.executeTest(self.configureLgrind)
    return

if __name__ == '__main__':
  import config.framework
  import sys
  framework = config.framework.Framework(sys.argv[1:])
  framework.setupLogging(sys.argv[1:])
  framework.children.append(Configure(framework))
  framework.configure()
  framework.dumpSubstitutions()
