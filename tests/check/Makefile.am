include $(top_srcdir)/common/check.mak

CHECK_REGISTRY = $(top_builddir)/tests/check/test-registry.xml

REGISTRY_ENVIRONMENT = \
	GST_REGISTRY_1_0=$(CHECK_REGISTRY)

# Note: GST_PLUGINS_GOOD_DIR is only set in an uninstalled setup
TESTS_ENVIRONMENT = \
        CK_DEFAULT_TIMEOUT=120                                   \
	$(REGISTRY_ENVIRONMENT)					\
	GST_PLUGIN_SYSTEM_PATH_1_0=					\
	GST_PLUGIN_PATH_1_0=$(top_builddir)/gnl:$(GST_PLUGINS_GOOD_DIR):$(GST_PLUGINS_BASE_DIR):$(GST_PLUGINS_DIR):$(GST_PLUGINS_BAD_DIR)

# ths core dumps of some machines have PIDs appended
CLEANFILES = core.* test-registry.xml

common_cflags=-I$(top_srcdir) $(GST_PLUGINS_BASE_CFLAGS) $(GST_OBJ_CFLAGS) \
	$(GST_CHECK_CFLAGS) $(GST_OPTION_CFLAGS) $(GST_CFLAGS)
common_ldadd= $(GST_PLUGINS_BASE_LIBS) \
	$(GST_OBJ_LIBS) $(GST_CHECK_LIBS) $(CHECK_LIBS)

testutils_noisnt_libraries=libtestutils.la
testutils_noinst_headers=gnl/common.h
libtestutils_la_LIBADD=$(common_ldadd)
libtestutils_la_CFLAGS=$(common_cflags)
libtestutils_la_SOURCES=gnl/common.c

clean-local: clean-local-check

noinst_LTLIBRARIES=$(testutils_noisnt_libraries)
noinst_HEADERS=$(testutils_noinst_headers)

TESTS = $(check_PROGRAMS)

AM_CFLAGS =  $(common_cflags) -UG_DISABLE_ASSERT -UG_DISABLE_CAST_CHECKS -DGST_USE_UNSTABLE_API
LDADD = $(common_ldadd) libtestutils.la

check_PROGRAMS =	\
	gnl/simple	\
	gnl/complex	\
	gnl/gnlsource	\
	gnl/gnloperation	\
	gnl/gnlcomposition

# these tests don't even pass
noinst_PROGRAMS =

SUPPRESSIONS = $(top_srcdir)/common/gst.supp
