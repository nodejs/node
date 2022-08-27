#include "session.h"
#include <aliased_struct-inl.h>
#include <async_wrap-inl.h>
#include <base_object-inl.h>
#include <crypto/crypto_common.h>
#include <crypto/crypto_keys.h>
#include <crypto/crypto_random.h>
#include <crypto/crypto_util.h>
#include <debug_utils-inl.h>
#include <env-inl.h>
#include <memory_tracker-inl.h>
#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>
#include <node_errors.h>
#include <node_sockaddr-inl.h>
#include <openssl/tls1.h>
#include <stream_base-inl.h>
#include <timer_wrap-inl.h>
#include <util.h>
#include <v8.h>
#include "crypto.h"
#include "defs.h"
#include "endpoint.h"
#include "http3.h"
#include "quic.h"
#include "stream.h"
#include "uv.h"

namespace node {

using crypto::CSPRNG;
using v8::Array;
using v8::ArrayBuffer;
using v8::ArrayBufferView;
using v8::BackingStore;
using v8::BigInt;
using v8::False;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Int32;
using v8::Integer;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::Nothing;
using v8::Number;
using v8::Object;
using v8::PropertyAttribute;
using v8::String;
using v8::True;
using v8::Uint32;
using v8::Uint8Array;
using v8::Undefined;
using v8::Value;

namespace quic {

// =============================================================================

class LogStream final : public AsyncWrap, public StreamBase {
 public:
  HAS_INSTANCE()
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env) {
    auto& state = BindingState::Get(env);
    v8::Local<v8::FunctionTemplate> tmpl =
        state.logstream_constructor_template();
    if (tmpl.IsEmpty()) {
      tmpl = v8::FunctionTemplate::New(env->isolate());
      tmpl->Inherit(AsyncWrap::GetConstructorTemplate(env));
      tmpl->InstanceTemplate()->SetInternalFieldCount(
          StreamBase::kInternalFieldCount);
      tmpl->SetClassName(state.logstream_string());
      StreamBase::AddMethods(env, tmpl);
      state.set_logstream_constructor_template(tmpl);
    }
    return tmpl;
  }
  static BaseObjectPtr<LogStream> Create(Environment* env) {
    v8::Local<v8::Object> obj;
    if (!GetConstructorTemplate(env)
             ->InstanceTemplate()
             ->NewInstance(env->context())
             .ToLocal(&obj)) {
      return BaseObjectPtr<LogStream>();
    }
    return MakeDetachedBaseObject<LogStream>(env, obj);
  }

  LogStream(Environment* env, Local<Object> obj)
      : AsyncWrap(env, obj, AsyncWrap::PROVIDER_QUICLOGSTREAM),
        StreamBase(env) {
    MakeWeak();
    StreamBase::AttachToObject(GetObject());
  }

  void Emit(const uint8_t* data, size_t len, uint32_t flags) {
    size_t remaining = len;
    while (remaining != 0) {
      uv_buf_t buf = EmitAlloc(len);
      ssize_t avail = std::min<size_t>(remaining, buf.len);
      memcpy(buf.base, data, avail);
      remaining -= avail;
      data += avail;
      if (reading_)
        EmitRead(avail, buf);
      else
        buffer_.emplace_back(Chunk{buf, avail});
    }

    if (ended_ && flags & NGTCP2_QLOG_WRITE_FLAG_FIN) EmitRead(UV_EOF);
  }

  void Emit(const std::string& line, uint32_t flags = 0) {
    Emit(reinterpret_cast<const uint8_t*>(line.c_str()), line.length(), flags);
  }

  inline void End() { ended_ = true; }

  int ReadStart() override {
    reading_ = true;
    for (const Chunk& chunk : buffer_) EmitRead(chunk.avail, chunk.buf);
    return 0;
  }

  int ReadStop() override {
    reading_ = false;
    return 0;
  }

  inline int DoShutdown(ShutdownWrap* req_wrap) override { UNREACHABLE(); }

  inline int DoWrite(WriteWrap* w,
                     uv_buf_t* bufs,
                     size_t count,
                     uv_stream_t* send_handle) override {
    UNREACHABLE();
  }

  inline bool IsAlive() override { return !ended_; }
  inline bool IsClosing() override { return ended_; }
  inline AsyncWrap* GetAsyncWrap() override { return this; }

  SET_NO_MEMORY_INFO();
  SET_MEMORY_INFO_NAME(LogStream);
  SET_SELF_SIZE(LogStream);

 private:
  struct Chunk {
    uv_buf_t buf;
    ssize_t avail;
  };

  bool ended_ = false;
  bool reading_ = false;
  std::vector<Chunk> buffer_;
};

// =============================================================================
// PreferredAddress

Maybe<PreferredAddress::AddressInfo> PreferredAddress::ipv4() const {
  if (!paddr_->ipv4_present) return Nothing<PreferredAddress::AddressInfo>();

  AddressInfo address;
  address.family = AF_INET;
  address.port = paddr_->ipv4_port;

  char host[NI_MAXHOST];
  // Return an empty string if unable to convert...
  if (uv_inet_ntop(AF_INET, paddr_->ipv4_addr, host, sizeof(host)) == 0)
    address.address = std::string(host);

  return Just(address);
}

Maybe<PreferredAddress::AddressInfo> PreferredAddress::ipv6() const {
  if (!paddr_->ipv6_present) return Nothing<PreferredAddress::AddressInfo>();

  AddressInfo address;
  address.family = AF_INET6;
  address.port = paddr_->ipv6_port;

  char host[NI_MAXHOST];
  // Return an empty string if unable to convert...
  if (uv_inet_ntop(AF_INET6, paddr_->ipv6_addr, host, sizeof(host)) == 0)
    address.address = std::string(host);

  return Just(address);
}

bool PreferredAddress::Use(const AddressInfo& address) const {
  uv_getaddrinfo_t req;

  if (!Resolve(address, &req)) return false;

  dest_->remote.addrlen = req.addrinfo->ai_addrlen;
  memcpy(dest_->remote.addr, req.addrinfo->ai_addr, req.addrinfo->ai_addrlen);
  uv_freeaddrinfo(req.addrinfo);
  return true;
}

void PreferredAddress::CopyToTransportParams(ngtcp2_transport_params* params,
                                             const sockaddr* addr) {
  CHECK_NOT_NULL(params);
  CHECK_NOT_NULL(addr);
  params->preferred_address_present = 1;
  switch (addr->sa_family) {
    case AF_INET: {
      const sockaddr_in* src = reinterpret_cast<const sockaddr_in*>(addr);
      memcpy(params->preferred_address.ipv4_addr,
             &src->sin_addr,
             sizeof(params->preferred_address.ipv4_addr));
      params->preferred_address.ipv4_port = SocketAddress::GetPort(addr);
      return;
    }
    case AF_INET6: {
      const sockaddr_in6* src = reinterpret_cast<const sockaddr_in6*>(addr);
      memcpy(params->preferred_address.ipv6_addr,
             &src->sin6_addr,
             sizeof(params->preferred_address.ipv6_addr));
      params->preferred_address.ipv6_port = SocketAddress::GetPort(addr);
      return;
    }
  }
  UNREACHABLE();
}

bool PreferredAddress::Resolve(const AddressInfo& address,
                               uv_getaddrinfo_t* req) const {
  addrinfo hints{};
  hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;
  hints.ai_family = address.family;
  hints.ai_socktype = SOCK_DGRAM;

  // Unfortunately ngtcp2 requires the selection of the
  // preferred address to be synchronous, which means we
  // have to do a sync resolve using uv_getaddrinfo here.
  return uv_getaddrinfo(env_->event_loop(),
                        req,
                        nullptr,
                        address.address.c_str(),
                        std::to_string(address.port).c_str(),
                        &hints) == 0 &&
         req->addrinfo != nullptr;
}

// =============================================================================
// Path

Path::Path(const SocketAddress& local, const SocketAddress& remote) {
  ngtcp2_addr_init(&this->local, local.data(), local.length());
  ngtcp2_addr_init(&this->remote, remote.data(), remote.length());
}

// ======================================================================================
// Utilities

namespace {

// The default random CIDFactory instance.
CIDFactory::RandomCIDFactory kDefaultCIDFactory;

// Qlog is a JSON-based logging format that is being standardized for low-level
// debug logging of QUIC connections and dataflows. The qlog output is generated
// optionally by ngtcp2 for us. The OnQlogWrite callback is registered with
// ngtcp2 to emit the qlog information. Every Session will have it's own qlog
// stream.
void OnQlogWrite(void* user_data,
                 uint32_t flags,
                 const void* data,
                 size_t len) {
  Session* session = static_cast<Session*>(user_data);
  Environment* env = session->env();

  // Fun fact... ngtcp2 does not emit the final qlog statement until the
  // ngtcp2_conn object is destroyed. Ideally, destroying is explicit, but
  // sometimes the Session object can be garbage collected without being
  // explicitly destroyed. During those times, we cannot call out to JavaScript.
  // Because we don't know for sure if we're in in a GC when this is called, it
  // is safer to just defer writes to immediate, and to keep it consistent,
  // let's just always defer (this is not performance sensitive so the deferring
  // is fine).

  DEBUG_ARGS(session,
             "QLOG: %s",
             std::string(reinterpret_cast<const char*>(data), len));

  if (session->qlogstream()) {
    std::vector<uint8_t> buffer(len);
    memcpy(buffer.data(), data, len);
    env->SetImmediate(
        [ptr = session->qlogstream(), buffer = std::move(buffer), flags](
            Environment*) { ptr->Emit(buffer.data(), buffer.size(), flags); });
  }
}

// Forwards detailed(verbose) debugging information from ngtcp2. Enabled using
// the NODE_DEBUG_NATIVE=NGTCP2_DEBUG category.
void Ngtcp2DebugLog(void* user_data, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  std::string format(fmt, strlen(fmt) + 1);
  format[strlen(fmt)] = '\n';
  // Debug() does not work with the va_list here. So we use vfprintf
  // directly instead. Ngtcp2DebugLog is only enabled when the debug
  // category is enabled.
  vfprintf(stderr, format.c_str(), ap);
  va_end(ap);
}
}  // namespace

CIDFactory& CIDFactory::random() {
  return kDefaultCIDFactory;
}

struct Session::NgCallbackScope final {
  Session* session;
  inline explicit NgCallbackScope(Session* session_) : session(session_) {
    CHECK(!session->in_ng_callback_);
    session->in_ng_callback_ = true;
  }
  QUIC_NO_COPY_OR_MOVE(NgCallbackScope)

  inline ~NgCallbackScope() { session->in_ng_callback_ = false; }

