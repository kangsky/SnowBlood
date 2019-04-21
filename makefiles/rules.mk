# build the object files
OBJDIR			:=	$(BUILDDIR)/obj
OBJS 			:=  $(addprefix $(OBJDIR)/,$(addsuffix .o,$(SRCS)))

# Static Library
$(TARGETLIB): $(OBJS)
	$(AR) rcs $@ $^
	$(RANLIB) $@
	mkdir -p $(BIN_DST_DIR)
	cp $(TARGETLIB) $(BIN_DST_DIR)

# Linking library
$(TARGET) : $(TARGETLIB)
	$(CC) -o $(TARGET) $(MAIN_SRC) $(COPTS) -L$(BIN_DST_DIR) -lscheduler
	cp $(TARGET) $(BIN_DST_DIR)

# Build rules
$(OBJDIR)/%.c.o: $(SRCROOT)/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $(COPTS) $(<) -o $@

$(OBJDIR)/%.cpp.o: $(SRCROOT)/%.cpp
	@mkdir -p $(dir $@)
	$(CC) -Drestrict=__restrict -std=c++11 -fno-exceptions -fno-rtti -c $(COPTS) $(<) -o $@

clean:
	rm -rf $(OBJROOT) $(DSTROOT) 
#	$(-v)rm -rf $(OBJROOT) $(DSTROOT)

print-%:
	@echo $* is $($*) 
