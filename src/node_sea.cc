#include "node_sea.h"

#include "blob_serializer_deserializer-inl.h"
#include "debug_utils-inl.h"
#include "env-inl.h"
#include "json_parser.h"
#include "node_contextify.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "node_internals.h"
#include "node_snapshot_builder.h"
#include "node_union_bytes.h"
#include "node_v8_platform-inl.h"
#include "util-inl.h"

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
using v8::ArrayBuffer;
using v8::BackingStore;
using v8::Context;
using v8::DataView;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::NewStringType;
using v8::Object;
using v8::ScriptCompiler;
using v8::String;
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
  sink.reserve(SeaResource::kHeaderSize + sea.main_code_or_snapshot.size());

  Debug("Write SEA magic %x\n", kMagic);
  size_t written_total = WriteArithmetic<uint32_t>(kMagic);

  uint32_t flags = static_cast<uint32_t>(sea.flags);
  Debug("Write SEA flags %x\n", flags);
  written_total += WriteArithmetic<uint32_t>(flags);
  DCHECK_EQ(written_total, SeaResource::kHeaderSize);

  Debug("Write SEA code path %p, size=%zu\n",
        sea.code_path.data(),
        sea.code_path.size());
  written_total +=
      WriteStringView(sea.code_path, StringLogMode::kAddressAndContent);

  Debug("Write SEA resource %s %p, size=%zu\n",
        sea.use_snapshot() ? "snapshot" : "code",
        sea.main_code_or_snapshot.data(),
        sea.main_code_or_snapshot.size());
  written_total +=
      WriteStringView(sea.main_code_or_snapshot,
                      sea.use_snapshot() ? StringLogMode::kAddressOnly
                                         : StringLogMode::kAddressAndContent);

  if (sea.code_cache.has_value()) {
    Debug("Write SEA resource code cache %p, size=%zu\n",
          sea.code_cache->data(),
          sea.code_cache->size());
    written_total +=
        WriteStringView(sea.code_cache.value(), StringLogMode::kAddressOnly);
  }
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

  std::string_view code_path =
      ReadStringView(StringLogMode::kAddressAndContent);
  Debug(
      "Read SEA code path %p, size=%zu\n", code_path.data(), code_path.size());

  bool use_snapshot = static_cast<bool>(flags & SeaFlags::kUseSnapshot);
  std::string_view code =
      ReadStringView(use_snapshot ? StringLogMode::kAddressOnly
                                  : StringLogMode::kAddressAndContent);

  Debug("Read SEA resource %s %p, size=%zu\n",
        use_snapshot ? "snapshot" : "code",
        code.data(),
        code.size());

  std::string_view code_cache;
  if (static_cast<bool>(flags & SeaFlags::kUseCodeCache)) {
    code_cache = ReadStringView(StringLogMode::kAddressOnly);
    Debug("Read SEA resource code cache %p, size=%zu\n",
          code_cache.data(),
          code_cache.size());
  }
  return {flags, code_path, code, code_cache};
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

bool SeaResource::use_snapshot() const {
  return static_cast<bool>(flags & SeaFlags::kUseSnapshot);
}

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

void IsSea(const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(IsSingleExecutable());
}

void IsExperimentalSeaWarningNeeded(const FunctionCallbackInfo<Value>& args) {
  bool is_building_sea =
      !per_process::cli_options->experimental_sea_config.empty();
  if (is_building_sea) {
    args.GetReturnValue().Set(true);
    return;
  }

  if (!IsSingleExecutable()) {
    args.GetReturnValue().Set(false);
    return;
  }

  SeaResource sea_resource = FindSingleExecutableResource();
  args.GetReturnValue().Set(!static_cast<bool>(
      sea_resource.flags & SeaFlags::kDisableExperimentalSeaWarning));
}

void GetCodeCache(const FunctionCallbackInfo<Value>& args) {
  if (!IsSingleExecutable()) {
    return;
  }

  Isolate* isolate = args.GetIsolate();

  SeaResource sea_resource = FindSingleExecutableResource();

  if (!static_cast<bool>(sea_resource.flags & SeaFlags::kUseCodeCache)) {
    return;
  }

  std::shared_ptr<BackingStore> backing_store = ArrayBuffer::NewBackingStore(
      const_cast<void*>(
          static_cast<const void*>(sea_resource.code_cache->data())),
      sea_resource.code_cache->length(),
      [](void* /* data */, size_t /* length */, void* /* deleter_data */) {
        // The code cache data blob is not freed here because it is a static
        // blob which is not allocated by the BackingStore allocator.
      },
      nullptr);
  Local<ArrayBuffer> array_buffer = ArrayBuffer::New(isolate, backing_store);
  Local<DataView> data_view =
      DataView::New(array_buffer, 0, array_buffer->ByteLength());

  args.GetReturnValue().Set(data_view);
}

