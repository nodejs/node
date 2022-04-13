// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_TORQUE_CODE_GENERATOR_H_
#define V8_TORQUE_TORQUE_CODE_GENERATOR_H_

#include <iostream>

#include "src/torque/cfg.h"
#include "src/torque/declarable.h"

namespace v8 {
namespace internal {
namespace torque {

class TorqueCodeGenerator {
 public:
  TorqueCodeGenerator(const ControlFlowGraph& cfg, std::ostream& out)
      : cfg_(cfg),
        out_(&out),
        out_decls_(&out),
        previous_position_(SourcePosition::Invalid()) {}

 protected:
  const ControlFlowGraph& cfg_;
  std::ostream* out_;
  std::ostream* out_decls_;
  size_t fresh_id_ = 0;
  SourcePosition previous_position_;
  std::map<DefinitionLocation, std::string> location_map_;

  std::string DefinitionToVariable(const DefinitionLocation& location) {
    if (location.IsPhi()) {
      std::stringstream stream;
      stream << "phi_bb" << location.GetPhiBlock()->id() << "_"
             << location.GetPhiIndex();
      return stream.str();
    } else if (location.IsParameter()) {
      auto it = location_map_.find(location);
      DCHECK_NE(it, location_map_.end());
      return it->second;
    } else {
      DCHECK(location.IsInstruction());
      auto it = location_map_.find(location);
      if (it == location_map_.end()) {
        it = location_map_.insert(std::make_pair(location, FreshNodeName()))
                 .first;
      }
      return it->second;
    }
  }

  void SetDefinitionVariable(const DefinitionLocation& definition,
                             const std::string& str) {
    DCHECK_EQ(location_map_.find(definition), location_map_.end());
    location_map_.insert(std::make_pair(definition, str));
  }

  std::ostream& out() { return *out_; }
  std::ostream& decls() { return *out_decls_; }

  static bool IsEmptyInstruction(const Instruction& instruction);
  virtual void EmitSourcePosition(SourcePosition pos,
                                  bool always_emit = false) = 0;

  std::string FreshNodeName() { return "tmp" + std::to_string(fresh_id_++); }
  std::string FreshCatchName() { return "catch" + std::to_string(fresh_id_++); }
  std::string FreshLabelName() { return "label" + std::to_string(fresh_id_++); }
  std::string BlockName(const Block* block) {
    return "block" + std::to_string(block->id());
  }

  void EmitInstruction(const Instruction& instruction,
                       Stack<std::string>* stack);

  template <typename T>
  void EmitIRAnnotation(const T& instruction, Stack<std::string>* stack) {
    out() << "    // " << instruction
          << ", starting stack size: " << stack->Size() << "\n";
  }

#define EMIT_INSTRUCTION_DECLARATION(T) \
  void EmitInstruction(const T& instruction, Stack<std::string>* stack);
  TORQUE_BACKEND_AGNOSTIC_INSTRUCTION_LIST(EMIT_INSTRUCTION_DECLARATION)
#undef EMIT_INSTRUCTION_DECLARATION

#define EMIT_INSTRUCTION_DECLARATION(T)              \
  virtual void EmitInstruction(const T& instruction, \
                               Stack<std::string>* stack) = 0;
  TORQUE_BACKEND_DEPENDENT_INSTRUCTION_LIST(EMIT_INSTRUCTION_DECLARATION)
#undef EMIT_INSTRUCTION_DECLARATION
};

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_TORQUE_CODE_GENERATOR_H_
