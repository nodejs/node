#include "env.h"
#include "env-inl.h"
#include "async-wrap.h"
#include "v8.h"
#include "v8-profiler.h"

#if defined(_MSC_VER)
#define getpid GetCurrentProcessId
#else
#include <unistd.h>
#endif

#include <stdio.h>
#include <algorithm>

namespace node {

using v8::Context;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Local;
using v8::StackFrame;
using v8::StackTrace;

void Environment::Start(int argc,
                        const char* const* argv,
                        int exec_argc,
                        const char* const* exec_argv,
                        bool start_profiler_idle_notifier) {
  HandleScope handle_scope(isolate());
  Context::Scope context_scope(context());

  isolate()->SetAutorunMicrotasks(false);

  uv_check_init(event_loop(), immediate_check_handle());
  uv_unref(reinterpret_cast<uv_handle_t*>(immediate_check_handle()));

  uv_idle_init(event_loop(), immediate_idle_handle());

  // Inform V8's CPU profiler when we're idle.  The profiler is sampling-based
  // but not all samples are created equal; mark the wall clock time spent in
  // epoll_wait() and friends so profiling tools can filter it out.  The samples
  // still end up in v8.log but with state=IDLE rather than state=EXTERNAL.
  // TODO(bnoordhuis) Depends on a libuv implementation detail that we should
  // probably fortify in the API contract, namely that the last started prepare
  // or check watcher runs first.  It's not 100% foolproof; if an add-on starts
  // a prepare or check watcher after us, any samples attributed to its callback
  // will be recorded with state=IDLE.
  uv_prepare_init(event_loop(), &idle_prepare_handle_);
  uv_check_init(event_loop(), &idle_check_handle_);
  uv_unref(reinterpret_cast<uv_handle_t*>(&idle_prepare_handle_));
  uv_unref(reinterpret_cast<uv_handle_t*>(&idle_check_handle_));

  auto close_and_finish = [](Environment* env, uv_handle_t* handle, void* arg) {
    handle->data = env;

    uv_close(handle, [](uv_handle_t* handle) {
      static_cast<Environment*>(handle->data)->FinishHandleCleanup(handle);
    });
  };

  RegisterHandleCleanup(
      reinterpret_cast<uv_handle_t*>(immediate_check_handle()),
      close_and_finish,
      nullptr);
  RegisterHandleCleanup(
      reinterpret_cast<uv_handle_t*>(immediate_idle_handle()),
      close_and_finish,
      nullptr);
  RegisterHandleCleanup(
      reinterpret_cast<uv_handle_t*>(&idle_prepare_handle_),
      close_and_finish,
      nullptr);
  RegisterHandleCleanup(
      reinterpret_cast<uv_handle_t*>(&idle_check_handle_),
      close_and_finish,
      nullptr);

  if (start_profiler_idle_notifier) {
    StartProfilerIdleNotifier();
  }

  auto process_template = FunctionTemplate::New(isolate());
  process_template->SetClassName(FIXED_ONE_BYTE_STRING(isolate(), "process"));

  auto process_object =
      process_template->GetFunction()->NewInstance(context()).ToLocalChecked();
  set_process_object(process_object);

  SetupProcessObject(this, argc, argv, exec_argc, exec_argv);
  LoadAsyncWrapperInfo(this);
}

void Environment::StartProfilerIdleNotifier() {
  uv_prepare_start(&idle_prepare_handle_, [](uv_prepare_t* handle) {
    Environment* env = ContainerOf(&Environment::idle_prepare_handle_, handle);
    env->isolate()->GetCpuProfiler()->SetIdle(true);
  });

  uv_check_start(&idle_check_handle_, [](uv_check_t* handle) {
    Environment* env = ContainerOf(&Environment::idle_check_handle_, handle);
    env->isolate()->GetCpuProfiler()->SetIdle(false);
  });
}

void Environment::StopProfilerIdleNotifier() {
  uv_prepare_stop(&idle_prepare_handle_);
  uv_check_stop(&idle_check_handle_);
}

bool Environment::IsInternalScriptId(int script_id) const {
  auto ids = internal_script_ids_;
  return ids.end() != std::find(ids.begin(), ids.end(), script_id);
}

void Environment::PrintStackTrace(FILE* stream, const char* prefix,
                                  PrintStackTraceMode mode) const {
  HandleScope handle_scope(isolate());
  // kExposeFramesAcrossSecurityOrigins is currently implied but let's be
  // explicit so it won't break after a V8 upgrade.
  // If you include kColumnOffset here, you should ensure that the offset of
  // the first line for non-eval'd code compensates for the module wrapper.
  using O = StackTrace::StackTraceOptions;
  auto options = static_cast<O>(StackTrace::kExposeFramesAcrossSecurityOrigins |
                                StackTrace::kFunctionName |
                                StackTrace::kLineNumber |
                                StackTrace::kScriptId |
                                StackTrace::kScriptName);
  auto stack_trace = StackTrace::CurrentStackTrace(isolate(), 12, options);
  for (int i = 0, n = stack_trace->GetFrameCount(); i < n; i += 1) {
    Local<v8::StackFrame> frame = stack_trace->GetFrame(i);
    if (mode == kNoInternalScripts && IsInternalScriptId(frame->GetScriptId()))
      continue;
    node::Utf8Value function_name(isolate(), frame->GetFunctionName());
    node::Utf8Value script_name(isolate(), frame->GetScriptName());
    fprintf(stream, "%s   at %s (%s:%d)\n", prefix,
            **function_name ? *function_name : "<anonymous>",
            *script_name, frame->GetLineNumber());
  }
}

void Environment::PrintSyncTrace() const {
  if (!trace_sync_io_)
    return;
  char prefix[32];
  snprintf(prefix, sizeof(prefix), "(node:%d) ", getpid());
  fprintf(stderr, "%sWARNING: Detected use of sync API\n", prefix);
  PrintStackTrace(stderr, prefix);
  fflush(stderr);
}

}  // namespace node
