#ifndef SRC_NODE_HTTP2_H_
#define SRC_NODE_HTTP2_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

// FIXME(joyeecheung): nghttp2.h needs stdint.h to compile on Windows
#include <cstdint>
#include "nghttp2/nghttp2.h"

#include "node_http2_state.h"
#include "node_http_common.h"
#include "node_mem.h"
#include "node_perf.h"
#include "stream_base.h"
#include "string_bytes.h"

#include <algorithm>
#include <queue>

namespace node {
namespace http2 {

// We strictly limit the number of outstanding unacknowledged PINGS a user
// may send in order to prevent abuse. The current default cap is 10. The
// user may set a different limit using a per Http2Session configuration
// option.
constexpr size_t kDefaultMaxPings = 10;

// Also strictly limit the number of outstanding SETTINGS frames a user sends
constexpr size_t kDefaultMaxSettings = 10;

// Default maximum total memory cap for Http2Session.
constexpr uint64_t kDefaultMaxSessionMemory = 10000000;

// These are the standard HTTP/2 defaults as specified by the RFC
#define DEFAULT_SETTINGS_HEADER_TABLE_SIZE 4096
#define DEFAULT_SETTINGS_ENABLE_PUSH 1
#define DEFAULT_SETTINGS_MAX_CONCURRENT_STREAMS 0xffffffffu
#define DEFAULT_SETTINGS_INITIAL_WINDOW_SIZE 65535
#define DEFAULT_SETTINGS_MAX_FRAME_SIZE 16384
#define DEFAULT_SETTINGS_MAX_HEADER_LIST_SIZE 65535
#define DEFAULT_SETTINGS_ENABLE_CONNECT_PROTOCOL 0
#define MAX_MAX_FRAME_SIZE 16777215
#define MIN_MAX_FRAME_SIZE DEFAULT_SETTINGS_MAX_FRAME_SIZE
#define MAX_INITIAL_WINDOW_SIZE 2147483647

template <typename T, void(*fn)(T*)>
struct Nghttp2Deleter {
  void operator()(T* ptr) const noexcept { fn(ptr); }
};

using Nghttp2OptionPointer =
    std::unique_ptr<nghttp2_option,
                    Nghttp2Deleter<nghttp2_option, nghttp2_option_del>>;

using Nghttp2SessionPointer =
    std::unique_ptr<nghttp2_session,
                    Nghttp2Deleter<nghttp2_session, nghttp2_session_del>>;

using Nghttp2SessionCallbacksPointer =
    std::unique_ptr<nghttp2_session_callbacks,
                    Nghttp2Deleter<nghttp2_session_callbacks,
                                   nghttp2_session_callbacks_del>>;

struct Http2HeadersTraits {
  typedef nghttp2_nv nv_t;
  static const uint8_t kNoneFlag = NGHTTP2_NV_FLAG_NONE;
};

struct Http2RcBufferPointerTraits {
  typedef nghttp2_rcbuf rcbuf_t;
  typedef nghttp2_vec vector_t;

  static void inc(rcbuf_t* buf) {
    CHECK_NOT_NULL(buf);
    nghttp2_rcbuf_incref(buf);
  }
  static void dec(rcbuf_t* buf) {
    CHECK_NOT_NULL(buf);
    nghttp2_rcbuf_decref(buf);
  }
  static vector_t get_vec(rcbuf_t* buf) {
    CHECK_NOT_NULL(buf);
    return nghttp2_rcbuf_get_buf(buf);
  }
  static bool is_static(const rcbuf_t* buf) {
    CHECK_NOT_NULL(buf);
    return nghttp2_rcbuf_is_static(buf);
  }
};

using Http2Headers = NgHeaders<Http2HeadersTraits>;
using Http2RcBufferPointer = NgRcBufPointer<Http2RcBufferPointerTraits>;


enum nghttp2_session_type {
  NGHTTP2_SESSION_SERVER,
  NGHTTP2_SESSION_CLIENT
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
  NGHTTP2_STREAM_FLAG_DESTROYED = 0x10,
  // Stream has trailers
  NGHTTP2_STREAM_FLAG_TRAILERS = 0x20,
  // Stream has received all the data it can
  NGHTTP2_STREAM_FLAG_EOS = 0x40
};

enum nghttp2_stream_options {
  // Stream is not going to have any DATA frames
  STREAM_OPTION_EMPTY_PAYLOAD = 0x1,
  // Stream might have trailing headers
  STREAM_OPTION_GET_TRAILERS = 0x2,
};

struct NgHttp2StreamWrite : public MemoryRetainer {
  WriteWrap* req_wrap = nullptr;
  uv_buf_t buf;

