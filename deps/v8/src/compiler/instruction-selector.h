// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_INSTRUCTION_SELECTOR_H_
#define V8_COMPILER_INSTRUCTION_SELECTOR_H_

#include <map>

#include "src/compiler/common-operator.h"
#include "src/compiler/instruction.h"
#include "src/compiler/instruction-scheduler.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class BasicBlock;
struct CallBuffer;  // TODO(bmeurer): Remove this.
class FlagsContinuation;
class Linkage;
class OperandGenerator;
struct SwitchInfo;

// This struct connects nodes of parameters which are going to be pushed on the
// call stack with their parameter index in the call descriptor of the callee.
class PushParameter {
 public:
  PushParameter() : node_(nullptr), type_(MachineType::None()) {}
  PushParameter(Node* node, MachineType type) : node_(node), type_(type) {}

  Node* node() const { return node_; }
  MachineType type() const { return type_; }

 private:
  Node* node_;
  MachineType type_;
};

// Instruction selection generates an InstructionSequence for a given Schedule.
class InstructionSelector final {
 public:
  // Forward declarations.
  class Features;

  enum SourcePositionMode { kCallSourcePositions, kAllSourcePositions };

  InstructionSelector(
      Zone* zone, size_t node_count, Linkage* linkage,
      InstructionSequence* sequence, Schedule* schedule,
      SourcePositionTable* source_positions, Frame* frame,
      SourcePositionMode source_position_mode = kCallSourcePositions,
      Features features = SupportedFeatures());

  // Visit code for the entire graph with the included schedule.
  void SelectInstructions();

  void StartBlock(RpoNumber rpo);
  void EndBlock(RpoNumber rpo);
  void AddInstruction(Instruction* instr);

  // ===========================================================================
  // ============= Architecture-independent code emission methods. =============
  // ===========================================================================

  Instruction* Emit(InstructionCode opcode, InstructionOperand output,
                    size_t temp_count = 0, InstructionOperand* temps = nullptr);
  Instruction* Emit(InstructionCode opcode, InstructionOperand output,
                    InstructionOperand a, size_t temp_count = 0,
                    InstructionOperand* temps = nullptr);
  Instruction* Emit(InstructionCode opcode, InstructionOperand output,
                    InstructionOperand a, InstructionOperand b,
                    size_t temp_count = 0, InstructionOperand* temps = nullptr);
  Instruction* Emit(InstructionCode opcode, InstructionOperand output,
                    InstructionOperand a, InstructionOperand b,
                    InstructionOperand c, size_t temp_count = 0,
                    InstructionOperand* temps = nullptr);
  Instruction* Emit(InstructionCode opcode, InstructionOperand output,
                    InstructionOperand a, InstructionOperand b,
                    InstructionOperand c, InstructionOperand d,
                    size_t temp_count = 0, InstructionOperand* temps = nullptr);
  Instruction* Emit(InstructionCode opcode, InstructionOperand output,
                    InstructionOperand a, InstructionOperand b,
                    InstructionOperand c, InstructionOperand d,
                    InstructionOperand e, size_t temp_count = 0,
                    InstructionOperand* temps = nullptr);
  Instruction* Emit(InstructionCode opcode, InstructionOperand output,
                    InstructionOperand a, InstructionOperand b,
                    InstructionOperand c, InstructionOperand d,
                    InstructionOperand e, InstructionOperand f,
                    size_t temp_count = 0, InstructionOperand* temps = nullptr);
  Instruction* Emit(InstructionCode opcode, size_t output_count,
                    InstructionOperand* outputs, size_t input_count,
                    InstructionOperand* inputs, size_t temp_count = 0,
                    InstructionOperand* temps = nullptr);
  Instruction* Emit(Instruction* instr);

  // ===========================================================================
  // ============== Architecture-independent CPU feature methods. ==============
  // ===========================================================================

  class Features final {
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

  // TODO(sigurds) This should take a CpuFeatures argument.
  static MachineOperatorBuilder::Flags SupportedMachineOperatorFlags();

  // ===========================================================================
  // ============ Architecture-independent graph covering methods. =============
  // ===========================================================================

  // Used in pattern matching during code generation.
  // Check if {node} can be covered while generating code for the current
  // instruction. A node can be covered if the {user} of the node has the only
  // edge and the two are in the same basic block.
  bool CanCover(Node* user, Node* node) const;

  // Checks if {node} was already defined, and therefore code was already
  // generated for it.
  bool IsDefined(Node* node) const;

  // Checks if {node} has any uses, and therefore code has to be generated for
  // it.
  bool IsUsed(Node* node) const;

