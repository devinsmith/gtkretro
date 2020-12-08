## Makefile for building glib with gcc (mingw/linux)

.PHONY: all clean

CC = gcc
LDFLAGS =
GLIB_VER=1.3

RM = rm -f

LIB = libdrsglib-$(GLIB_VER)
DLL = drsglib-$(GLIB_VER).dll
IMPORT = $(LIB)dll.a

WINDRES = windres

OBJS =			\
	garray.o		\
	gasyncqueue.o		\
	gbacktrace.o		\
	gcache.o		\
	gcompletion.o		\
	gconvert.o		\
	gdataset.o		\
	gdate.o		\
	gerror.o		\
	gfileutils.o		\
	ghash.o		\
	ghook.o		\
	giochannel.o		\
	glist.o		\
	gmain.o		\
	gmarkup.o		\
	gmem.o		\
	gmessages.o		\
	gnode.o		\
	gprimes.o		\
	gqsort.o		\
	gqueue.o		\
	gpattern.o	\
	grand.o		\
	grel.o		\
	gscanner.o		\
	gshell.o		\
	gslist.o		\
	gstrfuncs.o		\
	gstring.o		\
	gbsearcharray.o \
	gthread.o		\
	gthreadpool.o		\
	gtimer.o		\
	gtree.o		\
	gunibreak.o		\
	gunicollate.o	\
	gunidecomp.o		\
	guniprop.o		\
	gutf8.o		\
	gutils.o

CFLAGS = -I. -I.. -I../include -DG_LOG_DOMAIN=g_log_domain_glib -DGLIB_COMPILATION -DHAVE_CONFIG_H -DUSE_LIBICONV -DG_IMPLEMENT_INLINES

ifeq ($(OS),Windows_NT)
	CC = gcc
	OBJS += giowin32.o gspawn-win32.o gwin32.o glibres.o
	ifeq ($(SHELL),sh.exe)
		# Executing under cmd.exe
		RM = del /Q /f 2>NUL
	endif
else
	OBJS += giounix.o gspawn.o
	CFLAGS += -fPIC
	DLL = $(LIB).so
endif

# You could add this if you wanted... -DG_ENABLE_DEBUG
######################

all :				\
	glibconfig.h	\
	$(DLL)

################ glib

glib-$(GLIB_VER).dll : $(OBJS) glib.def
	dllwrap --def glib.def -o glib-$(GLIB_VER).dll $(OBJS) \
		--output-lib $(IMPORT) $(LIBICONV_LIBS) $(INTL_LIBS) -lws2_32

$(LIB).so : $(OBJS)
	gcc -shared -o $@ $(OBJS)

glibres.o: glib.rc
	$(WINDRES) -o $@ glib.rc

.c.o:
	$(CC) -Wall -O2 $(CFLAGS) -DGDK_COMPILATION -c $< -o $@

################ other stuff

clean:
	$(RM) $(OBJS)
