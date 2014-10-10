// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/instruction-selector.h"

#include "src/compiler/instruction-selector-impl.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties-inl.h"
#include "src/compiler/pipeline.h"

namespace v8 {
namespace internal {
namespace compiler {

InstructionSelector::InstructionSelector(InstructionSequence* sequence,
                                         SourcePositionTable* source_positions,
                                         Features features)
    : zone_(sequence->isolate()),
      sequence_(sequence),
      source_positions_(source_positions),
      features_(features),
      current_block_(NULL),
      instructions_(zone()),
      defined_(graph()->NodeCount(), false, zone()),
      used_(graph()->NodeCount(), false, zone()) {}


void InstructionSelector::SelectInstructions() {
  // Mark the inputs of all phis in loop headers as used.
  BasicBlockVector* blocks = schedule()->rpo_order();
  for (BasicBlockVectorIter i = blocks->begin(); i != blocks->end(); ++i) {
    BasicBlock* block = *i;
    if (!block->IsLoopHeader()) continue;
    DCHECK_NE(0, block->PredecessorCount());
    DCHECK_NE(1, block->PredecessorCount());
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
    size_t end = block->code_end_;
    size_t start = block->code_start_;
    sequence()->StartBlock(block);
    while (start-- > end) {
      sequence()->AddInstruction(instructions_[start], block);
    }
    sequence()->EndBlock(block);
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
  return block->rpo_number_ == (current_block_->rpo_number_ + 1) &&
         block->deferred_ == current_block_->deferred_;
}


bool InstructionSelector::CanCover(Node* user, Node* node) const {
  return node->OwnedBy(user) &&
         schedule()->block(node) == schedule()->block(user);
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
  return sequence()->IsDouble(node->id());
}


void InstructionSelector::MarkAsDouble(Node* node) {
  DCHECK_NOT_NULL(node);
  DCHECK(!IsReference(node));
  sequence()->MarkAsDouble(node->id());
}


bool InstructionSelector::IsReference(const Node* node) const {
  DCHECK_NOT_NULL(node);
  return sequence()->IsReference(node->id());
}


void InstructionSelector::MarkAsReference(Node* node) {
  DCHECK_NOT_NULL(node);
  DCHECK(!IsDouble(node));
  sequence()->MarkAsReference(node->id());
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
  DCHECK_EQ(call->op()->OutputCount(), buffer->descriptor->ReturnCount());
  DCHECK_EQ(OperatorProperties::GetValueInputCount(call->op()),
            buffer->input_count() + buffer->frame_state_count());

  if (buffer->descriptor->ReturnCount() > 0) {
    // Collect the projections that represent multiple outputs from this call.
    if (buffer->descriptor->ReturnCount() == 1) {
      buffer->output_nodes.push_back(call);
    } else {
      buffer->output_nodes.resize(buffer->descriptor->ReturnCount(), NULL);
      call->CollectProjections(&buffer->output_nodes);
    }

    // Filter out the outputs that aren't live because no projection uses them.
    for (size_t i = 0; i < buffer->output_nodes.size(); i++) {
      if (buffer->output_nodes[i] != NULL) {
        Node* output = buffer->output_nodes[i];
        MachineType type =
            buffer->descriptor->GetReturnType(static_cast<int>(i));
        LinkageLocation location =
            buffer->descriptor->GetReturnLocation(static_cast<int>(i));
        MarkAsRepresentation(type, output);
        buffer->outputs.push_back(g.DefineAsLocation(output, location, type));
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
  DCHECK_EQ(1, buffer->instruction_args.size());

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
  // TODO(bmeurer): We should not mutate the schedule.
  block->code_end_ = current_block_end;
  block->code_start_ = static_cast<int>(instructions_.size());

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
  Node* input = block->control_input_;
  switch (block->control_) {
    case BasicBlockData::kGoto:
      return VisitGoto(block->SuccessorAt(0));
    case BasicBlockData::kBranch: {
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
    case BasicBlockData::kReturn: {
      // If the result itself is a return, return its input.
      Node* value = (input != NULL && input->opcode() == IrOpcode::kReturn)
                        ? input->InputAt(0)
                        : input;
      return VisitReturn(value);
    }
    case BasicBlockData::kThrow:
      return VisitThrow(input);
    case BasicBlockData::kNone: {
      // TODO(titzer): exit block doesn't have control.
      DCHECK(input == NULL);
      break;
    }
    default:
      UNREACHABLE();
      break;
  }
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
      return VisitCall(node, NULL, NULL);
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
    case IrOpcode::kInt32Div:
      return VisitInt32Div(node);
    case IrOpcode::kInt32UDiv:
      return VisitInt32UDiv(node);
    case IrOpcode::kInt32Mod:
      return VisitInt32Mod(node);
    case IrOpcode::kInt32UMod:
      return VisitInt32UMod(node);
    case IrOpcode::kInt32LessThan:
      return VisitInt32LessThan(node);
    case IrOpcode::kInt32LessThanOrEqual:
      return VisitInt32LessThanOrEqual(node);
    case IrOpcode::kUint32LessThan:
      return VisitUint32LessThan(node);
    case IrOpcode::kUint32LessThanOrEqual:
      return VisitUint32LessThanOrEqual(node);
    case IrOpcode::kInt64Add:
      return VisitInt64Add(node);
    case IrOpcode::kInt64Sub:
      return VisitInt64Sub(node);
    case IrOpcode::kInt64Mul:
      return VisitInt64Mul(node);
    case IrOpcode::kInt64Div:
      return VisitInt64Div(node);
    case IrOpcode::kInt64UDiv:
      return VisitInt64UDiv(node);
    case IrOpcode::kInt64Mod:
      return VisitInt64Mod(node);
    case IrOpcode::kInt64UMod:
      return VisitInt64UMod(node);
    case IrOpcode::kInt64LessThan:
      return VisitInt64LessThan(node);
    case IrOpcode::kInt64LessThanOrEqual:
      return VisitInt64LessThanOrEqual(node);
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
    default:
      V8_Fatal(__FILE__, __LINE__, "Unexpected operator #%d:%s @ node #%d",
               node->opcode(), node->op()->mnemonic(), node->id());
  }
}


#if V8_TURBOFAN_BACKEND

void InstructionSelector::VisitWord32Equal(Node* node) {
  FlagsContinuation cont(kEqual, node);
  Int32BinopMatcher m(node);
  if (m.right().Is(0)) {
    return VisitWord32Test(m.left().node(), &cont);
  }
  VisitWord32Compare(node, &cont);
}


void InstructionSelector::VisitInt32LessThan(Node* node) {
  FlagsContinuation cont(kSignedLessThan, node);
  VisitWord32Compare(node, &cont);
}


void InstructionSelector::VisitInt32LessThanOrEqual(Node* node) {
  FlagsContinuation cont(kSignedLessThanOrEqual, node);
  VisitWord32Compare(node, &cont);
}


void InstructionSelector::VisitUint32LessThan(Node* node) {
  FlagsContinuation cont(kUnsignedLessThan, node);
  VisitWord32Compare(node, &cont);
}


void InstructionSelector::VisitUint32LessThanOrEqual(Node* node) {
  FlagsContinuation cont(kUnsignedLessThanOrEqual, node);
  VisitWord32Compare(node, &cont);
}


void InstructionSelector::VisitWord64Equal(Node* node) {
  FlagsContinuation cont(kEqual, node);
  Int64BinopMatcher m(node);
  if (m.right().Is(0)) {
    return VisitWord64Test(m.left().node(), &cont);
  }
  VisitWord64Compare(node, &cont);
}


void InstructionSelector::VisitInt32AddWithOverflow(Node* node) {
  if (Node* ovf = node->FindProjection(1)) {
    FlagsContinuation cont(kOverflow, ovf);
    return VisitInt32AddWithOverflow(node, &cont);
  }
  FlagsContinuation cont;
  VisitInt32AddWithOverflow(node, &cont);
}


void InstructionSelector::VisitInt32SubWithOverflow(Node* node) {
  if (Node* ovf = node->FindProjection(1)) {
    FlagsContinuation cont(kOverflow, ovf);
    return VisitInt32SubWithOverflow(node, &cont);
  }
  FlagsContinuation cont;
  VisitInt32SubWithOverflow(node, &cont);
}


void InstructionSelector::VisitInt64LessThan(Node* node) {
  FlagsContinuation cont(kSignedLessThan, node);
  VisitWord64Compare(node, &cont);
}


void InstructionSelector::VisitInt64LessThanOrEqual(Node* node) {
  FlagsContinuation cont(kSignedLessThanOrEqual, node);
  VisitWord64Compare(node, &cont);
}


void InstructionSelector::VisitTruncateFloat64ToInt32(Node* node) {
  OperandGenerator g(this);
  Emit(kArchTruncateDoubleToI, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitFloat64Equal(Node* node) {
  FlagsContinuation cont(kUnorderedEqual, node);
  VisitFloat64Compare(node, &cont);
}


void InstructionSelector::VisitFloat64LessThan(Node* node) {
  FlagsContinuation cont(kUnorderedLessThan, node);
  VisitFloat64Compare(node, &cont);
}


void InstructionSelector::VisitFloat64LessThanOrEqual(Node* node) {
  FlagsContinuation cont(kUnorderedLessThanOrEqual, node);
  VisitFloat64Compare(node, &cont);
}

#endif  // V8_TURBOFAN_BACKEND

// 32 bit targets do not implement the following instructions.
#if V8_TARGET_ARCH_32_BIT && V8_TURBOFAN_BACKEND

void InstructionSelector::VisitWord64And(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitWord64Or(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitWord64Xor(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitWord64Shl(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitWord64Shr(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitWord64Sar(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitWord64Ror(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitInt64Add(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitInt64Sub(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitInt64Mul(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitInt64Div(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitInt64UDiv(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitInt64Mod(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitInt64UMod(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitChangeInt32ToInt64(Node* node) {
  UNIMPLEMENTED();
}


void InstructionSelector::VisitChangeUint32ToUint64(Node* node) {
  UNIMPLEMENTED();
}


void InstructionSelector::VisitTruncateInt64ToInt32(Node* node) {
  UNIMPLEMENTED();
}

#endif  // V8_TARGET_ARCH_32_BIT && V8_TURBOFAN_BACKEND


// 32-bit targets and unsupported architectures need dummy implementations of
// selected 64-bit ops.
#if V8_TARGET_ARCH_32_BIT || !V8_TURBOFAN_BACKEND

void InstructionSelector::VisitWord64Test(Node* node, FlagsContinuation* cont) {
  UNIMPLEMENTED();
}


void InstructionSelector::VisitWord64Compare(Node* node,
                                             FlagsContinuation* cont) {
  UNIMPLEMENTED();
}

#endif  // V8_TARGET_ARCH_32_BIT || !V8_TURBOFAN_BACKEND


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
  for (InputIter i = node->inputs().begin(); i != node->inputs().end(); ++i) {
    MarkAsUsed(*i);
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


void InstructionSelector::VisitBranch(Node* branch, BasicBlock* tbranch,
                                      BasicBlock* fbranch) {
  OperandGenerator g(this);
  Node* user = branch;
  Node* value = branch->InputAt(0);

  FlagsContinuation cont(kNotEqual, tbranch, fbranch);

  // If we can fall through to the true block, invert the branch.
  if (IsNextInAssemblyOrder(tbranch)) {
    cont.Negate();
    cont.SwapBlocks();
  }

  // Try to combine with comparisons against 0 by simply inverting the branch.
  while (CanCover(user, value)) {
    if (value->opcode() == IrOpcode::kWord32Equal) {
      Int32BinopMatcher m(value);
      if (m.right().Is(0)) {
        user = value;
        value = m.left().node();
        cont.Negate();
      } else {
        break;
      }
    } else if (value->opcode() == IrOpcode::kWord64Equal) {
      Int64BinopMatcher m(value);
      if (m.right().Is(0)) {
        user = value;
        value = m.left().node();
        cont.Negate();
      } else {
        break;
      }
    } else {
      break;
    }
  }

  // Try to combine the branch with a comparison.
  if (CanCover(user, value)) {
    switch (value->opcode()) {
      case IrOpcode::kWord32Equal:
        cont.OverwriteAndNegateIfEqual(kEqual);
        return VisitWord32Compare(value, &cont);
      case IrOpcode::kInt32LessThan:
        cont.OverwriteAndNegateIfEqual(kSignedLessThan);
        return VisitWord32Compare(value, &cont);
      case IrOpcode::kInt32LessThanOrEqual:
        cont.OverwriteAndNegateIfEqual(kSignedLessThanOrEqual);
        return VisitWord32Compare(value, &cont);
      case IrOpcode::kUint32LessThan:
        cont.OverwriteAndNegateIfEqual(kUnsignedLessThan);
        return VisitWord32Compare(value, &cont);
      case IrOpcode::kUint32LessThanOrEqual:
        cont.OverwriteAndNegateIfEqual(kUnsignedLessThanOrEqual);
        return VisitWord32Compare(value, &cont);
      case IrOpcode::kWord64Equal:
        cont.OverwriteAndNegateIfEqual(kEqual);
        return VisitWord64Compare(value, &cont);
      case IrOpcode::kInt64LessThan:
        cont.OverwriteAndNegateIfEqual(kSignedLessThan);
        return VisitWord64Compare(value, &cont);
      case IrOpcode::kInt64LessThanOrEqual:
        cont.OverwriteAndNegateIfEqual(kSignedLessThanOrEqual);
        return VisitWord64Compare(value, &cont);
      case IrOpcode::kFloat64Equal:
        cont.OverwriteAndNegateIfEqual(kUnorderedEqual);
        return VisitFloat64Compare(value, &cont);
      case IrOpcode::kFloat64LessThan:
        cont.OverwriteAndNegateIfEqual(kUnorderedLessThan);
        return VisitFloat64Compare(value, &cont);
      case IrOpcode::kFloat64LessThanOrEqual:
        cont.OverwriteAndNegateIfEqual(kUnorderedLessThanOrEqual);
        return VisitFloat64Compare(value, &cont);
      case IrOpcode::kProjection:
        // Check if this is the overflow output projection of an
        // <Operation>WithOverflow node.
        if (OpParameter<size_t>(value) == 1u) {
          // We cannot combine the <Operation>WithOverflow with this branch
          // unless the 0th projection (the use of the actual value of the
          // <Operation> is either NULL, which means there's no use of the
          // actual value, or was already defined, which means it is scheduled
          // *AFTER* this branch).
          Node* node = value->InputAt(0);
          Node* result = node->FindProjection(0);
          if (result == NULL || IsDefined(result)) {
            switch (node->opcode()) {
              case IrOpcode::kInt32AddWithOverflow:
                cont.OverwriteAndNegateIfEqual(kOverflow);
                return VisitInt32AddWithOverflow(node, &cont);
              case IrOpcode::kInt32SubWithOverflow:
                cont.OverwriteAndNegateIfEqual(kOverflow);
                return VisitInt32SubWithOverflow(node, &cont);
              default:
                break;
            }
          }
        }
        break;
      default:
        break;
    }
  }

  // Branch could not be combined with a compare, emit compare against 0.
  VisitWord32Test(value, &cont);
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


FrameStateDescriptor* InstructionSelector::GetFrameStateDescriptor(
    Node* state) {
  DCHECK(state->opcode() == IrOpcode::kFrameState);
  DCHECK_EQ(5, state->InputCount());
  FrameStateCallInfo state_info = OpParameter<FrameStateCallInfo>(state);
  int parameters = OpParameter<int>(state->InputAt(0));
  int locals = OpParameter<int>(state->InputAt(1));
  int stack = OpParameter<int>(state->InputAt(2));

  FrameStateDescriptor* outer_state = NULL;
  Node* outer_node = state->InputAt(4);
  if (outer_node->opcode() == IrOpcode::kFrameState) {
    outer_state = GetFrameStateDescriptor(outer_node);
  }

  return new (instruction_zone())
      FrameStateDescriptor(state_info, parameters, locals, stack, outer_state);
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

  DCHECK_EQ(descriptor->parameters_count(), parameters->InputCount());
  DCHECK_EQ(descriptor->locals_count(), locals->InputCount());
  DCHECK_EQ(descriptor->stack_count(), stack->InputCount());

  OperandGenerator g(this);
  for (int i = 0; i < static_cast<int>(descriptor->parameters_count()); i++) {
    inputs->push_back(UseOrImmediate(&g, parameters->InputAt(i)));
  }
  if (descriptor->HasContext()) {
    inputs->push_back(UseOrImmediate(&g, context));
  }
  for (int i = 0; i < static_cast<int>(descriptor->locals_count()); i++) {
    inputs->push_back(UseOrImmediate(&g, locals->InputAt(i)));
  }
  for (int i = 0; i < static_cast<int>(descriptor->stack_count()); i++) {
    inputs->push_back(UseOrImmediate(&g, stack->InputAt(i)));
  }
}


#if !V8_TURBOFAN_BACKEND

#define DECLARE_UNIMPLEMENTED_SELECTOR(x) \
  void InstructionSelector::Visit##x(Node* node) { UNIMPLEMENTED(); }
MACHINE_OP_LIST(DECLARE_UNIMPLEMENTED_SELECTOR)
#undef DECLARE_UNIMPLEMENTED_SELECTOR


void InstructionSelector::VisitInt32AddWithOverflow(Node* node,
                                                    FlagsContinuation* cont) {
  UNIMPLEMENTED();
}


void InstructionSelector::VisitInt32SubWithOverflow(Node* node,
                                                    FlagsContinuation* cont) {
  UNIMPLEMENTED();
}


void InstructionSelector::VisitWord32Test(Node* node, FlagsContinuation* cont) {
  UNIMPLEMENTED();
}


void InstructionSelector::VisitWord32Compare(Node* node,
                                             FlagsContinuation* cont) {
  UNIMPLEMENTED();
}


void InstructionSelector::VisitFloat64Compare(Node* node,
                                              FlagsContinuation* cont) {
  UNIMPLEMENTED();
}


void InstructionSelector::VisitCall(Node* call, BasicBlock* continuation,
                                    BasicBlock* deoptimization) {}

#endif  // !V8_TURBOFAN_BACKEND

}  // namespace compiler
}  // namespace internal
}  // namespace v8
