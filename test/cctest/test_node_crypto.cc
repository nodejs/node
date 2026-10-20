// This simulates specifying the configuration option --openssl-system-ca-path
// and setting it to a file that does not exist.
#define NODE_OPENSSL_SYSTEM_CERT_PATH "/missing/ca.pem"

#include "crypto/crypto_cipher.h"
#include "crypto/crypto_context.h"
#include "gtest/gtest.h"
#include "node_options.h"
#include "openssl/err.h"
#include "util.h"

#include <climits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#if NCRYPTO_USE_OPENSSL3_PROVIDER
#include <openssl/core_dispatch.h>
#include <openssl/core_names.h>
#include <openssl/params.h>
#include <openssl/provider.h>
#endif

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

#if NCRYPTO_USE_OPENSSL3_PROVIDER
TEST(NodeCrypto, ProviderPkcs1PublicKeyImport) {
  ncrypto::ClearErrorOnReturn clear_errors;
  auto ctx = EVPKeyCtxPointer::NewFromAlgorithm(KeyAlgorithm::RSA);
  ASSERT_TRUE(ctx);
  ASSERT_TRUE(ctx.initForKeygen());
  ASSERT_TRUE(ctx.setRsaKeygenBits(2048));
  EVP_PKEY* raw = nullptr;
  ASSERT_EQ(EVP_PKEY_keygen(ctx.get(), &raw), 1);
  EVPKeyPointer key(raw);

  for (const auto format :
       {EVPKeyPointer::PKFormatType::PEM, EVPKeyPointer::PKFormatType::DER}) {
    const EVPKeyPointer::PublicKeyEncodingConfig config(
        false, format, EVPKeyPointer::PKEncodingType::PKCS1);
    auto encoded = key.writePublicKey(config);
    ASSERT_TRUE(encoded);
    const BUF_MEM* mem = encoded.value;
    ASSERT_NE(mem, nullptr);
    const ncrypto::Buffer<const unsigned char> input{
        reinterpret_cast<const unsigned char*>(mem->data), mem->length};
    auto imported = EVPKeyPointer::TryParsePublicKey(config, input);
    ASSERT_TRUE(imported);
    EXPECT_NE(EVP_PKEY_get0_provider(imported.value.get()), nullptr);
    EXPECT_TRUE(imported.value.isA(KeyAlgorithm::RSA));
    EXPECT_EQ(EVP_PKEY_eq(key.get(), imported.value.get()), 1);
  }
}

