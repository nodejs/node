#include "gtest/gtest.h"
#include "ncrypto.h"
#include "node_test_fixture.h"
#include "util.h"

#if !NCRYPTO_USE_BORINGSSL
#include <openssl/core_dispatch.h>
#include <openssl/core_names.h>
#include <openssl/params.h>
#include <openssl/provider.h>

#include <algorithm>
#include <array>
#include <cstring>

namespace {
using Result = std::array<unsigned char, 3>;

void* New(void*) {
  return new Result();
}

void Free(void* context) {
  delete static_cast<Result*>(context);
}

int ReadCustomization(void* context, const OSSL_PARAM* param) {
  auto& result = *static_cast<Result*>(context);
  if (param == nullptr || param->data == nullptr ||
      param->data_size > result.size()) {
    return 0;
  }
  result.fill(0);
  std::copy_n(static_cast<const unsigned char*>(param->data),
              param->data_size,
              result.begin());
  return 1;
}

int DigestInit(void* context, const OSSL_PARAM params[]) {
  const auto* param = OSSL_PARAM_locate_const(params, "customization");
  if (param == nullptr || param->data_type != OSSL_PARAM_OCTET_STRING) return 0;
  // Ignore other parameters so rejection must come from Node's metadata check.
  return ReadCustomization(context, param);
}

int MacInit(void* context,
            const unsigned char*,
            size_t,
            const OSSL_PARAM params[]) {
  const auto* digest = OSSL_PARAM_locate_const(params, OSSL_MAC_PARAM_DIGEST);
  if (digest == nullptr || digest->data_type != OSSL_PARAM_OCTET_STRING ||
      digest->data_size != 6 || std::memcmp(digest->data, "sha256", 6) != 0) {
    return 0;
  }
  const auto* custom = OSSL_PARAM_locate_const(params, OSSL_MAC_PARAM_CUSTOM);
  if (custom == nullptr || custom->data == nullptr) return 0;
  const bool binary =
      std::memchr(custom->data, 0, custom->data_size) != nullptr;
  if (custom->data_type !=
      (binary ? OSSL_PARAM_OCTET_STRING : OSSL_PARAM_UTF8_STRING)) {
    return 0;
  }
  if (!binary) {
    const char* text = nullptr;
    if (!OSSL_PARAM_get_utf8_string_ptr(custom, &text) ||
        text[custom->data_size] != '\0') {
      return 0;
    }
  }
  return ReadCustomization(context, custom);
}

int Update(void*, const unsigned char*, size_t) {
  return 1;
}

// Echo the customization, padded to three bytes, to verify its exact contents.
int Final(void* context, unsigned char* output, size_t* length, size_t size) {
  const auto& result = *static_cast<Result*>(context);
  if (size < result.size()) return 0;
  std::copy(result.begin(), result.end(), output);
  *length = result.size();
  return 1;
}

int GetSizes(OSSL_PARAM params[]) {
  for (const char* name :
       {OSSL_DIGEST_PARAM_SIZE, OSSL_DIGEST_PARAM_BLOCK_SIZE}) {
    auto* param = OSSL_PARAM_locate(params, name);
    if (param != nullptr && !OSSL_PARAM_set_size_t(param, Result().size())) {
      return 0;
    }
  }
  return 1;
}

int MacGetSizes(void*, OSSL_PARAM params[]) {
  return GetSizes(params);
}

const OSSL_PARAM* DigestSettable(void*, void*) {
  static const OSSL_PARAM params[] = {
      OSSL_PARAM_octet_string("customization", nullptr, 0),
      OSSL_PARAM_uint("function-name", nullptr),
      OSSL_PARAM_END};
  return params;
}

const OSSL_PARAM* MacSettable(void*, void*) {
  static const OSSL_PARAM params[] = {
      OSSL_PARAM_octet_string(OSSL_MAC_PARAM_DIGEST, nullptr, 0),
      OSSL_PARAM_uint(OSSL_MAC_PARAM_CUSTOM, nullptr),
      OSSL_PARAM_utf8_string(OSSL_MAC_PARAM_CUSTOM, nullptr, 0),
      OSSL_PARAM_octet_string(OSSL_MAC_PARAM_CUSTOM, nullptr, 0),
      OSSL_PARAM_uint(OSSL_MAC_PARAM_SALT, nullptr),
      OSSL_PARAM_END};
  return params;
}

const OSSL_ALGORITHM* Query(void*, int operation, int* no_cache) {
  *no_cache = 0;
  static const OSSL_DISPATCH digest[] = {
      {OSSL_FUNC_DIGEST_NEWCTX, reinterpret_cast<void (*)(void)>(New)},
      {OSSL_FUNC_DIGEST_FREECTX, reinterpret_cast<void (*)(void)>(Free)},
      {OSSL_FUNC_DIGEST_INIT, reinterpret_cast<void (*)(void)>(DigestInit)},
      {OSSL_FUNC_DIGEST_UPDATE, reinterpret_cast<void (*)(void)>(Update)},
      {OSSL_FUNC_DIGEST_FINAL, reinterpret_cast<void (*)(void)>(Final)},
      {OSSL_FUNC_DIGEST_GET_PARAMS, reinterpret_cast<void (*)(void)>(GetSizes)},
      {OSSL_FUNC_DIGEST_SETTABLE_CTX_PARAMS,
       reinterpret_cast<void (*)(void)>(DigestSettable)},
      {0, nullptr}};
  static const OSSL_DISPATCH mac[] = {
      {OSSL_FUNC_MAC_NEWCTX, reinterpret_cast<void (*)(void)>(New)},
      {OSSL_FUNC_MAC_FREECTX, reinterpret_cast<void (*)(void)>(Free)},
      {OSSL_FUNC_MAC_INIT, reinterpret_cast<void (*)(void)>(MacInit)},
      {OSSL_FUNC_MAC_UPDATE, reinterpret_cast<void (*)(void)>(Update)},
      {OSSL_FUNC_MAC_FINAL, reinterpret_cast<void (*)(void)>(Final)},
      {OSSL_FUNC_MAC_GET_CTX_PARAMS,
       reinterpret_cast<void (*)(void)>(MacGetSizes)},
      {OSSL_FUNC_MAC_SETTABLE_CTX_PARAMS,
       reinterpret_cast<void (*)(void)>(MacSettable)},
      {0, nullptr}};
  static const OSSL_ALGORITHM digests[] = {
      {"node-test-digest", "provider=node-test-params", digest, nullptr},
      {nullptr, nullptr, nullptr, nullptr}};
  static const OSSL_ALGORITHM macs[] = {
      {"node-test-mac", "provider=node-test-params", mac, nullptr},
      {nullptr, nullptr, nullptr, nullptr}};
  if (operation == OSSL_OP_DIGEST) return digests;
  return operation == OSSL_OP_MAC ? macs : nullptr;
}

int ProviderInit(const OSSL_CORE_HANDLE*,
                 const OSSL_DISPATCH*,
                 const OSSL_DISPATCH** out,
                 void** context) {
  static const OSSL_DISPATCH functions[] = {
      {OSSL_FUNC_PROVIDER_QUERY_OPERATION,
       reinterpret_cast<void (*)(void)>(Query)},
      {0, nullptr}};
  *context = nullptr;
  *out = functions;
  return 1;
}

void Unload(OSSL_PROVIDER* provider) {
  OSSL_PROVIDER_unload(provider);
}
}  // namespace

