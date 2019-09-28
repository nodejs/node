#ifndef SRC_NODE_HTTP2_H_
#define SRC_NODE_HTTP2_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

// FIXME(joyeecheung): nghttp2.h needs stdint.h to compile on Windows
#include <cstdint>
#include "nghttp2/nghttp2.h"

#include "node_http2_state.h"
#include "node_mem.h"
#include "node_perf.h"
#include "stream_base-inl.h"
#include "string_bytes.h"

#include <algorithm>
#include <queue>

namespace node {
namespace http2 {

using v8::Array;
using v8::Context;
using v8::Isolate;
using v8::MaybeLocal;

using performance::PerformanceEntry;

// We strictly limit the number of outstanding unacknowledged PINGS a user
// may send in order to prevent abuse. The current default cap is 10. The
// user may set a different limit using a per Http2Session configuration
// option.
#define DEFAULT_MAX_PINGS 10

// Also strictly limit the number of outstanding SETTINGS frames a user sends
#define DEFAULT_MAX_SETTINGS 10

// Default maximum total memory cap for Http2Session.
#define DEFAULT_MAX_SESSION_MEMORY 1e7

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

#define MAX_MAX_HEADER_LIST_SIZE 16777215u
#define DEFAULT_MAX_HEADER_LIST_PAIRS 128u

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

struct nghttp2_stream_write : public MemoryRetainer {
  WriteWrap* req_wrap = nullptr;
  uv_buf_t buf;

