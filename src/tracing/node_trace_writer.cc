#include "tracing/node_trace_writer.h"

#include <string.h>

#include "trace_event_common.h"
#include "util.h"

namespace node {
namespace tracing {

NodeTraceWriter::NodeTraceWriter(uv_loop_t* tracing_loop)
    : tracing_loop_(tracing_loop) {
  flush_signal_.data = this;
  int err = uv_async_init(tracing_loop_, &flush_signal_, FlushSignalCb);
  CHECK_EQ(err, 0);

  exit_signal_.data = this;
  err = uv_async_init(tracing_loop_, &exit_signal_, ExitSignalCb);
  CHECK_EQ(err, 0);
}

void NodeTraceWriter::WriteSuffix() {
  // If our final log file has traces, then end the file appropriately.
  // This means that if no trace events are recorded, then no trace file is
  // produced.
  bool should_flush = false;
  {
    Mutex::ScopedLock scoped_lock(stream_mutex_);
    if (total_traces_ > 0) {
      total_traces_ = 0; // so we don't write it again in FlushPrivate
      stream_ << "]}\n";
      should_flush = true;
    }
  }
  if (should_flush) {
    Flush(true);
  }
}

NodeTraceWriter::~NodeTraceWriter() {
  WriteSuffix();
  uv_fs_t req;
  int err;
  if (fd_ != -1) {
    err = uv_fs_close(tracing_loop_, &req, fd_, nullptr);
    CHECK_EQ(err, 0);
    uv_fs_req_cleanup(&req);
    uv_close(reinterpret_cast<uv_handle_t*>(&trace_file_pipe_), nullptr);
  }
  uv_async_send(&exit_signal_);
}

void NodeTraceWriter::OpenNewFileForStreaming() {
  ++file_num_;
  uv_fs_t req;
  std::string log_file = "node_trace." + std::to_string(file_num_) + ".log";
  fd_ = uv_fs_open(tracing_loop_, &req, log_file.c_str(),
      O_CREAT | O_WRONLY | O_TRUNC, 0644, NULL);
  CHECK_NE(fd_, -1);
  uv_fs_req_cleanup(&req);
  uv_pipe_init(tracing_loop_, &trace_file_pipe_, 0);
  uv_pipe_open(&trace_file_pipe_, fd_);
}

void NodeTraceWriter::AppendTraceEvent(TraceObject* trace_event) {
  Mutex::ScopedLock scoped_lock(stream_mutex_);
  // If this is the first trace event, open a new file for streaming.
  if (total_traces_ == 0) {
    OpenNewFileForStreaming();
    stream_ << "{\"traceEvents\":[";
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

void NodeTraceWriter::FlushPrivate() {
  std::string str;
  int highest_request_id;
  bool should_write = false;
  {
    Mutex::ScopedLock stream_scoped_lock(stream_mutex_);
    if (total_traces_ >= kTracesPerFile) {
      total_traces_ = 0;
      stream_ << "]}\n";
    }
    // str() makes a copy of the contents of the stream.
    str = stream_.str();
    stream_.str("");
    stream_.clear();
    if (str.length() > 0 && fd_ != -1) {
      Mutex::ScopedLock request_scoped_lock(request_mutex_);
      highest_request_id = num_write_requests_++;
      should_write = true;
    }
  }
  if (should_write) {
    WriteToFile(str, highest_request_id);
  }
}

void NodeTraceWriter::FlushSignalCb(uv_async_t* signal) {
  NodeTraceWriter* trace_writer = static_cast<NodeTraceWriter*>(signal->data);
  trace_writer->FlushPrivate();
}

// TODO: Remove (is it necessary to change the API? Since because of WriteSuffix
// it no longer matters whether it's true or false)
void NodeTraceWriter::Flush() {
  Flush(true);
}

void NodeTraceWriter::Flush(bool blocking) {
  int err = uv_async_send(&flush_signal_);
  CHECK_EQ(err, 0);
  Mutex::ScopedLock scoped_lock(request_mutex_);
  int request_id = num_write_requests_++;
  if (blocking) {
    // Wait until data associated with this request id has been written to disk.
    // This guarantees that data from all earlier requests have also been
    // written.
    while (request_id > highest_request_id_completed_) {
      request_cond_.Wait(scoped_lock);
    }
  }
}

void NodeTraceWriter::WriteToFile(std::string str, int highest_request_id) {
  const char* c_str = str.c_str();
  uv_buf_t uv_buf = uv_buf_init(const_cast<char*>(c_str), strlen(c_str));
  WriteRequest* write_req = new WriteRequest();
  write_req->writer = this;
  write_req->highest_request_id = highest_request_id;
  request_mutex_.Lock();
  // Manage a queue of WriteRequest objects because the behavior of uv_write is
  // is undefined if the same WriteRequest object is used more than once
  // between WriteCb calls. In addition, this allows us to keep track of the id
  // of the latest write request that actually been completed.
  write_req_queue_.push(write_req);
  request_mutex_.Unlock();
  // TODO: Is the return value of back() guaranteed to always have the
  // same address?
  uv_write(reinterpret_cast<uv_write_t*>(write_req),
           reinterpret_cast<uv_stream_t*>(&trace_file_pipe_),
           &uv_buf, 1, WriteCb);
}

void NodeTraceWriter::WriteCb(uv_write_t* req, int status) {
  WriteRequest* write_req = reinterpret_cast<WriteRequest*>(req);
  NodeTraceWriter* writer = write_req->writer;
  int highest_request_id = write_req->highest_request_id;
  {
    Mutex::ScopedLock scoped_lock(writer->request_mutex_);
    CHECK_EQ(write_req, writer->write_req_queue_.front());
    writer->write_req_queue_.pop();
    writer->highest_request_id_completed_ = highest_request_id;
    writer->request_cond_.Broadcast(scoped_lock);
  }
  delete write_req;
}

// static
void NodeTraceWriter::ExitSignalCb(uv_async_t* signal) {
  NodeTraceWriter* trace_writer = static_cast<NodeTraceWriter*>(signal->data);
  uv_close(reinterpret_cast<uv_handle_t*>(&trace_writer->flush_signal_), nullptr);
  uv_close(reinterpret_cast<uv_handle_t*>(&trace_writer->exit_signal_), nullptr);
}

}  // namespace tracing
}  // namespace node
