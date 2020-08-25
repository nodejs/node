#include "crypto/crypto_ssl.h"
#include "crypto/crypto_clienthello-inl.h"
#include "crypto/crypto_common.h"
#include "crypto/crypto_util.h"
#include "allocated_buffer-inl.h"
#include "async_wrap-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node_buffer.h"
#include "tls_wrap.h"
#include "v8.h"

namespace node {

using v8::Array;
using v8::ArrayBufferView;
using v8::Boolean;
using v8::Context;
using v8::Exception;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::String;
using v8::Uint32;
using v8::Value;

namespace crypto {
// Just to generate static methods
template void SSLWrap<TLSWrap>::AddMethods(Environment* env,
                                           Local<FunctionTemplate> t);
template void SSLWrap<TLSWrap>::ConfigureSecureContext(SecureContext* sc);
template int SSLWrap<TLSWrap>::SetCACerts(SecureContext* sc);
template void SSLWrap<TLSWrap>::MemoryInfo(MemoryTracker* tracker) const;
template SSL_SESSION* SSLWrap<TLSWrap>::GetSessionCallback(
    SSL* s,
    const unsigned char* key,
    int len,
    int* copy);
template int SSLWrap<TLSWrap>::NewSessionCallback(SSL* s,
                                                  SSL_SESSION* sess);
template void SSLWrap<TLSWrap>::KeylogCallback(const SSL* s,
                                               const char* line);
template void SSLWrap<TLSWrap>::OnClientHello(
    void* arg,
    const ClientHelloParser::ClientHello& hello);
template int SSLWrap<TLSWrap>::TLSExtStatusCallback(SSL* s, void* arg);
template void SSLWrap<TLSWrap>::DestroySSL();
template int SSLWrap<TLSWrap>::SSLCertCallback(SSL* s, void* arg);
template void SSLWrap<TLSWrap>::WaitForCertCb(CertCb cb, void* arg);
template int SSLWrap<TLSWrap>::SelectALPNCallback(
    SSL* s,
    const unsigned char** out,
    unsigned char* outlen,
    const unsigned char* in,
    unsigned int inlen,
    void* arg);

template <class Base>
void SSLWrap<Base>::AddMethods(Environment* env, Local<FunctionTemplate> t) {
  HandleScope scope(env->isolate());

  env->SetProtoMethodNoSideEffect(t, "getPeerCertificate", GetPeerCertificate);
  env->SetProtoMethodNoSideEffect(t, "getCertificate", GetCertificate);
  env->SetProtoMethodNoSideEffect(t, "getFinished", GetFinished);
  env->SetProtoMethodNoSideEffect(t, "getPeerFinished", GetPeerFinished);
  env->SetProtoMethodNoSideEffect(t, "getSession", GetSession);
  env->SetProtoMethod(t, "setSession", SetSession);
  env->SetProtoMethod(t, "loadSession", LoadSession);
  env->SetProtoMethodNoSideEffect(t, "isSessionReused", IsSessionReused);
  env->SetProtoMethodNoSideEffect(t, "verifyError", VerifyError);
  env->SetProtoMethodNoSideEffect(t, "getCipher", GetCipher);
  env->SetProtoMethodNoSideEffect(t, "getSharedSigalgs", GetSharedSigalgs);
  env->SetProtoMethodNoSideEffect(
      t, "exportKeyingMaterial", ExportKeyingMaterial);
  env->SetProtoMethod(t, "endParser", EndParser);
  env->SetProtoMethod(t, "certCbDone", CertCbDone);
  env->SetProtoMethod(t, "renegotiate", Renegotiate);
  env->SetProtoMethodNoSideEffect(t, "getTLSTicket", GetTLSTicket);
  env->SetProtoMethod(t, "newSessionDone", NewSessionDone);
  env->SetProtoMethod(t, "setOCSPResponse", SetOCSPResponse);
  env->SetProtoMethod(t, "requestOCSP", RequestOCSP);
  env->SetProtoMethodNoSideEffect(t, "getEphemeralKeyInfo",
                                  GetEphemeralKeyInfo);
  env->SetProtoMethodNoSideEffect(t, "getProtocol", GetProtocol);

  env->SetProtoMethod(t, "setMaxSendFragment", SetMaxSendFragment);

  env->SetProtoMethodNoSideEffect(t, "getALPNNegotiatedProtocol",
                                  GetALPNNegotiatedProto);
  env->SetProtoMethod(t, "setALPNProtocols", SetALPNProtocols);
}


template <class Base>
void SSLWrap<Base>::ConfigureSecureContext(SecureContext* sc) {
  // OCSP stapling
  SSL_CTX_set_tlsext_status_cb(sc->ctx_.get(), TLSExtStatusCallback);
  SSL_CTX_set_tlsext_status_arg(sc->ctx_.get(), nullptr);
}


template <class Base>
SSL_SESSION* SSLWrap<Base>::GetSessionCallback(SSL* s,
                                               const unsigned char* key,
                                               int len,
                                               int* copy) {
  Base* w = static_cast<Base*>(SSL_get_app_data(s));

  *copy = 0;
  return w->next_sess_.release();
}

template <class Base>
int SSLWrap<Base>::NewSessionCallback(SSL* s, SSL_SESSION* sess) {
  Base* w = static_cast<Base*>(SSL_get_app_data(s));
  Environment* env = w->ssl_env();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  if (!w->session_callbacks_)
    return 0;

  // Check if session is small enough to be stored
  int size = i2d_SSL_SESSION(sess, nullptr);
  if (size > SecureContext::kMaxSessionSize)
    return 0;

  // Serialize session
  Local<Object> session = Buffer::New(env, size).ToLocalChecked();
  unsigned char* session_data = reinterpret_cast<unsigned char*>(
      Buffer::Data(session));
  memset(session_data, 0, size);
  i2d_SSL_SESSION(sess, &session_data);

  unsigned int session_id_length;
  const unsigned char* session_id_data = SSL_SESSION_get_id(sess,
                                                            &session_id_length);
  Local<Object> session_id = Buffer::Copy(
      env,
      reinterpret_cast<const char*>(session_id_data),
      session_id_length).ToLocalChecked();
  Local<Value> argv[] = { session_id, session };
  // On servers, we pause the handshake until callback of 'newSession', which
  // calls NewSessionDoneCb(). On clients, there is no callback to wait for.
  if (w->is_server())
    w->awaiting_new_session_ = true;
  w->MakeCallback(env->onnewsession_string(), arraysize(argv), argv);

  return 0;
}

template <class Base>
void SSLWrap<Base>::KeylogCallback(const SSL* s, const char* line) {
  Base* w = static_cast<Base*>(SSL_get_app_data(s));
  Environment* env = w->ssl_env();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  const size_t size = strlen(line);
  Local<Value> line_bf = Buffer::Copy(env, line, 1 + size).ToLocalChecked();
  char* data = Buffer::Data(line_bf);
  data[size] = '\n';
  w->MakeCallback(env->onkeylog_string(), 1, &line_bf);
}

template <class Base>
void SSLWrap<Base>::OnClientHello(void* arg,
                                  const ClientHelloParser::ClientHello& hello) {
  Base* w = static_cast<Base*>(arg);
  Environment* env = w->ssl_env();
  HandleScope handle_scope(env->isolate());
  Local<Context> context = env->context();
  Context::Scope context_scope(context);

  Local<Object> hello_obj = Object::New(env->isolate());
  Local<Object> buff = Buffer::Copy(
      env,
      reinterpret_cast<const char*>(hello.session_id()),
      hello.session_size()).ToLocalChecked();
  hello_obj->Set(context, env->session_id_string(), buff).Check();
  if (hello.servername() == nullptr) {
    hello_obj->Set(context,
                   env->servername_string(),
                   String::Empty(env->isolate())).Check();
  } else {
    Local<String> servername = OneByteString(env->isolate(),
                                             hello.servername(),
                                             hello.servername_size());
    hello_obj->Set(context, env->servername_string(), servername).Check();
  }
  hello_obj->Set(context,
                 env->tls_ticket_string(),
                 Boolean::New(env->isolate(), hello.has_ticket())).Check();

  Local<Value> argv[] = { hello_obj };
  w->MakeCallback(env->onclienthello_string(), arraysize(argv), argv);
}

template <class Base>
void SSLWrap<Base>::GetPeerCertificate(
    const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  Environment* env = w->ssl_env();

  bool abbreviated = args.Length() < 1 || !args[0]->IsTrue();

  Local<Value> ret;
  if (GetPeerCert(env, w->ssl_, abbreviated, w->is_server()).ToLocal(&ret))
    args.GetReturnValue().Set(ret);
}

template <class Base>
void SSLWrap<Base>::GetCertificate(
    const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  Environment* env = w->ssl_env();

  Local<Value> ret;
  if (GetCert(env, w->ssl_).ToLocal(&ret))
    args.GetReturnValue().Set(ret);
}

template <class Base>
void SSLWrap<Base>::GetFinished(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());

