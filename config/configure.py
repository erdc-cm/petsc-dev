#!/usr/bin/env python
import os
import sys
import commands
# to load ~/.pythonrc.py before inserting correct BuildSystem to path
import user


if not hasattr(sys, 'version_info') or not sys.version_info[1] >= 2 or not sys.version_info[0] >= 2:
  print '**** You must have Python version 2.2 or higher to run config/configure.py ******'
  print '*           Python is easy to install for end users or sys-admin.               *'
  print '*                   http://www.python.org/download/                             *'
  print '*                                                                               *'
  print '*            You CANNOT configure PETSc without Python                          *'
  print '*    http://www.mcs.anl.gov/petsc/petsc-as/documentation/installation.html      *'
  print '*********************************************************************************'
  sys.exit(4)
  
def check_petsc_arch(opts):
  # If PETSC_ARCH not specified - use script name (if not configure.py)
  found = 0
  for name in opts:
    if name.find('PETSC_ARCH=') >= 0:
      found = 1
      break
  # If not yet specified - use the filename of script
  if not found:
      filename = os.path.basename(sys.argv[0])
      if not filename.startswith('configure') and not filename.startswith('reconfigure'):
        useName = 'PETSC_ARCH='+os.path.splitext(os.path.basename(sys.argv[0]))[0]
        opts.append(useName)
  return

def chkbrokencygwin():
  if os.path.exists('/usr/bin/cygcheck.exe'):
    buf = os.popen('/usr/bin/cygcheck.exe -c cygwin').read()
    if buf.find('1.5.11-1') > -1:
      return 1
    else:
      return 0
  return 0

def chkusingwindowspython():
  if os.path.exists('/usr/bin/cygcheck.exe'):
    if sys.platform != 'cygwin':
      return 1
  return 0

def chkcygwinpythonver():
  if os.path.exists('/usr/bin/cygcheck.exe'):
    buf = os.popen('/usr/bin/cygcheck.exe -c python').read()
    if (buf.find('2.4') > -1) or (buf.find('2.5') > -1) or (buf.find('2.6') > -1):
      return 1
    else:
      return 0
  return 0

def rhl9():
  try:
    file = open('/etc/redhat-release','r')
  except:
    return 0
  try:
    buf = file.read()
    file.close()
  except:
    # can't read file - assume dangerous RHL9
    return 1
  if buf.find('Shrike') > -1:
    return 1
  else:
    return 0

