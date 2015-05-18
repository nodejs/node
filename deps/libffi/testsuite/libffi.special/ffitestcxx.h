#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <ffi.h>
#include "fficonfig.h"

#define MAX_ARGS 256


/* Define __UNUSED__ that also other compilers than gcc can run the tests.  */
#undef __UNUSED__
#if defined(__GNUC__)
#define __UNUSED__ __attribute__((__unused__))
#else
#define __UNUSED__
#endif

#define CHECK(x) (!(x) ? abort() : (void)0)

/* Prefer MAP_ANON(YMOUS) to /dev/zero, since we don't need to keep a
   file open.  */
#ifdef HAVE_MMAP_ANON
# undef HAVE_MMAP_DEV_ZERO

# include <sys/mman.h>
# ifndef MAP_FAILED
#  define MAP_FAILED -1
# endif
# if !defined (MAP_ANONYMOUS) && defined (MAP_ANON)
#  define MAP_ANONYMOUS MAP_ANON
# endif
# define USING_MMAP

#endif

#ifdef HAVE_MMAP_DEV_ZERO

# include <sys/mman.h>
# ifndef MAP_FAILED
#  define MAP_FAILED -1
# endif
# define USING_MMAP

#endif


/* MinGW kludge.  */
#ifdef _WIN64
#define PRIdLL "I64d"
#define PRIuLL "I64u"
#else
#define PRIdLL "lld"
#define PRIuLL "llu"
#endif

