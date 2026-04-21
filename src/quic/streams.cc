#include "ngtcp2/ngtcp2.h"
#if HAVE_OPENSSL && HAVE_QUIC
#include "guard.h"
#ifndef OPENSSL_NO_QUIC
#include <aliased_struct-inl.h>
#include <async_wrap-inl.h>
#include <base_object-inl.h>
#include <env-inl.h>
#include <memory_tracker-inl.h>
#include <node_blob.h>
#include <node_bob-inl.h>
#include <node_file.h>
#include <node_sockaddr-inl.h>
#include "application.h"
#include "bindingdata.h"
#include "defs.h"
#include "session.h"
#include "streams.h"

namespace node {

using v8::Array;
using v8::ArrayBuffer;
using v8::ArrayBufferView;
using v8::BackingStore;
using v8::BigInt;
using v8::FunctionCallbackInfo;
using v8::Global;
using v8::Integer;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::Nothing;
using v8::Object;
using v8::ObjectTemplate;
using v8::SharedArrayBuffer;
using v8::Uint32;
using v8::Uint8Array;
using v8::Value;

namespace quic {

#define STREAM_STATE(V)                                                        \
  V(ID, id, stream_id)                                                         \
  V(PENDING, pending, uint8_t)                                                 \
  V(FIN_SENT, fin_sent, uint8_t)                                               \
  V(FIN_RECEIVED, fin_received, uint8_t)                                       \
  V(READ_ENDED, read_ended, uint8_t)                                           \
  V(WRITE_ENDED, write_ended, uint8_t)                                         \
  V(RESET, reset, uint8_t)                                                     \
  V(RESET_CODE, reset_code, uint64_t)                                          \
  V(HAS_OUTBOUND, has_outbound, uint8_t)                                       \
  V(HAS_READER, has_reader, uint8_t)                                           \
  /* Set when the stream has a block event handler */                          \
  V(WANTS_BLOCK, wants_block, uint8_t)                                         \
  /* Set when the stream has a headers event handler */                        \
  V(WANTS_HEADERS, wants_headers, uint8_t)                                     \
  /* Set when the stream has a reset event handler */                          \
  V(WANTS_RESET, wants_reset, uint8_t)                                         \
  /* Set when the stream has a trailers event handler */                       \
  V(WANTS_TRAILERS, wants_trailers, uint8_t)                                   \
  /* True when 0-RTT early data was received */                                \
  V(RECEIVED_EARLY_DATA, received_early_data, uint8_t)                         \
  V(WRITE_DESIRED_SIZE, write_desired_size, uint32_t)                          \
  V(HIGH_WATER_MARK, high_water_mark, uint32_t)

#define STREAM_STATS(V)                                                        \
  /* Marks the timestamp when the stream object was created. */                \
  V(CREATED_AT, created_at)                                                    \
  /* Marks the timestamp when the stream was opened. This can be different */  \
  /* from the created_at timestamp if the stream was created in as pending */  \
  V(OPENED_AT, opened_at)                                                      \
  /* Marks the timestamp when the stream last received data */                 \
  V(RECEIVED_AT, received_at)                                                  \
  /* Marks the timestamp when the stream last received an acknowledgement */   \
  V(ACKED_AT, acked_at)                                                        \
  /* Marks the timestamp when the stream was destroyed */                      \
  V(DESTROYED_AT, destroyed_at)                                                \
  /* Records the total number of bytes received by the stream */               \
  V(BYTES_RECEIVED, bytes_received)                                            \
  /* Records the total number of bytes sent by the stream */                   \
  V(BYTES_SENT, bytes_sent)                                                    \
  V(MAX_OFFSET, max_offset)                                                    \
  V(MAX_OFFSET_ACK, max_offset_ack)                                            \
  V(MAX_OFFSET_RECV, max_offset_received)                                      \
  V(FINAL_SIZE, final_size)

#define STREAM_JS_METHODS(V)                                                   \
  V(AttachSource, attachSource, false)                                         \
  V(Destroy, destroy, false)                                                   \
  V(SendHeaders, sendHeaders, false)                                           \
  V(StopSending, stopSending, false)                                           \
  V(ResetStream, resetStream, false)                                           \
  V(SetPriority, setPriority, false)                                           \
  V(GetPriority, getPriority, true)                                            \
  V(GetReader, getReader, false)                                               \
  V(InitStreamingSource, initStreamingSource, false)                           \
  V(Write, write, false)                                                       \
  V(EndWrite, endWrite, false)

// ============================================================================

PendingStream::PendingStream(Direction direction,
                             Stream* stream,
                             BaseObjectWeakPtr<Session> session)
    : direction_(direction), stream_(stream), session_(session) {
  if (session_) {
    if (direction == Direction::BIDIRECTIONAL) {
      session_->pending_bidi_stream_queue().PushBack(this);
    } else {
      session_->pending_uni_stream_queue().PushBack(this);
    }
  }
}

PendingStream::~PendingStream() {
  pending_stream_queue_.Remove();
  if (waiting_) {
    Debug(stream_, "A pending stream was canceled");
  }
}

void PendingStream::fulfill(stream_id id) {
  CHECK(waiting_);
  waiting_ = false;
  stream_->NotifyStreamOpened(id);
}

void PendingStream::reject(QuicError error) {
  CHECK(waiting_);
  waiting_ = false;
  stream_->Destroy(error);
}

struct Stream::PendingHeaders {
  HeadersKind kind;
  Global<Array> headers;
  HeadersFlags flags;
  PendingHeaders(HeadersKind kind_, Global<Array> headers_, HeadersFlags flags_)
      : kind(kind_), headers(std::move(headers_)), flags(flags_) {}
  DISALLOW_COPY_AND_MOVE(PendingHeaders)
};

// ============================================================================

struct Stream::State {
#define V(_, name, type) type name;
  STREAM_STATE(V)
#undef V
};

STAT_STRUCT(Stream, STREAM)

// ============================================================================

namespace {
// Creates an in-memory DataQueue entry by copying the requested range of
// the given ArrayBuffer into a fresh BackingStore. The caller's buffer is
// not detached or otherwise modified, so callers can safely reuse or
// mutate it after the call returns. Callers that want to ensure their
// buffer cannot be mutated after handing it off can call
// `ArrayBuffer.prototype.transfer()` themselves before calling into the
// QUIC API.
// Returns nullptr on zero length or allocation failure.
std::unique_ptr<DataQueue::Entry> CreateEntryFromBuffer(
    Environment* env, Local<ArrayBuffer> buffer, size_t offset, size_t length) {
  if (length == 0) return nullptr;
  JS_TRY_ALLOCATE_BACKING_OR_RETURN(env, copy, length, nullptr);
  memcpy(copy->Data(),
         static_cast<const uint8_t*>(buffer->Data()) + offset,
         length);
  return DataQueue::CreateInMemoryEntryFromBackingStore(
      std::move(copy), 0, length);
}
}  // namespace

Maybe<std::shared_ptr<DataQueue>> Stream::GetDataQueueFromSource(
    Environment* env, Local<Value> value) {
  DCHECK_IMPLIES(!value->IsUndefined(), value->IsObject());
  std::vector<std::unique_ptr<DataQueue::Entry>> entries;
  if (value->IsUndefined()) {
    // Return an empty DataQueue.
    return Just(std::shared_ptr<DataQueue>());
  } else if (value->IsArrayBuffer()) {
    auto buffer = value.As<ArrayBuffer>();
    auto length = buffer->ByteLength();
    if (length > 0) {
      auto entry = CreateEntryFromBuffer(env, buffer, 0, length);
      if (!entry) {
        return Nothing<std::shared_ptr<DataQueue>>();
      }
      entries.push_back(std::move(entry));
    }
    return Just(DataQueue::CreateIdempotent(std::move(entries)));
  } else if (value->IsSharedArrayBuffer()) {
    auto sab = value.As<SharedArrayBuffer>();
    auto length = sab->ByteLength();
    if (length > 0) {
      // SharedArrayBuffer cannot be detached, so we always copy. Note that
      // because of the nature of SAB, another thread can end up modifying
      // the SAB while we're copying, which is racy but unavoidable.
      JS_TRY_ALLOCATE_BACKING_OR_RETURN(
          env, backing, length, Nothing<std::shared_ptr<DataQueue>>());
      memcpy(backing->Data(), sab->Data(), length);
      entries.push_back(DataQueue::CreateInMemoryEntryFromBackingStore(
          std::move(backing), 0, length));
    }
    return Just(DataQueue::CreateIdempotent(std::move(entries)));
  } else if (value->IsArrayBufferView()) {
    auto view = value.As<ArrayBufferView>();
    auto offset = view->ByteOffset();
    auto length = view->ByteLength();
    if (length > 0) {
      auto entry = CreateEntryFromBuffer(env, view->Buffer(), offset, length);
      if (!entry) {
        return Nothing<std::shared_ptr<DataQueue>>();
      }
      entries.push_back(std::move(entry));
    }
    return Just(DataQueue::CreateIdempotent(std::move(entries)));
  } else if (Blob::HasInstance(env, value)) {
    Blob* blob;
    ASSIGN_OR_RETURN_UNWRAP(
        &blob, value, Nothing<std::shared_ptr<DataQueue>>());
    return Just(blob->getDataQueue().slice(0));
  } else if (value->IsString()) {
    Utf8Value str(env->isolate(), value);
    JS_TRY_ALLOCATE_BACKING_OR_RETURN(
        env, backing, str.length(), Nothing<std::shared_ptr<DataQueue>>());
    memcpy(backing->Data(), *str, str.length());
    auto len = backing->ByteLength();
    entries.push_back(DataQueue::CreateInMemoryEntryFromBackingStore(
        std::move(backing), 0, len));
    return Just(DataQueue::CreateIdempotent(std::move(entries)));
  }
  // FileHandle — create an fd-backed DataQueue from the file path.
  // The JS side validates and locks the FileHandle before passing
  // the C++ handle here. We detect FileHandle by checking if the
  // object's constructor name is "FileHandle".
  if (value->IsObject()) {
    auto obj = value.As<Object>();
    Local<v8::String> ctor_name;
    auto maybe_name = obj->GetConstructorName();
    if (!maybe_name.IsEmpty()) {
      ctor_name = maybe_name;
      Utf8Value name(env->isolate(), ctor_name);
      if (strcmp(*name, "FileHandle") == 0) {
        fs::FileHandle* file_handle;
        ASSIGN_OR_RETURN_UNWRAP(
            &file_handle, value, Nothing<std::shared_ptr<DataQueue>>());
        Local<Value> path;
        if (!v8::String::NewFromUtf8(env->isolate(),
                                     file_handle->original_name().c_str())
                 .ToLocal(&path)) {
          return Nothing<std::shared_ptr<DataQueue>>();
        }
        auto entry = DataQueue::CreateFdEntry(env, path);
        if (!entry) return Nothing<std::shared_ptr<DataQueue>>();
        size_t size = entry->size().value_or(0);
        auto queue = DataQueue::Create();
        if (!queue) return Nothing<std::shared_ptr<DataQueue>>();
        queue->append(std::move(entry));
        queue->cap(size);
        return Just(std::move(queue));
      }
    }
  }
  // TODO(jasnell): Add streaming sources...
  THROW_ERR_INVALID_ARG_TYPE(env, "Invalid data source type");
  return Nothing<std::shared_ptr<DataQueue>>();
}

// Provides the implementation of the various JavaScript APIs for the
// Stream object.
struct Stream::Impl {
  // Attaches an outbound data source to the stream.
  JS_METHOD(AttachSource) {
    Environment* env = Environment::GetCurrent(args);
    Stream* stream;
    ASSIGN_OR_RETURN_UNWRAP(&stream, args.This());

    std::shared_ptr<DataQueue> dataqueue;
    if (GetDataQueueFromSource(env, args[0]).To(&dataqueue)) {
      stream->set_outbound(std::move(dataqueue));
      // set_outbound does not call ResumeStream because during
      // construction the stream is not yet registered with the session.
      // When attaching a source after creation (via setBody), the
      // stream is already registered and must be resumed to enter the
      // send queue.
      if (!stream->is_pending()) {
        stream->session().ResumeStream(stream->id());
      }
    }
  }

