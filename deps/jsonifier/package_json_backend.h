#ifndef DEPS_JSONIFIER_PACKAGE_JSON_BACKEND_H_
#define DEPS_JSONIFIER_PACKAGE_JSON_BACKEND_H_

#include <string_view>

#include "node_json_events.h"

namespace node {
namespace modules {

struct PackageJSONFields;

inline constexpr std::string_view kJsonifierUtf8Bom = "\xEF\xBB\xBF";

class PackageJSONFieldSink {
 public:
  explicit PackageJSONFieldSink(PackageJSONFields* fields) : fields_(fields) {}

  void SetName(std::string_view value);
  void SetMain(std::string_view value);
  void SetType(std::string_view value);
  void SetExports(std::string_view value);
  void SetImports(std::string_view value);
  void SetScripts(std::string_view value);
  void SetPrivate(bool value);

 private:
  PackageJSONFields* fields_;
};

class PackageJSONBackendParser {
 public:
  virtual ~PackageJSONBackendParser();
  virtual bool Parse(std::string_view json, PackageJSONFieldSink* sink) = 0;
};

namespace isa_avx512 {
PackageJSONBackendParser* NewGenericParser();
std::string_view WalkJson(std::string_view json, json::JsonEventSink* sink);
}  // namespace isa_avx512

namespace isa_avx2 {
PackageJSONBackendParser* NewGenericParser();
std::string_view WalkJson(std::string_view json, json::JsonEventSink* sink);
}  // namespace isa_avx2

namespace isa_fallback {
PackageJSONBackendParser* NewGenericParser();
std::string_view WalkJson(std::string_view json, json::JsonEventSink* sink);
}  // namespace isa_fallback

namespace isa_neon {
PackageJSONBackendParser* NewGenericParser();
std::string_view WalkJson(std::string_view json, json::JsonEventSink* sink);
}  // namespace isa_neon

}  // namespace modules
}  // namespace node

#endif  // DEPS_JSONIFIER_PACKAGE_JSON_BACKEND_H_