  // We cannot just pass nullptr to SSL_get_finished()
  // because it would further be propagated to memcpy(),
  // where the standard requirements as described in ISO/IEC 9899:2011
  // sections 7.21.2.1, 7.21.1.2, and 7.1.4, would be violated.
  // Thus, we use a dummy byte.
  char dummy[1];
  size_t len = SSL_get_finished(w->ssl_.get(), dummy, sizeof dummy);
  if (len == 0)
    return;

  AllocatedBuffer buf = AllocatedBuffer::AllocateManaged(env, len);
  CHECK_EQ(len, SSL_get_finished(w->ssl_.get(), buf.data(), len));
  args.GetReturnValue().Set(buf.ToBuffer().ToLocalChecked());
}

template <class Base>
void SSLWrap<Base>::GetPeerFinished(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());

  // We cannot just pass nullptr to SSL_get_peer_finished()
  // because it would further be propagated to memcpy(),
  // where the standard requirements as described in ISO/IEC 9899:2011
  // sections 7.21.2.1, 7.21.1.2, and 7.1.4, would be violated.
  // Thus, we use a dummy byte.
  char dummy[1];
  size_t len = SSL_get_peer_finished(w->ssl_.get(), dummy, sizeof dummy);
  if (len == 0)
    return;

  AllocatedBuffer buf = AllocatedBuffer::AllocateManaged(env, len);
  CHECK_EQ(len, SSL_get_peer_finished(w->ssl_.get(), buf.data(), len));
  args.GetReturnValue().Set(buf.ToBuffer().ToLocalChecked());
}

