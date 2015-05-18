/* -----------------------------------------------------------------------
   closures.c - Copyright (c) 2007, 2009, 2010  Red Hat, Inc.
                Copyright (C) 2007, 2009, 2010 Free Software Foundation, Inc
                Copyright (c) 2011 Plausible Labs Cooperative, Inc.

   Code to allocate and deallocate memory for closures.

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   ``Software''), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED ``AS IS'', WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
   ----------------------------------------------------------------------- */

#if defined __linux__ && !defined _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <ffi.h>
#include <ffi_common.h>

#if !FFI_MMAP_EXEC_WRIT && !FFI_EXEC_TRAMPOLINE_TABLE
# if __gnu_linux__
/* This macro indicates it may be forbidden to map anonymous memory
   with both write and execute permission.  Code compiled when this
   option is defined will attempt to map such pages once, but if it
   fails, it falls back to creating a temporary file in a writable and
   executable filesystem and mapping pages from it into separate
   locations in the virtual memory space, one location writable and
   another executable.  */
#  define FFI_MMAP_EXEC_WRIT 1
#  define HAVE_MNTENT 1
# endif
# if defined(X86_WIN32) || defined(X86_WIN64) || defined(__OS2__)
/* Windows systems may have Data Execution Protection (DEP) enabled, 
   which requires the use of VirtualMalloc/VirtualFree to alloc/free
   executable memory. */
#  define FFI_MMAP_EXEC_WRIT 1
# endif
#endif

#if FFI_MMAP_EXEC_WRIT && !defined FFI_MMAP_EXEC_SELINUX
# ifdef __linux__
/* When defined to 1 check for SELinux and if SELinux is active,
   don't attempt PROT_EXEC|PROT_WRITE mapping at all, as that
   might cause audit messages.  */
#  define FFI_MMAP_EXEC_SELINUX 1
# endif
#endif

#if FFI_CLOSURES

# if FFI_EXEC_TRAMPOLINE_TABLE

// Per-target implementation; It's unclear what can reasonable be shared between two OS/architecture implementations.

# elif FFI_MMAP_EXEC_WRIT /* !FFI_EXEC_TRAMPOLINE_TABLE */

#define USE_LOCKS 1
#define USE_DL_PREFIX 1
#ifdef __GNUC__
#ifndef USE_BUILTIN_FFS
#define USE_BUILTIN_FFS 1
#endif
#endif

/* We need to use mmap, not sbrk.  */
#define HAVE_MORECORE 0

/* We could, in theory, support mremap, but it wouldn't buy us anything.  */
#define HAVE_MREMAP 0

/* We have no use for this, so save some code and data.  */
#define NO_MALLINFO 1

/* We need all allocations to be in regular segments, otherwise we
   lose track of the corresponding code address.  */
#define DEFAULT_MMAP_THRESHOLD MAX_SIZE_T

/* Don't allocate more than a page unless needed.  */
#define DEFAULT_GRANULARITY ((size_t)malloc_getpagesize)

#if FFI_CLOSURE_TEST
/* Don't release single pages, to avoid a worst-case scenario of
   continuously allocating and releasing single pages, but release
   pairs of pages, which should do just as well given that allocations
   are likely to be small.  */
#define DEFAULT_TRIM_THRESHOLD ((size_t)malloc_getpagesize)
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <string.h>
#include <stdio.h>
#if !defined(X86_WIN32) && !defined(X86_WIN64)
#ifdef HAVE_MNTENT
#include <mntent.h>
#endif /* HAVE_MNTENT */
#include <sys/param.h>
#include <pthread.h>

/* We don't want sys/mman.h to be included after we redefine mmap and
   dlmunmap.  */
#include <sys/mman.h>
#define LACKS_SYS_MMAN_H 1

#if FFI_MMAP_EXEC_SELINUX
#include <sys/statfs.h>
#include <stdlib.h>

static int selinux_enabled = -1;

