// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_GLOBAL_CONTEXT_H_
#define V8_TORQUE_GLOBAL_CONTEXT_H_

#include <map>
#include <memory>

#include "src/common/globals.h"
#include "src/torque/ast.h"
#include "src/torque/contextual.h"
#include "src/torque/declarable.h"

namespace v8 {
namespace internal {
namespace torque {

class GlobalContext : public ContextualClass<GlobalContext> {
 public:
  GlobalContext(GlobalContext&&) V8_NOEXCEPT = default;
  GlobalContext& operator=(GlobalContext&&) V8_NOEXCEPT = default;
  explicit GlobalContext(Ast ast);

  static Namespace* GetDefaultNamespace() { return Get().default_namespace_; }
  template <class T>
  T* RegisterDeclarable(std::unique_ptr<T> d) {
    T* ptr = d.get();
    declarables_.push_back(std::move(d));
    return ptr;
  }

  static const std::vector<std::unique_ptr<Declarable>>& AllDeclarables() {
    return Get().declarables_;
  }

  static void AddCppInclude(std::string include_path) {
    Get().cpp_includes_.insert(std::move(include_path));
  }
  static const std::set<std::string>& CppIncludes() {
    return Get().cpp_includes_;
  }

  static void SetCollectLanguageServerData() {
    Get().collect_language_server_data_ = true;
  }
  static bool collect_language_server_data() {
    return Get().collect_language_server_data_;
  }
  static void SetForceAssertStatements() {
    Get().force_assert_statements_ = true;
  }
  static bool force_assert_statements() {
    return Get().force_assert_statements_;
  }
  static Ast* ast() { return &Get().ast_; }
  static std::string MakeUniqueName(const std::string& base) {
    return base + "_" + std::to_string(Get().fresh_ids_[base]++);
  }

  struct PerFileStreams {
    std::stringstream csa_headerfile;
    std::stringstream csa_ccfile;
  };
  static PerFileStreams& GeneratedPerFile(SourceId file) {
    return Get().generated_per_file_[file];
  }

 private:
  bool collect_language_server_data_;
  bool force_assert_statements_;
  Namespace* default_namespace_;
  Ast ast_;
  std::vector<std::unique_ptr<Declarable>> declarables_;
  std::set<std::string> cpp_includes_;
  std::map<SourceId, PerFileStreams> generated_per_file_;
  std::map<std::string, size_t> fresh_ids_;

  friend class LanguageServerData;
};

template <class T>
T* RegisterDeclarable(std::unique_ptr<T> d) {
  return GlobalContext::Get().RegisterDeclarable(std::move(d));
}

class TargetArchitecture : public ContextualClass<TargetArchitecture> {
 public:
  explicit TargetArchitecture(bool force_32bit);

  static size_t TaggedSize() { return Get().tagged_size_; }
  static size_t RawPtrSize() { return Get().raw_ptr_size_; }
  static size_t MaxHeapAlignment() { return TaggedSize(); }
  static bool ArePointersCompressed() { return TaggedSize() < RawPtrSize(); }

 private:
  const size_t tagged_size_;
  const size_t raw_ptr_size_;
};

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_GLOBAL_CONTEXT_H_
