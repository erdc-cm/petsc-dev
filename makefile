# $Id: makefile,v 1.353 2001/08/28 19:43:38 balay Exp $ 
#
# This is the makefile for installing PETSc. See the file
# docs/installation.html for directions on installing PETSc.
# See also bmake/common for additional commands.
#
ALL: all
LOCDIR = . 
DIRS   = src include docs 
#
# Configuration Variables
#
# Read configure options from a file if CONFIGURE_OPTIONSis not defined
CONFIGURE_OPTIONS_FILE = ./config/configure_options
CONFIGURE_OPTIONS      = $(shell cat $(CONFIGURE_OPTIONS_FILE))
CONFIGURE_LOG_FILE     = configure_petsc.log
AUTOMAKE_ADD_FILES     = config/config.guess config/config.sub config/install-sh config/missing config/mkinstalldirs \
                         config/ltconfig config/ltmain.sh

include ${PETSC_DIR}/bmake/common/base
include ${PETSC_DIR}/bmake/common/test

#
# Configuration Targets
#
aclocal.m4: configure.in
	@echo "Making $@" >> $(CONFIGURE_LOG_FILE)
	@echo "----------------------------------------" >> $(CONFIGURE_LOG_FILE)
	@aclocal >> $(CONFIGURE_LOG_FILE)

bmake/config/petscconf.h.in: config/acconfig.h config/acsite.m4 configure.in
	@echo "Making $@" >> $(CONFIGURE_LOG_FILE)
	@echo "----------------------------------------" >> $(CONFIGURE_LOG_FILE)
	@autoheader -l config >> $(CONFIGURE_LOG_FILE)

$(AUTOMAKE_ADD_FILES):
	@echo "Making $@" >> $(CONFIGURE_LOG_FILE)
	@echo "----------------------------------------" >> $(CONFIGURE_LOG_FILE)
	@automake --foreign --add-missing Makefile >> $(CONFIGURE_LOG_FILE)

Makefile.am: $(AUTOMAKE_ADD_FILES)

Makefile.in: Makefile.am
	@echo "Making $@" >> $(CONFIGURE_LOG_FILE)
	@echo "----------------------------------------" >> $(CONFIGURE_LOG_FILE)
	@automake --foreign Makefile >> $(CONFIGURE_LOG_FILE)

configure: configure.in config/acsite.m4 aclocal.m4 bmake/config/petscconf.h.in $(AUTOMAKE_ADD_FILES)
	@echo "Making $@" >> $(CONFIGURE_LOG_FILE)
	@echo "----------------------------------------" >> $(CONFIGURE_LOG_FILE)
	@autoconf -l config >> $(CONFIGURE_LOG_FILE)

start_configure:
	-@$(RM) $(CONFIGURE_LOG_FILE)

configure_petsc: start_configure configure Makefile.in
	@echo "Configuring Petsc with options:" >> $(CONFIGURE_LOG_FILE)
	@echo "$(CONFIGURE_OPTIONS)" >> $(CONFIGURE_LOG_FILE)
	@echo "----------------------------------------" >> $(CONFIGURE_LOG_FILE)
	@echo "$(CONFIGURE_OPTIONS)" > $(CONFIGURE_OPTIONS_FILE)
	@./configure $(CONFIGURE_OPTIONS) >> $(CONFIGURE_LOG_FILE)

$(CONFIGURE_LOG_FILE): $(CONFIGURE_OPTIONS_FILE)
	if test -f "${PETSC_DIR}/configure.in"; then \
          $(MAKE) configure_petsc; \
        else \
          touch $(CONFIGURE_LOG_FILE); \
        fi

configure_clean:
	-@$(RM) aclocal.m4
	-@$(RM) bmake/config/petscconf.h.in
	-@$(RM) $(AUTOMAKE_ADD_FILES) Makefile.in
	-@$(RM) configure
