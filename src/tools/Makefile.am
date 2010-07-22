## Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_builddir) \
           -I$(top_srcdir)/include

SHBEUDIR = ../libshbeu
SHBEU_LIBS = $(SHBEUDIR)/libshbeu.la

if HAVE_NCURSES
ncurses_programs = shbeu-display
endif

bin_PROGRAMS = $(ncurses_programs)

shbeu_display_SOURCES = shbeu-display.c display.c
shbeu_display_LDADD = $(SHBEU_LIBS) $(UIOMUX_LIBS) -lncurses