static int
selinux_enabled_check (void)
{
  struct statfs sfs;
  FILE *f;
  char *buf = NULL;
  size_t len = 0;

  if (statfs ("/selinux", &sfs) >= 0
      && (unsigned int) sfs.f_type == 0xf97cff8cU)
    return 1;
  f = fopen ("/proc/mounts", "r");
  if (f == NULL)
    return 0;
  while (getline (&buf, &len, f) >= 0)
    {
      char *p = strchr (buf, ' ');
      if (p == NULL)
        break;
      p = strchr (p + 1, ' ');
      if (p == NULL)
        break;
      if (strncmp (p + 1, "selinuxfs ", 10) == 0)
        {
          free (buf);
          fclose (f);
          return 1;
        }
    }
  free (buf);
  fclose (f);
  return 0;
}

#define is_selinux_enabled() (selinux_enabled >= 0 ? selinux_enabled \
			      : (selinux_enabled = selinux_enabled_check ()))

#else

#define is_selinux_enabled() 0

#endif /* !FFI_MMAP_EXEC_SELINUX */

/* On PaX enable kernels that have MPROTECT enable we can't use PROT_EXEC. */
#ifdef FFI_MMAP_EXEC_EMUTRAMP_PAX
#include <stdlib.h>

static int emutramp_enabled = -1;

static int
emutramp_enabled_check (void)
{
  if (getenv ("FFI_DISABLE_EMUTRAMP") == NULL)
    return 1;
  else
    return 0;
}

#define is_emutramp_enabled() (emutramp_enabled >= 0 ? emutramp_enabled \
                               : (emutramp_enabled = emutramp_enabled_check ()))
#endif /* FFI_MMAP_EXEC_EMUTRAMP_PAX */

#elif defined (__CYGWIN__) || defined(__INTERIX)

#include <sys/mman.h>

/* Cygwin is Linux-like, but not quite that Linux-like.  */
#define is_selinux_enabled() 0

#endif /* !defined(X86_WIN32) && !defined(X86_WIN64) */

#ifndef FFI_MMAP_EXEC_EMUTRAMP_PAX
#define is_emutramp_enabled() 0
#endif /* FFI_MMAP_EXEC_EMUTRAMP_PAX */

/* Declare all functions defined in dlmalloc.c as static.  */
static void *dlmalloc(size_t);
static void dlfree(void*);
static void *dlcalloc(size_t, size_t) MAYBE_UNUSED;
static void *dlrealloc(void *, size_t) MAYBE_UNUSED;
static void *dlmemalign(size_t, size_t) MAYBE_UNUSED;
static void *dlvalloc(size_t) MAYBE_UNUSED;
static int dlmallopt(int, int) MAYBE_UNUSED;
static size_t dlmalloc_footprint(void) MAYBE_UNUSED;
static size_t dlmalloc_max_footprint(void) MAYBE_UNUSED;
static void** dlindependent_calloc(size_t, size_t, void**) MAYBE_UNUSED;
static void** dlindependent_comalloc(size_t, size_t*, void**) MAYBE_UNUSED;
static void *dlpvalloc(size_t) MAYBE_UNUSED;
static int dlmalloc_trim(size_t) MAYBE_UNUSED;
static size_t dlmalloc_usable_size(void*) MAYBE_UNUSED;
static void dlmalloc_stats(void) MAYBE_UNUSED;

#if !(defined(X86_WIN32) || defined(X86_WIN64) || defined(__OS2__)) || defined (__CYGWIN__) || defined(__INTERIX)
/* Use these for mmap and munmap within dlmalloc.c.  */
static void *dlmmap(void *, size_t, int, int, int, off_t);
static int dlmunmap(void *, size_t);
#endif /* !(defined(X86_WIN32) || defined(X86_WIN64) || defined(__OS2__)) || defined (__CYGWIN__) || defined(__INTERIX) */

#define mmap dlmmap
#define munmap dlmunmap

#include "dlmalloc.c"

#undef mmap
#undef munmap

