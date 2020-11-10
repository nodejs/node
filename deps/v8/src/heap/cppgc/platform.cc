// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/platform.h"

#include "src/base/platform/platform.h"
#include "src/heap/cppgc/gc-info-table.h"

namespace cppgc {
namespace internal {

static PageAllocator* g_page_allocator;

}  // namespace internal

void InitializePlatform(PageAllocator* page_allocator) {
  internal::g_page_allocator = page_allocator;
  internal::GlobalGCInfoTable::Create(page_allocator);
}

void ShutdownPlatform() { internal::g_page_allocator = nullptr; }

namespace internal {

void Abort() { v8::base::OS::Abort(); }

}  // namespace internal
}  // namespace cppgc
