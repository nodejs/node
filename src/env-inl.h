// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef SRC_ENV_INL_H_
#define SRC_ENV_INL_H_

#include "env.h"
#include "node.h"
#include "util.h"
#include "util-inl.h"
#include "uv.h"
#include "v8.h"

#include <stddef.h>
#include <stdint.h>

namespace node {

inline Environment::GCInfo::GCInfo()
    : type_(static_cast<v8::GCType>(0)),
      flags_(static_cast<v8::GCCallbackFlags>(0)),
      timestamp_(0) {
}

inline Environment::GCInfo::GCInfo(v8::Isolate* isolate,
                                   v8::GCType type,
                                   v8::GCCallbackFlags flags,
                                   uint64_t timestamp)
    : type_(type),
      flags_(flags),
      timestamp_(timestamp) {
  isolate->GetHeapStatistics(&stats_);
}

inline v8::GCType Environment::GCInfo::type() const {
  return type_;
}

inline v8::GCCallbackFlags Environment::GCInfo::flags() const {
  return flags_;
}

inline v8::HeapStatistics* Environment::GCInfo::stats() const {
  // TODO(bnoordhuis) Const-ify once https://codereview.chromium.org/63693005
  // lands and makes it way into a stable release.
  return const_cast<v8::HeapStatistics*>(&stats_);
}

inline uint64_t Environment::GCInfo::timestamp() const {
  return timestamp_;
}

inline Environment::IsolateData* Environment::IsolateData::Get(
    v8::Isolate* isolate) {
  return static_cast<IsolateData*>(isolate->GetData(kIsolateSlot));
}

inline Environment::IsolateData* Environment::IsolateData::GetOrCreate(
    v8::Isolate* isolate, uv_loop_t* loop) {
  IsolateData* isolate_data = Get(isolate);
  if (isolate_data == NULL) {
    isolate_data = new IsolateData(isolate, loop);
    isolate->SetData(kIsolateSlot, isolate_data);
  }
  isolate_data->ref_count_ += 1;
  return isolate_data;
}

inline void Environment::IsolateData::Put() {
  if (--ref_count_ == 0) {
    isolate()->SetData(kIsolateSlot, NULL);
    delete this;
  }
}

inline Environment::IsolateData::IsolateData(v8::Isolate* isolate,
                                             uv_loop_t* loop)
    : event_loop_(loop),
      isolate_(isolate),
#define V(PropertyName, StringValue)                                          \
    PropertyName ## _(isolate, FIXED_ONE_BYTE_STRING(isolate, StringValue)),
    PER_ISOLATE_STRING_PROPERTIES(V)
#undef V
    ref_count_(0) {
  QUEUE_INIT(&gc_tracker_queue_);
}

inline uv_loop_t* Environment::IsolateData::event_loop() const {
  return event_loop_;
}

inline v8::Isolate* Environment::IsolateData::isolate() const {
  return isolate_;
}

inline Environment::AsyncHooks::AsyncHooks() {
  for (int i = 0; i < kFieldsCount; i++) fields_[i] = 0;
}

inline uint32_t* Environment::AsyncHooks::fields() {
  return fields_;
}

inline int Environment::AsyncHooks::fields_count() const {
  return kFieldsCount;
}

inline bool Environment::AsyncHooks::call_init_hook() {
  return fields_[kCallInitHook] != 0;
}

inline Environment::DomainFlag::DomainFlag() {
  for (int i = 0; i < kFieldsCount; ++i) fields_[i] = 0;
}

inline uint32_t* Environment::DomainFlag::fields() {
  return fields_;
}

inline int Environment::DomainFlag::fields_count() const {
  return kFieldsCount;
}

inline uint32_t Environment::DomainFlag::count() const {
  return fields_[kCount];
}

inline Environment::TickInfo::TickInfo() : in_tick_(false), last_threw_(false) {
  for (int i = 0; i < kFieldsCount; ++i)
    fields_[i] = 0;
}

inline uint32_t* Environment::TickInfo::fields() {
  return fields_;
}

inline int Environment::TickInfo::fields_count() const {
  return kFieldsCount;
}

inline bool Environment::TickInfo::in_tick() const {
  return in_tick_;
}

inline uint32_t Environment::TickInfo::index() const {
  return fields_[kIndex];
}

inline bool Environment::TickInfo::last_threw() const {
  return last_threw_;
}

inline uint32_t Environment::TickInfo::length() const {
  return fields_[kLength];
}

inline void Environment::TickInfo::set_in_tick(bool value) {
  in_tick_ = value;
}

inline void Environment::TickInfo::set_index(uint32_t value) {
  fields_[kIndex] = value;
}

inline void Environment::TickInfo::set_last_threw(bool value) {
  last_threw_ = value;
}

inline Environment* Environment::New(v8::Local<v8::Context> context,
                                     uv_loop_t* loop) {
  Environment* env = new Environment(context, loop);
  env->AssignToContext(context);
  return env;
}

inline void Environment::AssignToContext(v8::Local<v8::Context> context) {
  context->SetAlignedPointerInEmbedderData(kContextEmbedderDataIndex, this);
}

inline Environment* Environment::GetCurrent(v8::Isolate* isolate) {
  return GetCurrent(isolate->GetCurrentContext());
}

inline Environment* Environment::GetCurrent(v8::Local<v8::Context> context) {
  return static_cast<Environment*>(
      context->GetAlignedPointerFromEmbedderData(kContextEmbedderDataIndex));
}

inline Environment::Environment(v8::Local<v8::Context> context,
                                uv_loop_t* loop)
    : isolate_(context->GetIsolate()),
      isolate_data_(IsolateData::GetOrCreate(context->GetIsolate(), loop)),
      using_smalloc_alloc_cb_(false),
      using_domains_(false),
      printed_error_(false),
      debugger_agent_(this),
      context_(context->GetIsolate(), context) {
  // We'll be creating new objects so make sure we've entered the context.
  v8::HandleScope handle_scope(isolate());
  v8::Context::Scope context_scope(context);
  set_binding_cache_object(v8::Object::New(isolate()));
  set_module_load_list_array(v8::Array::New(isolate()));
  RB_INIT(&cares_task_list_);
  QUEUE_INIT(&gc_tracker_queue_);
  QUEUE_INIT(&req_wrap_queue_);
  QUEUE_INIT(&handle_wrap_queue_);
  QUEUE_INIT(&handle_cleanup_queue_);
  handle_cleanup_waiting_ = 0;
}

inline Environment::~Environment() {
  v8::HandleScope handle_scope(isolate());

  context()->SetAlignedPointerInEmbedderData(kContextEmbedderDataIndex, NULL);
#define V(PropertyName, TypeName) PropertyName ## _.Reset();
  ENVIRONMENT_STRONG_PERSISTENT_PROPERTIES(V)
#undef V
  isolate_data()->Put();
}

inline void Environment::CleanupHandles() {
  while (!QUEUE_EMPTY(&handle_cleanup_queue_)) {
    QUEUE* q = QUEUE_HEAD(&handle_cleanup_queue_);
    QUEUE_REMOVE(q);

    HandleCleanup* hc = ContainerOf(&HandleCleanup::handle_cleanup_queue_, q);
    handle_cleanup_waiting_++;
    hc->cb_(this, hc->handle_, hc->arg_);
    delete hc;
  }

  while (handle_cleanup_waiting_ != 0)
    uv_run(event_loop(), UV_RUN_ONCE);
}

inline void Environment::Dispose() {
  delete this;
}

inline v8::Isolate* Environment::isolate() const {
  return isolate_;
}

inline bool Environment::call_async_init_hook() const {
  // The const_cast is okay, it doesn't violate conceptual const-ness.
  return const_cast<Environment*>(this)->async_hooks()->call_init_hook();
}

inline bool Environment::in_domain() const {
  // The const_cast is okay, it doesn't violate conceptual const-ness.
  return using_domains() &&
         const_cast<Environment*>(this)->domain_flag()->count() > 0;
}

inline Environment* Environment::from_immediate_check_handle(
    uv_check_t* handle) {
  return ContainerOf(&Environment::immediate_check_handle_, handle);
}

inline uv_check_t* Environment::immediate_check_handle() {
  return &immediate_check_handle_;
}

inline uv_idle_t* Environment::immediate_idle_handle() {
  return &immediate_idle_handle_;
}

inline Environment* Environment::from_idle_prepare_handle(
    uv_prepare_t* handle) {
  return ContainerOf(&Environment::idle_prepare_handle_, handle);
}

inline uv_prepare_t* Environment::idle_prepare_handle() {
  return &idle_prepare_handle_;
}

inline Environment* Environment::from_idle_check_handle(uv_check_t* handle) {
  return ContainerOf(&Environment::idle_check_handle_, handle);
}

inline uv_check_t* Environment::idle_check_handle() {
  return &idle_check_handle_;
}

inline void Environment::RegisterHandleCleanup(uv_handle_t* handle,
                                               HandleCleanupCb cb,
                                               void *arg) {
  HandleCleanup* hc = new HandleCleanup(handle, cb, arg);
  QUEUE_INSERT_TAIL(&handle_cleanup_queue_, &hc->handle_cleanup_queue_);
}

inline void Environment::FinishHandleCleanup(uv_handle_t* handle) {
  handle_cleanup_waiting_--;
}

inline uv_loop_t* Environment::event_loop() const {
  return isolate_data()->event_loop();
}

inline Environment::AsyncHooks* Environment::async_hooks() {
  return &async_hooks_;
}

inline Environment::DomainFlag* Environment::domain_flag() {
  return &domain_flag_;
}

inline Environment::TickInfo* Environment::tick_info() {
  return &tick_info_;
}

inline bool Environment::using_smalloc_alloc_cb() const {
  return using_smalloc_alloc_cb_;
}

inline void Environment::set_using_smalloc_alloc_cb(bool value) {
  using_smalloc_alloc_cb_ = value;
}

inline bool Environment::using_domains() const {
  return using_domains_;
}

inline void Environment::set_using_domains(bool value) {
  using_domains_ = value;
}

inline bool Environment::printed_error() const {
  return printed_error_;
}

inline void Environment::set_printed_error(bool value) {
  printed_error_ = value;
}

inline Environment* Environment::from_cares_timer_handle(uv_timer_t* handle) {
  return ContainerOf(&Environment::cares_timer_handle_, handle);
}

inline uv_timer_t* Environment::cares_timer_handle() {
  return &cares_timer_handle_;
}

inline ares_channel Environment::cares_channel() {
  return cares_channel_;
}

// Only used in the call to ares_init_options().
inline ares_channel* Environment::cares_channel_ptr() {
  return &cares_channel_;
}

inline ares_task_list* Environment::cares_task_list() {
  return &cares_task_list_;
}

inline Environment::IsolateData* Environment::isolate_data() const {
  return isolate_data_;
}

// this would have been a template function were it not for the fact that g++
// sometimes fails to resolve it...
#define THROW_ERROR(fun)                                                      \
  do {                                                                        \
    v8::HandleScope scope(isolate);                                           \
    isolate->ThrowException(fun(OneByteString(isolate, errmsg)));             \
  }                                                                           \
  while (0)

inline void Environment::ThrowError(v8::Isolate* isolate, const char* errmsg) {
  THROW_ERROR(v8::Exception::Error);
}

inline void Environment::ThrowTypeError(v8::Isolate* isolate,
                                        const char* errmsg) {
  THROW_ERROR(v8::Exception::TypeError);
}

inline void Environment::ThrowRangeError(v8::Isolate* isolate,
                                         const char* errmsg) {
  THROW_ERROR(v8::Exception::RangeError);
}

inline void Environment::ThrowError(const char* errmsg) {
  ThrowError(isolate(), errmsg);
}

inline void Environment::ThrowTypeError(const char* errmsg) {
  ThrowTypeError(isolate(), errmsg);
}

inline void Environment::ThrowRangeError(const char* errmsg) {
  ThrowRangeError(isolate(), errmsg);
}

inline void Environment::ThrowErrnoException(int errorno,
                                             const char* syscall,
                                             const char* message,
                                             const char* path) {
  isolate()->ThrowException(
      ErrnoException(isolate(), errorno, syscall, message, path));
}

inline void Environment::ThrowUVException(int errorno,
                                          const char* syscall,
                                          const char* message,
                                          const char* path) {
  isolate()->ThrowException(
      UVException(isolate(), errorno, syscall, message, path));
}

#define V(PropertyName, StringValue)                                          \
  inline                                                                      \
  v8::Local<v8::String> Environment::IsolateData::PropertyName() const {      \
    /* Strings are immutable so casting away const-ness here is okay. */      \
    return const_cast<IsolateData*>(this)->PropertyName ## _.Get(isolate());  \
  }
  PER_ISOLATE_STRING_PROPERTIES(V)
#undef V

#define V(PropertyName, StringValue)                                          \
  inline v8::Local<v8::String> Environment::PropertyName() const {            \
    return isolate_data()->PropertyName();                                    \
  }
  PER_ISOLATE_STRING_PROPERTIES(V)
#undef V

#define V(PropertyName, TypeName)                                             \
  inline v8::Local<TypeName> Environment::PropertyName() const {              \
    return StrongPersistentToLocal(PropertyName ## _);                        \
  }                                                                           \
  inline void Environment::set_ ## PropertyName(v8::Local<TypeName> value) {  \
    PropertyName ## _.Reset(isolate(), value);                                \
  }
  ENVIRONMENT_STRONG_PERSISTENT_PROPERTIES(V)
#undef V

#undef ENVIRONMENT_STRONG_PERSISTENT_PROPERTIES
#undef PER_ISOLATE_STRING_PROPERTIES

}  // namespace node

#endif  // SRC_ENV_INL_H_