void GetCodePath(const FunctionCallbackInfo<Value>& args) {
  DCHECK(IsSingleExecutable());

  Isolate* isolate = args.GetIsolate();

  SeaResource sea_resource = FindSingleExecutableResource();

  Local<String> code_path;
  if (!String::NewFromUtf8(isolate,
                           sea_resource.code_path.data(),
                           NewStringType::kNormal,
                           sea_resource.code_path.length())
           .ToLocal(&code_path)) {
    return;
  }

  args.GetReturnValue().Set(code_path);
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

  std::optional<bool> use_snapshot = parser.GetTopLevelBoolField("useSnapshot");
  if (!use_snapshot.has_value()) {
    FPrintF(
        stderr, "\"useSnapshot\" field of %s is not a Boolean\n", config_path);
    return std::nullopt;
  }
  if (use_snapshot.value()) {
    result.flags |= SeaFlags::kUseSnapshot;
  }

  std::optional<bool> use_code_cache =
      parser.GetTopLevelBoolField("useCodeCache");
  if (!use_code_cache.has_value()) {
    FPrintF(
        stderr, "\"useCodeCache\" field of %s is not a Boolean\n", config_path);
    return std::nullopt;
  }
  if (use_code_cache.value()) {
    result.flags |= SeaFlags::kUseCodeCache;
  }

  return result;
}

ExitCode GenerateSnapshotForSEA(const SeaConfig& config,
                                const std::vector<std::string>& args,
                                const std::vector<std::string>& exec_args,
                                const std::string& builder_script_content,
                                const SnapshotConfig& snapshot_config,
                                std::vector<char>* snapshot_blob) {
  SnapshotData snapshot;
  // TODO(joyeecheung): make the arguments configurable through the JSON
  // config or a programmatic API.
  std::vector<std::string> patched_args = {args[0], config.main_path};
  ExitCode exit_code = SnapshotBuilder::Generate(&snapshot,
                                                 patched_args,
                                                 exec_args,
                                                 builder_script_content,
                                                 snapshot_config);
  if (exit_code != ExitCode::kNoFailure) {
    return exit_code;
  }
  auto& persistents = snapshot.env_info.principal_realm.persistent_values;
  auto it = std::find_if(
      persistents.begin(), persistents.end(), [](const PropInfo& prop) {
        return prop.name == "snapshot_deserialize_main";
      });
  if (it == persistents.end()) {
    FPrintF(
        stderr,
        "%s does not invoke "
        "v8.startupSnapshot.setDeserializeMainFunction(), which is required "
        "for snapshot scripts used to build single executable applications."
        "\n",
        config.main_path);
    return ExitCode::kGenericUserError;
  }
  // We need the temporary variable for copy elision.
  std::vector<char> temp = snapshot.ToBlob();
  *snapshot_blob = std::move(temp);
  return ExitCode::kNoFailure;
}

