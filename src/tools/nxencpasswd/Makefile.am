bin_PROGRAMS = nxencpasswd
nxencpasswd_SOURCES = nxencpasswd.cpp
nxencpasswd_CPPFLAGS = -I@top_srcdir@/include
nxencpasswd_LDFLAGS = -I@top_srcdir@/include @EXEC_LDFLAGS@
nxencpasswd_LDADD = ../../libnetxms/libnetxms.la @EXEC_LIBS@

if USE_INTERNAL_LIBTRE
nxencpasswd_LDADD += ../../libtre/libnxtre.la
endif

EXTRA_DIST = \
	nxencpasswd.vcxproj nxencpasswd.vcxproj.filters \
	Makefile.w32
