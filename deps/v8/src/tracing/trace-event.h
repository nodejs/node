// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_TRACING_TRACE_EVENT_H_
#define SRC_TRACING_TRACE_EVENT_H_

#include <stddef.h>

#include "base/trace_event/common/trace_event_common.h"
#include "include/v8-platform.h"
#include "src/base/atomicops.h"

// This header file defines implementation details of how the trace macros in
// trace_event_common.h collect and store trace events. Anything not
// implementation-specific should go in trace_macros_common.h instead of here.


// The pointer returned from GetCategoryGroupEnabled() points to a
// value with zero or more of the following bits. Used in this class only.
// The TRACE_EVENT macros should only use the value as a bool.
// These values must be in sync with macro values in trace_log.h in
// chromium.
enum CategoryGroupEnabledFlags {
  // Category group enabled for the recording mode.
  kEnabledForRecording_CategoryGroupEnabledFlags = 1 << 0,
  // Category group enabled by SetEventCallbackEnabled().
  kEnabledForEventCallback_CategoryGroupEnabledFlags = 1 << 2,
  // Category group enabled to export events to ETW.
  kEnabledForETWExport_CategoryGroupEnabledFlags = 1 << 3,
};

// By default, const char* asrgument values are assumed to have long-lived scope
// and will not be copied. Use this macro to force a const char* to be copied.
#define TRACE_STR_COPY(str) v8::internal::tracing::TraceStringWithCopy(str)

// By default, uint64 ID argument values are not mangled with the Process ID in
// TRACE_EVENT_ASYNC macros. Use this macro to force Process ID mangling.
#define TRACE_ID_MANGLE(id) v8::internal::tracing::TraceID::ForceMangle(id)

// By default, pointers are mangled with the Process ID in TRACE_EVENT_ASYNC
// macros. Use this macro to prevent Process ID mangling.
#define TRACE_ID_DONT_MANGLE(id) v8::internal::tracing::TraceID::DontMangle(id)

// Sets the current sample state to the given category and name (both must be
// constant strings). These states are intended for a sampling profiler.
// Implementation note: we store category and name together because we don't
// want the inconsistency/expense of storing two pointers.
// |thread_bucket| is [0..2] and is used to statically isolate samples in one
// thread from others.
#define TRACE_EVENT_SET_SAMPLING_STATE_FOR_BUCKET(bucket_number, category, \
                                                  name)                    \
  v8::internal::tracing::TraceEventSamplingStateScope<bucket_number>::Set( \
      category "\0" name)

// Returns a current sampling state of the given bucket.
#define TRACE_EVENT_GET_SAMPLING_STATE_FOR_BUCKET(bucket_number) \
  v8::internal::tracing::TraceEventSamplingStateScope<bucket_number>::Current()

// Creates a scope of a sampling state of the given bucket.
//
// {  // The sampling state is set within this scope.
//    TRACE_EVENT_SAMPLING_STATE_SCOPE_FOR_BUCKET(0, "category", "name");
//    ...;
// }
#define TRACE_EVENT_SCOPED_SAMPLING_STATE_FOR_BUCKET(bucket_number, category, \
                                                     name)                    \
  v8::internal::TraceEventSamplingStateScope<bucket_number>                   \
      traceEventSamplingScope(category "\0" name);


#define INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED_FOR_RECORDING_MODE() \
  *INTERNAL_TRACE_EVENT_UID(category_group_enabled) &                    \
      (kEnabledForRecording_CategoryGroupEnabledFlags |                  \
       kEnabledForEventCallback_CategoryGroupEnabledFlags)

// The following macro has no implementation, but it needs to exist since
// it gets called from scoped trace events. It cannot call UNIMPLEMENTED()
// since an empty implementation is a valid one.
#define INTERNAL_TRACE_MEMORY(category, name)

////////////////////////////////////////////////////////////////////////////////
// Implementation specific tracing API definitions.