  inline explicit nghttp2_stream_write(uv_buf_t buf_) : buf(buf_) {}
  inline nghttp2_stream_write(WriteWrap* req, uv_buf_t buf_) :
      req_wrap(req), buf(buf_) {}

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(nghttp2_stream_write)
  SET_SELF_SIZE(nghttp2_stream_write)
};

struct nghttp2_header : public MemoryRetainer {
  nghttp2_rcbuf* name = nullptr;
  nghttp2_rcbuf* value = nullptr;
  uint8_t flags = 0;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(nghttp2_header)
  SET_SELF_SIZE(nghttp2_header)
};


// Unlike the HTTP/1 implementation, the HTTP/2 implementation is not limited
// to a fixed number of known supported HTTP methods. These constants, therefore
// are provided strictly as a convenience to users and are exposed via the
// require('http2').constants object.
#define HTTP_KNOWN_METHODS(V)                                                 \
  V(ACL, "ACL")                                                               \
  V(BASELINE_CONTROL, "BASELINE-CONTROL")                                     \
  V(BIND, "BIND")                                                             \
  V(CHECKIN, "CHECKIN")                                                       \
  V(CHECKOUT, "CHECKOUT")                                                     \
  V(CONNECT, "CONNECT")                                                       \
  V(COPY, "COPY")                                                             \
  V(DELETE, "DELETE")                                                         \
  V(GET, "GET")                                                               \
  V(HEAD, "HEAD")                                                             \
  V(LABEL, "LABEL")                                                           \
  V(LINK, "LINK")                                                             \
  V(LOCK, "LOCK")                                                             \
  V(MERGE, "MERGE")                                                           \
  V(MKACTIVITY, "MKACTIVITY")                                                 \
  V(MKCALENDAR, "MKCALENDAR")                                                 \
  V(MKCOL, "MKCOL")                                                           \
  V(MKREDIRECTREF, "MKREDIRECTREF")                                           \
  V(MKWORKSPACE, "MKWORKSPACE")                                               \
  V(MOVE, "MOVE")                                                             \
  V(OPTIONS, "OPTIONS")                                                       \
  V(ORDERPATCH, "ORDERPATCH")                                                 \
  V(PATCH, "PATCH")                                                           \
  V(POST, "POST")                                                             \
  V(PRI, "PRI")                                                               \
  V(PROPFIND, "PROPFIND")                                                     \
  V(PROPPATCH, "PROPPATCH")                                                   \
  V(PUT, "PUT")                                                               \
  V(REBIND, "REBIND")                                                         \
  V(REPORT, "REPORT")                                                         \
  V(SEARCH, "SEARCH")                                                         \
  V(TRACE, "TRACE")                                                           \
  V(UNBIND, "UNBIND")                                                         \
  V(UNCHECKOUT, "UNCHECKOUT")                                                 \
  V(UNLINK, "UNLINK")                                                         \
  V(UNLOCK, "UNLOCK")                                                         \
  V(UPDATE, "UPDATE")                                                         \
  V(UPDATEREDIRECTREF, "UPDATEREDIRECTREF")                                   \
  V(VERSION_CONTROL, "VERSION-CONTROL")

// These are provided strictly as a convenience to users and are exposed via the
// require('http2').constants objects
#define HTTP_KNOWN_HEADERS(V)                                                 \
  V(STATUS, ":status")                                                        \
  V(METHOD, ":method")                                                        \
  V(AUTHORITY, ":authority")                                                  \
  V(SCHEME, ":scheme")                                                        \
  V(PATH, ":path")                                                            \
  V(PROTOCOL, ":protocol")                                                    \
  V(ACCEPT_CHARSET, "accept-charset")                                         \
  V(ACCEPT_ENCODING, "accept-encoding")                                       \
  V(ACCEPT_LANGUAGE, "accept-language")                                       \
  V(ACCEPT_RANGES, "accept-ranges")                                           \
  V(ACCEPT, "accept")                                                         \
  V(ACCESS_CONTROL_ALLOW_CREDENTIALS, "access-control-allow-credentials")     \
  V(ACCESS_CONTROL_ALLOW_HEADERS, "access-control-allow-headers")             \
  V(ACCESS_CONTROL_ALLOW_METHODS, "access-control-allow-methods")             \
  V(ACCESS_CONTROL_ALLOW_ORIGIN, "access-control-allow-origin")               \
  V(ACCESS_CONTROL_EXPOSE_HEADERS, "access-control-expose-headers")           \
  V(ACCESS_CONTROL_MAX_AGE, "access-control-max-age")                         \
  V(ACCESS_CONTROL_REQUEST_HEADERS, "access-control-request-headers")         \
  V(ACCESS_CONTROL_REQUEST_METHOD, "access-control-request-method")           \
  V(AGE, "age")                                                               \
  V(ALLOW, "allow")                                                           \
  V(AUTHORIZATION, "authorization")                                           \
  V(CACHE_CONTROL, "cache-control")                                           \
  V(CONNECTION, "connection")                                                 \
  V(CONTENT_DISPOSITION, "content-disposition")                               \
  V(CONTENT_ENCODING, "content-encoding")                                     \
  V(CONTENT_LANGUAGE, "content-language")                                     \
  V(CONTENT_LENGTH, "content-length")                                         \
  V(CONTENT_LOCATION, "content-location")                                     \
  V(CONTENT_MD5, "content-md5")                                               \
  V(CONTENT_RANGE, "content-range")                                           \
  V(CONTENT_TYPE, "content-type")                                             \
  V(COOKIE, "cookie")                                                         \
  V(DATE, "date")                                                             \
  V(DNT, "dnt")                                                               \
  V(ETAG, "etag")                                                             \
  V(EXPECT, "expect")                                                         \
  V(EXPIRES, "expires")                                                       \
  V(FORWARDED, "forwarded")                                                   \
  V(FROM, "from")                                                             \
  V(HOST, "host")                                                             \
  V(IF_MATCH, "if-match")                                                     \
  V(IF_MODIFIED_SINCE, "if-modified-since")                                   \
  V(IF_NONE_MATCH, "if-none-match")                                           \
  V(IF_RANGE, "if-range")                                                     \
  V(IF_UNMODIFIED_SINCE, "if-unmodified-since")                               \
  V(LAST_MODIFIED, "last-modified")                                           \
  V(LINK, "link")                                                             \
  V(LOCATION, "location")                                                     \
  V(MAX_FORWARDS, "max-forwards")                                             \
  V(PREFER, "prefer")                                                         \
  V(PROXY_AUTHENTICATE, "proxy-authenticate")                                 \
  V(PROXY_AUTHORIZATION, "proxy-authorization")                               \
  V(RANGE, "range")                                                           \
  V(REFERER, "referer")                                                       \
  V(REFRESH, "refresh")                                                       \
  V(RETRY_AFTER, "retry-after")                                               \
  V(SERVER, "server")                                                         \
  V(SET_COOKIE, "set-cookie")                                                 \
  V(STRICT_TRANSPORT_SECURITY, "strict-transport-security")                   \
  V(TRAILER, "trailer")                                                       \
  V(TRANSFER_ENCODING, "transfer-encoding")                                   \
  V(TE, "te")                                                                 \
  V(TK, "tk")                                                                 \
  V(UPGRADE_INSECURE_REQUESTS, "upgrade-insecure-requests")                   \
  V(UPGRADE, "upgrade")                                                       \
  V(USER_AGENT, "user-agent")                                                 \
  V(VARY, "vary")                                                             \
  V(VIA, "via")                                                               \
  V(WARNING, "warning")                                                       \
  V(WWW_AUTHENTICATE, "www-authenticate")                                     \
  V(X_CONTENT_TYPE_OPTIONS, "x-content-type-options")                         \
  V(X_FRAME_OPTIONS, "x-frame-options")                                       \
  V(HTTP2_SETTINGS, "http2-settings")                                         \
  V(KEEP_ALIVE, "keep-alive")                                                 \
  V(PROXY_CONNECTION, "proxy-connection")

enum http_known_headers {
  HTTP_KNOWN_HEADER_MIN,
#define V(name, value) HTTP_HEADER_##name,
  HTTP_KNOWN_HEADERS(V)
#undef V
  HTTP_KNOWN_HEADER_MAX
};

// While some of these codes are used within the HTTP/2 implementation in
// core, they are provided strictly as a convenience to users and are exposed
// via the require('http2').constants object.
#define HTTP_STATUS_CODES(V)                                                  \
  V(CONTINUE, 100)                                                            \
  V(SWITCHING_PROTOCOLS, 101)                                                 \
  V(PROCESSING, 102)                                                          \
  V(EARLY_HINTS, 103)                                                         \
  V(OK, 200)                                                                  \
  V(CREATED, 201)                                                             \
  V(ACCEPTED, 202)                                                            \
  V(NON_AUTHORITATIVE_INFORMATION, 203)                                       \
  V(NO_CONTENT, 204)                                                          \
  V(RESET_CONTENT, 205)                                                       \
  V(PARTIAL_CONTENT, 206)                                                     \
  V(MULTI_STATUS, 207)                                                        \
  V(ALREADY_REPORTED, 208)                                                    \
  V(IM_USED, 226)                                                             \
  V(MULTIPLE_CHOICES, 300)                                                    \
  V(MOVED_PERMANENTLY, 301)                                                   \
  V(FOUND, 302)                                                               \
  V(SEE_OTHER, 303)                                                           \
  V(NOT_MODIFIED, 304)                                                        \
  V(USE_PROXY, 305)                                                           \
  V(TEMPORARY_REDIRECT, 307)                                                  \
  V(PERMANENT_REDIRECT, 308)                                                  \
  V(BAD_REQUEST, 400)                                                         \
  V(UNAUTHORIZED, 401)                                                        \
  V(PAYMENT_REQUIRED, 402)                                                    \
  V(FORBIDDEN, 403)                                                           \
  V(NOT_FOUND, 404)                                                           \
  V(METHOD_NOT_ALLOWED, 405)                                                  \
  V(NOT_ACCEPTABLE, 406)                                                      \
  V(PROXY_AUTHENTICATION_REQUIRED, 407)                                       \
  V(REQUEST_TIMEOUT, 408)                                                     \
  V(CONFLICT, 409)                                                            \
  V(GONE, 410)                                                                \
  V(LENGTH_REQUIRED, 411)                                                     \
  V(PRECONDITION_FAILED, 412)                                                 \
  V(PAYLOAD_TOO_LARGE, 413)                                                   \
  V(URI_TOO_LONG, 414)                                                        \
  V(UNSUPPORTED_MEDIA_TYPE, 415)                                              \
  V(RANGE_NOT_SATISFIABLE, 416)                                               \
  V(EXPECTATION_FAILED, 417)                                                  \
  V(TEAPOT, 418)                                                              \
  V(MISDIRECTED_REQUEST, 421)                                                 \
  V(UNPROCESSABLE_ENTITY, 422)                                                \
  V(LOCKED, 423)                                                              \
  V(FAILED_DEPENDENCY, 424)                                                   \
  V(UNORDERED_COLLECTION, 425)                                                \
  V(UPGRADE_REQUIRED, 426)                                                    \
  V(PRECONDITION_REQUIRED, 428)                                               \
  V(TOO_MANY_REQUESTS, 429)                                                   \
  V(REQUEST_HEADER_FIELDS_TOO_LARGE, 431)                                     \
  V(UNAVAILABLE_FOR_LEGAL_REASONS, 451)                                       \
  V(INTERNAL_SERVER_ERROR, 500)                                               \
  V(NOT_IMPLEMENTED, 501)                                                     \
  V(BAD_GATEWAY, 502)                                                         \
  V(SERVICE_UNAVAILABLE, 503)                                                 \
  V(GATEWAY_TIMEOUT, 504)                                                     \
  V(HTTP_VERSION_NOT_SUPPORTED, 505)                                          \
  V(VARIANT_ALSO_NEGOTIATES, 506)                                             \
  V(INSUFFICIENT_STORAGE, 507)                                                \
  V(LOOP_DETECTED, 508)                                                       \
  V(BANDWIDTH_LIMIT_EXCEEDED, 509)                                            \
  V(NOT_EXTENDED, 510)                                                        \
  V(NETWORK_AUTHENTICATION_REQUIRED, 511)

enum http_status_codes {
#define V(name, code) HTTP_STATUS_##name = code,
  HTTP_STATUS_CODES(V)
#undef V
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
  // Padding will be determined via a JS callback. Note that this can be
  // expensive because the callback is called once for every DATA and
  // HEADERS frame. For performance reasons, this strategy should be
  // avoided.
  PADDING_STRATEGY_CALLBACK
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
  Local<Object> session_handle_;
};

// The Http2Options class is used to parse the options object passed in to
// a Http2Session object and convert those into an appropriate nghttp2_option
// struct. This is the primary mechanism by which the Http2Session object is
// configured.
class Http2Options {
 public:
  Http2Options(Environment* env, nghttp2_session_type type);

