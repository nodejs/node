// This simulates specifying the configuration option --openssl-system-ca-path
// and setting it to a file that does not exist.
#define NODE_OPENSSL_SYSTEM_CERT_PATH "/missing/ca.pem"

#include "crypto/crypto_cipher.h"
#include "crypto/crypto_context.h"
#include "gtest/gtest.h"
#include "node_options.h"
#include "openssl/err.h"

#include <climits>

using ncrypto::Ec;
using ncrypto::EVPKeyCtxPointer;
using ncrypto::EVPKeyPointer;
using ncrypto::KeyAlgorithm;

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

TEST(NodeCrypto, KeyAlgorithmNames) {
  EXPECT_EQ(KeyAlgorithm::FromName("rsa-pss"), &KeyAlgorithm::RSA_PSS);
  EXPECT_EQ(KeyAlgorithm::FromName("ML-DSA-44"), &KeyAlgorithm::ML_DSA_44);
  EXPECT_EQ(KeyAlgorithm::FromName("unknown-key-algorithm"), nullptr);
  EXPECT_EQ(KeyAlgorithm::FromName(nullptr), nullptr);
  EXPECT_FALSE(EVPKeyPointer().isA(KeyAlgorithm::RSA));
  EXPECT_FALSE(EVPKeyPointer::isA(nullptr, KeyAlgorithm::RSA));
  auto empty = EVPKeyPointer::New();
  ASSERT_TRUE(empty);
  EXPECT_FALSE(empty.isA(KeyAlgorithm::RSA));
  EXPECT_FALSE(empty.supportsContextString());
  EXPECT_FALSE(EVPKeyPointer().supportsContextString());
  EXPECT_FALSE(empty.rawSeed());
  EXPECT_FALSE(EVPKeyPointer().rawSeed());
  EXPECT_FALSE(empty.isA("unknown-key-algorithm"));
  EXPECT_FALSE(empty.isA(static_cast<const char*>(nullptr)));
}

TEST(NodeCrypto, UnsupportedRawExports) {
  using Error = EVPKeyPointer::RawExportError;
  EVPKeyPointer key;
  for (int i = 0; i < 2; i++) {
    const auto seed = key.rawSeed();
    EXPECT_FALSE(seed);
    EXPECT_EQ(seed.error, Error::UNSUPPORTED_KEY_TYPE);
    const auto jwk = key.exportRawJwk(/* include_private */ false);
    EXPECT_FALSE(jwk);
    EXPECT_EQ(jwk.error, Error::UNSUPPORTED_KEY_TYPE);
    key = EVPKeyPointer::New();
    ASSERT_TRUE(key);
  }
  auto ctx = EVPKeyCtxPointer::NewFromAlgorithm(KeyAlgorithm::RSA);
  ASSERT_TRUE(ctx);
  ASSERT_EQ(EVP_PKEY_keygen_init(ctx.get()), 1);
  EVP_PKEY* raw = nullptr;
  ASSERT_EQ(EVP_PKEY_keygen(ctx.get(), &raw), 1);
  key.reset(raw);
  EXPECT_EQ(key.rawSeed().error, Error::UNSUPPORTED_KEY_TYPE);
  EXPECT_EQ(key.exportRawJwk(/* include_private */ false).error,
            Error::UNSUPPORTED_KEY_TYPE);
}

