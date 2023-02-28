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

#ifdef USING_MMAP
static inline void *
allocate_mmap (size_t size)
{
  void *page;
#if defined (HAVE_MMAP_DEV_ZERO)
  static int dev_zero_fd = -1;
#endif

#ifdef HAVE_MMAP_DEV_ZERO
  if (dev_zero_fd == -1)
    {
      dev_zero_fd = open ("/dev/zero", O_RDONLY);
      if (dev_zero_fd == -1)
	{
	  perror ("open /dev/zero: %m");
	  exit (1);
	}
    }
#endif


#ifdef HAVE_MMAP_ANON
  page = mmap (NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC,
	       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
#ifdef HAVE_MMAP_DEV_ZERO
  page = mmap (NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC,
	       MAP_PRIVATE, dev_zero_fd, 0);
#endif

  if (page == (char *) MAP_FAILED)
    {
      perror ("virtual memory exhausted");
      exit (1);
    }

  return page;
}

#endif
