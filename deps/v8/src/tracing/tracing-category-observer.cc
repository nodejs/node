// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/tracing/tracing-category-observer.h"

#include "src/base/atomic-utils.h"
#include "src/init/v8.h"
#include "src/logging/counters.h"
#include "src/tracing/trace-event.h"

namespace v8 {
namespace tracing {

TracingCategoryObserver* TracingCategoryObserver::instance_ = nullptr;

void TracingCategoryObserver::SetUp() {
  TracingCategoryObserver::instance_ = new TracingCategoryObserver();
  i::V8::GetCurrentPlatform()->GetTracingController()->AddTraceStateObserver(
      TracingCategoryObserver::instance_);
}

void TracingCategoryObserver::TearDown() {
  i::V8::GetCurrentPlatform()->GetTracingController()->RemoveTraceStateObserver(
      TracingCategoryObserver::instance_);
  delete TracingCategoryObserver::instance_;
}

void TracingCategoryObserver::OnTraceEnabled() {
  bool enabled = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("v8.runtime_stats"), &enabled);
  if (enabled) {
    i::TracingFlags::runtime_stats.fetch_or(ENABLED_BY_TRACING,
                                            std::memory_order_relaxed);
  }
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("v8.runtime_stats_sampling"), &enabled);
  if (enabled) {
    i::TracingFlags::runtime_stats.fetch_or(ENABLED_BY_SAMPLING,
                                            std::memory_order_relaxed);
  }
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
                                     &enabled);
  if (enabled) {
    i::TracingFlags::gc.fetch_or(ENABLED_BY_TRACING, std::memory_order_relaxed);
  }
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("v8.gc_stats"),
                                     &enabled);
  if (enabled) {
    i::TracingFlags::gc_stats.fetch_or(ENABLED_BY_TRACING,
                                       std::memory_order_relaxed);
  }
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("v8.ic_stats"),
                                     &enabled);
  if (enabled) {
    i::TracingFlags::ic_stats.fetch_or(ENABLED_BY_TRACING,
                                       std::memory_order_relaxed);
  }
}

void TracingCategoryObserver::OnTraceDisabled() {
  i::TracingFlags::runtime_stats.fetch_and(
      ~(ENABLED_BY_TRACING | ENABLED_BY_SAMPLING), std::memory_order_relaxed);

  i::TracingFlags::gc.fetch_and(~ENABLED_BY_TRACING, std::memory_order_relaxed);

  i::TracingFlags::gc_stats.fetch_and(~ENABLED_BY_TRACING,
                                      std::memory_order_relaxed);

  i::TracingFlags::ic_stats.fetch_and(~ENABLED_BY_TRACING,
                                      std::memory_order_relaxed);
}

}  // namespace tracing
}  // namespace v8
