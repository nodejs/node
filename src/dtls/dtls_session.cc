#include "dtls_session.h"
#include "dtls.h"
#include "dtls_endpoint.h"

#if HAVE_OPENSSL && HAVE_DTLS

#include <aliased_struct-inl.h>
#include <async_wrap-inl.h>
#include <base_object-inl.h>
#include <crypto/crypto_x509.h>
#include <env-inl.h>
#include <memory_tracker-inl.h>
#include <node_buffer.h>
#include <node_errors.h>
#include <node_sockaddr-inl.h>
#include <timer_wrap-inl.h>
#include <util-inl.h>

#include <openssl/err.h>
#include <openssl/srtp.h>
#include <openssl/ssl.h>
#include <openssl/x509_vfy.h>
#include <openssl/x509v3.h>

#include <cstring>

namespace node {

using ncrypto::MarkPopErrorOnReturn;
using v8::ArrayBuffer;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::String;
using v8::Uint32;
using v8::Uint8Array;
using v8::Value;

namespace dtls {

// The session state "indices" are byte offsets into DTLSSessionStateData,
// accessed from JS via a DataView. Pin them to the actual struct layout, as
// the endpoint state already does, so adding or reordering a field cannot
// silently point JS at the wrong byte.
static_assert(IDX_SESSION_STATE_HANDSHAKING ==
              offsetof(DTLSSessionStateData, handshaking));
static_assert(IDX_SESSION_STATE_OPEN == offsetof(DTLSSessionStateData, open));
static_assert(IDX_SESSION_STATE_CLOSING ==
              offsetof(DTLSSessionStateData, closing));
static_assert(IDX_SESSION_STATE_DESTROYED ==
              offsetof(DTLSSessionStateData, destroyed));
static_assert(IDX_SESSION_STATE_HAS_MESSAGE_LISTENER ==
              offsetof(DTLSSessionStateData, has_message_listener));
static_assert(IDX_SESSION_STATE_HAS_KEYLOG_LISTENER ==
              offsetof(DTLSSessionStateData, has_keylog_listener));
static_assert(IDX_SESSION_STATE_COUNT == sizeof(DTLSSessionStateData));

namespace {
// Format the OpenSSL error queue into a human readable message.
//
// The most recently queued entry is rendered and the queue is left alone for
// the enclosing MarkPopErrorOnReturn to unwind.
//
// Peeked at the top rather than taken from the bottom. ERR_get_error() pops
// the oldest entry in the whole queue, which is not necessarily one of ours:
// MarkPopErrorOnReturn records a position to unwind to, it does not empty the
// queue on the way in, so anything already there is below the mark and comes
// out first. An SSL_ERROR_SSL means this operation queued at least one entry,
// and the newest is certainly it.
//
// An SSL_ERROR_SSL can also be reported with nothing queued --
// ERR_error_string_n() renders 0 as "error:00000000:lib(0)::reason(0)", which
// tells nobody anything -- so fall back to a description of the SSL error
// code instead.
std::string FormatSSLError(int ssl_err) {
  unsigned long last = ERR_peek_last_error();  // NOLINT(runtime/int)
  if (last != 0) {
    char buf[256];
    ERR_error_string_n(last, buf, sizeof(buf));
    return buf;
  }
  switch (ssl_err) {
    case SSL_ERROR_SYSCALL:
      return "DTLS I/O error";
    case SSL_ERROR_ZERO_RETURN:
      return "DTLS connection closed by peer";
    default:
      return "DTLS protocol error";
  }
}
}  // namespace

DTLSSession::DTLSSession(Environment* env,
                         Local<Object> wrap,
                         DTLSEndpoint* endpoint,
                         ncrypto::SSLPointer ssl,
                         BIO* enc_in,
                         BIO* enc_out,
                         const SocketAddress& remote,
                         bool is_server)
    : AsyncWrap(env, wrap, PROVIDER_DTLS_SESSION),
      endpoint_(endpoint),
      ssl_(std::move(ssl)),
      enc_in_(enc_in),
      enc_out_(enc_out),
      retransmit_timer_(env,
                        [this] {
                          if (destroyed_) return;
                          // Keep ourselves alive across the callback: emitting
                          // an error or running Cycle() below can synchronously
                          // destroy this session, and this timer lives on it.
                          BaseObjectPtr<DTLSSession> strong_ref{this};
                          MarkPopErrorOnReturn mark_pop_error_on_return;

                          // The deadline is checked before retransmitting, so
                          // an expired handshake stops rather than sends once
                          // more.
                          if (HandshakeDeadlineExpired()) {
                            EmitHandshakeTimeout();
                            return;
                          }

                          DTLS_STAT_INCREMENT(DTLSSessionStats,
                                              retransmit_count);
                          int ret = DTLSv1_handle_timeout(ssl_.get());
                          if (ret < 0) {
                            // OpenSSL gave up first, having exhausted its own
                            // retransmit budget.
                            EmitHandshakeTimeout();
                            return;
                          }
                          Cycle();
                        }),
      remote_address_(remote),
      is_server_(is_server),
      state_(env->isolate()),
      stats_(env->isolate()) {
  // Both creation paths pass the endpoint that owns the session. The MTU is
  // read from it unconditionally below, so a null one was never survivable
  // and testing for it here only made it look as though it were.
  CHECK_NOT_NULL(endpoint);
  MakeWeak();
  DTLS_STAT_RECORD_TIMESTAMP(DTLSSessionStats, created_at);

  if (endpoint->handshake_timeout() > 0) {
    handshake_deadline_ =
        uv_hrtime() / 1000000 + endpoint->handshake_timeout();
  }
  retransmit_timer_.Unref();

  // Update shared state.
  state_->handshaking = 1;
  state_->open = 0;

  // Store this session in SSL app data for callbacks.
  SSL_set_app_data(ssl_.get(), this);

  // Set the MTU on the SSL object.
  SSL_set_mtu(ssl_.get(), endpoint->mtu());
}

DTLSSession::~DTLSSession() = default;

Local<FunctionTemplate> DTLSSession::GetConstructorTemplate(Environment* env) {
  auto tmpl = env->dtls_session_constructor_template();
  if (tmpl.IsEmpty()) {
    Isolate* isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, New);
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(isolate, "DTLSSession"));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        AsyncWrap::kInternalFieldCount);

