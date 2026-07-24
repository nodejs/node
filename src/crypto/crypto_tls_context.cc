#include "crypto/crypto_tls_context.h"

#include "crypto/crypto_context.h"
#include "crypto/crypto_keys.h"
#include "crypto/crypto_util.h"

namespace node::crypto {

using ncrypto::DeleteFnPtr;
using ncrypto::EVPKeyPointer;
using ncrypto::X509Pointer;

X509_STORE* GetOrCreateOwnedCertStore(Environment* env,
                                      SSL_CTX* ctx,
                                      X509_STORE** cache) {
  if (cache != nullptr && *cache != nullptr) return *cache;

  X509_STORE* cert_store = SSL_CTX_get_cert_store(ctx);
  if (cert_store == GetOrCreateRootCertStore(env)) {
    cert_store = NewRootCertStore(env);
    SSL_CTX_set_cert_store(ctx, cert_store);
  }

  if (cache != nullptr) *cache = cert_store;
  return cert_store;
}

void UseDefaultRootCertStore(Environment* env, SSL_CTX* ctx) {
  X509_STORE* store = GetOrCreateRootCertStore(env);

  // Increment reference count so global store is not deleted along with CTX.
  X509_STORE_up_ref(store);
  SSL_CTX_set_cert_store(ctx, store);
}

size_t AddCACertificates(Environment* env,
                         SSL_CTX* ctx,
                         const ncrypto::BIOPointer& bio,
                         X509_STORE** cache) {
  if (!bio) return 0;

  size_t count = 0;
  while (X509Pointer x509 = X509Pointer(PEM_read_bio_X509_AUX(
             bio.get(), nullptr, NoPasswordCallback, nullptr))) {
    CHECK_EQ(1,
             X509_STORE_add_cert(GetOrCreateOwnedCertStore(env, ctx, cache),
                                 x509.get()));
    CHECK_EQ(1, SSL_CTX_add_client_CA(ctx, x509.get()));
    count++;
  }
  return count;
}

bool AddCRL(Environment* env,
            SSL_CTX* ctx,
            const ncrypto::BIOPointer& bio,
            X509_STORE** cache) {
  if (!bio) return false;

  DeleteFnPtr<X509_CRL, X509_CRL_free> crl(
      PEM_read_bio_X509_CRL(bio.get(), nullptr, NoPasswordCallback, nullptr));
  if (!crl) return false;

  X509_STORE* cert_store = GetOrCreateOwnedCertStore(env, ctx, cache);
  CHECK_EQ(1, X509_STORE_add_crl(cert_store, crl.get()));
  CHECK_EQ(1,
           X509_STORE_set_flags(
               cert_store, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL));
  return true;
}

PrivateKeyResult UsePrivateKey(SSL_CTX* ctx,
                               const ncrypto::BIOPointer& bio,
                               const ByteSource* passphrase) {
  if (!bio) return PrivateKeyResult::kParseError;

  ByteSource empty_passphrase;
  if (passphrase == nullptr) passphrase = &empty_passphrase;
  // This redirection is necessary because the PasswordCallback expects a
  // pointer to a pointer to the passphrase ByteSource to allow passing in
  // const ByteSources.
  const ByteSource* pass_ptr = passphrase;
  EVPKeyPointer key(
      PEM_read_bio_PrivateKey(bio.get(), nullptr, PasswordCallback, &pass_ptr));
  if (!key) return PrivateKeyResult::kParseError;
  if (!SSL_CTX_use_PrivateKey(ctx, key.get()))
    return PrivateKeyResult::kApplyError;
  return PrivateKeyResult::kSuccess;
}

bool UsePrivateKey(SSL_CTX* ctx, const KeyObjectData& key) {
  if (key.GetKeyType() != KeyType::kKeyTypePrivate) return false;
  return SSL_CTX_use_PrivateKey(ctx, key.GetAsymmetricKey().get());
}

}  // namespace node::crypto
