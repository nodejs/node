#include "crypto/crypto_common.h"
#include "base_object-inl.h"
#include "crypto/crypto_util.h"
#include "crypto/crypto_x509.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "nbytes.h"
#include "ncrypto.h"
#include "node.h"
#include "node_buffer.h"
#include "node_crypto.h"
#include "node_internals.h"
#include "string_bytes.h"
#include "v8.h"

#include <openssl/ec.h>
#include <openssl/ecdh.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/pkcs12.h>

#include <string>
#include <unordered_map>

namespace node {

using ncrypto::ClearErrorOnReturn;
using ncrypto::ECKeyPointer;
using ncrypto::EVPKeyPointer;
using ncrypto::SSLPointer;
using ncrypto::SSLSessionPointer;
using ncrypto::StackOfX509;
using ncrypto::X509Pointer;
using ncrypto::X509View;
using v8::ArrayBuffer;
using v8::BackingStoreInitializationMode;
using v8::Context;
using v8::EscapableHandleScope;
using v8::Integer;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::Undefined;
using v8::Value;

namespace crypto {

SSLSessionPointer GetTLSSession(const unsigned char* buf, size_t length) {
  return SSLSessionPointer(d2i_SSL_SESSION(nullptr, &buf, length));
}

MaybeLocal<Value> GetValidationErrorReason(Environment* env, int err) {
  auto reason = X509Pointer::ErrorReason(err).value_or("");
  if (reason == "") return Undefined(env->isolate());
  return OneByteString(env->isolate(), reason);
}

MaybeLocal<Value> GetValidationErrorCode(Environment* env, int err) {
  if (err == 0) return Undefined(env->isolate());
  auto error = X509Pointer::ErrorCode(err);
  return OneByteString(env->isolate(), error);
}

MaybeLocal<Value> GetCert(Environment* env, const SSLPointer& ssl) {
  if (auto cert = ssl.getCertificate()) {
    return X509Certificate::toObject(env, cert);
  }
  return Undefined(env->isolate());
}

namespace {
StackOfX509 CloneSSLCerts(X509Pointer&& cert,
                          const STACK_OF(X509)* const ssl_certs) {
  StackOfX509 peer_certs(sk_X509_new(nullptr));
  if (!peer_certs) return StackOfX509();
  if (cert && !sk_X509_push(peer_certs.get(), cert.release()))
    return StackOfX509();
  for (int i = 0; i < sk_X509_num(ssl_certs); i++) {
    X509Pointer cert(X509_dup(sk_X509_value(ssl_certs, i)));
    if (!cert || !sk_X509_push(peer_certs.get(), cert.get()))
      return StackOfX509();
    // `cert` is now managed by the stack.
    cert.release();
  }
  return peer_certs;
}

MaybeLocal<Object> AddIssuerChainToObject(X509Pointer* cert,
                                          Local<Object> object,
                                          StackOfX509&& peer_certs,
                                          Environment* const env) {
  cert->reset(sk_X509_delete(peer_certs.get(), 0));
  for (;;) {
    int i;
    for (i = 0; i < sk_X509_num(peer_certs.get()); i++) {
      X509View ca(sk_X509_value(peer_certs.get(), i));
      if (!cert->view().isIssuedBy(ca)) continue;

      Local<Value> ca_info;
      if (!X509Certificate::toObject(env, ca).ToLocal(&ca_info)) return {};
      CHECK(ca_info->IsObject());
      if (object
              ->Set(env->context(),
                    env->issuercert_string(),
                    ca_info.As<Object>())
              .IsNothing()) {
        return {};
      }
      object = ca_info.As<Object>();

      // NOTE: Intentionally freeing cert that is not used anymore.
      // Delete cert and continue aggregating issuers.
      cert->reset(sk_X509_delete(peer_certs.get(), i));
      break;
    }

    // Issuer not found, break out of the loop.
    if (i == sk_X509_num(peer_certs.get()))
      break;
  }
  return MaybeLocal<Object>(object);
}

MaybeLocal<Object> GetLastIssuedCert(
    X509Pointer* cert,
    const SSLPointer& ssl,
    Local<Object> issuer_chain,
    Environment* const env) {
  Local<Value> ca_info;
  while (!cert->view().isIssuedBy(cert->view())) {
    auto ca = X509Pointer::IssuerFrom(ssl, cert->view());
    if (!ca) break;

    if (!X509Certificate::toObject(env, ca.view()).ToLocal(&ca_info)) return {};

    CHECK(ca_info->IsObject());

    if (issuer_chain
            ->Set(
                env->context(), env->issuercert_string(), ca_info.As<Object>())
            .IsNothing()) {
      return {};
    }
    issuer_chain = ca_info.As<Object>();

    // For self-signed certificates whose keyUsage field does not include
    // keyCertSign, X509_check_issued() will return false. Avoid going into an
    // infinite loop by checking if SSL_CTX_get_issuer() returned the same
    // certificate.
    if (cert->get() == ca.get()) break;

    // Delete previous cert and continue aggregating issuers.
    *cert = std::move(ca);
  }
  return MaybeLocal<Object>(issuer_chain);
}

Local<Value> maybeString(Environment* env,
                         std::optional<std::string_view> value) {
  if (!value.has_value()) return Undefined(env->isolate());
  return OneByteString(env->isolate(), value.value());
}
}  // namespace

MaybeLocal<Object> GetCipherInfo(Environment* env, const SSLPointer& ssl) {
  if (ssl.getCipher() == nullptr) return MaybeLocal<Object>();
  EscapableHandleScope scope(env->isolate());
  Local<Object> info = Object::New(env->isolate());

  if (info->Set(env->context(),
                env->name_string(),
                maybeString(env, ssl.getCipherName()))
          .IsNothing() ||
      info->Set(env->context(),
                env->standard_name_string(),
                maybeString(env, ssl.getCipherStandardName()))
          .IsNothing() ||
      info->Set(env->context(),
                env->version_string(),
                maybeString(env, ssl.getCipherVersion()))
          .IsNothing()) {
    return MaybeLocal<Object>();
  }

  return scope.Escape(info);
}

MaybeLocal<Object> GetEphemeralKey(Environment* env, const SSLPointer& ssl) {
  CHECK(!ssl.isServer());

  EscapableHandleScope scope(env->isolate());
  Local<Object> info = Object::New(env->isolate());
  EVPKeyPointer key = ssl.getPeerTempKey();
  if (!key) return scope.Escape(info);

  Local<Context> context = env->context();

  int kid = key.id();
  switch (kid) {
    case EVP_PKEY_DH:
      if (info->Set(context, env->type_string(), env->dh_string())
              .IsNothing() ||
          info->Set(context,
                    env->size_string(),
                    Integer::New(env->isolate(), key.bits()))
              .IsNothing()) {
        return MaybeLocal<Object>();
      }
      break;
    case EVP_PKEY_EC:
    case EVP_PKEY_X25519:
    case EVP_PKEY_X448:
      {
        const char* curve_name;
        if (kid == EVP_PKEY_EC) {
          int nid = ECKeyPointer::GetGroupName(key);
          curve_name = OBJ_nid2sn(nid);
        } else {
          curve_name = OBJ_nid2sn(kid);
        }
        if (info->Set(context, env->type_string(), env->ecdh_string())
                .IsNothing() ||
            info->Set(context,
                      env->name_string(),
                      OneByteString(env->isolate(), curve_name))
                .IsNothing() ||
            info->Set(context,
                      env->size_string(),
                      Integer::New(env->isolate(), key.bits()))
                .IsNothing()) {
          return MaybeLocal<Object>();
        }
      }
      break;
  }

  return scope.Escape(info);
}

MaybeLocal<Object> ECPointToBuffer(Environment* env,
                                   const EC_GROUP* group,
                                   const EC_POINT* point,
                                   point_conversion_form_t form,
                                   const char** error) {
  size_t len = EC_POINT_point2oct(group, point, form, nullptr, 0, nullptr);
  if (len == 0) {
    if (error != nullptr) *error = "Failed to get public key length";
    return MaybeLocal<Object>();
  }

  auto bs = ArrayBuffer::NewBackingStore(
      env->isolate(), len, BackingStoreInitializationMode::kUninitialized);

  len = EC_POINT_point2oct(group,
                           point,
                           form,
                           reinterpret_cast<unsigned char*>(bs->Data()),
                           bs->ByteLength(),
                           nullptr);
  if (len == 0) {
    if (error != nullptr) *error = "Failed to get public key";
    return MaybeLocal<Object>();
  }

  Local<ArrayBuffer> ab = ArrayBuffer::New(env->isolate(), std::move(bs));
  return Buffer::New(env, ab, 0, ab->ByteLength()).FromMaybe(Local<Object>());
}

MaybeLocal<Value> GetPeerCert(
    Environment* env,
    const SSLPointer& ssl,
    bool abbreviated,
    bool is_server) {
  ClearErrorOnReturn clear_error_on_return;

  // NOTE: This is because of the odd OpenSSL behavior. On client `cert_chain`
  // contains the `peer_certificate`, but on server it doesn't.
  X509Pointer cert(is_server ? SSL_get_peer_certificate(ssl.get()) : nullptr);
  STACK_OF(X509)* ssl_certs = SSL_get_peer_cert_chain(ssl.get());
  if (!cert && (ssl_certs == nullptr || sk_X509_num(ssl_certs) == 0))
    return Undefined(env->isolate());

  // Short result requested.
  if (abbreviated) {
    if (cert) {
      return X509Certificate::toObject(env, cert.view());
    }
    return X509Certificate::toObject(env,
                                     X509View(sk_X509_value(ssl_certs, 0)));
  }

  StackOfX509 peer_certs = CloneSSLCerts(std::move(cert), ssl_certs);
  if (peer_certs == nullptr)
    return Undefined(env->isolate());

  // First and main certificate.
  Local<Value> result;
  X509View first_cert(sk_X509_value(peer_certs.get(), 0));
  CHECK(first_cert);
  if (!X509Certificate::toObject(env, first_cert).ToLocal(&result)) return {};
  CHECK(result->IsObject());

  Local<Object> issuer_chain;
  MaybeLocal<Object> maybe_issuer_chain;

  maybe_issuer_chain = AddIssuerChainToObject(
      &cert, result.As<Object>(), std::move(peer_certs), env);
  if (!maybe_issuer_chain.ToLocal(&issuer_chain)) return {};

  maybe_issuer_chain =
      GetLastIssuedCert(
          &cert,
          ssl,
          issuer_chain,
          env);

  issuer_chain.Clear();
  if (!maybe_issuer_chain.ToLocal(&issuer_chain)) return {};

  // Last certificate should be self-signed.
  if (cert.view().isIssuedBy(cert.view()) &&
      issuer_chain->Set(env->context(), env->issuercert_string(), issuer_chain)
          .IsNothing()) {
    return {};
  }

  return result;
}

}  // namespace crypto
}  // namespace node