  // Immediately and forcefully destroys the stream.
  JS_METHOD(Destroy) {
    Stream* stream;
    ASSIGN_OR_RETURN_UNWRAP(&stream, args.This());
    if (args.Length() >= 1) {
      CHECK(args[0]->IsBigInt());
      bool lossless = false;
      uint64_t code = args[0].As<BigInt>()->Uint64Value(&lossless);
      // If the code cannot be represented in 64 bits, it is too large to be
      // a valid QUIC error code, error!
      if (!lossless) {
        THROW_ERR_INVALID_ARG_TYPE(stream->env(), "Error code is too large");
        return;
      }
      stream->Destroy(QuicError::ForApplication(code));
    } else {
      stream->Destroy();
    }
  }

  // Sends a block of headers to the peer. If the stream is not yet open,
  // the headers will be queued and sent immediately when the stream is
  // opened. Returns false if the application does not support headers.
  JS_METHOD(SendHeaders) {
    Stream* stream;
    ASSIGN_OR_RETURN_UNWRAP(&stream, args.This());
    CHECK(args[0]->IsUint32());  // Kind
    CHECK(args[1]->IsArray());   // Headers
    CHECK(args[2]->IsUint32());  // Flags

    HeadersKind kind = FromV8Value<HeadersKind>(args[0]);
    Local<Array> headers = args[1].As<Array>();
    HeadersFlags flags = FromV8Value<HeadersFlags>(args[2]);

    // If the stream is pending, the headers will be queued until the
    // stream is opened, at which time the queued header block will be
    // immediately sent when the stream is opened. If we already know
    // that the application does not support headers, return false
    // immediately so the JS side can throw an appropriate error.
    if (stream->is_pending()) {
      if (!stream->session().application().SupportsHeaders()) {
        return args.GetReturnValue().Set(false);
      }
      stream->EnqueuePendingHeaders(kind, headers, flags);
      return args.GetReturnValue().Set(true);
    }

    args.GetReturnValue().Set(stream->session().application().SendHeaders(
        *stream, kind, headers, flags));
  }

  // Tells the peer to stop sending data for this stream. This has the effect
  // of shutting down the readable side of the stream for this peer. Any data
  // that has already been received is still readable.
  JS_METHOD(StopSending) {
    Stream* stream;
    ASSIGN_OR_RETURN_UNWRAP(&stream, args.This());
    error_code code = 0;
    CHECK_IMPLIES(!args[0]->IsUndefined(), args[0]->IsBigInt());
    if (!args[0]->IsUndefined()) {
      bool unused = false;  // not used but still necessary.
      code = args[0].As<BigInt>()->Uint64Value(&unused);
    }

    stream->EndReadable();

    if (!stream->is_pending()) {
      // If the stream is a local unidirectional there's nothing to do here.
      if (stream->is_local_unidirectional()) return;
      stream->NotifyReadableEnded(code);
    } else {
      stream->pending_close_read_code_ = code;
    }
  }

  // Sends a reset stream to the peer to tell it we will not be sending any
  // more data for this stream. This has the effect of shutting down the
  // writable side of the stream for this peer. Any data that is held in the
  // outbound queue will be dropped. The stream may still be readable.
  JS_METHOD(ResetStream) {
    Stream* stream;
    ASSIGN_OR_RETURN_UNWRAP(&stream, args.This());
    error_code code = 0;
    CHECK_IMPLIES(!args[0]->IsUndefined(), args[0]->IsBigInt());
    if (!args[0]->IsUndefined()) {
      bool lossless = false;  // not used but still necessary.
      code = args[0].As<BigInt>()->Uint64Value(&lossless);
    }

    if (stream->state_->reset == 1) return;

    stream->EndWritable();
    // We can release our outbound here now. Since the stream is being reset
    // on the ngtcp2 side, we do not need to keep any of the data around
    // waiting for acknowledgement that will never come.
    stream->outbound_.reset();
    stream->state_->reset = 1;

    if (!stream->is_pending()) {
      if (stream->is_remote_unidirectional()) return;
      stream->NotifyWritableEnded(code);
    } else {
      stream->pending_close_write_code_ = code;
    }
  }

