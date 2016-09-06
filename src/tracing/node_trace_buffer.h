#ifndef SRC_NODE_TRACE_BUFFER_H_
#define SRC_NODE_TRACE_BUFFER_H_

#include "node_mutex.h"
#include "tracing/node_trace_writer.h"
#include "libplatform/v8-tracing.h"

#include <atomic>

namespace node {
namespace tracing {

using v8::platform::tracing::TraceBuffer;
using v8::platform::tracing::TraceBufferChunk;
using v8::platform::tracing::TraceObject;

// forward declaration
class NodeTraceBuffer;

class InternalTraceBuffer {
 public:
  InternalTraceBuffer(size_t max_chunks, NodeTraceWriter* trace_writer,
                      NodeTraceBuffer* external_buffer);

  TraceObject* AddTraceEvent(uint64_t* handle);
  TraceObject* GetEventByHandle(uint64_t handle);
  void Flush(bool blocking);

  static const double kFlushThreshold;

 private:
  uint64_t MakeHandle(size_t chunk_index, uint32_t chunk_seq,
                      size_t event_index) const;
  void ExtractHandle(uint64_t handle, size_t* chunk_index,
                     uint32_t* chunk_seq, size_t* event_index) const;
  size_t Capacity() const { return max_chunks_ * TraceBufferChunk::kChunkSize; }

  Mutex mutex_;
  size_t max_chunks_;
  NodeTraceWriter* trace_writer_;
  NodeTraceBuffer* external_buffer_;
  std::vector<std::unique_ptr<TraceBufferChunk>> chunks_;
  size_t total_chunks_ = 0;
  uint32_t current_chunk_seq_ = 1;
};

class NodeTraceBuffer : public TraceBuffer {
 public:
  NodeTraceBuffer(size_t max_chunks, NodeTraceWriter* trace_writer,
                  uv_loop_t* tracing_loop);
  ~NodeTraceBuffer();

  TraceObject* AddTraceEvent(uint64_t* handle) override;
  TraceObject* GetEventByHandle(uint64_t handle) override;
  bool Flush() override;
  bool Flush(bool blocking);

  static const size_t kBufferChunks = 1024;

 private:
  void FlushPrivate(bool blocking);
  static void NonBlockingFlushSignalCb(uv_async_t* signal);
  static void ExitSignalCb(uv_async_t* signal);

  uv_loop_t* tracing_loop_;
  uv_async_t flush_signal_;
  uv_async_t exit_signal_;
  std::unique_ptr<NodeTraceWriter> trace_writer_;
  // TODO: Change std::atomic to something less contentious.
  std::atomic<InternalTraceBuffer*> current_buf_;
  InternalTraceBuffer buffer1_;
  InternalTraceBuffer buffer2_;
};

}  // namespace tracing
}  // namespace node

#endif  // SRC_NODE_TRACING_CONTROLLER_H_