#
# Basic targets to build PETSc libraries.
# all     : builds the c, fortran, and f90 libraries
all       : $(CONFIGURE_LOG_FILE) chk_petsc_dir info info_h chklib_dir deletelibs build shared
all_lt    : $(CONFIGURE_LOG_FILE) chk_petsc_dir info info_h chklib_dir deletelibs build_lt shared
#
# Prints information about the system and version of PETSc being compiled
#
info:
	-@echo "=========================================="
	-@echo " "
	-@echo "See docs/troubleshooting.html and docs/bugreporting.html"
	-@echo "for help with installation problems. Please send EVERYTHING"
	-@echo "printed out below when reporting problems"
	-@echo " "
	-@echo "To subscribe to the PETSc users mailing list, send mail to "
	-@echo "majordomo@mcs.anl.gov with the message: "
	-@echo "subscribe petsc-users"
	-@echo " "
	-@echo "=========================================="
	-@echo On `date` on `hostname`
	-@echo Machine characteristics: `uname -a`
	-@echo "-----------------------------------------"
	-@echo "Using C compiler: ${CC} ${COPTFLAGS} ${CCPPFLAGS}"
	-@if [ -n "${C_CCV}" -a "${C_CCV}" != "unknown" ] ; then \
	  echo "C Compiler version:" ; ${C_CCV} ; fi ; true 
	-@if [ -n "${CXX_CCV}" -a "${CXX_CCV}" != "unknown" ] ; then \
	  echo "C++ Compiler version:" ; ${CXX_CCV} ; fi; true
	-@echo "Using Fortran compiler: ${FC} ${FOPTFLAGS} ${FCPPFLAGS}"
	-@if [ -n "${C_FCV}" -a "${C_FCV}" != "unknown" ] ; then \
	  echo "Fortran Compiler version:" ; ${C_FCV} ; fi; true
	-@echo "-----------------------------------------"
	-@grep PETSC_VERSION_NUMBER ${PETSC_DIR}/include/petscversion.h | ${SED} "s/........//"
	-@echo "-----------------------------------------"
	-@echo "Using PETSc flags: ${PETSCFLAGS} ${PCONF}"
	-@echo "-----------------------------------------"
	-@echo "Using configuration flags:"
	-@grep "define " ${PETSC_DIR}/bmake/${PETSC_ARCH}/petscconf.h
	-@echo "-----------------------------------------"
	-@echo "Using include paths: ${PETSC_INCLUDE}"
	-@echo "-----------------------------------------"
	-@echo "Using PETSc directory: ${PETSC_DIR}"
	-@echo "Using PETSc arch: ${PETSC_ARCH}"
	-@echo "------------------------------------------"
	-@echo "Using C linker: ${CLINKER}"
	-@echo "Using Fortran linker: ${FLINKER}"
	-@echo "Using libraries: ${PETSC_LIB}"
	-@echo "=========================================="