  JS_METHOD(SetPriority) {
    Stream* stream;
    ASSIGN_OR_RETURN_UNWRAP(&stream, args.This());
    CHECK(args[0]->IsUint32());  // Packed: (urgency << 1) | incremental

    uint32_t packed = args[0].As<Uint32>()->Value();
    StreamPriority priority = static_cast<StreamPriority>(packed >> 1);
    StreamPriorityFlags flags = (packed & 1)
                                    ? StreamPriorityFlags::INCREMENTAL
                                    : StreamPriorityFlags::NON_INCREMENTAL;

    // Always update the stored priority on the stream.
    stream->priority_ = StoredPriority{
        .priority = priority,
        .flags = flags,
        .pending = stream->is_pending(),
    };

    if (!stream->is_pending()) {
      stream->session().application().SetStreamPriority(
          *stream, priority, flags);
    }
  }

  JS_METHOD(GetPriority) {
    Stream* stream;
    ASSIGN_OR_RETURN_UNWRAP(&stream, args.This());

    // On the client side, priority is always read from the stream's
    // stored value since the client is the one setting it. On the
    // server side, we delegate to the application which can read
    // the peer's requested priority (e.g., from PRIORITY_UPDATE
    // frames in HTTP/3).
    if (!stream->session().is_server()) {
      auto& pri = stream->priority_;
      uint32_t packed = (static_cast<uint32_t>(pri.priority) << 1) |
                        (pri.flags == StreamPriorityFlags::INCREMENTAL ? 1 : 0);
      return args.GetReturnValue().Set(packed);
    }

    auto result = stream->session().application().GetStreamPriority(*stream);
    uint32_t packed =
        (static_cast<uint32_t>(result.priority) << 1) |
        (result.flags == StreamPriorityFlags::INCREMENTAL ? 1 : 0);
    args.GetReturnValue().Set(packed);
  }

  // Returns a Blob::Reader that can be used to read data that has been
  // received on the stream.
  JS_METHOD(GetReader) {
    Stream* stream;
    ASSIGN_OR_RETURN_UNWRAP(&stream, args.This());
    BaseObjectPtr<Blob::Reader> reader = stream->get_reader();
    if (reader) args.GetReturnValue().Set(reader->object());
    // Returns undefined when the stream is not readable (e.g. a local
    // unidirectional stream). The JS side checks for this.
  }

  JS_METHOD(InitStreamingSource) {
    Stream* stream;
    ASSIGN_OR_RETURN_UNWRAP(&stream, args.This());
    stream->InitStreaming();
  }

  JS_METHOD(Write) {
    Stream* stream;
    ASSIGN_OR_RETURN_UNWRAP(&stream, args.This());
    stream->WriteStreamData(args);
  }

  JS_METHOD(EndWrite) {
    Stream* stream;
    ASSIGN_OR_RETURN_UNWRAP(&stream, args.This());
    stream->EndWriting();
  }
};

// ============================================================================

class Stream::Outbound final : public MemoryRetainer {
 public:
  explicit Outbound(Stream* stream, std::shared_ptr<DataQueue> queue)
      : stream_(stream),
        queue_(std::move(queue)),
        reader_(queue_->get_reader()) {}

  // Creates an Outbound in streaming mode with a non-idempotent DataQueue
  // that can be appended to via AppendEntry().
  explicit Outbound(Stream* stream)
      : stream_(stream),
        queue_(DataQueue::Create()),
        reader_(queue_->get_reader()),
        streaming_(true) {}

  void Acknowledge(size_t amount) {
    size_t remaining = std::min(amount, total_ - uncommitted_);
    while (remaining > 0 && head_ != nullptr) {
      DCHECK_LE(head_->ack_offset, head_->offset);
      // The amount to acknowledge in this chunk is the lesser of the total
      // amount remaining to acknowledge or the total remaining unacknowledged
      // bytes in the chunk.
      size_t amount_to_ack =
          std::min(remaining, head_->offset - head_->ack_offset);
      // If the amount to ack is zero here, it means our ack offset has caught
      // up to our commit offset, which means there's nothing left to
      // acknowledge yet. We could treat this as an error but let's just stop
      // here.
      if (amount_to_ack == 0) break;

      // Adjust our remaining down and our ack_offset up...
      remaining -= amount_to_ack;
      head_->ack_offset += amount_to_ack;

      // If we've fully acknowledged this chunk, free it and decrement total.
      if (head_->ack_offset == head_->buf.len) {
        DCHECK_GE(total_, head_->buf.len);
        total_ -= head_->buf.len;
        // if tail_ == head_ here, it means we've fully acknowledged our current
        // buffer. Set tail to nullptr since we're freeing it here.
        if (head_.get() == tail_) {
          // In this case, commit_head_ should have already been set to nullptr.
          // Because we should only have hit this case if the entire buffer
          // had been committed.
          DCHECK(commit_head_ == nullptr);
          tail_ = nullptr;
        }
        head_ = std::move(head_->next);
        DCHECK_IMPLIES(head_ == nullptr, tail_ == nullptr);
      }
    }
  }

  void Commit(size_t amount) {
    // Commit amount number of bytes from the current uncommitted
    // byte queue. Importantly, this does not remove the bytes
    // from the byte queue.
    size_t remaining = std::min(uncommitted_, amount);
    // There's nothing to commit.
    while (remaining > 0 && commit_head_ != nullptr) {
      // The amount to commit is the lesser of the total amount remaining to
      // commit and the remaining uncommitted bytes in this chunk.
      size_t amount_to_commit = std::min(
          remaining,
          static_cast<size_t>(commit_head_->buf.len - commit_head_->offset));

      // The amount to commit here should never be zero because that means we
      // should have already advanced the commit head.
      DCHECK_NE(amount_to_commit, 0);
      uncommitted_ -= amount_to_commit;
      remaining -= amount_to_commit;
      commit_head_->offset += amount_to_commit;
      if (commit_head_->offset == commit_head_->buf.len) {
        count_--;
        commit_head_ = commit_head_->next.get();
      }
    }
  }

  void Cap() {
    // Calling cap without a value halts the ability to add any
    // new data to the queue if it is not idempotent. If it is
    // idempotent, it's a non-op.
    if (queue_) queue_->cap();
  }

  bool is_streaming() const { return streaming_; }
  size_t total() const { return total_; }
  size_t uncommitted() const { return uncommitted_; }

  // Total bytes in the pipeline: data appended to the DataQueue that
  // hasn't been pulled yet, plus data pulled but not yet acknowledged.
  // This is the number to compare against highWaterMark for backpressure.
  size_t queued_bytes() const { return queued_ + total_; }

  // Appends an entry to the underlying DataQueue. Only valid when
  // the Outbound was created in streaming mode.
  bool AppendEntry(std::unique_ptr<DataQueue::Entry> entry) {
    if (!streaming_ || !queue_) return false;
    auto size = entry->size();
    auto result = queue_->append(std::move(entry));
    if (result.has_value() && result.value()) {
      if (size.has_value()) queued_ += size.value();
      return true;
    }
    return false;
  }