  static inline bool InNgCallbackScope(const Session& session) {
    return session.in_ng_callback_;
  }
};

struct Session::MaybeCloseConnectionScope final {
  Session* session;
  bool silent = false;
  MaybeCloseConnectionScope(Session* session_, bool silent_)
      : session(session_),
        silent(silent_ || session->connection_close_depth_ > 0) {
    session->connection_close_depth_++;
  }
  QUIC_NO_COPY_OR_MOVE(MaybeCloseConnectionScope)
  ~MaybeCloseConnectionScope() noexcept {
    // We only want to trigger the sending the connection close if ...
    // a) Silent is not explicitly true,
    // b) We're not within the scope of an ngtcp2 callback, and
    // c) We are not already in a closing or draining period.
    session->connection_close_depth_--;
    if (session->connection_close_depth_ == 0 && !silent &&
        !NgCallbackScope::InNgCallbackScope(*session) &&
        !session->is_destroyed() && !session->is_in_closing_period() &&
        !session->is_in_draining_period()) {
      session->SendConnectionClose();
    }
  }
};

Session::SendPendingDataScope::SendPendingDataScope(Session* session_)
    : session(session_) {
  session->send_scope_depth_++;
}

Session::SendPendingDataScope::~SendPendingDataScope() {
  --(session->send_scope_depth_);
  if (session->send_scope_depth_ == 0 &&
      !NgCallbackScope::InNgCallbackScope(*session) &&
      !session->is_in_closing_period() && !session->is_in_draining_period()) {
    session->SendPendingData();
  }
}

// ======================================================================================
// Session::Config

Session::Config::Config(const Endpoint& endpoint,
                        const CID& dcid_,
                        const SocketAddress& local_address,
                        const SocketAddress& remote_address,
                        quic_version version_,
                        ngtcp2_crypto_side side)
    : side(side),
      version(version_),
      // For now, we're always using the default, but we will make this
      // configurable soon.
      cid_factory(kDefaultCIDFactory),
      local_addr(local_address),
      remote_addr(remote_address),
      dcid(dcid_),
      scid(cid_factory.Generate()) {
  ngtcp2_settings_default(this);
  initial_ts = uv_hrtime();

  if (UNLIKELY(endpoint.env()->enabled_debug_list()->enabled(
          DebugCategory::NGTCP2_DEBUG)))
    log_printf = Ngtcp2DebugLog;

  auto& config = endpoint.options();

  cc_algo = config.cc_algorithm;
  max_udp_payload_size = config.max_payload_size;

  if (config.max_window_override > 0) max_window = config.max_window_override;

  if (config.max_stream_window_override > 0)
    max_stream_window = config.max_stream_window_override;

  if (config.unacknowledged_packet_threshold > 0)
    ack_thresh = config.unacknowledged_packet_threshold;
}

void Session::Config::EnableQLog(const CID& ocid) {
  if (ocid) {
    qlog.odcid = *ocid;
    this->ocid = Just(ocid);
  }
  qlog.write = OnQlogWrite;
}

// ======================================================================================
// Session::Options

void Session::Options::MemoryInfo(MemoryTracker* tracker) const {
  if (preferred_address_ipv4.IsJust())
    tracker->TrackField("preferred_address_ipv4",
                        preferred_address_ipv4.FromJust());
  if (preferred_address_ipv6.IsJust())
    tracker->TrackField("preferred_address_ipv6",
                        preferred_address_ipv6.FromJust());
  tracker->TrackField("tls", crypto_options);
}

// ======================================================================================
// Session::TransportParams

Session::TransportParams::TransportParams(const Config& config,
                                          const Options& options)
    : TransportParams(Type::ENCRYPTED_EXTENSIONS) {
  ngtcp2_transport_params_default(&params_);
  params_.active_connection_id_limit = options.active_connection_id_limit;
  params_.initial_max_stream_data_bidi_local =
      options.initial_max_stream_data_bidi_local;
  params_.initial_max_stream_data_bidi_remote =
      options.initial_max_stream_data_bidi_remote;
  params_.initial_max_stream_data_uni = options.initial_max_stream_data_uni;
  params_.initial_max_streams_bidi = options.initial_max_streams_bidi;
  params_.initial_max_streams_uni = options.initial_max_streams_uni;
  params_.initial_max_data = options.initial_max_data;
  params_.max_idle_timeout = options.max_idle_timeout * NGTCP2_SECONDS;
  params_.max_ack_delay = options.max_ack_delay;
  params_.ack_delay_exponent = options.ack_delay_exponent;
  params_.max_datagram_frame_size = options.max_datagram_frame_size;
  params_.disable_active_migration = options.disable_active_migration ? 1 : 0;
  params_.preferred_address_present = 0;
  params_.stateless_reset_token_present = 0;
  params_.retry_scid_present = 0;

  if (config.side == NGTCP2_CRYPTO_SIDE_SERVER) {
    // For the server side, the original dcid is always set.
    params_.original_dcid = *config.ocid.ToChecked();

    // The retry_scid is only set if the server validated a retry token.
    if (config.retry_scid.IsJust()) {
      params_.retry_scid = *config.retry_scid.FromJust();
      params_.retry_scid_present = 1;
    }
  }

  if (options.preferred_address_ipv4.IsJust())
    SetPreferredAddress(options.preferred_address_ipv4.FromJust());

  if (options.preferred_address_ipv6.IsJust())
    SetPreferredAddress(options.preferred_address_ipv6.FromJust());
}

Session::TransportParams::TransportParams(Type type, const ngtcp2_vec& vec)
    : TransportParams(type) {
  int ret = ngtcp2_decode_transport_params(
      &params_,
      static_cast<ngtcp2_transport_params_type>(type),
      vec.base,
      vec.len);

  if (ret != 0) {
    ptr_ = nullptr;
    error_ = QuicError::ForNgtcp2Error(ret);
  }
}

Store Session::TransportParams::Encode(Environment* env) {
  if (ptr_ == nullptr) {
    error_ = QuicError::ForNgtcp2Error(NGTCP2_INTERNAL_ERROR);
    return Store();
  }

  // Preflight to see how much storage we'll need.
  ssize_t size = ngtcp2_encode_transport_params(
      nullptr, 0, static_cast<ngtcp2_transport_params_type>(type_), &params_);

  CHECK_GT(size, 0);

  auto result = ArrayBuffer::NewBackingStore(env->isolate(), size);

  auto ret = ngtcp2_encode_transport_params(
      static_cast<uint8_t*>(result->Data()),
      size,
      static_cast<ngtcp2_transport_params_type>(type_),
      &params_);

  if (ret != 0) {
    error_ = QuicError::ForNgtcp2Error(ret);
    return Store();
  }

  return Store(std::move(result), size);
}

void Session::TransportParams::SetPreferredAddress(
    const SocketAddress& address) {
  CHECK(ptr_ == &params_);
  params_.preferred_address_present = 1;
  switch (address.family()) {
    case AF_INET: {
      const sockaddr_in* src =
          reinterpret_cast<const sockaddr_in*>(address.data());
      memcpy(params_.preferred_address.ipv4_addr,
             &src->sin_addr,
             sizeof(params_.preferred_address.ipv4_addr));
      params_.preferred_address.ipv4_port = address.port();
      return;
    }
    case AF_INET6: {
      const sockaddr_in6* src =
          reinterpret_cast<const sockaddr_in6*>(address.data());
      memcpy(params_.preferred_address.ipv6_addr,
             &src->sin6_addr,
             sizeof(params_.preferred_address.ipv6_addr));
      params_.preferred_address.ipv6_port = address.port();
      return;
    }
  }
  UNREACHABLE();
}

void Session::TransportParams::GenerateStatelessResetToken(
    const Endpoint& endpoint, const CID& cid) {
  CHECK(ptr_ == &params_);
  CHECK(cid);
  params_.stateless_reset_token_present = 1;

  StatelessResetToken token(params_.stateless_reset_token,
                            endpoint.options().reset_token_secret,
                            cid);
}

void Session::TransportParams::GeneratePreferredAddressToken(Session* session,
                                                             CID* pscid) {
  CHECK_NOT_NULL(session);
  CHECK(ptr_ == &params_);
  CHECK(pscid);
  *pscid = session->cid_factory_.Generate();
  params_.preferred_address.cid = **pscid;
  StatelessResetToken new_token(
      params_.preferred_address.stateless_reset_token,
      session->endpoint().options().reset_token_secret,
      *pscid);
  session->endpoint_->AssociateStatelessResetToken(new_token, session);
}

// ======================================================================================
// Session::Application

std::string Session::Application::StreamData::ToString() const {
  return std::string("StreamData [") + std::to_string(id) +
         "]: buffers = " + std::to_string(count) +
         ", remaining = " + std::to_string(remaining) +
         ", fin = " + std::to_string(fin);
}

bool Session::Application::Start() {
  if (!started_) {
    DEBUG(session_, "Application started");
    started_ = true;
  }
  return true;
}

void Session::Application::AcknowledgeStreamData(Stream* stream,
                                                 uint64_t offset,
                                                 size_t datalen) {
  DEBUG_ARGS(
      session_, "Acknowledging %" PRIu64 " bytes of stream data", datalen);
  stream->Acknowledge(offset, datalen);
}

bool Session::Application::BlockStream(stream_id id) {
  DEBUG_ARGS(session_, "Application block stream %" PRIi64, id);
  auto stream = session().FindStream(id);
  if (stream) stream->Blocked();
  return true;
}

// Called to determine if a Header can be added to this application.
// Applications that do not support headers (which is the default) will always
// return false.
bool Session::Application::CanAddHeader(size_t current_count,
                                        size_t current_headers_length,
                                        size_t this_header_length) {
  return false;
}

SessionTicketAppData::Status Session::Application::GetSessionTicketAppData(
    const SessionTicketAppData& app_data, SessionTicketAppData::Flag flag) {
  DEBUG(session_, "Application getting session ticket app data");
  return flag == SessionTicketAppData::Flag::STATUS_RENEW
             ? SessionTicketAppData::Status::TICKET_USE_RENEW
             : SessionTicketAppData::Status::TICKET_USE;
}

BaseObjectPtr<Packet> Session::Application::CreateStreamDataPacket() {
  return Packet::Create(
      env(),
      session_->endpoint_.get(),
      session_->remote_address_,
      "stream data",
      ngtcp2_conn_get_max_udp_payload_size(session_->connection()));
}

void Session::Application::StreamClose(Stream* stream, Maybe<QuicError> error) {
  DEBUG_ARGS(session_, "Application closing stream %" PRIi64, stream->id());
  stream->Destroy(error);
}

void Session::Application::StreamReset(Stream* stream,
                                       uint64_t final_size,
                                       QuicError error) {
  DEBUG_ARGS(session_,
             "Application resetting stream %" PRIi64 " with final size %" PRIu64
             " [%s]",
             stream->id(),
             final_size,
             error);
  stream->ReceiveResetStream(final_size, error);
}

void Session::Application::StreamStopSending(Stream* stream, QuicError error) {
  DEBUG_ARGS(session_,
             "Application stop sending stream %" PRIi64 " [%s]",
             stream->id(),
             error);
  stream->ReceiveStopSending(error);
}

void Session::Application::SendPendingData() {
  PathStorage path;

  DEBUG(session_, "Starting pending data loop");
  BaseObjectPtr<Packet> packet;
  uint8_t* pos = nullptr;
  int err = 0;

  size_t maxPacketCount =
      std::min(static_cast<size_t>(64000),
               ngtcp2_conn_get_send_quantum(session_->connection()));
  size_t packetSendCount = 0;
  DEBUG_ARGS(session_,
             "Maximum number of packets to send this iteration: %" PRIu64,
             maxPacketCount);

  const auto updateTimer = [&] {
    ngtcp2_conn_update_pkt_tx_time(session_->connection(), uv_hrtime());
    session_->UpdateTimer();
  };

  const auto congestionLimited = [&](auto packet) {
    auto len = pos - ngtcp2_vec(*packet).base;
    // We are either congestion limited or done.
    if (len) {
      // Some data was serialized into the packet. We need to send it.
      packet->Truncate(len);
      session_->Send(std::move(packet), path);
    }

    updateTimer();
  };

  for (;;) {
    ssize_t ndatalen;
    StreamData stream_data;

    DEBUG(session_, "Selecting stream data to encode");
    err = GetStreamData(&stream_data);

    if (err < 0) {
      session_->last_error_ = QuicError::ForNgtcp2Error(NGTCP2_ERR_INTERNAL);
      return session_->CloseSilently();
    }

    DEBUG_ARGS(session_, "Stream Data: %s", stream_data);

    if (!packet) {
      packet = CreateStreamDataPacket();
      if (!packet) {
        session_->last_error_ = QuicError::ForNgtcp2Error(NGTCP2_ERR_INTERNAL);
        return session_->CloseSilently();
      }
      pos = ngtcp2_vec(*packet).base;
    }

    ssize_t nwrite = WriteVStream(&path, pos, &ndatalen, stream_data);

    DEBUG_ARGS(session_, "Total amount written to packet: %" PRId64, nwrite);
    DEBUG_ARGS(
        session_, "Stream data amount written to packet: %" PRId64, ndatalen);

    if (nwrite <= 0) {
      switch (nwrite) {
        case 0:
          DEBUG(session_, "No data written to packet. Congestion limited?");
          if (stream_data.id >= 0) ResumeStream(stream_data.id);
          return congestionLimited(std::move(packet));
        case NGTCP2_ERR_STREAM_DATA_BLOCKED: {
          DEBUG(session_,
                "Stream data is blocked. Congestion limited? We'll try again.");
          session().StreamDataBlocked(stream_data.id);
          if (session().max_data_left() == 0) {
            if (stream_data.id >= 0) ResumeStream(stream_data.id);
            return congestionLimited(std::move(packet));
          }
          CHECK_LE(ndatalen, 0);
          continue;
        }
        case NGTCP2_ERR_STREAM_SHUT_WR: {
          // Indicates that the writable side of the stream has been closed
          // locally or the stream is being reset. In either case, we can't send
          // any stream data!
          CHECK_GE(stream_data.id, 0);
          DEBUG_ARGS(
              session_, "Stream %" PRIi64 " is not writable.", stream_data.id);
          // We need to notify the stream that the writable side has been closed
          // and no more outbound data can be sent.
          CHECK_LE(ndatalen, 0);
          auto stream = session_->FindStream(stream_data.id);
          if (stream) stream->EndWritable();
          continue;
        }
        case NGTCP2_ERR_WRITE_MORE: {
          DEBUG(session_, "Packet is not yet full. Continuing to write more.");
          CHECK_GT(ndatalen, 0);
          if (!StreamCommit(&stream_data, ndatalen)) return session_->Close();
          pos += ndatalen;
          continue;
        }
      }

      DEBUG(session_,
            "An unexpected error occurred while write packet data. Closing "
            "session.");
      session_->last_error_ = QuicError::ForNgtcp2Error(nwrite);
      return session_->CloseSilently();
    }

    pos += nwrite;
    if (ndatalen > 0 && !StreamCommit(&stream_data, ndatalen)) {
      // Since we are closing the session here, we don't worry about updating
      // the pkt tx time. The failed StreamCommit should have updated the
      // last_error_ appropriately.
      return session_->CloseSilently();
    }

    if (stream_data.id >= 0 && ndatalen < 0) ResumeStream(stream_data.id);

    packet->Truncate(nwrite);
    session_->Send(std::move(packet), path);

    pos = nullptr;

    if (++packetSendCount == maxPacketCount) {
      DEBUG(session_,
            "Packet count limit reached. Breaking out of pending data loop.");
      break;
    }
  }

  updateTimer();
}

ssize_t Session::Application::WriteVStream(PathStorage* path,
                                           uint8_t* buf,
                                           ssize_t* ndatalen,
                                           const StreamData& stream_data) {
  CHECK_LE(stream_data.count, kMaxVectorCount);
  uint32_t flags = NGTCP2_WRITE_STREAM_FLAG_NONE;
  if (stream_data.remaining > 0) flags |= NGTCP2_WRITE_STREAM_FLAG_MORE;
  if (stream_data.fin) flags |= NGTCP2_WRITE_STREAM_FLAG_FIN;
  ssize_t ret = ngtcp2_conn_writev_stream(
      session_->connection(),
      &path->path,
      nullptr,
      buf,
      ngtcp2_conn_get_max_udp_payload_size(session_->connection()),
      ndatalen,
      flags,
      stream_data.id,
      stream_data.buf,
      stream_data.count,
      uv_hrtime());
  return ret;
}

// ======================================================================================
// Session
Session* Session::From(ngtcp2_conn* conn, void* user_data) {
  auto session = static_cast<Session*>(user_data);
  CHECK_EQ(conn, session->connection());
  return session;
}

Local<FunctionTemplate> Session::GetConstructorTemplate(Environment* env) {
  auto& state = BindingState::Get(env);
  auto tmpl = state.session_constructor_template();
  if (tmpl.IsEmpty()) {
    auto isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, IllegalConstructor);
    tmpl->SetClassName(state.session_string());
    tmpl->Inherit(AsyncWrap::GetConstructorTemplate(env));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        Session::kInternalFieldCount);
    SetProtoMethodNoSideEffect(
        isolate, tmpl, "getRemoteAddress", GetRemoteAddress);
    SetProtoMethodNoSideEffect(isolate, tmpl, "getCertificate", GetCertificate);
    SetProtoMethodNoSideEffect(
        isolate, tmpl, "getPeerCertificate", GetPeerCertificate);
    SetProtoMethodNoSideEffect(
        isolate, tmpl, "getEphemeralKeyInfo", GetEphemeralKeyInfo);
    SetProtoMethod(isolate, tmpl, "destroy", DoDestroy);
    SetProtoMethod(isolate, tmpl, "gracefulClose", GracefulClose);
    SetProtoMethod(isolate, tmpl, "silentClose", SilentClose);
    SetProtoMethod(isolate, tmpl, "updateKey", UpdateKey);
    SetProtoMethod(isolate, tmpl, "onClientHelloDone", OnClientHelloDone);
    SetProtoMethod(isolate, tmpl, "onOCSPDone", OnOCSPDone);
    SetProtoMethod(isolate, tmpl, "openStream", DoOpenStream);
    SetProtoMethod(isolate, tmpl, "sendDatagram", DoSendDatagram);
    state.set_session_constructor_template(tmpl);
  }
  return tmpl;
}

void Session::Initialize(Environment* env, Local<Object> target) {
  USE(GetConstructorTemplate(env));

  OptionsObject::Initialize(env, target);

#define V(name, _, __) NODE_DEFINE_CONSTANT(target, IDX_STATS_SESSION_##name);
  SESSION_STATS(V)
  NODE_DEFINE_CONSTANT(target, IDX_STATS_SESSION_COUNT);
#undef V
#define V(name, _, __) NODE_DEFINE_CONSTANT(target, IDX_STATE_SESSION_##name);
  SESSION_STATE(V)
  NODE_DEFINE_CONSTANT(target, IDX_STATE_SESSION_COUNT);
#undef V

  constexpr uint32_t STREAM_DIRECTION_BIDIRECTIONAL =
      static_cast<uint32_t>(Direction::BIDIRECTIONAL);
  constexpr uint32_t STREAM_DIRECTION_UNIDIRECTIONAL =
      static_cast<uint32_t>(Direction::UNIDIRECTIONAL);

  NODE_DEFINE_CONSTANT(target, STREAM_DIRECTION_BIDIRECTIONAL);
  NODE_DEFINE_CONSTANT(target, STREAM_DIRECTION_UNIDIRECTIONAL);
}

