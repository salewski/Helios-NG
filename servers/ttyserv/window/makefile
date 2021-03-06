# Makefile for IOS-Nucleus
.SUFFIXES:
.SUFFIXES: .z .i .o .x .c .a

HELIOS 		= /c/helios
INCLUDE 	= /ram/include/,$(HELIOS)/include
LIB 		= $(HELIOS)/lib
BIN 		= $(HELIOS)/bin
SYSSRC		= $(HELIOS)/sys_src
IOS		= $(HELIOS)/ios
KERNEL		= $(SYSSRC)/kernel
UTIL 		= $(SYSSRC)/util
IOSDEBUG	= $(IOS)/debug
INUCLEUS	= $(IOS)/inucleus

CC 		= cc
AMPP 		= ampp
ASM 		= asm
CP		= cp
OBJED		= objed
GDI		= gdi
SYSBUILD	= sysbuild
RM		= rm

SSTART		= $(LIB)/s0.o
CSTART		= $(LIB)/c0.o
UTILDEF		= $(LIB)/util.def
BOOT 		= $(LIB)/nboot.i
KINCLUDE	= $(KERNEL)/include
ERRORS		= errors
FIFO		= /ram

HFILES		= window.h $(IOSDEBUG)/debug.h
CFILES		= winlib.c globals.c termcap.c input.c terminal.c ansi.c window.c $(IOSDEBUG)/debug.c 
OFILES 		= winlib.o globals.o termcap.o input.o terminal.o ansi.o window.o debug.o

LIBS 		= $(HELIOS)/lib/helios.lib

NONE		= 0x0
TCAP		= 0x1
TCAP_ALL	= 0x2
INPUT		= 0x4
INPUT_ALL	= 0x8
TERM		= 0x10
TERM_ALL	= 0x20
ANSI		= 0x40
ANSI_ALL	= 0x80
READ		= 0x100
READ_ALL	= 0x200
WRITE		= 0x400
WRITE_ALL	= 0x800
OPEN		= 0x1000
CREATE		= 0x4000
DELETE		= 0x10000
ATTR		= 0x40000
SETTERM		= 0x100000
CBREAK		= 0x400000
MAIN		= 0x1000000
MAIN_ALL	= 0x2000000
ERROR		= 0x80000000

#DLEVEL		= DEBUG=0xFFFFFFFF
#DLEVEL		= DEBUG=$(MAIN_ALL)
#DLEVEL		= DEBUG=$(ATTRIBS)+$(CBREAK)
#+$(ANSI)+$(WRITE)+$(DOPRIVATE)+$(READ)
#+$(OPEN)+$(CREATE)+$(DELETE)
#DLEVEL		= DEBUG=$(MAIN_ALL)+$(CREATE)+$(DELETE)+$(OPEN)
#DLEVEL		= DEBUG=$(ERROR)
DLEVEL		= DEBUG=$(NONE)

DFLAGS		= -dSYSDEBUG
NCFLAGS 	= -t4 -ps1 -pg0 -j$(INCLUDE)/ -d$(DLEVEL) $(DFLAGS)
LINKOPTS	= -v -f -s 4000

#default rules

.c.x:
	$(CC) $(NCFLAGS) $*.c -s$*.x >>& $(ERRORS)

.c.o:  
	$(CC) $(NCFLAGS) $*.c -s$(FIFO)/$*.p >>& $(ERRORS) && \
	$(ASM) -p $(FIFO)/$*.p -o$@ && \
	$(RM) $(FIFO)/$*.p

.a.o:
	$(AMPP) -i$(KINCLUDE)/ $(KINCLUDE)/basic.m $*.a | \
	$(ASM) -p -o $@

.a.x:
	$(AMPP) -o$@ -i$(KINCLUDE)/ $(KINCLUDE)/basic.m $*.a

.x.o:
	$(ASM) -p $*.x -o$@

.o.z:
	asm $(LINKOPTS) $(CSTART) $*.o -o$@


default: delerror iwindow.i copywin setterm diag_wm showerror

$(OFILES)	: $(HFILES)

delerror	:
	-$(RM) -f $(ERRORS)

showerror:
	-grep '[0-9a-z_/][0-9a-z_/]\.[cho]' $(ERRORS)

iwindow.i	: $(OFILES)
	asm $(LINKOPTS) $(SSTART) $(OFILES)  -l$(LIBS) -o iwindow.i

dwindow.i	: $(OFILES)
	asm $(LINKOPTS) $(SSTART) $(OFILES)  -l$(LIBS) -o dwindow.i

copywin:
#	$(CP) iwindow.i $(LIB)/iwindow && \
#	$(CP) iwindow.i $(INUCLEUS)

debug.o		: $(IOSDEBUG)/debug.c makefile
	$(CC) $(NCFLAGS) $(IOSDEBUG)/debug.c -s$(FIFO)/$*.p >>& $(ERRORS) && \
	$(ASM) -p $(FIFO)/$*.p -o$@ && \
	$(RM) $(FIFO)/$*.p

setterm.o	: $(HFILES)

setterm		: setterm.o
	asm $(LINKOPTS) $(CSTART) -l$(LIBS) /helios/lib/c.lib $@.o -o$@ >>& $(ERRORS)
#	asm $(LINKOPTS) $(CSTART) -l$(LIBS) /helios/lib/c.lib $@.o -o$@ >>& $(ERRORS) && \
#	$(CP) $@ /c/helios/bin


diags.o		: $(IOSDEBUG)/diags.c
	$(CC) $(NCFLAGS) $(IOSDEBUG)/diags.c -s$(FIFO)/$*.p >>& $(ERRORS) && \
	$(ASM) -p $(FIFO)/$*.p -o$@ && \
	$(RM) $(FIFO)/$*.p

diag_wm.o	: $(HFILES) $(IOSDEBUG)/diags.h

diag_wm		: diag_wm.o diags.o
	asm $(LINKOPTS) $(CSTART) $@.o diags.o  -l$(LIBS) /helios/lib/c.lib -o$@ >>& $(ERRORS)
#	asm $(LINKOPTS) $(CSTART) $@.o diags.o  -l$(LIBS) -o$@ >>& $(ERRORS) && \
#	$(CP) $@ /c/helios/bin

