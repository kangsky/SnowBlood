SCHEDULERDIR	:=	scheduler

SRCS			+=	$(call FIND_SOURCES,$(SRCDIR)/$(SCHEDULERDIR),$(SRCROOT))

EXTRA_INCLUDES	+=	$(SRCDIR)/$(SCHEDULERDIR)/include \