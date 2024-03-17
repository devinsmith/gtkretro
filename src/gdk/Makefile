# Makefile for version of libdrsgdk

.PHONY: all clean

C_SRCS = gdk.c gdkdraw.c gdkimage.c gdkrectangle.c gdkwindow.c \
	gdkevents.c gdkregion.c gdkxid.c \
	gdkcolor.c gdkfont.c gdkinput.c gdkrgb.c gxid.c \
	gdkcursor.c gdkgc.c gdkpixmap.c gdkselection.c gxid_lib.c \
	gdkdnd.c gdkglobals.c gdkproperty.c gdkvisual.c

CPP_SRCS =

OBJS = $(C_SRCS:.c=.o) $(CPP_SRCS:.cpp=.o)
DEPS = $(C_SRCS:.c=.d) $(CPP_SRCS:.cpp=.d)

CC? = gcc
CXX? = g++
AR? = ar
RM = rm -f

INTERNAL_CFLAGS = -I. -I../glib -I../include -I../include/gdk -DUSE_SHM
CPPFLAGS += -MMD -MP -MT $@

LIB = libdrsgdk
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