template <class Base>
void SSLWrap<Base>::GetSession(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());

  SSL_SESSION* sess = SSL_get_session(w->ssl_.get());
  if (sess == nullptr)
    return;

  int slen = i2d_SSL_SESSION(sess, nullptr);
  if (slen <= 0)
    return;  // Invalid or malformed session.

  AllocatedBuffer sbuf = AllocatedBuffer::AllocateManaged(env, slen);
  unsigned char* p = reinterpret_cast<unsigned char*>(sbuf.data());
  CHECK_LT(0, i2d_SSL_SESSION(sess, &p));
  args.GetReturnValue().Set(sbuf.ToBuffer().ToLocalChecked());
}

template <class Base>
void SSLWrap<Base>::SetSession(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());

  if (args.Length() < 1)
    return THROW_ERR_MISSING_ARGS(env, "Session argument is mandatory");

  THROW_AND_RETURN_IF_NOT_BUFFER(env, args[0], "Session");

  SSLSessionPointer sess = GetTLSSession(args[0]);
  if (sess == nullptr)
    return;

  if (!SetTLSSession(w->ssl_, sess))
    return env->ThrowError("SSL_set_session error");
}

template <class Base>
void SSLWrap<Base>::LoadSession(const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());

  // TODO(@sam-github) check arg length and types in js, and CHECK in c++
  if (args.Length() >= 1 && Buffer::HasInstance(args[0])) {
    ArrayBufferViewContents<unsigned char> sbuf(args[0]);

    const unsigned char* p = sbuf.data();
    SSL_SESSION* sess = d2i_SSL_SESSION(nullptr, &p, sbuf.length());

    // Setup next session and move hello to the BIO buffer
    w->next_sess_.reset(sess);
  }
}

