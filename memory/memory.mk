MEMORYDIR    :=  memory
  
SRCS            +=  $(call FIND_SOURCES,$(SRCDIR)/$(MEMORYDIR),$(SRCROOT))

EXTRA_INCLUDES  +=  $(SRCDIR)/$(MEMORYDIR)/include \