#
#
MINFO = ${PETSC_DIR}/bmake/${PETSC_ARCH}/petscmachineinfo.h
info_h:
	-@$(RM) -f MINFO ${MINFO}
	-@echo  "static char *petscmachineinfo = \"  " >> MINFO
	-@echo  "Libraries compiled on `date` on `hostname` " >> MINFO
	-@echo  Machine characteristics: `uname -a` "" >> MINFO
	-@echo  "Using PETSc directory: ${PETSC_DIR}" >> MINFO
	-@echo  "Using PETSc arch: ${PETSC_ARCH}" >> MINFO
	-@echo  "-----------------------------------------\"; " >> MINFO
	-@echo  "static char *petsccompilerinfo = \"  " >> MINFO
	-@echo  "Using C compiler: ${CC} ${COPTFLAGS} ${CCPPFLAGS} " >> MINFO
	-@if [  "${C_CCV}" -a "${C_CCV}" != "unknown" ] ; then \
	  echo  "C Compiler version:"  >> MINFO ; ${C_CCV} >> MINFO 2>&1; fi ; true
	-@if [  "${CXX_CCV}" -a "${CXX_CCV}" != "unknown" ] ; then \
	  echo  "C++ Compiler version:"  >> MINFO; ${CXX_CCV} >> MINFO 2>&1 ; fi ; true
	-@echo  "Using Fortran compiler: ${FC} ${FOPTFLAGS} ${FCPPFLAGS}" >> MINFO
	-@if [  "${C_FCV}" -a "${C_FCV}" != "unknown" ] ; then \
	  echo  "Fortran Compiler version:" >> MINFO ; ${C_FCV} >> MINFO 2>&1 ; fi ; true
	-@echo  "-----------------------------------------\"; " >> MINFO
	-@echo  "static char *petsccompilerflagsinfo = \"  " >> MINFO
	-@echo  "Using PETSc flags: ${PETSCFLAGS} ${PCONF}" >> MINFO
	-@echo  "-----------------------------------------" >> MINFO
	-@echo  "Using configuration flags:" >> MINFO
	-@echo  "-----------------------------------------" >> MINFO
	-@echo  "Using include paths: ${PETSC_INCLUDE}" >> MINFO
	-@echo  "------------------------------------------\"; " >> MINFO
	-@echo  "static char *petsclinkerinfo = \"  " >> MINFO
	-@echo  "Using C linker: ${CLINKER}" >> MINFO
	-@echo  "Using Fortran linker: ${FLINKER}" >> MINFO
	-@echo  "Using libraries: ${PETSC_LIB} \"; " >> MINFO
	-@cat MINFO | ${SED} -e 's/$$/ \\n\\/' | sed -e 's/\;  \\n\\/\;/'> ${MINFO}
	-@chmod g+w ${MINFO}
	-@$(RM) MINFO
