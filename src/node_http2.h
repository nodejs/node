#ifndef SRC_NODE_HTTP2_H_
#define SRC_NODE_HTTP2_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "nghttp2/nghttp2.h"
#include "node_http2_state.h"
#include "stream_base-inl.h"
#include "string_bytes.h"

#include <queue>

namespace node {
namespace http2 {

using v8::Array;
using v8::Context;
using v8::EscapableHandleScope;
using v8::Isolate;
using v8::MaybeLocal;

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
#define DEBUG_HTTP2SESSION(session, message)                                  \
  do {                                                                        \
    DEBUG_HTTP2("Http2Session %s (%.0lf) " message "\n",                      \
                   session->TypeName(),                                       \
                   session->get_async_id());                                  \
  } while (0)
#define DEBUG_HTTP2SESSION2(session, message, ...)                            \
  do {                                                                        \
    DEBUG_HTTP2("Http2Session %s (%.0lf) " message "\n",                      \
                   session->TypeName(),                                       \
                   session->get_async_id(),                                   \
                  __VA_ARGS__);                                               \
  } while (0)
#define DEBUG_HTTP2STREAM(stream, message)                                    \
  do {                                                                        \
    DEBUG_HTTP2("Http2Stream %d (%.0lf) [Http2Session %s (%.0lf)] " message   \
                "\n", stream->id(), stream->get_async_id(),                   \
                stream->session()->TypeName(),                                \
                stream->session()->get_async_id());                           \
  } while (0)
#define DEBUG_HTTP2STREAM2(stream, message, ...)                              \
  do {                                                                        \
    DEBUG_HTTP2("Http2Stream %d (%.0lf) [Http2Session %s (%.0lf)] " message   \
                "\n", stream->id(), stream->get_async_id(),                   \
                stream->session()->TypeName(),                                \
                stream->session()->get_async_id(),                            \
                __VA_ARGS__);                                                 \
  } while (0)
#else
#define DEBUG_HTTP2(...) do {} while (0)
#define DEBUG_HTTP2SESSION(...) do {} while (0)
#define DEBUG_HTTP2SESSION2(...) do {} while (0)
#define DEBUG_HTTP2STREAM(...) do {} while (0)
#define DEBUG_HTTP2STREAM2(...) do {} while (0)
#endif

#define DEFAULT_MAX_PINGS 10
#define DEFAULT_SETTINGS_HEADER_TABLE_SIZE 4096
#define DEFAULT_SETTINGS_ENABLE_PUSH 1
#define DEFAULT_SETTINGS_INITIAL_WINDOW_SIZE 65535
#define DEFAULT_SETTINGS_MAX_FRAME_SIZE 16384
#define DEFAULT_SETTINGS_MAX_HEADER_LIST_SIZE 65535
#define MAX_MAX_FRAME_SIZE 16777215
#define MIN_MAX_FRAME_SIZE DEFAULT_SETTINGS_MAX_FRAME_SIZE
#define MAX_INITIAL_WINDOW_SIZE 2147483647

#define MAX_MAX_HEADER_LIST_SIZE 16777215u
#define DEFAULT_MAX_HEADER_LIST_PAIRS 128u

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
  NGHTTP2_STREAM_FLAG_DESTROYED = 0x10,
  // Stream has trailers
  NGHTTP2_STREAM_FLAG_TRAILERS = 0x20
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
  uint8_t flags = 0;
};



struct nghttp2_stream_write_t {
  void* data;
  int status;
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
  // Padding will ensure all data frames are maxFrameSize
  PADDING_STRATEGY_MAX,
  // Padding will be determined via a JS callback. Note that this can be
  // expensive because the callback is called once for every DATA and
  // HEADERS frame. For performance reasons, this strategy should be
  // avoided.
  PADDING_STRATEGY_CALLBACK
};

// These are the error codes provided by the underlying nghttp2 implementation.
#define NGHTTP2_ERROR_CODES(V)                                                 \
  V(NGHTTP2_ERR_INVALID_ARGUMENT)                                              \
  V(NGHTTP2_ERR_BUFFER_ERROR)                                                  \
  V(NGHTTP2_ERR_UNSUPPORTED_VERSION)                                           \
  V(NGHTTP2_ERR_WOULDBLOCK)                                                    \
  V(NGHTTP2_ERR_PROTO)                                                         \
  V(NGHTTP2_ERR_INVALID_FRAME)                                                 \
  V(NGHTTP2_ERR_EOF)                                                           \
  V(NGHTTP2_ERR_DEFERRED)                                                      \
  V(NGHTTP2_ERR_STREAM_ID_NOT_AVAILABLE)                                       \
  V(NGHTTP2_ERR_STREAM_CLOSED)                                                 \
  V(NGHTTP2_ERR_STREAM_CLOSING)                                                \
  V(NGHTTP2_ERR_STREAM_SHUT_WR)                                                \
  V(NGHTTP2_ERR_INVALID_STREAM_ID)                                             \
  V(NGHTTP2_ERR_INVALID_STREAM_STATE)                                          \
  V(NGHTTP2_ERR_DEFERRED_DATA_EXIST)                                           \
  V(NGHTTP2_ERR_START_STREAM_NOT_ALLOWED)                                      \
  V(NGHTTP2_ERR_GOAWAY_ALREADY_SENT)                                           \
  V(NGHTTP2_ERR_INVALID_HEADER_BLOCK)                                          \
  V(NGHTTP2_ERR_INVALID_STATE)                                                 \
  V(NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE)                                     \
  V(NGHTTP2_ERR_FRAME_SIZE_ERROR)                                              \
  V(NGHTTP2_ERR_HEADER_COMP)                                                   \
  V(NGHTTP2_ERR_FLOW_CONTROL)                                                  \
  V(NGHTTP2_ERR_INSUFF_BUFSIZE)                                                \
  V(NGHTTP2_ERR_PAUSE)                                                         \
  V(NGHTTP2_ERR_TOO_MANY_INFLIGHT_SETTINGS)                                    \
  V(NGHTTP2_ERR_PUSH_DISABLED)                                                 \
  V(NGHTTP2_ERR_DATA_EXIST)                                                    \
  V(NGHTTP2_ERR_SESSION_CLOSING)                                               \
  V(NGHTTP2_ERR_HTTP_HEADER)                                                   \
  V(NGHTTP2_ERR_HTTP_MESSAGING)                                                \
  V(NGHTTP2_ERR_REFUSED_STREAM)                                                \
  V(NGHTTP2_ERR_INTERNAL)                                                      \
  V(NGHTTP2_ERR_CANCEL)                                                        \
  V(NGHTTP2_ERR_FATAL)                                                         \
  V(NGHTTP2_ERR_NOMEM)                                                         \
  V(NGHTTP2_ERR_CALLBACK_FAILURE)                                              \
  V(NGHTTP2_ERR_BAD_CLIENT_MAGIC)                                              \
  V(NGHTTP2_ERR_FLOODED)

const char* nghttp2_errname(int rv) {
  switch (rv) {
#define V(code) case code: return #code;
  NGHTTP2_ERROR_CODES(V)
#undef V
    default:
      return "NGHTTP2_UNKNOWN_ERROR";
  }
}

enum session_state_flags {
  SESSION_STATE_NONE = 0x0,
  SESSION_STATE_DESTROYING = 0x1
};

// This allows for 4 default-sized frames with their frame headers
static const size_t kAllocBufferSize = 4 * (16384 + 9);

typedef uint32_t(*get_setting)(nghttp2_session* session,
                               nghttp2_settings_id id);

class Http2Session;
class Http2Stream;

// The Http2Options class is used to parse the options object passed in to
// a Http2Session object and convert those into an appropriate nghttp2_option
// struct. This is the primary mechanism by which the Http2Session object is
// configured.
class Http2Options {
 public:
  explicit Http2Options(Environment* env);

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
    padding_strategy_ = static_cast<padding_strategy_type>(val);
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

