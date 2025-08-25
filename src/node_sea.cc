#include "node_sea.h"

#include "blob_serializer_deserializer-inl.h"
#include "debug_utils-inl.h"
#include "env-inl.h"
#include "node_contextify.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "node_internals.h"
#include "node_options.h"
#include "node_snapshot_builder.h"
#include "node_union_bytes.h"
#include "node_v8_platform-inl.h"
#include "simdjson.h"
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

using node::ExitCode;
using v8::ArrayBuffer;
using v8::BackingStore;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::LocalVector;
using v8::MaybeLocal;
using v8::NewStringType;
using v8::Object;
using v8::ScriptCompiler;
using v8::ScriptOrigin;
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

  Debug("Write SEA resource exec argv extension %u\n",
        static_cast<uint8_t>(sea.exec_argv_extension));
  written_total +=
      WriteArithmetic<uint8_t>(static_cast<uint8_t>(sea.exec_argv_extension));
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

  if (!sea.assets.empty()) {
    Debug("Write SEA resource assets size %zu\n", sea.assets.size());
    written_total += WriteArithmetic<size_t>(sea.assets.size());
    for (auto const& [key, content] : sea.assets) {
      Debug("Write SEA resource asset %s at %p, size=%zu\n",
            key,
            content.data(),
            content.size());
      written_total += WriteStringView(key, StringLogMode::kAddressAndContent);
      written_total += WriteStringView(content, StringLogMode::kAddressOnly);
    }
  }

  if (static_cast<bool>(sea.flags & SeaFlags::kIncludeExecArgv)) {
    Debug("Write SEA resource exec argv size %zu\n", sea.exec_argv.size());
    written_total += WriteArithmetic<size_t>(sea.exec_argv.size());
    for (const auto& arg : sea.exec_argv) {
      Debug("Write SEA resource exec arg %s at %p, size=%zu\n",
            arg.data(),
            arg.data(),
            arg.size());
      written_total += WriteStringView(arg, StringLogMode::kAddressAndContent);
    }
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

  uint8_t extension_value = ReadArithmetic<uint8_t>();
  SeaExecArgvExtension exec_argv_extension =
      static_cast<SeaExecArgvExtension>(extension_value);
  Debug("Read SEA resource exec argv extension %u\n", extension_value);
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

  std::unordered_map<std::string_view, std::string_view> assets;
  if (static_cast<bool>(flags & SeaFlags::kIncludeAssets)) {
    size_t assets_size = ReadArithmetic<size_t>();
    Debug("Read SEA resource assets size %zu\n", assets_size);
    for (size_t i = 0; i < assets_size; ++i) {
      std::string_view key = ReadStringView(StringLogMode::kAddressAndContent);
      std::string_view content = ReadStringView(StringLogMode::kAddressOnly);
      Debug("Read SEA resource asset %s at %p, size=%zu\n",
            key,
            content.data(),
            content.size());
      assets.emplace(key, content);
    }
  }

  std::vector<std::string_view> exec_argv;
  if (static_cast<bool>(flags & SeaFlags::kIncludeExecArgv)) {
    size_t exec_argv_size = ReadArithmetic<size_t>();
    Debug("Read SEA resource exec args size %zu\n", exec_argv_size);
    exec_argv.reserve(exec_argv_size);
    for (size_t i = 0; i < exec_argv_size; ++i) {
      std::string_view arg = ReadStringView(StringLogMode::kAddressAndContent);
      Debug("Read SEA resource exec arg %s at %p, size=%zu\n",
            arg.data(),
            arg.data(),
            arg.size());
      exec_argv.emplace_back(arg);
    }
  }
  return {flags,
          exec_argv_extension,
          code_path,
          code,
          code_cache,
          assets,
          exec_argv};
}

std::string_view FindSingleExecutableBlob() {
#if !defined(DISABLE_SINGLE_EXECUTABLE_APPLICATION)
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
#else
  UNREACHABLE();
#endif  // !defined(DISABLE_SINGLE_EXECUTABLE_APPLICATION)
}

}  // anonymous namespace

bool SeaResource::use_snapshot() const {
  return static_cast<bool>(flags & SeaFlags::kUseSnapshot);
}

