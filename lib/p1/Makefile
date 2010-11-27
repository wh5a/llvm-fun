##===- projects/sample/lib/sample/Makefile -----------------*- Makefile -*-===##

#
# Indicate where we are relative to the top of the source tree.
#
LEVEL=../..

#
# Give the name of a library.  This will build a dynamic version.
#
LIBRARYNAME=P1
#DONT_BUILD_RELINKED=1
BUILD_ARCHIVE=1
LOADABLE_MODULE = 1
LINK_COMPONENTS = #core support system
# Using core appears to crash it; support or system gives 'redefinitions'

#
# Include Makefile.common so we know what to do.
#
include $(LEVEL)/Makefile.common
