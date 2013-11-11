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
#include "util.h"
#include "util-inl.h"
#include "uv.h"
#include "v8.h"

#include <stddef.h>
#include <stdint.h>

namespace node {

inline Environment::IsolateData* Environment::IsolateData::GetOrCreate(
    v8::Isolate* isolate) {
  IsolateData* isolate_data = static_cast<IsolateData*>(isolate->GetData());
  if (isolate_data == NULL) {
    isolate_data = new IsolateData(isolate);
    isolate->SetData(isolate_data);
  }
  isolate_data->ref_count_ += 1;
  return isolate_data;
}

inline void Environment::IsolateData::Put() {
  if (--ref_count_ == 0) {
    isolate()->SetData(NULL);
    delete this;
  }
}

inline Environment::IsolateData::IsolateData(v8::Isolate* isolate)
    : event_loop_(uv_default_loop()),
      isolate_(isolate),
#define V(PropertyName, StringValue)                                          \
    PropertyName ## _(isolate, FIXED_ONE_BYTE_STRING(isolate, StringValue)),
    PER_ISOLATE_STRING_PROPERTIES(V)
#undef V
    ref_count_(0) {
}

inline uv_loop_t* Environment::IsolateData::event_loop() const {
  return event_loop_;
}

inline v8::Isolate* Environment::IsolateData::isolate() const {
  return isolate_;
}

inline Environment::AsyncListener::AsyncListener() {
  for (int i = 0; i < kFieldsCount; ++i)
    fields_[i] = 0;
}

inline uint32_t* Environment::AsyncListener::fields() {
  return fields_;
}

inline int Environment::AsyncListener::fields_count() const {
  return kFieldsCount;
}

inline uint32_t Environment::AsyncListener::count() const {
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

inline Environment* Environment::New(v8::Local<v8::Context> context) {
  Environment* env = new Environment(context);
  context->SetAlignedPointerInEmbedderData(kContextEmbedderDataIndex, env);
  return env;
}

inline Environment* Environment::GetCurrent(v8::Isolate* isolate) {
  return GetCurrent(isolate->GetCurrentContext());
}

inline Environment* Environment::GetCurrent(v8::Local<v8::Context> context) {
  return static_cast<Environment*>(
      context->GetAlignedPointerFromEmbedderData(kContextEmbedderDataIndex));
}

inline Environment* Environment::GetCurrentChecked(v8::Isolate* isolate) {
  if (isolate == NULL) {
    return NULL;
  } else {
    return GetCurrentChecked(isolate->GetCurrentContext());
  }
}

inline Environment* Environment::GetCurrentChecked(
    v8::Local<v8::Context> context) {
  if (context.IsEmpty()) {
    return NULL;
  } else {
    return GetCurrent(context);
  }
}

inline Environment::Environment(v8::Local<v8::Context> context)
    : isolate_(context->GetIsolate()),
      isolate_data_(IsolateData::GetOrCreate(context->GetIsolate())),
      using_smalloc_alloc_cb_(false),
      context_(context->GetIsolate(), context) {
  // We'll be creating new objects so make sure we've entered the context.
  v8::HandleScope handle_scope(isolate());
  v8::Context::Scope context_scope(context);
  set_binding_cache_object(v8::Object::New());
  set_module_load_list_array(v8::Array::New());
  RB_INIT(&cares_task_list_);
}

inline Environment::~Environment() {
  context()->SetAlignedPointerInEmbedderData(kContextEmbedderDataIndex, NULL);
#define V(PropertyName, TypeName) PropertyName ## _.Dispose();
  ENVIRONMENT_STRONG_PERSISTENT_PROPERTIES(V)
#undef V
  isolate_data()->Put();
}

inline void Environment::Dispose() {
  delete this;
}

inline v8::Isolate* Environment::isolate() const {
  return isolate_;
}

inline bool Environment::has_async_listeners() const {
  // The const_cast is okay, it doesn't violate conceptual const-ness.
  return const_cast<Environment*>(this)->async_listener()->count() > 0;
}

inline Environment* Environment::from_immediate_check_handle(
    uv_check_t* handle) {
  return CONTAINER_OF(handle, Environment, immediate_check_handle_);
}

inline uv_check_t* Environment::immediate_check_handle() {
  return &immediate_check_handle_;
}

inline uv_idle_t* Environment::immediate_idle_handle() {
  return &immediate_idle_handle_;
}

inline Environment* Environment::from_idle_prepare_handle(
    uv_prepare_t* handle) {
  return CONTAINER_OF(handle, Environment, idle_prepare_handle_);
}

inline uv_prepare_t* Environment::idle_prepare_handle() {
  return &idle_prepare_handle_;
}

inline Environment* Environment::from_idle_check_handle(uv_check_t* handle) {
  return CONTAINER_OF(handle, Environment, idle_check_handle_);
}

inline uv_check_t* Environment::idle_check_handle() {
  return &idle_check_handle_;
}

inline uv_loop_t* Environment::event_loop() const {
  return isolate_data()->event_loop();
}

inline Environment::AsyncListener* Environment::async_listener() {
  return &async_listener_count_;
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

inline Environment* Environment::from_cares_timer_handle(uv_timer_t* handle) {
  return CONTAINER_OF(handle, Environment, cares_timer_handle_);
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
