#-----------------------------------------------------------------------------
# Helios generic make system
#-----------------------------------------------------------------------------
# Helios generic component make rules for: Assembler Macro Pre-Processor (AMPP)
#
# File: /HSRC/cmds/ampp/makefile
#
# These are the host/processor independent rules and variables for this
# component of Helios. It is included by the processor specific makefile
# That may add to some of its functionality. You should only alter this
# makefile by using "ifneq ($(HPROC),YYY)" clauses for a specific processor.
#
# SccsId: %W% %G%
# RcsId: $Id: makefile,v 1.6 1992/06/29 17:03:59 nickc Exp $
#		(C) Copyright 1990 Perihelion Software
#
# WARNING: assumes you are using GNUmake.
#-----------------------------------------------------------------------------

ifndef InProcMake
# This part of the makefile is run in the components generic source directory

  # These variables should already be set by your make script or environment.
  ifndef HPROC
    HPROC = TRAN
  endif

  ifndef HHOST
    HHOST = SUN4
  endif

  ifndef HSRC
    HSRC = /usr/perihelion/Helios
  endif

  ifndef HPROD
    HPROD = $(HSRC)/slashhelios/$(HPROC)
  endif

.PHONY: notarget
notarget:
	@$(MAKE) -C $(HPROC)

.DEFAULT:
	@$(MAKE) -C $(HPROC) $@

else
#-----------------------------------------------------------------------------
# This part of the makefile is always run in a processor specific subdirectory.

# Manditory variables for a component:
# name of this dir relative to $(HSRC)
COMPONENT = cmds/ampp

# Manditory includes
include $(HSRC)/makeinc/$(HHOST).mak	# Default host system variables
include $(HSRC)/makeinc/$(HPROC).mak	# Default processor rules and vars.

# Manditory phoney targets
.PHONY: default clean Xdefault Xclean
# "default" - should make this component in its proc subdir
# "install" - copy the new component to $(HPROD) production directory
# "clean"   - remove all intermediate and executable files from proc subdir
# Phoney targets prefixed by X are for adding processor specific targets.
#
# Processor specific sources are held in the processor specific subdir.,
# generic ones in the directory above it.
HVPATH := $(HSRC)/$(COMPONENT)

ifdef RSRC
HVPATH := $(RSRC)/$(COMPONENT)/$(HPROC):$(HVPATH):$(RSRC)/$(COMPONENT)
endif

vpath %.h $(HVPATH)
vpath %.c $(HVPATH)
vpath %.a $(HVPATH)

#-----------------------------------------------------------------------------
# Generic component make rules and variables added below here

# Note all final generic targets here
TARGETS = ampp

default : $(TARGETS) Xdefault

install	: $(TARGETS) Xinstall
	$(CP) ampp $(BIN)
	$(TOUCH) install

clean	: Xclean
	-$(RM)	$(OBJECTS) $(TARGETS) $(XOBJECTS) $(XTARGETS) mkout \
		core ../mkout install installdef Xinstall

#-----------------------------------------------------------------------------
# for archive, backup and cleaning purposes,
# note all generic SOURCE and OBJECT files:

# Note all sources
SOURCES		= $(SOURCES.c) $(SOURCES.a) \
		   $(HEADERS.c) $(HEADERS.m) \
		   $(SOURCES.odd)

#Full set of library object files
#OBJECTS	= $(SOURCES.c:.c=.p) $(SOURCES.a:.a=.p)
# Or if you are not a library:
OBJECTS	= $(SOURCES.c:.c=.o)

# C source files
SOURCES.c	= ampp.c macro.c lex.c sym.c builtin.c defs.c \
		putback.c charstack.c output.c

# AMPP source files
SOURCES.a	= #t.a			# test file

# C header files
HEADERS.c	= ampp.h

# AMPP macro files
HEADERS.m	=

# Any other source files
SOURCES.odd	= makefile hash.c


#-----------------------------------------------------------------------------
# Other generic component variables:

# Set any flags required
ifeq ($(HPROC),ARM)
  NCFLAGS := $(NCFLAGS)
else
  NCFLAGS := $(NCFLAGS) -wd
endif

# Note any library def files to be linked with
LIBDEFS	=


#-----------------------------------------------------------------------------
# Generic targets and their dependencies:

# uncomment if you have compiler or makedepend generated dependencies
#include depends

$(OBJECTS): ampp.h

ampp:	$(OBJECTS) 
	$(LINK) $(LINKFLAGS) -o$@ -s30000 -h50000 -nampp $(LIB)/cstart.o $^



#-----------------------------------------------------------------------------
endif #InProcMake	# This line must be at EOF

