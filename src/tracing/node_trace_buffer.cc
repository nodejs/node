#include "tracing/node_trace_buffer.h"

#include "tracing/agent.h"

namespace node {
namespace tracing {

const double InternalTraceBuffer::kFlushThreshold = 0.75;

InternalTraceBuffer::InternalTraceBuffer(size_t max_chunks,
    NodeTraceWriter* trace_writer, Agent* agent)
    : max_chunks_(max_chunks), trace_writer_(trace_writer), agent_(agent) {
  chunks_.resize(max_chunks);
}

TraceObject* InternalTraceBuffer::AddTraceEvent(uint64_t* handle) {
  Mutex::ScopedLock scoped_lock(mutex_);
  // If the buffer usage exceeds kFlushThreshold, attempt to perform a flush.
  // This number should be customizable.
  if (total_chunks_ >= max_chunks_ * kFlushThreshold) {
    // Tell tracing agent thread to perform a Flush on this buffer.
    // Note that the signal might be ignored if the writer is busy now.
    agent_->SendFlushSignal();
  }
  // Create new chunk if last chunk is full or there is no chunk.
  if (total_chunks_ == 0 || chunks_[total_chunks_ - 1]->IsFull()) {
    if (total_chunks_ == max_chunks_) {
      // There is no more space to store more trace events.
      *handle = MakeHandle(0, 0, 0);
      return nullptr;
    }
    auto& chunk = chunks_[total_chunks_++];
    if (chunk) {
      chunk->Reset(current_chunk_seq_++);
    } else {
      chunk.reset(new TraceBufferChunk(current_chunk_seq_++));
    }
  }
  auto& chunk = chunks_[total_chunks_ - 1];
  size_t event_index;
  TraceObject* trace_object = chunk->AddTraceEvent(&event_index);
  *handle = MakeHandle(total_chunks_ - 1, chunk->seq(), event_index);
  return trace_object;
}

TraceObject* InternalTraceBuffer::GetEventByHandle(uint64_t handle) {
  Mutex::ScopedLock scoped_lock(mutex_);
  size_t chunk_index, event_index;
  uint32_t chunk_seq;
  ExtractHandle(handle, &chunk_index, &chunk_seq, &event_index);
  if (chunk_index >= total_chunks_) {
    return NULL;
  }
  auto& chunk = chunks_[chunk_index];
  if (chunk->seq() != chunk_seq) {
    return NULL;
  }
  return chunk->GetEventAt(event_index);
}

void InternalTraceBuffer::Flush() {
  for (size_t i = 0; i < total_chunks_; ++i) {
    auto& chunk = chunks_[i];
    for (size_t j = 0; j < chunk->size(); ++j) {
      trace_writer_->AppendTraceEvent(chunk->GetEventAt(j));
    }
  }
  trace_writer_->Flush();
  total_chunks_ = 0;
}

uint64_t InternalTraceBuffer::MakeHandle(
    size_t chunk_index, uint32_t chunk_seq, size_t event_index) const {
  return static_cast<uint64_t>(chunk_seq) * Capacity() +
         chunk_index * TraceBufferChunk::kChunkSize + event_index;
}

void InternalTraceBuffer::ExtractHandle(
    uint64_t handle, size_t* chunk_index,
    uint32_t* chunk_seq, size_t* event_index) const {
  *chunk_seq = static_cast<uint32_t>(handle / Capacity());
  size_t indices = handle % Capacity();
  *chunk_index = indices / TraceBufferChunk::kChunkSize;
  *event_index = indices % TraceBufferChunk::kChunkSize;
}

NodeTraceBuffer::NodeTraceBuffer(size_t max_chunks,
    NodeTraceWriter* trace_writer, Agent* agent)
    : trace_writer_(trace_writer),
      buffer1_(max_chunks, trace_writer, agent),
      buffer2_(max_chunks, trace_writer, agent) {
  current_buf_.store(&buffer1_);
}

TraceObject* NodeTraceBuffer::AddTraceEvent(uint64_t* handle) {
  return current_buf_.load()->AddTraceEvent(handle);
}

TraceObject* NodeTraceBuffer::GetEventByHandle(uint64_t handle) {
  return current_buf_.load()->GetEventByHandle(handle);
}

bool NodeTraceBuffer::Flush() {
  // This function should mainly be called from the tracing agent thread.
  // TODO: However, it could be called from the main thread, when
  // the tracing controller stops tracing.
  // Ideally we would want all Flushes to be done on the tracing thread, to
  // prevent concurrent access of trace_writer_.
  if (!trace_writer_->IsReady()) {
    return false;
  }
  InternalTraceBuffer* prev_buf = current_buf_.load();
  if (prev_buf == &buffer1_) {
    current_buf_.store(&buffer2_);
  } else {
    current_buf_.store(&buffer1_);
  }
  // Flush the other buffer.
  // Note that concurrently, we can AddTraceEvent to the current buffer.
  prev_buf->Flush();
  return true;
}

}  // namespace tracing
}  // namespace node
