#ifndef SRC_QUIC_NODE_QUIC_HTTP3_APPLICATION_H_
#define SRC_QUIC_NODE_QUIC_HTTP3_APPLICATION_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"
#include "node_http_common.h"
#include "node_mem.h"
#include "node_quic_session.h"
#include "node_quic_stream-inl.h"
#include "node_quic_util.h"
#include "v8.h"
#include <ngtcp2/ngtcp2.h>
#include <nghttp3/nghttp3.h>

#include <string>
namespace node {

namespace quic {

struct Http3RcBufferPointerTraits {
  typedef nghttp3_rcbuf rcbuf_t;
  typedef nghttp3_vec vector_t;

  static void inc(rcbuf_t* buf) {
    nghttp3_rcbuf_incref(buf);
  }
  static void dec(rcbuf_t* buf) {
    nghttp3_rcbuf_decref(buf);
  }
  static vector_t get_vec(const rcbuf_t* buf) {
    return nghttp3_rcbuf_get_buf(buf);
  }
  static bool is_static(const rcbuf_t* buf) {
    return nghttp3_rcbuf_is_static(buf);
  }
};
struct Http3HeadersTraits {
  typedef nghttp3_nv nv_t;
  static const uint8_t kNoneFlag = NGHTTP3_NV_FLAG_NONE;
};

using Http3RcBufferPointer = NgRcBufPointer<Http3RcBufferPointerTraits>;
using Http3Headers = NgHeaders<Http3HeadersTraits>;

constexpr uint64_t DEFAULT_QPACK_MAX_TABLE_CAPACITY = 4096;
constexpr uint64_t DEFAULT_QPACK_BLOCKED_STREAMS = 100;
constexpr size_t DEFAULT_MAX_HEADER_LIST_SIZE = 65535;
constexpr size_t DEFAULT_MAX_PUSHES = 65535;

using Http3ConnectionPointer = DeleteFnPtr<nghttp3_conn, nghttp3_conn_del>;

// The Http3Header instance is used for inbound (received) header
// name/value pairs. The class wraps instances of nghttp3_rcbuf
// (ref counted buffers) and provides a utility for accessing the
// contents of the buffers.
class Http3Header : public QuicHeader {
 public:
  static bool IsZeroLength(nghttp3_rcbuf* name, nghttp3_rcbuf* value) {
    return Http3RcBufferPointer::IsZeroLength(name) ||
           Http3RcBufferPointer::IsZeroLength(value);
  }

  Http3Header(int32_t token, nghttp3_rcbuf* name, nghttp3_rcbuf* value);
  Http3Header(Http3Header&& other) noexcept;

  v8::MaybeLocal<v8::String> GetName(QuicApplication* app) const override;
  v8::MaybeLocal<v8::String> GetValue(QuicApplication* app) const override;

  std::string name() const override;
  std::string value() const override;

  size_t length() const override;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(Http3Header)
  SET_SELF_SIZE(Http3Header)

 private:
  // nghttp3 uses a numeric identifier for a large number
  // of known HTTP header names. These allow us to use
  // static strings for those rather than allocating new
  // strings all of the time. The list of strings supported
  // is included in node_http_common.h
#define V1(name, value) case NGHTTP3_QPACK_TOKEN__##name: return value;
#define V2(name, value) case NGHTTP3_QPACK_TOKEN_##name: return value;
  static const char* ToHttpHeaderName(int32_t token) {
    switch (token) {
      default:
        // Fall through
      case -1: return nullptr;
      HTTP_SPECIAL_HEADERS(V1)
      HTTP_REGULAR_HEADERS(V2)
    }
  }
#undef V1
#undef V2

