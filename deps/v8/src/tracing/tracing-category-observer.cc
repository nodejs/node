// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/tracing/tracing-category-observer.h"

#include "src/flags.h"
#include "src/tracing/trace-event.h"
#include "src/v8.h"

namespace v8 {
namespace tracing {

TracingCategoryObserver* TracingCategoryObserver::instance_ = nullptr;

void TracingCategoryObserver::SetUp() {
  TracingCategoryObserver::instance_ = new TracingCategoryObserver();
  v8::internal::V8::GetCurrentPlatform()->AddTraceStateObserver(
      TracingCategoryObserver::instance_);
  TRACE_EVENT_WARMUP_CATEGORY(TRACE_DISABLED_BY_DEFAULT("v8.runtime_stats"));
  TRACE_EVENT_WARMUP_CATEGORY(
      TRACE_DISABLED_BY_DEFAULT("v8.runtime_stats_sampling"));
  TRACE_EVENT_WARMUP_CATEGORY(TRACE_DISABLED_BY_DEFAULT("v8.gc_stats"));
  TRACE_EVENT_WARMUP_CATEGORY(TRACE_DISABLED_BY_DEFAULT("v8.ic_stats"));
}

void TracingCategoryObserver::TearDown() {
  v8::internal::V8::GetCurrentPlatform()->RemoveTraceStateObserver(
      TracingCategoryObserver::instance_);
  delete TracingCategoryObserver::instance_;
}

void TracingCategoryObserver::OnTraceEnabled() {
  bool enabled = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("v8.runtime_stats"), &enabled);
  if (enabled) {
    v8::internal::FLAG_runtime_stats |= ENABLED_BY_TRACING;
  }
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("v8.runtime_stats_sampling"), &enabled);
  if (enabled) {
    v8::internal::FLAG_runtime_stats |= ENABLED_BY_SAMPLING;
  }
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("v8.gc_stats"),
                                     &enabled);
  if (enabled) {
    v8::internal::FLAG_gc_stats |= ENABLED_BY_TRACING;
  }
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("v8.ic_stats"),
                                     &enabled);
  if (enabled) {
    v8::internal::FLAG_ic_stats |= ENABLED_BY_TRACING;
  }
}

void TracingCategoryObserver::OnTraceDisabled() {
  v8::internal::FLAG_runtime_stats &=
      ~(ENABLED_BY_TRACING | ENABLED_BY_SAMPLING);
  v8::internal::FLAG_gc_stats &= ~ENABLED_BY_TRACING;
  v8::internal::FLAG_ic_stats &= ~ENABLED_BY_TRACING;
}

}  // namespace tracing
}  // namespace v8