std::optional<std::string> GenerateCodeCache(std::string_view main_path,
                                             std::string_view main_script) {
  RAIIIsolate raii_isolate(SnapshotBuilder::GetEmbeddedSnapshotData());
  Isolate* isolate = raii_isolate.get();

  v8::Isolate::Scope isolate_scope(isolate);
  HandleScope handle_scope(isolate);

  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  errors::PrinterTryCatch bootstrapCatch(
      isolate, errors::PrinterTryCatch::kPrintSourceLine);

  Local<String> filename;
  if (!String::NewFromUtf8(isolate,
                           main_path.data(),
                           NewStringType::kNormal,
                           main_path.length())
           .ToLocal(&filename)) {
    return std::nullopt;
  }

  Local<String> content;
  if (!String::NewFromUtf8(isolate,
                           main_script.data(),
                           NewStringType::kNormal,
                           main_script.length())
           .ToLocal(&content)) {
    return std::nullopt;
  }

  std::vector<Local<String>> parameters = {
      FIXED_ONE_BYTE_STRING(isolate, "exports"),
      FIXED_ONE_BYTE_STRING(isolate, "require"),
      FIXED_ONE_BYTE_STRING(isolate, "module"),
      FIXED_ONE_BYTE_STRING(isolate, "__filename"),
      FIXED_ONE_BYTE_STRING(isolate, "__dirname"),
  };

  // TODO(RaisinTen): Using the V8 code cache prevents us from using `import()`
  // in the SEA code. Support it.
  // Refs: https://github.com/nodejs/node/pull/48191#discussion_r1213271430
  Local<Function> fn;
  if (!contextify::CompileFunction(context, filename, content, &parameters)
           .ToLocal(&fn)) {
    return std::nullopt;
  }

  std::unique_ptr<ScriptCompiler::CachedData> cache{
      ScriptCompiler::CreateCodeCacheForFunction(fn)};
  std::string code_cache(cache->data, cache->data + cache->length);
  return code_cache;
}

ExitCode GenerateSingleExecutableBlob(
    const SeaConfig& config,
    const std::vector<std::string>& args,
    const std::vector<std::string>& exec_args) {
  std::string main_script;
  // TODO(joyeecheung): unify the file utils.
  int r = ReadFileSync(&main_script, config.main_path.c_str());
  if (r != 0) {
    const char* err = uv_strerror(r);
    FPrintF(stderr, "Cannot read main script %s:%s\n", config.main_path, err);
    return ExitCode::kGenericUserError;
  }

  std::vector<char> snapshot_blob;
  bool builds_snapshot_from_main =
      static_cast<bool>(config.flags & SeaFlags::kUseSnapshot);
  if (builds_snapshot_from_main) {
    // TODO(joyeecheung): allow passing snapshot configuration in SEA configs.
    SnapshotConfig snapshot_config;
    snapshot_config.builder_script_path = main_script;
    ExitCode exit_code = GenerateSnapshotForSEA(
        config, args, exec_args, main_script, snapshot_config, &snapshot_blob);
    if (exit_code != ExitCode::kNoFailure) {
      return exit_code;
    }
  }

  std::optional<std::string_view> optional_sv_code_cache;
  std::string code_cache;
  if (static_cast<bool>(config.flags & SeaFlags::kUseCodeCache)) {
    if (builds_snapshot_from_main) {
      FPrintF(stderr,
              "\"useCodeCache\" is redundant when \"useSnapshot\" is true\n");
    } else {
      std::optional<std::string> optional_code_cache =
          GenerateCodeCache(config.main_path, main_script);
      if (!optional_code_cache.has_value()) {
        FPrintF(stderr, "Cannot generate V8 code cache\n");
        return ExitCode::kGenericUserError;
      }
      code_cache = optional_code_cache.value();
      optional_sv_code_cache = code_cache;
    }
  }

  SeaResource sea{
      config.flags,
      config.main_path,
      builds_snapshot_from_main
          ? std::string_view{snapshot_blob.data(), snapshot_blob.size()}
          : std::string_view{main_script.data(), main_script.size()},
      optional_sv_code_cache};

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

ExitCode BuildSingleExecutableBlob(const std::string& config_path,
                                   const std::vector<std::string>& args,
                                   const std::vector<std::string>& exec_args) {
  std::optional<SeaConfig> config_opt =
      ParseSingleExecutableConfig(config_path);
  if (config_opt.has_value()) {
    ExitCode code =
        GenerateSingleExecutableBlob(config_opt.value(), args, exec_args);
    return code;
  }

  return ExitCode::kGenericUserError;
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  SetMethod(context, target, "isSea", IsSea);
  SetMethod(context,
            target,
            "isExperimentalSeaWarningNeeded",
            IsExperimentalSeaWarningNeeded);
  SetMethod(context, target, "getCodePath", GetCodePath);
  SetMethod(context, target, "getCodeCache", GetCodeCache);
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(IsSea);
  registry->Register(IsExperimentalSeaWarningNeeded);
  registry->Register(GetCodePath);
  registry->Register(GetCodeCache);
}

}  // namespace sea
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(sea, node::sea::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(sea, node::sea::RegisterExternalReferences)

#endif  // !defined(DISABLE_SINGLE_EXECUTABLE_APPLICATION)
