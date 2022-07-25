// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/platform.h"

#include "src/base/lazy-instance.h"
#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/base/platform/platform.h"
#include "src/base/sanitizer/asan.h"
#include "src/heap/cppgc/gc-info-table.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/platform.h"

namespace cppgc {
namespace internal {

void Fatal(const std::string& reason, const SourceLocation& loc) {
#ifdef DEBUG
  V8_Fatal(loc.FileName(), static_cast<int>(loc.Line()), "%s", reason.c_str());
#else   // !DEBUG
  V8_Fatal("%s", reason.c_str());
#endif  // !DEBUG
}

void FatalOutOfMemoryHandler::operator()(const std::string& reason,
                                         const SourceLocation& loc) const {
  if (custom_handler_) {
    (*custom_handler_)(reason, loc, heap_);
    FATAL("Custom out of memory handler should not have returned");
  }
#ifdef DEBUG
  V8_Fatal(loc.FileName(), static_cast<int>(loc.Line()),
           "Oilpan: Out of memory (%s)", reason.c_str());
#else   // !DEBUG
  V8_Fatal("Oilpan: Out of memory");
#endif  // !DEBUG
}

void FatalOutOfMemoryHandler::SetCustomHandler(Callback* callback) {
  custom_handler_ = callback;
}

}  // namespace internal

namespace {
PageAllocator* g_page_allocator = nullptr;
}  // namespace

TracingController* Platform::GetTracingController() {
  static v8::base::LeakyObject<TracingController> tracing_controller;
  return tracing_controller.get();
}

void InitializeProcess(PageAllocator* page_allocator) {
#if defined(V8_USE_ADDRESS_SANITIZER) && defined(V8_TARGET_ARCH_64_BIT)
  // Retrieve asan's internal shadow memory granularity and check that Oilpan's
  // object alignment/sizes are multiple of this granularity. This is needed to
  // perform poisoness checks.
  size_t shadow_scale;
  __asan_get_shadow_mapping(&shadow_scale, nullptr);
  DCHECK(shadow_scale);
  const size_t poisoning_granularity = 1 << shadow_scale;
  CHECK_EQ(0u, internal::kAllocationGranularity % poisoning_granularity);
#endif

  CHECK(!g_page_allocator);
  internal::GlobalGCInfoTable::Initialize(page_allocator);
  g_page_allocator = page_allocator;
}

void ShutdownProcess() { g_page_allocator = nullptr; }

}  // namespace cppgc
