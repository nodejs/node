#include "tracing/node_trace_writer.h"

#include <string.h>
#include <fcntl.h>

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
      total_traces_ = 0;  // so we don't write it again in FlushPrivate
      // Appends "]}" to stream_.
      delete json_trace_writer_;
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
  }
  uv_async_send(&exit_signal_);
  Mutex::ScopedLock scoped_lock(request_mutex_);
  while (!exited_) {
    exit_cond_.Wait(scoped_lock);
  }
}

void NodeTraceWriter::OpenNewFileForStreaming() {
  ++file_num_;
  uv_fs_t req;
  std::ostringstream log_file;
  log_file << "node_trace." << file_num_ << ".log";
  fd_ = uv_fs_open(tracing_loop_, &req, log_file.str().c_str(),
      O_CREAT | O_WRONLY | O_TRUNC, 0644, NULL);
  CHECK_NE(fd_, -1);
  uv_fs_req_cleanup(&req);
}

void NodeTraceWriter::AppendTraceEvent(TraceObject* trace_event) {
  Mutex::ScopedLock scoped_lock(stream_mutex_);
  // If this is the first trace event, open a new file for streaming.
  if (total_traces_ == 0) {
    OpenNewFileForStreaming();
    // Constructing a new JSONTraceWriter object appends "{\"traceEvents\":["
    // to stream_.
    // In other words, the constructor initializes the serialization stream
    // to a state where we can start writing trace events to it.
    // Repeatedly constructing and destroying json_trace_writer_ allows
    // us to use V8's JSON writer instead of implementing our own.
    json_trace_writer_ = TraceWriter::CreateJSONTraceWriter(stream_);
  }
  ++total_traces_;
  json_trace_writer_->AppendTraceEvent(trace_event);
}

void NodeTraceWriter::FlushPrivate() {
  std::string str;
  int highest_request_id;
  {
    Mutex::ScopedLock stream_scoped_lock(stream_mutex_);
    if (total_traces_ >= kTracesPerFile) {
      total_traces_ = 0;
      // Destroying the member JSONTraceWriter object appends "]}" to
      // stream_ - in other words, ending a JSON file.
      delete json_trace_writer_;
    }
    // str() makes a copy of the contents of the stream.
    str = stream_.str();
    stream_.str("");
    stream_.clear();
  }
  {
    Mutex::ScopedLock request_scoped_lock(request_mutex_);
    highest_request_id = num_write_requests_;
  }
  WriteToFile(std::move(str), highest_request_id);
}

void NodeTraceWriter::FlushSignalCb(uv_async_t* signal) {
  NodeTraceWriter* trace_writer = static_cast<NodeTraceWriter*>(signal->data);
  trace_writer->FlushPrivate();
}

// TODO(matthewloring): Remove (is it necessary to change the API?
// Since because of WriteSuffix it no longer matters whether it's true or false)
void NodeTraceWriter::Flush() {
  Flush(true);
}

void NodeTraceWriter::Flush(bool blocking) {
  Mutex::ScopedLock scoped_lock(request_mutex_);
  if (!json_trace_writer_) {
    return;
  }
  int request_id = ++num_write_requests_;
  int err = uv_async_send(&flush_signal_);
  CHECK_EQ(err, 0);
  if (blocking) {
    // Wait until data associated with this request id has been written to disk.
    // This guarantees that data from all earlier requests have also been
    // written.
    while (request_id > highest_request_id_completed_) {
      request_cond_.Wait(scoped_lock);
    }
  }
}

void NodeTraceWriter::WriteToFile(std::string&& str, int highest_request_id) {
  WriteRequest* write_req = new WriteRequest();
  write_req->str = std::move(str);
  write_req->writer = this;
  write_req->highest_request_id = highest_request_id;
  uv_buf_t uv_buf = uv_buf_init(const_cast<char*>(write_req->str.c_str()),
      write_req->str.length());
  request_mutex_.Lock();
  // Manage a queue of WriteRequest objects because the behavior of uv_write is
  // is undefined if the same WriteRequest object is used more than once
  // between WriteCb calls. In addition, this allows us to keep track of the id
  // of the latest write request that actually been completed.
  write_req_queue_.push(write_req);
  request_mutex_.Unlock();
  int err = uv_fs_write(tracing_loop_, reinterpret_cast<uv_fs_t*>(write_req),
      fd_, &uv_buf, 1, -1, WriteCb);
  CHECK_EQ(err, 0);
}

void NodeTraceWriter::WriteCb(uv_fs_t* req) {
  WriteRequest* write_req = reinterpret_cast<WriteRequest*>(req);
  CHECK_GE(write_req->req.result, 0);

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
  uv_close(reinterpret_cast<uv_handle_t*>(&trace_writer->flush_signal_),
           nullptr);
  uv_close(reinterpret_cast<uv_handle_t*>(&trace_writer->exit_signal_),
           [](uv_handle_t* signal) {
      NodeTraceWriter* trace_writer =
          static_cast<NodeTraceWriter*>(signal->data);
      Mutex::ScopedLock scoped_lock(trace_writer->request_mutex_);
      trace_writer->exited_ = true;
      trace_writer->exit_cond_.Signal(scoped_lock);
  });
}

}  // namespace tracing
}  // namespace node
