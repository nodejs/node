#include "crypto/crypto_spkac.h"
#include "crypto/crypto_common.h"
#include "crypto/crypto_util.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node.h"
#include "v8.h"

namespace node {

using v8::FunctionCallbackInfo;
using v8::Local;
using v8::Object;
using v8::Value;

namespace crypto {
namespace SPKAC {
bool VerifySpkac(const ArrayBufferOrViewContents<char>& input) {
  size_t length = input.size();
#ifdef OPENSSL_IS_BORINGSSL
  // OpenSSL uses EVP_DecodeBlock, which explicitly removes trailing characters,
  // while BoringSSL uses EVP_DecodedLength and EVP_DecodeBase64, which do not.
  // As such, we trim those characters here for compatibility.
  length = std::string(input.data()).find_last_not_of(" \n\r\t") + 1;
#endif
  NetscapeSPKIPointer spki(
      NETSCAPE_SPKI_b64_decode(input.data(), length));
  if (!spki)
    return false;

  EVPKeyPointer pkey(X509_PUBKEY_get(spki->spkac->pubkey));
  if (!pkey)
    return false;

  return NETSCAPE_SPKI_verify(spki.get(), pkey.get()) > 0;
}

void VerifySpkac(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  ArrayBufferOrViewContents<char> input(args[0]);
  if (input.size() == 0)
    return args.GetReturnValue().SetEmptyString();

  if (UNLIKELY(!input.CheckSizeInt32()))
    return THROW_ERR_OUT_OF_RANGE(env, "spkac is too large");

  args.GetReturnValue().Set(VerifySpkac(input));
}

ByteSource ExportPublicKey(Environment* env,
                           const ArrayBufferOrViewContents<char>& input) {
  BIOPointer bio(BIO_new(BIO_s_mem()));
  if (!bio) return ByteSource();

  size_t length = input.size();
#ifdef OPENSSL_IS_BORINGSSL
  // OpenSSL uses EVP_DecodeBlock, which explicitly removes trailing characters,
  // while BoringSSL uses EVP_DecodedLength and EVP_DecodeBase64, which do not.
  // As such, we trim those characters here for compatibility.
  length = std::string(input.data()).find_last_not_of(" \n\r\t") + 1;
#endif
  NetscapeSPKIPointer spki(
      NETSCAPE_SPKI_b64_decode(input.data(), length));
  if (!spki) return ByteSource();

  EVPKeyPointer pkey(NETSCAPE_SPKI_get_pubkey(spki.get()));
  if (!pkey) return ByteSource();

  if (PEM_write_bio_PUBKEY(bio.get(), pkey.get()) <= 0) return ByteSource();

  return ByteSource::FromBIO(bio);
}

void ExportPublicKey(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  ArrayBufferOrViewContents<char> input(args[0]);
  if (input.size() == 0) return args.GetReturnValue().SetEmptyString();

  if (UNLIKELY(!input.CheckSizeInt32()))
    return THROW_ERR_OUT_OF_RANGE(env, "spkac is too large");

  ByteSource pkey = ExportPublicKey(env, input);
  if (!pkey) return args.GetReturnValue().SetEmptyString();

  args.GetReturnValue().Set(pkey.ToBuffer(env).FromMaybe(Local<Value>()));
}

ByteSource ExportChallenge(const ArrayBufferOrViewContents<char>& input) {
  size_t length = input.size();
#ifdef OPENSSL_IS_BORINGSSL
  // OpenSSL uses EVP_DecodeBlock, which explicitly removes trailing characters,
  // while BoringSSL uses EVP_DecodedLength and EVP_DecodeBase64, which do not.
  // As such, we trim those characters here for compatibility.
  length = std::string(input.data()).find_last_not_of(" \n\r\t") + 1;
#endif
  NetscapeSPKIPointer sp(
      NETSCAPE_SPKI_b64_decode(input.data(), length));
  if (!sp)
    return ByteSource();

  char* buf = nullptr;
  ASN1_STRING_to_UTF8(
    reinterpret_cast<unsigned char**>(&buf),
    sp->spkac->challenge);

  return ByteSource::Allocated(buf, strlen(buf));
}

void ExportChallenge(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  ArrayBufferOrViewContents<char> input(args[0]);
  if (input.size() == 0)
    return args.GetReturnValue().SetEmptyString();

  if (UNLIKELY(!input.CheckSizeInt32()))
    return THROW_ERR_OUT_OF_RANGE(env, "spkac is too large");

  ByteSource cert = ExportChallenge(input);
  if (!cert)
    return args.GetReturnValue().SetEmptyString();

  Local<Value> outString =
      Encode(env->isolate(), cert.data<char>(), cert.size(), BUFFER);

  args.GetReturnValue().Set(outString);
}

void Initialize(Environment* env, Local<Object> target) {
  env->SetMethodNoSideEffect(target, "certVerifySpkac", VerifySpkac);
  env->SetMethodNoSideEffect(target, "certExportPublicKey", ExportPublicKey);
  env->SetMethodNoSideEffect(target, "certExportChallenge", ExportChallenge);
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(VerifySpkac);
  registry->Register(ExportPublicKey);
  registry->Register(ExportChallenge);
}
}  // namespace SPKAC
}  // namespace crypto
}  // namespace node