class NodeCryptoParamsEnv : public EnvironmentTestFixture {};

TEST_F(NodeCryptoParamsEnv, ProviderParameterTypes) {
  ncrypto::ClearErrorOnReturn clear_errors;
  ncrypto::DeleteFnPtr<OSSL_LIB_CTX, OSSL_LIB_CTX_free> libctx(
      OSSL_LIB_CTX_new());
  ASSERT_TRUE(libctx);
  ncrypto::DeleteFnPtr<OSSL_PROVIDER, Unload> default_provider(
      OSSL_PROVIDER_load(libctx.get(), "default"));
  ASSERT_TRUE(default_provider);
  ASSERT_EQ(
      OSSL_PROVIDER_add_builtin(libctx.get(), "node-test-params", ProviderInit),
      1);
  ncrypto::DeleteFnPtr<OSSL_PROVIDER, Unload> provider(
      OSSL_PROVIDER_load(libctx.get(), "node-test-params"));
  ASSERT_TRUE(provider);
  auto* previous = OSSL_LIB_CTX_set0_default(libctx.get());
  auto restore =
      node::OnScopeLeave([previous] { OSSL_LIB_CTX_set0_default(previous); });
  v8::HandleScope handle_scope(isolate_);
  Argv argv;
  Env env{handle_scope, argv};
  v8::Local<v8::Value> result;
  ASSERT_TRUE(node::LoadEnvironment(*env, R"JS(
    const assert = require('node:assert/strict');
    const { createHash, hash, createMac } = require('node:crypto');
    const binary = Buffer.from([0x61, 0, 0x62]);
    const empty = Buffer.alloc(0);
    const key = Buffer.from('key');
    for (const customization of [binary, empty]) {
      const options = { customization, outputEncoding: 'buffer' };
      const expected = customization.length ? customization : Buffer.alloc(3);
      assert.deepStrictEqual(
        createHash('node-test-digest', options).digest(), expected);
      assert.deepStrictEqual(hash('node-test-digest', '', options), expected);
    }
    const wrongType = { customization: binary, functionName: '' };
    const unsupported = {
      code: 'ERR_INVALID_ARG_VALUE',
      message: "The property 'options.functionName' is not supported by digest node-test-digest",
    };
    assert.throws(() => createHash('node-test-digest', wrongType), unsupported);
    assert.throws(() => hash('node-test-digest', '', wrongType), unsupported);
    // The nonzero suffix detects borrowed storage lacking a UTF-8 terminator.
    for (const customization of [Buffer.from('abc!').subarray(0, 3), binary, empty]) {
      const options = { digest: 'sha256', customization };
      const expected = customization.length ? customization : Buffer.alloc(3);
      assert.deepStrictEqual(createMac('node-test-mac', key, options).final(), expected);
    }
    assert.throws(() => createMac('node-test-mac', key, {
      digest: 'sha256', customization: empty, salt: empty,
    }), { code: 'ERR_INVALID_ARG_VALUE', message: /options.salt.*not supported/ });
    return true;
  )JS")
                  .ToLocal(&result));
  EXPECT_TRUE(result->IsTrue());
}
#endif  // !NCRYPTO_USE_BORINGSSL