    SetProtoMethod(isolate, tmpl, "send", DoSend);
    SetProtoMethod(isolate, tmpl, "close", DoClose);
    SetProtoMethod(isolate, tmpl, "destroy", DoDestroy);
    SetProtoMethod(isolate, tmpl, "getState", GetState);
    SetProtoMethod(isolate, tmpl, "getStats", GetStats);
    SetProtoMethod(isolate, tmpl, "getRemoteAddress", GetRemoteAddress);
    SetProtoMethod(isolate, tmpl, "getProtocol", GetProtocol);
    SetProtoMethod(isolate, tmpl, "getCipher", GetCipher);
    SetProtoMethod(isolate, tmpl, "getPeerCertificate", GetPeerCertificate);
    SetProtoMethod(
        isolate, tmpl, "getPeerX509Certificate", GetPeerX509Certificate);
    SetProtoMethod(isolate, tmpl, "getALPNProtocol", GetALPNProtocol);
    SetProtoMethod(isolate, tmpl, "exportKeyingMaterial", ExportKeyingMaterial);
    SetProtoMethod(isolate, tmpl, "getSRTPProfile", GetSRTPProfile);
    SetProtoMethod(isolate, tmpl, "getServername", GetServername);
    SetProtoMethod(isolate, tmpl, "getSession", GetSession);
    SetProtoMethod(isolate, tmpl, "wasReused", WasReused);
    SetProtoMethod(isolate, tmpl, "getVerifyError", GetVerifyError);

    env->set_dtls_session_constructor_template(tmpl);
  }
  return tmpl;
}

void DTLSSession::InitPerContext(Local<Object> target,
                                 Local<Context> context,
                                 Environment* env) {
  SetConstructorFunction(
      context, target, "DTLSSession", GetConstructorTemplate(env));
}

void DTLSSession::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(New);
  registry->Register(DoSend);
  registry->Register(DoClose);
  registry->Register(DoDestroy);
  registry->Register(GetState);
  registry->Register(GetStats);
  registry->Register(GetRemoteAddress);
  registry->Register(GetProtocol);
  registry->Register(GetCipher);
  registry->Register(GetPeerCertificate);
  registry->Register(GetPeerX509Certificate);
  registry->Register(GetALPNProtocol);
  registry->Register(ExportKeyingMaterial);
  registry->Register(GetSRTPProfile);
  registry->Register(GetServername);
  registry->Register(GetVerifyError);
  registry->Register(GetSession);
  registry->Register(WasReused);
}

