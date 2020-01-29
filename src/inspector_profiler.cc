#include "inspector_profiler.h"
#include "base_object-inl.h"
#include "debug_utils-inl.h"
#include "diagnosticfilename-inl.h"
#include "memory_tracker-inl.h"
#include "node_file.h"
#include "node_errors.h"
#include "node_internals.h"
#include "util-inl.h"
#include "v8-inspector.h"

#include <sstream>

namespace node {
namespace profiler {

using errors::TryCatchScope;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::NewStringType;
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

size_t V8ProfilerConnection::DispatchMessage(const char* method,
                                             const char* params) {
  std::stringstream ss;
  size_t id = next_id();
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
  Debug(env(),
        DebugCategory::INSPECTOR_PROFILER,
        "Dispatching message %s\n",
        message.c_str());
  session_->Dispatch(StringView(message_data, message.length()));
  // TODO(joyeecheung): use this to identify the ending message.
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
  Context::Scope context_scope(env->context());

  // TODO(joyeecheung): always parse the message so that we can use the id to
  // identify ending messages as well as printing the message in the debug
  // output when there is an error.
  const char* type = connection_->type();
  Debug(env,
        DebugCategory::INSPECTOR_PROFILER,
        "Receive %s profile message, ending = %s\n",
        type,
        connection_->ending() ? "true" : "false");
  if (!connection_->ending()) {
    return;
  }

  // Convert StringView to a Local<String>.
  Local<String> message_str;
  if (!String::NewFromTwoByte(isolate,
                              message.characters16(),
                              NewStringType::kNormal,
                              message.length())
           .ToLocal(&message_str)) {
    fprintf(stderr, "Failed to convert %s profile message\n", type);
    return;
  }

  connection_->WriteProfile(message_str);
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
  std::string thread_id = std::to_string(env()->thread_id());
  std::string pid = std::to_string(uv_os_getpid());
  std::string timestamp = std::to_string(
      static_cast<uint64_t>(GetCurrentTimeInMicroseconds() / 1000));
  char filename[1024];
  snprintf(filename,
           sizeof(filename),
           "coverage-%s-%s-%s.json",
           pid.c_str(),
           timestamp.c_str(),
           thread_id.c_str());
  return filename;
}

static MaybeLocal<Object> ParseProfile(Environment* env,
                                       Local<String> message,
                                       const char* type) {
  Local<Context> context = env->context();
  Isolate* isolate = env->isolate();

  // Get message.result from the response
  Local<Value> parsed;
  if (!v8::JSON::Parse(context, message).ToLocal(&parsed) ||
      !parsed->IsObject()) {
    fprintf(stderr, "Failed to parse %s profile result as JSON object\n", type);
    return MaybeLocal<Object>();
  }

  Local<Value> result_v;
  if (!parsed.As<Object>()
           ->Get(context, FIXED_ONE_BYTE_STRING(isolate, "result"))
           .ToLocal(&result_v)) {
    fprintf(stderr, "Failed to get 'result' from %s profile message\n", type);
    return MaybeLocal<Object>();
  }

  if (!result_v->IsObject()) {
    fprintf(
        stderr, "'result' from %s profile message is not an object\n", type);
    return MaybeLocal<Object>();
  }

  return result_v.As<Object>();
}

void V8ProfilerConnection::WriteProfile(Local<String> message) {
  Local<Context> context = env_->context();

  // Get message.result from the response.
  Local<Object> result;
  if (!ParseProfile(env_, message, type()).ToLocal(&result)) {
    return;
  }
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

void V8CoverageConnection::WriteProfile(Local<String> message) {
  Isolate* isolate = env_->isolate();
  Local<Context> context = env_->context();
  HandleScope handle_scope(isolate);
  Context::Scope context_scope(context);

  // Get message.result from the response.
  Local<Object> result;
  if (!ParseProfile(env_, message, type()).ToLocal(&result)) {
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

void V8CoverageConnection::End() {
  CHECK_EQ(ending_, false);
  ending_ = true;
  DispatchMessage("Profiler.takePreciseCoverage");
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
  DispatchMessage("Profiler.start");
  std::string params = R"({ "interval": )";
  params += std::to_string(env()->cpu_prof_interval());
  params += " }";
  DispatchMessage("Profiler.setSamplingInterval", params.c_str());
}

void V8CpuProfilerConnection::End() {
  CHECK_EQ(ending_, false);
  ending_ = true;
  DispatchMessage("Profiler.stop");
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
  CHECK_EQ(ending_, false);
  ending_ = true;
  DispatchMessage("HeapProfiler.stopSampling");
}

// For now, we only support coverage profiling, but we may add more
// in the future.
static void EndStartedProfilers(Environment* env) {
  Debug(env, DebugCategory::INSPECTOR_PROFILER, "EndStartedProfilers\n");
  V8ProfilerConnection* connection = env->cpu_profiler_connection();
  if (connection != nullptr && !connection->ending()) {
    Debug(env, DebugCategory::INSPECTOR_PROFILER, "Ending cpu profiling\n");
    connection->End();
  }

  connection = env->heap_profiler_connection();
  if (connection != nullptr && !connection->ending()) {
    Debug(env, DebugCategory::INSPECTOR_PROFILER, "Ending heap profiling\n");
    connection->End();
  }

  connection = env->coverage_connection();
  if (connection != nullptr && !connection->ending()) {
    Debug(
        env, DebugCategory::INSPECTOR_PROFILER, "Ending coverage collection\n");
    connection->End();
  }
}

std::string GetCwd(Environment* env) {
  char cwd[PATH_MAX_BYTES];
  size_t size = PATH_MAX_BYTES;
  const int err = uv_cwd(cwd, &size);

  if (err == 0) {
    CHECK_GT(size, 0);
    return cwd;
  }

  // This can fail if the cwd is deleted. In that case, fall back to
  // exec_path.
  const std::string& exec_path = env->exec_path();
  return exec_path.substr(0, exec_path.find_last_of(kPathSeparator));
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
    env->set_cpu_prof_dir(dir.empty() ? GetCwd(env) : dir);
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
    env->set_heap_prof_dir(dir.empty() ? GetCwd(env) : dir);
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

static void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context,
                       void* priv) {
  Environment* env = Environment::GetCurrent(context);
  env->SetMethod(target, "setCoverageDirectory", SetCoverageDirectory);
  env->SetMethod(target, "setSourceMapCacheGetter", SetSourceMapCacheGetter);
}

}  // namespace profiler
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(profiler, node::profiler::Initialize)
