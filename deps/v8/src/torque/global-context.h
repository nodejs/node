// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_GLOBAL_CONTEXT_H_
#define V8_TORQUE_GLOBAL_CONTEXT_H_

#include "src/torque/declarable.h"
#include "src/torque/declarations.h"
#include "src/torque/scope.h"
#include "src/torque/type-oracle.h"

namespace v8 {
namespace internal {
namespace torque {

class GlobalContext;
class Scope;
class TypeOracle;
class Builtin;
class Label;

class Module {
 public:
  explicit Module(const std::string& name, bool is_default)
      : name_(name), is_default_(is_default) {}
  const std::string& name() const { return name_; }
  bool IsDefault() const { return is_default_; }
  std::ostream& source_stream() { return source_stream_; }
  std::ostream& header_stream() { return header_stream_; }
  std::string source() { return source_stream_.str(); }
  std::string header() { return header_stream_.str(); }

 private:
  std::string name_;
  bool is_default_;
  std::stringstream header_stream_;
  std::stringstream source_stream_;
};

class GlobalContext {
 public:
  explicit GlobalContext(Ast ast)
      : verbose_(false),
        next_label_number_(0),
        default_module_(GetModule("base", true)),
        ast_(std::move(ast)) {}
  Module* GetDefaultModule() { return default_module_; }
  Module* GetModule(const std::string& name, bool is_default = false) {
    auto i = modules_.find(name);
    if (i != modules_.end()) {
      return i->second.get();
    }
    Module* module = new Module(name, is_default);
    modules_[name] = std::unique_ptr<Module>(module);
    return module;
  }

  int GetNextLabelNumber() { return next_label_number_++; }

  const std::map<std::string, std::unique_ptr<Module>>& GetModules() const {
    return modules_;
  }

  void SetVerbose() { verbose_ = true; }
  bool verbose() const { return verbose_; }

  friend class CurrentCallableActivator;
  friend class BreakContinueActivator;

  Callable* GetCurrentCallable() const { return current_callable_; }
  Block* GetCurrentBreak() const { return break_continue_stack_.back().first; }
  Block* GetCurrentContinue() const {
    return break_continue_stack_.back().second;
  }

  Declarations* declarations() { return &declarations_; }
  Ast* ast() { return &ast_; }

 private:
  bool verbose_;
  int next_label_number_;
  Declarations declarations_;
  Callable* current_callable_;
  std::vector<std::pair<Block*, Block*>> break_continue_stack_;
  std::map<std::string, std::unique_ptr<Module>> modules_;
  Module* default_module_;
  Ast ast_;
};

class CurrentCallableActivator {
 public:
  CurrentCallableActivator(GlobalContext& context, Callable* callable,
                           CallableNode* decl)
      : context_(context), scope_activator_(context.declarations(), decl) {
    remembered_callable_ = context_.current_callable_;
    context_.current_callable_ = callable;
  }
  ~CurrentCallableActivator() {
    context_.current_callable_ = remembered_callable_;
  }

 private:
  GlobalContext& context_;
  Callable* remembered_callable_;
  Declarations::NodeScopeActivator scope_activator_;
};

class BreakContinueActivator {
 public:
  BreakContinueActivator(GlobalContext& context, Block* break_block,
                         Block* continue_block)
      : context_(context) {
    context_.break_continue_stack_.push_back({break_block, continue_block});
  }
  ~BreakContinueActivator() { context_.break_continue_stack_.pop_back(); }

 private:
  GlobalContext& context_;
};

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_GLOBAL_CONTEXT_H_