// Get a pointer to the enabled state of the given trace category. Only
// long-lived literal strings should be given as the category group. The
// returned pointer can be held permanently in a local static for example. If
// the unsigned char is non-zero, tracing is enabled. If tracing is enabled,
// TRACE_EVENT_API_ADD_TRACE_EVENT can be called. It's OK if tracing is disabled
// between the load of the tracing state and the call to
// TRACE_EVENT_API_ADD_TRACE_EVENT, because this flag only provides an early out
// for best performance when tracing is disabled.
// const uint8_t*
//     TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(const char* category_group)
#define TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED              \
  v8::internal::tracing::TraceEventHelper::GetCurrentPlatform() \
      ->GetCategoryGroupEnabled

// Get the number of times traces have been recorded. This is used to implement
// the TRACE_EVENT_IS_NEW_TRACE facility.
// unsigned int TRACE_EVENT_API_GET_NUM_TRACES_RECORDED()
#define TRACE_EVENT_API_GET_NUM_TRACES_RECORDED UNIMPLEMENTED()

// Add a trace event to the platform tracing system.
// uint64_t TRACE_EVENT_API_ADD_TRACE_EVENT(
//                    char phase,
//                    const uint8_t* category_group_enabled,
//                    const char* name,
//                    uint64_t id,
//                    uint64_t bind_id,
//                    int num_args,
//                    const char** arg_names,
//                    const uint8_t* arg_types,
//                    const uint64_t* arg_values,
//                    unsigned int flags)
#define TRACE_EVENT_API_ADD_TRACE_EVENT \
  v8::internal::tracing::TraceEventHelper::GetCurrentPlatform()->AddTraceEvent

// Set the duration field of a COMPLETE trace event.
// void TRACE_EVENT_API_UPDATE_TRACE_EVENT_DURATION(
//     const uint8_t* category_group_enabled,
//     const char* name,
//     uint64_t id)
#define TRACE_EVENT_API_UPDATE_TRACE_EVENT_DURATION             \
  v8::internal::tracing::TraceEventHelper::GetCurrentPlatform() \
      ->UpdateTraceEventDuration

// Defines atomic operations used internally by the tracing system.
#define TRACE_EVENT_API_ATOMIC_WORD v8::base::AtomicWord
#define TRACE_EVENT_API_ATOMIC_LOAD(var) v8::base::NoBarrier_Load(&(var))
#define TRACE_EVENT_API_ATOMIC_STORE(var, value) \
  v8::base::NoBarrier_Store(&(var), (value))

// The thread buckets for the sampling profiler.
extern TRACE_EVENT_API_ATOMIC_WORD g_trace_state[3];

#define TRACE_EVENT_API_THREAD_BUCKET(thread_bucket) \
  g_trace_state[thread_bucket]

////////////////////////////////////////////////////////////////////////////////

// Implementation detail: trace event macros create temporary variables
// to keep instrumentation overhead low. These macros give each temporary
// variable a unique name based on the line number to prevent name collisions.
#define INTERNAL_TRACE_EVENT_UID3(a, b) trace_event_unique_##a##b
#define INTERNAL_TRACE_EVENT_UID2(a, b) INTERNAL_TRACE_EVENT_UID3(a, b)
#define INTERNAL_TRACE_EVENT_UID(name_prefix) \
  INTERNAL_TRACE_EVENT_UID2(name_prefix, __LINE__)

// Implementation detail: internal macro to create static category.
// No barriers are needed, because this code is designed to operate safely
// even when the unsigned char* points to garbage data (which may be the case
// on processors without cache coherency).
// TODO(fmeawad): This implementation contradicts that we can have a different
// configuration for each isolate,
// https://code.google.com/p/v8/issues/detail?id=4563
#define INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO_CUSTOM_VARIABLES(             \
    category_group, atomic, category_group_enabled)                          \
  category_group_enabled =                                                   \
      reinterpret_cast<const uint8_t*>(TRACE_EVENT_API_ATOMIC_LOAD(atomic)); \
  if (!category_group_enabled) {                                             \
    category_group_enabled =                                                 \
        TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(category_group);          \
    TRACE_EVENT_API_ATOMIC_STORE(                                            \
        atomic, reinterpret_cast<TRACE_EVENT_API_ATOMIC_WORD>(               \
                    category_group_enabled));                                \
  }

