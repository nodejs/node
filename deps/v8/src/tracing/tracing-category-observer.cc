// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/tracing/tracing-category-observer.h"

#include "include/v8-platform.h"
#include "src/base/atomic-utils.h"
#include "src/init/v8.h"
#include "src/logging/counters.h"
#include "src/logging/tracing-flags.h"
#include "src/tracing/trace-event.h"

namespace v8 {
namespace tracing {

class TracingCategoryObserverImpl
#if defined(V8_USE_PERFETTO)
    : public perfetto::TrackEventSessionObserver {
#else
    : public TracingController::TraceStateObserver {
#endif
 public:
#if defined(V8_USE_PERFETTO)
  // perfetto::TrackEventSessionObserver
  void OnStart(const perfetto::DataSourceBase::StartArgs&) override;
  void OnStop(const perfetto::DataSourceBase::StopArgs&) override;
#else
  // v8::TracingController::TraceStateObserver
  void OnTraceEnabled() final;
  void OnTraceDisabled() final;
#endif
};

static TracingCategoryObserverImpl* instance_ = nullptr;

void TracingCategoryObserver::SetUp() {
  instance_ = new TracingCategoryObserverImpl();
#if defined(V8_USE_PERFETTO)
  TrackEvent::AddSessionObserver(instance_);
  // Fire the observer if tracing is already in progress.
  if (TrackEvent::IsEnabled()) instance_->OnStart({});
#else
  i::V8::GetCurrentPlatform()->GetTracingController()->AddTraceStateObserver(
      instance_);
#endif
}

void TracingCategoryObserver::TearDown() {
#if defined(V8_USE_PERFETTO)
  TrackEvent::RemoveSessionObserver(instance_);
#else
  i::V8::GetCurrentPlatform()->GetTracingController()->RemoveTraceStateObserver(
      instance_);
#endif
  delete instance_;
}

#if defined(V8_USE_PERFETTO)
void TracingCategoryObserverImpl::OnStart(
    const perfetto::DataSourceBase::StartArgs&) {
#else
void TracingCategoryObserverImpl::OnTraceEnabled() {
#endif
  bool enabled = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("v8.runtime_stats"), &enabled);
  if (enabled) {
    i::TracingFlags::runtime_stats.fetch_or(
        TracingCategoryObserver::ENABLED_BY_TRACING, std::memory_order_relaxed);
  }
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("v8.runtime_stats_sampling"), &enabled);
  if (enabled) {
    i::TracingFlags::runtime_stats.fetch_or(
        TracingCategoryObserver::ENABLED_BY_SAMPLING,
        std::memory_order_relaxed);
  }
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
                                     &enabled);
  if (enabled) {
    i::TracingFlags::gc.fetch_or(TracingCategoryObserver::ENABLED_BY_TRACING,
                                 std::memory_order_relaxed);
  }
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("v8.gc_stats"),
                                     &enabled);
  if (enabled) {
    i::TracingFlags::gc_stats.fetch_or(
        TracingCategoryObserver::ENABLED_BY_TRACING, std::memory_order_relaxed);
  }
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("v8.ic_stats"),
                                     &enabled);
  if (enabled) {
    i::TracingFlags::ic_stats.fetch_or(
        TracingCategoryObserver::ENABLED_BY_TRACING, std::memory_order_relaxed);
  }

  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("v8.zone_stats"),
                                     &enabled);
  if (enabled) {
    i::TracingFlags::zone_stats.fetch_or(
        TracingCategoryObserver::ENABLED_BY_TRACING, std::memory_order_relaxed);
  }
}

#if defined(V8_USE_PERFETTO)
void TracingCategoryObserverImpl::OnStop(
    const perfetto::DataSourceBase::StopArgs&) {
#else
void TracingCategoryObserverImpl::OnTraceDisabled() {
#endif
  i::TracingFlags::runtime_stats.fetch_and(
      ~(TracingCategoryObserver::ENABLED_BY_TRACING |
        TracingCategoryObserver::ENABLED_BY_SAMPLING),
      std::memory_order_relaxed);

  i::TracingFlags::gc.fetch_and(~TracingCategoryObserver::ENABLED_BY_TRACING,
                                std::memory_order_relaxed);

  i::TracingFlags::gc_stats.fetch_and(
      ~TracingCategoryObserver::ENABLED_BY_TRACING, std::memory_order_relaxed);

  i::TracingFlags::ic_stats.fetch_and(
      ~TracingCategoryObserver::ENABLED_BY_TRACING, std::memory_order_relaxed);
}

}  // namespace tracing
}  // namespace v8
