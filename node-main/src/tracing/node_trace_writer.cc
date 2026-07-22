#include "tracing/node_trace_writer.h"

#include "util-inl.h"

#include <fcntl.h>
#include <cstring>

namespace node {
namespace tracing {

NodeTraceWriter::NodeTraceWriter(const std::string& log_file_pattern)
    : log_file_pattern_(log_file_pattern) {}

void NodeTraceWriter::InitializeOnThread(uv_loop_t* loop) {
  CHECK_NULL(tracing_loop_);
  tracing_loop_ = loop;

  flush_signal_.data = this;
  int err = uv_async_init(tracing_loop_, &flush_signal_,
                          [](uv_async_t* signal) {
    NodeTraceWriter* trace_writer =
        ContainerOf(&NodeTraceWriter::flush_signal_, signal);
    trace_writer->FlushPrivate();
  });
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
      total_traces_ = kTracesPerFile;  // Act as if we reached the file limit.
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
  if (fd_ != -1) {
    CHECK_EQ(0, uv_fs_close(nullptr, &req, fd_, nullptr));
    uv_fs_req_cleanup(&req);
  }
  uv_async_send(&exit_signal_);
  Mutex::ScopedLock scoped_lock(request_mutex_);
  while (!exited_) {
    exit_cond_.Wait(scoped_lock);
  }
}

void replace_substring(std::string* target,
                       const std::string& search,
                       const std::string& insert) {
  size_t pos = target->find(search);
  for (; pos != std::string::npos; pos = target->find(search, pos)) {
    target->replace(pos, search.size(), insert);
    pos += insert.size();
  }
}

void NodeTraceWriter::OpenNewFileForStreaming() {
  ++file_num_;
  uv_fs_t req;

  // Evaluate a JS-style template string, it accepts the values ${pid} and
  // ${rotation}
  std::string filepath(log_file_pattern_);
  replace_substring(&filepath, "${pid}", std::to_string(uv_os_getpid()));
  replace_substring(&filepath, "${rotation}", std::to_string(file_num_));

  if (fd_ != -1) {
    CHECK_EQ(uv_fs_close(nullptr, &req, fd_, nullptr), 0);
    uv_fs_req_cleanup(&req);
  }

  fd_ = uv_fs_open(nullptr, &req, filepath.c_str(),
      O_CREAT | O_WRONLY | O_TRUNC, 0644, nullptr);
  uv_fs_req_cleanup(&req);
  if (fd_ < 0) {
    fprintf(stderr, "Could not open trace file %s: %s\n",
                    filepath.c_str(),
                    uv_strerror(fd_));
    fd_ = -1;
  }
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
    json_trace_writer_.reset(TraceWriter::CreateJSONTraceWriter(stream_));
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
      json_trace_writer_.reset();
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

void NodeTraceWriter::Flush(bool blocking) {
  Mutex::ScopedLock scoped_lock(request_mutex_);
  {
    // We need to lock the mutexes here in a nested fashion; stream_mutex_
    // protects json_trace_writer_, and without request_mutex_ there might be
    // a time window in which the stream state changes?
    Mutex::ScopedLock stream_mutex_lock(stream_mutex_);
    if (!json_trace_writer_)
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
  if (fd_ == -1) return;

  uv_buf_t buf = uv_buf_init(nullptr, 0);
  {
    Mutex::ScopedLock lock(request_mutex_);
    write_req_queue_.emplace(WriteRequest {
      std::move(str), highest_request_id
    });
    if (write_req_queue_.size() == 1) {
      buf = uv_buf_init(
          const_cast<char*>(write_req_queue_.front().str.c_str()),
          write_req_queue_.front().str.length());
    }
  }
  // Only one write request for the same file descriptor should be active at
  // a time.
  if (buf.base != nullptr && fd_ != -1) {
    StartWrite(buf);
  }
}

void NodeTraceWriter::StartWrite(uv_buf_t buf) {
  int err = uv_fs_write(
      tracing_loop_, &write_req_, fd_, &buf, 1, -1,
      [](uv_fs_t* req) {
        NodeTraceWriter* writer =
            ContainerOf(&NodeTraceWriter::write_req_, req);
        writer->AfterWrite();
      });
  CHECK_EQ(err, 0);
}

void NodeTraceWriter::AfterWrite() {
  CHECK_GE(write_req_.result, 0);
  uv_fs_req_cleanup(&write_req_);

  uv_buf_t buf = uv_buf_init(nullptr, 0);
  {
    Mutex::ScopedLock scoped_lock(request_mutex_);
    int highest_request_id = write_req_queue_.front().highest_request_id;
    write_req_queue_.pop();
    highest_request_id_completed_ = highest_request_id;
    request_cond_.Broadcast(scoped_lock);
    if (!write_req_queue_.empty()) {
      buf = uv_buf_init(
          const_cast<char*>(write_req_queue_.front().str.c_str()),
          write_req_queue_.front().str.length());
    }
  }
  if (buf.base != nullptr && fd_ != -1) {
    StartWrite(buf);
  }
}

// static
void NodeTraceWriter::ExitSignalCb(uv_async_t* signal) {
  NodeTraceWriter* trace_writer =
      ContainerOf(&NodeTraceWriter::exit_signal_, signal);
  // Close both flush_signal_ and exit_signal_.
  uv_close(reinterpret_cast<uv_handle_t*>(&trace_writer->flush_signal_),
           [](uv_handle_t* signal) {
             NodeTraceWriter* trace_writer =
                 ContainerOf(&NodeTraceWriter::flush_signal_,
                             reinterpret_cast<uv_async_t*>(signal));
             uv_close(
                 reinterpret_cast<uv_handle_t*>(&trace_writer->exit_signal_),
                 [](uv_handle_t* signal) {
                   NodeTraceWriter* trace_writer =
                       ContainerOf(&NodeTraceWriter::exit_signal_,
                                   reinterpret_cast<uv_async_t*>(signal));
                   Mutex::ScopedLock scoped_lock(trace_writer->request_mutex_);
                   trace_writer->exited_ = true;
                   trace_writer->exit_cond_.Signal(scoped_lock);
                 });
           });
}
}  // namespace tracing
}  // namespace node