namespace {
void CheckRawJwkImport(const EVPKeyPointer& key) {
  auto jwk = key.exportRawJwk(/* include_private */ true);
  ASSERT_TRUE(jwk);
  const auto& data = jwk.value;
  const ncrypto::Buffer<const unsigned char> pub = data.public_key;
  const ncrypto::Buffer<const unsigned char> priv = data.private_key;
  auto private_key = EVPKeyPointer::NewRawJwk(*data.algorithm, pub, priv);
  ASSERT_TRUE(private_key);
  auto exported = private_key.exportRawJwk(/* include_private */ true);
  ASSERT_TRUE(exported);
  EXPECT_EQ(exported.value.algorithm, data.algorithm);
  ASSERT_EQ(exported.value.private_key.size(), priv.len);
  EXPECT_EQ(memcmp(exported.value.private_key.get(), priv.data, priv.len), 0);
  auto public_key = EVPKeyPointer::NewRawJwk(*data.algorithm, pub);
  ASSERT_TRUE(public_key);
  auto exported_public = public_key.exportRawJwk(/* include_private */ false);
  ASSERT_TRUE(exported_public);
  EXPECT_FALSE(exported_public.value.private_key);
  ASSERT_EQ(exported_public.value.public_key.size(), pub.len);
  EXPECT_EQ(memcmp(exported_public.value.public_key.get(), pub.data, pub.len),
            0);

  auto different_pub = ncrypto::DataPointer::Copy({pub.data, pub.len});
  ASSERT_TRUE(different_pub);
  ASSERT_GT(different_pub.size(), 0u);
  different_pub.get<unsigned char>()[0] ^= 1;
  const ncrypto::Buffer<const unsigned char> mismatch = different_pub;
  EXPECT_FALSE(EVPKeyPointer::NewRawJwk(*data.algorithm, mismatch, priv));
  EXPECT_FALSE(EVPKeyPointer::NewRawJwk(
      *data.algorithm, pub, ncrypto::Buffer<const unsigned char>{nullptr, 0}));
  EXPECT_FALSE(EVPKeyPointer::NewRawJwk(KeyAlgorithm::RSA, pub, priv));
}
}  // namespace

TEST(NodeCrypto, RsaPrimeProduct) {
  ncrypto::ClearErrorOnReturn clear_errors;
  EXPECT_FALSE(ncrypto::Rsa().checkPrimeProduct());
  auto ctx = EVPKeyCtxPointer::NewFromAlgorithm(KeyAlgorithm::RSA);
  ASSERT_TRUE(ctx);
  ASSERT_TRUE(ctx.initForKeygen());
  EVP_PKEY* raw = nullptr;
  ASSERT_EQ(EVP_PKEY_keygen(ctx.get(), &raw), 1);
  EVPKeyPointer key(raw);
  ncrypto::Rsa rsa = key;
  ASSERT_TRUE(rsa);
  EXPECT_TRUE(rsa.checkPrimeProduct());
  auto pub = rsa.getPublicKey();
  ncrypto::BignumPointer n(BN_dup(pub.n));
  ncrypto::BignumPointer e(BN_dup(pub.e));
  ASSERT_TRUE(n);
  ASSERT_TRUE(e);
  ASSERT_EQ(BN_add_word(n.get(), 1), 1);
  ASSERT_TRUE(rsa.setPublicKey(std::move(n), std::move(e)));
  EXPECT_FALSE(rsa.checkPrimeProduct());
}