#define INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category_group)             \
  static TRACE_EVENT_API_ATOMIC_WORD INTERNAL_TRACE_EVENT_UID(atomic) = 0; \
  const uint8_t* INTERNAL_TRACE_EVENT_UID(category_group_enabled);         \
  INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO_CUSTOM_VARIABLES(                 \
      category_group, INTERNAL_TRACE_EVENT_UID(atomic),                    \
      INTERNAL_TRACE_EVENT_UID(category_group_enabled));

// Implementation detail: internal macro to create static category and add
// event if the category is enabled.
#define INTERNAL_TRACE_EVENT_ADD(phase, category_group, name, flags, ...)    \
  do {                                                                       \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category_group);                  \
    if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED_FOR_RECORDING_MODE()) {  \
      v8::internal::tracing::AddTraceEvent(                                  \
          phase, INTERNAL_TRACE_EVENT_UID(category_group_enabled), name,     \
          v8::internal::tracing::kNoId, v8::internal::tracing::kNoId, flags, \
          ##__VA_ARGS__);                                                    \
    }                                                                        \
  } while (0)

// Implementation detail: internal macro to create static category and add begin
// event if the category is enabled. Also adds the end event when the scope
// ends.
#define INTERNAL_TRACE_EVENT_ADD_SCOPED(category_group, name, ...)          \
  INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category_group);                   \
  v8::internal::tracing::ScopedTracer INTERNAL_TRACE_EVENT_UID(tracer);     \
  if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED_FOR_RECORDING_MODE()) {   \
    uint64_t h = v8::internal::tracing::AddTraceEvent(                      \
        TRACE_EVENT_PHASE_COMPLETE,                                         \
        INTERNAL_TRACE_EVENT_UID(category_group_enabled), name,             \
        v8::internal::tracing::kNoId, v8::internal::tracing::kNoId,         \
        TRACE_EVENT_FLAG_NONE, ##__VA_ARGS__);                              \
    INTERNAL_TRACE_EVENT_UID(tracer)                                        \
        .Initialize(INTERNAL_TRACE_EVENT_UID(category_group_enabled), name, \
                    h);                                                     \
  }

#define INTERNAL_TRACE_EVENT_ADD_SCOPED_WITH_FLOW(category_group, name,     \
                                                  bind_id, flow_flags, ...) \
  INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category_group);                   \
  v8::internal::tracing::ScopedTracer INTERNAL_TRACE_EVENT_UID(tracer);     \
  if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED_FOR_RECORDING_MODE()) {   \
    unsigned int trace_event_flags = flow_flags;                            \
    v8::internal::tracing::TraceID trace_event_bind_id(bind_id,             \
                                                       &trace_event_flags); \
    uint64_t h = v8::internal::tracing::AddTraceEvent(                      \
        TRACE_EVENT_PHASE_COMPLETE,                                         \
        INTERNAL_TRACE_EVENT_UID(category_group_enabled), name,             \
        v8::internal::tracing::kNoId, trace_event_bind_id.data(),           \
        trace_event_flags, ##__VA_ARGS__);                                  \
    INTERNAL_TRACE_EVENT_UID(tracer)                                        \
        .Initialize(INTERNAL_TRACE_EVENT_UID(category_group_enabled), name, \
                    h);                                                     \
  }

// Implementation detail: internal macro to create static category and add
// event if the category is enabled.
#define INTERNAL_TRACE_EVENT_ADD_WITH_ID(phase, category_group, name, id,      \
                                         flags, ...)                           \
  do {                                                                         \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category_group);                    \
    if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED_FOR_RECORDING_MODE()) {    \
      unsigned int trace_event_flags = flags | TRACE_EVENT_FLAG_HAS_ID;        \
      v8::internal::tracing::TraceID trace_event_trace_id(id,                  \
                                                          &trace_event_flags); \
      v8::internal::tracing::AddTraceEvent(                                    \
          phase, INTERNAL_TRACE_EVENT_UID(category_group_enabled), name,       \
          trace_event_trace_id.data(), v8::internal::tracing::kNoId,           \
          trace_event_flags, ##__VA_ARGS__);                                   \
    }                                                                          \
  } while (0)