bool SeaResource::use_code_cache() const {
  return static_cast<bool>(flags & SeaFlags::kUseCodeCache);
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

std::tuple<int, char**> FixupArgsForSEA(int argc, char** argv) {
  // Repeats argv[0] at position 1 on argv as a replacement for the missing
  // entry point file path.
  if (IsSingleExecutable()) {
    static std::vector<char*> new_argv;
    static std::vector<std::string> exec_argv_storage;
    static std::vector<std::string> cli_extension_args;

    SeaResource sea_resource = FindSingleExecutableResource();

    new_argv.clear();
    exec_argv_storage.clear();
    cli_extension_args.clear();

    // Handle CLI extension mode for --node-options
    if (sea_resource.exec_argv_extension == SeaExecArgvExtension::kCli) {
      // Extract --node-options and filter argv
      for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "--node-options=", 15) == 0) {
          std::string node_options = argv[i] + 15;
          std::vector<std::string> errors;
          cli_extension_args = ParseNodeOptionsEnvVar(node_options, &errors);
          // Remove this argument by shifting the rest
          for (int j = i; j < argc - 1; ++j) {
            argv[j] = argv[j + 1];
          }
          argc--;
          i--;  // Adjust index since we removed an element
        }
      }
    }

    // Reserve space for argv[0], exec argv, cli extension args, original argv,
    // and nullptr
    new_argv.reserve(argc + sea_resource.exec_argv.size() +
                     cli_extension_args.size() + 2);
    new_argv.emplace_back(argv[0]);

    // Insert exec argv from SEA config
    if (!sea_resource.exec_argv.empty()) {
      exec_argv_storage.reserve(sea_resource.exec_argv.size() +
                                cli_extension_args.size());
      for (const auto& arg : sea_resource.exec_argv) {
        exec_argv_storage.emplace_back(arg);
        new_argv.emplace_back(exec_argv_storage.back().data());
      }
    }

    // Insert CLI extension args
    for (const auto& arg : cli_extension_args) {
      exec_argv_storage.emplace_back(arg);
      new_argv.emplace_back(exec_argv_storage.back().data());
    }

    // Add actual run time arguments
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
  SeaExecArgvExtension exec_argv_extension = SeaExecArgvExtension::kEnv;
  std::unordered_map<std::string, std::string> assets;
  std::vector<std::string> exec_argv;
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

  simdjson::ondemand::parser parser;
  simdjson::ondemand::document document;
  simdjson::ondemand::object main_object;
  simdjson::error_code error =
      parser.iterate(simdjson::pad(config)).get(document);

  if (!error) {
    error = document.get_object().get(main_object);
  }
  if (error) {
    FPrintF(stderr,
            "Cannot parse JSON from %s: %s\n",
            config_path,
            simdjson::error_message(error));
    return std::nullopt;
  }

  bool use_snapshot_value = false;
  bool use_code_cache_value = false;

  for (auto field : main_object) {
    std::string_view key;
    if (field.unescaped_key().get(key)) {
      FPrintF(stderr, "Cannot read key from %s\n", config_path);
      return std::nullopt;
    }
    if (key == "main") {
      if (field.value().get_string().get(result.main_path) ||
          result.main_path.empty()) {
        FPrintF(stderr,
                "\"main\" field of %s is not a non-empty string\n",
                config_path);
        return std::nullopt;
      }
    } else if (key == "output") {
      if (field.value().get_string().get(result.output_path) ||
          result.output_path.empty()) {
        FPrintF(stderr,
                "\"output\" field of %s is not a non-empty string\n",
                config_path);
        return std::nullopt;
      }
    } else if (key == "disableExperimentalSEAWarning") {
      bool disable_experimental_sea_warning;
      if (field.value().get_bool().get(disable_experimental_sea_warning)) {
        FPrintF(
            stderr,
            "\"disableExperimentalSEAWarning\" field of %s is not a Boolean\n",
            config_path);
        return std::nullopt;
      }
      if (disable_experimental_sea_warning) {
        result.flags |= SeaFlags::kDisableExperimentalSeaWarning;
      }
    } else if (key == "useSnapshot") {
      if (field.value().get_bool().get(use_snapshot_value)) {
        FPrintF(stderr,
                "\"useSnapshot\" field of %s is not a Boolean\n",
                config_path);
        return std::nullopt;
      }
      if (use_snapshot_value) {
        result.flags |= SeaFlags::kUseSnapshot;
      }
    } else if (key == "useCodeCache") {
      if (field.value().get_bool().get(use_code_cache_value)) {
        FPrintF(stderr,
                "\"useCodeCache\" field of %s is not a Boolean\n",
                config_path);
        return std::nullopt;
      }
      if (use_code_cache_value) {
        result.flags |= SeaFlags::kUseCodeCache;
      }
    } else if (key == "assets") {
      simdjson::ondemand::object assets_object;
      if (field.value().get_object().get(assets_object)) {
        FPrintF(stderr,
                "\"assets\" field of %s is not a map of strings\n",
                config_path);
        return std::nullopt;
      }
      simdjson::ondemand::value asset_value;
      for (auto asset_field : assets_object) {
        std::string_view key_str;
        std::string_view value_str;
        if (asset_field.unescaped_key().get(key_str) ||
            asset_field.value().get(asset_value) ||
            asset_value.get_string().get(value_str)) {
          FPrintF(stderr,
                  "\"assets\" field of %s is not a map of strings\n",
                  config_path);
          return std::nullopt;
        }

        result.assets.emplace(key_str, value_str);
      }

      if (!result.assets.empty()) {
        result.flags |= SeaFlags::kIncludeAssets;
      }
    } else if (key == "execArgv") {
      simdjson::ondemand::array exec_argv_array;
      if (field.value().get_array().get(exec_argv_array)) {
        FPrintF(stderr,
                "\"execArgv\" field of %s is not an array of strings\n",
                config_path);
        return std::nullopt;
      }
      std::vector<std::string> exec_argv;
      for (auto argv : exec_argv_array) {
        std::string_view argv_str;
        if (argv.get_string().get(argv_str)) {
          FPrintF(stderr,
                  "\"execArgv\" field of %s is not an array of strings\n",
                  config_path);
          return std::nullopt;
        }
        exec_argv.emplace_back(argv_str);
      }
      if (!exec_argv.empty()) {
        result.flags |= SeaFlags::kIncludeExecArgv;
        result.exec_argv = std::move(exec_argv);
      }
    } else if (key == "execArgvExtension") {
      std::string_view extension_str;
      if (field.value().get_string().get(extension_str)) {
        FPrintF(stderr,
                "\"execArgvExtension\" field of %s is not a string\n",
                config_path);
        return std::nullopt;
      }
      if (extension_str == "none") {
        result.exec_argv_extension = SeaExecArgvExtension::kNone;
      } else if (extension_str == "env") {
        result.exec_argv_extension = SeaExecArgvExtension::kEnv;
      } else if (extension_str == "cli") {
        result.exec_argv_extension = SeaExecArgvExtension::kCli;
      } else {
        FPrintF(stderr,
                "\"execArgvExtension\" field of %s must be one of "
                "\"none\", \"env\", or \"cli\"\n",
                config_path);
        return std::nullopt;
      }
    }
  }

  if (static_cast<bool>(result.flags & SeaFlags::kUseSnapshot) &&
      static_cast<bool>(result.flags & SeaFlags::kUseCodeCache)) {
    // TODO(joyeecheung): code cache in snapshot should be configured by
    // separate snapshot configurations.
    FPrintF(stderr,
            "\"useCodeCache\" is redundant when \"useSnapshot\" is true\n");
  }

  if (result.main_path.empty()) {
    FPrintF(stderr,
            "\"main\" field of %s is not a non-empty string\n",
            config_path);
    return std::nullopt;
  }

  if (result.output_path.empty()) {
    FPrintF(stderr,
            "\"output\" field of %s is not a non-empty string\n",
            config_path);
    return std::nullopt;
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
  auto it = std::ranges::find_if(persistents, [](const PropInfo& prop) {
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

  LocalVector<String> parameters(
      isolate,
      {
          FIXED_ONE_BYTE_STRING(isolate, "exports"),
          FIXED_ONE_BYTE_STRING(isolate, "require"),
          FIXED_ONE_BYTE_STRING(isolate, "module"),
          FIXED_ONE_BYTE_STRING(isolate, "__filename"),
          FIXED_ONE_BYTE_STRING(isolate, "__dirname"),
      });
  ScriptOrigin script_origin(filename, 0, 0, true);
  ScriptCompiler::Source script_source(content, script_origin);
  MaybeLocal<Function> maybe_fn =
      ScriptCompiler::CompileFunction(context,
                                      &script_source,
                                      parameters.size(),
                                      parameters.data(),
                                      0,
                                      nullptr);
  Local<Function> fn;
  if (!maybe_fn.ToLocal(&fn)) {
    return std::nullopt;
  }

  // TODO(RaisinTen): Using the V8 code cache prevents us from using `import()`
  // in the SEA code. Support it.
  // Refs: https://github.com/nodejs/node/pull/48191#discussion_r1213271430
  std::unique_ptr<ScriptCompiler::CachedData> cache{
      ScriptCompiler::CreateCodeCacheForFunction(fn)};
  std::string code_cache(cache->data, cache->data + cache->length);
  return code_cache;
}

int BuildAssets(const std::unordered_map<std::string, std::string>& config,
                std::unordered_map<std::string, std::string>* assets) {
  for (auto const& [key, path] : config) {
    std::string blob;
    int r = ReadFileSync(&blob, path.c_str());
    if (r != 0) {
      const char* err = uv_strerror(r);
      FPrintF(stderr, "Cannot read asset %s: %s\n", path.c_str(), err);
      return r;
    }
    assets->emplace(key, std::move(blob));
  }
  return 0;
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
    std::optional<std::string> optional_code_cache =
        GenerateCodeCache(config.main_path, main_script);
    if (!optional_code_cache.has_value()) {
      FPrintF(stderr, "Cannot generate V8 code cache\n");
      return ExitCode::kGenericUserError;
    }
    code_cache = optional_code_cache.value();
    optional_sv_code_cache = code_cache;
  }

  std::unordered_map<std::string, std::string> assets;
  if (!config.assets.empty() && BuildAssets(config.assets, &assets) != 0) {
    return ExitCode::kGenericUserError;
  }
  std::unordered_map<std::string_view, std::string_view> assets_view;
  for (auto const& [key, content] : assets) {
    assets_view.emplace(key, content);
  }
  std::vector<std::string_view> exec_argv_view;
  for (const auto& arg : config.exec_argv) {
    exec_argv_view.emplace_back(arg);
  }
  SeaResource sea{
      config.flags,
      config.exec_argv_extension,
      config.main_path,
      builds_snapshot_from_main
          ? std::string_view{snapshot_blob.data(), snapshot_blob.size()}
          : std::string_view{main_script.data(), main_script.size()},
      optional_sv_code_cache,
      assets_view,
      exec_argv_view};

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

void GetAsset(const FunctionCallbackInfo<Value>& args) {
  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsString());
  Utf8Value key(args.GetIsolate(), args[0]);
  SeaResource sea_resource = FindSingleExecutableResource();
  if (sea_resource.assets.empty()) {
    return;
  }
  auto it = sea_resource.assets.find(*key);
  if (it == sea_resource.assets.end()) {
    return;
  }
  // We cast away the constness here, the JS land should ensure that
  // the data is not mutated.
  std::unique_ptr<v8::BackingStore> store = ArrayBuffer::NewBackingStore(
      const_cast<char*>(it->second.data()),
      it->second.size(),
      [](void*, size_t, void*) {},
      nullptr);
  Local<ArrayBuffer> ab = ArrayBuffer::New(args.GetIsolate(), std::move(store));
  args.GetReturnValue().Set(ab);
}

MaybeLocal<Value> LoadSingleExecutableApplication(
    const StartExecutionCallbackInfo& info) {
  // Here we are currently relying on the fact that in NodeMainInstance::Run(),
  // env->context() is entered.
  Local<Context> context = Isolate::GetCurrent()->GetCurrentContext();
  Environment* env = Environment::GetCurrent(context);
  SeaResource sea = FindSingleExecutableResource();

  CHECK(!sea.use_snapshot());
  // TODO(joyeecheung): this should be an external string. Refactor UnionBytes
  // and make it easy to create one based on static content on the fly.
  Local<Value> main_script =
      ToV8Value(env->context(), sea.main_code_or_snapshot).ToLocalChecked();
  return info.run_cjs->Call(
      env->context(), Null(env->isolate()), 1, &main_script);
}

bool MaybeLoadSingleExecutableApplication(Environment* env) {
#ifndef DISABLE_SINGLE_EXECUTABLE_APPLICATION
  if (!IsSingleExecutable()) {
    return false;
  }

  SeaResource sea = FindSingleExecutableResource();

  if (sea.use_snapshot()) {
    // The SEA preparation blob building process should already enforce this,
    // this check is just here to guard against the unlikely case where
    // the SEA preparation blob has been manually modified by someone.
    CHECK(!env->snapshot_deserialize_main().IsEmpty());
    LoadEnvironment(env, StartExecutionCallback{});
    return true;
  }

  LoadEnvironment(env, LoadSingleExecutableApplication);
  return true;
#else
  return false;
#endif
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
  SetMethod(context, target, "getAsset", GetAsset);
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(IsSea);
  registry->Register(IsExperimentalSeaWarningNeeded);
  registry->Register(GetAsset);
}

}  // namespace sea
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(sea, node::sea::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(sea, node::sea::RegisterExternalReferences)
