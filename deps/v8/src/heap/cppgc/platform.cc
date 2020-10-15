// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/platform.h"

#include "src/base/platform/platform.h"
#include "src/heap/cppgc/gc-info-table.h"

namespace cppgc {

void InitializeProcess(PageAllocator* page_allocator) {
  internal::GlobalGCInfoTable::Create(page_allocator);
}

void ShutdownProcess() {}

namespace internal {

void Abort() { v8::base::OS::Abort(); }

}  // namespace internal
}  // namespace cppgc