namespace {
struct RsaLoadTestContext {
  OSSL_FUNC_BIO_read_ex_fn* read = nullptr;
  int selection = 0;
  int loads = 0;
  int frees = 0;
};

void* RsaLoadTestDecoderNew(void* context) {
  return context;
}

void RsaLoadTestDecoderFree(void*) {}

int RsaLoadTestDecode(void* context,
                      OSSL_CORE_BIO* input,
                      int selection,
                      OSSL_CALLBACK* callback,
                      void* arg,
                      OSSL_PASSPHRASE_CALLBACK*,
                      void*) {
  auto* state = static_cast<RsaLoadTestContext*>(context);
  unsigned char sentinel = 0;
  size_t size = 0;
  // Only exercise construction and reference ownership, not ASN.1 parsing.
  if (!state->read(input, &sentinel, 1, &size) || size != 1 ||
      sentinel != 0x42) {
    return 0;
  }
  state->selection = selection;
  char type[] = "RSA";
  const OSSL_PARAM params[] = {
      OSSL_PARAM_utf8_string(
          OSSL_OBJECT_PARAM_DATA_TYPE, type, sizeof(type) - 1),
      OSSL_PARAM_octet_string(
          OSSL_OBJECT_PARAM_REFERENCE, &state, sizeof(state)),
      OSSL_PARAM_END,
  };
  return callback(params, arg);
}

void* RsaLoadTestLoad(const void* reference, size_t size) {
  if (size != sizeof(RsaLoadTestContext*)) return nullptr;
  auto* state = *static_cast<RsaLoadTestContext* const*>(reference);
  state->loads++;
  return state;
}

void RsaLoadTestFree(void* context) {
  static_cast<RsaLoadTestContext*>(context)->frees++;
}

int RsaLoadTestHas(const void*, int selection) {
  return (selection & OSSL_KEYMGMT_SELECT_PRIVATE_KEY) == 0;
}

const OSSL_ALGORITHM* RsaLoadTestQuery(void*, int operation, int* no_cache) {
  *no_cache = 0;
  static const OSSL_DISPATCH decoder[] = {
      {OSSL_FUNC_DECODER_NEWCTX,
       reinterpret_cast<void (*)(void)>(RsaLoadTestDecoderNew)},
      {OSSL_FUNC_DECODER_FREECTX,
       reinterpret_cast<void (*)(void)>(RsaLoadTestDecoderFree)},
      {OSSL_FUNC_DECODER_DECODE,
       reinterpret_cast<void (*)(void)>(RsaLoadTestDecode)},
      {0, nullptr},
  };
  static const OSSL_DISPATCH keymgmt[] = {
      {OSSL_FUNC_KEYMGMT_LOAD,
       reinterpret_cast<void (*)(void)>(RsaLoadTestLoad)},
      {OSSL_FUNC_KEYMGMT_FREE,
       reinterpret_cast<void (*)(void)>(RsaLoadTestFree)},
      {OSSL_FUNC_KEYMGMT_HAS, reinterpret_cast<void (*)(void)>(RsaLoadTestHas)},
      {0, nullptr},
  };
  static const OSSL_ALGORITHM decoders[] = {
      {"RSA",
       "provider=node-test-rsa-load,input=der,structure=type-specific",
       decoder,
       "Test RSA decoder without export"},
      {nullptr, nullptr, nullptr, nullptr},
  };
  static const OSSL_ALGORITHM keymgmts[] = {
      {"RSA",
       "provider=node-test-rsa-load",
       keymgmt,
       "Test RSA reference load"},
      {nullptr, nullptr, nullptr, nullptr},
  };
  if (operation == OSSL_OP_DECODER) return decoders;
  return operation == OSSL_OP_KEYMGMT ? keymgmts : nullptr;
}

void RsaLoadTestTeardown(void* context) {
  delete static_cast<RsaLoadTestContext*>(context);
}

int RsaLoadTestProviderInit(const OSSL_CORE_HANDLE*,
                            const OSSL_DISPATCH* in,
                            const OSSL_DISPATCH** out,
                            void** context) {
  auto state = std::make_unique<RsaLoadTestContext>();
  for (; in->function_id != 0; in++) {
    if (in->function_id == OSSL_FUNC_BIO_READ_EX) {
      state->read = OSSL_FUNC_BIO_read_ex(in);
    }
  }
  if (state->read == nullptr) return 0;
  static const OSSL_DISPATCH dispatch[] = {
      {OSSL_FUNC_PROVIDER_QUERY_OPERATION,
       reinterpret_cast<void (*)(void)>(RsaLoadTestQuery)},
      {OSSL_FUNC_PROVIDER_TEARDOWN,
       reinterpret_cast<void (*)(void)>(RsaLoadTestTeardown)},
      {0, nullptr},
  };
  *context = state.release();
  *out = dispatch;
  return 1;
}
}  // namespace

TEST(NodeCrypto, ProviderPkcs1PublicKeyLoadWithoutExport) {
  ncrypto::ClearErrorOnReturn clear_errors;
  ncrypto::DeleteFnPtr<OSSL_LIB_CTX, OSSL_LIB_CTX_free> libctx(
      OSSL_LIB_CTX_new());
  ASSERT_TRUE(libctx);
  ASSERT_EQ(OSSL_PROVIDER_add_builtin(
                libctx.get(), "node-test-rsa-load", RsaLoadTestProviderInit),
            1);
  auto* provider = OSSL_PROVIDER_load(libctx.get(), "node-test-rsa-load");
  auto unload_provider =
      node::OnScopeLeave([provider] { OSSL_PROVIDER_unload(provider); });
  ASSERT_NE(provider, nullptr);
  auto* state = static_cast<RsaLoadTestContext*>(
      OSSL_PROVIDER_get0_provider_ctx(provider));
  OSSL_LIB_CTX* previous_libctx = OSSL_LIB_CTX_set0_default(libctx.get());
  auto restore_libctx = node::OnScopeLeave(
      [previous_libctx] { OSSL_LIB_CTX_set0_default(previous_libctx); });

  // Neither decoder export nor keymgmt import is available in this provider.
  const unsigned char sentinel[] = {0x42};
  const EVPKeyPointer::PublicKeyEncodingConfig config(
      false,
      EVPKeyPointer::PKFormatType::DER,
      EVPKeyPointer::PKEncodingType::PKCS1);
  {
    auto imported = EVPKeyPointer::TryParsePublicKey(config, {sentinel, 1});
    ASSERT_TRUE(imported);
    EXPECT_EQ(EVP_PKEY_get0_provider(imported.value.get()), provider);
    EXPECT_TRUE(imported.value.isA(KeyAlgorithm::RSA));
    EXPECT_EQ(state->selection, EVP_PKEY_PUBLIC_KEY);
    EXPECT_EQ(state->loads, 1);
    EXPECT_EQ(state->frees, 0);
  }
  EXPECT_EQ(state->frees, 1);
}
#endif

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

