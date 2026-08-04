#include "dtls_session.h"
#include "dtls.h"
#include "dtls_endpoint.h"

#if HAVE_OPENSSL && HAVE_DTLS

#include <aliased_struct-inl.h>
#include <async_wrap-inl.h>
#include <base_object-inl.h>
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
using v8::Value;

namespace dtls {

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
                          DTLS_STAT_INCREMENT(DTLSSessionStats,
                                              retransmit_count);
                          int ret = DTLSv1_handle_timeout(ssl_.get());
                          if (ret < 0) {
                            // Handshake timeout expired.
                            HandleScope hs(this->env()->isolate());
                            Context::Scope cs(this->env()->context());
                            Local<Value> argv[] = {
                                String::NewFromUtf8(this->env()->isolate(),
                                                    "DTLS handshake timeout")
                                    .ToLocalChecked(),
                            };
                            EmitCallback(DTLS_CB_SESSION_ERROR, 1, argv);
                            return;
                          }
                          Cycle();
                        }),
      remote_address_(remote),
      is_server_(is_server),
      state_(env->isolate()),
      stats_(env->isolate()) {
  MakeWeak();
  DTLS_STAT_RECORD_TIMESTAMP(DTLSSessionStats, created_at);
  retransmit_timer_.Unref();

  // Update shared state.
  state_->handshaking = 1;
  state_->open = 0;

  // Store this session in SSL app data for callbacks.
  SSL_set_app_data(ssl_.get(), this);

  // Enable keylog for TLS key export (useful for Wireshark debugging).
  SSL_CTX_set_keylog_callback(SSL_get_SSL_CTX(ssl_.get()), SSLKeylogCallback);

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
    SetProtoMethod(isolate, tmpl, "getALPNProtocol", GetALPNProtocol);
    SetProtoMethod(isolate, tmpl, "exportKeyingMaterial", ExportKeyingMaterial);
    SetProtoMethod(isolate, tmpl, "getSRTPProfile", GetSRTPProfile);
    SetProtoMethod(isolate, tmpl, "getServername", GetServername);

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
  registry->Register(GetALPNProtocol);
  registry->Register(ExportKeyingMaterial);
  registry->Register(GetSRTPProfile);
  registry->Register(GetServername);
}