void Session::RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(DoDestroy);
  registry->Register(GetRemoteAddress);
  registry->Register(GetCertificate);
  registry->Register(GetEphemeralKeyInfo);
  registry->Register(GetPeerCertificate);
  registry->Register(GracefulClose);
  registry->Register(SilentClose);
  registry->Register(UpdateKey);
  registry->Register(OnClientHelloDone);
  registry->Register(OnOCSPDone);
  registry->Register(DoOpenStream);
  registry->Register(DoSendDatagram);
  OptionsObject::RegisterExternalReferences(registry);
}

BaseObjectPtr<Session> Session::Create(BaseObjectPtr<Endpoint> endpoint,
                                       const Config& config,
                                       const Options& options) {
  auto env = endpoint->env();
  Local<Object> obj;
  if (!GetConstructorTemplate(env)
           ->InstanceTemplate()
           ->NewInstance(env->context())
           .ToLocal(&obj))
    return BaseObjectPtr<Session>();
  return MakeDetachedBaseObject<Session>(
      obj, std::move(endpoint), config, options);
}

std::string Session::diagnostic_name() const {
  const auto get_type = [&] { return is_server() ? "server" : "client"; };

  return MemoryInfoName() + " (" + get_type() + "," +
         std::to_string(env()->thread_id()) + ":" +
         std::to_string(static_cast<int64_t>(get_async_id())) + ")";
}

Session::Session(Local<Object> object,
                 BaseObjectPtr<Endpoint> endpoint,
                 const Config& config,
                 const Options& options)
    : SessionStatsBase(endpoint->env()),
      AsyncWrap(endpoint->env(), object, AsyncWrap::PROVIDER_QUICSESSION),
      allocator_(BindingState::Get(env())),
      options_(std::move(options)),
      endpoint_(std::move(endpoint)),
      state_(env()->isolate()),
      cid_factory_(config.cid_factory),
      maybe_cid_factory_ref_(config.cid_factory.StrongRef()),
      local_address_(config.local_addr),
      remote_address_(config.remote_addr),
      application_(SelectApplication(config, options_)),
      crypto_context_(this, options_.crypto_options, config.side),
      timer_(env(),
             [this, self = BaseObjectPtr<Session>(this)] { OnTimeout(); }),
      dcid_(config.dcid),
      scid_(config.scid),
      preferred_address_cid_(nullptr, 0) {
  MakeWeak();
  timer_.Unref();

  if (config.ocid.IsJust()) ocid_ = config.ocid;

  ExtendMaxStreams(
      EndpointLabel::LOCAL, Direction::BIDIRECTIONAL, DEFAULT_MAX_STREAMS_BIDI);
  ExtendMaxStreams(
      EndpointLabel::LOCAL, Direction::UNIDIRECTIONAL, DEFAULT_MAX_STREAMS_UNI);

  const auto defineProperty = [&](auto name, auto value) {
    object
        ->DefineOwnProperty(
            env()->context(), name, value, PropertyAttribute::ReadOnly)
        .Check();
  };

  defineProperty(env()->state_string(), state_.GetArrayBuffer());
  defineProperty(env()->stats_string(), ToBigUint64Array(env()));

  if (UNLIKELY(options.qlog)) {
    qlogstream_ = LogStream::Create(env());
    if (LIKELY(qlogstream_)) {
      defineProperty(BindingState::Get(env()).qlog_string(),
                     qlogstream_->object());
    }
  }

  if (UNLIKELY(options.crypto_options.keylog)) {
    keylogstream_ = LogStream::Create(env());
    if (LIKELY(keylogstream_)) {
      defineProperty(BindingState::Get(env()).keylog_string(),
                     keylogstream_->object());
    }
  }

  ngtcp2_conn* conn;
  Path path(local_address_, remote_address_);
  TransportParams transport_params(config, options);
  switch (config.side) {
    case NGTCP2_CRYPTO_SIDE_SERVER: {
      DEBUG_ARGS(this, "Initializing as server session [%s]", scid_);
      transport_params.GenerateStatelessResetToken(*endpoint_, scid_);
      if (transport_params->preferred_address_present)
        transport_params.GeneratePreferredAddressToken(this,
                                                       &preferred_address_cid_);
      CHECK_EQ(ngtcp2_conn_server_new(&conn,
                                      dcid_,
                                      scid_,
                                      &path,
                                      config.version,
                                      &callbacks[config.side],
                                      &config,
                                      transport_params,
                                      &allocator_,
                                      this),
               0);
      break;
    }
    case NGTCP2_CRYPTO_SIDE_CLIENT: {
      DEBUG_ARGS(this, "Initializing as client session [%s]", scid_);
      CHECK_EQ(ngtcp2_conn_client_new(&conn,
                                      dcid_,
                                      scid_,
                                      &path,
                                      config.version,
                                      &callbacks[config.side],
                                      &config,
                                      transport_params,
                                      &allocator_,
                                      this),
               0);
      crypto_context_.MaybeSetEarlySession(config.session_ticket);
      break;
    }
    default:
      UNREACHABLE();
  }

  connection_.reset(conn);

  // We index the Session by our local CID (the scid) and dcid (the peer's cid)
  endpoint_->AddSession(scid_, BaseObjectPtr<Session>(this));
  endpoint_->AssociateCID(dcid_, scid_);

  crypto_context_.Start();

  UpdateDataStats();
}

Session::~Session() {
  DEBUG_ARGS(this, "Destroying session [%s]", scid_);
  if (qlogstream_) {
    env()->SetImmediate(
        [ptr = std::move(qlogstream_)](Environment*) { ptr->End(); });
  }
  if (keylogstream_) {
    env()->SetImmediate(
        [ptr = std::move(keylogstream_)](Environment*) { ptr->End(); });
  }
  CHECK(streams_.empty());
}

void Session::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("options", options_);
  tracker->TrackField("endpoint", endpoint_);
  tracker->TrackField("streams", streams_);
  tracker->TrackField("local_address", local_address_);
  tracker->TrackField("remote_address", remote_address_);
  tracker->TrackField("application", application_);
  tracker->TrackField("crypto_context", crypto_context_);
  tracker->TrackField("timer", timer_);
  tracker->TrackField("conn_closebuf", conn_closebuf_);
  tracker->TrackField("qlogstream", qlogstream_);
  tracker->TrackField("keylogstream", keylogstream_);
}

const Endpoint& Session::endpoint() const {
  return *endpoint_.get();
}

BaseObjectPtr<Stream> Session::FindStream(stream_id id) const {
  auto it = streams_.find(id);
  return it == std::end(streams_) ? BaseObjectPtr<Stream>() : it->second;
}

bool Session::HandshakeCompleted() {
  state_->handshake_completed = true;
  RecordTimestamp(&SessionStats::handshake_completed_at);
  DEBUG(this, "Handshake is completed");

  if (!crypto_context_.was_early_data_accepted()) {
    ngtcp2_conn_early_data_rejected(connection());
  }

  // When in a server session, handshake completed == handshake confirmed.
  if (is_server()) {
    HandshakeConfirmed();

    uint8_t token[NGTCP2_CRYPTO_MAX_REGULAR_TOKENLEN];
    size_t tokenlen = 0;

    if (!endpoint_->GenerateNewToken(version(), token, remote_address_)
             .To(&tokenlen)) {
      // Failed to generate a new token on handshake complete.
      // This isn't the end of the world, just keep going.
      return true;
    }

    if (NGTCP2_ERR(
            ngtcp2_conn_submit_new_token(connection_.get(), token, tokenlen))) {
      // Submitting the new token failed...
      return false;
    }
  }

  EmitHandshakeComplete();

  return true;
}

void Session::HandshakeConfirmed() {
  state_->handshake_confirmed = true;
  RecordTimestamp(&SessionStats::handshake_confirmed_at);
  DEBUG(this, "Handshake is confirmed");
}

void Session::Close() {
  if (!is_destroyed()) {
    DEBUG(this, "Close");
    DoClose();
  }
}

void Session::CloseSilently() {
  if (!is_destroyed()) {
    DEBUG(this, "Close silently");
    DoClose(true);
  }
}

void Session::CloseGracefully() {
  if (is_destroyed()) return;

  // If there are no open streams, then we can close just immediately and not
  // worry about waiting around for the right moment.
  if (streams_.empty()) return DoClose();

  DEBUG(this, "Close gracefully");

  state_->graceful_closing = 1;
  RecordTimestamp(&SessionStats::graceful_closing_at);
}

void Session::DoClose(bool silent) {
  CHECK(!is_destroyed());
  // Once Close has been called, we cannot re-enter
  if (state_->closing) return;
  state_->closing = 1;

  DEBUG(this, "Closing immediately");

  // Iterate through all of the known streams and close them. The streams
  // will remove themselves from the Session as soon as they are closed.
  // Note: we create a copy because the streams will remove themselves
  // while they are cleaning up which will invalidate the iterator.
  auto streams = streams_;
  for (auto& stream : streams) stream.second->Destroy(Just(last_error_));
  streams.clear();

  // If the state has not been passed out to JavaScript yet, we can skip closing
  // entirely and drop directly out to Destroy.
  if (!state_->wrapped) return Destroy();

  // If we're not running within a ngtcp2 callback scope, schedule a
  // CONNECTION_CLOSE to be sent when Close exits. If we are within a ngtcp2
  // callback scope, sending the CONNECTION_CLOSE will be deferred.
  {
    MaybeCloseConnectionScope close_scope(this, silent);
    RecordTimestamp(&SessionStats::closing_at);

    state_->closing = true;
    state_->silent_close = silent ? 1 : 0;
  }

  // We emit a close callback so that the JavaScript side can clean up anything
  // it needs to clean up before destroying. It's the JavaScript side's
  // responsibility to call destroy() when ready.
  EmitClose();
}

void Session::Destroy() {
  if (is_destroyed()) return;

  DEBUG(this, "Destroyed");

  // The DoClose() method should have already been called.
  CHECK(state_->closing);

  // We create a copy of the streams because they will remove themselves
  // from streams_ as they are cleaning up, causing the iterator to be
  // invalidated.
  auto streams = streams_;
  for (auto& stream : streams) stream.second->Destroy(Just(last_error_));

  CHECK(streams_.empty());

  RecordTimestamp(&SessionStats::destroyed_at);
  state_->closing = 0;
  state_->graceful_closing = 0;

  timer_.Stop();

  // The Session instances are kept alive using a in the Endpoint. Removing the
  // Session from the Endpoint will free that pointer, allowing the Session to
  // be deconstructed once the stack unwinds and any remaining
  // BaseObjectPtr<Session> instances fall out of scope.

  std::vector<ngtcp2_cid> cids(ngtcp2_conn_get_num_scid(connection()));
  std::vector<ngtcp2_cid_token> tokens(
      ngtcp2_conn_get_num_active_dcid(connection()));
  ngtcp2_conn_get_scid(connection(), cids.data());
  ngtcp2_conn_get_active_dcid(connection(), tokens.data());

  endpoint_->DisassociateCID(dcid_);
  endpoint_->DisassociateCID(preferred_address_cid_);

  for (auto cid : cids) endpoint_->DisassociateCID(CID(&cid));

  for (auto token : tokens) {
    if (token.token_present)
      endpoint_->DisassociateStatelessResetToken(
          StatelessResetToken(token.token));
  }

  state_->destroyed = 1;

  BaseObjectPtr<Endpoint> endpoint = std::move(endpoint_);

  endpoint->RemoveSession(scid_, remote_address_);
}

Session::TransportParams Session::GetLocalTransportParams() const {
  CHECK(!is_destroyed());
  return TransportParams(TransportParams::Type::ENCRYPTED_EXTENSIONS,
                         ngtcp2_conn_get_local_transport_params(connection()));
}

Session::TransportParams Session::GetRemoteTransportParams() const {
  CHECK(!is_destroyed());
  return TransportParams(TransportParams::Type::ENCRYPTED_EXTENSIONS,
                         ngtcp2_conn_get_remote_transport_params(connection()));
}

bool Session::is_in_closing_period() const {
  return ngtcp2_conn_is_in_closing_period(connection());
}

bool Session::is_in_draining_period() const {
  return ngtcp2_conn_is_in_draining_period(connection());
}

bool Session::is_unable_to_send_packets() const {
  return NgCallbackScope::InNgCallbackScope(*this) || is_destroyed() ||
         is_in_draining_period() || (is_server() && is_in_closing_period()) ||
         !endpoint_;
}

uint64_t Session::max_data_left() const {
  return ngtcp2_conn_get_max_data_left(connection());
}

uint64_t Session::max_local_streams_uni() const {
  return ngtcp2_conn_get_max_local_streams_uni(connection());
}

uint64_t Session::max_local_streams_bidi() const {
  return ngtcp2_conn_get_local_transport_params(connection())
      ->initial_max_streams_bidi;
}

quic_version Session::version() const {
  return ngtcp2_conn_get_negotiated_version(connection());
}

void Session::ExtendOffset(size_t amount) {
  DEBUG_ARGS(this, "Extending offset by %" PRIu64 " bytes", amount);
  ngtcp2_conn_extend_max_offset(connection(), amount);
}

void Session::ExtendStreamOffset(stream_id id, size_t amount) {
  DEBUG_ARGS(this,
             "Extending stream %" PRIi64 " offset by %" PRIu64 " bytes",
             id,
             amount);
  ngtcp2_conn_extend_max_stream_offset(connection(), id, amount);
}

void Session::UpdateTimer() {
  // Both uv_hrtime and ngtcp2_conn_get_expiry return nanosecond units.
  uint64_t expiry = ngtcp2_conn_get_expiry(connection());
  uint64_t now = uv_hrtime();

  if (expiry <= now) {
    // The timer has already expired.
    return OnTimeout();
  }

  auto timeout = (expiry - now) / NGTCP2_MILLISECONDS;

  DEBUG_ARGS(this, "Setting transmission timer to %" PRIu64, timeout);

  // If timeout is zero here, it means our timer is less than a millisecond
  // off from expiry. Let's bump the timer to 1.
  timer_.Update(timeout == 0 ? 1 : timeout);
}

void Session::OnTimeout() {
  HandleScope scope(env()->isolate());

  if (is_destroyed()) return;

  DEBUG(this, "Transmission timer has expired");
  DEBUG_ARGS(
      this, "In Closing Period? %s", is_in_closing_period() ? "Yes" : "No");
  DEBUG_ARGS(
      this, "In Draining Period? %s", is_in_draining_period() ? "Yes" : "No");

  int ret = ngtcp2_conn_handle_expiry(connection(), uv_hrtime());
  if (NGTCP2_OK(ret) && !is_in_closing_period() && !is_in_draining_period()) {
    DEBUG(this, "Retransmitting");
    SendPendingDataScope send_scope(this);
    return;
  }

  DEBUG(this, "Closing silently after timeout");
  last_error_ = QuicError::ForNgtcp2Error(ret);
  CloseSilently();
}