#
# Builds the PETSc libraries
# This target also builds fortran77 and f90 interface
# files and compiles .F files
#
build:
	-@echo "BEGINNING TO COMPILE LIBRARIES IN ALL DIRECTORIES"
	-@echo "========================================="
	-@${OMAKE} BOPT=${BOPT} PETSC_ARCH=${PETSC_ARCH} PETSC_DIR=${PETSC_DIR} ACTION=libfast tree 
	${RANLIB} ${PETSC_LIB_DIR}/*.a
	-@chmod g+w  ${PETSC_LIB_DIR}/*.a
	-@echo "Completed building libraries"
	-@echo "========================================="
#
# Builds the PETSc libraries
# This target is the same as 'build', but uses the 'lib' target instead
#
build_lt:
	-@echo "BEGINNING TO COMPILE LIBTOOL LIBRARIES IN ALL DIRECTORIES"
	-@echo "========================================="
	-@${OMAKE} BOPT=${BOPT} PETSC_ARCH=${PETSC_ARCH} PETSC_DIR=${PETSC_DIR} ACTION=lib tree 
	-@echo "Completed building libraries"
	-@echo "========================================="
#
# Builds PETSc test examples for a given BOPT and architecture
#
testexamples: info chkopts
	-@echo "BEGINNING TO COMPILE AND RUN TEST EXAMPLES"
	-@echo "Due to different numerical round-off on certain"
	-@echo "machines some of the numbers may not match exactly."
	-@echo "========================================="
	-@${OMAKE} BOPT=${BOPT} PETSC_ARCH=${PETSC_ARCH}  PETSC_DIR=${PETSC_DIR} ACTION=testexamples_1  tree 
	-@echo "Completed compiling and running test examples"
	-@echo "========================================="
testfortran: info chkopts
	-@echo "BEGINNING TO COMPILE AND RUN FORTRAN TEST EXAMPLES"
	-@echo "========================================="
	-@echo "Due to different numerical round-off on certain"
	-@echo "machines or the way Fortran formats numbers"
	-@echo "some of the results may not match exactly."
	-@echo "========================================="
	-@${OMAKE} BOPT=${BOPT} PETSC_ARCH=${PETSC_ARCH} PETSC_DIR=${PETSC_DIR} ACTION=testexamples_3  tree 
	-@echo "Completed compiling and running Fortran test examples"
	-@echo "========================================="
testexamples_uni: info chkopts
	-@echo "BEGINNING TO COMPILE AND RUN TEST UNI-PROCESSOR EXAMPLES"
	-@echo "Due to different numerical round-off on certain"
	-@echo "machines some of the numbers may not match exactly."
	-@echo "========================================="
	-@${OMAKE} BOPT=${BOPT} PETSC_ARCH=${PETSC_ARCH} PETSC_DIR=${PETSC_DIR} ACTION=testexamples_4  tree 
	-@echo "Completed compiling and running uniprocessor test examples"
	-@echo "========================================="
testfortran_uni: info chkopts
	-@echo "BEGINNING TO COMPILE AND RUN TEST UNI-PROCESSOR FORTRAN EXAMPLES"
	-@echo "Due to different numerical round-off on certain"
	-@echo "machines some of the numbers may not match exactly."
	-@echo "========================================="
	-@${OMAKE} BOPT=${BOPT} PETSC_ARCH=${PETSC_ARCH} PETSC_DIR=${PETSC_DIR} ACTION=testexamples_9  tree 
	-@echo "Completed compiling and running uniprocessor fortran test examples"
	-@echo "========================================="

# Ranlib on the libraries
ranlib:
	${RANLIB} ${PETSC_LIB_DIR}/*.a

# Deletes PETSc libraries
deletelibs: chkopts_basic
	-${RM} -f ${PETSC_LIB_DIR}/*

# Cleans up build
allclean: deletelibs
	-@${OMAKE} BOPT=${BOPT} PETSC_ARCH=${PETSC_ARCH} PETSC_DIR=${PETSC_DIR} ACTION=clean tree

#
#   Updates your PETSc version to the latest set of patches
#
update:
	-@bin/petscupdate

#
# Check if PETSC_DIR variable specified is valid
#
chk_petsc_dir:
	@if [ ! -f ${PETSC_DIR}/include/petscversion.h ]; then \
	  echo "Incorrect PETSC_DIR specified: ${PETSC_DIR}!"; \
	  echo "You need to use / to separate directories, not \\!"; \
	  echo "Aborting build"; \
	  false; fi

# ------------------------------------------------------------------
#
# All remaining actions are intended for PETSc developers only.
# PETSc users should not generally need to use these commands.
#

# To access the tags in EMACS, type M-x visit-tags-table and specify
# the file petsc/TAGS.	
# 1) To move to where a PETSc function is defined, enter M-. and the
#     function name.
# 2) To search for a string and move to the first occurrence,
#     use M-x tags-search and the string.
#     To locate later occurrences, use M-,
# Builds all etags files
alletags:
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSDIR=${PETSC_DIR} etags
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSDIR=${PETSC_DIR} etags_complete
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSDIR=${PETSC_DIR} etags_noexamples
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSDIR=${PETSC_DIR} etags_examples
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSDIR=${PETSC_DIR} etags_makefiles
# Builds the basic etags file.	This should be employed by most users.
etags:
	-${RM} ${TAGSDIR}/TAGS
	-touch ${TAGSDIR}/TAGS
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS ACTION=etags_sourcec alltree
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS ACTION=etags_sourcej alltree
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS ACTION=etags_sourceh alltree
	-cd src/fortran; ${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS ACTION=etags_sourcef alltree
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS ACTION=etags_examplesc alltree
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS ACTION=etags_examplesf alltree
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS ACTION=etags_examplesch alltree
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS ACTION=etags_examplesfh alltree
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS ACTION=etags_makefile alltree
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS etags_bmakefiles
	-chmod g+w TAGS
# Builds complete etags list; only for PETSc developers.
etags_complete:
	-${RM} ${TAGSDIR}/TAGS_COMPLETE
	-touch ${TAGSDIR}/TAGS_COMPLETE
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS_COMPLETE ACTION=etags_sourcec alltree
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS_COMPLETE ACTION=etags_sourcej alltree
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS_COMPLETE ACTION=etags_sourceh alltree
	-cd src/fortran; ${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS_COMPLETE ACTION=etags_sourcef alltree
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS_COMPLETE ACTION=etags_examplesc alltree
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS_COMPLETE ACTION=etags_examplesf alltree
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS_COMPLETE ACTION=etags_examplesch alltree
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS_COMPLETE ACTION=etags_examplesfh alltree
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS_COMPLETE ACTION=etags_makefile alltree
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS_COMPLETE etags_bmakefiles
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS_COMPLETE ACTION=etags_docs alltree
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS_COMPLETE ACTION=etags_scripts alltree
	-chmod g+w TAGS_COMPLETE
# Builds the etags file that excludes the examples directories
etags_noexamples:
	-${RM} ${TAGSDIR}/TAGS_NO_EXAMPLES
	-touch ${TAGSDIR}/TAGS_NO_EXAMPLES
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS_NO_EXAMPLES ACTION=etags_sourcec alltree
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS_NO_EXAMPLES ACTION=etags_sourcej alltree
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS_NO_EXAMPLES ACTION=etags_sourceh alltree
	-cd src/fortran; ${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS_NO_EXAMPLES ACTION=etags_sourcef alltree
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS_NO_EXAMPLES ACTION=etags_makefile alltree
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS_NO_EXAMPLES etags_bmakefiles
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS_NO_EXAMPLES ACTION=etags_docs alltree
	-chmod g+w TAGS_NO_EXAMPLES
# Builds the etags file for makefiles
etags_makefiles: 
	-${RM} ${TAGSDIR}/TAGS_MAKEFILES
	-touch ${TAGSDIR}/TAGS_MAKEFILES
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS_MAKEFILES ACTION=etags_makefile alltree
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS_MAKEFILES etags_bmakefiles
	-chmod g+w TAGS_MAKEFILES
# Builds the etags file for examples
etags_examples: 
	-${RM} ${TAGSDIR}/TAGS_EXAMPLES
	-touch ${TAGSDIR}/TAGS_EXAMPLES
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS_EXAMPLES ACTION=etags_examplesc alltree
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS_EXAMPLES ACTION=etags_examplesch alltree
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS_EXAMPLES ACTION=etags_examplesf alltree
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS_EXAMPLES ACTION=etags_examplesfh alltree
	-chmod g+w TAGS_EXAMPLES
etags_fexamples: 
	-${RM} ${TAGSDIR}/TAGS_FEXAMPLES
	-touch ${TAGSDIR}/TAGS_FEXAMPLES
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS_FEXAMPLES ACTION=etags_examplesf alltree
	-${OMAKE} PETSC_DIR=${PETSC_DIR} TAGSFILE=${TAGSDIR}/TAGS_EXAMPLES ACTION=etags_examplesfh alltree
	-chmod g+w TAGS_FEXAMPLES
#
# These are here for the target allci and allco, and etags
#

BMAKEFILES = bmake/common/base bmake/common/rules_win32 bmake/common/test \
	     bmake/common/bopt* bmake/*/rules bmake/*/variables bmake/*/packages \
	     bmake/*/petscconf.h bmake/*/petscfix.h bmake/win32/makefile.dos bmake/config/*.in \
             bmake/*/buildtest bmake/adic.init bmake/adicmf.init
