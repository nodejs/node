// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/file-visitor.h"

#include "src/torque/declarable.h"
#include "src/torque/parameter-difference.h"

namespace v8 {
namespace internal {
namespace torque {

Signature FileVisitor::MakeSignature(const CallableNodeSignature* signature) {
  LabelDeclarationVector definition_vector;
  for (const auto& label : signature->labels) {
    LabelDeclaration def = {label.name, GetTypeVector(label.types)};
    definition_vector.push_back(def);
  }
  Signature result{signature->parameters.names,
                   {GetTypeVector(signature->parameters.types),
                    signature->parameters.has_varargs},
                   declarations()->GetType(signature->return_type),
                   definition_vector};
  return result;
}

Signature FileVisitor::MakeSignatureFromReturnType(
    TypeExpression* return_type) {
  Signature result{{}, {{}, false}, declarations()->GetType(return_type), {}};
  return result;
}

void FileVisitor::QueueGenericSpecialization(
    const SpecializationKey& key, CallableNode* callable,
    const CallableNodeSignature* signature, base::Optional<Statement*> body) {
  pending_specializations_.push_back(
      {key, callable, signature, body, CurrentSourcePosition::Get()});
}

void FileVisitor::SpecializeGeneric(
    const PendingSpecialization& specialization) {
  CurrentSourcePosition::Scope scope(specialization.request_position);
  if (completed_specializations_.find(specialization.key) !=
      completed_specializations_.end()) {
    std::stringstream stream;
    stream << "cannot redeclare specialization of "
           << specialization.key.first->name() << " with types <"
           << specialization.key.second << ">";
    ReportError(stream.str());
  }
  if (!specialization.body) {
    std::stringstream stream;
    stream << "missing specialization of " << specialization.key.first->name()
           << " with types <" << specialization.key.second << ">";
    ReportError(stream.str());
  }
  Declarations::ScopedGenericSpecializationKey instantiation(
      declarations(), specialization.key);
  FileVisitor::ScopedModuleActivator activator(
      this, specialization.key.first->module());
  Specialize(specialization.key, specialization.callable,
             specialization.signature, *specialization.body);
  completed_specializations_.insert(specialization.key);
}

void FileVisitor::DrainSpecializationQueue() {
  while (pending_specializations_.size() != 0) {
    PendingSpecialization specialization(pending_specializations_.front());
    pending_specializations_.pop_front();
    if (completed_specializations_.find(specialization.key) ==
        completed_specializations_.end()) {
      Declarations::ScopedGenericScopeChainSnapshot scope(declarations(),
                                                          specialization.key);
      SpecializeGeneric(specialization);
    }
  }
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
