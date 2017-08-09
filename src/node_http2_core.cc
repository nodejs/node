#include "node_http2_core-inl.h"

namespace node {
namespace http2 {

#ifdef NODE_DEBUG_HTTP2
int Nghttp2Session::OnNghttpError(nghttp2_session* session,
                                  const char* message,
                                  size_t len,
                                  void* user_data) {
  Nghttp2Session* handle = static_cast<Nghttp2Session*>(user_data);
  DEBUG_HTTP2("Nghttp2Session %d: Error '%.*s'\n",
              handle->session_type_, len, message);
  return 0;
}
#endif

// nghttp2 calls this at the beginning a new HEADERS or PUSH_PROMISE frame.
// We use it to ensure that an Nghttp2Stream instance is allocated to store
// the state.
int Nghttp2Session::OnBeginHeadersCallback(nghttp2_session* session,
                                           const nghttp2_frame* frame,
                                           void* user_data) {
  Nghttp2Session* handle = static_cast<Nghttp2Session*>(user_data);
  int32_t id = (frame->hd.type == NGHTTP2_PUSH_PROMISE) ?
    frame->push_promise.promised_stream_id :
    frame->hd.stream_id;
  DEBUG_HTTP2("Nghttp2Session %d: beginning headers for stream %d\n",
              handle->session_type_, id);

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
int Nghttp2Session::OnHeaderCallback(nghttp2_session* session,
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
int Nghttp2Session::OnFrameReceive(nghttp2_session* session,
                                   const nghttp2_frame* frame,
                                   void* user_data) {
  Nghttp2Session* handle = static_cast<Nghttp2Session*>(user_data);
  DEBUG_HTTP2("Nghttp2Session %d: complete frame received: type: %d\n",
              handle->session_type_, frame->hd.type);
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

int Nghttp2Session::OnFrameNotSent(nghttp2_session* session,
                                   const nghttp2_frame* frame,
                                   int error_code,
                                   void* user_data) {
  Nghttp2Session* handle = static_cast<Nghttp2Session*>(user_data);
  DEBUG_HTTP2("Nghttp2Session %d: frame type %d was not sent, code: %d\n",
              handle->session_type_, frame->hd.type, error_code);
  // Do not report if the frame was not sent due to the session closing
  if (error_code != NGHTTP2_ERR_SESSION_CLOSING &&
      error_code != NGHTTP2_ERR_STREAM_CLOSED &&
      error_code != NGHTTP2_ERR_STREAM_CLOSING)
    handle->OnFrameError(frame->hd.stream_id, frame->hd.type, error_code);
  return 0;
}

// Called when nghttp2 closes a stream, either in response to an RST_STREAM
// frame or the stream closing naturally on it's own
int Nghttp2Session::OnStreamClose(nghttp2_session *session,
                                  int32_t id,
                                  uint32_t code,
                                  void *user_data) {
  Nghttp2Session* handle = static_cast<Nghttp2Session*>(user_data);
  DEBUG_HTTP2("Nghttp2Session %d: stream %d closed, code: %d\n",
              handle->session_type_, id, code);
  Nghttp2Stream* stream = handle->FindStream(id);
  // Intentionally ignore the callback if the stream does not exist
  if (stream != nullptr)
    stream->Close(code);
  return 0;
}

// Called by nghttp2 multiple times while processing a DATA frame
int Nghttp2Session::OnDataChunkReceived(nghttp2_session *session,
                                        uint8_t flags,
                                        int32_t id,
                                        const uint8_t *data,
                                        size_t len,
                                        void *user_data) {
  Nghttp2Session* handle = static_cast<Nghttp2Session*>(user_data);
  DEBUG_HTTP2("Nghttp2Session %d: buffering data chunk for stream %d, size: "
              "%d, flags: %d\n", handle->session_type_, id, len, flags);
  Nghttp2Stream* stream = handle->FindStream(id);
  nghttp2_data_chunk_t* chunk = data_chunk_free_list.pop();
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

// Called by nghttp2 when it needs to determine how much padding to apply
// to a DATA or HEADERS frame
ssize_t Nghttp2Session::OnSelectPadding(nghttp2_session* session,
                                        const nghttp2_frame* frame,
                                        size_t maxPayloadLen,
                                        void* user_data) {
  Nghttp2Session* handle = static_cast<Nghttp2Session*>(user_data);
  assert(handle->HasGetPaddingCallback());
  ssize_t padding = handle->GetPadding(frame->hd.length, maxPayloadLen);
    DEBUG_HTTP2("Nghttp2Session %d: using padding, size: %d\n",
                handle->session_type_, padding);
  return padding;
}

void Nghttp2Session::GetTrailers(nghttp2_session* session,
                                 Nghttp2Session* handle,
                                 Nghttp2Stream* stream,
                                 uint32_t* flags) {
  if (stream->GetTrailers()) {
    // Only when we are done sending the last chunk of data do we check for
    // any trailing headers that are to be sent. This is the only opportunity
    // we have to make this check. If there are trailers, then the
    // NGHTTP2_DATA_FLAG_NO_END_STREAM flag must be set.
    SubmitTrailers submit_trailers { handle, stream, flags };
    handle->OnTrailers(stream, submit_trailers);
  }
}

void Nghttp2Session::SubmitTrailers::Submit(nghttp2_nv* trailers,
                                            size_t length) const {
  if (length == 0) return;
  DEBUG_HTTP2("Nghttp2Session %d: sending trailers for stream %d, "
              "count: %d\n", handle_->session_type_, stream_->id(),
              length);
  *flags_ |= NGHTTP2_DATA_FLAG_NO_END_STREAM;
  nghttp2_submit_trailer(handle_->session_,
                         stream_->id(),
                         trailers,
                         length);
}

// Called by nghttp2 to collect the data while a file response is sent.
// The buf is the DATA frame buffer that needs to be filled with at most
// length bytes. flags is used to control what nghttp2 does next.
ssize_t Nghttp2Session::OnStreamReadFD(nghttp2_session* session,
                                       int32_t id,
                                       uint8_t* buf,
                                       size_t length,
                                       uint32_t* flags,
                                       nghttp2_data_source* source,
                                       void* user_data) {
  Nghttp2Session* handle = static_cast<Nghttp2Session*>(user_data);
  DEBUG_HTTP2("Nghttp2Session %d: reading outbound file data for stream %d\n",
              handle->session_type_, id);
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
    DEBUG_HTTP2("Nghttp2Session %d: no more data for stream %d\n",
                handle->session_type_, id);
    *flags |= NGHTTP2_DATA_FLAG_EOF;
    GetTrailers(session, handle, stream, flags);
  }

  return numchars;
}

// Called by nghttp2 to collect the data to pack within a DATA frame.
// The buf is the DATA frame buffer that needs to be filled with at most
// length bytes. flags is used to control what nghttp2 does next.
ssize_t Nghttp2Session::OnStreamRead(nghttp2_session* session,
                                     int32_t id,
                                     uint8_t* buf,
                                     size_t length,
                                     uint32_t* flags,
                                     nghttp2_data_source* source,
                                     void* user_data) {
  Nghttp2Session* handle = static_cast<Nghttp2Session*>(user_data);
  DEBUG_HTTP2("Nghttp2Session %d: reading outbound data for stream %d\n",
              handle->session_type_, id);
  Nghttp2Stream* stream = handle->FindStream(id);
  size_t remaining = length;
  size_t offset = 0;

  // While there is data in the queue, copy data into buf until it is full.
  // There may be data left over, which will be sent the next time nghttp
  // calls this callback.
  while (stream->queue_head_ != nullptr) {
    DEBUG_HTTP2("Nghttp2Session %d: processing outbound data chunk\n",
                handle->session_type_);
    nghttp2_stream_write_queue* head = stream->queue_head_;
    while (stream->queue_head_index_ < head->nbufs) {
      if (remaining == 0) {
        goto end;
      }

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
    DEBUG_HTTP2("Nghttp2Session %d: deferring stream %d\n",
                handle->session_type_, id);
    return NGHTTP2_ERR_DEFERRED;
  }
  if (!writable) {
    DEBUG_HTTP2("Nghttp2Session %d: no more data for stream %d\n",
                handle->session_type_, id);
    *flags |= NGHTTP2_DATA_FLAG_EOF;

    GetTrailers(session, handle, stream, flags);
  }
  assert(offset <= length);
  return offset;
}

Freelist<nghttp2_data_chunk_t, FREELIST_MAX>
    data_chunk_free_list;

Freelist<Nghttp2Stream, FREELIST_MAX> stream_free_list;

Freelist<nghttp2_header_list, FREELIST_MAX> header_free_list;

Freelist<nghttp2_data_chunks_t, FREELIST_MAX>
    data_chunks_free_list;

Nghttp2Session::Callbacks Nghttp2Session::callback_struct_saved[2] = {
  Callbacks(false),
  Callbacks(true)
};

}  // namespace http2
}  // namespace node