BaseObjectPtr<DTLSSession> DTLSSession::Create(Environment* env,
                                               DTLSEndpoint* endpoint,
                                               SSL_CTX* ssl_ctx,
                                               const SocketAddress& remote,
                                               bool is_server,
                                               const char* servername,
                                               const char* verify_host,
                                               bool verify_is_ip) {
  // Create the SSL object.
  SSL* ssl_raw = SSL_new(ssl_ctx);
  if (ssl_raw == nullptr) {
    THROW_ERR_CRYPTO_OPERATION_FAILED(env, "SSL_new failed");
    return {};
  }

  ncrypto::SSLPointer ssl(ssl_raw);

  // Create memory BIOs for encrypted data I/O.
  BIO* enc_in = BIO_new(BIO_s_mem());
  BIO* enc_out = BIO_new(BIO_s_mem());
  if (enc_in == nullptr || enc_out == nullptr) {
    BIO_free(enc_in);
    BIO_free(enc_out);
    THROW_ERR_CRYPTO_OPERATION_FAILED(env, "BIO_new failed");
    return {};
  }

  // Make the BIOs non-blocking.
  BIO_set_mem_eof_return(enc_in, -1);
  BIO_set_mem_eof_return(enc_out, -1);

  // Associate BIOs with the SSL object. SSL_set_bio takes ownership.
  SSL_set_bio(ssl.get(), enc_in, enc_out);

  // Set the MTU (since we use SSL_OP_NO_QUERY_MTU).
  SSL_set_mtu(ssl.get(), endpoint->mtu());

  // Set the handshake direction.
  if (is_server) {
    SSL_set_accept_state(ssl.get());
  } else {
    SSL_set_connect_state(ssl.get());

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

  auto session = MakeBaseObject<DTLSSession>(
      env, obj, endpoint, std::move(ssl), enc_in, enc_out, remote, is_server);

  return session;
}

BaseObjectPtr<DTLSSession> DTLSSession::CreateFromSSL(
    Environment* env,
    DTLSEndpoint* endpoint,
    ncrypto::SSLPointer ssl,
    BIO* enc_in,
    BIO* enc_out,
    const SocketAddress& remote) {
  Local<FunctionTemplate> tmpl = GetConstructorTemplate(env);
  Local<Object> obj;
  if (!tmpl->InstanceTemplate()->NewInstance(env->context()).ToLocal(&obj)) {
    return {};
  }

  return MakeBaseObject<DTLSSession>(env,
                                     obj,
                                     endpoint,
                                     std::move(ssl),
                                     enc_in,
                                     enc_out,
                                     remote,
                                     true /* is_server */);
}

void DTLSSession::New(const FunctionCallbackInfo<Value>& args) {
  // Sessions are created internally via DTLSSession::Create,
  // not directly from JS.
  CHECK(args.IsConstructCall());
}

void DTLSSession::Receive(const uint8_t* data, size_t len) {
  if (destroyed_ || closed_) return;

  // Write the encrypted datagram into enc_in_ BIO.
  int written = BIO_write(enc_in_, data, len);
  if (written <= 0) return;

  // Run the state machine.
  Cycle();
}

void DTLSSession::Cycle() {
  if (destroyed_) return;

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

  HandleScope handle_scope(env()->isolate());
  Context::Scope context_scope(env()->context());

  // If handshake is not yet complete, drive it forward.
  if (!handshake_complete_) {
    int ret = SSL_do_handshake(ssl_.get());
    if (ret <= 0) {
      int err = SSL_get_error(ssl_.get(), ret);
      if (err == SSL_ERROR_SSL) {
        unsigned long ossl_err = ERR_get_error();  // NOLINT(runtime/int)
        char err_buf[256];
        ERR_error_string_n(ossl_err, err_buf, sizeof(err_buf));
        // Flush any fatal alert OpenSSL queued for the peer before emitting the
        // error, which tears the session down and detaches the endpoint.
        EncOut();
        Local<Value> argv[] = {
            String::NewFromUtf8(env()->isolate(), err_buf).ToLocalChecked(),
        };
        EmitCallback(DTLS_CB_SESSION_ERROR, 1, argv);
        cycle_depth_--;
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

      Local<Value> argv[] = {
          String::NewFromUtf8(env()->isolate(), SSL_get_version(ssl_.get()))
              .ToLocalChecked(),
      };
      EmitCallback(DTLS_CB_SESSION_HANDSHAKE, 1, argv);
    }
  }

  // Read any decrypted application data.
  ClearOut();
  // Flush any pending encrypted output.
  EncOut();

  UpdateTimer();
  cycle_depth_--;
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
    // Emit the data to JS via callback.
    Local<Value> argv[] = {
        Buffer::Copy(env(), reinterpret_cast<const char*>(buf), read)
            .ToLocalChecked(),
    };
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
      unsigned long ossl_err = ERR_get_error();  // NOLINT(runtime/int)
      char err_buf[256];
      ERR_error_string_n(ossl_err, err_buf, sizeof(err_buf));
      // Flush any fatal alert OpenSSL queued for the peer before emitting the
      // error, which tears the session down and detaches the endpoint.
      EncOut();
      Local<Value> argv[] = {
          String::NewFromUtf8(env()->isolate(), err_buf).ToLocalChecked(),
      };
      EmitCallback(DTLS_CB_SESSION_ERROR, 1, argv);
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

  // Read encrypted data from enc_out_ BIO and send via UDP.
  // Read in a loop since there may be multiple DTLS records.
  uint8_t buf[65536];
  int read;
  while ((read = BIO_read(enc_out_, buf, sizeof(buf))) > 0) {
    ep->SendTo(remote_address_, buf, read);
  }
}

void DTLSSession::UpdateTimer() {
  if (destroyed_) return;

  struct timeval tv;
  if (DTLSv1_get_timeout(ssl_.get(), &tv)) {
    uint64_t timeout_ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    if (timeout_ms == 0) timeout_ms = 1;  // Minimum 1ms.
    retransmit_timer_.Update(timeout_ms);
  } else {
    // No timeout needed (handshake complete or not started).
    retransmit_timer_.Stop();
  }
}

int DTLSSession::Send(const uint8_t* data, size_t len) {
  if (destroyed_ || closed_) return -1;

  if (!handshake_complete_) {
    // Can't send application data before handshake.
    return -1;
  }

  int written = SSL_write(ssl_.get(), data, len);
  if (written > 0) {
    DTLS_STAT_INCREMENT_N(DTLSSessionStats, bytes_sent, written);
    DTLS_STAT_INCREMENT(DTLSSessionStats, messages_sent);
    EncOut();
  }
  return written;
}

void DTLSSession::Close() {
  if (destroyed_ || closed_) return;

  // Emitting the close below can synchronously free this session (a client
  // session that owns its endpoint tears the endpoint -- and thus itself --
  // down from the close callback), and we call Destroy() afterwards. Pin a
  // strong reference so `this` survives until we return.
  BaseObjectPtr<DTLSSession> strong_ref{this};

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

  // Promote to strong ref to keep endpoint alive during removal,
  // then release our weak pointer.
  BaseObjectPtr<DTLSEndpoint> ep = endpoint_;
  endpoint_.reset();
  if (ep) ep->RemoveSession(remote_address_);
}

void DTLSSession::SSLKeylogCallback(const SSL* ssl, const char* line) {
  DTLSSession* session = static_cast<DTLSSession*>(SSL_get_app_data(ssl));
  if (session == nullptr || session->destroyed_) return;

  HandleScope handle_scope(session->env()->isolate());
  Context::Scope context_scope(session->env()->context());

  Local<Value> argv[] = {
      String::NewFromUtf8(session->env()->isolate(), line).ToLocalChecked(),
  };
  session->EmitCallback(DTLS_CB_SESSION_KEYLOG, 1, argv);
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
  args.GetReturnValue().Set(
      String::NewFromUtf8(session->env()->isolate(), version).ToLocalChecked());
}

void DTLSSession::GetCipher(const FunctionCallbackInfo<Value>& args) {
  DTLSSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.This());
  Environment* env = session->env();

  const SSL_CIPHER* cipher = SSL_get_current_cipher(session->ssl_.get());
  if (cipher == nullptr) return;

  Local<Object> info = Object::New(env->isolate());
  info->Set(env->context(),
            FIXED_ONE_BYTE_STRING(env->isolate(), "name"),
            String::NewFromUtf8(env->isolate(), SSL_CIPHER_get_name(cipher))
                .ToLocalChecked())
      .Check();
  info->Set(
          env->context(),
          FIXED_ONE_BYTE_STRING(env->isolate(), "standardName"),
          String::NewFromUtf8(env->isolate(), SSL_CIPHER_standard_name(cipher))
              .ToLocalChecked())
      .Check();
  info->Set(env->context(),
            FIXED_ONE_BYTE_STRING(env->isolate(), "version"),
            String::NewFromUtf8(env->isolate(), SSL_CIPHER_get_version(cipher))
                .ToLocalChecked())
      .Check();

  args.GetReturnValue().Set(info);
}

void DTLSSession::GetPeerCertificate(const FunctionCallbackInfo<Value>& args) {
  DTLSSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.This());
  Environment* env = session->env();

  X509* peer_cert = SSL_get0_peer_certificate(session->ssl_.get());
  if (peer_cert == nullptr) return;

  // Return the PEM-encoded certificate.
  BIO* bio = BIO_new(BIO_s_mem());
  if (PEM_write_bio_X509(bio, peer_cert)) {
    char* data;
    long len = BIO_get_mem_data(bio, &data);  // NOLINT(runtime/int)
    if (len > 0) {
      args.GetReturnValue().Set(
          String::NewFromUtf8(
              env->isolate(), data, v8::NewStringType::kNormal, len)
              .ToLocalChecked());
    }
  }
  BIO_free(bio);
}

