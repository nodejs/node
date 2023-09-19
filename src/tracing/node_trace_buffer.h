#ifndef SRC_TRACING_NODE_TRACE_BUFFER_H_
#define SRC_TRACING_NODE_TRACE_BUFFER_H_

#include "tracing/agent.h"
#include "node_mutex.h"
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
  InternalTraceBuffer(size_t max_chunks, uint32_t id, Agent* agent);

  TraceObject* AddTraceEvent(uint64_t* handle);
  TraceObject* GetEventByHandle(uint64_t handle);
  void Flush(bool blocking);
  bool IsFull() const {
    return total_chunks_ == max_chunks_ && chunks_[total_chunks_ - 1]->IsFull();
  }
  bool IsFlushing() const {
    return flushing_;
  }

 private:
  uint64_t MakeHandle(size_t chunk_index, uint32_t chunk_seq,
                      size_t event_index) const;
  void ExtractHandle(uint64_t handle, uint32_t* buffer_id, size_t* chunk_index,
                     uint32_t* chunk_seq, size_t* event_index) const;
  size_t Capacity() const { return max_chunks_ * TraceBufferChunk::kChunkSize; }

  Mutex mutex_;
  bool flushing_;
  size_t max_chunks_;
  Agent* agent_;
  std::vector<std::unique_ptr<TraceBufferChunk>> chunks_;
  size_t total_chunks_ = 0;
  uint32_t current_chunk_seq_ = 1;
  uint32_t id_;
};

class NodeTraceBuffer : public TraceBuffer {
 public:
  NodeTraceBuffer(size_t max_chunks, Agent* agent, uv_loop_t* tracing_loop);
  ~NodeTraceBuffer() override;

  TraceObject* AddTraceEvent(uint64_t* handle) override;
  TraceObject* GetEventByHandle(uint64_t handle) override;
  bool Flush() override;

  static const size_t kBufferChunks = 1024;

 private:
  bool TryLoadAvailableBuffer();
  static void NonBlockingFlushSignalCb(uv_async_t* signal);
  static void ExitSignalCb(uv_async_t* signal);

  uv_loop_t* tracing_loop_;
  uv_async_t flush_signal_;
  uv_async_t exit_signal_;
  bool exited_ = false;
  // Used exclusively for exit logic.
  Mutex exit_mutex_;
  // Used to wait until async handles have been closed.
  ConditionVariable exit_cond_;
  std::atomic<InternalTraceBuffer*> current_buf_;
  InternalTraceBuffer buffer1_;
  InternalTraceBuffer buffer2_;
};

}  // namespace tracing
}  // namespace node

#endif  // SRC_TRACING_NODE_TRACE_BUFFER_H_
