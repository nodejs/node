#ifndef SRC_BUILTIN_INFO_H_
#define SRC_BUILTIN_INFO_H_

#include <cinttypes>
#include <string>
#include <unordered_map>
#include <vector>

// This file must not depend on node.h or other code that depends on
// the full Node.js implementation because it is used during the
// compilation of the Node.js implementation itself (especially js2c).

namespace node {
namespace builtins {

enum class BuiltinSourceType {
  kBootstrapRealm,    // internal/bootstrap/realm
  kBootstrapScript,   // internal/bootstrap/*
  kPerContextScript,  // internal/per_context/*
  kMainScript,        // internal/main/*
  kFunction,          // others
  kSourceTextModule,  // selected modules, ends with .mjs in source
};

struct BuiltinInfo {
  // C++17 inline static
  inline static const std::unordered_map<BuiltinSourceType,
                                         std::vector<std::string>>
      parameter_map{
          {BuiltinSourceType::kBootstrapRealm,
           {"process",
            "getLinkedBinding",
            "getInternalBinding",
            "primordials"}},
          {BuiltinSourceType::kBootstrapScript,
           {"process", "require", "internalBinding", "primordials"}},
          {BuiltinSourceType::kPerContextScript,
           {"exports", "primordials", "privateSymbols", "perIsolateSymbols"}},
          {BuiltinSourceType::kMainScript,
           {"process", "require", "internalBinding", "primordials"}},
          {BuiltinSourceType::kFunction,
           {"exports",
            "require",
            "module",
            "process",
            "internalBinding",
            "primordials"}},
      };
};

std::string GetBuiltinSourceTypeName(BuiltinSourceType type);
BuiltinSourceType GetBuiltinSourceType(const std::string& id,
                                       const std::string& filename);
}  // namespace builtins
}  // namespace node

#endif  // SRC_BUILTIN_INFO_H_
