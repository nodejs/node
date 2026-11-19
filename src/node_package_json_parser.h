#ifndef SRC_NODE_PACKAGE_JSON_PARSER_H_
#define SRC_NODE_PACKAGE_JSON_PARSER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace node {
namespace modules {

struct PackageJSONFields {
  std::optional<std::string> name;
  std::optional<std::string> main;
  std::string type = "none";
  std::optional<std::string> exports;
  std::optional<std::string> imports;
  std::optional<std::string> scripts;
  std::optional<bool> is_private;
};

class JsonifierPackageJSONParser {
 public:
  JsonifierPackageJSONParser();
  ~JsonifierPackageJSONParser();
  JsonifierPackageJSONParser(const JsonifierPackageJSONParser&) = delete;
  JsonifierPackageJSONParser& operator=(const JsonifierPackageJSONParser&) =
      delete;

  bool Parse(std::string_view json, PackageJSONFields* fields);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace modules
}  // namespace node

#endif  // SRC_NODE_PACKAGE_JSON_PARSER_H_