BaseObjectPtr<DTLSSession> DTLSSession::Create(Environment* env,
                                               DTLSEndpoint* endpoint,
                                               DTLSContext* context,
                                               const SocketAddress& remote,
                                               bool is_server,
                                               const char* servername,
                                               const char* verify_host,
                                               bool verify_is_ip,
                                               const ncrypto::Buffer<
                                                   const unsigned char>&
                                                   resume) {
  // Create the SSL object.
  SSL* ssl_raw = SSL_new(context->ssl_ctx());
  if (ssl_raw == nullptr) {
    THROW_ERR_CRYPTO_OPERATION_FAILED(env, "SSL_new failed");
    return {};
  }
  context->BindToSSL(ssl_raw);

  ncrypto::SSLPointer ssl(ssl_raw);

  // Create memory BIOs for encrypted data I/O.
  // Both must preserve datagram boundaries. OpenSSL's DTLS record layer
  // assumes a read returns exactly one datagram (see the isdtls branch of
  // tls_default_read_n()), and its write path emits one BIO_write per record,
  // each sized to fit SSL_set_mtu(). BIO_s_dgram_mem() honours both, already
  // reports "empty" as a retry -- so no BIO_set_mem_eof_return() is needed --
  // and grows on write.
  auto enc_in = ncrypto::BIOPointer::New(BIO_s_dgram_mem());
  auto enc_out = ncrypto::BIOPointer::New(BIO_s_dgram_mem());
  if (!enc_in || !enc_out) {
    THROW_ERR_CRYPTO_OPERATION_FAILED(env, "BIO_new failed");
    return {};
  }

  // Associate BIOs with the SSL object. SSL_set_bio takes ownership.
  BIO* enc_in_raw = enc_in.release();
  BIO* enc_out_raw = enc_out.release();
  SSL_set_bio(ssl.get(), enc_in_raw, enc_out_raw);

  // Set the MTU (since we use SSL_OP_NO_QUERY_MTU).
  SSL_set_mtu(ssl.get(), endpoint->mtu());

  // Set the handshake direction.
  if (is_server) {
    SSL_set_accept_state(ssl.get());
  } else {
    SSL_set_connect_state(ssl.get());

    // Offer a previous session for resumption. Like SNI this has to happen
    // before Cycle() emits the ClientHello, since the session id and ticket
    // ride in it.
    //
    // A session that OpenSSL rejects is not an error: it falls back to a full
    // handshake, which is what an expired or unknown ticket should do. Only a
    // blob that will not parse is worth reporting, and that is the caller
    // handing over something that is not a session at all.
    if (resume.len > 0) {
      const unsigned char* p = resume.data;
      ncrypto::SSLSessionPointer sess(d2i_SSL_SESSION(nullptr, &p, resume.len));
      if (!sess) {
        THROW_ERR_CRYPTO_OPERATION_FAILED(env,
                                          "Failed to parse resumed session");
        return {};
      }
      // Failure here also just means no resumption, so it is not fatal.
      USE(SSL_set_session(ssl.get(), sess.get()));
    }

    // Configure SNI and peer identity verification BEFORE the handshake
    // starts. The caller (DTLSEndpoint::Connect) runs Cycle() immediately
    // after Create() returns, which emits the ClientHello, so anything that
    // must appear in that flight (SNI) has to be set here rather than via a
    // post-construction setter.
    if (servername != nullptr && servername[0] != '\0') {
      if (!SSL_set_tlsext_host_name(ssl.get(), servername)) {
        THROW_ERR_CRYPTO_OPERATION_FAILED(env,
                                          "Failed to set servername (SNI)");
        return {};
      }
    }

    // When identity verification is requested, bind the expected peer name
    // (or IP) into the verification parameters. Combined with the context's
    // SSL_VERIFY_PEER mode this makes a name mismatch fail the handshake,
    // rather than accepting any certificate that merely chains to a trusted
    // CA. A failure to configure it is fatal: proceeding would silently skip
    // the identity check.
    if (verify_host != nullptr && verify_host[0] != '\0') {
      if (verify_is_ip) {
        if (!X509_VERIFY_PARAM_set1_ip_asc(SSL_get0_param(ssl.get()),
                                           verify_host)) {
          THROW_ERR_CRYPTO_OPERATION_FAILED(
              env, "Failed to set peer IP address for verification");
          return {};
        }
      } else {
        SSL_set_hostflags(ssl.get(), X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
        if (!SSL_set1_host(ssl.get(), verify_host)) {
          THROW_ERR_CRYPTO_OPERATION_FAILED(
              env, "Failed to set peer hostname for verification");
          return {};
        }
      }
    }
  }

  // Create the JS wrapper object.
  Local<FunctionTemplate> tmpl = GetConstructorTemplate(env);
  Local<Object> obj;
  if (!tmpl->InstanceTemplate()->NewInstance(env->context()).ToLocal(&obj)) {
    return {};
  }

  auto session = MakeBaseObject<DTLSSession>(env,
                                             obj,
                                             endpoint,
                                             std::move(ssl),
                                             enc_in_raw,
                                             enc_out_raw,
                                             remote,
                                             is_server);
  if (session) {
    // Hold the context. Nothing else does, for a client.
    session->context_ = BaseObjectPtr<DTLSContext>(context);
  }

  return session;
}

BaseObjectPtr<DTLSSession> DTLSSession::CreateFromSSL(
    Environment* env,
    DTLSEndpoint* endpoint,
    DTLSContext* context,
    ncrypto::SSLPointer ssl,
    BIO* enc_in,
    BIO* enc_out,
    const SocketAddress& remote) {
  Local<FunctionTemplate> tmpl = GetConstructorTemplate(env);
  Local<Object> obj;
  if (!tmpl->InstanceTemplate()->NewInstance(env->context()).ToLocal(&obj)) {
    return {};
  }

  auto session = MakeBaseObject<DTLSSession>(env,
                                             obj,
                                             endpoint,
                                             std::move(ssl),
                                             enc_in,
                                             enc_out,
                                             remote,
                                             true /* is_server */);
  if (session) {
    // The SSL was created from this context by the endpoint, before the
    // session existed, so bind it here. Held for the same reason the client
    // path holds it: the SSL references the SSL_CTX, but nothing references
    // the wrapper whose ex_data slot the PSK callbacks resolve through.
    context->BindToSSL(session->ssl_.get());
    session->context_ = BaseObjectPtr<DTLSContext>(context);
  }
  return session;
}

void DTLSSession::New(const FunctionCallbackInfo<Value>& args) {
  // Sessions are created internally via DTLSSession::Create,
  // not directly from JS.
  CHECK(args.IsConstructCall());
}

void DTLSSession::Receive(const uint8_t* data, size_t len) {
  if (destroyed_ || closed_) return;

  MarkPopErrorOnReturn mark_pop_error_on_return;

  // Write the encrypted datagram into enc_in_ BIO.
  int written = BIO_write(enc_in_, data, len);
  if (written <= 0) return;

  // Run the state machine.
  Cycle();
}

void DTLSSession::Cycle() {
  if (destroyed_) return;

  // Everything OpenSSL queues while the pump runs is consumed here (for the
  // error callbacks below) or discarded on the way out. Leaving entries behind
  // would misattribute them to whatever crypto operation runs next on this
  // thread -- including unrelated node:crypto work.
  MarkPopErrorOnReturn mark_pop_error_on_return;

  // Pin a strong reference to ourselves for the duration of the pump. A JS
  // callback dispatched below (message/handshake/error) can synchronously
  // destroy this session, which removes the endpoint's only strong reference
  // and would otherwise free `this` while we are still using ssl_/state_.
  BaseObjectPtr<DTLSSession> strong_ref{this};

  // Prevent infinite recursion.
  if (++cycle_depth_ > 1) {
    cycle_depth_--;
    return;
  }

  CycleInner();
  cycle_depth_--;

  // A callback that ran inside OpenSSL cannot report its own exception, so it
  // parks it and this is the first point where running JavaScript is safe
  // again. CycleInner() reports it itself when the handshake failed, since
  // there it is the more useful error; this covers the case where it did not
  // fail, which is every keylog exception and any PSK or SNI exception the
  // handshake recovered from.
  EmitPendingError();
  EmitSendError();
}

void DTLSSession::CycleInner() {
  HandleScope handle_scope(env()->isolate());
  Context::Scope context_scope(env()->context());

  // If handshake is not yet complete, drive it forward.
  if (!handshake_complete_) {
    int ret = SSL_do_handshake(ssl_.get());
    if (ret <= 0) {
      int err = SSL_get_error(ssl_.get(), ret);
      if (err == SSL_ERROR_SSL) {
        std::string message = FormatSSLError(err);
        // Flush any fatal alert OpenSSL queued for the peer before emitting the
        // error, which tears the session down and detaches the endpoint.
        EncOut();
        // An exception from a callback that ran inside the handshake is the
        // more useful report: OpenSSL only knows the handshake failed, not
        // that user code threw. It is emitted here, at a point where running
        // JavaScript is safe.
        if (!pending_error_.IsEmpty()) {
          Local<Value> argv[] = {pending_error_.Get(env()->isolate())};
          pending_error_.Reset();
          EmitCallback(DTLS_CB_SESSION_ERROR, 1, argv);
          return;
        }
        Local<String> str;
        if (String::NewFromUtf8(env()->isolate(), message.c_str())
                .ToLocal(&str)) {
          Local<Value> argv[] = {str};
          EmitCallback(DTLS_CB_SESSION_ERROR, 1, argv);
        }
        return;
      }
      // SSL_ERROR_WANT_READ/WRITE is normal during handshake.
    }
    // Flush any handshake data produced.
    EncOut();

    // Check if handshake just completed.
    if (SSL_is_init_finished(ssl_.get()) && !handshake_complete_) {
      handshake_complete_ = true;
      state_->handshaking = 0;
      state_->open = 1;
      DTLS_STAT_RECORD_TIMESTAMP(DTLSSessionStats, handshake_completed_at);

      // Skip only the emit on failure: the read below and the cycle_depth_
      // decrement at the end of Cycle() still have to happen.
      Local<String> str;
      if (String::NewFromUtf8(env()->isolate(), SSL_get_version(ssl_.get()))
              .ToLocal(&str)) {
        Local<Value> argv[] = {str};
        EmitCallback(DTLS_CB_SESSION_HANDSHAKE, 1, argv);
      }
    }
  }

  // Read any decrypted application data.
  ClearOut();
  // Flush any pending encrypted output.
  EncOut();

  UpdateTimer();
}

void DTLSSession::ClearOut() {
  if (destroyed_) return;

  // Try to read decrypted application data from OpenSSL. A DTLS record's
  // plaintext is at most 2^14 bytes, so one SSL_read yields at most that much.
  uint8_t buf[16384];
  int read;

  while ((read = SSL_read(ssl_.get(), buf, sizeof(buf))) > 0) {
    DTLS_STAT_INCREMENT_N(DTLSSessionStats, bytes_received, read);
    DTLS_STAT_INCREMENT(DTLSSessionStats, messages_received);

    // Only materialise the payload as a JS Buffer if something is listening.
    // JS sets this flag from the onmessage setter; without the check, every
    // datagram was copied into the heap and dispatched into JS only for the
    // handler to find no listener and drop it. Reading continues either way,
    // so the data is still drained out of OpenSSL rather than accumulating.
    if (!state_->has_message_listener) continue;

    // continue, not return: this loop is what drains OpenSSL, and bailing
    // out of it would leave the remaining records buffered.
    Local<Object> chunk;
    if (!Buffer::Copy(env(), reinterpret_cast<const char*>(buf), read)
             .ToLocal(&chunk)) {
      continue;
    }
    Local<Value> argv[] = {chunk};
    EmitCallback(DTLS_CB_SESSION_MESSAGE, 1, argv);
    // The message handler may have destroyed the session synchronously; stop
    // reading if so (Cycle()'s strong reference keeps `this` itself alive).
    if (destroyed_) return;
  }

  int err = SSL_get_error(ssl_.get(), read);
  switch (err) {
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
      // Normal - need more data or need to flush.
      break;

    case SSL_ERROR_ZERO_RETURN:
      // Peer sent close_notify.
      if (!closed_) {
        closed_ = true;
        state_->closing = 1;
        state_->open = 0;
        // Send our close_notify back.
        SSL_shutdown(ssl_.get());
        EncOut();
        // Detach from the endpoint's session table before notifying JS so an
        // observer of the close sees a consistent session count. Cycle() holds
        // a strong reference for the duration of the pump.
        if (auto ep = endpoint_.get()) ep->RemoveSession(remote_address_);
        Local<Value> argv[] = {};
        EmitCallback(DTLS_CB_SESSION_CLOSE, 0, argv);
        Destroy();
      }
      break;

    case SSL_ERROR_SSL: {
      // SSL error during handshake or data exchange.
      std::string message = FormatSSLError(err);
      // Flush any fatal alert OpenSSL queued for the peer before emitting the
      // error, which tears the session down and detaches the endpoint.
      EncOut();
      Local<String> str;
      if (String::NewFromUtf8(env()->isolate(), message.c_str())
              .ToLocal(&str)) {
        Local<Value> argv[] = {str};
        EmitCallback(DTLS_CB_SESSION_ERROR, 1, argv);
      }
      break;
    }

    default:
      break;
  }
}

void DTLSSession::EncOut() {
  if (destroyed_) return;
  auto ep = endpoint_.get();
  if (ep == nullptr) return;

  // enc_out_ is a datagram BIO, so each BIO_read yields exactly one datagram
  // -- one DTLS record as OpenSSL framed it against the MTU. Loop because a
  // single flight is several records, and send each one separately rather than
  // concatenating them into a datagram that would need IP fragmentation.
  //
  // BIO_pending() on a datagram BIO reports the size of the next datagram, so
  // the buffer can be sized to it exactly. Reading into a smaller one would
  // silently truncate the record, and a fixed 64 KiB frame is a lot of stack
  // for a ~1200 byte datagram in a function Cycle() can re-enter.
  MaybeStackBuffer<uint8_t, 1500> buf;
  for (;;) {
    size_t pending = BIO_pending(enc_out_);
    if (pending == 0) break;
    buf.AllocateSufficientStorage(pending);
    int read = BIO_read(enc_out_, buf.out(), pending);
    if (read <= 0) break;
    int err = ep->SendTo(remote_address_, buf.out(), read);
    if (err != 0) {
      // A record that cannot be sent will not be retransmitted into
      // existence: EMSGSIZE means the MTU is wrong for this path and
      // ENETUNREACH means there is no path. Both used to present as a
      // handshake that went quiet until the timeout. Keep the first, since
      // the rest of the flight will fail the same way.
      if (send_error_ == 0) send_error_ = err;
      break;
    }
  }
}

bool DTLSSession::HandshakeDeadlineExpired() const {
  if (handshake_deadline_ == 0 || handshake_complete_) return false;
  return uv_hrtime() / 1000000 >= handshake_deadline_;
}

void DTLSSession::EmitHandshakeTimeout() {
  HandleScope hs(env()->isolate());
  Context::Scope cs(env()->context());
  Local<String> message;
  if (!String::NewFromUtf8(env()->isolate(), "DTLS handshake timeout")
           .ToLocal(&message)) {
    return;
  }
  Local<Value> argv[] = {message};
  EmitCallback(DTLS_CB_SESSION_ERROR, 1, argv);
}

void DTLSSession::UpdateTimer() {
  if (destroyed_) return;

  struct timeval tv;
  if (DTLSv1_get_timeout(ssl_.get(), &tv)) {
    uint64_t timeout_ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    if (timeout_ms == 0) timeout_ms = 1;  // Minimum 1ms.

    // Wake at the deadline if it falls first. The retransmit schedule doubles
    // -- 1s, 3s, 7s, 15s, 31s -- so without this a 60s limit would not be
    // noticed until 63s, and a shorter one could be out by almost its own
    // length.
    if (handshake_deadline_ > 0 && !handshake_complete_) {
      uint64_t now = uv_hrtime() / 1000000;
      uint64_t remaining =
          handshake_deadline_ > now ? handshake_deadline_ - now : 1;
      if (remaining < timeout_ms) timeout_ms = remaining;
    }

    retransmit_timer_.Update(timeout_ms);
  } else {
    // No timeout needed (handshake complete or not started).
    retransmit_timer_.Stop();
  }
}

// Returns the number of bytes written, or -1 with a JS exception pending.
// Every failure path throws: a bare -1 return is trivially ignored, and
// send() already throws for a destroyed session and for a bad argument type,
// so returning a sentinel from the remaining paths was the inconsistency.
int DTLSSession::Send(const uint8_t* data, size_t len) {
  if (destroyed_ || closed_) {
    THROW_ERR_INVALID_STATE(env(), "Session is closed");
    return -1;
  }

  if (!handshake_complete_) {
    THROW_ERR_INVALID_STATE(
        env(),
        "Cannot send application data before the handshake completes");
    return -1;
  }

  // DTLS carries application data in a single record per datagram and does
  // not fragment it, so anything above the maximum plaintext record length is
  // unsendable. Report the limit rather than letting SSL_write fail opaquely.
  if (len > SSL3_RT_MAX_PLAIN_LENGTH) {
    THROW_ERR_OUT_OF_RANGE(
        env(),
        "data is %zu bytes, which exceeds the %d byte maximum for a single "
        "DTLS record",
        len,
        SSL3_RT_MAX_PLAIN_LENGTH);
    return -1;
  }

  MarkPopErrorOnReturn mark_pop_error_on_return;

  int written = SSL_write(ssl_.get(), data, len);
  if (written <= 0) {
    int err = SSL_get_error(ssl_.get(), written);
    std::string message = FormatSSLError(err);
    THROW_ERR_CRYPTO_OPERATION_FAILED(env(), message.c_str());
    return -1;
  }

  DTLS_STAT_INCREMENT_N(DTLSSessionStats, bytes_sent, written);
  DTLS_STAT_INCREMENT(DTLSSessionStats, messages_sent);
  EncOut();
  return written;
}

void DTLSSession::Close() {
  if (destroyed_ || closed_) return;

  // Emitting the close below can synchronously free this session (a client
  // session that owns its endpoint tears the endpoint -- and thus itself --
  // down from the close callback), and we call Destroy() afterwards. Pin a
  // strong reference so `this` survives until we return.
  BaseObjectPtr<DTLSSession> strong_ref{this};

  MarkPopErrorOnReturn mark_pop_error_on_return;

  closed_ = true;
  state_->closing = 1;
  DTLS_STAT_RECORD_TIMESTAMP(DTLSSessionStats, closing_at);

  // Send close_notify.
  int ret = SSL_shutdown(ssl_.get());
  if (ret == 0) {
    // Need to call again for bidirectional shutdown.
    SSL_shutdown(ssl_.get());
  }
  EncOut();

  retransmit_timer_.Stop();

  state_->open = 0;

  // Detach from the endpoint's session table before notifying JS, so an
  // observer of the close (e.g. one awaiting `closed`) sees a consistent
  // session count. We stay alive via strong_ref, and endpoint_ remains valid
  // for the callback below; the Destroy() that follows clears it.
  if (auto ep = endpoint_.get()) ep->RemoveSession(remote_address_);

  // Notify JS.
  HandleScope handle_scope(env()->isolate());
  Context::Scope context_scope(env()->context());
  Local<Value> argv[] = {};
  EmitCallback(DTLS_CB_SESSION_CLOSE, 0, argv);

  // Release the remaining resources. RemoveSession above already detached us,
  // so the one inside Destroy() is a no-op.
  Destroy();
}

void DTLSSession::Destroy() {
  if (destroyed_) return;
  destroyed_ = true;
  closed_ = true;

  state_->destroyed = 1;
  DTLS_STAT_RECORD_TIMESTAMP(DTLSSessionStats, destroyed_at);
  state_->open = 0;
  state_->handshaking = 0;

  retransmit_timer_.Close();

  // A parked exception is a strong reference to a JS Error, whose stack
  // normally reaches back to this session. Nothing will emit it now.
  pending_error_.Reset();

  // Promote to strong ref to keep endpoint alive during removal,
  // then release our weak pointer.
  BaseObjectPtr<DTLSEndpoint> ep = endpoint_;
  endpoint_.reset();
  if (ep) ep->RemoveSession(remote_address_);
}

void DTLSSession::SSLKeylogCallback(const SSL* ssl, const char* line) {
  DTLSSession* session = static_cast<DTLSSession*>(SSL_get_app_data(ssl));
  if (session == nullptr || session->destroyed_) return;

  // `line` carries the connection's secrets. Do not copy it into the JS heap
  // unless something is actually listening -- once it is a JS string it is
  // reachable from heap snapshots, core dumps and the inspector for as long as
  // the string lives.
  if (!session->state_->has_keylog_listener) return;

  HandleScope handle_scope(session->env()->isolate());
  Context::Scope context_scope(session->env()->context());

  Local<String> str;
  if (!String::NewFromUtf8(session->env()->isolate(), line).ToLocal(&str)) {
    return;
  }
  // OpenSSL calls this from ssl_log_secret while deriving the master secret,
  // so this runs inside SSL_do_handshake(). MakeCallback() would drain the
  // microtask and tick queues here, letting a tick scheduled by the handler
  // re-enter this SSL -- session.close() calls SSL_shutdown() -- part-way
  // through a handshake transition. Call() runs the handler and nothing else.
  v8::TryCatch try_catch(session->env()->isolate());
  Local<Value> argv[] = {str};
  if (session->CallCallback(DTLS_CB_SESSION_KEYLOG, 1, argv).IsEmpty() &&
      try_catch.HasCaught() && !try_catch.HasTerminated()) {
    Local<Value> exception = try_catch.Exception();
    // Must not return into OpenSSL with an exception pending.
    try_catch.Reset();
    session->SetPendingError(exception);
  }
}

void DTLSSession::EmitSendError() {
  if (send_error_ == 0 || destroyed_) return;
  int err = send_error_;
  send_error_ = 0;

  HandleScope handle_scope(env()->isolate());
  Local<String> message;
  if (!String::NewFromUtf8(env()->isolate(), uv_strerror(err))
           .ToLocal(&message)) {
    return;
  }
  Local<Value> argv[] = {message};
  EmitCallback(DTLS_CB_SESSION_ERROR, 1, argv);
}

void DTLSSession::EmitPendingError() {
  if (pending_error_.IsEmpty() || destroyed_) return;
  HandleScope handle_scope(env()->isolate());
  Local<Value> argv[] = {pending_error_.Get(env()->isolate())};
  pending_error_.Reset();
  EmitCallback(DTLS_CB_SESSION_ERROR, 1, argv);
}

MaybeLocal<Value> DTLSSession::CallCallback(int cb_index,
                                            int argc,
                                            Local<Value>* argv) {
  auto ep = endpoint_.get();
  if (ep == nullptr) return MaybeLocal<Value>();
  Local<Function> cb = ep->GetCallback(cb_index);
  if (cb.IsEmpty()) return MaybeLocal<Value>();

  return cb->Call(env()->context(), object(), argc, argv);
}

MaybeLocal<Value> DTLSSession::EmitCallback(int cb_index,
                                            int argc,
                                            Local<Value>* argv) {
  auto ep = endpoint_.get();
  if (ep == nullptr) return MaybeLocal<Value>();
  Local<Function> cb = ep->GetCallback(cb_index);
  if (cb.IsEmpty()) return MaybeLocal<Value>();

  return MakeCallback(cb, argc, argv);
}

// --- JS binding methods ---

void DTLSSession::DoSend(const FunctionCallbackInfo<Value>& args) {
  DTLSSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.This());

  if (!Buffer::HasInstance(args[0])) {
    return THROW_ERR_INVALID_ARG_TYPE(session->env(), "data must be a Buffer");
  }

  const uint8_t* data = reinterpret_cast<const uint8_t*>(Buffer::Data(args[0]));
  size_t len = Buffer::Length(args[0]);

  int written = session->Send(data, len);
  if (written < 0) return;  // Send() has thrown.
  args.GetReturnValue().Set(written);
}

