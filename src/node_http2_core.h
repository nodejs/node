#ifndef SRC_NODE_HTTP2_CORE_H_
#define SRC_NODE_HTTP2_CORE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "util-inl.h"
#include "uv.h"
#include "nghttp2/nghttp2.h"

#include <queue>
#include <stdio.h>
#include <unordered_map>

namespace node {
namespace http2 {

#ifdef NODE_DEBUG_HTTP2

// Adapted from nghttp2 own debug printer
static inline void _debug_vfprintf(const char* fmt, va_list args) {
  vfprintf(stderr, fmt, args);
}

void inline debug_vfprintf(const char* format, ...) {
  va_list args;
  va_start(args, format);
  _debug_vfprintf(format, args);
  va_end(args);
}

#define DEBUG_HTTP2(...) debug_vfprintf(__VA_ARGS__);
#else
#define DEBUG_HTTP2(...)                                                       \
  do {                                                                         \
  } while (0)
#endif

class Nghttp2Session;
class Nghttp2Stream;

struct nghttp2_stream_write_t;

#define MAX_BUFFER_COUNT 16

enum nghttp2_session_type {
  NGHTTP2_SESSION_SERVER,
  NGHTTP2_SESSION_CLIENT
};

enum nghttp2_shutdown_flags {
  NGHTTP2_SHUTDOWN_FLAG_GRACEFUL
};

enum nghttp2_stream_flags {
  NGHTTP2_STREAM_FLAG_NONE = 0x0,
  // Writable side has ended
  NGHTTP2_STREAM_FLAG_SHUT = 0x1,
  // Reading has started
  NGHTTP2_STREAM_FLAG_READ_START = 0x2,
  // Reading is paused
  NGHTTP2_STREAM_FLAG_READ_PAUSED = 0x4,
  // Stream is closed
  NGHTTP2_STREAM_FLAG_CLOSED = 0x8,
  // Stream is destroyed
  NGHTTP2_STREAM_FLAG_DESTROYED = 0x10
};

enum nghttp2_stream_options {
  STREAM_OPTION_EMPTY_PAYLOAD = 0x1,
  STREAM_OPTION_GET_TRAILERS = 0x2,
};

// Callbacks
typedef void (*nghttp2_stream_write_cb)(
    nghttp2_stream_write_t* req,
    int status);

struct nghttp2_stream_write {
  unsigned int nbufs = 0;
  nghttp2_stream_write_t* req = nullptr;
  nghttp2_stream_write_cb cb = nullptr;
  MaybeStackBuffer<uv_buf_t, MAX_BUFFER_COUNT> bufs;
};

struct nghttp2_header {
  nghttp2_rcbuf* name = nullptr;
  nghttp2_rcbuf* value = nullptr;
};

// Handle Types
class Nghttp2Session {
 public:
  // Initializes the session instance
  inline int Init(
      uv_loop_t*,
      const nghttp2_session_type type = NGHTTP2_SESSION_SERVER,
      nghttp2_option* options = nullptr,
      nghttp2_mem* mem = nullptr);

  // Frees this session instance
  inline int Free();
  inline void MarkDestroying();
  bool IsDestroying() {
    return destroying_;
  }

  inline const char* TypeName() {
    switch (session_type_) {
      case NGHTTP2_SESSION_SERVER: return "server";
      case NGHTTP2_SESSION_CLIENT: return "client";
      default:
        // This should never happen
        ABORT();
    }
  }

  // Returns the pointer to the identified stream, or nullptr if
  // the stream does not exist
  inline Nghttp2Stream* FindStream(int32_t id);

  // Submits a new request. If the request is a success, assigned
  // will be a pointer to the Nghttp2Stream instance assigned.
  // This only works if the session is a client session.
  inline int32_t SubmitRequest(
      nghttp2_priority_spec* prispec,
      nghttp2_nv* nva,
      size_t len,
      Nghttp2Stream** assigned = nullptr,
      int options = 0);

  // Submits a notice to the connected peer that the session is in the
  // process of shutting down.
  inline void SubmitShutdownNotice();

