#ifndef SRC_QUIC_HTTP3_H_
#define SRC_QUIC_HTTP3_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "env.h"
#include "memory_tracker.h"
#include "node_http_common.h"
#include "node_mem.h"
#include "quic/session.h"
#include "quic/stream.h"
#include "quic/quic.h"
#include "v8.h"
#include <ngtcp2/ngtcp2.h>
#include <nghttp3/nghttp3.h>

namespace node {
namespace quic {

constexpr uint64_t kVarintMax = ((1ull << 62) - 1);
constexpr uint64_t DEFAULT_HTTP3_MAX_PUSHES = 0;
constexpr uint64_t DEFAULT_HTTP3_QPACK_MAX_TABLE_CAPACITY = 4096;
constexpr uint64_t DEFAULT_HTTP3_QPACK_BLOCKED_STREAMS = 100;

struct Http3RcBufferPointerTraits {
  typedef nghttp3_rcbuf rcbuf_t;
  typedef nghttp3_vec vector_t;

  static inline void inc(rcbuf_t* buf) {
    nghttp3_rcbuf_incref(buf);
  }
  static inline void dec(rcbuf_t* buf) {
    nghttp3_rcbuf_decref(buf);
  }
  static inline vector_t get_vec(const rcbuf_t* buf) {
    return nghttp3_rcbuf_get_buf(buf);
  }
  static inline bool is_static(const rcbuf_t* buf) {
    return nghttp3_rcbuf_is_static(buf);
  }
};

struct Http3HeadersTraits {
  typedef nghttp3_nv nv_t;
};

using Http3ConnectionPointer = DeleteFnPtr<nghttp3_conn, nghttp3_conn_del>;
using Http3RcBufferPointer = NgRcBufPointer<Http3RcBufferPointerTraits>;
using Http3Headers = NgHeaders<Http3HeadersTraits>;

struct Http3HeaderTraits {
  typedef Http3RcBufferPointer rcbufferpointer_t;
  typedef BindingState allocator_t;

  static const char* ToHttpHeaderName(int32_t token);
};

using Http3Header = NgHeader<Http3HeaderTraits>;

class Http3Application;

class Http3Application final : public Session::Application {
 public:
  // Configuration Options that are specific to HTTP3
  struct Options : public Session::Application::Options,
                   public MemoryRetainer {
    // The maximum header section size
    uint64_t max_field_section_size = kVarintMax;

    // The maximum number of concurrent push streams
    // accepted from a remote endpoint
    uint64_t max_pushes = DEFAULT_HTTP3_MAX_PUSHES;

    // The maximum size of the the qpack (header compression)
    // dynamic table.
    uint64_t qpack_max_table_capacity = DEFAULT_HTTP3_QPACK_MAX_TABLE_CAPACITY;

    // The maximum number of streams that can be blocked during
    // qpack decoding.
    uint64_t qpack_blocked_streams = DEFAULT_HTTP3_QPACK_BLOCKED_STREAMS;

    Options() = default;
    Options(const Options& other) noexcept;

    inline Options& operator=(const Options& other) noexcept {
      if (this == &other) return *this;
      this->~Options();
      return *new(this) Options(other);
    }

    SET_NO_MEMORY_INFO();
    SET_MEMORY_INFO_NAME(Http3Application::Options)
    SET_SELF_SIZE(Options)
  };

  Http3Application(
      Session* session,
      const std::shared_ptr<Application::Options>& options);

  Http3Application(const Http3Application& other) = delete;
  Http3Application(Http3Application&& other) = delete;
  Http3Application& operator=(const Http3Application& other) = delete;
  Http3Application& operator=(Http3Application&& other) = delete;

  bool Initialize() override;

  bool ReceiveStreamData(
      uint32_t flags,
      stream_id id,
      const uint8_t* data,
      size_t datalen,
      uint64_t offset) override;

  void AcknowledgeStreamData(
      stream_id id,
      uint64_t offset,
      size_t datalen) override;

  bool BlockStream(stream_id id) override;

  bool CanAddHeader(
      size_t current_count,
      size_t current_headers_length,
      size_t this_header_length) override;

  void ExtendMaxStreamsRemoteUni(uint64_t max_streams) override;

  void ExtendMaxStreamsRemoteBidi(uint64_t max_streams) override;

  void ExtendMaxStreamData(stream_id id, uint64_t max_data) override;

  void ResumeStream(stream_id id) override;

  void SetSessionTicketAppData(const SessionTicketAppData& app_data) override;

  SessionTicketAppData::Status GetSessionTicketAppData(
      const SessionTicketAppData& app_data,
      SessionTicketAppData::Flag flag) override;

