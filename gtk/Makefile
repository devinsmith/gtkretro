# Makefile for version of libdrsgtk

.PHONY: all clean


C_SRCS = fnmatch.c gtkcheckbutton.c gtkfilesel.c gtkinputdialog.c gtkmisc.c \
	gtkruler.c gtktipsquery.c gtkvscale.c gtkaccelgroup.c gtkcheckmenuitem.c \
	gtkfixed.c gtkinvisible.c gtknotebook.c gtkscale.c gtktogglebutton.c \
	gtkvscrollbar.c gtkaccellabel.c gtkclist.c gtkfontsel.c gtkitem.c \
	gtkobject.c gtkscrollbar.c gtktoolbar.c gtkvseparator.c gtkadjustment.c \
	gtkcolorsel.c gtkframe.c gtkitemfactory.c gtkoptionmenu.c \
	gtkscrolledwindow.c gtktooltips.c gtkwidget.c gtkalignment.c gtkcombo.c \
	gtkgamma.c gtklabel.c gtkpacker.c gtkselection.c gtktree.c gtkwindow.c \
	gtkarg.c gtkcontainer.c gtkgc.c gtklayout.c gtkpaned.c gtkseparator.c \
	gtktreeitem.c simple.c gtkargcollector.c gtkctree.c gtkhandlebox.c \
	gtklist.c gtkpixmap.c gtksignal.c gtkarrow.c \
	gtkcurve.c gtkhbbox.c gtklistitem.c gtkplug.c gtksocket.c \
	gtkaspectframe.c gtkdata.c gtkhbox.c gtkmain.c \
	gtkpreview.c gtkspinbutton.c gtkbbox.c \
	gtkdialog.c gtkhpaned.c gtkmarshal.c gtkprogressbar.c gtkstatusbar.c \
	gtktypeutils.c gtkbin.c gtkdnd.c gtkhruler.c gtkmenubar.c gtkprogress.c \
	gtkstyle.c gtkvbbox.c gtkbindings.c gtkdrawingarea.c gtkhscale.c gtkmenu.c \
	gtkradiobutton.c gtktable.c gtkvbox.c gtkbox.c gtkeditable.c gtkhscrollbar.c \
	gtkmenufactory.c gtkradiomenuitem.c gtktearoffmenuitem.c gtkviewport.c \
	gtkbutton.c gtkentry.c gtkhseparator.c gtkmenuitem.c gtkrange.c gtktext.c \
	gtkvpaned.c gtkcalendar.c gtkeventbox.c gtkimage.c gtkmenushell.c gtkrc.c \
	gtkthemes.c gtkvruler.c


CPP_SRCS =

OBJS = $(C_SRCS:.c=.o) $(CPP_SRCS:.cpp=.o)
DEPS = $(C_SRCS:.c=.d) $(CPP_SRCS:.cpp=.d)

CC? = gcc
CXX? = g++
AR? = ar
RM = rm -f

DEFINES =
INTERNAL_CFLAGS = -I. -I../glib -I../include -I../ $(DEFINES) -I../gdk
CPPFLAGS += -MMD -MP -MT $@

LIB = libdrsgtk
STATIC_LIB = $(LIB).a
SHARED_LIB = $(LIB).so
CFLAGS = -Wall -O2

ifeq ($(OS),Windows_NT)
	CC = gcc
	ifeq ($(SHELL),sh.exe)
		# Executing under cmd.exe
		RM = del /Q 2>NUL
	endif
else
	CFLAGS += -fPIC
endif

all: $(SHARED_LIB)

$(STATIC_LIB): $(OBJS)
	$(RM) $(LIB)
	$(AR) rc $(LIB) $(OBJS)
	ranlib $(LIB)

$(SHARED_LIB): $(OBJS)
	$(CC) -shared -o $@ $(OBJS) $(LD_FLAGS)

.c.o:
	$(CC) $(CFLAGS) $(INTERNAL_CFLAGS) $(CPPFLAGS) -o $@ -c $<

.cpp.o:
	$(CXX) $(CFLAGS) $(INTERNAL_CFLAGS) $(CPPFLAGS) -o $@ -c $<

clean:
	$(RM) $(OBJS)
	$(RM) $(DEPS)
	$(RM) $(SHARED_LIB)

# Include automatically generated dependency files
-include $(DEPS)
