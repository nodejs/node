#include "inspector_profiler.h"
#include "base_object-inl.h"
#include "debug_utils-inl.h"
#include "diagnosticfilename-inl.h"
#include "memory_tracker-inl.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "node_file.h"
#include "node_internals.h"
#include "util-inl.h"
#include "v8-inspector.h"

#include <cinttypes>
#include <sstream>

namespace node {
namespace profiler {

using errors::TryCatchScope;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::HeapProfiler;
using v8::HeapStatistics;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::NewStringType;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Value;

using v8_inspector::StringView;

V8ProfilerConnection::V8ProfilerConnection(Environment* env)
    : session_(env->inspector_agent()->Connect(
          std::make_unique<V8ProfilerConnection::V8ProfilerSessionDelegate>(
              this),
          false)),
      env_(env) {}

uint32_t V8ProfilerConnection::DispatchMessage(const char* method,
                                               const char* params,
                                               bool is_profile_request) {
  std::stringstream ss;
  uint32_t id = next_id();
  ss << R"({ "id": )" << id;
  DCHECK(method != nullptr);
  ss << R"(, "method": ")" << method << '"';
  if (params != nullptr) {
    ss << R"(, "params": )" << params;
  }
  ss << " }";
  std::string message = ss.str();
  const uint8_t* message_data =
      reinterpret_cast<const uint8_t*>(message.c_str());
  // Save the id of the profile request to identify its response.
  if (is_profile_request) {
    profile_ids_.insert(id);
  }
  Debug(env(),
        DebugCategory::INSPECTOR_PROFILER,
        "Dispatching message %s\n",
        message.c_str());
  session_->Dispatch(StringView(message_data, message.length()));
  return id;
}

static void WriteResult(Environment* env,
                        const char* path,
                        Local<String> result) {
  int ret = WriteFileSync(env->isolate(), path, result);
  if (ret != 0) {
    char err_buf[128];
    uv_err_name_r(ret, err_buf, sizeof(err_buf));
    fprintf(stderr, "%s: Failed to write file %s\n", err_buf, path);
    return;
  }
  Debug(env, DebugCategory::INSPECTOR_PROFILER, "Written result to %s\n", path);
}

void V8ProfilerConnection::V8ProfilerSessionDelegate::SendMessageToFrontend(
    const v8_inspector::StringView& message) {
  Environment* env = connection_->env();
  Isolate* isolate = env->isolate();
  HandleScope handle_scope(isolate);
  Local<Context> context = env->context();
  Context::Scope context_scope(context);

  const char* type = connection_->type();
  // Convert StringView to a Local<String>.
  Local<String> message_str;
  if (!String::NewFromTwoByte(isolate,
                              message.characters16(),
                              NewStringType::kNormal,
                              message.length())
           .ToLocal(&message_str)) {
    fprintf(
        stderr, "Failed to convert %s profile message to V8 string\n", type);
    return;
  }

  Debug(env,
        DebugCategory::INSPECTOR_PROFILER,
        "Receive %s profile message\n",
        type);

  Local<Value> parsed;
  if (!v8::JSON::Parse(context, message_str).ToLocal(&parsed) ||
      !parsed->IsObject()) {
    fprintf(stderr, "Failed to parse %s profile result as JSON object\n", type);
    return;
  }

  Local<Object> response = parsed.As<Object>();
  Local<Value> id_v;
  if (!response->Get(context, FIXED_ONE_BYTE_STRING(isolate, "id"))
           .ToLocal(&id_v) ||
      !id_v->IsUint32()) {
    Utf8Value str(isolate, message_str);
    fprintf(
        stderr, "Cannot retrieve id from the response message:\n%s\n", *str);
    return;
  }
  uint32_t id = id_v.As<v8::Uint32>()->Value();

  if (!connection_->HasProfileId(id)) {
    Utf8Value str(isolate, message_str);
    Debug(env, DebugCategory::INSPECTOR_PROFILER, "%s\n", *str);
    return;
  } else {
    Debug(env,
          DebugCategory::INSPECTOR_PROFILER,
          "Writing profile response (id = %" PRIu64 ")\n",
          static_cast<uint64_t>(id));
  }

  // Get message.result from the response.
  Local<Value> result_v;
  if (!response->Get(context, FIXED_ONE_BYTE_STRING(isolate, "result"))
           .ToLocal(&result_v)) {
    fprintf(stderr, "Failed to get 'result' from %s profile response\n", type);
    return;
  }

  if (!result_v->IsObject()) {
    fprintf(
        stderr, "'result' from %s profile response is not an object\n", type);
    return;
  }

  connection_->WriteProfile(result_v.As<Object>());
  connection_->RemoveProfileId(id);
}

static bool EnsureDirectory(const std::string& directory, const char* type) {
  fs::FSReqWrapSync req_wrap_sync;
  int ret = fs::MKDirpSync(nullptr, &req_wrap_sync.req, directory, 0777,
                           nullptr);
  if (ret < 0 && ret != UV_EEXIST) {
    char err_buf[128];
    uv_err_name_r(ret, err_buf, sizeof(err_buf));
    fprintf(stderr,
            "%s: Failed to create %s profile directory %s\n",
            err_buf,
            type,
            directory.c_str());
    return false;
  }
  return true;
}

std::string V8CoverageConnection::GetFilename() const {
  uint64_t timestamp =
      static_cast<uint64_t>(GetCurrentTimeInMicroseconds() / 1000);
  return SPrintF("coverage-%s-%s-%s.json",
      uv_os_getpid(),
      timestamp,
      env()->thread_id());
}

void V8ProfilerConnection::WriteProfile(Local<Object> result) {
  Local<Context> context = env_->context();

  // Generate the profile output from the subclass.
  Local<Object> profile;
  if (!GetProfile(result).ToLocal(&profile)) {
    return;
  }

  Local<String> result_s;
  if (!v8::JSON::Stringify(context, profile).ToLocal(&result_s)) {
    fprintf(stderr, "Failed to stringify %s profile result\n", type());
    return;
  }

  // Create the directory if necessary.
  std::string directory = GetDirectory();
  DCHECK(!directory.empty());
  if (!EnsureDirectory(directory, type())) {
    return;
  }

  std::string filename = GetFilename();
  DCHECK(!filename.empty());
  std::string path = directory + kPathSeparator + filename;

  WriteResult(env_, path.c_str(), result_s);
}

void V8CoverageConnection::WriteProfile(Local<Object> result) {
  Isolate* isolate = env_->isolate();
  Local<Context> context = env_->context();
  HandleScope handle_scope(isolate);
  Context::Scope context_scope(context);

  // This is only set up during pre-execution (when the environment variables
  // becomes available in the JS land). If it's empty, we don't have coverage
  // directory path (which is resolved in JS land at the moment) either, so
  // the best we could to is to just discard the profile and do nothing.
  // This should only happen in half-baked Environments created using the
  // embedder API.
  if (env_->source_map_cache_getter().IsEmpty()) {
    return;
  }

  // Generate the profile output from the subclass.
  Local<Object> profile;
  if (!GetProfile(result).ToLocal(&profile)) {
    return;
  }

  // append source-map cache information to coverage object:
  Local<Value> source_map_cache_v;
  {
    TryCatchScope try_catch(env());
    {
      Isolate::AllowJavascriptExecutionScope allow_js_here(isolate);
      Local<Function> source_map_cache_getter = env_->source_map_cache_getter();
      if (!source_map_cache_getter->Call(
              context, Undefined(isolate), 0, nullptr)
              .ToLocal(&source_map_cache_v)) {
        return;
      }
    }
    if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
      PrintCaughtException(isolate, context, try_catch);
    }
  }
  // Avoid writing to disk if no source-map data:
  if (!source_map_cache_v->IsUndefined()) {
    profile->Set(context, FIXED_ONE_BYTE_STRING(isolate, "source-map-cache"),
                source_map_cache_v).ToChecked();
  }

