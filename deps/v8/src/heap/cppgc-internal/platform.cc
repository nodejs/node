// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/platform.h"

#include "include/cppgc/common.h"
#include "src/base/lazy-instance.h"
#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/base/page-allocator.h"
#include "src/base/platform/platform.h"
#include "src/base/sanitizer/asan.h"
#include "src/base/sanitizer/lsan-page-allocator.h"
#include "src/base/synchronization-point-support.h"
#include "src/heap/cppgc-internal/gc-info-table.h"
#include "src/heap/cppgc-internal/globals.h"
#include "src/heap/cppgc-internal/platform.h"

#if defined(CPPGC_CAGED_HEAP)
#include "src/heap/cppgc-internal/caged-heap.h"
#endif  // defined(CPPGC_CAGED_HEAP)

namespace cppgc {
namespace internal {

namespace {

PageAllocator* g_page_allocator = nullptr;

PageAllocator& CreateAllocatorIfNeeded(PageAllocator* page_allocator) {
  if (!page_allocator) {
    static v8::base::LeakyObject<v8::base::PageAllocator>
        default_page_allocator;
    page_allocator = default_page_allocator.get();
  }
#if defined(LEAK_SANITIZER)
  // If lsan is enabled, override the given allocator with the custom lsan
  // allocator.
  static v8::base::LeakyObject<v8::base::LsanPageAllocator> lsan_page_allocator(
      page_allocator);
  page_allocator = lsan_page_allocator.get();
#endif  // LEAK_SANITIZER
  return *page_allocator;
}

}  // namespace

void Fatal(const std::string& reason, SourceLocation loc) {
  FATAL_WITH_LOC(loc, "%s", reason.c_str());
}

void FatalOutOfMemoryHandler::operator()(const std::string& reason,
                                         SourceLocation loc) const {
  if (custom_handler_) {
    (*custom_handler_)(reason, loc, heap_);
    FATAL("Custom out of memory handler should not have returned");
  }
  FATAL_WITH_LOC(loc, "Oilpan: Out of memory (%s)", reason.c_str());
}

void FatalOutOfMemoryHandler::SetCustomHandler(Callback* callback) {
  custom_handler_ = callback;
}

FatalOutOfMemoryHandler& GetGlobalOOMHandler() {
  static FatalOutOfMemoryHandler oom_handler;
  return oom_handler;
}

PageAllocator& GetGlobalPageAllocator() {
  CHECK_NOT_NULL(g_page_allocator);
  return *g_page_allocator;
}

}  // namespace internal

TracingController* Platform::GetTracingController() {
  static v8::base::LeakyObject<TracingController> tracing_controller;
  return tracing_controller.get();
}

bool IsInitialized() { return internal::g_page_allocator != nullptr; }

void InitializeProcess(PageAllocator* page_allocator,
                       size_t desired_heap_size) {
#if defined(V8_USE_ADDRESS_SANITIZER) && defined(V8_HOST_ARCH_64_BIT)
  // Retrieve asan's internal shadow memory granularity and check that Oilpan's
  // object alignment/sizes are multiple of this granularity. This is needed to
  // perform poisoness checks.
  size_t shadow_scale;
  __asan_get_shadow_mapping(&shadow_scale, nullptr);
  DCHECK(shadow_scale);
  const size_t poisoning_granularity = 1 << shadow_scale;
  CHECK_EQ(0u, internal::kAllocationGranularity % poisoning_granularity);
#endif

  auto& allocator = internal::CreateAllocatorIfNeeded(page_allocator);

  CHECK(!internal::g_page_allocator);
  v8::base::SynchronizationPointSupport::Initialize();
  internal::GlobalGCInfoTable::Initialize(allocator);
#if defined(CPPGC_CAGED_HEAP)
  internal::CagedHeap::InitializeIfNeeded(allocator, desired_heap_size);
#endif  // defined(CPPGC_CAGED_HEAP)
  internal::g_page_allocator = &allocator;
}

void ShutdownProcess() { internal::g_page_allocator = nullptr; }

StackStartScope::StackStartScope(void* stack_start)
    : previous_stack_start_(
          v8::base::Stack::SetCurrentThreadStackStart(stack_start)) {
  CHECK_NOT_NULL(stack_start);
  CHECK_IMPLIES(previous_stack_start_, stack_start <= previous_stack_start_);
}

StackStartScope::~StackStartScope() {
  void* stack_start =
      v8::base::Stack::SetCurrentThreadStackStart(previous_stack_start_);
  CHECK_NOT_NULL(stack_start);
  CHECK_IMPLIES(previous_stack_start_, stack_start <= previous_stack_start_);
}

}  // namespace cppgc
