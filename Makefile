# Set this!

BASEDIR=/home/gram/PEW

# Nothing should need to change below here

SRCDIR=$(BASEDIR)/src
INCDIR=$(BASEDIR)/include
OBJDIR=$(BASEDIR)/obj
RUNDIR=$(BASEDIR)/run

########################################################################

CFLAGS=-I$(INCDIR) -c -DUNIX -O -g -Wall
#LFLAGS=-g -lm -L/usr/local/lib -lefence
LFLAGS=-g -lm

########################################################################

all: ec ei eci pa gp epretty errors

ec: $(RUNDIR)/ec errors

ei: $(RUNDIR)/ei errors

eci: $(RUNDIR)/eci errors

pa: $(RUNDIR)/pa

gp: $(RUNDIR)/gp

epretty: $(RUNDIR)/epretty

errors: $(RUNDIR)/ERROR.HLP

$(RUNDIR)/ERROR.HLP: $(SRCDIR)/ERROR.TBL $(SRCDIR)/ERROR.HLP
	cp $(SRCDIR)/ERROR.* $(RUNDIR)

clean:
	-rm $(OBJDIR)/*.o $(RUNDIR)/eci $(RUNDIR)/gp $(RUNDIR)/pa $(RUNDIR)/ec $(RUNDIR)/ei $(RUNDIR)/ERROR.*

########################################################################

ECIOBJS= $(OBJDIR)/browse.o   $(OBJDIR)/interp.o   $(OBJDIR)/ecirun.o\
	 $(OBJDIR)/ecisym.o   $(OBJDIR)/error.o    $(OBJDIR)/compiler.o\
	 $(OBJDIR)/pew_eci.o  $(OBJDIR)/elink.o    $(OBJDIR)/unixgen.o\
	 $(OBJDIR)/unix.o     $(OBJDIR)/pasparse.o $(OBJDIR)/estparse.o\
	 $(OBJDIR)/stmt_exp.o $(OBJDIR)/tree.o

EIOBJS=  $(OBJDIR)/browse.o $(OBJDIR)/interp.o  $(OBJDIR)/pew_run.o\
	 $(OBJDIR)/symbol.o $(OBJDIR)/unixgen.o $(OBJDIR)/unix.o\
	 $(OBJDIR)/error.o  $(OBJDIR)/tree.o

ECOBJS=  $(OBJDIR)/compiler.o $(OBJDIR)/pew_ec.o   $(OBJDIR)/elink.o\
	 $(OBJDIR)/unixgen.o  $(OBJDIR)/unix.o     $(OBJDIR)/ecsymbol.o\
	 $(OBJDIR)/pasparse.o $(OBJDIR)/estparse.o $(OBJDIR)/stmt_exp.o

PAOBJS=  $(OBJDIR)/pa.o $(OBJDIR)/gmt.o $(OBJDIR)/gaussj.o $(OBJDIR)/nrutil.o

GPOBJS=  $(OBJDIR)/prune.o $(OBJDIR)/prutils.o $(OBJDIR)/gaussj.o $(OBJDIR)/nrutil.o

EPOBJS=  $(OBJDIR)/epretty.o

########################################################################

$(RUNDIR)/eci: $(ECIOBJS)
	cc -o $(RUNDIR)/eci $(ECIOBJS) $(LFLAGS)

$(RUNDIR)/ei: $(EIOBJS)
	cc -o $(RUNDIR)/ei $(EIOBJS) $(LFLAGS)

$(RUNDIR)/ec: $(ECOBJS)
	cc -o $(RUNDIR)/ec $(ECOBJS) $(LFLAGS)

$(RUNDIR)/pa: $(PAOBJS)
	cc -o $(RUNDIR)/pa $(PAOBJS) $(LFLAGS)

$(RUNDIR)/gp: $(GPOBJS)
	cc -o $(RUNDIR)/gp $(GPOBJS) $(LFLAGS)

$(RUNDIR)/epretty: $(EPOBJS)
	cc -o $(RUNDIR)/epretty $(EPOBJS) $(LFLAGS)

########################################################################

LIBHDRS= $(INCDIR)/misc.h $(INCDIR)/help.h $(INCDIR)/screen.h\
	 $(INCDIR)/menu.h $(INCDIR)/gen.h $(INCDIR)/keybd.h

TERPHDRS = $(INCDIR)/symbol.h $(INCDIR)/interp.h

COMPHDRS = $(INCDIR)/symbol.h $(INCDIR)/compiler.h

MATHDRS = $(INCDIR)/nr.h $(INCDIR)/nrutil.h

$(OBJDIR)/browse.o: $(SRCDIR)/browse.c $(LIBHDRS) $(TERPHDRS)
	cc -o $(OBJDIR)/browse.o $(CFLAGS) $(SRCDIR)/browse.c

$(OBJDIR)/interp.o: $(SRCDIR)/interp.c $(LIBHDRS) $(TERPHDRS) $(INCDIR)/tree.h
	cc -o $(OBJDIR)/interp.o $(CFLAGS) $(SRCDIR)/interp.c

$(OBJDIR)/error.o: $(SRCDIR)/error.c $(LIBHDRS)
	cc -o $(OBJDIR)/error.o $(CFLAGS) $(SRCDIR)/error.c

$(OBJDIR)/compiler.o: $(SRCDIR)/compiler.c $(LIBHDRS) $(COMPHDRS)
	cc -o $(OBJDIR)/compiler.o $(CFLAGS) $(SRCDIR)/compiler.c

$(OBJDIR)/elink.o: $(SRCDIR)/elink.c $(LIBHDRS) $(COMPHDRS)
	cc -o $(OBJDIR)/elink.o $(CFLAGS) $(SRCDIR)/elink.c

$(OBJDIR)/unixgen.o: $(SRCDIR)/unixgen.c $(LIBHDRS)
	cc -o $(OBJDIR)/unixgen.o $(CFLAGS) $(SRCDIR)/unixgen.c

$(OBJDIR)/unix.o: $(SRCDIR)/unix.c $(LIBHDRS)
	cc -o $(OBJDIR)/unix.o $(CFLAGS) $(SRCDIR)/unix.c

$(OBJDIR)/pasparse.o: $(SRCDIR)/pasparse.c $(LIBHDRS) $(COMPHDRS)
	cc -o $(OBJDIR)/pasparse.o $(CFLAGS) $(SRCDIR)/pasparse.c

$(OBJDIR)/estparse.o: $(SRCDIR)/estparse.c $(LIBHDRS) $(COMPHDRS)
	cc -o $(OBJDIR)/estparse.o $(CFLAGS) $(SRCDIR)/estparse.c

$(OBJDIR)/stmt_exp.o: $(SRCDIR)/stmt_exp.c $(LIBHDRS) $(COMPHDRS)
	cc -o $(OBJDIR)/stmt_exp.o $(CFLAGS) $(SRCDIR)/stmt_exp.c

$(OBJDIR)/tree.o: $(SRCDIR)/tree.c $(LIBHDRS) $(INCDIR)/tree.h
	cc -o $(OBJDIR)/tree.o $(CFLAGS) $(SRCDIR)/tree.c

$(OBJDIR)/pew_run.o: $(SRCDIR)/pew_run.c $(LIBHDRS) $(INCDIR)/tree.h $(TERPHDRS)
	cc -o $(OBJDIR)/pew_run.o $(CFLAGS) $(SRCDIR)/pew_run.c

$(OBJDIR)/symbol.o: $(SRCDIR)/symbol.c $(LIBHDRS) $(TERPHDRS)
	cc -o $(OBJDIR)/symbol.o $(CFLAGS) $(SRCDIR)/symbol.c

$(OBJDIR)/pew_ec.o: $(SRCDIR)/pew_ec.c $(LIBHDRS) $(COMPHDRS)
	cc -o $(OBJDIR)/pew_ec.o $(CFLAGS) $(SRCDIR)/pew_ec.c

$(OBJDIR)/pa.o: $(SRCDIR)/pa.c $(LIBHDRS) $(MATHDRS) $(INCDIR)/pa.h
	cc -o $(OBJDIR)/pa.o $(CFLAGS) $(SRCDIR)/pa.c

$(OBJDIR)/gmt.o: $(SRCDIR)/gmt.c $(LIBHDRS) $(INCDIR)/pa.h
	cc -o $(OBJDIR)/gmt.o $(CFLAGS) $(SRCDIR)/gmt.c

$(OBJDIR)/gaussj.o: $(SRCDIR)/gaussj.c
	cc -o $(OBJDIR)/gaussj.o $(CFLAGS) $(SRCDIR)/gaussj.c

$(OBJDIR)/nrutil.o: $(SRCDIR)/nrutil.c
	cc -o $(OBJDIR)/nrutil.o $(CFLAGS) $(SRCDIR)/nrutil.c

$(OBJDIR)/prune.o: $(SRCDIR)/prune.c $(LIBHDRS) $(MATHDRS) $(INCDIR)/prune.h
	cc -o $(OBJDIR)/prune.o $(CFLAGS) $(SRCDIR)/prune.c

$(OBJDIR)/prutils.o: $(SRCDIR)/prutils.c $(INCDIR)/prune.h
	cc -o $(OBJDIR)/prutils.o $(CFLAGS) $(SRCDIR)/prutils.c

$(OBJDIR)/pew_eci.o: $(SRCDIR)/pew_ec.c $(LIBHDRS) $(COMPHDRS)
	cc -o $(OBJDIR)/pew_eci.o -DEC_EI $(CFLAGS) $(SRCDIR)/pew_ec.c

$(OBJDIR)/ecisym.o: $(SRCDIR)/symbol.c $(LIBHDRS) $(TERPHDRS)
	cc -o $(OBJDIR)/ecisym.o -DEC_EI $(CFLAGS) $(SRCDIR)/symbol.c

$(OBJDIR)/ecirun.o: $(SRCDIR)/pew_run.c $(LIBHDRS) $(TERPHDRS) $(INCDIR)/tree.h
	cc -o $(OBJDIR)/ecirun.o -DEC_EI $(CFLAGS) $(SRCDIR)/pew_run.c

$(OBJDIR)/ecsymbol.o : $(SRCDIR)/symbol.c $(LIBHDRS) $(TERPHDRS)
	cc -o $(OBJDIR)/ecsymbol.o -DMAKING_EC $(CFLAGS) $(SRCDIR)/symbol.c
	
$(OBJDIR)/tree.o: $(SRCDIR)/tree.c $(LIBHDRS) $(INCDIR)/tree.h
	cc -o $(OBJDIR)/tree.o $(CFLAGS) $(SRCDIR)/tree.c

$(OBJDIR)/epretty.o: $(SRCDIR)/epretty.c $(INCDIR)/misc.h $(INCDIR)/ecode.h
	cc -o $(OBJDIR)/epretty.o $(CFLAGS) $(SRCDIR)/epretty.c




