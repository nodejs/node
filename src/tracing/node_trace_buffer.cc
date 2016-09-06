#include "tracing/node_trace_buffer.h"

namespace node {
namespace tracing {

const double InternalTraceBuffer::kFlushThreshold = 0.75;

InternalTraceBuffer::InternalTraceBuffer(size_t max_chunks,
    NodeTraceWriter* trace_writer, NodeTraceBuffer* external_buffer)
    : max_chunks_(max_chunks), trace_writer_(trace_writer), external_buffer_(external_buffer) {
  chunks_.resize(max_chunks);
}

TraceObject* InternalTraceBuffer::AddTraceEvent(uint64_t* handle) {
  Mutex::ScopedLock scoped_lock(mutex_);
  // If the buffer usage exceeds kFlushThreshold, attempt to perform a flush.
  // This number should be customizable.
  if (total_chunks_ >= max_chunks_ * kFlushThreshold) {
    mutex_.Unlock();
    external_buffer_->Flush(false);
    mutex_.Lock();
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

void InternalTraceBuffer::Flush(bool blocking) {
  {
    Mutex::ScopedLock scoped_lock(mutex_);
    for (size_t i = 0; i < total_chunks_; ++i) {
      auto& chunk = chunks_[i];
      for (size_t j = 0; j < chunk->size(); ++j) {
        trace_writer_->AppendTraceEvent(chunk->GetEventAt(j));
      }
    }
    total_chunks_ = 0;
  }
  trace_writer_->Flush(blocking);
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
    NodeTraceWriter* trace_writer, uv_loop_t* tracing_loop)
    : trace_writer_(trace_writer), tracing_loop_(tracing_loop),
      buffer1_(max_chunks, trace_writer, this),
      buffer2_(max_chunks, trace_writer, this) {
  current_buf_.store(&buffer1_);

  flush_signal_.data = this;
  int err = uv_async_init(tracing_loop_, &flush_signal_, NonBlockingFlushSignalCb);
  CHECK_EQ(err, 0);
  
  exit_signal_.data = this;
  err = uv_async_init(tracing_loop_, &exit_signal_, ExitSignalCb);
  CHECK_EQ(err, 0);
}

NodeTraceBuffer::~NodeTraceBuffer() {
  uv_async_send(&exit_signal_);
}

TraceObject* NodeTraceBuffer::AddTraceEvent(uint64_t* handle) {
  return current_buf_.load()->AddTraceEvent(handle);
}

TraceObject* NodeTraceBuffer::GetEventByHandle(uint64_t handle) {
  return current_buf_.load()->GetEventByHandle(handle);
}

// TODO: This function is here to match the parent class signature,
// but it's not expressive enough because it doesn't allow us to specify
// whether it should block or not. The parent class signature should be changed
// in the future to include this parameter, and when that's done, this
// function should be removed.
bool NodeTraceBuffer::Flush() {
  return Flush(true);
}

void NodeTraceBuffer::FlushPrivate(bool blocking) {
  InternalTraceBuffer* prev_buf = current_buf_.load();
  if (prev_buf == &buffer1_) {
    current_buf_.store(&buffer2_);
  } else {
    current_buf_.store(&buffer1_);
  }
  // Flush the other buffer.
  // Note that concurrently, we can AddTraceEvent to the current buffer.
  prev_buf->Flush(blocking);
}

// static
void NodeTraceBuffer::NonBlockingFlushSignalCb(uv_async_t* signal) {
  NodeTraceBuffer* buffer = reinterpret_cast<NodeTraceBuffer*>(signal->data);
  buffer->FlushPrivate(false);
}

bool NodeTraceBuffer::Flush(bool blocking) {
  if (blocking) {
    FlushPrivate(true);
  } else {
    uv_async_send(&flush_signal_);
  }
  return true;
}

// static
void NodeTraceBuffer::ExitSignalCb(uv_async_t* signal) {
  NodeTraceBuffer* buffer = reinterpret_cast<NodeTraceBuffer*>(signal->data);
  uv_close(reinterpret_cast<uv_handle_t*>(&buffer->flush_signal_), nullptr);
  uv_close(reinterpret_cast<uv_handle_t*>(&buffer->exit_signal_), nullptr);
}

}  // namespace tracing
}  // namespace node
