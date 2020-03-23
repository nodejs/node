// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_CSA_GENERATOR_H_
#define V8_TORQUE_CSA_GENERATOR_H_

#include <iostream>

#include "src/torque/cfg.h"
#include "src/torque/declarable.h"

namespace v8 {
namespace internal {
namespace torque {

class CSAGenerator {
 public:
  CSAGenerator(const ControlFlowGraph& cfg, std::ostream& out,
               base::Optional<Builtin::Kind> linkage = base::nullopt)
      : cfg_(cfg),
        out_(&out),
        out_decls_(&out),
        linkage_(linkage),
        previous_position_(SourcePosition::Invalid()) {}
  base::Optional<Stack<std::string>> EmitGraph(Stack<std::string> parameters);

  static constexpr const char* ARGUMENTS_VARIABLE_STRING = "arguments";

  static void EmitCSAValue(VisitResult result, const Stack<std::string>& values,
                           std::ostream& out);

 private:
  const ControlFlowGraph& cfg_;
  std::ostream* out_;
  std::ostream* out_decls_;
  size_t fresh_id_ = 0;
  base::Optional<Builtin::Kind> linkage_;
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

  bool IsEmptyInstruction(const Instruction& instruction);
  void EmitSourcePosition(SourcePosition pos, bool always_emit = false);

  std::string PreCallableExceptionPreparation(
      base::Optional<Block*> catch_block);
  void PostCallableExceptionPreparation(
      const std::string& catch_name, const Type* return_type,
      base::Optional<Block*> catch_block, Stack<std::string>* stack,
      const base::Optional<DefinitionLocation>& exception_object_definition);

  std::string FreshNodeName() { return "tmp" + std::to_string(fresh_id_++); }
  std::string FreshCatchName() { return "catch" + std::to_string(fresh_id_++); }
  std::string FreshLabelName() { return "label" + std::to_string(fresh_id_++); }
  std::string BlockName(const Block* block) {
    return "block" + std::to_string(block->id());
  }

  void ProcessArgumentsCommon(const TypeVector& parameter_types,
                              std::vector<std::string>* args,
                              std::vector<std::string>* constexpr_arguments,
                              Stack<std::string>* stack);

  Stack<std::string> EmitBlock(const Block* block);
  void EmitInstruction(const Instruction& instruction,
                       Stack<std::string>* stack);
#define EMIT_INSTRUCTION_DECLARATION(T) \
  void EmitInstruction(const T& instruction, Stack<std::string>* stack);
  TORQUE_INSTRUCTION_LIST(EMIT_INSTRUCTION_DECLARATION)
#undef EMIT_INSTRUCTION_DECLARATION
};

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_CSA_GENERATOR_H_
