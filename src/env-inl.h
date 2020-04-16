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
#include "node_context_data.h"

#include <cstddef>
#include <cstdint>

#include <utility>

namespace node {

inline v8::Isolate* IsolateData::isolate() const {
  return isolate_;
}

inline uv_loop_t* IsolateData::event_loop() const {
  return event_loop_;
}

inline bool IsolateData::uses_node_allocator() const {
  return uses_node_allocator_;
}

inline v8::ArrayBuffer::Allocator* IsolateData::allocator() const {
  return allocator_;
}

inline NodeArrayBufferAllocator* IsolateData::node_allocator() const {
  return node_allocator_;
}

inline MultiIsolatePlatform* IsolateData::platform() const {
  return platform_;
}

inline AsyncHooks::AsyncHooks()
    : async_ids_stack_(env()->isolate(), 16 * 2),
      fields_(env()->isolate(), kFieldsCount),
      async_id_fields_(env()->isolate(), kUidFieldsCount) {
  clear_async_id_stack();

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
inline AliasedUint32Array& AsyncHooks::fields() {
  return fields_;
}

inline AliasedFloat64Array& AsyncHooks::async_id_fields() {
  return async_id_fields_;
}

inline AliasedFloat64Array& AsyncHooks::async_ids_stack() {
  return async_ids_stack_;
}

inline v8::Local<v8::Array> AsyncHooks::execution_async_resources() {
  return PersistentToLocal::Strong(execution_async_resources_);
}

inline v8::Local<v8::String> AsyncHooks::provider_string(int idx) {
  return providers_[idx].Get(env()->isolate());
}

inline void AsyncHooks::no_force_checks() {
  fields_[kCheck] -= 1;
}

inline Environment* AsyncHooks::env() {
  return Environment::ForAsyncHooks(this);
}

// Remember to keep this code aligned with pushAsyncContext() in JS.
inline void AsyncHooks::push_async_context(double async_id,
                                           double trigger_async_id,
                                           v8::Local<v8::Value> resource) {
  v8::HandleScope handle_scope(env()->isolate());

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
  fields_[kStackLength] += 1;
  async_id_fields_[kExecutionAsyncId] = async_id;
  async_id_fields_[kTriggerAsyncId] = trigger_async_id;

  auto resources = execution_async_resources();
  USE(resources->Set(env()->context(), offset, resource));
}

// Remember to keep this code aligned with popAsyncContext() in JS.
inline bool AsyncHooks::pop_async_context(double async_id) {
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

  auto resources = execution_async_resources();
  USE(resources->Delete(env()->context(), offset));

  return fields_[kStackLength] > 0;
}

// Keep in sync with clearAsyncIdStack in lib/internal/async_hooks.js.
inline void AsyncHooks::clear_async_id_stack() {
  auto isolate = env()->isolate();
  v8::HandleScope handle_scope(isolate);
  execution_async_resources_.Reset(isolate, v8::Array::New(isolate));

  async_id_fields_[kExecutionAsyncId] = 0;
  async_id_fields_[kTriggerAsyncId] = 0;
  fields_[kStackLength] = 0;
}

// The DefaultTriggerAsyncIdScope(AsyncWrap*) constructor is defined in
// async_wrap-inl.h to avoid a circular dependency.

inline AsyncHooks::DefaultTriggerAsyncIdScope ::DefaultTriggerAsyncIdScope(
    Environment* env, double default_trigger_async_id)
    : async_hooks_(env->async_hooks()) {
  if (env->async_hooks()->fields()[AsyncHooks::kCheck] > 0) {
    CHECK_GE(default_trigger_async_id, 0);
  }

  old_default_trigger_async_id_ =
    async_hooks_->async_id_fields()[AsyncHooks::kDefaultTriggerAsyncId];
  async_hooks_->async_id_fields()[AsyncHooks::kDefaultTriggerAsyncId] =
    default_trigger_async_id;
}

inline AsyncHooks::DefaultTriggerAsyncIdScope ::~DefaultTriggerAsyncIdScope() {
  async_hooks_->async_id_fields()[AsyncHooks::kDefaultTriggerAsyncId] =
    old_default_trigger_async_id_;
}

Environment* Environment::ForAsyncHooks(AsyncHooks* hooks) {
  return ContainerOf(&Environment::async_hooks_, hooks);
}

inline size_t Environment::async_callback_scope_depth() const {
  return async_callback_scope_depth_;
}

inline void Environment::PushAsyncCallbackScope() {
  async_callback_scope_depth_++;
}

inline void Environment::PopAsyncCallbackScope() {
  async_callback_scope_depth_--;
}

inline ImmediateInfo::ImmediateInfo(v8::Isolate* isolate)
    : fields_(isolate, kFieldsCount) {}

inline AliasedUint32Array& ImmediateInfo::fields() {
  return fields_;
}

inline uint32_t ImmediateInfo::count() const {
  return fields_[kCount];
}

inline uint32_t ImmediateInfo::ref_count() const {
  return fields_[kRefCount];
}

inline bool ImmediateInfo::has_outstanding() const {
  return fields_[kHasOutstanding] == 1;
}

inline void ImmediateInfo::ref_count_inc(uint32_t increment) {
  fields_[kRefCount] += increment;
}

inline void ImmediateInfo::ref_count_dec(uint32_t decrement) {
  fields_[kRefCount] -= decrement;
}

inline TickInfo::TickInfo(v8::Isolate* isolate)
    : fields_(isolate, kFieldsCount) {}

inline AliasedUint8Array& TickInfo::fields() {
  return fields_;
}

inline bool TickInfo::has_tick_scheduled() const {
  return fields_[kHasTickScheduled] == 1;
}

inline bool TickInfo::has_rejection_to_warn() const {
  return fields_[kHasRejectionToWarn] == 1;
}

inline void Environment::AssignToContext(v8::Local<v8::Context> context,
                                         const ContextInfo& info) {
  context->SetAlignedPointerInEmbedderData(
      ContextEmbedderIndex::kEnvironment, this);
  // Used by Environment::GetCurrent to know that we are on a node context.
  context->SetAlignedPointerInEmbedderData(
    ContextEmbedderIndex::kContextTag, Environment::kNodeContextTagPtr);
#if HAVE_INSPECTOR
  inspector_agent()->ContextCreated(context, info);
#endif  // HAVE_INSPECTOR
}

inline Environment* Environment::GetCurrent(v8::Isolate* isolate) {
  if (UNLIKELY(!isolate->InContext())) return nullptr;
  v8::HandleScope handle_scope(isolate);
  return GetCurrent(isolate->GetCurrentContext());
}

inline Environment* Environment::GetCurrent(v8::Local<v8::Context> context) {
  if (UNLIKELY(context.IsEmpty())) {
    return nullptr;
  }
  if (UNLIKELY(context->GetNumberOfEmbedderDataFields() <=
               ContextEmbedderIndex::kContextTag)) {
    return nullptr;
  }
  if (UNLIKELY(context->GetAlignedPointerFromEmbedderData(
                   ContextEmbedderIndex::kContextTag) !=
               Environment::kNodeContextTagPtr)) {
    return nullptr;
  }
  return static_cast<Environment*>(
      context->GetAlignedPointerFromEmbedderData(
          ContextEmbedderIndex::kEnvironment));
}

inline Environment* Environment::GetCurrent(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  return GetFromCallbackData(info.Data());
}

template <typename T>
inline Environment* Environment::GetCurrent(
    const v8::PropertyCallbackInfo<T>& info) {
  return GetFromCallbackData(info.Data());
}

inline Environment* Environment::GetFromCallbackData(v8::Local<v8::Value> val) {
  DCHECK(val->IsObject());
  v8::Local<v8::Object> obj = val.As<v8::Object>();
  DCHECK_GE(obj->InternalFieldCount(), 1);
  Environment* env =
      static_cast<Environment*>(obj->GetAlignedPointerFromInternalField(0));
  DCHECK(env->as_callback_data_template()->HasInstance(obj));
  return env;
}

inline Environment* Environment::GetThreadLocalEnv() {
  return static_cast<Environment*>(uv_key_get(&thread_local_env));
}

inline bool Environment::profiler_idle_notifier_started() const {
  return profiler_idle_notifier_started_;
}

inline v8::Isolate* Environment::isolate() const {
  return isolate_;
}

inline Environment* Environment::from_timer_handle(uv_timer_t* handle) {
  return ContainerOf(&Environment::timer_handle_, handle);
}

inline uv_timer_t* Environment::timer_handle() {
  return &timer_handle_;
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
                                               void* arg) {
  handle_cleanup_queue_.push_back(HandleCleanup{handle, cb, arg});
}

template <typename T, typename OnCloseCallback>
inline void Environment::CloseHandle(T* handle, OnCloseCallback callback) {
  handle_cleanup_waiting_++;
  static_assert(sizeof(T) >= sizeof(uv_handle_t), "T is a libuv handle");
  static_assert(offsetof(T, data) == offsetof(uv_handle_t, data),
                "T is a libuv handle");
  static_assert(offsetof(T, close_cb) == offsetof(uv_handle_t, close_cb),
                "T is a libuv handle");
  struct CloseData {
    Environment* env;
    OnCloseCallback callback;
    void* original_data;
  };
  handle->data = new CloseData { this, callback, handle->data };
  uv_close(reinterpret_cast<uv_handle_t*>(handle), [](uv_handle_t* handle) {
    std::unique_ptr<CloseData> data { static_cast<CloseData*>(handle->data) };
    data->env->handle_cleanup_waiting_--;
    handle->data = data->original_data;
    data->callback(reinterpret_cast<T*>(handle));
  });
}

void Environment::IncreaseWaitingRequestCounter() {
  request_waiting_++;
}

void Environment::DecreaseWaitingRequestCounter() {
  request_waiting_--;
  CHECK_GE(request_waiting_, 0);
}

inline uv_loop_t* Environment::event_loop() const {
  return isolate_data()->event_loop();
}

inline void Environment::TryLoadAddon(
    const char* filename,
    int flags,
    const std::function<bool(binding::DLib*)>& was_loaded) {
  loaded_addons_.emplace_back(filename, flags);
  if (!was_loaded(&loaded_addons_.back())) {
    loaded_addons_.pop_back();
  }
}

#if HAVE_INSPECTOR
inline bool Environment::is_in_inspector_console_call() const {
  return is_in_inspector_console_call_;
}

inline void Environment::set_is_in_inspector_console_call(bool value) {
  is_in_inspector_console_call_ = value;
}
#endif

inline AsyncHooks* Environment::async_hooks() {
  return &async_hooks_;
}

inline ImmediateInfo* Environment::immediate_info() {
  return &immediate_info_;
}

inline TickInfo* Environment::tick_info() {
  return &tick_info_;
}

inline uint64_t Environment::timer_base() const {
  return timer_base_;
}

inline std::shared_ptr<KVStore> Environment::env_vars() {
  return env_vars_;
}

inline void Environment::set_env_vars(std::shared_ptr<KVStore> env_vars) {
  env_vars_ = env_vars;
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
  return options_->abort_on_uncaught_exception;
}

inline void Environment::set_abort_on_uncaught_exception(bool value) {
  options_->abort_on_uncaught_exception = value;
}

inline AliasedUint32Array& Environment::should_abort_on_uncaught_toggle() {
  return should_abort_on_uncaught_toggle_;
}

inline AliasedInt32Array& Environment::stream_base_state() {
  return stream_base_state_;
}

inline uint32_t Environment::get_next_module_id() {
  return module_id_counter_++;
}
inline uint32_t Environment::get_next_script_id() {
  return script_id_counter_++;
}
inline uint32_t Environment::get_next_function_id() {
  return function_id_counter_++;
}

ShouldNotAbortOnUncaughtScope::ShouldNotAbortOnUncaughtScope(
    Environment* env)
    : env_(env) {
  env_->PushShouldNotAbortOnUncaughtScope();
}

ShouldNotAbortOnUncaughtScope::~ShouldNotAbortOnUncaughtScope() {
  Close();
}

void ShouldNotAbortOnUncaughtScope::Close() {
  if (env_ != nullptr) {
    env_->PopShouldNotAbortOnUncaughtScope();
    env_ = nullptr;
  }
}

inline void Environment::PushShouldNotAbortOnUncaughtScope() {
  should_not_abort_scope_counter_++;
}

inline void Environment::PopShouldNotAbortOnUncaughtScope() {
  should_not_abort_scope_counter_--;
}

inline bool Environment::inside_should_not_abort_on_uncaught_scope() const {
  return should_not_abort_scope_counter_ > 0;
}

inline std::vector<double>* Environment::destroy_async_id_list() {
  return &destroy_async_id_list_;
}

inline double Environment::new_async_id() {
  async_hooks()->async_id_fields()[AsyncHooks::kAsyncIdCounter] += 1;
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
  CHECK_NOT_NULL(heap_statistics_buffer_);
  return heap_statistics_buffer_;
}

inline void Environment::set_heap_statistics_buffer(double* pointer) {
  CHECK_NULL(heap_statistics_buffer_);  // Should be set only once.
  heap_statistics_buffer_ = pointer;
}

inline double* Environment::heap_space_statistics_buffer() const {
  CHECK_NOT_NULL(heap_space_statistics_buffer_);
  return heap_space_statistics_buffer_;
}

inline void Environment::set_heap_space_statistics_buffer(double* pointer) {
  CHECK_NULL(heap_space_statistics_buffer_);  // Should be set only once.
  heap_space_statistics_buffer_ = pointer;
}

inline double* Environment::heap_code_statistics_buffer() const {
  CHECK_NOT_NULL(heap_code_statistics_buffer_);
  return heap_code_statistics_buffer_;
}

inline void Environment::set_heap_code_statistics_buffer(double* pointer) {
  CHECK_NULL(heap_code_statistics_buffer_);  // Should be set only once.
  heap_code_statistics_buffer_ = pointer;
}

inline char* Environment::http_parser_buffer() const {
  return http_parser_buffer_;
}

inline void Environment::set_http_parser_buffer(char* buffer) {
  CHECK_NULL(http_parser_buffer_);  // Should be set only once.
  http_parser_buffer_ = buffer;
}

inline bool Environment::http_parser_buffer_in_use() const {
  return http_parser_buffer_in_use_;
}

inline void Environment::set_http_parser_buffer_in_use(bool in_use) {
  http_parser_buffer_in_use_ = in_use;
}

inline http2::Http2State* Environment::http2_state() const {
  return http2_state_.get();
}

inline void Environment::set_http2_state(
    std::unique_ptr<http2::Http2State> buffer) {
  CHECK(!http2_state_);  // Should be set only once.
  http2_state_ = std::move(buffer);
}

inline AliasedFloat64Array* Environment::fs_stats_field_array() {
  return &fs_stats_field_array_;
}

inline AliasedBigUint64Array* Environment::fs_stats_field_bigint_array() {
  return &fs_stats_field_bigint_array_;
}

inline std::vector<std::unique_ptr<fs::FileHandleReadWrap>>&
Environment::file_handle_read_wrap_freelist() {
  return file_handle_read_wrap_freelist_;
}

inline std::shared_ptr<EnvironmentOptions> Environment::options() {
  return options_;
}

inline const std::vector<std::string>& Environment::argv() {
  return argv_;
}

inline const std::vector<std::string>& Environment::exec_argv() {
  return exec_argv_;
}

inline const std::string& Environment::exec_path() const {
  return exec_path_;
}

#if HAVE_INSPECTOR
inline void Environment::set_coverage_directory(const char* dir) {
  coverage_directory_ = std::string(dir);
}

inline void Environment::set_coverage_connection(
    std::unique_ptr<profiler::V8CoverageConnection> connection) {
  CHECK_NULL(coverage_connection_);
  std::swap(coverage_connection_, connection);
}

inline profiler::V8CoverageConnection* Environment::coverage_connection() {
  return coverage_connection_.get();
}

inline const std::string& Environment::coverage_directory() const {
  return coverage_directory_;
}

inline void Environment::set_cpu_profiler_connection(
    std::unique_ptr<profiler::V8CpuProfilerConnection> connection) {
  CHECK_NULL(cpu_profiler_connection_);
  std::swap(cpu_profiler_connection_, connection);
}

inline profiler::V8CpuProfilerConnection*
Environment::cpu_profiler_connection() {
  return cpu_profiler_connection_.get();
}

inline void Environment::set_cpu_prof_interval(uint64_t interval) {
  cpu_prof_interval_ = interval;
}

inline uint64_t Environment::cpu_prof_interval() const {
  return cpu_prof_interval_;
}

inline void Environment::set_cpu_prof_name(const std::string& name) {
  cpu_prof_name_ = name;
}

inline const std::string& Environment::cpu_prof_name() const {
  return cpu_prof_name_;
}

inline void Environment::set_cpu_prof_dir(const std::string& dir) {
  cpu_prof_dir_ = dir;
}

inline const std::string& Environment::cpu_prof_dir() const {
  return cpu_prof_dir_;
}

inline void Environment::set_heap_profiler_connection(
    std::unique_ptr<profiler::V8HeapProfilerConnection> connection) {
  CHECK_NULL(heap_profiler_connection_);
  std::swap(heap_profiler_connection_, connection);
}

inline profiler::V8HeapProfilerConnection*
Environment::heap_profiler_connection() {
  return heap_profiler_connection_.get();
}

inline void Environment::set_heap_prof_name(const std::string& name) {
  heap_prof_name_ = name;
}

inline const std::string& Environment::heap_prof_name() const {
  return heap_prof_name_;
}

inline void Environment::set_heap_prof_dir(const std::string& dir) {
  heap_prof_dir_ = dir;
}

inline const std::string& Environment::heap_prof_dir() const {
  return heap_prof_dir_;
}

inline void Environment::set_heap_prof_interval(uint64_t interval) {
  heap_prof_interval_ = interval;
}

inline uint64_t Environment::heap_prof_interval() const {
  return heap_prof_interval_;
}

#endif  // HAVE_INSPECTOR

inline
std::shared_ptr<ExclusiveAccess<HostPort>> Environment::inspector_host_port() {
  return inspector_host_port_;
}

inline std::shared_ptr<PerIsolateOptions> IsolateData::options() {
  return options_;
}

inline void IsolateData::set_options(
    std::shared_ptr<PerIsolateOptions> options) {
  options_ = std::move(options);
}

std::unique_ptr<Environment::NativeImmediateCallback>
Environment::NativeImmediateQueue::Shift() {
  std::unique_ptr<Environment::NativeImmediateCallback> ret = std::move(head_);
  if (ret) {
    head_ = ret->get_next();
    if (!head_)
      tail_ = nullptr;  // The queue is now empty.
  }
  size_--;
  return ret;
}

void Environment::NativeImmediateQueue::Push(
    std::unique_ptr<Environment::NativeImmediateCallback> cb) {
  NativeImmediateCallback* prev_tail = tail_;

  size_++;
  tail_ = cb.get();
  if (prev_tail != nullptr)
    prev_tail->set_next(std::move(cb));
  else
    head_ = std::move(cb);
}

void Environment::NativeImmediateQueue::ConcatMove(
    NativeImmediateQueue&& other) {
  size_ += other.size_;
  if (tail_ != nullptr)
    tail_->set_next(std::move(other.head_));
  else
    head_ = std::move(other.head_);
  tail_ = other.tail_;
  other.tail_ = nullptr;
  other.size_ = 0;
}

size_t Environment::NativeImmediateQueue::size() const {
  return size_.load();
}

template <typename Fn>
void Environment::CreateImmediate(Fn&& cb, bool ref) {
  auto callback = std::make_unique<NativeImmediateCallbackImpl<Fn>>(
      std::move(cb), ref);
  native_immediates_.Push(std::move(callback));
}

template <typename Fn>
void Environment::SetImmediate(Fn&& cb) {
  CreateImmediate(std::move(cb), true);

  if (immediate_info()->ref_count() == 0)
    ToggleImmediateRef(true);
  immediate_info()->ref_count_inc(1);
}

template <typename Fn>
void Environment::SetUnrefImmediate(Fn&& cb) {
  CreateImmediate(std::move(cb), false);
}

template <typename Fn>
void Environment::SetImmediateThreadsafe(Fn&& cb) {
  auto callback = std::make_unique<NativeImmediateCallbackImpl<Fn>>(
      std::move(cb), false);
  {
    Mutex::ScopedLock lock(native_immediates_threadsafe_mutex_);
    native_immediates_threadsafe_.Push(std::move(callback));
  }
  uv_async_send(&task_queues_async_);
}

template <typename Fn>
void Environment::RequestInterrupt(Fn&& cb) {
  auto callback = std::make_unique<NativeImmediateCallbackImpl<Fn>>(
      std::move(cb), false);
  {
    Mutex::ScopedLock lock(native_immediates_threadsafe_mutex_);
    native_immediates_interrupts_.Push(std::move(callback));
  }
  uv_async_send(&task_queues_async_);
  RequestInterruptFromV8();
}

Environment::NativeImmediateCallback::NativeImmediateCallback(bool refed)
  : refed_(refed) {}

bool Environment::NativeImmediateCallback::is_refed() const {
  return refed_;
}

std::unique_ptr<Environment::NativeImmediateCallback>
Environment::NativeImmediateCallback::get_next() {
  return std::move(next_);
}

void Environment::NativeImmediateCallback::set_next(
    std::unique_ptr<NativeImmediateCallback> next) {
  next_ = std::move(next);
}

template <typename Fn>
Environment::NativeImmediateCallbackImpl<Fn>::NativeImmediateCallbackImpl(
    Fn&& callback, bool refed)
  : NativeImmediateCallback(refed),
    callback_(std::move(callback)) {}

template <typename Fn>
void Environment::NativeImmediateCallbackImpl<Fn>::Call(Environment* env) {
  callback_(env);
}

inline bool Environment::can_call_into_js() const {
  return can_call_into_js_ && !is_stopping();
}

inline void Environment::set_can_call_into_js(bool can_call_into_js) {
  can_call_into_js_ = can_call_into_js;
}

inline bool Environment::has_run_bootstrapping_code() const {
  return has_run_bootstrapping_code_;
}

inline void Environment::set_has_run_bootstrapping_code(bool value) {
  has_run_bootstrapping_code_ = value;
}

inline bool Environment::has_serialized_options() const {
  return has_serialized_options_;
}

inline void Environment::set_has_serialized_options(bool value) {
  has_serialized_options_ = value;
}

inline bool Environment::is_main_thread() const {
  return flags_ & kIsMainThread;
}

inline bool Environment::owns_process_state() const {
  return flags_ & kOwnsProcessState;
}

inline bool Environment::owns_inspector() const {
  return flags_ & kOwnsInspector;
}

inline uint64_t Environment::thread_id() const {
  return thread_id_;
}

inline worker::Worker* Environment::worker_context() const {
  return worker_context_;
}

inline void Environment::set_worker_context(worker::Worker* context) {
  CHECK_NULL(worker_context_);  // Should be set only once.
  worker_context_ = context;
}

inline void Environment::add_sub_worker_context(worker::Worker* context) {
  sub_worker_contexts_.insert(context);
}

inline void Environment::remove_sub_worker_context(worker::Worker* context) {
  sub_worker_contexts_.erase(context);
}

template <typename Fn>
inline void Environment::ForEachWorker(Fn&& iterator) {
  for (worker::Worker* w : sub_worker_contexts_) iterator(w);
}

inline void Environment::add_refs(int64_t diff) {
  task_queues_async_refs_ += diff;
  CHECK_GE(task_queues_async_refs_, 0);
  if (task_queues_async_refs_ == 0)
    uv_unref(reinterpret_cast<uv_handle_t*>(&task_queues_async_));
  else
    uv_ref(reinterpret_cast<uv_handle_t*>(&task_queues_async_));
}

inline bool Environment::is_stopping() const {
  return is_stopping_.load();
}

inline void Environment::set_stopping(bool value) {
  is_stopping_.store(value);
}

inline std::list<node_module>* Environment::extra_linked_bindings() {
  return &extra_linked_bindings_;
}

inline node_module* Environment::extra_linked_bindings_head() {
  return extra_linked_bindings_.size() > 0 ?
      &extra_linked_bindings_.front() : nullptr;
}

inline const Mutex& Environment::extra_linked_bindings_mutex() const {
  return extra_linked_bindings_mutex_;
}

inline performance::PerformanceState* Environment::performance_state() {
  return performance_state_.get();
}

inline std::unordered_map<std::string, uint64_t>*
    Environment::performance_marks() {
  return &performance_marks_;
}

inline IsolateData* Environment::isolate_data() const {
  return isolate_data_;
}

inline char* Environment::AllocateUnchecked(size_t size) {
  return static_cast<char*>(
      isolate_data()->allocator()->AllocateUninitialized(size));
}

inline char* Environment::Allocate(size_t size) {
  char* ret = AllocateUnchecked(size);
  CHECK_NE(ret, nullptr);
  return ret;
}

inline void Environment::Free(char* data, size_t size) {
  if (data != nullptr)
    isolate_data()->allocator()->Free(data, size);
}

inline AllocatedBuffer Environment::AllocateManaged(size_t size, bool checked) {
  char* data = checked ? Allocate(size) : AllocateUnchecked(size);
  if (data == nullptr) size = 0;
  return AllocatedBuffer(this, uv_buf_init(data, size));
}

inline AllocatedBuffer::AllocatedBuffer(Environment* env, uv_buf_t buf)
    : env_(env), buffer_(buf) {}

inline void AllocatedBuffer::Resize(size_t len) {
  // The `len` check is to make sure we don't end up with `nullptr` as our base.
  char* new_data = env_->Reallocate(buffer_.base, buffer_.len,
                                    len > 0 ? len : 1);
  CHECK_NOT_NULL(new_data);
  buffer_ = uv_buf_init(new_data, len);
}

inline uv_buf_t AllocatedBuffer::release() {
  uv_buf_t ret = buffer_;
  buffer_ = uv_buf_init(nullptr, 0);
  return ret;
}

inline char* AllocatedBuffer::data() {
  return buffer_.base;
}

inline const char* AllocatedBuffer::data() const {
  return buffer_.base;
}

inline size_t AllocatedBuffer::size() const {
  return buffer_.len;
}

inline AllocatedBuffer::AllocatedBuffer(Environment* env)
    : env_(env), buffer_(uv_buf_init(nullptr, 0)) {}

inline AllocatedBuffer::AllocatedBuffer(AllocatedBuffer&& other)
    : AllocatedBuffer() {
  *this = std::move(other);
}

inline AllocatedBuffer& AllocatedBuffer::operator=(AllocatedBuffer&& other) {
  clear();
  env_ = other.env_;
  buffer_ = other.release();
  return *this;
}

inline AllocatedBuffer::~AllocatedBuffer() {
  clear();
}

inline void AllocatedBuffer::clear() {
  uv_buf_t buf = release();
  if (buf.base != nullptr) {
    CHECK_NOT_NULL(env_);
    env_->Free(buf.base, buf.len);
  }
}

// It's a bit awkward to define this Buffer::New() overload here, but it
// avoids a circular dependency with node_internals.h.
namespace Buffer {
v8::MaybeLocal<v8::Object> New(Environment* env,
                               char* data,
                               size_t length,
                               bool uses_malloc);
}

inline v8::MaybeLocal<v8::Object> AllocatedBuffer::ToBuffer() {
  CHECK_NOT_NULL(env_);
  v8::MaybeLocal<v8::Object> obj = Buffer::New(env_, data(), size(), false);
  if (!obj.IsEmpty()) release();
  return obj;
}

inline v8::Local<v8::ArrayBuffer> AllocatedBuffer::ToArrayBuffer() {
  CHECK_NOT_NULL(env_);
  uv_buf_t buf = release();
  return v8::ArrayBuffer::New(env_->isolate(),
                              buf.base,
                              buf.len,
                              v8::ArrayBufferCreationMode::kInternalized);
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
                                     v8::Local<v8::Signature> signature,
                                     v8::ConstructorBehavior behavior,
                                     v8::SideEffectType side_effect_type) {
  v8::Local<v8::Object> external = as_callback_data();
  return v8::FunctionTemplate::New(isolate(), callback, external,
                                   signature, 0, behavior, side_effect_type);
}

inline void Environment::SetMethod(v8::Local<v8::Object> that,
                                   const char* name,
                                   v8::FunctionCallback callback) {
  v8::Local<v8::Context> context = isolate()->GetCurrentContext();
  v8::Local<v8::Function> function =
      NewFunctionTemplate(callback, v8::Local<v8::Signature>(),
                          v8::ConstructorBehavior::kThrow,
                          v8::SideEffectType::kHasSideEffect)
          ->GetFunction(context)
          .ToLocalChecked();
  // kInternalized strings are created in the old space.
  const v8::NewStringType type = v8::NewStringType::kInternalized;
  v8::Local<v8::String> name_string =
      v8::String::NewFromUtf8(isolate(), name, type).ToLocalChecked();
  that->Set(context, name_string, function).Check();
  function->SetName(name_string);  // NODE_SET_METHOD() compatibility.
}

inline void Environment::SetMethodNoSideEffect(v8::Local<v8::Object> that,
                                               const char* name,
                                               v8::FunctionCallback callback) {
  v8::Local<v8::Context> context = isolate()->GetCurrentContext();
  v8::Local<v8::Function> function =
      NewFunctionTemplate(callback, v8::Local<v8::Signature>(),
                          v8::ConstructorBehavior::kThrow,
                          v8::SideEffectType::kHasNoSideEffect)
          ->GetFunction(context)
          .ToLocalChecked();
  // kInternalized strings are created in the old space.
  const v8::NewStringType type = v8::NewStringType::kInternalized;
  v8::Local<v8::String> name_string =
      v8::String::NewFromUtf8(isolate(), name, type).ToLocalChecked();
  that->Set(context, name_string, function).Check();
  function->SetName(name_string);  // NODE_SET_METHOD() compatibility.
}

inline void Environment::SetProtoMethod(v8::Local<v8::FunctionTemplate> that,
                                        const char* name,
                                        v8::FunctionCallback callback) {
  v8::Local<v8::Signature> signature = v8::Signature::New(isolate(), that);
  v8::Local<v8::FunctionTemplate> t =
      NewFunctionTemplate(callback, signature, v8::ConstructorBehavior::kThrow,
                          v8::SideEffectType::kHasSideEffect);
  // kInternalized strings are created in the old space.
  const v8::NewStringType type = v8::NewStringType::kInternalized;
  v8::Local<v8::String> name_string =
      v8::String::NewFromUtf8(isolate(), name, type).ToLocalChecked();
  that->PrototypeTemplate()->Set(name_string, t);
  t->SetClassName(name_string);  // NODE_SET_PROTOTYPE_METHOD() compatibility.
}

inline void Environment::SetProtoMethodNoSideEffect(
    v8::Local<v8::FunctionTemplate> that,
    const char* name,
    v8::FunctionCallback callback) {
  v8::Local<v8::Signature> signature = v8::Signature::New(isolate(), that);
  v8::Local<v8::FunctionTemplate> t =
      NewFunctionTemplate(callback, signature, v8::ConstructorBehavior::kThrow,
                          v8::SideEffectType::kHasNoSideEffect);
  // kInternalized strings are created in the old space.
  const v8::NewStringType type = v8::NewStringType::kInternalized;
  v8::Local<v8::String> name_string =
      v8::String::NewFromUtf8(isolate(), name, type).ToLocalChecked();
  that->PrototypeTemplate()->Set(name_string, t);
  t->SetClassName(name_string);  // NODE_SET_PROTOTYPE_METHOD() compatibility.
}

inline void Environment::SetInstanceMethod(v8::Local<v8::FunctionTemplate> that,
                                           const char* name,
                                           v8::FunctionCallback callback) {
  v8::Local<v8::Signature> signature = v8::Signature::New(isolate(), that);
  v8::Local<v8::FunctionTemplate> t =
      NewFunctionTemplate(callback, signature, v8::ConstructorBehavior::kThrow,
                          v8::SideEffectType::kHasSideEffect);
  // kInternalized strings are created in the old space.
  const v8::NewStringType type = v8::NewStringType::kInternalized;
  v8::Local<v8::String> name_string =
      v8::String::NewFromUtf8(isolate(), name, type).ToLocalChecked();
  that->InstanceTemplate()->Set(name_string, t);
  t->SetClassName(name_string);
}

void Environment::AddCleanupHook(void (*fn)(void*), void* arg) {
  auto insertion_info = cleanup_hooks_.emplace(CleanupHookCallback {
    fn, arg, cleanup_hook_counter_++
  });
  // Make sure there was no existing element with these values.
  CHECK_EQ(insertion_info.second, true);
}

void Environment::RemoveCleanupHook(void (*fn)(void*), void* arg) {
  CleanupHookCallback search { fn, arg, 0 };
  cleanup_hooks_.erase(search);
}

inline void Environment::RegisterFinalizationGroupForCleanup(
    v8::Local<v8::FinalizationGroup> group) {
  cleanup_finalization_groups_.emplace_back(isolate(), group);
  uv_async_send(&task_queues_async_);
}

size_t CleanupHookCallback::Hash::operator()(
    const CleanupHookCallback& cb) const {
  return std::hash<void*>()(cb.arg_);
}

bool CleanupHookCallback::Equal::operator()(
    const CleanupHookCallback& a, const CleanupHookCallback& b) const {
  return a.fn_ == b.fn_ && a.arg_ == b.arg_;
}

BaseObject* CleanupHookCallback::GetBaseObject() const {
  if (fn_ == BaseObject::DeleteMe)
    return static_cast<BaseObject*>(arg_);
  else
    return nullptr;
}

template <typename T>
void Environment::ForEachBaseObject(T&& iterator) {
  for (const auto& hook : cleanup_hooks_) {
    BaseObject* obj = hook.GetBaseObject();
    if (obj != nullptr)
      iterator(obj);
  }
}

void Environment::modify_base_object_count(int64_t delta) {
  base_object_count_ += delta;
}

int64_t Environment::base_object_count() const {
  return base_object_count_;
}

#define VP(PropertyName, StringValue) V(v8::Private, PropertyName)
#define VY(PropertyName, StringValue) V(v8::Symbol, PropertyName)
#define VS(PropertyName, StringValue) V(v8::String, PropertyName)
#define V(TypeName, PropertyName)                                             \
  inline                                                                      \
  v8::Local<TypeName> IsolateData::PropertyName() const {                     \
    return PropertyName ## _ .Get(isolate_);                                  \
  }
  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(VP)
  PER_ISOLATE_SYMBOL_PROPERTIES(VY)
  PER_ISOLATE_STRING_PROPERTIES(VS)
#undef V
#undef VS
#undef VY
#undef VP

#define VP(PropertyName, StringValue) V(v8::Private, PropertyName)
#define VY(PropertyName, StringValue) V(v8::Symbol, PropertyName)
#define VS(PropertyName, StringValue) V(v8::String, PropertyName)
#define V(TypeName, PropertyName)                                             \
  inline v8::Local<TypeName> Environment::PropertyName() const {              \
    return isolate_data()->PropertyName();                                    \
  }
  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(VP)
  PER_ISOLATE_SYMBOL_PROPERTIES(VY)
  PER_ISOLATE_STRING_PROPERTIES(VS)
#undef V
#undef VS
#undef VY
#undef VP

#define V(PropertyName, TypeName)                                             \
  inline v8::Local<TypeName> Environment::PropertyName() const {              \
    return PersistentToLocal::Strong(PropertyName ## _);                      \
  }                                                                           \
  inline void Environment::set_ ## PropertyName(v8::Local<TypeName> value) {  \
    PropertyName ## _.Reset(isolate(), value);                                \
  }
  ENVIRONMENT_STRONG_PERSISTENT_TEMPLATES(V)
  ENVIRONMENT_STRONG_PERSISTENT_VALUES(V)
#undef V

  inline v8::Local<v8::Context> Environment::context() const {
    return PersistentToLocal::Strong(context_);
  }
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_ENV_INL_H_
