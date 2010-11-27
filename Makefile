##===- projects/sample/Makefile ----------------------------*- Makefile -*-===##
#
# This is a sample Makefile for a project that uses LLVM.
#
##===----------------------------------------------------------------------===##

#
# Indicates our relative path to the top of the project's root directory.
#
LEVEL = .
DIRS = lib
EXTRA_DIST = include

#
# Include the Master Makefile that knows how to build all.
#
include $(LEVEL)/Makefile.common

#
# using various llvm commands
#

.PRECIOUS: %.bc

# create .bc from source
%.bc:	%.c
	llvm-gcc -emit-llvm -O0 -c $*.c -o $*.bc

# create human-readable assembly (.ll) from .bc
%.ll: %.bc
	llvm-dis -f $^

# create executable from .bc
%.exe: %.bc
	llc -f $^
	gcc $*.s -o $*.exe

# create bitcode optimized to keep vars in registers
%.mem2reg: %.bc
	opt -mem2reg $^ > $*.mem2reg

# run printCode on a .bc file
%.printCode: %.bc
	opt -load Debug/lib/P1.so -printCode $*.bc > $*.printCode

# run optLoads on a .bc file, creating a new .bc file
%.optLoads: %.bc
	opt -load Debug/lib/P1.so -optLoads $*.bc > $*.optLoads
	mv $*.optLoads $*.bc

# run mem2reg then sameRhs on a .c file, creating a .bc file
%.sameRhs: %.c
	make $*.bc
	make $*.mem2reg
	mv $*.mem2reg $*.bc
	opt -load Debug/lib/P1.so -sameRhs $*.bc > $*.sameRhs
	mv $*.sameRhs $*.bc