void DTLSSession::DoClose(const FunctionCallbackInfo<Value>& args) {
  DTLSSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.This());
  session->Close();
}

void DTLSSession::DoDestroy(const FunctionCallbackInfo<Value>& args) {
  DTLSSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.This());
  session->Destroy();
}

void DTLSSession::GetState(const FunctionCallbackInfo<Value>& args) {
  DTLSSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.This());
  args.GetReturnValue().Set(session->state_.GetArrayBuffer());
}

void DTLSSession::GetStats(const FunctionCallbackInfo<Value>& args) {
  DTLSSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.This());
  args.GetReturnValue().Set(session->stats_.GetArrayBuffer());
}

void DTLSSession::GetRemoteAddress(const FunctionCallbackInfo<Value>& args) {
  DTLSSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.This());
  Environment* env = session->env();

  Local<Object> obj;
  if (session->remote_address_.ToJS(env).ToLocal(&obj)) {
    args.GetReturnValue().Set(obj);
  }
}

void DTLSSession::GetProtocol(const FunctionCallbackInfo<Value>& args) {
  DTLSSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.This());

  const char* version = SSL_get_version(session->ssl_.get());
  Local<String> str;
  if (!String::NewFromUtf8(session->env()->isolate(), version).ToLocal(&str)) {
    return;
  }
  args.GetReturnValue().Set(str);
}