#if !(defined(X86_WIN32) || defined(X86_WIN64) || defined(__OS2__)) || defined (__CYGWIN__) || defined(__INTERIX)

/* A mutex used to synchronize access to *exec* variables in this file.  */
static pthread_mutex_t open_temp_exec_file_mutex = PTHREAD_MUTEX_INITIALIZER;

/* A file descriptor of a temporary file from which we'll map
   executable pages.  */
static int execfd = -1;

/* The amount of space already allocated from the temporary file.  */
static size_t execsize = 0;

/* Open a temporary file name, and immediately unlink it.  */
static int
open_temp_exec_file_name (char *name)
{
  int fd = mkstemp (name);

  if (fd != -1)
    unlink (name);

  return fd;
}

/* Open a temporary file in the named directory.  */
static int
open_temp_exec_file_dir (const char *dir)
{
  static const char suffix[] = "/ffiXXXXXX";
  int lendir = strlen (dir);
  char *tempname = __builtin_alloca (lendir + sizeof (suffix));

  if (!tempname)
    return -1;

  memcpy (tempname, dir, lendir);
  memcpy (tempname + lendir, suffix, sizeof (suffix));

  return open_temp_exec_file_name (tempname);
}

/* Open a temporary file in the directory in the named environment
   variable.  */
static int
open_temp_exec_file_env (const char *envvar)
{
  const char *value = getenv (envvar);

  if (!value)
    return -1;

  return open_temp_exec_file_dir (value);
}

#ifdef HAVE_MNTENT
/* Open a temporary file in an executable and writable mount point
   listed in the mounts file.  Subsequent calls with the same mounts
   keep searching for mount points in the same file.  Providing NULL
   as the mounts file closes the file.  */
static int
open_temp_exec_file_mnt (const char *mounts)
{
  static const char *last_mounts;
  static FILE *last_mntent;

  if (mounts != last_mounts)
    {
      if (last_mntent)
	endmntent (last_mntent);

      last_mounts = mounts;

      if (mounts)
	last_mntent = setmntent (mounts, "r");
      else
	last_mntent = NULL;
    }

  if (!last_mntent)
    return -1;

  for (;;)
    {
      int fd;
      struct mntent mnt;
      char buf[MAXPATHLEN * 3];

      if (getmntent_r (last_mntent, &mnt, buf, sizeof (buf)) == NULL)
	return -1;

      if (hasmntopt (&mnt, "ro")
	  || hasmntopt (&mnt, "noexec")
	  || access (mnt.mnt_dir, W_OK))
	continue;

      fd = open_temp_exec_file_dir (mnt.mnt_dir);

      if (fd != -1)
	return fd;
    }
}
#endif /* HAVE_MNTENT */

/* Instructions to look for a location to hold a temporary file that
   can be mapped in for execution.  */
static struct
{
  int (*func)(const char *);
  const char *arg;
  int repeat;
} open_temp_exec_file_opts[] = {
  { open_temp_exec_file_env, "TMPDIR", 0 },
  { open_temp_exec_file_dir, "/tmp", 0 },
  { open_temp_exec_file_dir, "/var/tmp", 0 },
  { open_temp_exec_file_dir, "/dev/shm", 0 },
  { open_temp_exec_file_env, "HOME", 0 },
#ifdef HAVE_MNTENT
  { open_temp_exec_file_mnt, "/etc/mtab", 1 },
  { open_temp_exec_file_mnt, "/proc/mounts", 1 },
#endif /* HAVE_MNTENT */
};

/* Current index into open_temp_exec_file_opts.  */
static int open_temp_exec_file_opts_idx = 0;

/* Reset a current multi-call func, then advances to the next entry.
   If we're at the last, go back to the first and return nonzero,
   otherwise return zero.  */
