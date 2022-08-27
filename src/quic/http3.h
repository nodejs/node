#pragma once

#include "quic/defs.h"
#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "quic.h"
#include "session.h"
#include "stream.h"

#include <base_object.h>
#include <env.h>
#include <memory_tracker.h>
#include <nghttp3/nghttp3.h>
#include <ngtcp2/ngtcp2.h>
#include <node_http_common.h>
#include <node_mem.h>
#include <v8.h>

namespace node {
namespace quic {

constexpr uint64_t kVarintMax = ((1ull << 62) - 1);
constexpr uint64_t DEFAULT_HTTP3_MAX_PUSHES = 0;
constexpr uint64_t DEFAULT_HTTP3_QPACK_MAX_TABLE_CAPACITY = 4096;
constexpr uint64_t DEFAULT_HTTP3_QPACK_BLOCKED_STREAMS = 100;

struct Http3RcBufferPointerTraits final {
  typedef nghttp3_rcbuf rcbuf_t;
  typedef nghttp3_vec vector_t;
  static void inc(rcbuf_t* buf);
  static void dec(rcbuf_t* buf);
  static vector_t get_vec(const rcbuf_t* buf);
  static bool is_static(const rcbuf_t* buf);
};

struct Http3HeadersTraits final {
  typedef nghttp3_nv nv_t;
};

using Http3ConnectionPointer = DeleteFnPtr<nghttp3_conn, nghttp3_conn_del>;
using Http3RcBufferPointer = NgRcBufPointer<Http3RcBufferPointerTraits>;
using Http3Headers = NgHeaders<Http3HeadersTraits>;

struct Http3HeaderTraits final {
  typedef Http3RcBufferPointer rcbufferpointer_t;
  typedef BindingState allocator_t;

  static const char* ToHttpHeaderName(int32_t token);
};

using Http3Header = NgHeader<Http3HeaderTraits>;

class Http3Application;

class Http3Application final : public Session::Application {
 public:
  Http3Application(Session* session, const Options& options);
  QUIC_NO_COPY_OR_MOVE(Http3Application)

  bool Start() override;

  // Called when the QUIC Session receives data for a stream. This is the
  // primary entry point of data into nghttp3 from ngtcp2.
  bool ReceiveStreamData(Stream* stream,
                         ReceiveStreamDataFlags flags,
                         const uint8_t* data,
                         size_t datalen,
                         uint64_t offset) override;

  void AcknowledgeStreamData(Stream* stream,
                             uint64_t offset,
                             size_t datalen) override;

  bool CanAddHeader(size_t current_count,
                    size_t current_headers_length,
                    size_t this_header_length) override;

  void SetStreamPriority(
      Stream* stream,
      StreamPriority priority = StreamPriority::DEFAULT,
      StreamPriorityFlags flags = StreamPriorityFlags::NONE) override;

  StreamPriority GetStreamPriority(Stream* stream) override;

  // Called when the QUIC Session detects that a stream is blocked by flow
  // control. This tells nghttp3 not to generate http3 stream data temporarily.
  bool BlockStream(stream_id id) override;

  void ResumeStream(stream_id id) override;

  void ExtendMaxStreams(EndpointLabel label,
                        Direction direction,
                        uint64_t max_streams) override;

  void ExtendMaxStreamData(Stream* stream, uint64_t max_data) override;

  void SetSessionTicketAppData(const SessionTicketAppData& app_data) override;

  SessionTicketAppData::Status GetSessionTicketAppData(
      const SessionTicketAppData& app_data,
      SessionTicketAppData::Flag flag) override;

  // Called by the QUIC Session when a stream is closed.
  void StreamClose(Stream* stream, v8::Maybe<QuicError> error) override;

  // Called by the QUIC Session when a stream reset is received from the remote
  // peer.
  void StreamReset(Stream* stream,
                   uint64_t final_size,
                   QuicError error) override;

  // TODO(@jasnell): Need to verify specifically when this is called.
  void StreamStopSending(Stream* stream, QuicError error) override;

  bool SendHeaders(stream_id id,
                   HeadersKind kind,
                   const v8::Local<v8::Array>& headers,
                   HeadersFlags flags = HeadersFlags::NONE) override;

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(Http3Application)
  SET_SELF_SIZE(Http3Application)

