#include "allocated_buffer-inl.h"
#include "base_object-inl.h"
#include "env-inl.h"
#include "node_buffer.h"
#include "node_crypto.h"
#include "crypto/crypto_common.h"
#include "node.h"
#include "node_internals.h"
#include "node_url.h"
#include "string_bytes.h"
#include "memory_tracker-inl.h"
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

using v8::Array;
using v8::ArrayBufferView;
using v8::Context;
using v8::EscapableHandleScope;
using v8::Integer;
using v8::Local;
using v8::MaybeLocal;
using v8::NewStringType;
using v8::Object;
using v8::String;
using v8::Undefined;
using v8::Value;

namespace crypto {
static constexpr int X509_NAME_FLAGS =
    ASN1_STRFLGS_ESC_CTRL |
    ASN1_STRFLGS_UTF8_CONVERT |
    XN_FLAG_SEP_MULTILINE |
    XN_FLAG_FN_SN;

int SSL_CTX_get_issuer(SSL_CTX* ctx, X509* cert, X509** issuer) {
  X509_STORE* store = SSL_CTX_get_cert_store(ctx);
  DeleteFnPtr<X509_STORE_CTX, X509_STORE_CTX_free> store_ctx(
      X509_STORE_CTX_new());
  return store_ctx.get() != nullptr &&
         X509_STORE_CTX_init(store_ctx.get(), store, nullptr, nullptr) == 1 &&
         X509_STORE_CTX_get1_issuer(issuer, store_ctx.get(), cert) == 1;
}

void LogSecret(
    const SSLPointer& ssl,
    const char* name,
    const unsigned char* secret,
    size_t secretlen) {
  auto keylog_cb = SSL_CTX_get_keylog_callback(SSL_get_SSL_CTX(ssl.get()));
  unsigned char crandom[32];

  if (keylog_cb == nullptr ||
      SSL_get_client_random(ssl.get(), crandom, 32) != 32) {
    return;
  }

  std::string line = name;
  line += " " + StringBytes::hex_encode(
      reinterpret_cast<const char*>(crandom), 32);
  line += " " + StringBytes::hex_encode(
      reinterpret_cast<const char*>(secret), secretlen);
  keylog_cb(ssl.get(), line.c_str());
}

bool SetALPN(const SSLPointer& ssl, const std::string& alpn) {
  return SSL_set_alpn_protos(
      ssl.get(),
      reinterpret_cast<const uint8_t*>(alpn.c_str()),
      alpn.length()) == 0;
}

bool SetALPN(const SSLPointer& ssl, Local<Value> alpn) {
  if (!alpn->IsArrayBufferView())
    return false;
  ArrayBufferViewContents<unsigned char> protos(alpn.As<ArrayBufferView>());
  return SSL_set_alpn_protos(ssl.get(), protos.data(), protos.length()) == 0;
}

MaybeLocal<Value> GetSSLOCSPResponse(
    Environment* env,
    SSL* ssl,
    Local<Value> default_value) {
  const unsigned char* resp;
  int len = SSL_get_tlsext_status_ocsp_resp(ssl, &resp);
  if (resp == nullptr)
    return default_value;

  Local<Value> ret;
  MaybeLocal<Object> maybe_buffer =
      Buffer::Copy(env, reinterpret_cast<const char*>(resp), len);

  if (!maybe_buffer.ToLocal(&ret))
    return MaybeLocal<Value>();

  return ret;
}

bool SetTLSSession(
    const SSLPointer& ssl,
    const unsigned char* buf,
    size_t length) {
  SSLSessionPointer s(d2i_SSL_SESSION(nullptr, &buf, length));
  return s == nullptr ? false : SetTLSSession(ssl, s);
}

bool SetTLSSession(
    const SSLPointer& ssl,
    const SSLSessionPointer& session) {
  return session != nullptr && SSL_set_session(ssl.get(), session.get()) == 1;
}

SSLSessionPointer GetTLSSession(Local<Value> val) {
  if (!val->IsArrayBufferView())
    return SSLSessionPointer();
  ArrayBufferViewContents<unsigned char> sbuf(val.As<ArrayBufferView>());
  return GetTLSSession(sbuf.data(), sbuf.length());
}

SSLSessionPointer GetTLSSession(const unsigned char* buf, size_t length) {
  return SSLSessionPointer(d2i_SSL_SESSION(nullptr, &buf, length));
}

std::unordered_multimap<std::string, std::string>
GetCertificateAltNames(X509* cert) {
  std::unordered_multimap<std::string, std::string> map;
  BIOPointer bio(BIO_new(BIO_s_mem()));
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

long VerifyPeerCertificate(  // NOLINT(runtime/int)
    const SSLPointer& ssl,
    long def) {  // NOLINT(runtime/int)
  long err = def;  // NOLINT(runtime/int)
  if (X509* peer_cert = SSL_get_peer_certificate(ssl.get())) {
    X509_free(peer_cert);
    err = SSL_get_verify_result(ssl.get());
  } else {
    const SSL_CIPHER* curr_cipher = SSL_get_current_cipher(ssl.get());
    const SSL_SESSION* sess = SSL_get_session(ssl.get());
    // Allow no-cert for PSK authentication in TLS1.2 and lower.
    // In TLS1.3 check that session was reused because TLS1.3 PSK
    // looks like session resumption.
    if (SSL_CIPHER_get_auth_nid(curr_cipher) == NID_auth_psk ||
        (SSL_SESSION_get_protocol_version(sess) == TLS1_3_VERSION &&
         SSL_session_reused(ssl.get()))) {
      return X509_V_OK;
    }
  }
  return err;
}

int UseSNIContext(const SSLPointer& ssl, BaseObjectPtr<SecureContext> context) {
  SSL_CTX* ctx = context->ctx_.get();
  X509* x509 = SSL_CTX_get0_certificate(ctx);
  EVP_PKEY* pkey = SSL_CTX_get0_privatekey(ctx);
  STACK_OF(X509)* chain;

  int err = SSL_CTX_get0_chain_certs(ctx, &chain);
  if (err == 1) err = SSL_use_certificate(ssl.get(), x509);
  if (err == 1) err = SSL_use_PrivateKey(ssl.get(), pkey);
  if (err == 1 && chain != nullptr) err = SSL_set1_chain(ssl.get(), chain);
  return err;
}

const char* GetClientHelloALPN(const SSLPointer& ssl) {
  const unsigned char* buf;
  size_t len;
  size_t rem;

  if (!SSL_client_hello_get0_ext(
          ssl.get(),
          TLSEXT_TYPE_application_layer_protocol_negotiation,
          &buf,
          &rem) ||
      rem < 2) {
    return nullptr;
  }

  len = (buf[0] << 8) | buf[1];
  if (len + 2 != rem) return nullptr;
  return reinterpret_cast<const char*>(buf + 3);
}

const char* GetClientHelloServerName(const SSLPointer& ssl) {
  const unsigned char* buf;
  size_t len;
  size_t rem;

  if (!SSL_client_hello_get0_ext(
          ssl.get(),
          TLSEXT_TYPE_server_name,
          &buf,
          &rem) || rem <= 2) {
    return nullptr;
  }

  len = (*buf << 8) | *(buf + 1);
  if (len + 2 != rem)
    return nullptr;
  rem = len;

  if (rem == 0 || *(buf + 2) != TLSEXT_NAMETYPE_host_name) return nullptr;
  rem--;
  if (rem <= 2)
    return nullptr;
  len = (*(buf + 3) << 8) | *(buf + 4);
  if (len + 2 > rem)
    return nullptr;
  return reinterpret_cast<const char*>(buf + 5);
}

const char* GetServerName(SSL* ssl) {
  return SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
}

bool SetGroups(SecureContext* sc, const char* groups) {
  return SSL_CTX_set1_groups_list(**sc, groups) == 1;
}

const char* X509ErrorCode(long err) {  // NOLINT(runtime/int)
  const char* code = "UNSPECIFIED";
#define CASE_X509_ERR(CODE) case X509_V_ERR_##CODE: code = #CODE; break;
  switch (err) {
    // if you modify anything in here, *please* update the respective section in
    // doc/api/tls.md as well
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

MaybeLocal<Value> GetValidationErrorReason(Environment* env, int err) {
  if (err == 0)
    return Undefined(env->isolate());
  const char* reason = X509_verify_cert_error_string(err);
  return OneByteString(env->isolate(), reason);
}

MaybeLocal<Value> GetValidationErrorCode(Environment* env, int err) {
  if (err == 0)
    return Undefined(env->isolate());
  return OneByteString(env->isolate(), X509ErrorCode(err));
}

MaybeLocal<Value> GetCert(Environment* env, const SSLPointer& ssl) {
  ClearErrorOnReturn clear_error_on_return;
  X509* cert = SSL_get_certificate(ssl.get());
  if (cert == nullptr)
    return Undefined(env->isolate());

  MaybeLocal<Object> maybe_cert = X509ToObject(env, cert);
  return maybe_cert.FromMaybe<Value>(Local<Value>());
}

Local<Value> ToV8Value(Environment* env, const BIOPointer& bio) {
  BUF_MEM* mem;
  BIO_get_mem_ptr(bio.get(), &mem);
  MaybeLocal<String> ret =
      String::NewFromUtf8(
          env->isolate(),
          mem->data,
          NewStringType::kNormal,
          mem->length);
  USE(BIO_reset(bio.get()));
  return ret.FromMaybe(Local<Value>());
}

namespace {
template <typename T>
bool Set(
    Local<Context> context,
    Local<Object> target,
    Local<Value> name,
    MaybeLocal<T> maybe_value) {
  Local<Value> value;
  if (!maybe_value.ToLocal(&value))
    return false;

  // Undefined is ignored, but still considered successful
  if (value->IsUndefined())
    return true;

  return !target->Set(context, name, value).IsNothing();
}

MaybeLocal<Value> GetCipherValue(Environment* env,
    const SSL_CIPHER* cipher,
    const char* (*getstr)(const SSL_CIPHER* cipher)) {
  if (cipher == nullptr)
    return Undefined(env->isolate());

  return OneByteString(env->isolate(), getstr(cipher));
}

MaybeLocal<Value> GetCipherName(Environment* env, const SSL_CIPHER* cipher) {
  return GetCipherValue(env, cipher, SSL_CIPHER_get_name);
}

MaybeLocal<Value> GetCipherStandardName(
    Environment* env,
    const SSL_CIPHER* cipher) {
  return GetCipherValue(env, cipher, SSL_CIPHER_standard_name);
}

MaybeLocal<Value> GetCipherVersion(Environment* env, const SSL_CIPHER* cipher) {
  return GetCipherValue(env, cipher, SSL_CIPHER_get_version);
}

StackOfX509 CloneSSLCerts(X509Pointer&& cert,
                          const STACK_OF(X509)* const ssl_certs) {
  StackOfX509 peer_certs(sk_X509_new(nullptr));
  if (cert)
    sk_X509_push(peer_certs.get(), cert.release());
  for (int i = 0; i < sk_X509_num(ssl_certs); i++) {
    X509Pointer cert(X509_dup(sk_X509_value(ssl_certs, i)));
    if (!cert || !sk_X509_push(peer_certs.get(), cert.get()))
      return StackOfX509();
    // `cert` is now managed by the stack.
    cert.release();
  }
  return peer_certs;
}

MaybeLocal<Object> AddIssuerChainToObject(
    X509Pointer* cert,
    Local<Object> object,
    StackOfX509&& peer_certs,
    Environment* const env) {
  Local<Context> context = env->isolate()->GetCurrentContext();
  cert->reset(sk_X509_delete(peer_certs.get(), 0));
  for (;;) {
    int i;
    for (i = 0; i < sk_X509_num(peer_certs.get()); i++) {
      X509* ca = sk_X509_value(peer_certs.get(), i);
      if (X509_check_issued(ca, cert->get()) != X509_V_OK)
        continue;

      Local<Object> ca_info;
      MaybeLocal<Object> maybe_ca_info = X509ToObject(env, ca);
      if (!maybe_ca_info.ToLocal(&ca_info))
        return MaybeLocal<Object>();

      if (!Set<Object>(context, object, env->issuercert_string(), ca_info))
        return MaybeLocal<Object>();
      object = ca_info;

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
  Local<Context> context = env->isolate()->GetCurrentContext();
  while (X509_check_issued(cert->get(), cert->get()) != X509_V_OK) {
    X509* ca;
    if (SSL_CTX_get_issuer(SSL_get_SSL_CTX(ssl.get()), cert->get(), &ca) <= 0)
      break;

    Local<Object> ca_info;
    MaybeLocal<Object> maybe_ca_info = X509ToObject(env, ca);
    if (!maybe_ca_info.ToLocal(&ca_info))
      return MaybeLocal<Object>();

    if (!Set<Object>(context, issuer_chain, env->issuercert_string(), ca_info))
      return MaybeLocal<Object>();
    issuer_chain = ca_info;

    // Take the value of cert->get() before the call to cert->reset()
    // in order to compare it to ca after and provide a way to exit this loop
    // in case it gets stuck.
    X509* value_before_reset = cert->get();

    // Delete previous cert and continue aggregating issuers.
    cert->reset(ca);

    if (value_before_reset == ca)
      break;
  }
  return MaybeLocal<Object>(issuer_chain);
}

void AddFingerprintDigest(
    const unsigned char* md,
    unsigned int md_size,
    char (*fingerprint)[3 * EVP_MAX_MD_SIZE + 1]) {
  unsigned int i;
  const char hex[] = "0123456789ABCDEF";

  for (i = 0; i < md_size; i++) {
    (*fingerprint)[3*i] = hex[(md[i] & 0xf0) >> 4];
    (*fingerprint)[(3*i)+1] = hex[(md[i] & 0x0f)];
    (*fingerprint)[(3*i)+2] = ':';
  }

  if (md_size > 0) {
    (*fingerprint)[(3*(md_size-1))+2] = '\0';
  } else {
    (*fingerprint)[0] = '\0';
  }
}

MaybeLocal<Value> GetCurveASN1Name(Environment* env, const int nid) {
  const char* nist = OBJ_nid2sn(nid);
  return nist != nullptr ?
      MaybeLocal<Value>(OneByteString(env->isolate(), nist)) :
      MaybeLocal<Value>(Undefined(env->isolate()));
}

MaybeLocal<Value> GetCurveNistName(Environment* env, const int nid) {
  const char* nist = EC_curve_nid2nist(nid);
  return nist != nullptr ?
      MaybeLocal<Value>(OneByteString(env->isolate(), nist)) :
      MaybeLocal<Value>(Undefined(env->isolate()));
}

MaybeLocal<Value> GetECPubKey(
    Environment* env,
    const EC_GROUP* group,
    const ECPointer& ec) {
  const EC_POINT* pubkey = EC_KEY_get0_public_key(ec.get());
  if (pubkey == nullptr)
    return Undefined(env->isolate());

  return ECPointToBuffer(
      env,
      group,
      pubkey,
      EC_KEY_get_conv_form(ec.get()),
      nullptr).FromMaybe(Local<Object>());
}

MaybeLocal<Value> GetECGroup(
    Environment* env,
    const EC_GROUP* group,
    const ECPointer& ec) {
  if (group == nullptr)
    return Undefined(env->isolate());

  int bits = EC_GROUP_order_bits(group);
  if (bits <= 0)
    return Undefined(env->isolate());

  return Integer::New(env->isolate(), bits);
}

MaybeLocal<Object> GetPubKey(Environment* env, const RSAPointer& rsa) {
  int size = i2d_RSA_PUBKEY(rsa.get(), nullptr);
  CHECK_GE(size, 0);

  AllocatedBuffer buffer = AllocatedBuffer::AllocateManaged(env, size);
  unsigned char* serialized =
      reinterpret_cast<unsigned char*>(buffer.data());
  i2d_RSA_PUBKEY(rsa.get(), &serialized);
  return buffer.ToBuffer();
}

MaybeLocal<Value> GetExponentString(
    Environment* env,
    const BIOPointer& bio,
    const BIGNUM* e) {
  uint64_t exponent_word = static_cast<uint64_t>(BN_get_word(e));
  uint32_t lo = static_cast<uint32_t>(exponent_word);
  uint32_t hi = static_cast<uint32_t>(exponent_word >> 32);
  if (hi == 0)
    BIO_printf(bio.get(), "0x%x", lo);
  else
    BIO_printf(bio.get(), "0x%x%08x", hi, lo);

  return ToV8Value(env, bio);
}

Local<Value> GetBits(Environment* env, const BIGNUM* n) {
  return Integer::New(env->isolate(), BN_num_bits(n));
}

MaybeLocal<Value> GetModulusString(
    Environment* env,
    const BIOPointer& bio,
    const BIGNUM* n) {
  BN_print(bio.get(), n);
  return ToV8Value(env, bio);
}
}  // namespace

MaybeLocal<Object> GetRawDERCertificate(Environment* env, X509* cert) {
  int size = i2d_X509(cert, nullptr);

  AllocatedBuffer buffer = AllocatedBuffer::AllocateManaged(env, size);
  unsigned char* serialized =
      reinterpret_cast<unsigned char*>(buffer.data());
  i2d_X509(cert, &serialized);
  return buffer.ToBuffer();
}

MaybeLocal<Value> GetSerialNumber(Environment* env, X509* cert) {
  if (ASN1_INTEGER* serial_number = X509_get_serialNumber(cert)) {
    BignumPointer bn(ASN1_INTEGER_to_BN(serial_number, nullptr));
    if (bn) {
      char* data = BN_bn2hex(bn.get());
      ByteSource buf = ByteSource::Allocated(data, strlen(data));
      if (buf)
        return OneByteString(env->isolate(), buf.get());
    }
  }

  return Undefined(env->isolate());
}

MaybeLocal<Value> GetKeyUsage(Environment* env, X509* cert) {
  StackOfASN1 eku(static_cast<STACK_OF(ASN1_OBJECT)*>(
      X509_get_ext_d2i(cert, NID_ext_key_usage, nullptr, nullptr)));
  if (eku) {
    const int count = sk_ASN1_OBJECT_num(eku.get());
    MaybeStackBuffer<Local<Value>, 16> ext_key_usage(count);
    char buf[256];

    int j = 0;
    for (int i = 0; i < count; i++) {
      if (OBJ_obj2txt(buf,
                      sizeof(buf),
                      sk_ASN1_OBJECT_value(eku.get(), i), 1) >= 0) {
        ext_key_usage[j++] = OneByteString(env->isolate(), buf);
      }
    }

    return Array::New(env->isolate(), ext_key_usage.out(), count);
  }

  return Undefined(env->isolate());
}

MaybeLocal<Value> GetFingerprintDigest(
    Environment* env,
    const EVP_MD* method,
    X509* cert) {
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int md_size;
  char fingerprint[EVP_MAX_MD_SIZE * 3 + 1];

  if (X509_digest(cert, method, md, &md_size)) {
    AddFingerprintDigest(md, md_size, &fingerprint);
    return OneByteString(env->isolate(), fingerprint);
  }
  return Undefined(env->isolate());
}

MaybeLocal<Value> GetValidTo(
    Environment* env,
    X509* cert,
    const BIOPointer& bio) {
  ASN1_TIME_print(bio.get(), X509_get0_notAfter(cert));
  return ToV8Value(env, bio);
}

MaybeLocal<Value> GetValidFrom(
    Environment* env,
    X509* cert,
    const BIOPointer& bio) {
  ASN1_TIME_print(bio.get(), X509_get0_notBefore(cert));
  return ToV8Value(env, bio);
}

bool SafeX509ExtPrint(const BIOPointer& out, X509_EXTENSION* ext) {
  const X509V3_EXT_METHOD* method = X509V3_EXT_get(ext);

  if (method != X509V3_EXT_get_nid(NID_subject_alt_name))
    return false;

  GENERAL_NAMES* names = static_cast<GENERAL_NAMES*>(X509V3_EXT_d2i(ext));
  if (names == nullptr)
    return false;

  for (int i = 0; i < sk_GENERAL_NAME_num(names); i++) {
    GENERAL_NAME* gen = sk_GENERAL_NAME_value(names, i);

    if (i != 0)
      BIO_write(out.get(), ", ", 2);

    if (gen->type == GEN_DNS) {
      ASN1_IA5STRING* name = gen->d.dNSName;

      BIO_write(out.get(), "DNS:", 4);
      BIO_write(out.get(), name->data, name->length);
    } else {
      STACK_OF(CONF_VALUE)* nval = i2v_GENERAL_NAME(
          const_cast<X509V3_EXT_METHOD*>(method), gen, nullptr);
      if (nval == nullptr)
        return false;
      X509V3_EXT_val_prn(out.get(), nval, 0, 0);
      sk_CONF_VALUE_pop_free(nval, X509V3_conf_free);
    }
  }
  sk_GENERAL_NAME_pop_free(names, GENERAL_NAME_free);

  return true;
}

MaybeLocal<Value> GetIssuerString(
    Environment* env,
    const BIOPointer& bio,
    X509* cert) {
  X509_NAME* issuer_name = X509_get_issuer_name(cert);
  if (X509_NAME_print_ex(bio.get(), issuer_name, 0, X509_NAME_FLAGS) <= 0) {
    USE(BIO_reset(bio.get()));
    return Undefined(env->isolate());
  }

  return ToV8Value(env, bio);
}

MaybeLocal<Value> GetSubject(
    Environment* env,
    const BIOPointer& bio,
    X509* cert) {
  if (X509_NAME_print_ex(
          bio.get(),
          X509_get_subject_name(cert),
          0,
          X509_NAME_FLAGS) <= 0) {
    USE(BIO_reset(bio.get()));
    return Undefined(env->isolate());
  }

  return ToV8Value(env, bio);
}

MaybeLocal<Value> GetCipherName(Environment* env, const SSLPointer& ssl) {
  return GetCipherName(env, SSL_get_current_cipher(ssl.get()));
}

MaybeLocal<Value> GetCipherStandardName(
    Environment* env,
    const SSLPointer& ssl) {
  return GetCipherStandardName(env, SSL_get_current_cipher(ssl.get()));
}

MaybeLocal<Value> GetCipherVersion(Environment* env, const SSLPointer& ssl) {
  return GetCipherVersion(env, SSL_get_current_cipher(ssl.get()));
}

MaybeLocal<Array> GetClientHelloCiphers(
    Environment* env,
    const SSLPointer& ssl) {
  EscapableHandleScope scope(env->isolate());
  const unsigned char* buf;
  size_t len = SSL_client_hello_get0_ciphers(ssl.get(), &buf);
  size_t count = len / 2;
  MaybeStackBuffer<Local<Value>, 16> ciphers(count);
  int j = 0;
  for (size_t n = 0; n < len; n += 2) {
    const SSL_CIPHER* cipher = SSL_CIPHER_find(ssl.get(), buf);
    buf += 2;
    Local<Object> obj = Object::New(env->isolate());
    if (!Set(env->context(),
             obj,
             env->name_string(),
             GetCipherName(env, cipher)) ||
        !Set(env->context(),
             obj,
             env->standard_name_string(),
             GetCipherStandardName(env, cipher)) ||
        !Set(env->context(),
             obj,
             env->version_string(),
             GetCipherVersion(env, cipher))) {
      return MaybeLocal<Array>();
    }
    ciphers[j++] = obj;
  }
  Local<Array> ret = Array::New(env->isolate(), ciphers.out(), count);
  return scope.Escape(ret);
}


MaybeLocal<Object> GetCipherInfo(Environment* env, const SSLPointer& ssl) {
  if (SSL_get_current_cipher(ssl.get()) == nullptr)
    return MaybeLocal<Object>();
  EscapableHandleScope scope(env->isolate());
  Local<Object> info = Object::New(env->isolate());

  if (!Set<Value>(env->context(),
                  info,
                  env->name_string(),
                  GetCipherName(env, ssl)) ||
      !Set<Value>(env->context(),
                  info,
                  env->standard_name_string(),
                  GetCipherStandardName(env, ssl)) ||
      !Set<Value>(env->context(),
                  info,
                  env->version_string(),
                  GetCipherVersion(env, ssl))) {
    return MaybeLocal<Object>();
  }

  return scope.Escape(info);
}

MaybeLocal<Object> GetEphemeralKey(Environment* env, const SSLPointer& ssl) {
  CHECK_EQ(SSL_is_server(ssl.get()), 0);
  EVP_PKEY* raw_key;

  EscapableHandleScope scope(env->isolate());
  Local<Object> info = Object::New(env->isolate());
  if (!SSL_get_server_tmp_key(ssl.get(), &raw_key))
    return scope.Escape(info);

  Local<Context> context = env->context();
  crypto::EVPKeyPointer key(raw_key);

  int kid = EVP_PKEY_id(key.get());
  int bits = EVP_PKEY_bits(key.get());
  switch (kid) {
    case EVP_PKEY_DH:
      if (!Set<String>(context, info, env->type_string(), env->dh_string()) ||
          !Set<Integer>(context,
               info,
               env->size_string(),
               Integer::New(env->isolate(), bits))) {
        return MaybeLocal<Object>();
      }
      break;
    case EVP_PKEY_EC:
    case EVP_PKEY_X25519:
    case EVP_PKEY_X448:
      {
        const char* curve_name;
        if (kid == EVP_PKEY_EC) {
          ECKeyPointer ec(EVP_PKEY_get1_EC_KEY(key.get()));
          int nid = EC_GROUP_get_curve_name(EC_KEY_get0_group(ec.get()));
          curve_name = OBJ_nid2sn(nid);
        } else {
          curve_name = OBJ_nid2sn(kid);
        }
        if (!Set<String>(context,
                         info,
                         env->type_string(),
                         env->ecdh_string()) ||
            !Set<String>(context,
                info,
                env->name_string(),
                OneByteString(env->isolate(), curve_name)) ||
            !Set<Integer>(context,
                 info,
                 env->size_string(),
                 Integer::New(env->isolate(), bits))) {
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
  AllocatedBuffer buf = AllocatedBuffer::AllocateManaged(env, len);
  len = EC_POINT_point2oct(group,
                           point,
                           form,
                           reinterpret_cast<unsigned char*>(buf.data()),
                           buf.size(),
                           nullptr);
  if (len == 0) {
    if (error != nullptr) *error = "Failed to get public key";
    return MaybeLocal<Object>();
  }
  return buf.ToBuffer();
}

MaybeLocal<Value> GetPeerCert(
    Environment* env,
    const SSLPointer& ssl,
    bool abbreviated,
    bool is_server) {
  ClearErrorOnReturn clear_error_on_return;
  Local<Object> result;
  MaybeLocal<Object> maybe_cert;

  // NOTE: This is because of the odd OpenSSL behavior. On client `cert_chain`
  // contains the `peer_certificate`, but on server it doesn't.
  X509Pointer cert(is_server ? SSL_get_peer_certificate(ssl.get()) : nullptr);
  STACK_OF(X509)* ssl_certs = SSL_get_peer_cert_chain(ssl.get());
  if (!cert && (ssl_certs == nullptr || sk_X509_num(ssl_certs) == 0))
    return Undefined(env->isolate());

  // Short result requested.
  if (abbreviated) {
    maybe_cert =
        X509ToObject(env, cert ? cert.get() : sk_X509_value(ssl_certs, 0));
    return maybe_cert.ToLocal(&result) ? result : MaybeLocal<Value>();
  }

  StackOfX509 peer_certs = CloneSSLCerts(std::move(cert), ssl_certs);
  if (peer_certs == nullptr)
    return Undefined(env->isolate());

  // First and main certificate.
  X509Pointer first_cert(sk_X509_value(peer_certs.get(), 0));
  CHECK(first_cert);
  maybe_cert = X509ToObject(env, first_cert.release());
  if (!maybe_cert.ToLocal(&result))
    return MaybeLocal<Value>();

  Local<Object> issuer_chain;
  MaybeLocal<Object> maybe_issuer_chain;

  maybe_issuer_chain =
      AddIssuerChainToObject(
          &cert,
          result,
          std::move(peer_certs),
          env);
  if (!maybe_issuer_chain.ToLocal(&issuer_chain))
    return MaybeLocal<Value>();

  maybe_issuer_chain =
      GetLastIssuedCert(
          &cert,
          ssl,
          issuer_chain,
          env);

  issuer_chain.Clear();
  if (!maybe_issuer_chain.ToLocal(&issuer_chain))
    return MaybeLocal<Value>();

  // Last certificate should be self-signed.
  if (X509_check_issued(cert.get(), cert.get()) == X509_V_OK &&
      !Set<Object>(env->context(),
           issuer_chain,
           env->issuercert_string(),
           issuer_chain)) {
    return MaybeLocal<Value>();
  }

  return result;
}

MaybeLocal<Object> X509ToObject(Environment* env, X509* cert) {
  EscapableHandleScope scope(env->isolate());
  Local<Context> context = env->context();
  Local<Object> info = Object::New(env->isolate());

  BIOPointer bio(BIO_new(BIO_s_mem()));

  if (!Set<Value>(context,
                  info,
                  env->subject_string(),
                  GetSubject(env, bio, cert)) ||
      !Set<Value>(context,
                  info,
                  env->issuer_string(),
                  GetIssuerString(env, bio, cert)) ||
      !Set<Value>(context,
                  info,
                  env->subjectaltname_string(),
                  GetInfoString<NID_subject_alt_name>(env, bio, cert)) ||
      !Set<Value>(context,
                  info,
                  env->infoaccess_string(),
                  GetInfoString<NID_info_access>(env, bio, cert))) {
    return MaybeLocal<Object>();
  }

  EVPKeyPointer pkey(X509_get_pubkey(cert));
  RSAPointer rsa;
  ECPointer ec;
  if (pkey) {
    switch (EVP_PKEY_id(pkey.get())) {
      case EVP_PKEY_RSA:
        rsa.reset(EVP_PKEY_get1_RSA(pkey.get()));
        break;
      case EVP_PKEY_EC:
        ec.reset(EVP_PKEY_get1_EC_KEY(pkey.get()));
        break;
    }
  }

  if (rsa) {
    const BIGNUM* n;
    const BIGNUM* e;
    RSA_get0_key(rsa.get(), &n, &e, nullptr);
    if (!Set<Value>(context,
                    info,
                    env->modulus_string(),
                    GetModulusString(env, bio, n)) ||
        !Set<Value>(context, info, env->bits_string(), GetBits(env, n)) ||
        !Set<Value>(context,
                    info,
                    env->exponent_string(),
                    GetExponentString(env, bio, e)) ||
        !Set<Object>(context,
                     info,
                     env->pubkey_string(),
                     GetPubKey(env, rsa))) {
      return MaybeLocal<Object>();
    }
  } else if (ec) {
    const EC_GROUP* group = EC_KEY_get0_group(ec.get());

    if (!Set<Value>(context,
                    info,
                    env->bits_string(),
                    GetECGroup(env, group, ec)) ||
        !Set<Value>(context,
                    info,
                    env->pubkey_string(),
                    GetECPubKey(env, group, ec))) {
      return MaybeLocal<Object>();
    }

    const int nid = EC_GROUP_get_curve_name(group);
    if (nid != 0) {
      // Curve is well-known, get its OID and NIST nick-name (if it has one).

      if (!Set<Value>(context,
                      info,
                      env->asn1curve_string(),
                      GetCurveASN1Name(env, nid)) ||
          !Set<Value>(context,
                      info,
                      env->nistcurve_string(),
                      GetCurveNistName(env, nid))) {
        return MaybeLocal<Object>();
      }
    } else {
      // Unnamed curves can be described by their mathematical properties,
      // but aren't used much (at all?) with X.509/TLS. Support later if needed.
    }
  }

  // pkey, rsa, and ec pointers are no longer needed.
  pkey.reset();
  rsa.reset();
  ec.reset();

  if (!Set<Value>(context,
                  info,
                  env->valid_from_string(),
                  GetValidFrom(env, cert, bio)) ||
      !Set<Value>(context,
                  info,
                  env->valid_to_string(),
                  GetValidTo(env, cert, bio))) {
    return MaybeLocal<Object>();
  }

  // bio is no longer needed
  bio.reset();

  if (!Set<Value>(context,
                  info,
                  env->fingerprint_string(),
                  GetFingerprintDigest(env, EVP_sha1(), cert)) ||
      !Set<Value>(context,
                  info,
                  env->fingerprint256_string(),
                  GetFingerprintDigest(env, EVP_sha256(), cert)) ||
      !Set<Value>(context,
                  info,
                  env->ext_key_usage_string(),
                  GetKeyUsage(env, cert)) ||
      !Set<Value>(context,
                  info,
                  env->serial_number_string(),
                  GetSerialNumber(env, cert)) ||
      !Set<Object>(context,
                   info,
                   env->raw_string(),
                   GetRawDERCertificate(env, cert))) {
    return MaybeLocal<Object>();
  }

  return scope.Escape(info);
}

}  // namespace crypto
}  // namespace node
