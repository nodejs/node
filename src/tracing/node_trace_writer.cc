#include "tracing/node_trace_writer.h"

#include <string.h>

#include "trace_event_common.h"
#include "util.h"

namespace node {
namespace tracing {

NodeTraceWriter::NodeTraceWriter(uv_loop_t* tracing_loop)
    : tracing_loop_(tracing_loop) {
  write_req_.writer = this;
}

void NodeTraceWriter::WriteSuffix() {
  // If our final log file has traces, then end the file appropriately.
  // This means that if no trace events are recorded, then no trace file is
  // produced.
  if (total_traces_ > 0) {
    WriteToFile("]}\n");
  }
}

NodeTraceWriter::~NodeTraceWriter() {
  WriteSuffix();
  uv_fs_t req;
  int err = uv_fs_close(tracing_loop_, &req, fd_, nullptr);
  CHECK_EQ(err, 0);
  uv_fs_req_cleanup(&req);
  uv_close(reinterpret_cast<uv_handle_t*>(&trace_file_pipe_), nullptr);
}

void NodeTraceWriter::OpenNewFileForStreaming() {
  ++file_num_;
  uv_fs_t req;
  std::string log_file = "node_trace.log." + std::to_string(file_num_);
  fd_ = uv_fs_open(tracing_loop_, &req, log_file.c_str(),
      O_CREAT | O_WRONLY | O_TRUNC, 0644, NULL);
  uv_fs_req_cleanup(&req);
  uv_pipe_init(tracing_loop_, &trace_file_pipe_, 0);
  uv_pipe_open(&trace_file_pipe_, fd_);
  // Note that the following does not get flushed to file immediately.
  stream_ << "{\"traceEvents\":[";
}

void NodeTraceWriter::AppendTraceEvent(TraceObject* trace_event) {
  // Set a writing_ flag here so that we only do a single Flush at any one
  // point in time. This is because we need the stream_ to be alive when we
  // are flushing.
  is_writing_ = true;
  // If this is the first trace event, open a new file for streaming.
  if (total_traces_ == 0) {
    OpenNewFileForStreaming();
  } else {
    stream_ << ",\n";
  }
  ++total_traces_;
  stream_ << "{\"pid\":" << trace_event->pid()
          << ",\"tid\":" << trace_event->tid()
          << ",\"ts\":" << trace_event->ts()
          << ",\"tts\":" << trace_event->tts() << ",\"ph\":\""
          << trace_event->phase() << "\",\"cat\":\""
          << TracingController::GetCategoryGroupName(
                 trace_event->category_enabled_flag())
          << "\",\"name\":\"" << trace_event->name()
          << "\",\"args\":{},\"dur\":" << trace_event->duration()
          << ",\"tdur\":" << trace_event->cpu_duration();
  if (trace_event->flags() & TRACE_EVENT_FLAG_HAS_ID) {
    if (trace_event->scope() != nullptr) {
      stream_ << ",\"scope\":\"" << trace_event->scope() << "\"";
    }
    stream_ << ",\"id\":" << trace_event->id();
  }
  stream_ << "}";
}

void NodeTraceWriter::Flush() {
  if (total_traces_ >= kTracesPerFile) {
    total_traces_ = 0;
    stream_ << "]}\n";
  }
  WriteToFile(stream_.str().c_str());
}

void NodeTraceWriter::WriteToFile(const char* str) {
  uv_buf_t uv_buf = uv_buf_init(const_cast<char*>(str), strlen(str));
  // We can reuse write_req_ here because we are assured that no other
  // uv_write using write_req_ is ongoing. In the case of synchronous writes
  // where the callback has not been fired, it is unclear whether it is OK
  // to reuse write_req_.
  uv_write(reinterpret_cast<uv_write_t*>(&write_req_),
           reinterpret_cast<uv_stream_t*>(&trace_file_pipe_),
           &uv_buf, 1, WriteCb);
}

void NodeTraceWriter::WriteCb(uv_write_t* req, int status) {
  NodeTraceWriter* writer = reinterpret_cast<WriteRequest*>(req)->writer;
  writer->stream_.str("");
  writer->is_writing_ = false;
}

}  // namespace tracing
}  // namespace node
