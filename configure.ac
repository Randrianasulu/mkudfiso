dnl Run this with autoconf to produce a configure script
AC_INIT(mkudfiso, 0.1, i_like_pie@nerdgrounds.com)
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_SRCDIR(src)
AC_PROG_RANLIB(ranlib)
AC_PROG_CC(g++)
AC_PROG_CXX(g++)

AC_DEFINE(_FILE_OFFSET_BITS,64)
AC_DEFINE(__USE_FILE_OFFSET64,1)
AC_DEFINE(__USE_LARGEFILE64,1)
AC_DEFINE(LINUX,1)
AC_DEFINE(__J4_INTERNAL,1)
AC_CHECK_FUNCS(lseek64 open64 ftruncate64)
AC_CHECK_FUNCS(_lseeki64)

AM_INIT_AUTOMAKE

AC_PROG_CXX

AC_OUTPUT(Makefile src/Makefile)

