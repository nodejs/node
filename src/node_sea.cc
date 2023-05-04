#include "node_sea.h"

#include "debug_utils-inl.h"
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
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::Local;
using v8::Object;
using v8::Value;

namespace node {
namespace sea {

namespace {
// A special number that will appear at the beginning of the single executable
// preparation blobs ready to be injected into the binary. We use this to check
// that the data given to us are intended for building single executable
// applications.
const uint32_t kMagic = 0x143da20;

enum class SeaFlags : uint32_t {
  kDefault = 0,
  kDisableExperimentalSeaWarning = 1 << 0,
};

SeaFlags operator|(SeaFlags x, SeaFlags y) {
  return static_cast<SeaFlags>(static_cast<uint32_t>(x) |
                               static_cast<uint32_t>(y));
}

SeaFlags operator&(SeaFlags x, SeaFlags y) {
  return static_cast<SeaFlags>(static_cast<uint32_t>(x) &
                               static_cast<uint32_t>(y));
}

SeaFlags operator|=(/* NOLINT (runtime/references) */ SeaFlags& x, SeaFlags y) {
  return x = x | y;
}

struct SeaResource {
  SeaFlags flags = SeaFlags::kDefault;
  std::string_view code;
  static constexpr size_t kHeaderSize = sizeof(kMagic) + sizeof(SeaFlags);
};

SeaResource FindSingleExecutableResource() {
  CHECK(IsSingleExecutable());
  static const SeaResource sea_resource = []() -> SeaResource {
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
    SeaFlags flags{
        reinterpret_cast<const SeaFlags*>(code + sizeof(first_word))[0]};
    // TODO(joyeecheung): do more checks here e.g. matching the versions.
    return {
        flags,
        {
            code + SeaResource::kHeaderSize,
            size - SeaResource::kHeaderSize,
        },
    };
  }();
  return sea_resource;
}

}  // namespace

std::string_view FindSingleExecutableCode() {
  SeaResource sea_resource = FindSingleExecutableResource();
  return sea_resource.code;
}

bool IsSingleExecutable() {
  return postject_has_resource();
}

void IsExperimentalSeaWarningNeeded(const FunctionCallbackInfo<Value>& args) {
  if (!IsSingleExecutable()) {
    args.GetReturnValue().Set(false);
    return;
  }

  SeaResource sea_resource = FindSingleExecutableResource();
  args.GetReturnValue().Set(!static_cast<bool>(
      sea_resource.flags & SeaFlags::kDisableExperimentalSeaWarning));
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

namespace {

struct SeaConfig {
  std::string main_path;
  std::string output_path;
  SeaFlags flags = SeaFlags::kDefault;
};

std::optional<SeaConfig> ParseSingleExecutableConfig(
    const std::string& config_path) {
  std::string config;
  int r = ReadFileSync(&config, config_path.c_str());
  if (r != 0) {
    const char* err = uv_strerror(r);
    FPrintF(stderr,
            "Cannot read single executable configuration from %s: %s\n",
            config_path,
            err);
    return std::nullopt;
  }

  SeaConfig result;
  JSONParser parser;
  if (!parser.Parse(config)) {
    FPrintF(stderr, "Cannot parse JSON from %s\n", config_path);
    return std::nullopt;
  }

  result.main_path =
      parser.GetTopLevelStringField("main").value_or(std::string());
  if (result.main_path.empty()) {
    FPrintF(stderr,
            "\"main\" field of %s is not a non-empty string\n",
            config_path);
    return std::nullopt;
  }

  result.output_path =
      parser.GetTopLevelStringField("output").value_or(std::string());
  if (result.output_path.empty()) {
    FPrintF(stderr,
            "\"output\" field of %s is not a non-empty string\n",
            config_path);
    return std::nullopt;
  }

  std::optional<bool> disable_experimental_sea_warning =
      parser.GetTopLevelBoolField("disableExperimentalSEAWarning");
  if (!disable_experimental_sea_warning.has_value()) {
    FPrintF(stderr,
            "\"disableExperimentalSEAWarning\" field of %s is not a Boolean\n",
            config_path);
    return std::nullopt;
  }
  if (disable_experimental_sea_warning.value()) {
    result.flags |= SeaFlags::kDisableExperimentalSeaWarning;
  }

  return result;
}

bool GenerateSingleExecutableBlob(const SeaConfig& config) {
  std::string main_script;
  // TODO(joyeecheung): unify the file utils.
  int r = ReadFileSync(&main_script, config.main_path.c_str());
  if (r != 0) {
    const char* err = uv_strerror(r);
    FPrintF(stderr, "Cannot read main script %s:%s\n", config.main_path, err);
    return false;
  }

  std::vector<char> sink;
  // TODO(joyeecheung): reuse the SnapshotSerializerDeserializer for this.
  sink.reserve(SeaResource::kHeaderSize + main_script.size());
  const char* pos = reinterpret_cast<const char*>(&kMagic);
  sink.insert(sink.end(), pos, pos + sizeof(kMagic));
  pos = reinterpret_cast<const char*>(&(config.flags));
  sink.insert(sink.end(), pos, pos + sizeof(SeaFlags));
  sink.insert(
      sink.end(), main_script.data(), main_script.data() + main_script.size());

  uv_buf_t buf = uv_buf_init(sink.data(), sink.size());
  r = WriteFileSync(config.output_path.c_str(), buf);
  if (r != 0) {
    const char* err = uv_strerror(r);
    FPrintF(stderr, "Cannot write output to %s:%s\n", config.output_path, err);
    return false;
  }

  FPrintF(stderr,
          "Wrote single executable preparation blob to %s\n",
          config.output_path);
  return true;
}

}  // anonymous namespace

ExitCode BuildSingleExecutableBlob(const std::string& config_path) {
  std::optional<SeaConfig> config_opt =
      ParseSingleExecutableConfig(config_path);
  if (!config_opt.has_value() ||
      !GenerateSingleExecutableBlob(config_opt.value())) {
    return ExitCode::kGenericUserError;
  }

  return ExitCode::kNoFailure;
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  SetMethod(context,
            target,
            "isExperimentalSeaWarningNeeded",
            IsExperimentalSeaWarningNeeded);
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(IsExperimentalSeaWarningNeeded);
}

}  // namespace sea
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(sea, node::sea::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(sea, node::sea::RegisterExternalReferences)

#endif  // !defined(DISABLE_SINGLE_EXECUTABLE_APPLICATION)