// Adds a trace event with a given timestamp. Not Implemented.
#define INTERNAL_TRACE_EVENT_ADD_WITH_TIMESTAMP(phase, category_group, name, \
                                                timestamp, flags, ...)       \
  UNIMPLEMENTED()

// Adds a trace event with a given id and timestamp. Not Implemented.
#define INTERNAL_TRACE_EVENT_ADD_WITH_ID_AND_TIMESTAMP(     \
    phase, category_group, name, id, timestamp, flags, ...) \
  UNIMPLEMENTED()

// Adds a trace event with a given id, thread_id, and timestamp. Not
// Implemented.
#define INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMP(            \
    phase, category_group, name, id, thread_id, timestamp, flags, ...) \
  UNIMPLEMENTED()

namespace v8 {
namespace internal {
namespace tracing {

// Specify these values when the corresponding argument of AddTraceEvent is not
// used.
const int kZeroNumArgs = 0;
const uint64_t kNoId = 0;

class TraceEventHelper {
 public:
  static v8::Platform* GetCurrentPlatform();
};

// TraceID encapsulates an ID that can either be an integer or pointer. Pointers
// are by default mangled with the Process ID so that they are unlikely to
// collide when the same pointer is used on different processes.
class TraceID {
 public:
  class DontMangle {
   public:
    explicit DontMangle(const void* id)
        : data_(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(id))) {}
    explicit DontMangle(uint64_t id) : data_(id) {}
    explicit DontMangle(unsigned int id) : data_(id) {}
    explicit DontMangle(uint16_t id) : data_(id) {}
    explicit DontMangle(unsigned char id) : data_(id) {}
    explicit DontMangle(int64_t id) : data_(static_cast<uint64_t>(id)) {}
    explicit DontMangle(int id) : data_(static_cast<uint64_t>(id)) {}
    explicit DontMangle(int16_t id) : data_(static_cast<uint64_t>(id)) {}
    explicit DontMangle(signed char id) : data_(static_cast<uint64_t>(id)) {}
    uint64_t data() const { return data_; }

   private:
    uint64_t data_;
  };

  class ForceMangle {
   public:
    explicit ForceMangle(uint64_t id) : data_(id) {}
    explicit ForceMangle(unsigned int id) : data_(id) {}
    explicit ForceMangle(uint16_t id) : data_(id) {}
    explicit ForceMangle(unsigned char id) : data_(id) {}
    explicit ForceMangle(int64_t id) : data_(static_cast<uint64_t>(id)) {}
    explicit ForceMangle(int id) : data_(static_cast<uint64_t>(id)) {}
    explicit ForceMangle(int16_t id) : data_(static_cast<uint64_t>(id)) {}
    explicit ForceMangle(signed char id) : data_(static_cast<uint64_t>(id)) {}
    uint64_t data() const { return data_; }

   private:
    uint64_t data_;
  };

  TraceID(const void* id, unsigned int* flags)
      : data_(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(id))) {
    *flags |= TRACE_EVENT_FLAG_MANGLE_ID;
  }
  TraceID(ForceMangle id, unsigned int* flags) : data_(id.data()) {
    *flags |= TRACE_EVENT_FLAG_MANGLE_ID;
  }
  TraceID(DontMangle id, unsigned int* flags) : data_(id.data()) {}
  TraceID(uint64_t id, unsigned int* flags) : data_(id) { (void)flags; }
  TraceID(unsigned int id, unsigned int* flags) : data_(id) { (void)flags; }
  TraceID(uint16_t id, unsigned int* flags) : data_(id) { (void)flags; }
  TraceID(unsigned char id, unsigned int* flags) : data_(id) { (void)flags; }
  TraceID(int64_t id, unsigned int* flags) : data_(static_cast<uint64_t>(id)) {
    (void)flags;
  }
  TraceID(int id, unsigned int* flags) : data_(static_cast<uint64_t>(id)) {
    (void)flags;
  }
  TraceID(int16_t id, unsigned int* flags) : data_(static_cast<uint64_t>(id)) {
    (void)flags;
  }
  TraceID(signed char id, unsigned int* flags)
      : data_(static_cast<uint64_t>(id)) {
    (void)flags;
  }

