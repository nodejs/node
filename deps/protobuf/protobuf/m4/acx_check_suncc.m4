dnl Check for the presence of the Sun Studio compiler.
dnl If Sun Studio compiler is found, set appropriate flags.
dnl Additionally, Sun Studio doesn't default to 64-bit by itself,
dnl nor does it automatically look in standard Solaris places for
dnl 64-bit libs, so we must add those options and paths to the search
dnl paths.

dnl TODO(kenton):  This is pretty hacky.  It sets CXXFLAGS, which the autoconf
dnl docs say should never be overridden except by the user.  It also isn't
dnl cross-compile safe.  We should fix these problems, but since I don't have
dnl Sun CC at my disposal for testing, someone else will have to do it.

AC_DEFUN([ACX_CHECK_SUNCC],[

  AC_LANG_PUSH([C++])
  AC_CHECK_DECL([__SUNPRO_CC], [SUNCC="yes"], [SUNCC="no"])
  AC_LANG_POP()


  AC_ARG_ENABLE([64bit-solaris],
    [AS_HELP_STRING([--disable-64bit-solaris],
      [Build 64 bit binary on Solaris @<:@default=on@:>@])],
             [ac_enable_64bit="$enableval"],
             [ac_enable_64bit="yes"])

  AS_IF([test "$SUNCC" = "yes" -a "x${ac_cv_env_CXXFLAGS_set}" = "x"],[
    dnl Sun Studio has a crashing bug with -xO4 in some cases. Keep this
    dnl at -xO3 until a proper test to detect those crashes can be done.
    CXXFLAGS="-g0 -xO3 -xlibmil -xdepend -xbuiltin -mt -template=no%extdef ${CXXFLAGS}"
  ])

  case $host_os in
    *solaris*)
      AC_CHECK_PROGS(ISAINFO, [isainfo], [no])
      AS_IF([test "x$ISAINFO" != "xno"],
            [isainfo_b=`${ISAINFO} -b`],
            [isainfo_b="x"])

      AS_IF([test "$isainfo_b" != "x"],[

        isainfo_k=`${ISAINFO} -k`

        AS_IF([test "x$ac_enable_64bit" = "xyes"],[

          AS_IF([test "x$libdir" = "x\${exec_prefix}/lib"],[
           dnl The user hasn't overridden the default libdir, so we'll
           dnl the dir suffix to match solaris 32/64-bit policy
           libdir="${libdir}/${isainfo_k}"
          ])

          dnl This should just be set in CPPFLAGS and in LDFLAGS, but libtool
          dnl does the wrong thing if you don't put it into CXXFLAGS. sigh.
          dnl (It also needs it in CFLAGS, or it does a different wrong thing!)
          CXXFLAGS="${CXXFLAGS} -m64"
          ac_cv_env_CXXFLAGS_set=set
          ac_cv_env_CXXFLAGS_value='-m64'

          CFLAGS="${CFLAGS} -m64"
          ac_cv_env_CFLAGS_set=set
          ac_cv_env_CFLAGS_value='-m64'

          AS_IF([test "$target_cpu" = "sparc" -a "x$SUNCC" = "xyes" ],[
            CXXFLAGS="-xmemalign=8s ${CXXFLAGS}"
          ])
        ])
      ])
    ;;
  esac

  AS_IF([test "$target_cpu" = "sparc" -a "x$SUNCC" = "xyes" ],[
    CXXFLAGS="-xregs=no%appl ${CXXFLAGS}"
  ])
])
