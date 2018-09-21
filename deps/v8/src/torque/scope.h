// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_SCOPE_H_
#define V8_TORQUE_SCOPE_H_

#include <map>
#include <string>

#include "src/torque/ast.h"
#include "src/torque/types.h"
#include "src/torque/utils.h"

namespace v8 {
namespace internal {
namespace torque {

class ScopeChain;
class Variable;
class Declarable;

class Scope {
 public:
  explicit Scope(ScopeChain& scope_chain);

  void Stream(std::ostream& stream) const {
    stream << "scope " << std::to_string(scope_number_) << " {";
    for (auto& c : lookup_) {
      stream << c.first << ",";
    }
    stream << "}";
  }

  int scope_number() const { return scope_number_; }

  ScopeChain& GetScopeChain() const { return scope_chain_; }

  void AddLiveVariables(std::set<const Variable*>& set);

  void Print();

  class Activator;

 private:
  friend class ScopeChain;

  void CheckAlreadyDeclared(SourcePosition pos, const std::string& name,
                            const char* new_type);

  void Declare(const std::string& name, Declarable* d) {
    DCHECK_EQ(lookup_.end(), lookup_.find(name));
    DCHECK(d != nullptr);
    lookup_[name] = d;
  }

  Declarable* Lookup(const std::string& name) {
    auto i = lookup_.find(name);
    if (i == lookup_.end()) {
      return nullptr;
    }
    return i->second;
  }

  ScopeChain& scope_chain_;
  int scope_number_;
  int private_label_number_;
  std::map<std::string, Declarable*> lookup_;
};

class Scope::Activator {
 public:
  explicit Activator(Scope* scope);
  ~Activator();

 private:
  Scope* scope_;
};

class ScopeChain {
 public:
  ScopeChain() : next_scope_number_(0) {}
  Scope* NewScope();

  Scope* TopScope() const { return current_scopes_.back(); }
  void PushScope(Scope* scope) { current_scopes_.push_back(scope); }
  void PopScope() { current_scopes_.pop_back(); }

  std::set<const Variable*> GetLiveVariables() {
    std::set<const Variable*> result;
    for (auto scope : current_scopes_) {
      scope->AddLiveVariables(result);
    }
    return result;
  }

  void Declare(const std::string& name, Declarable* d) {
    TopScope()->Declare(name, d);
  }

  Declarable* Lookup(const std::string& name) {
    auto e = current_scopes_.rend();
    auto c = current_scopes_.rbegin();
    while (c != e) {
      Declarable* result = (*c)->Lookup(name);
      if (result != nullptr) return result;
      ++c;
    }
    return nullptr;
  }

  Declarable* ShallowLookup(const std::string& name) {
    auto& e = current_scopes_.back();
    return e->Lookup(name);
  }

  Declarable* LookupGlobalScope(const std::string& name) {
    auto& e = current_scopes_.front();
    return e->Lookup(name);
  }

  void Print() {
    for (auto s : current_scopes_) {
      s->Print();
    }
  }

  struct Snapshot {
    ScopeChain* chain;
    std::vector<Scope*> current_scopes;
  };

  Snapshot TaskSnapshot() { return {this, current_scopes_}; }

  class ScopedSnapshotRestorer {
   public:
    explicit ScopedSnapshotRestorer(const Snapshot& snapshot)
        : chain_(snapshot.chain) {
      saved_ = chain_->current_scopes_;
      chain_->current_scopes_ = snapshot.current_scopes;
    }
    ~ScopedSnapshotRestorer() { chain_->current_scopes_ = saved_; }

   private:
    ScopeChain* chain_;
    std::vector<Scope*> saved_;
  };

 private:
  friend class Scope;
  friend class ScopedSnapshotRestorer;

  int GetNextScopeNumber() { return next_scope_number_++; }

  int next_scope_number_;
  std::vector<std::unique_ptr<Scope>> scopes_;
  std::vector<Scope*> current_scopes_;
};

inline std::ostream& operator<<(std::ostream& os, const Scope& scope) {
  scope.Stream(os);
  return os;
}

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_SCOPE_H_