void Session::SendConnectionClose() {
  CHECK(!NgCallbackScope::InNgCallbackScope(*this));

  if (is_destroyed() || is_in_draining_period() || state_->silent_close) return;

  DEBUG(this, "Sending connection close");

  auto on_exit = OnScopeLeave([this] { UpdateTimer(); });

  switch (crypto_context_.side()) {
    case NGTCP2_CRYPTO_SIDE_SERVER: {
      if (!is_in_closing_period() && !StartClosingPeriod()) {
        DEBUG(this, "Failed to start closing period gracefully.");
        CloseSilently();
      } else {
        DEBUG(this, "Started closing period");
        CHECK(conn_closebuf_);
        Send(conn_closebuf_->Clone());
      }
      return;
    }
    case NGTCP2_CRYPTO_SIDE_CLIENT: {
      Path path(local_address_, remote_address_);
      auto packet = Packet::Create(env(),
                                   endpoint_.get(),
                                   remote_address_,
                                   "immediate connection close (client)");
      ngtcp2_vec vec = *packet;
      ssize_t nwrite = ngtcp2_conn_write_connection_close(connection(),
                                                          &path,
                                                          nullptr,
                                                          vec.base,
                                                          vec.len,
                                                          last_error_,
                                                          uv_hrtime());

      if (UNLIKELY(nwrite < 0)) {
        DEBUG(this, "Failed to start closing gracefully");
        last_error_ = QuicError::ForNgtcp2Error(NGTCP2_INTERNAL_ERROR);
        CloseSilently();
      } else {
        DEBUG(this, "Starting closing period");
        packet->Truncate(nwrite);
        Send(std::move(packet));
      }
      return;
    }
  }
  UNREACHABLE();
}

void Session::Send(BaseObjectPtr<Packet> packet) {
  CHECK(!is_destroyed());
  CHECK(!is_in_draining_period());

  if (packet->length() > 0) {
    DEBUG_ARGS(this, "Sending packet [%s]", packet.get());
    IncrementStat(&SessionStats::bytes_sent, packet->length());
    endpoint_->Send(std::move(packet));
  }
}

void Session::Send(BaseObjectPtr<Packet> packet, const PathStorage& path) {
  UpdatePath(path);
  return Send(std::move(packet));
}

void Session::UpdatePath(const PathStorage& storage) {
  remote_address_.Update(storage.path.remote.addr, storage.path.remote.addrlen);
  local_address_.Update(storage.path.local.addr, storage.path.local.addrlen);
  DEBUG_ARGS(this, "Path updated %s <=> %s", remote_address_, local_address_);
}

void Session::SendPendingData() {
  if (!is_unable_to_send_packets()) application_->SendPendingData();
}

bool Session::StartClosingPeriod() {
  if (is_destroyed()) return false;
  if (is_in_closing_period()) return true;

  auto packet = Packet::Create(
      env(), endpoint_.get(), remote_address_, "server connection close");
  ngtcp2_vec vec = *packet;

  ssize_t nwrite = ngtcp2_conn_write_connection_close(connection(),
                                                      nullptr,
                                                      nullptr,
                                                      vec.base,
                                                      vec.len,
                                                      last_error_.get(),
                                                      uv_hrtime());
  if (nwrite < 0) {
    last_error_ = QuicError::ForNgtcp2Error(NGTCP2_INTERNAL_ERROR);
    return false;
  }

  packet->Truncate(nwrite);
  conn_closebuf_ = std::move(packet);
  return true;
}

void Session::UpdateDataStats() {
  if (state_->destroyed) return;

  ngtcp2_conn_stat stat;
  ngtcp2_conn_get_conn_stat(connection(), &stat);

  SetStat(&SessionStats::bytes_in_flight, stat.bytes_in_flight);
  SetStat(&SessionStats::congestion_recovery_start_ts,
          stat.congestion_recovery_start_ts);
  SetStat(&SessionStats::cwnd, stat.cwnd);
  SetStat(&SessionStats::delivery_rate_sec, stat.delivery_rate_sec);
  SetStat(&SessionStats::first_rtt_sample_ts, stat.first_rtt_sample_ts);
  SetStat(&SessionStats::initial_rtt, stat.initial_rtt);
  SetStat(&SessionStats::last_tx_pkt_ts,
          reinterpret_cast<uint64_t>(stat.last_tx_pkt_ts));
  SetStat(&SessionStats::latest_rtt, stat.latest_rtt);
  SetStat(&SessionStats::loss_detection_timer, stat.loss_detection_timer);
  SetStat(&SessionStats::loss_time, reinterpret_cast<uint64_t>(stat.loss_time));
  SetStat(&SessionStats::max_udp_payload_size, stat.max_udp_payload_size);
  SetStat(&SessionStats::min_rtt, stat.min_rtt);
  SetStat(&SessionStats::pto_count, stat.pto_count);
  SetStat(&SessionStats::rttvar, stat.rttvar);
  SetStat(&SessionStats::smoothed_rtt, stat.smoothed_rtt);
  SetStat(&SessionStats::ssthresh, stat.ssthresh);

  if (stat.bytes_in_flight > GetStat(&SessionStats::max_bytes_in_flight))
    SetStat(&SessionStats::max_bytes_in_flight, stat.bytes_in_flight);
}

void Session::AcknowledgeStreamDataOffset(Stream* stream,
                                          uint64_t offset,
                                          uint64_t datalen) {
  if (is_destroyed()) return;
  application_->AcknowledgeStreamData(stream, offset, datalen);
}

void Session::ActivateConnectionId(uint64_t seq,
                                   const CID& cid,
                                   Maybe<StatelessResetToken> reset_token) {
  DEBUG_ARGS(this, "Activating new CID %s", cid);
  endpoint_->AssociateCID(scid_, cid);
  if (reset_token.IsJust()) {
    endpoint_->AssociateStatelessResetToken(reset_token.FromJust(), this);
  }
}

void Session::DeactivateConnectionId(uint64_t seq,
                                     const CID& cid,
                                     Maybe<StatelessResetToken> reset_token) {
  DEBUG_ARGS(this, "Deactivating CID %s", cid);
  endpoint_->DisassociateCID(cid);
  if (reset_token.IsJust()) {
    endpoint_->DisassociateStatelessResetToken(reset_token.FromJust());
  }
}

datagram_id Session::SendDatagram(Store&& data) {
  auto tp = ngtcp2_conn_get_remote_transport_params(connection());
  uint64_t max_datagram_size = tp->max_datagram_frame_size;
  if (max_datagram_size == 0 || data.length() > max_datagram_size) {
    DEBUG(this, "Datagram is too large");
    return 0;
  }

  BaseObjectPtr<Packet> packet;
  uint8_t* pos = nullptr;
  int accepted = 0;
  ngtcp2_vec vec = data;
  PathStorage path;
  int flags = NGTCP2_WRITE_DATAGRAM_FLAG_MORE;
  datagram_id did = last_datagram_id_ + 1;

  // Let's give it a max number of attempts to send the datagram
  static const int kMaxAttempts = 16;
  int attempts = 0;

  for (;;) {
    if (!packet) {
      packet =
          Packet::Create(env(),
                         endpoint_.get(),
                         remote_address_,
                         "datagram",
                         ngtcp2_conn_get_max_udp_payload_size(connection()));
      if (!packet) {
        last_error_ = QuicError::ForNgtcp2Error(NGTCP2_ERR_INTERNAL);
        CloseSilently();
        return 0;
      }
      pos = ngtcp2_vec(*packet).base;
    }

    ssize_t nwrite = ngtcp2_conn_writev_datagram(connection(),
                                                 &path.path,
                                                 nullptr,
                                                 pos,
                                                 packet->length(),
                                                 &accepted,
                                                 flags,
                                                 did,
                                                 &vec,
                                                 1,
                                                 uv_hrtime());

    if (nwrite < 0) {
      switch (nwrite) {
        case 0: {
          // We cannot send data because of congestion control or the data will
          // not fit. Since datagrams are best effort, we are going to abandon
          // the attempt and just return.
          DEBUG(this,
                "Cannot send datagram because of congestion control or size");
          CHECK_EQ(accepted, 0);
          return 0;
        }
        case NGTCP2_ERR_WRITE_MORE: {
          // We keep on looping! Keep on sending!
          continue;
        }
        case NGTCP2_ERR_INVALID_STATE: {
          // The remote endpoint does not want to accept datagrams. That's ok,
          // just return 0.
          DEBUG(this, "The remote endpoint does not support datagrams");
          return 0;
        }
        case NGTCP2_ERR_INVALID_ARGUMENT: {
          // The datagram is too large. That should have been caught above but
          // that's ok. We'll just abandon the attempt and return.
          DEBUG(this, "The datagram is too large");
          return 0;
        }
      }
      last_error_ = QuicError::ForNgtcp2Error(nwrite);
      CloseSilently();
      return 0;
    }

    // In this case, a complete packet was written and we need to send it along.
    packet->Truncate(nwrite);
    Send(std::move(packet));
    ngtcp2_conn_update_pkt_tx_time(connection(), uv_hrtime());

    if (accepted != 0) {
      // Yay! The datagram was accepted into the packet we just sent and we can
      // just return the datagram ID.
      last_datagram_id_ = did;
      return did;
    }

    // We sent a packet, but it wasn't the datagram packet. That can happen.
    // Let's loop around and try again.
    if (++attempts == kMaxAttempts) {
      DEBUG(this, "Too many attempts to send datagram");
      break;
    }
  }

  return 0;
}

void Session::DatagramAcknowledged(datagram_id id) {
  EmitDatagramAcknowledged(id);
}

void Session::DatagramLost(datagram_id id) {
  EmitDatagramLost(id);
}

void Session::DatagramReceived(const uint8_t* data,
                               size_t datalen,
                               DatagramReceivedFlag flag) {
  // If there is nothing watching for the datagram on the JavaScript side,
  // we just drop it on the floor.
  if (state_->datagram == 0 || datalen == 0) return;

  DEBUG_ARGS(this, "%" PRIu64 " byte datagram received", datalen);

  auto backing = ArrayBuffer::NewBackingStore(env()->isolate(), datalen);
  memcpy(backing->Data(), data, datalen);
  EmitDatagram(Store(std::move(backing), datalen), flag);
}

void Session::ExtendMaxStreamData(Stream* stream, uint64_t max) {
  application_->ExtendMaxStreamData(stream, max);
}

void Session::ExtendMaxStreams(EndpointLabel label,
                               Direction direction,
                               uint64_t max) {
  application_->ExtendMaxStreams(label, direction, max);
}

bool Session::GenerateNewConnectionId(ngtcp2_cid* cid,
                                      size_t len,
                                      uint8_t* token) {
  CID cid_(cid);
  cid_factory_.Generate(cid, len);
  DEBUG_ARGS(this, "Generated new CID %s", cid_);
  StatelessResetToken new_token(
      token, endpoint_->options().reset_token_secret, cid_);
  endpoint_->AssociateCID(cid_, scid_);
  endpoint_->AssociateStatelessResetToken(new_token, this);
  return true;
}

bool Session::Receive(Store&& store,
                      const SocketAddress& local_address,
                      const SocketAddress& remote_address) {
  CHECK(!is_destroyed());

  const auto receivePacket = [&](ngtcp2_path* path, ngtcp2_vec vec) {
    CHECK(!is_destroyed());

    uint64_t now = uv_hrtime();
    ngtcp2_pkt_info pi{};  // Not used but required.
    DEBUG_ARGS(this, "Reading %" PRIu64 " byte packet", vec.len);
    int err =
        ngtcp2_conn_read_pkt(connection(), path, &pi, vec.base, vec.len, now);
    switch (err) {
      case 0: {
        DEBUG(this, "Packet received successfully");
        // Return true so we send after receiving.
        return true;
      }
      case NGTCP2_ERR_DRAINING: {
        DEBUG(this, "Connection entered draining state");
        // Connection has entered the draining state, no further data should be
        // sent. This happens when the remote peer has sent a CONNECTION_CLOSE.
        return false;
      }
      case NGTCP2_ERR_CRYPTO: {
        // Crypto error happened! Set the last error to the tls alert
        last_error_ =
            QuicError::ForTlsAlert(ngtcp2_conn_get_tls_alert(connection()));
        DEBUG_ARGS(this, "Encountered crypto error [%s]", last_error_);
        Close();
        return false;
      }
      case NGTCP2_ERR_RETRY: {
        // This should only ever happen on the server. We have to sent a path
        // validation challenge in the form of a RETRY packet to the peer and
        // drop the connection.
        CHECK(is_server());
        DEBUG(this, "Needs path validation");
        endpoint_->SendRetry(
            version(), dcid_, scid_, local_address_, remote_address_);
        CloseSilently();
        return false;
      }
      case NGTCP2_ERR_DROP_CONN: {
        DEBUG(this, "Needs drop connection");
        // There's nothing else to do but drop the connection state.
        CloseSilently();
        return false;
      }
    }
    // Shouldn't happen but just in case.
    last_error_ = QuicError::ForNgtcp2Error(err);
    DEBUG_ARGS(this,
               "There was an error processing the packet [%d, %s]",
               err,
               last_error_);
    Close();
    return false;
  };

  DEBUG_ARGS(this,
             "Receiving a packet with %" PRIu64 " bytes [%s => %s]",
             store.length(),
             remote_address,
             local_address);

  auto update_stats = OnScopeLeave([&] { UpdateDataStats(); });
  remote_address_ = remote_address;
  Path path(local_address, remote_address_);
  IncrementStat(&SessionStats::bytes_received, store.length());
  if (receivePacket(&path, store)) SendPendingDataScope send_scope(this);

  if (!is_destroyed()) UpdateTimer();

  return true;
}

int Session::ReceiveCryptoData(ngtcp2_crypto_level level,
                               uint64_t offset,
                               const uint8_t* data,
                               size_t datalen) {
  DEBUG_ARGS(this, "Receiving %" PRIu64 " bytes of crypto data", datalen);
  return crypto_context_.Receive(level, offset, data, datalen);
}

bool Session::ReceiveRxKey(ngtcp2_crypto_level level) {
  // If this is the client side, we'll initialize our application now.
  DEBUG(this, "Received rx key");
  return !is_server() && level == NGTCP2_CRYPTO_LEVEL_APPLICATION
             ? application_->Start()
             : true;
}

bool Session::ReceiveTxKey(ngtcp2_crypto_level level) {
  // If this is the server side, we'll initialize our application now.
  DEBUG(this, "Received tx key");
  return is_server() && level == NGTCP2_CRYPTO_LEVEL_APPLICATION
             ? application_->Start()
             : true;
}

void Session::ReceiveNewToken(const ngtcp2_vec* token) {
  // Currently, we don't do anything with this. We may want to use it in the
  // future.
  DEBUG(this, "Received new token");
}

void Session::ReceiveStatelessReset(const ngtcp2_pkt_stateless_reset* sr) {
  state_->stateless_reset = 1;
  DEBUG(this, "Received stateless reset");
  // TODO(@jasnell): Currently, we don't do anything more with this. What should
  // we do?
  //                 It's not yet clear if we handle this or if ngtcp2 does the
  //                 right thing.
}

