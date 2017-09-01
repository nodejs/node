#ifndef SRC_NODE_HTTP2_CORE_INL_H_
#define SRC_NODE_HTTP2_CORE_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_http2_core.h"
#include "node_internals.h"  // arraysize
#include "freelist.h"

namespace node {
namespace http2 {

#define FREELIST_MAX 1024

#define LINKED_LIST_ADD(list, item)                                           \
  do {                                                                        \
    if (list ## _tail_ == nullptr) {                                          \
      list ## _head_ = item;                                                  \
      list ## _tail_ = item;                                                  \
    } else {                                                                  \
      list ## _tail_->next = item;                                            \
      list ## _tail_ = item;                                                  \
    }                                                                         \
  } while (0);

extern Freelist<nghttp2_data_chunk_t, FREELIST_MAX>
    data_chunk_free_list;

extern Freelist<Nghttp2Stream, FREELIST_MAX> stream_free_list;

extern Freelist<nghttp2_header_list, FREELIST_MAX> header_free_list;

extern Freelist<nghttp2_data_chunks_t, FREELIST_MAX>
    data_chunks_free_list;

#ifdef NODE_DEBUG_HTTP2
inline int Nghttp2Session::OnNghttpError(nghttp2_session* session,
                                         const char* message,
                                         size_t len,
                                         void* user_data) {
  Nghttp2Session* handle = static_cast<Nghttp2Session*>(user_data);
  DEBUG_HTTP2("Nghttp2Session %s: Error '%.*s'\n",
              handle->TypeName(), len, message);
  return 0;
}
#endif

// nghttp2 calls this at the beginning a new HEADERS or PUSH_PROMISE frame.
// We use it to ensure that an Nghttp2Stream instance is allocated to store
// the state.
inline int Nghttp2Session::OnBeginHeadersCallback(nghttp2_session* session,
                                                  const nghttp2_frame* frame,
                                                  void* user_data) {
  Nghttp2Session* handle = static_cast<Nghttp2Session*>(user_data);
  int32_t id = (frame->hd.type == NGHTTP2_PUSH_PROMISE) ?
  frame->push_promise.promised_stream_id :
  frame->hd.stream_id;
  DEBUG_HTTP2("Nghttp2Session %s: beginning headers for stream %d\n",
              handle->TypeName(), id);

  Nghttp2Stream* stream = handle->FindStream(id);
  if (stream == nullptr) {
  Nghttp2Stream::Init(id, handle, frame->headers.cat);
  } else {
  stream->StartHeaders(frame->headers.cat);
  }
  return 0;
}

// nghttp2 calls this once for every header name-value pair in a HEADERS
// or PUSH_PROMISE block. CONTINUATION frames are handled automatically
// and transparently so we do not need to worry about those at all.
inline int Nghttp2Session::OnHeaderCallback(nghttp2_session* session,
                                            const nghttp2_frame* frame,
                                            nghttp2_rcbuf *name,
                                            nghttp2_rcbuf *value,
                                            uint8_t flags,
                                            void* user_data) {
  Nghttp2Session* handle = static_cast<Nghttp2Session*>(user_data);
  int32_t id = (frame->hd.type == NGHTTP2_PUSH_PROMISE) ?
  frame->push_promise.promised_stream_id :
  frame->hd.stream_id;
  Nghttp2Stream* stream = handle->FindStream(id);
  nghttp2_header_list* header = header_free_list.pop();
  header->name = name;
  header->value = value;
  nghttp2_rcbuf_incref(name);
  nghttp2_rcbuf_incref(value);
  LINKED_LIST_ADD(stream->current_headers, header);
  return 0;
}


// When nghttp2 has completely processed a frame, it calls OnFrameReceive.
// It is our responsibility to delegate out from there. We can ignore most
// control frames since nghttp2 will handle those for us.
inline int Nghttp2Session::OnFrameReceive(nghttp2_session* session,
                                          const nghttp2_frame* frame,
                                          void* user_data) {
  Nghttp2Session* handle = static_cast<Nghttp2Session*>(user_data);
  DEBUG_HTTP2("Nghttp2Session %s: complete frame received: type: %d\n",
              handle->TypeName(), frame->hd.type);
  bool ack;
  switch (frame->hd.type) {
    case NGHTTP2_DATA:
      handle->HandleDataFrame(frame);
      break;
    case NGHTTP2_PUSH_PROMISE:
    case NGHTTP2_HEADERS:
      handle->HandleHeadersFrame(frame);
      break;
    case NGHTTP2_SETTINGS:
      ack = (frame->hd.flags & NGHTTP2_FLAG_ACK) == NGHTTP2_FLAG_ACK;
      handle->OnSettings(ack);
      break;
    case NGHTTP2_PRIORITY:
      handle->HandlePriorityFrame(frame);
      break;
    case NGHTTP2_GOAWAY:
      handle->HandleGoawayFrame(frame);
      break;
    default:
      break;
  }
  return 0;
}

inline int Nghttp2Session::OnFrameNotSent(nghttp2_session *session,
                                         const nghttp2_frame *frame,
                                         int error_code,
                                         void *user_data) {
  Nghttp2Session *handle = static_cast<Nghttp2Session *>(user_data);
  DEBUG_HTTP2("Nghttp2Session %s: frame type %d was not sent, code: %d\n",
              handle->TypeName(), frame->hd.type, error_code);
  // Do not report if the frame was not sent due to the session closing
  if (error_code != NGHTTP2_ERR_SESSION_CLOSING &&
      error_code != NGHTTP2_ERR_STREAM_CLOSED &&
      error_code != NGHTTP2_ERR_STREAM_CLOSING)
    handle->OnFrameError(frame->hd.stream_id, frame->hd.type, error_code);
  return 0;
}

inline int Nghttp2Session::OnInvalidHeader(nghttp2_session* session,
                                           const nghttp2_frame* frame,
                                           nghttp2_rcbuf* name,
                                           nghttp2_rcbuf* value,
                                           uint8_t flags,
                                           void* user_data) {
  // Ignore invalid header fields by default.
  return 0;
}

// Called when nghttp2 closes a stream, either in response to an RST_STREAM
// frame or the stream closing naturally on it's own
inline int Nghttp2Session::OnStreamClose(nghttp2_session *session,
                                         int32_t id,
                                         uint32_t code,
                                         void *user_data) {
  Nghttp2Session *handle = static_cast<Nghttp2Session *>(user_data);
  DEBUG_HTTP2("Nghttp2Session %s: stream %d closed, code: %d\n",
              handle->TypeName(), id, code);
  Nghttp2Stream *stream = handle->FindStream(id);
  // Intentionally ignore the callback if the stream does not exist
  if (stream != nullptr)
    stream->Close(code);
  return 0;
}

// Called by nghttp2 to collect the data while a file response is sent.
// The buf is the DATA frame buffer that needs to be filled with at most
// length bytes. flags is used to control what nghttp2 does next.
inline ssize_t Nghttp2Session::OnStreamReadFD(nghttp2_session *session,
                                              int32_t id,
                                              uint8_t *buf,
                                              size_t length,
                                              uint32_t *flags,
                                              nghttp2_data_source *source,
                                              void *user_data) {
  Nghttp2Session *handle = static_cast<Nghttp2Session *>(user_data);
  DEBUG_HTTP2("Nghttp2Session %s: reading outbound file data for stream %d\n",
              handle->TypeName(), id);
  Nghttp2Stream *stream = handle->FindStream(id);

  int fd = source->fd;
  int64_t offset = stream->fd_offset_;
  ssize_t numchars = 0;

  if (stream->fd_length_ >= 0 &&
      stream->fd_length_ < static_cast<int64_t>(length))
    length = stream->fd_length_;

  uv_buf_t data;
  data.base = reinterpret_cast<char *>(buf);
  data.len = length;

  uv_fs_t read_req;

  if (length > 0) {
    numchars = uv_fs_read(handle->loop_,
                          &read_req,
                          fd, &data, 1,
                          offset, nullptr);
    uv_fs_req_cleanup(&read_req);
  }

  // Close the stream with an error if reading fails
  if (numchars < 0)
    return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;

  // Update the read offset for the next read
  stream->fd_offset_ += numchars;
  stream->fd_length_ -= numchars;

  // if numchars < length, assume that we are done.
  if (static_cast<size_t>(numchars) < length || length <= 0) {
    DEBUG_HTTP2("Nghttp2Session %s: no more data for stream %d\n",
                handle->TypeName(), id);
    *flags |= NGHTTP2_DATA_FLAG_EOF;
    GetTrailers(session, handle, stream, flags);
  }

  return numchars;
}

// Called by nghttp2 to collect the data to pack within a DATA frame.
// The buf is the DATA frame buffer that needs to be filled with at most
// length bytes. flags is used to control what nghttp2 does next.
inline ssize_t Nghttp2Session::OnStreamRead(nghttp2_session *session,
                                            int32_t id,
                                            uint8_t *buf,
                                            size_t length,
                                            uint32_t *flags,
                                            nghttp2_data_source *source,
                                            void *user_data) {
  Nghttp2Session *handle = static_cast<Nghttp2Session *>(user_data);
  DEBUG_HTTP2("Nghttp2Session %s: reading outbound data for stream %d\n",
              handle->TypeName(), id);
  Nghttp2Stream *stream = handle->FindStream(id);
  size_t remaining = length;
  size_t offset = 0;

  // While there is data in the queue, copy data into buf until it is full.
  // There may be data left over, which will be sent the next time nghttp
  // calls this callback.
  while (stream->queue_head_ != nullptr) {
    DEBUG_HTTP2("Nghttp2Session %s: processing outbound data chunk\n",
                handle->TypeName());
    nghttp2_stream_write_queue *head = stream->queue_head_;
    while (stream->queue_head_index_ < head->nbufs) {
      if (remaining == 0)
        goto end;

      unsigned int n = stream->queue_head_index_;
      // len is the number of bytes in head->bufs[n] that are yet to be written
      size_t len = head->bufs[n].len - stream->queue_head_offset_;
      size_t bytes_to_write = len < remaining ? len : remaining;
      memcpy(buf + offset,
             head->bufs[n].base + stream->queue_head_offset_,
             bytes_to_write);
      offset += bytes_to_write;
      remaining -= bytes_to_write;
      if (bytes_to_write < len) {
        stream->queue_head_offset_ += bytes_to_write;
      } else {
        stream->queue_head_index_++;
        stream->queue_head_offset_ = 0;
      }
    }
    stream->queue_head_offset_ = 0;
    stream->queue_head_index_ = 0;
    stream->queue_head_ = head->next;
    head->cb(head->req, 0);
    delete head;
  }
  stream->queue_tail_ = nullptr;

end:
  // If we are no longer writable and there is no more data in the queue,
  // then we need to set the NGHTTP2_DATA_FLAG_EOF flag.
  // If we are still writable but there is not yet any data to send, set the
  // NGHTTP2_ERR_DEFERRED flag. This will put the stream into a pending state
  // that will wait for data to become available.
  // If neither of these flags are set, then nghttp2 will call this callback
  // again to get the data for the next DATA frame.
  int writable = stream->queue_head_ != nullptr || stream->IsWritable();
  if (offset == 0 && writable && stream->queue_head_ == nullptr) {
    DEBUG_HTTP2("Nghttp2Session %s: deferring stream %d\n",
                handle->TypeName(), id);
    return NGHTTP2_ERR_DEFERRED;
  }
  if (!writable) {
    DEBUG_HTTP2("Nghttp2Session %s: no more data for stream %d\n",
                handle->TypeName(), id);
    *flags |= NGHTTP2_DATA_FLAG_EOF;

    GetTrailers(session, handle, stream, flags);
  }
  CHECK(offset <= length);
  return offset;
}

// Called by nghttp2 when it needs to determine how much padding to apply
// to a DATA or HEADERS frame
inline ssize_t Nghttp2Session::OnSelectPadding(nghttp2_session *session,
                                               const nghttp2_frame *frame,
                                               size_t maxPayloadLen,
                                               void *user_data) {
  Nghttp2Session *handle = static_cast<Nghttp2Session *>(user_data);
  CHECK(handle->HasGetPaddingCallback());
  ssize_t padding = handle->GetPadding(frame->hd.length, maxPayloadLen);
  DEBUG_HTTP2("Nghttp2Session %s: using padding, size: %d\n",
              handle->TypeName(), padding);
  return padding;
}

// Called by nghttp2 multiple times while processing a DATA frame
inline int Nghttp2Session::OnDataChunkReceived(nghttp2_session *session,
                                               uint8_t flags,
                                               int32_t id,
                                               const uint8_t *data,
                                               size_t len,
                                               void *user_data) {
  Nghttp2Session *handle = static_cast<Nghttp2Session *>(user_data);
  DEBUG_HTTP2("Nghttp2Session %s: buffering data chunk for stream %d, size: "
              "%d, flags: %d\n", handle->TypeName(), id, len, flags);
  Nghttp2Stream *stream = handle->FindStream(id);
  nghttp2_data_chunk_t *chunk = data_chunk_free_list.pop();
  chunk->buf = uv_buf_init(new char[len], len);
  memcpy(chunk->buf.base, data, len);
  if (stream->data_chunks_tail_ == nullptr) {
    stream->data_chunks_head_ =
        stream->data_chunks_tail_ = chunk;
  } else {
    stream->data_chunks_tail_->next = chunk;
    stream->data_chunks_tail_ = chunk;
  }
  return 0;
}

inline void Nghttp2Session::GetTrailers(nghttp2_session *session,
                                        Nghttp2Session *handle,
                                        Nghttp2Stream *stream,
                                        uint32_t *flags) {
  if (stream->GetTrailers()) {
    // Only when we are done sending the last chunk of data do we check for
    // any trailing headers that are to be sent. This is the only opportunity
    // we have to make this check. If there are trailers, then the
    // NGHTTP2_DATA_FLAG_NO_END_STREAM flag must be set.
    SubmitTrailers submit_trailers{handle, stream, flags};
    handle->OnTrailers(stream, submit_trailers);
  }
}

inline void Nghttp2Session::SubmitTrailers::Submit(nghttp2_nv *trailers,
                                                   size_t length) const {
  if (length == 0)
    return;
  DEBUG_HTTP2("Nghttp2Session %s: sending trailers for stream %d, "
              "count: %d\n", handle_->TypeName(), stream_->id(), length);
  *flags_ |= NGHTTP2_DATA_FLAG_NO_END_STREAM;
  nghttp2_submit_trailer(handle_->session_,
                         stream_->id(),
                         trailers,
                         length);
}

// See: https://nghttp2.org/documentation/nghttp2_submit_shutdown_notice.html
inline void Nghttp2Session::SubmitShutdownNotice() {
  DEBUG_HTTP2("Nghttp2Session %s: submitting shutdown notice\n", TypeName());
  nghttp2_submit_shutdown_notice(session_);
}

// Sends a SETTINGS frame on the current session
// Note that this *should* send a SETTINGS frame even if niv == 0 and there
// are no settings entries to send.
inline int Nghttp2Session::SubmitSettings(const nghttp2_settings_entry iv[],
                                          size_t niv) {
  DEBUG_HTTP2("Nghttp2Session %s: submitting settings, count: %d\n",
              TypeName(), niv);
  return nghttp2_submit_settings(session_, NGHTTP2_FLAG_NONE, iv, niv);
}

// Returns the Nghttp2Stream associated with the given id, or nullptr if none
inline Nghttp2Stream* Nghttp2Session::FindStream(int32_t id) {
  auto s = streams_.find(id);
  if (s != streams_.end()) {
    DEBUG_HTTP2("Nghttp2Session %s: stream %d found\n",
                TypeName(), id);
    return s->second;
  } else {
    DEBUG_HTTP2("Nghttp2Session %s: stream %d not found\n",
                TypeName(), id);
    return nullptr;
  }
}

// Flushes any received queued chunks of data out to the JS layer
inline void Nghttp2Stream::FlushDataChunks(bool done) {
  while (data_chunks_head_ != nullptr) {
    DEBUG_HTTP2("Nghttp2Stream %d: emitting data chunk\n", id_);
    nghttp2_data_chunk_t* item = data_chunks_head_;
    data_chunks_head_ = item->next;
    // item will be passed to the Buffer instance and freed on gc
    session_->OnDataChunk(this, item);
  }
  data_chunks_tail_ = nullptr;
  if (done)
    session_->OnDataChunk(this, nullptr);
}

// Passes all of the the chunks for a data frame out to the JS layer
// The chunks are collected as the frame is being processed and sent out
// to the JS side only when the frame is fully processed.
inline void Nghttp2Session::HandleDataFrame(const nghttp2_frame* frame) {
  int32_t id = frame->hd.stream_id;
  DEBUG_HTTP2("Nghttp2Session %s: handling data frame for stream %d\n",
              TypeName(), id);
  Nghttp2Stream* stream = this->FindStream(id);
  // If the stream does not exist, something really bad happened
  CHECK_NE(stream, nullptr);
  bool done = (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) ==
              NGHTTP2_FLAG_END_STREAM;
  stream->FlushDataChunks(done);
}

// Passes all of the collected headers for a HEADERS frame out to the JS layer.
// The headers are collected as the frame is being processed and sent out
// to the JS side only when the frame is fully processed.
inline void Nghttp2Session::HandleHeadersFrame(const nghttp2_frame* frame) {
  int32_t id = (frame->hd.type == NGHTTP2_PUSH_PROMISE) ?
    frame->push_promise.promised_stream_id : frame->hd.stream_id;
  DEBUG_HTTP2("Nghttp2Session %s: handling headers frame for stream %d\n",
              TypeName(), id);
  Nghttp2Stream* stream = FindStream(id);
  // If the stream does not exist, something really bad happened
  CHECK_NE(stream, nullptr);
  OnHeaders(stream,
            stream->headers(),
            stream->headers_category(),
            frame->hd.flags);
  stream->FreeHeaders();
}

// Notifies the JS layer that a PRIORITY frame has been received
inline void Nghttp2Session::HandlePriorityFrame(const nghttp2_frame* frame) {
  nghttp2_priority priority_frame = frame->priority;
  int32_t id = frame->hd.stream_id;
  DEBUG_HTTP2("Nghttp2Session %s: handling priority frame for stream %d\n",
              TypeName(), id);
  // Ignore the priority frame if stream ID is <= 0
  // This actually should never happen because nghttp2 should treat this as
  // an error condition that terminates the session.
  if (id > 0) {
    nghttp2_priority_spec spec = priority_frame.pri_spec;
    OnPriority(id, spec.stream_id, spec.weight, spec.exclusive);
  }
}

// Notifies the JS layer that a GOAWAY frame has been received
inline void Nghttp2Session::HandleGoawayFrame(const nghttp2_frame* frame) {
  nghttp2_goaway goaway_frame = frame->goaway;
  DEBUG_HTTP2("Nghttp2Session %s: handling goaway frame\n", TypeName());

  OnGoAway(goaway_frame.last_stream_id,
           goaway_frame.error_code,
           goaway_frame.opaque_data,
           goaway_frame.opaque_data_len);
}

// Prompts nghttp2 to flush the queue of pending data frames
inline void Nghttp2Session::SendPendingData() {
  DEBUG_HTTP2("Nghttp2Session %s: Sending pending data\n", TypeName());
  // Do not attempt to send data on the socket if the destroying flag has
  // been set. That means everything is shutting down and the socket
  // will not be usable.
  if (IsDestroying())
    return;
  const uint8_t* data;
  ssize_t len = 0;
  size_t ncopy = 0;
  uv_buf_t buf;
  AllocateSend(SEND_BUFFER_RECOMMENDED_SIZE, &buf);
  while (nghttp2_session_want_write(session_)) {
    len = nghttp2_session_mem_send(session_, &data);
    CHECK_GE(len, 0);  // If this is less than zero, we're out of memory
    // While len is greater than 0, send a chunk
    while (len > 0) {
      ncopy = len;
      if (ncopy > buf.len)
        ncopy = buf.len;
      memcpy(buf.base, data, ncopy);
      Send(&buf, ncopy);
      len -= ncopy;
      CHECK_GE(len, 0);  // This should never be less than zero
    }
  }
}

// Initialize the Nghttp2Session handle by creating and
// assigning the Nghttp2Session instance and associated
// uv_loop_t.
inline int Nghttp2Session::Init(uv_loop_t* loop,
                               const nghttp2_session_type type,
                               nghttp2_option* options,
                               nghttp2_mem* mem) {
  DEBUG_HTTP2("Nghttp2Session %s: initializing session\n",
              SessionTypeName(type));
  loop_ = loop;
  session_type_ = type;
  destroying_ = false;
  int ret = 0;

  nghttp2_session_callbacks* callbacks
      = callback_struct_saved[HasGetPaddingCallback() ? 1 : 0].callbacks;

  nghttp2_option* opts;
  if (options != nullptr) {
    opts = options;
  } else {
    nghttp2_option_new(&opts);
  }

  switch (type) {
    case NGHTTP2_SESSION_SERVER:
      ret = nghttp2_session_server_new3(&session_,
                                        callbacks,
                                        this,
                                        opts,
                                        mem);
      break;
    case NGHTTP2_SESSION_CLIENT:
      ret = nghttp2_session_client_new3(&session_,
                                        callbacks,
                                        this,
                                        opts,
                                        mem);
      break;
  }
  if (opts != options) {
    nghttp2_option_del(opts);
  }

  // For every node::Http2Session instance, there is a uv_prep_t handle
  // whose callback is triggered on every tick of the event loop. When
  // run, nghttp2 is prompted to send any queued data it may have stored.
  uv_prepare_init(loop_, &prep_);
  uv_prepare_start(&prep_, [](uv_prepare_t* t) {
    Nghttp2Session* session = ContainerOf(&Nghttp2Session::prep_, t);
    session->SendPendingData();
  });
//  uv_unref(reinterpret_cast<uv_handle_t*>(&prep_));
  return ret;
}

inline void Nghttp2Session::MarkDestroying() {
  destroying_ = true;
}

inline int Nghttp2Session::Free() {
  CHECK(session_ != nullptr);
  DEBUG_HTTP2("Nghttp2Session %s: freeing session\n", TypeName());
  // Stop the loop
  uv_prepare_stop(&prep_);
  auto PrepClose = [](uv_handle_t* handle) {
    Nghttp2Session* session =
        ContainerOf(&Nghttp2Session::prep_,
                    reinterpret_cast<uv_prepare_t*>(handle));
    session->OnFreeSession();
  };
  uv_close(reinterpret_cast<uv_handle_t*>(&prep_), PrepClose);
  nghttp2_session_terminate_session(session_, NGHTTP2_NO_ERROR);
  nghttp2_session_del(session_);
  session_ = nullptr;
  loop_ = nullptr;
  DEBUG_HTTP2("Nghttp2Session %s: session freed\n", TypeName());
  return 1;
}

// Write data received from the socket to the underlying nghttp2_session.
inline ssize_t Nghttp2Session::Write(const uv_buf_t* bufs, unsigned int nbufs) {
  size_t total = 0;
  for (unsigned int n = 0; n < nbufs; n++) {
    ssize_t ret =
      nghttp2_session_mem_recv(session_,
                               reinterpret_cast<uint8_t*>(bufs[n].base),
                               bufs[n].len);
    if (ret < 0) {
      return ret;
    } else {
      total += ret;
    }
  }
  SendPendingData();
  return total;
}

inline void Nghttp2Session::AddStream(Nghttp2Stream* stream) {
  streams_[stream->id()] = stream;
}

// Removes a stream instance from this session
inline void Nghttp2Session::RemoveStream(int32_t id) {
  streams_.erase(id);
}

// Implementation for Nghttp2Stream functions

inline Nghttp2Stream* Nghttp2Stream::Init(
    int32_t id,
    Nghttp2Session* session,
    nghttp2_headers_category category,
    bool getTrailers) {
  DEBUG_HTTP2("Nghttp2Stream %d: initializing stream\n", id);
  Nghttp2Stream* stream = stream_free_list.pop();
  stream->ResetState(id, session, category, getTrailers);
  session->AddStream(stream);
  return stream;
}


// Resets the state of the stream instance to defaults
inline void Nghttp2Stream::ResetState(
    int32_t id,
    Nghttp2Session* session,
    nghttp2_headers_category category,
    bool getTrailers) {
  DEBUG_HTTP2("Nghttp2Stream %d: resetting stream state\n", id);
  session_ = session;
  queue_head_ = nullptr;
  queue_tail_ = nullptr;
  data_chunks_head_ = nullptr;
  data_chunks_tail_ = nullptr;
  current_headers_head_ = nullptr;
  current_headers_tail_ = nullptr;
  current_headers_category_ = category;
  flags_ = NGHTTP2_STREAM_FLAG_NONE;
  id_ = id;
  code_ = NGHTTP2_NO_ERROR;
  prev_local_window_size_ = 65535;
  queue_head_index_ = 0;
  queue_head_offset_ = 0;
  getTrailers_ = getTrailers;
}


inline void Nghttp2Stream::Destroy() {
  DEBUG_HTTP2("Nghttp2Stream %d: destroying stream\n", id_);
  // Do nothing if this stream instance is already destroyed
  if (IsDestroyed())
    return;
  flags_ |= NGHTTP2_STREAM_FLAG_DESTROYED;
  Nghttp2Session* session = this->session_;

  if (session != nullptr) {
    // Remove this stream from the associated session
    session_->RemoveStream(this->id());
    session_ = nullptr;
  }

  // Free any remaining incoming data chunks.
  while (data_chunks_head_ != nullptr) {
    nghttp2_data_chunk_t* chunk = data_chunks_head_;
    data_chunks_head_ = chunk->next;
    delete[] chunk->buf.base;
    data_chunk_free_list.push(chunk);
  }
  data_chunks_tail_ = nullptr;

  // Free any remaining outgoing data chunks.
  while (queue_head_ != nullptr) {
    nghttp2_stream_write_queue* head = queue_head_;
    queue_head_ = head->next;
    head->cb(head->req, UV_ECANCELED);
    delete head;
  }
  queue_tail_ = nullptr;

  // Free any remaining headers
  FreeHeaders();

  // Return this stream instance to the freelist
  stream_free_list.push(this);
}

inline void Nghttp2Stream::FreeHeaders() {
  DEBUG_HTTP2("Nghttp2Stream %d: freeing headers\n", id_);
  while (current_headers_head_ != nullptr) {
    DEBUG_HTTP2("Nghttp2Stream %d: freeing header item\n", id_);
    nghttp2_header_list* item = current_headers_head_;
    current_headers_head_ = item->next;
    header_free_list.push(item);
  }
  current_headers_tail_ = nullptr;
}

// Submit informational headers for a stream.
inline int Nghttp2Stream::SubmitInfo(nghttp2_nv* nva, size_t len) {
  DEBUG_HTTP2("Nghttp2Stream %d: sending informational headers, count: %d\n",
              id_, len);
  CHECK_GT(len, 0);
  return nghttp2_submit_headers(session_->session(),
                                NGHTTP2_FLAG_NONE,
                                id_, nullptr,
                                nva, len, nullptr);
}

inline int Nghttp2Stream::SubmitPriority(nghttp2_priority_spec* prispec,
                                         bool silent) {
  DEBUG_HTTP2("Nghttp2Stream %d: sending priority spec\n", id_);
  return silent ?
      nghttp2_session_change_stream_priority(session_->session(),
                                             id_, prispec) :
      nghttp2_submit_priority(session_->session(),
                              NGHTTP2_FLAG_NONE,
                              id_, prispec);
}

// Submit an RST_STREAM frame
inline int Nghttp2Stream::SubmitRstStream(const uint32_t code) {
  DEBUG_HTTP2("Nghttp2Stream %d: sending rst-stream, code: %d\n", id_, code);
  session_->SendPendingData();
  return nghttp2_submit_rst_stream(session_->session(),
                                   NGHTTP2_FLAG_NONE,
                                   id_,
                                   code);
}

// Submit a push promise.
inline int32_t Nghttp2Stream::SubmitPushPromise(
    nghttp2_nv* nva,
    size_t len,
    Nghttp2Stream** assigned,
    bool emptyPayload) {
  CHECK_GT(len, 0);
  DEBUG_HTTP2("Nghttp2Stream %d: sending push promise\n", id_);
  int32_t ret = nghttp2_submit_push_promise(session_->session(),
                                            NGHTTP2_FLAG_NONE,
                                            id_, nva, len,
                                            nullptr);
  if (ret > 0) {
    auto stream = Nghttp2Stream::Init(ret, session_);
    if (emptyPayload) stream->Shutdown();
    if (assigned != nullptr) *assigned = stream;
  }
  return ret;
}

// Initiate a response. If the nghttp2_stream is still writable by
// the time this is called, then an nghttp2_data_provider will be
// initialized, causing at least one (possibly empty) data frame to
// be sent.
inline int Nghttp2Stream::SubmitResponse(nghttp2_nv* nva,
                                         size_t len,
                                         bool emptyPayload,
                                         bool getTrailers) {
  CHECK_GT(len, 0);
  DEBUG_HTTP2("Nghttp2Stream %d: submitting response\n", id_);
  getTrailers_ = getTrailers;
  nghttp2_data_provider* provider = nullptr;
  nghttp2_data_provider prov;
  prov.source.ptr = this;
  prov.read_callback = Nghttp2Session::OnStreamRead;
  if (!emptyPayload && IsWritable())
    provider = &prov;

  return nghttp2_submit_response(session_->session(), id_,
                                 nva, len, provider);
}

// Initiate a response that contains data read from a file descriptor.
inline int Nghttp2Stream::SubmitFile(int fd,
                                     nghttp2_nv* nva, size_t len,
                                     int64_t offset,
                                     int64_t length,
                                     bool getTrailers) {
  CHECK_GT(len, 0);
  CHECK_GT(fd, 0);
  DEBUG_HTTP2("Nghttp2Stream %d: submitting file\n", id_);
  getTrailers_ = getTrailers;
  nghttp2_data_provider prov;
  prov.source.ptr = this;
  prov.source.fd = fd;
  prov.read_callback = Nghttp2Session::OnStreamReadFD;

  if (offset > 0) fd_offset_ = offset;
  if (length > -1) fd_length_ = length;

  return nghttp2_submit_response(session_->session(), id_,
                                 nva, len, &prov);
}

// Initiate a request. If writable is true (the default), then
// an nghttp2_data_provider will be initialized, causing at
// least one (possibly empty) data frame to to be sent.
inline int32_t Nghttp2Session::SubmitRequest(
    nghttp2_priority_spec* prispec,
    nghttp2_nv* nva,
    size_t len,
    Nghttp2Stream** assigned,
    bool emptyPayload,
    bool getTrailers) {
  CHECK_GT(len, 0);
  DEBUG_HTTP2("Nghttp2Session: submitting request\n");
  nghttp2_data_provider* provider = nullptr;
  nghttp2_data_provider prov;
  prov.source.ptr = this;
  prov.read_callback = OnStreamRead;
  if (!emptyPayload)
    provider = &prov;
  int32_t ret = nghttp2_submit_request(session_,
                                       prispec, nva, len,
                                       provider, nullptr);
  // Assign the Nghttp2Stream handle
  if (ret > 0) {
    Nghttp2Stream* stream = Nghttp2Stream::Init(ret, this,
                                                NGHTTP2_HCAT_HEADERS,
                                                getTrailers);
    if (emptyPayload) stream->Shutdown();
    if (assigned != nullptr) *assigned = stream;
  }
  return ret;
}

// Queue the given set of uv_but_t handles for writing to an
// nghttp2_stream. The callback will be invoked once the chunks
// of data have been flushed to the underlying nghttp2_session.
// Note that this does *not* mean that the data has been flushed
// to the socket yet.
inline int Nghttp2Stream::Write(nghttp2_stream_write_t* req,
                                const uv_buf_t bufs[],
                                unsigned int nbufs,
                                nghttp2_stream_write_cb cb) {
  if (!IsWritable()) {
    if (cb != nullptr)
      cb(req, UV_EOF);
    return 0;
  }
  DEBUG_HTTP2("Nghttp2Stream %d: queuing buffers  to send, count: %d\n",
              id_, nbufs);
  nghttp2_stream_write_queue* item = new nghttp2_stream_write_queue;
  item->cb = cb;
  item->req = req;
  item->nbufs = nbufs;
  item->bufs.AllocateSufficientStorage(nbufs);
  req->handle = this;
  req->item = item;
  memcpy(*(item->bufs), bufs, nbufs * sizeof(*bufs));

  if (queue_head_ == nullptr) {
    queue_head_ = item;
    queue_tail_ = item;
  } else {
    queue_tail_->next = item;
    queue_tail_ = item;
  }
  nghttp2_session_resume_data(session_->session(), id_);
  return 0;
}

inline void Nghttp2Stream::ReadStart() {
  // Has no effect if IsReading() is true.
  if (IsReading())
    return;
  DEBUG_HTTP2("Nghttp2Stream %d: start reading\n", id_);
  if (IsPaused()) {
    // If handle->reading is less than zero, read_start had never previously
    // been called. If handle->reading is zero, reading had started and read
    // stop had been previously called, meaning that the flow control window
    // has been explicitly set to zero. Reset the flow control window now to
    // restart the flow of data.
    nghttp2_session_set_local_window_size(session_->session(),
                                          NGHTTP2_FLAG_NONE,
                                          id_,
                                          prev_local_window_size_);
  }
  flags_ |= NGHTTP2_STREAM_FLAG_READ_START;
  flags_ &= ~NGHTTP2_STREAM_FLAG_READ_PAUSED;

  // Flush any queued data chunks immediately out to the JS layer
  FlushDataChunks();
}

inline void Nghttp2Stream::ReadStop() {
  DEBUG_HTTP2("Nghttp2Stream %d: stop reading\n", id_);
  // Has no effect if IsReading() is false, which will happen if we either
  // have not started reading yet at all (NGHTTP2_STREAM_FLAG_READ_START is not
  // set) or if we're already paused (NGHTTP2_STREAM_FLAG_READ_PAUSED is set.
  if (!IsReading())
    return;
  flags_ |= NGHTTP2_STREAM_FLAG_READ_PAUSED;

  // When not reading, explicitly set the local window size to 0 so that
  // the peer does not keep sending data that has to be buffered
  int32_t ret =
    nghttp2_session_get_stream_local_window_size(session_->session(), id_);
  if (ret >= 0)
    prev_local_window_size_ = ret;
  nghttp2_session_set_local_window_size(session_->session(),
                                        NGHTTP2_FLAG_NONE,
                                        id_, 0);
}

nghttp2_data_chunks_t::~nghttp2_data_chunks_t() {
  for (unsigned int n = 0; n < nbufs; n++) {
    free(buf[n].base);
  }
}

Nghttp2Session::Callbacks::Callbacks(bool kHasGetPaddingCallback) {
  nghttp2_session_callbacks_new(&callbacks);
  nghttp2_session_callbacks_set_on_begin_headers_callback(
    callbacks, OnBeginHeadersCallback);
  nghttp2_session_callbacks_set_on_header_callback2(
    callbacks, OnHeaderCallback);
  nghttp2_session_callbacks_set_on_frame_recv_callback(
    callbacks, OnFrameReceive);
  nghttp2_session_callbacks_set_on_stream_close_callback(
    callbacks, OnStreamClose);
  nghttp2_session_callbacks_set_on_data_chunk_recv_callback(
    callbacks, OnDataChunkReceived);
  nghttp2_session_callbacks_set_on_frame_not_send_callback(
    callbacks, OnFrameNotSent);
  nghttp2_session_callbacks_set_on_invalid_header_callback2(
    callbacks, OnInvalidHeader);

#ifdef NODE_DEBUG_HTTP2
  nghttp2_session_callbacks_set_error_callback(
    callbacks, OnNghttpError);
#endif

  if (kHasGetPaddingCallback) {
    nghttp2_session_callbacks_set_select_padding_callback(
      callbacks, OnSelectPadding);
  }
}

Nghttp2Session::Callbacks::~Callbacks() {
  nghttp2_session_callbacks_del(callbacks);
}

Nghttp2Session::SubmitTrailers::SubmitTrailers(
    Nghttp2Session* handle,
    Nghttp2Stream* stream,
    uint32_t* flags)
  : handle_(handle), stream_(stream), flags_(flags) { }

}  // namespace http2
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_HTTP2_CORE_INL_H_
