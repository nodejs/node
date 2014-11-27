/* opensslconf.h */
/* WARNING: Generated automatically from opensslconf.h.in by Configure. */

/* OpenSSL was configured with the following options: */
#undef OPENSSL_SYSNAME_WIN32
#if defined(_WIN32)
# define OPENSSL_SYSNAME_WIN32
#endif

#ifndef OPENSSL_DOING_MAKEDEPEND
# ifndef OPENSSL_NO_CAPIENG
#  define OPENSSL_NO_CAPIENG
# endif
# ifndef OPENSSL_NO_EC_NISTP_64_GCC_128
#  define OPENSSL_NO_EC_NISTP_64_GCC_128
# endif
# ifndef OPENSSL_NO_GMP
#  define OPENSSL_NO_GMP
# endif
# ifndef OPENSSL_NO_GOST
#  define OPENSSL_NO_GOST
# endif
# ifndef OPENSSL_NO_HW_PADLOCK
#  define OPENSSL_NO_HW_PADLOCK
# endif
# ifndef OPENSSL_NO_JPAKE
#  define OPENSSL_NO_JPAKE
# endif
# ifndef OPENSSL_NO_KRB5
#  define OPENSSL_NO_KRB5
# endif
# ifndef OPENSSL_NO_MD2
#  define OPENSSL_NO_MD2
# endif
# ifndef OPENSSL_NO_RC5
#  define OPENSSL_NO_RC5
# endif
# ifndef OPENSSL_NO_RFC3779
#  define OPENSSL_NO_RFC3779
# endif
# ifndef OPENSSL_NO_SCTP
#  define OPENSSL_NO_SCTP
# endif
# ifndef OPENSSL_NO_STORE
#  define OPENSSL_NO_STORE
# endif
#endif /* OPENSSL_DOING_MAKEDEPEND */

#ifndef OPENSSL_THREADS
# define OPENSSL_THREADS
#endif
#ifndef OPENSSL_NO_DYNAMIC_ENGINE
# define OPENSSL_NO_DYNAMIC_ENGINE
#endif

/* The OPENSSL_NO_* macros are also defined as NO_* if the application
   asks for it.  This is a transient feature that is provided for those
   who haven't had the time to do the appropriate changes in their
   applications.  */
#ifdef OPENSSL_ALGORITHM_DEFINES
# if defined(OPENSSL_NO_CAMELLIA) && !defined(NO_CAMELLIA)
#  define NO_CAMELLIA
# endif
# if defined(OPENSSL_NO_CAPIENG) && !defined(NO_CAPIENG)
#  define NO_CAPIENG
# endif
# if defined(OPENSSL_NO_CAST) && !defined(NO_CAST)
#  define NO_CAST
# endif
# if defined(OPENSSL_NO_CMS) && !defined(NO_CMS)
#  define NO_CMS
# endif
# if defined(OPENSSL_NO_FIPS) && !defined(NO_FIPS)
#  define NO_FIPS
# endif
# if defined(OPENSSL_NO_GMP) && !defined(NO_GMP)
#  define NO_GMP
# endif
# if defined(OPENSSL_NO_IDEA) && !defined(NO_IDEA)
#  define NO_IDEA
# endif
# if defined(OPENSSL_NO_JPAKE) && !defined(NO_JPAKE)
#  define NO_JPAKE
# endif
# if defined(OPENSSL_NO_KRB5) && !defined(NO_KRB5)
#  define NO_KRB5
# endif
# if defined(OPENSSL_NO_MD2) && !defined(NO_MD2)
#  define NO_MD2
# endif
# if defined(OPENSSL_NO_MDC2) && !defined(NO_MDC2)
#  define NO_MDC2
# endif
# if defined(OPENSSL_NO_RC5) && !defined(NO_RC5)
#  define NO_RC5
# endif
# if defined(OPENSSL_NO_RFC3779) && !defined(NO_RFC3779)
#  define NO_RFC3779
# endif
# if defined(OPENSSL_NO_SEED) && !defined(NO_SEED)
#  define NO_SEED
# endif
# if defined(OPENSSL_NO_SHA0) && !defined(NO_SHA0)
#  define NO_SHA0
# endif
# if defined(OPENSSL_NO_STORE) && !defined(NO_STORE)
#  define NO_STORE
# endif
# if defined(OPENSSL_NO_WHRLPOOL) && !defined(NO_WHRLPOOL)
#  define NO_WHRLPOOL
# endif
# if defined(OPENSSL_NO_MDC2) && !defined(NO_MDC2)
#  define NO_MDC2
# endif
#endif

