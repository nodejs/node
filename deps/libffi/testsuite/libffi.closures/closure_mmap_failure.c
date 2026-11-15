/* Area: closures */
/* { dg-do run { target x86_64-*-linux-gnu* } } */
/* { dg-options "-Wl,--export-dynamic" { target x86_64-*-linux-gnu* } } */

#define _GNU_SOURCE 1
#include "ffitest.h"

/* The Emscripten browser harness compiles every testcase and ignores the
   dg-do target above.  This test interposes mmap through RTLD_NEXT and needs
   dlsym on a glibc/ELF system; on wasm, dlsym from an unopened handle aborts.
   Guard the body so it is a harmless no-op anywhere it cannot run.  */
#if defined(__GLIBC__)

#include <dlfcn.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>

typedef void *(*mmap_fn) (void *, size_t, int, int, int, off_t);

static mmap_fn real_mmap;
static int reject_exec_mappings;

/* Model a hardened runtime that rejects every executable mapping.  */
void *
mmap (void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
  if (reject_exec_mappings && (prot & PROT_EXEC))
    {
      errno = EPERM;
      return MAP_FAILED;
    }

  return real_mmap (addr, length, prot, flags, fd, offset);
}

int
main (void)
{
  ffi_closure *closure;
  void *code = NULL;

  real_mmap = (mmap_fn) dlsym (RTLD_NEXT, "mmap");
  CHECK (real_mmap != NULL);

  reject_exec_mappings = 1;

  /* The failure path must terminate instead of cycling through temporary
     backing files forever.  */
  alarm (5);
  closure = ffi_closure_alloc (sizeof (ffi_closure), &code);
  alarm (0);

  CHECK (closure == NULL);
  CHECK (code == NULL);

  exit (0);
}

#else
int main (void) { return 0; }
#endif
