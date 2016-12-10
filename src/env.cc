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

namespace node {

using v8::Context;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Local;
using v8::Message;
using v8::StackFrame;
using v8::StackTrace;

void Environment::Start(int argc,
                        const char* const* argv,
                        int exec_argc,
                        const char* const* exec_argv,
                        bool start_profiler_idle_notifier) {
  HandleScope handle_scope(isolate());
  Context::Scope context_scope(context());

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

  uv_idle_init(event_loop(), destroy_ids_idle_handle());
  uv_unref(reinterpret_cast<uv_handle_t*>(destroy_ids_idle_handle()));

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

void Environment::PrintSyncTrace() const {
  if (!trace_sync_io_)
    return;

  HandleScope handle_scope(isolate());
  Local<v8::StackTrace> stack =
      StackTrace::CurrentStackTrace(isolate(), 10, StackTrace::kDetailed);

  fprintf(stderr, "(node:%d) WARNING: Detected use of sync API\n", getpid());

  for (int i = 0; i < stack->GetFrameCount() - 1; i++) {
    Local<StackFrame> stack_frame = stack->GetFrame(i);
    node::Utf8Value fn_name_s(isolate(), stack_frame->GetFunctionName());
    node::Utf8Value script_name(isolate(), stack_frame->GetScriptName());
    const int line_number = stack_frame->GetLineNumber();
    const int column = stack_frame->GetColumn();

    if (stack_frame->IsEval()) {
      if (stack_frame->GetScriptId() == Message::kNoScriptIdInfo) {
        fprintf(stderr, "    at [eval]:%i:%i\n", line_number, column);
      } else {
        fprintf(stderr,
                "    at [eval] (%s:%i:%i)\n",
                *script_name,
                line_number,
                column);
      }
      break;
    }

    if (fn_name_s.length() == 0) {
      fprintf(stderr, "    at %s:%i:%i\n", *script_name, line_number, column);
    } else {
      fprintf(stderr,
              "    at %s (%s:%i:%i)\n",
              *fn_name_s,
              *script_name,
              line_number,
              column);
    }
  }
  fflush(stderr);
}

}  // namespace node
