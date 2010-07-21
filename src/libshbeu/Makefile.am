## Process this file with automake to produce Makefile.in

INCLUDES = $(INCLTDL) \
           -I$(top_builddir) \
           -I$(top_srcdir)/include

EXTRA_DIST = Version_script.in

AM_CFLAGS =

# Libraries to build
lib_LTLIBRARIES = libshbeu.la

noinst_HEADERS = shbeu_regs.h

libshbeu_la_SOURCES = \
	beu.c

libshbeu_la_CFLAGS = -v -Wall -O2 -I $(srcdir) -fPIC -fno-common
libshbeu_la_LDFLAGS = -version-info @SHARED_VERSION_INFO@ @SHLIB_VERSION_ARG@
libshbeu_la_LIBADD =
