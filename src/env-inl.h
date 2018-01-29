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

#include "aliased_buffer.h"
#include "env.h"
#include "node.h"
#include "util-inl.h"
#include "uv.h"
#include "v8.h"
#include "node_perf_common.h"

#include <stddef.h>
#include <stdint.h>

namespace node {

inline v8::Isolate* IsolateData::isolate() const {
  return isolate_;
}

inline uv_loop_t* IsolateData::event_loop() const {
  return event_loop_;
}

inline uint32_t* IsolateData::zero_fill_field() const {
  return zero_fill_field_;
}

inline MultiIsolatePlatform* IsolateData::platform() const {
  return platform_;
}

inline Environment::AsyncHooks::AsyncHooks()
    : async_ids_stack_(env()->isolate(), 16 * 2),
      fields_(env()->isolate(), kFieldsCount),
      async_id_fields_(env()->isolate(), kUidFieldsCount) {
  v8::HandleScope handle_scope(env()->isolate());

  // Always perform async_hooks checks, not just when async_hooks is enabled.
  // TODO(AndreasMadsen): Consider removing this for LTS releases.
  // See discussion in https://github.com/nodejs/node/pull/15454
  // When removing this, do it by reverting the commit. Otherwise the test
  // and flag changes won't be included.
  fields_[kCheck] = 1;

  // kDefaultTriggerAsyncId should be -1, this indicates that there is no
  // specified default value and it should fallback to the executionAsyncId.
  // 0 is not used as the magic value, because that indicates a missing context
  // which is different from a default context.
  async_id_fields_[AsyncHooks::kDefaultTriggerAsyncId] = -1;

  // kAsyncIdCounter should start at 1 because that'll be the id the execution
  // context during bootstrap (code that runs before entering uv_run()).
  async_id_fields_[AsyncHooks::kAsyncIdCounter] = 1;

  // Create all the provider strings that will be passed to JS. Place them in
  // an array so the array index matches the PROVIDER id offset. This way the
  // strings can be retrieved quickly.
#define V(Provider)                                                           \
  providers_[AsyncWrap::PROVIDER_ ## Provider].Set(                           \
      env()->isolate(),                                                       \
      v8::String::NewFromOneByte(                                             \
        env()->isolate(),                                                     \
        reinterpret_cast<const uint8_t*>(#Provider),                          \
        v8::NewStringType::kInternalized,                                     \
        sizeof(#Provider) - 1).ToLocalChecked());
  NODE_ASYNC_PROVIDER_TYPES(V)
#undef V
}

inline AliasedBuffer<uint32_t, v8::Uint32Array>&
Environment::AsyncHooks::fields() {
  return fields_;
}

inline AliasedBuffer<double, v8::Float64Array>&
Environment::AsyncHooks::async_id_fields() {
  return async_id_fields_;
}

inline AliasedBuffer<double, v8::Float64Array>&
Environment::AsyncHooks::async_ids_stack() {
  return async_ids_stack_;
}

inline v8::Local<v8::String> Environment::AsyncHooks::provider_string(int idx) {
  return providers_[idx].Get(env()->isolate());
}

inline void Environment::AsyncHooks::no_force_checks() {
  // fields_ does not have the -= operator defined
  fields_[kCheck] = fields_[kCheck] - 1;
}

inline Environment* Environment::AsyncHooks::env() {
  return Environment::ForAsyncHooks(this);
}

// Remember to keep this code aligned with pushAsyncIds() in JS.
inline void Environment::AsyncHooks::push_async_ids(double async_id,
                                              double trigger_async_id) {
  // Since async_hooks is experimental, do only perform the check
  // when async_hooks is enabled.
  if (fields_[kCheck] > 0) {
    CHECK_GE(async_id, -1);
    CHECK_GE(trigger_async_id, -1);
  }

  uint32_t offset = fields_[kStackLength];
  if (offset * 2 >= async_ids_stack_.Length())
    grow_async_ids_stack();
  async_ids_stack_[2 * offset] = async_id_fields_[kExecutionAsyncId];
  async_ids_stack_[2 * offset + 1] = async_id_fields_[kTriggerAsyncId];
  fields_[kStackLength] = fields_[kStackLength] + 1;
  async_id_fields_[kExecutionAsyncId] = async_id;
  async_id_fields_[kTriggerAsyncId] = trigger_async_id;
}

// Remember to keep this code aligned with popAsyncIds() in JS.
inline bool Environment::AsyncHooks::pop_async_id(double async_id) {
  // In case of an exception then this may have already been reset, if the
  // stack was multiple MakeCallback()'s deep.
  if (fields_[kStackLength] == 0) return false;

  // Ask for the async_id to be restored as a check that the stack
  // hasn't been corrupted.
  // Since async_hooks is experimental, do only perform the check
  // when async_hooks is enabled.
  if (fields_[kCheck] > 0 && async_id_fields_[kExecutionAsyncId] != async_id) {
    fprintf(stderr,
            "Error: async hook stack has become corrupted ("
            "actual: %.f, expected: %.f)\n",
            async_id_fields_.GetValue(kExecutionAsyncId),
            async_id);
    DumpBacktrace(stderr);
    fflush(stderr);
    if (!env()->abort_on_uncaught_exception())
      exit(1);
    fprintf(stderr, "\n");
    fflush(stderr);
    ABORT_NO_BACKTRACE();
  }

  uint32_t offset = fields_[kStackLength] - 1;
  async_id_fields_[kExecutionAsyncId] = async_ids_stack_[2 * offset];
  async_id_fields_[kTriggerAsyncId] = async_ids_stack_[2 * offset + 1];
  fields_[kStackLength] = offset;

  return fields_[kStackLength] > 0;
}

inline void Environment::AsyncHooks::clear_async_id_stack() {
  async_id_fields_[kExecutionAsyncId] = 0;
  async_id_fields_[kTriggerAsyncId] = 0;
  fields_[kStackLength] = 0;
}

inline Environment::AsyncHooks::DefaultTriggerAsyncIdScope
  ::DefaultTriggerAsyncIdScope(Environment* env,
                               double default_trigger_async_id)
    : async_id_fields_ref_(env->async_hooks()->async_id_fields()) {
  if (env->async_hooks()->fields()[AsyncHooks::kCheck] > 0) {
    CHECK_GE(default_trigger_async_id, 0);
  }

  old_default_trigger_async_id_ =
    async_id_fields_ref_[AsyncHooks::kDefaultTriggerAsyncId];
  async_id_fields_ref_[AsyncHooks::kDefaultTriggerAsyncId] =
    default_trigger_async_id;
}

inline Environment::AsyncHooks::DefaultTriggerAsyncIdScope
  ::~DefaultTriggerAsyncIdScope() {
  async_id_fields_ref_[AsyncHooks::kDefaultTriggerAsyncId] =
    old_default_trigger_async_id_;
}


Environment* Environment::ForAsyncHooks(AsyncHooks* hooks) {
  return ContainerOf(&Environment::async_hooks_, hooks);
}


inline Environment::AsyncCallbackScope::AsyncCallbackScope(Environment* env)
    : env_(env) {
  env_->makecallback_cntr_++;
}

inline Environment::AsyncCallbackScope::~AsyncCallbackScope() {
  env_->makecallback_cntr_--;
}

inline bool Environment::AsyncCallbackScope::in_makecallback() const {
  return env_->makecallback_cntr_ > 1;
}

inline Environment::ImmediateInfo::ImmediateInfo(v8::Isolate* isolate)
    : fields_(isolate, kFieldsCount) {}

inline AliasedBuffer<uint32_t, v8::Uint32Array>&
    Environment::ImmediateInfo::fields() {
  return fields_;
}

inline uint32_t Environment::ImmediateInfo::count() const {
  return fields_[kCount];
}

inline uint32_t Environment::ImmediateInfo::ref_count() const {
  return fields_[kRefCount];
}

inline bool Environment::ImmediateInfo::has_outstanding() const {
  return fields_[kHasOutstanding] == 1;
}

inline void Environment::ImmediateInfo::count_inc(uint32_t increment) {
  fields_[kCount] = fields_[kCount] + increment;
}

inline void Environment::ImmediateInfo::count_dec(uint32_t decrement) {
  fields_[kCount] = fields_[kCount] - decrement;
}

inline void Environment::ImmediateInfo::ref_count_inc(uint32_t increment) {
  fields_[kRefCount] = fields_[kRefCount] + increment;
}

inline void Environment::ImmediateInfo::ref_count_dec(uint32_t decrement) {
  fields_[kRefCount] = fields_[kRefCount] - decrement;
}

inline Environment::TickInfo::TickInfo(v8::Isolate* isolate)
    : fields_(isolate, kFieldsCount) {}

inline AliasedBuffer<uint8_t, v8::Uint8Array>& Environment::TickInfo::fields() {
  return fields_;
}

inline bool Environment::TickInfo::has_scheduled() const {
  return fields_[kHasScheduled] == 1;
}

inline bool Environment::TickInfo::has_promise_rejections() const {
  return fields_[kHasPromiseRejections] == 1;
}

inline void Environment::TickInfo::promise_rejections_toggle_on() {
  fields_[kHasPromiseRejections] = 1;
}

inline void Environment::AssignToContext(v8::Local<v8::Context> context,
                                         const ContextInfo& info) {
  context->SetAlignedPointerInEmbedderData(kContextEmbedderDataIndex, this);
#if HAVE_INSPECTOR
  inspector_agent()->ContextCreated(context, info);
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
      immediate_info_(context->GetIsolate()),
      tick_info_(context->GetIsolate()),
      timer_base_(uv_now(isolate_data->event_loop())),
      printed_error_(false),
      trace_sync_io_(false),
      abort_on_uncaught_exception_(false),
      emit_napi_warning_(true),
      makecallback_cntr_(0),
      should_abort_on_uncaught_toggle_(isolate_, 1),
#if HAVE_INSPECTOR
      inspector_agent_(new inspector::Agent(this)),
#endif
      handle_cleanup_waiting_(0),
      http_parser_buffer_(nullptr),
      fs_stats_field_array_(isolate_, kFsStatsFieldsLength),
      context_(context->GetIsolate(), context) {
  // We'll be creating new objects so make sure we've entered the context.
  v8::HandleScope handle_scope(isolate());
  v8::Context::Scope context_scope(context);
  set_as_external(v8::External::New(isolate(), this));

  AssignToContext(context, ContextInfo(""));

  destroy_async_id_list_.reserve(512);
  performance_state_.reset(new performance::performance_state(isolate()));
  performance_state_->milestones[
      performance::NODE_PERFORMANCE_MILESTONE_ENVIRONMENT] =
          PERFORMANCE_NOW();
  performance_state_->milestones[
    performance::NODE_PERFORMANCE_MILESTONE_NODE_START] =
        performance::performance_node_start;
  performance_state_->milestones[
    performance::NODE_PERFORMANCE_MILESTONE_V8_START] =
        performance::performance_v8_start;

  // By default, always abort when --abort-on-uncaught-exception was passed.
  should_abort_on_uncaught_toggle_[0] = 1;
}

inline Environment::~Environment() {
  v8::HandleScope handle_scope(isolate());

#if HAVE_INSPECTOR
  // Destroy inspector agent before erasing the context. The inspector
  // destructor depends on the context still being accessible.
  inspector_agent_.reset();
#endif

  context()->SetAlignedPointerInEmbedderData(kContextEmbedderDataIndex,
                                             nullptr);
#define V(PropertyName, TypeName) PropertyName ## _.Reset();
  ENVIRONMENT_STRONG_PERSISTENT_PROPERTIES(V)
#undef V

  delete[] heap_statistics_buffer_;
  delete[] heap_space_statistics_buffer_;
  delete[] http_parser_buffer_;
}

inline v8::Isolate* Environment::isolate() const {
  return isolate_;
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

inline Environment::ImmediateInfo* Environment::immediate_info() {
  return &immediate_info_;
}

inline Environment::TickInfo* Environment::tick_info() {
  return &tick_info_;
}

inline uint64_t Environment::timer_base() const {
  return timer_base_;
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

inline AliasedBuffer<uint32_t, v8::Uint32Array>&
Environment::should_abort_on_uncaught_toggle() {
  return should_abort_on_uncaught_toggle_;
}

Environment::ShouldNotAbortOnUncaughtScope::ShouldNotAbortOnUncaughtScope(
    Environment* env)
    : env_(env) {
  env_->should_not_abort_scope_counter_++;
}

Environment::ShouldNotAbortOnUncaughtScope::~ShouldNotAbortOnUncaughtScope() {
  Close();
}

void Environment::ShouldNotAbortOnUncaughtScope::Close() {
  if (env_ != nullptr) {
    env_->should_not_abort_scope_counter_--;
    env_ = nullptr;
  }
}

bool Environment::inside_should_not_abort_on_uncaught_scope() const {
  return should_not_abort_scope_counter_ > 0;
}

inline std::vector<double>* Environment::destroy_async_id_list() {
  return &destroy_async_id_list_;
}

inline double Environment::new_async_id() {
  async_hooks()->async_id_fields()[AsyncHooks::kAsyncIdCounter] =
    async_hooks()->async_id_fields()[AsyncHooks::kAsyncIdCounter] + 1;
  return async_hooks()->async_id_fields()[AsyncHooks::kAsyncIdCounter];
}

inline double Environment::execution_async_id() {
  return async_hooks()->async_id_fields()[AsyncHooks::kExecutionAsyncId];
}

inline double Environment::trigger_async_id() {
  return async_hooks()->async_id_fields()[AsyncHooks::kTriggerAsyncId];
}

inline double Environment::get_default_trigger_async_id() {
  double default_trigger_async_id =
    async_hooks()->async_id_fields()[AsyncHooks::kDefaultTriggerAsyncId];
  // If defaultTriggerAsyncId isn't set, use the executionAsyncId
  if (default_trigger_async_id < 0)
    default_trigger_async_id = execution_async_id();
  return default_trigger_async_id;
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

inline http2::http2_state* Environment::http2_state() const {
  return http2_state_.get();
}

inline void Environment::set_http2_state(
    std::unique_ptr<http2::http2_state> buffer) {
  CHECK(!http2_state_);  // Should be set only once.
  http2_state_ = std::move(buffer);
}

inline AliasedBuffer<double, v8::Float64Array>*
Environment::fs_stats_field_array() {
  return &fs_stats_field_array_;
}

void Environment::CreateImmediate(native_immediate_callback cb,
                               void* data,
                               v8::Local<v8::Object> obj,
                               bool ref) {
  native_immediate_callbacks_.push_back({
    cb,
    data,
    std::unique_ptr<v8::Persistent<v8::Object>>(obj.IsEmpty() ?
        nullptr : new v8::Persistent<v8::Object>(isolate_, obj)),
    ref
  });
  immediate_info()->count_inc(1);
}

void Environment::SetImmediate(native_immediate_callback cb,
                               void* data,
                               v8::Local<v8::Object> obj) {
  CreateImmediate(cb, data, obj, true);

  if (immediate_info()->ref_count() == 0)
    ToggleImmediateRef(true);
  immediate_info()->ref_count_inc(1);
}

void Environment::SetUnrefImmediate(native_immediate_callback cb,
                                    void* data,
                                    v8::Local<v8::Object> obj) {
  CreateImmediate(cb, data, obj, false);
}

inline performance::performance_state* Environment::performance_state() {
  return performance_state_.get();
}

inline std::map<std::string, uint64_t>* Environment::performance_marks() {
  return &performance_marks_;
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
