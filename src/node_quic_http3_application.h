#ifndef SRC_NODE_QUIC_HTTP3_APPLICATION_H_
#define SRC_NODE_QUIC_HTTP3_APPLICATION_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"
#include "node_http_common.h"
#include "node_mem.h"
#include "node_quic_session.h"
#include "node_quic_stream.h"
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

class Http3Header : public QuicHeader {
 public:
  Http3Header(int32_t token, nghttp3_rcbuf* name, nghttp3_rcbuf* value);
  Http3Header(Http3Header&& other) noexcept;

  v8::MaybeLocal<v8::String> GetName(QuicApplication* app) const override;
  v8::MaybeLocal<v8::String> GetValue(QuicApplication* app) const override;

  std::string GetName() const override;
  std::string GetValue() const override;

 private:
  int32_t token_ = -1;
  Http3RcBufferPointer name_;
  Http3RcBufferPointer value_;
};

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

  void StreamOpen(int64_t stream_id) override;
  void StreamClose(int64_t stream_id, uint64_t app_error_code) override;

  void StreamReset(
      int64_t stream_id,
      uint64_t final_size,
      uint64_t app_error_code) override;

  void ExtendMaxStreamsRemoteUni(uint64_t max_streams) override;
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

  bool SendPendingData() override;
  bool SendStreamData(QuicStream* stream) override;

  // Implementation for mem::NgLibMemoryManager
  void CheckAllocatedSize(size_t previous_size) const;
  void IncreaseAllocatedSize(size_t size);
  void DecreaseAllocatedSize(size_t size);

  SET_SELF_SIZE(Http3Application)
  SET_MEMORY_INFO_NAME(Http3Application)
  void MemoryInfo(MemoryTracker* tracker) const override;

 private:
  nghttp3_conn* Connection() { return connection_.get(); }
  QuicStream* FindOrCreateStream(int64_t stream_id);

  bool CreateAndBindControlStream();
  bool CreateAndBindQPackStreams();

  bool StreamCommit(int64_t stream_id, ssize_t datalen);
  void SetStreamFin(int64_t stream_id);

  ssize_t H3ReadData(
      int64_t stream_id,
      nghttp3_vec* vec,
      size_t veccnt,
      uint32_t* pflags);

  void H3AckedStreamData(int64_t stream_id, size_t datalen);
  void H3StreamClose(int64_t stream_id, uint64_t app_error_code);
  void H3ReceiveData(int64_t stream_id, const uint8_t* data, size_t datalen);
  void H3DeferredConsume(int64_t stream_id, size_t consumed);
  void H3BeginHeaders(
      int64_t stream_id,
      QuicStreamHeadersKind kind = QUICSTREAM_HEADERS_KIND_NONE);
  bool H3ReceiveHeader(
      int64_t stream_id,
      int32_t token,
      nghttp3_rcbuf* name,
      nghttp3_rcbuf* value,
      uint8_t flags);
  void H3EndHeaders(int64_t stream_id);
  int H3BeginPushPromise(int64_t stream_id, int64_t push_id);
  bool H3ReceivePushPromise(
      int64_t stream_id,
      int64_t push_id,
      int32_t token,
      nghttp3_rcbuf* name,
      nghttp3_rcbuf* value,
      uint8_t flags);
  int H3EndPushPromise(int64_t stream_id, int64_t push_id);
  void H3CancelPush(int64_t push_id, int64_t stream_id);
  void H3SendStopSending(int64_t stream_id, uint64_t app_error_code);
  int H3PushStream(int64_t push_id, int64_t stream_id);
  int H3EndStream(int64_t stream_id);

  bool IsControlStream(int64_t stream_id) {
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

  uint64_t qpack_max_table_capacity_ = DEFAULT_QPACK_MAX_TABLE_CAPACITY;
  uint64_t qpack_blocked_streams_ = DEFAULT_QPACK_BLOCKED_STREAMS;
  size_t max_header_list_size_ = DEFAULT_MAX_HEADER_LIST_SIZE;
  size_t max_pushes_ = DEFAULT_MAX_PUSHES;

  nghttp3_conn* CreateConnection(nghttp3_conn_settings* settings);

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
#endif  // SRC_NODE_QUIC_HTTP3_APPLICATION_H_
