#!/usr/bin/env python
from __future__ import generators
import user
import config.autoconf
import os

class Configure(config.autoconf.Configure):
  def __init__(self, framework):
    config.autoconf.Configure.__init__(self, framework)
    self.headerPrefix = ''
    self.substPrefix  = ''
    self.foundX11     = 0
    self.compilers    = self.framework.require('config.compilers',    self)
    self.make         = self.framework.require('PETSc.packages.Make', self)
    return

  def __str__(self):
    if self.foundX11:
      desc = ['X11:']	
      desc.append('  Includes: '+str(self.include))
      desc.append('  Library: '+str(self.lib))
      return '\n'.join(desc)+'\n'
    return ''
    
  def setupHelp(self, help):
    import nargs
    help.addArgument('X11', '-with-x=<bool>',                nargs.ArgBool(None, 1,   'Activate X11'))
    help.addArgument('X11', '-with-x-include=<include dir>', nargs.ArgDir(None, None, 'Specify an include directory for X11'))
    help.addArgument('X11', '-with-x-lib=<X11 lib>',         nargs.Arg(None, None,    'Specify X11 library file'))
    return

  def generateGuesses(self):
    '''Generate list of possible locations of X11'''
    # This needs to be implemented
    return

  def checkXMake(self):
    import shutil

    includeDir = ''
    libraryDir = ''
    # Create Imakefile
    dir = os.path.abspath('conftestdir')
    if os.path.exists(dir): shutil.rmtree(dir)
    os.mkdir(dir)
    os.chdir(dir)
    f = file('Imakefile', 'w')
    f.write('''
acfindx:
	@echo \'X_INCLUDE_ROOT = ${INCROOT}\'
	@echo \'X_USR_LIB_DIR = ${USRLIBDIR}\'
	@echo \'X_LIB_DIR = ${LIBDIR}\'
''')
    f.close()
    # Compile makefile
    try:
      (output, error, status) = config.base.Configure.executeShellCommand('xmkmf', log = self.framework.log)
      if not status and os.path.exists('Makefile'):
        (output, error, status) = config.base.Configure.executeShellCommand(self.make.make+' acfindx', log = self.framework.log)
        results                 = self.parseShellOutput(output)
        if not ('X_INCLUDE_ROOT' in results and 'X_USR_LIB_DIR' in results and 'X_LIB_DIR' in results):
          raise RuntimeError('Invalid output: '+str(output))
        # Open Windows xmkmf reportedly sets LIBDIR instead of USRLIBDIR.
        for ext in ['.a', '.so', '.sl']:
          if not os.path.isfile(os.path.join(results['X_USR_LIB_DIR'])) and os.path.isfile(os.path.join(results['X_LIB_DIR'])):
            results['X_USR_LIB_DIR'] = results['X_LIB_DIR']
            break
        # Screen out bogus values from the imake configuration.  They are
        # bogus both because they are the default anyway, and because
        # using them would break gcc on systems where it needs fixed includes.
        if not results['X_INCLUDE_ROOT'] == '/usr/include' and os.path.isfile(os.path.join(results['X_INCLUDE_ROOT'], 'X11', 'Xos.h')):
          includeDir = results['X_INCLUDE_ROOT']
        if not (results['X_USR_LIB_DIR'] == '/lib' or results['X_USR_LIB_DIR'] == '/usr/lib') and os.path.isdir(results['X_USR_LIB_DIR']):
          libraryDir = results['X_USR_LIB_DIR']
    except RuntimeError, e:
      self.framework.log.write('Error using Xmake: '+str(e)+'\n')
    # Cleanup
    os.chdir(os.path.dirname(dir))
    shutil.rmtree(dir)
    return (includeDir, libraryDir)

  def configureLibrary(self):
    '''Checks for X windows, sets PETSC_HAVE_X11 if found, and defines X_CFLAGS, X_PRE_LIBS, X_LIBS, and X_EXTRA_LIBS'''
    # This needs to be rewritten to use generateGuesses()
    foundInclude = 0
    includeDirs  = ['/usr/X11/include',
                   '/usr/X11R6/include',
                   '/usr/X11R5/include',
                   '/usr/X11R4/include',
                   '/usr/include/X11',
                   '/usr/include/X11R6',
                   '/usr/include/X11R5',
                   '/usr/include/X11R4',
                   '/usr/local/X11/include',
                   '/usr/local/X11R6/include',
                   '/usr/local/X11R5/include',
                   '/usr/local/X11R4/include',
                   '/usr/local/include/X11',
                   '/usr/local/include/X11R6',
                   '/usr/local/include/X11R5',
                   '/usr/local/include/X11R4',
                   '/usr/X386/include',
                   '/usr/x386/include',
                   '/usr/XFree86/include/X11',
                   '/usr/include',
                   '/usr/local/include',
                   '/usr/unsupported/include',
                   '/usr/athena/include',
                   '/usr/local/x11r5/include',
                   '/usr/lpp/Xamples/include',
                   '/usr/openwin/include',
                   '/usr/openwin/share/include']
    includeDir   = ''
    foundLibrary = 0
    libraryDirs  = map(lambda s: s.replace('include', 'lib'), includeDirs)
    libraryDir   = ''
    # Guess X location
    (includeDirGuess, libraryDirGuess) = self.checkXMake()
    # Check for X11 includes
    if self.framework.argDB.has_key('with-x-include'):
      if not os.path.isdir(self.framework.argDB['with-x-include']):
        raise RuntimeError('Invalid X include directory specified by --with-x-include='+os.path.abspath(self.framework.argDB['with-x-include']))
      includeDir = self.framework.argDB['with-x-include']
    else:
      testInclude  = 'X11/Intrinsic.h'

      # Check guess
      if includeDirGuess and os.path.isfile(os.path.join(includeDirGuess, testInclude)):
        foundInclude = 1
        includeDir   = includeDirGuess
      # Check default compiler paths
      if not foundInclude and self.checkPreprocess('#include <'+testInclude+'>\n'):
        foundInclude = 1
      # Check standard paths
      if not foundInclude:
        for dir in includeDirs:
          if os.path.isfile(os.path.join(dir, testInclude)):
            foundInclude = 1
            includeDir   = dir
    # Check for X11 libraries
    if self.framework.argDB.has_key('with-x-library'):
      if not os.path.isfile(self.framework.argDB['with-x-library']):
        raise RuntimeError('Invalid X library specified by --with-x-library='+os.path.abspath(self.framework.argDB['with-x-library']))
      libraryDir = os.path.dirname(self.framework.argDB['with-x-library'])
    else:
      testLibrary  = 'Xt'
      testFunction = 'XtMalloc'

      # Check guess
      if libraryDirGuess:
        for ext in ['.a', '.so', '.sl', '.dll.a']:
          if os.path.isfile(os.path.join(libraryDirGuess, 'lib'+testLibrary+ext)):
            foundLibrary = 1
            libraryDir   = libraryDirGuess
            break
      # Check default compiler libraries
      if not foundLibrary:
        oldLibs = self.framework.argDB['LIBS']
        self.framework.argDB['LIBS'] += ' -l'+testLibrary
        self.pushLanguage(self.language[-1])
        if self.checkLink('', testFunction+'();\n'):
          foundLibrary = 1
        self.framework.argDB['LIBS'] = oldLibs
        self.popLanguage()
      # Check standard paths
      if not foundLibrary:
        for dir in libraryDirs:
          for ext in ['.a', '.so', '.sl']:
            if os.path.isfile(os.path.join(dir, 'lib'+testLibrary+ext)):
              foundLibrary = 1
              libraryDir   = dir
      # Verify that library can be linked with
      if foundLibrary:
        oldLibs = self.framework.argDB['LIBS']
        if libraryDir:
          self.framework.argDB['LIBS'] += ' -L'+libraryDir
        self.framework.argDB['LIBS'] += ' -l'+testLibrary
        self.pushLanguage(self.language[-1])
        if not self.checkLink('', testFunction+'();\n'):
          foundLibrary = 0
        self.framework.argDB['LIBS'] = oldLibs
        self.popLanguage()
          
    if not foundInclude or not foundLibrary:
      self.emptySubstitutions()
    else:
      self.foundX11  = 1
      if includeDir:
        self.include = '-I'+includeDir
      else:
        self.include = ''
      if libraryDir:
        self.lib     = '-L'+libraryDir+' -lX11'
      else:
        self.lib     = '-lX11'

      self.addDefine('HAVE_X11', 1)
      self.addSubstitution('X_CFLAGS',     self.include)
      self.addSubstitution('X_LIBS',       self.lib)
      self.addSubstitution('X_PRE_LIBS',   '')
      self.addSubstitution('X_EXTRA_LIBS', '')
      self.addSubstitution('X11_INCLUDE',  self.include)
      self.addSubstitution('X11_LIB',      self.lib)
    return

  def emptySubstitutions(self):
    self.framework.log.write('Configuring PETSc to not use X\n')
    self.addDefine('HAVE_X11', 0)
    self.addSubstitution('X_CFLAGS',     '')
    self.addSubstitution('X_PRE_LIBS',   '')
    self.addSubstitution('X_LIBS',       '')
    self.addSubstitution('X_EXTRA_LIBS', '')
    self.addSubstitution('X11_INCLUDE',  '')
    self.addSubstitution('X11_LIB',      '')
    return

  def configure(self):
    if not self.framework.argDB['with-x'] or self.framework.argDB['with-64-bit-ints']:
      self.executeTest(self.emptySubstitutions)
      return
    self.executeTest(self.configureLibrary)
    return

if __name__ == '__main__':
  import config.framework
  import sys
  framework = config.framework.Framework(sys.argv[1:])
  framework.setupLogging()
  framework.children.append(Configure(framework))
  framework.configure()
  framework.dumpSubstitutions()
