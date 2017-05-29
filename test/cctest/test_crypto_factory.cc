#include "gtest/gtest.h"
#include "libplatform/libplatform.h"
#include "node_crypto_factory.h"

using node::crypto::CryptoFactory;
using node::crypto::Crypto;

TEST(CryptFactoryDeathTest, GetUnexistingCrypto) {
  ASSERT_EXIT(CryptoFactory::Get("bugus"),
      ::testing::ExitedWithCode(9),
      "node: could not find a crypto implementation for bugus");
}