  inline nghttp3_conn* connection() const { return connection_.get(); }

 protected:
  int GetStreamData(StreamData* data) override;
  bool StreamCommit(StreamData* data, size_t datalen) override;
  bool ShouldSetFin(const StreamData& data) override;

 private:
  BaseObjectPtr<Stream> FindOrCreateStream(stream_id id,
                                           void* stream_user_data = nullptr);
  bool CreateAndBindControlStream();
  bool CreateAndBindQPackStreams();
  void CreateConnection();
  void ScheduleStream(stream_id id);
  void UnscheduleStream(stream_id id);

  ssize_t ReadData(stream_id id,
                   nghttp3_vec* vec,
                   size_t veccnt,
                   uint32_t* pflags);

  inline bool is_control_stream(int64_t stream_id) const {
    return stream_id == control_stream_id_ ||
           stream_id == qpack_dec_stream_id_ ||
           stream_id == qpack_enc_stream_id_;
  }

  nghttp3_mem alloc_info_;
  Http3ConnectionPointer connection_;
  stream_id control_stream_id_;
  stream_id qpack_enc_stream_id_;
  stream_id qpack_dec_stream_id_;

  // ====================================================================================
  // nghttp3 callbacks

  static const nghttp3_callbacks callbacks_;

  static int OnAckedStreamData(nghttp3_conn* conn,
                               stream_id id,
                               uint64_t datalen,
                               void* conn_user_data,
                               void* stream_user_data);

  static int OnStreamClose(nghttp3_conn* conn,
                           stream_id id,
                           uint64_t app_error_code,
                           void* conn_user_data,
                           void* stream_user_data);
  static int OnReceiveData(nghttp3_conn* conn,
                           stream_id id,
                           const uint8_t* data,
                           size_t datalen,
                           void* conn_user_data,
                           void* stream_user_data);
  static int OnDeferredConsume(nghttp3_conn* conn,
                               stream_id id,
                               size_t consumed,
                               void* conn_user_data,
                               void* stream_user_data);

  static int OnBeginHeaders(nghttp3_conn* conn,
                            stream_id id,
                            void* conn_user_data,
                            void* stream_user_data);
  static int OnReceiveHeader(nghttp3_conn* conn,
                             stream_id id,
                             int32_t token,
                             nghttp3_rcbuf* name,
                             nghttp3_rcbuf* value,
                             uint8_t flags,
                             void* conn_user_data,
                             void* stream_user_data);
  static int OnEndHeaders(nghttp3_conn* conn,
                          stream_id id,
                          int fin,
                          void* conn_user_data,
                          void* stream_user_data);

  static int OnBeginTrailers(nghttp3_conn* conn,
                             stream_id id,
                             void* conn_user_data,
                             void* stream_user_data);
  static int OnReceiveTrailer(nghttp3_conn* conn,
                              stream_id id,
                              int32_t token,
                              nghttp3_rcbuf* name,
                              nghttp3_rcbuf* value,
                              uint8_t flags,
                              void* conn_user_data,
                              void* stream_user_data);
  static int OnEndTrailers(nghttp3_conn* conn,
                           stream_id id,
                           int fin,
                           void* conn_user_data,
                           void* stream_user_data);
  static int OnStopSending(nghttp3_conn* conn,
                           stream_id id,
                           error_code app_error_code,
                           void* conn_user_data,
                           void* stream_user_data);
  static int OnEndStream(nghttp3_conn* conn,
                         stream_id id,
                         void* conn_user_data,
                         void* stream_user_data);
  static int OnResetStream(nghttp3_conn* conn,
                           stream_id id,
                           error_code app_error_code,
                           void* conn_user_data,
                           void* stream_user_data);
  static int OnShutdown(nghttp3_conn* conn, int64_t id, void* conn_user_data);

  static ssize_t OnReadData(nghttp3_conn* conn,
                            stream_id id,
                            nghttp3_vec* vec,
                            size_t veccnt,
                            uint32_t* pflags,
                            void* conn_user_data,
                            void* stream_user_data);
};

}  // namespace quic
}  // namespace node

#endif  // #if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
