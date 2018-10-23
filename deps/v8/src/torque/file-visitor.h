// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_FILE_VISITOR_H_
#define V8_TORQUE_FILE_VISITOR_H_

#include <deque>
#include <string>

#include "src/torque/ast.h"
#include "src/torque/global-context.h"
#include "src/torque/types.h"
#include "src/torque/utils.h"

namespace v8 {
namespace internal {
namespace torque {

class FileVisitor {
 public:
  explicit FileVisitor(GlobalContext& global_context)
      : global_context_(global_context),
        declarations_(global_context.declarations()),
        module_(global_context.GetDefaultModule()) {}

  TypeVector GetTypeVector(const std::vector<TypeExpression*>& v) {
    TypeVector result;
    for (TypeExpression* t : v) {
      result.push_back(declarations()->GetType(t));
    }
    return result;
  }

  Ast* ast() { return global_context_.ast(); }
  Declarations* declarations() { return global_context_.declarations(); }

  void DrainSpecializationQueue();

  class ScopedModuleActivator {
   public:
    ScopedModuleActivator(FileVisitor* visitor, Module* module)
        : visitor_(visitor), saved_module_(visitor->CurrentModule()) {
      visitor->module_ = module;
    }
    ~ScopedModuleActivator() { visitor_->module_ = saved_module_; }

   private:
    FileVisitor* visitor_;
    Module* saved_module_;
  };

 protected:
  Module* CurrentModule() const { return module_; }

  friend class ScopedModuleActivator;

  std::string GetParameterVariableFromName(const std::string& name) {
    return std::string("p_") + name;
  }

  Signature MakeSignature(const CallableNodeSignature* signature);
  Signature MakeSignatureFromReturnType(TypeExpression* return_type);

  struct PendingSpecialization {
    SpecializationKey key;
    CallableNode* callable;
    const CallableNodeSignature* signature;
    base::Optional<Statement*> body;
    SourcePosition request_position;
  };

  void QueueGenericSpecialization(const SpecializationKey& key,
                                  CallableNode* callable,
                                  const CallableNodeSignature* signature,
                                  base::Optional<Statement*> body);

  void SpecializeGeneric(const PendingSpecialization& specialization);

  virtual void Specialize(const SpecializationKey&, CallableNode* callable,
                          const CallableNodeSignature* signature,
                          Statement* body) = 0;

  GlobalContext& global_context_;
  Declarations* declarations_;
  std::deque<PendingSpecialization> pending_specializations_;
  std::set<SpecializationKey> completed_specializations_;
  Callable* current_callable_;
  Module* module_;
};

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_FILE_VISITOR_H_
