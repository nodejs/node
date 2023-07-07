#include "node_sea.h"

#include "blob_serializer_deserializer-inl.h"
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

class SeaSerializer : public BlobSerializer<SeaSerializer> {
 public:
  SeaSerializer()
      : BlobSerializer<SeaSerializer>(
            per_process::enabled_debug_list.enabled(DebugCategory::SEA)) {}

  template <typename T,
            std::enable_if_t<!std::is_same<T, std::string>::value>* = nullptr,
            std::enable_if_t<!std::is_arithmetic<T>::value>* = nullptr>
  size_t Write(const T& data);
};

template <>
size_t SeaSerializer::Write(const SeaResource& sea) {
  sink.reserve(SeaResource::kHeaderSize + sea.code.size());

  Debug("Write SEA magic %x\n", kMagic);
  size_t written_total = WriteArithmetic<uint32_t>(kMagic);

  uint32_t flags = static_cast<uint32_t>(sea.flags);
  Debug("Write SEA flags %x\n", flags);
  written_total += WriteArithmetic<uint32_t>(flags);
  DCHECK_EQ(written_total, SeaResource::kHeaderSize);

  Debug("Write SEA resource code %p, size=%zu\n",
        sea.code.data(),
        sea.code.size());
  written_total += WriteStringView(sea.code, StringLogMode::kAddressAndContent);
  return written_total;
}

class SeaDeserializer : public BlobDeserializer<SeaDeserializer> {
 public:
  explicit SeaDeserializer(std::string_view v)
      : BlobDeserializer<SeaDeserializer>(
            per_process::enabled_debug_list.enabled(DebugCategory::SEA), v) {}

  template <typename T,
            std::enable_if_t<!std::is_same<T, std::string>::value>* = nullptr,
            std::enable_if_t<!std::is_arithmetic<T>::value>* = nullptr>
  T Read();
};

template <>
SeaResource SeaDeserializer::Read() {
  uint32_t magic = ReadArithmetic<uint32_t>();
  Debug("Read SEA magic %x\n", magic);

  CHECK_EQ(magic, kMagic);
  SeaFlags flags(static_cast<SeaFlags>(ReadArithmetic<uint32_t>()));
  Debug("Read SEA flags %x\n", static_cast<uint32_t>(flags));
  CHECK_EQ(read_total, SeaResource::kHeaderSize);

  std::string_view code = ReadStringView(StringLogMode::kAddressAndContent);
  Debug("Read SEA resource code %p, size=%zu\n", code.data(), code.size());
  return {flags, code};
}

std::string_view FindSingleExecutableBlob() {
  CHECK(IsSingleExecutable());
  static const std::string_view result = []() -> std::string_view {
    size_t size;
#ifdef __APPLE__
    postject_options options;
    postject_options_init(&options);
    options.macho_segment_name = "NODE_SEA";
    const char* blob = static_cast<const char*>(
        postject_find_resource("NODE_SEA_BLOB", &size, &options));
#else
    const char* blob = static_cast<const char*>(
        postject_find_resource("NODE_SEA_BLOB", &size, nullptr));
#endif
    return {blob, size};
  }();
  per_process::Debug(DebugCategory::SEA,
                     "Found SEA blob %p, size=%zu\n",
                     result.data(),
                     result.size());
  return result;
}

}  // anonymous namespace

SeaResource FindSingleExecutableResource() {
  static const SeaResource sea_resource = []() -> SeaResource {
    std::string_view blob = FindSingleExecutableBlob();
    per_process::Debug(DebugCategory::SEA,
                       "Found SEA resource %p, size=%zu\n",
                       blob.data(),
                       blob.size());
    SeaDeserializer deserializer(blob);
    return deserializer.Read<SeaResource>();
  }();
  return sea_resource;
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

ExitCode GenerateSingleExecutableBlob(const SeaConfig& config) {
  std::string main_script;
  // TODO(joyeecheung): unify the file utils.
  int r = ReadFileSync(&main_script, config.main_path.c_str());
  if (r != 0) {
    const char* err = uv_strerror(r);
    FPrintF(stderr, "Cannot read main script %s:%s\n", config.main_path, err);
    return ExitCode::kGenericUserError;
  }

  SeaResource sea{config.flags, main_script};

  SeaSerializer serializer;
  serializer.Write(sea);

  uv_buf_t buf = uv_buf_init(serializer.sink.data(), serializer.sink.size());
  r = WriteFileSync(config.output_path.c_str(), buf);
  if (r != 0) {
    const char* err = uv_strerror(r);
    FPrintF(stderr, "Cannot write output to %s:%s\n", config.output_path, err);
    return ExitCode::kGenericUserError;
  }

  FPrintF(stderr,
          "Wrote single executable preparation blob to %s\n",
          config.output_path);
  return ExitCode::kNoFailure;
}

}  // anonymous namespace

ExitCode BuildSingleExecutableBlob(const std::string& config_path) {
  std::optional<SeaConfig> config_opt =
      ParseSingleExecutableConfig(config_path);
  if (config_opt.has_value()) {
    ExitCode code = GenerateSingleExecutableBlob(config_opt.value());
    return code;
  }

  return ExitCode::kGenericUserError;
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