  inline explicit NgHttp2StreamWrite(uv_buf_t buf_) : buf(buf_) {}
  inline NgHttp2StreamWrite(WriteWrap* req, uv_buf_t buf_) :
      req_wrap(req), buf(buf_) {}

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(NgHttp2StreamWrite)
  SET_SELF_SIZE(NgHttp2StreamWrite)
};

// The Padding Strategy determines the method by which extra padding is
// selected for HEADERS and DATA frames. These are configurable via the
// options passed in to a Http2Session object.
enum padding_strategy_type {
  // No padding strategy. This is the default.
  PADDING_STRATEGY_NONE,
  // Attempts to ensure that the frame is 8-byte aligned
  PADDING_STRATEGY_ALIGNED,
  // Padding will ensure all data frames are maxFrameSize
  PADDING_STRATEGY_MAX,
  // Removed and turned into an alias because it is unreasonably expensive for
  // very little benefit.
  PADDING_STRATEGY_CALLBACK = PADDING_STRATEGY_ALIGNED
};

enum session_state_flags {
  SESSION_STATE_NONE = 0x0,
  SESSION_STATE_HAS_SCOPE = 0x1,
  SESSION_STATE_WRITE_SCHEDULED = 0x2,
  SESSION_STATE_CLOSED = 0x4,
  SESSION_STATE_CLOSING = 0x8,
  SESSION_STATE_SENDING = 0x10,
  SESSION_STATE_WRITE_IN_PROGRESS = 0x20,
  SESSION_STATE_READING_STOPPED = 0x40,
  SESSION_STATE_NGHTTP2_RECV_PAUSED = 0x80
};

typedef uint32_t(*get_setting)(nghttp2_session* session,
                               nghttp2_settings_id id);

class Http2Session;
class Http2Stream;

// This scope should be present when any call into nghttp2 that may schedule
// data to be written to the underlying transport is made, and schedules
// such a write automatically once the scope is exited.
class Http2Scope {
 public:
  explicit Http2Scope(Http2Stream* stream);
  explicit Http2Scope(Http2Session* session);
  ~Http2Scope();

 private:
  Http2Session* session_ = nullptr;
  v8::Local<v8::Object> session_handle_;
};

// The Http2Options class is used to parse the options object passed in to
// a Http2Session object and convert those into an appropriate nghttp2_option
// struct. This is the primary mechanism by which the Http2Session object is
// configured.
class Http2Options {
 public:
  Http2Options(Environment* env, nghttp2_session_type type);

  ~Http2Options() = default;

  nghttp2_option* operator*() const {
    return options_.get();
  }

  void SetMaxHeaderPairs(uint32_t max) {
    max_header_pairs_ = max;
  }

  uint32_t GetMaxHeaderPairs() const {
    return max_header_pairs_;
  }

  void SetPaddingStrategy(padding_strategy_type val) {
    padding_strategy_ = val;
  }

  padding_strategy_type GetPaddingStrategy() const {
    return padding_strategy_;
  }

  void SetMaxOutstandingPings(size_t max) {
    max_outstanding_pings_ = max;
  }

  size_t GetMaxOutstandingPings() const {
    return max_outstanding_pings_;
  }

  void SetMaxOutstandingSettings(size_t max) {
    max_outstanding_settings_ = max;
  }

  size_t GetMaxOutstandingSettings() const {
    return max_outstanding_settings_;
  }

  void SetMaxSessionMemory(uint64_t max) {
    max_session_memory_ = max;
  }

  uint64_t GetMaxSessionMemory() const {
    return max_session_memory_;
  }

 private:
  Nghttp2OptionPointer options_;
  uint64_t max_session_memory_ = kDefaultMaxSessionMemory;
  uint32_t max_header_pairs_ = DEFAULT_MAX_HEADER_LIST_PAIRS;
  padding_strategy_type padding_strategy_ = PADDING_STRATEGY_NONE;
  size_t max_outstanding_pings_ = kDefaultMaxPings;
  size_t max_outstanding_settings_ = kDefaultMaxSettings;
};

struct Http2Priority : public nghttp2_priority_spec {
  Http2Priority(Environment* env,
                v8::Local<v8::Value> parent,
                v8::Local<v8::Value> weight,
                v8::Local<v8::Value> exclusive);
};

class Http2StreamListener : public StreamListener {
 public:
  uv_buf_t OnStreamAlloc(size_t suggested_size) override;
  void OnStreamRead(ssize_t nread, const uv_buf_t& buf) override;
};

struct Http2HeaderTraits {
  typedef Http2RcBufferPointer rcbufferpointer_t;
  typedef Http2Session allocator_t;