def petsc_configure(configure_options): 
  print '================================================================================='
  print '             Configuring PETSc to compile on your system                         '
  print '================================================================================='  

  # Command line arguments take precedence (but don't destroy argv[0])
  sys.argv = sys.argv[:1] + configure_options + sys.argv[1:]
  # check PETSC_ARCH
  check_petsc_arch(sys.argv)
  extraLogs = []

  # support a few standard configure option types
  foundsudo = 0
  for l in range(0,len(sys.argv)):
    if sys.argv[l] == '--with-sudo=sudo':
      foundsudo = 1
  
  for l in range(0,len(sys.argv)):
    name = sys.argv[l]
    if name.find('enable-') >= 0:
      if name.find('=') == -1:
        sys.argv[l] = name.replace('enable-','with-')+'=1'
      else:
        head, tail = name.split('=', 1)
        sys.argv[l] = head.replace('enable-','with-')+'='+tail
    if name.find('disable-') >= 0:
      if name.find('=') == -1:
        sys.argv[l] = name.replace('disable-','with-')+'=0'
      else:
        head, tail = name.split('=', 1)
        if tail == '1': tail = '0'
        sys.argv[l] = head.replace('disable-','with-')+'='+tail
    if name.find('without-') >= 0:
      if name.find('=') == -1:
        sys.argv[l] = name.replace('without-','with-')+'=0'
      else:
        head, tail = name.split('=', 1)
        if tail == '1': tail = '0'
        sys.argv[l] = head.replace('without-','with-')+'='+tail
    if name.find('prefix=') >= 0 and not foundsudo:
      head, installdir = name.split('=', 1)      
      if os.path.exists(installdir):
        if not os.access(installdir,os.W_OK):
          print 'You do not have write access to requested install directory given with --prefix='+installdir+' perhaps use --with-sudo=sudo also'
          sys.exit(3)
        else:
          try:
            os.mkdir(installdir)
          except:
            print 'You do not have write access to create install directory given with --prefix='+installdir+' perhaps use --with-sudo=sudo also'
            sys.exit(3)
        

  # Check for sudo
  if os.getuid() == 0:
    print '================================================================================='
    print '             *** Do not run configure as root, or using sudo. ***'
    print '             *** Use the --with-sudo=sudo option to have      ***'
    print '             *** installs of external packages done with sudo ***'
    print '             *** use only with --prefix= when installing in   ***'
    print '             *** system directories                           ***'
    print '================================================================================='
    sys.exit(3)

  # Check for broken cygwin
  if chkbrokencygwin():
    print '================================================================================='
    print ' *** cygwin-1.5.11-1 detected. config/configure.py fails with this version   ***'
    print ' *** Please upgrade to cygwin-1.5.12-1 or newer version. This can  ***'
    print ' *** be done by running cygwin-setup, selecting "next" all the way.***'
    print '================================================================================='
    sys.exit(3)

  # Disable threads on RHL9
  if rhl9():
    sys.argv.append('--useThreads=0')
    extraLogs.append('''\
================================================================================
   *** RHL9 detected. Threads do not work correctly with this distribution ***
    ****** Disabling thread usage for this run of config/configure.py *******
================================================================================''')
  # Make sure cygwin-python is used on windows
  if chkusingwindowspython():
    print '================================================================================='
    print ' *** Non-cygwin python detected. Please rerun config/configure.py with cygwin-python ***'
    print '================================================================================='
    sys.exit(3)
    
  # Threads don't work for cygwin & python-2.4, 2.5 etc..
  if chkcygwinpythonver():
    sys.argv.append('--useThreads=0')
    extraLogs.append('''\
================================================================================
** Cygwin-python-2.4/2.5 detected. Threads do not work correctly with this version *
 ********* Disabling thread usage for this run of config/configure.py **********
================================================================================''')
          
  # Should be run from the toplevel
  configDir = os.path.abspath(os.path.join('config'))
  bsDir     = os.path.join(configDir, 'BuildSystem')
  if not os.path.isdir(configDir):
    raise RuntimeError('Run configure from $PETSC_DIR, not '+os.path.abspath('.'))
  if not os.path.isdir(bsDir):
    print '================================================================================='
    print '''++ Could not locate BuildSystem in %s/python.''' % os.getcwd()
    print '''++ Downloading it using "hg clone http://hg.mcs.anl.gov/petsc/BuildSystem %s/python/BuildSystem"''' % os.getcwd()
    print '================================================================================='
    (status,output) = commands.getstatusoutput('hg clone http://hg.mcs.anl.gov/petsc/BuildSystem python/BuildSystem')
    if status:
      if output.find('ommand not found') >= 0:
        print '================================================================================='
        print '''** Unable to locate hg (Mercurial) to download BuildSystem; make sure hg is in your path'''
        print '''** or manually copy BuildSystem to $PETSC_DIR/python/BuildSystem from a machine where'''
        print '''** you do have hg installed and can clone BuildSystem. '''
        print '================================================================================='
      elif output.find('Cannot resolve host') >= 0:
        print '================================================================================='
        print '''** Unable to download BuildSystem. You must be off the network.'''
        print '''** Connect to the internet and run config/configure.py again.'''
        print '================================================================================='
      else:
        print '================================================================================='
        print '''** Unable to download BuildSystem. Please send this message to petsc-maint@mcs.anl.gov'''
        print '================================================================================='
      print output
      sys.exit(3)
      
  sys.path.insert(0, bsDir)
  sys.path.insert(0, configDir)
  import config.base
  import config.framework
  import cPickle

  # Disable shared libraries by default
  import nargs
  if nargs.Arg.findArgument('with-shared', sys.argv[1:]) is None:
    sys.argv.append('--with-shared=0')

  framework = None
  try:
    framework = config.framework.Framework(sys.argv[1:]+['--configModules=PETSc.Configure','--optionsModule=PETSc.compilerOptions'], loadArgDB = 0)
    framework.setup()
    framework.logPrint('\n'.join(extraLogs))
    framework.configure(out = sys.stdout)
    framework.storeSubstitutions(framework.argDB)
    framework.argDB['configureCache'] = cPickle.dumps(framework)
    import PETSc.packages
    for i in framework.packages:
      if hasattr(i,'postProcess'):
        i.postProcess()
    framework.logClear()
    return 0
  except (RuntimeError, config.base.ConfigureSetupError), e:
    emsg = str(e)
    if not emsg.endswith('\n'): emsg = emsg+'\n'
    msg ='*********************************************************************************\n'\
    +'         UNABLE to CONFIGURE with GIVEN OPTIONS    (see configure.log for details):\n' \
    +'---------------------------------------------------------------------------------------\n'  \
    +emsg+'*********************************************************************************\n'
    se = ''
  except (TypeError, ValueError), e:
    emsg = str(e)
    if not emsg.endswith('\n'): emsg = emsg+'\n'
    msg ='*********************************************************************************\n'\
    +'                ERROR in COMMAND LINE ARGUMENT to config/configure.py \n' \
    +'---------------------------------------------------------------------------------------\n'  \
    +emsg+'*********************************************************************************\n'
    se = ''
  except ImportError, e :
    emsg = str(e)
    if not emsg.endswith('\n'): emsg = emsg+'\n'
    msg ='*********************************************************************************\n'\
    +'                     UNABLE to FIND MODULE for config/configure.py \n' \
    +'---------------------------------------------------------------------------------------\n'  \
    +emsg+'*********************************************************************************\n'
    se = ''
  except OSError, e :
    emsg = str(e)
    if not emsg.endswith('\n'): emsg = emsg+'\n'
    msg ='*********************************************************************************\n'\
    +'                    UNABLE to EXECUTE BINARIES for config/configure.py \n' \
    +'---------------------------------------------------------------------------------------\n'  \
    +emsg+'*********************************************************************************\n'
    se = ''
  except SystemExit, e:
    if e.code is None or e.code == 0:
      return
    msg ='*********************************************************************************\n'\
    +'           CONFIGURATION CRASH  (Please send configure.log to petsc-maint@mcs.anl.gov)\n' \
    +'*********************************************************************************\n'
    se  = str(e)
  except Exception, e:
    msg ='*********************************************************************************\n'\
    +'          CONFIGURATION CRASH  (Please send configure.log to petsc-maint@mcs.anl.gov)\n' \
    +'*********************************************************************************\n'
    se  = str(e)

  print msg
  if not framework is None:
    framework.logClear()
    if hasattr(framework, 'log'):
      import traceback
      framework.log.write(msg+se)
      traceback.print_tb(sys.exc_info()[2], file = framework.log)
      if os.path.isfile(framework.logName+'.bkp'):
        if framework.debugIndent is None:
          framework.debugIndent = '  '
        framework.logPrintDivider()
        framework.logPrintBox('Previous configure logs below', debugSection = None)
        f = file(framework.logName+'.bkp')
        framework.log.write(f.read())
        f.close()
      sys.exit(1)
  else:
    print se
    import traceback
    traceback.print_tb(sys.exc_info()[2])

if __name__ == '__main__':
  petsc_configure([])