TEST(NodeCrypto, EcGroupNames) {
  ncrypto::ClearErrorOnReturn clear_errors;
  EXPECT_FALSE(Ec::GetCurveName(EVPKeyPointer()));
  EXPECT_FALSE(Ec::CheckCurveName(nullptr));
  EXPECT_FALSE(Ec::CheckCurveName("node-test-unknown-curve"));
  for (const char* name : {"P-256", "prime256v1"}) {
    EXPECT_TRUE(Ec::CheckCurveName(name));
    auto ctx = EVPKeyCtxPointer::NewFromAlgorithm(KeyAlgorithm::EC);
    ASSERT_TRUE(ctx.initForParamgen());
    ASSERT_TRUE(ctx.setEcParameters(name, OPENSSL_EC_NAMED_CURVE));
    auto key = ctx.paramgen();
    ASSERT_TRUE(key);
    const auto group = Ec::GetCurveName(key);
    ASSERT_TRUE(group);
    EXPECT_EQ(group.value(), "prime256v1");
  }
}

#if NCRYPTO_USE_OPENSSL3_PROVIDER
namespace {
// Deliberately longer than the fixed-size group buffer formerly used by
// ncrypto.
constexpr char kProviderEcGroup[] =
    "node-test-provider-ec-group-without-an-object-identifier-"
    "and-with-a-name-longer-than-eighty-bytes";

int EcGroupTestSetParams(void* data, const OSSL_PARAM params[]) {
  const OSSL_PARAM* group =
      OSSL_PARAM_locate_const(params, OSSL_PKEY_PARAM_GROUP_NAME);
  if (group == nullptr) return 1;
  const char* name = nullptr;
  if (OSSL_PARAM_get_utf8_string_ptr(group, &name) != 1 || name == nullptr) {
    return 0;
  }
  *static_cast<std::string*>(data) = name;
  return 1;
}

void* EcGroupTestGenInit(void*, int, const OSSL_PARAM params[]) {
  auto data = std::make_unique<std::string>();
  if (params != nullptr && !EcGroupTestSetParams(data.get(), params)) {
    return nullptr;
  }
  return data.release();
}

void* EcGroupTestGen(void* data, OSSL_CALLBACK*, void*) {
  const auto& group = *static_cast<std::string*>(data);
  // Defer rejecting unknown names until generation, as a provider may do.
  if (group != kProviderEcGroup && group != "prime256v1") {
    ERR_raise(ERR_LIB_USER, ERR_R_PASSED_INVALID_ARGUMENT);
    return nullptr;
  }
  return new std::string(group);
}

void EcGroupTestFree(void* data) {
  delete static_cast<std::string*>(data);
}

int EcGroupTestHas(const void* data, int selection) {
  return (selection & OSSL_KEYMGMT_SELECT_KEYPAIR) == 0 &&
         !static_cast<const std::string*>(data)->empty();
}

int EcGroupTestGetParams(void* data, OSSL_PARAM params[]) {
  OSSL_PARAM* group = OSSL_PARAM_locate(params, OSSL_PKEY_PARAM_GROUP_NAME);
  return group == nullptr ||
         OSSL_PARAM_set_utf8_string(group,
                                    static_cast<std::string*>(data)->c_str());
}

const OSSL_PARAM* EcGroupTestParams(void*, void*) {
  static const OSSL_PARAM params[] = {
      OSSL_PARAM_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME, nullptr, 0),
      OSSL_PARAM_utf8_string(OSSL_PKEY_PARAM_EC_ENCODING, nullptr, 0),
      OSSL_PARAM_END,
  };
  return params;
}

