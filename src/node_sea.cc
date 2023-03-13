#include "node_sea.h"

#include "env-inl.h"
#include "json_parser.h"
#include "node_external_reference.h"
#include "node_internals.h"
#include "node_union_bytes.h"

// The POSTJECT_SENTINEL_FUSE macro is a string of random characters selected by
// the Node.js project that is present only once in the entire binary. It is
// used by the postject_has_resource() function to efficiently detect if a
// resource has been injected. See
// https://github.com/nodejs/postject/blob/35343439cac8c488f2596d7c4c1dddfec1fddcae/postject-api.h#L42-L45.
#define POSTJECT_SENTINEL_FUSE "NODE_SEA_FUSE_fce680ab2cc467b6e072b8b5df1996b2"
#include "postject-api.h"
#undef POSTJECT_SENTINEL_FUSE

#include <memory>
#include <string_view>
#include <tuple>
#include <vector>

#if !defined(DISABLE_SINGLE_EXECUTABLE_APPLICATION)

using node::ExitCode;
using node::Utf8Value;
using v8::ArrayBuffer;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::Global;
using v8::HandleScope;
using v8::Isolate;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::Nothing;
using v8::Object;
using v8::String;
using v8::TryCatch;
using v8::Value;

namespace node {
namespace sea {

static const uint32_t kMagic = 0x143da20;

std::string_view FindSingleExecutableCode() {
  CHECK(IsSingleExecutable());
  static const std::string_view sea_code = []() -> std::string_view {
    size_t size;
#ifdef __APPLE__
    postject_options options;
    postject_options_init(&options);
    options.macho_segment_name = "NODE_SEA";
    const char* code = static_cast<const char*>(
        postject_find_resource("NODE_SEA_BLOB", &size, &options));
#else
    const char* code = static_cast<const char*>(
        postject_find_resource("NODE_SEA_BLOB", &size, nullptr));
#endif
    uint32_t first_word = reinterpret_cast<const uint32_t*>(code)[0];
    CHECK_EQ(first_word, kMagic);
    // TODO(joyeecheung): do more checks here e.g. matching the versions.
    return {code + sizeof(first_word), size - sizeof(first_word)};
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

ExitCode BuildSingleExecutableBlob(const std::string& config_path) {
  std::string config;
  int r = ReadFileSync(&config, config_path.c_str());
  if (r != 0) {
    fprintf(stderr,
            "Cannot read single executable configuration from %s\n",
            config_path.c_str());
    return ExitCode::kGenericUserError;
  }

  std::string main_path;
  std::string output_path;
  {
    JSONParser parser;
    if (!parser.Parse(config)) {
      fprintf(stderr, "Cannot parse JSON from %s\n", config_path.c_str());
      return ExitCode::kGenericUserError;
    }

    main_path = parser.GetTopLevelField("main").value_or(std::string());
    if (main_path.empty()) {
      fprintf(stderr,
              "\"main\" field of %s is not a string\n",
              config_path.c_str());
      return ExitCode::kGenericUserError;
    }

    output_path = parser.GetTopLevelField("output").value_or(std::string());
    if (output_path.empty()) {
      fprintf(stderr,
              "\"output\" field of %s is not a string\n",
              config_path.c_str());
      return ExitCode::kGenericUserError;
    }
  }

  std::string main_script;
  // TODO(joyeecheung): unify the file utils.
  r = ReadFileSync(&main_script, main_path.c_str());
  if (r != 0) {
    fprintf(stderr, "Cannot read main script %s\n", main_path.c_str());
    return ExitCode::kGenericUserError;
  }

  std::vector<char> sink;
  // TODO(joyeecheung): reuse the SnapshotSerializerDeserializer for this.
  sink.reserve(sizeof(kMagic) + main_script.size());
  const char* pos = reinterpret_cast<const char*>(&kMagic);
  sink.insert(sink.end(), pos, pos + sizeof(kMagic));
  sink.insert(
      sink.end(), main_script.data(), main_script.data() + main_script.size());

  uv_buf_t buf = uv_buf_init(sink.data(), sink.size());
  r = WriteFileSync(output_path.c_str(), buf);
  if (r != 0) {
    fprintf(stderr, "Cannot write output to %s\n", output_path.c_str());
    return ExitCode::kGenericUserError;
  }

  fprintf(stderr,
          "Wrote single executable preparation blob to %s\n",
          output_path.c_str());
  return ExitCode::kNoFailure;
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
