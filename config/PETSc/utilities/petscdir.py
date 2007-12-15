import config.base
import os
import re

class Configure(config.base.Configure):
  def __init__(self, framework):
    config.base.Configure.__init__(self, framework)
    self.headerPrefix = 'PETSC'
    self.substPrefix  = 'PETSC'
    self.isPetsc      = 1
    return

  def __str__(self):
    if not hasattr(self, 'dir'):
      return ''
    desc  = []
    cdir  = str(self.dir)
    envdir  = os.getenv('PETSC_DIR')
    if not cdir == envdir :
      desc.append('  **\n  ** Before running "make" your PETSC_DIR must be specified with:')
      desc.append('  **  ** setenv PETSC_DIR '+str(cdir)+' (csh/tcsh)')
      desc.append('  **  ** PETSC_DIR='+str(cdir)+'; export PETSC_DIR (sh/bash)\n  **')
    else:
      desc.append('  PETSC_DIR: '+str(self.dir))
    desc.append('  **\n  ** Now build and test the libraries with "make all test"\n  **')
    return '\n'.join(desc)+'\n'

  def setupHelp(self, help):
    import nargs
    help.addArgument('PETSc', '-PETSC_DIR',                        nargs.Arg(None, None, 'The root directory of the PETSc installation'))
    help.addArgument('PETSc', '-with-installation-method=<method>', nargs.Arg(None, 'tarball', 'Method of installation, e.g. tarball, clone, etc.'))
    return

  def configureDirectories(self):
    '''Checks PETSC_DIR and sets if not set'''
    if 'PETSC_DIR' in self.framework.argDB:
      self.dir = self.framework.argDB['PETSC_DIR']
      if self.dir == 'pwd':
        raise RuntimeError('You have set -PETSC_DIR=pwd, you need to use back quotes around the pwd\n  like -PETSC_DIR=`pwd`')
      if not os.path.isdir(self.dir):
        raise RuntimeError('The value you set with -PETSC_DIR='+self.dir+' is not a directory')
    elif 'PETSC_DIR' in os.environ:
      self.dir = os.environ['PETSC_DIR']
      if self.dir == 'pwd':
        raise RuntimeError('''
The environmental variable PETSC_DIR is set incorrectly. Please use the following: [notice backquotes]
  For sh/bash  : PETSC_DIR=`pwd`; export PETSC_DIR
  for csh/tcsh : setenv PETSC_DIR `pwd`''')
      elif not os.path.isdir(self.dir):
        raise RuntimeError('The environmental variable PETSC_DIR '+self.dir+' is not a directory')
    else:
      self.dir = os.getcwd()
    if self.isPetsc and not os.path.realpath(self.dir) == os.path.realpath(os.getcwd()):
      raise RuntimeError('The environmental variable PETSC_DIR '+self.dir+' MUST be the current directory '+os.getcwd())
    if self.dir[1] == ':':
      try:
        dir = self.dir.replace('\\','/')
        (dir, error, status) = self.executeShellCommand('cygpath -au '+dir)
        self.dir = dir.replace('\n','')
      except RuntimeError:
        pass
    versionHeader = os.path.join(self.dir, 'include', 'petscversion.h')
    versionInfo = []
    if os.path.exists(versionHeader):
      f = file(versionHeader)
      for line in f:
        if line.find('define PETSC_VERSION') >= 0:
          versionInfo.append(line[:-1])
      f.close()
    else:
      raise RuntimeError('Invalid PETSc directory '+str(self.dir)+' it may not exist?')
    self.logPrint('Version Information:')
    for line in versionInfo:
      self.logPrint(line)
    self.addMakeMacro('DIR', self.dir)
    self.addDefine('DIR', self.dir)
    self.framework.argDB['search-dirs'].append(os.path.join(self.dir, 'bin', 'win32fe'))

    import sys

    auxDir      = os.path.join(self.dir,'config','configarch')
    configSub   = os.path.join(auxDir, 'config.sub')
    configGuess = os.path.join(auxDir, 'config.guess')
    
    try:
      host   = config.base.Configure.executeShellCommand(self.shell+' '+configGuess, log = self.framework.log)[0]
      output = config.base.Configure.executeShellCommand(self.shell+' '+configSub+' '+host, log = self.framework.log)[0]
    except RuntimeError, e:
      fd = open(configGuess)
      data = fd.read()
      fd.close()
      if data.find('\r\n') >= 0:
        raise RuntimeError('''It appears petsc.tar.gz is uncompressed on Windows (perhaps with Winzip)
          and files copied over to Unix/Linux. Windows introduces LF characters which are
          inappropriate on other systems. Please use gunzip/tar on the install machine.\n''')
      raise RuntimeError('Unable to determine host type using '+configSub+': '+str(e))
    m = re.match(r'^(?P<cpu>[^-]*)-(?P<vendor>[^-]*)-(?P<os>.*)$', output)
    if not m:
      raise RuntimeError('Unable to parse output of '+configSub+': '+output)
    self.framework.host_cpu    = m.group('cpu')
    self.framework.host_vendor = m.group('vendor')
    self.framework.host_os     = m.group('os')

    return

  def configureExternalPackagesDir(self):
    if self.framework.externalPackagesDir is None:
      self.externalPackagesDir = os.path.join(self.dir, 'externalpackages')
    else:
      self.externalPackagesDir = self.framework.externalPackagesDir
    return

  def configureInstallationMethod(self):
    if os.path.exists(os.path.join(self.dir, '.hg')):
      self.logPrint('This is a Mercurial clone')
      self.isClone = 1
    elif os.path.exists(os.path.join(self.dir, 'BitKeeper')):
      self.logPrint('This is a BitKeeper clone')
      self.isClone = 1
    elif os.path.exists(os.path.join(self.dir, 'BK')):
      self.logPrint('This is a fake BitKeeper clone')
      self.isClone = 1
    elif os.path.exists(os.path.join(self.dir, '_darcs')):
      self.logPrint('This is a DARCS clone')
      self.isClone = 1
    elif self.framework.argDB['with-installation-method'] == 'clone':
      self.logPrint('This is a forced clone')
      self.isClone = 1
    else:
      self.logPrint('This is a tarball installation')
      self.isClone = 0
    if self.isClone and not os.path.exists(os.path.join(self.dir, 'bin', 'maint')):
      raise RuntimeError('Your petsc-dev directory is broken, remove the entire directory and start all over again')
    return

  def configure(self):
    self.executeTest(self.configureDirectories)
    self.executeTest(self.configureExternalPackagesDir)
    self.executeTest(self.configureInstallationMethod)
    return
