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

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "env.h"
#include "node.h"
#include "util.h"
#include "util-inl.h"
#include "uv.h"
#include "v8.h"

#include <stddef.h>
#include <stdint.h>

namespace node {

inline IsolateData::IsolateData(v8::Isolate* isolate, uv_loop_t* event_loop,
                                uint32_t* zero_fill_field) :

// Create string and private symbol properties as internalized one byte strings.
//
// Internalized because it makes property lookups a little faster and because
// the string is created in the old space straight away.  It's going to end up
// in the old space sooner or later anyway but now it doesn't go through
// v8::Eternal's new space handling first.
//
// One byte because our strings are ASCII and we can safely skip V8's UTF-8
// decoding step.  It's a one-time cost, but why pay it when you don't have to?
#define V(PropertyName, StringValue)                                          \
    PropertyName ## _(                                                        \
        isolate,                                                              \
        v8::Private::New(                                                     \
            isolate,                                                          \
            v8::String::NewFromOneByte(                                       \
                isolate,                                                      \
                reinterpret_cast<const uint8_t*>(StringValue),                \
                v8::NewStringType::kInternalized,                             \
                sizeof(StringValue) - 1).ToLocalChecked())),
  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(V)
#undef V
#define V(PropertyName, StringValue)                                          \
    PropertyName ## _(                                                        \
        isolate,                                                              \
        v8::String::NewFromOneByte(                                           \
            isolate,                                                          \
            reinterpret_cast<const uint8_t*>(StringValue),                    \
            v8::NewStringType::kInternalized,                                 \
            sizeof(StringValue) - 1).ToLocalChecked()),
    PER_ISOLATE_STRING_PROPERTIES(V)
#undef V
    event_loop_(event_loop), zero_fill_field_(zero_fill_field) {}

inline uv_loop_t* IsolateData::event_loop() const {
  return event_loop_;
}

inline uint32_t* IsolateData::zero_fill_field() const {
  return zero_fill_field_;
}