  uint64_t data() const { return data_; }

 private:
  uint64_t data_;
};

// Simple union to store various types as uint64_t.
union TraceValueUnion {
  bool as_bool;
  uint64_t as_uint;
  int64_t as_int;
  double as_double;
  const void* as_pointer;
  const char* as_string;
};

// Simple container for const char* that should be copied instead of retained.
class TraceStringWithCopy {
 public:
  explicit TraceStringWithCopy(const char* str) : str_(str) {}
  operator const char*() const { return str_; }

 private:
  const char* str_;
};

// Define SetTraceValue for each allowed type. It stores the type and
// value in the return arguments. This allows this API to avoid declaring any
// structures so that it is portable to third_party libraries.
#define INTERNAL_DECLARE_SET_TRACE_VALUE(actual_type, union_member,         \
                                         value_type_id)                     \
  static V8_INLINE void SetTraceValue(actual_type arg, unsigned char* type, \
                                      uint64_t* value) {                    \
    TraceValueUnion type_value;                                             \
    type_value.union_member = arg;                                          \
    *type = value_type_id;                                                  \
    *value = type_value.as_uint;                                            \
  }
// Simpler form for int types that can be safely casted.
#define INTERNAL_DECLARE_SET_TRACE_VALUE_INT(actual_type, value_type_id)    \
  static V8_INLINE void SetTraceValue(actual_type arg, unsigned char* type, \
                                      uint64_t* value) {                    \
    *type = value_type_id;                                                  \
    *value = static_cast<uint64_t>(arg);                                    \
  }