  Local<String> result_s;
  if (!v8::JSON::Stringify(context, profile).ToLocal(&result_s)) {
    fprintf(stderr, "Failed to stringify %s profile result\n", type());
    return;
  }

  // Create the directory if necessary.
  std::string directory = GetDirectory();
  DCHECK(!directory.empty());
  if (!EnsureDirectory(directory, type())) {
    return;
  }

  std::string filename = GetFilename();
  DCHECK(!filename.empty());
  std::string path = directory + kPathSeparator + filename;

  WriteResult(env_, path.c_str(), result_s);
}

MaybeLocal<Object> V8CoverageConnection::GetProfile(Local<Object> result) {
  return result;
}

std::string V8CoverageConnection::GetDirectory() const {
  return env()->coverage_directory();
}

void V8CoverageConnection::Start() {
  DispatchMessage("Profiler.enable");
  DispatchMessage("Profiler.startPreciseCoverage",
                  R"({ "callCount": true, "detailed": true })");
}

void V8CoverageConnection::TakeCoverage() {
  DispatchMessage("Profiler.takePreciseCoverage", nullptr, true);
}

void V8CoverageConnection::StopCoverage() {
  DispatchMessage("Profiler.stopPreciseCoverage");
}

void V8CoverageConnection::End() {
  Debug(env_,
      DebugCategory::INSPECTOR_PROFILER,
      "V8CoverageConnection::End(), ending = %d\n", ending_);
  if (ending_) {
    return;
  }
  ending_ = true;
  TakeCoverage();
}