DOCS	   = bmake/readme bmake/petscconf.defs
SCRIPTS    = maint/addlinks maint/builddist maint/buildlinks maint/wwwman \
	     maint/xclude maint/crontab  \
	     maint/autoftp include/foldinclude/generateincludes

updatewebdocs:
	-chmod -R ug+w /mcs/tmp/petsc-tmp
	-chgrp -R petsc /mcs/tmp/petsc-tmp
	-/bin/rm -rf /mcs/tmp/petscdocs
	-/bin/cp -r /mcs/tmp/petsc-tmp/docs /mcs/tmp/petscdocs
	-maint/update-docs.py /mcs/tmp/petscdocs
	-find /mcs/tmp/petscdocs -type d -name "*" -exec chmod g+w {} \;
	-/bin/cp -r /mcs/tmp/petscdocs/* ${PETSC_DIR}/docs
	-/bin/rm -rf /mcs/tmp/petscdocs

# Builds all the documentation - should be done every night
alldoc: allmanualpages
	cd docs/tex/manual; ${OMAKE} manual.dvi manual.ps manual.html splitmanual.html

# Deletes man pages (HTML version)
deletemanualpages:
	${RM} -f ${LOC}/docs/manualpages/*/*.html \
                 ${LOC}/docs/manualpages/manualpages.cit 

