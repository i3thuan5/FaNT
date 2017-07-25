################################################
#
#	@(#).make	1.2 6/17/90
# Makefile from GUENI
################################################

## List of files to make the program :

SOURCES   = ugst-utl.c cascg712.c iir-lib.c fir-hp.c fir-wb.c fir-lib.c fir-irs.c fir-flat.c sv-p56.c filter_add_noise.c
USERLIBS  = 
SYSLIBS   = -lm 
PROGRAM   = filter_add_noise

## Options for compiler, linker:
CC        = gcc 
XINCLUDE  = 
CFLAGS    = -g -Wall $(XINCLUDE) -O3 
LDFLAGS   = 

#################################################

OBJS     = $(SOURCES:.c=.o) 

.KEEP_STATE:

all:	$(PROGRAM)

$(PROGRAM):	$(OBJS)
		$(LINK.c) $(LDFLAGS) -o $@ $(OBJS) $(SYSLIBS) $(USERLIBS)


clean:
	rm -rf $(PROGRAM) $(OBJS)

.c.o:
	$(CC) $(CFLAGS)  -c $<
