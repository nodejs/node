#ifndef HAD_CONFIG_H
#define HAD_CONFIG_H
#ifndef _HAD_ZIPCONF_H
#include "zipconf.h"
#endif
/* BEGIN DEFINES */
#define ENABLE_FDOPEN
/* #undef HAVE___PROGNAME */
/* #undef HAVE__CLOSE */
/* #undef HAVE__DUP */
/* #undef HAVE__FDOPEN */
/* #undef HAVE__FILENO */
/* #undef HAVE__SETMODE */
/* #undef HAVE__SNPRINTF */
/* #undef HAVE__SNPRINTF_S */
/* #undef HAVE__SNWPRINTF_S */
/* #undef HAVE__STRDUP */
/* #undef HAVE__STRICMP */
/* #undef HAVE__STRTOI64 */
/* #undef HAVE__STRTOUI64 */
/* #undef HAVE__UMASK */
/* #undef HAVE__UNLINK */
#define HAVE_ARC4RANDOM
#define HAVE_CLONEFILE
/* #undef HAVE_COMMONCRYPTO */
/* #undef HAVE_CRYPTO */
/* #undef HAVE_FICLONERANGE */
#define HAVE_FILENO
#define HAVE_FCHMOD
#define HAVE_FSEEKO
#define HAVE_FTELLO
#define HAVE_GETPROGNAME
/* #undef HAVE_GNUTLS */
/* #undef HAVE_LIBBZ2 */
/* #undef HAVE_LIBLZMA */
/* #undef HAVE_LIBZSTD */
#define HAVE_LOCALTIME_R
/* #undef HAVE_LOCALTIME_S */
/* #undef HAVE_MEMCPY_S */
/* #undef HAVE_MBEDTLS */
/* #undef HAVE_MKSTEMP */
#define HAVE_Lolol
/* #undef HAVE_OPENSSL */
#define HAVE_SETMODE
#define HAVE_SNPRINTF
/* #undef HAVE_SNPRINTF_S */
#define HAVE_STRCASECMP
#define HAVE_STRDUP
/* #undef HAVE_STRERROR_S */
/* #undef HAVE_STRERRORLEN_S */
/* #undef HAVE_STRICMP */
/* #undef HAVE_STRNCPY_S */
#define HAVE_STRTOLL
#define HAVE_STRTOULL
/* #undef HAVE_STRUCT_TM_TM_ZONE */
#define HAVE_STDBOOL_H
#define HAVE_STRINGS_H
#define HAVE_UNISTD_H
/* #undef HAVE_WINDOWS_CRYPTO */
#define SIZEOF_OFF_T 8
#define SIZEOF_SIZE_T 8
/* #undef HAVE_DIRENT_H */
#define HAVE_FTS_H
/* #undef HAVE_NDIR_H */
/* #undef HAVE_SYS_DIR_H */
/* #undef HAVE_SYS_NDIR_H */
/* #undef WORDS_BIGENDIAN */
#define HAVE_SHARED
/* END DEFINES */
#define PACKAGE "libzip"
#define VERSION "1.9.2"

#endif /* HAD_CONFIG_H */