std::string V8CpuProfilerConnection::GetDirectory() const {
  return env()->cpu_prof_dir();
}

std::string V8CpuProfilerConnection::GetFilename() const {
  return env()->cpu_prof_name();
}

MaybeLocal<Object> V8CpuProfilerConnection::GetProfile(Local<Object> result) {
  Local<Value> profile_v;
  if (!result
           ->Get(env()->context(),
                 FIXED_ONE_BYTE_STRING(env()->isolate(), "profile"))
           .ToLocal(&profile_v)) {
    fprintf(stderr, "'profile' from CPU profile result is undefined\n");
    return MaybeLocal<Object>();
  }
  if (!profile_v->IsObject()) {
    fprintf(stderr, "'profile' from CPU profile result is not an Object\n");
    return MaybeLocal<Object>();
  }
  return profile_v.As<Object>();
}

void V8CpuProfilerConnection::Start() {
  DispatchMessage("Profiler.enable");
  std::string params = R"({ "interval": )";
  params += std::to_string(env()->cpu_prof_interval());
  params += " }";
  DispatchMessage("Profiler.setSamplingInterval", params.c_str());
  DispatchMessage("Profiler.start");
}

void V8CpuProfilerConnection::End() {
  Debug(env_,
      DebugCategory::INSPECTOR_PROFILER,
      "V8CpuProfilerConnection::End(), ending = %d\n", ending_);
  if (ending_) {
    return;
  }
  ending_ = true;
  DispatchMessage("Profiler.stop", nullptr, true);
}

std::string V8HeapProfilerConnection::GetDirectory() const {
  return env()->heap_prof_dir();
}

std::string V8HeapProfilerConnection::GetFilename() const {
  return env()->heap_prof_name();
}

MaybeLocal<Object> V8HeapProfilerConnection::GetProfile(Local<Object> result) {
  Local<Value> profile_v;
  if (!result
           ->Get(env()->context(),
                 FIXED_ONE_BYTE_STRING(env()->isolate(), "profile"))
           .ToLocal(&profile_v)) {
    fprintf(stderr, "'profile' from heap profile result is undefined\n");
    return MaybeLocal<Object>();
  }
  if (!profile_v->IsObject()) {
    fprintf(stderr, "'profile' from heap profile result is not an Object\n");
    return MaybeLocal<Object>();
  }
  return profile_v.As<Object>();
}

void V8HeapProfilerConnection::Start() {
  DispatchMessage("HeapProfiler.enable");
  std::string params = R"({ "samplingInterval": )";
  params += std::to_string(env()->heap_prof_interval());
  params += " }";
  DispatchMessage("HeapProfiler.startSampling", params.c_str());
}

void V8HeapProfilerConnection::End() {
  Debug(env_,
      DebugCategory::INSPECTOR_PROFILER,
      "V8HeapProfilerConnection::End(), ending = %d\n", ending_);
  if (ending_) {
    return;
  }
  ending_ = true;
  DispatchMessage("HeapProfiler.stopSampling", nullptr, true);
}

// For now, we only support coverage profiling, but we may add more
// in the future.
static void EndStartedProfilers(Environment* env) {
  // TODO(joyeechueng): merge these connections and use one session per env.
  Debug(env, DebugCategory::INSPECTOR_PROFILER, "EndStartedProfilers\n");
  V8ProfilerConnection* connection = env->cpu_profiler_connection();
  if (connection != nullptr) {
    connection->End();
  }

  connection = env->heap_profiler_connection();
  if (connection != nullptr) {
    connection->End();
  }

  connection = env->coverage_connection();
  if (connection != nullptr) {
    connection->End();
  }
}

