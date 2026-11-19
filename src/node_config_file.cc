#include "node_config_file.h"
#include "debug_utils-inl.h"
#include "node_json.h"
#include "node_version.h"

#include <algorithm>
#include <cinttypes>

namespace node {

constexpr std::string_view kConfigFileFlag = "--config-file";
// TODO(marco-ippolito): Remove these aliases in a semver-major.
constexpr std::string_view kExperimentalConfigFileFlag =
    "--experimental-config-file";
constexpr std::string_view kDefaultConfigFileFlag =
    "--experimental-default-config-file";
constexpr std::string_view kDefaultConfigFileName = "node.config.json";
constexpr std::string_view kSchemaField = "$schema";

inline bool HasEqualsPrefix(std::string_view arg, std::string_view flag) {
  return arg.size() > flag.size() && arg.starts_with(flag) &&
         arg[flag.size()] == '=';
}

inline bool IsBareConfigFileFlag(std::string_view arg) {
  return arg == kConfigFileFlag || arg == kExperimentalConfigFileFlag ||
         arg == kDefaultConfigFileFlag;
}

inline bool IsConfigFileFlag(std::string_view arg) {
  return IsBareConfigFileFlag(arg) || HasEqualsPrefix(arg, kConfigFileFlag) ||
         HasEqualsPrefix(arg, kExperimentalConfigFileFlag) ||
         HasEqualsPrefix(arg, kDefaultConfigFileFlag);
}

inline std::string DefaultConfigFileArg() {
  return std::string(kConfigFileFlag) + "=" +
         std::string(kDefaultConfigFileName);
}

// Returns the index of the first entry in `args` that is neither the program
// name nor an option for Node.js itself. Everything from there on is the
// script and its arguments, or follows `--`, so it must not be read as a
// config file flag.
size_t GetNodeOptionsEnd(const std::vector<std::string>& args) {
  // The bare flags take no value. Spell them out the way GetDataFromArgs()
  // does before the real parse, otherwise the parser would consume the next
  // argument as the path.
  std::vector<std::string> remaining;
  remaining.reserve(args.size());
  for (const std::string& arg : args) {
    remaining.push_back(IsBareConfigFileFlag(arg) ? DefaultConfigFileArg()
                                                  : arg);
  }

  // Parsing into throwaway options stops exactly where the real parse will.
  // Any errors are reported by the real parse later on.
  PerProcessOptions options;
  std::vector<std::string> v8_args;
  std::vector<std::string> errors;
  options_parser::Parse(
      &remaining, nullptr, &v8_args, &options, kDisallowedInEnvvar, &errors);

  // `remaining` is left with the program name and the unparsed arguments.
  return args.size() - remaining.size() + 1;
}

std::optional<std::string_view> ConfigReader::GetDataFromArgs(
    std::vector<std::string>* args) {
  std::optional<std::string_view> result;
  invalid_default_config_file_argument_ = false;

  // Only pay for the extra parse when a config file flag might be present.
  if (args->empty() || std::ranges::none_of(*args, IsConfigFileFlag)) {
    return result;
  }

  const size_t node_options_end = GetNodeOptionsEnd(*args);
  for (size_t i = 1; i < node_options_end; ++i) {
    std::string& arg = (*args)[i];

    if (IsBareConfigFileFlag(arg)) {
      // --config-file, --experimental-config-file or
      // --experimental-default-config-file
      arg = DefaultConfigFileArg();
      result = kDefaultConfigFileName;
    } else if (HasEqualsPrefix(arg, kConfigFileFlag) ||
               HasEqualsPrefix(arg, kExperimentalConfigFileFlag)) {
      // --config-file=path or --experimental-config-file=path
      std::string_view path = std::string_view(arg).substr(arg.find('=') + 1);
      if (!path.empty()) {
        result = path;
      }
    } else if (HasEqualsPrefix(arg, kDefaultConfigFileFlag)) {
      invalid_default_config_file_argument_ = true;
    }
  }

  return result;
}

bool ConfigReader::HasInvalidDefaultConfigFileArgument() const {
  return invalid_default_config_file_argument_;
}

ParseResult ConfigReader::ProcessOptionValue(
    const std::pair<std::string, options_parser::OptionMappingDetails>&
        option_details,
    const json::Value& option_value,
    std::vector<std::string>* output) {
  const std::string& option_name = option_details.first;
  const options_parser::OptionType option_type = option_details.second.type;

  switch (option_type) {
    case options_parser::OptionType::kBoolean: {
      bool result;
      if (!option_value.GetBool(&result)) {
        FPrintF(stderr, "Invalid value for %s\n", option_name);
        return ParseResult::InvalidContent;
      }

      if (result) {
        // If the value is true, we need to set the flag
        output->push_back(option_name);
      } else {
        // Ensure negation is made putting the "--no-" prefix
        output->push_back("--no-" +
                          option_name.substr(2, option_name.size() - 2));
      }

      break;
    }
    // String array can allow both string and array types
    case options_parser::OptionType::kStringList: {
      switch (option_value.type()) {
        case json::ValueType::kArray: {
          for (size_t i = 0; i < option_value.size(); ++i) {
            std::string_view import;
            if (!option_value.At(i).GetString(&import)) {
              FPrintF(stderr, "Invalid value for %s\n", option_name);
              return ParseResult::InvalidContent;
            }
            output->push_back(option_name + "=" + std::string(import));
          }
          break;
        }
        case json::ValueType::kString: {
          std::string_view result;
          CHECK(option_value.GetString(&result));
          output->push_back(option_name + "=" + std::string(result));
          break;
        }
        default:
          FPrintF(stderr, "Invalid value for %s\n", option_name);
          return ParseResult::InvalidContent;
      }
      break;
    }
    case options_parser::OptionType::kString: {
      std::string_view result;
      if (!option_value.GetString(&result)) {
        FPrintF(stderr, "Invalid value for %s\n", option_name);
        return ParseResult::InvalidContent;
      }
      output->push_back(option_name + "=" + std::string(result));
      break;
    }
    case options_parser::OptionType::kInteger: {
      int64_t result;
      if (!option_value.GetInt64(&result)) {
        FPrintF(stderr, "Invalid value for %s\n", option_name);
        return ParseResult::InvalidContent;
      }
      output->push_back(option_name + "=" + std::to_string(result));
      break;
    }
    case options_parser::OptionType::kHostPort:
    case options_parser::OptionType::kUInteger: {
      uint64_t result;
      if (!option_value.GetUint64(&result)) {
        FPrintF(stderr, "Invalid value for %s\n", option_name);
        return ParseResult::InvalidContent;
      }
      output->push_back(option_name + "=" + std::to_string(result));
      break;
    }
    case options_parser::OptionType::kNoOp: {
      FPrintF(
          stderr, "No-op flag %s is currently not supported\n", option_name);
      return ParseResult::InvalidContent;
    }
    case options_parser::OptionType::kV8Option: {
      FPrintF(stderr, "V8 flag %s is currently not supported\n", option_name);
      return ParseResult::InvalidContent;
    }
    default:
      UNREACHABLE();
  }
  return ParseResult::Valid;
}

ParseResult ConfigReader::ParseOptions(
    const json::Value& options_object,
    std::unordered_set<std::string>* unique_options,
    const std::string& namespace_name) {
  // Determine which options map to use and output vector
  std::unordered_map<std::string, options_parser::OptionMappingDetails>
      options_map;
  std::vector<std::string>* output_vector;

  if (namespace_name == "nodeOptions") {
    // Special case for backward compatibility: handle nodeOptions with env
    // options map
    options_map = options_parser::MapEnvOptionsFlagInputType();
    output_vector = &node_options_;
  } else {
    // Handle other namespaces
    options_map = options_parser::MapOptionsByNamespace(namespace_name);
    output_vector = &namespace_options_;

    if (!env_options_initialized_) {
      env_options_map_ = options_parser::MapEnvOptionsFlagInputType();
      env_options_initialized_ = true;
    }
  }

  for (size_t i = 0; i < options_object.size(); ++i) {
    std::string_view option_key = options_object.KeyAt(i);
    json::Value option_value = options_object.At(i);

    // The key needs to match the CLI option
    std::string prefix = "--";
    auto option = options_map.find(prefix.append(option_key));
    if (option != options_map.end()) {
      // If the option has already been set, return an error
      if (unique_options->contains(option->first)) {
        FPrintF(stderr, "Option %s is already defined\n", option->first);
        return ParseResult::InvalidContent;
      }
      // Add the option to the unique set to prevent duplicates
      // on future iterations
      unique_options->insert(option->first);
      // Process the option value based on its type
      ParseResult result =
          ProcessOptionValue(*option, option_value, output_vector);
      if (result != ParseResult::Valid) {
        return result;
      }
    } else {
      FPrintF(stderr,
              "Unknown or not allowed option %s for namespace %s\n",
              option_key,
              namespace_name);
      return ParseResult::InvalidContent;
    }
  }
  return ParseResult::Valid;
}

ParseResult ConfigReader::ParseNodeVersion(
    const json::Value& version_value, const std::string_view& config_path) {
  int64_t version;
  if (!version_value.GetInt64(&version)) {
    FPrintF(stderr,
            "\"nodeVersion\" value unexpected for %s "
            "(should be an integer)\n",
            config_path.data());
    return ParseResult::InvalidContent;
  }

  if (version != NODE_MAJOR_VERSION) {
    FPrintF(stderr,
            "\"nodeVersion\" %" PRId64
            " does not match current Node.js version %d "
            "for %s\n",
            version,
            NODE_MAJOR_VERSION,
            config_path.data());
    return ParseResult::InvalidContent;
  }

  return ParseResult::Valid;
}

ParseResult ConfigReader::ParseConfigs(const json::Value& configs,
                                       const std::string_view& config_path) {
  for (size_t index = 0; index < configs.size(); ++index) {
    json::Value config_wrapper = configs.At(index);
    if (!config_wrapper.IsObject()) {
      FPrintF(stderr,
              "\"configs[%zu]\" value unexpected for %s "
              "(should be an object)\n",
              index,
              config_path.data());
      return ParseResult::InvalidContent;
    }

    json::Value version_value = config_wrapper.Find("nodeVersion");
    if (!version_value) {
      FPrintF(stderr,
              "\"configs[%zu].nodeVersion\" is required for %s\n",
              index,
              config_path.data());
      return ParseResult::InvalidContent;
    }

    int64_t version;
    if (!version_value.GetInt64(&version)) {
      FPrintF(stderr,
              "\"configs[%zu].nodeVersion\" value unexpected for %s "
              "(should be an integer)\n",
              index,
              config_path.data());
      return ParseResult::InvalidContent;
    }

    if (version != NODE_MAJOR_VERSION) {
      continue;
    }

    json::Value config_value = config_wrapper.Find("config");
    if (!config_value) {
      FPrintF(stderr,
              "\"configs[%zu].config\" is required for %s\n",
              index,
              config_path.data());
      return ParseResult::InvalidContent;
    }

    if (!config_value.IsObject()) {
      FPrintF(stderr,
              "\"configs[%zu].config\" value unexpected for %s "
              "(should be an object)\n",
              index,
              config_path.data());
      return ParseResult::InvalidContent;
    }

    return ParseConfigObject(config_value, config_path, false);
  }

  FPrintF(stderr,
          "No config found for current Node.js version %d in "
          "\"configs\" for %s\n",
          NODE_MAJOR_VERSION,
          config_path.data());
  return ParseResult::InvalidContent;
}

ParseResult ConfigReader::ParseConfigObject(const json::Value& config_object,
                                            const std::string_view& config_path,
                                            bool allow_version_selection) {
  // Get all available namespaces for validation
  std::vector<std::string> available_namespaces =
      options_parser::MapAvailableNamespaces();
  // Add "nodeOptions" as a special case for backward compatibility
  available_namespaces.emplace_back("nodeOptions");

  // Create a set for faster lookup of valid namespaces
  std::unordered_set<std::string> valid_namespaces(available_namespaces.begin(),
                                                   available_namespaces.end());
  // Create a set to track unique options
  std::unordered_set<std::string> unique_options;
  // Namespaces in OPTION_NAMESPACE_LIST
  std::unordered_set<std::string> namespaces_with_implicit_flags;

  // Iterate through the main object to find all namespaces
  for (size_t i = 0; i < config_object.size(); ++i) {
    std::string_view field_name = config_object.KeyAt(i);
    json::Value field = config_object.At(i);

    std::string namespace_name(field_name);

    if (namespace_name == "$schema") {
      continue;
    }

    if (namespace_name == "nodeVersion") {
      ParseResult result = ParseNodeVersion(field, config_path);
      if (result != ParseResult::Valid) {
        return result;
      }
      continue;
    }

    if (namespace_name == "configs") {
      if (!allow_version_selection) {
        FPrintF(stderr,
                "\"configs\" is not allowed inside a versioned config "
                "for %s\n",
                config_path.data());
        return ParseResult::InvalidContent;
      }

      if (!field.IsArray()) {
        FPrintF(stderr,
                "\"configs\" value unexpected for %s "
                "(should be an array)\n",
                config_path.data());
        return ParseResult::InvalidContent;
      }
      ParseResult result = ParseConfigs(field, config_path);
      if (result != ParseResult::Valid) {
        return result;
      }
      continue;
    }

    // TODO(@marco-ippolito): Remove warning for testRunner namespace
    if (namespace_name == "testRunner") {
      FPrintF(stderr,
              "the \"testRunner\" namespace has been removed. "
              "Use \"test\" instead.\n");
      // Better to throw an error than to ignore it
      // Otherwise users might think their test suite is green
      // when it's not running
      return ParseResult::InvalidContent;
    }

    if (namespace_name == kSchemaField) {
      continue;
    }

    // Check if this field is a valid namespace
    if (!valid_namespaces.contains(namespace_name)) {
      FPrintF(stderr, "Unknown namespace %s\n", namespace_name);
      return ParseResult::InvalidContent;
    }

    // List of implicit namespace flags
    for (auto ns_enum : options_parser::AllNamespaces()) {
      std::string ns_str = options_parser::NamespaceEnumToString(ns_enum);
      if (!ns_str.empty() && namespace_name == ns_str) {
        namespaces_with_implicit_flags.insert(namespace_name);
        break;
      }
    }

    // If namespace value is not an object
    if (!field.IsObject()) {
      FPrintF(stderr,
              "\"%s\" value unexpected for %s (should be an object)\n",
              namespace_name,
              config_path.data());
      return ParseResult::InvalidContent;
    }

    // Process options for this namespace using the unified method
    ParseResult result = ParseOptions(field, &unique_options, namespace_name);
    if (result != ParseResult::Valid) {
      return result;
    }
  }

  // Add implicit flags for namespaces (--bench, --test, --permission, --watch)
  // These flags are automatically enabled when their namespace is present
  for (const auto& ns : namespaces_with_implicit_flags) {
    std::string flag = "--" + ns;
    std::string no_flag = "--no-" + ns;
    // We skip if the user has already set the flag or its negation
    if (!unique_options.contains(flag) && !unique_options.contains(no_flag)) {
      namespace_options_.push_back(flag);
    }
  }

  return ParseResult::Valid;
}

ParseResult ConfigReader::ParseConfig(const std::string_view& config_path) {
  std::string file_content;
  // Read the configuration file
  int r = ReadFileSync(&file_content, config_path.data());
  if (r != 0) {
    const char* err = uv_strerror(r);
    FPrintF(
        stderr, "Cannot read configuration from %s: %s\n", config_path, err);
    return ParseResult::FileError;
  }

  // Parse the configuration file
  json::Document document;
  if (!document.Parse(file_content)) {
    FPrintF(stderr, "Can't parse %s\n", config_path.data());
    return ParseResult::InvalidContent;
  }

  // Validate config is an object
  json::Value main_object = document.root();
  if (!main_object.IsObject()) {
    FPrintF(stderr,
            "Root value unexpected not an object for %s\n\n",
            config_path.data());
    return ParseResult::InvalidContent;
  }

  bool has_configs = false;
  bool has_other_fields = false;
  for (size_t i = 0; i < main_object.size(); ++i) {
    std::string_view field_name = main_object.KeyAt(i);

    if (field_name == "$schema") {
      continue;
    }

    if (field_name == "configs") {
      has_configs = true;
    } else {
      has_other_fields = true;
    }
  }

  if (has_configs && has_other_fields) {
    FPrintF(stderr,
            "\"configs\" cannot be mixed with other configuration fields "
            "for %s\n",
            config_path.data());
    return ParseResult::InvalidContent;
  }

  return ParseConfigObject(main_object, config_path, true);
}

std::string ConfigReader::GetNodeOptions() {
  std::string acc = "";
  const size_t total_options = node_options_.size();
  acc.reserve(total_options * 2);
  for (auto& opt : node_options_) {
    acc += " " + opt;
  }
  return acc;
}

const std::vector<std::string>& ConfigReader::GetNamespaceFlags() const {
  return namespace_options_;
}

size_t ConfigReader::GetFlagsSize() {
  return node_options_.size();
}
}  // namespace node
