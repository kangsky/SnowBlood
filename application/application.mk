APPDIR    	:=  application

APP_SRCS		+=  $(basename $(call FIND_SOURCES,$(SRCDIR)/$(APPDIR),$(SRCROOT)))
