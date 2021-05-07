// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/platform.h"

#include "src/base/lazy-instance.h"
#include "src/base/platform/platform.h"
#include "src/heap/cppgc/gc-info-table.h"

namespace cppgc {

namespace {
PageAllocator* g_page_allocator = nullptr;
}  // namespace

TracingController* Platform::GetTracingController() {
  static v8::base::LeakyObject<TracingController> tracing_controller;
  return tracing_controller.get();
}

void InitializeProcess(PageAllocator* page_allocator) {
  CHECK(!g_page_allocator);
  internal::GlobalGCInfoTable::Initialize(page_allocator);
  g_page_allocator = page_allocator;
}

void ShutdownProcess() { g_page_allocator = nullptr; }

namespace internal {

void Abort() { v8::base::OS::Abort(); }

}  // namespace internal
}  // namespace cppgc