static int
open_temp_exec_file_opts_next (void)
{
  if (open_temp_exec_file_opts[open_temp_exec_file_opts_idx].repeat)
    open_temp_exec_file_opts[open_temp_exec_file_opts_idx].func (NULL);

  open_temp_exec_file_opts_idx++;
  if (open_temp_exec_file_opts_idx
      == (sizeof (open_temp_exec_file_opts)
	  / sizeof (*open_temp_exec_file_opts)))
    {
      open_temp_exec_file_opts_idx = 0;
      return 1;
    }

  return 0;
}

/* Return a file descriptor of a temporary zero-sized file in a
   writable and exexutable filesystem.  */
static int
open_temp_exec_file (void)
{
  int fd;

  do
    {
      fd = open_temp_exec_file_opts[open_temp_exec_file_opts_idx].func
	(open_temp_exec_file_opts[open_temp_exec_file_opts_idx].arg);

      if (!open_temp_exec_file_opts[open_temp_exec_file_opts_idx].repeat
	  || fd == -1)
	{
	  if (open_temp_exec_file_opts_next ())
	    break;
	}
    }
  while (fd == -1);

  return fd;
}

/* Map in a chunk of memory from the temporary exec file into separate
   locations in the virtual memory address space, one writable and one
   executable.  Returns the address of the writable portion, after
   storing an offset to the corresponding executable portion at the
   last word of the requested chunk.  */
static void *
dlmmap_locked (void *start, size_t length, int prot, int flags, off_t offset)
{
  void *ptr;

  if (execfd == -1)
    {
      open_temp_exec_file_opts_idx = 0;
    retry_open:
      execfd = open_temp_exec_file ();
      if (execfd == -1)
	return MFAIL;
    }

  offset = execsize;

  if (ftruncate (execfd, offset + length))
    return MFAIL;

  flags &= ~(MAP_PRIVATE | MAP_ANONYMOUS);
  flags |= MAP_SHARED;

  ptr = mmap (NULL, length, (prot & ~PROT_WRITE) | PROT_EXEC,
	      flags, execfd, offset);
  if (ptr == MFAIL)
    {
      if (!offset)
	{
	  close (execfd);
	  goto retry_open;
	}
      ftruncate (execfd, offset);
      return MFAIL;
    }
  else if (!offset
	   && open_temp_exec_file_opts[open_temp_exec_file_opts_idx].repeat)
    open_temp_exec_file_opts_next ();

  start = mmap (start, length, prot, flags, execfd, offset);

  if (start == MFAIL)
    {
      munmap (ptr, length);
      ftruncate (execfd, offset);
      return start;
    }

  mmap_exec_offset ((char *)start, length) = (char*)ptr - (char*)start;

  execsize += length;

  return start;
}

/* Map in a writable and executable chunk of memory if possible.
   Failing that, fall back to dlmmap_locked.  */
static void *
dlmmap (void *start, size_t length, int prot,
	int flags, int fd, off_t offset)
{
  void *ptr;

  assert (start == NULL && length % malloc_getpagesize == 0
	  && prot == (PROT_READ | PROT_WRITE)
	  && flags == (MAP_PRIVATE | MAP_ANONYMOUS)
	  && fd == -1 && offset == 0);

#if FFI_CLOSURE_TEST
  printf ("mapping in %zi\n", length);
#endif

  if (execfd == -1 && is_emutramp_enabled ())
    {
      ptr = mmap (start, length, prot & ~PROT_EXEC, flags, fd, offset);
      return ptr;
    }

  if (execfd == -1 && !is_selinux_enabled ())
    {
      ptr = mmap (start, length, prot | PROT_EXEC, flags, fd, offset);

      if (ptr != MFAIL || (errno != EPERM && errno != EACCES))
	/* Cool, no need to mess with separate segments.  */
	return ptr;

      /* If MREMAP_DUP is ever introduced and implemented, try mmap
	 with ((prot & ~PROT_WRITE) | PROT_EXEC) and mremap with
	 MREMAP_DUP and prot at this point.  */
    }

  if (execsize == 0 || execfd == -1)
    {
      pthread_mutex_lock (&open_temp_exec_file_mutex);
      ptr = dlmmap_locked (start, length, prot, flags, offset);
      pthread_mutex_unlock (&open_temp_exec_file_mutex);

      return ptr;
    }

  return dlmmap_locked (start, length, prot, flags, offset);
}