INTERNAL_DECLARE_SET_TRACE_VALUE_INT(uint64_t, TRACE_VALUE_TYPE_UINT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(unsigned int, TRACE_VALUE_TYPE_UINT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(uint16_t, TRACE_VALUE_TYPE_UINT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(unsigned char, TRACE_VALUE_TYPE_UINT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(int64_t, TRACE_VALUE_TYPE_INT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(int, TRACE_VALUE_TYPE_INT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(int16_t, TRACE_VALUE_TYPE_INT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(signed char, TRACE_VALUE_TYPE_INT)
INTERNAL_DECLARE_SET_TRACE_VALUE(bool, as_bool, TRACE_VALUE_TYPE_BOOL)
INTERNAL_DECLARE_SET_TRACE_VALUE(double, as_double, TRACE_VALUE_TYPE_DOUBLE)
INTERNAL_DECLARE_SET_TRACE_VALUE(const void*, as_pointer,
                                 TRACE_VALUE_TYPE_POINTER)
INTERNAL_DECLARE_SET_TRACE_VALUE(const char*, as_string,
                                 TRACE_VALUE_TYPE_STRING)
INTERNAL_DECLARE_SET_TRACE_VALUE(const TraceStringWithCopy&, as_string,
                                 TRACE_VALUE_TYPE_COPY_STRING)

#undef INTERNAL_DECLARE_SET_TRACE_VALUE
#undef INTERNAL_DECLARE_SET_TRACE_VALUE_INT

// These AddTraceEvent template
// function is defined here instead of in the macro, because the arg_values
// could be temporary objects, such as std::string. In order to store
// pointers to the internal c_str and pass through to the tracing API,
// the arg_values must live throughout these procedures.

static V8_INLINE uint64_t AddTraceEvent(char phase,
                                        const uint8_t* category_group_enabled,
                                        const char* name, uint64_t id,
                                        uint64_t bind_id, unsigned int flags) {
  return TRACE_EVENT_API_ADD_TRACE_EVENT(phase, category_group_enabled, name,
                                         id, bind_id, kZeroNumArgs, NULL, NULL,
                                         NULL, flags);
}

template <class ARG1_TYPE>
static V8_INLINE uint64_t AddTraceEvent(char phase,
                                        const uint8_t* category_group_enabled,
                                        const char* name, uint64_t id,
                                        uint64_t bind_id, unsigned int flags,
                                        const char* arg1_name,
                                        const ARG1_TYPE& arg1_val) {
  const int num_args = 1;
  uint8_t arg_types[1];
  uint64_t arg_values[1];
  SetTraceValue(arg1_val, &arg_types[0], &arg_values[0]);
  return TRACE_EVENT_API_ADD_TRACE_EVENT(phase, category_group_enabled, name,
                                         id, bind_id, num_args, &arg1_name,
                                         arg_types, arg_values, flags);
}

template <class ARG1_TYPE, class ARG2_TYPE>
static V8_INLINE uint64_t AddTraceEvent(
    char phase, const uint8_t* category_group_enabled, const char* name,
    uint64_t id, uint64_t bind_id, unsigned int flags, const char* arg1_name,
    const ARG1_TYPE& arg1_val, const char* arg2_name,
    const ARG2_TYPE& arg2_val) {
  const int num_args = 2;
  const char* arg_names[2] = {arg1_name, arg2_name};
  unsigned char arg_types[2];
  uint64_t arg_values[2];
  SetTraceValue(arg1_val, &arg_types[0], &arg_values[0]);
  SetTraceValue(arg2_val, &arg_types[1], &arg_values[1]);
  return TRACE_EVENT_API_ADD_TRACE_EVENT(phase, category_group_enabled, name,
                                         id, bind_id, num_args, arg_names,
                                         arg_types, arg_values, flags);
}

// Used by TRACE_EVENTx macros. Do not use directly.
class ScopedTracer {
 public:
  // Note: members of data_ intentionally left uninitialized. See Initialize.
  ScopedTracer() : p_data_(NULL) {}

  ~ScopedTracer() {
    if (p_data_ && *data_.category_group_enabled)
      TRACE_EVENT_API_UPDATE_TRACE_EVENT_DURATION(
          data_.category_group_enabled, data_.name, data_.event_handle);
  }

  void Initialize(const uint8_t* category_group_enabled, const char* name,
                  uint64_t event_handle) {
    data_.category_group_enabled = category_group_enabled;
    data_.name = name;
    data_.event_handle = event_handle;
    p_data_ = &data_;
  }

 private:
  // This Data struct workaround is to avoid initializing all the members
  // in Data during construction of this object, since this object is always
  // constructed, even when tracing is disabled. If the members of Data were
  // members of this class instead, compiler warnings occur about potential
  // uninitialized accesses.
  struct Data {
    const uint8_t* category_group_enabled;
    const char* name;
    uint64_t event_handle;
  };
  Data* p_data_;
  Data data_;
};

// Used by TRACE_EVENT_BINARY_EFFICIENTx macro. Do not use directly.
class ScopedTraceBinaryEfficient {
 public:
  ScopedTraceBinaryEfficient(const char* category_group, const char* name);
  ~ScopedTraceBinaryEfficient();

 private:
  const uint8_t* category_group_enabled_;
  const char* name_;
  uint64_t event_handle_;
};

// TraceEventSamplingStateScope records the current sampling state
// and sets a new sampling state. When the scope exists, it restores
// the sampling state having recorded.
template <size_t BucketNumber>
class TraceEventSamplingStateScope {
 public:
  explicit TraceEventSamplingStateScope(const char* category_and_name) {
    previous_state_ = TraceEventSamplingStateScope<BucketNumber>::Current();
    TraceEventSamplingStateScope<BucketNumber>::Set(category_and_name);
  }

  ~TraceEventSamplingStateScope() {
    TraceEventSamplingStateScope<BucketNumber>::Set(previous_state_);
  }

  static V8_INLINE const char* Current() {
    return reinterpret_cast<const char*>(
        TRACE_EVENT_API_ATOMIC_LOAD(g_trace_state[BucketNumber]));
  }

  static V8_INLINE void Set(const char* category_and_name) {
    TRACE_EVENT_API_ATOMIC_STORE(g_trace_state[BucketNumber],
                                 reinterpret_cast<TRACE_EVENT_API_ATOMIC_WORD>(
                                     const_cast<char*>(category_and_name)));
  }

 private:
  const char* previous_state_;
};

}  // namespace tracing
}  // namespace internal
}  // namespace v8

#endif  // SRC_TRACING_TRACE_EVENT_H_
