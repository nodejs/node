#ifndef _WIN32

#if defined(_AIX)
#include <stddef.h>
#include <dlfcn.h>
#else
const char* dlopen_pong(void);
#endif

__attribute__((visibility("default")))
const char* dlopen_ping(void) {
#if defined(_AIX)
  /* ping.so is built with -Wl,-G: link-time import of dlopen_pong is resolved
   * when ping.so is loaded. On --shared libnode builds that resolution can fail
   * even when dlopen_pong appears in binding's loader table (dump). Resolve
   * at call time via the global symbol set after process.dlopen(RTLD_GLOBAL). */
  typedef const char* (*pong_fn)(void);
  pong_fn pong =
      (pong_fn)dlsym(RTLD_DEFAULT, "dlopen_pong");
  return pong != NULL ? pong() : NULL;
#else
  return dlopen_pong();
#endif
}

#endif  // _WIN32