  // HTTP/2 does not support identifying header names by token id.
  // HTTP/3 will, however, so we prepare for that now.
  static const char* ToHttpHeaderName(int32_t token) { return nullptr; }
};

using Http2Header = NgHeader<Http2HeaderTraits>;

class Http2Stream : public AsyncWrap,
                    public StreamBase {
 public:
  static Http2Stream* New(
      Http2Session* session,
      int32_t id,
      nghttp2_headers_category category = NGHTTP2_HCAT_HEADERS,
      int options = 0);
  ~Http2Stream() override;

  nghttp2_stream* operator*();

  Http2Session* session() { return session_.get(); }
  const Http2Session* session() const { return session_.get(); }

  void EmitStatistics();

  // Required for StreamBase
  int ReadStart() override;

  // Required for StreamBase
  int ReadStop() override;

  // Required for StreamBase
  ShutdownWrap* CreateShutdownWrap(v8::Local<v8::Object> object) override;
  int DoShutdown(ShutdownWrap* req_wrap) override;

  bool HasWantsWrite() const override { return true; }

  // Initiate a response on this stream.
  int SubmitResponse(const Http2Headers& headers, int options);

  // Submit informational headers for this stream
  int SubmitInfo(const Http2Headers& headers);

  // Submit trailing headers for this stream
  int SubmitTrailers(const Http2Headers& headers);
  void OnTrailers();

  // Submit a PRIORITY frame for this stream
  int SubmitPriority(nghttp2_priority_spec* prispec, bool silent = false);

  // Submits an RST_STREAM frame using the given code
  void SubmitRstStream(const uint32_t code);

  void FlushRstStream();

  // Submits a PUSH_PROMISE frame with this stream as the parent.
  Http2Stream* SubmitPushPromise(
      const Http2Headers& headers,
      int32_t* ret,
      int options = 0);


  void Close(int32_t code);

  // Destroy this stream instance and free all held memory.
  void Destroy();

  inline bool IsDestroyed() const {
    return flags_ & NGHTTP2_STREAM_FLAG_DESTROYED;
  }

  inline bool IsWritable() const {
    return !(flags_ & NGHTTP2_STREAM_FLAG_SHUT);
  }

  inline bool IsPaused() const {
    return flags_ & NGHTTP2_STREAM_FLAG_READ_PAUSED;
  }

  inline bool IsClosed() const {
    return flags_ & NGHTTP2_STREAM_FLAG_CLOSED;
  }

  inline bool HasTrailers() const {
    return flags_ & NGHTTP2_STREAM_FLAG_TRAILERS;
  }

  // Returns true if this stream is in the reading state, which occurs when
  // the NGHTTP2_STREAM_FLAG_READ_START flag has been set and the
  // NGHTTP2_STREAM_FLAG_READ_PAUSED flag is *not* set.
  inline bool IsReading() const {
    return flags_ & NGHTTP2_STREAM_FLAG_READ_START &&
           !(flags_ & NGHTTP2_STREAM_FLAG_READ_PAUSED);
  }

  // Returns the RST_STREAM code used to close this stream
  inline int32_t code() const { return code_; }

  // Returns the stream identifier for this stream
  inline int32_t id() const { return id_; }

  inline void IncrementAvailableOutboundLength(size_t amount);
  inline void DecrementAvailableOutboundLength(size_t amount);

  bool AddHeader(nghttp2_rcbuf* name, nghttp2_rcbuf* value, uint8_t flags);

  template <typename Fn>
  void TransferHeaders(Fn&& fn) {
    size_t i = 0;
    for (const auto& header : current_headers_ )
      fn(header, i++);
    current_headers_.clear();
  }

  size_t headers_count() const {
    return current_headers_.size();
  }

  inline nghttp2_headers_category headers_category() const {
    return current_headers_category_;
  }

  void StartHeaders(nghttp2_headers_category category);

  // Required for StreamBase
  bool IsAlive() override {
    return true;
  }

  // Required for StreamBase
  bool IsClosing() override {
    return false;
  }

  AsyncWrap* GetAsyncWrap() override { return this; }

  int DoWrite(WriteWrap* w, uv_buf_t* bufs, size_t count,
              uv_stream_t* send_handle) override;

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackField("current_headers", current_headers_);
    tracker->TrackField("queue", queue_);
  }