void Session::ReceiveStreamData(Stream* stream,
                                Application::ReceiveStreamDataFlags flags,
                                uint64_t offset,
                                const uint8_t* data,
                                size_t datalen) {
  application_->ReceiveStreamData(stream, flags, data, datalen, offset);
}

void Session::RemoveConnectionId(const CID& cid) {
  endpoint_->DisassociateCID(cid);
}

void Session::SelectPreferredAddress(const PreferredAddress& preferredAddress) {
  if (options_.preferred_address_strategy ==
      PreferredAddress::Policy::IGNORE_PREFERED) {
    DEBUG(this, "Ignoring preferred address");
    return;
  }

  auto local_address = endpoint_->local_address().ToChecked();
  int family = local_address.family();

  switch (family) {
    case AF_INET: {
      auto ipv4 = preferredAddress.ipv4();
      if (ipv4.IsJust()) {
        PreferredAddress::AddressInfo info = ipv4.FromJust();
        if (info.address.empty() || info.port == 0) return;
        SocketAddress::New(
            AF_INET, info.address.c_str(), info.port, &remote_address_);
        DEBUG_ARGS(this, "Using preferred IPv4 address [%s]", remote_address_);
        state_->using_preferred_address = 1;
        preferredAddress.Use(info);
      }
      break;
    }
    case AF_INET6: {
      auto ipv6 = preferredAddress.ipv6();
      if (ipv6.IsJust()) {
        PreferredAddress::AddressInfo info = ipv6.FromJust();
        if (info.address.empty() || info.port == 0) return;
        SocketAddress::New(
            AF_INET, info.address.c_str(), info.port, &remote_address_);
        DEBUG_ARGS(this, "Using preferred IPv6 address [%s]", remote_address_);
        state_->using_preferred_address = 1;
        preferredAddress.Use(info);
      }
      break;
    }
  }
}

void Session::StreamClose(Stream* stream, Maybe<QuicError> error) {
  application_->StreamClose(stream, error);
}

void Session::StreamOpen(stream_id id) {
  // Currently, we don't do anything with stream open. That may change later.
}

void Session::StreamReset(Stream* stream,
                          uint64_t final_size,
                          QuicError error) {
  application_->StreamReset(stream, final_size, error);
}

void Session::StreamStopSending(Stream* stream, QuicError error) {
  application_->StreamStopSending(stream, error);
}

void Session::ReportPathValidationStatus(PathValidationResult result,
                                         PathValidationFlags flags,
                                         const SocketAddress& local_address,
                                         const SocketAddress& remote_address) {
  EmitPathValidation(result, flags, local_address, remote_address);
}

void Session::SetSessionTicketAppData(const SessionTicketAppData& app_data) {
  application_->SetSessionTicketAppData(app_data);
}

SessionTicketAppData::Status Session::GetSessionTicketAppData(
    const SessionTicketAppData& app_data, SessionTicketAppData::Flag flag) {
  return application_->GetSessionTicketAppData(app_data, flag);
}

bool Session::can_create_streams() const {
  return !state_->destroyed && !state_->graceful_closing && !state_->closing &&
         !is_in_closing_period() && !is_in_draining_period();
}

BaseObjectPtr<Stream> Session::CreateStream(stream_id id) {
  if (!can_create_streams()) {
    DEBUG(this, "Unable to create streams right now");
    // No-can-do-s-ville, My friend.
    return BaseObjectPtr<Stream>();
  }
  auto stream = Stream::Create(env(), this, id);
  if (stream) DEBUG_ARGS(this, "Created stream %" PRIu64, id);
  AddStream(stream);
  return stream;
}

BaseObjectPtr<Stream> Session::OpenStream(Direction direction) {
  if (!can_create_streams()) {
    return BaseObjectPtr<Stream>();
  }
  stream_id id;
  switch (direction) {
    case Direction::BIDIRECTIONAL:
      DEBUG(this, "Opening bidirectional stream");
      if (ngtcp2_conn_open_bidi_stream(connection(), &id, nullptr) == 0) {
        return CreateStream(id);
      }
      break;
    case Direction::UNIDIRECTIONAL:
      DEBUG(this, "Opening unidirectional stream");
      if (ngtcp2_conn_open_uni_stream(connection(), &id, nullptr) == 0) {
        return CreateStream(id);
      }
      break;
    default:
      UNREACHABLE();
  }
  return BaseObjectPtr<Stream>();
}

void Session::AddStream(const BaseObjectPtr<Stream>& stream) {
  streams_[stream->id()] = stream;

  DEBUG_ARGS(this, "Added stream %" PRIi64, stream->id());

  // Update tracking statistics for the number of streams associated with this
  // session.
  switch (stream->origin()) {
    case Stream::Origin::CLIENT:
      if (is_server())
        IncrementStat(&SessionStats::streams_in_count);
      else
        IncrementStat(&SessionStats::streams_out_count);
      break;
    case Stream::Origin::SERVER:
      if (is_server())
        IncrementStat(&SessionStats::streams_out_count);
      else
        IncrementStat(&SessionStats::streams_in_count);
  }
  IncrementStat(&SessionStats::streams_out_count);
  switch (stream->direction()) {
    case Direction::BIDIRECTIONAL:
      IncrementStat(&SessionStats::bidi_stream_count);
      break;
    case Direction::UNIDIRECTIONAL:
      IncrementStat(&SessionStats::uni_stream_count);
      break;
  }
}

void Session::RemoveStream(stream_id id) {
  // ngtcp2 does not extend the max streams count automatically except in very
  // specific conditions, none of which apply once we've gotten this far. We
  // need to manually extend when a remote peer initiated stream is removed.
  if (!is_in_draining_period() && !is_in_closing_period() &&
      !state_->silent_close &&
      !ngtcp2_conn_is_local_stream(connection_.get(), id)) {
    if (ngtcp2_is_bidi_stream(id))
      ngtcp2_conn_extend_max_streams_bidi(connection_.get(), 1);
    else
      ngtcp2_conn_extend_max_streams_uni(connection_.get(), 1);
  }

  DEBUG_ARGS(this, "Removing stream %" PRIi64, id);

  // Frees the persistent reference to the Stream object, allowing it to be gc'd
  // any time after the JS side releases it's own reference.
  streams_.erase(id);
}

void Session::ResumeStream(stream_id id) {
  application_->ResumeStream(id);
}

void Session::ShutdownStream(stream_id id, QuicError code) {
  if (is_in_closing_period() || is_in_draining_period() ||
      state_->silent_close == 1) {
    DEBUG_ARGS(this, "Shutdown stream " PRIi64 " [%s]", code);
    SendPendingDataScope send_scope(this);
    ngtcp2_conn_shutdown_stream(connection(),
                                id,
                                code.type() == QuicError::Type::APPLICATION
                                    ? code.code()
                                    : NGTCP2_APP_NOERROR);
  }
}

void Session::StreamDataBlocked(stream_id id) {
  IncrementStat(&SessionStats::block_count);
  application_->BlockStream(id);
}

void Session::ShutdownStreamWrite(stream_id id, QuicError code) {
  if (is_in_closing_period() || is_in_draining_period() ||
      state_->silent_close == 1) {
    return;  // Nothing to do because we can't send any frames.
  }
  DEBUG_ARGS(this, "Shutdown stream %" PRIi64 " write [%s]", id, code);
  SendPendingDataScope send_scope(this);
  ngtcp2_conn_shutdown_stream_write(
      connection(),
      id,
      code.type() == QuicError::Type::APPLICATION ? code.code() : 0);
}

// ======================================================================================
// V8 Callouts

void Session::EmitDatagramAcknowledged(datagram_id id) {
  CHECK(!is_destroyed());
  if (!env()->can_call_into_js()) return;

  auto& state = BindingState::Get(env());

  CallbackScope cb_scope(this);

  Local<Value> arg = BigInt::NewFromUnsigned(env()->isolate(), id);
  MakeCallback(state.session_datagram_ack_callback(), 1, &arg);
}

void Session::EmitDatagramLost(datagram_id id) {
  CHECK(!is_destroyed());
  if (!env()->can_call_into_js()) return;

  auto& state = BindingState::Get(env());

  CallbackScope cb_scope(this);

  Local<Value> arg = BigInt::NewFromUnsigned(env()->isolate(), id);
  MakeCallback(state.session_datagram_lost_callback(), 1, &arg);
}

void Session::EmitDatagram(Store&& datagram, DatagramReceivedFlag flag) {
  CHECK(!is_destroyed());
  if (!env()->can_call_into_js()) return;

  auto& state = BindingState::Get(env());

  CallbackScope cbv_scope(this);

  Local<Value> argv[] = {
      datagram.ToArrayBufferView<Uint8Array>(env()),
      flag.early ? v8::True(env()->isolate()) : v8::False(env()->isolate()),
  };

  DEBUG(this, "Emitting datagram event");

  MakeCallback(state.session_datagram_callback(), arraysize(argv), argv);
}

void Session::EmitHandshakeComplete() {
  CHECK(!is_destroyed());
  if (!env()->can_call_into_js()) return;

  auto& state = BindingState::Get(env());

  CallbackScope cb_scope(this);

  auto isolate = env()->isolate();
  Local<Value> argv[] = {Undefined(isolate),     // The negotiated server name
                         GetALPNProtocol(this),  // The negotiated alpn protocol
                         Undefined(isolate),     // Cipher name
                         Undefined(isolate),     // Cipher version
                         Undefined(isolate),     // Validation error reason
                         Undefined(isolate),     // Validation error code
                         crypto_context_.was_early_data_accepted()
                             ? v8::True(isolate)
                             : v8::False(isolate)};

  static constexpr auto kServerName = 0;
  static constexpr auto kCipherName = 2;
  static constexpr auto kCipherVersion = 3;
  static constexpr auto kValidationErrorReason = 4;
  static constexpr auto kValidationErrorCode = 5;

  int err = crypto_context_.VerifyPeerIdentity();

  if (!ToV8Value(env()->context(), crypto_context_.servername())
           .ToLocal(&argv[kServerName]) ||
      !crypto_context_.cipher_name(env()).ToLocal(&argv[kCipherName]) ||
      !crypto_context_.cipher_version(env()).ToLocal(&argv[kCipherVersion])) {
    return;
  }

  if (err != X509_V_OK && (!crypto::GetValidationErrorReason(env(), err)
                                .ToLocal(&argv[kValidationErrorReason]) ||
                           !crypto::GetValidationErrorCode(env(), err)
                                .ToLocal(&argv[kValidationErrorCode]))) {
    return;
  }

  DEBUG(this, "Emitting handshake complete event");

  MakeCallback(state.session_handshake_callback(), arraysize(argv), argv);
}

void Session::EmitVersionNegotiation(const ngtcp2_pkt_hd& hd,
                                     const quic_version* sv,
                                     size_t nsv) {
  CHECK(!is_destroyed());
  CHECK(!is_server());
  if (!env()->can_call_into_js()) return;

  auto& state = BindingState::Get(env());

  CallbackScope cb_scope(this);

  std::vector<Local<Value>> versions(nsv);

  auto isolate = env()->isolate();

  for (size_t n = 0; n < nsv; n++) versions[n] = Integer::New(isolate, sv[n]);

  // Currently, we only support one version of QUIC but in
  // the future that may change. The callback below passes
  // an array back to the JavaScript side to future-proof.
  Local<Value> supported = Integer::New(isolate, NGTCP2_PROTO_VER_MAX);

  Local<Value> argv[] = {// The version configured for this session.
                         Integer::New(isolate, version()),
                         // The versions requested.
                         Array::New(isolate, versions.data(), nsv),
                         // The versions we actually support.
                         Array::New(isolate, &supported, 1)};

  DEBUG(this, "Emitting version negotiation event");

  MakeCallback(
      state.session_version_negotiation_callback(), arraysize(argv), argv);
}

void Session::EmitSessionTicket(Store&& ticket) {
  CHECK(!is_destroyed());
  if (!env()->can_call_into_js()) return;

  // If there is nothing listening for the session ticket, don't both emitting
  // it.
  if (LIKELY(state_->session_ticket == 0)) return;

  CallbackScope cb_scope(this);

  auto remote_transport_params = GetRemoteTransportParams();
  Store transport_params;
  if (remote_transport_params)
    transport_params = remote_transport_params.Encode(env());

  auto sessionTicket = SessionTicket::Create(
      env(), std::move(ticket), std::move(transport_params));
  Local<Value> argv = sessionTicket->object();

  DEBUG(this, "Emitting session ticket event");

  MakeCallback(BindingState::Get(env()).session_ticket_callback(), 1, &argv);
}

void Session::EmitError(QuicError error) {
  CHECK(!is_destroyed());

  if (!env()->can_call_into_js()) return Destroy();

  CallbackScope cb_scope(this);
  Local<Value> argv[] = {
      Integer::New(env()->isolate(), static_cast<int>(error.type())),
      BigInt::NewFromUnsigned(env()->isolate(), error.code()),
      Undefined(env()->isolate()),
  };
  if (error->reasonlen > 0 &&
      !ToV8Value(env()->context(), error.reason()).ToLocal(&argv[2])) {
    return;
  }
  MakeCallback(
      BindingState::Get(env()).session_error_callback(), arraysize(argv), argv);
}

void Session::EmitClose() {
  CHECK(!is_destroyed());

  if (!env()->can_call_into_js()) return Destroy();

  CallbackScope cb_scope(this);

  // If last_error_.code() is anything other than 0, then we will emit
  // an error. Otherwise, nothing will be emitted.
  if (last_error_->error_code == NGTCP2_NO_ERROR ||
      (last_error_->type ==
           NGTCP2_CONNECTION_CLOSE_ERROR_CODE_TYPE_APPLICATION &&
       last_error_->error_code == NGTCP2_APP_NOERROR)) {
    MakeCallback(BindingState::Get(env()).session_close_callback(), 0, nullptr);
    return;
  }

  // Otherwise, we will emit the codes and let the JavaScript side construct a
  // proper error from them.

  EmitError(last_error_);
}

bool Session::EmitClientHello() {
  CHECK(!is_destroyed());

  if (!env()->can_call_into_js()) {
    // If we can't call into JavaScript here, technically we probably shouldn't
    // be able to do anything... but, let's try to gracefully handle it without
    // blocking the tls handshake. If there's an error, we'll handle down the
    // line.
    crypto_context_.OnClientHelloDone();
    return true;
  }

  auto isolate = env()->isolate();

  CallbackScope cb_scope(this);

  Local<Value> argv[3] = {
      Undefined(isolate), Undefined(isolate), Undefined(isolate)};

  if (!crypto_context_.hello_alpn(env()).ToLocal(&argv[0]) ||
      !crypto_context_.hello_servername(env()).ToLocal(&argv[1]) ||
      !crypto_context_.hello_ciphers(env()).ToLocal(&argv[2])) {
    return false;
  }

  DEBUG(this, "Emitting client hello event");

  MakeCallback(BindingState::Get(env()).session_client_hello_callback(),
               arraysize(argv),
               argv);

  return true;
}

