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
#include "callback_queue-inl.h"
#include "env.h"
#include "node.h"
#include "node_context_data.h"
#include "node_internals.h"
#include "node_perf_common.h"
#include "util-inl.h"
#include "uv.h"
#include "v8-fast-api-calls.h"
#include "v8.h"

#include <cstddef>
#include <cstdint>

#include <utility>

namespace node {

NoArrayBufferZeroFillScope::NoArrayBufferZeroFillScope(
    IsolateData* isolate_data)
    : node_allocator_(isolate_data->node_allocator()) {
  if (node_allocator_ != nullptr) node_allocator_->zero_fill_field()[0] = 0;
}

NoArrayBufferZeroFillScope::~NoArrayBufferZeroFillScope() {
  if (node_allocator_ != nullptr) node_allocator_->zero_fill_field()[0] = 1;
}

inline v8::Isolate* IsolateData::isolate() const {
  return isolate_;
}

inline uv_loop_t* IsolateData::event_loop() const {
  return event_loop_;
}

inline NodeArrayBufferAllocator* IsolateData::node_allocator() const {
  return node_allocator_;
}

inline MultiIsolatePlatform* IsolateData::platform() const {
  return platform_;
}

inline void IsolateData::set_worker_context(worker::Worker* context) {
  CHECK_NULL(worker_context_);  // Should be set only once.
  worker_context_ = context;
}

inline worker::Worker* IsolateData::worker_context() const {
  return worker_context_;
}

inline v8::Local<v8::String> IsolateData::async_wrap_provider(int index) const {
  return async_wrap_providers_[index].Get(isolate_);
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

v8::Local<v8::Array> AsyncHooks::js_execution_async_resources() {
  if (UNLIKELY(js_execution_async_resources_.IsEmpty())) {
    js_execution_async_resources_.Reset(
        env()->isolate(), v8::Array::New(env()->isolate()));
  }
  return PersistentToLocal::Strong(js_execution_async_resources_);
}

v8::Local<v8::Object> AsyncHooks::native_execution_async_resource(size_t i) {
  if (i >= native_execution_async_resources_.size()) return {};
  return native_execution_async_resources_[i];
}

inline void AsyncHooks::SetJSPromiseHooks(v8::Local<v8::Function> init,
                                          v8::Local<v8::Function> before,
                                          v8::Local<v8::Function> after,
                                          v8::Local<v8::Function> resolve) {
  js_promise_hooks_[0].Reset(env()->isolate(), init);
  js_promise_hooks_[1].Reset(env()->isolate(), before);
  js_promise_hooks_[2].Reset(env()->isolate(), after);
  js_promise_hooks_[3].Reset(env()->isolate(), resolve);
  for (auto it = contexts_.begin(); it != contexts_.end(); it++) {
    if (it->IsEmpty()) {
      contexts_.erase(it--);
      continue;
    }
    PersistentToLocal::Weak(env()->isolate(), *it)
        ->SetPromiseHooks(init, before, after, resolve);
  }
}

inline v8::Local<v8::String> AsyncHooks::provider_string(int idx) {
  return env()->isolate_data()->async_wrap_provider(idx);
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
                                           v8::Local<v8::Object> resource) {
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

#ifdef DEBUG
  for (uint32_t i = offset; i < native_execution_async_resources_.size(); i++)
    CHECK(native_execution_async_resources_[i].IsEmpty());
#endif

  // When this call comes from JS (as a way of increasing the stack size),
  // `resource` will be empty, because JS caches these values anyway.
  if (!resource.IsEmpty()) {
    native_execution_async_resources_.resize(offset + 1);
    // Caveat: This is a v8::Local<> assignment, we do not keep a v8::Global<>!
    native_execution_async_resources_[offset] = resource;
  }
}

// Remember to keep this code aligned with popAsyncContext() in JS.
inline bool AsyncHooks::pop_async_context(double async_id) {
  // In case of an exception then this may have already been reset, if the
  // stack was multiple MakeCallback()'s deep.
  if (UNLIKELY(fields_[kStackLength] == 0)) return false;

  // Ask for the async_id to be restored as a check that the stack
  // hasn't been corrupted.
  // Since async_hooks is experimental, do only perform the check
  // when async_hooks is enabled.
  if (UNLIKELY(fields_[kCheck] > 0 &&
               async_id_fields_[kExecutionAsyncId] != async_id)) {
    FailWithCorruptedAsyncStack(async_id);
  }

  uint32_t offset = fields_[kStackLength] - 1;
  async_id_fields_[kExecutionAsyncId] = async_ids_stack_[2 * offset];
  async_id_fields_[kTriggerAsyncId] = async_ids_stack_[2 * offset + 1];
  fields_[kStackLength] = offset;

  if (LIKELY(offset < native_execution_async_resources_.size() &&
             !native_execution_async_resources_[offset].IsEmpty())) {
#ifdef DEBUG
    for (uint32_t i = offset + 1;
         i < native_execution_async_resources_.size();
         i++) {
      CHECK(native_execution_async_resources_[i].IsEmpty());
    }
#endif
    native_execution_async_resources_.resize(offset);
    if (native_execution_async_resources_.size() <
            native_execution_async_resources_.capacity() / 2 &&
        native_execution_async_resources_.size() > 16) {
      native_execution_async_resources_.shrink_to_fit();
    }
  }

  if (UNLIKELY(js_execution_async_resources()->Length() > offset)) {
    v8::HandleScope handle_scope(env()->isolate());
    USE(js_execution_async_resources()->Set(
        env()->context(),
        env()->length_string(),
        v8::Integer::NewFromUnsigned(env()->isolate(), offset)));
  }

  return fields_[kStackLength] > 0;
}

void AsyncHooks::clear_async_id_stack() {
  v8::Isolate* isolate = env()->isolate();
  v8::HandleScope handle_scope(isolate);
  if (!js_execution_async_resources_.IsEmpty()) {
    USE(PersistentToLocal::Strong(js_execution_async_resources_)->Set(
        env()->context(),
        env()->length_string(),
        v8::Integer::NewFromUnsigned(isolate, 0)));
  }
  native_execution_async_resources_.clear();
  native_execution_async_resources_.shrink_to_fit();

  async_id_fields_[kExecutionAsyncId] = 0;
  async_id_fields_[kTriggerAsyncId] = 0;
  fields_[kStackLength] = 0;
}

inline void AsyncHooks::AddContext(v8::Local<v8::Context> ctx) {
  ctx->SetPromiseHooks(
    js_promise_hooks_[0].IsEmpty() ?
      v8::Local<v8::Function>() :
      PersistentToLocal::Strong(js_promise_hooks_[0]),
    js_promise_hooks_[1].IsEmpty() ?
      v8::Local<v8::Function>() :
      PersistentToLocal::Strong(js_promise_hooks_[1]),
    js_promise_hooks_[2].IsEmpty() ?
      v8::Local<v8::Function>() :
      PersistentToLocal::Strong(js_promise_hooks_[2]),
    js_promise_hooks_[3].IsEmpty() ?
      v8::Local<v8::Function>() :
      PersistentToLocal::Strong(js_promise_hooks_[3]));

  size_t id = contexts_.size();
  contexts_.resize(id + 1);
  contexts_[id].Reset(env()->isolate(), ctx);
  contexts_[id].SetWeak();
}

inline void AsyncHooks::RemoveContext(v8::Local<v8::Context> ctx) {
  v8::Isolate* isolate = env()->isolate();
  v8::HandleScope handle_scope(isolate);
  contexts_.erase(std::remove_if(contexts_.begin(),
                                 contexts_.end(),
                                 [&](auto&& el) { return el.IsEmpty(); }),
                  contexts_.end());
  for (auto it = contexts_.begin(); it != contexts_.end(); it++) {
    v8::Local<v8::Context> saved_context =
      PersistentToLocal::Weak(isolate, *it);
    if (saved_context == ctx) {
      it->Reset();
      contexts_.erase(it);
      break;
    }
  }
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
  // Used to retrieve bindings
  context->SetAlignedPointerInEmbedderData(
      ContextEmbedderIndex::kBindingListIndex, &(this->bindings_));

#if HAVE_INSPECTOR
  inspector_agent()->ContextCreated(context, info);
#endif  // HAVE_INSPECTOR

  this->async_hooks()->AddContext(context);
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
  return GetCurrent(info.GetIsolate()->GetCurrentContext());
}

template <typename T>
inline Environment* Environment::GetCurrent(
    const v8::PropertyCallbackInfo<T>& info) {
  return GetCurrent(info.GetIsolate()->GetCurrentContext());
}

template <typename T, typename U>
inline T* Environment::GetBindingData(const v8::PropertyCallbackInfo<U>& info) {
  return GetBindingData<T>(info.GetIsolate()->GetCurrentContext());
}

template <typename T>
inline T* Environment::GetBindingData(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  return GetBindingData<T>(info.GetIsolate()->GetCurrentContext());
}

template <typename T>
inline T* Environment::GetBindingData(v8::Local<v8::Context> context) {
  BindingDataStore* map = static_cast<BindingDataStore*>(
      context->GetAlignedPointerFromEmbedderData(
          ContextEmbedderIndex::kBindingListIndex));
  DCHECK_NOT_NULL(map);
  auto it = map->find(T::type_name);
  if (UNLIKELY(it == map->end())) return nullptr;
  T* result = static_cast<T*>(it->second.get());
  DCHECK_NOT_NULL(result);
  DCHECK_EQ(result->env(), GetCurrent(context));
  return result;
}

template <typename T>
inline T* Environment::AddBindingData(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> target) {
  DCHECK_EQ(GetCurrent(context), this);
  // This won't compile if T is not a BaseObject subclass.
  BaseObjectPtr<T> item = MakeDetachedBaseObject<T>(this, target);
  BindingDataStore* map = static_cast<BindingDataStore*>(
      context->GetAlignedPointerFromEmbedderData(
          ContextEmbedderIndex::kBindingListIndex));
  DCHECK_NOT_NULL(map);
  auto result = map->emplace(T::type_name, item);
  CHECK(result.second);
  DCHECK_EQ(GetBindingData<T>(context), item.get());
  return item.get();
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

inline void Environment::set_force_context_aware(bool value) {
  options_->force_context_aware = value;
}

inline bool Environment::force_context_aware() const {
  return options_->force_context_aware;
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

inline std::string Environment::GetCwd() {
  char cwd[PATH_MAX_BYTES];
  size_t size = PATH_MAX_BYTES;
  const int err = uv_cwd(cwd, &size);

  if (err == 0) {
    CHECK_GT(size, 0);
    return cwd;
  }

  // This can fail if the cwd is deleted. In that case, fall back to
  // exec_path.
  const std::string& exec_path = exec_path_;
  return exec_path.substr(0, exec_path.find_last_of(kPathSeparator));
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

template <typename Fn>
void Environment::SetImmediate(Fn&& cb, CallbackFlags::Flags flags) {
  auto callback = native_immediates_.CreateCallback(std::move(cb), flags);
  native_immediates_.Push(std::move(callback));

  if (flags & CallbackFlags::kRefed) {
    if (immediate_info()->ref_count() == 0)
      ToggleImmediateRef(true);
    immediate_info()->ref_count_inc(1);
  }
}

template <typename Fn>
void Environment::SetImmediateThreadsafe(Fn&& cb, CallbackFlags::Flags flags) {
  auto callback = native_immediates_threadsafe_.CreateCallback(
      std::move(cb), flags);
  {
    Mutex::ScopedLock lock(native_immediates_threadsafe_mutex_);
    native_immediates_threadsafe_.Push(std::move(callback));
    if (task_queues_async_initialized_)
      uv_async_send(&task_queues_async_);
  }
}

template <typename Fn>
void Environment::RequestInterrupt(Fn&& cb) {
  auto callback = native_immediates_interrupts_.CreateCallback(
      std::move(cb), CallbackFlags::kRefed);
  {
    Mutex::ScopedLock lock(native_immediates_threadsafe_mutex_);
    native_immediates_interrupts_.Push(std::move(callback));
    if (task_queues_async_initialized_)
      uv_async_send(&task_queues_async_);
  }
  RequestInterruptFromV8();
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

inline void Environment::DoneBootstrapping() {
  has_run_bootstrapping_code_ = true;
  // This adjusts the return value of base_object_created_after_bootstrap() so
  // that tests that check the count do not have to account for internally
  // created BaseObjects.
  base_object_created_by_bootstrap_ = base_object_count_;
}

inline bool Environment::has_serialized_options() const {
  return has_serialized_options_;
}

inline void Environment::set_has_serialized_options(bool value) {
  has_serialized_options_ = value;
}

inline bool Environment::is_main_thread() const {
  return worker_context() == nullptr;
}

inline bool Environment::no_native_addons() const {
  return (flags_ & EnvironmentFlags::kNoNativeAddons) ||
          !options_->allow_native_addons;
}

inline bool Environment::should_not_register_esm_loader() const {
  return flags_ & EnvironmentFlags::kNoRegisterESMLoader;
}

inline bool Environment::owns_process_state() const {
  return flags_ & EnvironmentFlags::kOwnsProcessState;
}

inline bool Environment::owns_inspector() const {
  return flags_ & EnvironmentFlags::kOwnsInspector;
}

inline bool Environment::should_create_inspector() const {
  return (flags_ & EnvironmentFlags::kNoCreateInspector) == 0;
}

inline bool Environment::tracks_unmanaged_fds() const {
  return flags_ & EnvironmentFlags::kTrackUnmanagedFds;
}

inline bool Environment::hide_console_windows() const {
  return flags_ & EnvironmentFlags::kHideConsoleWindows;
}

inline bool Environment::no_global_search_paths() const {
  return (flags_ & EnvironmentFlags::kNoGlobalSearchPaths) ||
         !options_->global_search_paths;
}

inline bool Environment::no_browser_globals() const {
  // configure --no-browser-globals
#ifdef NODE_NO_BROWSER_GLOBALS
  return true;
#else
  return flags_ & EnvironmentFlags::kNoBrowserGlobals;
#endif
}

bool Environment::filehandle_close_warning() const {
  return emit_filehandle_warning_;
}

void Environment::set_filehandle_close_warning(bool on) {
  emit_filehandle_warning_ = on;
}

void Environment::set_source_maps_enabled(bool on) {
  source_maps_enabled_ = on;
}

bool Environment::source_maps_enabled() const {
  return source_maps_enabled_;
}

inline uint64_t Environment::thread_id() const {
  return thread_id_;
}

inline worker::Worker* Environment::worker_context() const {
  return isolate_data()->worker_context();
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

inline node_module* Environment::extra_linked_bindings_tail() {
  return extra_linked_bindings_.size() > 0 ?
      &extra_linked_bindings_.back() : nullptr;
}

inline const Mutex& Environment::extra_linked_bindings_mutex() const {
  return extra_linked_bindings_mutex_;
}

inline performance::PerformanceState* Environment::performance_state() {
  return performance_state_.get();
}

inline IsolateData* Environment::isolate_data() const {
  return isolate_data_;
}

inline uv_buf_t Environment::allocate_managed_buffer(
    const size_t suggested_size) {
  NoArrayBufferZeroFillScope no_zero_fill_scope(isolate_data());
  std::unique_ptr<v8::BackingStore> bs =
      v8::ArrayBuffer::NewBackingStore(isolate(), suggested_size);
  uv_buf_t buf = uv_buf_init(static_cast<char*>(bs->Data()), bs->ByteLength());
  released_allocated_buffers_.emplace(buf.base, std::move(bs));
  return buf;
}

inline std::unique_ptr<v8::BackingStore> Environment::release_managed_buffer(
    const uv_buf_t& buf) {
  std::unique_ptr<v8::BackingStore> bs;
  if (buf.base != nullptr) {
    auto it = released_allocated_buffers_.find(buf.base);
    CHECK_NE(it, released_allocated_buffers_.end());
    bs = std::move(it->second);
    released_allocated_buffers_.erase(it);
  }
  return bs;
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

inline v8::Local<v8::FunctionTemplate> Environment::NewFunctionTemplate(
    v8::FunctionCallback callback,
    v8::Local<v8::Signature> signature,
    v8::ConstructorBehavior behavior,
    v8::SideEffectType side_effect_type,
    const v8::CFunction* c_function) {
  return v8::FunctionTemplate::New(isolate(),
                                   callback,
                                   v8::Local<v8::Value>(),
                                   signature,
                                   0,
                                   behavior,
                                   side_effect_type,
                                   c_function);
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

inline void Environment::SetFastMethod(v8::Local<v8::Object> that,
                                       const char* name,
                                       v8::FunctionCallback slow_callback,
                                       const v8::CFunction* c_function) {
  v8::Local<v8::Context> context = isolate()->GetCurrentContext();
  v8::Local<v8::Function> function =
      NewFunctionTemplate(slow_callback,
                          v8::Local<v8::Signature>(),
                          v8::ConstructorBehavior::kThrow,
                          v8::SideEffectType::kHasNoSideEffect,
                          c_function)
          ->GetFunction(context)
          .ToLocalChecked();
  const v8::NewStringType type = v8::NewStringType::kInternalized;
  v8::Local<v8::String> name_string =
      v8::String::NewFromUtf8(isolate(), name, type).ToLocalChecked();
  that->Set(context, name_string, function).Check();
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

inline void Environment::SetConstructorFunction(
    v8::Local<v8::Object> that,
    const char* name,
    v8::Local<v8::FunctionTemplate> tmpl,
    SetConstructorFunctionFlag flag) {
  SetConstructorFunction(that, OneByteString(isolate(), name), tmpl, flag);
}

inline void Environment::SetConstructorFunction(
    v8::Local<v8::Object> that,
    v8::Local<v8::String> name,
    v8::Local<v8::FunctionTemplate> tmpl,
    SetConstructorFunctionFlag flag) {
  if (LIKELY(flag == SetConstructorFunctionFlag::SET_CLASS_NAME))
    tmpl->SetClassName(name);
  that->Set(
      context(),
      name,
      tmpl->GetFunction(context()).ToLocalChecked()).Check();
}

void Environment::AddCleanupHook(CleanupCallback fn, void* arg) {
  auto insertion_info = cleanup_hooks_.emplace(CleanupHookCallback {
    fn, arg, cleanup_hook_counter_++
  });
  // Make sure there was no existing element with these values.
  CHECK_EQ(insertion_info.second, true);
}

void Environment::RemoveCleanupHook(CleanupCallback fn, void* arg) {
  CleanupHookCallback search { fn, arg, 0 };
  cleanup_hooks_.erase(search);
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

template <typename T>
void Environment::ForEachBindingData(T&& iterator) {
  BindingDataStore* map = static_cast<BindingDataStore*>(
      context()->GetAlignedPointerFromEmbedderData(
          ContextEmbedderIndex::kBindingListIndex));
  DCHECK_NOT_NULL(map);
  for (auto& it : *map) {
    iterator(it.first, it.second);
  }
}

void Environment::modify_base_object_count(int64_t delta) {
  base_object_count_ += delta;
}

int64_t Environment::base_object_created_after_bootstrap() const {
  return base_object_count_ - base_object_created_by_bootstrap_;
}

int64_t Environment::base_object_count() const {
  return base_object_count_;
}

void Environment::set_main_utf16(std::unique_ptr<v8::String::Value> str) {
  CHECK(!main_utf16_);
  main_utf16_ = std::move(str);
}

void Environment::set_process_exit_handler(
    std::function<void(Environment*, int)>&& handler) {
  process_exit_handler_ = std::move(handler);
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

v8::Local<v8::Context> Environment::context() const {
  return PersistentToLocal::Strong(context_);
}

}  // namespace node

// These two files depend on each other. Including base_object-inl.h after this
// file is the easiest way to avoid issues with that circular dependency.
#include "base_object-inl.h"

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_ENV_INL_H_