  ~Http2Options() {
    nghttp2_option_del(options_);
  }

  nghttp2_option* operator*() const {
    return options_;
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

  size_t GetMaxOutstandingPings() {
    return max_outstanding_pings_;
  }

  void SetMaxOutstandingSettings(size_t max) {
    max_outstanding_settings_ = max;
  }

  size_t GetMaxOutstandingSettings() {
    return max_outstanding_settings_;
  }

  void SetMaxSessionMemory(uint64_t max) {
    max_session_memory_ = max;
  }

  uint64_t GetMaxSessionMemory() {
    return max_session_memory_;
  }

 private:
  nghttp2_option* options_;
  uint64_t max_session_memory_ = DEFAULT_MAX_SESSION_MEMORY;
  uint32_t max_header_pairs_ = DEFAULT_MAX_HEADER_LIST_PAIRS;
  padding_strategy_type padding_strategy_ = PADDING_STRATEGY_NONE;
  size_t max_outstanding_pings_ = DEFAULT_MAX_PINGS;
  size_t max_outstanding_settings_ = DEFAULT_MAX_SETTINGS;
};

class Http2Priority {
 public:
  Http2Priority(Environment* env,
                Local<Value> parent,
                Local<Value> weight,
                Local<Value> exclusive);

  nghttp2_priority_spec* operator*() {
    return &spec;
  }
 private:
  nghttp2_priority_spec spec;
};

class Http2StreamListener : public StreamListener {
 public:
  uv_buf_t OnStreamAlloc(size_t suggested_size) override;
  void OnStreamRead(ssize_t nread, const uv_buf_t& buf) override;
};

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
  int SubmitResponse(nghttp2_nv* nva, size_t len, int options);