void DTLSSession::GetCipher(const FunctionCallbackInfo<Value>& args) {
  DTLSSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.This());
  Environment* env = session->env();

  const SSL_CIPHER* cipher = SSL_get_current_cipher(session->ssl_.get());
  if (cipher == nullptr) return;

  // Build the three strings up front so a failure leaves the return value
  // untouched rather than a half-populated object.
  Local<String> name;
  Local<String> standard_name;
  Local<String> version;
  if (!String::NewFromUtf8(env->isolate(), SSL_CIPHER_get_name(cipher))
           .ToLocal(&name) ||
      !String::NewFromUtf8(env->isolate(), SSL_CIPHER_standard_name(cipher))
           .ToLocal(&standard_name) ||
      !String::NewFromUtf8(env->isolate(), SSL_CIPHER_get_version(cipher))
           .ToLocal(&version)) {
    return;
  }

  Local<Object> info = Object::New(env->isolate());
  info->Set(env->context(), env->name_string(), name).Check();
  info->Set(env->context(),
            FIXED_ONE_BYTE_STRING(env->isolate(), "standardName"),
            standard_name)
      .Check();
  info->Set(env->context(), env->version_string(), version).Check();

  args.GetReturnValue().Set(info);
}

void DTLSSession::GetPeerCertificate(const FunctionCallbackInfo<Value>& args) {
  DTLSSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.This());
  Environment* env = session->env();

  MarkPopErrorOnReturn mark_pop_error_on_return;

  X509* peer_cert = SSL_get0_peer_certificate(session->ssl_.get());
  if (peer_cert == nullptr) return;

  // Return the PEM-encoded certificate. This is the leaf only; the chain and
  // the parsed fields node:tls exposes are not available here.
  auto bio = ncrypto::BIOPointer::NewMem();
  if (!bio) return;

  if (PEM_write_bio_X509(bio.get(), peer_cert)) {
    char* data;
    long len = BIO_get_mem_data(bio.get(), &data);  // NOLINT(runtime/int)
    Local<String> str;
    if (len > 0 && String::NewFromUtf8(
                       env->isolate(), data, v8::NewStringType::kNormal, len)
                       .ToLocal(&str)) {
      args.GetReturnValue().Set(str);
    }
  }
}

