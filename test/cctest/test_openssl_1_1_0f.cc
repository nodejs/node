#include "gtest/gtest.h"
#include "libplatform/libplatform.h"
#include "node_crypto_factory.h"
#include "crypto_impl/openssl.h"

using node::crypto::CryptoFactory;
using node::crypto::Crypto;
using node::crypto::OpenSSL;

const char version[] = "openssl_1_1_0f";

TEST(OpenSSL_1_1_0e, Version) {
  EXPECT_EQ(CryptoFactory::Get(version)->Version(), version);
}

TEST(OpenSSL_1_1_0e, Name) {
  EXPECT_EQ(CryptoFactory::Get(version)->Name(), "openssl");
}

TEST(OpenSSL_1_1_0e, HasSNI) {
  EXPECT_TRUE(CryptoFactory::Get(version)->HasSNI());
}

TEST(OpenSSL_1_1_0e, HasNPN) {
  EXPECT_TRUE(CryptoFactory::Get(version)->HasNPN());
}

TEST(OpenSSL_1_1_0e, HasALPN) {
  EXPECT_FALSE(CryptoFactory::Get(version)->HasALPN());
}

TEST(OpenSSL_1_1_0e, HasOCSP) {
  EXPECT_TRUE(CryptoFactory::Get(version)->HasOCSP());
}