/* Release memory at the given address, as well as the corresponding
   executable page if it's separate.  */
static int
dlmunmap (void *start, size_t length)
{
  /* We don't bother decreasing execsize or truncating the file, since
     we can't quite tell whether we're unmapping the end of the file.
     We don't expect frequent deallocation anyway.  If we did, we
     could locate pages in the file by writing to the pages being
     deallocated and checking that the file contents change.
     Yuck.  */
  msegmentptr seg = segment_holding (gm, start);
  void *code;

#if FFI_CLOSURE_TEST
  printf ("unmapping %zi\n", length);
#endif

  if (seg && (code = add_segment_exec_offset (start, seg)) != start)
    {
      int ret = munmap (code, length);
      if (ret)
	return ret;
    }

  return munmap (start, length);
}

#if FFI_CLOSURE_FREE_CODE
/* Return segment holding given code address.  */
static msegmentptr
segment_holding_code (mstate m, char* addr)
{
  msegmentptr sp = &m->seg;
  for (;;) {
    if (addr >= add_segment_exec_offset (sp->base, sp)
	&& addr < add_segment_exec_offset (sp->base, sp) + sp->size)
      return sp;
    if ((sp = sp->next) == 0)
      return 0;
  }
}
#endif

#endif /* !(defined(X86_WIN32) || defined(X86_WIN64) || defined(__OS2__)) || defined (__CYGWIN__) || defined(__INTERIX) */

/* Allocate a chunk of memory with the given size.  Returns a pointer
   to the writable address, and sets *CODE to the executable
   corresponding virtual address.  */
void *
ffi_closure_alloc (size_t size, void **code)
{
  void *ptr;

  if (!code)
    return NULL;

  ptr = dlmalloc (size);

  if (ptr)
    {
      msegmentptr seg = segment_holding (gm, ptr);

      *code = add_segment_exec_offset (ptr, seg);
    }

  return ptr;
}

/* Release a chunk of memory allocated with ffi_closure_alloc.  If
   FFI_CLOSURE_FREE_CODE is nonzero, the given address can be the
   writable or the executable address given.  Otherwise, only the
   writable address can be provided here.  */
void
ffi_closure_free (void *ptr)
{
#if FFI_CLOSURE_FREE_CODE
  msegmentptr seg = segment_holding_code (gm, ptr);

  if (seg)
    ptr = sub_segment_exec_offset (ptr, seg);
#endif

  dlfree (ptr);
}


#if FFI_CLOSURE_TEST
/* Do some internal sanity testing to make sure allocation and
   deallocation of pages are working as intended.  */
int main ()
{
  void *p[3];
#define GET(idx, len) do { p[idx] = dlmalloc (len); printf ("allocated %zi for p[%i]\n", (len), (idx)); } while (0)
#define PUT(idx) do { printf ("freeing p[%i]\n", (idx)); dlfree (p[idx]); } while (0)
  GET (0, malloc_getpagesize / 2);
  GET (1, 2 * malloc_getpagesize - 64 * sizeof (void*));
  PUT (1);
  GET (1, 2 * malloc_getpagesize);
  GET (2, malloc_getpagesize / 2);
  PUT (1);
  PUT (0);
  PUT (2);
  return 0;
}
#endif /* FFI_CLOSURE_TEST */
# else /* ! FFI_MMAP_EXEC_WRIT */

/* On many systems, memory returned by malloc is writable and
   executable, so just use it.  */

#include <stdlib.h>

void *
ffi_closure_alloc (size_t size, void **code)
{
  if (!code)
    return NULL;

  return *code = malloc (size);
}

void
ffi_closure_free (void *ptr)
{
  free (ptr);
}

# endif /* ! FFI_MMAP_EXEC_WRIT */
#endif /* FFI_CLOSURES */
