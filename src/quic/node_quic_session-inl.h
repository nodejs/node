#ifndef SRC_QUIC_NODE_QUIC_SESSION_INL_H_
#define SRC_QUIC_NODE_QUIC_SESSION_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "debug_utils-inl.h"
#include "node_crypto.h"
#include "node_crypto_common.h"
#include "node_quic_crypto.h"
#include "node_quic_session.h"
#include "node_quic_socket-inl.h"
#include "node_quic_stream-inl.h"

#include <openssl/ssl.h>
#include <memory>
#include <string>

namespace node {

namespace quic {

void QuicSessionConfig::GenerateStatelessResetToken(
    QuicSession* session,
    const QuicCID& cid) {
  transport_params.stateless_reset_token_present = 1;
  StatelessResetToken token(
    transport_params.stateless_reset_token,
    session->socket()->session_reset_secret(),
    cid);
  Debug(session, "Generated stateless reset token %s for CID %s", token, cid);
}

void QuicSessionConfig::GeneratePreferredAddressToken(
    ConnectionIDStrategy connection_id_strategy,
    QuicSession* session,
    QuicCID* pscid) {
  connection_id_strategy(session, pscid->cid(), kScidLen);
  transport_params.preferred_address.cid = **pscid;

  StatelessResetToken(
    transport_params.preferred_address.stateless_reset_token,
    session->socket()->session_reset_secret(),
    *pscid);
}

void QuicSessionConfig::set_original_connection_id(
    const QuicCID& ocid,
    const QuicCID& scid) {
  if (ocid) {
    transport_params.original_dcid = *ocid;
    transport_params.retry_scid = *scid;
    transport_params.retry_scid_present = 1;
  }
}

void QuicSessionConfig::set_qlog(const ngtcp2_qlog_settings& qlog_) {
  qlog = qlog_;
}

QuicCryptoContext::QuicCryptoContext(
    QuicSession* session,
    BaseObjectPtr<crypto::SecureContext> secure_context,
    ngtcp2_crypto_side side,
    uint32_t options) :
    session_(session),
    secure_context_(secure_context),
    side_(side),
    options_(options) {
  ssl_.reset(SSL_new(secure_context_->ctx_.get()));
  CHECK(ssl_);
}

void QuicCryptoContext::Initialize() {
  InitializeTLS(session(), ssl_);
}

// Cancels and frees any remaining outbound handshake data
// at each crypto level.
uint64_t QuicCryptoContext::Cancel() {
  uint64_t len = handshake_[0].Cancel();
  len += handshake_[1].Cancel();
  len += handshake_[2].Cancel();
  return len;
}

v8::MaybeLocal<v8::Value> QuicCryptoContext::ocsp_response() const {
  Environment* env = session()->env();
  return crypto::GetSSLOCSPResponse(
      env,
      ssl_.get(),
      v8::Undefined(env->isolate()));
}

ngtcp2_crypto_level QuicCryptoContext::read_crypto_level() const {
  return from_ossl_level(SSL_quic_read_level(ssl_.get()));
}

ngtcp2_crypto_level QuicCryptoContext::write_crypto_level() const {
  return from_ossl_level(SSL_quic_write_level(ssl_.get()));
}

// TLS Keylogging is enabled per-QuicSession by attaching an handler to the
// "keylog" event. Each keylog line is emitted to JavaScript where it can
// be routed to whatever destination makes sense. Typically, this will be
// to a keylog file that can be consumed by tools like Wireshark to intercept
// and decrypt QUIC network traffic.
void QuicCryptoContext::Keylog(const char* line) {
  if (UNLIKELY(session_->state_->keylog_enabled))
    session_->listener()->OnKeylog(line, strlen(line));
}

// Following a pause in the handshake for OCSP or client hello, we kickstart
// the handshake again here by triggering ngtcp2 to serialize data.
void QuicCryptoContext::ResumeHandshake() {
  // We haven't received any actual new handshake data but calling
  // this will trigger the handshake to continue.
  Receive(read_crypto_level(), 0, nullptr, 0);
  session_->SendPendingData();
}

// For 0RTT, this sets the TLS session data from the given buffer.
bool QuicCryptoContext::set_session(crypto::SSLSessionPointer session) {
  if (side_ == NGTCP2_CRYPTO_SIDE_CLIENT && session != nullptr) {
    set_early_data(
        SSL_SESSION_get_max_early_data(session.get()) == 0xffffffffUL);
  }
  return crypto::SetTLSSession(ssl_, std::move(session));
}

v8::MaybeLocal<v8::Array> QuicCryptoContext::hello_ciphers() const {
  return crypto::GetClientHelloCiphers(session()->env(), ssl_);
}

v8::MaybeLocal<v8::Value> QuicCryptoContext::cipher_name() const {
  return crypto::GetCipherName(session()->env(), ssl_);
}

v8::MaybeLocal<v8::Value> QuicCryptoContext::cipher_version() const {
  return crypto::GetCipherVersion(session()->env(), ssl_);
}

const char* QuicCryptoContext::servername() const {
  return crypto::GetServerName(ssl_.get());
}

const char* QuicCryptoContext::hello_alpn() const {
  return crypto::GetClientHelloALPN(ssl_);
}

const char* QuicCryptoContext::hello_servername() const {
  return crypto::GetClientHelloServerName(ssl_);
}

v8::MaybeLocal<v8::Object> QuicCryptoContext::ephemeral_key() const {
  return crypto::GetEphemeralKey(session()->env(), ssl_);
}

v8::MaybeLocal<v8::Value> QuicCryptoContext::peer_cert(bool abbreviated) const {
  return crypto::GetPeerCert(
      session()->env(),
      ssl_,
      abbreviated,
      session()->is_server());
}

v8::MaybeLocal<v8::Value> QuicCryptoContext::cert() const {
  return crypto::GetCert(session()->env(), ssl_);
}

std::string QuicCryptoContext::selected_alpn() const {
  const unsigned char* alpn_buf = nullptr;
  unsigned int alpnlen;
  SSL_get0_alpn_selected(ssl_.get(), &alpn_buf, &alpnlen);
  return alpnlen ?
      std::string(reinterpret_cast<const char*>(alpn_buf), alpnlen) :
      std::string();
}

bool QuicCryptoContext::early_data() const {
  return
      (is_early_data() &&
       SSL_get_early_data_status(ssl_.get()) == SSL_EARLY_DATA_ACCEPTED) ||
      SSL_get_max_early_data(ssl_.get()) == 0xffffffffUL;
}

void QuicCryptoContext::set_tls_alert(int err) {
  Debug(session(), "TLS Alert [%d]: %s", err, SSL_alert_type_string_long(err));
  session_->set_last_error(QuicError(QUIC_ERROR_CRYPTO, err));
}

QuicApplication::QuicApplication(QuicSession* session) : session_(session) {}

void QuicApplication::set_stream_fin(int64_t stream_id) {
  BaseObjectPtr<QuicStream> stream = session()->FindStream(stream_id);
  CHECK(stream);
  stream->set_fin_sent();
}

std::unique_ptr<QuicPacket> QuicApplication::CreateStreamDataPacket() {
  return QuicPacket::Create(
      "stream data",
      session()->max_packet_length());
}

Environment* QuicApplication::env() const {
  return session()->env();
}

// Extends the stream-level flow control by the given number of bytes.
void QuicSession::ExtendStreamOffset(int64_t stream_id, size_t amount) {
  Debug(this, "Extending max stream %" PRId64 " offset by %" PRId64 " bytes",
        stream_id, amount);
  ngtcp2_conn_extend_max_stream_offset(
      connection(),
      stream_id,
      amount);
}

// Extends the connection-level flow control for the entire session by
// the given number of bytes.
void QuicSession::ExtendOffset(size_t amount) {
  Debug(this, "Extending session offset by %" PRId64 " bytes", amount);
  ngtcp2_conn_extend_max_offset(connection(), amount);
}

// Copies the local transport params into the given struct for serialization.
void QuicSession::GetLocalTransportParams(ngtcp2_transport_params* params) {
  CHECK(!is_destroyed());
  ngtcp2_conn_get_local_transport_params(connection(), params);
}

// Gets the QUIC version negotiated for this QuicSession
uint32_t QuicSession::negotiated_version() const {
  CHECK(!is_destroyed());
  return ngtcp2_conn_get_negotiated_version(connection());
}

bool QuicSession::is_handshake_completed() const {
  DCHECK(!is_destroyed());
  return ngtcp2_conn_get_handshake_completed(connection());
}

void QuicSession::ShutdownStream(int64_t stream_id, uint64_t code) {
  if (is_in_closing_period() ||
      is_in_draining_period() ||
      is_silent_closing()) {
    return;  // Nothing to do because we can't send any frames.
  }
  SendSessionScope send_scope(this);
  ngtcp2_conn_shutdown_stream(connection(), stream_id, 0);
}

// When a QuicSession hits the idle timeout, it is to be silently and
// immediately closed without attempting to send any additional data to
// the peer. All existing streams are abandoned and closed.
void QuicSession::OnIdleTimeout() {
  if (!is_destroyed()) {
    if (state_->idle_timeout == 1) {
      Debug(this, "Idle timeout");
      Close(QuicSessionListener::SESSION_CLOSE_FLAG_SILENT);
      return;
    }
    state_->idle_timeout = 1;
    UpdateClosingTimer();
  }
}

QuicCID QuicSession::dcid() const {
  return QuicCID(ngtcp2_conn_get_dcid(connection()));
}

void QuicSession::CheckAllocatedSize(size_t previous_size) const {
  CHECK_GE(current_ngtcp2_memory_, previous_size);
}

void QuicSession::IncreaseAllocatedSize(size_t size) {
  current_ngtcp2_memory_ += size;
}

void QuicSession::DecreaseAllocatedSize(size_t size) {
  current_ngtcp2_memory_ -= size;
}

uint64_t QuicSession::max_data_left() const {
  return ngtcp2_conn_get_max_data_left(connection());
}

uint64_t QuicSession::max_local_streams_uni() const {
  return ngtcp2_conn_get_max_local_streams_uni(connection());
}

void QuicSession::set_last_error(QuicError error) {
  last_error_ = error;
}

void QuicSession::set_last_error(int32_t family, uint64_t code) {
  set_last_error({ family, code });
}

void QuicSession::set_last_error(int32_t family, int code) {
  set_last_error({ family, code });
}

bool QuicSession::is_in_closing_period() const {
  return ngtcp2_conn_is_in_closing_period(connection());
}

bool QuicSession::is_in_draining_period() const {
  return ngtcp2_conn_is_in_draining_period(connection());
}

bool QuicSession::HasStream(int64_t id) const {
  return streams_.find(id) != std::end(streams_);
}

bool QuicSession::allow_early_data() const {
  // TODO(@jasnell): For now, we always allow early data.
  // Later there will be reasons we do not want to allow
  // it, such as lack of available system resources.
  return true;
}

void QuicSession::SetSessionTicketAppData(
    const SessionTicketAppData& app_data) {
  application_->SetSessionTicketAppData(app_data);
}

SessionTicketAppData::Status QuicSession::GetSessionTicketAppData(
    const SessionTicketAppData& app_data,
    SessionTicketAppData::Flag flag) {
  return application_->GetSessionTicketAppData(app_data, flag);
}

bool QuicSession::is_server() const {
  return crypto_context_->side() == NGTCP2_CRYPTO_SIDE_SERVER;
}

void QuicSession::StartGracefulClose() {
  set_graceful_closing();
  RecordTimestamp(&QuicSessionStats::closing_at);
}

// The connection ID Strategy is a function that generates
// connection ID values. By default these are generated randomly.
void QuicSession::set_connection_id_strategy(ConnectionIDStrategy strategy) {
  CHECK_NOT_NULL(strategy);
  connection_id_strategy_ = strategy;
}

bool QuicSession::is_unable_to_send_packets() {
  return NgCallbackScope::InNgCallbackScope(this) ||
      is_destroyed() ||
      is_in_draining_period() ||
      (is_server() && is_in_closing_period()) ||
      socket() == nullptr;
}

void QuicSession::set_preferred_address_strategy(
    PreferredAddressStrategy strategy) {
  preferred_address_strategy_ = strategy;
}

QuicSocket* QuicSession::socket() const {
  return socket_.get();
}

// Indicates that the stream is blocked from transmitting any
// data. The specific handling of this is application specific.
// By default, we keep track of statistics but leave it up to
// the application to perform specific handling.
void QuicSession::StreamDataBlocked(int64_t stream_id) {
  IncrementStat(&QuicSessionStats::block_count);
  listener_->OnStreamBlocked(stream_id);
}

// When a server advertises a preferred address in its initial
// transport parameters, ngtcp2 on the client side will trigger
// the OnSelectPreferredAdddress callback which will call this.
// The paddr argument contains the advertised preferred address.
// If the new address is going to be used, it needs to be copied
// over to dest, otherwise dest is left alone. There are two
// possible strategies that we currently support via user
// configuration: use the preferred address or ignore it.
void QuicSession::SelectPreferredAddress(
    const PreferredAddress& preferred_address) {
  CHECK(!is_server());
  preferred_address_strategy_(this, preferred_address);
}

// This variant of SendPacket is used by QuicApplication
// instances to transmit a packet and update the network
// path used at the same time.
bool QuicSession::SendPacket(
  std::unique_ptr<QuicPacket> packet,
  const ngtcp2_path_storage& path) {
  UpdateEndpoint(path.path);
  return SendPacket(std::move(packet));
}

// Set the transport parameters received from the remote peer
void QuicSession::set_remote_transport_params() {
  DCHECK(!is_destroyed());
  ngtcp2_conn_get_remote_transport_params(connection(), &transport_params_);
  set_transport_params_set();
}

// Submits information headers only if the selected application
// supports headers.
bool QuicSession::SubmitInformation(
    int64_t stream_id,
    v8::Local<v8::Array> headers) {
  return application_->SubmitInformation(stream_id, headers);
}

// Submits initial headers only if the selected application
// supports headers. For http3, for instance, this is the
// method used to submit both request and response headers.
bool QuicSession::SubmitHeaders(
    int64_t stream_id,
    v8::Local<v8::Array> headers,
    uint32_t flags) {
  return application_->SubmitHeaders(stream_id, headers, flags);
}

// Submits trailing headers only if the selected application
// supports headers.
bool QuicSession::SubmitTrailers(
    int64_t stream_id,
    v8::Local<v8::Array> headers) {
  return application_->SubmitTrailers(stream_id, headers);
}

// Submits a new push stream
BaseObjectPtr<QuicStream> QuicSession::SubmitPush(
    int64_t stream_id,
    v8::Local<v8::Array> headers) {
  return application_->SubmitPush(stream_id, headers);
}

}  // namespace quic
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_QUIC_NODE_QUIC_SESSION_INL_H_