 private:
  nghttp2_option* options_;
  uint32_t max_header_pairs_ = DEFAULT_MAX_HEADER_LIST_PAIRS;
  padding_strategy_type padding_strategy_ = PADDING_STRATEGY_NONE;
  size_t max_outstanding_pings_ = DEFAULT_MAX_PINGS;
};

// The Http2Settings class is used to parse the settings passed in for
// an Http2Session, converting those into an array of nghttp2_settings_entry
// structs.
class Http2Settings {
 public:
  explicit Http2Settings(Environment* env);

  size_t length() const { return count_; }

  nghttp2_settings_entry* operator*() {
    return *entries_;
  }

  // Returns a Buffer instance with the serialized SETTINGS payload
  inline Local<Value> Pack();

  // Resets the default values in the settings buffer
  static inline void RefreshDefaults(Environment* env);

  // Update the local or remote settings for the given session
  static inline void Update(Environment* env,
                            Http2Session* session,
                            get_setting fn);

 private:
  Environment* env_;
  size_t count_ = 0;
  MaybeStackBuffer<nghttp2_settings_entry, IDX_SETTINGS_COUNT> entries_;
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

class Http2Stream : public AsyncWrap,
                    public StreamBase {
 public:
  Http2Stream(Http2Session* session,
              int32_t id,
              nghttp2_headers_category category = NGHTTP2_HCAT_HEADERS,
              int options = 0);
  ~Http2Stream() override;

  nghttp2_stream* operator*();

  Http2Session* session() { return session_; }

  // Queue outbound chunks of data to be sent on this stream
  inline int Write(
      nghttp2_stream_write_t* req,
      const uv_buf_t bufs[],
      unsigned int nbufs,
      nghttp2_stream_write_cb cb);

  inline void AddChunk(const uint8_t* data, size_t len);

  inline void FlushDataChunks();

  // Process a Data Chunk
  void OnDataChunk(uv_buf_t* chunk);


  // Required for StreamBase
  int ReadStart() override;

  // Required for StreamBase
  int ReadStop() override;

  // Required for StreamBase
  int DoShutdown(ShutdownWrap* req_wrap);

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
  inline Http2Stream* SubmitPushPromise(
      nghttp2_nv* nva,
      size_t len,
      int32_t* ret,
      int options = 0);


  inline void Close(int32_t code);

  // Shutdown the writable side of the stream
  inline void Shutdown();

  // Destroy this stream instance and free all held memory.
  inline void Destroy();

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

  inline bool AddHeader(nghttp2_rcbuf* name,
                        nghttp2_rcbuf* value,
                        uint8_t flags);

  inline nghttp2_header* headers() {
    return current_headers_.data();
  }

  inline nghttp2_headers_category headers_category() const {
    return current_headers_category_;
  }

  inline size_t headers_count() const {
    return current_headers_.size();
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

  AsyncWrap* GetAsyncWrap() override { return static_cast<AsyncWrap*>(this); }
  void* Cast() override { return reinterpret_cast<void*>(this); }

  int DoWrite(WriteWrap* w, uv_buf_t* bufs, size_t count,
              uv_stream_t* send_handle) override;

  size_t self_size() const override { return sizeof(*this); }

  // Handling Trailer Headers
  class SubmitTrailers {
   public:
    inline void Submit(nghttp2_nv* trailers, size_t length) const;

    inline SubmitTrailers(Http2Session* sesion,
                          Http2Stream* stream,
                          uint32_t* flags);

   private:
    Http2Session* const session_;
    Http2Stream* const stream_;
    uint32_t* const flags_;

    friend class Http2Stream;
  };

  void OnTrailers(const SubmitTrailers& submit_trailers);

  // JavaScript API
  static void GetID(const FunctionCallbackInfo<Value>& args);
  static void Destroy(const FunctionCallbackInfo<Value>& args);
  static void FlushData(const FunctionCallbackInfo<Value>& args);
  static void Priority(const FunctionCallbackInfo<Value>& args);
  static void PushPromise(const FunctionCallbackInfo<Value>& args);
  static void RefreshState(const FunctionCallbackInfo<Value>& args);
  static void Info(const FunctionCallbackInfo<Value>& args);
  static void RespondFD(const FunctionCallbackInfo<Value>& args);
  static void Respond(const FunctionCallbackInfo<Value>& args);
  static void RstStream(const FunctionCallbackInfo<Value>& args);

  class Provider;

 private:
  Http2Session* session_;                       // The Parent HTTP/2 Session
  int32_t id_;                                  // The Stream Identifier
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

  // Inbound Data... This is the data received via DATA frames for this stream.
  std::queue<uv_buf_t> data_chunks_;

  // Outbound Data... This is the data written by the JS layer that is
  // waiting to be written out to the socket.
  std::queue<nghttp2_stream_write*> queue_;
  unsigned int queue_index_ = 0;
  size_t queue_offset_ = 0;
  int64_t fd_offset_ = 0;
  int64_t fd_length_ = -1;
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

class Http2Stream::Provider::FD : public Http2Stream::Provider {
 public:
  FD(int options, int fd);
  FD(Http2Stream* stream, int options, int fd);

  static ssize_t OnRead(nghttp2_session* session,
                        int32_t id,
                        uint8_t* buf,
                        size_t length,
                        uint32_t* flags,
                        nghttp2_data_source* source,
                        void* user_data);
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


class Http2Session : public AsyncWrap {
 public:
  Http2Session(Environment* env,
               Local<Object> wrap,
               nghttp2_session_type type = NGHTTP2_SESSION_SERVER);
  ~Http2Session() override;

  class Http2Ping;

  void Start();
  void Stop();
  void Close();
  void Consume(Local<External> external);
  void Unconsume();

  bool Ping(v8::Local<v8::Function> function);

  inline void SendPendingData();

  // Submits a new request. If the request is a success, assigned
  // will be a pointer to the Http2Stream instance assigned.
  // This only works if the session is a client session.
  inline Http2Stream* SubmitRequest(
      nghttp2_priority_spec* prispec,
      nghttp2_nv* nva,
      size_t len,
      int32_t* ret,
      int options = 0);

  nghttp2_session_type type() const { return session_type_; }

  inline nghttp2_session* session() const { return session_; }

  nghttp2_session* operator*() { return session_; }

  uint32_t GetMaxHeaderPairs() const { return max_header_pairs_; }

  inline const char* TypeName();

  inline void MarkDestroying() { flags_ |= SESSION_STATE_DESTROYING; }
  inline bool IsDestroying() { return flags_ & SESSION_STATE_DESTROYING; }

  // Returns pointer to the stream, or nullptr if stream does not exist
  inline Http2Stream* FindStream(int32_t id);

  // Adds a stream instance to this session
  inline void AddStream(Http2Stream* stream);

  // Removes a stream instance from this session
  inline void RemoveStream(int32_t id);

  // Sends a notice to the connected peer that the session is shutting down.
  inline void SubmitShutdownNotice();

  // Submits a SETTINGS frame to the connected peer.
  inline void Settings(const nghttp2_settings_entry iv[], size_t niv);

  // Write data to the session
  inline ssize_t Write(const uv_buf_t* bufs, size_t nbufs);

  inline void SetChunksSinceLastWrite(size_t n = 0);

  size_t self_size() const override { return sizeof(*this); }

  char* stream_alloc() {
    return stream_buf_;
  }

  inline void GetTrailers(Http2Stream* stream, uint32_t* flags);

  static void OnStreamAllocImpl(size_t suggested_size,
                                uv_buf_t* buf,
                                void* ctx);
  static void OnStreamReadImpl(ssize_t nread,
                               const uv_buf_t* bufs,
                               uv_handle_type pending,
                               void* ctx);

  // The JavaScript API
  static void New(const FunctionCallbackInfo<Value>& args);
  static void Consume(const FunctionCallbackInfo<Value>& args);
  static void Unconsume(const FunctionCallbackInfo<Value>& args);
  static void Destroying(const FunctionCallbackInfo<Value>& args);
  static void Destroy(const FunctionCallbackInfo<Value>& args);
  static void Settings(const FunctionCallbackInfo<Value>& args);
  static void Request(const FunctionCallbackInfo<Value>& args);
  static void SetNextStreamID(const FunctionCallbackInfo<Value>& args);
  static void ShutdownNotice(const FunctionCallbackInfo<Value>& args);
  static void Goaway(const FunctionCallbackInfo<Value>& args);
  static void UpdateChunksSent(const FunctionCallbackInfo<Value>& args);
  static void RefreshState(const FunctionCallbackInfo<Value>& args);
  static void Ping(const FunctionCallbackInfo<Value>& args);

  template <get_setting fn>
  static void RefreshSettings(const FunctionCallbackInfo<Value>& args);

  template <get_setting fn>
  static void GetSettings(const FunctionCallbackInfo<Value>& args);

  void Send(WriteWrap* req, char* buf, size_t length);
  WriteWrap* AllocateSend();

  uv_loop_t* event_loop() const {
    return env()->event_loop();
  }

  Http2Ping* PopPing();
  bool AddPing(Http2Ping* ping);

 private:
  // Frame Padding Strategies
  inline ssize_t OnMaxFrameSizePadding(size_t frameLength,
                                       size_t maxPayloadLen);
  inline ssize_t OnCallbackPadding(size_t frame,
                                   size_t maxPayloadLen);

  // Frame Handler
  inline void HandleDataFrame(const nghttp2_frame* frame);
  inline void HandleGoawayFrame(const nghttp2_frame* frame);
  inline void HandleHeadersFrame(const nghttp2_frame* frame);
  inline void HandlePriorityFrame(const nghttp2_frame* frame);
  inline void HandleSettingsFrame(const nghttp2_frame* frame);
  inline void HandlePingFrame(const nghttp2_frame* frame);

  // nghttp2 callbacks
  static inline int OnBeginHeadersCallback(
      nghttp2_session* session,
      const nghttp2_frame* frame,
      void* user_data);
  static inline int OnHeaderCallback(
      nghttp2_session* session,
      const nghttp2_frame* frame,
      nghttp2_rcbuf* name,
      nghttp2_rcbuf* value,
      uint8_t flags,
      void* user_data);
  static inline int OnFrameReceive(
      nghttp2_session* session,
      const nghttp2_frame* frame,
      void* user_data);
  static inline int OnFrameNotSent(
      nghttp2_session* session,
      const nghttp2_frame* frame,
      int error_code,
      void* user_data);
  static inline int OnStreamClose(
      nghttp2_session* session,
      int32_t id,
      uint32_t code,
      void* user_data);
  static inline int OnInvalidHeader(
      nghttp2_session* session,
      const nghttp2_frame* frame,
      nghttp2_rcbuf* name,
      nghttp2_rcbuf* value,
      uint8_t flags,
      void* user_data);
  static inline int OnDataChunkReceived(
      nghttp2_session* session,
      uint8_t flags,
      int32_t id,
      const uint8_t* data,
      size_t len,
      void* user_data);
  static inline ssize_t OnSelectPadding(
      nghttp2_session* session,
      const nghttp2_frame* frame,
      size_t maxPayloadLen,
      void* user_data);
  static inline int OnNghttpError(
      nghttp2_session* session,
      const char* message,
      size_t len,
      void* user_data);


  static inline ssize_t OnStreamReadFD(
      nghttp2_session* session,
      int32_t id,
      uint8_t* buf,
      size_t length,
      uint32_t* flags,
      nghttp2_data_source* source,
      void* user_data);
  static inline ssize_t OnStreamRead(
      nghttp2_session* session,
      int32_t id,
      uint8_t* buf,
      size_t length,
      uint32_t* flags,
      nghttp2_data_source* source,
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

  // The session type: client or server
  nghttp2_session_type session_type_;

  // The maximum number of header pairs permitted for streams on this session
  uint32_t max_header_pairs_ = DEFAULT_MAX_HEADER_LIST_PAIRS;

  // The collection of active Http2Streams associated with this session
  std::unordered_map<int32_t, Http2Stream*> streams_;

  int flags_ = SESSION_STATE_NONE;

  // The StreamBase instance being used for i/o
  StreamBase* stream_;
  StreamResource::Callback<StreamResource::AllocCb> prev_alloc_cb_;
  StreamResource::Callback<StreamResource::ReadCb> prev_read_cb_;
  padding_strategy_type padding_strategy_ = PADDING_STRATEGY_NONE;

  // use this to allow timeout tracking during long-lasting writes
  uint32_t chunks_sent_since_last_write_ = 0;

  uv_prepare_t* prep_ = nullptr;
  char stream_buf_[kAllocBufferSize];

  size_t max_outstanding_pings_ = DEFAULT_MAX_PINGS;
  std::queue<Http2Ping*> outstanding_pings_;
};

class Http2Session::Http2Ping : public AsyncWrap {
 public:
  explicit Http2Ping(Http2Session* session);
  ~Http2Ping();

  size_t self_size() const override { return sizeof(*this); }

  void Send(uint8_t* payload);
  void Done(bool ack, const uint8_t* payload = nullptr);

 private:
  Http2Session* session_;
  uint64_t startTime_;
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

  template<bool may_internalize>
  static MaybeLocal<String> New(Environment* env, nghttp2_rcbuf* buf) {
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
      // This is a short header name, so there is a good chance V8 already has
      // it internalized.
      return GetInternalizedString(env, vec);
    }

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
  ~Headers() {}

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

}  // namespace http2
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_HTTP2_H_