void DTLSSession::GetPeerX509Certificate(
    const FunctionCallbackInfo<Value>& args) {
  DTLSSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.This());
  Environment* env = session->env();

  MarkPopErrorOnReturn mark_pop_error_on_return;

  // The two sides need different handling: SSL_get_peer_cert_chain() omits
  // the peer's leaf on the server but includes it on the client, and this
  // flag is what tells X509Certificate which it is dealing with. Getting it
  // wrong drops or duplicates the leaf rather than failing outright.
  auto flag = session->is_server_
                  ? crypto::X509Certificate::GetPeerCertificateFlag::SERVER
                  : crypto::X509Certificate::GetPeerCertificateFlag::NONE;

  Local<Object> cert;
  if (crypto::X509Certificate::GetPeerCert(env, session->ssl_, flag)
          .ToLocal(&cert)) {
    args.GetReturnValue().Set(cert);
  }
}

void DTLSSession::GetALPNProtocol(const FunctionCallbackInfo<Value>& args) {
  DTLSSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.This());

  const unsigned char* alpn = nullptr;
  unsigned int alpn_len = 0;
  SSL_get0_alpn_selected(session->ssl_.get(), &alpn, &alpn_len);

  if (alpn == nullptr || alpn_len == 0) return;

  Local<String> str;
  if (!String::NewFromUtf8(session->env()->isolate(),
                           reinterpret_cast<const char*>(alpn),
                           v8::NewStringType::kNormal,
                           alpn_len)
           .ToLocal(&str)) {
    return;
  }
  args.GetReturnValue().Set(str);
}

