# Empty TraceDaemon Makefile.am. Insert a real one here.

LIBS += $(THREAD_LIBS)

bin_PROGRAMS = lttd

lttd_SOURCES = lttd.c

lttd_DEPENDENCIES = ../liblttd/liblttd.la
lttd_LDADD = $(lttd_DEPENDENCIES)

