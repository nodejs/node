#ifndef SRC_CRYPTO_CRYPTO_X509_H_
#define SRC_CRYPTO_CRYPTO_X509_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "crypto/crypto_util.h"
#include "env.h"
#include "memory_tracker.h"
#include "node_worker.h"
#include "v8.h"

namespace node {
namespace crypto {

// The ManagedX509 class is essentially a smart pointer for
// X509 objects that allows an X509Certificate instance to
// be cloned at the JS level while pointing at the same
// underlying X509 instance.
class ManagedX509 : public MemoryRetainer {
 public:
  ManagedX509() = default;
  explicit ManagedX509(X509Pointer&& cert);
  ManagedX509(const ManagedX509& that);
  ManagedX509& operator=(const ManagedX509& that);

  operator bool() const { return !!cert_; }
  X509* get() const { return cert_.get(); }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(ManagedX509)
  SET_SELF_SIZE(ManagedX509)

 private:
  X509Pointer cert_;
};

class X509Certificate : public BaseObject {
 public:
  enum class GetPeerCertificateFlag {
    NONE,
    SERVER
  };

  static void Initialize(Environment* env, v8::Local<v8::Object> target);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static bool HasInstance(Environment* env, v8::Local<v8::Object> object);

  static v8::MaybeLocal<v8::Object> New(
      Environment* env,
      X509Pointer cert,
      STACK_OF(X509)* issuer_chain = nullptr);

  static v8::MaybeLocal<v8::Object> New(
      Environment* env,
      std::shared_ptr<ManagedX509> cert,
      STACK_OF(X509)* issuer_chain = nullptr);

  static v8::MaybeLocal<v8::Object> GetCert(
      Environment* env,
      const SSLPointer& ssl);

  static v8::MaybeLocal<v8::Object> GetPeerCert(
      Environment* env,
      const SSLPointer& ssl,
      GetPeerCertificateFlag flag);

  static v8::Local<v8::Object> Wrap(
      Environment* env,
      v8::Local<v8::Object> object,
      X509Pointer cert);

  static void Parse(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Subject(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SubjectAltName(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Issuer(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void InfoAccess(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ValidFrom(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ValidTo(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void KeyUsage(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SerialNumber(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Raw(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void PublicKey(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Pem(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void CheckCA(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void CheckHost(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void CheckEmail(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void CheckIP(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void CheckIssued(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void CheckPrivateKey(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Verify(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ToLegacy(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetIssuerCert(const v8::FunctionCallbackInfo<v8::Value>& args);

  X509* get() { return cert_->get(); }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(X509Certificate)
  SET_SELF_SIZE(X509Certificate)

  class X509CertificateTransferData : public worker::TransferData {
   public:
    explicit X509CertificateTransferData(
        const std::shared_ptr<ManagedX509>& data)
        : data_(data) {}

    BaseObjectPtr<BaseObject> Deserialize(
        Environment* env,
        v8::Local<v8::Context> context,
        std::unique_ptr<worker::TransferData> self) override;

    SET_MEMORY_INFO_NAME(X509CertificateTransferData)
    SET_SELF_SIZE(X509CertificateTransferData)
    SET_NO_MEMORY_INFO()

   private:
    std::shared_ptr<ManagedX509> data_;
  };

  BaseObject::TransferMode GetTransferMode() const override;
  std::unique_ptr<worker::TransferData> CloneForMessaging() const override;

 private:
  X509Certificate(
      Environment* env,
      v8::Local<v8::Object> object,
      std::shared_ptr<ManagedX509> cert,
      STACK_OF(X509)* issuer_chain = nullptr);

  std::shared_ptr<ManagedX509> cert_;
  BaseObjectPtr<X509Certificate> issuer_cert_;
};

}  // namespace crypto
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_CRYPTO_CRYPTO_X509_H_
