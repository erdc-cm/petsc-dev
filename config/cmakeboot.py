#!/usr/bin/env python

import os,sys,subprocess,string
from collections import deque
sys.path.insert(0, os.path.join(os.environ['PETSC_DIR'], 'config'))
sys.path.insert(0, os.path.join(os.environ['PETSC_DIR'], 'config', 'BuildSystem'))
import script

def printFunction(s):
  'The print statement cannot be passed as a default argument'
  print(s)

class PETScMaker(script.Script):
 def __init__(self, petscdir, petscarch, argDB = None, framework = None):
   import RDict

   if not argDB:
     argDB = RDict.RDict(None, None, 0, 0, readonly = True)
     argDB.saveFilename = os.path.join(petscdir,petscarch,'conf','RDict.db')
     argDB.load()
   script.Script.__init__(self, argDB = argDB)
   self.framework = framework
   self.log = sys.stdout

 def __str__(self):
   return ''

 def setupModules(self):
   self.mpi           = self.framework.require('config.packages.MPI',         None)
   self.base          = self.framework.require('config.base',                 None)
   self.setCompilers  = self.framework.require('config.setCompilers',         None)   
   self.arch          = self.framework.require('PETSc.utilities.arch',        None)
   self.petscdir      = self.framework.require('PETSc.utilities.petscdir',    None)
   self.languages     = self.framework.require('PETSc.utilities.languages',   None)
   self.debugging     = self.framework.require('PETSc.utilities.debugging',   None)
   self.make          = self.framework.require('PETSc.utilities.Make',        None)
   self.cmake         = self.framework.require('PETSc.utilities.CMake',       None)
   self.CHUD          = self.framework.require('PETSc.utilities.CHUD',        None)
   self.compilers     = self.framework.require('config.compilers',            None)
   self.types         = self.framework.require('config.types',                None)
   self.headers       = self.framework.require('config.headers',              None)
   self.functions     = self.framework.require('config.functions',            None)
   self.libraries     = self.framework.require('config.libraries',            None)
   self.scalarType    = self.framework.require('PETSc.utilities.scalarTypes', None)
   self.memAlign      = self.framework.require('PETSc.utilities.memAlign',    None)
   self.libraryOptions= self.framework.require('PETSc.utilities.libraryOptions', None)      
   self.compilerFlags = self.framework.require('config.compilerFlags', self)
   return

 def setup(self):
   script.Script.setup(self)
   if not self.framework:
     self.framework = self.loadConfigure()
   self.setupModules()

 def cmakeboot(self, args, logPrint):
   self.setup()
   options = deque()
   langlist = [('C','C')]
   if hasattr(self.compilers,'FC'):
     langlist.append(('FC','Fortran'))
   if (self.languages.clanguage == 'Cxx'):
     langlist.append(('Cxx','CXX'))
   for petsclanguage,cmakelanguage in langlist:
     self.setCompilers.pushLanguage(petsclanguage)
     options.append('-DCMAKE_'+cmakelanguage+'_COMPILER=' + self.compilers.getCompiler())
     flags = [self.setCompilers.getCompilerFlags(),
              self.setCompilers.CPPFLAGS,
              self.CHUD.CPPFLAGS]
     options.append('-DCMAKE_'+cmakelanguage+'_FLAGS=' + ''.join(flags))
     options.append('-DCMAKE_'+cmakelanguage+'_COMPILER=' + self.setCompilers.getCompiler())
     self.setCompilers.popLanguage()
   cmd = [self.cmake.cmake, self.petscdir.dir] + map(lambda x:x.strip(), options) + args
   archdir = os.path.join(self.petscdir.dir, self.arch.arch)
   logPrint('Invoking: %s' % cmd)
   retcode = subprocess.call(cmd, cwd=archdir)
   if retcode < 0:
     raise OSError('CMake process was terminated by signal %d' % (-retcode,))
   if retcode > 0:
     raise OSError('CMake process failed with status %d' % (retcode,))
   logPrint('CMake configuration completed successfully.')
   
   def quoteIfNeeded(path):
     "Don't need quotes unless the path has bits that would confuse the shell"
     safe = string.letters + string.digits + os.path.sep + os.path.pardir + '-_'
     if set(path).issubset(safe):
       return path
     else:
       return '"' + path + '"'
   logPrint('Build the library with: make -C %s' % quoteIfNeeded(archdir))

def main(petscdir, petscarch, argDB=None, framework=None, logPrint=printFunction, args=[]):
  # This can be called as a stand-alone program, or by importing it from
  # python.  The latter functionality is needed because argDB does not
  # get written until the very end of configure, but we want to run this
  # automatically during configure (if CMake is available).
  #
  # Strangely, we can't store logPrint in the PETScMaker because
  # (somewhere) it creeps into framework (which I don't want to modify)
  # and makes the result unpickleable.  This is not a problem when run
  # as a standalone program (because the database is read-only), but is
  # not okay when called from configure.
  PETScMaker(petscdir,petscarch,argDB,framework).cmakeboot(args,logPrint)

if __name__ == "__main__":
  main(petscdir=os.environ['PETSC_DIR'], petscarch=os.environ['PETSC_ARCH'], args=sys.argv[1:])
