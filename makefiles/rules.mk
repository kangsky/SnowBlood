# build the object files
OBJDIR			:=	$(BUILDDIR)/obj
OBJS 			:=  $(addprefix $(OBJDIR)/,$(addsuffix .o,$(SRCS)))
APP_OBJS 		:=  $(addprefix $(OBJDIR)/,$(addsuffix .o,$(APP_SRCS)))

# Static Library
$(TARGETLIB): $(OBJS)
	$(AR) rcs $@ $^
	$(RANLIB) $@
	mkdir -p $(BIN_DST_DIR)
	cp $(TARGETLIB) $(BIN_DST_DIR)

# Linking library
$(TARGET) : $(TARGETLIB) $(APP_OBJS) 
	$(CC) -o $(TARGET) $(MAIN_SRC) $(COPTS) -L$(BIN_DST_DIR) -lscheduler
	cp $(TARGET) $(BIN_DST_DIR)

# Build rules
$(OBJDIR)/%.c.o: $(SRCROOT)/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $(COPTS) $(<) -o $@

$(OBJDIR)/%.cpp.o: $(SRCROOT)/%.cpp
	@mkdir -p $(dir $@)
	$(CC) -Drestrict=__restrict -std=c++11 -fno-exceptions -fno-rtti -c $(COPTS) $(<) -o $@

# Build rules for direct executable files which are excluded from static library
$(OBJDIR)/%.o: $(SRCROOT)/%.c
	@mkdir -p $(dir $@)
	$(CC) -o $(basename $@) $(<) $(COPTS)
	cp $(basename $@) $(BIN_DST_DIR)

clean:
	rm -rf $(OBJROOT) $(DSTROOT) 
#	$(-v)rm -rf $(OBJROOT) $(DSTROOT)
