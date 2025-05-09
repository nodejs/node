#include "node_config_file.h"
#include "debug_utils-inl.h"
#include "simdjson.h"

#include <string>

namespace node {

std::optional<std::string_view> ConfigReader::GetDataFromArgs(
    const std::vector<std::string>& args) {
  constexpr std::string_view flag_path = "--experimental-config-file";
  constexpr std::string_view default_file =
      "--experimental-default-config-file";

  bool has_default_config_file = false;

  for (auto it = args.begin(); it != args.end(); ++it) {
    if (*it == flag_path) {
      // Case: "--experimental-config-file foo"
      if (auto next = std::next(it); next != args.end()) {
        return *next;
      }
    } else if (it->starts_with(flag_path)) {
      // Case: "--experimental-config-file=foo"
      if (it->size() > flag_path.size() && (*it)[flag_path.size()] == '=') {
        return it->substr(flag_path.size() + 1);
      }
    } else if (*it == default_file || it->starts_with(default_file)) {
      has_default_config_file = true;
    }
  }

  if (has_default_config_file) {
    return "node.config.json";
  }

  return std::nullopt;
}

ParseResult ConfigReader::ParseNodeOptions(
    simdjson::ondemand::object* node_options_object) {
  auto env_options_map = options_parser::MapEnvOptionsFlagInputType();
  simdjson::ondemand::value ondemand_value;
  std::string_view key;

  for (auto field : *node_options_object) {
    if (field.unescaped_key().get(key) || field.value().get(ondemand_value)) {
      return ParseResult::InvalidContent;
    }

    // The key needs to match the CLI option
    std::string prefix = "--";
    auto it = env_options_map.find(prefix.append(key));
    if (it != env_options_map.end()) {
      ParseResult result = ProcessOptionValue(key,
                                              it->first,
                                              ondemand_value,
                                              it->second,
                                              &node_options_,
                                              &unique_node_options_);
      if (result != ParseResult::Valid) {
        return result;
      }
    } else {
      FPrintF(stderr, "Unknown or not allowed option %s\n", key.data());
      return ParseResult::InvalidContent;
    }
  }
  return ParseResult::Valid;
}

ParseResult ConfigReader::ProcessOptionValue(
    const std::string_view& key,
    const std::string& option_name,
    simdjson::ondemand::value& ondemand_value,
    options_parser::OptionType option_type,
    std::vector<std::string>* output,
    std::unordered_set<std::string>* unique_options) {
  switch (option_type) {
    case options_parser::OptionType::kBoolean: {
      bool result;
      if (ondemand_value.get_bool().get(result)) {
        FPrintF(stderr, "Invalid value for %s\n", it->first.c_str());
        return ParseResult::InvalidContent;
      }

      if (result) {
        // If the value is true, we need to set the flag
        node_options_.push_back(it->first);
      }

      break;
    }
    // String array can allow both string and array types
    case options_parser::OptionType::kStringList: {
      simdjson::ondemand::json_type field_type;
      if (ondemand_value.type().get(field_type)) {
        return ParseResult::InvalidContent;
      }
      switch (field_type) {
        case simdjson::ondemand::json_type::array: {
          std::vector<std::string> result;
          simdjson::ondemand::array raw_imports;
          if (ondemand_value.get_array().get(raw_imports)) {
            FPrintF(stderr, "Invalid value for %s\n", option_name.c_str());
            return ParseResult::InvalidContent;
          }
          for (auto raw_import : raw_imports) {
            std::string_view import;
            if (raw_import.get_string(import)) {
              FPrintF(stderr, "Invalid value for %s\n", option_name.c_str());
              return ParseResult::InvalidContent;
            }
            output->push_back(option_name + "=" + std::string(import));
          }
          break;
        }
        case simdjson::ondemand::json_type::string: {
          std::string result;
          if (ondemand_value.get_string(result)) {
            FPrintF(stderr, "Invalid value for %s\n", option_name.c_str());
            return ParseResult::InvalidContent;
          }
          output->push_back(option_name + "=" + result);
          break;
        }
        default:
          FPrintF(stderr, "Invalid value for %s\n", option_name.c_str());
          return ParseResult::InvalidContent;
      }
      break;
    }
    case options_parser::OptionType::kString: {
      std::string result;
      if (ondemand_value.get_string(result)) {
        FPrintF(stderr, "Invalid value for %s\n", option_name.c_str());
        return ParseResult::InvalidContent;
      }
      output->push_back(option_name + "=" + result);
      break;
    }
    case options_parser::OptionType::kInteger: {
      int64_t result;
      if (ondemand_value.get_int64().get(result)) {
        FPrintF(stderr, "Invalid value for %s\n", option_name.c_str());
        return ParseResult::InvalidContent;
      }
      output->push_back(option_name + "=" + std::to_string(result));
      break;
    }
    case options_parser::OptionType::kHostPort:
    case options_parser::OptionType::kUInteger: {
      uint64_t result;
      if (ondemand_value.get_uint64().get(result)) {
        FPrintF(stderr, "Invalid value for %s\n", option_name.c_str());
        return ParseResult::InvalidContent;
      }
      output->push_back(option_name + "=" + std::to_string(result));
      break;
    }
    case options_parser::OptionType::kNoOp: {
      FPrintF(stderr,
              "No-op flag %s is currently not supported\n",
              option_name.c_str());
      return ParseResult::InvalidContent;
      break;
    }
    case options_parser::OptionType::kV8Option: {
      FPrintF(stderr,
              "V8 flag %s is currently not supported\n",
              option_name.c_str());
      return ParseResult::InvalidContent;
    }
    default:
      UNREACHABLE();
  }
  unique_options->insert(option_name);
  return ParseResult::Valid;
}

ParseResult ConfigReader::ParseConfig(const std::string_view& config_path) {
  std::string file_content;
  // Read the configuration file
  int r = ReadFileSync(&file_content, config_path.data());
  if (r != 0) {
    const char* err = uv_strerror(r);
    FPrintF(
      stderr,
      "Cannot read configuration from %s: %s\n",
      config_path,
      err);
    return ParseResult::FileError;
  }

  // Parse the configuration file
  simdjson::ondemand::parser json_parser;
  simdjson::ondemand::document document;
  if (json_parser.iterate(file_content).get(document)) {
    FPrintF(stderr, "Can't parse %s\n", config_path.data());
    return ParseResult::InvalidContent;
  }

  // Validate config is an object
  simdjson::ondemand::object main_object;
  auto root_error = document.get_object().get(main_object);
  if (root_error) {
    if (root_error == simdjson::error_code::INCORRECT_TYPE) {
      FPrintF(
        stderr,
        "Root value unexpected not an object for %s\n\n",
        config_path.data());
    } else {
      FPrintF(stderr, "Can't parse %s\n", config_path.data());
    }
    return ParseResult::InvalidContent;
  }

  // Get all available namespaces for validation
  std::vector<std::string> available_namespaces =
      options_parser::MapAvailableNamespaces();
  // Add "nodeOptions" as a special case for backward compatibility
  available_namespaces.emplace_back("nodeOptions");

  // Create a set for faster lookup of valid namespaces
  std::unordered_set<std::string> valid_namespaces(available_namespaces.begin(),
                                                   available_namespaces.end());

  // Iterate through the main object to find all namespaces
  for (auto field : main_object) {
    std::string_view field_name;
    if (field.unescaped_key().get(field_name)) {
      return ParseResult::InvalidContent;
    }

    // Check if this field is a valid namespace
    std::string namespace_name(field_name);
    if (valid_namespaces.find(namespace_name) == valid_namespaces.end()) {
      // If not, skip it
      continue;
    }

    // Get the namespace object
    simdjson::ondemand::object namespace_object;
    auto field_error = field.value().get_object().get(namespace_object);

    // If namespace value is not an object
    if (field_error) {
      FPrintF(stderr,
              "\"%s\" value unexpected for %s (should be an object)\n",
              namespace_name.c_str(),
              config_path.data());
      return ParseResult::InvalidContent;
    }

    // Special case for backward compatibility: handle nodeOptions with existing
    // method
    if (namespace_name == "nodeOptions") {
      ParseResult result = ParseNodeOptions(&namespace_object);
      if (result != ParseResult::Valid) {
        return result;
      }
    } else {
      // Process options for this namespace
      ParseResult result =
          ParseNamespaceOptions(&namespace_object, namespace_name);
      if (result != ParseResult::Valid) {
        return result;
      }
    }
  }

  return ParseResult::Valid;
}

ParseResult ConfigReader::ParseNamespaceOptions(
    simdjson::ondemand::object* options_object,
    const std::string& namespace_name) {
  auto options_map = options_parser::MapOptionsByNamespace(namespace_name);
  simdjson::ondemand::value ondemand_value;
  std::string_view key;

  for (auto field : *options_object) {
    if (field.unescaped_key().get(key) || field.value().get(ondemand_value)) {
      return ParseResult::InvalidContent;
    }

    // The key needs to match the option for this namespace
    std::string prefix = "--";
    auto it = options_map.find(prefix.append(key));
    if (it != options_map.end()) {
      ParseResult result = ProcessOptionValue(key,
                                              it->first,
                                              ondemand_value,
                                              it->second,
                                              &namespace_options_,
                                              &unique_namespace_options_);
      if (result != ParseResult::Valid) {
        return result;
      }
    } else {
      FPrintF(stderr,
              "Unknown or not allowed option %s for namespace %s\n",
              key.data(),
              namespace_name.c_str());
      return ParseResult::InvalidContent;
    }
  }
  return ParseResult::Valid;
}

std::string ConfigReader::AssignNodeOptions() {
  std::string acc = "";
  const size_t total_options = node_options_.size() + namespace_options_.size();
  acc.reserve(total_options * 2);
  // In order to guarantee the order of the options, we need to iterate
  // through the node_options_ first, then the namespace_options_
  // This is needed in order to avoid breaking changes while introducing
  // config namespaces
  for (size_t i = 0; i < node_options_.size(); ++i) {
    acc += " " + node_options_[i];
  }
  for (size_t i = 0; i < namespace_options_.size(); ++i) {
    acc += " " + namespace_options_[i];
  }
  return acc;
}

size_t ConfigReader::GetFlagsSize() {
  return node_options_.size() + namespace_options_.size();
}
}  // namespace node