template <class Base>
void SSLWrap<Base>::IsSessionReused(const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  bool yes = SSL_session_reused(w->ssl_.get());
  args.GetReturnValue().Set(yes);
}

template <class Base>
void SSLWrap<Base>::EndParser(const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  w->hello_parser_.End();
}

template <class Base>
void SSLWrap<Base>::Renegotiate(const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());

  ClearErrorOnReturn clear_error_on_return;

  if (SSL_renegotiate(w->ssl_.get()) != 1) {
    return ThrowCryptoError(w->ssl_env(), ERR_get_error());
  }
}

template <class Base>
void SSLWrap<Base>::GetTLSTicket(const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  Environment* env = w->ssl_env();

  SSL_SESSION* sess = SSL_get_session(w->ssl_.get());
  if (sess == nullptr)
    return;

  const unsigned char* ticket;
  size_t length;
  SSL_SESSION_get0_ticket(sess, &ticket, &length);

  if (ticket == nullptr)
    return;

  Local<Object> buff = Buffer::Copy(
      env, reinterpret_cast<const char*>(ticket), length).ToLocalChecked();

  args.GetReturnValue().Set(buff);
}

template <class Base>
void SSLWrap<Base>::NewSessionDone(const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  w->awaiting_new_session_ = false;
  w->NewSessionDoneCb();
}

template <class Base>
void SSLWrap<Base>::SetOCSPResponse(const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  Environment* env = w->env();

  if (args.Length() < 1)
    return THROW_ERR_MISSING_ARGS(env, "OCSP response argument is mandatory");

  THROW_AND_RETURN_IF_NOT_BUFFER(env, args[0], "OCSP response");

  w->ocsp_response_.Reset(args.GetIsolate(), args[0].As<ArrayBufferView>());
}

template <class Base>
void SSLWrap<Base>::RequestOCSP(const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());

  SSL_set_tlsext_status_type(w->ssl_.get(), TLSEXT_STATUSTYPE_ocsp);
}

template <class Base>
void SSLWrap<Base>::GetEphemeralKeyInfo(
    const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  Environment* env = Environment::GetCurrent(args);

  CHECK(w->ssl_);

  // tmp key is available on only client
  if (w->is_server())
    return args.GetReturnValue().SetNull();

  Local<Object> ret;
  if (GetEphemeralKey(env, w->ssl_).ToLocal(&ret))
    args.GetReturnValue().Set(ret);

  // TODO(@sam-github) semver-major: else return ThrowCryptoError(env,
  // ERR_get_error())
}

template <class Base>
void SSLWrap<Base>::SetMaxSendFragment(
    const FunctionCallbackInfo<Value>& args) {
  CHECK(args.Length() >= 1 && args[0]->IsNumber());

  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());

  int rv = SSL_set_max_send_fragment(
      w->ssl_.get(),
      args[0]->Int32Value(w->ssl_env()->context()).FromJust());
  args.GetReturnValue().Set(rv);
}

template <class Base>
void SSLWrap<Base>::VerifyError(const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());

  // XXX(bnoordhuis) The UNABLE_TO_GET_ISSUER_CERT error when there is no
  // peer certificate is questionable but it's compatible with what was
  // here before.
  long x509_verify_error =  // NOLINT(runtime/int)
      VerifyPeerCertificate(w->ssl_, X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT);

  if (x509_verify_error == X509_V_OK)
    return args.GetReturnValue().SetNull();

  const char* reason = X509_verify_cert_error_string(x509_verify_error);
  const char* code = reason;
  code = X509ErrorCode(x509_verify_error);

  Isolate* isolate = args.GetIsolate();
  Local<String> reason_string = OneByteString(isolate, reason);
  Local<Value> exception_value = Exception::Error(reason_string);
  Local<Object> exception_object =
    exception_value->ToObject(isolate->GetCurrentContext()).ToLocalChecked();
  exception_object->Set(w->env()->context(), w->env()->code_string(),
                        OneByteString(isolate, code)).Check();
  args.GetReturnValue().Set(exception_object);
}

