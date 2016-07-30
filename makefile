##
## PIN tools
##


##############################################################
#
# Here are some things you might want to configure
#
##############################################################

TARGET_COMPILER?=gnu
ifdef OS
    ifeq (${OS},Windows_NT)
        TARGET_COMPILER=ms
    endif
endif

##############################################################
#
# include *.config files
#
##############################################################

ifeq ($(TARGET_COMPILER),gnu)
    include ../makefile.gnu.config
    OPT=-g
    CXXFLAGS = -I$(PIN_HOME)/InstLib -fomit-frame-pointer -Wall -Werror -Wno-unknown-pragmas $(DBG) $(OPT) -MMD
endif

ifeq ($(TARGET_COMPILER),ms)
    include ../makefile.ms.config
#    DBG?=
endif


 
TOOL_ROOTS = pinatrace buffer-lin bb-sequence traceusage traceId cjmp-cache cjmp-sequence0 cjmp-sequence cjmp-sequence2 cjmp-sequence3 countreps \
             icount5 trace-sequence traceNum bb-sequence2 edgcnt cjmp-noprint nullpin bb-sequence64 traceNum2 bb-dep bb-dep2 countreps-max bb-rep bbdep_threadpool
TOOLS = $(TOOL_ROOTS:%=$(OBJDIR)%$(PINTOOL_SUFFIX)) 

all: tools
tools: $(OBJDIR) $(TOOLS)
#test: $(OBJDIR) pincov.test

## build rules

$(OBJDIR):
	mkdir -p $(OBJDIR)



$(OBJDIR)%.o : %.cpp
	$(CXX) -c $(CXXFLAGS) $(PIN_CXXFLAGS) ${OUTOPT}$@ $<

$(TOOLS): $(PIN_LIBNAMES)
$(TOOLS): %$(PINTOOL_SUFFIX) : %.o
	$(PIN_LD) $(PIN_LDFLAGS) ${LINK_OUT}$@ $< $(PIN_LIBS) $(DBG)

## cleaning
clean:
	-rm -rf $(OBJDIR) *.out *.tested *.failed

-include *.d

