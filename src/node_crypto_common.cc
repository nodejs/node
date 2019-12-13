#include "env-inl.h"
#include "node_crypto.h"
#include "node_crypto_common.h"
#include "node.h"
#include "node_internals.h"
#include "node_url.h"
#include "string_bytes.h"
#include "v8.h"

#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#include <string>
#include <unordered_map>

namespace node {

using v8::Array;
using v8::Context;
using v8::Integer;
using v8::Local;
using v8::Object;
using v8::Value;

namespace crypto {

void LogSecret(
    SSL* ssl,
    const char* name,
    const unsigned char* secret,
    size_t secretlen) {
  if (auto keylog_cb = SSL_CTX_get_keylog_callback(SSL_get_SSL_CTX(ssl))) {
    unsigned char crandom[32];
    if (SSL_get_client_random(ssl, crandom, 32) != 32)
      return;
    std::string line = name;
    line += " " + StringBytes::hex_encode(
        reinterpret_cast<const char*>(crandom), 32);
    line += " " + StringBytes::hex_encode(
        reinterpret_cast<const char*>(secret), secretlen);
    keylog_cb(ssl, line.c_str());
  }
}

void SetALPN(SSL* ssl, const std::string& alpn) {
  CHECK_EQ(SSL_set_alpn_protos(
      ssl,
      reinterpret_cast<const uint8_t*>(alpn.c_str()),
      alpn.length()), 0);
}

std::string GetSSLOCSPResponse(SSL* ssl) {
  const unsigned char* resp;
  int len = SSL_get_tlsext_status_ocsp_resp(ssl, &resp);
  if (len < 0) len = 0;
  return std::string(reinterpret_cast<const char*>(resp), len);
}

bool SetTLSSession(SSL* ssl, const unsigned char* buf, size_t length) {
  SSLSessionPointer s(d2i_SSL_SESSION(nullptr, &buf, length));
  return s != nullptr && SSL_set_session(ssl, s.get()) == 1;
}

std::unordered_multimap<std::string, std::string>
GetCertificateAltNames(
    X509* cert) {
  std::unordered_multimap<std::string, std::string> map;
  crypto::BIOPointer bio(BIO_new(BIO_s_mem()));
  BUF_MEM* mem;
  int idx = X509_get_ext_by_NID(cert, NID_subject_alt_name, -1);
  if (idx < 0)  // There is no subject alt name
    return map;

  X509_EXTENSION* ext = X509_get_ext(cert, idx);
  CHECK_NOT_NULL(ext);
  const X509V3_EXT_METHOD* method = X509V3_EXT_get(ext);
  CHECK_EQ(method, X509V3_EXT_get_nid(NID_subject_alt_name));

  GENERAL_NAMES* names = static_cast<GENERAL_NAMES*>(X509V3_EXT_d2i(ext));
  if (names == nullptr)  // There are no names
    return map;

  for (int i = 0; i < sk_GENERAL_NAME_num(names); i++) {
    USE(BIO_reset(bio.get()));
    GENERAL_NAME* gen = sk_GENERAL_NAME_value(names, i);
    if (gen->type == GEN_DNS) {
      ASN1_IA5STRING* name = gen->d.dNSName;
      BIO_write(bio.get(), name->data, name->length);
      BIO_get_mem_ptr(bio.get(), &mem);
      map.emplace("dns", std::string(mem->data, mem->length));
    } else {
      STACK_OF(CONF_VALUE)* nval = i2v_GENERAL_NAME(
          const_cast<X509V3_EXT_METHOD*>(method), gen, nullptr);
      if (nval == nullptr)
        continue;
      X509V3_EXT_val_prn(bio.get(), nval, 0, 0);
      sk_CONF_VALUE_pop_free(nval, X509V3_conf_free);
      BIO_get_mem_ptr(bio.get(), &mem);
      std::string value(mem->data, mem->length);
      if (value.compare(0, 11, "IP Address:") == 0) {
        map.emplace("ip", value.substr(11));
      } else if (value.compare(0, 4, "URI:") == 0) {
        url::URL url(value.substr(4));
        if (url.flags() & url::URL_FLAGS_CANNOT_BE_BASE ||
            url.flags() & url::URL_FLAGS_FAILED) {
          continue;  // Skip this one
        }
        map.emplace("uri", url.host());
      }
    }
  }
  sk_GENERAL_NAME_pop_free(names, GENERAL_NAME_free);
  return map;
}

std::string GetCertificateCN(X509* cert) {
  X509_NAME* subject = X509_get_subject_name(cert);
  if (subject != nullptr) {
    int nid = OBJ_txt2nid("CN");
    int idx = X509_NAME_get_index_by_NID(subject, nid, -1);
    if (idx != -1) {
      X509_NAME_ENTRY* cn = X509_NAME_get_entry(subject, idx);
      if (cn != nullptr) {
        ASN1_STRING* cn_str = X509_NAME_ENTRY_get_data(cn);
        if (cn_str != nullptr) {
          return std::string(reinterpret_cast<const char*>(
              ASN1_STRING_get0_data(cn_str)));
        }
      }
    }
  }
  return std::string();
}

int VerifyPeerCertificate(SSL* ssl, int def) {
  int err = def;
  if (X509* peer_cert = SSL_get_peer_certificate(ssl)) {
    X509_free(peer_cert);
    err = SSL_get_verify_result(ssl);
  } else {
    const SSL_CIPHER* curr_cipher = SSL_get_current_cipher(ssl);
    const SSL_SESSION* sess = SSL_get_session(ssl);
    if (SSL_CIPHER_get_auth_nid(curr_cipher) == NID_auth_psk ||
        (SSL_SESSION_get_protocol_version(sess) == TLS1_3_VERSION &&
         SSL_session_reused(ssl))) {
      return X509_V_OK;
    }
  }
  return err;
}

int UseSNIContext(SSL* ssl, SecureContext* context) {
  SSL_CTX* ctx = context->ctx_.get();
  X509* x509 = SSL_CTX_get0_certificate(ctx);
  EVP_PKEY* pkey = SSL_CTX_get0_privatekey(ctx);
  STACK_OF(X509)* chain;

  int err = SSL_CTX_get0_chain_certs(ctx, &chain);
  if (err)
    err = SSL_use_certificate(ssl, x509);
  if (err)
    err = SSL_use_PrivateKey(ssl, pkey);
  if (err && chain != nullptr)
    err = SSL_set1_chain(ssl, chain);
  return err;
}

const char* GetClientHelloALPN(SSL* ssl) {
    const unsigned char* buf;
    size_t len;
    size_t rem;

    if (!SSL_client_hello_get0_ext(
            ssl,
            TLSEXT_TYPE_application_layer_protocol_negotiation,
            &buf, &rem) || rem < 2) {
      return nullptr;
    }

    len = (buf[0] << 8) | buf[1];
    if (len + 2 != rem)
      return nullptr;
    buf += 3;
    return reinterpret_cast<const char*>(buf);
}

const char* GetClientHelloServerName(SSL* ssl) {
  const unsigned char* buf;
  size_t len;
  size_t rem;

  if (!SSL_client_hello_get0_ext(
          ssl,
          TLSEXT_TYPE_server_name,
          &buf,
          &rem) || rem <= 2) {
    return nullptr;
  }

  len = *(buf++) << 8;
  len += *(buf++);
  if (len + 2 != rem)
    return nullptr;
  rem = len;

  if (rem == 0 || *buf++ != TLSEXT_NAMETYPE_host_name)
    return nullptr;
  rem--;
  if (rem <= 2)
    return nullptr;
  len = *(buf++) << 8;
  len += *(buf++);
  if (len + 2 > rem)
    return nullptr;
  rem = len;
  return reinterpret_cast<const char*>(buf);
}

const char* GetServerName(SSL* ssl) {
  return SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
}

Local<Array> GetClientHelloCiphers(Environment* env, SSL* ssl) {
  const unsigned char* buf;
  size_t len = SSL_client_hello_get0_ciphers(ssl, &buf);
  std::vector<Local<Value>> ciphers_array;
  for (size_t n = 0; n < len; n += 2) {
    const SSL_CIPHER* cipher = SSL_CIPHER_find(ssl, buf);
    buf += 2;
    const char* cipher_name = SSL_CIPHER_get_name(cipher);
    const char* cipher_version = SSL_CIPHER_get_version(cipher);
    Local<Object> obj = Object::New(env->isolate());
    obj->Set(
        env->context(),
        env->name_string(),
        OneByteString(env->isolate(), cipher_name)).FromJust();
    obj->Set(
        env->context(),
        env->version_string(),
        OneByteString(env->isolate(), cipher_version)).FromJust();
    ciphers_array.push_back(obj);
  }
  return Array::New(env->isolate(), ciphers_array.data(), ciphers_array.size());
}

bool SetGroups(SecureContext* sc, const char* groups) {
  return SSL_CTX_set1_groups_list(**sc, groups) == 1;
}

const char* X509ErrorCode(int err) {
  const char* code = "UNSPECIFIED";
#define CASE_X509_ERR(CODE) case X509_V_ERR_##CODE: code = #CODE; break;
  switch (err) {
    CASE_X509_ERR(UNABLE_TO_GET_ISSUER_CERT)
    CASE_X509_ERR(UNABLE_TO_GET_CRL)
    CASE_X509_ERR(UNABLE_TO_DECRYPT_CERT_SIGNATURE)
    CASE_X509_ERR(UNABLE_TO_DECRYPT_CRL_SIGNATURE)
    CASE_X509_ERR(UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY)
    CASE_X509_ERR(CERT_SIGNATURE_FAILURE)
    CASE_X509_ERR(CRL_SIGNATURE_FAILURE)
    CASE_X509_ERR(CERT_NOT_YET_VALID)
    CASE_X509_ERR(CERT_HAS_EXPIRED)
    CASE_X509_ERR(CRL_NOT_YET_VALID)
    CASE_X509_ERR(CRL_HAS_EXPIRED)
    CASE_X509_ERR(ERROR_IN_CERT_NOT_BEFORE_FIELD)
    CASE_X509_ERR(ERROR_IN_CERT_NOT_AFTER_FIELD)
    CASE_X509_ERR(ERROR_IN_CRL_LAST_UPDATE_FIELD)
    CASE_X509_ERR(ERROR_IN_CRL_NEXT_UPDATE_FIELD)
    CASE_X509_ERR(OUT_OF_MEM)
    CASE_X509_ERR(DEPTH_ZERO_SELF_SIGNED_CERT)
    CASE_X509_ERR(SELF_SIGNED_CERT_IN_CHAIN)
    CASE_X509_ERR(UNABLE_TO_GET_ISSUER_CERT_LOCALLY)
    CASE_X509_ERR(UNABLE_TO_VERIFY_LEAF_SIGNATURE)
    CASE_X509_ERR(CERT_CHAIN_TOO_LONG)
    CASE_X509_ERR(CERT_REVOKED)
    CASE_X509_ERR(INVALID_CA)
    CASE_X509_ERR(PATH_LENGTH_EXCEEDED)
    CASE_X509_ERR(INVALID_PURPOSE)
    CASE_X509_ERR(CERT_UNTRUSTED)
    CASE_X509_ERR(CERT_REJECTED)
    CASE_X509_ERR(HOSTNAME_MISMATCH)
  }
#undef CASE_X509_ERR
  return code;
}

Local<Value> GetValidationErrorReason(Environment* env, int err) {
  const char* reason = X509_verify_cert_error_string(err);
  return OneByteString(env->isolate(), reason);
}

Local<Value> GetValidationErrorCode(Environment* env, int err) {
  return OneByteString(env->isolate(), X509ErrorCode(err));
}

Local<Value> GetCert(Environment* env, SSL* ssl) {
  ClearErrorOnReturn clear_error_on_return;
  Local<Value> value = v8::Undefined(env->isolate());
  X509* cert = SSL_get_certificate(ssl);
  if (cert != nullptr)
    value = X509ToObject(env, cert);
  return value;
}

Local<Value> GetCipherName(Environment* env, SSL* ssl) {
  Local<Value> cipher;
  const SSL_CIPHER* c = SSL_get_current_cipher(ssl);
  if (c != nullptr) {
    const char* cipher_name = SSL_CIPHER_get_name(c);
    cipher = OneByteString(env->isolate(), cipher_name);
  }
  return cipher;
}

Local<Value> GetCipherVersion(Environment* env, SSL* ssl) {
  Local<Value> version;
  const SSL_CIPHER* c = SSL_get_current_cipher(ssl);
  if (c != nullptr) {
    const char* cipher_version = SSL_CIPHER_get_version(c);
    version = OneByteString(env->isolate(), cipher_version);
  }
  return version;
}

Local<Value> GetEphemeralKey(Environment* env, SSL* ssl) {
  Local<Context> context = env->context();

  Local<Object> info = Object::New(env->isolate());

  EVP_PKEY* raw_key;
  if (SSL_get_server_tmp_key(ssl, &raw_key)) {
    crypto::EVPKeyPointer key(raw_key);
    int kid = EVP_PKEY_id(key.get());
    switch (kid) {
      case EVP_PKEY_DH:
        info->Set(context, env->type_string(),
                  FIXED_ONE_BYTE_STRING(env->isolate(), "DH")).FromJust();
        info->Set(context, env->size_string(),
                  Integer::New(env->isolate(), EVP_PKEY_bits(key.get())))
            .FromJust();
        break;
      case EVP_PKEY_EC:
      case EVP_PKEY_X25519:
      case EVP_PKEY_X448:
        {
          const char* curve_name;
          if (kid == EVP_PKEY_EC) {
            EC_KEY* ec = EVP_PKEY_get1_EC_KEY(key.get());
            int nid = EC_GROUP_get_curve_name(EC_KEY_get0_group(ec));
            curve_name = OBJ_nid2sn(nid);
            EC_KEY_free(ec);
          } else {
            curve_name = OBJ_nid2sn(kid);
          }
          info->Set(context, env->type_string(),
                    FIXED_ONE_BYTE_STRING(
                        env->isolate(),
                        "ECDH")).FromJust();
          info->Set(context, env->name_string(),
                    OneByteString(
                        env->isolate(),
                        curve_name)).FromJust();
          info->Set(context, env->size_string(),
                    Integer::New(
                        env->isolate(),
                        EVP_PKEY_bits(key.get()))).FromJust();
        }
        break;
      default:
        break;
    }
  }
  return info;
}

Local<Value> GetPeerCert(
    Environment* env,
    SSL* ssl,
    bool abbreviated,
    bool is_server) {
  ClearErrorOnReturn clear_error_on_return;

  Local<Value> result = v8::Undefined(env->isolate());
  Local<Object> issuer_chain;

  // NOTE: This is because of the odd OpenSSL behavior. On client `cert_chain`
  // contains the `peer_certificate`, but on server it doesn't.
  X509Pointer cert(is_server ? SSL_get_peer_certificate(ssl) : nullptr);
  STACK_OF(X509)* ssl_certs = SSL_get_peer_cert_chain(ssl);
  if (!cert && (ssl_certs == nullptr || sk_X509_num(ssl_certs) == 0))
    return result;

  // Short result requested.
  if (abbreviated)
    return X509ToObject(env, cert ? cert.get() : sk_X509_value(ssl_certs, 0));

  if (auto peer_certs = CloneSSLCerts(std::move(cert), ssl_certs)) {
    // First and main certificate.
    X509Pointer cert(sk_X509_value(peer_certs.get(), 0));
    CHECK(cert);
    result = X509ToObject(env, cert.release());

    Local<Object> issuer_chain =
        GetLastIssuedCert(
            &cert,
            ssl,
            AddIssuerChainToObject(
                &cert,
                result.As<Object>(),
                std::move(peer_certs),
                env),
            env);
    // Last certificate should be self-signed.
    if (X509_check_issued(cert.get(), cert.get()) == X509_V_OK)
      USE(issuer_chain->Set(
          env->context(),
          env->issuercert_string(),
          issuer_chain));
  }
  return result;
}

}  // namespace crypto
}  // namespace node