  int Pull(bob::Next<ngtcp2_vec> next,
           int options,
           ngtcp2_vec* data,
           size_t count,
           size_t max_count_hint) {
    if (next_pending_) {
      // An async read is in flight, but there may be uncommitted bytes
      // from a previous read that ngtcp2 didn't accept (nwrite=0 due
      // to pacing/congestion). Return those bytes so the send loop can
      // retry rather than blocking until the async read completes.
      if (uncommitted_ > 0) {
        PullUncommitted(std::move(next));
        return bob::Status::STATUS_CONTINUE;
      }
      std::move(next)(bob::Status::STATUS_BLOCK, nullptr, 0, [](int) {});
      return bob::Status::STATUS_BLOCK;
    }

    if (errored_) {
      std::move(next)(UV_EBADF, nullptr, 0, [](int) {});
      return UV_EBADF;
    }

    // If eos_ is true and there are no uncommitted bytes we'll return eos,
    // otherwise, return whatever is in the uncommitted queue.
    if (eos_) {
      if (uncommitted_ > 0) {
        PullUncommitted(std::move(next));
        return bob::Status::STATUS_CONTINUE;
      }
      std::move(next)(bob::Status::STATUS_EOS, nullptr, 0, [](int) {});
      return bob::Status::STATUS_EOS;
    }

    // If there are uncommitted bytes in the queue_, and there are enough to
    // fill a full data packet, then pull will just return the current
    // uncommitted bytes currently in the queue rather than reading more from
    // the queue.
    if (uncommitted_ >= kDefaultMaxPacketLength) {
      PullUncommitted(std::move(next));
      return bob::Status::STATUS_CONTINUE;
    }

    DCHECK(queue_);
    DCHECK(reader_);

    // At this point, we know our reader hasn't finished yet, there might be
    // uncommitted bytes but we want to go ahead and pull some more. We request
    // that the pull is sync but allow for it to be async.
    int ret = reader_->Pull(
        [this](auto status, auto vecs, auto count, auto done) {
          // The status should never be wait here.
          DCHECK_NE(status, bob::Status::STATUS_WAIT);

          if (status < 0) {
            // If next_pending_ is true then a pull from the reader ended up
            // being asynchronous, our stream is blocking waiting for the data,
            // but we have an error! oh no! We need to error the stream.
            if (next_pending_) {
              next_pending_ = false;
              stream_->Destroy(
                  QuicError::ForNgtcp2Error(NGTCP2_INTERNAL_ERROR));
              // We do not need to worry about calling MarkErrored in this case
              // since we are immediately destroying the stream which will
              // release the outbound buffer anyway.
            }
            return;
          }

          if (status == bob::Status::STATUS_EOS) {
            DCHECK_EQ(count, 0);
            DCHECK_NULL(vecs);
            MarkEnded();
            // If next_pending_ is true then a pull from the reader ended up
            // being asynchronous, our stream is blocking waiting for the data.
            // Here, there is no more data to read, but we will might have data
            // in the uncommitted queue. We'll resume the stream so that the
            // session will try to read from it again.
            // We must clear next_pending_ before calling ResumeStream because
            // ResumeStream can synchronously re-enter Outbound::Pull.
            if (next_pending_) {
              next_pending_ = false;
              stream_->session().ResumeStream(stream_->id());
            }
            return;
          }

          if (status == bob::Status::STATUS_BLOCK) {
            DCHECK_EQ(count, 0);
            DCHECK_NULL(vecs);
            // If next_pending_ is true then a pull from the reader ended up
            // being asynchronous, our stream is blocking waiting for the data.
            // Here, we're still blocking! so there's nothing left for us to do!
            return;
          }

          DCHECK_EQ(status, bob::Status::STATUS_CONTINUE);
          // If the read returns bytes, those will be added to the uncommitted
          // bytes in the queue.
          Append(vecs, count, std::move(done));

          // If next_pending_ is true, then a pull from the reader ended up
          // being asynchronous, our stream is blocking waiting for the data.
          // Now that we have data, let's resume the stream so the session will
          // pull from it again.
          // We must clear next_pending_ before calling ResumeStream because
          // ResumeStream can synchronously re-enter Outbound::Pull.
          if (next_pending_) {
            next_pending_ = false;
            stream_->session().ResumeStream(stream_->id());
          }
        },
        bob::OPTIONS_SYNC,
        nullptr,
        0,
        kMaxVectorCount);

    // There was an error. We'll report that immediately. We do not have
    // to destroy the stream here since that will be taken care of by
    // the caller.
    if (ret < 0) {
      MarkErrored();
      std::move(next)(ret, nullptr, 0, [](int) {});
      // Since we are erroring and won't be able to make use of this DataQueue
      // any longer, let's free both it and the reader and put ourselves into
      // an errored state. Further attempts to read from the outbound will
      // result in a UV_EBADF error. The caller, however, should handle this by
      // closing down the stream so that doesn't happen.
      return ret;
    }

    if (ret == bob::Status::STATUS_EOS) {
      // Here, we know we are done with the DataQueue and the Reader, but we
      // might not yet have committed or acknowledged all of the queued data.
      // We'll release our references to the queue_ and reader_ but everything
      // else is untouched.
      MarkEnded();
      if (uncommitted_ > 0) {
        // If the read returns eos, and there are uncommitted bytes in the
        // queue, we'll set eos_ to true and return the current set of
        // uncommitted bytes.
        PullUncommitted(std::move(next));
        return bob::STATUS_CONTINUE;
      }
      // If the read returns eos, and there are no uncommitted bytes in the
      // queue, we'll return eos with no data.
      std::move(next)(bob::Status::STATUS_EOS, nullptr, 0, [](int) {});
      return bob::Status::STATUS_EOS;
    }

    if (ret == bob::Status::STATUS_BLOCK) {
      // If the read returns blocked, and there are uncommitted bytes in the
      // queue, we'll return the current set of uncommitted bytes.
      if (uncommitted_ > 0) {
        PullUncommitted(std::move(next));
        return bob::Status::STATUS_CONTINUE;
      }
      // If the read returns blocked, and there are no uncommitted bytes in the
      // queue, we'll return blocked.
      std::move(next)(bob::Status::STATUS_BLOCK, nullptr, 0, [](int) {});
      return bob::Status::STATUS_BLOCK;
    }

    // Reads here are generally expected to be synchronous. If we have a reader
    // that insists on providing data asynchronously, then we'll have to block
    // until the data is actually available. However, if there are uncommitted
    // bytes already buffered (from a previous async read), return those now
    // rather than blocking — the async callback will resume the stream when
    // more data arrives.
    if (ret == bob::Status::STATUS_WAIT) {
      next_pending_ = true;
      if (uncommitted_ > 0) {
        PullUncommitted(std::move(next));
        return bob::Status::STATUS_CONTINUE;
      }

      std::move(next)(bob::Status::STATUS_BLOCK, nullptr, 0, [](int) {});
      return bob::Status::STATUS_BLOCK;
    }

    DCHECK_EQ(ret, bob::Status::STATUS_CONTINUE);
    PullUncommitted(std::move(next));
    return bob::Status::STATUS_CONTINUE;
  }

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackField("queue", queue_);
    tracker->TrackField("reader", reader_);
    tracker->TrackFieldWithSize("buffer", total_);
  }

  SET_MEMORY_INFO_NAME(Stream::Outbound)
  SET_SELF_SIZE(Outbound)

 private:
  struct OnComplete {
    bob::Done done;
    explicit OnComplete(bob::Done done) : done(std::move(done)) {}
    ~OnComplete() { std::move(done)(0); }
  };

  void PullUncommitted(bob::Next<ngtcp2_vec> next) {
    MaybeStackBuffer<ngtcp2_vec, 16> chunks;
    chunks.AllocateSufficientStorage(count_);
    auto head = commit_head_;
    size_t n = 0;
    while (head != nullptr && n < count_) {
      // There might only be one byte here but there should never be zero.
      DCHECK_LT(head->offset, head->buf.len);
      chunks[n].base = head->buf.base + head->offset;
      chunks[n].len = head->buf.len - head->offset;
      head = head->next.get();
      n++;
    }
    std::move(next)(bob::Status::STATUS_CONTINUE, chunks.out(), n, [](int) {});
  }

