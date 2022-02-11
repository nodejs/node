#if defined(OPENSSL_LINUX) && defined(__i386__)
# include "./archs/linux-elf/no-asm/include/openssl/opensslconf.h"
#elif defined(OPENSSL_LINUX) && defined(__x86_64__)
# include "./archs/linux-x86_64/no-asm/include/openssl/opensslconf.h"
#elif defined(OPENSSL_LINUX) && defined(__arm__)
# include "./archs/linux-armv4/no-asm/include/openssl/opensslconf.h"
#elif defined(OPENSSL_LINUX) && defined(__aarch64__)
# include "./archs/linux-aarch64/no-asm/include/openssl/opensslconf.h"
#elif defined(__APPLE__) && defined(__MACH__) && defined(__i386__)
# include "./archs/darwin-i386-cc/no-asm/include/openssl/opensslconf.h"
#elif defined(__APPLE__) && defined(__MACH__) && defined(__x86_64__)
# include "./archs/darwin64-x86_64-cc/no-asm/include/openssl/opensslconf.h"
#elif defined(__APPLE__) && defined(__MACH__) && defined(__arm64__)
# include "./archs/darwin64-arm64-cc/no-asm/include/openssl/opensslconf.h"
#elif defined(_WIN32) && defined(_M_IX86)
# include "./archs/VC-WIN32/no-asm/include/openssl/opensslconf.h"
#elif defined(_WIN32) && defined(_M_X64)
# include "./archs/VC-WIN64A/no-asm/include/openssl/opensslconf.h"
#elif defined(_WIN32) && defined(_M_ARM64)
# include "./archs/VC-WIN64-ARM/no-asm/include/openssl/opensslconf.h"
#elif (defined(__FreeBSD__) || defined(__OpenBSD__)) && defined(__i386__)
# include "./archs/BSD-x86/no-asm/include/openssl/opensslconf.h"
#elif (defined(__FreeBSD__) || defined(__OpenBSD__)) && defined(__x86_64__)
# include "./archs/BSD-x86_64/no-asm/include/openssl/opensslconf.h"
#elif defined(__sun) && defined(__i386__)
# include "./archs/solaris-x86-gcc/no-asm/include/openssl/opensslconf.h"
#elif defined(__sun) && defined(__x86_64__)
# include "./archs/solaris64-x86_64-gcc/no-asm/include/openssl/opensslconf.h"
#elif defined(OPENSSL_LINUX) && defined(__PPC64__) && defined(L_ENDIAN)
# include "./archs/linux-ppc64le/no-asm/include/openssl/opensslconf.h"
#elif defined(OPENSSL_LINUX) && defined(__PPC64__)
# include "./archs/linux-ppc64/no-asm/include/openssl/opensslconf.h"
#elif defined(OPENSSL_LINUX) && !defined(__PPC64__) && defined(__ppc__)
# include "./archs/linux-ppc/no-asm/include/openssl/opensslconf.h"
#elif defined(_AIX) && defined(_ARCH_PPC64)
# include "./archs/aix64-gcc-as/no-asm/include/openssl/opensslconf.h"
#elif defined(_AIX) && !defined(_ARCH_PPC64) && defined(_ARCH_PPC)
# include "./archs/aix-gcc/no-asm/include/openssl/opensslconf.h"
#elif defined(OPENSSL_LINUX) && defined(__s390x__)
# include "./archs/linux64-s390x/no-asm/include/openssl/opensslconf.h"
#elif defined(OPENSSL_LINUX) && defined(__s390__)
# include "./archs/linux32-s390x/no-asm/include/openssl/opensslconf.h"
#elif defined(OPENSSL_LINUX) && defined(__mips64) && defined(__MIPSEL__)
# include "./archs/linux64-mips64/no-asm/include/openssl/opensslconf.h"
#else
# include "./archs/linux-elf/no-asm/include/openssl/opensslconf.h"
#endif
