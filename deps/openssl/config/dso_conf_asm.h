#undef OPENSSL_LINUX
#if defined(__linux) && !defined(__ANDROID__)
# define OPENSSL_LINUX 1
#endif

#if defined(OPENSSL_LINUX) && defined(__i386__)
# include "./archs/linux-elf/asm/include/crypto/dso_conf.h"
#elif defined(OPENSSL_LINUX) && defined(__ILP32__)
# include "./archs/linux-x32/asm/include/crypto/dso_conf.h"
#elif defined(OPENSSL_LINUX) && defined(__x86_64__)
# include "./archs/linux-x86_64/asm/include/crypto/dso_conf.h"
#elif defined(OPENSSL_LINUX) && defined(__arm__)
# include "./archs/linux-armv4/asm/include/crypto/dso_conf.h"
#elif defined(OPENSSL_LINUX) && defined(__aarch64__)
# include "./archs/linux-aarch64/asm/include/crypto/dso_conf.h"
#elif defined(__APPLE__) && defined(__MACH__) && defined(__i386__)
# include "./archs/darwin-i386-cc/asm/include/crypto/dso_conf.h"
#elif defined(__APPLE__) && defined(__MACH__) && defined(__x86_64__)
# include "./archs/darwin64-x86_64-cc/asm/include/crypto/dso_conf.h"
#elif defined(__APPLE__) && defined(__MACH__) && defined(__arm64__)
# include "./archs/darwin64-arm64-cc/asm/include/crypto/dso_conf.h"
#elif defined(_WIN32) && defined(_M_IX86)
# include "./archs/VC-WIN32/asm/include/crypto/dso_conf.h"
#elif defined(_WIN32) && defined(_M_X64)
# include "./archs/VC-WIN64A/asm/include/crypto/dso_conf.h"
#elif (defined(__FreeBSD__) || defined(__OpenBSD__)) && defined(__i386__)
# include "./archs/BSD-x86/asm/include/crypto/dso_conf.h"
#elif (defined(__FreeBSD__) || defined(__OpenBSD__)) && defined(__x86_64__)
# include "./archs/BSD-x86_64/asm/include/crypto/dso_conf.h"
#elif defined(__sun) && defined(__i386__)
# include "./archs/solaris-x86-gcc/asm/include/crypto/dso_conf.h"
#elif defined(__sun) && defined(__x86_64__)
# include "./archs/solaris64-x86_64-gcc/asm/include/crypto/dso_conf.h"
#elif defined(OPENSSL_LINUX) && defined(__PPC64__)  && defined(L_ENDIAN)
# include "./archs/linux-ppc64le/asm/include/crypto/dso_conf.h"
#elif defined(_AIX) && defined(_ARCH_PPC64)
# include "./archs/aix64-gcc-as/asm/include/crypto/dso_conf.h"
#elif defined(OPENSSL_LINUX) && defined(__s390x__)
# include "./archs/linux64-s390x/asm/include/crypto/dso_conf.h"
#elif defined(OPENSSL_LINUX) && defined(__s390__)
# include "./archs/linux32-s390x/asm/include/crypto/dso_conf.h"
#else
# include "./archs/linux-elf/asm/include/crypto/dso_conf.h"
#endif

/* GOST is not included in all platform */
#ifndef OPENSSL_NO_GOST
# define OPENSSL_NO_GOST
#endif
/* HW_PADLOCK is not included in all platform */
#ifndef OPENSSL_NO_HW_PADLOCK
# define OPENSSL_NO_HW_PADLOCK
#endif
