#include "gtest/gtest.h"
#include "libplatform/libplatform.h"
#include "node_crypto_factory.h"
#include "crypto_impl/openssl.h"

using node::crypto::CryptoFactory;
using node::crypto::Crypto;
using node::crypto::OpenSSL;

const char version[] = "openssl_1_0_2e";

TEST(CryptFactoryTest, Version) {
  EXPECT_EQ(CryptoFactory::Get(version)->Version(), version);
}

TEST(CryptFactoryTest, Name) {
  EXPECT_EQ(CryptoFactory::Get(version)->Name(), "openssl");
}

TEST(CryptFactoryTest, HasSNI) {
  EXPECT_TRUE(CryptoFactory::Get(version)->HasSNI());
}

TEST(CryptFactoryTest, HasNPN) {
  EXPECT_TRUE(CryptoFactory::Get(version)->HasNPN());
}

TEST(CryptFactoryTest, HasALPN) {
  EXPECT_FALSE(CryptoFactory::Get(version)->HasALPN());
}

TEST(CryptFactoryTest, HasOCSP) {
  EXPECT_TRUE(CryptoFactory::Get(version)->HasOCSP());
}
