// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <include/cppgc/default-platform.h>

namespace cppgc {

// static
void DefaultPlatform::InitializeProcess(DefaultPlatform* platform) {
  cppgc::InitializeProcess(platform->GetPageAllocator());
}

}  // namespace cppgc
