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
  X509_STORE* store = node::crypto::NewRootCertStore(nullptr);
  ASSERT_TRUE(store);
  ASSERT_EQ(ERR_peek_error(), 0UL) << "NewRootCertStore should not have left "
                                      "any errors on the OpenSSL error stack\n";
  X509_STORE_free(store);
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