TEST(NodeCrypto, EcKeyComponents) {
  ncrypto::ClearErrorOnReturn clear_errors;
  ncrypto::BignumPointer x;
  ncrypto::BignumPointer y;
  ncrypto::BignumPointer priv;
  int degree = 0;
  EXPECT_FALSE(
      ncrypto::Ec::GetKeyComponents(EVPKeyPointer(), &x, &y, &priv, &degree));
  EXPECT_EQ(ncrypto::Ec::GetCurveId(EVPKeyPointer()), NID_undef);
  EXPECT_FALSE(ncrypto::Ec::TryExportPublic(EVPKeyPointer(),
                                            POINT_CONVERSION_UNCOMPRESSED));
  EXPECT_FALSE(ncrypto::Ec::ExportPrivate(EVPKeyPointer()));
  auto empty = EVPKeyPointer::New();
  ASSERT_TRUE(empty);
  EXPECT_FALSE(ncrypto::Ec::GetKeyComponents(empty, &x, &y, &priv, &degree));
  for (int nid : {NID_X9_62_prime256v1, NID_secp384r1, NID_secp521r1}) {
    auto ec = ncrypto::ECKeyPointer::NewByCurveName(nid);
    ASSERT_TRUE(ec);
    EXPECT_FALSE(ec.checkPrivateKey());
    auto parameters = EVPKeyPointer::New();
    ASSERT_TRUE(parameters);
    ASSERT_TRUE(parameters.set(ec));
    EXPECT_EQ(ncrypto::Ec::GetCurveId(parameters), nid);
    EXPECT_FALSE(
        ncrypto::Ec::GetKeyComponents(parameters, &x, &y, nullptr, &degree));
    ASSERT_TRUE(ec.generate());
    EXPECT_TRUE(ec.checkPrivateKey());
    auto key = EVPKeyPointer::New();
    ASSERT_TRUE(key);
    ASSERT_TRUE(key.set(ec));
    EXPECT_TRUE(key.isA(KeyAlgorithm::EC));
    EXPECT_EQ(ncrypto::Ec::GetCurveId(key), nid);
    ASSERT_TRUE(ncrypto::Ec::GetKeyComponents(key, &x, &y, &priv, &degree));
    EXPECT_EQ(degree, EC_GROUP_get_degree(ec.getGroup()));
    EXPECT_EQ(BN_cmp(priv.get(), ec.getPrivateKey()), 0);
    const size_t width = (degree + 7) / 8;
    auto point = ncrypto::DataPointer::Alloc(1 + 2 * width);
    ASSERT_TRUE(point);
    ASSERT_EQ(EC_POINT_point2oct(ec.getGroup(),
                                 ec.getPublicKey(),
                                 POINT_CONVERSION_UNCOMPRESSED,
                                 point.get<unsigned char>(),
                                 point.size(),
                                 nullptr),
              point.size());
    auto x_bytes = x.encodePadded(width);
    auto y_bytes = y.encodePadded(width);
    ASSERT_TRUE(x_bytes);
    ASSERT_TRUE(y_bytes);
    EXPECT_EQ(memcmp(x_bytes.get(), point.get<unsigned char>() + 1, width), 0);
    EXPECT_EQ(
        memcmp(y_bytes.get(), point.get<unsigned char>() + 1 + width, width),
        0);
    auto raw_private = ncrypto::Ec::ExportPrivate(key);
    auto expected_private = priv.encodePadded(width);
    ASSERT_TRUE(raw_private);
    ASSERT_EQ(raw_private.size(), expected_private.size());
    EXPECT_EQ(
        memcmp(raw_private.get(), expected_private.get(), raw_private.size()),
        0);
    auto raw_public =
        ncrypto::Ec::TryExportPublic(key, POINT_CONVERSION_UNCOMPRESSED);
#if NCRYPTO_USE_OPENSSL3_PROVIDER
    ASSERT_TRUE(raw_public);
    ASSERT_EQ(raw_public.size(), point.size());
    EXPECT_EQ(memcmp(raw_public.get(), point.get(), point.size()), 0);
#else
    EXPECT_FALSE(raw_public);
#endif
    EXPECT_FALSE(
        ncrypto::Ec::TryExportPublic(key, POINT_CONVERSION_COMPRESSED));
    auto public_ec = ncrypto::ECKeyPointer::NewByCurveName(nid);
    ASSERT_TRUE(public_ec);
    ASSERT_TRUE(public_ec.setPublicKeyRaw(x, y));
    auto public_key = EVPKeyPointer::New();
    ASSERT_TRUE(public_key);
    ASSERT_TRUE(public_key.set(public_ec));
    EXPECT_TRUE(
        ncrypto::Ec::GetKeyComponents(public_key, &x, &y, nullptr, &degree));
    EXPECT_FALSE(
        ncrypto::Ec::GetKeyComponents(public_key, &x, &y, &priv, &degree));
    EXPECT_FALSE(ncrypto::Ec::ExportPrivate(public_key));
  }
}

TEST(NodeCrypto, ResolveKeyAlgorithm) {
  ncrypto::ClearErrorOnReturn clear_errors;
  EXPECT_EQ(EVPKeyPointer().getAlgorithm(), nullptr);
  auto key = EVPKeyPointer::New();
  ASSERT_TRUE(key);
  EXPECT_EQ(key.getAlgorithm(), nullptr);

  const unsigned char seed[64] = {};
  for (const auto* algorithm : {&KeyAlgorithm::ED25519,
                                &KeyAlgorithm::X25519,
                                &KeyAlgorithm::ML_DSA_44,
                                &KeyAlgorithm::ML_KEM_768}) {
    if (!algorithm->isAvailable()) continue;
    const ncrypto::Buffer<const unsigned char> input{
        seed, algorithm->seedSize() == 0 ? 32 : algorithm->seedSize()};
    key = algorithm->seedSize() == 0
              ? EVPKeyPointer::NewRawPrivate(*algorithm, input)
              : EVPKeyPointer::NewRawSeed(*algorithm, input);
    ASSERT_TRUE(key);
    EXPECT_EQ(key.getAlgorithm(), algorithm);
    CheckRawJwkImport(key);
    if (algorithm->seedSize() != 0) {
      auto exported = key.rawSeed();
      ASSERT_TRUE(exported);
      ASSERT_EQ(exported.value.size(), input.len);
      EXPECT_EQ(memcmp(exported.value.get(), input.data, input.len), 0);
    }
    EVPKeyPointer moved(std::move(key));
    EXPECT_EQ(key.getAlgorithm(), nullptr);
    EXPECT_EQ(moved.getAlgorithm(), algorithm);
    key.reset(moved.release());
    EXPECT_EQ(moved.getAlgorithm(), nullptr);
    EXPECT_EQ(key.getAlgorithm(), algorithm);
    key.reset();
    EXPECT_EQ(key.getAlgorithm(), nullptr);
  }
#if NCRYPTO_USE_OPENSSL3_PROVIDER
  // Legacy keys must resolve even without provider-backed key material.
  key = EVPKeyPointer::New();
  ASSERT_EQ(EVP_PKEY_set_type(key.get(), NID_rsaEncryption), 1);
  EXPECT_EQ(key.getAlgorithm(), &KeyAlgorithm::RSA);
#endif
}

