#include "gtest/gtest.h"
#include "libplatform/libplatform.h"
#include "node_crypto_factory.h"
#include "crypto_impl/openssl/openssl.h"

using node::crypto::CryptoFactory;
using node::crypto::Crypto;
using node::crypto::OpenSSL;

const char version[] = "openssl_1_0_2e";

TEST(OpenSSL_1_0_2e, Version) {
  EXPECT_EQ(CryptoFactory::Get(version)->Version(), "1.0.2e");
}

TEST(OpenSSL_1_0_2e, Name) {
  EXPECT_EQ(CryptoFactory::Get(version)->Name(), "openssl");
}

TEST(OpenSSL_1_0_2e, HasSNI) {
  EXPECT_TRUE(CryptoFactory::Get(version)->HasSNI());
}

TEST(OpenSSL_1_0_2e, HasNPN) {
  EXPECT_TRUE(CryptoFactory::Get(version)->HasNPN());
}

TEST(OpenSSL_1_0_2e, HasALPN) {
  EXPECT_FALSE(CryptoFactory::Get(version)->HasALPN());
}

TEST(OpenSSL_1_0_2e, HasOCSP) {
  EXPECT_TRUE(CryptoFactory::Get(version)->HasOCSP());
}
