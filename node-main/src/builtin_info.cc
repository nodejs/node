#include "builtin_info.h"

namespace node {
namespace builtins {

BuiltinSourceType GetBuiltinSourceType(const std::string& id,
                                       const std::string& filename) {
  if (filename.ends_with(".mjs")) {
    return BuiltinSourceType::kSourceTextModule;
  }
  if (id.starts_with("internal/bootstrap/realm")) {
    return BuiltinSourceType::kBootstrapRealm;
  }
  if (id.starts_with("internal/bootstrap/")) {
    return BuiltinSourceType::kBootstrapScript;
  }
  if (id.starts_with("internal/per_context/")) {
    return BuiltinSourceType::kPerContextScript;
  }
  if (id.starts_with("internal/main/")) {
    return BuiltinSourceType::kMainScript;
  }
  if (id.starts_with("internal/deps/v8/tools/")) {
    return BuiltinSourceType::kSourceTextModule;
  }
  return BuiltinSourceType::kFunction;
}

std::string GetBuiltinSourceTypeName(BuiltinSourceType type) {
  switch (type) {
    case BuiltinSourceType::kBootstrapRealm:
      return "kBootstrapRealm";
    case BuiltinSourceType::kBootstrapScript:
      return "kBootstrapScript";
    case BuiltinSourceType::kPerContextScript:
      return "kPerContextScript";
    case BuiltinSourceType::kMainScript:
      return "kMainScript";
    case BuiltinSourceType::kFunction:
      return "kFunction";
    case BuiltinSourceType::kSourceTextModule:
      return "kSourceTextModule";
  }
  abort();
}

}  // namespace builtins
}  // namespace node