TEST(NodeCrypto, PublicKeyTypeNames) {
  EXPECT_EQ(EVPKeyPointer().getKeyTypeName(), nullptr);
  EXPECT_EQ(EVPKeyPointer::New().getKeyTypeName(), nullptr);
  EXPECT_STREQ(KeyAlgorithm::RSA.keyTypeName(), "rsa");
  EXPECT_STREQ(KeyAlgorithm::RSA_PSS.keyTypeName(), "rsa-pss");
  EXPECT_STREQ(KeyAlgorithm::ML_DSA_44.keyTypeName(), "ml-dsa-44");
  EXPECT_STREQ(KeyAlgorithm::SLH_DSA_SHAKE_256S.keyTypeName(),
               "slh-dsa-shake-256s");
  EXPECT_EQ(KeyAlgorithm::SM2.keyTypeName(), nullptr);
  const unsigned char seed[32] = {};
  auto key =
      EVPKeyPointer::NewRawPrivate(KeyAlgorithm::ED25519, {seed, sizeof(seed)});
  ASSERT_TRUE(key);
  EXPECT_STREQ(key.getKeyTypeName(), "ed25519");
}

#if NCRYPTO_USE_OPENSSL3_PROVIDER
TEST(NodeCrypto, LegacyKeyAlgorithmResolution) {
  ncrypto::ClearErrorOnReturn clear_errors;
  auto key = EVPKeyPointer::New();
  ASSERT_TRUE(key);
  ASSERT_EQ(EVP_PKEY_set_type(key.get(), NID_rsassaPss), 1);
  EXPECT_EQ(key.getAlgorithm(), &KeyAlgorithm::RSA_PSS);
  EXPECT_STREQ(key.getKeyTypeName(), "rsa-pss");
#ifndef OPENSSL_NO_SM2
  ASSERT_EQ(EVP_PKEY_set_type(key.get(), NID_sm2), 1);
  EXPECT_TRUE(key.isA(KeyAlgorithm::SM2));
  EXPECT_FALSE(key.isA(KeyAlgorithm::EC));
  EXPECT_EQ(key.getAlgorithm(), &KeyAlgorithm::SM2);
  EXPECT_EQ(key.getKeyTypeName(), nullptr);
#endif
}
#endif

TEST(NodeCrypto, NamedKeysAndEcCurves) {
  EXPECT_EQ(Ec::GetNamedKeyAlgorithm("ED25519"), &KeyAlgorithm::ED25519);
  EXPECT_EQ(Ec::GetNamedKeyAlgorithm("X25519"), &KeyAlgorithm::X25519);
  EXPECT_EQ(Ec::GetNamedKeyAlgorithm("P-256"), nullptr);
  EXPECT_EQ(Ec::GetNamedKeyAlgorithm("prime256v1"), nullptr);
  EXPECT_EQ(Ec::GetNamedKeyAlgorithm("unknown-curve"), nullptr);
  EXPECT_EQ(Ec::GetCurveIdFromName("P-256"),
            Ec::GetCurveIdFromName("prime256v1"));
}

