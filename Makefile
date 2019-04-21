# Makefile for snowblood project

# Configurable path defines
SRCROOT			:=	$(shell pwd)
OBJROOT			:=	$(SRCROOT)/build
DSTROOT			:=	$(SRCROOT)/dst

# Compilers
CC				:=	clang
ARCH			:=  x86-64
AR				:=	ar
RANLIB			:= 	ranlib

# Destination
BIN_DST_DIR				:=	$(DSTROOT)/bin
INCLUDE_DST_DIR			:=	$(DSTROOT)/include
BUILDDIR				:=	$(SRCROOT)/build
export MAKEFILE_ROOT	:= $(SRCROOT)/makefiles

include $(MAKEFILE_ROOT)/macros.mk

PROG			=	SnowBlood
BIN             = 	$(BUILDDIR)/$(PROG).bin
ORDERFILE      	= 	$(BUILDDIR)/$(PROG).order
MAPFILE         = 	$(BUILDDIR)/$(PROG).map
DISFILE         = 	$(BUILDDIR)/$(PROG).dis
DSYM            = 	$(BUILDDIR)/$(PROG).dSYM
LTOOBJFILE      = 	$(BUILDDIR)/$(PROG).o
TARGETLIB		=	$(BUILDDIR)/libscheduler.a
TARGET			=	$(BUILDDIR)/SnowBlood

# Libraries


# Sources
SRCDIR			:=	$(SRCROOT)

# source files
SRCS			:=	
MAIN_SRC		:= 	$(SRCDIR)/SnowBlood.c

# include files
EXTRA_INCLUDES		:=	$(SRCDIR)/include \
						$(SRCDIR)/scheduler/include \
						$(SRCDIR)/common/include \

# Sub-makefiles
include scheduler/scheduler.mk
include common/common.mk

INCLUDES            +=	$(addprefix -I, $(EXTRA_INCLUDES)) \

# Default build options
COPTS				:=	-ffreestanding \
						-g \
						-O0 \
						$(INCLUDES)

LDOPTS				:=	-Wl,-map,$(MAPFILE) 

# Targets
.PHONY: all build install 
.DEFAULT: all
all: install
install: build
build: $(TARGET)

# Include common rules
include $(MAKEFILE_ROOT)/rules.mk