  void MarkErrored() {
    errored_ = true;
    head_.reset();
    tail_ = nullptr;
    commit_head_ = nullptr;
    total_ = 0;
    count_ = 0;
    uncommitted_ = 0;
    MarkEnded();
  }

  void MarkEnded() {
    eos_ = true;
    queue_.reset();
    reader_.reset();
  }

  void Append(const DataQueue::Vec* vectors, size_t count, bob::Done done) {
    if (count == 0) return;
    // The done callback should only be invoked after we're done with
    // all of the vectors passed in this call. To ensure of that, we
    // wrap it with a shared pointer that calls done when the final
    // instance is dropped.
    auto on_complete = std::make_shared<OnComplete>(std::move(done));
    for (size_t n = 0; n < count; n++) {
      if (vectors[n].len == 0 || vectors[n].base == nullptr) continue;
      auto entry = std::make_unique<Entry>(vectors[n], on_complete);
      if (tail_ == nullptr) {
        head_ = std::move(entry);
        tail_ = head_.get();
        commit_head_ = head_.get();
      } else {
        DCHECK_NULL(tail_->next);
        tail_->next = std::move(entry);
        tail_ = tail_->next.get();
        if (commit_head_ == nullptr) commit_head_ = tail_;
      }
      count_++;
      total_ += vectors[n].len;
      uncommitted_ += vectors[n].len;
      if (queued_ >= vectors[n].len) {
        queued_ -= vectors[n].len;
      } else {
        queued_ = 0;
      }
    }
  }

  Stream* stream_;
  std::shared_ptr<DataQueue> queue_;
  std::shared_ptr<DataQueue::Reader> reader_;

  bool errored_ = false;

  // True when in streaming mode (non-idempotent queue, appendable).
  bool streaming_ = false;

  // Will be set to true if the reader_ ends up providing a pull result
  // asynchronously.
  bool next_pending_ = false;

  // Will be set to true once reader_ has returned eos.
  bool eos_ = false;

  // The collection of buffers that we have pulled from reader_ and that we
  // are holding onto until they are acknowledged.
  struct Entry {
    size_t offset = 0;
    size_t ack_offset = 0;
    DataQueue::Vec buf;
    std::shared_ptr<OnComplete> on_complete;
    std::unique_ptr<Entry> next;
    Entry(DataQueue::Vec buf, std::shared_ptr<OnComplete> on_complete)
        : buf(buf), on_complete(std::move(on_complete)) {}
  };

  std::unique_ptr<Entry> head_ = nullptr;
  Entry* commit_head_ = nullptr;
  Entry* tail_ = nullptr;

  // The total number of uncommitted chunks.
  size_t count_ = 0;

  // The total number of bytes currently held in the buffer.
  size_t total_ = 0;

  // The current byte offset of buffer_ that has been confirmed to have been
  // sent. Any offset lower than this represents bytes that we are currently
  // waiting to be acknowledged. When we receive acknowledgement, we will
  // automatically free held bytes from the buffer.
  size_t uncommitted_ = 0;