void StartProfilers(Environment* env) {
  AtExit(env, [](void* env) {
    EndStartedProfilers(static_cast<Environment*>(env));
  }, env);

  Isolate* isolate = env->isolate();
  Local<String> coverage_str = env->env_vars()->Get(
      isolate, FIXED_ONE_BYTE_STRING(isolate, "NODE_V8_COVERAGE"))
      .FromMaybe(Local<String>());
  if (!coverage_str.IsEmpty() && coverage_str->Length() > 0) {
    CHECK_NULL(env->coverage_connection());
    env->set_coverage_connection(std::make_unique<V8CoverageConnection>(env));
    env->coverage_connection()->Start();
  }
  if (env->options()->cpu_prof) {
    const std::string& dir = env->options()->cpu_prof_dir;
    env->set_cpu_prof_interval(env->options()->cpu_prof_interval);
    env->set_cpu_prof_dir(dir.empty() ? env->GetCwd() : dir);
    if (env->options()->cpu_prof_name.empty()) {
      DiagnosticFilename filename(env, "CPU", "cpuprofile");
      env->set_cpu_prof_name(*filename);
    } else {
      env->set_cpu_prof_name(env->options()->cpu_prof_name);
    }
    CHECK_NULL(env->cpu_profiler_connection());
    env->set_cpu_profiler_connection(
        std::make_unique<V8CpuProfilerConnection>(env));
    env->cpu_profiler_connection()->Start();
  }
  if (env->options()->heap_prof) {
    const std::string& dir = env->options()->heap_prof_dir;
    env->set_heap_prof_interval(env->options()->heap_prof_interval);
    env->set_heap_prof_dir(dir.empty() ? env->GetCwd() : dir);
    if (env->options()->heap_prof_name.empty()) {
      DiagnosticFilename filename(env, "Heap", "heapprofile");
      env->set_heap_prof_name(*filename);
    } else {
      env->set_heap_prof_name(env->options()->heap_prof_name);
    }
    env->set_heap_profiler_connection(
        std::make_unique<profiler::V8HeapProfilerConnection>(env));
    env->heap_profiler_connection()->Start();
  }
}

static void SetCoverageDirectory(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsString());
  Environment* env = Environment::GetCurrent(args);
  node::Utf8Value directory(env->isolate(), args[0].As<String>());
  env->set_coverage_directory(*directory);
}


static void SetSourceMapCacheGetter(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsFunction());
  Environment* env = Environment::GetCurrent(args);
  env->set_source_map_cache_getter(args[0].As<Function>());
}

static void TakeCoverage(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  V8CoverageConnection* connection = env->coverage_connection();

  Debug(
    env,
    DebugCategory::INSPECTOR_PROFILER,
    "TakeCoverage, connection %s nullptr\n",
    connection == nullptr ? "==" : "!=");

  if (connection != nullptr) {
    Debug(env, DebugCategory::INSPECTOR_PROFILER, "taking coverage\n");
    connection->TakeCoverage();
  }
}

static void StopCoverage(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  V8CoverageConnection* connection = env->coverage_connection();

  Debug(env,
        DebugCategory::INSPECTOR_PROFILER,
        "StopCoverage, connection %s nullptr\n",
        connection == nullptr ? "==" : "!=");

  if (connection != nullptr) {
    Debug(env, DebugCategory::INSPECTOR_PROFILER, "Stopping coverage\n");
    connection->StopCoverage();
  }
}

void ProfileWatchdog::Start(Environment* env) {
  if (state() == ProfileWatchdogState::kInitialized) {
    int rc;
    rc = uv_loop_init(&loop_);
    if (rc != 0) {
      env->ThrowUVException(rc, "uv_loop_init", "Failed to call uv_loop_init");
      delete this;
      return;
    }
    rc = uv_async_init(&loop_, &async_, [](uv_async_t* task_async) {
      ProfileWatchdog* w = ContainerOf(&ProfileWatchdog::async_, task_async);
      w->HandleTasks();
    });
    if (rc != 0) {
      env->ThrowUVException(
          rc, "uv_async_init", "Failed to call uv_async_init");
      delete this;
      return;
    }
    rc = uv_timer_init(&loop_, &timer_);
    if (rc != 0) {
      uv_close(reinterpret_cast<uv_handle_t*>(&async_), nullptr);
      env->ThrowUVException(
          rc, "uv_timer_init", "Failed to call uv_timer_init");
      delete this;
      return;
    }
    rc = uv_timer_start(&timer_, &ProfileWatchdog::Timer, interval_, 0);
    if (rc != 0) {
      uv_close(reinterpret_cast<uv_handle_t*>(&async_), nullptr);
      env->ThrowUVException(
          rc, "uv_timer_start", "Failed to call uv_timer_start");
      delete this;
      return;
    }

    rc = uv_thread_create(&thread_, &ProfileWatchdog::Run, this);
    if (rc == 0) {
      set_state(ProfileWatchdogState::kRunning);
    } else {
      uv_close(reinterpret_cast<uv_handle_t*>(&timer_), nullptr);
      uv_close(reinterpret_cast<uv_handle_t*>(&async_), nullptr);
      env->ThrowUVException(
          rc, "uv_thread_create", "Failed to call uv_thread_create");
      delete this;
      return;
    }
  }
}

