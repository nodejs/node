#ifndef SRC_NODE_CONFIG_FILE_H_
#define SRC_NODE_CONFIG_FILE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <map>
#include <string>
#include <unordered_set>
#include <variant>
#include <vector>
#include "node_internals.h"
#include "simdjson.h"
#include "util-inl.h"

namespace node {

// When trying to parse the configuration file, we can have three possible
// results:
//  - Valid: The file was successfully parsed and the content is valid.
//  - FileError: There was an error reading the file.
//  - InvalidContent: The file was read, but the content is invalid.
enum ParseResult { Valid, FileError, InvalidContent };

// ConfigReader is the class that parses the configuration JSON file.
// It reads the file provided by --experimental-config-file and
// extracts the flags.
class ConfigReader {
 public:
  ParseResult ParseConfig(const std::string_view& config_path);

  std::optional<std::string_view> GetDataFromArgs(
      const std::vector<std::string>& args);

  std::string AssignNodeOptions();

  size_t GetFlagsSize();

 private:
  // Parse the nodeOptions object from the configuration file
  ParseResult ParseNodeOptions(simdjson::ondemand::object* node_options_object);

  // Parse options for a specific namespace
  ParseResult ParseNamespaceOptions(simdjson::ondemand::object* options_object,
                                    const std::string& namespace_name);

  // Process a single option value based on its type
  ParseResult ProcessOptionValue(
      const std::string_view& key,
      const std::string& option_name,
      simdjson::ondemand::value& ondemand_value,
      options_parser::OptionType option_type,
      std::vector<std::string>* output,
      std::unordered_set<std::string>* unique_options);

  std::unordered_set<std::string> unique_node_options_;
  std::vector<std::string> node_options_;
  std::unordered_set<std::string> unique_namespace_options_;
  std::vector<std::string> namespace_options_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_CONFIG_FILE_H_