void DTLSSession::GetALPNProtocol(const FunctionCallbackInfo<Value>& args) {
  DTLSSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.This());

  const unsigned char* alpn = nullptr;
  unsigned int alpn_len = 0;
  SSL_get0_alpn_selected(session->ssl_.get(), &alpn, &alpn_len);

  if (alpn != nullptr && alpn_len > 0) {
    args.GetReturnValue().Set(
        String::NewFromUtf8(session->env()->isolate(),
                            reinterpret_cast<const char*>(alpn),
                            v8::NewStringType::kNormal,
                            alpn_len)
            .ToLocalChecked());
  }
}

void DTLSSession::ExportKeyingMaterial(
    const FunctionCallbackInfo<Value>& args) {
  DTLSSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.This());
  Environment* env = session->env();

  if (!args[0]->IsNumber() || !args[1]->IsString()) {
    return THROW_ERR_INVALID_ARG_TYPE(
        env, "Expected (length: number, label: string[, context: Buffer])");
  }

  int length = args[0]->Int32Value(env->context()).FromJust();
  Utf8Value label(env->isolate(), args[1]);

  const uint8_t* context_value = nullptr;
  size_t context_len = 0;
  bool use_context = false;

  if (args.Length() > 2 && Buffer::HasInstance(args[2])) {
    context_value = reinterpret_cast<const uint8_t*>(Buffer::Data(args[2]));
    context_len = Buffer::Length(args[2]);
    use_context = true;
  }

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

  args.GetReturnValue().Set(
      Buffer::Copy(env, reinterpret_cast<const char*>(out.data()), length)
          .ToLocalChecked());
}

void DTLSSession::GetSRTPProfile(const FunctionCallbackInfo<Value>& args) {
  DTLSSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.This());

  const SRTP_PROTECTION_PROFILE* profile =
      SSL_get_selected_srtp_profile(session->ssl_.get());

  if (profile != nullptr) {
    args.GetReturnValue().Set(
        String::NewFromUtf8(session->env()->isolate(), profile->name)
            .ToLocalChecked());
  }
}

void DTLSSession::GetServername(const FunctionCallbackInfo<Value>& args) {
  DTLSSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.This());

  const char* servername =
      SSL_get_servername(session->ssl_.get(), TLSEXT_NAMETYPE_host_name);
  if (servername != nullptr) {
    args.GetReturnValue().Set(
        String::NewFromUtf8(session->env()->isolate(), servername)
            .ToLocalChecked());
  }
}

void DTLSSession::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("remote_address", remote_address_);
}

}  // namespace dtls
}  // namespace node

#endif  // HAVE_OPENSSL && HAVE_DTLS