void ProfileWatchdog::Stop() {
  if (state() != ProfileWatchdogState::kClosing &&
      state() != ProfileWatchdogState::kClosed) {
    set_state(ProfileWatchdogState::kClosing);
  }
}

void ProfileWatchdog::Start(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  ProfileWatchdog* watchdog = Unwrap<ProfileWatchdog>(args.Holder());
  if (watchdog) {
    watchdog->Start(env);
  }
}

void ProfileWatchdog::Stop(const FunctionCallbackInfo<Value>& args) {
  ProfileWatchdog* watchdog = Unwrap<ProfileWatchdog>(args.Holder());
  if (watchdog) {
    watchdog->Stop();
  }
}

void ProfileWatchdog::Run(void* arg) {
  ProfileWatchdog* wd = static_cast<ProfileWatchdog*>(arg);
  uv_run(&wd->loop_, UV_RUN_DEFAULT);
  CheckedUvLoopClose(&wd->loop_);
}

void ProfileWatchdog::Timer(uv_timer_t* timer) {
  ProfileWatchdog* w = ContainerOf(&ProfileWatchdog::timer_, timer);
  Environment* env = w->env();
  if (!env || env->is_stopping()) {
    return;
  }
  if (w->state() == ProfileWatchdogState::kClosed) {
    return;
  }
  env->RequestInterrupt([watchdog = std::move(w)](Environment* env) {
    if (env->is_stopping() ||
        watchdog->state() == ProfileWatchdogState::kClosed) {
      return;
    }
    if (watchdog->state() == ProfileWatchdogState::kClosing) {
      delete watchdog;
      return;
    }
    if (watchdog->TimeoutHandler()) {
      watchdog->AddTask(
          [watchdog = std::move(watchdog)]() { watchdog->SetTimeout(); });
    }
  });
}

template <typename Fn>
void ProfileWatchdog::AddTask(Fn&& cb, CallbackFlags::Flags flags) {
  auto callback = tasks_.CreateCallback(std::move(cb), flags);
  {
    Mutex::ScopedLock lock(task_mutex_);
    tasks_.Push(std::move(callback));
  }
  uv_async_send(&async_);
}

void ProfileWatchdog::HandleTasks() {
  while (tasks_.size() > 0) {
    CallbackQueue<void> queue;
    {
      Mutex::ScopedLock lock(task_mutex_);
      queue.ConcatMove(std::move(tasks_));
    }
    while (auto head = queue.Shift()) head->Call();
  }
}

void ProfileWatchdog::SetTimeout() {
  uv_timer_start(&timer_, &ProfileWatchdog::Timer, interval_, 0);
}

Local<FunctionTemplate> ProfileWatchdog::GetConstructorTemplate(
    Environment* env) {
  Local<FunctionTemplate> tmpl = env->profile_watchdog_ctor_template();
  if (tmpl.IsEmpty()) {
    Isolate* isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, nullptr);
    tmpl->SetClassName(
        FIXED_ONE_BYTE_STRING(env->isolate(), "ProfileWatchdog"));
    SetProtoMethod(isolate, tmpl, "start", ProfileWatchdog::Start);
    SetProtoMethod(isolate, tmpl, "stop", ProfileWatchdog::Stop);
    env->set_profile_watchdog_ctor_template(tmpl);
  }
  return tmpl;
}

ProfileWatchdog::ProfileWatchdog(Environment* env, Local<Object> object)
    : BaseObject(env, object) {}

ProfileWatchdog::~ProfileWatchdog() {
  ProfileWatchdogState old_state = state();
  set_state(ProfileWatchdogState::kClosed);
  if (old_state != ProfileWatchdogState::kInitialized) {
    AddTask([self = std::move(this)]() {
      uv_close(reinterpret_cast<uv_handle_t*>(&self->timer_), nullptr);
      uv_close(reinterpret_cast<uv_handle_t*>(&self->async_), nullptr);
      uv_stop(&self->loop_);
    });
    CHECK_EQ(uv_thread_join(&thread_), 0);
  }
}

