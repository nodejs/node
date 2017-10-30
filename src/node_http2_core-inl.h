#ifndef SRC_NODE_HTTP2_CORE_INL_H_
#define SRC_NODE_HTTP2_CORE_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_http2_core.h"
#include "node_internals.h"  // arraysize
#include "freelist.h"

namespace node {
namespace http2 {

#define FREELIST_MAX 10240

// Instances of Nghttp2Stream are created and pooled in order to speed
// allocation under load.
extern Freelist<Nghttp2Stream, FREELIST_MAX> stream_free_list;

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
  // If this is a push promise frame, we want to grab the handle of
  // the promised stream.
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
                                            nghttp2_rcbuf* name,
                                            nghttp2_rcbuf* value,
                                            uint8_t flags,
                                            void* user_data) {
  Nghttp2Session* handle = static_cast<Nghttp2Session*>(user_data);
  // If this is a push promise frame, we want to grab the handle of
  // the promised stream.
  int32_t id = (frame->hd.type == NGHTTP2_PUSH_PROMISE) ?
      frame->push_promise.promised_stream_id :
      frame->hd.stream_id;
  Nghttp2Stream* stream = handle->FindStream(id);
  // The header name and value are stored in a reference counted buffer
  // provided to us by nghttp2. We need to increment the reference counter
  // here, then decrement it when we're done using it later.
  nghttp2_rcbuf_incref(name);
  nghttp2_rcbuf_incref(value);
  nghttp2_header header;
  header.name = name;
  header.value = value;
  stream->headers()->emplace(header);
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
      // Intentional fall-through, handled just like headers frames
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

// nghttp2 will call this if an error occurs attempting to send a frame.
// Unless the stream or session is closed, this really should not happen
// unless there is a serious flaw in our implementation.
inline int Nghttp2Session::OnFrameNotSent(nghttp2_session* session,
                                         const nghttp2_frame* frame,
                                         int error_code,
                                         void* user_data) {
  Nghttp2Session* handle = static_cast<Nghttp2Session*>(user_data);
  DEBUG_HTTP2("Nghttp2Session %s: frame type %d was not sent, code: %d\n",
              handle->TypeName(), frame->hd.type, error_code);
  // Do not report if the frame was not sent due to the session closing
  if (error_code != NGHTTP2_ERR_SESSION_CLOSING &&
      error_code != NGHTTP2_ERR_STREAM_CLOSED &&
      error_code != NGHTTP2_ERR_STREAM_CLOSING) {
    handle->OnFrameError(frame->hd.stream_id,
                         frame->hd.type,
                         error_code);
  }
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
inline int Nghttp2Session::OnStreamClose(nghttp2_session* session,
                                         int32_t id,
                                         uint32_t code,
                                         void* user_data) {
  Nghttp2Session*handle = static_cast<Nghttp2Session*>(user_data);
  DEBUG_HTTP2("Nghttp2Session %s: stream %d closed, code: %d\n",
              handle->TypeName(), id, code);
  Nghttp2Stream* stream = handle->FindStream(id);
  // Intentionally ignore the callback if the stream does not exist
  if (stream != nullptr)
    stream->Close(code);
  return 0;
}

// Called by nghttp2 to collect the data while a file response is sent.
// The buf is the DATA frame buffer that needs to be filled with at most
// length bytes. flags is used to control what nghttp2 does next.
inline ssize_t Nghttp2Session::OnStreamReadFD(nghttp2_session* session,
                                              int32_t id,
                                              uint8_t* buf,
                                              size_t length,
                                              uint32_t* flags,
                                              nghttp2_data_source* source,
                                              void* user_data) {
  Nghttp2Session* handle = static_cast<Nghttp2Session*>(user_data);
  DEBUG_HTTP2("Nghttp2Session %s: reading outbound file data for stream %d\n",
              handle->TypeName(), id);
  Nghttp2Stream* stream = handle->FindStream(id);

  int fd = source->fd;
  int64_t offset = stream->fd_offset_;
  ssize_t numchars = 0;

  if (stream->fd_length_ >= 0 &&
      stream->fd_length_ < static_cast<int64_t>(length))
    length = stream->fd_length_;

  uv_buf_t data;
  data.base = reinterpret_cast<char*>(buf);
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
inline ssize_t Nghttp2Session::OnStreamRead(nghttp2_session* session,
                                            int32_t id,
                                            uint8_t* buf,
                                            size_t length,
                                            uint32_t* flags,
                                            nghttp2_data_source* source,
                                            void* user_data) {
  Nghttp2Session* handle = static_cast<Nghttp2Session*>(user_data);
  DEBUG_HTTP2("Nghttp2Session %s: reading outbound data for stream %d\n",
              handle->TypeName(), id);
  Nghttp2Stream* stream = handle->FindStream(id);
  size_t remaining = length;
  size_t offset = 0;

  // While there is data in the queue, copy data into buf until it is full.
  // There may be data left over, which will be sent the next time nghttp
  // calls this callback.
  while (!stream->queue_.empty()) {
    DEBUG_HTTP2("Nghttp2Session %s: processing outbound data chunk\n",
                handle->TypeName());
    nghttp2_stream_write* head = stream->queue_.front();
    while (stream->queue_index_ < head->nbufs) {
      if (remaining == 0)
        goto end;

      unsigned int n = stream->queue_index_;
      // len is the number of bytes in head->bufs[n] that are yet to be written
      size_t len = head->bufs[n].len - stream->queue_offset_;
      size_t bytes_to_write = len < remaining ? len : remaining;
      memcpy(buf + offset,
             head->bufs[n].base + stream->queue_offset_,
             bytes_to_write);
      offset += bytes_to_write;
      remaining -= bytes_to_write;
      if (bytes_to_write < len) {
        stream->queue_offset_ += bytes_to_write;
      } else {
        stream->queue_index_++;
        stream->queue_offset_ = 0;
      }
    }
    stream->queue_offset_ = 0;
    stream->queue_index_ = 0;
    head->cb(head->req, 0);
    delete head;
    stream->queue_.pop();
  }

end:
  // If we are no longer writable and there is no more data in the queue,
  // then we need to set the NGHTTP2_DATA_FLAG_EOF flag.
  // If we are still writable but there is not yet any data to send, set the
  // NGHTTP2_ERR_DEFERRED flag. This will put the stream into a pending state
  // that will wait for data to become available.
  // If neither of these flags are set, then nghttp2 will call this callback
  // again to get the data for the next DATA frame.
  int writable = !stream->queue_.empty() || stream->IsWritable();
  if (offset == 0 && writable && stream->queue_.empty()) {
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
#if defined(DEBUG) && DEBUG
  CHECK(offset <= length);
#endif
  return offset;
}

// Called by nghttp2 when it needs to determine how much padding to apply
// to a DATA or HEADERS frame
inline ssize_t Nghttp2Session::OnSelectPadding(nghttp2_session* session,
                                               const nghttp2_frame* frame,
                                               size_t maxPayloadLen,
                                               void* user_data) {
  Nghttp2Session* handle = static_cast<Nghttp2Session*>(user_data);
#if defined(DEBUG) && DEBUG
  CHECK(handle->HasGetPaddingCallback());
#endif
  ssize_t padding = handle->GetPadding(frame->hd.length, maxPayloadLen);
  DEBUG_HTTP2("Nghttp2Session %s: using padding, size: %d\n",
              handle->TypeName(), padding);
  return padding;
}

// While nghttp2 is processing a DATA frame, it will call the
// OnDataChunkReceived callback multiple times, passing along individual
// chunks of data from the DATA frame payload. These *must* be memcpy'd
// out because the pointer to the data will quickly become invalid.
inline int Nghttp2Session::OnDataChunkReceived(nghttp2_session* session,
                                               uint8_t flags,
                                               int32_t id,
                                               const uint8_t* data,
                                               size_t len,
                                               void* user_data) {
  Nghttp2Session* handle = static_cast<Nghttp2Session*>(user_data);
  DEBUG_HTTP2("Nghttp2Session %s: buffering data chunk for stream %d, size: "
              "%d, flags: %d\n", handle->TypeName(),
              id, len, flags);
  // We should never actually get a 0-length chunk so this check is
  // only a precaution at this point.
  if (len > 0) {
    nghttp2_session_consume_connection(session, len);
    Nghttp2Stream* stream = handle->FindStream(id);
    char* buf = Malloc<char>(len);
    memcpy(buf, data, len);
    stream->data_chunks_.emplace(uv_buf_init(buf, len));
  }
  return 0;
}

// Only when we are done sending the last chunk of data do we check for
// any trailing headers that are to be sent. This is the only opportunity
// we have to make this check. If there are trailers, then the
// NGHTTP2_DATA_FLAG_NO_END_STREAM flag must be set.
inline void Nghttp2Session::GetTrailers(nghttp2_session* session,
                                        Nghttp2Session* handle,
                                        Nghttp2Stream* stream,
                                        uint32_t* flags) {
  if (stream->GetTrailers()) {
    SubmitTrailers submit_trailers{handle, stream, flags};
    handle->OnTrailers(stream, submit_trailers);
  }
}

// Submits any trailing header fields that have been collected
inline void Nghttp2Session::SubmitTrailers::Submit(nghttp2_nv* trailers,
                                                   size_t length) const {
  if (length == 0)
    return;
  DEBUG_HTTP2("Nghttp2Session %s: sending trailers for stream %d, "
              "count: %d\n", handle_->TypeName(),
              stream_->id(), length);
  *flags_ |= NGHTTP2_DATA_FLAG_NO_END_STREAM;
  nghttp2_submit_trailer(handle_->session_,
                         stream_->id(),
                         trailers,
                         length);
}

// Submits a graceful shutdown notice to nghttp
// See: https://nghttp2.org/documentation/nghttp2_submit_shutdown_notice.html
inline void Nghttp2Session::SubmitShutdownNotice() {
  DEBUG_HTTP2("Nghttp2Session %s: submitting shutdown notice\n",
              TypeName());
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
    DEBUG_HTTP2("Nghttp2Session %s: stream %d not found\n", TypeName(), id);
    return nullptr;
  }
}

// Flushes one buffered data chunk at a time.
inline void Nghttp2Stream::FlushDataChunks() {
  if (!data_chunks_.empty()) {
    uv_buf_t buf = data_chunks_.front();
    data_chunks_.pop();
    if (buf.len > 0) {
      nghttp2_session_consume_stream(session_->session(), id_, buf.len);
      session_->OnDataChunk(this, &buf);
    } else {
      session_->OnDataChunk(this, nullptr);
    }
  }
}

// Called when a DATA frame has been completely processed. Will check to
// see if the END_STREAM flag is set, and will flush the queued data chunks
// to JS if the stream is flowing
inline void Nghttp2Session::HandleDataFrame(const nghttp2_frame* frame) {
  int32_t id = frame->hd.stream_id;
  DEBUG_HTTP2("Nghttp2Session %s: handling data frame for stream %d\n",
              TypeName(), id);
  Nghttp2Stream* stream = this->FindStream(id);
  // If the stream does not exist, something really bad happened
#if defined(DEBUG) && DEBUG
  CHECK_NE(stream, nullptr);
#endif
  if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM)
    stream->data_chunks_.emplace(uv_buf_init(0, 0));
  if (stream->IsReading())
    stream->FlushDataChunks();
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
#if defined(DEBUG) && DEBUG
  CHECK_NE(stream, nullptr);
#endif
  OnHeaders(stream,
            stream->headers(),
            stream->headers_category(),
            frame->hd.flags);
}

// Notifies the JS layer that a PRIORITY frame has been received
inline void Nghttp2Session::HandlePriorityFrame(const nghttp2_frame* frame) {
  nghttp2_priority priority_frame = frame->priority;
  int32_t id = frame->hd.stream_id;
  DEBUG_HTTP2("Nghttp2Session %s: handling priority frame for stream %d\n",
              TypeName(), id);

  // Priority frame stream ID should never be <= 0. nghttp2 handles this
  // as an error condition that terminates the session, so we should be
  // good here

#if defined(DEBUG) && DEBUG
  CHECK_GT(id, 0);
#endif

  nghttp2_priority_spec spec = priority_frame.pri_spec;
  OnPriority(id, spec.stream_id, spec.weight, spec.exclusive);
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

  uv_buf_t dest;
  AllocateSend(&dest);
  size_t destLength = 0;             // amount of data stored in dest
  size_t destRemaining = dest.len;   // amount space remaining in dest
  size_t destOffset = 0;             // current write offset of dest

  const uint8_t* src;                // pointer to the serialized data
  ssize_t srcLength = 0;             // length of serialized data chunk

  // While srcLength is greater than zero
  while ((srcLength = nghttp2_session_mem_send(session_, &src)) > 0) {
    DEBUG_HTTP2("Nghttp2Session %s: nghttp2 has %d bytes to send\n",
                TypeName(), srcLength);
    size_t srcRemaining = srcLength;
    size_t srcOffset = 0;

    // The amount of data we have to copy is greater than the space
    // remaining. Copy what we can into the remaining space, send it,
    // the proceed with the rest.
    while (srcRemaining > destRemaining) {
      DEBUG_HTTP2("Nghttp2Session %s: pushing %d bytes to the socket\n",
                  TypeName(), destLength + destRemaining);
      memcpy(dest.base + destOffset, src + srcOffset, destRemaining);
      destLength += destRemaining;
      Send(&dest, destLength);
      destOffset = 0;
      destLength = 0;
      srcRemaining -= destRemaining;
      srcOffset += destRemaining;
      destRemaining = dest.len;
    }

    if (srcRemaining > 0) {
      memcpy(dest.base + destOffset, src + srcOffset, srcRemaining);
      destLength += srcRemaining;
      destOffset += srcRemaining;
      destRemaining -= srcRemaining;
      srcRemaining = 0;
      srcOffset = 0;
    }
  }

  if (destLength > 0) {
    DEBUG_HTTP2("Nghttp2Session %s: pushing %d bytes to the socket\n",
                TypeName(), destLength);
    Send(&dest, destLength);
  }
}

// Initialize the Nghttp2Session handle by creating and
// assigning the Nghttp2Session instance and associated
// uv_loop_t.
inline int Nghttp2Session::Init(uv_loop_t* loop,
                               const nghttp2_session_type type,
                               nghttp2_option* options,
                               nghttp2_mem* mem) {
  loop_ = loop;
  session_type_ = type;
  DEBUG_HTTP2("Nghttp2Session %s: initializing session\n", TypeName());
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
  return ret;
}

inline void Nghttp2Session::MarkDestroying() {
  destroying_ = true;
}

inline int Nghttp2Session::Free() {
#if defined(DEBUG) && DEBUG
  CHECK(session_ != nullptr);
#endif
  DEBUG_HTTP2("Nghttp2Session %s: freeing session\n", TypeName());
  // Stop the loop
  CHECK_EQ(uv_prepare_stop(&prep_), 0);
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
    int options) {
  DEBUG_HTTP2("Nghttp2Stream %d: initializing stream\n", id);
  Nghttp2Stream* stream = stream_free_list.pop();
  stream->ResetState(id, session, category, options);
  session->AddStream(stream);
  return stream;
}


// Resets the state of the stream instance to defaults
inline void Nghttp2Stream::ResetState(
    int32_t id,
    Nghttp2Session* session,
    nghttp2_headers_category category,
    int options) {
  DEBUG_HTTP2("Nghttp2Stream %d: resetting stream state\n", id);
  session_ = session;
  while (!queue_.empty()) {
    nghttp2_stream_write* head = queue_.front();
    delete head;
    queue_.pop();
  }
  while (!data_chunks_.empty())
    data_chunks_.pop();
  while (!current_headers_.empty())
    current_headers_.pop();
  current_headers_category_ = category;
  flags_ = NGHTTP2_STREAM_FLAG_NONE;
  id_ = id;
  code_ = NGHTTP2_NO_ERROR;
  prev_local_window_size_ = 65535;
  queue_index_ = 0;
  queue_offset_ = 0;
  getTrailers_ = options & STREAM_OPTION_GET_TRAILERS;
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
  while (!data_chunks_.empty())
    data_chunks_.pop();

  // Free any remaining outgoing data chunks.
  while (!queue_.empty()) {
    nghttp2_stream_write* head = queue_.front();
    head->cb(head->req, UV_ECANCELED);
    delete head;
    queue_.pop();
  }

  // Free any remaining headers
  while (!current_headers_.empty())
    current_headers_.pop();

  // Return this stream instance to the freelist
  stream_free_list.push(this);
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
    int options) {
#if defined(DEBUG) && DEBUG
  CHECK_GT(len, 0);
#endif
  DEBUG_HTTP2("Nghttp2Stream %d: sending push promise\n", id_);
  int32_t ret = nghttp2_submit_push_promise(session_->session(),
                                            NGHTTP2_FLAG_NONE,
                                            id_, nva, len,
                                            nullptr);
  if (ret > 0) {
    auto stream = Nghttp2Stream::Init(ret, session_);
    if (options & STREAM_OPTION_EMPTY_PAYLOAD)
      stream->Shutdown();
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
                                         int options) {
#if defined(DEBUG) && DEBUG
  CHECK_GT(len, 0);
#endif
  DEBUG_HTTP2("Nghttp2Stream %d: submitting response\n", id_);
  getTrailers_ = options & STREAM_OPTION_GET_TRAILERS;
  nghttp2_data_provider* provider = nullptr;
  nghttp2_data_provider prov;
  prov.source.ptr = this;
  prov.read_callback = Nghttp2Session::OnStreamRead;
  if (IsWritable() && !(options & STREAM_OPTION_EMPTY_PAYLOAD))
    provider = &prov;

  return nghttp2_submit_response(session_->session(), id_,
                                 nva, len, provider);
}

// Initiate a response that contains data read from a file descriptor.
inline int Nghttp2Stream::SubmitFile(int fd,
                                     nghttp2_nv* nva, size_t len,
                                     int64_t offset,
                                     int64_t length,
                                     int options) {
#if defined(DEBUG) && DEBUG
  CHECK_GT(len, 0);
  CHECK_GT(fd, 0);
#endif
  DEBUG_HTTP2("Nghttp2Stream %d: submitting file\n", id_);
  getTrailers_ = options & STREAM_OPTION_GET_TRAILERS;
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
    int options) {
#if defined(DEBUG) && DEBUG
  CHECK_GT(len, 0);
#endif
  DEBUG_HTTP2("Nghttp2Session: submitting request\n");
  nghttp2_data_provider* provider = nullptr;
  nghttp2_data_provider prov;
  prov.source.ptr = this;
  prov.read_callback = OnStreamRead;
  if (!(options & STREAM_OPTION_EMPTY_PAYLOAD))
    provider = &prov;
  int32_t ret = nghttp2_submit_request(session_,
                                       prispec, nva, len,
                                       provider, nullptr);
  // Assign the Nghttp2Stream handle
  if (ret > 0) {
    Nghttp2Stream* stream = Nghttp2Stream::Init(ret, this,
                                                NGHTTP2_HCAT_HEADERS,
                                                options);
    if (options & STREAM_OPTION_EMPTY_PAYLOAD)
      stream->Shutdown();
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
  nghttp2_stream_write* item = new nghttp2_stream_write;
  item->cb = cb;
  item->req = req;
  item->nbufs = nbufs;
  item->bufs.AllocateSufficientStorage(nbufs);
  memcpy(*(item->bufs), bufs, nbufs * sizeof(*bufs));
  queue_.push(item);
  nghttp2_session_resume_data(session_->session(), id_);
  return 0;
}

inline void Nghttp2Stream::ReadStart() {
  if (IsReading())
    return;
  DEBUG_HTTP2("Nghttp2Stream %d: start reading\n", id_);
  flags_ |= NGHTTP2_STREAM_FLAG_READ_START;
  flags_ &= ~NGHTTP2_STREAM_FLAG_READ_PAUSED;

  // Flush any queued data chunks immediately out to the JS layer
  FlushDataChunks();
}

inline void Nghttp2Stream::ReadResume() {
  DEBUG_HTTP2("Nghttp2Stream %d: resume reading\n", id_);
  flags_ &= ~NGHTTP2_STREAM_FLAG_READ_PAUSED;

  // Flush any queued data chunks immediately out to the JS layer
  FlushDataChunks();
}

inline void Nghttp2Stream::ReadStop() {
  DEBUG_HTTP2("Nghttp2Stream %d: stop reading\n", id_);
  if (!IsReading())
    return;
  flags_ |= NGHTTP2_STREAM_FLAG_READ_PAUSED;
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
