#ifndef SRC_CRYPTO_CRYPTO_TLS_CONTEXT_H_
#define SRC_CRYPTO_CRYPTO_TLS_CONTEXT_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "ncrypto.h"

namespace node {

class Environment;

namespace crypto {

class ByteSource;
class KeyObjectData;

// Return a store that may be modified by this SSL_CTX. If the context uses
// Node's shared root store, replace it with a private copy first.
X509_STORE* GetOrCreateOwnedCertStore(Environment* env,
                                      SSL_CTX* ctx,
                                      X509_STORE** cache = nullptr);

// Attach Node's process-wide default root store to the SSL_CTX.
void UseDefaultRootCertStore(Environment* env, SSL_CTX* ctx);

// Add every PEM certificate in |bio| to the context's certificate store and
// acceptable-client-CA list. Returns the number of certificates read.
size_t AddCACertificates(Environment* env,
                         SSL_CTX* ctx,
                         const ncrypto::BIOPointer& bio,
                         X509_STORE** cache = nullptr);

// Add one PEM CRL and enable CRL checking.
bool AddCRL(Environment* env,
            SSL_CTX* ctx,
            const ncrypto::BIOPointer& bio,
            X509_STORE** cache = nullptr);

enum class PrivateKeyResult {
  kSuccess,
  kParseError,
  kApplyError,
};

PrivateKeyResult UsePrivateKey(SSL_CTX* ctx,
                               const ncrypto::BIOPointer& bio,
                               const ByteSource* passphrase = nullptr);

bool UsePrivateKey(SSL_CTX* ctx, const KeyObjectData& key);

int SSL_CTX_use_certificate_chain(SSL_CTX* ctx,
                                  ncrypto::BIOPointer&& in,
                                  ncrypto::X509Pointer* cert,
                                  ncrypto::X509Pointer* issuer);

}  // namespace crypto
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_CRYPTO_CRYPTO_TLS_CONTEXT_H_