# Builds all versions of the man pages
allmanualpages: deletemanualpages
	-${OMAKE} ACTION=manualpages_buildcite tree_basic LOC=${LOC}
	-${OMAKE} ACTION=manualpages tree_basic  LOC=${LOC}
	-maint/wwwindex.py ${PETSC_DIR} ${LOC}
	-${OMAKE} ACTION=manexamples tree  LOC=${LOC}
	-${OMAKE} manconcepts  LOC=${LOC}
	-${OMAKE} ACTION=getexlist tree LOC=${LOC}
	-${OMAKE} ACTION=exampleconcepts tree LOC=${LOC}
	-maint/helpindex.py ${PETSC_DIR} ${LOC}
	-@chmod g+w ${LOC}/docs/manualpages/*/*.html

# Builds .html versions of the source
allhtml: 
	-${OMAKE} ACTION=html PETSC_DIR=${PETSC_DIR} tree LOC=${LOC}

allcleanhtml: 
	-${OMAKE} ACTION=cleanhtml PETSC_DIR=${PETSC_DIR} tree

# Builds Fortran stub files
allfortranstubs:
	-@include/foldinclude/generateincludes
	-@${RM} -f src/fortran/auto/*.c
	-${OMAKE} ACTION=fortranstubs tree_basic
	-@cd src/fortran/auto; ${OMAKE} fixfortran
	chmod g+w src/fortran/auto/*.c

allci: 
	-@${OMAKE} BOPT=${BOPT} PETSC_ARCH=${PETSC_ARCH} ACTION=ci  alltree 

allco: 
	-@${OMAKE} BOPT=${BOPT} PETSC_ARCH=${PETSC_ARCH} ACTION=co  alltree 

# usage make allrcslabel NEW_RCS_LABEL=v_2_0_28
allrcslabel: 
	-@${OMAKE} BOPT=${BOPT} PETSC_ARCH=${PETSC_ARCH} NEW_RCS_LABEL=${NEW_RCS_LABEL} ACTION=rcslabel  alltree 
#
#   The commands below are for generating ADIC versions of the code;
# they are not currently used.
#
alladicignore:
	-@${RM} ${INSTALL_LIB_DIR}/adicignore
	-@${OMAKE} BOPT=${BOPT} PETSC_ARCH=${PETSC_ARCH} ACTION=adicignore  tree 

alladic:
	-@echo "Beginning to compile ADIC source code in all directories"
	-@echo "Using ADIC compiler: ${ADIC_CC} ${CCPPFLAGS}"
	-@echo "========================================="
	-@cd include ; \
           ${ADIC_CC} -s -f 1 ${CCPPFLAGS} petsc.h 
	-@${OMAKE} BOPT=${BOPT} PETSC_ARCH=${PETSC_ARCH} ACTION=adic  tree 
	-@cd src/inline ; \
            ${OMAKE} BOPT=${BOPT} PETSC_ARCH=${PETSC_ARCH} adic
	-@cd src/blaslapack ; \
            ${OMAKE} BOPT=${BOPT} PETSC_ARCH=${PETSC_ARCH} ACTION=adic  tree

alladiclib:
	-@echo "Beginning to compile ADIC libraries in all directories"
	-@echo "Using compiler: ${CC} ${COPTFLAGS}"
	-@echo "-----------------------------------------"
	-@echo "Using PETSc flags: ${PETSCFLAGS} ${PCONF}"
	-@echo "-----------------------------------------"
	-@echo "Using configuration flags:"
	-@grep "define " bmake/${PETSC_ARCH}/petscconf.h
	-@echo "-----------------------------------------"
	-@echo "Using include paths: ${PETSC_INCLUDE}"
	-@echo "-----------------------------------------"
	-@echo "Using PETSc directory: ${PETSC_DIR}"
	-@echo "Using PETSc arch: ${PETSC_ARCH}"
	-@echo "========================================="
	-@${RM} -f  ${INSTALL_LIB_DIR}/*adic.a
	-@${OMAKE} BOPT=${BOPT} PETSC_ARCH=${PETSC_ARCH} ACTION=adiclib  tree
	-@cd src/blaslapack ; \
            ${OMAKE} BOPT=${BOPT} PETSC_ARCH=${PETSC_ARCH} ACTION=adiclib  tree
	-@cd src/adic/src ; \
            ${OMAKE} BOPT=${BOPT} PETSC_ARCH=${PETSC_ARCH} lib

# -------------------------------------------------------------------------------
#
# Some macros to check if the fortran interface is up-to-date.
#
countfortranfunctions: 
	-@cd ${PETSC_DIR}/src/fortran; egrep '^void' custom/*.c auto/*.c | \
	cut -d'(' -f1 | tr -s  ' ' | cut -d' ' -f2 | uniq | egrep -v "(^$$|Petsc)" | \
	sed "s/_$$//" | sort > /tmp/countfortranfunctions

countcfunctions:
	-@ grep extern ${PETSC_DIR}/include/*.h *.h | grep "(" | tr -s ' ' | \
	cut -d'(' -f1 | cut -d' ' -f3 | grep -v "\*" | tr -s '\012' |  \
	tr 'A-Z' 'a-z' |  sort > /tmp/countcfunctions

difffortranfunctions: countfortranfunctions countcfunctions
	-@echo -------------- Functions missing in the fortran interface ---------------------
	-@${DIFF} /tmp/countcfunctions /tmp/countfortranfunctions | grep "^<" | cut -d' ' -f2
	-@echo ----------------- Functions missing in the C interface ------------------------
	-@${DIFF} /tmp/countcfunctions /tmp/countfortranfunctions | grep "^>" | cut -d' ' -f2
	-@${RM}  /tmp/countcfunctions /tmp/countfortranfunctions

checkbadfortranstubs:
	-@echo "========================================="
	-@echo "Functions with MPI_Comm as an Argument"
	-@echo "========================================="
	-@cd ${PETSC_DIR}/src/fortran/auto; grep '^void' *.c | grep 'MPI_Comm' | \
	tr -s ' ' | tr -s ':' ' ' |cut -d'(' -f1 | cut -d' ' -f1,3
	-@echo "========================================="
	-@echo "Functions with a String as an Argument"
	-@echo "========================================="
	-@cd ${PETSC_DIR}/src/fortran/auto; grep '^void' *.c | grep 'char \*' | \
	tr -s ' ' | tr -s ':' ' ' |cut -d'(' -f1 | cut -d' ' -f1,3
	-@echo "========================================="
	-@echo "Functions with Pointers to PETSc Objects as Argument"
	-@echo "========================================="
	-@cd ${PETSC_DIR}/src/fortran/auto; \
	_p_OBJ=`grep _p_ ${PETSC_DIR}/include/*.h | tr -s ' ' | \
	cut -d' ' -f 3 | tr -s '\012' | grep -v '{' | cut -d'*' -f1 | \
	sed "s/_p_//g" | tr -s '\012 ' ' *|' ` ; \
	for OBJ in $$_p_OBJ; do \
	grep "$$OBJ \*" *.c | tr -s ' ' | tr -s ':' ' ' | \
	cut -d'(' -f1 | cut -d' ' -f1,3; \
	done 
# Builds noise routines (not yet publically available)
# Note:	 libfast cannot run on .F files on certain machines, so we
# use lib and check for errors here.
noise: info chklib_dir
	-@echo "Beginning to compile noise routines"
	-@echo "========================================="
	-@cd src/snes/interface/noise; \
	  ${OMAKE} BOPT=${BOPT} PETSC_ARCH=${PETSC_ARCH} lib > trashz 2>&1; \
	  grep -v clog trashz | grep -v "information sections" | \
	  egrep -i '(Error|warning|Can)' >> /dev/null;\
	  if [ "$$?" != 1 ]; then \
	  cat trashz ; fi; ${RM} trashz
	${RANLIB} ${INSTALL_LIB_DIR}/libpetscsnes.a
	-@chmod g+w  ${INSTALL_LIB_DIR}/libpetscsnes.a
	-@echo "Completed compiling noise routines"
	-@echo "========================================="

#
# Automatically generates PETSc exercises in html from the tutorial examples.
#
# The introduction for each section is obtained from docs/manualpages/bop.${MANSEC} is under RCS and may be edited
#  (used also in introductions to the manual pages)
# The overall introduction is in docs/exercises/introduction.html and is under RCS and may be edited
# The list of exercises is from TUTORIALS in each directory's makefile
#
# DO NOT EDIT the pageform.txt or *.htm files generated since they will be automatically replaced.
# The pagemaker rule is in the file bmake/common (at the bottom)
#
# Eventually the line below will replace the two cd in the rule below, it is just this way now for speed
#	-@${OMAKE} PETSC_DIR=${PETSC_DIR} pagemaker
#
exercises:
	-@echo "========================================="
	-@echo "Generating HTML tutorial exercises"
	-@${RM} docs/pageform.txt
	-@echo "title=\"PETSc Exercises\""                >  docs/pageform.txt 
	-@echo "access_title=Exercise Sections"              >>  docs/pageform.txt 
	-@echo "access_format=short"                        >> docs/pageform.txt
	-@echo "startpage=../exercises/introduction.htm"  >> docs/pageform.txt
	-@echo "NONE title=\"Introduction\" command=link src=../exercises/introduction.htm" >> docs/pageform.txt
	-@echo "Generating HTML for individual directories"
	-@echo "========================================="
	-@${OMAKE} PETSC_DIR=${PETSC_DIR} ACTION=pagemaker tree
	-@echo "Completed HTML for individual directories"
	-@echo "NONE title=\"<HR>\" " >> docs/pageform.txt; 
	-@echo "NONE title=\"PETSc Documentation\" command=link src=../index.html target=replace" >> docs/pageform.txt
	/home/MPI/class/mpiexmpl/maint/makepage.new -pageform=docs/pageform.txt -access_extra=/dev/null -outdir=docs/exercises
	-@echo "========================================="

.PHONY: info info_h build build_lt testexamples testfortran testexamples_uni testfortran_uni ranlib deletelibs allclean update chk_petsc_dir \
        alletags etags etags_complete etags_noexamples etags_makefiles etags_examples etags_fexamples updatewebdocs alldoc allmanualpages \
        allhtml allcleanhtml allfortranstubs allci allco allrcslabel alladicignore alladic alladiclib countfortranfunctions \
        start_configure configure_petsc configure_clean

