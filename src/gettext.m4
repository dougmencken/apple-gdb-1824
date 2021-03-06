# This file is derived from `gettext.m4'.  The difference is that the
# included macros assume Cygnus-style source and build trees.

# Macro to add for using GNU gettext.
# Ulrich Drepper <drepper@cygnus.com>, 1995.
#
# This file file be copied and used freely without restrictions.  It can
# be used in projects which are not available under the GNU Public License
# but which still want to provide support for the GNU gettext
# functionality.
# Please note that the actual code is *not* freely available.

#serial 3

AC_DEFUN([CY_WITH_NLS],
  [AC_PREREQ([2.59])dnl
   AC_MSG_CHECKING([whether NLS is requested])

    dnl# Default is enabled NLS:
    AC_ARG_ENABLE([nls],
      [AS_HELP_STRING([--disable-nls],
                      [do not use Native Language Support])],
      [USE_NLS=${enableval}],[USE_NLS=yes])dnl# End AC_ARG_ENABLE
    AC_MSG_RESULT([${USE_NLS}])
    AC_SUBST([USE_NLS])dnl

    USE_INCLUDED_LIBINTL=yes

    dnl# If we use NLS figure out what method:
    if test "x${USE_NLS}" = "xyes"; then
      AC_DEFINE([ENABLE_NLS],[1],[Define to 1 if NLS is requested])
      AC_MSG_CHECKING([whether included gettext is requested])
      AC_ARG_WITH([included-gettext],
        [AS_HELP_STRING([--with-included-gettext],
                        [use the GNU gettext library included here])],
        [nls_cv_force_use_gnu_gettext=${withval}],
        [nls_cv_force_use_gnu_gettext=no])
      AC_MSG_RESULT([${nls_cv_force_use_gnu_gettext}])
      AC_SUBST([nls_cv_force_use_gnu_gettext])dnl

      nls_cv_use_gnu_gettext="${nls_cv_force_use_gnu_gettext}"
      AC_SUBST([nls_cv_use_gnu_gettext])
      if test "x${nls_cv_force_use_gnu_gettext}" != "xyes"; then
        dnl# User does not insist on using GNU NLS library. Figure out what
        dnl# to use. If gettext or catgets are available (in this order) we
        dnl# use this. Else we have to fall back to GNU NLS library.
        dnl# catgets is only used if permitted by option --with-catgets.
	nls_cv_header_intl=""
	nls_cv_header_libgt=""
	CATOBJEXT=NONE

	AC_CHECK_HEADER([libintl.h],
	  [AC_CACHE_CHECK([for gettext in libc],[gt_cv_func_gettext_libc],
	    [AC_LINK_IFELSE([AC_LANG_SOURCE([[#include <libintl.h>]],[[return (int) gettext ("")]])],
	       [gt_cv_func_gettext_libc=yes],[gt_cv_func_gettext_libc=no])])dnl# End AC_LINK_IFELSE

	   if test "x${gt_cv_func_gettext_libc}" != "xyes"; then
	     AC_CHECK_LIB([intl],[bindtextdomain],
	       [AC_CACHE_CHECK([for gettext in libintl],
		 [gt_cv_func_gettext_libintl],
		 [AC_LINK_IFELSE([AC_LANG_SOURCE([[]],[[return (int) gettext ("")]])],
		 [gt_cv_func_gettext_libintl=yes],
		 [gt_cv_func_gettext_libintl=no])])])dnl# End AC_LINK_IFELSE
	   fi dnl# End gt_cv_func_gettext_libc check

	   if test "x${gt_cv_func_gettext_libc}" = "xyes" \
	      || test "x${gt_cv_func_gettext_libintl}" = "xyes"; then
	      AC_DEFINE([HAVE_GETTEXT],[1],
			[Define as 1 if you have gettext and do NOT want to use GNU gettext.])
	      AM_PATH_PROG_WITH_TEST([MSGFMT],[msgfmt],
		[test -z "`$ac_dir/$ac_word -h 2>&1 | grep 'dv '`"],[:])dnl
	      if test "$MSGFMT" != ":"; then
		AC_CHECK_FUNCS([dcgettext])
		AC_PATH_PROG([GMSGFMT],[gmsgfmt],[$MSGFMT])
		AM_PATH_PROG_WITH_TEST([XGETTEXT],[xgettext],
		  [test -z "`${ac_dir}/${ac_word} -h 2>&1 | grep '(HELP)'`"],[:])
		AC_LINK_IFELSE([AC_LANG_SOURCE([[#define NONEMPTY_FIELD]],[[extern int _nl_msg_cat_cntr;
			       return _nl_msg_cat_cntr]])],
		  [CATOBJEXT=.gmo
		   DATADIRNAME=share],
		  [CATOBJEXT=.mo
		   DATADIRNAME=lib])
		INSTOBJEXT=.mo
	      fi dnl# end MSGFMT check
	    fi dnl# end gt_cv_func_gettext_libc check
	])dnl# end AC_CHECK_HEADER

        dnl# In the standard gettext, we would now check for catgets.
        dnl# However, we never want to use catgets for our releases.

        if test "x${CATOBJEXT}" = "xNONE"; then
	  dnl# Neither gettext nor catgets in included in the C library.
	  dnl# Fall back on GNU gettext library.
	  nls_cv_use_gnu_gettext=yes
	  AC_SUBST([nls_cv_use_gnu_gettext])
        fi dnl# end CATOBJEXT check
      fi dnl# end nls_cv_force_use_gnu_gettext check

      if test "x${nls_cv_use_gnu_gettext}" = "xyes"; then
        dnl# Mark actions used to generate GNU NLS library.
        INTLOBJS="\$(GETTOBJS)"
        AM_PATH_PROG_WITH_TEST([MSGFMT],[msgfmt],
	  [test -z "`${ac_dir}/${ac_word} -h 2>&1 | grep 'dv '`"],[:])
        AC_PATH_PROG([GMSGFMT],[gmsgfmt],[$MSGFMT])
        AM_PATH_PROG_WITH_TEST([XGETTEXT],[xgettext],
	  [test -z "`${ac_dir}/${ac_word} -h 2>&1 | grep '(HELP)'`"],[:])
        AC_SUBST([MSGFMT])
	USE_INCLUDED_LIBINTL=yes
        CATOBJEXT=.gmo
        INSTOBJEXT=.mo
        DATADIRNAME=share
# APPLE LOCAL: Link directly to the .a file.
	INTLDEPS='$(top_builddir)/../intl/.libs/libintl.a'
	INTLLIBS=$INTLDEPS
	LIBS=`echo ${LIBS} | sed -e 's/-lintl//'`
        nls_cv_header_intl=libintl.h
        nls_cv_header_libgt=libgettext.h
      fi dnl# end nls_cv_use_gnu_gettext check

      dnl# Test whether we really found GNU xgettext.
      if test "x${XGETTEXT}" != "x:"; then
      dnl# If it is no GNU xgettext we define it as : so that the
      dnl# Makefiles still can work.
	if ${XGETTEXT} --omit-header /dev/null 2> /dev/null; then
	  : ;
	else
	  AC_MSG_RESULT([found xgettext programs is not GNU xgettext; ignore it])
	  XGETTEXT=":"
	fi dnl# end XGETTEXT inner check
      fi dnl# end XGETTEXT outer check

      # We need to process the po/ directory.
      POSUB=po
    else
      DATADIRNAME=share
      nls_cv_header_intl=libintl.h
      nls_cv_header_libgt=libgettext.h
    fi dnl# end USE_NLS check

    # If this is used in GNU gettext we have to set USE_NLS to `yes'
    # because some of the sources are only built for this goal.
    if test "x${PACKAGE}" = "xgettext"; then
      USE_NLS=yes
      USE_INCLUDED_LIBINTL=yes
    fi dnl# end PACKAGE check

    dnl# These rules are solely for the distribution goal. While doing this
    dnl# we only have to keep exactly one list of the available catalogs
    dnl# in configure.ac.
    for lang in ${ALL_LINGUAS}; do
      GMOFILES="${GMOFILES} ${lang}.gmo"
      POFILES="${POFILES} ${lang}.po"
    done dnl# end ALL_LIGNUAS check

    dnl# Make all variables we use known to autoconf.
    AC_SUBST([USE_INCLUDED_LIBINTL])dnl
    AC_SUBST([CATALOGS])dnl
    AC_SUBST([CATOBJEXT])dnl
    AC_SUBST([DATADIRNAME])dnl
    AC_SUBST([GMOFILES])dnl
    AC_SUBST([INSTOBJEXT])dnl
    AC_SUBST([INTLDEPS])dnl
    AC_SUBST([INTLLIBS])dnl
    AC_SUBST([INTLOBJS])dnl
    AC_SUBST([POFILES])dnl
    AC_SUBST([POSUB])dnl
  ])dnl# end CY_WITH_NLS

dnl# check for LC_MESSAGES has been moved to a separate file:
sinclude(lcmessage.m4)

AC_DEFUN([CY_GNU_GETTEXT],
  [AC_REQUIRE([AC_PROG_MAKE_SET])dnl
   AC_REQUIRE([AC_PROG_CC])dnl
   AC_REQUIRE([AC_PROG_RANLIB])dnl
   AC_REQUIRE([AC_HEADER_STDC])dnl
   AC_REQUIRE([AC_C_CONST])dnl
   AC_REQUIRE([AC_C_INLINE])dnl
   AC_REQUIRE([AC_TYPE_OFF_T])dnl
   AC_REQUIRE([AC_TYPE_SIZE_T])dnl
   AC_REQUIRE([AC_FUNC_ALLOCA])dnl
   AC_REQUIRE([AC_FUNC_MMAP])dnl

   AC_CHECK_HEADERS_ONCE([argz.h limits.h locale.h nl_types.h malloc.h \
                          string.h unistd.h values.h sys/param.h])
   AC_CHECK_FUNCS([getcwd munmap putenv setenv setlocale strchr \
                   strcasecmp __argz_count __argz_stringify __argz_next])

   if test "${ac_cv_func_stpcpy+set}" != "set"; then
     AC_CHECK_FUNCS([stpcpy])
   fi
   if test "${ac_cv_func_stpcpy}" = "yes"; then
     AC_DEFINE([HAVE_STPCPY],[1],[Define if you have the stpcpy function])
   fi

   AC_REQUIRE([AM_LC_MESSAGES])dnl
   AC_REQUIRE([CY_WITH_NLS])dnl

   if test "x${CATOBJEXT}" != "x"; then
     if test "x${ALL_LINGUAS}" = "x"; then
       LINGUAS=""
     else
       AC_MSG_CHECKING([for catalogs to be installed])
       NEW_LINGUAS=""
       for lang in ${LINGUAS=${ALL_LINGUAS}}; do
         case "${ALL_LINGUAS}" in
          *${lang}*) NEW_LINGUAS="${NEW_LINGUAS} ${lang}" ;;
         esac
       done
       LINGUAS=${NEW_LINGUAS}
       AC_MSG_RESULT([${LINGUAS}])
     fi

     dnl# Construct list of names of catalog files to be constructed.
     if test -n "${LINGUAS}"; then
       for lang in ${LINGUAS}; do
         CATALOGS="${CATALOGS} ${lang}${CATOBJEXT}"
       done
     fi
   fi

   dnl# The reference to <locale.h> in the installed <libintl.h> file
   dnl# must be resolved because we cannot expect the users of this
   dnl# to define HAVE_LOCALE_H.
   if test "x${ac_cv_header_locale_h}" = "xyes"; then
     INCLUDE_LOCALE_H="#include <locale.h>"
   else
     INCLUDE_LOCALE_H="\
/* The system does not provide the header <locale.h>. Take care yourself.  */"
   fi
   AC_SUBST([INCLUDE_LOCALE_H])dnl

   dnl# Determine which catalog format we have (if any is needed)
   dnl# For now we know about two different formats:
   dnl#   Linux libc-5 and the normal X/Open format
   if test -f ${srcdir}/po2tbl.sed.in; then
      if test "x${CATOBJEXT}" = "x.cat"; then
	 AC_CHECK_HEADER([linux/version.h],
                         [msgformat=linux],[msgformat=xopen])dnl

	 dnl# Transform the SED scripts while copying because some dumb
	 dnl# SEDs cannot handle comments.
	 sed -e '/^#/d' ${srcdir}/${msgformat}-msg.sed > po2msg.sed
      fi
      dnl# po2tbl.sed is always needed.
      sed -e '/^#.*[^\\]$/d' -e '/^#$/d' \
	 ${srcdir}/po2tbl.sed.in > po2tbl.sed
   fi

   dnl# In the intl/Makefile.in we have a special dependency which makes
   dnl# only sense for gettext. We comment this out for non-gettext
   dnl# packages.
   if test "x${PACKAGE}" = "xgettext"; then
     GT_NO="#NO#"
     GT_YES=""
   else
     GT_NO=""
     GT_YES="#YES#"
   fi
   AC_SUBST([GT_NO])dnl
   AC_SUBST([GT_YES])dnl

   MKINSTALLDIRS="\$(srcdir)/../../mkinstalldirs"
   AC_SUBST([MKINSTALLDIRS])dnl

   dnl# *** For now the libtool support in intl/Makefile is not for real.
   l=l
   AC_SUBST([l])dnl

   dnl# Generate list of files to be processed by xgettext which will
   dnl# be included in po/Makefile. But only do this if the po directory
   dnl# exists in srcdir and contains POTFILES.in.
   if test -f ${srcdir}/po/POTFILES.in; then
      test -d po || mkdir po
      if test "x${srcdir}" != "x."; then
	 if test "x`echo ${srcdir} | sed 's@/.*@@'`" = "x"; then
	    posrcprefix="${srcdir}/"
	 else
	    posrcprefix="../${srcdir}/"
	 fi
      else
	 posrcprefix="../"
      fi
      rm -f po/POTFILES
      sed -e "/^#/d" -e "/^\$/d" -e "s,.*,	$posrcprefix& \\\\," -e "\$s/\(.*\) \\\\/\1/" \
	 < ${srcdir}/po/POTFILES.in > po/POTFILES
   fi
  ])dnl

dnl# progtest macro has been moved to a separate file:
sinclude(progtest.m4)

dnl# (inclusion of lcmessage.m4 has been moved to above)

dnl# EOF