inline Environment::AsyncHooks::AsyncHooks(v8::Isolate* isolate)
    : isolate_(isolate),
      fields_(),
      uid_fields_() {
  v8::HandleScope handle_scope(isolate_);

  // kAsyncUidCntr should start at 1 because that'll be the id the execution
  // context during bootstrap (code that runs before entering uv_run()).
  uid_fields_[AsyncHooks::kAsyncUidCntr] = 1;

  // Create all the provider strings that will be passed to JS. Place them in
  // an array so the array index matches the PROVIDER id offset. This way the
  // strings can be retrieved quickly.
#define V(Provider)                                                           \
  providers_[AsyncWrap::PROVIDER_ ## Provider].Set(                           \
      isolate_,                                                               \
      v8::String::NewFromOneByte(                                             \
        isolate_,                                                             \
        reinterpret_cast<const uint8_t*>(#Provider),                          \
        v8::NewStringType::kInternalized,                                     \
        sizeof(#Provider) - 1).ToLocalChecked());
  NODE_ASYNC_PROVIDER_TYPES(V)
#undef V
}

inline uint32_t* Environment::AsyncHooks::fields() {
  return fields_;
}

inline int Environment::AsyncHooks::fields_count() const {
  return kFieldsCount;
}

inline double* Environment::AsyncHooks::uid_fields() {
  return uid_fields_;
}

inline int Environment::AsyncHooks::uid_fields_count() const {
  return kUidFieldsCount;
}

inline v8::Local<v8::String> Environment::AsyncHooks::provider_string(int idx) {
  return providers_[idx].Get(isolate_);
}

inline void Environment::AsyncHooks::push_ids(double async_id,
                                              double trigger_id) {
  CHECK_GE(async_id, -1);
  CHECK_GE(trigger_id, -1);

  ids_stack_.push({ uid_fields_[kCurrentAsyncId],
                    uid_fields_[kCurrentTriggerId] });
  uid_fields_[kCurrentAsyncId] = async_id;
  uid_fields_[kCurrentTriggerId] = trigger_id;
}

inline bool Environment::AsyncHooks::pop_ids(double async_id) {
  // In case of an exception then this may have already been reset, if the
  // stack was multiple MakeCallback()'s deep.
  if (ids_stack_.empty()) return false;

  // Ask for the async_id to be restored as a sanity check that the stack
  // hasn't been corrupted.
  if (uid_fields_[kCurrentAsyncId] != async_id) {
    fprintf(stderr,
            "Error: async hook stack has become corrupted ("
            "actual: %.f, expected: %.f)\n",
            uid_fields_[kCurrentAsyncId],
            async_id);
    Environment* env = Environment::GetCurrent(isolate_);
    DumpBacktrace(stderr);
    fflush(stderr);
    if (!env->abort_on_uncaught_exception())
      exit(1);
    fprintf(stderr, "\n");
    fflush(stderr);
    ABORT_NO_BACKTRACE();
  }

  auto ids = ids_stack_.top();
  ids_stack_.pop();
  uid_fields_[kCurrentAsyncId] = ids.async_id;
  uid_fields_[kCurrentTriggerId] = ids.trigger_id;
  return !ids_stack_.empty();
}

inline size_t Environment::AsyncHooks::stack_size() {
  return ids_stack_.size();
}

inline void Environment::AsyncHooks::clear_id_stack() {
  while (!ids_stack_.empty())
    ids_stack_.pop();
  uid_fields_[kCurrentAsyncId] = 0;
  uid_fields_[kCurrentTriggerId] = 0;
}

inline Environment::AsyncHooks::InitScope::InitScope(
    Environment* env, double init_trigger_id)
        : env_(env),
          uid_fields_ref_(env->async_hooks()->uid_fields()) {
  CHECK_GE(init_trigger_id, -1);
  env->async_hooks()->push_ids(uid_fields_ref_[AsyncHooks::kCurrentAsyncId],
                               init_trigger_id);
}

inline Environment::AsyncHooks::InitScope::~InitScope() {
  env_->async_hooks()->pop_ids(uid_fields_ref_[AsyncHooks::kCurrentAsyncId]);
}

inline Environment::AsyncHooks::ExecScope::ExecScope(
    Environment* env, double async_id, double trigger_id)
        : env_(env),
          async_id_(async_id),
          disposed_(false) {
  CHECK_GE(async_id, -1);
  CHECK_GE(trigger_id, -1);
  env->async_hooks()->push_ids(async_id, trigger_id);
}

inline Environment::AsyncHooks::ExecScope::~ExecScope() {
  if (disposed_) return;
  Dispose();
}

inline void Environment::AsyncHooks::ExecScope::Dispose() {
  disposed_ = true;
  env_->async_hooks()->pop_ids(async_id_);
}

inline Environment::AsyncCallbackScope::AsyncCallbackScope(Environment* env)
    : env_(env) {
  env_->makecallback_cntr_++;
}

inline Environment::AsyncCallbackScope::~AsyncCallbackScope() {
  env_->makecallback_cntr_--;
}

inline bool Environment::AsyncCallbackScope::in_makecallback() {
  return env_->makecallback_cntr_ > 1;
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

inline Environment::TickInfo::TickInfo() {
  for (int i = 0; i < kFieldsCount; ++i)
    fields_[i] = 0;
}

inline uint32_t* Environment::TickInfo::fields() {
  return fields_;
}

inline int Environment::TickInfo::fields_count() const {
  return kFieldsCount;
}

inline uint32_t Environment::TickInfo::index() const {
  return fields_[kIndex];
}

inline uint32_t Environment::TickInfo::length() const {
  return fields_[kLength];
}

inline void Environment::TickInfo::set_index(uint32_t value) {
  fields_[kIndex] = value;
}

inline void Environment::AssignToContext(v8::Local<v8::Context> context) {
  context->SetAlignedPointerInEmbedderData(kContextEmbedderDataIndex, this);
#if HAVE_INSPECTOR
  inspector_agent()->ContextCreated(context);
#endif  // HAVE_INSPECTOR
}

inline Environment* Environment::GetCurrent(v8::Isolate* isolate) {
  return GetCurrent(isolate->GetCurrentContext());
}

inline Environment* Environment::GetCurrent(v8::Local<v8::Context> context) {
  return static_cast<Environment*>(
      context->GetAlignedPointerFromEmbedderData(kContextEmbedderDataIndex));
}

inline Environment* Environment::GetCurrent(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK(info.Data()->IsExternal());
  return static_cast<Environment*>(info.Data().As<v8::External>()->Value());
}

template <typename T>
inline Environment* Environment::GetCurrent(
    const v8::PropertyCallbackInfo<T>& info) {
  CHECK(info.Data()->IsExternal());
  return static_cast<Environment*>(
      info.Data().template As<v8::External>()->Value());
}

inline Environment::Environment(IsolateData* isolate_data,
                                v8::Local<v8::Context> context)
    : isolate_(context->GetIsolate()),
      isolate_data_(isolate_data),
      async_hooks_(context->GetIsolate()),
      timer_base_(uv_now(isolate_data->event_loop())),
      using_domains_(false),
      printed_error_(false),
      trace_sync_io_(false),
      abort_on_uncaught_exception_(false),
      makecallback_cntr_(0),
#if HAVE_INSPECTOR
      inspector_agent_(this),
#endif
      handle_cleanup_waiting_(0),
      http_parser_buffer_(nullptr),
      fs_stats_field_array_(nullptr),
      context_(context->GetIsolate(), context) {
  // We'll be creating new objects so make sure we've entered the context.
  v8::HandleScope handle_scope(isolate());
  v8::Context::Scope context_scope(context);
  set_as_external(v8::External::New(isolate(), this));
  set_binding_cache_object(v8::Object::New(isolate()));
  set_module_load_list_array(v8::Array::New(isolate()));

  AssignToContext(context);

  destroy_ids_list_.reserve(512);
  performance_state_ = Calloc<performance::performance_state>(1);
  performance_state_->milestones[
      performance::NODE_PERFORMANCE_MILESTONE_ENVIRONMENT] =
          PERFORMANCE_NOW();
  performance_state_->milestones[
    performance::NODE_PERFORMANCE_MILESTONE_NODE_START] =
        performance::performance_node_start;
  performance_state_->milestones[
    performance::NODE_PERFORMANCE_MILESTONE_V8_START] =
        performance::performance_v8_start;
}

inline Environment::~Environment() {
  v8::HandleScope handle_scope(isolate());

  context()->SetAlignedPointerInEmbedderData(kContextEmbedderDataIndex,
                                             nullptr);
#define V(PropertyName, TypeName) PropertyName ## _.Reset();
  ENVIRONMENT_STRONG_PERSISTENT_PROPERTIES(V)
#undef V

  delete[] heap_statistics_buffer_;
  delete[] heap_space_statistics_buffer_;
  delete[] http_parser_buffer_;
  free(http2_state_buffer_);
  free(performance_state_);
}

inline v8::Isolate* Environment::isolate() const {
  return isolate_;
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

inline Environment* Environment::from_destroy_ids_timer_handle(
    uv_timer_t* handle) {
  return ContainerOf(&Environment::destroy_ids_timer_handle_, handle);
}

inline uv_timer_t* Environment::destroy_ids_timer_handle() {
  return &destroy_ids_timer_handle_;
}

inline void Environment::RegisterHandleCleanup(uv_handle_t* handle,
                                               HandleCleanupCb cb,
                                               void *arg) {
  handle_cleanup_queue_.PushBack(new HandleCleanup(handle, cb, arg));
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

inline uint64_t Environment::timer_base() const {
  return timer_base_;
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

inline void Environment::set_trace_sync_io(bool value) {
  trace_sync_io_ = value;
}

inline bool Environment::abort_on_uncaught_exception() const {
  return abort_on_uncaught_exception_;
}

inline void Environment::set_abort_on_uncaught_exception(bool value) {
  abort_on_uncaught_exception_ = value;
}

inline std::vector<double>* Environment::destroy_ids_list() {
  return &destroy_ids_list_;
}

inline double Environment::new_async_id() {
  return ++async_hooks()->uid_fields()[AsyncHooks::kAsyncUidCntr];
}

inline double Environment::current_async_id() {
  return async_hooks()->uid_fields()[AsyncHooks::kCurrentAsyncId];
}

inline double Environment::trigger_id() {
  return async_hooks()->uid_fields()[AsyncHooks::kCurrentTriggerId];
}

inline double Environment::get_init_trigger_id() {
  double* uid_fields = async_hooks()->uid_fields();
  double tid = uid_fields[AsyncHooks::kInitTriggerId];
  uid_fields[AsyncHooks::kInitTriggerId] = 0;
  if (tid <= 0) tid = current_async_id();
  return tid;
}

inline void Environment::set_init_trigger_id(const double id) {
  async_hooks()->uid_fields()[AsyncHooks::kInitTriggerId] = id;
}

inline double* Environment::heap_statistics_buffer() const {
  CHECK_NE(heap_statistics_buffer_, nullptr);
  return heap_statistics_buffer_;
}

inline void Environment::set_heap_statistics_buffer(double* pointer) {
  CHECK_EQ(heap_statistics_buffer_, nullptr);  // Should be set only once.
  heap_statistics_buffer_ = pointer;
}

inline double* Environment::heap_space_statistics_buffer() const {
  CHECK_NE(heap_space_statistics_buffer_, nullptr);
  return heap_space_statistics_buffer_;
}

inline void Environment::set_heap_space_statistics_buffer(double* pointer) {
  CHECK_EQ(heap_space_statistics_buffer_, nullptr);  // Should be set only once.
  heap_space_statistics_buffer_ = pointer;
}

inline char* Environment::http_parser_buffer() const {
  return http_parser_buffer_;
}

inline void Environment::set_http_parser_buffer(char* buffer) {
  CHECK_EQ(http_parser_buffer_, nullptr);  // Should be set only once.
  http_parser_buffer_ = buffer;
}

inline http2::http2_state* Environment::http2_state_buffer() const {
  return http2_state_buffer_;
}

inline void Environment::set_http2_state_buffer(http2::http2_state* buffer) {
  CHECK_EQ(http2_state_buffer_, nullptr);  // Should be set only once.
  http2_state_buffer_ = buffer;
}

inline double* Environment::fs_stats_field_array() const {
  return fs_stats_field_array_;
}

inline void Environment::set_fs_stats_field_array(double* fields) {
  CHECK_EQ(fs_stats_field_array_, nullptr);  // Should be set only once.
  fs_stats_field_array_ = fields;
}

inline performance::performance_state* Environment::performance_state() {
  return performance_state_;
}

inline std::map<std::string, uint64_t>* Environment::performance_marks() {
  return &performance_marks_;
}

inline Environment* Environment::from_performance_check_handle(
    uv_check_t* handle) {
  return ContainerOf(&Environment::performance_check_handle_, handle);
}

inline Environment* Environment::from_performance_idle_handle(
    uv_idle_t* handle) {
  return ContainerOf(&Environment::performance_idle_handle_, handle);
}

inline Environment* Environment::from_performance_prepare_handle(
    uv_prepare_t* handle) {
  return ContainerOf(&Environment::performance_prepare_handle_, handle);
}

inline uv_check_t* Environment::performance_check_handle() {
  return &performance_check_handle_;
}

inline uv_idle_t* Environment::performance_idle_handle() {
  return &performance_idle_handle_;
}

inline uv_prepare_t* Environment::performance_prepare_handle() {
  return &performance_prepare_handle_;
}

inline IsolateData* Environment::isolate_data() const {
  return isolate_data_;
}

inline void Environment::ThrowError(const char* errmsg) {
  ThrowError(v8::Exception::Error, errmsg);
}

inline void Environment::ThrowTypeError(const char* errmsg) {
  ThrowError(v8::Exception::TypeError, errmsg);
}

inline void Environment::ThrowRangeError(const char* errmsg) {
  ThrowError(v8::Exception::RangeError, errmsg);
}

inline void Environment::ThrowError(
    v8::Local<v8::Value> (*fun)(v8::Local<v8::String>),
    const char* errmsg) {
  v8::HandleScope handle_scope(isolate());
  isolate()->ThrowException(fun(OneByteString(isolate(), errmsg)));
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
                                          const char* path,
                                          const char* dest) {
  isolate()->ThrowException(
      UVException(isolate(), errorno, syscall, message, path, dest));
}

inline v8::Local<v8::FunctionTemplate>
    Environment::NewFunctionTemplate(v8::FunctionCallback callback,
                                     v8::Local<v8::Signature> signature) {
  v8::Local<v8::External> external = as_external();
  return v8::FunctionTemplate::New(isolate(), callback, external, signature);
}

inline void Environment::SetMethod(v8::Local<v8::Object> that,
                                   const char* name,
                                   v8::FunctionCallback callback) {
  v8::Local<v8::Function> function =
      NewFunctionTemplate(callback)->GetFunction();
  // kInternalized strings are created in the old space.
  const v8::NewStringType type = v8::NewStringType::kInternalized;
  v8::Local<v8::String> name_string =
      v8::String::NewFromUtf8(isolate(), name, type).ToLocalChecked();
  that->Set(name_string, function);
  function->SetName(name_string);  // NODE_SET_METHOD() compatibility.
}

inline void Environment::SetProtoMethod(v8::Local<v8::FunctionTemplate> that,
                                        const char* name,
                                        v8::FunctionCallback callback) {
  v8::Local<v8::Signature> signature = v8::Signature::New(isolate(), that);
  v8::Local<v8::FunctionTemplate> t = NewFunctionTemplate(callback, signature);
  // kInternalized strings are created in the old space.
  const v8::NewStringType type = v8::NewStringType::kInternalized;
  v8::Local<v8::String> name_string =
      v8::String::NewFromUtf8(isolate(), name, type).ToLocalChecked();
  that->PrototypeTemplate()->Set(name_string, t);
  t->SetClassName(name_string);  // NODE_SET_PROTOTYPE_METHOD() compatibility.
}

inline void Environment::SetTemplateMethod(v8::Local<v8::FunctionTemplate> that,
                                           const char* name,
                                           v8::FunctionCallback callback) {
  v8::Local<v8::FunctionTemplate> t = NewFunctionTemplate(callback);
  // kInternalized strings are created in the old space.
  const v8::NewStringType type = v8::NewStringType::kInternalized;
  v8::Local<v8::String> name_string =
      v8::String::NewFromUtf8(isolate(), name, type).ToLocalChecked();
  that->Set(name_string, t);
  t->SetClassName(name_string);  // NODE_SET_METHOD() compatibility.
}

#define VP(PropertyName, StringValue) V(v8::Private, PropertyName)
#define VS(PropertyName, StringValue) V(v8::String, PropertyName)
#define V(TypeName, PropertyName)                                             \
  inline                                                                      \
  v8::Local<TypeName> IsolateData::PropertyName(v8::Isolate* isolate) const { \
    /* Strings are immutable so casting away const-ness here is okay. */      \
    return const_cast<IsolateData*>(this)->PropertyName ## _.Get(isolate);    \
  }
  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(VP)
  PER_ISOLATE_STRING_PROPERTIES(VS)
#undef V
#undef VS
#undef VP

#define VP(PropertyName, StringValue) V(v8::Private, PropertyName)
#define VS(PropertyName, StringValue) V(v8::String, PropertyName)
#define V(TypeName, PropertyName)                                             \
  inline v8::Local<TypeName> Environment::PropertyName() const {              \
    return isolate_data()->PropertyName(isolate());                           \
  }
  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(VP)
  PER_ISOLATE_STRING_PROPERTIES(VS)
#undef V
#undef VS
#undef VP

#define V(PropertyName, TypeName)                                             \
  inline v8::Local<TypeName> Environment::PropertyName() const {              \
    return StrongPersistentToLocal(PropertyName ## _);                        \
  }                                                                           \
  inline void Environment::set_ ## PropertyName(v8::Local<TypeName> value) {  \
    PropertyName ## _.Reset(isolate(), value);                                \
  }
  ENVIRONMENT_STRONG_PERSISTENT_PROPERTIES(V)
#undef V

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_ENV_INL_H_
