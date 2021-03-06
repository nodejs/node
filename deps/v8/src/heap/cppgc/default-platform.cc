// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <include/cppgc/default-platform.h>

#if !CPPGC_IS_STANDALONE
#include <v8.h>
#endif  // !CPPGC_IS_STANDALONE

namespace cppgc {

// static
void DefaultPlatform::InitializeProcess(DefaultPlatform* platform) {
#if CPPGC_IS_STANDALONE
  cppgc::InitializeProcess(platform->GetPageAllocator());
#else
  // v8::V8::InitializePlatform transitively calls cppgc::InitializeProcess.
  v8::V8::InitializePlatform(platform->v8_platform_.get());
#endif  // CPPGC_IS_STANDALONE
}

}  // namespace cppgc
