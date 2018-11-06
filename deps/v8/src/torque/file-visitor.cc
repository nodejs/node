// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/file-visitor.h"

#include "src/torque/declarable.h"

namespace v8 {
namespace internal {
namespace torque {

Signature FileVisitor::MakeSignature(const CallableNodeSignature* signature) {
  LabelDeclarationVector definition_vector;
  for (const auto& label : signature->labels) {
    LabelDeclaration def = {label.name, GetTypeVector(label.types)};
    definition_vector.push_back(def);
  }
  base::Optional<std::string> arguments_variable;
  if (signature->parameters.has_varargs)
    arguments_variable = signature->parameters.arguments_variable;
  Signature result{signature->parameters.names,
                   arguments_variable,
                   {GetTypeVector(signature->parameters.types),
                    signature->parameters.has_varargs},
                   signature->parameters.implicit_count,
                   Declarations::GetType(signature->return_type),
                   definition_vector};
  return result;
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