  void StreamClose(stream_id id, error_code app_error_code);

  void StreamReset(stream_id id, error_code app_error_code);

  bool SendHeaders(
      stream_id id,
      Stream::HeadersKind kind,
      const v8::Local<v8::Array>& headers,
      Stream::SendHeadersFlags flags) override;

  inline const Options& options() const {
    return static_cast<const Options&>(Application::options());
  }

  SET_SELF_SIZE(Http3Application)
  SET_MEMORY_INFO_NAME(Http3Application)
  SET_NO_MEMORY_INFO()

 protected:
  int GetStreamData(StreamData* stream_data) override;
  bool ShouldSetFin(const StreamData& stream_data) override;
  bool StreamCommit(StreamData* stream_data, size_t datalen) override;

 private:
  BaseObjectPtr<Stream> FindOrCreateStream(stream_id id);
  bool CreateAndBindControlStream();
  bool CreateAndBindQPackStreams();
  void CreateConnection();
  void ScheduleStream(stream_id id);
  void UnscheduleStream(stream_id id);

  ssize_t ReadData(
      stream_id id,
      nghttp3_vec* vec,
      size_t veccnt,
      uint32_t* pflags);

  void AckedStreamData(stream_id id, size_t datalen);

  void StreamClosed(stream_id id, error_code app_error_code);

  void ReceiveData(stream_id id, const uint8_t* data, size_t datalen);

  void DeferredConsume(stream_id id, size_t consumed);

  void BeginHeaders(stream_id id);

  void ReceiveHeader(
      stream_id id,
      int32_t token,
      nghttp3_rcbuf* name,
      nghttp3_rcbuf* value,
      uint8_t flags);

  void EndHeaders(stream_id id);

  void BeginTrailers(stream_id id);

  void ReceiveTrailer(
      stream_id id,
      int32_t token,
      nghttp3_rcbuf* name,
      nghttp3_rcbuf* value,
      uint8_t flags);

  void EndTrailers(stream_id id);

  void BeginPushPromise(stream_id id, int64_t push_id);

  void ReceivePushPromise(
      stream_id id,
      int64_t push_id,
      int32_t token,
      nghttp3_rcbuf* name,
      nghttp3_rcbuf* value,
      uint8_t flags);

  void EndPushPromise(stream_id id, int64_t push_id);

  void CancelPush(int64_t push_id, stream_id id);

  void SendStopSending(stream_id id, error_code app_error_code);

  void PushStream(int64_t push_id, stream_id stream_id);

  void EndStream(stream_id id);

  void ResetStream(stream_id id, error_code app_error_code);

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

  static const nghttp3_callbacks callbacks_;

  static ssize_t OnReadData(
      nghttp3_conn* conn,
      int64_t stream_id,
      nghttp3_vec* vec,
      size_t veccnt,
      uint32_t* pflags,
      void* conn_user_data,
      void* stream_user_data);

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

  static int OnBeginTrailers(
      nghttp3_conn* conn,
      int64_t stream_id,
      void* conn_user_data,
      void* stream_user_data);

  static int OnReceiveTrailer(
      nghttp3_conn* conn,
      int64_t stream_id,
      int32_t token,
      nghttp3_rcbuf* name,
      nghttp3_rcbuf* value,
      uint8_t flags,
      void* conn_user_data,
      void* stream_user_data);

  static int OnEndTrailers(
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

  static int OnResetStream(
      nghttp3_conn* conn,
      int64_t stream_id,
      uint64_t app_error_code,
      void* conn_user_data,
      void* stream_user_data);
};

class Http3OptionsObject final : public BaseObject {
 public:
  static bool HasInstance(Environment* env, const v8::Local<v8::Value>& value);
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void Initialize(Environment* env, v8::Local<v8::Object> target);

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

  Http3OptionsObject(
      Environment* env,
      v8::Local<v8::Object> object,
      std::shared_ptr<Http3Application::Options> options =
          std::make_shared<Http3Application::Options>());

  inline std::shared_ptr<Http3Application::Options> options() const {
    return options_;
  }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(Http3OptionsObject)
  SET_SELF_SIZE(Http3OptionsObject)

 private:
  v8::Maybe<bool> SetOption(
      const v8::Local<v8::Object>& object,
      const v8::Local<v8::String>& name,
      uint64_t Http3Application::Options::*member);

  std::shared_ptr<Http3Application::Options> options_;
};

}  // namespace quic
}  // namespace node

#endif  // #if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_QUIC_HTTP3_H_