bool Session::EmitOCSP() {
  CHECK(!is_destroyed());

  if (!env()->can_call_into_js()) {
    // If we can't call into JavaScript here, technically we probably shouldn't
    // be able to do anything... but, let's try to gracefully handle it without
    // blocking the tls handshake. If there's an error, we'll handle down the
    // line.
    crypto_context_.OnOCSPDone(Store());
    return true;
  }

  auto isolate = env()->isolate();

  CallbackScope cb_scope(this);

  Local<Value> argv[2] = {v8::Undefined(isolate), v8::Undefined(isolate)};

  if (!GetCertificateData(env(),
                          crypto_context_.secure_context_.get(),
                          GetCertificateType::SELF)
           .ToLocal(&argv[0]) ||
      !GetCertificateData(env(),
                          crypto_context_.secure_context_.get(),
                          GetCertificateType::ISSUER)
           .ToLocal(&argv[1])) {
    return false;
  }

  DEBUG(this, "Emitting ocsp request event");

  MakeCallback(BindingState::Get(env()).session_ocsp_request_callback(),
               arraysize(argv),
               argv);
  return true;
}

void Session::EmitOCSPResponse() {
  CHECK(!is_destroyed());

  if (!env()->can_call_into_js()) return;

  auto isolate = env()->isolate();

  CallbackScope cb_scope(this);

  Local<Value> res = v8::Undefined(isolate);

  const unsigned char* resp;
  int len = SSL_get_tlsext_status_ocsp_resp(crypto_context_.ssl_.get(), &resp);
  if (resp != nullptr && len > 0) {
    std::shared_ptr<BackingStore> store =
        ArrayBuffer::NewBackingStore(isolate, len);
    memcpy(store->Data(), resp, len);
    res = ArrayBuffer::New(isolate, store);
  }

  DEBUG(this, "Emitting ocsp response event");

  MakeCallback(
      BindingState::Get(env()).session_ocsp_response_callback(), 1, &res);
}

void Session::EmitPathValidation(PathValidationResult result,
                                 PathValidationFlags flags,
                                 const SocketAddress& local_address,
                                 const SocketAddress& remote_address) {
  CHECK(!is_destroyed());

  if (!env()->can_call_into_js()) return;

  if (LIKELY(state_->path_validation == 0)) return;

  auto isolate = env()->isolate();

  CallbackScope cb_scope(this);

  auto& state = BindingState::Get(env());

  const auto resultToString = [&] {
    switch (result) {
      case PathValidationResult::ABORTED:
        return state.aborted_string();
      case PathValidationResult::FAILURE:
        return state.failure_string();
      case PathValidationResult::SUCCESS:
        return state.success_string();
    }
    UNREACHABLE();
  };

  Local<Value> argv[4] = {
      resultToString(),
      SocketAddressBase::Create(env(),
                                std::make_shared<SocketAddress>(local_address))
          ->object(),
      SocketAddressBase::Create(env(),
                                std::make_shared<SocketAddress>(remote_address))
          ->object(),
      flags.preferredAddress ? v8::True(isolate) : v8::False(isolate),
  };

  DEBUG(this, "Emitting path validation event");

  MakeCallback(BindingState::Get(env()).session_ocsp_request_callback(),
               arraysize(argv),
               argv);
}

void Session::EmitKeylog(const char* line) {
  if (!env()->can_call_into_js()) return;
  if (keylogstream_) {
    std::string data = line;
    data += "\n";
    env()->SetImmediate([ptr = keylogstream_, data = std::move(data)](
                            Environment* env) { ptr->Emit(data); });
  }
}

void Session::EmitNewStream(const BaseObjectPtr<Stream>& stream) {
  if (is_destroyed()) return;
  if (!env()->can_call_into_js()) return;
  CallbackScope cb_scope(this);
  Local<Value> arg = stream->object();

  DEBUG(this, "Emitting new stream event");

  MakeCallback(BindingState::Get(env()).stream_created_callback(), 1, &arg);
}

// ======================================================================================
// V8 Callbacks

void Session::DoDestroy(const FunctionCallbackInfo<Value>& args) {
  Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  session->Destroy();
}

void Session::GetRemoteAddress(const FunctionCallbackInfo<Value>& args) {
  auto env = Environment::GetCurrent(args);
  Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  auto address = session->remote_address();
  args.GetReturnValue().Set(
      SocketAddressBase::Create(env, std::make_shared<SocketAddress>(address))
          ->object());
}

void Session::GetCertificate(const FunctionCallbackInfo<Value>& args) {
  auto env = Environment::GetCurrent(args);
  Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  Local<Value> ret;
  if (session->crypto_context().cert(env).ToLocal(&ret))
    args.GetReturnValue().Set(ret);
}

void Session::GetEphemeralKeyInfo(const FunctionCallbackInfo<Value>& args) {
  auto env = Environment::GetCurrent(args);
  Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  Local<Object> ret;
  if (!session->is_server() &&
      session->crypto_context().ephemeral_key(env).ToLocal(&ret))
    args.GetReturnValue().Set(ret);
}

void Session::GetPeerCertificate(const FunctionCallbackInfo<Value>& args) {
  auto env = Environment::GetCurrent(args);
  Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  Local<Value> ret;
  if (session->crypto_context().peer_cert(env).ToLocal(&ret))
    args.GetReturnValue().Set(ret);
}

void Session::GracefulClose(const FunctionCallbackInfo<Value>& args) {
  Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  session->CloseGracefully();
}

void Session::SilentClose(const FunctionCallbackInfo<Value>& args) {
  // This is intended for testing only!
  Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  session->CloseSilently();
}

void Session::UpdateKey(const FunctionCallbackInfo<Value>& args) {
  Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  // Initiating a key update may fail if it is done too early (either
  // before the TLS handshake has been confirmed or while a previous
  // key update is being processed). When it fails, InitiateKeyUpdate()
  // will return false.
  session->IncrementStat(&SessionStats::keyupdate_count);
  args.GetReturnValue().Set(session->crypto_context().InitiateKeyUpdate());
}

void Session::OnClientHelloDone(const FunctionCallbackInfo<Value>& args) {
  Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  session->crypto_context().OnClientHelloDone();
}

void Session::OnOCSPDone(const FunctionCallbackInfo<Value>& args) {
  Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  if (args[0]->IsArrayBufferView()) {
    session->crypto_context().OnOCSPDone(Store(args[0].As<ArrayBufferView>()));
  } else {
    session->crypto_context().OnOCSPDone(Store());
  }
}

void Session::DoOpenStream(const FunctionCallbackInfo<Value>& args) {
  Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  CHECK(args[0]->IsUint32());
  Direction direction = static_cast<Direction>(args[0].As<Uint32>()->Value());
  BaseObjectPtr<Stream> stream = session->OpenStream(direction);

  if (stream) args.GetReturnValue().Set(stream->object());
}

void Session::DoSendDatagram(const FunctionCallbackInfo<Value>& args) {
  auto env = Environment::GetCurrent(args);
  Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  CHECK(args[0]->IsArrayBufferView());
  args.GetReturnValue().Set(
      BigInt::New(env->isolate(),
                  session->SendDatagram(Store(args[0].As<ArrayBufferView>()))));
}

// ======================================================================================
// ngtcp2 static callbacks

const ngtcp2_callbacks Session::callbacks[2] = {
    // NGTCP2_CRYPTO_SIDE_CLIENT
    {ngtcp2_crypto_client_initial_cb,
     nullptr,
     OnReceiveCryptoData,
     OnHandshakeCompleted,
     OnReceiveVersionNegotiation,
     ngtcp2_crypto_encrypt_cb,
     ngtcp2_crypto_decrypt_cb,
     ngtcp2_crypto_hp_mask_cb,
     OnReceiveStreamData,
     OnAcknowledgeStreamDataOffset,
     OnStreamOpen,
     OnStreamClose,
     OnReceiveStatelessReset,
     ngtcp2_crypto_recv_retry_cb,
     OnExtendMaxStreamsBidi,
     OnExtendMaxStreamsUni,
     OnRand,
     OnGetNewConnectionId,
     OnRemoveConnectionId,
     ngtcp2_crypto_update_key_cb,
     OnPathValidation,
     OnSelectPreferredAddress,
     OnStreamReset,
     OnExtendMaxRemoteStreamsBidi,
     OnExtendMaxRemoteStreamsUni,
     OnExtendMaxStreamData,
     OnConnectionIdStatus,
     OnHandshakeConfirmed,
     OnReceiveNewToken,
     ngtcp2_crypto_delete_crypto_aead_ctx_cb,
     ngtcp2_crypto_delete_crypto_cipher_ctx_cb,
     OnReceiveDatagram,
     OnAcknowledgeDatagram,
     OnLostDatagram,
     OnGetPathChallengeData,
     OnStreamStopSending,
     ngtcp2_crypto_version_negotiation_cb,
     OnReceiveRxKey,
     OnReceiveTxKey},
    // NGTCP2_CRYPTO_SIDE_SERVER
    {nullptr,
     ngtcp2_crypto_recv_client_initial_cb,
     OnReceiveCryptoData,
     OnHandshakeCompleted,
     nullptr,
     ngtcp2_crypto_encrypt_cb,
     ngtcp2_crypto_decrypt_cb,
     ngtcp2_crypto_hp_mask_cb,
     OnReceiveStreamData,
     OnAcknowledgeStreamDataOffset,
     OnStreamOpen,
     OnStreamClose,
     OnReceiveStatelessReset,
     nullptr,
     OnExtendMaxStreamsBidi,
     OnExtendMaxStreamsUni,
     OnRand,
     OnGetNewConnectionId,
     OnRemoveConnectionId,
     ngtcp2_crypto_update_key_cb,
     OnPathValidation,
     nullptr,
     OnStreamReset,
     OnExtendMaxRemoteStreamsBidi,
     OnExtendMaxRemoteStreamsUni,
     OnExtendMaxStreamData,
     OnConnectionIdStatus,
     nullptr,
     nullptr,
     ngtcp2_crypto_delete_crypto_aead_ctx_cb,
     ngtcp2_crypto_delete_crypto_cipher_ctx_cb,
     OnReceiveDatagram,
     OnAcknowledgeDatagram,
     OnLostDatagram,
     OnGetPathChallengeData,
     OnStreamStopSending,
     ngtcp2_crypto_version_negotiation_cb,
     OnReceiveRxKey,
     OnReceiveTxKey}};

#define NGTCP2_CALLBACK_SCOPE(name)                                            \
  auto name = Session::From(conn, user_data);                                  \
  if (UNLIKELY(name->is_destroyed())) return NGTCP2_ERR_CALLBACK_FAILURE;      \
  NgCallbackScope callback_scope(name);

int Session::OnAcknowledgeStreamDataOffset(ngtcp2_conn* conn,
                                           int64_t stream_id,
                                           uint64_t offset,
                                           uint64_t datalen,
                                           void* user_data,
                                           void* stream_user_data) {
  NGTCP2_CALLBACK_SCOPE(session)
  session->AcknowledgeStreamDataOffset(
      Stream::From(conn, stream_user_data), offset, datalen);
  return NGTCP2_SUCCESS;
}

int Session::OnAcknowledgeDatagram(ngtcp2_conn* conn,
                                   uint64_t dgram_id,
                                   void* user_data) {
  NGTCP2_CALLBACK_SCOPE(session)
  session->DatagramAcknowledged(dgram_id);
  return NGTCP2_SUCCESS;
}

int Session::OnConnectionIdStatus(ngtcp2_conn* conn,
                                  int type,
                                  uint64_t seq,
                                  const ngtcp2_cid* cid,
                                  const uint8_t* token,
                                  void* user_data) {
  NGTCP2_CALLBACK_SCOPE(session);
  switch (type) {
    case NGTCP2_CONNECTION_ID_STATUS_TYPE_ACTIVATE:
      session->ActivateConnectionId(seq,
                                    CID(cid),
                                    token != nullptr
                                        ? Just(StatelessResetToken(token))
                                        : Nothing<StatelessResetToken>());
      break;
    case NGTCP2_CONNECTION_ID_STATUS_TYPE_DEACTIVATE:
      session->DeactivateConnectionId(seq,
                                      CID(cid),
                                      token != nullptr
                                          ? Just(StatelessResetToken(token))
                                          : Nothing<StatelessResetToken>());
      break;
    default:
      UNREACHABLE();
  }
  return NGTCP2_SUCCESS;
}

int Session::OnExtendMaxRemoteStreamsBidi(ngtcp2_conn* conn,
                                          uint64_t max_streams,
                                          void* user_data) {
  NGTCP2_CALLBACK_SCOPE(session);
  session->ExtendMaxStreams(
      EndpointLabel::REMOTE, Direction::BIDIRECTIONAL, max_streams);
  return NGTCP2_SUCCESS;
}

int Session::OnExtendMaxRemoteStreamsUni(ngtcp2_conn* conn,
                                         uint64_t max_streams,
                                         void* user_data) {
  NGTCP2_CALLBACK_SCOPE(session);
  session->ExtendMaxStreams(
      EndpointLabel::REMOTE, Direction::UNIDIRECTIONAL, max_streams);
  return NGTCP2_SUCCESS;
}

int Session::OnExtendMaxStreamsBidi(ngtcp2_conn* conn,
                                    uint64_t max_streams,
                                    void* user_data) {
  NGTCP2_CALLBACK_SCOPE(session);
  session->ExtendMaxStreams(
      EndpointLabel::LOCAL, Direction::BIDIRECTIONAL, max_streams);
  return NGTCP2_SUCCESS;
}

int Session::OnExtendMaxStreamData(ngtcp2_conn* conn,
                                   int64_t stream_id,
                                   uint64_t max_data,
                                   void* user_data,
                                   void* stream_user_data) {
  NGTCP2_CALLBACK_SCOPE(session);
  session->ExtendMaxStreamData(Stream::From(conn, stream_user_data), max_data);
  return NGTCP2_SUCCESS;
}

int Session::OnExtendMaxStreamsUni(ngtcp2_conn* conn,
                                   uint64_t max_streams,
                                   void* user_data) {
  NGTCP2_CALLBACK_SCOPE(session);
  session->ExtendMaxStreams(
      EndpointLabel::LOCAL, Direction::BIDIRECTIONAL, max_streams);
  return NGTCP2_SUCCESS;
}

int Session::OnGetNewConnectionId(ngtcp2_conn* conn,
                                  ngtcp2_cid* cid,
                                  uint8_t* token,
                                  size_t cidlen,
                                  void* user_data) {
  NGTCP2_CALLBACK_SCOPE(session);
  return session->GenerateNewConnectionId(cid, cidlen, token)
             ? NGTCP2_SUCCESS
             : NGTCP2_ERR_CALLBACK_FAILURE;
}

