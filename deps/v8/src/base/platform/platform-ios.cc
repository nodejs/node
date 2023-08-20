// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/base-export.h"
#include "src/base/build_config.h"

// pthread_jit_write_protect_np is marked as not available in the iOS
// SDK but it is there for the iOS simulator. So we provide a thunk
// and a forward declaration in a compilation target that doesn't
// include pthread.h to avoid the compiler error.
extern "C" void pthread_jit_write_protect_np(int enable);

namespace v8::base {

#if V8_HAS_PTHREAD_JIT_WRITE_PROTECT && defined(V8_OS_IOS)
V8_BASE_EXPORT void SetJitWriteProtected(int enable) {
  pthread_jit_write_protect_np(enable);
}
#endif

}  // namespace v8::base