  SET_MEMORY_INFO_NAME(Http2Stream)
  SET_SELF_SIZE(Http2Stream)

  std::string diagnostic_name() const override;

  // JavaScript API
  static void GetID(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Destroy(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Priority(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void PushPromise(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RefreshState(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Info(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Trailers(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Respond(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RstStream(const v8::FunctionCallbackInfo<v8::Value>& args);

  class Provider;

  struct Statistics {
    uint64_t start_time;
    uint64_t end_time;
    uint64_t first_header;     // Time first header was received
    uint64_t first_byte;       // Time first DATA frame byte was received
    uint64_t first_byte_sent;  // Time first DATA frame byte was sent
    uint64_t sent_bytes;
    uint64_t received_bytes;
  };

  Statistics statistics_ = {};

 private:
  Http2Stream(Http2Session* session,
              v8::Local<v8::Object> obj,
              int32_t id,
              nghttp2_headers_category category,
              int options);

  BaseObjectWeakPtr<Http2Session> session_;     // The Parent HTTP/2 Session
  int32_t id_ = 0;                              // The Stream Identifier
  int32_t code_ = NGHTTP2_NO_ERROR;             // The RST_STREAM code (if any)
  int flags_ = NGHTTP2_STREAM_FLAG_NONE;        // Internal state flags

  uint32_t max_header_pairs_ = DEFAULT_MAX_HEADER_LIST_PAIRS;
  uint32_t max_header_length_ = DEFAULT_SETTINGS_MAX_HEADER_LIST_SIZE;

  // The Current Headers block... As headers are received for this stream,
  // they are temporarily stored here until the OnFrameReceived is called
  // signalling the end of the HEADERS frame
  nghttp2_headers_category current_headers_category_ = NGHTTP2_HCAT_HEADERS;
  uint32_t current_headers_length_ = 0;  // total number of octets
  std::vector<Http2Header> current_headers_;

  // This keeps track of the amount of data read from the socket while the
  // socket was in paused mode. When `ReadStart()` is called (and not before
  // then), we tell nghttp2 that we consumed that data to get proper
  // backpressure handling.
  size_t inbound_consumed_data_while_paused_ = 0;

  // Outbound Data... This is the data written by the JS layer that is
  // waiting to be written out to the socket.
  std::queue<NgHttp2StreamWrite> queue_;
  size_t available_outbound_length_ = 0;

  Http2StreamListener stream_listener_;

  friend class Http2Session;
};

class Http2Stream::Provider {
 public:
  Provider(Http2Stream* stream, int options);
  explicit Provider(int options);
  virtual ~Provider();

  nghttp2_data_provider* operator*() {
    return !empty_ ? &provider_ : nullptr;
  }

  class FD;
  class Stream;
 protected:
  nghttp2_data_provider provider_;

 private:
  bool empty_ = false;
};

class Http2Stream::Provider::Stream : public Http2Stream::Provider {
 public:
  Stream(Http2Stream* stream, int options);
  explicit Stream(int options);

  static ssize_t OnRead(nghttp2_session* session,
                        int32_t id,
                        uint8_t* buf,
                        size_t length,
                        uint32_t* flags,
                        nghttp2_data_source* source,
                        void* user_data);
};

struct SessionJSFields {
  uint8_t bitfield;
  uint8_t priority_listener_count;
  uint8_t frame_error_listener_count;
  uint32_t max_invalid_frames = 1000;
  uint32_t max_rejected_streams = 100;
};

// Indices for js_fields_, which serves as a way to communicate data with JS
// land fast. In particular, we store information about the number/presence
// of certain event listeners in JS, and skip calls from C++ into JS if they
// are missing.
enum SessionUint8Fields {
  kBitfield = offsetof(SessionJSFields, bitfield),  // See below
  kSessionPriorityListenerCount =
      offsetof(SessionJSFields, priority_listener_count),
  kSessionFrameErrorListenerCount =
      offsetof(SessionJSFields, frame_error_listener_count),
  kSessionMaxInvalidFrames = offsetof(SessionJSFields, max_invalid_frames),
  kSessionMaxRejectedStreams = offsetof(SessionJSFields, max_rejected_streams),
  kSessionUint8FieldCount = sizeof(SessionJSFields)
};

enum SessionBitfieldFlags {
  kSessionHasRemoteSettingsListeners,
  kSessionRemoteSettingsIsUpToDate,
  kSessionHasPingListeners,
  kSessionHasAltsvcListeners
};

class Http2Session : public AsyncWrap,
                     public StreamListener,
                     public mem::NgLibMemoryManager<Http2Session, nghttp2_mem> {
 public:
  Http2Session(Environment* env,
               v8::Local<v8::Object> wrap,
               nghttp2_session_type type = NGHTTP2_SESSION_SERVER);
  ~Http2Session() override;

  class Http2Ping;
  class Http2Settings;

  void EmitStatistics();

  inline StreamBase* underlying_stream() {
    return static_cast<StreamBase*>(stream_);
  }

  void Close(uint32_t code = NGHTTP2_NO_ERROR,
             bool socket_closed = false);
  void Consume(v8::Local<v8::Object> stream);
  void Goaway(uint32_t code, int32_t lastStreamID,
              const uint8_t* data, size_t len);
  void AltSvc(int32_t id,
              uint8_t* origin,
              size_t origin_len,
              uint8_t* value,
              size_t value_len);
  void Origin(nghttp2_origin_entry* ov, size_t count);

  uint8_t SendPendingData();

  // Submits a new request. If the request is a success, assigned
  // will be a pointer to the Http2Stream instance assigned.
  // This only works if the session is a client session.
  Http2Stream* SubmitRequest(
      nghttp2_priority_spec* prispec,
      const Http2Headers& headers,
      int32_t* ret,
      int options = 0);

  inline nghttp2_session_type type() const { return session_type_; }

  inline nghttp2_session* session() const { return session_.get(); }

  inline nghttp2_session* operator*() { return session_.get(); }

  inline uint32_t GetMaxHeaderPairs() const { return max_header_pairs_; }

  inline const char* TypeName() const;

  inline bool IsDestroyed() {
    return (flags_ & SESSION_STATE_CLOSED) || session_ == nullptr;
  }

  // Schedule a write if nghttp2 indicates it wants to write to the socket.
  void MaybeScheduleWrite();

  // Stop reading if nghttp2 doesn't want to anymore.
  void MaybeStopReading();

  // Returns pointer to the stream, or nullptr if stream does not exist
  inline Http2Stream* FindStream(int32_t id);

  inline bool CanAddStream();

  // Adds a stream instance to this session
  inline void AddStream(Http2Stream* stream);

  // Removes a stream instance from this session
  inline void RemoveStream(Http2Stream* stream);

  // Indicates whether there currently exist outgoing buffers for this stream.
  bool HasWritesOnSocketForStream(Http2Stream* stream);

  // Write data from stream_buf_ to the session
  ssize_t ConsumeHTTP2Data();

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackField("streams", streams_);
    tracker->TrackField("outstanding_pings", outstanding_pings_);
    tracker->TrackField("outstanding_settings", outstanding_settings_);
    tracker->TrackField("outgoing_buffers", outgoing_buffers_);
    tracker->TrackFieldWithSize("stream_buf", stream_buf_.len);
    tracker->TrackFieldWithSize("outgoing_storage", outgoing_storage_.size());
    tracker->TrackFieldWithSize("pending_rst_streams",
                                pending_rst_streams_.size() * sizeof(int32_t));
    tracker->TrackFieldWithSize("nghttp2_memory", current_nghttp2_memory_);
  }

  SET_MEMORY_INFO_NAME(Http2Session)
  SET_SELF_SIZE(Http2Session)

  std::string diagnostic_name() const override;

  // Schedule an RstStream for after the current write finishes.
  inline void AddPendingRstStream(int32_t stream_id) {
    pending_rst_streams_.emplace_back(stream_id);
  }

  inline bool HasPendingRstStream(int32_t stream_id) {
    return pending_rst_streams_.end() != std::find(pending_rst_streams_.begin(),
                                                   pending_rst_streams_.end(),
                                                   stream_id);
  }

  // Handle reads/writes from the underlying network transport.
  uv_buf_t OnStreamAlloc(size_t suggested_size) override;
  void OnStreamRead(ssize_t nread, const uv_buf_t& buf) override;
  void OnStreamAfterWrite(WriteWrap* w, int status) override;

  // Implementation for mem::NgLibMemoryManager
  void CheckAllocatedSize(size_t previous_size) const;
  void IncreaseAllocatedSize(size_t size);
  void DecreaseAllocatedSize(size_t size);

  // The JavaScript API
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Consume(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Destroy(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Settings(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Request(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetNextStreamID(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Goaway(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void UpdateChunksSent(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RefreshState(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Ping(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void AltSvc(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Origin(const v8::FunctionCallbackInfo<v8::Value>& args);

  template <get_setting fn>
  static void RefreshSettings(const v8::FunctionCallbackInfo<v8::Value>& args);

  uv_loop_t* event_loop() const {
    return env()->event_loop();
  }

  BaseObjectPtr<Http2Ping> PopPing();
  Http2Ping* AddPing(BaseObjectPtr<Http2Ping> ping);

  BaseObjectPtr<Http2Settings> PopSettings();
  Http2Settings* AddSettings(BaseObjectPtr<Http2Settings> settings);

  void IncrementCurrentSessionMemory(uint64_t amount) {
    current_session_memory_ += amount;
  }

  void DecrementCurrentSessionMemory(uint64_t amount) {
    DCHECK_LE(amount, current_session_memory_);
    current_session_memory_ -= amount;
  }

  // Tell our custom memory allocator that this rcbuf is independent of
  // this session now, and may outlive it.
  void StopTrackingRcbuf(nghttp2_rcbuf* buf);

  // Returns the current session memory including memory allocated by nghttp2,
  // the current outbound storage queue, and pending writes.
  uint64_t GetCurrentSessionMemory() {
    uint64_t total = current_session_memory_ + sizeof(Http2Session);
    total += current_nghttp2_memory_;
    total += outgoing_storage_.size();
    return total;
  }

  // Return true if current_session_memory + amount is less than the max
  bool IsAvailableSessionMemory(uint64_t amount) {
    return GetCurrentSessionMemory() + amount <= max_session_memory_;
  }

  struct Statistics {
    uint64_t start_time;
    uint64_t end_time;
    uint64_t ping_rtt;
    uint64_t data_sent;
    uint64_t data_received;
    uint32_t frame_count;
    uint32_t frame_sent;
    int32_t stream_count;
    size_t max_concurrent_streams;
    double stream_average_duration;
  };

  Statistics statistics_ = {};

 private:
  // Frame Padding Strategies
  ssize_t OnDWordAlignedPadding(size_t frameLength,
                                size_t maxPayloadLen);
  ssize_t OnMaxFrameSizePadding(size_t frameLength,
                                size_t maxPayloadLen);

  // Frame Handler
  int HandleDataFrame(const nghttp2_frame* frame);
  void HandleGoawayFrame(const nghttp2_frame* frame);
  void HandleHeadersFrame(const nghttp2_frame* frame);
  void HandlePriorityFrame(const nghttp2_frame* frame);
  void HandleSettingsFrame(const nghttp2_frame* frame);
  void HandlePingFrame(const nghttp2_frame* frame);
  void HandleAltSvcFrame(const nghttp2_frame* frame);
  void HandleOriginFrame(const nghttp2_frame* frame);

  // nghttp2 callbacks
  static int OnBeginHeadersCallback(
      nghttp2_session* session,
      const nghttp2_frame* frame,
      void* user_data);
  static int OnHeaderCallback(
      nghttp2_session* session,
      const nghttp2_frame* frame,
      nghttp2_rcbuf* name,
      nghttp2_rcbuf* value,
      uint8_t flags,
      void* user_data);
  static int OnFrameReceive(
      nghttp2_session* session,
      const nghttp2_frame* frame,
      void* user_data);
  static int OnFrameNotSent(
      nghttp2_session* session,
      const nghttp2_frame* frame,
      int error_code,
      void* user_data);
  static int OnFrameSent(
      nghttp2_session* session,
      const nghttp2_frame* frame,
      void* user_data);
  static int OnStreamClose(
      nghttp2_session* session,
      int32_t id,
      uint32_t code,
      void* user_data);
  static int OnInvalidHeader(
      nghttp2_session* session,
      const nghttp2_frame* frame,
      nghttp2_rcbuf* name,
      nghttp2_rcbuf* value,
      uint8_t flags,
      void* user_data);
  static int OnDataChunkReceived(
      nghttp2_session* session,
      uint8_t flags,
      int32_t id,
      const uint8_t* data,
      size_t len,
      void* user_data);
  static ssize_t OnSelectPadding(
      nghttp2_session* session,
      const nghttp2_frame* frame,
      size_t maxPayloadLen,
      void* user_data);
  static int OnNghttpError(
      nghttp2_session* session,
      const char* message,
      size_t len,
      void* user_data);
  static int OnSendData(
      nghttp2_session* session,
      nghttp2_frame* frame,
      const uint8_t* framehd,
      size_t length,
      nghttp2_data_source* source,
      void* user_data);
  static int OnInvalidFrame(
      nghttp2_session* session,
      const nghttp2_frame* frame,
      int lib_error_code,
      void* user_data);

  struct Callbacks {
    inline explicit Callbacks(bool kHasGetPaddingCallback);

    Nghttp2SessionCallbacksPointer callbacks;
  };

  /* Use callback_struct_saved[kHasGetPaddingCallback ? 1 : 0] */
  static const Callbacks callback_struct_saved[2];

  // The underlying nghttp2_session handle
  Nghttp2SessionPointer session_;

  // JS-accessible numeric fields, as indexed by SessionUint8Fields.
  SessionJSFields js_fields_ = {};

  // The session type: client or server
  nghttp2_session_type session_type_;

  // The maximum number of header pairs permitted for streams on this session
  uint32_t max_header_pairs_ = DEFAULT_MAX_HEADER_LIST_PAIRS;

  // The maximum amount of memory allocated for this session
  uint64_t max_session_memory_ = kDefaultMaxSessionMemory;
  uint64_t current_session_memory_ = 0;
  // The amount of memory allocated by nghttp2 internals
  uint64_t current_nghttp2_memory_ = 0;

  // The collection of active Http2Streams associated with this session
  std::unordered_map<int32_t, Http2Stream*> streams_;

  int flags_ = SESSION_STATE_NONE;

  // The StreamBase instance being used for i/o
  padding_strategy_type padding_strategy_ = PADDING_STRATEGY_NONE;

  // use this to allow timeout tracking during long-lasting writes
  uint32_t chunks_sent_since_last_write_ = 0;

  uv_buf_t stream_buf_ = uv_buf_init(nullptr, 0);
  // When processing input data, either stream_buf_ab_ or stream_buf_allocation_
  // will be set. stream_buf_ab_ is lazily created from stream_buf_allocation_.
  v8::Global<v8::ArrayBuffer> stream_buf_ab_;
  AllocatedBuffer stream_buf_allocation_;
  size_t stream_buf_offset_ = 0;

  size_t max_outstanding_pings_ = kDefaultMaxPings;
  std::queue<BaseObjectPtr<Http2Ping>> outstanding_pings_;

  size_t max_outstanding_settings_ = kDefaultMaxSettings;
  std::queue<BaseObjectPtr<Http2Settings>> outstanding_settings_;

  std::vector<NgHttp2StreamWrite> outgoing_buffers_;
  std::vector<uint8_t> outgoing_storage_;
  size_t outgoing_length_ = 0;
  std::vector<int32_t> pending_rst_streams_;
  // Count streams that have been rejected while being opened. Exceeding a fixed
  // limit will result in the session being destroyed, as an indication of a
  // misbehaving peer. This counter is reset once new streams are being
  // accepted again.
  uint32_t rejected_stream_count_ = 0;
  // Also use the invalid frame count as a measure for rejecting input frames.
  uint32_t invalid_frame_count_ = 0;

  void PushOutgoingBuffer(NgHttp2StreamWrite&& write);
  void CopyDataIntoOutgoing(const uint8_t* src, size_t src_length);
  void ClearOutgoing(int status);

  friend class Http2Scope;
  friend class Http2StreamListener;
};

class Http2SessionPerformanceEntry : public performance::PerformanceEntry {
 public:
  Http2SessionPerformanceEntry(
      Environment* env,
      const Http2Session::Statistics& stats,
      nghttp2_session_type type) :
          performance::PerformanceEntry(
              env, "Http2Session", "http2",
              stats.start_time,
              stats.end_time),
          ping_rtt_(stats.ping_rtt),
          data_sent_(stats.data_sent),
          data_received_(stats.data_received),
          frame_count_(stats.frame_count),
          frame_sent_(stats.frame_sent),
          stream_count_(stats.stream_count),
          max_concurrent_streams_(stats.max_concurrent_streams),
          stream_average_duration_(stats.stream_average_duration),
          session_type_(type) { }

  uint64_t ping_rtt() const { return ping_rtt_; }
  uint64_t data_sent() const { return data_sent_; }
  uint64_t data_received() const { return data_received_; }
  uint32_t frame_count() const { return frame_count_; }
  uint32_t frame_sent() const { return frame_sent_; }
  int32_t stream_count() const { return stream_count_; }
  size_t max_concurrent_streams() const { return max_concurrent_streams_; }
  double stream_average_duration() const { return stream_average_duration_; }
  nghttp2_session_type type() const { return session_type_; }

  void Notify(v8::Local<v8::Value> obj) {
    performance::PerformanceEntry::Notify(env(), kind(), obj);
  }

 private:
  uint64_t ping_rtt_;
  uint64_t data_sent_;
  uint64_t data_received_;
  uint32_t frame_count_;
  uint32_t frame_sent_;
  int32_t stream_count_;
  size_t max_concurrent_streams_;
  double stream_average_duration_;
  nghttp2_session_type session_type_;
};

class Http2StreamPerformanceEntry
    : public performance::PerformanceEntry {
 public:
  Http2StreamPerformanceEntry(
      Environment* env,
      int32_t id,
      const Http2Stream::Statistics& stats) :
          performance::PerformanceEntry(
              env, "Http2Stream", "http2",
              stats.start_time,
              stats.end_time),
          id_(id),
          first_header_(stats.first_header),
          first_byte_(stats.first_byte),
          first_byte_sent_(stats.first_byte_sent),
          sent_bytes_(stats.sent_bytes),
          received_bytes_(stats.received_bytes) { }

  int32_t id() const { return id_; }
  uint64_t first_header() const { return first_header_; }
  uint64_t first_byte() const { return first_byte_; }
  uint64_t first_byte_sent() const { return first_byte_sent_; }
  uint64_t sent_bytes() const { return sent_bytes_; }
  uint64_t received_bytes() const { return received_bytes_; }

  void Notify(v8::Local<v8::Value> obj) {
    performance::PerformanceEntry::Notify(env(), kind(), obj);
  }

 private:
  int32_t id_;
  uint64_t first_header_;
  uint64_t first_byte_;
  uint64_t first_byte_sent_;
  uint64_t sent_bytes_;
  uint64_t received_bytes_;
};

class Http2Session::Http2Ping : public AsyncWrap {
 public:
  explicit Http2Ping(Http2Session* session, v8::Local<v8::Object> obj);

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackField("session", session_);
  }

  SET_MEMORY_INFO_NAME(Http2Ping)
  SET_SELF_SIZE(Http2Ping)

  void Send(const uint8_t* payload);
  void Done(bool ack, const uint8_t* payload = nullptr);
  void DetachFromSession();

 private:
  Http2Session* session_;
  uint64_t startTime_;
};

// The Http2Settings class is used to parse the settings passed in for
// an Http2Session, converting those into an array of nghttp2_settings_entry
// structs.
class Http2Session::Http2Settings : public AsyncWrap {
 public:
  Http2Settings(Environment* env,
                Http2Session* session,
                v8::Local<v8::Object> obj,
                uint64_t start_time = uv_hrtime());

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackField("session", session_);
  }

  SET_MEMORY_INFO_NAME(Http2Settings)
  SET_SELF_SIZE(Http2Settings)

  void Send();
  void Done(bool ack);

  // Returns a Buffer instance with the serialized SETTINGS payload
  v8::Local<v8::Value> Pack();

  // Resets the default values in the settings buffer
  static void RefreshDefaults(Environment* env);

  // Update the local or remote settings for the given session
  static void Update(Environment* env,
                     Http2Session* session,
                     get_setting fn);

 private:
  void Init();
  Http2Session* session_;
  uint64_t startTime_;
  size_t count_ = 0;
  nghttp2_settings_entry entries_[IDX_SETTINGS_COUNT];
};

class Origins {
 public:
  Origins(v8::Isolate* isolate,
          v8::Local<v8::Context> context,
          v8::Local<v8::String> origin_string,
          size_t origin_count);
  ~Origins() = default;

  nghttp2_origin_entry* operator*() {
    return reinterpret_cast<nghttp2_origin_entry*>(*buf_);
  }

  size_t length() const {
    return count_;
  }

 private:
  size_t count_;
  MaybeStackBuffer<char, 512> buf_;
};

}  // namespace http2
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_HTTP2_H_
