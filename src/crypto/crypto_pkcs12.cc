#include "crypto/crypto_pkcs12.h"
#include "crypto/crypto_context.h"
#include "crypto/crypto_keys.h"
#include "crypto/crypto_util.h"
#include "crypto/crypto_x509.h"
#include "env-inl.h"
#include "ncrypto.h"
#include "node_errors.h"
#include "util-inl.h"
#include "v8.h"

#include <openssl/pkcs12.h>

#include <string>

namespace node {

using ncrypto::BIOPointer;
using v8::Array;
using v8::ArrayBufferView;
using v8::FunctionCallbackInfo;
using v8::Local;
using v8::LocalVector;
using v8::Object;
using v8::Value;

namespace crypto {

// Parses a PKCS#12 bundle from `bio`. See crypto_pkcs12.h; shared with
// SecureContext::LoadPKCS12(), which backs the TLS `pfx` option.
PKCS12ParseResult ParsePKCS12Bundle(const BIOPointer& bio, const char* pass) {
  using PKCS12Pointer = ncrypto::DeleteFnPtr<PKCS12, PKCS12_free>;

  ncrypto::ClearErrorOnReturn clear_error_on_return;

  if (!bio) {
    return PKCS12ParseResult(PKCS12ParseError::FAILED, ERR_get_error());
  }

  PKCS12* p12_ptr = nullptr;
  if (!d2i_PKCS12_bio(bio.get(), &p12_ptr) || p12_ptr == nullptr) {
    return PKCS12ParseResult(PKCS12ParseError::NOT_RECOGNIZED, ERR_get_error());
  }
  PKCS12Pointer p12(p12_ptr);

  EVP_PKEY* pkey_ptr = nullptr;
  X509* cert_ptr = nullptr;
  STACK_OF(X509)* ca_ptr = nullptr;

  if (!PKCS12_parse(p12.get(), pass, &pkey_ptr, &cert_ptr, &ca_ptr)) {
    unsigned long err = ERR_get_error();  // NOLINT(runtime/int)
#ifdef OPENSSL_IS_BORINGSSL
    // BoringSSL's d2i_PKCS12() only copies the input; it does no ASN.1
    // validation, so malformed input gets this far rather than failing above.
    // PKCS8_R_BAD_PKCS12_DATA is raised only for structural failures -- a
    // wrong passphrase is reported as PKCS8_R_INCORRECT_PASSWORD instead --
    // so it means the same thing the d2i failure does on OpenSSL.
    if (ERR_GET_LIB(err) == ERR_LIB_PKCS8 &&
        ERR_GET_REASON(err) == PKCS8_R_BAD_PKCS12_DATA) {
      return PKCS12ParseResult(PKCS12ParseError::NOT_RECOGNIZED, err);
    }
#endif
#if OPENSSL_VERSION_MAJOR >= 3
    // OpenSSL 3 reports algorithms that moved to the legacy provider as a
    // bare "unsupported" error.
    if (ERR_GET_REASON(err) == ERR_R_UNSUPPORTED) {
      return PKCS12ParseResult(PKCS12ParseError::UNSUPPORTED_ALGORITHM, err);
    }
#endif
    return PKCS12ParseResult(PKCS12ParseError::FAILED, err);
  }

  PKCS12Contents contents;
  contents.key.reset(pkey_ptr);
  contents.cert.reset(cert_ptr);
  contents.ca.reset(ca_ptr);
  return PKCS12ParseResult(std::move(contents));
}

namespace {

// Returns [ keyHandle | null, certificate | null,
//           [ ...additionalCertificates ] ].
void ParsePKCS12(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK_GE(args.Length(), 2);

  BIOPointer bio(LoadBIO(env, args[0]));
  if (!bio) {
    return THROW_ERR_CRYPTO_OPERATION_FAILED(env,
                                             "Unable to read PKCS#12 data");
  }

  // PKCS12_parse() takes a NUL-terminated C string; a view is not one, so copy
  // the bytes out.
  std::string pass_storage;
  const char* pass = nullptr;
  if (!args[1]->IsUndefined()) {
    THROW_AND_RETURN_IF_NOT_BUFFER(env, args[1], "passphrase");
    Local<ArrayBufferView> abv = args[1].As<ArrayBufferView>();
    size_t len = abv->ByteLength();
    pass_storage.resize(len);
    if (len > 0) abv->CopyContents(pass_storage.data(), len);
    // PKCS12_parse() has no length parameter, so an embedded NUL byte would
    // truncate the passphrase and open the bundle with only the bytes that
    // precede it. Reject it rather than silently accepting a shorter
    // passphrase than the caller supplied. The JS layer checks this too;
    // this is the backstop.
    if (pass_storage.find('\0') != std::string::npos) {
      return THROW_ERR_INVALID_ARG_VALUE(
          env, "passphrase must not contain null bytes");
    }
    pass = pass_storage.c_str();
  }

  auto parsed = ParsePKCS12Bundle(bio, pass);

  if (!parsed) {
    switch (*parsed.error) {
      case PKCS12ParseError::NOT_RECOGNIZED:
        return THROW_ERR_CRYPTO_OPERATION_FAILED(
            env, "Input is not a valid PKCS#12 structure");
      case PKCS12ParseError::UNSUPPORTED_ALGORITHM:
        return THROW_ERR_CRYPTO_UNSUPPORTED_OPERATION(
            env,
            "PKCS#12 bundle uses an unsupported algorithm; it may require "
            "the OpenSSL legacy provider");
      case PKCS12ParseError::FAILED:
        return ThrowCryptoError(env,
                                parsed.openssl_error.value_or(0),
                                "Unable to parse PKCS#12 data");
    }
  }

  Local<Value> key_value = v8::Null(env->isolate());
  if (parsed.value.key) {
    auto data = KeyObjectData::CreateAsymmetric(KeyType::kKeyTypePrivate,
                                                std::move(parsed.value.key));
    Local<Object> handle;
    if (!KeyObjectHandle::Create(env, data).ToLocal(&handle)) return;
    key_value = handle;
  }

  Local<Value> cert_value = v8::Null(env->isolate());
  if (parsed.value.cert) {
    Local<Object> cert;
    if (!X509Certificate::New(env, std::move(parsed.value.cert))
             .ToLocal(&cert)) {
      return;
    }
    cert_value = cert;
  }

  const int ca_count = parsed.value.ca ? sk_X509_num(parsed.value.ca.get()) : 0;
  LocalVector<Value> ca_values(env->isolate());
  ca_values.reserve(ca_count > 0 ? ca_count : 0);

  for (int i = 0; i < ca_count; i++) {
    X509* ca = sk_X509_value(parsed.value.ca.get(), i);
    if (ca == nullptr) continue;
    // New() takes ownership, so hand it a reference of its own rather than
    // the stack's.
    X509_up_ref(ca);
    ncrypto::X509Pointer owned(ca);
    Local<Object> obj;
    if (!X509Certificate::New(env, std::move(owned)).ToLocal(&obj)) return;
    ca_values.push_back(obj);
  }

  Local<Value> result[] = {
      key_value,
      cert_value,
      Array::New(env->isolate(), ca_values.data(), ca_values.size()),
  };

  args.GetReturnValue().Set(
      Array::New(env->isolate(), result, arraysize(result)));
}

}  // anonymous namespace

void PKCS12Parser::Initialize(Environment* env, Local<Object> target) {
  SetMethodNoSideEffect(env->context(), target, "parsePKCS12", ParsePKCS12);
}

void PKCS12Parser::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(ParsePKCS12);
}

}  // namespace crypto
}  // namespace node