  // Submits a SETTINGS frame to the connected peer.
  inline int SubmitSettings(const nghttp2_settings_entry iv[], size_t niv);

  // Write data to the session
  inline ssize_t Write(const uv_buf_t* bufs, unsigned int nbufs);

  // Returns the nghttp2 library session
  inline nghttp2_session* session() const { return session_; }

  nghttp2_session_type type() const {
    return session_type_;
  }

 protected:
  // Adds a stream instance to this session
  inline void AddStream(Nghttp2Stream* stream);

  // Removes a stream instance from this session
  inline void RemoveStream(int32_t id);

  virtual void Send(uv_buf_t* buf, size_t length) {}
  virtual void OnHeaders(
      Nghttp2Stream* stream,
      std::queue<nghttp2_header>* headers,
      nghttp2_headers_category cat,
      uint8_t flags) {}
  virtual void OnStreamClose(int32_t id, uint32_t code) {}
  virtual void OnDataChunk(Nghttp2Stream* stream,
                           uv_buf_t* chunk) {}
  virtual void OnSettings(bool ack) {}
  virtual void OnPriority(int32_t id,
                          int32_t parent,
                          int32_t weight,
                          int8_t exclusive) {}
  virtual void OnGoAway(int32_t lastStreamID,
                        uint32_t errorCode,
                        uint8_t* data,
                        size_t length) {}
  virtual void OnFrameError(int32_t id,
                            uint8_t type,
                            int error_code) {}
  virtual ssize_t GetPadding(size_t frameLength,
                             size_t maxFrameLength) { return 0; }
  virtual void OnFreeSession() {}
  virtual void AllocateSend(uv_buf_t* buf) = 0;

  virtual bool HasGetPaddingCallback() { return false; }

  class SubmitTrailers {
   public:
    inline void Submit(nghttp2_nv* trailers, size_t length) const;

   private:
    inline SubmitTrailers(Nghttp2Session* handle,
                          Nghttp2Stream* stream,
                          uint32_t* flags);

    Nghttp2Session* const handle_;
    Nghttp2Stream* const stream_;
    uint32_t* const flags_;

    friend class Nghttp2Session;
  };

  virtual void OnTrailers(Nghttp2Stream* stream,
                          const SubmitTrailers& submit_trailers) {}

 private:
  inline void SendPendingData();
  inline void HandleHeadersFrame(const nghttp2_frame* frame);
  inline void HandlePriorityFrame(const nghttp2_frame* frame);
  inline void HandleDataFrame(const nghttp2_frame* frame);
  inline void HandleGoawayFrame(const nghttp2_frame* frame);

  static inline void GetTrailers(nghttp2_session* session,
                                 Nghttp2Session* handle,
                                 Nghttp2Stream* stream,
                                 uint32_t* flags);

  /* callbacks for nghttp2 */
#ifdef NODE_DEBUG_HTTP2
  static inline int OnNghttpError(nghttp2_session* session,
                                  const char* message,
                                  size_t len,
                                  void* user_data);
#endif

  static inline int OnBeginHeadersCallback(nghttp2_session* session,
                                           const nghttp2_frame* frame,
                                           void* user_data);
  static inline int OnHeaderCallback(nghttp2_session* session,
                                     const nghttp2_frame* frame,
                                     nghttp2_rcbuf* name,
                                     nghttp2_rcbuf* value,
                                     uint8_t flags,
                                     void* user_data);
  static inline int OnFrameReceive(nghttp2_session* session,
                                   const nghttp2_frame* frame,
                                   void* user_data);
  static inline int OnFrameNotSent(nghttp2_session* session,
                                   const nghttp2_frame* frame,
                                   int error_code,
                                   void* user_data);
  static inline int OnStreamClose(nghttp2_session* session,
                                  int32_t id,
                                  uint32_t code,
                                  void* user_data);
  static inline int OnInvalidHeader(nghttp2_session* session,
                                    const nghttp2_frame* frame,
                                    nghttp2_rcbuf* name,
                                    nghttp2_rcbuf* value,
                                    uint8_t flags,
                                    void* user_data);
  static inline int OnDataChunkReceived(nghttp2_session* session,
                                        uint8_t flags,
                                        int32_t id,
                                        const uint8_t* data,
                                        size_t len,
                                        void* user_data);
  static inline ssize_t OnStreamReadFD(nghttp2_session* session,
                                       int32_t id,
                                       uint8_t* buf,
                                       size_t length,
                                       uint32_t* flags,
                                       nghttp2_data_source* source,
                                       void* user_data);
  static inline ssize_t OnStreamRead(nghttp2_session* session,
                                     int32_t id,
                                     uint8_t* buf,
                                     size_t length,
                                     uint32_t* flags,
                                     nghttp2_data_source* source,
                                     void* user_data);
  static inline ssize_t OnSelectPadding(nghttp2_session* session,
                                        const nghttp2_frame* frame,
                                        size_t maxPayloadLen,
                                        void* user_data);

