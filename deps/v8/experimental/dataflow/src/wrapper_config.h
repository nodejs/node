// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WRAPPER_CONFIG_H
#define WRAPPER_CONFIG_H

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "llvm/Support/JSON.h"

namespace dfg {

struct WrapperOp {
  std::string MethodName;  // e.g. "load", "store", "operator=", "operator*",
                           // "operator->", "constructor"
  int ValueArgIndex =
      -1;  // Argument index containing the value being stored/loaded
};

struct WrapperConfig {
  std::string WrapperTypeName;
  std::vector<WrapperOp> LoadOps;
  std::vector<WrapperOp> StoreOps;

  const WrapperOp* FindLoadOp(const std::string& MethodName) const {
    for (const auto& Op : LoadOps) {
      if (Op.MethodName == MethodName ||
          (Op.MethodName == "implicit_cast" &&
           MethodName.rfind("operator ", 0) == 0)) {
        return &Op;
      }
    }
    return nullptr;
  }

  const WrapperOp* FindStoreOp(const std::string& MethodName) const {
    for (const auto& Op : StoreOps) {
      if (Op.MethodName == MethodName) {
        return &Op;
      }
    }
    return nullptr;
  }
};

class WrapperConfigManager {
 public:
  static WrapperConfigManager& Get() {
    static WrapperConfigManager Instance;
    return Instance;
  }

  // Load configuration from JSON string
  bool LoadFromJson(const std::string& JsonStr);

  // Load configuration from file
  bool LoadFromFile(const std::string& FilePath);

  // Get wrapper configuration for a given C++ type name
  const WrapperConfig* GetConfigForType(const std::string& TypeName) const;

  // Check if a function name is a configured transparent identity/cast function
  bool IsIdentityFunction(const std::string& Name) const;

  // Check if a class name represents a registered wrapper or its base/internal classes
  bool IsWrapperClass(const std::string& ClassName) const;

  // Register a default configuration
  void RegisterDefaultConfigs();

 private:
  WrapperConfigManager() { RegisterDefaultConfigs(); }

  std::unordered_map<std::string, WrapperConfig> Configs;
  std::unordered_set<std::string> IdentityFnNames;

  // Helper to strip template arguments and namespaces to get base class name
  std::string GetBaseTypeName(const std::string& TypeName) const;
};

}  // namespace dfg

#endif  // WRAPPER_CONFIG_H