template <class Base>
void SSLWrap<Base>::GetCipher(const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  Environment* env = w->ssl_env();

  const SSL_CIPHER* c = SSL_get_current_cipher(w->ssl_.get());
  if (c == nullptr)
    return;

  Local<Object> ret;
  if (GetCipherInfo(env, w->ssl_).ToLocal(&ret))
    args.GetReturnValue().Set(ret);
}

template <class Base>
void SSLWrap<Base>::GetSharedSigalgs(const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  Environment* env = w->ssl_env();

  SSL* ssl = w->ssl_.get();
  int nsig = SSL_get_shared_sigalgs(ssl, 0, nullptr, nullptr, nullptr, nullptr,
                                    nullptr);
  MaybeStackBuffer<Local<Value>, 16> ret_arr(nsig);

  for (int i = 0; i < nsig; i++) {
    int hash_nid;
    int sign_nid;
    std::string sig_with_md;

    SSL_get_shared_sigalgs(ssl, i, &sign_nid, &hash_nid, nullptr, nullptr,
                           nullptr);

    switch (sign_nid) {
      case EVP_PKEY_RSA:
        sig_with_md = "RSA+";
        break;

      case EVP_PKEY_RSA_PSS:
        sig_with_md = "RSA-PSS+";
        break;

      case EVP_PKEY_DSA:
        sig_with_md = "DSA+";
        break;

      case EVP_PKEY_EC:
        sig_with_md = "ECDSA+";
        break;

      case NID_ED25519:
        sig_with_md = "Ed25519+";
        break;

      case NID_ED448:
        sig_with_md = "Ed448+";
        break;
#ifndef OPENSSL_NO_GOST
      case NID_id_GostR3410_2001:
        sig_with_md = "gost2001+";
        break;

      case NID_id_GostR3410_2012_256:
        sig_with_md = "gost2012_256+";
        break;

      case NID_id_GostR3410_2012_512:
        sig_with_md = "gost2012_512+";
        break;
#endif  // !OPENSSL_NO_GOST
      default:
        const char* sn = OBJ_nid2sn(sign_nid);

        if (sn != nullptr) {
          sig_with_md = std::string(sn) + "+";
        } else {
          sig_with_md = "UNDEF+";
        }
        break;
    }

    const char* sn_hash = OBJ_nid2sn(hash_nid);
    if (sn_hash != nullptr) {
      sig_with_md += std::string(sn_hash);
    } else {
      sig_with_md += "UNDEF";
    }
    ret_arr[i] = OneByteString(env->isolate(), sig_with_md.c_str());
  }

  args.GetReturnValue().Set(
                 Array::New(env->isolate(), ret_arr.out(), ret_arr.length()));
}

template <class Base>
void SSLWrap<Base>::ExportKeyingMaterial(
    const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsString());

  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  Environment* env = w->ssl_env();

  uint32_t olen = args[0].As<Uint32>()->Value();
  node::Utf8Value label(env->isolate(), args[1]);

  AllocatedBuffer out = AllocatedBuffer::AllocateManaged(env, olen);

  ByteSource context;
  bool use_context = !args[2]->IsUndefined();
  if (use_context)
    context = ByteSource::FromBuffer(args[2]);

  if (SSL_export_keying_material(w->ssl_.get(),
                                 reinterpret_cast<unsigned char*>(out.data()),
                                 olen,
                                 *label,
                                 label.length(),
                                 reinterpret_cast<const unsigned char*>(
                                     context.get()),
                                 context.size(),
                                 use_context) != 1) {
    return ThrowCryptoError(env, ERR_get_error(), "SSL_export_keying_material");
  }

  args.GetReturnValue().Set(out.ToBuffer().ToLocalChecked());
}

template <class Base>
void SSLWrap<Base>::GetProtocol(const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());

  const char* tls_version = SSL_get_version(w->ssl_.get());
  args.GetReturnValue().Set(OneByteString(args.GetIsolate(), tls_version));
}

