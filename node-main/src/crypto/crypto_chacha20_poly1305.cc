#include "crypto/crypto_chacha20_poly1305.h"
#include "async_wrap-inl.h"
#include "base_object-inl.h"
#include "crypto/crypto_cipher.h"
#include "crypto/crypto_keys.h"
#include "crypto/crypto_util.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "threadpoolwork-inl.h"
#include "v8.h"

#include <openssl/evp.h>

namespace node {

using ncrypto::Cipher;
using ncrypto::CipherCtxPointer;
using ncrypto::DataPointer;
using v8::FunctionCallbackInfo;
using v8::JustVoid;
using v8::Local;
using v8::Maybe;
using v8::Nothing;
using v8::Object;
using v8::Value;

namespace crypto {
namespace {
constexpr size_t kChaCha20Poly1305KeySize = 32;
constexpr size_t kChaCha20Poly1305IvSize = 12;
constexpr size_t kChaCha20Poly1305TagSize = 16;

bool ValidateIV(Environment* env,
                CryptoJobMode mode,
                Local<Value> value,
                ChaCha20Poly1305CipherConfig* params) {
  ArrayBufferOrViewContents<unsigned char> iv(value);
  if (!iv.CheckSizeInt32()) [[unlikely]] {
    THROW_ERR_OUT_OF_RANGE(env, "iv is too large");
    return false;
  }

  if (iv.size() != kChaCha20Poly1305IvSize) {
    THROW_ERR_CRYPTO_INVALID_IV(env);
    return false;
  }

  if (mode == kCryptoJobAsync) {
    params->iv = iv.ToCopy();
  } else {
    params->iv = iv.ToByteSource();
  }

  return true;
}

bool ValidateAuthTag(Environment* env,
                     CryptoJobMode mode,
                     WebCryptoCipherMode cipher_mode,
                     Local<Value> value,
                     ChaCha20Poly1305CipherConfig* params) {
  switch (cipher_mode) {
    case kWebCryptoCipherDecrypt: {
      if (!IsAnyBufferSource(value)) {
        THROW_ERR_CRYPTO_INVALID_TAG_LENGTH(
            env, "Authentication tag must be a buffer");
        return false;
      }

      ArrayBufferOrViewContents<unsigned char> tag(value);
      if (!tag.CheckSizeInt32()) [[unlikely]] {
        THROW_ERR_OUT_OF_RANGE(env, "tag is too large");
        return false;
      }

      if (tag.size() != kChaCha20Poly1305TagSize) {
        THROW_ERR_CRYPTO_INVALID_TAG_LENGTH(
            env, "Invalid authentication tag length");
        return false;
      }

      if (mode == kCryptoJobAsync) {
        params->tag = tag.ToCopy();
      } else {
        params->tag = tag.ToByteSource();
      }
      break;
    }
    case kWebCryptoCipherEncrypt: {
      // For encryption, the value should be the tag length (passed from
      // JavaScript) We expect it to be the tag size constant for
      // ChaCha20-Poly1305
      if (!value->IsUint32()) {
        THROW_ERR_CRYPTO_INVALID_TAG_LENGTH(env, "Tag length must be a number");
        return false;
      }

      uint32_t tag_length = value.As<v8::Uint32>()->Value();
      if (tag_length != kChaCha20Poly1305TagSize) {
        THROW_ERR_CRYPTO_INVALID_TAG_LENGTH(
            env, "Invalid tag length for ChaCha20-Poly1305");
        return false;
      }
      // Tag is generated during encryption, not provided
      break;
    }
    default:
      UNREACHABLE();
  }

  return true;
}

bool ValidateAdditionalData(Environment* env,
                            CryptoJobMode mode,
                            Local<Value> value,
                            ChaCha20Poly1305CipherConfig* params) {
  if (IsAnyBufferSource(value)) {
    ArrayBufferOrViewContents<unsigned char> additional_data(value);
    if (!additional_data.CheckSizeInt32()) [[unlikely]] {
      THROW_ERR_OUT_OF_RANGE(env, "additional data is too large");
      return false;
    }

    if (mode == kCryptoJobAsync) {
      params->additional_data = additional_data.ToCopy();
    } else {
      params->additional_data = additional_data.ToByteSource();
    }
  }

  return true;
}
}  // namespace

ChaCha20Poly1305CipherConfig::ChaCha20Poly1305CipherConfig(
    ChaCha20Poly1305CipherConfig&& other) noexcept
    : mode(other.mode),
      cipher(other.cipher),
      iv(std::move(other.iv)),
      additional_data(std::move(other.additional_data)),
      tag(std::move(other.tag)) {}

ChaCha20Poly1305CipherConfig& ChaCha20Poly1305CipherConfig::operator=(
    ChaCha20Poly1305CipherConfig&& other) noexcept {
  if (&other == this) return *this;
  this->~ChaCha20Poly1305CipherConfig();
  return *new (this) ChaCha20Poly1305CipherConfig(std::move(other));
}

void ChaCha20Poly1305CipherConfig::MemoryInfo(MemoryTracker* tracker) const {
  // If mode is sync, then the data in each of these properties
  // is not owned by the ChaCha20Poly1305CipherConfig, so we ignore it.
  if (mode == kCryptoJobAsync) {
    tracker->TrackFieldWithSize("iv", iv.size());
    tracker->TrackFieldWithSize("additional_data", additional_data.size());
    tracker->TrackFieldWithSize("tag", tag.size());
  }
}

Maybe<void> ChaCha20Poly1305CipherTraits::AdditionalConfig(
    CryptoJobMode mode,
    const FunctionCallbackInfo<Value>& args,
    unsigned int offset,
    WebCryptoCipherMode cipher_mode,
    ChaCha20Poly1305CipherConfig* params) {
  Environment* env = Environment::GetCurrent(args);

  params->mode = mode;
  params->cipher = ncrypto::Cipher::CHACHA20_POLY1305;

  if (!params->cipher) {
    THROW_ERR_CRYPTO_UNKNOWN_CIPHER(env);
    return Nothing<void>();
  }

  // IV parameter (required)
  if (!ValidateIV(env, mode, args[offset], params)) {
    return Nothing<void>();
  }

  // Authentication tag parameter (only for decryption) or tag length (for
  // encryption)
  if (static_cast<unsigned int>(args.Length()) > offset + 1) {
    if (!ValidateAuthTag(env, mode, cipher_mode, args[offset + 1], params)) {
      return Nothing<void>();
    }
  }

  // Additional authenticated data parameter (optional)
  if (static_cast<unsigned int>(args.Length()) > offset + 2) {
    if (!ValidateAdditionalData(env, mode, args[offset + 2], params)) {
      return Nothing<void>();
    }
  }

  return JustVoid();
}

WebCryptoCipherStatus ChaCha20Poly1305CipherTraits::DoCipher(
    Environment* env,
    const KeyObjectData& key_data,
    WebCryptoCipherMode cipher_mode,
    const ChaCha20Poly1305CipherConfig& params,
    const ByteSource& in,
    ByteSource* out) {
  CHECK_EQ(key_data.GetKeyType(), kKeyTypeSecret);

  // Validate key size
  if (key_data.GetSymmetricKeySize() != kChaCha20Poly1305KeySize) {
    return WebCryptoCipherStatus::INVALID_KEY_TYPE;
  }

  auto ctx = CipherCtxPointer::New();
  CHECK(ctx);

  const bool encrypt = cipher_mode == kWebCryptoCipherEncrypt;

  if (!ctx.init(params.cipher, encrypt)) {
    return WebCryptoCipherStatus::FAILED;
  }

  if (!ctx.setKeyLength(key_data.GetSymmetricKeySize()) ||
      !ctx.init(
          Cipher(),
          encrypt,
          reinterpret_cast<const unsigned char*>(key_data.GetSymmetricKey()),
          params.iv.data<unsigned char>())) {
    return WebCryptoCipherStatus::FAILED;
  }

  size_t tag_len = 0;

  switch (cipher_mode) {
    case kWebCryptoCipherDecrypt: {
      if (params.tag.size() != kChaCha20Poly1305TagSize) {
        return WebCryptoCipherStatus::FAILED;
      }
      if (!ctx.setAeadTag(ncrypto::Buffer<const char>{
              .data = params.tag.data<char>(),
              .len = params.tag.size(),
          })) {
        return WebCryptoCipherStatus::FAILED;
      }
      break;
    }
    case kWebCryptoCipherEncrypt: {
      tag_len = kChaCha20Poly1305TagSize;
      break;
    }
    default:
      UNREACHABLE();
  }

  size_t total = 0;
  int buf_len = in.size() + ctx.getBlockSize() + tag_len;
  int out_len;

  // Process additional authenticated data if present
  ncrypto::Buffer<const unsigned char> buffer = {
      .data = params.additional_data.data<unsigned char>(),
      .len = params.additional_data.size(),
  };
  if (params.additional_data.size() && !ctx.update(buffer, nullptr, &out_len)) {
    return WebCryptoCipherStatus::FAILED;
  }

  auto buf = DataPointer::Alloc(buf_len);
  auto ptr = static_cast<unsigned char*>(buf.get());

  // Process the input data
  buffer = {
      .data = in.data<unsigned char>(),
      .len = in.size(),
  };
  if (in.empty()) {
    if (!ctx.update({}, ptr, &out_len)) {
      return WebCryptoCipherStatus::FAILED;
    }
  } else if (!ctx.update(buffer, ptr, &out_len)) {
    return WebCryptoCipherStatus::FAILED;
  }

  total += out_len;
  CHECK_LE(out_len, buf_len);
  out_len = ctx.getBlockSize();
  if (!ctx.update({}, ptr + total, &out_len, true)) {
    return WebCryptoCipherStatus::FAILED;
  }
  total += out_len;

  // If encrypting, grab the generated auth tag and append it to the ciphertext
  if (encrypt) {
    if (!ctx.getAeadTag(kChaCha20Poly1305TagSize, ptr + total)) {
      return WebCryptoCipherStatus::FAILED;
    }
    total += kChaCha20Poly1305TagSize;
  }

  if (total == 0) {
    *out = ByteSource();
    return WebCryptoCipherStatus::OK;
  }

  // Size down to the actual used space
  buf = buf.resize(total);
  *out = ByteSource::Allocated(buf.release());

  return WebCryptoCipherStatus::OK;
}

void ChaCha20Poly1305::Initialize(Environment* env, Local<Object> target) {
  ChaCha20Poly1305CryptoJob::Initialize(env, target);
}

void ChaCha20Poly1305::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  ChaCha20Poly1305CryptoJob::RegisterExternalReferences(registry);
}

}  // namespace crypto
}  // namespace node