const OSSL_PARAM* EcGroupTestGettableParams(void*) {
  return EcGroupTestParams(nullptr, nullptr);
}

const OSSL_ALGORITHM* EcGroupTestQuery(void*, int operation, int* no_cache) {
  *no_cache = 0;
  static const OSSL_DISPATCH keymgmt[] = {
      {OSSL_FUNC_KEYMGMT_GEN_INIT,
       reinterpret_cast<void (*)(void)>(EcGroupTestGenInit)},
      {OSSL_FUNC_KEYMGMT_GEN_SET_PARAMS,
       reinterpret_cast<void (*)(void)>(EcGroupTestSetParams)},
      {OSSL_FUNC_KEYMGMT_GEN_SETTABLE_PARAMS,
       reinterpret_cast<void (*)(void)>(EcGroupTestParams)},
      {OSSL_FUNC_KEYMGMT_GEN, reinterpret_cast<void (*)(void)>(EcGroupTestGen)},
      {OSSL_FUNC_KEYMGMT_GEN_CLEANUP,
       reinterpret_cast<void (*)(void)>(EcGroupTestFree)},
      {OSSL_FUNC_KEYMGMT_FREE,
       reinterpret_cast<void (*)(void)>(EcGroupTestFree)},
      {OSSL_FUNC_KEYMGMT_HAS, reinterpret_cast<void (*)(void)>(EcGroupTestHas)},
      {OSSL_FUNC_KEYMGMT_GET_PARAMS,
       reinterpret_cast<void (*)(void)>(EcGroupTestGetParams)},
      {OSSL_FUNC_KEYMGMT_GETTABLE_PARAMS,
       reinterpret_cast<void (*)(void)>(EcGroupTestGettableParams)},
      {0, nullptr},
  };
  static const OSSL_ALGORITHM algorithms[] = {
      {"EC", "provider=node-test-ec", keymgmt, "Test provider EC group names"},
      {nullptr, nullptr, nullptr, nullptr},
  };
  return operation == OSSL_OP_KEYMGMT ? algorithms : nullptr;
}

int EcGroupTestProviderInit(const OSSL_CORE_HANDLE*,
                            const OSSL_DISPATCH*,
                            const OSSL_DISPATCH** out,
                            void**) {
  static const OSSL_DISPATCH dispatch[] = {
      {OSSL_FUNC_PROVIDER_QUERY_OPERATION,
       reinterpret_cast<void (*)(void)>(EcGroupTestQuery)},
      {0, nullptr},
  };
  *out = dispatch;
  return 1;
}

void EcGroupTestUnloadProvider(OSSL_PROVIDER* provider) {
  OSSL_PROVIDER_unload(provider);
}
}  // namespace

