#ifndef SRC_NODE_TRACE_WRITER_H_
#define SRC_NODE_TRACE_WRITER_H_

#include <sstream>

#include "libplatform/v8-tracing.h"
#include "uv.h"

namespace node {
namespace tracing {

using v8::platform::tracing::TraceObject;
using v8::platform::tracing::TraceWriter;
using v8::platform::tracing::TracingController;

class NodeTraceWriter : public TraceWriter {
 public:
  NodeTraceWriter(uv_loop_t* tracing_loop);
  ~NodeTraceWriter();

  bool IsReady() { return !is_writing_; }
  void AppendTraceEvent(TraceObject* trace_event) override;
  void Flush() override;

  static const int kTracesPerFile = 1 << 20;

 private:
  struct WriteRequest {
    uv_write_t req;
    NodeTraceWriter* writer;
  };

  static void WriteCb(uv_write_t* req, int status);
  void OpenNewFileForStreaming();
  void WriteToFile(const char* str);
  void WriteSuffix();

  uv_loop_t* tracing_loop_;
  int fd_;
  int total_traces_ = 0;
  int file_num_ = 0;
  WriteRequest write_req_;
  uv_pipe_t trace_file_pipe_;
  bool is_writing_ = false;
  std::ostringstream stream_;
};

}  // namespace tracing
}  // namespace node

#endif  // SRC_NODE_TRACE_WRITER_H_