#include "node_sea.h"

#include "env-inl.h"
#include "node_external_reference.h"
#include "node_internals.h"
#include "node_union_bytes.h"
#include "simdutf.h"
#include "v8.h"

// The POSTJECT_SENTINEL_FUSE macro is a string of random characters selected by
// the Node.js project that is present only once in the entire binary. It is
// used by the postject_has_resource() function to efficiently detect if a
// resource has been injected. See
// https://github.com/nodejs/postject/blob/35343439cac8c488f2596d7c4c1dddfec1fddcae/postject-api.h#L42-L45.
#define POSTJECT_SENTINEL_FUSE "NODE_JS_FUSE_fce680ab2cc467b6e072b8b5df1996b2"
#include "postject-api.h"
#undef POSTJECT_SENTINEL_FUSE

#include <memory>
#include <string_view>
#include <tuple>
#include <vector>

#if !defined(DISABLE_SINGLE_EXECUTABLE_APPLICATION)

using v8::Context;
using v8::FunctionCallbackInfo;
using v8::Local;
using v8::Object;
using v8::Value;

namespace {

const std::string_view FindSingleExecutableCode() {
  static const std::string_view sea_code = []() -> std::string_view {
    size_t size;
#ifdef __APPLE__
    postject_options options;
    postject_options_init(&options);
    options.macho_segment_name = "NODE_JS";
    const char* code = static_cast<const char*>(
        postject_find_resource("NODE_JS_CODE", &size, &options));
#else
    const char* code = static_cast<const char*>(
        postject_find_resource("NODE_JS_CODE", &size, nullptr));
#endif
    return {code, size};
  }();
  return sea_code;
}

void GetSingleExecutableCode(const FunctionCallbackInfo<Value>& args) {
  node::Environment* env = node::Environment::GetCurrent(args);

  static const std::string_view sea_code = FindSingleExecutableCode();

  if (sea_code.empty()) {
    return;
  }

  // TODO(joyeecheung): Use one-byte strings for ASCII-only source to save
  // memory/binary size - using UTF16 by default results in twice of the size
  // than necessary.
  static const node::UnionBytes sea_code_union_bytes =
      []() -> node::UnionBytes {
    size_t expected_u16_length =
        simdutf::utf16_length_from_utf8(sea_code.data(), sea_code.size());
    auto out = std::make_shared<std::vector<uint16_t>>(expected_u16_length);
    size_t u16_length = simdutf::convert_utf8_to_utf16(
        sea_code.data(),
        sea_code.size(),
        reinterpret_cast<char16_t*>(out->data()));
    out->resize(u16_length);
    return node::UnionBytes{out};
  }();

  args.GetReturnValue().Set(
      sea_code_union_bytes.ToStringChecked(env->isolate()));
}

}  // namespace

namespace node {
namespace sea {

bool IsSingleExecutable() {
  return postject_has_resource();
}

std::tuple<int, char**> FixupArgsForSEA(int argc, char** argv) {
  // Repeats argv[0] at position 1 on argv as a replacement for the missing
  // entry point file path.
  if (IsSingleExecutable()) {
    static std::vector<char*> new_argv;
    new_argv.reserve(argc + 2);
    new_argv.emplace_back(argv[0]);
    new_argv.insert(new_argv.end(), argv, argv + argc);
    new_argv.emplace_back(nullptr);
    argc = new_argv.size() - 1;
    argv = new_argv.data();
  }

  return {argc, argv};
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  SetMethod(
      context, target, "getSingleExecutableCode", GetSingleExecutableCode);
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(GetSingleExecutableCode);
}

}  // namespace sea
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(sea, node::sea::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(sea, node::sea::RegisterExternalReferences)

#endif  // !defined(DISABLE_SINGLE_EXECUTABLE_APPLICATION)
