#ifndef SRC_TRACING_NODE_TRACE_WRITER_H_
#define SRC_TRACING_NODE_TRACE_WRITER_H_

#include <sstream>
#include <queue>

#include "libplatform/v8-tracing.h"
#include "tracing/agent.h"
#include "uv.h"

namespace node {
namespace tracing {

using v8::platform::tracing::TraceObject;
using v8::platform::tracing::TraceWriter;

class NodeTraceWriter : public AsyncTraceWriter {
 public:
  explicit NodeTraceWriter(const std::string& log_file_pattern);
  ~NodeTraceWriter();

  void InitializeOnThread(uv_loop_t* loop) override;
  void AppendTraceEvent(TraceObject* trace_event) override;
  void Flush(bool blocking) override;

  static const int kTracesPerFile = 1 << 19;

 private:
  struct WriteRequest {
    std::string str;
    int highest_request_id;
  };

  void AfterWrite();
  void StartWrite(uv_buf_t buf);
  void OpenNewFileForStreaming();
  void WriteToFile(std::string&& str, int highest_request_id);
  void WriteSuffix();
  void FlushPrivate();
  static void ExitSignalCb(uv_async_t* signal);

  uv_loop_t* tracing_loop_ = nullptr;
  // Triggers callback to initiate writing the contents of stream_ to disk.
  uv_async_t flush_signal_;
  // Triggers callback to close async objects, ending the tracing thread.
  uv_async_t exit_signal_;
  // Prevents concurrent R/W on state related to serialized trace data
  // before it's written to disk, namely stream_ and total_traces_
  // as well as json_trace_writer_.
  Mutex stream_mutex_;
  // Prevents concurrent R/W on state related to write requests.
  // If both mutexes are locked, request_mutex_ has to be locked first.
  Mutex request_mutex_;
  // Allows blocking calls to Flush() to wait on a condition for
  // trace events to be written to disk.
  ConditionVariable request_cond_;
  // Used to wait until async handles have been closed.
  ConditionVariable exit_cond_;
  int fd_ = -1;
  uv_fs_t write_req_;
  std::queue<WriteRequest> write_req_queue_;
  int num_write_requests_ = 0;
  int highest_request_id_completed_ = 0;
  int total_traces_ = 0;
  int file_num_ = 0;
  std::string log_file_pattern_;
  std::ostringstream stream_;
  std::unique_ptr<TraceWriter> json_trace_writer_;
  bool exited_ = false;
};

}  // namespace tracing
}  // namespace node

#endif  // SRC_TRACING_NODE_TRACE_WRITER_H_
