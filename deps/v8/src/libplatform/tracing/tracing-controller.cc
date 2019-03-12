// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/libplatform/v8-tracing.h"

#include "src/base/atomicops.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/time.h"

namespace v8 {
namespace platform {
namespace tracing {

#define MAX_CATEGORY_GROUPS 200

// Parallel arrays g_category_groups and g_category_group_enabled are separate
// so that a pointer to a member of g_category_group_enabled can be easily
// converted to an index into g_category_groups. This allows macros to deal
// only with char enabled pointers from g_category_group_enabled, and we can
// convert internally to determine the category name from the char enabled
// pointer.
const char* g_category_groups[MAX_CATEGORY_GROUPS] = {
    "toplevel",
    "tracing categories exhausted; must increase MAX_CATEGORY_GROUPS",
    "__metadata"};

// The enabled flag is char instead of bool so that the API can be used from C.
unsigned char g_category_group_enabled[MAX_CATEGORY_GROUPS] = {0};
// Indexes here have to match the g_category_groups array indexes above.
const int g_category_categories_exhausted = 1;
// Metadata category not used in V8.
// const int g_category_metadata = 2;
const int g_num_builtin_categories = 3;

// Skip default categories.
v8::base::AtomicWord g_category_index = g_num_builtin_categories;

TracingController::TracingController() = default;

TracingController::~TracingController() {
  StopTracing();

  {
    // Free memory for category group names allocated via strdup.
    base::MutexGuard lock(mutex_.get());
    for (size_t i = g_category_index - 1; i >= g_num_builtin_categories; --i) {
      const char* group = g_category_groups[i];
      g_category_groups[i] = nullptr;
      free(const_cast<char*>(group));
    }
    g_category_index = g_num_builtin_categories;
  }
}

void TracingController::Initialize(TraceBuffer* trace_buffer) {
  trace_buffer_.reset(trace_buffer);
  mutex_.reset(new base::Mutex());
}

int64_t TracingController::CurrentTimestampMicroseconds() {
  return base::TimeTicks::HighResolutionNow().ToInternalValue();
}

int64_t TracingController::CurrentCpuTimestampMicroseconds() {
  return base::ThreadTicks::Now().ToInternalValue();
}

uint64_t TracingController::AddTraceEvent(
    char phase, const uint8_t* category_enabled_flag, const char* name,
    const char* scope, uint64_t id, uint64_t bind_id, int num_args,
    const char** arg_names, const uint8_t* arg_types,
    const uint64_t* arg_values,
    std::unique_ptr<v8::ConvertableToTraceFormat>* arg_convertables,
    unsigned int flags) {
  uint64_t handle = 0;
  if (mode_ != DISABLED) {
    TraceObject* trace_object = trace_buffer_->AddTraceEvent(&handle);
    if (trace_object) {
      trace_object->Initialize(
          phase, category_enabled_flag, name, scope, id, bind_id, num_args,
          arg_names, arg_types, arg_values, arg_convertables, flags,
          CurrentTimestampMicroseconds(), CurrentCpuTimestampMicroseconds());
    }
  }
  return handle;
}

uint64_t TracingController::AddTraceEventWithTimestamp(
    char phase, const uint8_t* category_enabled_flag, const char* name,
    const char* scope, uint64_t id, uint64_t bind_id, int num_args,
    const char** arg_names, const uint8_t* arg_types,
    const uint64_t* arg_values,
    std::unique_ptr<v8::ConvertableToTraceFormat>* arg_convertables,
    unsigned int flags, int64_t timestamp) {
  uint64_t handle = 0;
  if (mode_ != DISABLED) {
    TraceObject* trace_object = trace_buffer_->AddTraceEvent(&handle);
    if (trace_object) {
      trace_object->Initialize(phase, category_enabled_flag, name, scope, id,
                               bind_id, num_args, arg_names, arg_types,
                               arg_values, arg_convertables, flags, timestamp,
                               CurrentCpuTimestampMicroseconds());
    }
  }
  return handle;
}

void TracingController::UpdateTraceEventDuration(
    const uint8_t* category_enabled_flag, const char* name, uint64_t handle) {
  TraceObject* trace_object = trace_buffer_->GetEventByHandle(handle);
  if (!trace_object) return;
  trace_object->UpdateDuration(CurrentTimestampMicroseconds(),
                               CurrentCpuTimestampMicroseconds());
}

const uint8_t* TracingController::GetCategoryGroupEnabled(
    const char* category_group) {
  return GetCategoryGroupEnabledInternal(category_group);
}

const char* TracingController::GetCategoryGroupName(
    const uint8_t* category_group_enabled) {
  // Calculate the index of the category group by finding
  // category_group_enabled in g_category_group_enabled array.
  uintptr_t category_begin =
      reinterpret_cast<uintptr_t>(g_category_group_enabled);
  uintptr_t category_ptr = reinterpret_cast<uintptr_t>(category_group_enabled);
  // Check for out of bounds category pointers.
  DCHECK(category_ptr >= category_begin &&
         category_ptr < reinterpret_cast<uintptr_t>(g_category_group_enabled +
                                                    MAX_CATEGORY_GROUPS));
  uintptr_t category_index =
      (category_ptr - category_begin) / sizeof(g_category_group_enabled[0]);
  return g_category_groups[category_index];
}

void TracingController::StartTracing(TraceConfig* trace_config) {
  trace_config_.reset(trace_config);
  std::unordered_set<v8::TracingController::TraceStateObserver*> observers_copy;
  {
    base::MutexGuard lock(mutex_.get());
    mode_ = RECORDING_MODE;
    UpdateCategoryGroupEnabledFlags();
    observers_copy = observers_;
  }
  for (auto o : observers_copy) {
    o->OnTraceEnabled();
  }
}

void TracingController::StopTracing() {
  if (mode_ == DISABLED) {
    return;
  }
  DCHECK(trace_buffer_);
  mode_ = DISABLED;
  UpdateCategoryGroupEnabledFlags();
  std::unordered_set<v8::TracingController::TraceStateObserver*> observers_copy;
  {
    base::MutexGuard lock(mutex_.get());
    observers_copy = observers_;
  }
  for (auto o : observers_copy) {
    o->OnTraceDisabled();
  }
  trace_buffer_->Flush();
}

void TracingController::UpdateCategoryGroupEnabledFlag(size_t category_index) {
  unsigned char enabled_flag = 0;
  const char* category_group = g_category_groups[category_index];
  if (mode_ == RECORDING_MODE &&
      trace_config_->IsCategoryGroupEnabled(category_group)) {
    enabled_flag |= ENABLED_FOR_RECORDING;
  }

  // TODO(fmeawad): EventCallback and ETW modes are not yet supported in V8.
  // TODO(primiano): this is a temporary workaround for catapult:#2341,
  // to guarantee that metadata events are always added even if the category
  // filter is "-*". See crbug.com/618054 for more details and long-term fix.
  if (mode_ == RECORDING_MODE && !strcmp(category_group, "__metadata")) {
    enabled_flag |= ENABLED_FOR_RECORDING;
  }

  base::Relaxed_Store(reinterpret_cast<base::Atomic8*>(
                          g_category_group_enabled + category_index),
                      enabled_flag);
}

void TracingController::UpdateCategoryGroupEnabledFlags() {
  size_t category_index = base::Relaxed_Load(&g_category_index);
  for (size_t i = 0; i < category_index; i++) UpdateCategoryGroupEnabledFlag(i);
}

const uint8_t* TracingController::GetCategoryGroupEnabledInternal(
    const char* category_group) {
  // Check that category groups does not contain double quote
  DCHECK(!strchr(category_group, '"'));

  // The g_category_groups is append only, avoid using a lock for the fast path.
  size_t category_index = base::Acquire_Load(&g_category_index);

  // Search for pre-existing category group.
  for (size_t i = 0; i < category_index; ++i) {
    if (strcmp(g_category_groups[i], category_group) == 0) {
      return &g_category_group_enabled[i];
    }
  }

  // Slow path. Grab the lock.
  base::MutexGuard lock(mutex_.get());

  // Check the list again with lock in hand.
  unsigned char* category_group_enabled = nullptr;
  category_index = base::Acquire_Load(&g_category_index);
  for (size_t i = 0; i < category_index; ++i) {
    if (strcmp(g_category_groups[i], category_group) == 0) {
      return &g_category_group_enabled[i];
    }
  }

  // Create a new category group.
  // Check that there is a slot for the new category_group.
  DCHECK(category_index < MAX_CATEGORY_GROUPS);
  if (category_index < MAX_CATEGORY_GROUPS) {
    // Don't hold on to the category_group pointer, so that we can create
    // category groups with strings not known at compile time (this is
    // required by SetWatchEvent).
    const char* new_group = strdup(category_group);
    g_category_groups[category_index] = new_group;
    DCHECK(!g_category_group_enabled[category_index]);
    // Note that if both included and excluded patterns in the
    // TraceConfig are empty, we exclude nothing,
    // thereby enabling this category group.
    UpdateCategoryGroupEnabledFlag(category_index);
    category_group_enabled = &g_category_group_enabled[category_index];
    // Update the max index now.
    base::Release_Store(&g_category_index, category_index + 1);
  } else {
    category_group_enabled =
        &g_category_group_enabled[g_category_categories_exhausted];
  }
  return category_group_enabled;
}

void TracingController::AddTraceStateObserver(
    v8::TracingController::TraceStateObserver* observer) {
  {
    base::MutexGuard lock(mutex_.get());
    observers_.insert(observer);
    if (mode_ != RECORDING_MODE) return;
  }
  // Fire the observer if recording is already in progress.
  observer->OnTraceEnabled();
}

void TracingController::RemoveTraceStateObserver(
    v8::TracingController::TraceStateObserver* observer) {
  base::MutexGuard lock(mutex_.get());
  DCHECK(observers_.find(observer) != observers_.end());
  observers_.erase(observer);
}

}  // namespace tracing
}  // namespace platform
}  // namespace v8