int Session::OnGetPathChallengeData(ngtcp2_conn* conn,
                                    uint8_t* data,
                                    void* user_data) {
  // For now, simple random data will suffice. Later we might need to make this
  // more cryptographically secure / pseudorandom for more protection.
  CHECK(CSPRNG(data, NGTCP2_PATH_CHALLENGE_DATALEN).is_ok());
  return NGTCP2_SUCCESS;
}

int Session::OnHandshakeCompleted(ngtcp2_conn* conn, void* user_data) {
  NGTCP2_CALLBACK_SCOPE(session);
  return session->HandshakeCompleted() ? NGTCP2_SUCCESS
                                       : NGTCP2_ERR_CALLBACK_FAILURE;
}

int Session::OnHandshakeConfirmed(ngtcp2_conn* conn, void* user_data) {
  NGTCP2_CALLBACK_SCOPE(session);
  session->HandshakeConfirmed();
  return NGTCP2_SUCCESS;
}

int Session::OnLostDatagram(ngtcp2_conn* conn,
                            uint64_t dgram_id,
                            void* user_data) {
  NGTCP2_CALLBACK_SCOPE(session);
  session->DatagramLost(dgram_id);
  return NGTCP2_SUCCESS;
}

int Session::OnPathValidation(ngtcp2_conn* conn,
                              uint32_t flags,
                              const ngtcp2_path* path,
                              ngtcp2_path_validation_result res,
                              void* user_data) {
  NGTCP2_CALLBACK_SCOPE(session);

  session->ReportPathValidationStatus(
      static_cast<PathValidationResult>(res),
      PathValidationFlags{/* .preferredAddres = */ QUIC_FLAG(
          NGTCP2_PATH_VALIDATION_FLAG_PREFERRED_ADDR)},
      SocketAddress(path->local.addr),
      SocketAddress(path->remote.addr));
  return NGTCP2_SUCCESS;
}

int Session::OnReceiveCryptoData(ngtcp2_conn* conn,
                                 ngtcp2_crypto_level crypto_level,
                                 uint64_t offset,
                                 const uint8_t* data,
                                 size_t datalen,
                                 void* user_data) {
  NGTCP2_CALLBACK_SCOPE(session);
  return session->ReceiveCryptoData(crypto_level, offset, data, datalen);
}

int Session::OnReceiveDatagram(ngtcp2_conn* conn,
                               uint32_t flags,
                               const uint8_t* data,
                               size_t datalen,
                               void* user_data) {
  NGTCP2_CALLBACK_SCOPE(session);
  session->DatagramReceived(data,
                            datalen,
                            DatagramReceivedFlag{/* .early = */ QUIC_FLAG(
                                NGTCP2_DATAGRAM_FLAG_EARLY)});
  return NGTCP2_SUCCESS;
}

int Session::OnReceiveNewToken(ngtcp2_conn* conn,
                               const ngtcp2_vec* token,
                               void* user_data) {
  NGTCP2_CALLBACK_SCOPE(session);
  session->ReceiveNewToken(token);
  return NGTCP2_SUCCESS;
}

int Session::OnReceiveRxKey(ngtcp2_conn* conn,
                            ngtcp2_crypto_level level,
                            void* user_data) {
  return Session::From(conn, user_data)->ReceiveRxKey(level)
             ? NGTCP2_SUCCESS
             : NGTCP2_ERR_CALLBACK_FAILURE;
}

int Session::OnReceiveTxKey(ngtcp2_conn* conn,
                            ngtcp2_crypto_level level,
                            void* user_data) {
  return Session::From(conn, user_data)->ReceiveTxKey(level)
             ? NGTCP2_SUCCESS
             : NGTCP2_ERR_CALLBACK_FAILURE;
}

int Session::OnReceiveStatelessReset(ngtcp2_conn* conn,
                                     const ngtcp2_pkt_stateless_reset* sr,
                                     void* user_data) {
  NGTCP2_CALLBACK_SCOPE(session);
  session->ReceiveStatelessReset(sr);
  return NGTCP2_SUCCESS;
}

int Session::OnReceiveStreamData(ngtcp2_conn* conn,
                                 uint32_t flags,
                                 int64_t stream_id,
                                 uint64_t offset,
                                 const uint8_t* data,
                                 size_t datalen,
                                 void* user_data,
                                 void* stream_user_data) {
  NGTCP2_CALLBACK_SCOPE(session);

  Application::ReceiveStreamDataFlags receive_flags{
      /* .fin =   */ QUIC_FLAG(NGTCP2_STREAM_DATA_FLAG_FIN),
      /* .early = */ QUIC_FLAG(NGTCP2_STREAM_DATA_FLAG_EARLY)};

  if (stream_user_data == nullptr) {
    // What we likely have here is an implicitly created stream. Let's try to
    // create it. If successful, we'll pass our lovely bit of received data on
    // to it for processing. Otherwise, we are going to tell ngtcp2 to shut down
    // the stream.
    auto stream = session->CreateStream(stream_id);
    if (stream) {
      session->EmitNewStream(stream);
      session->ReceiveStreamData(
          stream.get(), receive_flags, offset, data, datalen);
    } else {
      USE(ngtcp2_conn_shutdown_stream(
          session->connection(), stream_id, NGTCP2_APP_NOERROR));
    }
  } else {
    session->ReceiveStreamData(Stream::From(conn, stream_user_data),
                               receive_flags,
                               offset,
                               data,
                               datalen);
  }
  return NGTCP2_SUCCESS;
}

int Session::OnReceiveVersionNegotiation(ngtcp2_conn* conn,
                                         const ngtcp2_pkt_hd* hd,
                                         const uint32_t* sv,
                                         size_t nsv,
                                         void* user_data) {
  NGTCP2_CALLBACK_SCOPE(session);
  session->EmitVersionNegotiation(*hd, sv, nsv);
  return NGTCP2_SUCCESS;
}

int Session::OnRemoveConnectionId(ngtcp2_conn* conn,
                                  const ngtcp2_cid* cid,
                                  void* user_data) {
  NGTCP2_CALLBACK_SCOPE(session);
  session->RemoveConnectionId(CID(cid));
  return NGTCP2_SUCCESS;
}

int Session::OnSelectPreferredAddress(ngtcp2_conn* conn,
                                      ngtcp2_path* dest,
                                      const ngtcp2_preferred_addr* paddr,
                                      void* user_data) {
  NGTCP2_CALLBACK_SCOPE(session);
  session->SelectPreferredAddress(
      PreferredAddress(session->env(), dest, paddr));
  return NGTCP2_SUCCESS;
}

int Session::OnStreamClose(ngtcp2_conn* conn,
                           uint32_t flags,
                           int64_t stream_id,
                           uint64_t app_error_code,
                           void* user_data,
                           void* stream_user_data) {
  NGTCP2_CALLBACK_SCOPE(session);
  session->StreamClose(Stream::From(conn, stream_user_data),
                       QUIC_FLAG(NGTCP2_STREAM_CLOSE_FLAG_APP_ERROR_CODE_SET)
                           ? Just(QuicError::ForApplication(app_error_code))
                           : Nothing<QuicError>());
  return NGTCP2_SUCCESS;
}

int Session::OnStreamOpen(ngtcp2_conn* conn,
                          int64_t stream_id,
                          void* user_data) {
  NGTCP2_CALLBACK_SCOPE(session);
  session->StreamOpen(stream_id);
  return NGTCP2_SUCCESS;
}

int Session::OnStreamReset(ngtcp2_conn* conn,
                           int64_t stream_id,
                           uint64_t final_size,
                           uint64_t app_error_code,
                           void* user_data,
                           void* stream_user_data) {
  NGTCP2_CALLBACK_SCOPE(session);
  session->StreamReset(Stream::From(conn, stream_user_data),
                       final_size,
                       QuicError::ForApplication(app_error_code));
  return NGTCP2_SUCCESS;
}

int Session::OnStreamStopSending(ngtcp2_conn* conn,
                                 int64_t stream_id,
                                 uint64_t app_error_code,
                                 void* user_data,
                                 void* stream_user_data) {
  NGTCP2_CALLBACK_SCOPE(session);
  session->StreamStopSending(Stream::From(conn, stream_user_data),
                             QuicError::ForApplication(app_error_code));
  return NGTCP2_SUCCESS;
}

void Session::OnRand(uint8_t* dest, size_t destlen, const ngtcp2_rand_ctx*) {
  CHECK(CSPRNG(dest, destlen).is_ok());
}

// ======================================================================================
// Session::OptionsObject

void Session::OptionsObject::Initialize(Environment* env,
                                        Local<Object> target) {
  SetConstructorFunction(env->context(),
                         target,
                         "SessionOptions",
                         GetConstructorTemplate(env),
                         SetConstructorFunctionFlag::NONE);
}

void Session::OptionsObject::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(New);
}

Local<FunctionTemplate> Session::OptionsObject::GetConstructorTemplate(
    Environment* env) {
  auto& state = BindingState::Get(env);
  Local<FunctionTemplate> tmpl = state.session_options_constructor_template();
  if (tmpl.IsEmpty()) {
    auto isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, New);
    tmpl->Inherit(BaseObject::GetConstructorTemplate(env));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        OptionsObject::kInternalFieldCount);
    tmpl->SetClassName(state.session_options_string());
    state.set_session_options_constructor_template(tmpl);
  }
  return tmpl;
}

Session::OptionsObject::OptionsObject(Environment* env,
                                      v8::Local<v8::Object> object)
    : BaseObject(env, object) {
  MakeWeak();
}

template <typename Opt>
Maybe<bool> Session::OptionsObject::SetOption(Opt* options,
                                              const Local<Object>& object,
                                              const Local<String>& name,
                                              uint64_t Opt::*member) {
  Local<Value> value;
  if (!object->Get(env()->context(), name).ToLocal(&value))
    return Nothing<bool>();

  if (value->IsUndefined()) return Just(false);

  CHECK_IMPLIES(!value->IsBigInt(), value->IsNumber());

  uint64_t val = 0;
  if (value->IsBigInt()) {
    bool lossless = true;
    val = value.As<BigInt>()->Uint64Value(&lossless);
    if (!lossless) {
      Utf8Value label(env()->isolate(), name);
      THROW_ERR_OUT_OF_RANGE(
          env(),
          (std::string("options.") + (*label) + " is out of range").c_str());
      return Nothing<bool>();
    }
  } else {
    val = static_cast<int64_t>(value.As<Number>()->Value());
  }
  options->*member = val;
  return Just(true);
}

template <typename Opt>
Maybe<bool> Session::OptionsObject::SetOption(Opt* options,
                                              const Local<Object>& object,
                                              const Local<String>& name,
                                              uint32_t Opt::*member) {
  Local<Value> value;
  if (!object->Get(env()->context(), name).ToLocal(&value))
    return Nothing<bool>();

  if (value->IsUndefined()) return Just(false);

  CHECK(value->IsUint32());
  uint32_t val = value.As<Uint32>()->Value();
  options->*member = val;
  return Just(true);
}

template <typename Opt>
Maybe<bool> Session::OptionsObject::SetOption(Opt* options,
                                              const Local<Object>& object,
                                              const Local<String>& name,
                                              bool Opt::*member) {
  Local<Value> value;
  if (!object->Get(env()->context(), name).ToLocal(&value))
    return Nothing<bool>();
  if (value->IsUndefined()) return Just(false);
  CHECK(value->IsBoolean());
  options->*member = value->IsTrue();
  return Just(true);
}

template <typename Opt>
Maybe<bool> Session::OptionsObject::SetOption(Opt* options,
                                              const Local<Object>& object,
                                              const Local<String>& name,
                                              std::string Opt::*member) {
  Local<Value> value;
  if (!object->Get(env()->context(), name).ToLocal(&value))
    return Nothing<bool>();
  if (value->IsUndefined()) return Just(false);
  Utf8Value val(env()->isolate(), value);
  options->*member = val.ToString();
  return Just(true);
}

template <typename Opt>
Maybe<bool> Session::OptionsObject::SetOption(
    Opt* options,
    const Local<Object>& object,
    const Local<String>& name,
    std::vector<std::shared_ptr<crypto::KeyObjectData>> Opt::*member) {
  Local<Value> value;
  if (!object->Get(env()->context(), name).ToLocal(&value))
    return Nothing<bool>();
  if (value->IsArray()) {
    auto context = env()->context();
    auto values = value.As<v8::Array>();
    uint32_t count = values->Length();
    for (uint32_t n = 0; n < count; n++) {
      Local<Value> item;
      if (!values->Get(context, n).ToLocal(&item)) {
        return Nothing<bool>();
      }
      if (crypto::KeyObjectHandle::HasInstance(env(), item)) {
        crypto::KeyObjectHandle* handle;
        ASSIGN_OR_RETURN_UNWRAP(&handle, item, Nothing<bool>());
        (options->*member).push_back(handle->Data());
      }
    }
  } else if (crypto::KeyObjectHandle::HasInstance(env(), value)) {
    crypto::KeyObjectHandle* handle;
    ASSIGN_OR_RETURN_UNWRAP(&handle, value, Nothing<bool>());
    (options->*member).push_back(handle->Data());
  } else {
    UNREACHABLE();
  }
  return Just(true);
}

template <typename Opt>
Maybe<bool> Session::OptionsObject::SetOption(Opt* options,
                                              const Local<Object>& object,
                                              const Local<String>& name,
                                              std::vector<Store> Opt::*member) {
  Local<Value> value;
  if (!object->Get(env()->context(), name).ToLocal(&value))
    return Nothing<bool>();
  if (value->IsArray()) {
    auto context = env()->context();
    auto values = value.As<v8::Array>();
    uint32_t count = values->Length();
    for (uint32_t n = 0; n < count; n++) {
      Local<Value> item;
      if (!values->Get(context, n).ToLocal(&item)) {
        return Nothing<bool>();
      }
      if (item->IsArrayBufferView()) {
        Store store(item.As<ArrayBufferView>());
        (options->*member).push_back(std::move(store));
      }
    }
  } else if (value->IsArrayBufferView()) {
    Store store(value.As<ArrayBufferView>());
    (options->*member).push_back(std::move(store));
  }

  return Just(true);
}

