AM_CFLAGS= -Wall $(FNVUM_DEPS_CFLAGS) $(FNPOM_DEPS_CFLAGS) -g
AM_LDFLAGS=

bin_PROGRAMS = fnctl fnvum fnpom fnweb
lib_LTLIBRARIES = libfn.la
include_HEADERS = libfn.h

libfn_la_SOURCES = libfn.c

fnctl_SOURCES = fnctl.c
fnctl_LDADD = -lfn

fnpom_SOURCES = fnpom.c
fnpom_LDADD = -lfn $(FNPOM_DEPS_LIBS)

fnvum_SOURCES = fnvum.c
fnvum_LDADD = -lfn $(FNVUM_DEPS_LIBS)

fnweb_SOURCES = fnweb.c
fnweb_LDADD = -lfn -lrt $(FNWEB_DEPS_LIBS)