/* crypto/opensslconf.h.in */

#ifdef OPENSSL_DOING_MAKEDEPEND
 /* Include any symbols here that have to be explicitly set to enable a feature
  * that should be visible to makedepend.
  *
  * [Our "make depend" doesn't actually look at this, we use actual build settings
  * instead; we want to make it easy to remove subdirectories with disabled algorithms.]
  */
# ifndef OPENSSL_FIPS
#  define OPENSSL_FIPS
# endif
#endif

/* Generate 80386 code? */
#undef I386_ONLY

#if !(defined(VMS) || defined(__VMS)) /* VMS uses logical names instead */
# if defined(HEADER_CRYPTLIB_H) && !defined(OPENSSLDIR)
#  if defined(_WIN32)
#   define ENGINESDIR "ssl/lib/engines"
#   define OPENSSLDIR "ssl"
#  else
#   define ENGINESDIR "/usr/local/ssl/lib/engines"
#   define OPENSSLDIR "/usr/local/ssl"
#  endif
# endif
#endif

#undef OPENSSL_UNISTD
#define OPENSSL_UNISTD <unistd.h>
#if !defined(_WIN32) && !defined(__arm__) && !defined(__mips__) && !defined(SWIG)
# include <unistd.h>
#endif

#undef OPENSSL_EXPORT_VAR_AS_FUNCTION
#if defined(_WIN32)
# define OPENSSL_EXPORT_VAR_AS_FUNCTION
#endif

#if defined(HEADER_IDEA_H)
# undef IDEA_INT
# define IDEA_INT unsigned int
#endif

#if defined(HEADER_MD2_H)
# undef MD2_INT
# define MD2_INT unsigned int
#endif

#if defined(HEADER_RC2_H)
/* I need to put in a mod for the alpha - eay */
# undef RC2_INT
# define RC2_INT unsigned int
#endif

#if defined(HEADER_RC4_H)
 /* using int types make the structure larger but make the code faster
  * on most boxes I have tested - up to %20 faster. */
 /*
  * I don't know what does "most" mean, but declaring "int" is a must on:
  * - Intel P6 because partial register stalls are very expensive;
  * - elder Alpha because it lacks byte load/store instructions;
  */
# undef RC4_INT
# if defined(__arm__)
#  define RC4_INT unsigned char
# else
#  define RC4_INT unsigned int
# endif

  /*
   * This enables code handling data aligned at natural CPU word
   * boundary. See crypto/rc4/rc4_enc.c for further details.
   */
# undef RC4_CHUNK
# if (defined(_M_X64) || defined(__x86_64__)) && defined(_WIN32)
#  define RC4_CHUNK unsigned long long
# elif (defined(_M_X64) || defined(__x86_64__)) && !defined(_WIN32)
#  define RC4_CHUNK unsigned long
# elif defined(__arm__)
#  define RC4_CHUNK unsigned long
# else
   /* On x86 RC4_CHUNK is not defined */
# endif
#endif

#if defined(HEADER_NEW_DES_H) || defined(HEADER_DES_H)
 /* If this is set to 'unsigned int' on a DEC Alpha, this gives about a
  * %20 speed up (longs are 8 bytes, int's are 4). */
# undef DES_LONG
# if defined(_M_X64) || defined(__x86_64__) || defined(__arm__) || defined(__mips__)
#  define DES_LONG unsigned int
# elif defined(_M_IX86) || defined(__i386__)
#  define DES_LONG unsigned long
# endif
#endif

#if defined(HEADER_BN_H) && !defined(CONFIG_HEADER_BN_H)
# define CONFIG_HEADER_BN_H

# undef BL_LLONG
# if defined(_M_IX86) || defined(__i386__) || defined(__arm__)
#  define BL_LLONG
# endif

 /* Should we define BN_DIV2W here? */

 /* Only one for the following should be defined */
 /* The prime number generation stuff may not work when
  * EIGHT_BIT but I don't care since I've only used this mode
  * for debuging the bignum libraries */
