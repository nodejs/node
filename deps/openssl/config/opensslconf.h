#undef OPENSSL_LINUX
#if defined(__linux) && !defined(__ANDROID__)
# define OPENSSL_LINUX 1
#endif

#if defined(OPENSSL_NO_ASM)
# include "./opensslconf_no-asm.h"
#else
# include "./opensslconf_asm.h"
#endif

/* GOST is not included in all platform */
#ifndef OPENSSL_NO_GOST
# define OPENSSL_NO_GOST
#endif
/* HW_PADLOCK is not included in all platform */
#ifndef OPENSSL_NO_HW_PADLOCK
# define OPENSSL_NO_HW_PADLOCK
#endif
/* musl in Alpine Linux does not support getcontext etc.*/
#if defined(OPENSSL_LINUX) && !defined(__GLIBC__) && !defined(__clang__)
# define OPENSSL_NO_ASYNC
#endif
