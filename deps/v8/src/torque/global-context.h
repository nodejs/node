// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_GLOBAL_CONTEXT_H_
#define V8_TORQUE_GLOBAL_CONTEXT_H_

#include <map>

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

  static void RegisterClass(const TypeAlias* alias) {
    DCHECK(alias->ParentScope()->IsNamespace());
    Get().classes_.push_back(alias);
  }

  using GlobalClassList = std::vector<const TypeAlias*>;

  static const GlobalClassList& GetClasses() { return Get().classes_; }

  static void AddCppInclude(std::string include_path) {
    Get().cpp_includes_.push_back(std::move(include_path));
  }
  static const std::vector<std::string>& CppIncludes() {
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
  static size_t FreshId() { return Get().fresh_id_++; }

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
  std::vector<std::string> cpp_includes_;
  std::map<SourceId, PerFileStreams> generated_per_file_;
  GlobalClassList classes_;
  size_t fresh_id_ = 0;

  friend class LanguageServerData;
};

template <class T>
T* RegisterDeclarable(std::unique_ptr<T> d) {
  return GlobalContext::Get().RegisterDeclarable(std::move(d));
}

class TargetArchitecture : public ContextualClass<TargetArchitecture> {
 public:
  explicit TargetArchitecture(bool force_32bit);

  static int TaggedSize() { return Get().tagged_size_; }
  static int RawPtrSize() { return Get().raw_ptr_size_; }

 private:
  const int tagged_size_;
  const int raw_ptr_size_;
};

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_GLOBAL_CONTEXT_H_
