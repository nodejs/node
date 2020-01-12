/* Copyright libuv contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "uv.h"
#include "internal.h"

#include <errno.h>
#include <string.h>

#include <syscall.h>
#include <unistd.h>


struct uv__sysctl_args {
  int* name;
  int nlen;
  void* oldval;
  size_t* oldlenp;
  void* newval;
  size_t newlen;
  unsigned long unused[4];
};


int uv__random_sysctl(void* buf, size_t buflen) {
  static int name[] = {1 /*CTL_KERN*/, 40 /*KERN_RANDOM*/, 6 /*RANDOM_UUID*/};
  struct uv__sysctl_args args;
  char uuid[16];
  char* p;
  char* pe;
  size_t n;

  p = buf;
  pe = p + buflen;

  while (p < pe) {
    memset(&args, 0, sizeof(args));

    args.name = name;
    args.nlen = ARRAY_SIZE(name);
    args.oldval = uuid;
    args.oldlenp = &n;
    n = sizeof(uuid);

    /* Emits a deprecation warning with some kernels but that seems like
     * an okay trade-off for the fallback of the fallback: this function is
     * only called when neither getrandom(2) nor /dev/urandom are available.
     * Fails with ENOSYS on kernels configured without CONFIG_SYSCTL_SYSCALL.
     * At least arm64 never had a _sysctl system call and therefore doesn't
     * have a SYS__sysctl define either.
     */
#ifdef SYS__sysctl
    if (syscall(SYS__sysctl, &args) == -1)
      return UV__ERR(errno);
#else
    {
      (void) &args;
      return UV_ENOSYS;
    }
#endif

    if (n != sizeof(uuid))
      return UV_EIO;  /* Can't happen. */

    /* uuid[] is now a type 4 UUID. Bytes 6 and 8 (counting from zero) contain
     * 4 and 5 bits of entropy, respectively. For ease of use, we skip those
     * and only use 14 of the 16 bytes.
     */
    uuid[6] = uuid[14];
    uuid[8] = uuid[15];

    n = pe - p;
    if (n > 14)
      n = 14;

    memcpy(p, uuid, n);
    p += n;
  }

  return 0;
}