  // Submit informational headers for this stream
  int SubmitInfo(nghttp2_nv* nva, size_t len);

  // Submit trailing headers for this stream
  int SubmitTrailers(nghttp2_nv* nva, size_t len);
  void OnTrailers();

  // Submit a PRIORITY frame for this stream
  int SubmitPriority(nghttp2_priority_spec* prispec, bool silent = false);

  // Submits an RST_STREAM frame using the given code
  void SubmitRstStream(const uint32_t code);

  void FlushRstStream();

  // Submits a PUSH_PROMISE frame with this stream as the parent.
  Http2Stream* SubmitPushPromise(
      nghttp2_nv* nva,
      size_t len,
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

  inline std::vector<nghttp2_header> move_headers() {
    return std::move(current_headers_);
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
  static void GetID(const FunctionCallbackInfo<Value>& args);
  static void Destroy(const FunctionCallbackInfo<Value>& args);
  static void Priority(const FunctionCallbackInfo<Value>& args);
  static void PushPromise(const FunctionCallbackInfo<Value>& args);
  static void RefreshState(const FunctionCallbackInfo<Value>& args);
  static void Info(const FunctionCallbackInfo<Value>& args);
  static void Trailers(const FunctionCallbackInfo<Value>& args);
  static void Respond(const FunctionCallbackInfo<Value>& args);
  static void RstStream(const FunctionCallbackInfo<Value>& args);

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
  std::vector<nghttp2_header> current_headers_;

  // This keeps track of the amount of data read from the socket while the
  // socket was in paused mode. When `ReadStart()` is called (and not before
  // then), we tell nghttp2 that we consumed that data to get proper
  // backpressure handling.
  size_t inbound_consumed_data_while_paused_ = 0;

  // Outbound Data... This is the data written by the JS layer that is
  // waiting to be written out to the socket.
  std::queue<nghttp2_stream_write> queue_;
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
               Local<Object> wrap,
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
  void Consume(Local<Object> stream);
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
      nghttp2_nv* nva,
      size_t len,
      int32_t* ret,
      int options = 0);

  inline nghttp2_session_type type() const { return session_type_; }

  inline nghttp2_session* session() const { return session_; }

  inline nghttp2_session* operator*() { return session_; }

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
  static void New(const FunctionCallbackInfo<Value>& args);
  static void Consume(const FunctionCallbackInfo<Value>& args);
  static void Destroy(const FunctionCallbackInfo<Value>& args);
  static void Settings(const FunctionCallbackInfo<Value>& args);
  static void Request(const FunctionCallbackInfo<Value>& args);
  static void SetNextStreamID(const FunctionCallbackInfo<Value>& args);
  static void Goaway(const FunctionCallbackInfo<Value>& args);
  static void UpdateChunksSent(const FunctionCallbackInfo<Value>& args);
  static void RefreshState(const FunctionCallbackInfo<Value>& args);
  static void Ping(const FunctionCallbackInfo<Value>& args);
  static void AltSvc(const FunctionCallbackInfo<Value>& args);
  static void Origin(const FunctionCallbackInfo<Value>& args);

  template <get_setting fn>
  static void RefreshSettings(const FunctionCallbackInfo<Value>& args);

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
  ssize_t OnCallbackPadding(size_t frameLength,
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
    inline ~Callbacks();

    nghttp2_session_callbacks* callbacks;
  };

  /* Use callback_struct_saved[kHasGetPaddingCallback ? 1 : 0] */
  static const Callbacks callback_struct_saved[2];

  // The underlying nghttp2_session handle
  nghttp2_session* session_;

  // JS-accessible numeric fields, as indexed by SessionUint8Fields.
  SessionJSFields js_fields_ = {};

  // The session type: client or server
  nghttp2_session_type session_type_;

  // The maximum number of header pairs permitted for streams on this session
  uint32_t max_header_pairs_ = DEFAULT_MAX_HEADER_LIST_PAIRS;

  // The maximum amount of memory allocated for this session
  uint64_t max_session_memory_ = DEFAULT_MAX_SESSION_MEMORY;
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

  size_t max_outstanding_pings_ = DEFAULT_MAX_PINGS;
  std::queue<BaseObjectPtr<Http2Ping>> outstanding_pings_;

  size_t max_outstanding_settings_ = DEFAULT_MAX_SETTINGS;
  std::queue<BaseObjectPtr<Http2Settings>> outstanding_settings_;

  std::vector<nghttp2_stream_write> outgoing_buffers_;
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

  void PushOutgoingBuffer(nghttp2_stream_write&& write);
  void CopyDataIntoOutgoing(const uint8_t* src, size_t src_length);
  void ClearOutgoing(int status);

  friend class Http2Scope;
  friend class Http2StreamListener;
};

class Http2SessionPerformanceEntry : public PerformanceEntry {
 public:
  Http2SessionPerformanceEntry(
      Environment* env,
      const Http2Session::Statistics& stats,
      nghttp2_session_type type) :
          PerformanceEntry(env, "Http2Session", "http2",
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

  void Notify(Local<Value> obj) {
    PerformanceEntry::Notify(env(), kind(), obj);
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

class Http2StreamPerformanceEntry : public PerformanceEntry {
 public:
  Http2StreamPerformanceEntry(
      Environment* env,
      int32_t id,
      const Http2Stream::Statistics& stats) :
          PerformanceEntry(env, "Http2Stream", "http2",
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

  void Notify(Local<Value> obj) {
    PerformanceEntry::Notify(env(), kind(), obj);
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
  Local<Value> Pack();

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

class ExternalHeader :
    public String::ExternalOneByteStringResource {
 public:
  explicit ExternalHeader(nghttp2_rcbuf* buf)
     : buf_(buf), vec_(nghttp2_rcbuf_get_buf(buf)) {
  }

  ~ExternalHeader() override {
    nghttp2_rcbuf_decref(buf_);
    buf_ = nullptr;
  }

  const char* data() const override {
    return const_cast<const char*>(reinterpret_cast<char*>(vec_.base));
  }

  size_t length() const override {
    return vec_.len;
  }

  static inline
  MaybeLocal<String> GetInternalizedString(Environment* env,
                                           const nghttp2_vec& vec) {
    return String::NewFromOneByte(env->isolate(),
                                  vec.base,
                                  v8::NewStringType::kInternalized,
                                  vec.len);
  }

  template <bool may_internalize>
  static MaybeLocal<String> New(Http2Session* session, nghttp2_rcbuf* buf) {
    Environment* env = session->env();
    if (nghttp2_rcbuf_is_static(buf)) {
      auto& static_str_map = env->isolate_data()->http2_static_strs;
      v8::Eternal<v8::String>& eternal = static_str_map[buf];
      if (eternal.IsEmpty()) {
        Local<String> str =
            GetInternalizedString(env, nghttp2_rcbuf_get_buf(buf))
                .ToLocalChecked();
        eternal.Set(env->isolate(), str);
        return str;
      }
      return eternal.Get(env->isolate());
    }

    nghttp2_vec vec = nghttp2_rcbuf_get_buf(buf);
    if (vec.len == 0) {
      nghttp2_rcbuf_decref(buf);
      return String::Empty(env->isolate());
    }

    if (may_internalize && vec.len < 64) {
      nghttp2_rcbuf_decref(buf);
      // This is a short header name, so there is a good chance V8 already has
      // it internalized.
      return GetInternalizedString(env, vec);
    }

    session->StopTrackingRcbuf(buf);
    ExternalHeader* h_str = new ExternalHeader(buf);
    MaybeLocal<String> str = String::NewExternalOneByte(env->isolate(), h_str);
    if (str.IsEmpty())
      delete h_str;

    return str;
  }

 private:
  nghttp2_rcbuf* buf_;
  nghttp2_vec vec_;
};

class Headers {
 public:
  Headers(Isolate* isolate, Local<Context> context, Local<Array> headers);
  ~Headers() = default;

  nghttp2_nv* operator*() {
    return reinterpret_cast<nghttp2_nv*>(*buf_);
  }

  size_t length() const {
    return count_;
  }

 private:
  size_t count_;
  MaybeStackBuffer<char, 3000> buf_;
};

class Origins {
 public:
  Origins(Isolate* isolate,
          Local<Context> context,
          Local<v8::String> origin_string,
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