  // Checks if {node} is currently live.
  bool IsLive(Node* node) const { return !IsDefined(node) && IsUsed(node); }

  // Gets the effect level of {node}.
  int GetEffectLevel(Node* node) const;

  int GetVirtualRegister(const Node* node);
  const std::map<NodeId, int> GetVirtualRegistersForTesting() const;

  Isolate* isolate() const { return sequence()->isolate(); }

 private:
  friend class OperandGenerator;

  void EmitTableSwitch(const SwitchInfo& sw, InstructionOperand& index_operand);
  void EmitLookupSwitch(const SwitchInfo& sw,
                        InstructionOperand& value_operand);

  // Inform the instruction selection that {node} was just defined.
  void MarkAsDefined(Node* node);

  // Inform the instruction selection that {node} has at least one use and we
  // will need to generate code for it.
  void MarkAsUsed(Node* node);

  // Sets the effect level of {node}.
  void SetEffectLevel(Node* node, int effect_level);

  // Inform the register allocation of the representation of the value produced
  // by {node}.
  void MarkAsRepresentation(MachineRepresentation rep, Node* node);
  void MarkAsWord32(Node* node) {
    MarkAsRepresentation(MachineRepresentation::kWord32, node);
  }
  void MarkAsWord64(Node* node) {
    MarkAsRepresentation(MachineRepresentation::kWord64, node);
  }
  void MarkAsFloat32(Node* node) {
    MarkAsRepresentation(MachineRepresentation::kFloat32, node);
  }
  void MarkAsFloat64(Node* node) {
    MarkAsRepresentation(MachineRepresentation::kFloat64, node);
  }
  void MarkAsReference(Node* node) {
    MarkAsRepresentation(MachineRepresentation::kTagged, node);
  }

  // Inform the register allocation of the representation of the unallocated
  // operand {op}.
  void MarkAsRepresentation(MachineRepresentation rep,
                            const InstructionOperand& op);

  enum CallBufferFlag {
    kCallCodeImmediate = 1u << 0,
    kCallAddressImmediate = 1u << 1,
    kCallTail = 1u << 2
  };
  typedef base::Flags<CallBufferFlag> CallBufferFlags;

  // Initialize the call buffer with the InstructionOperands, nodes, etc,
  // corresponding
  // to the inputs and outputs of the call.
  // {call_code_immediate} to generate immediate operands to calls of code.
  // {call_address_immediate} to generate immediate operands to address calls.
  void InitializeCallBuffer(Node* call, CallBuffer* buffer,
                            CallBufferFlags flags, int stack_param_delta = 0);
  bool IsTailCallAddressImmediate();

  FrameStateDescriptor* GetFrameStateDescriptor(Node* node);

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

  void VisitFinishRegion(Node* node);
  void VisitGuard(Node* node);
  void VisitParameter(Node* node);
  void VisitIfException(Node* node);
  void VisitOsrValue(Node* node);
  void VisitPhi(Node* node);
  void VisitProjection(Node* node);
  void VisitConstant(Node* node);
  void VisitCall(Node* call, BasicBlock* handler = nullptr);
  void VisitTailCall(Node* call);
  void VisitGoto(BasicBlock* target);
  void VisitBranch(Node* input, BasicBlock* tbranch, BasicBlock* fbranch);
  void VisitSwitch(Node* node, const SwitchInfo& sw);
  void VisitDeoptimize(DeoptimizeKind kind, Node* value);
  void VisitReturn(Node* ret);
  void VisitThrow(Node* value);

  void EmitPrepareArguments(ZoneVector<compiler::PushParameter>* arguments,
                            const CallDescriptor* descriptor, Node* node);

  // ===========================================================================

  Schedule* schedule() const { return schedule_; }
  Linkage* linkage() const { return linkage_; }
  InstructionSequence* sequence() const { return sequence_; }
  Zone* instruction_zone() const { return sequence()->zone(); }
  Zone* zone() const { return zone_; }

  // ===========================================================================

  Zone* const zone_;
  Linkage* const linkage_;
  InstructionSequence* const sequence_;
  SourcePositionTable* const source_positions_;
  SourcePositionMode const source_position_mode_;
  Features features_;
  Schedule* const schedule_;
  BasicBlock* current_block_;
  ZoneVector<Instruction*> instructions_;
  BoolVector defined_;
  BoolVector used_;
  IntVector effect_level_;
  IntVector virtual_registers_;
  InstructionScheduler* scheduler_;
  Frame* frame_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_INSTRUCTION_SELECTOR_H_
