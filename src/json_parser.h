#ifndef SRC_JSON_PARSER_H_
#define SRC_JSON_PARSER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include "simdjson.h"

namespace node {
// This is intended to be used to get some top-level fields out of a JSON
// without having to spin up a full Node.js environment that unnecessarily
// complicates things.
class JSONParser {
 public:
  using StringDict = std::unordered_map<std::string, std::string>;
  JSONParser();
  ~JSONParser() = default;
  bool Parse(const std::string& content);
  std::optional<std::string> GetTopLevelStringField(std::string_view field);
  std::optional<bool> GetTopLevelBoolField(std::string_view field);
  std::optional<StringDict> GetTopLevelStringDict(std::string_view field);
  std::string GetErrorMessage() const { return error_message_; }

 private:
  simdjson::ondemand::parser parser_;
  std::string json_content_;
  std::string error_message_;
  bool parsed_ = false;
};
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_JSON_PARSER_H_
