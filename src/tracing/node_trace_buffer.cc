#include "tracing/node_trace_buffer.h"
#include "util-inl.h"

namespace node {
namespace tracing {

InternalTraceBuffer::InternalTraceBuffer(size_t max_chunks, uint32_t id,
                                         Agent* agent)
    : flushing_(false), max_chunks_(max_chunks),
      agent_(agent), id_(id) {
  chunks_.resize(max_chunks);
}

TraceObject* InternalTraceBuffer::AddTraceEvent(uint64_t* handle) {
  Mutex::ScopedLock scoped_lock(mutex_);
  // Create new chunk if last chunk is full or there is no chunk.
  if (total_chunks_ == 0 || chunks_[total_chunks_ - 1]->IsFull()) {
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
  if (handle == 0) {
    // A handle value of zero never has a trace event associated with it.
    return nullptr;
  }
  size_t chunk_index, event_index;
  uint32_t buffer_id, chunk_seq;
  ExtractHandle(handle, &buffer_id, &chunk_index, &chunk_seq, &event_index);
  if (buffer_id != id_ || chunk_index >= total_chunks_) {
    // Either the chunk belongs to the other buffer, or is outside the current
    // range of chunks loaded in memory (the latter being true suggests that
    // the chunk has already been flushed and is no longer in memory.)
    return nullptr;
  }
  auto& chunk = chunks_[chunk_index];
  if (chunk->seq() != chunk_seq) {
    // Chunk is no longer in memory.
    return nullptr;
  }
  return chunk->GetEventAt(event_index);
}

void InternalTraceBuffer::Flush(bool blocking) {
  {
    Mutex::ScopedLock scoped_lock(mutex_);
    if (total_chunks_ > 0) {
      flushing_ = true;
      for (size_t i = 0; i < total_chunks_; ++i) {
        auto& chunk = chunks_[i];
        for (size_t j = 0; j < chunk->size(); ++j) {
          TraceObject* trace_event = chunk->GetEventAt(j);
          // Another thread may have added a trace that is yet to be
          // initialized. Skip such traces.
          // https://github.com/nodejs/node/issues/21038.
          if (trace_event->name()) {
            agent_->AppendTraceEvent(trace_event);
          }
        }
      }
      total_chunks_ = 0;
      flushing_ = false;
    }
  }
  agent_->Flush(blocking);
}

uint64_t InternalTraceBuffer::MakeHandle(
    size_t chunk_index, uint32_t chunk_seq, size_t event_index) const {
  return ((static_cast<uint64_t>(chunk_seq) * Capacity() +
          chunk_index * TraceBufferChunk::kChunkSize + event_index) << 1) + id_;
}

void InternalTraceBuffer::ExtractHandle(
    uint64_t handle, uint32_t* buffer_id, size_t* chunk_index,
    uint32_t* chunk_seq, size_t* event_index) const {
  *buffer_id = static_cast<uint32_t>(handle & 0x1);
  handle >>= 1;
  *chunk_seq = static_cast<uint32_t>(handle / Capacity());
  size_t indices = handle % Capacity();
  *chunk_index = indices / TraceBufferChunk::kChunkSize;
  *event_index = indices % TraceBufferChunk::kChunkSize;
}

NodeTraceBuffer::NodeTraceBuffer(size_t max_chunks,
    Agent* agent, uv_loop_t* tracing_loop)
    : tracing_loop_(tracing_loop),
      buffer1_(max_chunks, 0, agent),
      buffer2_(max_chunks, 1, agent) {
  current_buf_.store(&buffer1_);

  flush_signal_.data = this;
  int err = uv_async_init(tracing_loop_, &flush_signal_,
                          NonBlockingFlushSignalCb);
  CHECK_EQ(err, 0);

  exit_signal_.data = this;
  err = uv_async_init(tracing_loop_, &exit_signal_, ExitSignalCb);
  CHECK_EQ(err, 0);
}

NodeTraceBuffer::~NodeTraceBuffer() {
  uv_async_send(&exit_signal_);
  Mutex::ScopedLock scoped_lock(exit_mutex_);
  while (!exited_) {
    exit_cond_.Wait(scoped_lock);
  }
}

TraceObject* NodeTraceBuffer::AddTraceEvent(uint64_t* handle) {
  // If the buffer is full, attempt to perform a flush.
  if (!TryLoadAvailableBuffer()) {
    // Assign a value of zero as the trace event handle.
    // This is equivalent to calling InternalTraceBuffer::MakeHandle(0, 0, 0),
    // and will cause GetEventByHandle to return NULL if passed as an argument.
    *handle = 0;
    return nullptr;
  }
  return current_buf_.load()->AddTraceEvent(handle);
}

TraceObject* NodeTraceBuffer::GetEventByHandle(uint64_t handle) {
  return current_buf_.load()->GetEventByHandle(handle);
}

bool NodeTraceBuffer::Flush() {
  buffer1_.Flush(true);
  buffer2_.Flush(true);
  return true;
}

// Attempts to set current_buf_ such that it references a buffer that can
// write at least one trace event. If both buffers are unavailable this
// method returns false; otherwise it returns true.
bool NodeTraceBuffer::TryLoadAvailableBuffer() {
  InternalTraceBuffer* prev_buf = current_buf_.load();
  if (prev_buf->IsFull()) {
    uv_async_send(&flush_signal_);  // trigger flush on a separate thread
    InternalTraceBuffer* other_buf = prev_buf == &buffer1_ ?
      &buffer2_ : &buffer1_;
    if (!other_buf->IsFull()) {
      current_buf_.store(other_buf);
    } else {
      return false;
    }
  }
  return true;
}

// static
void NodeTraceBuffer::NonBlockingFlushSignalCb(uv_async_t* signal) {
  NodeTraceBuffer* buffer = reinterpret_cast<NodeTraceBuffer*>(signal->data);
  if (buffer->buffer1_.IsFull() && !buffer->buffer1_.IsFlushing()) {
    buffer->buffer1_.Flush(false);
  }
  if (buffer->buffer2_.IsFull() && !buffer->buffer2_.IsFlushing()) {
    buffer->buffer2_.Flush(false);
  }
}

// static
void NodeTraceBuffer::ExitSignalCb(uv_async_t* signal) {
  NodeTraceBuffer* buffer =
      ContainerOf(&NodeTraceBuffer::exit_signal_, signal);

  // Close both flush_signal_ and exit_signal_.
  uv_close(reinterpret_cast<uv_handle_t*>(&buffer->flush_signal_),
           [](uv_handle_t* signal) {
    NodeTraceBuffer* buffer =
        ContainerOf(&NodeTraceBuffer::flush_signal_,
                    reinterpret_cast<uv_async_t*>(signal));

    uv_close(reinterpret_cast<uv_handle_t*>(&buffer->exit_signal_),
             [](uv_handle_t* signal) {
      NodeTraceBuffer* buffer =
          ContainerOf(&NodeTraceBuffer::exit_signal_,
                      reinterpret_cast<uv_async_t*>(signal));
        Mutex::ScopedLock scoped_lock(buffer->exit_mutex_);
        buffer->exited_ = true;
        buffer->exit_cond_.Signal(scoped_lock);
    });
  });
}

}  // namespace tracing
}  // namespace node