void DTLSSession::ExportKeyingMaterial(
    const FunctionCallbackInfo<Value>& args) {
  DTLSSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.This());
  Environment* env = session->env();

  // The JS wrapper validates these; a bad value here is a bug on our side, not
  // a user error. Int32Value(...).FromJust() used to accept a negative length
  // straight into std::vector's constructor, which aborted the process.
  CHECK(args[0]->IsUint32());
  CHECK(args[1]->IsString());

  uint32_t length = args[0].As<Uint32>()->Value();
  Utf8Value label(env->isolate(), args[1]);

  const uint8_t* context_value = nullptr;
  size_t context_len = 0;
  bool use_context = false;

  if (args.Length() > 2 && Buffer::HasInstance(args[2])) {
    context_value = reinterpret_cast<const uint8_t*>(Buffer::Data(args[2]));
    context_len = Buffer::Length(args[2]);
    use_context = true;
  }

  MarkPopErrorOnReturn mark_pop_error_on_return;

  std::vector<uint8_t> out(length);
  int ret = SSL_export_keying_material(session->ssl_.get(),
                                       out.data(),
                                       length,
                                       *label,
                                       label.length(),
                                       context_value,
                                       context_len,
                                       use_context ? 1 : 0);

  if (ret != 1) {
    return THROW_ERR_CRYPTO_OPERATION_FAILED(
        env, "SSL_export_keying_material failed");
  }

  Local<Object> buf;
  if (!Buffer::Copy(env, reinterpret_cast<const char*>(out.data()), length)
           .ToLocal(&buf)) {
    return;
  }
  args.GetReturnValue().Set(buf);
}