  struct Callbacks {
    inline explicit Callbacks(bool kHasGetPaddingCallback);
    inline ~Callbacks();

    nghttp2_session_callbacks* callbacks;
  };

  /* Use callback_struct_saved[kHasGetPaddingCallback ? 1 : 0] */
  static Callbacks callback_struct_saved[2];

  nghttp2_session* session_;
  uv_loop_t* loop_;
  uv_prepare_t prep_;
  nghttp2_session_type session_type_;
  std::unordered_map<int32_t, Nghttp2Stream*> streams_;
  bool destroying_ = false;

  friend class Nghttp2Stream;
};



class Nghttp2Stream {
 public:
  static inline Nghttp2Stream* Init(
      int32_t id,
      Nghttp2Session* session,
      nghttp2_headers_category category = NGHTTP2_HCAT_HEADERS,
      int options = 0);

  inline ~Nghttp2Stream() {
#if defined(DEBUG) && DEBUG
    CHECK_EQ(session_, nullptr);
#endif
    DEBUG_HTTP2("Nghttp2Stream %d: freed\n", id_);
  }

  inline void FlushDataChunks();

  // Resets the state of the stream instance to defaults
  inline void ResetState(
      int32_t id,
      Nghttp2Session* session,
      nghttp2_headers_category category = NGHTTP2_HCAT_HEADERS,
      int options = 0);

  // Destroy this stream instance and free all held memory.
  // Note that this will free queued outbound and inbound
  // data chunks and inbound headers, so it's important not
  // to call this until those are fully consumed.
  //
  // Also note: this does not actually destroy the instance.
  // instead, it frees the held memory, removes the stream
  // from the parent session, and returns the instance to
  // the FreeList so that it can be reused.
  inline void Destroy();

  // Returns true if this stream has been destroyed
  inline bool IsDestroyed() const {
    return flags_ & NGHTTP2_STREAM_FLAG_DESTROYED;
  }

  // Queue outbound chunks of data to be sent on this stream
  inline int Write(
      nghttp2_stream_write_t* req,
      const uv_buf_t bufs[],
      unsigned int nbufs,
      nghttp2_stream_write_cb cb);

  // Initiate a response on this stream.
  inline int SubmitResponse(nghttp2_nv* nva,
                            size_t len,
                            int options);

  // Send data read from a file descriptor as the response on this stream.
  inline int SubmitFile(int fd,
                        nghttp2_nv* nva, size_t len,
                        int64_t offset,
                        int64_t length,
                        int options);

  // Submit informational headers for this stream
  inline int SubmitInfo(nghttp2_nv* nva, size_t len);

  // Submit a PRIORITY frame for this stream
  inline int SubmitPriority(nghttp2_priority_spec* prispec,
                            bool silent = false);

  // Submits an RST_STREAM frame using the given code
  inline int SubmitRstStream(const uint32_t code);

  // Submits a PUSH_PROMISE frame with this stream as the parent.
  inline int SubmitPushPromise(
      nghttp2_nv* nva,
      size_t len,
      Nghttp2Stream** assigned = nullptr,
      int options = 0);

