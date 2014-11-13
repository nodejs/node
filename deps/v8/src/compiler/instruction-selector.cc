// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/instruction-selector.h"

#include "src/compiler/graph.h"
#include "src/compiler/instruction-selector-impl.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties-inl.h"
#include "src/compiler/pipeline.h"

namespace v8 {
namespace internal {
namespace compiler {

InstructionSelector::InstructionSelector(Zone* local_zone, Graph* graph,
                                         Linkage* linkage,
                                         InstructionSequence* sequence,
                                         Schedule* schedule,
                                         SourcePositionTable* source_positions,
                                         Features features)
    : zone_(local_zone),
      linkage_(linkage),
      sequence_(sequence),
      source_positions_(source_positions),
      features_(features),
      schedule_(schedule),
      node_map_(graph->NodeCount(), kNodeUnmapped, zone()),
      current_block_(NULL),
      instructions_(zone()),
      defined_(graph->NodeCount(), false, zone()),
      used_(graph->NodeCount(), false, zone()) {}


void InstructionSelector::SelectInstructions() {
  // Mark the inputs of all phis in loop headers as used.
  BasicBlockVector* blocks = schedule()->rpo_order();
  for (BasicBlockVectorIter i = blocks->begin(); i != blocks->end(); ++i) {
    BasicBlock* block = *i;
    if (!block->IsLoopHeader()) continue;
    DCHECK_NE(0, static_cast<int>(block->PredecessorCount()));
    DCHECK_NE(1, static_cast<int>(block->PredecessorCount()));
    for (BasicBlock::const_iterator j = block->begin(); j != block->end();
         ++j) {
      Node* phi = *j;
      if (phi->opcode() != IrOpcode::kPhi) continue;

      // Mark all inputs as used.
      Node::Inputs inputs = phi->inputs();
      for (InputIter k = inputs.begin(); k != inputs.end(); ++k) {
        MarkAsUsed(*k);
      }
    }
  }

  // Visit each basic block in post order.
  for (BasicBlockVectorRIter i = blocks->rbegin(); i != blocks->rend(); ++i) {
    VisitBlock(*i);
  }

  // Schedule the selected instructions.
  for (BasicBlockVectorIter i = blocks->begin(); i != blocks->end(); ++i) {
    BasicBlock* block = *i;
    InstructionBlock* instruction_block =
        sequence()->InstructionBlockAt(block->GetRpoNumber());
    size_t end = instruction_block->code_end();
    size_t start = instruction_block->code_start();
    sequence()->StartBlock(block->GetRpoNumber());
    while (start-- > end) {
      sequence()->AddInstruction(instructions_[start]);
    }
    sequence()->EndBlock(block->GetRpoNumber());
  }
}


Instruction* InstructionSelector::Emit(InstructionCode opcode,
                                       InstructionOperand* output,
                                       size_t temp_count,
                                       InstructionOperand** temps) {
  size_t output_count = output == NULL ? 0 : 1;
  return Emit(opcode, output_count, &output, 0, NULL, temp_count, temps);
}


Instruction* InstructionSelector::Emit(InstructionCode opcode,
                                       InstructionOperand* output,
                                       InstructionOperand* a, size_t temp_count,
                                       InstructionOperand** temps) {
  size_t output_count = output == NULL ? 0 : 1;
  return Emit(opcode, output_count, &output, 1, &a, temp_count, temps);
}


Instruction* InstructionSelector::Emit(InstructionCode opcode,
                                       InstructionOperand* output,
                                       InstructionOperand* a,
                                       InstructionOperand* b, size_t temp_count,
                                       InstructionOperand** temps) {
  size_t output_count = output == NULL ? 0 : 1;
  InstructionOperand* inputs[] = {a, b};
  size_t input_count = arraysize(inputs);
  return Emit(opcode, output_count, &output, input_count, inputs, temp_count,
              temps);
}


Instruction* InstructionSelector::Emit(InstructionCode opcode,
                                       InstructionOperand* output,
                                       InstructionOperand* a,
                                       InstructionOperand* b,
                                       InstructionOperand* c, size_t temp_count,
                                       InstructionOperand** temps) {
  size_t output_count = output == NULL ? 0 : 1;
  InstructionOperand* inputs[] = {a, b, c};
  size_t input_count = arraysize(inputs);
  return Emit(opcode, output_count, &output, input_count, inputs, temp_count,
              temps);
}


Instruction* InstructionSelector::Emit(
    InstructionCode opcode, InstructionOperand* output, InstructionOperand* a,
    InstructionOperand* b, InstructionOperand* c, InstructionOperand* d,
    size_t temp_count, InstructionOperand** temps) {
  size_t output_count = output == NULL ? 0 : 1;
  InstructionOperand* inputs[] = {a, b, c, d};
  size_t input_count = arraysize(inputs);
  return Emit(opcode, output_count, &output, input_count, inputs, temp_count,
              temps);
}


Instruction* InstructionSelector::Emit(
    InstructionCode opcode, size_t output_count, InstructionOperand** outputs,
    size_t input_count, InstructionOperand** inputs, size_t temp_count,
    InstructionOperand** temps) {
  Instruction* instr =
      Instruction::New(instruction_zone(), opcode, output_count, outputs,
                       input_count, inputs, temp_count, temps);
  return Emit(instr);
}


Instruction* InstructionSelector::Emit(Instruction* instr) {
  instructions_.push_back(instr);
  return instr;
}


bool InstructionSelector::IsNextInAssemblyOrder(const BasicBlock* block) const {
  return current_block_->GetAoNumber().IsNext(block->GetAoNumber());
}


bool InstructionSelector::CanCover(Node* user, Node* node) const {
  return node->OwnedBy(user) &&
         schedule()->block(node) == schedule()->block(user);
}


int InstructionSelector::GetVirtualRegister(const Node* node) {
  if (node_map_[node->id()] == kNodeUnmapped) {
    node_map_[node->id()] = sequence()->NextVirtualRegister();
  }
  return node_map_[node->id()];
}


int InstructionSelector::GetMappedVirtualRegister(const Node* node) const {
  return node_map_[node->id()];
}


bool InstructionSelector::IsDefined(Node* node) const {
  DCHECK_NOT_NULL(node);
  NodeId id = node->id();
  DCHECK(id >= 0);
  DCHECK(id < static_cast<NodeId>(defined_.size()));
  return defined_[id];
}


void InstructionSelector::MarkAsDefined(Node* node) {
  DCHECK_NOT_NULL(node);
  NodeId id = node->id();
  DCHECK(id >= 0);
  DCHECK(id < static_cast<NodeId>(defined_.size()));
  defined_[id] = true;
}


bool InstructionSelector::IsUsed(Node* node) const {
  if (!node->op()->HasProperty(Operator::kEliminatable)) return true;
  NodeId id = node->id();
  DCHECK(id >= 0);
  DCHECK(id < static_cast<NodeId>(used_.size()));
  return used_[id];
}


void InstructionSelector::MarkAsUsed(Node* node) {
  DCHECK_NOT_NULL(node);
  NodeId id = node->id();
  DCHECK(id >= 0);
  DCHECK(id < static_cast<NodeId>(used_.size()));
  used_[id] = true;
}


bool InstructionSelector::IsDouble(const Node* node) const {
  DCHECK_NOT_NULL(node);
  int virtual_register = GetMappedVirtualRegister(node);
  if (virtual_register == kNodeUnmapped) return false;
  return sequence()->IsDouble(virtual_register);
}


void InstructionSelector::MarkAsDouble(Node* node) {
  DCHECK_NOT_NULL(node);
  DCHECK(!IsReference(node));
  sequence()->MarkAsDouble(GetVirtualRegister(node));
}


bool InstructionSelector::IsReference(const Node* node) const {
  DCHECK_NOT_NULL(node);
  int virtual_register = GetMappedVirtualRegister(node);
  if (virtual_register == kNodeUnmapped) return false;
  return sequence()->IsReference(virtual_register);
}


void InstructionSelector::MarkAsReference(Node* node) {
  DCHECK_NOT_NULL(node);
  DCHECK(!IsDouble(node));
  sequence()->MarkAsReference(GetVirtualRegister(node));
}


void InstructionSelector::MarkAsRepresentation(MachineType rep,
                                               InstructionOperand* op) {
  UnallocatedOperand* unalloc = UnallocatedOperand::cast(op);
  switch (RepresentationOf(rep)) {
    case kRepFloat32:
    case kRepFloat64:
      sequence()->MarkAsDouble(unalloc->virtual_register());
      break;
    case kRepTagged:
      sequence()->MarkAsReference(unalloc->virtual_register());
      break;
    default:
      break;
  }
}


void InstructionSelector::MarkAsRepresentation(MachineType rep, Node* node) {
  DCHECK_NOT_NULL(node);
  switch (RepresentationOf(rep)) {
    case kRepFloat32:
    case kRepFloat64:
      MarkAsDouble(node);
      break;
    case kRepTagged:
      MarkAsReference(node);
      break;
    default:
      break;
  }
}


// TODO(bmeurer): Get rid of the CallBuffer business and make
// InstructionSelector::VisitCall platform independent instead.
CallBuffer::CallBuffer(Zone* zone, CallDescriptor* d,
                       FrameStateDescriptor* frame_desc)
    : descriptor(d),
      frame_state_descriptor(frame_desc),
      output_nodes(zone),
      outputs(zone),
      instruction_args(zone),
      pushed_nodes(zone) {
  output_nodes.reserve(d->ReturnCount());
  outputs.reserve(d->ReturnCount());
  pushed_nodes.reserve(input_count());
  instruction_args.reserve(input_count() + frame_state_value_count());
}


// TODO(bmeurer): Get rid of the CallBuffer business and make
// InstructionSelector::VisitCall platform independent instead.
void InstructionSelector::InitializeCallBuffer(Node* call, CallBuffer* buffer,
                                               bool call_code_immediate,
                                               bool call_address_immediate) {
  OperandGenerator g(this);
  DCHECK_EQ(call->op()->OutputCount(),
            static_cast<int>(buffer->descriptor->ReturnCount()));
  DCHECK_EQ(
      call->op()->ValueInputCount(),
      static_cast<int>(buffer->input_count() + buffer->frame_state_count()));

  if (buffer->descriptor->ReturnCount() > 0) {
    // Collect the projections that represent multiple outputs from this call.
    if (buffer->descriptor->ReturnCount() == 1) {
      buffer->output_nodes.push_back(call);
    } else {
      buffer->output_nodes.resize(buffer->descriptor->ReturnCount(), NULL);
      call->CollectProjections(&buffer->output_nodes);
    }

    // Filter out the outputs that aren't live because no projection uses them.
    size_t outputs_needed_by_framestate =
        buffer->frame_state_descriptor == NULL
            ? 0
            : buffer->frame_state_descriptor->state_combine()
                  .ConsumedOutputCount();
    for (size_t i = 0; i < buffer->output_nodes.size(); i++) {
      bool output_is_live =
          buffer->output_nodes[i] != NULL || i < outputs_needed_by_framestate;
      if (output_is_live) {
        MachineType type =
            buffer->descriptor->GetReturnType(static_cast<int>(i));
        LinkageLocation location =
            buffer->descriptor->GetReturnLocation(static_cast<int>(i));

        Node* output = buffer->output_nodes[i];
        InstructionOperand* op =
            output == NULL ? g.TempLocation(location, type)
                           : g.DefineAsLocation(output, location, type);
        MarkAsRepresentation(type, op);

        buffer->outputs.push_back(op);
      }
    }
  }

  // The first argument is always the callee code.
  Node* callee = call->InputAt(0);
  switch (buffer->descriptor->kind()) {
    case CallDescriptor::kCallCodeObject:
      buffer->instruction_args.push_back(
          (call_code_immediate && callee->opcode() == IrOpcode::kHeapConstant)
              ? g.UseImmediate(callee)
              : g.UseRegister(callee));
      break;
    case CallDescriptor::kCallAddress:
      buffer->instruction_args.push_back(
          (call_address_immediate &&
           (callee->opcode() == IrOpcode::kInt32Constant ||
            callee->opcode() == IrOpcode::kInt64Constant))
              ? g.UseImmediate(callee)
              : g.UseRegister(callee));
      break;
    case CallDescriptor::kCallJSFunction:
      buffer->instruction_args.push_back(
          g.UseLocation(callee, buffer->descriptor->GetInputLocation(0),
                        buffer->descriptor->GetInputType(0)));
      break;
  }
  DCHECK_EQ(1, static_cast<int>(buffer->instruction_args.size()));

  // If the call needs a frame state, we insert the state information as
  // follows (n is the number of value inputs to the frame state):
  // arg 1               : deoptimization id.
  // arg 2 - arg (n + 1) : value inputs to the frame state.
  if (buffer->frame_state_descriptor != NULL) {
    InstructionSequence::StateId state_id =
        sequence()->AddFrameStateDescriptor(buffer->frame_state_descriptor);
    buffer->instruction_args.push_back(g.TempImmediate(state_id.ToInt()));

    Node* frame_state =
        call->InputAt(static_cast<int>(buffer->descriptor->InputCount()));
    AddFrameStateInputs(frame_state, &buffer->instruction_args,
                        buffer->frame_state_descriptor);
  }
  DCHECK(1 + buffer->frame_state_value_count() ==
         buffer->instruction_args.size());

  size_t input_count = static_cast<size_t>(buffer->input_count());

  // Split the arguments into pushed_nodes and instruction_args. Pushed
  // arguments require an explicit push instruction before the call and do
  // not appear as arguments to the call. Everything else ends up
  // as an InstructionOperand argument to the call.
  InputIter iter(call->inputs().begin());
  int pushed_count = 0;
  for (size_t index = 0; index < input_count; ++iter, ++index) {
    DCHECK(iter != call->inputs().end());
    DCHECK(index == static_cast<size_t>(iter.index()));
    DCHECK((*iter)->op()->opcode() != IrOpcode::kFrameState);
    if (index == 0) continue;  // The first argument (callee) is already done.
    InstructionOperand* op =
        g.UseLocation(*iter, buffer->descriptor->GetInputLocation(index),
                      buffer->descriptor->GetInputType(index));
    if (UnallocatedOperand::cast(op)->HasFixedSlotPolicy()) {
      int stack_index = -UnallocatedOperand::cast(op)->fixed_slot_index() - 1;
      if (static_cast<size_t>(stack_index) >= buffer->pushed_nodes.size()) {
        buffer->pushed_nodes.resize(stack_index + 1, NULL);
      }
      DCHECK_EQ(NULL, buffer->pushed_nodes[stack_index]);
      buffer->pushed_nodes[stack_index] = *iter;
      pushed_count++;
    } else {
      buffer->instruction_args.push_back(op);
    }
  }
  CHECK_EQ(pushed_count, static_cast<int>(buffer->pushed_nodes.size()));
  DCHECK(static_cast<size_t>(input_count) ==
         (buffer->instruction_args.size() + buffer->pushed_nodes.size() -
          buffer->frame_state_value_count()));
}


void InstructionSelector::VisitBlock(BasicBlock* block) {
  DCHECK_EQ(NULL, current_block_);
  current_block_ = block;
  int current_block_end = static_cast<int>(instructions_.size());

  // Generate code for the block control "top down", but schedule the code
  // "bottom up".
  VisitControl(block);
  std::reverse(instructions_.begin() + current_block_end, instructions_.end());

  // Visit code in reverse control flow order, because architecture-specific
  // matching may cover more than one node at a time.
  for (BasicBlock::reverse_iterator i = block->rbegin(); i != block->rend();
       ++i) {
    Node* node = *i;
    // Skip nodes that are unused or already defined.
    if (!IsUsed(node) || IsDefined(node)) continue;
    // Generate code for this node "top down", but schedule the code "bottom
    // up".
    size_t current_node_end = instructions_.size();
    VisitNode(node);
    std::reverse(instructions_.begin() + current_node_end, instructions_.end());
  }

  // We're done with the block.
  InstructionBlock* instruction_block =
      sequence()->InstructionBlockAt(block->GetRpoNumber());
  instruction_block->set_code_start(static_cast<int>(instructions_.size()));
  instruction_block->set_code_end(current_block_end);

  current_block_ = NULL;
}


static inline void CheckNoPhis(const BasicBlock* block) {
#ifdef DEBUG
  // Branch targets should not have phis.
  for (BasicBlock::const_iterator i = block->begin(); i != block->end(); ++i) {
    const Node* node = *i;
    CHECK_NE(IrOpcode::kPhi, node->opcode());
  }
#endif
}


void InstructionSelector::VisitControl(BasicBlock* block) {
  Node* input = block->control_input();
  switch (block->control()) {
    case BasicBlock::kGoto:
      return VisitGoto(block->SuccessorAt(0));
    case BasicBlock::kBranch: {
      DCHECK_EQ(IrOpcode::kBranch, input->opcode());
      BasicBlock* tbranch = block->SuccessorAt(0);
      BasicBlock* fbranch = block->SuccessorAt(1);
      // SSA deconstruction requires targets of branches not to have phis.
      // Edge split form guarantees this property, but is more strict.
      CheckNoPhis(tbranch);
      CheckNoPhis(fbranch);
      if (tbranch == fbranch) return VisitGoto(tbranch);
      return VisitBranch(input, tbranch, fbranch);
    }
    case BasicBlock::kReturn: {
      // If the result itself is a return, return its input.
      Node* value = (input != NULL && input->opcode() == IrOpcode::kReturn)
                        ? input->InputAt(0)
                        : input;
      return VisitReturn(value);
    }
    case BasicBlock::kThrow:
      return VisitThrow(input);
    case BasicBlock::kNone: {
      // TODO(titzer): exit block doesn't have control.
      DCHECK(input == NULL);
      break;
    }
    default:
      UNREACHABLE();
      break;
  }
}


MachineType InstructionSelector::GetMachineType(Node* node) {
  DCHECK_NOT_NULL(schedule()->block(node));  // should only use scheduled nodes.
  switch (node->opcode()) {
    case IrOpcode::kStart:
    case IrOpcode::kLoop:
    case IrOpcode::kEnd:
    case IrOpcode::kBranch:
    case IrOpcode::kIfTrue:
    case IrOpcode::kIfFalse:
    case IrOpcode::kEffectPhi:
    case IrOpcode::kMerge:
    case IrOpcode::kTerminate:
      // No code needed for these graph artifacts.
      return kMachNone;
    case IrOpcode::kFinish:
      return kMachAnyTagged;
    case IrOpcode::kParameter:
      return linkage()->GetParameterType(OpParameter<int>(node));
    case IrOpcode::kPhi:
      return OpParameter<MachineType>(node);
    case IrOpcode::kProjection:
      // TODO(jarin) Really project from outputs.
      return kMachAnyTagged;
    case IrOpcode::kInt32Constant:
      return kMachInt32;
    case IrOpcode::kInt64Constant:
      return kMachInt64;
    case IrOpcode::kExternalConstant:
      return kMachPtr;
    case IrOpcode::kFloat64Constant:
      return kMachFloat64;
    case IrOpcode::kHeapConstant:
    case IrOpcode::kNumberConstant:
      return kMachAnyTagged;
    case IrOpcode::kCall:
      return kMachAnyTagged;
    case IrOpcode::kFrameState:
    case IrOpcode::kStateValues:
      return kMachNone;
    case IrOpcode::kLoad:
      return OpParameter<LoadRepresentation>(node);
    case IrOpcode::kStore:
      return kMachNone;
    case IrOpcode::kWord32And:
    case IrOpcode::kWord32Or:
    case IrOpcode::kWord32Xor:
    case IrOpcode::kWord32Shl:
    case IrOpcode::kWord32Shr:
    case IrOpcode::kWord32Sar:
    case IrOpcode::kWord32Ror:
      return kMachInt32;
    case IrOpcode::kWord32Equal:
      return kMachBool;
    case IrOpcode::kWord64And:
    case IrOpcode::kWord64Or:
    case IrOpcode::kWord64Xor:
    case IrOpcode::kWord64Shl:
    case IrOpcode::kWord64Shr:
    case IrOpcode::kWord64Sar:
    case IrOpcode::kWord64Ror:
      return kMachInt64;
    case IrOpcode::kWord64Equal:
      return kMachBool;
    case IrOpcode::kInt32Add:
    case IrOpcode::kInt32AddWithOverflow:
    case IrOpcode::kInt32Sub:
    case IrOpcode::kInt32SubWithOverflow:
    case IrOpcode::kInt32Mul:
    case IrOpcode::kInt32Div:
    case IrOpcode::kInt32Mod:
      return kMachInt32;
    case IrOpcode::kInt32LessThan:
    case IrOpcode::kInt32LessThanOrEqual:
    case IrOpcode::kUint32LessThan:
    case IrOpcode::kUint32LessThanOrEqual:
      return kMachBool;
    case IrOpcode::kInt64Add:
    case IrOpcode::kInt64Sub:
    case IrOpcode::kInt64Mul:
    case IrOpcode::kInt64Div:
    case IrOpcode::kInt64Mod:
      return kMachInt64;
    case IrOpcode::kInt64LessThan:
    case IrOpcode::kInt64LessThanOrEqual:
      return kMachBool;
    case IrOpcode::kChangeFloat32ToFloat64:
    case IrOpcode::kChangeInt32ToFloat64:
    case IrOpcode::kChangeUint32ToFloat64:
      return kMachFloat64;
    case IrOpcode::kChangeFloat64ToInt32:
      return kMachInt32;
    case IrOpcode::kChangeFloat64ToUint32:
      return kMachUint32;
    case IrOpcode::kChangeInt32ToInt64:
      return kMachInt64;
    case IrOpcode::kChangeUint32ToUint64:
      return kMachUint64;
    case IrOpcode::kTruncateFloat64ToFloat32:
      return kMachFloat32;
    case IrOpcode::kTruncateFloat64ToInt32:
    case IrOpcode::kTruncateInt64ToInt32:
      return kMachInt32;
    case IrOpcode::kFloat64Add:
    case IrOpcode::kFloat64Sub:
    case IrOpcode::kFloat64Mul:
    case IrOpcode::kFloat64Div:
    case IrOpcode::kFloat64Mod:
    case IrOpcode::kFloat64Sqrt:
    case IrOpcode::kFloat64Floor:
    case IrOpcode::kFloat64Ceil:
    case IrOpcode::kFloat64RoundTruncate:
    case IrOpcode::kFloat64RoundTiesAway:
      return kMachFloat64;
    case IrOpcode::kFloat64Equal:
    case IrOpcode::kFloat64LessThan:
    case IrOpcode::kFloat64LessThanOrEqual:
      return kMachBool;
    default:
      V8_Fatal(__FILE__, __LINE__, "Unexpected operator #%d:%s @ node #%d",
               node->opcode(), node->op()->mnemonic(), node->id());
  }
  return kMachNone;
}


void InstructionSelector::VisitNode(Node* node) {
  DCHECK_NOT_NULL(schedule()->block(node));  // should only use scheduled nodes.
  SourcePosition source_position = source_positions_->GetSourcePosition(node);
  if (!source_position.IsUnknown()) {
    DCHECK(!source_position.IsInvalid());
    if (FLAG_turbo_source_positions || node->opcode() == IrOpcode::kCall) {
      Emit(SourcePositionInstruction::New(instruction_zone(), source_position));
    }
  }
  switch (node->opcode()) {
    case IrOpcode::kStart:
    case IrOpcode::kLoop:
    case IrOpcode::kEnd:
    case IrOpcode::kBranch:
    case IrOpcode::kIfTrue:
    case IrOpcode::kIfFalse:
    case IrOpcode::kEffectPhi:
    case IrOpcode::kMerge:
      // No code needed for these graph artifacts.
      return;
    case IrOpcode::kFinish:
      return MarkAsReference(node), VisitFinish(node);
    case IrOpcode::kParameter: {
      MachineType type = linkage()->GetParameterType(OpParameter<int>(node));
      MarkAsRepresentation(type, node);
      return VisitParameter(node);
    }
    case IrOpcode::kPhi: {
      MachineType type = OpParameter<MachineType>(node);
      MarkAsRepresentation(type, node);
      return VisitPhi(node);
    }
    case IrOpcode::kProjection:
      return VisitProjection(node);
    case IrOpcode::kInt32Constant:
    case IrOpcode::kInt64Constant:
    case IrOpcode::kExternalConstant:
      return VisitConstant(node);
    case IrOpcode::kFloat32Constant:
      return MarkAsDouble(node), VisitConstant(node);
    case IrOpcode::kFloat64Constant:
      return MarkAsDouble(node), VisitConstant(node);
    case IrOpcode::kHeapConstant:
    case IrOpcode::kNumberConstant:
      // TODO(turbofan): only mark non-smis as references.
      return MarkAsReference(node), VisitConstant(node);
    case IrOpcode::kCall:
      return VisitCall(node);
    case IrOpcode::kFrameState:
    case IrOpcode::kStateValues:
      return;
    case IrOpcode::kLoad: {
      LoadRepresentation rep = OpParameter<LoadRepresentation>(node);
      MarkAsRepresentation(rep, node);
      return VisitLoad(node);
    }
    case IrOpcode::kStore:
      return VisitStore(node);
    case IrOpcode::kWord32And:
      return VisitWord32And(node);
    case IrOpcode::kWord32Or:
      return VisitWord32Or(node);
    case IrOpcode::kWord32Xor:
      return VisitWord32Xor(node);
    case IrOpcode::kWord32Shl:
      return VisitWord32Shl(node);
    case IrOpcode::kWord32Shr:
      return VisitWord32Shr(node);
    case IrOpcode::kWord32Sar:
      return VisitWord32Sar(node);
    case IrOpcode::kWord32Ror:
      return VisitWord32Ror(node);
    case IrOpcode::kWord32Equal:
      return VisitWord32Equal(node);
    case IrOpcode::kWord64And:
      return VisitWord64And(node);
    case IrOpcode::kWord64Or:
      return VisitWord64Or(node);
    case IrOpcode::kWord64Xor:
      return VisitWord64Xor(node);
    case IrOpcode::kWord64Shl:
      return VisitWord64Shl(node);
    case IrOpcode::kWord64Shr:
      return VisitWord64Shr(node);
    case IrOpcode::kWord64Sar:
      return VisitWord64Sar(node);
    case IrOpcode::kWord64Ror:
      return VisitWord64Ror(node);
    case IrOpcode::kWord64Equal:
      return VisitWord64Equal(node);
    case IrOpcode::kInt32Add:
      return VisitInt32Add(node);
    case IrOpcode::kInt32AddWithOverflow:
      return VisitInt32AddWithOverflow(node);
    case IrOpcode::kInt32Sub:
      return VisitInt32Sub(node);
    case IrOpcode::kInt32SubWithOverflow:
      return VisitInt32SubWithOverflow(node);
    case IrOpcode::kInt32Mul:
      return VisitInt32Mul(node);
    case IrOpcode::kInt32MulHigh:
      return VisitInt32MulHigh(node);
    case IrOpcode::kInt32Div:
      return VisitInt32Div(node);
    case IrOpcode::kInt32Mod:
      return VisitInt32Mod(node);
    case IrOpcode::kInt32LessThan:
      return VisitInt32LessThan(node);
    case IrOpcode::kInt32LessThanOrEqual:
      return VisitInt32LessThanOrEqual(node);
    case IrOpcode::kUint32Div:
      return VisitUint32Div(node);
    case IrOpcode::kUint32LessThan:
      return VisitUint32LessThan(node);
    case IrOpcode::kUint32LessThanOrEqual:
      return VisitUint32LessThanOrEqual(node);
    case IrOpcode::kUint32Mod:
      return VisitUint32Mod(node);
    case IrOpcode::kUint32MulHigh:
      return VisitUint32MulHigh(node);
    case IrOpcode::kInt64Add:
      return VisitInt64Add(node);
    case IrOpcode::kInt64Sub:
      return VisitInt64Sub(node);
    case IrOpcode::kInt64Mul:
      return VisitInt64Mul(node);
    case IrOpcode::kInt64Div:
      return VisitInt64Div(node);
    case IrOpcode::kInt64Mod:
      return VisitInt64Mod(node);
    case IrOpcode::kInt64LessThan:
      return VisitInt64LessThan(node);
    case IrOpcode::kInt64LessThanOrEqual:
      return VisitInt64LessThanOrEqual(node);
    case IrOpcode::kUint64Div:
      return VisitUint64Div(node);
    case IrOpcode::kUint64LessThan:
      return VisitUint64LessThan(node);
    case IrOpcode::kUint64Mod:
      return VisitUint64Mod(node);
    case IrOpcode::kChangeFloat32ToFloat64:
      return MarkAsDouble(node), VisitChangeFloat32ToFloat64(node);
    case IrOpcode::kChangeInt32ToFloat64:
      return MarkAsDouble(node), VisitChangeInt32ToFloat64(node);
    case IrOpcode::kChangeUint32ToFloat64:
      return MarkAsDouble(node), VisitChangeUint32ToFloat64(node);
    case IrOpcode::kChangeFloat64ToInt32:
      return VisitChangeFloat64ToInt32(node);
    case IrOpcode::kChangeFloat64ToUint32:
      return VisitChangeFloat64ToUint32(node);
    case IrOpcode::kChangeInt32ToInt64:
      return VisitChangeInt32ToInt64(node);
    case IrOpcode::kChangeUint32ToUint64:
      return VisitChangeUint32ToUint64(node);
    case IrOpcode::kTruncateFloat64ToFloat32:
      return MarkAsDouble(node), VisitTruncateFloat64ToFloat32(node);
    case IrOpcode::kTruncateFloat64ToInt32:
      return VisitTruncateFloat64ToInt32(node);
    case IrOpcode::kTruncateInt64ToInt32:
      return VisitTruncateInt64ToInt32(node);
    case IrOpcode::kFloat64Add:
      return MarkAsDouble(node), VisitFloat64Add(node);
    case IrOpcode::kFloat64Sub:
      return MarkAsDouble(node), VisitFloat64Sub(node);
    case IrOpcode::kFloat64Mul:
      return MarkAsDouble(node), VisitFloat64Mul(node);
    case IrOpcode::kFloat64Div:
      return MarkAsDouble(node), VisitFloat64Div(node);
    case IrOpcode::kFloat64Mod:
      return MarkAsDouble(node), VisitFloat64Mod(node);
    case IrOpcode::kFloat64Sqrt:
      return MarkAsDouble(node), VisitFloat64Sqrt(node);
    case IrOpcode::kFloat64Equal:
      return VisitFloat64Equal(node);
    case IrOpcode::kFloat64LessThan:
      return VisitFloat64LessThan(node);
    case IrOpcode::kFloat64LessThanOrEqual:
      return VisitFloat64LessThanOrEqual(node);
    case IrOpcode::kFloat64Floor:
      return MarkAsDouble(node), VisitFloat64Floor(node);
    case IrOpcode::kFloat64Ceil:
      return MarkAsDouble(node), VisitFloat64Ceil(node);
    case IrOpcode::kFloat64RoundTruncate:
      return MarkAsDouble(node), VisitFloat64RoundTruncate(node);
    case IrOpcode::kFloat64RoundTiesAway:
      return MarkAsDouble(node), VisitFloat64RoundTiesAway(node);
    case IrOpcode::kLoadStackPointer:
      return VisitLoadStackPointer(node);
    default:
      V8_Fatal(__FILE__, __LINE__, "Unexpected operator #%d:%s @ node #%d",
               node->opcode(), node->op()->mnemonic(), node->id());
      break;
  }
}


#if V8_TURBOFAN_BACKEND

void InstructionSelector::VisitTruncateFloat64ToInt32(Node* node) {
  OperandGenerator g(this);
  Emit(kArchTruncateDoubleToI, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitLoadStackPointer(Node* node) {
  OperandGenerator g(this);
  Emit(kArchStackPointer, g.DefineAsRegister(node));
}

#endif  // V8_TURBOFAN_BACKEND

// 32 bit targets do not implement the following instructions.
#if V8_TARGET_ARCH_32_BIT && !V8_TARGET_ARCH_X64 && V8_TURBOFAN_BACKEND

void InstructionSelector::VisitWord64And(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitWord64Or(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitWord64Xor(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitWord64Shl(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitWord64Shr(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitWord64Sar(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitWord64Ror(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitWord64Equal(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitInt64Add(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitInt64Sub(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitInt64Mul(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitInt64Div(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitInt64LessThan(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitInt64LessThanOrEqual(Node* node) {
  UNIMPLEMENTED();
}


void InstructionSelector::VisitUint64Div(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitInt64Mod(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitUint64LessThan(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitUint64Mod(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitChangeInt32ToInt64(Node* node) {
  UNIMPLEMENTED();
}


void InstructionSelector::VisitChangeUint32ToUint64(Node* node) {
  UNIMPLEMENTED();
}


void InstructionSelector::VisitTruncateInt64ToInt32(Node* node) {
  UNIMPLEMENTED();
}

#endif  // V8_TARGET_ARCH_32_BIT && !V8_TARGET_ARCH_X64 && V8_TURBOFAN_BACKEND


void InstructionSelector::VisitFinish(Node* node) {
  OperandGenerator g(this);
  Node* value = node->InputAt(0);
  Emit(kArchNop, g.DefineSameAsFirst(node), g.Use(value));
}


void InstructionSelector::VisitParameter(Node* node) {
  OperandGenerator g(this);
  int index = OpParameter<int>(node);
  Emit(kArchNop,
       g.DefineAsLocation(node, linkage()->GetParameterLocation(index),
                          linkage()->GetParameterType(index)));
}


void InstructionSelector::VisitPhi(Node* node) {
  // TODO(bmeurer): Emit a PhiInstruction here.
  PhiInstruction* phi = new (instruction_zone())
      PhiInstruction(instruction_zone(), GetVirtualRegister(node));
  sequence()->InstructionBlockAt(current_block_->GetRpoNumber())->AddPhi(phi);
  const int input_count = node->op()->InputCount();
  phi->operands().reserve(static_cast<size_t>(input_count));
  for (int i = 0; i < input_count; ++i) {
    Node* const input = node->InputAt(i);
    MarkAsUsed(input);
    phi->operands().push_back(GetVirtualRegister(input));
  }
}


void InstructionSelector::VisitProjection(Node* node) {
  OperandGenerator g(this);
  Node* value = node->InputAt(0);
  switch (value->opcode()) {
    case IrOpcode::kInt32AddWithOverflow:
    case IrOpcode::kInt32SubWithOverflow:
      if (OpParameter<size_t>(node) == 0) {
        Emit(kArchNop, g.DefineSameAsFirst(node), g.Use(value));
      } else {
        DCHECK(OpParameter<size_t>(node) == 1u);
        MarkAsUsed(value);
      }
      break;
    default:
      break;
  }
}


void InstructionSelector::VisitConstant(Node* node) {
  // We must emit a NOP here because every live range needs a defining
  // instruction in the register allocator.
  OperandGenerator g(this);
  Emit(kArchNop, g.DefineAsConstant(node));
}


void InstructionSelector::VisitGoto(BasicBlock* target) {
  if (IsNextInAssemblyOrder(target)) {
    // fall through to the next block.
    Emit(kArchNop, NULL)->MarkAsControl();
  } else {
    // jump to the next block.
    OperandGenerator g(this);
    Emit(kArchJmp, NULL, g.Label(target))->MarkAsControl();
  }
}


void InstructionSelector::VisitReturn(Node* value) {
  OperandGenerator g(this);
  if (value != NULL) {
    Emit(kArchRet, NULL, g.UseLocation(value, linkage()->GetReturnLocation(),
                                       linkage()->GetReturnType()));
  } else {
    Emit(kArchRet, NULL);
  }
}


void InstructionSelector::VisitThrow(Node* value) {
  UNIMPLEMENTED();  // TODO(titzer)
}


void InstructionSelector::FillTypeVectorFromStateValues(
    ZoneVector<MachineType>* types, Node* state_values) {
  DCHECK(state_values->opcode() == IrOpcode::kStateValues);
  int count = state_values->InputCount();
  types->reserve(static_cast<size_t>(count));
  for (int i = 0; i < count; i++) {
    types->push_back(GetMachineType(state_values->InputAt(i)));
  }
}


FrameStateDescriptor* InstructionSelector::GetFrameStateDescriptor(
    Node* state) {
  DCHECK(state->opcode() == IrOpcode::kFrameState);
  DCHECK_EQ(5, state->InputCount());
  DCHECK_EQ(IrOpcode::kStateValues, state->InputAt(0)->opcode());
  DCHECK_EQ(IrOpcode::kStateValues, state->InputAt(1)->opcode());
  DCHECK_EQ(IrOpcode::kStateValues, state->InputAt(2)->opcode());
  FrameStateCallInfo state_info = OpParameter<FrameStateCallInfo>(state);

  int parameters = state->InputAt(0)->InputCount();
  int locals = state->InputAt(1)->InputCount();
  int stack = state->InputAt(2)->InputCount();

  FrameStateDescriptor* outer_state = NULL;
  Node* outer_node = state->InputAt(4);
  if (outer_node->opcode() == IrOpcode::kFrameState) {
    outer_state = GetFrameStateDescriptor(outer_node);
  }

  return new (instruction_zone()) FrameStateDescriptor(
      instruction_zone(), state_info, parameters, locals, stack, outer_state);
}


static InstructionOperand* UseOrImmediate(OperandGenerator* g, Node* input) {
  switch (input->opcode()) {
    case IrOpcode::kInt32Constant:
    case IrOpcode::kNumberConstant:
    case IrOpcode::kFloat64Constant:
    case IrOpcode::kHeapConstant:
      return g->UseImmediate(input);
    default:
      return g->UseUnique(input);
  }
}


void InstructionSelector::AddFrameStateInputs(
    Node* state, InstructionOperandVector* inputs,
    FrameStateDescriptor* descriptor) {
  DCHECK_EQ(IrOpcode::kFrameState, state->op()->opcode());

  if (descriptor->outer_state() != NULL) {
    AddFrameStateInputs(state->InputAt(4), inputs, descriptor->outer_state());
  }

  Node* parameters = state->InputAt(0);
  Node* locals = state->InputAt(1);
  Node* stack = state->InputAt(2);
  Node* context = state->InputAt(3);

  DCHECK_EQ(IrOpcode::kStateValues, parameters->op()->opcode());
  DCHECK_EQ(IrOpcode::kStateValues, locals->op()->opcode());
  DCHECK_EQ(IrOpcode::kStateValues, stack->op()->opcode());

  DCHECK_EQ(static_cast<int>(descriptor->parameters_count()),
            parameters->InputCount());
  DCHECK_EQ(static_cast<int>(descriptor->locals_count()), locals->InputCount());
  DCHECK_EQ(static_cast<int>(descriptor->stack_count()), stack->InputCount());

  ZoneVector<MachineType> types(instruction_zone());
  types.reserve(descriptor->GetSize());

  OperandGenerator g(this);
  size_t value_index = 0;
  for (int i = 0; i < static_cast<int>(descriptor->parameters_count()); i++) {
    Node* input_node = parameters->InputAt(i);
    inputs->push_back(UseOrImmediate(&g, input_node));
    descriptor->SetType(value_index++, GetMachineType(input_node));
  }
  if (descriptor->HasContext()) {
    inputs->push_back(UseOrImmediate(&g, context));
    descriptor->SetType(value_index++, kMachAnyTagged);
  }
  for (int i = 0; i < static_cast<int>(descriptor->locals_count()); i++) {
    Node* input_node = locals->InputAt(i);
    inputs->push_back(UseOrImmediate(&g, input_node));
    descriptor->SetType(value_index++, GetMachineType(input_node));
  }
  for (int i = 0; i < static_cast<int>(descriptor->stack_count()); i++) {
    Node* input_node = stack->InputAt(i);
    inputs->push_back(UseOrImmediate(&g, input_node));
    descriptor->SetType(value_index++, GetMachineType(input_node));
  }
  DCHECK(value_index == descriptor->GetSize());
}


#if !V8_TURBOFAN_BACKEND

#define DECLARE_UNIMPLEMENTED_SELECTOR(x) \
  void InstructionSelector::Visit##x(Node* node) { UNIMPLEMENTED(); }
MACHINE_OP_LIST(DECLARE_UNIMPLEMENTED_SELECTOR)
#undef DECLARE_UNIMPLEMENTED_SELECTOR


void InstructionSelector::VisitCall(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitBranch(Node* branch, BasicBlock* tbranch,
                                      BasicBlock* fbranch) {
  UNIMPLEMENTED();
}


// static
MachineOperatorBuilder::Flags
InstructionSelector::SupportedMachineOperatorFlags() {
  return MachineOperatorBuilder::Flag::kNoFlags;
}

#endif  // !V8_TURBOFAN_BACKEND

}  // namespace compiler
}  // namespace internal
}  // namespace v8