void DTLSSession::GetSRTPProfile(const FunctionCallbackInfo<Value>& args) {
  DTLSSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.This());

  const SRTP_PROTECTION_PROFILE* profile =
      SSL_get_selected_srtp_profile(session->ssl_.get());

  if (profile == nullptr) return;

  Local<String> str;
  if (!String::NewFromUtf8(session->env()->isolate(), profile->name)
           .ToLocal(&str)) {
    return;
  }
  args.GetReturnValue().Set(str);
}

void DTLSSession::GetVerifyError(const FunctionCallbackInfo<Value>& args) {
  DTLSSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.This());

  MarkPopErrorOnReturn mark_pop_error_on_return;

  // Nothing has been verified before the handshake completes, and asking is
  // not merely meaningless but fatal: with no negotiated cipher,
  // SSL_get_current_cipher() is NULL and SSL_CIPHER_get_auth_nid(), which
  // ncrypto calls to allow for PSK, dereferences it without checking. The
  // session reaches JavaScript before its handshake runs, so this is
  // reachable from the listen() callback.
  if (!session->handshake_complete_) {
    args.GetReturnValue().Set(FIXED_ONE_BYTE_STRING(
        session->env()->isolate(), "HANDSHAKE_INCOMPLETE"));
    return;
  }

  // SSL_get_verify_result() reports X509_V_OK when the peer sent no
  // certificate at all, because there was nothing to find fault with. Route
  // through ncrypto, which reports std::nullopt for that case (allowing for
  // PSK and resumption, where the absence is legitimate) so it can be
  // distinguished from a certificate that actually verified.
  long verify_error =  // NOLINT(runtime/int)
      session->ssl_.verifyPeerCertificate().value_or(
          X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT);

  // undefined means authorized; anything else is the short error code, e.g.
  // "UNABLE_TO_GET_ISSUER_CERT" or "CERT_HAS_EXPIRED".
  if (verify_error == X509_V_OK) return;

  const char* code = ncrypto::X509Pointer::ErrorCode(verify_error);
  Local<String> str;
  if (!String::NewFromUtf8(session->env()->isolate(), code).ToLocal(&str)) {
    return;
  }
  args.GetReturnValue().Set(str);
}

void DTLSSession::GetServername(const FunctionCallbackInfo<Value>& args) {
  DTLSSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.This());

  const char* servername =
      SSL_get_servername(session->ssl_.get(), TLSEXT_NAMETYPE_host_name);
  if (servername == nullptr) return;

  Local<String> str;
  if (!String::NewFromUtf8(session->env()->isolate(), servername)
           .ToLocal(&str)) {
    return;
  }
  args.GetReturnValue().Set(str);
}

void DTLSSession::GetSession(const FunctionCallbackInfo<Value>& args) {
  DTLSSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.This());
  Environment* env = session->env();

  if (!session->ssl_) return;

  // SSL_get1_session() takes a reference, which the pointer releases.
  ncrypto::SSLSessionPointer sess(SSL_get1_session(session->ssl_.get()));
  if (!sess) return;

  int size = i2d_SSL_SESSION(sess.get(), nullptr);
  if (size <= 0) return;

  auto store = ArrayBuffer::NewBackingStore(env->isolate(), size);
  unsigned char* p = static_cast<unsigned char*>(store->Data());
  if (i2d_SSL_SESSION(sess.get(), &p) <= 0) return;

  Local<ArrayBuffer> buffer =
      ArrayBuffer::New(env->isolate(), std::move(store));
  args.GetReturnValue().Set(Uint8Array::New(buffer, 0, size));
}

void DTLSSession::WasReused(const FunctionCallbackInfo<Value>& args) {
  DTLSSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.This());
  if (!session->ssl_) return;
  args.GetReturnValue().Set(SSL_session_reused(session->ssl_.get()) == 1);
}

void DTLSSession::SetSNIContext(DTLSContext* context) {
  sni_context_ = BaseObjectPtr<DTLSContext>(context);
}

void DTLSSession::SetPendingError(Local<Value> error) {
  pending_error_.Reset(env()->isolate(), error);
}

void DTLSSession::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("pending_error", pending_error_);
  tracker->TrackField("remote_address", remote_address_);
  tracker->TrackField("context", context_);
  tracker->TrackField("sni_context", sni_context_);
}

}  // namespace dtls
}  // namespace node

#endif  // HAVE_OPENSSL && HAVE_DTLS
