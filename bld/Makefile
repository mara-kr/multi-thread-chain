LIB = libchain

OBJECTS = \
	chain.o \
	thread.o

DEPS += \
	libmsp \

LIBCHAIN_ENABLE_DIAGNOSTICS = 1
override SRC_ROOT = ../../src

override CFLAGS += \
	-I$(SRC_ROOT) \
	-I$(SRC_ROOT)/include/libchain \

include ../Makefile.config
