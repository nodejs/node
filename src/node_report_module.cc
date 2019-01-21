#include "env.h"
#include "node_errors.h"
#include "node_internals.h"
#include "node_options.h"
#include "node_report.h"
#include "util.h"

#include "env-inl.h"
#include "handle_wrap.h"
#include "node_buffer.h"
#include "stream_base-inl.h"
#include "stream_wrap.h"
#include "util-inl.h"

#include <v8.h>
#include <atomic>
#include <sstream>

namespace report {
using node::Environment;
using node::FIXED_ONE_BYTE_STRING;
using node::PerIsolateOptions;
using node::Utf8Value;
using v8::Array;
using v8::Boolean;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::V8;
using v8::Value;

// Internal/static function declarations
void OnUncaughtException(const FunctionCallbackInfo<Value>& info);
static void Initialize(Local<Object> exports,
                       Local<Value> unused,
                       Local<Context> context);

// External JavaScript API for triggering a report
void TriggerReport(const FunctionCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  Isolate* isolate = env->isolate();
  HandleScope scope(isolate);
  std::string filename;
  Local<String> stackstr;

  if (info.Length() == 1) {
    stackstr = info[0].As<String>();
  } else {
    filename = *String::Utf8Value(isolate, info[0]);
    stackstr = info[1].As<String>();
  }

  filename = TriggerNodeReport(
      isolate, env, "JavaScript API", __func__, filename, stackstr);
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

// Callbacks for triggering report on uncaught exception.
// Calls triggered from JS land.
void OnUncaughtException(const FunctionCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  Isolate* isolate = env->isolate();
  HandleScope scope(isolate);
  std::string filename;
  std::shared_ptr<PerIsolateOptions> options = env->isolate_data()->options();

  // Trigger report if requested
  if (options->report_uncaught_exception) {
    TriggerNodeReport(
        isolate, env, "exception", __func__, filename, info[0].As<String>());
  }
}

// Signal handler for report action, called from JS land (util.js)
void OnUserSignal(const FunctionCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  Isolate* isolate = env->isolate();
  CHECK(info[0]->IsString());
  Local<String> str = info[0].As<String>();
  String::Utf8Value value(isolate, str);
  std::string filename;
  TriggerNodeReport(
      isolate, env, *value, __func__, filename, info[0].As<String>());
}

// A method to sync up data elements in the JS land with its
// corresponding elements in the C++ world. Required because
// (i) the tunables are first intercepted through the CLI but
// later modified via APIs. (ii) the report generation events
// are controlled partly from C++ and partly from JS.
void SyncConfig(const FunctionCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  Local<Context> context = env->context();
  std::shared_ptr<PerIsolateOptions> options = env->isolate_data()->options();

  CHECK_EQ(info.Length(), 2);
  Local<Object> obj;
  if (!info[0]->ToObject(context).ToLocal(&obj)) return;
  bool sync = info[1].As<Boolean>()->Value();

  // Events array
  Local<String> eventskey = FIXED_ONE_BYTE_STRING(env->isolate(), "events");
  Local<Value> events_unchecked;
  if (!obj->Get(context, eventskey).ToLocal(&events_unchecked)) return;
  Local<Array> events;
  if (events_unchecked->IsUndefined() || events_unchecked->IsNull()) {
    events_unchecked = Array::New(env->isolate(), 0);
    if (obj->Set(context, eventskey, events_unchecked).IsNothing()) return;
  }
  events = events_unchecked.As<Array>();

  // Signal
  Local<String> signalkey = env->signal_string();
  Local<Value> signal_unchecked;
  if (!obj->Get(context, signalkey).ToLocal(&signal_unchecked)) return;
  Local<String> signal;
  if (signal_unchecked->IsUndefined() || signal_unchecked->IsNull())
    signal_unchecked = signalkey;
  signal = signal_unchecked.As<String>();

  Utf8Value signalstr(env->isolate(), signal);

  // Report file
  Local<String> filekey = FIXED_ONE_BYTE_STRING(env->isolate(), "filename");
  Local<Value> file_unchecked;
  if (!obj->Get(context, filekey).ToLocal(&file_unchecked)) return;
  Local<String> file;
  if (file_unchecked->IsUndefined() || file_unchecked->IsNull())
    file_unchecked = filekey;
  file = file_unchecked.As<String>();

  Utf8Value filestr(env->isolate(), file);

  // Report file path
  Local<String> pathkey = FIXED_ONE_BYTE_STRING(env->isolate(), "path");
  Local<Value> path_unchecked;
  if (!obj->Get(context, pathkey).ToLocal(&path_unchecked)) return;
  Local<String> path;
  if (path_unchecked->IsUndefined() || path_unchecked->IsNull())
    path_unchecked = pathkey;
  path = path_unchecked.As<String>();

  Utf8Value pathstr(env->isolate(), path);

  // Report verbosity
  Local<String> verbosekey = FIXED_ONE_BYTE_STRING(env->isolate(), "verbose");
  Local<Value> verbose_unchecked;
  if (!obj->Get(context, verbosekey).ToLocal(&verbose_unchecked)) return;
  Local<Boolean> verbose;
  if (verbose_unchecked->IsUndefined() || verbose_unchecked->IsNull())
    verbose_unchecked = Boolean::New(env->isolate(), "verbose");
  verbose = verbose_unchecked.As<Boolean>();

  bool verb = verbose->BooleanValue(env->isolate());

  if (sync) {
    static const std::string e = "exception";
    static const std::string s = "signal";
    static const std::string f = "fatalerror";
    for (uint32_t i = 0; i < events->Length(); i++) {
      Local<Value> v;
      if (!events->Get(context, i).ToLocal(&v)) return;
      Local<String> elem;
      if (!v->ToString(context).ToLocal(&elem)) return;
      String::Utf8Value buf(env->isolate(), elem);
      if (*buf == e) {
        options->report_uncaught_exception = true;
      } else if (*buf == s) {
        options->report_on_signal = true;
      } else if (*buf == f) {
        options->report_on_fatalerror = true;
      }
    }
    CHECK_NOT_NULL(*signalstr);
    options->report_signal = *signalstr;
    CHECK_NOT_NULL(*filestr);
    options->report_filename = *filestr;
    CHECK_NOT_NULL(*pathstr);
    options->report_directory = *pathstr;
    options->report_verbose = verb;
  } else {
    int i = 0;
    if (options->report_uncaught_exception &&
        events
            ->Set(context,
                  i++,
                  FIXED_ONE_BYTE_STRING(env->isolate(), "exception"))
            .IsNothing())
      return;
    if (options->report_on_signal &&
        events
            ->Set(context, i++, FIXED_ONE_BYTE_STRING(env->isolate(), "signal"))
            .IsNothing())
      return;
    if (options->report_on_fatalerror &&
        events
            ->Set(
                context, i, FIXED_ONE_BYTE_STRING(env->isolate(), "fatalerror"))
            .IsNothing())
      return;

    Local<Value> signal_value;
    Local<Value> file_value;
    Local<Value> path_value;
    std::string signal = options->report_signal;
    if (signal.empty()) signal = "SIGUSR2";
    if (!node::ToV8Value(context, signal).ToLocal(&signal_value))
      return;
    if (!obj->Set(context, signalkey, signal_value).FromJust()) return;

    if (!node::ToV8Value(context, options->report_filename)
             .ToLocal(&file_value))
      return;
    if (!obj->Set(context, filekey, file_value).FromJust()) return;

    if (!node::ToV8Value(context, options->report_directory)
             .ToLocal(&path_value))
      return;
    if (!obj->Set(context, pathkey, path_value).FromJust()) return;

    if (!obj->Set(context,
                  verbosekey,
                  Boolean::New(env->isolate(), options->report_verbose))
             .FromJust())
      return;
  }
}

static void Initialize(Local<Object> exports,
                       Local<Value> unused,
                       Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);
  std::shared_ptr<PerIsolateOptions> options = env->isolate_data()->options();
  env->SetMethod(exports, "triggerReport", TriggerReport);
  env->SetMethod(exports, "getReport", GetReport);
  env->SetMethod(exports, "onUnCaughtException", OnUncaughtException);
  env->SetMethod(exports, "onUserSignal", OnUserSignal);
  env->SetMethod(exports, "syncConfig", SyncConfig);

  // TODO(gireeshpunathil) if we are retaining this flag,
  // insert more verbose information at vital control flow
  // points. Right now, it is only this one.
  if (options->report_verbose) {
    std::cerr << "report: initialization complete, event flags:" << std::endl;
    std::cerr << "report_uncaught_exception: "
              << options->report_uncaught_exception << std::endl;
    std::cerr << "report_on_signal: " << options->report_on_signal << std::endl;
    std::cerr << "report_on_fatalerror: " << options->report_on_fatalerror
              << std::endl;
    std::cerr << "report_signal: " << options->report_signal << std::endl;
    std::cerr << "report_filename: " << options->report_filename << std::endl;
    std::cerr << "report_directory: " << options->report_directory << std::endl;
    std::cerr << "report_verbose: " << options->report_verbose << std::endl;
  }
}

}  // namespace report

NODE_MODULE_CONTEXT_AWARE_INTERNAL(report, report::Initialize)
