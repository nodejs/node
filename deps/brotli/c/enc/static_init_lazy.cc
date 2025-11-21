#include "../common/platform.h"
#include "../common/static_init.h"
#include "static_init.h"

#if (BROTLI_STATIC_INIT != BROTLI_STATIC_INIT_LAZY)
#error "BROTLI_STATIC_INIT should be BROTLI_STATIC_INIT_LAZY"
#else
void BrotliEncoderLazyStaticInit(void) {
  /* From https://en.cppreference.com/w/cpp/language/storage_duration.html:
     ### Static block variables ###
     Block variables with static or thread (since C++11) storage duration are
     initialized the first time control passes through their declaration...
     On all further calls, the declaration is skipped...
     If multiple threads attempt to initialize the same static local variable
     concurrently, the initialization occurs exactly once...
     Usual implementations of this feature use variants of the double-checked
     locking pattern, which reduces runtime overhead for already-initialized
     local statics to a single non-atomic boolean comparison.
   */
  static bool ok = [](){
    BrotliEncoderLazyStaticInitInner();
    return true;
  }();
  if (!ok) BROTLI_CRASH();
}
#endif  /* BROTLI_STATIC_INIT_LAZY */
