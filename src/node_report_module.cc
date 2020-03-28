#include "env.h"
#include "node_errors.h"
#include "node_internals.h"
#include "node_options.h"
#include "node_report.h"
#include "util-inl.h"

#include "handle_wrap.h"
#include "node_buffer.h"
#include "stream_base-inl.h"
#include "stream_wrap.h"

#include <v8.h>
#include <atomic>
#include <sstream>

namespace report {
using node::Environment;
using node::Mutex;
using node::Utf8Value;
using v8::Boolean;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

void WriteReport(const FunctionCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  Isolate* isolate = env->isolate();
  HandleScope scope(isolate);
  std::string filename;
  Local<String> stackstr;

  CHECK_EQ(info.Length(), 4);
  String::Utf8Value message(isolate, info[0].As<String>());
  String::Utf8Value trigger(isolate, info[1].As<String>());
  stackstr = info[3].As<String>();

  if (info[2]->IsString())
    filename = *String::Utf8Value(isolate, info[2]);

  filename = TriggerNodeReport(
      isolate, env, *message, *trigger, filename, stackstr);
  // Return value is the report filename
  info.GetReturnValue().Set(
      String::NewFromUtf8(isolate, filename.c_str(), v8::NewStringType::kNormal)
          .ToLocalChecked());
}

// External JavaScript API for returning a report
void GetReport(const FunctionCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  Isolate* isolate = env->isolate();
  HandleScope scope(isolate);
  std::ostringstream out;

  GetNodeReport(
      isolate, env, "JavaScript API", __func__, info[0].As<String>(), out);

  // Return value is the contents of a report as a string.
  info.GetReturnValue().Set(String::NewFromUtf8(isolate,
                                                out.str().c_str(),
                                                v8::NewStringType::kNormal)
                                .ToLocalChecked());
}

static void GetCompact(const FunctionCallbackInfo<Value>& info) {
  node::Mutex::ScopedLock lock(node::per_process::cli_options_mutex);
  info.GetReturnValue().Set(node::per_process::cli_options->report_compact);
}

static void SetCompact(const FunctionCallbackInfo<Value>& info) {
  node::Mutex::ScopedLock lock(node::per_process::cli_options_mutex);
  Environment* env = Environment::GetCurrent(info);
  Isolate* isolate = env->isolate();
  bool compact = info[0]->ToBoolean(isolate)->Value();
  node::per_process::cli_options->report_compact = compact;
}

static void GetDirectory(const FunctionCallbackInfo<Value>& info) {
  node::Mutex::ScopedLock lock(node::per_process::cli_options_mutex);
  Environment* env = Environment::GetCurrent(info);
  std::string directory = node::per_process::cli_options->report_directory;
  auto result = String::NewFromUtf8(env->isolate(),
                                    directory.c_str(),
                                    v8::NewStringType::kNormal);
  info.GetReturnValue().Set(result.ToLocalChecked());
}

static void SetDirectory(const FunctionCallbackInfo<Value>& info) {
  node::Mutex::ScopedLock lock(node::per_process::cli_options_mutex);
  Environment* env = Environment::GetCurrent(info);
  CHECK(info[0]->IsString());
  Utf8Value dir(env->isolate(), info[0].As<String>());
  node::per_process::cli_options->report_directory = *dir;
}

static void GetFilename(const FunctionCallbackInfo<Value>& info) {
  node::Mutex::ScopedLock lock(node::per_process::cli_options_mutex);
  Environment* env = Environment::GetCurrent(info);
  std::string filename = node::per_process::cli_options->report_filename;
  auto result = String::NewFromUtf8(env->isolate(),
                                    filename.c_str(),
                                    v8::NewStringType::kNormal);
  info.GetReturnValue().Set(result.ToLocalChecked());
}

static void SetFilename(const FunctionCallbackInfo<Value>& info) {
  node::Mutex::ScopedLock lock(node::per_process::cli_options_mutex);
  Environment* env = Environment::GetCurrent(info);
  CHECK(info[0]->IsString());
  Utf8Value name(env->isolate(), info[0].As<String>());
  node::per_process::cli_options->report_filename = *name;
}

static void GetSignal(const FunctionCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  std::string signal = env->isolate_data()->options()->report_signal;
  auto result = String::NewFromUtf8(env->isolate(),
                                    signal.c_str(),
                                    v8::NewStringType::kNormal);
  info.GetReturnValue().Set(result.ToLocalChecked());
}

static void SetSignal(const FunctionCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  CHECK(info[0]->IsString());
  Utf8Value signal(env->isolate(), info[0].As<String>());
  env->isolate_data()->options()->report_signal = *signal;
}

static void ShouldReportOnFatalError(const FunctionCallbackInfo<Value>& info) {
  Mutex::ScopedLock lock(node::per_process::cli_options_mutex);
  info.GetReturnValue().Set(
      node::per_process::cli_options->report_on_fatalerror);
}

static void SetReportOnFatalError(const FunctionCallbackInfo<Value>& info) {
  CHECK(info[0]->IsBoolean());
  Mutex::ScopedLock lock(node::per_process::cli_options_mutex);
  node::per_process::cli_options->report_on_fatalerror = info[0]->IsTrue();
}

static void ShouldReportOnSignal(const FunctionCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  info.GetReturnValue().Set(env->isolate_data()->options()->report_on_signal);
}

static void SetReportOnSignal(const FunctionCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  CHECK(info[0]->IsBoolean());
  env->isolate_data()->options()->report_on_signal = info[0]->IsTrue();
}

static void ShouldReportOnUncaughtException(
    const FunctionCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  info.GetReturnValue().Set(
      env->isolate_data()->options()->report_uncaught_exception);
}

static void SetReportOnUncaughtException(
    const FunctionCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  CHECK(info[0]->IsBoolean());
  env->isolate_data()->options()->report_uncaught_exception = info[0]->IsTrue();
}

static void Initialize(Local<Object> exports,
                       Local<Value> unused,
                       Local<Context> context,
                       void* priv) {
  Environment* env = Environment::GetCurrent(context);

  env->SetMethod(exports, "writeReport", WriteReport);
  env->SetMethod(exports, "getReport", GetReport);
  env->SetMethod(exports, "getCompact", GetCompact);
  env->SetMethod(exports, "setCompact", SetCompact);
  env->SetMethod(exports, "getDirectory", GetDirectory);
  env->SetMethod(exports, "setDirectory", SetDirectory);
  env->SetMethod(exports, "getFilename", GetFilename);
  env->SetMethod(exports, "setFilename", SetFilename);
  env->SetMethod(exports, "getSignal", GetSignal);
  env->SetMethod(exports, "setSignal", SetSignal);
  env->SetMethod(exports, "shouldReportOnFatalError", ShouldReportOnFatalError);
  env->SetMethod(exports, "setReportOnFatalError", SetReportOnFatalError);
  env->SetMethod(exports, "shouldReportOnSignal", ShouldReportOnSignal);
  env->SetMethod(exports, "setReportOnSignal", SetReportOnSignal);
  env->SetMethod(exports, "shouldReportOnUncaughtException",
                 ShouldReportOnUncaughtException);
  env->SetMethod(exports, "setReportOnUncaughtException",
                 SetReportOnUncaughtException);
}

}  // namespace report

NODE_MODULE_CONTEXT_AWARE_INTERNAL(report, report::Initialize)