  int32_t token_ = -1;
  Http3RcBufferPointer name_;
  Http3RcBufferPointer value_;
};

struct Http3ApplicationConfig : public nghttp3_conn_settings {
  Http3ApplicationConfig() {
    nghttp3_conn_settings_default(this);
    qpack_max_table_capacity = DEFAULT_QPACK_MAX_TABLE_CAPACITY;
    qpack_blocked_streams = DEFAULT_QPACK_BLOCKED_STREAMS;
    max_header_list_size = DEFAULT_MAX_HEADER_LIST_SIZE;
    max_pushes = DEFAULT_MAX_PUSHES;
  }
  uint64_t max_header_pairs = DEFAULT_MAX_HEADER_LIST_PAIRS;
  uint64_t max_header_length = DEFAULT_MAX_HEADER_LENGTH;
};

// Http3Application is used whenever the h3 alpn identifier is used.
// It causes the QuicSession to apply HTTP/3 semantics to the connection,
// including handling of headers and other HTTP/3 specific processing.
class Http3Application final :
    public QuicApplication,
    public mem::NgLibMemoryManager<Http3Application, nghttp3_mem> {
 public:
  explicit Http3Application(QuicSession* session);

  bool Initialize() override;

  bool ReceiveStreamData(
    int64_t stream_id,
    int fin,
    const uint8_t* data,
    size_t datalen,
    uint64_t offset) override;

  void AcknowledgeStreamData(
      int64_t stream_id,
      uint64_t offset,
      size_t datalen) override;

  void StreamClose(int64_t stream_id, uint64_t app_error_code) override;

  void StreamReset(
      int64_t stream_id,
      uint64_t app_error_code) override;

  void ResumeStream(int64_t stream_id) override;

  void ExtendMaxStreamData(int64_t stream_id, uint64_t max_data) override;

  bool SubmitInformation(
      int64_t stream_id,
      v8::Local<v8::Array> headers) override;

  bool SubmitHeaders(
      int64_t stream_id,
      v8::Local<v8::Array> headers,
      uint32_t flags) override;

  bool SubmitTrailers(
      int64_t stream_id,
      v8::Local<v8::Array> headers) override;

  BaseObjectPtr<QuicStream> SubmitPush(
      int64_t id,
      v8::Local<v8::Array> headers) override;

  // Implementation for mem::NgLibMemoryManager
  void CheckAllocatedSize(size_t previous_size) const;
  void IncreaseAllocatedSize(size_t size);
  void DecreaseAllocatedSize(size_t size);

  SET_SELF_SIZE(Http3Application)
  SET_MEMORY_INFO_NAME(Http3Application)
  void MemoryInfo(MemoryTracker* tracker) const override;

 private:
  void SetConfig(int idx, uint64_t Http3ApplicationConfig::*member);

  nghttp3_conn* connection() const { return connection_.get(); }
  BaseObjectPtr<QuicStream> FindOrCreateStream(int64_t stream_id);

  bool CreateAndBindControlStream();
  bool CreateAndBindQPackStreams();
  int64_t CreateAndBindPushStream(int64_t push_id);

  int GetStreamData(StreamData* stream_data) override;

  bool BlockStream(int64_t stream_id) override;
  bool StreamCommit(StreamData* stream_data, size_t datalen) override;
  bool ShouldSetFin(const StreamData& data) override;
  bool SubmitPushPromise(
      int64_t id,
      int64_t* push_id,
      int64_t* stream_id,
      const Http3Headers& headers);
  bool SubmitInformation(int64_t id, const Http3Headers& headers);
  bool SubmitTrailers(int64_t id, const Http3Headers& headers);
  bool SubmitHeaders(int64_t id, const Http3Headers& headers, int32_t flags);

  ssize_t ReadData(
      int64_t stream_id,
      nghttp3_vec* vec,
      size_t veccnt,
      uint32_t* pflags);

  void AckedStreamData(int64_t stream_id, size_t datalen);
  void StreamClosed(int64_t stream_id, uint64_t app_error_code);
  void ReceiveData(int64_t stream_id, const uint8_t* data, size_t datalen);
  void DeferredConsume(int64_t stream_id, size_t consumed);
  void BeginHeaders(
      int64_t stream_id,
      QuicStreamHeadersKind kind = QUICSTREAM_HEADERS_KIND_NONE);
  bool ReceiveHeader(
      int64_t stream_id,
      int32_t token,
      nghttp3_rcbuf* name,
      nghttp3_rcbuf* value,
      uint8_t flags);
  void EndHeaders(int64_t stream_id, int64_t push_id = 0);
  void CancelPush(int64_t push_id, int64_t stream_id);
  void SendStopSending(int64_t stream_id, uint64_t app_error_code);
  void PushStream(int64_t push_id, int64_t stream_id);
  void EndStream(int64_t stream_id);

  bool is_control_stream(int64_t stream_id) const {
    return stream_id == control_stream_id_ ||
           stream_id == qpack_dec_stream_id_ ||
           stream_id == qpack_enc_stream_id_;
  }

  nghttp3_mem alloc_info_;
  Http3ConnectionPointer connection_;
  int64_t control_stream_id_;
  int64_t qpack_enc_stream_id_;
  int64_t qpack_dec_stream_id_;
  size_t current_nghttp3_memory_ = 0;

  Http3ApplicationConfig config_;

  void CreateConnection();

  static const nghttp3_conn_callbacks callbacks_[2];

  static int OnAckedStreamData(
      nghttp3_conn* conn,
      int64_t stream_id,
      size_t datalen,
      void* conn_user_data,
      void* stream_user_data);

  static int OnStreamClose(
      nghttp3_conn* conn,
      int64_t stream_id,
      uint64_t app_error_code,
      void* conn_user_data,
      void* stream_user_data);

  static int OnReceiveData(
      nghttp3_conn* conn,
      int64_t stream_id,
      const uint8_t* data,
      size_t datalen,
      void* conn_user_data,
      void* stream_user_data);

  static int OnDeferredConsume(
      nghttp3_conn* conn,
      int64_t stream_id,
      size_t consumed,
      void* conn_user_data,
      void* stream_user_data);

  static int OnBeginHeaders(
      nghttp3_conn* conn,
      int64_t stream_id,
      void* conn_user_data,
      void* stream_user_data);

  static int OnBeginTrailers(
      nghttp3_conn* conn,
      int64_t stream_id,
      void* conn_user_data,
      void* stream_user_data);

  static int OnReceiveHeader(
      nghttp3_conn* conn,
      int64_t stream_id,
      int32_t token,
      nghttp3_rcbuf* name,
      nghttp3_rcbuf* value,
      uint8_t flags,
      void* conn_user_data,
      void* stream_user_data);

  static int OnEndHeaders(
      nghttp3_conn* conn,
      int64_t stream_id,
      void* conn_user_data,
      void* stream_user_data);

  static int OnBeginPushPromise(
      nghttp3_conn* conn,
      int64_t stream_id,
      int64_t push_id,
      void* conn_user_data,
      void* stream_user_data);

  static int OnReceivePushPromise(
      nghttp3_conn* conn,
      int64_t stream_id,
      int64_t push_id,
      int32_t token,
      nghttp3_rcbuf* name,
      nghttp3_rcbuf* value,
      uint8_t flags,
      void* conn_user_data,
      void* stream_user_data);

  static int OnEndPushPromise(
      nghttp3_conn* conn,
      int64_t stream_id,
      int64_t push_id,
      void* conn_user_data,
      void* stream_user_data);

  static int OnCancelPush(
      nghttp3_conn* conn,
      int64_t push_id,
      int64_t stream_id,
      void* conn_user_data,
      void* stream_user_data);

  static int OnSendStopSending(
      nghttp3_conn* conn,
      int64_t stream_id,
      uint64_t app_error_code,
      void* conn_user_data,
      void* stream_user_data);

  static int OnPushStream(
      nghttp3_conn* conn,
      int64_t push_id,
      int64_t stream_id,
      void* conn_user_data);

  static int OnEndStream(
      nghttp3_conn* conn,
      int64_t stream_id,
      void* conn_user_data,
      void* stream_user_data);

  static ssize_t OnReadData(
      nghttp3_conn* conn,
      int64_t stream_id,
      nghttp3_vec* vec,
      size_t veccnt,
      uint32_t* pflags,
      void* conn_user_data,
      void* stream_user_data);
};

}  // namespace quic

}  // namespace node

#endif  // NODE_WANT_INTERNALS
#endif  // SRC_QUIC_NODE_QUIC_HTTP3_APPLICATION_H_