  // Bytes appended to the DataQueue that haven't been pulled yet.
  // Decremented in Pull() when data moves from the queue to the buffer.
  size_t queued_ = 0;
};

// ============================================================================

#define V(name, key, no_side_effect)                                           \
  if (no_side_effect) {                                                        \
    SetProtoMethodNoSideEffect(env->isolate(), tmpl, #key, Impl::name);        \
  } else {                                                                     \
    SetProtoMethod(env->isolate(), tmpl, #key, Impl::name);                    \
  }
JS_CONSTRUCTOR_IMPL(Stream, stream_constructor_template, {
  JS_ILLEGAL_CONSTRUCTOR();
  JS_INHERIT(AsyncWrap);
  JS_CLASS(stream);
  STREAM_JS_METHODS(V)
})
#undef V

void Stream::RegisterExternalReferences(ExternalReferenceRegistry* registry) {
#define V(name, _, __) registry->Register(Impl::name);
  STREAM_JS_METHODS(V)
#undef V
}

void Stream::InitPerIsolate(IsolateData* data, Local<ObjectTemplate> target) {
  // TODO(@jasnell): Implement the per-isolate state
}

void Stream::InitPerContext(Realm* realm, Local<Object> target) {
  USE(GetConstructorTemplate(realm->env()));

#define V(name, _) IDX_STATS_STREAM_##name,
  enum StreamStatsIdx { STREAM_STATS(V) IDX_STATS_STREAM_COUNT };
#undef V

#define V(name, key, __)                                                       \
  auto IDX_STATE_STREAM_##name = offsetof(Stream::State, key);
  STREAM_STATE(V)
#undef V

#define V(name, _) NODE_DEFINE_CONSTANT(target, IDX_STATS_STREAM_##name);
  STREAM_STATS(V)
#undef V
#define V(name, _, __) NODE_DEFINE_CONSTANT(target, IDX_STATE_STREAM_##name);
  STREAM_STATE(V)
#undef V

  constexpr int QUIC_STREAM_HEADERS_KIND_HINTS =
      static_cast<uint8_t>(HeadersKind::HINTS);
  constexpr int QUIC_STREAM_HEADERS_KIND_INITIAL =
      static_cast<uint8_t>(HeadersKind::INITIAL);
  constexpr int QUIC_STREAM_HEADERS_KIND_TRAILING =
      static_cast<uint8_t>(HeadersKind::TRAILING);

  constexpr int QUIC_STREAM_HEADERS_FLAGS_NONE =
      static_cast<uint8_t>(HeadersFlags::NONE);
  constexpr int QUIC_STREAM_HEADERS_FLAGS_TERMINAL =
      static_cast<uint8_t>(HeadersFlags::TERMINAL);

  NODE_DEFINE_CONSTANT(target, QUIC_STREAM_HEADERS_KIND_HINTS);
  NODE_DEFINE_CONSTANT(target, QUIC_STREAM_HEADERS_KIND_INITIAL);
  NODE_DEFINE_CONSTANT(target, QUIC_STREAM_HEADERS_KIND_TRAILING);

  NODE_DEFINE_CONSTANT(target, QUIC_STREAM_HEADERS_FLAGS_NONE);
  NODE_DEFINE_CONSTANT(target, QUIC_STREAM_HEADERS_FLAGS_TERMINAL);
}

Stream* Stream::From(void* stream_user_data) {
  return static_cast<Stream*>(stream_user_data);
}

BaseObjectPtr<Stream> Stream::Create(Session* session,
                                     stream_id id,
                                     std::shared_ptr<DataQueue> source) {
  DCHECK_GE(id, 0);
  DCHECK_NOT_NULL(session);
  JS_NEW_INSTANCE_OR_RETURN(session->env(), obj, {});
  return MakeDetachedBaseObject<Stream>(
      BaseObjectWeakPtr<Session>(session), obj, id, std::move(source));
}

BaseObjectPtr<Stream> Stream::Create(Session* session,
                                     Direction direction,
                                     std::shared_ptr<DataQueue> source) {
  DCHECK_NOT_NULL(session);
  JS_NEW_INSTANCE_OR_RETURN(session->env(), obj, {});
  return MakeBaseObject<Stream>(
      BaseObjectWeakPtr<Session>(session), obj, direction, std::move(source));
}

Stream::Stream(BaseObjectWeakPtr<Session> session,
               Local<Object> object,
               stream_id id,
               std::shared_ptr<DataQueue> source)
    : AsyncWrap(session->env(), object, PROVIDER_QUIC_STREAM),
      stats_(env()->isolate()),
      state_(env()->isolate()),
      session_(std::move(session)),
      inbound_(DataQueue::Create()),
      headers_(env()->isolate()) {
  MakeWeak();
  DCHECK(id < kMaxStreamId);
  state_->id = id;
  state_->pending = 0;
  // Allows us to be notified when data is actually read from the
  // inbound queue so that we can update the stream flow control.
  inbound_->addBackpressureListener(this);

  JS_DEFINE_READONLY_PROPERTY(
      env(), object, env()->state_string(), state_.GetArrayBuffer());
  JS_DEFINE_READONLY_PROPERTY(
      env(), object, env()->stats_string(), stats_.GetArrayBuffer());

  set_outbound(std::move(source));

  STAT_RECORD_TIMESTAMP(Stats, created_at);
  auto params = ngtcp2_conn_get_local_transport_params(this->session());
  STAT_SET(Stats, max_offset, params->initial_max_data);
  STAT_SET(Stats, opened_at, stats_->created_at);
}

Stream::Stream(BaseObjectWeakPtr<Session> session,
               Local<Object> object,
               Direction direction,
               std::shared_ptr<DataQueue> source)
    : AsyncWrap(session->env(), object, PROVIDER_QUIC_STREAM),
      stats_(env()->isolate()),
      state_(env()->isolate()),
      session_(std::move(session)),
      inbound_(DataQueue::Create()),
      maybe_pending_stream_(
          std::make_unique<PendingStream>(direction, this, session_)),
      headers_(env()->isolate()) {
  MakeWeak();
  state_->id = kMaxStreamId;
  state_->pending = 1;

  // Allows us to be notified when data is actually read from the
  // inbound queue so that we can update the stream flow control.
  inbound_->addBackpressureListener(this);

  JS_DEFINE_READONLY_PROPERTY(
      env(), object, env()->state_string(), state_.GetArrayBuffer());
  JS_DEFINE_READONLY_PROPERTY(
      env(), object, env()->stats_string(), stats_.GetArrayBuffer());

  set_outbound(std::move(source));

  STAT_RECORD_TIMESTAMP(Stats, created_at);
  auto params = ngtcp2_conn_get_local_transport_params(this->session());
  STAT_SET(Stats, max_offset, params->initial_max_data);
}

Stream::~Stream() {
  // Make sure that Destroy() was called before Stream is actually destructed.
  DCHECK_NE(stats_->destroyed_at, 0);
}

void Stream::NotifyStreamOpened(stream_id id) {
  CHECK(is_pending());
  DCHECK(id < kMaxStreamId);
  Debug(this, "Pending stream opened with id %" PRIi64, id);
  state_->pending = 0;
  state_->id = id;
  STAT_RECORD_TIMESTAMP(Stats, opened_at);
  // Now that the stream is actually opened, add it to the sessions
  // list of known open streams.
  session().AddStream(BaseObjectPtr<Stream>(this),
                      Session::CreateStreamOption::DO_NOT_NOTIFY);

  CHECK_EQ(ngtcp2_conn_set_stream_user_data(this->session(), id, this), 0);
  maybe_pending_stream_.reset();

  if (priority_.pending) {
    session().application().SetStreamPriority(
        *this, priority_.priority, priority_.flags);
    priority_.pending = false;
  }
  if (!pending_headers_queue_.empty()) {
    if (!session().application().SupportsHeaders()) {
      // Headers were enqueued while the application was not yet known
      // (headers_supported == 0), and the negotiated application does
      // not support headers. This is a fatal mismatch.
      Destroy(QuicError::ForApplication(0));
      return;
    }
    decltype(pending_headers_queue_) queue;
    pending_headers_queue_.swap(queue);
    for (auto& headers : queue) {
      session().application().SendHeaders(
          *this,
          headers->kind,
          headers->headers.Get(env()->isolate()),
          headers->flags);
    }
  }
  // If the stream is not a local undirectional stream and is_readable is
  // false, then we should shutdown the streams readable side now.
  if (!is_local_unidirectional() && !is_readable()) {
    NotifyReadableEnded(pending_close_read_code_);
  }
  if (!is_remote_unidirectional() && !is_writable() &&
      !session_->application().stream_fin_managed_by_application()) {
    NotifyWritableEnded(pending_close_write_code_);
  }

  // Finally, if we have an outbound data source attached already, make
  // sure our stream is scheduled. This is likely a bit superfluous
  // since the stream likely hasn't had any opporunity to get blocked
  // yet, but just for completeness, let's make sure.
  if (outbound_) session().ResumeStream(id);
}

void Stream::NotifyReadableEnded(error_code code) {
  CHECK(!is_pending());
  Session::SendPendingDataScope send_scope(&session());
  CHECK_EQ(ngtcp2_conn_shutdown_stream_read(session(), 0, id(), code), 0);
}

void Stream::NotifyWritableEnded(error_code code) {
  CHECK(!is_pending());
  Session::SendPendingDataScope send_scope(&session());
  ngtcp2_conn_shutdown_stream_write(session(), 0, id(), code);
}

void Stream::EnqueuePendingHeaders(HeadersKind kind,
                                   Local<Array> headers,
                                   HeadersFlags flags) {
  Debug(this, "Enqueuing headers for pending stream");
  pending_headers_queue_.push_back(std::make_unique<PendingHeaders>(
      kind, Global<Array>(env()->isolate(), headers), flags));
}

bool Stream::is_pending() const {
  return state_->pending;
}

stream_id Stream::id() const {
  return state_->id;
}

Side Stream::origin() const {
  CHECK(!is_pending());
  return (state_->id & 0b01) ? Side::SERVER : Side::CLIENT;
}

Direction Stream::direction() const {
  if (state_->pending) {
    CHECK(maybe_pending_stream_.has_value());
    auto& val = maybe_pending_stream_.value();
    return val->direction();
  }
  return (state_->id & 0b10) ? Direction::UNIDIRECTIONAL
                             : Direction::BIDIRECTIONAL;
}

Session& Stream::session() const {
  return *session_;
}

bool Stream::is_local_unidirectional() const {
  return direction() == Direction::UNIDIRECTIONAL &&
         ngtcp2_conn_is_local_stream(*session_, id());
}

bool Stream::is_remote_unidirectional() const {
  return direction() == Direction::UNIDIRECTIONAL &&
         !ngtcp2_conn_is_local_stream(*session_, id());
}

bool Stream::is_eos() const {
  return state_->fin_sent;
}

bool Stream::wants_trailers() const {
  return state_->wants_trailers;
}

void Stream::set_early() {
  state_->received_early_data = 1;
}

bool Stream::is_writable() const {
  // Remote unidirectional streams are never writable, and remote streams can
  // never be pending.
  if (!is_pending() && direction() == Direction::UNIDIRECTIONAL &&
      !ngtcp2_conn_is_local_stream(session(), id())) {
    return false;
  }
  return state_->write_ended == 0;
}

bool Stream::has_outbound() const {
  return outbound_ != nullptr;
}

bool Stream::has_reader() const {
  return reader_ != nullptr;
}

Blob::Reader* Stream::reader() const {
  return reader_.get();
}

bool Stream::is_readable() const {
  // Local unidirectional streams are never readable, and remote streams can
  // never be pending.
  if (!is_pending() && direction() == Direction::UNIDIRECTIONAL &&
      ngtcp2_conn_is_local_stream(session(), id())) {
    return false;
  }
  return state_->read_ended == 0;
}

BaseObjectPtr<Blob::Reader> Stream::get_reader() {
  if (!is_readable() || state_->has_reader) return {};
  state_->has_reader = 1;
  auto reader = Blob::Reader::Create(env(), Blob::Create(env(), inbound_));
  reader_ = reader;
  return reader;
}

void Stream::set_final_size(uint64_t final_size) {
  DCHECK_IMPLIES(state_->fin_received == 1,
                 final_size <= STAT_GET(Stats, final_size));
  state_->fin_received = 1;
  STAT_SET(Stats, final_size, final_size);
}

void Stream::set_outbound(std::shared_ptr<DataQueue> source) {
  if (!source || !is_writable()) return;
  Debug(this, "Setting the outbound data source");
  DCHECK_NULL(outbound_);
  outbound_ = std::make_unique<Outbound>(this, std::move(source));
  state_->has_outbound = 1;
  // Note: We intentionally do NOT call ResumeStream here. During
  // construction, the stream has not yet been added to the session's
  // streams map, so FindStream would fail. The caller (CreateStream /
  // AddStream) is responsible for calling ResumeStream after the
  // stream is registered.
}

void Stream::InitStreaming() {
  auto env = this->env();
  if (outbound_ != nullptr) {
    return THROW_ERR_INVALID_STATE(
        env, "Outbound data source is already initialized");
  }
  if (!is_writable()) {
    return THROW_ERR_INVALID_STATE(env, "Stream is not writable");
  }
  Debug(this, "Initializing streaming outbound source");
  outbound_ = std::make_unique<Outbound>(this);
  state_->has_outbound = 1;
  if (!is_pending()) session_->ResumeStream(id());
}

void Stream::WriteStreamData(const FunctionCallbackInfo<Value>& args) {
  auto env = this->env();
  if (outbound_ == nullptr || !outbound_->is_streaming()) {
    return THROW_ERR_INVALID_STATE(env, "Streaming source is not initialized");
  }

  if (!is_writable()) {
    return THROW_ERR_INVALID_STATE(env, "Stream is no longer writable");
  }

  auto append_view = [&](Local<Value> value) -> bool {
    if (!value->IsUint8Array()) {
      THROW_ERR_INVALID_ARG_TYPE(env, "Expected Uint8Array");
      return false;
    }
    auto view = value.As<Uint8Array>();
    auto length = view->ByteLength();
    if (length == 0) return true;
    auto entry =
        CreateEntryFromBuffer(env, view->Buffer(), view->ByteOffset(), length);
    if (!entry) {
      return false;
    }
    return outbound_->AppendEntry(std::move(entry));
  };

  // There must always be exactly one argument to WriteStreamData.
  CHECK_EQ(args.Length(), 1);

  // The args[0] must always be an Array of Uint8Arrays
  CHECK(args[0]->IsArray());

  auto array = args[0].As<Array>();
  for (uint32_t i = 0; i < array->Length(); i++) {
    Local<Value> item;
    if (!array->Get(env->context(), i).ToLocal(&item)) return;
    if (!append_view(item)) return;
  }

  if (!is_pending()) session_->ResumeStream(id());

  UpdateWriteDesiredSize();
  args.GetReturnValue().Set(static_cast<double>(outbound_->total()));
}

void Stream::EndWriting() {
  auto env = this->env();
  if (outbound_ == nullptr || !outbound_->is_streaming()) {
    return THROW_ERR_INVALID_STATE(env, "Streaming source is not initialized");
  }

  if (!is_writable()) {
    return THROW_ERR_INVALID_STATE(env, "Stream is no longer writable");
  }
  Debug(this, "Ending streaming outbound source");
  EndWritable();
  if (!is_pending()) session_->ResumeStream(id());
}

void Stream::EntryRead(size_t amount) {
  // Called when the JS consumer reads data from the inbound DataQueue.
  // Extend the flow control window so the sender can transmit more.
  session().ExtendStreamOffset(id(), amount);
  session().ExtendOffset(amount);
}

int Stream::DoPull(bob::Next<ngtcp2_vec> next,
                   int options,
                   ngtcp2_vec* data,
                   size_t count,
                   size_t max_count_hint) {
  if (is_eos()) {
    std::move(next)(bob::Status::STATUS_EOS, nullptr, 0, [](int) {});
    return bob::Status::STATUS_EOS;
  }

  // If an outbound source has not yet been attached, block until one is
  // available. When AttachOutboundSource() is called the stream will be
  // resumed. Note that when we say "block" here we don't mean it in the
  // traditional "block the thread" sense. Instead, this will inform the
  // Session to not try to send any more data from this stream until there
  // is a source attached.
  if (outbound_ == nullptr) {
    std::move(next)(bob::Status::STATUS_BLOCK, nullptr, 0, [](size_t len) {});
    return bob::Status::STATUS_BLOCK;
  }

  return outbound_->Pull(std::move(next), options, data, count, max_count_hint);
}

void Stream::BeginHeaders(HeadersKind kind) {
  headers_length_ = 0;
  headers_.clear();
  set_headers_kind(kind);
}

void Stream::set_headers_kind(HeadersKind kind) {
  headers_kind_ = kind;
}

bool Stream::AddHeader(const Header& header) {
  size_t len = header.length();
  if (!session_->application().CanAddHeader(
          headers_.size(), headers_length_, len)) {
    return false;
  }

  headers_length_ += len;

  auto& state = BindingData::Get(env());

  const auto push = [&](auto raw) {
    Local<Value> value;
    if (!raw.ToLocal(&value)) [[unlikely]] {
      return false;
    }
    headers_.push_back(value);
    return true;
  };

  return push(header.GetName(&state)) && push(header.GetValue(&state));
}

void Stream::Acknowledge(size_t datalen) {
  if (outbound_ == nullptr) return;

  Debug(this, "Acknowledging %zu bytes", datalen);

  // ngtcp2 guarantees that offset must always be greater than the previously
  // received offset.
  STAT_INCREMENT_N(Stats, max_offset_ack, datalen);

  // Consumes the given number of bytes in the buffer.
  outbound_->Acknowledge(datalen);
  STAT_RECORD_TIMESTAMP(Stats, acked_at);
  UpdateWriteDesiredSize();
}

void Stream::Commit(size_t datalen, bool fin) {
  Debug(this, "Committing %zu bytes", datalen);
  STAT_INCREMENT_N(Stats, bytes_sent, datalen);
  if (outbound_) outbound_->Commit(datalen);
  if (fin) state_->fin_sent = 1;
}

void Stream::EndWritable() {
  if (!is_writable()) return;
  // If an outbound_ has been attached, we want to mark it as being ended.
  // If the outbound_ is wrapping an idempotent DataQueue, then capping
  // will be a non-op since we're not going to be writing any more data
  // into it anyway.
  if (outbound_) outbound_->Cap();
  state_->write_ended = 1;
}

void Stream::EndReadable(std::optional<uint64_t> maybe_final_size) {
  if (!is_readable()) return;
  state_->read_ended = 1;
  set_final_size(maybe_final_size.value_or(STAT_GET(Stats, bytes_received)));
  inbound_->cap(STAT_GET(Stats, final_size));
  // Notify the JS reader so it can see EOS. Pass fin=true so the
  // wakeup promise resolves with a value the iterator can check to
  // avoid waiting for another wakeup that will never come.
  if (reader_) reader_->NotifyPull(true);
}

void Stream::Destroy(QuicError error) {
  if (stats_->destroyed_at != 0) return;
  // Record the destroyed at timestamp before notifying the JavaScript side
  // that the stream is being destroyed.
  STAT_RECORD_TIMESTAMP(Stats, destroyed_at);

  DCHECK_NOT_NULL(session_.get());

  if (!state_->pending) {
    Debug(
        this, "Stream %" PRIi64 " being destroyed with error %s", id(), error);
  } else {
    Debug(this, "Pending stream being destroyed with error %s", error);
  }
  state_->pending = 0;

  maybe_pending_stream_.reset();

  // End the writable before marking as destroyed.
  EndWritable();

  // Also end the readable side if it isn't already.
  EndReadable();

  // We are going to release our reference to the outbound_ queue here.
  outbound_.reset();

  // We reset the inbound here also. However, it's important to note that
  // the JavaScript side could still have a reader on the inbound DataQueue,
  // which may keep that data alive a bit longer.
  inbound_->removeBackpressureListener(this);
  inbound_.reset();
  reader_.reset();

  // Notify the JavaScript side that our handle is being destroyed. The
  // JavaScript side should clean up any state that it needs to and should
  // detach itself from the handle. After this is called, it should no
  // longer be considered safe for the JavaScript side to access the
  // handle.
  EmitClose(error);

  auto session = session_;
  session_.reset();
  // EmitClose above triggers MakeCallback which can destroy the session
  // via JS re-entrancy. The weak pointer may now be null.
  if (session) session->RemoveStream(id());

  // Critically, make sure that the RemoveStream call is the last thing
  // trying to use this stream object. Once that call is made, the stream
  // object is no longer valid and should not be accessed.
  // Specifically, the session object's streams map holds the its
  // BaseObjectPtr<Stream> instances in a detached state, meaning that
  // once that BaseObjectPtr is deleted the Stream will be freed as well.
}

void Stream::ReceiveData(const uint8_t* data,
                         size_t len,
                         ReceiveDataFlags flags) {
  // If reading has ended, or there is no data, there's nothing to do but maybe
  // end the readable side if this is the last bit of data we've received.
  Debug(this, "Receiving %zu bytes of data", len);
  if (state_->read_ended == 1 || len == 0) {
    if (flags.fin) EndReadable();
    return;
  }

  if (flags.early) state_->received_early_data = 1;
  STAT_INCREMENT_N(Stats, bytes_received, len);
  STAT_SET(Stats, max_offset_received, STAT_GET(Stats, bytes_received));
  STAT_RECORD_TIMESTAMP(Stats, received_at);
  JS_TRY_ALLOCATE_BACKING(env(), backing, len)
  memcpy(backing->Data(), data, len);
  inbound_->append(DataQueue::CreateInMemoryEntryFromBackingStore(
      std::move(backing), 0, len));

  // Notify the JS reader that data is available.
  if (reader_) reader_->NotifyPull();

  if (flags.fin) EndReadable();
}

void Stream::ReceiveStopSending(QuicError error) {
  // STOP_SENDING from the peer asks us to stop sending. Per RFC 9000
  // §3.5 the receiver SHOULD respond with RESET_STREAM, which is what
  // ngtcp2_conn_shutdown_stream_write below schedules. If our
  // writable side has already been shut down (e.g. we already sent
  // RESET_STREAM ourselves or finished sending with FIN) there is
  // nothing more to do here. The previous guard checked
  // `state_->read_ended` which is unrelated to the writable side and
  // suppressed STOP_SENDING handling whenever a sibling RESET_STREAM
  // frame had been processed first within the same packet.
  if (state_->write_ended) return;
  Debug(this, "Received stop sending with error %s", error);
  ngtcp2_conn_shutdown_stream_write(session(), 0, id(), error.code());
  EndWritable();
}

void Stream::ReceiveStreamReset(uint64_t final_size, QuicError error) {
  // Importantly, reset stream only impacts the inbound data flow. It has no
  // impact on the outbound data flow. It is essentially a signal that the peer
  // has abruptly terminated the writable end of their stream with an error.
  // Any data we have received up to this point remains in the queue waiting to
  // be read.
  Debug(this,
        "Received stream reset with final size %" PRIu64 " and error %s",
        final_size,
        error);
  state_->reset_code = error.code();
  EndReadable(final_size);
  EmitReset(error);
}

// ============================================================================

void Stream::EmitBlocked() {
  // state_->wants_block will be set from the javascript side if the
  // stream object has a handler for the blocked event.
  Debug(this, "Blocked");
  if (!env()->can_call_into_js() || !state_->wants_block) {
    return;
  }
  CallbackScope<Stream> cb_scope(this);
  MakeCallback(BindingData::Get(env()).stream_blocked_callback(), 0, nullptr);
}

void Stream::EmitDrain() {
  if (!env()->can_call_into_js()) return;
  CallbackScope<Stream> cb_scope(this);
  MakeCallback(BindingData::Get(env()).stream_drain_callback(), 0, nullptr);
}

void Stream::UpdateWriteDesiredSize() {
  if (!outbound_ || !outbound_->is_streaming()) return;

  uint64_t available;
  uint64_t hwm = state_->high_water_mark;

  if (is_pending()) {
    // Pending streams don't have a stream ID yet, so ngtcp2 can't
    // report their flow control window. Use the high water mark as
    // the available capacity so writes can proceed while pending.
    available = hwm > 0 ? hwm : std::numeric_limits<uint32_t>::max();
  } else {
    // Calculate available capacity based on QUIC flow control.
    // The effective limit is the minimum of stream-level and
    // connection-level flow control remaining.
    ngtcp2_conn* conn = session();
    uint64_t stream_left = ngtcp2_conn_get_max_stream_data_left(conn, id());
    uint64_t conn_left = ngtcp2_conn_get_max_data_left(conn);
    available = std::min(stream_left, conn_left);

    // Apply the high water mark as an additional ceiling.
    if (hwm > 0) {
      available = std::min(available, hwm);
    }
  }

  // Total bytes in the pipeline: data in the DataQueue (not yet pulled by
  // ngtcp2) plus data pulled but not yet acknowledged. Using queued_bytes()
  // ensures that data appended via writeSync is accounted for in
  // backpressure even before ngtcp2 pulls it.
  uint64_t buffered = outbound_->queued_bytes();
  uint64_t desired = (available > buffered) ? (available - buffered) : 0;

  // Clamp to uint32 range since write_desired_size is uint32_t.
  uint32_t clamped = static_cast<uint32_t>(
      std::min<uint64_t>(desired, std::numeric_limits<uint32_t>::max()));

  uint32_t old_size = state_->write_desired_size;
  state_->write_desired_size = clamped;

  // Fire drain when transitioning from 0 to non-zero
  if (old_size == 0 && desired > 0) {
    EmitDrain();
  }
}

void Stream::EmitClose(const QuicError& error) {
  if (!env()->can_call_into_js()) return;
  CallbackScope<Stream> cb_scope(this);
  Local<Value> err;
  if (!error.ToV8Value(env()).ToLocal(&err)) return;
  MakeCallback(BindingData::Get(env()).stream_close_callback(), 1, &err);
}

void Stream::EmitHeaders() {
  // state_->wants_headers will be set from the javascript side if the
  // stream object has a handler for the headers event.
  if (!env()->can_call_into_js() || !state_->wants_headers) {
    return;
  }
  CallbackScope<Stream> cb_scope(this);

  Local<Value> argv[] = {
      Array::New(env()->isolate(), headers_.data(), headers_.size()),
      Integer::NewFromUnsigned(env()->isolate(),
                               static_cast<uint32_t>(headers_kind_))};

  headers_.clear();

  MakeCallback(
      BindingData::Get(env()).stream_headers_callback(), arraysize(argv), argv);
}

void Stream::EmitReset(const QuicError& error) {
  // state_->wants_reset will be set from the javascript side if the
  // stream object has a handler for the reset event.
  if (!env()->can_call_into_js() || !state_->wants_reset) {
    return;
  }
  CallbackScope<Stream> cb_scope(this);
  Local<Value> err;
  if (!error.ToV8Value(env()).ToLocal(&err)) return;

  MakeCallback(BindingData::Get(env()).stream_reset_callback(), 1, &err);
}

void Stream::EmitWantTrailers() {
  // state_->wants_trailers will be set from the javascript side if the
  // stream object has a handler for the trailers event.
  if (!env()->can_call_into_js() || !state_->wants_trailers) {
    return;
  }
  CallbackScope<Stream> cb_scope(this);
  MakeCallback(BindingData::Get(env()).stream_trailers_callback(), 0, nullptr);
}

// ============================================================================

void Stream::Schedule(Queue* queue) {
  // If this stream is not already in the queue to send data, add it.
  Debug(this, "Scheduled");
  if (outbound_ && stream_queue_.IsEmpty()) queue->PushBack(this);
}

void Stream::Unschedule() {
  // Remove this stream from the send queue. Used when the stream becomes
  // flow-control blocked so that SendPendingData does not spin retrying it.
  Debug(this, "Unscheduled");
  stream_queue_.Remove();
}

}  // namespace quic
}  // namespace node

#endif  // OPENSSL_NO_QUIC
#endif  // HAVE_OPENSSL && HAVE_QUIC
