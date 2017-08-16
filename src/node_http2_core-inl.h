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

// See: https://nghttp2.org/documentation/nghttp2_submit_shutdown_notice.html
inline void Nghttp2Session::SubmitShutdownNotice() {
  DEBUG_HTTP2("Nghttp2Session %d: submitting shutdown notice\n", session_type_);
  nghttp2_submit_shutdown_notice(session_);
}

// Sends a SETTINGS frame on the current session
// Note that this *should* send a SETTINGS frame even if niv == 0 and there
// are no settings entries to send.
inline int Nghttp2Session::SubmitSettings(const nghttp2_settings_entry iv[],
                                          size_t niv) {
  DEBUG_HTTP2("Nghttp2Session %d: submitting settings, count: %d\n",
              session_type_, niv);
  return nghttp2_submit_settings(session_, NGHTTP2_FLAG_NONE, iv, niv);
}

// Returns the Nghttp2Stream associated with the given id, or nullptr if none
inline Nghttp2Stream* Nghttp2Session::FindStream(int32_t id) {
  auto s = streams_.find(id);
  if (s != streams_.end()) {
    DEBUG_HTTP2("Nghttp2Session %d: stream %d found\n", session_type_, id);
    return s->second;
  } else {
    DEBUG_HTTP2("Nghttp2Session %d: stream %d not found\n", session_type_, id);
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
  DEBUG_HTTP2("Nghttp2Session %d: handling data frame for stream %d\n",
              session_type_, id);
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
  DEBUG_HTTP2("Nghttp2Session %d: handling headers frame for stream %d\n",
              session_type_, id);
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
  DEBUG_HTTP2("Nghttp2Session %d: handling priority frame for stream %d\n",
              session_type_, id);
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
  DEBUG_HTTP2("Nghttp2Session %d: handling goaway frame\n", session_type_);

  OnGoAway(goaway_frame.last_stream_id,
           goaway_frame.error_code,
           goaway_frame.opaque_data,
           goaway_frame.opaque_data_len);
}

// Prompts nghttp2 to flush the queue of pending data frames
inline void Nghttp2Session::SendPendingData() {
  DEBUG_HTTP2("Nghttp2Session %d: Sending pending data\n", session_type_);
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
  DEBUG_HTTP2("Nghttp2Session %d: initializing session\n", type);
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
  assert(session_ != nullptr);
  DEBUG_HTTP2("Nghttp2Session %d: freeing session\n", session_type_);
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
  DEBUG_HTTP2("Nghttp2Session %d: session freed\n", session_type_);
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

  // nghttp2_session_callbacks_set_on_invalid_frame_recv(
  //   callbacks, OnInvalidFrameReceived);

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