TEST(NodeCrypto, ProviderEcGroupName) {
  ncrypto::ClearErrorOnReturn clear_errors;
  ncrypto::DeleteFnPtr<OSSL_LIB_CTX, OSSL_LIB_CTX_free> libctx(
      OSSL_LIB_CTX_new());
  ASSERT_TRUE(libctx);
  ASSERT_EQ(OSSL_PROVIDER_add_builtin(
                libctx.get(), "node-test-ec", EcGroupTestProviderInit),
            1);
  ncrypto::DeleteFnPtr<OSSL_PROVIDER, EcGroupTestUnloadProvider> provider(
      OSSL_PROVIDER_load(libctx.get(), "node-test-ec"));
  ASSERT_TRUE(provider);
  {
    OSSL_LIB_CTX* previous_libctx = OSSL_LIB_CTX_set0_default(libctx.get());
    auto restore_libctx = node::OnScopeLeave(
        [previous_libctx] { OSSL_LIB_CTX_set0_default(previous_libctx); });
    // The provider supports generation, but intentionally has no import API.
    EXPECT_TRUE(Ec::CheckCurveName(kProviderEcGroup));
    EXPECT_FALSE(Ec::CheckCurveName("node-test-unknown-curve"));
    EXPECT_TRUE(Ec::CheckCurveName("P-256"));

    auto ctx = EVPKeyCtxPointer::NewFromAlgorithm(KeyAlgorithm::EC);
    ASSERT_TRUE(ctx);
    ASSERT_TRUE(ctx.initForParamgen());
    ASSERT_TRUE(ctx.setEcParameters(kProviderEcGroup, OPENSSL_EC_NAMED_CURVE));
    auto key = ctx.paramgen();
    ASSERT_TRUE(key);
    EXPECT_EQ(Ec::GetCurveId(key), NID_undef);
    const auto name = Ec::GetCurveName(key);
    ASSERT_TRUE(name);
    EXPECT_EQ(name.value(), kProviderEcGroup);

    // Only usable built-in curves are enumerated. The provider-only group
    // remains usable by name without appearing in this list.
    ERR_clear_error();
    ERR_raise(ERR_LIB_USER, ERR_R_INTERNAL_ERROR);
    const auto saved_error = ERR_peek_error();
    std::vector<std::string> curves;
    EXPECT_TRUE(Ec::GetCurves([&curves](const char* curve) {
      curves.emplace_back(curve);
      return true;
    }));
    EXPECT_EQ(curves, std::vector<std::string>{"prime256v1"});
    EXPECT_EQ(ERR_peek_error(), saved_error);
    EXPECT_EQ(ERR_peek_last_error(), saved_error);

    unsigned int calls = 0;
    EXPECT_FALSE(Ec::GetCurves([&calls](const char*) {
      calls++;
      ERR_raise(ERR_LIB_USER, ERR_R_PASSED_INVALID_ARGUMENT);
      return false;
    }));
    EXPECT_EQ(calls, 1U);
    EXPECT_EQ(ERR_get_error(), saved_error);
    EXPECT_EQ(ERR_GET_REASON(ERR_get_error()), ERR_R_PASSED_INVALID_ARGUMENT);
    EXPECT_EQ(ERR_get_error(), 0UL);

    ASSERT_EQ(EVP_set_default_properties(nullptr, "provider=default"), 1);
    EXPECT_TRUE(Ec::GetCurves([](const char*) {
      ADD_FAILURE() << "No EC groups should match the default properties";
      return true;
    }));
  }
}
#endif

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

#if NCRYPTO_USE_OPENSSL3_PROVIDER
TEST(NodeCrypto, PrivateKeyEncodingOwnsFetchedCipher) {
  ncrypto::ClearErrorOnReturn clear_errors;
  EVPKeyPointer::PrivateKeyEncodingConfig assigned;
  {
    EVPKeyPointer::PrivateKeyEncodingConfig original;
    original.cipher =
        ncrypto::Cipher::FromNameForKeyEncoding("2.16.840.1.101.3.4.1.42");
    ASSERT_TRUE(original.cipher);
    ASSERT_NE(EVP_CIPHER_get0_provider(original.cipher.get()), nullptr);
    const auto copied = original;
    assigned = copied;
  }

  ASSERT_TRUE(assigned.cipher);
  EXPECT_NE(EVP_CIPHER_get0_provider(assigned.cipher.get()), nullptr);
  EXPECT_EQ(EVP_CIPHER_is_a(assigned.cipher.get(), "AES-256-CBC"), 1);
  auto ctx = ncrypto::CipherCtxPointer::New();
  const unsigned char key[32] = {};
  const unsigned char iv[16] = {};
  EXPECT_TRUE(ctx.init(assigned.cipher, true, key, iv));
}

TEST(NodeCrypto, PrivateKeyEncodingProviderOnlyCipher) {
  ncrypto::ClearErrorOnReturn clear_errors;
  const auto available = ncrypto::Cipher::FromName("AES-128-CBC-CTS");
  if (!available) GTEST_SKIP();
  const auto cipher =
      ncrypto::Cipher::FromNameForKeyEncoding("AES-128-CBC-CTS");
  ASSERT_TRUE(cipher);
  EXPECT_NE(EVP_CIPHER_get0_provider(cipher.get()), nullptr);
  EXPECT_EQ(EVP_CIPHER_is_a(cipher.get(), "AES-128-CBC-CTS"), 1);
}
#endif
