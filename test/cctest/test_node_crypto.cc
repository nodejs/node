// This simulates specifying the configuration option --openssl-system-ca-path
// and setting it to a file that does not exist.
#define NODE_OPENSSL_SYSTEM_CERT_PATH "/missing/ca.pem"

#include "crypto/crypto_cipher.h"
#include "crypto/crypto_context.h"
#include "gtest/gtest.h"
#include "node_options.h"
#include "openssl/err.h"

#include <climits>

/*
 * This test verifies that a call to NewRootCertDir with the build time
 * configuration option --openssl-system-ca-path set to an missing file, will
 * not leave any OpenSSL errors on the OpenSSL error stack.
 * See https://github.com/nodejs/node/issues/35456 for details.
 */
TEST(NodeCrypto, NewRootCertStore) {
  node::per_process::cli_options->ssl_openssl_cert_store = true;
  X509_STORE* store = node::crypto::NewRootCertStore();
  ASSERT_TRUE(store);
  ASSERT_EQ(ERR_peek_error(), 0UL) << "NewRootCertStore should not have left "
                                      "any errors on the OpenSSL error stack\n";
  X509_STORE_free(store);
}

/*
 * This test verifies that OpenSSL memory tracking constants are properly
 * defined.
 */
TEST(NodeCrypto, MemoryTrackingConstants) {
  // Verify that our memory tracking constants are defined and reasonable
  EXPECT_GT(node::crypto::kSizeOf_SSL_CTX, static_cast<size_t>(0))
      << "SSL_CTX size constant should be positive";
  EXPECT_GT(node::crypto::kSizeOf_X509, static_cast<size_t>(0))
      << "X509 size constant should be positive";
  EXPECT_GT(node::crypto::kSizeOf_EVP_MD_CTX, static_cast<size_t>(0))
      << "EVP_MD_CTX size constant should be positive";

  // Verify reasonable size ranges (basic sanity check)
  EXPECT_LT(node::crypto::kSizeOf_SSL_CTX, static_cast<size_t>(10000))
      << "SSL_CTX size should be reasonable";
  EXPECT_LT(node::crypto::kSizeOf_X509, static_cast<size_t>(10000))
      << "X509 size should be reasonable";
  EXPECT_LT(node::crypto::kSizeOf_EVP_MD_CTX, static_cast<size_t>(1000))
      << "EVP_MD_CTX size should be reasonable";

  // Specific values we expect based on our implementation
  EXPECT_EQ(node::crypto::kSizeOf_SSL_CTX, static_cast<size_t>(240));
  EXPECT_EQ(node::crypto::kSizeOf_X509, static_cast<size_t>(128));
  EXPECT_EQ(node::crypto::kSizeOf_EVP_MD_CTX, static_cast<size_t>(48));
}

TEST(NodeCrypto, TryGetIntCipherOutputLength) {
  int output_len = 0;

  EXPECT_TRUE(
      node::crypto::TryGetIntCipherOutputLength(INT_MAX - 16, 16, &output_len));
  EXPECT_EQ(output_len, INT_MAX);

  EXPECT_FALSE(
      node::crypto::TryGetIntCipherOutputLength(INT_MAX - 15, 16, &output_len));

  EXPECT_FALSE(node::crypto::TryGetIntCipherOutputLength(
      0, static_cast<size_t>(INT_MAX) + 1, &output_len));
}
