#include "inspector_profiler.h"
#include "base_object-inl.h"
#include "debug_utils.h"
#include "node_file.h"
#include "node_internals.h"
#include "v8-inspector.h"

namespace node {
namespace profiler {

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

using v8_inspector::StringBuffer;
using v8_inspector::StringView;

#ifdef __POSIX__
const char* const kPathSeparator = "/";
#else
const char* const kPathSeparator = "\\/";
#endif

std::unique_ptr<StringBuffer> ToProtocolString(Isolate* isolate,
                                               Local<Value> value) {
  TwoByteValue buffer(isolate, value);
  return StringBuffer::create(StringView(*buffer, buffer.length()));
}

V8ProfilerConnection::V8ProfilerConnection(Environment* env)
    : session_(env->inspector_agent()->Connect(
          std::make_unique<V8ProfilerConnection::V8ProfilerSessionDelegate>(
              this),
          false)),
      env_(env) {}

void V8ProfilerConnection::DispatchMessage(Local<String> message) {
  session_->Dispatch(ToProtocolString(env()->isolate(), message)->string());
}

bool V8ProfilerConnection::WriteResult(const char* path, Local<String> result) {
  int ret = WriteFileSync(env()->isolate(), path, result);
  if (ret != 0) {
    char err_buf[128];
    uv_err_name_r(ret, err_buf, sizeof(err_buf));
    fprintf(stderr, "%s: Failed to write file %s\n", err_buf, path);
    return false;
  }
  Debug(
      env(), DebugCategory::INSPECTOR_PROFILER, "Written result to %s\n", path);
  return true;
}

void V8CoverageConnection::OnMessage(const v8_inspector::StringView& message) {
  Debug(env(),
        DebugCategory::INSPECTOR_PROFILER,
        "Receive coverage message, ending = %s\n",
        ending_ ? "true" : "false");
  if (!ending_) {
    return;
  }
  Isolate* isolate = env()->isolate();
  Local<Context> context = env()->context();
  HandleScope handle_scope(isolate);
  Context::Scope context_scope(context);
  Local<String> result;
  if (!String::NewFromTwoByte(isolate,
                              message.characters16(),
                              NewStringType::kNormal,
                              message.length())
           .ToLocal(&result)) {
    fprintf(stderr, "Failed to covert coverage message\n");
  }
  WriteCoverage(result);
}

bool V8CoverageConnection::WriteCoverage(Local<String> message) {
  const std::string& directory = env()->coverage_directory();
  CHECK(!directory.empty());
  uv_fs_t req;
  int ret = fs::MKDirpSync(nullptr, &req, directory, 0777, nullptr);
  uv_fs_req_cleanup(&req);
  if (ret < 0 && ret != UV_EEXIST) {
    char err_buf[128];
    uv_err_name_r(ret, err_buf, sizeof(err_buf));
    fprintf(stderr,
            "%s: Failed to create coverage directory %s\n",
            err_buf,
            directory.c_str());
    return false;
  }

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
  std::string target = directory + kPathSeparator + filename;
  MaybeLocal<String> result = GetResult(message);
  if (result.IsEmpty()) {
    return false;
  }
  return WriteResult(target.c_str(), result.ToLocalChecked());
}

MaybeLocal<String> V8CoverageConnection::GetResult(Local<String> message) {
  Local<Context> context = env()->context();
  Isolate* isolate = env()->isolate();
  Local<Value> parsed;
  if (!v8::JSON::Parse(context, message).ToLocal(&parsed) ||
      !parsed->IsObject()) {
    fprintf(stderr, "Failed to parse coverage result as JSON object\n");
    return MaybeLocal<String>();
  }

  Local<Value> result_v;
  if (!parsed.As<Object>()
           ->Get(context, FIXED_ONE_BYTE_STRING(isolate, "result"))
           .ToLocal(&result_v)) {
    fprintf(stderr, "Failed to get result from coverage message\n");
    return MaybeLocal<String>();
  }

  if (result_v->IsUndefined()) {
    fprintf(stderr, "'result' from coverage message is undefined\n");
    return MaybeLocal<String>();
  }

  Local<String> result_s;
  if (!v8::JSON::Stringify(context, result_v).ToLocal(&result_s)) {
    fprintf(stderr, "Failed to stringify coverage result\n");
    return MaybeLocal<String>();
  }

  return result_s;
}

void V8CoverageConnection::Start() {
  Debug(env(),
        DebugCategory::INSPECTOR_PROFILER,
        "Sending Profiler.startPreciseCoverage\n");
  Isolate* isolate = env()->isolate();
  Local<String> enable = FIXED_ONE_BYTE_STRING(
      isolate, R"({"id": 1, "method": "Profiler.enable"})");
  Local<String> start = FIXED_ONE_BYTE_STRING(isolate, R"({
      "id": 2,
      "method": "Profiler.startPreciseCoverage",
      "params": { "callCount": true, "detailed": true }
  })");
  DispatchMessage(enable);
  DispatchMessage(start);
}

void V8CoverageConnection::End() {
  CHECK_EQ(ending_, false);
  ending_ = true;
  Debug(env(),
        DebugCategory::INSPECTOR_PROFILER,
        "Sending Profiler.takePreciseCoverage\n");
  Isolate* isolate = env()->isolate();
  HandleScope scope(isolate);
  Local<String> end = FIXED_ONE_BYTE_STRING(isolate, R"({
      "id": 3,
      "method": "Profiler.takePreciseCoverage"
  })");
  DispatchMessage(end);
}

// For now, we only support coverage profiling, but we may add more
// in the future.
void EndStartedProfilers(Environment* env) {
  Debug(env, DebugCategory::INSPECTOR_PROFILER, "EndStartedProfilers\n");
  V8ProfilerConnection* connection = env->coverage_connection();
  if (connection != nullptr && !connection->ending()) {
    Debug(
        env, DebugCategory::INSPECTOR_PROFILER, "Ending coverage collection\n");
    connection->End();
  }
}

void StartCoverageCollection(Environment* env) {
  CHECK_NULL(env->coverage_connection());
  env->set_coverage_connection(std::make_unique<V8CoverageConnection>(env));
  env->coverage_connection()->Start();
}

static void SetCoverageDirectory(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsString());
  Environment* env = Environment::GetCurrent(args);
  node::Utf8Value directory(env->isolate(), args[0].As<String>());
  env->set_coverage_directory(*directory);
}

static void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context,
                       void* priv) {
  Environment* env = Environment::GetCurrent(context);
  env->SetMethod(target, "setCoverageDirectory", SetCoverageDirectory);
}

}  // namespace profiler
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(profiler, node::profiler::Initialize)