TEST(NodeCrypto, NamedRawKey) {
  const unsigned char seed[32] = {};
  const ncrypto::Buffer<const unsigned char> input{seed, sizeof(seed)};
  auto key = EVPKeyPointer::NewRawPrivate(KeyAlgorithm::ED25519, input);
  ASSERT_TRUE(key);
  EXPECT_TRUE(key.isA(KeyAlgorithm::ED25519));
  EXPECT_FALSE(key.isA(KeyAlgorithm::X25519));
  ASSERT_EQ(key.getAlgorithm(), &KeyAlgorithm::ED25519);
  EXPECT_TRUE(key.getAlgorithm()->isOneShot());
  EXPECT_TRUE(key.supportsRawPublic());
  EXPECT_TRUE(key.supportsRawPrivate());
  EXPECT_EQ(key.getAlgorithm()->seedSize(), 0);
#if NCRYPTO_USE_OPENSSL3_PROVIDER
  // Provider aliases are recognized without comparing the primary type name.
  EXPECT_TRUE(key.isA("1.3.101.112"));
  auto ctx = ncrypto::EVPKeyCtxPointer::NewFromName("1.3.101.112");
  ASSERT_TRUE(ctx);
  ASSERT_TRUE(ctx.initForKeygen());
  EVP_PKEY* generated = nullptr;
  ASSERT_EQ(EVP_PKEY_keygen(ctx.get(), &generated), 1);
  EXPECT_TRUE(EVPKeyPointer(generated).isA(KeyAlgorithm::ED25519));
#endif
  auto pub = key.rawPublicKey();
  ASSERT_TRUE(pub);
  auto imported = EVPKeyPointer::NewRawPublic(KeyAlgorithm::ED25519, pub);
  ASSERT_TRUE(imported);
  EXPECT_TRUE(imported.isA(KeyAlgorithm::ED25519));
  auto exported = imported.rawPublicKey();
  ASSERT_EQ(pub.size(), exported.size());
  EXPECT_EQ(memcmp(pub.get(), exported.get(), pub.size()), 0);
}

TEST(NodeCrypto, NamedRsaPssKey) {
  ncrypto::ClearErrorOnReturn clear_errors;
  EXPECT_FALSE(EVPKeyCtxPointer::NewFromName("unknown-key-algorithm"));
  EXPECT_FALSE(EVPKeyCtxPointer::NewFromName(nullptr));
#ifndef OPENSSL_IS_BORINGSSL
  auto ctx = EVPKeyCtxPointer::NewFromAlgorithm(KeyAlgorithm::RSA_PSS);
  ASSERT_TRUE(ctx);
  ASSERT_TRUE(ctx.initForKeygen());
  ASSERT_TRUE(ctx.setRsaKeygenBits(2048));
  EVP_PKEY* raw = nullptr;
  ASSERT_EQ(EVP_PKEY_keygen(ctx.get(), &raw), 1);
  EVPKeyPointer key(raw);
  EXPECT_TRUE(key.isA(KeyAlgorithm::RSA_PSS));
  EXPECT_FALSE(key.isA(KeyAlgorithm::RSA));
  EXPECT_FALSE(key.supportsContextString());
  EXPECT_TRUE(key.isRsaVariant());
  EXPECT_EQ(key.getDefaultSignPadding(), RSA_PKCS1_PSS_PADDING);
  EXPECT_FALSE(key.supportsRawPublic());
  ASSERT_EQ(key.getAlgorithm(), &KeyAlgorithm::RSA_PSS);
  EXPECT_FALSE(key.getAlgorithm()->isOneShot());
#endif
}

