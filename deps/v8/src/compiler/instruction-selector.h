// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_INSTRUCTION_SELECTOR_H_
#define V8_COMPILER_INSTRUCTION_SELECTOR_H_

#include <deque>

#include "src/compiler/common-operator.h"
#include "src/compiler/instruction.h"
#include "src/compiler/machine-operator.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
struct CallBuffer;  // TODO(bmeurer): Remove this.
class FlagsContinuation;

class InstructionSelector V8_FINAL {
 public:
  // Forward declarations.
  class Features;

  InstructionSelector(InstructionSequence* sequence,
                      SourcePositionTable* source_positions,
                      Features features = SupportedFeatures());

  // Visit code for the entire graph with the included schedule.
  void SelectInstructions();

  // ===========================================================================
  // ============= Architecture-independent code emission methods. =============
  // ===========================================================================

  Instruction* Emit(InstructionCode opcode, InstructionOperand* output,
                    size_t temp_count = 0, InstructionOperand* *temps = NULL);
  Instruction* Emit(InstructionCode opcode, InstructionOperand* output,
                    InstructionOperand* a, size_t temp_count = 0,
                    InstructionOperand* *temps = NULL);
  Instruction* Emit(InstructionCode opcode, InstructionOperand* output,
                    InstructionOperand* a, InstructionOperand* b,
                    size_t temp_count = 0, InstructionOperand* *temps = NULL);
  Instruction* Emit(InstructionCode opcode, InstructionOperand* output,
                    InstructionOperand* a, InstructionOperand* b,
                    InstructionOperand* c, size_t temp_count = 0,
                    InstructionOperand* *temps = NULL);
  Instruction* Emit(InstructionCode opcode, InstructionOperand* output,
                    InstructionOperand* a, InstructionOperand* b,
                    InstructionOperand* c, InstructionOperand* d,
                    size_t temp_count = 0, InstructionOperand* *temps = NULL);
  Instruction* Emit(InstructionCode opcode, size_t output_count,
                    InstructionOperand** outputs, size_t input_count,
                    InstructionOperand** inputs, size_t temp_count = 0,
                    InstructionOperand* *temps = NULL);
  Instruction* Emit(Instruction* instr);

  // ===========================================================================
  // ============== Architecture-independent CPU feature methods. ==============
  // ===========================================================================

  class Features V8_FINAL {
   public:
    Features() : bits_(0) {}
    explicit Features(unsigned bits) : bits_(bits) {}
    explicit Features(CpuFeature f) : bits_(1u << f) {}
    Features(CpuFeature f1, CpuFeature f2) : bits_((1u << f1) | (1u << f2)) {}

    bool Contains(CpuFeature f) const { return (bits_ & (1u << f)); }

   private:
    unsigned bits_;
  };

  bool IsSupported(CpuFeature feature) const {
    return features_.Contains(feature);
  }

  // Returns the features supported on the target platform.
  static Features SupportedFeatures() {
    return Features(CpuFeatures::SupportedFeatures());
  }

 private:
  friend class OperandGenerator;

  // ===========================================================================
  // ============ Architecture-independent graph covering methods. =============
  // ===========================================================================

  // Checks if {block} will appear directly after {current_block_} when
  // assembling code, in which case, a fall-through can be used.
  bool IsNextInAssemblyOrder(const BasicBlock* block) const;

  // Used in pattern matching during code generation.
  // Check if {node} can be covered while generating code for the current
  // instruction. A node can be covered if the {user} of the node has the only
  // edge and the two are in the same basic block.
  bool CanCover(Node* user, Node* node) const;

  // Checks if {node} was already defined, and therefore code was already
  // generated for it.
  bool IsDefined(Node* node) const;

  // Inform the instruction selection that {node} was just defined.
  void MarkAsDefined(Node* node);

  // Checks if {node} has any uses, and therefore code has to be generated for
  // it.
  bool IsUsed(Node* node) const;

  // Inform the instruction selection that {node} has at least one use and we
  // will need to generate code for it.
  void MarkAsUsed(Node* node);

  // Checks if {node} is marked as double.
  bool IsDouble(const Node* node) const;

  // Inform the register allocator of a double result.
  void MarkAsDouble(Node* node);

  // Checks if {node} is marked as reference.
  bool IsReference(const Node* node) const;

  // Inform the register allocator of a reference result.
  void MarkAsReference(Node* node);

  // Inform the register allocation of the representation of the value produced
  // by {node}.
  void MarkAsRepresentation(MachineType rep, Node* node);

  // Initialize the call buffer with the InstructionOperands, nodes, etc,
  // corresponding
  // to the inputs and outputs of the call.
  // {call_code_immediate} to generate immediate operands to calls of code.
  // {call_address_immediate} to generate immediate operands to address calls.
  void InitializeCallBuffer(Node* call, CallBuffer* buffer,
                            bool call_code_immediate,
                            bool call_address_immediate, BasicBlock* cont_node,
                            BasicBlock* deopt_node);

  // ===========================================================================
  // ============= Architecture-specific graph covering methods. ===============
  // ===========================================================================

  // Visit nodes in the given block and generate code.
  void VisitBlock(BasicBlock* block);

  // Visit the node for the control flow at the end of the block, generating
  // code if necessary.
  void VisitControl(BasicBlock* block);

  // Visit the node and generate code, if any.
  void VisitNode(Node* node);

#define DECLARE_GENERATOR(x) void Visit##x(Node* node);
  MACHINE_OP_LIST(DECLARE_GENERATOR)
#undef DECLARE_GENERATOR

  void VisitInt32AddWithOverflow(Node* node, FlagsContinuation* cont);
  void VisitInt32SubWithOverflow(Node* node, FlagsContinuation* cont);

  void VisitWord32Test(Node* node, FlagsContinuation* cont);
  void VisitWord64Test(Node* node, FlagsContinuation* cont);
  void VisitWord32Compare(Node* node, FlagsContinuation* cont);
  void VisitWord64Compare(Node* node, FlagsContinuation* cont);
  void VisitFloat64Compare(Node* node, FlagsContinuation* cont);

  void VisitParameter(Node* node);
  void VisitPhi(Node* node);
  void VisitProjection(Node* node);
  void VisitConstant(Node* node);
  void VisitCall(Node* call, BasicBlock* continuation,
                 BasicBlock* deoptimization);
  void VisitGoto(BasicBlock* target);
  void VisitBranch(Node* input, BasicBlock* tbranch, BasicBlock* fbranch);
  void VisitReturn(Node* value);
  void VisitThrow(Node* value);
  void VisitDeoptimize(Node* deopt);

  // ===========================================================================

  Graph* graph() const { return sequence()->graph(); }
  Linkage* linkage() const { return sequence()->linkage(); }
  Schedule* schedule() const { return sequence()->schedule(); }
  InstructionSequence* sequence() const { return sequence_; }
  Zone* instruction_zone() const { return sequence()->zone(); }
  Zone* zone() { return &zone_; }

  // ===========================================================================

  typedef zone_allocator<Instruction*> InstructionPtrZoneAllocator;
  typedef std::deque<Instruction*, InstructionPtrZoneAllocator> Instructions;

  Zone zone_;
  InstructionSequence* sequence_;
  SourcePositionTable* source_positions_;
  Features features_;
  BasicBlock* current_block_;
  Instructions instructions_;
  BoolVector defined_;
  BoolVector used_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_INSTRUCTION_SELECTOR_H_
