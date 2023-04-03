#include "node_sea.h"

#include "env-inl.h"
#include "node_external_reference.h"
#include "node_internals.h"
#include "node_union_bytes.h"

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

namespace node {
namespace sea {

std::string_view FindSingleExecutableCode() {
  CHECK(IsSingleExecutable());
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

bool IsSingleExecutable() {
  return postject_has_resource();
}

void IsSingleExecutable(const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(IsSingleExecutable());
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
  SetMethod(context, target, "isSea", IsSingleExecutable);
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(IsSingleExecutable);
}

}  // namespace sea
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(sea, node::sea::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(sea, node::sea::RegisterExternalReferences)

#endif  // !defined(DISABLE_SINGLE_EXECUTABLE_APPLICATION)