TEST(NodeCrypto, ProviderPqcKeyWithoutLegacyId) {
  ncrypto::ClearErrorOnReturn clear_errors;
  if (!KeyAlgorithm::ML_DSA_44.isAvailable() ||
      !KeyAlgorithm::ML_KEM_768.isAvailable())
    GTEST_SKIP();
  const unsigned char seed[64] = {};
  for (const auto* algorithm :
       {&KeyAlgorithm::ML_DSA_44, &KeyAlgorithm::ML_KEM_768}) {
    const ncrypto::Buffer<const unsigned char> input{seed,
                                                     algorithm->seedSize()};
    auto key = EVPKeyPointer::NewRawSeed(*algorithm, input);
    ASSERT_TRUE(key);
#if NCRYPTO_USE_OPENSSL3_PROVIDER
    EXPECT_EQ(EVP_PKEY_id(key.get()), -1);
#endif
    EXPECT_TRUE(key.isA(*algorithm));
    ASSERT_EQ(key.getAlgorithm(), algorithm);
    EXPECT_EQ(key.getAlgorithm()->seedSize(), input.len);
    EXPECT_TRUE(key.supportsRawPublic());
    EXPECT_FALSE(key.supportsRawPrivate());
    auto exported_seed = key.rawSeed();
    ASSERT_TRUE(exported_seed);
    ASSERT_EQ(exported_seed.value.size(), input.len);
    EXPECT_EQ(memcmp(exported_seed.value.get(), seed, input.len), 0);
    auto pub = key.rawPublicKey();
    ASSERT_TRUE(pub);
    auto imported = EVPKeyPointer::NewRawPublic(*algorithm, pub);
    ASSERT_TRUE(imported);
    EXPECT_EQ(imported.getAlgorithm(), algorithm);

    auto private_jwk = key.exportRawJwk(/* include_private */ true);
    ASSERT_TRUE(private_jwk);
    EXPECT_EQ(private_jwk.value.algorithm, algorithm);
    ASSERT_EQ(private_jwk.value.private_key.size(), input.len);
    EXPECT_EQ(memcmp(private_jwk.value.private_key.get(), seed, input.len), 0);
    ASSERT_EQ(private_jwk.value.public_key.size(), pub.size());
    EXPECT_EQ(memcmp(private_jwk.value.public_key.get(), pub.get(), pub.size()),
              0);
    auto public_jwk = imported.exportRawJwk(/* include_private */ false);
    ASSERT_TRUE(public_jwk);
    EXPECT_EQ(public_jwk.value.algorithm, algorithm);
    EXPECT_FALSE(public_jwk.value.private_key);
    ASSERT_EQ(public_jwk.value.public_key.size(), pub.size());
    EXPECT_EQ(memcmp(public_jwk.value.public_key.get(), pub.get(), pub.size()),
              0);
    EXPECT_EQ(imported.rawSeed().error,
              EVPKeyPointer::RawExportError::MISSING_SEED);
    EXPECT_EQ(imported.exportRawJwk(/* include_private */ true).error,
              EVPKeyPointer::RawExportError::MISSING_SEED);
  }
}

TEST(NodeCrypto, PqcJwkRawPrivateExport) {
  ncrypto::ClearErrorOnReturn clear_errors;
  const auto& algorithm = KeyAlgorithm::SLH_DSA_SHA2_128S;
  if (!algorithm.isAvailable()) GTEST_SKIP();
  auto ctx = EVPKeyCtxPointer::NewFromAlgorithm(algorithm);
  ASSERT_TRUE(ctx);
  ASSERT_EQ(EVP_PKEY_keygen_init(ctx.get()), 1);
  EVP_PKEY* raw = nullptr;
  ASSERT_EQ(EVP_PKEY_keygen(ctx.get(), &raw), 1);
  EVPKeyPointer key(raw);
  CheckRawJwkImport(key);
  auto priv = key.rawPrivateKey();
  auto pub = key.rawPublicKey();
  ASSERT_TRUE(priv);
  ASSERT_TRUE(pub);
  auto jwk = key.exportRawJwk(/* include_private */ true);
  ASSERT_TRUE(jwk);
  EXPECT_EQ(jwk.value.algorithm, &algorithm);
  ASSERT_EQ(jwk.value.private_key.size(), priv.size());
  EXPECT_EQ(memcmp(jwk.value.private_key.get(), priv.get(), priv.size()), 0);
  ASSERT_EQ(jwk.value.public_key.size(), pub.size());
  EXPECT_EQ(memcmp(jwk.value.public_key.get(), pub.get(), pub.size()), 0);
  EXPECT_EQ(key.rawSeed().error,
            EVPKeyPointer::RawExportError::UNSUPPORTED_KEY_TYPE);
}

#ifdef OPENSSL_IS_BORINGSSL
TEST(NodeCrypto, UnavailableBoringSSLKeyAlgorithms) {
  ncrypto::ClearErrorOnReturn clear_errors;
  for (const auto* algorithm :
       {&KeyAlgorithm::SM2, &KeyAlgorithm::X448, &KeyAlgorithm::ED448}) {
    EXPECT_FALSE(algorithm->isAvailable());
    EXPECT_FALSE(EVPKeyCtxPointer::NewFromAlgorithm(*algorithm));
  }
}
#endif
