#ifndef SRC_CRYPTO_CRYPTO_KEYS_H_
#define SRC_CRYPTO_CRYPTO_KEYS_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "crypto/crypto_util.h"
#include "base_object.h"
#include "env.h"
#include "memory_tracker.h"
#include "node_buffer.h"
#include "node_worker.h"
#include "v8.h"

#include <openssl/evp.h>

#include <memory>
#include <string>

namespace node::crypto {
enum KeyType {
  kKeyTypeSecret,
  kKeyTypePublic,
  kKeyTypePrivate
};

enum KeyEncodingContext {
  kKeyContextInput,
  kKeyContextExport,
  kKeyContextGenerate
};

enum class ParseKeyResult {
  kParseKeyNotRecognized =
      static_cast<int>(ncrypto::EVPKeyPointer::PKParseError::NOT_RECOGNIZED),
  kParseKeyNeedPassphrase =
      static_cast<int>(ncrypto::EVPKeyPointer::PKParseError::NEED_PASSPHRASE),
  kParseKeyFailed =
      static_cast<int>(ncrypto::EVPKeyPointer::PKParseError::FAILED),
  kParseKeyOk,
};

// Objects of this class can safely be shared among threads.
class KeyObjectData final : public MemoryRetainer {
 public:
  static KeyObjectData CreateSecret(ByteSource key);

  static KeyObjectData CreateAsymmetric(KeyType type,
                                        ncrypto::EVPKeyPointer&& pkey);

  KeyObjectData(std::nullptr_t = nullptr);

  inline operator bool() const { return data_ != nullptr; }

  KeyType GetKeyType() const;

  // These functions allow unprotected access to the raw key material and should
  // only be used to implement cryptographic operations requiring the key.
  const ncrypto::EVPKeyPointer& GetAsymmetricKey() const;
  const char* GetSymmetricKey() const;
  size_t GetSymmetricKeySize() const;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(KeyObjectData)
  SET_SELF_SIZE(KeyObjectData)

  Mutex& mutex() const;

  static v8::Maybe<ncrypto::EVPKeyPointer::PublicKeyEncodingConfig>
  GetPublicKeyEncodingFromJs(const v8::FunctionCallbackInfo<v8::Value>& args,
                             unsigned int* offset,
                             KeyEncodingContext context);

  static KeyObjectData GetPrivateKeyFromJs(
      const v8::FunctionCallbackInfo<v8::Value>& args,
      unsigned int* offset,
      bool allow_key_object);

  static KeyObjectData GetPublicOrPrivateKeyFromJs(
      const v8::FunctionCallbackInfo<v8::Value>& args, unsigned int* offset);

  static v8::Maybe<ncrypto::EVPKeyPointer::PrivateKeyEncodingConfig>
  GetPrivateKeyEncodingFromJs(const v8::FunctionCallbackInfo<v8::Value>& args,
                              unsigned int* offset,
                              KeyEncodingContext context);

  bool ToEncodedPublicKey(
      Environment* env,
      const ncrypto::EVPKeyPointer::PublicKeyEncodingConfig& config,
      v8::Local<v8::Value>* out);

  bool ToEncodedPrivateKey(
      Environment* env,
      const ncrypto::EVPKeyPointer::PrivateKeyEncodingConfig& config,
      v8::Local<v8::Value>* out);

  inline KeyObjectData addRef() const {
    return KeyObjectData(key_type_, mutex_, data_);
  }

  inline KeyObjectData addRefWithType(KeyType type) const {
    return KeyObjectData(type, mutex_, data_);
  }

 private:
  explicit KeyObjectData(ByteSource symmetric_key);
  explicit KeyObjectData(KeyType type, ncrypto::EVPKeyPointer&& pkey);

  static KeyObjectData GetParsedKey(KeyType type,
                                    Environment* env,
                                    ncrypto::EVPKeyPointer&& pkey,
                                    ParseKeyResult ret,
                                    const char* default_msg);

  KeyType key_type_;
  mutable std::shared_ptr<Mutex> mutex_;

