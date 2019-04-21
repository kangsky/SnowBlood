# Utility macros

# FIND_SOURCES_UNCHANGED: Find C and C++ source files under $(1)
FIND_SOURCES_UNCHANGED	 = $(wildcard $(1)/*.cpp) $(wildcard $(1)/*.c)

# CHANGE_PATH: Take files from $(1) and return their paths relative to $(2)
CHANGE_PATH		 = $(patsubst $(2)/%,%,$(1))

# FIND_SOURCES: Find C, C++ and assembler source files under $(1) and return paths
#               relative to $2
FIND_SOURCES	 = $(call CHANGE_PATH,$(call FIND_SOURCES_UNCHANGED,$(1)),$(2))