MemoryProfileWatchdog::MemoryProfileWatchdog(Environment* env,
                                             v8::Local<v8::Object> object,
                                             v8::Local<v8::Object> options)
    : ProfileWatchdog(env, object) {
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();

  Local<String> interval_key = FIXED_ONE_BYTE_STRING(isolate, "interval");
  Local<String> max_rss_key = FIXED_ONE_BYTE_STRING(isolate, "maxRss");
  Local<String> max_used_heap_size_key =
      FIXED_ONE_BYTE_STRING(isolate, "maxUsedHeapSize");
  Local<String> filename_key = FIXED_ONE_BYTE_STRING(isolate, "filename");

  Local<Value> interval = options->Get(context, interval_key).ToLocalChecked();
  interval_ = static_cast<uint64_t>(interval.As<Number>()->Value());

  if (options->Has(context, max_rss_key).FromMaybe(false)) {
    Local<Value> max_rss = options->Get(context, max_rss_key).ToLocalChecked();
    max_rss_ = static_cast<size_t>(max_rss.As<Number>()->Value());
  }

  if (options->Has(context, max_used_heap_size_key).FromMaybe(false)) {
    Local<Value> max_used_heap_size =
        options->Get(context, max_used_heap_size_key).ToLocalChecked();
    max_used_heap_size_ =
        static_cast<size_t>(max_used_heap_size.As<Number>()->Value());
  }

  Local<Value> filename = options->Get(context, filename_key).ToLocalChecked();
  node::Utf8Value filename_tmp(isolate, filename);
  filename_ = std::string(*filename_tmp);
}

bool MemoryProfileWatchdog::TimeoutHandler() {
  bool reached = false;
  if (max_rss_) {
    size_t rss = 0;
    uv_resident_set_memory(&rss);
    if (rss >= max_rss_) {
      reached = true;
    }
  }

  if (!reached && max_used_heap_size_) {
    Isolate* isolate = env()->isolate();
    HeapStatistics heap_statistics;
    isolate->GetHeapStatistics(&heap_statistics);
    if (heap_statistics.used_heap_size() >= max_used_heap_size_) {
      reached = true;
    }
  }

  if (reached) {
    HeapProfiler::HeapSnapshotOptions options;
    options.numerics_mode = HeapProfiler::NumericsMode::kExposeNumericValues;
    options.snapshot_mode = HeapProfiler::HeapSnapshotMode::kExposeInternals;
    heap::WriteSnapshot(env(), filename_.c_str(), options);
    delete this;
    return false;
  }
  return true;
}

void MemoryProfileWatchdog::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  CHECK(args[0]->IsObject());
  Environment* env = Environment::GetCurrent(args);
  new MemoryProfileWatchdog(env, args.This(), args[0].As<Object>());
}

void MemoryProfileWatchdog::Init(Environment* env, Local<Object> target) {
  Isolate* isolate = env->isolate();
  Local<FunctionTemplate> constructor =
      NewFunctionTemplate(isolate, MemoryProfileWatchdog::New);
  constructor->InstanceTemplate()->SetInternalFieldCount(
      BaseObject::kInternalFieldCount);
  constructor->Inherit(ProfileWatchdog::GetConstructorTemplate(env));
  SetConstructorFunction(
      env->context(), target, "MemoryProfileWatchdog", constructor);
}

static void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context,
                       void* priv) {
  SetMethod(context, target, "setCoverageDirectory", SetCoverageDirectory);
  SetMethod(
      context, target, "setSourceMapCacheGetter", SetSourceMapCacheGetter);
  SetMethod(context, target, "takeCoverage", TakeCoverage);
  SetMethod(context, target, "stopCoverage", StopCoverage);
  Environment* env = Environment::GetCurrent(context);
  MemoryProfileWatchdog::Init(env, target);
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(SetCoverageDirectory);
  registry->Register(SetSourceMapCacheGetter);
  registry->Register(TakeCoverage);
  registry->Register(StopCoverage);
  registry->Register(MemoryProfileWatchdog::New);
  registry->Register(MemoryProfileWatchdog::Start);
  registry->Register(MemoryProfileWatchdog::Stop);
}

}  // namespace profiler
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(profiler, node::profiler::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(profiler,
                                node::profiler::RegisterExternalReferences)
