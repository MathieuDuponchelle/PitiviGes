dnl Perform a check for a feature for GStreamer
dnl Richard Boulton <richard-alsa@tartarus.org>
dnl Last modification: 25/06/2001
dnl GNL_CHECK_FEATURE(FEATURE-NAME, FEATURE-DESCRIPTION,
dnl                   DEPENDENT-PLUGINS, TEST-FOR-FEATURE,
dnl                   DISABLE-BY-DEFAULT, ACTION-IF-USE, ACTION-IF-NOTUSE)
dnl
dnl This macro adds a command line argument to enable the user to enable
dnl or disable a feature, and if the feature is enabled, performs a supplied
dnl test to check if the feature is available.
dnl
dnl The test should define HAVE_<FEATURE-NAME> to "yes" or "no" depending
dnl on whether the feature is available.
dnl
dnl The macro will set USE_<<FEATURE-NAME> to "yes" or "no" depending on
dnl whether the feature is to be used.
dnl
dnl The macro will call AM_CONDITIONAL(USE_<<FEATURE-NAME>, ...) to allow
dnl the feature to control what is built in Makefile.ams.  If you want
dnl additional actions resulting from the test, you can add them with the
dnl ACTION-IF-USE and ACTION-IF-NOTUSE parameters.
dnl 
dnl FEATURE-NAME        is the name of the feature, and should be in
dnl                     purely upper case characters.
dnl FEATURE-DESCRIPTION is used to describe the feature in help text for
dnl                     the command line argument.
dnl DEPENDENT-PLUGINS   lists any plugins which depend on this feature.
dnl TEST-FOR-FEATURE    is a test which sets HAVE_<FEATURE-NAME> to "yes"
dnl                     or "no" depending on whether the feature is
dnl                     available.
dnl DISABLE-BY-DEFAULT  if "disabled", the feature is disabled by default,
dnl                     if any other value, the feature is enabled by default.
dnl ACTION-IF-USE       any extra actions to perform if the feature is to be
dnl                     used.
dnl ACTION-IF-NOTUSE    any extra actions to perform if the feature is not to
dnl                     be used.
dnl
AC_DEFUN(GNL_CHECK_FEATURE,
[dnl
builtin(define, [gst_endisable], ifelse($5, [disabled], [enable], [disable]))dnl
AC_ARG_ENABLE(translit([$1], A-Z, a-z),
  [  ]builtin(format, --%-26s gst_endisable %s, gst_endisable-translit([$1], A-Z, a-z), [$2]ifelse([$3],,,: [$3])),
  [ case "${enableval}" in
      yes) USE_[$1]=yes ;;
      no) USE_[$1]=no ;;
      *) AC_MSG_ERROR(bad value ${enableval} for --enable-translit([$1], A-Z, a-z)) ;;
    esac],
  [ USE_$1=]ifelse($5, [disabled], [no], [yes]))           dnl DEFAULT

dnl *** If it's enabled
if test x$USE_[$1] = xyes; then
  gst_check_save_LIBS=$LIBS
  gst_check_save_LDFLAGS=$LDFLAGS
  gst_check_save_CFLAGS=$CFLAGS
  gst_check_save_CPPFLAGS=$CPPFLAGS
  gst_check_save_CXXFLAGS=$CXXFLAGS
  HAVE_[$1]=no
  $4
  LIBS=$gst_check_save_LIBS
  LDFLAGS=$gst_check_save_LDFLAGS
  CFLAGS=$gst_check_save_CFLAGS
  CPPFLAGS=$gst_check_save_CPPFLAGS
  CXXFLAGS=$gst_check_save_CXXFLAGS

  dnl If it isn't found, unset USE_[$1]
  if test x$HAVE_[$1] = xno; then
    USE_[$1]=no
  fi
fi
dnl *** Warn if it's disabled or not found
if test x$USE_[$1] = xyes; then
  ifelse([$6], , :, [$6])
else
  ifelse([$3], , :, [AC_MSG_WARN(
***** NOTE: These plugins won't be built: [$3]
)])
  ifelse([$7], , :, [$7])
fi
dnl *** Define the conditional as appropriate
AM_CONDITIONAL(USE_[$1], test x$USE_[$1] = xyes)
])

dnl Use a -config program which accepts --cflags and --libs parameters
dnl to set *_CFLAGS and *_LIBS and check existence of a feature.
dnl Richard Boulton <richard-alsa@tartarus.org>
dnl Last modification: 26/06/2001
dnl GNL_CHECK_CONFIGPROG(FEATURE-NAME, CONFIG-PROG-FILENAME, MODULES)
dnl
dnl This check was written for GStreamer: it should be renamed and checked
dnl for portability if you decide to use it elsewhere.
dnl
AC_DEFUN(GNL_CHECK_CONFIGPROG,
[
  AC_PATH_PROG([$1]_CONFIG, [$2], no)
  if test x$[$1]_CONFIG = xno; then
    [$1]_LIBS=
    [$1]_CFLAGS=
    HAVE_[$1]=no
  else
    [$1]_LIBS=`[$2] --libs [$3]`
    [$1]_CFLAGS=`[$2] --cflags [$3]`
    HAVE_[$1]=yes
  fi
  AC_SUBST([$1]_LIBS)
  AC_SUBST([$1]_CFLAGS)
])


dnl sidplay stuff (taken from xmms)
AC_DEFUN(AC_FIND_FILE,
[
  $3=NO
  for i in $2; do
    for j in $1; do
      if test -r "$i/$j"; then
        $3=$i
        break 2
      fi
    done
  done
])