void Session::OptionsObject::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  auto env = Environment::GetCurrent(args);
  auto& state = BindingState::Get(env);

  static constexpr auto kAlpn = 0;
  static constexpr auto kHostname = 1;
  static constexpr auto kPreferredAddressStrategy = 2;
  static constexpr auto kConnectionIdFactory = 3;
  static constexpr auto kQlogEnabled = 4;
  static constexpr auto kTlsOptions = 5;
  static constexpr auto kApplicationOptions = 6;
  static constexpr auto kTransportParams = 7;
  static constexpr auto kIpv4PreferredAddress = 8;
  static constexpr auto kIpv6PreferredAddress = 9;

  CHECK(args[kAlpn]->IsString());
  CHECK_IMPLIES(!args[kHostname]->IsUndefined(), args[kHostname]->IsString());
  CHECK_IMPLIES(!args[kPreferredAddressStrategy]->IsUndefined(),
                args[kPreferredAddressStrategy]->IsInt32());
  CHECK_IMPLIES(!args[kConnectionIdFactory]->IsUndefined(),
                args[kConnectionIdFactory]->IsObject());
  CHECK_IMPLIES(!args[kQlogEnabled]->IsUndefined(),
                args[kQlogEnabled]->IsBoolean());
  CHECK_IMPLIES(!args[kTlsOptions]->IsUndefined(),
                args[kTlsOptions]->IsObject());
  CHECK_IMPLIES(!args[kApplicationOptions]->IsUndefined(),
                args[kApplicationOptions]->IsObject());
  CHECK_IMPLIES(!args[kTransportParams]->IsUndefined(),
                args[kTransportParams]->IsObject());
  CHECK_IMPLIES(
      !args[kIpv4PreferredAddress]->IsUndefined(),
      SocketAddressBase::HasInstance(env, args[kIpv4PreferredAddress]));
  CHECK_IMPLIES(
      !args[kIpv6PreferredAddress]->IsUndefined(),
      SocketAddressBase::HasInstance(env, args[kIpv6PreferredAddress]));

  OptionsObject* options = new OptionsObject(env, args.This());

  Utf8Value alpn(env->isolate(), args[kAlpn]);
  options->options_.alpn = std::string(1, alpn.length()) + (*alpn);

  if (!args[kHostname]->IsUndefined()) {
    Utf8Value hostname(env->isolate(), args[kHostname]);
    options->options_.hostname = *hostname;
  }

  if (!args[kPreferredAddressStrategy]->IsUndefined()) {
    auto value = args[kPreferredAddressStrategy].As<Int32>()->Value();
    if (value < 0 || value > static_cast<int>(PreferredAddress::Policy::USE)) {
      THROW_ERR_INVALID_ARG_VALUE(env, "Invalid preferred address policy.");
      return;
    }
    options->options_.preferred_address_strategy =
        static_cast<PreferredAddress::Policy>(value);
  }

  // TODO(@jasnell): Skipping this for now
  // Add support for the other strategies once implemented
  // if (RandomConnectionIDBase::HasInstance(env, args[5])) {
  //   RandomConnectionIDBase* cid_strategy;
  //   ASSIGN_OR_RETURN_UNWRAP(&cid_strategy, args[5]);
  //   options->options()->cid_strategy = cid_strategy->strategy();
  //   options->options()->cid_strategy_strong_ref.reset(cid_strategy);
  // } else {
  //   UNREACHABLE();
  // }

  options->options_.qlog = args[kQlogEnabled]->IsTrue();

  if (!args[kTlsOptions]->IsUndefined()) {
    // TLS Options
    Local<Object> tls_options = args[kTlsOptions].As<Object>();

    if (UNLIKELY(options
                     ->SetOption(&options->options_.crypto_options,
                                 tls_options,
                                 state.reject_unauthorized_string(),
                                 &CryptoContext::Options::reject_unauthorized)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(&options->options_.crypto_options,
                                 tls_options,
                                 state.client_hello_string(),
                                 &CryptoContext::Options::client_hello)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(&options->options_.crypto_options,
                                 tls_options,
                                 state.enable_tls_trace_string(),
                                 &CryptoContext::Options::enable_tls_trace)
                     .IsNothing()) ||
        UNLIKELY(
            options
                ->SetOption(&options->options_.crypto_options,
                            tls_options,
                            state.request_peer_certificate_string(),
                            &CryptoContext::Options::request_peer_certificate)
                .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(&options->options_.crypto_options,
                                 tls_options,
                                 state.ocsp_string(),
                                 &CryptoContext::Options::ocsp)
                     .IsNothing()) ||
        UNLIKELY(
            options
                ->SetOption(&options->options_.crypto_options,
                            tls_options,
                            state.verify_hostname_identity_string(),
                            &CryptoContext::Options::verify_hostname_identity)
                .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(&options->options_.crypto_options,
                                 tls_options,
                                 state.keylog_string(),
                                 &CryptoContext::Options::keylog)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(&options->options_.crypto_options,
                                 tls_options,
                                 state.session_id_string(),
                                 &CryptoContext::Options::session_id_ctx)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(&options->options_.crypto_options,
                                 tls_options,
                                 state.ciphers_string(),
                                 &CryptoContext::Options::ciphers)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(&options->options_.crypto_options,
                                 tls_options,
                                 state.groups_string(),
                                 &CryptoContext::Options::groups)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(&options->options_.crypto_options,
                                 tls_options,
                                 state.keys_string(),
                                 &CryptoContext::Options::keys)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(&options->options_.crypto_options,
                                 tls_options,
                                 state.certs_string(),
                                 &CryptoContext::Options::certs)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(&options->options_.crypto_options,
                                 tls_options,
                                 state.ca_string(),
                                 &CryptoContext::Options::ca)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(&options->options_.crypto_options,
                                 tls_options,
                                 state.crl_string(),
                                 &CryptoContext::Options::crl)
                     .IsNothing())) {
      return;  // Failed!
    }
  }

  if (!args[kApplicationOptions]->IsUndefined()) {
    Local<Object> app_options = args[kApplicationOptions].As<Object>();

    if (UNLIKELY(options
                     ->SetOption(&options->options_.application,
                                 app_options,
                                 state.max_header_pairs_string(),
                                 &Application::Options::max_header_pairs)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(&options->options_.application,
                                 app_options,
                                 state.max_header_length_string(),
                                 &Application::Options::max_header_length)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(&options->options_.application,
                                 app_options,
                                 state.max_field_section_size_string(),
                                 &Application::Options::max_field_section_size)
                     .IsNothing()) ||
        UNLIKELY(
            options
                ->SetOption(&options->options_.application,
                            app_options,
                            state.qpack_max_table_capacity_string(),
                            &Application::Options::qpack_max_dtable_capacity)
                .IsNothing()) ||
        UNLIKELY(
            options
                ->SetOption(
                    &options->options_.application,
                    app_options,
                    state.qpack_encoder_max_dtable_capacity_string(),
                    &Application::Options::qpack_encoder_max_dtable_capacity)
                .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(&options->options_.application,
                                 app_options,
                                 state.qpack_blocked_streams_string(),
                                 &Application::Options::qpack_blocked_streams)
                     .IsNothing())) {
      // Intentionally do not return here.
    }
  }
  // TODO(@jasnell): Skipping this for now
  // if (Http3OptionsObject::HasInstance(env, args[kApplicationOptions])) {
  //   Http3OptionsObject* http3Options;
  //   ASSIGN_OR_RETURN_UNWRAP(&http3Options, args[kApplicationOptions]);
  //   options->options()->application = http3Options->options();
  // }

  if (!args[kTransportParams]->IsUndefined()) {
    // Transport params
    Local<Object> params = args[kTransportParams].As<Object>();

    if (UNLIKELY(options
                     ->SetOption(
                         &options->options_,
                         params,
                         state.initial_max_stream_data_bidi_local_string(),
                         &Session::Options::initial_max_stream_data_bidi_local)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(
                         &options->options_,
                         params,
                         state.initial_max_stream_data_bidi_remote_string(),
                         &Session::Options::initial_max_stream_data_bidi_remote)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(&options->options_,
                                 params,
                                 state.initial_max_stream_data_uni_string(),
                                 &Session::Options::initial_max_stream_data_uni)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(&options->options_,
                                 params,
                                 state.initial_max_data_string(),
                                 &Session::Options::initial_max_data)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(&options->options_,
                                 params,
                                 state.initial_max_streams_bidi_string(),
                                 &Session::Options::initial_max_streams_bidi)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(&options->options_,
                                 params,
                                 state.initial_max_streams_uni_string(),
                                 &Session::Options::initial_max_streams_uni)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(&options->options_,
                                 params,
                                 state.max_idle_timeout_string(),
                                 &Session::Options::max_idle_timeout)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(&options->options_,
                                 params,
                                 state.active_connection_id_limit_string(),
                                 &Session::Options::active_connection_id_limit)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(&options->options_,
                                 params,
                                 state.ack_delay_exponent_string(),
                                 &Session::Options::ack_delay_exponent)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(&options->options_,
                                 params,
                                 state.max_ack_delay_string(),
                                 &Session::Options::max_ack_delay)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(&options->options_,
                                 params,
                                 state.max_datagram_frame_size_string(),
                                 &Session::Options::max_datagram_frame_size)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(&options->options_,
                                 params,
                                 state.disable_active_migration_string(),
                                 &Session::Options::disable_active_migration)
                     .IsNothing())) {
      return;
    }
  }

  if (!args[kIpv4PreferredAddress]->IsUndefined()) {
    SocketAddressBase* preferred_addr;
    ASSIGN_OR_RETURN_UNWRAP(&preferred_addr, args[kIpv4PreferredAddress]);
    CHECK_EQ(preferred_addr->address()->family(), AF_INET);
    options->options_.preferred_address_ipv4 = Just(*preferred_addr->address());
  }

  if (!args[kIpv6PreferredAddress]->IsUndefined()) {
    SocketAddressBase* preferred_addr;
    ASSIGN_OR_RETURN_UNWRAP(&preferred_addr, args[kIpv6PreferredAddress]);
    CHECK_EQ(preferred_addr->address()->family(), AF_INET6);
    options->options_.preferred_address_ipv6 = Just(*preferred_addr->address());
  }
}

void Session::OptionsObject::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("options", options_);
}

// ======================================================================================
// Default Application

namespace {
inline void Consume(ngtcp2_vec** pvec, size_t* pcnt, size_t len) {
  ngtcp2_vec* v = *pvec;
  size_t cnt = *pcnt;

  for (; cnt > 0; --cnt, ++v) {
    if (v->len > len) {
      v->len -= len;
      v->base += len;
      break;
    }
    len -= v->len;
  }

  *pvec = v;
  *pcnt = cnt;
}

inline int IsEmpty(const ngtcp2_vec* vec, size_t cnt) {
  size_t i;
  for (i = 0; i < cnt && vec[i].len == 0; ++i) {
  }
  return i == cnt;
}

template <typename T>
size_t get_length(const T* vec, size_t count) {
  CHECK_NOT_NULL(vec);
  size_t len = 0;
  for (size_t n = 0; n < count; n++) len += vec[n].len;
  return len;
}
}  // namespace

class DefaultApplication final : public Session::Application {
 public:
  DefaultApplication(Session* session,
                     const Session::Config& config,
                     const Application::Options& options);

  QUIC_NO_COPY_OR_MOVE(DefaultApplication)

  bool ReceiveStreamData(Stream* stream,
                         ReceiveStreamDataFlags flags,
                         const uint8_t* data,
                         size_t datalen,
                         uint64_t offset) override;

  int GetStreamData(StreamData* stream_data) override;

  void ResumeStream(stream_id id) override;
  bool ShouldSetFin(const StreamData& stream_data) override;
  bool StreamCommit(StreamData* stream_data, size_t datalen) override;

  SET_SELF_SIZE(DefaultApplication)
  SET_MEMORY_INFO_NAME(DefaultApplication)
  SET_NO_MEMORY_INFO()

 private:
  void ScheduleStream(stream_id id);
  void UnscheduleStream(stream_id id);

  Stream::Queue stream_queue_;
};

DefaultApplication::DefaultApplication(Session* session,
                                       const Session::Config& config,
                                       const Application::Options& options)
    : Session::Application(session, options) {}

void DefaultApplication::ScheduleStream(stream_id id) {
  auto stream = session().FindStream(id);
  if (LIKELY(stream && !stream->is_destroyed())) {
    DEBUG_ARGS(&session(), "Scheduling stream %" PRIi64, id);
    stream->Schedule(&stream_queue_);
  }
}

void DefaultApplication::UnscheduleStream(stream_id id) {
  auto stream = session().FindStream(id);
  if (LIKELY(stream)) {
    DEBUG_ARGS(&session(), "Unscheduling stream %" PRIi64, id);
    stream->Unschedule();
  }
}

void DefaultApplication::ResumeStream(stream_id id) {
  ScheduleStream(id);
}

bool DefaultApplication::ReceiveStreamData(Stream* stream,
                                           ReceiveStreamDataFlags flags,
                                           const uint8_t* data,
                                           size_t datalen,
                                           uint64_t offset) {
  // One potential DOS attack vector is to send a bunch of empty stream frames
  // to commit resources. Check that here. Essentially, we only want to create a
  // new stream if the datalen is greater than 0, otherwise, we ignore the
  // packet. ngtcp2 should be handling this for us, but we handle it just to be
  // safe. We also want to make sure that the stream hasn't been destroyed.
  if (LIKELY(datalen > 0 && !stream->is_destroyed())) {
    DEBUG_ARGS(&session(), "Receive stream %" PRIi64 " data", stream->id());
    stream->ReceiveData(flags, data, datalen, offset);
  }
  return true;
}

int DefaultApplication::GetStreamData(StreamData* stream_data) {
  if (stream_queue_.IsEmpty()) return 0;

  Stream* stream = stream_queue_.PopFront();
  CHECK_NOT_NULL(stream);
  stream_data->stream.reset(stream);
  stream_data->id = stream->id();
  auto next =
      [&](int status, const ngtcp2_vec* data, size_t count, bob::Done done) {
        switch (status) {
          case bob::Status::STATUS_BLOCK:
            // Fall through
          case bob::Status::STATUS_WAIT:
            return;
          case bob::Status::STATUS_EOS:
          case bob::Status::STATUS_END:
            stream_data->fin = 1;
        }

        stream_data->count = count;

        if (count > 0) {
          stream->Schedule(&stream_queue_);
          stream_data->remaining = get_length(data, count);
        } else {
          stream_data->remaining = 0;
        }

        // Not calling done here because we defer committing
        // the data until after we're sure it's written.
      };

  if (LIKELY(!stream->is_eos())) {
    int ret = stream->Pull(std::move(next),
                           bob::Options::OPTIONS_SYNC,
                           stream_data->data,
                           arraysize(stream_data->data),
                           kMaxVectorCount);
    switch (ret) {
      case bob::Status::STATUS_EOS:
      case bob::Status::STATUS_END:
        stream_data->fin = 1;
        break;
    }
  } else {
    stream_data->fin = 1;
  }

  return 0;
}

bool DefaultApplication::StreamCommit(StreamData* stream_data, size_t datalen) {
  CHECK(stream_data->stream);
  stream_data->remaining -= datalen;
  Consume(&stream_data->buf, &stream_data->count, datalen);
  stream_data->stream->Commit(datalen);
  return true;
}

bool DefaultApplication::ShouldSetFin(const StreamData& stream_data) {
  if (!stream_data.stream || !IsEmpty(stream_data.buf, stream_data.count))
    return false;
  return true;
}

// ======================================================================================

std::unique_ptr<Session::Application> Session::SelectApplication(
    const Config& config, const Options& options) {
  if (options.alpn == NGHTTP3_ALPN_H3)
    return std::make_unique<Http3Application>(this, options.application);

  // In the future, we may end up supporting additional QUIC protocols. As they
  // are added, extend the cases here to create and return them.

  return std::make_unique<DefaultApplication>(
      this, config, options.application);
}

}  // namespace quic
}  // namespace node
