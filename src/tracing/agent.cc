#include "tracing/agent.h"

#include "env-inl.h"
#include "libplatform/libplatform.h"
#include "tracing/trace_config_parser.h"

namespace node {
namespace tracing {

Agent::Agent(Environment* env) : parent_env_(env) {}

void Agent::Start(v8::Platform* platform, const char* trace_config_file) {
  auto env = parent_env_;
  platform_ = platform;

  int err = uv_loop_init(&child_loop_);
  CHECK_EQ(err, 0);

  NodeTraceWriter* trace_writer = new NodeTraceWriter(&child_loop_);
  TraceBuffer* trace_buffer = new NodeTraceBuffer(
      NodeTraceBuffer::kBufferChunks, trace_writer, this);

  flush_signal_.data = trace_buffer;
  err = uv_async_init(&child_loop_, &flush_signal_, FlushSignalCb);
  CHECK_EQ(err, 0);

  tracing_controller_ = new TracingController();

  TraceConfig* trace_config = new TraceConfig();
  if (trace_config_file) {
    std::ifstream fin(trace_config_file);
    std::string str((std::istreambuf_iterator<char>(fin)),
                    std::istreambuf_iterator<char>());
    TraceConfigParser::FillTraceConfig(env->isolate(), trace_config,
                                       str.c_str());
  } else {
    trace_config->AddIncludedCategory("v8");
    trace_config->AddIncludedCategory("node");
  }
  tracing_controller_->Initialize(trace_buffer);
  tracing_controller_->StartTracing(trace_config);
  v8::platform::SetTracingController(platform, tracing_controller_);

  err = uv_thread_create(&thread_, ThreadCb, this);
  CHECK_EQ(err, 0);
}

void Agent::Stop() {
  if (!IsStarted()) {
    return;
  }
  // Perform final Flush on TraceBuffer. We don't want the tracing controller
  // to flush the buffer again on destruction of the V8::Platform.

  // TODO: Note that the serialization of this Flush is done on the main thread
  // and passed to libuv file write on the child thread later.
  // We might want to perform the serialization on the child thread instead,
  // but the current TracingController does not allow override of StopTracing().
  // By allowing serialization to be performed on the main thread,
  // NodeTraceWriter::IsReady() is now accessed from multiple threads, and
  // there might be race conditions if the child thread is still processing
  // something at the moment.
  // This can also be resolved by ensuring the child thread is free before
  // stopping tracing.
  tracing_controller_->StopTracing();
  delete tracing_controller_;
  v8::platform::SetTracingController(platform_, nullptr);
  // TODO: We might need to cleanup loop properly upon exit.
  // 1. Close child_loop_.
  // 2. Close flush_signal_.
  // 3. Destroy thread_.
}

// static
void Agent::ThreadCb(void* agent) {
  static_cast<Agent*>(agent)->WorkerRun();
}

void Agent::WorkerRun() {
  uv_run(&child_loop_, UV_RUN_DEFAULT);
}

// static
void Agent::FlushSignalCb(uv_async_t* signal) {
  static_cast<TraceBuffer*>(signal->data)->Flush();
}

void Agent::SendFlushSignal() {
  int err = uv_async_send(&flush_signal_);
  CHECK_EQ(err, 0);
}

}  // namespace tracing
}  // namespace node