  struct Data {
    const ByteSource symmetric_key;
    const ncrypto::EVPKeyPointer asymmetric_key;
    explicit Data(ByteSource symmetric_key)
        : symmetric_key(std::move(symmetric_key)) {}
    explicit Data(ncrypto::EVPKeyPointer asymmetric_key)
        : asymmetric_key(std::move(asymmetric_key)) {}
  };
  std::shared_ptr<Data> data_;

  KeyObjectData(KeyType type,
                std::shared_ptr<Mutex> mutex,
                std::shared_ptr<Data> data)
      : key_type_(type), mutex_(std::move(mutex)), data_(std::move(data)) {}
};

class KeyObjectHandle : public BaseObject {
 public:
  static bool HasInstance(Environment* env, v8::Local<v8::Value> value);
  static v8::Local<v8::Function> Initialize(Environment* env);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  static v8::MaybeLocal<v8::Object> Create(Environment* env,
                                           const KeyObjectData& data);

  // TODO(tniessen): track the memory used by OpenSSL types
  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(KeyObjectHandle)
  SET_SELF_SIZE(KeyObjectHandle)

  const KeyObjectData& Data();

 protected:
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void Init(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetKeyDetail(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Equals(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void ExportJWK(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void GetAsymmetricKeyType(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  v8::Local<v8::Value> GetAsymmetricKeyType() const;

  static void CheckEcKeyData(const v8::FunctionCallbackInfo<v8::Value>& args);
  bool CheckEcKeyData() const;

  static void GetSymmetricKeySize(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  static void Export(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void RawPublicKey(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RawPrivateKey(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ExportECPublicRaw(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ExportECPrivateRaw(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RawSeed(const v8::FunctionCallbackInfo<v8::Value>& args);

  v8::MaybeLocal<v8::Value> ExportSecretKey() const;
  v8::MaybeLocal<v8::Value> ExportPublicKey(
      const ncrypto::EVPKeyPointer::PublicKeyEncodingConfig& config) const;
  v8::MaybeLocal<v8::Value> ExportPrivateKey(
      const ncrypto::EVPKeyPointer::PrivateKeyEncodingConfig& config) const;

  KeyObjectHandle(Environment* env,
                  v8::Local<v8::Object> wrap);

 private:
  KeyObjectData data_;
};

class NativeKeyObject : public BaseObject {
 public:
  static void Initialize(Environment* env, v8::Local<v8::Object> target);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void CreateNativeKeyObjectClass(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(NativeKeyObject)
  SET_SELF_SIZE(NativeKeyObject)

  class KeyObjectTransferData : public worker::TransferData {
   public:
    explicit KeyObjectTransferData(const KeyObjectData& data)
        : data_(data.addRef()) {}

    BaseObjectPtr<BaseObject> Deserialize(
        Environment* env,
        v8::Local<v8::Context> context,
        std::unique_ptr<worker::TransferData> self) override;

    SET_MEMORY_INFO_NAME(KeyObjectTransferData)
    SET_SELF_SIZE(KeyObjectTransferData)
    SET_NO_MEMORY_INFO()

   private:
    KeyObjectData data_;
  };

  BaseObject::TransferMode GetTransferMode() const override;
  std::unique_ptr<worker::TransferData> CloneForMessaging() const override;

 private:
  NativeKeyObject(Environment* env,
                  v8::Local<v8::Object> wrap,
                  const KeyObjectData& handle_data)
      : BaseObject(env, wrap), handle_data_(handle_data.addRef()) {
    MakeWeak();
  }

  KeyObjectData handle_data_;
};

enum WebCryptoKeyFormat {
  kWebCryptoKeyFormatRaw,
  kWebCryptoKeyFormatPKCS8,
  kWebCryptoKeyFormatSPKI,
  kWebCryptoKeyFormatJWK
};

namespace Keys {
void Initialize(Environment* env, v8::Local<v8::Object> target);
void RegisterExternalReferences(ExternalReferenceRegistry* registry);
}  // namespace Keys
}  // namespace node::crypto

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_CRYPTO_CRYPTO_KEYS_H_