template <class Base>
int SSLWrap<Base>::SelectALPNCallback(SSL* s,
                                      const unsigned char** out,
                                      unsigned char* outlen,
                                      const unsigned char* in,
                                      unsigned int inlen,
                                      void* arg) {
  Base* w = static_cast<Base*>(SSL_get_app_data(s));
  Environment* env = w->env();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Value> alpn_buffer =
      w->object()->GetPrivate(
          env->context(),
          env->alpn_buffer_private_symbol()).ToLocalChecked();
  ArrayBufferViewContents<unsigned char> alpn_protos(alpn_buffer);
  int status = SSL_select_next_proto(const_cast<unsigned char**>(out), outlen,
                                     alpn_protos.data(), alpn_protos.length(),
                                     in, inlen);
  // According to 3.2. Protocol Selection of RFC7301, fatal
  // no_application_protocol alert shall be sent but OpenSSL 1.0.2 does not
  // support it yet. See
  // https://rt.openssl.org/Ticket/Display.html?id=3463&user=guest&pass=guest
  return status == OPENSSL_NPN_NEGOTIATED ? SSL_TLSEXT_ERR_OK
                                          : SSL_TLSEXT_ERR_NOACK;
}

template <class Base>
void SSLWrap<Base>::GetALPNNegotiatedProto(
    const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());

  const unsigned char* alpn_proto;
  unsigned int alpn_proto_len;

  SSL_get0_alpn_selected(w->ssl_.get(), &alpn_proto, &alpn_proto_len);

  Local<Value> result;
  if (alpn_proto_len == 0) {
    result = False(args.GetIsolate());
  } else if (alpn_proto_len == sizeof("h2") - 1 &&
             0 == memcmp(alpn_proto, "h2", sizeof("h2") - 1)) {
    result = w->env()->h2_string();
  } else if (alpn_proto_len == sizeof("http/1.1") - 1 &&
             0 == memcmp(alpn_proto, "http/1.1", sizeof("http/1.1") - 1)) {
    result = w->env()->http_1_1_string();
  } else {
    result = OneByteString(args.GetIsolate(), alpn_proto, alpn_proto_len);
  }

  args.GetReturnValue().Set(result);
}

template <class Base>
void SSLWrap<Base>::SetALPNProtocols(const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  Environment* env = w->env();
  if (args.Length() < 1 || !Buffer::HasInstance(args[0]))
    return env->ThrowTypeError("Must give a Buffer as first argument");

  if (w->is_client()) {
    CHECK(SetALPN(w->ssl_, args[0]));
  } else {
    CHECK(
        w->object()->SetPrivate(
            env->context(),
            env->alpn_buffer_private_symbol(),
            args[0]).FromJust());
    // Server should select ALPN protocol from list of advertised by client
    SSL_CTX_set_alpn_select_cb(SSL_get_SSL_CTX(w->ssl_.get()),
                               SelectALPNCallback,
                               nullptr);
  }
}

template <class Base>
int SSLWrap<Base>::TLSExtStatusCallback(SSL* s, void* arg) {
  Base* w = static_cast<Base*>(SSL_get_app_data(s));
  Environment* env = w->env();
  HandleScope handle_scope(env->isolate());

  if (w->is_client()) {
    // Incoming response
    Local<Value> arg;
    MaybeLocal<Value> ret = GetSSLOCSPResponse(env, s, Null(env->isolate()));
    if (ret.ToLocal(&arg))
      w->MakeCallback(env->onocspresponse_string(), 1, &arg);

    // No async acceptance is possible, so always return 1 to accept the
    // response.  The listener for 'OCSPResponse' event has no control over
    // return value, but it can .destroy() the connection if the response is not
    // acceptable.
    return 1;
  } else {
    // Outgoing response
    if (w->ocsp_response_.IsEmpty())
      return SSL_TLSEXT_ERR_NOACK;

    Local<ArrayBufferView> obj = PersistentToLocal::Default(env->isolate(),
                                                            w->ocsp_response_);
    size_t len = obj->ByteLength();

    // OpenSSL takes control of the pointer after accepting it
    unsigned char* data = MallocOpenSSL<unsigned char>(len);
    obj->CopyContents(data, len);

    if (!SSL_set_tlsext_status_ocsp_resp(s, data, len))
      OPENSSL_free(data);
    w->ocsp_response_.Reset();

    return SSL_TLSEXT_ERR_OK;
  }
}

template <class Base>
void SSLWrap<Base>::WaitForCertCb(CertCb cb, void* arg) {
  cert_cb_ = cb;
  cert_cb_arg_ = arg;
}