# undef SIXTY_FOUR_BIT_LONG
# undef SIXTY_FOUR_BIT
# undef THIRTY_TWO_BIT
# undef SIXTEEN_BIT
# undef EIGHT_BIT
# if (defined(_M_X64) || defined(__x86_64__)) && defined(_WIN32)
#  define SIXTY_FOUR_BIT
# elif (defined(_M_X64) || defined(__x86_64__)) && !defined(_WIN32)
#  define SIXTY_FOUR_BIT_LONG
# elif defined(_M_IX86) || defined(__i386__) || defined(__arm__) || defined(__mips__)
#  define THIRTY_TWO_BIT
# endif
#endif

#if defined(HEADER_RC4_LOCL_H) && !defined(CONFIG_HEADER_RC4_LOCL_H)
# define CONFIG_HEADER_RC4_LOCL_H
 /* if this is defined data[i] is used instead of *data, this is a %20
  * speedup on x86 */
# undef RC4_INDEX
# if defined(_M_IX86) || defined(__i386__)
#  define RC4_INDEX
# endif
#endif

#if defined(HEADER_BF_LOCL_H) && !defined(CONFIG_HEADER_BF_LOCL_H)
# define CONFIG_HEADER_BF_LOCL_H
# undef BF_PTR
# if defined(__arm__)
#  define BF_PTR
# endif
#endif /* HEADER_BF_LOCL_H */

#if defined(HEADER_DES_LOCL_H) && !defined(CONFIG_HEADER_DES_LOCL_H)
# define CONFIG_HEADER_DES_LOCL_H

# ifndef DES_DEFAULT_OPTIONS
   /* the following is tweaked from a config script, that is why it is a
    * protected undef/define */
#  undef DES_PTR
#  if !defined(_WIN32) && (defined(_M_IX86) || defined(__i386__))
#   define DES_PTR
#  endif

   /* This helps C compiler generate the correct code for multiple functional
    * units.  It reduces register dependancies at the expense of 2 more
    * registers */
#  undef DES_RISC1
#  if !defined(_WIN32) && (defined(_M_IX86) || defined(__i386__))
#   define DES_RISC1
#  endif

#  undef DES_RISC2

#  if defined(DES_RISC1) && defined(DES_RISC2)
#   error YOU SHOULD NOT HAVE BOTH DES_RISC1 AND DES_RISC2 DEFINED!!!!!
#  endif

  /* Unroll the inner loop, this sometimes helps, sometimes hinders.
   * Very mucy CPU dependant */
#  undef DES_UNROLL
#  if !defined(_WIN32)
#   define DES_UNROLL
#  endif

  /* These default values were supplied by
   * Peter Gutman <pgut001@cs.auckland.ac.nz>
   * They are only used if nothing else has been defined */
# if !defined(DES_PTR) && !defined(DES_RISC1) && !defined(DES_RISC2) && !defined(DES_UNROLL)
   /* Special defines which change the way the code is built depending on the
      CPU and OS.  For SGI machines you can use _MIPS_SZLONG (32 or 64) to find
      even newer MIPS CPU's, but at the moment one size fits all for
      optimization options.  Older Sparc's work better with only UNROLL, but
      there's no way to tell at compile time what it is you're running on */
#  if defined( sun )		/* Newer Sparc's */
#    define DES_PTR
#    define DES_RISC1
#    define DES_UNROLL
#  elif defined( __ultrix )	/* Older MIPS */
#    define DES_PTR
#    define DES_RISC2
#    define DES_UNROLL
#  elif defined( __osf1__ )	/* Alpha */
#    define DES_PTR
#    define DES_RISC2
#  elif defined ( _AIX )		/* RS6000 */
    /* Unknown */
#  elif defined( __hpux )		/* HP-PA */
    /* Unknown */
#  elif defined( __aux )		/* 68K */
    /* Unknown */
#  elif defined( __dgux )		/* 88K (but P6 in latest boxes) */
#    define DES_UNROLL
#  elif defined( __sgi )		/* Newer MIPS */
#    define DES_PTR
#    define DES_RISC2
#    define DES_UNROLL
#  elif defined(i386) || defined(__i386__)	/* x86 boxes, should be gcc */
#    define DES_PTR
#    define DES_RISC1
#    define DES_UNROLL
#  endif /* Systems-specific speed defines */
# endif

# endif /* DES_DEFAULT_OPTIONS */
#endif /* HEADER_DES_LOCL_H */
