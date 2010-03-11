AM_CPPFLAGS = -I$(top_builddir)/src/lib -I$(top_builddir)/src/bin -I$(top_srcdir)/ext

CLEANFILES = *.gcno *.gcda

TESTS =
if HAVE_GTEST
TESTS += run_unittests
run_unittests_SOURCES = $(top_srcdir)/src/lib/dns/tests/unittest_util.h
run_unittests_SOURCES += $(top_srcdir)/src/lib/dns/tests/unittest_util.cc
run_unittests_SOURCES += ../auth_srv.h ../auth_srv.cc
run_unittests_SOURCES += auth_srv_unittest.cc
run_unittests_SOURCES += run_unittests.cc
run_unittests_CPPFLAGS = $(AM_CPPFLAGS) $(GTEST_INCLUDES)
run_unittests_LDFLAGS = $(AM_LDFLAGS) $(GTEST_LDFLAGS)
run_unittests_LDADD = $(GTEST_LDADD)
run_unittests_LDADD += $(SQLITE_LIBS)
run_unittests_LDADD +=  $(top_builddir)/src/lib/auth/.libs/libauth.a
run_unittests_LDADD +=  $(top_builddir)/src/lib/dns/.libs/libdns.a
run_unittests_LDADD += $(top_builddir)/src/lib/config/libcfgclient.a
run_unittests_LDADD += $(top_builddir)/src/lib/cc/libcc.a
run_unittests_LDADD += $(top_builddir)/src/lib/exceptions/.libs/libexceptions.a
if HAVE_BOOSTLIB
run_unittests_LDFLAGS += $(BOOST_LDFLAGS)
run_unittests_LDADD += $(BOOST_SYSTEM_LIB)
endif
endif

noinst_PROGRAMS = $(TESTS)