  // Marks the Writable side of the stream as being shutdown
  inline void Shutdown() {
    flags_ |= NGHTTP2_STREAM_FLAG_SHUT;
    nghttp2_session_resume_data(session_->session(), id_);
  }

  // Returns true if this stream is writable.
  inline bool IsWritable() const {
    return !(flags_ & NGHTTP2_STREAM_FLAG_SHUT);
  }

  // Start Reading. If there are queued data chunks, they are pushed into
  // the session to be emitted at the JS side
  inline void ReadStart();

  // Resume Reading
  inline void ReadResume();

  // Stop/Pause Reading.
  inline void ReadStop();

  // Returns true if reading is paused
  inline bool IsPaused() const {
    return flags_ & NGHTTP2_STREAM_FLAG_READ_PAUSED;
  }

  inline bool GetTrailers() const {
    return getTrailers_;
  }

  // Returns true if this stream is in the reading state, which occurs when
  // the NGHTTP2_STREAM_FLAG_READ_START flag has been set and the
  // NGHTTP2_STREAM_FLAG_READ_PAUSED flag is *not* set.
  inline bool IsReading() const {
    return flags_ & NGHTTP2_STREAM_FLAG_READ_START &&
           !(flags_ & NGHTTP2_STREAM_FLAG_READ_PAUSED);
  }

  inline void Close(int32_t code) {
    DEBUG_HTTP2("Nghttp2Stream %d: closing with code %d\n", id_, code);
    flags_ |= NGHTTP2_STREAM_FLAG_CLOSED;
    code_ = code;
    session_->OnStreamClose(id_, code);
    DEBUG_HTTP2("Nghttp2Stream %d: closed\n", id_);
  }

  // Returns true if this stream has been closed either by receiving or
  // sending an RST_STREAM frame.
  inline bool IsClosed() const {
    return flags_ & NGHTTP2_STREAM_FLAG_CLOSED;
  }

  // Returns the RST_STREAM code used to close this stream
  inline int32_t code() const {
    return code_;
  }

  // Returns the stream identifier for this stream
  inline int32_t id() const {
    return id_;
  }

  inline std::queue<nghttp2_header>* headers() {
    return &current_headers_;
  }

  inline nghttp2_headers_category headers_category() const {
    return current_headers_category_;
  }

  void StartHeaders(nghttp2_headers_category category) {
    DEBUG_HTTP2("Nghttp2Stream %d: starting headers, category: %d\n",
                id_, category);
    // We shouldn't be in the middle of a headers block already.
    // Something bad happened if this fails
#if defined(DEBUG) && DEBUG
    CHECK(current_headers_.empty());
#endif
    current_headers_category_ = category;
  }

 private:
  // The Parent HTTP/2 Session
  Nghttp2Session* session_ = nullptr;

  // The Stream Identifier
  int32_t id_ = 0;

  // Internal state flags
  int flags_ = 0;

  // Outbound Data... This is the data written by the JS layer that is
  // waiting to be written out to the socket.
  std::queue<nghttp2_stream_write*> queue_;
  unsigned int queue_index_ = 0;
  size_t queue_offset_ = 0;
  int64_t fd_offset_ = 0;
  int64_t fd_length_ = -1;

  // The Current Headers block... As headers are received for this stream,
  // they are temporarily stored here until the OnFrameReceived is called
  // signalling the end of the HEADERS frame
  nghttp2_headers_category current_headers_category_ = NGHTTP2_HCAT_HEADERS;
  std::queue<nghttp2_header> current_headers_;

  // Inbound Data... This is the data received via DATA frames for this stream.
  std::queue<uv_buf_t> data_chunks_;

  // The RST_STREAM code used to close this stream
  int32_t code_ = NGHTTP2_NO_ERROR;

  int32_t prev_local_window_size_ = 65535;

  // True if this stream will have outbound trailers
  bool getTrailers_ = false;

  friend class Nghttp2Session;
};

struct nghttp2_stream_write_t {
  void* data;
  int status;
};

}  // namespace http2
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_HTTP2_CORE_H_