template <class Base>
int SSLWrap<Base>::SSLCertCallback(SSL* s, void* arg) {
  Base* w = static_cast<Base*>(SSL_get_app_data(s));

  if (!w->is_server())
    return 1;

  if (!w->is_waiting_cert_cb())
    return 1;

  if (w->cert_cb_running_)
    // Not an error. Suspend handshake with SSL_ERROR_WANT_X509_LOOKUP, and
    // handshake will continue after certcb is done.
    return -1;

  Environment* env = w->env();
  Local<Context> context = env->context();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(context);
  w->cert_cb_running_ = true;

  Local<Object> info = Object::New(env->isolate());

  const char* servername = GetServerName(s);
  if (servername == nullptr) {
    info->Set(context,
              env->servername_string(),
              String::Empty(env->isolate())).Check();
  } else {
    Local<String> str = OneByteString(env->isolate(), servername,
                                      strlen(servername));
    info->Set(context, env->servername_string(), str).Check();
  }

  const bool ocsp = (SSL_get_tlsext_status_type(s) == TLSEXT_STATUSTYPE_ocsp);
  info->Set(context, env->ocsp_request_string(),
            Boolean::New(env->isolate(), ocsp)).Check();

  Local<Value> argv[] = { info };
  w->MakeCallback(env->oncertcb_string(), arraysize(argv), argv);

  if (!w->cert_cb_running_)
    return 1;

  // Performing async action, wait...
  return -1;
}

template <class Base>
void SSLWrap<Base>::CertCbDone(const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  Environment* env = w->env();

  CHECK(w->is_waiting_cert_cb() && w->cert_cb_running_);

  Local<Object> object = w->object();
  Local<Value> ctx = object->Get(env->context(),
                                 env->sni_context_string()).ToLocalChecked();
  Local<FunctionTemplate> cons = env->secure_context_constructor_template();

  if (cons->HasInstance(ctx)) {
    SecureContext* sc = Unwrap<SecureContext>(ctx.As<Object>());
    CHECK_NOT_NULL(sc);
    // Store the SNI context for later use.
    w->sni_context_ = BaseObjectPtr<SecureContext>(sc);

    if (UseSNIContext(w->ssl_, w->sni_context_) && !w->SetCACerts(sc)) {
      // Not clear why sometimes we throw error, and sometimes we call
      // onerror(). Both cause .destroy(), but onerror does a bit more.
      unsigned long err = ERR_get_error();  // NOLINT(runtime/int)
      return ThrowCryptoError(env, err, "CertCbDone");
    }
  } else if (ctx->IsObject()) {
    // Failure: incorrect SNI context object
    Local<Value> err = Exception::TypeError(env->sni_context_err_string());
    w->MakeCallback(env->onerror_string(), 1, &err);
    return;
  }

  CertCb cb;
  void* arg;

  cb = w->cert_cb_;
  arg = w->cert_cb_arg_;

  w->cert_cb_running_ = false;
  w->cert_cb_ = nullptr;
  w->cert_cb_arg_ = nullptr;

  cb(arg);
}

template <class Base>
void SSLWrap<Base>::DestroySSL() {
  if (!ssl_)
    return;

  env_->isolate()->AdjustAmountOfExternalAllocatedMemory(-kExternalSize);
  ssl_.reset();
}

template <class Base>
int SSLWrap<Base>::SetCACerts(SecureContext* sc) {
  int err = SSL_set1_verify_cert_store(ssl_.get(),
                                       SSL_CTX_get_cert_store(sc->ctx_.get()));
  if (err != 1)
    return err;

  STACK_OF(X509_NAME)* list = SSL_dup_CA_list(
      SSL_CTX_get_client_CA_list(sc->ctx_.get()));

  // NOTE: `SSL_set_client_CA_list` takes the ownership of `list`
  SSL_set_client_CA_list(ssl_.get(), list);
  return 1;
}

template <class Base>
void SSLWrap<Base>::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("ocsp_response", ocsp_response_);
  tracker->TrackField("sni_context", sni_context_);
}

}  // namespace crypto
}  // namespace node
