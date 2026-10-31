#ifndef SRC_CRYPTO_CRYPTO_X509_H_
#define SRC_CRYPTO_CRYPTO_X509_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "crypto/crypto_util.h"
#include "env.h"
#include "memory_tracker.h"
#include "ncrypto.h"
#include "node_worker.h"
#include "v8.h"

#include <memory>
#include <optional>
#include <vector>

namespace node {
namespace crypto {

// ManagedX509 owns one isolate-local X509. Adopt makes the ownership transfer
// explicit at each call site.
class ManagedX509 final : public MemoryRetainer {
 public:
  static std::shared_ptr<ManagedX509> Adopt(ncrypto::X509Pointer cert);

  ManagedX509(const ManagedX509&) = delete;
  ManagedX509& operator=(const ManagedX509&) = delete;

  inline operator bool() const { return !!cert_; }
  inline X509* get() { return cert_.get(); }
  inline ncrypto::X509View view() const { return cert_; }
  inline operator ncrypto::X509View() const { return cert_; }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(ManagedX509)
  SET_SELF_SIZE(ManagedX509)

 private:
  explicit ManagedX509(ncrypto::X509Pointer cert);

  ncrypto::X509Pointer cert_;
};

// An immutable certificate-encoding snapshot. Certificate zero is the leaf and
// all remaining certificates follow in the supplied order. Sharing this
// snapshot between isolates is safe; the isolate-local X509 objects are always
// reparsed from it.
class X509CertificateSnapshot final : public MemoryRetainer {
 public:
  using DER = std::vector<unsigned char>;

  static std::shared_ptr<const X509CertificateSnapshot> Create(
      ncrypto::X509View leaf,
      const STACK_OF(X509) * issuer_chain = nullptr,
      OPENSSL_SIZE_T issuers_start = 0);

  ncrypto::X509Pointer Parse(size_t index) const;

  inline size_t size() const { return certificates_.size(); }
  inline const DER& at(size_t index) const { return certificates_.at(index); }
  size_t size_bytes() const;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(X509CertificateSnapshot)
  SET_SELF_SIZE(X509CertificateSnapshot)

 private:
  explicit X509CertificateSnapshot(std::vector<DER> certificates);

  const std::vector<DER> certificates_;
};

class X509Certificate final : public BaseObject {
 public:
  enum class GetPeerCertificateFlag {
    NONE,
    SERVER
  };

  static void Initialize(Environment* env, v8::Local<v8::Object> target);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static bool HasInstance(Environment* env, v8::Local<v8::Value> value);

  static void CreateX509CertificateClass(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void IsX509Certificate(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void NewFromHandle(const v8::FunctionCallbackInfo<v8::Value>& args);

  static v8::MaybeLocal<v8::Object> New(
      Environment* env,
      ncrypto::X509Pointer cert,
      const STACK_OF(X509) * issuer_chain = nullptr,
      OPENSSL_SIZE_T issuers_start = 0);

  static v8::MaybeLocal<v8::Object> New(
      Environment* env,
      std::shared_ptr<ManagedX509> cert,
      std::shared_ptr<const X509CertificateSnapshot> snapshot,
      size_t snapshot_index = 0);

  static v8::MaybeLocal<v8::Object> New(
      Environment* env,
      std::shared_ptr<const X509CertificateSnapshot> snapshot,
      size_t snapshot_index = 0);

  static v8::MaybeLocal<v8::Object> GetCert(Environment* env,
                                            const ncrypto::SSLPointer& ssl);

  static v8::MaybeLocal<v8::Object> GetPeerCert(Environment* env,
                                                const ncrypto::SSLPointer& ssl,
                                                GetPeerCertificateFlag flag);

  static v8::Local<v8::Object> Wrap(Environment* env,
                                    v8::Local<v8::Object> object,
                                    ncrypto::X509Pointer cert);

  inline BaseObjectPtr<X509Certificate> getIssuerCert() const {
    return issuer_cert_;
  }

  inline ncrypto::X509View view() const { return *cert_; }
  inline X509* get() { return cert_->get(); }
  inline const X509CertificateSnapshot::DER& snapshot_der() const {
    return snapshot_->at(snapshot_index_);
  }

  v8::MaybeLocal<v8::Value> toObject(Environment* env);
  static v8::MaybeLocal<v8::Value> toObject(Environment* env,
                                            const ncrypto::X509View& cert);

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(X509Certificate)
  SET_SELF_SIZE(X509Certificate)

  class X509CertificateTransferData : public worker::TransferData {
   public:
    explicit X509CertificateTransferData(
        std::shared_ptr<const X509CertificateSnapshot> snapshot,
        size_t snapshot_index)
        : snapshot_(std::move(snapshot)), snapshot_index_(snapshot_index) {}

    BaseObjectPtr<BaseObject> Deserialize(
        Environment* env,
        v8::Local<v8::Context> context,
        std::unique_ptr<worker::TransferData> self) override;

    void MemoryInfo(MemoryTracker* tracker) const override;
    SET_MEMORY_INFO_NAME(X509CertificateTransferData)
    SET_SELF_SIZE(X509CertificateTransferData)

   private:
    std::shared_ptr<const X509CertificateSnapshot> snapshot_;
    size_t snapshot_index_;
  };

  BaseObject::TransferMode GetTransferMode() const override;
  std::unique_ptr<worker::TransferData> CloneForMessaging() const override;

 private:
  X509Certificate(Environment* env,
                  v8::Local<v8::Object> object,
                  std::shared_ptr<ManagedX509> cert,
                  std::shared_ptr<const X509CertificateSnapshot> snapshot,
                  size_t snapshot_index,
                  v8::Local<v8::Object> issuer_chain = v8::Local<v8::Object>());

  std::shared_ptr<ManagedX509> cert_;
  std::shared_ptr<const X509CertificateSnapshot> snapshot_;
  size_t snapshot_index_ = 0;
  BaseObjectPtr<X509Certificate> issuer_cert_;
};

inline X509Certificate::GetPeerCertificateFlag operator|(
    X509Certificate::GetPeerCertificateFlag lhs,
    X509Certificate::GetPeerCertificateFlag rhs) {
  return static_cast<X509Certificate::GetPeerCertificateFlag>(
      static_cast<int>(lhs) | static_cast<int>(rhs));
}

inline X509Certificate::GetPeerCertificateFlag operator&(
    X509Certificate::GetPeerCertificateFlag lhs,
    X509Certificate::GetPeerCertificateFlag rhs) {
  return static_cast<X509Certificate::GetPeerCertificateFlag>(
      static_cast<int>(lhs) & static_cast<int>(rhs));
}

}  // namespace crypto
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_CRYPTO_CRYPTO_X509_H_
