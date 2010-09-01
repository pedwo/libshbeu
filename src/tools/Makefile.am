## Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_builddir) \
           -I$(top_srcdir)/include

SHBEUDIR = ../libshbeu
SHBEU_LIBS = $(SHBEUDIR)/libshbeu.la

if HAVE_NCURSES
ncurses_lib = -lncurses
endif

bin_PROGRAMS = shbeu-display

noinst_HEADERS = display.h

shbeu_display_SOURCES = shbeu-display.c display.c
shbeu_display_LDADD = $(SHBEU_LIBS) $(UIOMUX_LIBS) $(ncurses_lib) -lrt
