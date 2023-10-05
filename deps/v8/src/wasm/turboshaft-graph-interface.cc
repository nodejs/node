// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/turboshaft-graph-interface.h"

#include "src/common/globals.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/graph.h"
#include "src/compiler/wasm-compiler-definitions.h"
#include "src/trap-handler/trap-handler.h"
#include "src/wasm/compilation-environment.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/memory-tracing.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-linkage.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-opcodes-inl.h"

namespace v8::internal::wasm {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

using Assembler =
    compiler::turboshaft::Assembler<compiler::turboshaft::reducer_list<>>;
using compiler::CallDescriptor;
using compiler::MemoryAccessKind;
using compiler::TrapId;
using compiler::turboshaft::ConditionWithHint;
using compiler::turboshaft::Float32;
using compiler::turboshaft::Float64;
using compiler::turboshaft::FloatRepresentation;
using compiler::turboshaft::Graph;
using compiler::turboshaft::Label;
using compiler::turboshaft::LoadOp;
using compiler::turboshaft::LoopLabel;
using compiler::turboshaft::MemoryRepresentation;
using compiler::turboshaft::OpIndex;
using compiler::turboshaft::PendingLoopPhiOp;
using compiler::turboshaft::RegisterRepresentation;
using compiler::turboshaft::StoreOp;
using compiler::turboshaft::SupportedOperations;
using compiler::turboshaft::Tagged;
using compiler::turboshaft::WordRepresentation;
using TSBlock = compiler::turboshaft::Block;
using compiler::turboshaft::TSCallDescriptor;
using compiler::turboshaft::V;
using compiler::turboshaft::Word32;
using compiler::turboshaft::Word64;
using compiler::turboshaft::WordPtr;

#define LOAD_INSTANCE_FIELD(name, representation)                       \
  asm_.Load(instance_node_, LoadOp::Kind::TaggedBase(), representation, \
            WasmInstanceObject::k##name##Offset)

#define LOAD_ROOT(name)                                          \
  asm_.Load(asm_.LoadRootRegister(), LoadOp::Kind::RawAligned(), \
            MemoryRepresentation::PointerSized(),                \
            IsolateData::root_slot_offset(RootIndex::k##name))

class TurboshaftGraphBuildingInterface {
 public:
  using ValidationTag = Decoder::FullValidationTag;
  using FullDecoder =
      WasmFullDecoder<ValidationTag, TurboshaftGraphBuildingInterface>;
  static constexpr bool kUsesPoppedArgs = true;
  static constexpr MemoryRepresentation kMaybeSandboxedPointer =
      V8_ENABLE_SANDBOX_BOOL ? MemoryRepresentation::SandboxedPointer()
                             : MemoryRepresentation::PointerSized();

  struct Value : public ValueBase<ValidationTag> {
    OpIndex op = OpIndex::Invalid();
    template <typename... Args>
    explicit Value(Args&&... args) V8_NOEXCEPT
        : ValueBase(std::forward<Args>(args)...) {}
  };

  struct Control : public ControlBase<Value, ValidationTag> {
    TSBlock* merge_block = nullptr;
    // for 'if', loops, and 'try' respectively.
    TSBlock* false_or_loop_or_catch_block = nullptr;
    V<Tagged> exception = OpIndex::Invalid();  // Only for 'try-catch'.

    template <typename... Args>
    explicit Control(Args&&... args) V8_NOEXCEPT
        : ControlBase(std::forward<Args>(args)...) {}
  };

  TurboshaftGraphBuildingInterface(Graph& graph, Zone* zone,
                                   compiler::NodeOriginTable* node_origins)
      : asm_(graph, graph, zone, node_origins) {}

  void StartFunction(FullDecoder* decoder) {
    TSBlock* block = asm_.NewBlock();
    asm_.Bind(block);
    // Set 0 as the current source position (before locals declarations).
    asm_.SetCurrentOrigin(WasmPositionToOpIndex(0));
    instance_node_ = asm_.Parameter(0, RegisterRepresentation::Tagged());
    ssa_env_.resize(decoder->num_locals());
    uint32_t index = 0;
    for (; index < decoder->sig_->parameter_count(); index++) {
      // Parameter indices are shifted by 1 because parameter 0 is the instance.
      ssa_env_[index] = asm_.Parameter(
          index + 1, RepresentationFor(decoder->sig_->GetParam(index)));
    }
    while (index < decoder->num_locals()) {
      ValueType type = decoder->local_type(index);
      OpIndex op;
      if (!type.is_defaultable()) {
        DCHECK(type.is_reference());
        // TODO(jkummerow): Consider using "the hole" instead, to make any
        // illegal uses more obvious.
        op = asm_.Null(type.AsNullable());
      } else {
        op = DefaultValue(type);
      }
      while (index < decoder->num_locals() &&
             decoder->local_type(index) == type) {
        ssa_env_[index++] = op;
      }
    }

    StackCheck();  // TODO(14108): Remove for leaf functions.

    if (v8_flags.trace_wasm) {
      asm_.SetCurrentOrigin(WasmPositionToOpIndex(decoder->position()));
      CallRuntime(Runtime::kWasmTraceEnter, {});
    }
  }

  void StartFunctionBody(FullDecoder* decoder, Control* block) {}

  void FinishFunction(FullDecoder* decoder) {
    for (OpIndex index : asm_.output_graph().AllOperationIndices()) {
      WasmCodePosition position =
          OpIndexToWasmPosition(asm_.output_graph().operation_origins()[index]);
      asm_.output_graph().source_positions()[index] = SourcePosition(position);
    }
  }

  void OnFirstError(FullDecoder*) {}

  void NextInstruction(FullDecoder* decoder, WasmOpcode) {
    asm_.SetCurrentOrigin(WasmPositionToOpIndex(decoder->position()));
  }

  // ******** Control Flow ********
  // The basic structure of control flow is {block_phis_}. It contains a mapping
  // from blocks to phi inputs corresponding to the SSA values plus the stack
  // merge values at the beginning of the block.
  // - When we create a new block (to be bound in the future), we register it to
  //   {block_phis_} with {NewBlock}.
  // - When we encounter an jump to a block, we invoke {SetupControlFlowEdge}.
  // - Finally, when we bind a block, we setup its phis, the SSA environment,
  //   and its merge values, with {EnterBlock}.
  // - When we create a loop, we generate PendingLoopPhis for the SSA state and
  //   the incoming stack values. We also create a block which will act as a
  //   merge block for all loop backedges (since a loop in Turboshaft can only
  //   have one backedge). When we PopControl a loop, we enter the merge block
  //   to create its Phis for all backedges as necessary, and use those values
  //   to patch the backedge of the PendingLoopPhis of the loop.

  void Block(FullDecoder* decoder, Control* block) {
    block->merge_block = NewBlock(decoder, block->br_merge());
  }

  void Loop(FullDecoder* decoder, Control* block) {
    TSBlock* loop = asm_.NewLoopHeader();
    asm_.Goto(loop);
    asm_.Bind(loop);
    for (uint32_t i = 0; i < decoder->num_locals(); i++) {
      OpIndex phi = asm_.PendingLoopPhi(
          ssa_env_[i], RepresentationFor(decoder->local_type(i)));
      ssa_env_[i] = phi;
    }
    uint32_t arity = block->start_merge.arity;
    Value* stack_base = arity > 0 ? decoder->stack_value(arity) : nullptr;
    for (uint32_t i = 0; i < arity; i++) {
      OpIndex phi = asm_.PendingLoopPhi(stack_base[i].op,
                                        RepresentationFor(stack_base[i].type));
      block->start_merge[i].op = phi;
    }

    StackCheck();

    TSBlock* loop_merge = NewBlock(decoder, &block->start_merge);
    block->merge_block = loop_merge;
    block->false_or_loop_or_catch_block = loop;
  }

  void If(FullDecoder* decoder, const Value& cond, Control* if_block) {
    TSBlock* true_block = NewBlock(decoder, nullptr);
    TSBlock* false_block = NewBlock(decoder, nullptr);
    TSBlock* merge_block = NewBlock(decoder, &if_block->end_merge);
    if_block->false_or_loop_or_catch_block = false_block;
    if_block->merge_block = merge_block;
    SetupControlFlowEdge(decoder, true_block);
    SetupControlFlowEdge(decoder, false_block);
    // TODO(14108): Branch hints.
    asm_.Branch(ConditionWithHint(cond.op), true_block, false_block);
    EnterBlock(decoder, true_block, nullptr);
  }

  void Else(FullDecoder* decoder, Control* if_block) {
    if (if_block->reachable()) {
      SetupControlFlowEdge(decoder, if_block->merge_block);
      asm_.Goto(if_block->merge_block);
    }
    EnterBlock(decoder, if_block->false_or_loop_or_catch_block, nullptr);
  }

  void BrOrRet(FullDecoder* decoder, uint32_t depth, uint32_t drop_values) {
    if (depth == decoder->control_depth() - 1) {
      DoReturn(decoder, drop_values);
    } else {
      Control* target = decoder->control_at(depth);
      SetupControlFlowEdge(decoder, target->merge_block, drop_values);
      asm_.Goto(target->merge_block);
    }
  }

  void BrIf(FullDecoder* decoder, const Value& cond, uint32_t depth) {
    if (depth == decoder->control_depth() - 1) {
      TSBlock* return_block = NewBlock(decoder, nullptr);
      SetupControlFlowEdge(decoder, return_block);
      TSBlock* non_branching = NewBlock(decoder, nullptr);
      SetupControlFlowEdge(decoder, non_branching);
      asm_.Branch(ConditionWithHint(cond.op), return_block, non_branching);
      EnterBlock(decoder, return_block, nullptr);
      DoReturn(decoder, 0);
      EnterBlock(decoder, non_branching, nullptr);
    } else {
      Control* target = decoder->control_at(depth);
      SetupControlFlowEdge(decoder, target->merge_block);
      TSBlock* non_branching = NewBlock(decoder, nullptr);
      SetupControlFlowEdge(decoder, non_branching);
      asm_.Branch(ConditionWithHint(cond.op), target->merge_block,
                  non_branching);
      EnterBlock(decoder, non_branching, nullptr);
    }
  }

  void BrTable(FullDecoder* decoder, const BranchTableImmediate& imm,
               const Value& key) {
    compiler::turboshaft::SwitchOp::Case* cases =
        asm_.output_graph()
            .graph_zone()
            ->AllocateArray<compiler::turboshaft::SwitchOp::Case>(
                imm.table_count);
    BranchTableIterator<ValidationTag> new_block_iterator(decoder, imm);
    std::vector<TSBlock*> intermediate_blocks;
    TSBlock* default_case = nullptr;
    while (new_block_iterator.has_next()) {
      TSBlock* intermediate = NewBlock(decoder, nullptr);
      SetupControlFlowEdge(decoder, intermediate);
      intermediate_blocks.emplace_back(intermediate);
      uint32_t i = new_block_iterator.cur_index();
      if (i == imm.table_count) {
        default_case = intermediate;
      } else {
        cases[i] = {static_cast<int>(i), intermediate, BranchHint::kNone};
      }
      new_block_iterator.next();
    }
    DCHECK_NOT_NULL(default_case);
    asm_.Switch(key.op, base::VectorOf(cases, imm.table_count), default_case);

    int i = 0;
    BranchTableIterator<ValidationTag> branch_iterator(decoder, imm);
    while (branch_iterator.has_next()) {
      TSBlock* intermediate = intermediate_blocks[i];
      i++;
      EnterBlock(decoder, intermediate, nullptr);
      BrOrRet(decoder, branch_iterator.next(), 0);
    }
  }

  void FallThruTo(FullDecoder* decoder, Control* block) {
    // TODO(14108): Why is {block->reachable()} not reliable here? Maybe it is
    // not in other spots as well.
    if (asm_.current_block() != nullptr) {
      SetupControlFlowEdge(decoder, block->merge_block);
      asm_.Goto(block->merge_block);
    }
  }

  void PopControl(FullDecoder* decoder, Control* block) {
    switch (block->kind) {
      case kControlIf:
        if (block->reachable()) {
          SetupControlFlowEdge(decoder, block->merge_block);
          asm_.Goto(block->merge_block);
        }
        EnterBlock(decoder, block->false_or_loop_or_catch_block, nullptr);
        // Exceptionally for one-armed if, we cannot take the values from the
        // stack; we have to pass the stack values at the beginning of the
        // if-block.
        SetupControlFlowEdge(decoder, block->merge_block, 0, OpIndex::Invalid(),
                             &block->start_merge);
        asm_.Goto(block->merge_block);
        EnterBlock(decoder, block->merge_block, block->br_merge());
        break;
      case kControlIfElse:
      case kControlBlock:
      case kControlTry:
      case kControlTryCatch:
      case kControlTryCatchAll:
        // {block->reachable()} is not reliable here for exceptions, because
        // the decoder sets the reachability to the upper block's reachability
        // before calling this interface function.
        if (asm_.current_block() != nullptr) {
          SetupControlFlowEdge(decoder, block->merge_block);
          asm_.Goto(block->merge_block);
        }
        EnterBlock(decoder, block->merge_block, block->br_merge());
        break;
      case kControlLoop: {
        TSBlock* post_loop = NewBlock(decoder, nullptr);
        if (block->reachable()) {
          SetupControlFlowEdge(decoder, post_loop);
          asm_.Goto(post_loop);
        }
        if (block->merge_block->PredecessorCount() == 0) {
          // Turns out, the loop has no backedges, i.e. it is not quite a loop
          // at all. Replace it with a merge, and its PendingPhis with one-input
          // phis.
          block->false_or_loop_or_catch_block->SetKind(
              compiler::turboshaft::Block::Kind::kMerge);
          auto to = asm_.output_graph()
                        .operations(*block->false_or_loop_or_catch_block)
                        .begin();
          for (uint32_t i = 0; i < ssa_env_.size() + block->br_merge()->arity;
               ++i, ++to) {
            // TODO(manoskouk): Add `->` operator to the iterator.
            PendingLoopPhiOp& pending_phi = (*to).Cast<PendingLoopPhiOp>();
            OpIndex replaced = asm_.output_graph().Index(*to);
            asm_.output_graph().Replace<compiler::turboshaft::PhiOp>(
                replaced, base::VectorOf({pending_phi.first()}),
                pending_phi.rep);
          }
        } else {
          // We abuse the start merge of the loop, which is not used otherwise
          // anymore, to store backedge inputs for the pending phi stack values
          // of the loop.
          EnterBlock(decoder, block->merge_block, block->br_merge());
          asm_.Goto(block->false_or_loop_or_catch_block);
          auto to = asm_.output_graph()
                        .operations(*block->false_or_loop_or_catch_block)
                        .begin();
          for (uint32_t i = 0; i < ssa_env_.size(); ++i, ++to) {
            PendingLoopPhiOp& pending_phi = (*to).Cast<PendingLoopPhiOp>();
            OpIndex replaced = asm_.output_graph().Index(*to);
            asm_.output_graph().Replace<compiler::turboshaft::PhiOp>(
                replaced, base::VectorOf({pending_phi.first(), ssa_env_[i]}),
                pending_phi.rep);
          }
          for (uint32_t i = 0; i < block->br_merge()->arity; ++i, ++to) {
            PendingLoopPhiOp& pending_phi = (*to).Cast<PendingLoopPhiOp>();
            OpIndex replaced = asm_.output_graph().Index(*to);
            asm_.output_graph().Replace<compiler::turboshaft::PhiOp>(
                replaced,
                base::VectorOf(
                    {pending_phi.first(), (*block->br_merge())[i].op}),
                pending_phi.rep);
          }
        }
        EnterBlock(decoder, post_loop, nullptr);
        break;
      }
    }
  }

  void DoReturn(FullDecoder* decoder, uint32_t drop_values) {
    size_t return_count = decoder->sig_->return_count();
    base::SmallVector<OpIndex, 8> return_values(return_count);
    Value* stack_base = return_count == 0
                            ? nullptr
                            : decoder->stack_value(static_cast<uint32_t>(
                                  return_count + drop_values));
    for (size_t i = 0; i < return_count; i++) {
      return_values[i] = stack_base[i].op;
    }
    if (v8_flags.trace_wasm) {
      V<WordPtr> info = asm_.IntPtrConstant(0);
      if (return_count == 1) {
        wasm::ValueType return_type = decoder->sig_->GetReturn(0);
        int size = return_type.value_kind_size();
        // TODO(14108): This won't fit everything.
        info = asm_.StackSlot(size, size);
        // TODO(14108): Write barrier might be needed.
        asm_.Store(
            info, return_values[0], StoreOp::Kind::RawAligned(),
            MemoryRepresentation::FromMachineType(return_type.machine_type()),
            compiler::kNoWriteBarrier);
      }
      CallRuntime(Runtime::kWasmTraceExit, {info});
    }
    asm_.Return(asm_.Word32Constant(0), base::VectorOf(return_values));
  }

  void UnOp(FullDecoder* decoder, WasmOpcode opcode, const Value& value,
            Value* result) {
    result->op = UnOpImpl(decoder, opcode, value.op, value.type);
  }

  void BinOp(FullDecoder* decoder, WasmOpcode opcode, const Value& lhs,
             const Value& rhs, Value* result) {
    result->op = BinOpImpl(opcode, lhs.op, rhs.op);
  }

  void TraceInstruction(FullDecoder* decoder, uint32_t markid) {
    // TODO(14108): Implement.
  }

  void I32Const(FullDecoder* decoder, Value* result, int32_t value) {
    result->op = asm_.Word32Constant(value);
  }

  void I64Const(FullDecoder* decoder, Value* result, int64_t value) {
    result->op = asm_.Word64Constant(value);
  }

  void F32Const(FullDecoder* decoder, Value* result, float value) {
    result->op = asm_.Float32Constant(value);
  }

  void F64Const(FullDecoder* decoder, Value* result, double value) {
    result->op = asm_.Float64Constant(value);
  }

  void S128Const(FullDecoder* decoder, const Simd128Immediate& imm,
                 Value* result) {
    result->op = asm_.Simd128Constant(imm.value);
  }

  void RefNull(FullDecoder* decoder, ValueType type, Value* result) {
    result->op = asm_.Null(type);
  }

  void RefFunc(FullDecoder* decoder, uint32_t function_index, Value* result) {
    V<FixedArray> functions = LOAD_INSTANCE_FIELD(
        WasmInternalFunctions, MemoryRepresentation::TaggedPointer());
    V<Tagged> maybe_function = LoadFixedArrayElement(functions, function_index);

    Label<WasmInternalFunction> done(&asm_);
    IF (asm_.IsSmi(maybe_function)) {
      V<Word32> function_index_constant = asm_.Word32Constant(function_index);
      V<WasmInternalFunction> from_builtin = CallBuiltinFromRuntimeStub(
          decoder, wasm::WasmCode::kWasmRefFunc, {function_index_constant});
      GOTO(done, from_builtin);
    }
    ELSE {
      GOTO(done, V<WasmInternalFunction>::Cast(maybe_function));
    }
    END_IF
    BIND(done, result_value);

    result->op = result_value;
  }

  void RefAsNonNull(FullDecoder* decoder, const Value& arg, Value* result) {
    result->op =
        asm_.AssertNotNull(arg.op, arg.type, TrapId::kTrapNullDereference);
  }

  void Drop(FullDecoder* decoder) {}

  void LocalGet(FullDecoder* decoder, Value* result,
                const IndexImmediate& imm) {
    result->op = ssa_env_[imm.index];
  }

  void LocalSet(FullDecoder* decoder, const Value& value,
                const IndexImmediate& imm) {
    ssa_env_[imm.index] = value.op;
  }

  void LocalTee(FullDecoder* decoder, const Value& value, Value* result,
                const IndexImmediate& imm) {
    ssa_env_[imm.index] = result->op = value.op;
  }

  void GlobalGet(FullDecoder* decoder, Value* result,
                 const GlobalIndexImmediate& imm) {
    result->op = asm_.GlobalGet(instance_node_, imm.global);
  }

  void GlobalSet(FullDecoder* decoder, const Value& value,
                 const GlobalIndexImmediate& imm) {
    asm_.GlobalSet(instance_node_, value.op, imm.global);
  }

  void Trap(FullDecoder* decoder, TrapReason reason) {
    asm_.TrapIfNot(asm_.Word32Constant(0), OpIndex::Invalid(),
                   GetTrapIdForTrap(reason));
    asm_.Unreachable();
  }

  void AssertNullTypecheck(FullDecoder* decoder, const Value& obj,
                           Value* result) {
    asm_.TrapIfNot(asm_.IsNull(obj.op, obj.type), OpIndex::Invalid(),
                   TrapId::kTrapIllegalCast);
    Forward(decoder, obj, result);
  }

  void AssertNotNullTypecheck(FullDecoder* decoder, const Value& obj,
                              Value* result) {
    asm_.AssertNotNull(obj.op, obj.type, TrapId::kTrapIllegalCast);
    Forward(decoder, obj, result);
  }

  void NopForTestingUnsupportedInLiftoff(FullDecoder* decoder) {
    Bailout(decoder);
  }

  void Select(FullDecoder* decoder, const Value& cond, const Value& fval,
              const Value& tval, Value* result) {
    using Implementation = compiler::turboshaft::SelectOp::Implementation;
    bool use_select = false;
    switch (tval.type.kind()) {
      case kI32:
        if (SupportedOperations::word32_select()) use_select = true;
        break;
      case kI64:
        if (SupportedOperations::word64_select()) use_select = true;
        break;
      case kF32:
        if (SupportedOperations::float32_select()) use_select = true;
        break;
      case kF64:
        if (SupportedOperations::float64_select()) use_select = true;
        break;
      case kRef:
      case kRefNull:
      case kS128:
        break;
      case kI8:
      case kI16:
      case kRtt:
      case kVoid:
      case kBottom:
        UNREACHABLE();
    }

    if (use_select) {
      result->op =
          asm_.Select(cond.op, tval.op, fval.op, RepresentationFor(tval.type),
                      BranchHint::kNone, Implementation::kCMove);
      return;
    } else {
      TSBlock* true_block = asm_.NewBlock();
      TSBlock* false_block = asm_.NewBlock();
      TSBlock* merge_block = asm_.NewBlock();
      asm_.Branch(ConditionWithHint(cond.op), true_block, false_block);
      asm_.Bind(true_block);
      asm_.Goto(merge_block);
      asm_.Bind(false_block);
      asm_.Goto(merge_block);
      asm_.Bind(merge_block);
      result->op = asm_.Phi({tval.op, fval.op}, RepresentationFor(tval.type));
    }
  }

  // TODO(14108): Cache memories' starts and sizes. Consider VariableReducer,
  // LoadElimination, or manual handling like ssa_env_.
  void LoadMem(FullDecoder* decoder, LoadType type,
               const MemoryAccessImmediate& imm, const Value& index,
               Value* result) {
#if defined(V8_TARGET_BIG_ENDIAN)
    // TODO(14108): Implement for big endian.
    Bailout(decoder);
#endif

    MemoryRepresentation repr =
        MemoryRepresentation::FromMachineType(type.mem_type());

    auto [final_index, strategy] =
        BoundsCheckMem(imm.memory, repr, index.op, imm.offset,
                       compiler::EnforceBoundsCheck::kCanOmitBoundsCheck);

    V<WordPtr> mem_start = MemStart(imm.memory->index);

    LoadOp::Kind load_kind = GetMemoryAccessKind(repr, strategy);

    // TODO(14108): If offset is in int range, use it as static offset.
    OpIndex load = asm_.Load(asm_.WordPtrAdd(mem_start, imm.offset),
                             final_index, load_kind, repr);
    OpIndex extended_load =
        (type.value_type() == kWasmI64 && repr.SizeInBytes() < 8)
            ? (repr.IsSigned() ? asm_.ChangeInt32ToInt64(load)
                               : asm_.ChangeUint32ToUint64(load))
            : load;

    if (v8_flags.trace_wasm_memory) {
      // TODO(14259): Implement memory tracing for multiple memories.
      CHECK_EQ(0, imm.memory->index);
      TraceMemoryOperation(false, repr, final_index, imm.offset);
    }

    result->op = extended_load;
  }

  void LoadTransform(FullDecoder* decoder, LoadType type,
                     LoadTransformationKind transform,
                     const MemoryAccessImmediate& imm, const Value& index,
                     Value* result) {
#if defined(V8_TARGET_BIG_ENDIAN)
    // TODO(14108): Implement for big endian.
    Bailout(decoder);
#endif
    MemoryRepresentation repr =
        transform == LoadTransformationKind::kExtend
            ? MemoryRepresentation::Int64()
            : MemoryRepresentation::FromMachineType(type.mem_type());

    auto [final_index, strategy] =
        BoundsCheckMem(imm.memory, repr, index.op, imm.offset,
                       compiler::EnforceBoundsCheck::kCanOmitBoundsCheck);

    compiler::turboshaft::Simd128LoadTransformOp::LoadKind load_kind =
        GetMemoryAccessKind(repr, strategy);

    using TransformKind =
        compiler::turboshaft::Simd128LoadTransformOp::TransformKind;

    TransformKind transform_kind;

    if (transform == LoadTransformationKind::kExtend) {
      if (type.mem_type() == MachineType::Int8()) {
        transform_kind = TransformKind::k8x8S;
      } else if (type.mem_type() == MachineType::Uint8()) {
        transform_kind = TransformKind::k8x8U;
      } else if (type.mem_type() == MachineType::Int16()) {
        transform_kind = TransformKind::k16x4S;
      } else if (type.mem_type() == MachineType::Uint16()) {
        transform_kind = TransformKind::k16x4U;
      } else if (type.mem_type() == MachineType::Int32()) {
        transform_kind = TransformKind::k32x2S;
      } else if (type.mem_type() == MachineType::Uint32()) {
        transform_kind = TransformKind::k32x2U;
      } else {
        UNREACHABLE();
      }
    } else if (transform == LoadTransformationKind::kSplat) {
      if (type.mem_type() == MachineType::Int8()) {
        transform_kind = TransformKind::k8Splat;
      } else if (type.mem_type() == MachineType::Int16()) {
        transform_kind = TransformKind::k16Splat;
      } else if (type.mem_type() == MachineType::Int32()) {
        transform_kind = TransformKind::k32Splat;
      } else if (type.mem_type() == MachineType::Int64()) {
        transform_kind = TransformKind::k64Splat;
      } else {
        UNREACHABLE();
      }
    } else {
      if (type.mem_type() == MachineType::Int32()) {
        transform_kind = TransformKind::k32Zero;
      } else if (type.mem_type() == MachineType::Int64()) {
        transform_kind = TransformKind::k64Zero;
      } else {
        UNREACHABLE();
      }
    }

    OpIndex load = asm_.Simd128LoadTransform(
        asm_.WordPtrAdd(MemStart(imm.mem_index), imm.offset), final_index,
        load_kind, transform_kind, 0);

    if (v8_flags.trace_wasm_memory) {
      TraceMemoryOperation(false, repr, final_index, imm.offset);
    }

    result->op = load;
  }

  void LoadLane(FullDecoder* decoder, LoadType type, const Value& value,
                const Value& index, const MemoryAccessImmediate& imm,
                const uint8_t laneidx, Value* result) {
    using compiler::turboshaft::Simd128LaneMemoryOp;
#if defined(V8_TARGET_BIG_ENDIAN)
    // TODO(14108): Implement for big endian.
    Bailout(decoder);
#endif

    MemoryRepresentation repr =
        MemoryRepresentation::FromMachineType(type.mem_type());

    auto [final_index, strategy] =
        BoundsCheckMem(imm.memory, repr, index.op, imm.offset,
                       compiler::EnforceBoundsCheck::kCanOmitBoundsCheck);
    Simd128LaneMemoryOp::Kind kind = GetMemoryAccessKind(repr, strategy);

    Simd128LaneMemoryOp::LaneKind lane_kind;

    switch (repr) {
      case MemoryRepresentation::Int8():
        lane_kind = Simd128LaneMemoryOp::LaneKind::k8;
        break;
      case MemoryRepresentation::Int16():
        lane_kind = Simd128LaneMemoryOp::LaneKind::k16;
        break;
      case MemoryRepresentation::Int32():
        lane_kind = Simd128LaneMemoryOp::LaneKind::k32;
        break;
      case MemoryRepresentation::Int64():
        lane_kind = Simd128LaneMemoryOp::LaneKind::k64;
        break;
      default:
        UNREACHABLE();
    }

    // TODO(14108): If `offset` is in int range, use it as static offset, or
    // consider using a larger type as offset.
    OpIndex load = asm_.Simd128LaneMemory(
        asm_.WordPtrAdd(MemStart(imm.mem_index), imm.offset), final_index,
        value.op, Simd128LaneMemoryOp::Mode::kLoad, kind, lane_kind, laneidx,
        0);

    if (v8_flags.trace_wasm_memory) {
      TraceMemoryOperation(false, repr, final_index, imm.offset);
    }

    result->op = load;
  }

  void StoreMem(FullDecoder* decoder, StoreType type,
                const MemoryAccessImmediate& imm, const Value& index,
                const Value& value) {
#if defined(V8_TARGET_BIG_ENDIAN)
    // TODO(14108): Implement for big endian.
    Bailout(decoder);
#endif

    MemoryRepresentation repr =
        MemoryRepresentation::FromMachineRepresentation(type.mem_rep());

    auto [final_index, strategy] =
        BoundsCheckMem(imm.memory, repr, index.op, imm.offset,
                       wasm::kPartialOOBWritesAreNoops
                           ? compiler::EnforceBoundsCheck::kCanOmitBoundsCheck
                           : compiler::EnforceBoundsCheck::kNeedsBoundsCheck);

    V<WordPtr> mem_start = MemStart(imm.memory->index);

    StoreOp::Kind store_kind = GetMemoryAccessKind(repr, strategy);

    OpIndex store_value = value.op;
    if (value.type == kWasmI64 && repr.SizeInBytes() <= 4) {
      store_value = asm_.TruncateWord64ToWord32(store_value);
    }
    // TODO(14108): If offset is in int range, use it as static offset.
    asm_.Store(mem_start, asm_.WordPtrAdd(imm.offset, final_index), store_value,
               store_kind, repr, compiler::kNoWriteBarrier, 0);

    if (v8_flags.trace_wasm_memory) {
      // TODO(14259): Implement memory tracing for multiple memories.
      CHECK_EQ(0, imm.memory->index);
      TraceMemoryOperation(true, repr, final_index, imm.offset);
    }
  }

  void StoreLane(FullDecoder* decoder, StoreType type,
                 const MemoryAccessImmediate& imm, const Value& index,
                 const Value& value, const uint8_t laneidx) {
    using compiler::turboshaft::Simd128LaneMemoryOp;
#if defined(V8_TARGET_BIG_ENDIAN)
    // TODO(14108): Implement for big endian.
    Bailout(decoder);
#endif

    MemoryRepresentation repr =
        MemoryRepresentation::FromMachineRepresentation(type.mem_rep());

    auto [final_index, strategy] =
        BoundsCheckMem(imm.memory, repr, index.op, imm.offset,
                       kPartialOOBWritesAreNoops
                           ? compiler::EnforceBoundsCheck::kCanOmitBoundsCheck
                           : compiler::EnforceBoundsCheck::kNeedsBoundsCheck);
    Simd128LaneMemoryOp::Kind kind = GetMemoryAccessKind(repr, strategy);

    Simd128LaneMemoryOp::LaneKind lane_kind;

    switch (repr) {
      // TODO(manoskouk): Why use unsigned representations here as opposed to
      // LoadLane?
      case MemoryRepresentation::Uint8():
        lane_kind = Simd128LaneMemoryOp::LaneKind::k8;
        break;
      case MemoryRepresentation::Uint16():
        lane_kind = Simd128LaneMemoryOp::LaneKind::k16;
        break;
      case MemoryRepresentation::Uint32():
        lane_kind = Simd128LaneMemoryOp::LaneKind::k32;
        break;
      case MemoryRepresentation::Uint64():
        lane_kind = Simd128LaneMemoryOp::LaneKind::k64;
        break;
      default:
        UNREACHABLE();
    }

    // TODO(14108): If `offset` is in int range, use it as static offset, or
    // consider using a larger type as offset.
    asm_.Simd128LaneMemory(asm_.WordPtrAdd(MemStart(imm.mem_index), imm.offset),
                           final_index, value.op,
                           Simd128LaneMemoryOp::Mode::kStore, kind, lane_kind,
                           laneidx, 0);

    if (v8_flags.trace_wasm_memory) {
      TraceMemoryOperation(true, repr, final_index, imm.offset);
    }
  }

  void CurrentMemoryPages(FullDecoder* decoder, const MemoryIndexImmediate& imm,
                          Value* result) {
    V<WordPtr> result_wordptr =
        asm_.WordPtrShiftRightArithmetic(MemSize(imm.index), kWasmPageSizeLog2);
    // In the 32-bit case, truncation happens implicitly.
    if (imm.memory->is_memory64) {
      result->op = asm_.ChangeIntPtrToInt64(result_wordptr);
    } else {
      result->op = asm_.TruncateWordPtrToWord32(result_wordptr);
    }
  }

  void MemoryGrow(FullDecoder* decoder, const MemoryIndexImmediate& imm,
                  const Value& value, Value* result) {
    if (!imm.memory->is_memory64) {
      result->op = CallBuiltinFromRuntimeStub(
          decoder, WasmCode::kWasmMemoryGrow,
          {asm_.Word32Constant(imm.index), value.op});
    } else {
      Label<Word64> done(&asm_);

      IF (asm_.Uint64LessThanOrEqual(
              value.op, asm_.Word64Constant(static_cast<int64_t>(kMaxInt)))) {
        GOTO(done, asm_.ChangeInt32ToInt64(CallBuiltinFromRuntimeStub(
                       decoder, WasmCode::kWasmMemoryGrow,
                       {asm_.Word32Constant(imm.index),
                        asm_.TruncateWord64ToWord32(value.op)})));
      }
      ELSE {
        GOTO(done, asm_.Word64Constant(int64_t{-1}));
      }
      END_IF

      BIND(done, result_64);

      result->op = result_64;
    }
  }

  void CallDirect(FullDecoder* decoder, const CallFunctionImmediate& imm,
                  const Value args[], Value returns[]) {
    if (imm.index < decoder->module_->num_imported_functions) {
      auto [target, ref] = BuildImportedFunctionTargetAndRef(imm.index);
      BuildWasmCall(decoder, imm.sig, target, ref, args, returns);
    } else {
      // Locally defined function.
      V<WordPtr> callee =
          asm_.RelocatableConstant(imm.index, RelocInfo::WASM_CALL);
      BuildWasmCall(decoder, imm.sig, callee, instance_node_, args, returns);
    }
  }

  void ReturnCall(FullDecoder* decoder, const CallFunctionImmediate& imm,
                  const Value args[]) {
    if (imm.index < decoder->module_->num_imported_functions) {
      auto [target, ref] = BuildImportedFunctionTargetAndRef(imm.index);
      BuildWasmReturnCall(imm.sig, target, ref, args);
    } else {
      // Locally defined function.
      V<WordPtr> callee =
          asm_.RelocatableConstant(imm.index, RelocInfo::WASM_CALL);
      BuildWasmReturnCall(imm.sig, callee, instance_node_, args);
    }
  }

  void CallIndirect(FullDecoder* decoder, const Value& index,
                    const CallIndirectImmediate& imm, const Value args[],
                    Value returns[]) {
    auto [target, ref] = BuildIndirectCallTargetAndRef(decoder, index.op, imm);
    if (!target.valid()) return;
    BuildWasmCall(decoder, imm.sig, target, ref, args, returns);
  }

  void ReturnCallIndirect(FullDecoder* decoder, const Value& index,
                          const CallIndirectImmediate& imm,
                          const Value args[]) {
    auto [target, ref] = BuildIndirectCallTargetAndRef(decoder, index.op, imm);
    if (!target.valid()) return;
    BuildWasmReturnCall(imm.sig, target, ref, args);
  }

  void CallRef(FullDecoder* decoder, const Value& func_ref,
               const FunctionSig* sig, uint32_t sig_index, const Value args[],
               Value returns[]) {
    auto [target, ref] =
        BuildFunctionReferenceTargetAndRef(func_ref.op, func_ref.type);
    BuildWasmCall(decoder, sig, target, ref, args, returns);
  }

  void ReturnCallRef(FullDecoder* decoder, const Value& func_ref,
                     const FunctionSig* sig, uint32_t sig_index,
                     const Value args[]) {
    auto [target, ref] =
        BuildFunctionReferenceTargetAndRef(func_ref.op, func_ref.type);
    BuildWasmReturnCall(sig, target, ref, args);
  }

  void BrOnNull(FullDecoder* decoder, const Value& ref_object, uint32_t depth,
                bool pass_null_along_branch, Value* result_on_fallthrough) {
    result_on_fallthrough->op = ref_object.op;
    TSBlock* null_branch = asm_.NewBlock();
    TSBlock* non_null_branch = asm_.NewBlock();
    asm_.Branch(ConditionWithHint(asm_.IsNull(ref_object.op, ref_object.type),
                                  BranchHint::kFalse),
                null_branch, non_null_branch);
    asm_.Bind(null_branch);
    BrOrRet(decoder, depth, pass_null_along_branch ? 0 : 1);
    asm_.Bind(non_null_branch);
  }

  void BrOnNonNull(FullDecoder* decoder, const Value& ref_object, Value* result,
                   uint32_t depth, bool /* drop_null_on_fallthrough */) {
    result->op = ref_object.op;
    TSBlock* null_branch = asm_.NewBlock();
    TSBlock* non_null_branch = asm_.NewBlock();
    asm_.Branch(ConditionWithHint(asm_.IsNull(ref_object.op, ref_object.type),
                                  BranchHint::kFalse),
                null_branch, non_null_branch);
    asm_.Bind(non_null_branch);
    BrOrRet(decoder, depth, 0);
    asm_.Bind(null_branch);
  }

  void SimdOp(FullDecoder* decoder, WasmOpcode opcode, const Value* args,
              Value* result) {
    switch (opcode) {
#define HANDLE_BINARY_OPCODE(kind)                            \
  case kExpr##kind:                                           \
    result->op = asm_.Simd128Binop(                           \
        args[0].op, args[1].op,                               \
        compiler::turboshaft::Simd128BinopOp::Kind::k##kind); \
    break;
      FOREACH_SIMD_128_BINARY_OPCODE(HANDLE_BINARY_OPCODE)
#undef HANDLE_BINARY_OPCODE

#define HANDLE_INVERSE_COMPARISON(wasm_kind, ts_kind)            \
  case kExpr##wasm_kind:                                         \
    result->op = asm_.Simd128Binop(                              \
        args[1].op, args[0].op,                                  \
        compiler::turboshaft::Simd128BinopOp::Kind::k##ts_kind); \
    break;

      HANDLE_INVERSE_COMPARISON(I8x16LtS, I8x16GtS)
      HANDLE_INVERSE_COMPARISON(I8x16LtU, I8x16GtU)
      HANDLE_INVERSE_COMPARISON(I8x16LeS, I8x16GeS)
      HANDLE_INVERSE_COMPARISON(I8x16LeU, I8x16GeU)

      HANDLE_INVERSE_COMPARISON(I16x8LtS, I16x8GtS)
      HANDLE_INVERSE_COMPARISON(I16x8LtU, I16x8GtU)
      HANDLE_INVERSE_COMPARISON(I16x8LeS, I16x8GeS)
      HANDLE_INVERSE_COMPARISON(I16x8LeU, I16x8GeU)

      HANDLE_INVERSE_COMPARISON(I32x4LtS, I32x4GtS)
      HANDLE_INVERSE_COMPARISON(I32x4LtU, I32x4GtU)
      HANDLE_INVERSE_COMPARISON(I32x4LeS, I32x4GeS)
      HANDLE_INVERSE_COMPARISON(I32x4LeU, I32x4GeU)

      HANDLE_INVERSE_COMPARISON(I64x2LtS, I64x2GtS)
      HANDLE_INVERSE_COMPARISON(I64x2LeS, I64x2GeS)

      HANDLE_INVERSE_COMPARISON(F32x4Gt, F32x4Lt)
      HANDLE_INVERSE_COMPARISON(F32x4Ge, F32x4Le)
      HANDLE_INVERSE_COMPARISON(F64x2Gt, F64x2Lt)
      HANDLE_INVERSE_COMPARISON(F64x2Ge, F64x2Le)

#undef HANDLE_INVERSE_COMPARISON

#define HANDLE_UNARY_NON_OPTIONAL_OPCODE(kind)                            \
  case kExpr##kind:                                                       \
    result->op = asm_.Simd128Unary(                                       \
        args[0].op, compiler::turboshaft::Simd128UnaryOp::Kind::k##kind); \
    break;
      FOREACH_SIMD_128_UNARY_NON_OPTIONAL_OPCODE(
          HANDLE_UNARY_NON_OPTIONAL_OPCODE)
#undef HANDLE_UNARY_NON_OPTIONAL_OPCODE

#define HANDLE_UNARY_OPTIONAL_OPCODE(kind, feature, external_ref)           \
  case kExpr##kind:                                                         \
    if (SupportedOperations::feature()) {                                   \
      result->op = asm_.Simd128Unary(                                       \
          args[0].op, compiler::turboshaft::Simd128UnaryOp::Kind::k##kind); \
    } else {                                                                \
      result->op = CallCStackSlotToStackSlot(                               \
          args[0].op, ExternalReference::external_ref(),                    \
          MemoryRepresentation::Simd128());                                 \
    }                                                                       \
    break;
      HANDLE_UNARY_OPTIONAL_OPCODE(F32x4Ceil, float32_round_up, wasm_f32x4_ceil)
      HANDLE_UNARY_OPTIONAL_OPCODE(F32x4Floor, float32_round_down,
                                   wasm_f32x4_floor)
      HANDLE_UNARY_OPTIONAL_OPCODE(F32x4Trunc, float32_round_to_zero,
                                   wasm_f32x4_trunc)
      HANDLE_UNARY_OPTIONAL_OPCODE(F32x4NearestInt, float32_round_ties_even,
                                   wasm_f32x4_nearest_int)
      HANDLE_UNARY_OPTIONAL_OPCODE(F64x2Ceil, float64_round_up, wasm_f64x2_ceil)
      HANDLE_UNARY_OPTIONAL_OPCODE(F64x2Floor, float64_round_down,
                                   wasm_f64x2_floor)
      HANDLE_UNARY_OPTIONAL_OPCODE(F64x2Trunc, float64_round_to_zero,
                                   wasm_f64x2_trunc)
      HANDLE_UNARY_OPTIONAL_OPCODE(F64x2NearestInt, float64_round_ties_even,
                                   wasm_f64x2_nearest_int)
#undef HANDLE_UNARY_OPTIONAL_OPCODE

#define HANDLE_SHIFT_OPCODE(kind)                             \
  case kExpr##kind:                                           \
    result->op = asm_.Simd128Shift(                           \
        args[0].op, args[1].op,                               \
        compiler::turboshaft::Simd128ShiftOp::Kind::k##kind); \
    break;
      FOREACH_SIMD_128_SHIFT_OPCODE(HANDLE_SHIFT_OPCODE)
#undef HANDLE_SHIFT_OPCODE

#define HANDLE_TEST_OPCODE(kind)                                         \
  case kExpr##kind:                                                      \
    result->op = asm_.Simd128Test(                                       \
        args[0].op, compiler::turboshaft::Simd128TestOp::Kind::k##kind); \
    break;
      FOREACH_SIMD_128_TEST_OPCODE(HANDLE_TEST_OPCODE)
#undef HANDLE_TEST_OPCODE

#define HANDLE_SPLAT_OPCODE(kind)                                         \
  case kExpr##kind##Splat:                                                \
    result->op = asm_.Simd128Splat(                                       \
        args[0].op, compiler::turboshaft::Simd128SplatOp::Kind::k##kind); \
    break;
      FOREACH_SIMD_128_SPLAT_OPCODE(HANDLE_SPLAT_OPCODE)
#undef HANDLE_SPLAT_OPCODE

// Ternary mask operators put the mask as first input.
#define HANDLE_TERNARY_MASK_OPCODE(kind)                        \
  case kExpr##kind:                                             \
    result->op = asm_.Simd128Ternary(                           \
        args[2].op, args[0].op, args[1].op,                     \
        compiler::turboshaft::Simd128TernaryOp::Kind::k##kind); \
    break;
      FOREACH_SIMD_128_TERNARY_MASK_OPCODE(HANDLE_TERNARY_MASK_OPCODE)
#undef HANDLE_TERNARY_MASK_OPCODE

#define HANDLE_TERNARY_OTHER_OPCODE(kind)                       \
  case kExpr##kind:                                             \
    result->op = asm_.Simd128Ternary(                           \
        args[0].op, args[1].op, args[2].op,                     \
        compiler::turboshaft::Simd128TernaryOp::Kind::k##kind); \
    break;
      FOREACH_SIMD_128_TERNARY_OTHER_OPCODE(HANDLE_TERNARY_OTHER_OPCODE)
#undef HANDLE_TERNARY_OTHER_OPCODE

      default:
        UNREACHABLE();
    }
  }

  void SimdLaneOp(FullDecoder* decoder, WasmOpcode opcode,
                  const SimdLaneImmediate& imm,
                  base::Vector<const Value> inputs, Value* result) {
    using compiler::turboshaft::Simd128ExtractLaneOp;
    using compiler::turboshaft::Simd128ReplaceLaneOp;
    switch (opcode) {
      case kExprI8x16ExtractLaneS:
        result->op = asm_.Simd128ExtractLane(
            inputs[0].op, Simd128ExtractLaneOp::Kind::kI8x16S, imm.lane);
        break;
      case kExprI8x16ExtractLaneU:
        result->op = asm_.Simd128ExtractLane(
            inputs[0].op, Simd128ExtractLaneOp::Kind::kI8x16U, imm.lane);
        break;
      case kExprI16x8ExtractLaneS:
        result->op = asm_.Simd128ExtractLane(
            inputs[0].op, Simd128ExtractLaneOp::Kind::kI16x8S, imm.lane);
        break;
      case kExprI16x8ExtractLaneU:
        result->op = asm_.Simd128ExtractLane(
            inputs[0].op, Simd128ExtractLaneOp::Kind::kI16x8U, imm.lane);
        break;
      case kExprI32x4ExtractLane:
        result->op = asm_.Simd128ExtractLane(
            inputs[0].op, Simd128ExtractLaneOp::Kind::kI32x4, imm.lane);
        break;
      case kExprI64x2ExtractLane:
        result->op = asm_.Simd128ExtractLane(
            inputs[0].op, Simd128ExtractLaneOp::Kind::kI64x2, imm.lane);
        break;
      case kExprF32x4ExtractLane:
        result->op = asm_.Simd128ExtractLane(
            inputs[0].op, Simd128ExtractLaneOp::Kind::kF32x4, imm.lane);
        break;
      case kExprF64x2ExtractLane:
        result->op = asm_.Simd128ExtractLane(
            inputs[0].op, Simd128ExtractLaneOp::Kind::kF64x2, imm.lane);
        break;
      case kExprI8x16ReplaceLane:
        result->op = asm_.Simd128ReplaceLane(inputs[0].op, inputs[1].op,
                                             Simd128ReplaceLaneOp::Kind::kI8x16,
                                             imm.lane);
        break;
      case kExprI16x8ReplaceLane:
        result->op = asm_.Simd128ReplaceLane(inputs[0].op, inputs[1].op,
                                             Simd128ReplaceLaneOp::Kind::kI16x8,
                                             imm.lane);
        break;
      case kExprI32x4ReplaceLane:
        result->op = asm_.Simd128ReplaceLane(inputs[0].op, inputs[1].op,
                                             Simd128ReplaceLaneOp::Kind::kI32x4,
                                             imm.lane);
        break;
      case kExprI64x2ReplaceLane:
        result->op = asm_.Simd128ReplaceLane(inputs[0].op, inputs[1].op,
                                             Simd128ReplaceLaneOp::Kind::kI64x2,
                                             imm.lane);
        break;
      case kExprF32x4ReplaceLane:
        result->op = asm_.Simd128ReplaceLane(inputs[0].op, inputs[1].op,
                                             Simd128ReplaceLaneOp::Kind::kF32x4,
                                             imm.lane);
        break;
      case kExprF64x2ReplaceLane:
        result->op = asm_.Simd128ReplaceLane(inputs[0].op, inputs[1].op,
                                             Simd128ReplaceLaneOp::Kind::kF64x2,
                                             imm.lane);
        break;
      default:
        UNREACHABLE();
    }
  }

  void Simd8x16ShuffleOp(FullDecoder* decoder, const Simd128Immediate& imm,
                         const Value& input0, const Value& input1,
                         Value* result) {
    result->op = asm_.Simd128Shuffle(input0.op, input1.op, imm.value);
  }

  void Try(FullDecoder* decoder, Control* block) {
    block->false_or_loop_or_catch_block = NewBlock(decoder, nullptr);
    block->merge_block = NewBlock(decoder, block->br_merge());
  }

  void Throw(FullDecoder* decoder, const TagIndexImmediate& imm,
             const Value arg_values[]) {
    size_t count = imm.tag->sig->parameter_count();
    base::SmallVector<OpIndex, 8> values(count);
    for (size_t index = 0; index < count; index++) {
      values[index] = arg_values[index].op;
    }

    uint32_t encoded_size = WasmExceptionPackage::GetEncodedSize(imm.tag);

    V<FixedArray> values_array =
        CallBuiltinFromRuntimeStub(decoder, WasmCode::kWasmAllocateFixedArray,
                                   {asm_.IntPtrConstant(encoded_size)});

    uint32_t index = 0;
    const wasm::WasmTagSig* sig = imm.tag->sig;

    // Encode the exception values in {values_array}.
    for (size_t i = 0; i < count; i++) {
      OpIndex value = values[i];
      switch (sig->GetParam(i).kind()) {
        case kF32:
          value = asm_.BitcastFloat32ToWord32(value);
          V8_FALLTHROUGH;
        case kI32:
          BuildEncodeException32BitValue(values_array, index, value);
          // We need 2 Smis to encode a 32-bit value.
          index += 2;
          break;
        case kF64:
          value = asm_.BitcastFloat64ToWord64(value);
          V8_FALLTHROUGH;
        case kI64: {
          OpIndex upper_half = asm_.TruncateWord64ToWord32(
              asm_.Word64ShiftRightLogical(value, 32));
          BuildEncodeException32BitValue(values_array, index, upper_half);
          index += 2;
          OpIndex lower_half = asm_.TruncateWord64ToWord32(value);
          BuildEncodeException32BitValue(values_array, index, lower_half);
          index += 2;
          break;
        }
        case wasm::kRef:
        case wasm::kRefNull:
        case wasm::kRtt:
          StoreFixedArrayElement(values_array, index, value,
                                 compiler::kFullWriteBarrier);
          index++;
          break;
        case kS128: {
          using Kind = compiler::turboshaft::Simd128ExtractLaneOp::Kind;
          BuildEncodeException32BitValue(
              values_array, index,
              asm_.Simd128ExtractLane(value, Kind::kI32x4, 0));
          index += 2;
          BuildEncodeException32BitValue(
              values_array, index,
              asm_.Simd128ExtractLane(value, Kind::kI32x4, 1));
          index += 2;
          BuildEncodeException32BitValue(
              values_array, index,
              asm_.Simd128ExtractLane(value, Kind::kI32x4, 2));
          index += 2;
          BuildEncodeException32BitValue(
              values_array, index,
              asm_.Simd128ExtractLane(value, Kind::kI32x4, 3));
          index += 2;
          break;
        }
        case kI8:
        case kI16:
        case kVoid:
        case kBottom:
          UNREACHABLE();
      }
    }

    V<FixedArray> instance_tags =
        LOAD_INSTANCE_FIELD(TagsTable, MemoryRepresentation::TaggedPointer());
    auto tag =
        V<WasmTagObject>::Cast(LoadFixedArrayElement(instance_tags, imm.index));

    CallBuiltinFromRuntimeStub(decoder, WasmCode::kWasmThrow,
                               {tag, values_array}, CheckForException::kYes);
    asm_.Unreachable();
  }

  void Rethrow(FullDecoder* decoder, Control* block) {
    CallBuiltinFromRuntimeStub(decoder, WasmCode::kWasmRethrow,
                               {block->exception}, CheckForException::kYes);
    asm_.Unreachable();
  }

  // TODO(14108): Optimize in case of unreachable catch block?
  void CatchException(FullDecoder* decoder, const TagIndexImmediate& imm,
                      Control* block, base::Vector<Value> values) {
    EnterBlock(decoder, block->false_or_loop_or_catch_block, nullptr,
               &block->exception);
    V<NativeContext> native_context = LOAD_INSTANCE_FIELD(
        NativeContext, MemoryRepresentation::TaggedPointer());
    V<WasmTagObject> caught_tag = CallBuiltinFromRuntimeStub(
        decoder, WasmCode::kWasmGetOwnProperty,
        {block->exception, LOAD_ROOT(wasm_exception_tag_symbol),
         native_context});
    V<FixedArray> instance_tags =
        LOAD_INSTANCE_FIELD(TagsTable, MemoryRepresentation::TaggedPointer());
    auto expected_tag =
        V<WasmTagObject>::Cast(LoadFixedArrayElement(instance_tags, imm.index));
    TSBlock* if_catch = asm_.NewBlock();
    TSBlock* if_no_catch = NewBlock(decoder, nullptr);
    SetupControlFlowEdge(decoder, if_no_catch);

    // If the tags don't match we continue with the next tag by setting the
    // no-catch environment as the new {block->catch_block} here.
    block->false_or_loop_or_catch_block = if_no_catch;

    if (imm.tag->sig->parameter_count() == 1 &&
        imm.tag->sig->GetParam(0).is_reference_to(HeapType::kExtern)) {
      // Check for the special case where the tag is WebAssembly.JSTag and the
      // exception is not a WebAssembly.Exception. In this case the exception is
      // caught and pushed on the operand stack.
      // Only perform this check if the tag signature is the same as
      // the JSTag signature, i.e. a single externref or (ref extern), otherwise
      // we know statically that it cannot be the JSTag.
      V<Word32> caught_tag_undefined =
          asm_.TaggedEqual(caught_tag, LOAD_ROOT(UndefinedValue));
      TSBlock* js_exception = asm_.NewBlock();
      TSBlock* wasm_exception = asm_.NewBlock();
      TSBlock* no_catch_merge = asm_.NewBlock();
      asm_.Branch(ConditionWithHint(caught_tag_undefined, BranchHint::kFalse),
                  js_exception, wasm_exception);

      asm_.Bind(js_exception);
      V<Tagged> tag_object = asm_.Load(
          native_context, LoadOp::Kind::TaggedBase(),
          MemoryRepresentation::TaggedPointer(),
          NativeContext::OffsetOfElementAt(Context::WASM_JS_TAG_INDEX));
      V<Tagged> js_tag = asm_.Load(tag_object, LoadOp::Kind::TaggedBase(),
                                   MemoryRepresentation::TaggedPointer(),
                                   WasmTagObject::kTagOffset);
      asm_.Branch(ConditionWithHint(asm_.TaggedEqual(expected_tag, js_tag)),
                  if_catch, no_catch_merge);

      asm_.Bind(wasm_exception);
      TSBlock* unpack_block = asm_.NewBlock();
      asm_.Branch(ConditionWithHint(asm_.TaggedEqual(caught_tag, expected_tag)),
                  unpack_block, no_catch_merge);
      asm_.Bind(unpack_block);
      UnpackWasmException(decoder, block->exception, values);
      asm_.Goto(if_catch);

      asm_.Bind(no_catch_merge);
      asm_.Goto(if_no_catch);

      asm_.Bind(if_catch);
      V<Tagged> args[2] = {block->exception, V<Tagged>::Cast(values[0].op)};
      values[0].op = asm_.Phi(base::VectorOf(args, 2));
    } else {
      asm_.Branch(ConditionWithHint(asm_.TaggedEqual(caught_tag, expected_tag)),
                  if_catch, if_no_catch);
      asm_.Bind(if_catch);
      UnpackWasmException(decoder, block->exception, values);
    }
  }

  // TODO(14108): Optimize in case of unreachable catch block?
  void Delegate(FullDecoder* decoder, uint32_t depth, Control* block) {
    EnterBlock(decoder, block->false_or_loop_or_catch_block, nullptr,
               &block->exception);
    if (depth == decoder->control_depth() - 1) {
      // We just throw to the caller, no need to handle the exception in this
      // frame.
      CallBuiltinFromRuntimeStub(decoder, WasmCode::kWasmRethrow,
                                 {block->exception});
      asm_.Unreachable();
    } else {
      DCHECK(decoder->control_at(depth)->is_try());
      TSBlock* target_catch =
          decoder->control_at(depth)->false_or_loop_or_catch_block;
      SetupControlFlowEdge(decoder, target_catch, 0, block->exception);
      asm_.Goto(target_catch);
    }
  }

  void CatchAll(FullDecoder* decoder, Control* block) {
    DCHECK(block->is_try_catchall() || block->is_try_catch());
    DCHECK_EQ(decoder->control_at(0), block);
    EnterBlock(decoder, block->false_or_loop_or_catch_block, nullptr,
               &block->exception);
  }

  V<BigInt> BuildChangeInt64ToBigInt(V<Word64> input) {
    V<WordPtr> builtin =
        Is64() ? asm_.RelocatableConstant(WasmCode::kI64ToBigInt,
                                          RelocInfo::WASM_STUB_CALL)
               : asm_.RelocatableConstant(WasmCode::kI32PairToBigInt,
                                          RelocInfo::WASM_STUB_CALL);
    CallInterfaceDescriptor interface_descriptor =
        Is64()
            ? Builtins::CallInterfaceDescriptorFor(Builtin::kI64ToBigInt)
            : Builtins::CallInterfaceDescriptorFor(Builtin::kI32PairToBigInt);
    const CallDescriptor* call_descriptor =
        compiler::Linkage::GetStubCallDescriptor(
            asm_.graph_zone(),  // zone
            interface_descriptor,
            0,                                  // stack parameter count
            CallDescriptor::kNoFlags,           // flags
            compiler::Operator::kNoProperties,  // properties
            StubCallMode::kCallWasmRuntimeStub);
    const TSCallDescriptor* ts_call_descriptor = TSCallDescriptor::Create(
        call_descriptor, compiler::CanThrow::kNo, asm_.graph_zone());
    if (Is64()) {
      return asm_.Call(builtin, {input}, ts_call_descriptor);
    }
    V<Word32> low_word = asm_.TruncateWord64ToWord32(input);
    V<Word32> high_word = asm_.TruncateWord64ToWord32(asm_.ShiftRightLogical(
        input, asm_.Word32Constant(32), WordRepresentation::Word64()));
    return asm_.Call(builtin, {low_word, high_word}, ts_call_descriptor);
  }

  void AtomicNotify(FullDecoder* decoder, const MemoryAccessImmediate& imm,
                    OpIndex index, OpIndex value, Value* result) {
    V<WordPtr> converted_index;
    compiler::BoundsCheckResult bounds_check_result;
    std::tie(converted_index, bounds_check_result) = CheckBoundsAndAlignment(
        imm.memory, MemoryRepresentation::Int32(), index, imm.offset,
        decoder->position(), compiler::EnforceBoundsCheck::kNeedsBoundsCheck);

    OpIndex effective_offset = asm_.WordPtrAdd(converted_index, imm.offset);

    result->op = CallBuiltinFromRuntimeStub(
        decoder, WasmCode::kWasmAtomicNotify,
        {asm_.Word32Constant(imm.memory->index), effective_offset, value});
  }

  void AtomicWait(FullDecoder* decoder, WasmOpcode opcode,
                  const MemoryAccessImmediate& imm, OpIndex index,
                  OpIndex expected, V<Word64> timeout, Value* result) {
    V<WordPtr> converted_index;
    compiler::BoundsCheckResult bounds_check_result;
    std::tie(converted_index, bounds_check_result) = CheckBoundsAndAlignment(
        imm.memory,
        opcode == kExprI32AtomicWait ? MemoryRepresentation::Int32()
                                     : MemoryRepresentation::Int64(),
        index, imm.offset, decoder->position(),
        compiler::EnforceBoundsCheck::kNeedsBoundsCheck);

    OpIndex effective_offset = asm_.WordPtrAdd(converted_index, imm.offset);
    V<BigInt> bigint_timeout = BuildChangeInt64ToBigInt(timeout);

    if (opcode == kExprI32AtomicWait) {
      result->op = CallBuiltinFromRuntimeStub(
          decoder, WasmCode::kWasmI32AtomicWait,
          {asm_.Word32Constant(imm.memory->index), effective_offset, expected,
           bigint_timeout});
      return;
    }
    DCHECK_EQ(opcode, kExprI64AtomicWait);
    V<BigInt> bigint_expected = BuildChangeInt64ToBigInt(expected);
    result->op = CallBuiltinFromRuntimeStub(
        decoder, WasmCode::kWasmI64AtomicWait,
        {asm_.Word32Constant(imm.memory->index), effective_offset,
         bigint_expected, bigint_timeout});
  }

  void AtomicOp(FullDecoder* decoder, WasmOpcode opcode, const Value args[],
                const size_t argc, const MemoryAccessImmediate& imm,
                Value* result) {
    if (opcode == WasmOpcode::kExprAtomicNotify) {
      return AtomicNotify(decoder, imm, args[0].op, args[1].op, result);
    }
    if (opcode == WasmOpcode::kExprI32AtomicWait ||
        opcode == WasmOpcode::kExprI64AtomicWait) {
      return AtomicWait(decoder, opcode, imm, args[0].op, args[1].op,
                        args[2].op, result);
    }
    using Binop = compiler::turboshaft::AtomicRMWOp::BinOp;
    enum OpType { kBinop, kLoad, kStore };
    struct AtomicOpInfo {
      OpType op_type;
      // Initialize with a default value, to allow constexpr constructors.
      Binop bin_op = Binop::kAdd;
      RegisterRepresentation result_rep;
      MemoryRepresentation input_rep;

      constexpr AtomicOpInfo(Binop bin_op, RegisterRepresentation result_rep,
                             MemoryRepresentation input_rep)
          : op_type(kBinop),
            bin_op(bin_op),
            result_rep(result_rep),
            input_rep(input_rep) {}

      constexpr AtomicOpInfo(OpType op_type, RegisterRepresentation result_rep,
                             MemoryRepresentation input_rep)
          : op_type(op_type), result_rep(result_rep), input_rep(input_rep) {}

      static constexpr AtomicOpInfo Get(wasm::WasmOpcode opcode) {
        switch (opcode) {
#define CASE_BINOP(OPCODE, BINOP, RESULT, INPUT)                           \
  case WasmOpcode::kExpr##OPCODE:                                          \
    return AtomicOpInfo(Binop::k##BINOP, RegisterRepresentation::RESULT(), \
                        MemoryRepresentation::INPUT());
#define RMW_OPERATION(V)                                          \
  V(I32AtomicAdd, Add, Word32, Uint32)                            \
  V(I32AtomicAdd8U, Add, Word32, Uint8)                           \
  V(I32AtomicAdd16U, Add, Word32, Uint16)                         \
  V(I32AtomicSub, Sub, Word32, Uint32)                            \
  V(I32AtomicSub8U, Sub, Word32, Uint8)                           \
  V(I32AtomicSub16U, Sub, Word32, Uint16)                         \
  V(I32AtomicAnd, And, Word32, Uint32)                            \
  V(I32AtomicAnd8U, And, Word32, Uint8)                           \
  V(I32AtomicAnd16U, And, Word32, Uint16)                         \
  V(I32AtomicOr, Or, Word32, Uint32)                              \
  V(I32AtomicOr8U, Or, Word32, Uint8)                             \
  V(I32AtomicOr16U, Or, Word32, Uint16)                           \
  V(I32AtomicXor, Xor, Word32, Uint32)                            \
  V(I32AtomicXor8U, Xor, Word32, Uint8)                           \
  V(I32AtomicXor16U, Xor, Word32, Uint16)                         \
  V(I32AtomicExchange, Exchange, Word32, Uint32)                  \
  V(I32AtomicExchange8U, Exchange, Word32, Uint8)                 \
  V(I32AtomicExchange16U, Exchange, Word32, Uint16)               \
  V(I32AtomicCompareExchange, CompareExchange, Word32, Uint32)    \
  V(I32AtomicCompareExchange8U, CompareExchange, Word32, Uint8)   \
  V(I32AtomicCompareExchange16U, CompareExchange, Word32, Uint16) \
  V(I64AtomicAdd, Add, Word64, Uint64)                            \
  V(I64AtomicAdd8U, Add, Word64, Uint8)                           \
  V(I64AtomicAdd16U, Add, Word64, Uint16)                         \
  V(I64AtomicAdd32U, Add, Word64, Uint32)                         \
  V(I64AtomicSub, Sub, Word64, Uint64)                            \
  V(I64AtomicSub8U, Sub, Word64, Uint8)                           \
  V(I64AtomicSub16U, Sub, Word64, Uint16)                         \
  V(I64AtomicSub32U, Sub, Word64, Uint32)                         \
  V(I64AtomicAnd, And, Word64, Uint64)                            \
  V(I64AtomicAnd8U, And, Word64, Uint8)                           \
  V(I64AtomicAnd16U, And, Word64, Uint16)                         \
  V(I64AtomicAnd32U, And, Word64, Uint32)                         \
  V(I64AtomicOr, Or, Word64, Uint64)                              \
  V(I64AtomicOr8U, Or, Word64, Uint8)                             \
  V(I64AtomicOr16U, Or, Word64, Uint16)                           \
  V(I64AtomicOr32U, Or, Word64, Uint32)                           \
  V(I64AtomicXor, Xor, Word64, Uint64)                            \
  V(I64AtomicXor8U, Xor, Word64, Uint8)                           \
  V(I64AtomicXor16U, Xor, Word64, Uint16)                         \
  V(I64AtomicXor32U, Xor, Word64, Uint32)                         \
  V(I64AtomicExchange, Exchange, Word64, Uint64)                  \
  V(I64AtomicExchange8U, Exchange, Word64, Uint8)                 \
  V(I64AtomicExchange16U, Exchange, Word64, Uint16)               \
  V(I64AtomicExchange32U, Exchange, Word64, Uint32)               \
  V(I64AtomicCompareExchange, CompareExchange, Word64, Uint64)    \
  V(I64AtomicCompareExchange8U, CompareExchange, Word64, Uint8)   \
  V(I64AtomicCompareExchange16U, CompareExchange, Word64, Uint16) \
  V(I64AtomicCompareExchange32U, CompareExchange, Word64, Uint32)

          RMW_OPERATION(CASE_BINOP)
#undef RMW_OPERATION
#undef CASE
#define CASE_LOAD(OPCODE, RESULT, INPUT)                         \
  case WasmOpcode::kExpr##OPCODE:                                \
    return AtomicOpInfo(kLoad, RegisterRepresentation::RESULT(), \
                        MemoryRepresentation::INPUT());
#define LOAD_OPERATION(V)             \
  V(I32AtomicLoad, Word32, Uint32)    \
  V(I32AtomicLoad16U, Word32, Uint16) \
  V(I32AtomicLoad8U, Word32, Uint8)   \
  V(I64AtomicLoad, Word64, Uint64)    \
  V(I64AtomicLoad32U, Word64, Uint32) \
  V(I64AtomicLoad16U, Word64, Uint16) \
  V(I64AtomicLoad8U, Word64, Uint8)
          LOAD_OPERATION(CASE_LOAD)
#undef LOAD_OPERATION
#undef CASE_LOAD
#define CASE_STORE(OPCODE, INPUT, OUTPUT)                        \
  case WasmOpcode::kExpr##OPCODE:                                \
    return AtomicOpInfo(kStore, RegisterRepresentation::INPUT(), \
                        MemoryRepresentation::OUTPUT());
#define STORE_OPERATION(V)             \
  V(I32AtomicStore, Word32, Uint32)    \
  V(I32AtomicStore16U, Word32, Uint16) \
  V(I32AtomicStore8U, Word32, Uint8)   \
  V(I64AtomicStore, Word64, Uint64)    \
  V(I64AtomicStore32U, Word64, Uint32) \
  V(I64AtomicStore16U, Word64, Uint16) \
  V(I64AtomicStore8U, Word64, Uint8)
          STORE_OPERATION(CASE_STORE)
#undef STORE_OPERATION_OPERATION
#undef CASE_STORE
          default:
            UNREACHABLE();
        }
      }
    };

    AtomicOpInfo info = AtomicOpInfo::Get(opcode);

    V<WordPtr> index;
    compiler::BoundsCheckResult bounds_check_result;
    std::tie(index, bounds_check_result) = CheckBoundsAndAlignment(
        imm.memory, info.input_rep, args[0].op, imm.offset, decoder->position(),
        compiler::EnforceBoundsCheck::kCanOmitBoundsCheck);
    // MemoryAccessKind::kUnaligned is impossible due to explicit aligment
    // check.
    MemoryAccessKind access_kind =
        bounds_check_result == compiler::BoundsCheckResult::kTrapHandler
            ? MemoryAccessKind::kProtected
            : MemoryAccessKind::kNormal;

    if (info.op_type == kBinop) {
      if (info.bin_op == Binop::kCompareExchange) {
        result->op = asm_.AtomicCompareExchange(
            MemBuffer(imm.memory->index, imm.offset), index, args[1].op,
            args[2].op, info.result_rep, info.input_rep, access_kind);
        return;
      }
      result->op = asm_.AtomicRMW(MemBuffer(imm.memory->index, imm.offset),
                                  index, args[1].op, info.bin_op,
                                  info.result_rep, info.input_rep, access_kind);
      return;
    }
    if (info.op_type == kStore) {
      OpIndex value = args[1].op;
      if (info.result_rep == RegisterRepresentation::Word64() &&
          info.input_rep != MemoryRepresentation::Uint64()) {
        value = asm_.TruncateWord64ToWord32(value);
      }
      asm_.Store(MemBuffer(imm.memory->index, imm.offset), index, value,
                 access_kind == MemoryAccessKind::kProtected
                     ? LoadOp::Kind::Protected().Atomic()
                     : LoadOp::Kind::RawAligned().Atomic(),
                 info.input_rep, compiler::kNoWriteBarrier);
      return;
    }
    DCHECK_EQ(info.op_type, kLoad);
    result->op = asm_.Load(MemBuffer(imm.memory->index, imm.offset), index,
                           access_kind == MemoryAccessKind::kProtected
                               ? LoadOp::Kind::Protected().Atomic()
                               : LoadOp::Kind::RawAligned().Atomic(),
                           info.input_rep, info.result_rep);
  }

  void AtomicFence(FullDecoder* decoder) { Bailout(decoder); }

  void MemoryInit(FullDecoder* decoder, const MemoryInitImmediate& imm,
                  const Value& dst, const Value& src, const Value& size) {
    V<WordPtr> dst_uintptr =
        MemoryIndexToUintPtrOrOOBTrap(imm.memory.memory->is_memory64, dst.op);
    DCHECK_EQ(size.type, kWasmI32);
    V<Word32> result = CallCStackSlotToInt32(
        ExternalReference::wasm_memory_init(),
        {{asm_.BitcastTaggedToWord(instance_node_),
          MemoryRepresentation::PointerSized()},
         {asm_.Word32Constant(imm.memory.index), MemoryRepresentation::Int32()},
         {dst_uintptr, MemoryRepresentation::PointerSized()},
         {src.op, MemoryRepresentation::Int32()},
         {asm_.Word32Constant(imm.data_segment.index),
          MemoryRepresentation::Int32()},
         {size.op, MemoryRepresentation::Int32()}});
    asm_.TrapIfNot(result, OpIndex::Invalid(), TrapId::kTrapMemOutOfBounds);
  }

  void MemoryCopy(FullDecoder* decoder, const MemoryCopyImmediate& imm,
                  const Value& dst, const Value& src, const Value& size) {
    bool is_memory_64 = imm.memory_src.memory->is_memory64;
    DCHECK_EQ(is_memory_64, imm.memory_dst.memory->is_memory64);
    V<WordPtr> dst_uintptr =
        MemoryIndexToUintPtrOrOOBTrap(is_memory_64, dst.op);
    V<WordPtr> src_uintptr =
        MemoryIndexToUintPtrOrOOBTrap(is_memory_64, src.op);
    V<WordPtr> size_uintptr =
        MemoryIndexToUintPtrOrOOBTrap(is_memory_64, size.op);
    V<Word32> result = CallCStackSlotToInt32(
        ExternalReference::wasm_memory_copy(),
        {{asm_.BitcastTaggedToWord(instance_node_),
          MemoryRepresentation::PointerSized()},
         {asm_.Word32Constant(imm.memory_dst.index),
          MemoryRepresentation::Int32()},
         {asm_.Word32Constant(imm.memory_src.index),
          MemoryRepresentation::Int32()},
         {dst_uintptr, MemoryRepresentation::PointerSized()},
         {src_uintptr, MemoryRepresentation::PointerSized()},
         {size_uintptr, MemoryRepresentation::PointerSized()}});
    asm_.TrapIfNot(result, OpIndex::Invalid(), TrapId::kTrapMemOutOfBounds);
  }

  void MemoryFill(FullDecoder* decoder, const MemoryIndexImmediate& imm,
                  const Value& dst, const Value& value, const Value& size) {
    bool is_memory_64 = imm.memory->is_memory64;
    V<WordPtr> dst_uintptr =
        MemoryIndexToUintPtrOrOOBTrap(is_memory_64, dst.op);
    V<WordPtr> size_uintptr =
        MemoryIndexToUintPtrOrOOBTrap(is_memory_64, size.op);
    V<Word32> result = CallCStackSlotToInt32(
        ExternalReference::wasm_memory_fill(),
        {{asm_.BitcastTaggedToWord(instance_node_),
          MemoryRepresentation::PointerSized()},
         {asm_.Word32Constant(imm.index), MemoryRepresentation::Int32()},
         {dst_uintptr, MemoryRepresentation::PointerSized()},
         {value.op, MemoryRepresentation::Int32()},
         {size_uintptr, MemoryRepresentation::PointerSized()}});

    asm_.TrapIfNot(result, OpIndex::Invalid(), TrapId::kTrapMemOutOfBounds);
  }

  void DataDrop(FullDecoder* decoder, const IndexImmediate& imm) {
    V<FixedUInt32Array> data_segment_sizes = LOAD_INSTANCE_FIELD(
        DataSegmentSizes, MemoryRepresentation::TaggedPointer());
    asm_.Store(data_segment_sizes, asm_.Word32Constant(0),
               StoreOp::Kind::TaggedBase(), MemoryRepresentation::Int32(),
               compiler::kNoWriteBarrier,
               FixedUInt32Array::kHeaderSize + imm.index * kUInt32Size);
  }

  void TableGet(FullDecoder* decoder, const Value& index, Value* result,
                const IndexImmediate& imm) {
    ValueType table_type = decoder->module_->tables[imm.index].type;
    auto stub = IsSubtypeOf(table_type, kWasmFuncRef, decoder->module_)
                    ? WasmCode::kWasmTableGetFuncRef
                    : WasmCode::kWasmTableGet;
    result->op = CallBuiltinFromRuntimeStub(
        decoder, stub, {asm_.IntPtrConstant(imm.index), index.op});
  }

  void TableSet(FullDecoder* decoder, const Value& index, const Value& value,
                const IndexImmediate& imm) {
    ValueType table_type = decoder->module_->tables[imm.index].type;
    auto stub = IsSubtypeOf(table_type, kWasmFuncRef, decoder->module_)
                    ? WasmCode::kWasmTableSetFuncRef
                    : WasmCode::kWasmTableSet;
    CallBuiltinFromRuntimeStub(
        decoder, stub, {asm_.IntPtrConstant(imm.index), index.op, value.op});
  }

  void TableInit(FullDecoder* decoder, const TableInitImmediate& imm,
                 const Value* args) {
    V<Word32> dst = args[0].op;
    V<Word32> src = args[1].op;
    V<Word32> size = args[2].op;
    CallBuiltinFromRuntimeStub(
        decoder, WasmCode::kWasmTableInit,
        {dst, src, size, asm_.NumberConstant(imm.table.index),
         asm_.NumberConstant(imm.element_segment.index)});
  }

  void TableCopy(FullDecoder* decoder, const TableCopyImmediate& imm,
                 const Value args[]) {
    V<Word32> dst = args[0].op;
    V<Word32> src = args[1].op;
    V<Word32> size = args[2].op;
    CallBuiltinFromRuntimeStub(
        decoder, WasmCode::kWasmTableCopy,
        {dst, src, size, asm_.NumberConstant(imm.table_dst.index),
         asm_.NumberConstant(imm.table_src.index)});
  }

  void TableGrow(FullDecoder* decoder, const IndexImmediate& imm,
                 const Value& value, const Value& delta, Value* result) {
    V<Smi> result_smi = CallBuiltinFromRuntimeStub(
        decoder, WasmCode::kWasmTableGrow,
        {asm_.NumberConstant(imm.index), delta.op, value.op});
    result->op = asm_.UntagSmi(result_smi);
  }

  void TableFill(FullDecoder* decoder, const IndexImmediate& imm,
                 const Value& start, const Value& value, const Value& count) {
    CallBuiltinFromRuntimeStub(
        decoder, WasmCode::kWasmTableFill,
        {asm_.NumberConstant(imm.index), start.op, count.op, value.op});
  }

  void TableSize(FullDecoder* decoder, const IndexImmediate& imm,
                 Value* result) {
    V<FixedArray> tables =
        LOAD_INSTANCE_FIELD(Tables, MemoryRepresentation::TaggedPointer());
    auto table =
        V<WasmTableObject>::Cast(LoadFixedArrayElement(tables, imm.index));
    V<Smi> size_smi = asm_.Load(table, LoadOp::Kind::TaggedBase(),
                                MemoryRepresentation::TaggedSigned(),
                                WasmTableObject::kCurrentLengthOffset);
    result->op = asm_.UntagSmi(size_smi);
  }

  void ElemDrop(FullDecoder* decoder, const IndexImmediate& imm) {
    V<FixedArray> elem_segments = LOAD_INSTANCE_FIELD(
        ElementSegments, MemoryRepresentation::TaggedPointer());
    StoreFixedArrayElement(elem_segments, imm.index, LOAD_ROOT(EmptyFixedArray),
                           compiler::kFullWriteBarrier);
  }

  void StructNew(FullDecoder* decoder, const StructIndexImmediate& imm,
                 const Value args[], Value* result) {
    Bailout(decoder);
  }
  void StructNewDefault(FullDecoder* decoder, const StructIndexImmediate& imm,
                        Value* result) {
    Bailout(decoder);
  }

  void StructGet(FullDecoder* decoder, const Value& struct_object,
                 const FieldImmediate& field, bool is_signed, Value* result) {
    result->op = asm_.StructGet(struct_object.op, field.struct_imm.struct_type,
                                field.field_imm.index, is_signed,
                                struct_object.type.is_nullable()
                                    ? compiler::kWithNullCheck
                                    : compiler::kWithoutNullCheck);
  }

  void StructSet(FullDecoder* decoder, const Value& struct_object,
                 const FieldImmediate& field, const Value& field_value) {
    asm_.StructSet(struct_object.op, field_value.op,
                   field.struct_imm.struct_type, field.field_imm.index,
                   struct_object.type.is_nullable()
                       ? compiler::kWithNullCheck
                       : compiler::kWithoutNullCheck);
  }

  void ArrayNew(FullDecoder* decoder, const ArrayIndexImmediate& imm,
                const Value& length, const Value& initial_value,
                Value* result) {
    Bailout(decoder);
  }

  void ArrayNewDefault(FullDecoder* decoder, const ArrayIndexImmediate& imm,
                       const Value& length, Value* result) {
    Bailout(decoder);
  }

  void ArrayGet(FullDecoder* decoder, const Value& array_obj,
                const ArrayIndexImmediate& imm, const Value& index,
                bool is_signed, Value* result) {
    BoundsCheckArray(array_obj.op, index.op, array_obj.type);
    result->op = asm_.ArrayGet(array_obj.op, index.op,
                               imm.array_type->element_type(), is_signed);
  }

  void ArraySet(FullDecoder* decoder, const Value& array_obj,
                const ArrayIndexImmediate& imm, const Value& index,
                const Value& value) {
    BoundsCheckArray(array_obj.op, index.op, array_obj.type);
    asm_.ArraySet(array_obj.op, index.op, value.op,
                  imm.array_type->element_type());
  }

  void ArrayLen(FullDecoder* decoder, const Value& array_obj, Value* result) {
    result->op =
        asm_.ArrayLength(array_obj.op, array_obj.type.is_nullable()
                                           ? compiler::kWithNullCheck
                                           : compiler::kWithoutNullCheck);
  }

  void ArrayCopy(FullDecoder* decoder, const Value& dst, const Value& dst_index,
                 const Value& src, const Value& src_index,
                 const ArrayIndexImmediate& src_imm, const Value& length) {
    BoundsCheckArrayWithLength(dst.op, dst_index.op, length.op,
                               dst.type.is_nullable()
                                   ? compiler::kWithNullCheck
                                   : compiler::kWithoutNullCheck);
    BoundsCheckArrayWithLength(src.op, src_index.op, length.op,
                               src.type.is_nullable()
                                   ? compiler::kWithNullCheck
                                   : compiler::kWithoutNullCheck);

    ValueType element_type = src_imm.array_type->element_type();

    Label<> end(&asm_);
    Label<> builtin(&asm_);
    Label<> reverse(&asm_);

    GOTO_IF(asm_.Word32Equal(length.op, 0), end);

    // Values determined by test/mjsunit/wasm/array-copy-benchmark.js on x64.
    int array_copy_max_loop_length;
    switch (element_type.kind()) {
      case wasm::kI32:
      case wasm::kI64:
      case wasm::kI8:
      case wasm::kI16:
        array_copy_max_loop_length = 20;
        break;
      case wasm::kF32:
      case wasm::kF64:
        array_copy_max_loop_length = 35;
        break;
      case wasm::kS128:
        array_copy_max_loop_length = 100;
        break;
      case wasm::kRtt:
      case wasm::kRef:
      case wasm::kRefNull:
        array_copy_max_loop_length = 15;
        break;
      case wasm::kVoid:
      case wasm::kBottom:
        UNREACHABLE();
    }

    GOTO_IF(asm_.Uint32LessThan(array_copy_max_loop_length, length.op),
            builtin);
    V<Word32> src_end_index =
        asm_.Word32Sub(asm_.Word32Add(src_index.op, length.op), 1);
    GOTO_IF(asm_.Uint32LessThan(src_index.op, dst_index.op), reverse);

    {
      LoopLabel<Word32, Word32> loop_label(&asm_);
      GOTO(loop_label, src_index.op, dst_index.op);
      LOOP(loop_label, src_index_loop, dst_index_loop) {
        OpIndex value =
            asm_.ArrayGet(src.op, src_index_loop, element_type, true);
        asm_.ArraySet(dst.op, dst_index_loop, value, element_type);

        V<Word32> condition =
            asm_.Uint32LessThan(src_index_loop, src_end_index);
        IF (condition) {
          GOTO(loop_label, asm_.Word32Add(src_index_loop, 1),
               asm_.Word32Add(dst_index_loop, 1));
        }
        END_IF

        GOTO(end);
      }
    }

    {
      BIND(reverse);
      V<Word32> dst_end_index =
          asm_.Word32Sub(asm_.Word32Add(dst_index.op, length.op), 1);
      LoopLabel<Word32, Word32> loop_label(&asm_);
      GOTO(loop_label, src_end_index, dst_end_index);
      LOOP(loop_label, src_index_loop, dst_index_loop) {
        OpIndex value =
            asm_.ArrayGet(src.op, src_index_loop, element_type, true);
        asm_.ArraySet(dst.op, dst_index_loop, value, element_type);

        V<Word32> condition = asm_.Uint32LessThan(src_index.op, src_index_loop);
        IF (condition) {
          GOTO(loop_label, asm_.Word32Sub(src_index_loop, 1),
               asm_.Word32Sub(dst_index_loop, 1));
        }
        END_IF

        GOTO(end);
      }
    }

    {
      BIND(builtin);
      MachineType arg_types[]{
          MachineType::TaggedPointer(), MachineType::TaggedPointer(),
          MachineType::Uint32(),        MachineType::TaggedPointer(),
          MachineType::Uint32(),        MachineType::Uint32()};
      MachineSignature sig(0, 6, arg_types);

      CallC(&sig, ExternalReference::wasm_array_copy(),
            {instance_node_, dst.op, dst_index.op, src.op, src_index.op,
             length.op});
      GOTO(end);
    }

    BIND(end);
  }

  void ArrayFill(FullDecoder* decoder, ArrayIndexImmediate& imm,
                 const Value& array, const Value& index, const Value& value,
                 const Value& length) {
    Bailout(decoder);
  }

  void ArrayNewFixed(FullDecoder* decoder, const ArrayIndexImmediate& array_imm,
                     const IndexImmediate& length_imm, const Value elements[],
                     Value* result) {
    Bailout(decoder);
  }

  void ArrayNewSegment(FullDecoder* decoder,
                       const ArrayIndexImmediate& array_imm,
                       const IndexImmediate& segment_imm, const Value& offset,
                       const Value& length, Value* result) {
    bool is_element = array_imm.array_type->element_type().is_reference();
    result->op = CallBuiltinFromRuntimeStub(
        decoder, WasmCode::kWasmArrayNewSegment,
        {asm_.Word32Constant(segment_imm.index), offset.op, length.op,
         asm_.SmiConstant(Smi::FromInt(is_element ? 1 : 0)),
         asm_.RttCanon(instance_node_, array_imm.index)});
  }

  void ArrayInitSegment(FullDecoder* decoder,
                        const ArrayIndexImmediate& array_imm,
                        const IndexImmediate& segment_imm, const Value& array,
                        const Value& array_index, const Value& segment_offset,
                        const Value& length) {
    bool is_element = array_imm.array_type->element_type().is_reference();
    CallBuiltinFromRuntimeStub(
        decoder, WasmCode::kWasmArrayInitSegment,
        {array_index.op, segment_offset.op, length.op,
         asm_.SmiConstant(Smi::FromInt(segment_imm.index)),
         asm_.SmiConstant(Smi::FromInt(is_element ? 1 : 0)), array.op});
  }

  void I31New(FullDecoder* decoder, const Value& input, Value* result) {
    if constexpr (SmiValuesAre31Bits()) {
      V<Word32> shifted =
          asm_.Word32ShiftLeft(input.op, kSmiTagSize + kSmiShiftSize);
      if (Is64()) {
        // The uppermost bits don't matter.
        result->op = asm_.BitcastWord32ToWord64(shifted);
      } else {
        result->op = shifted;
      }
    } else {
      // Set the topmost bit to sign-extend the second bit. This way,
      // interpretation in JS (if this value escapes there) will be the same as
      // i31.get_s.
      V<WordPtr> input_wordptr = asm_.ChangeUint32ToUintPtr(input.op);
      result->op = asm_.WordPtrShiftRightArithmetic(
          asm_.WordPtrShiftLeft(input_wordptr, kSmiShiftSize + kSmiTagSize + 1),
          1);
    }
  }

  void I31GetS(FullDecoder* decoder, const Value& input, Value* result) {
    V<Tagged> input_non_null =
        input.type.is_nullable()
            ? asm_.AssertNotNull(input.op, kWasmI31Ref,
                                 TrapId::kTrapNullDereference)
            : input.op;
    if constexpr (SmiValuesAre31Bits()) {
      result->op = asm_.Word32ShiftRightArithmeticShiftOutZeros(
          asm_.TruncateWordPtrToWord32(
              asm_.BitcastTaggedToWord(input_non_null)),
          kSmiTagSize + kSmiShiftSize);
    } else {
      // Topmost bit is already sign-extended.
      result->op = asm_.TruncateWordPtrToWord32(
          asm_.WordPtrShiftRightArithmeticShiftOutZeros(
              asm_.BitcastTaggedToWord(input_non_null),
              kSmiTagSize + kSmiShiftSize));
    }
  }

  void I31GetU(FullDecoder* decoder, const Value& input, Value* result) {
    V<Tagged> input_non_null =
        input.type.is_nullable()
            ? asm_.AssertNotNull(input.op, kWasmI31Ref,
                                 TrapId::kTrapNullDereference)
            : input.op;
    if constexpr (SmiValuesAre31Bits()) {
      result->op = asm_.Word32ShiftRightLogical(
          asm_.TruncateWordPtrToWord32(
              asm_.BitcastTaggedToWord(input_non_null)),
          kSmiTagSize + kSmiShiftSize);
    } else {
      // Topmost bit is sign-extended, remove it.
      result->op = asm_.TruncateWordPtrToWord32(asm_.WordPtrShiftRightLogical(
          asm_.WordPtrShiftLeft(asm_.BitcastTaggedToWord(input_non_null), 1),
          kSmiTagSize + kSmiShiftSize + 1));
    }
  }

  void RefTest(FullDecoder* decoder, uint32_t ref_index, const Value& object,
               Value* result, bool null_succeeds) {
    V<Map> rtt = asm_.RttCanon(instance_node_, ref_index);
    compiler::WasmTypeCheckConfig config{
        object.type, ValueType::RefMaybeNull(
                         ref_index, null_succeeds ? kNullable : kNonNullable)};
    result->op = asm_.WasmTypeCheck(object.op, rtt, config);
  }

  void RefTestAbstract(FullDecoder* decoder, const Value& object, HeapType type,
                       Value* result, bool null_succeeds) {
    compiler::WasmTypeCheckConfig config{
        object.type, ValueType::RefMaybeNull(
                         type, null_succeeds ? kNullable : kNonNullable)};
    V<Map> rtt = OpIndex::Invalid();
    result->op = asm_.WasmTypeCheck(object.op, rtt, config);
  }

  void RefCast(FullDecoder* decoder, uint32_t ref_index, const Value& object,
               Value* result, bool null_succeeds) {
    if (v8_flags.experimental_wasm_assume_ref_cast_succeeds) {
      // TODO(14108): Implement type guards.
      Forward(decoder, object, result);
      return;
    }
    V<Map> rtt = asm_.RttCanon(instance_node_, ref_index);
    DCHECK_EQ(result->type.is_nullable(), null_succeeds);
    compiler::WasmTypeCheckConfig config{object.type, result->type};
    result->op = asm_.WasmTypeCast(object.op, rtt, config);
  }

  void RefCastAbstract(FullDecoder* decoder, const Value& object, HeapType type,
                       Value* result, bool null_succeeds) {
    if (v8_flags.experimental_wasm_assume_ref_cast_succeeds) {
      // TODO(14108): Implement type guards.
      Forward(decoder, object, result);
      return;
    }
    // TODO(jkummerow): {type} is redundant.
    DCHECK_IMPLIES(null_succeeds, result->type.is_nullable());
    DCHECK_EQ(type, result->type.heap_type());
    compiler::WasmTypeCheckConfig config{
        object.type, ValueType::RefMaybeNull(
                         type, null_succeeds ? kNullable : kNonNullable)};
    V<Map> rtt = OpIndex::Invalid();
    result->op = asm_.WasmTypeCast(object.op, rtt, config);
  }

  void BrOnCast(FullDecoder* decoder, uint32_t ref_index, const Value& object,
                Value* value_on_branch, uint32_t br_depth, bool null_succeeds) {
    V<Map> rtt = asm_.RttCanon(instance_node_, ref_index);
    compiler::WasmTypeCheckConfig config{
        object.type, ValueType::RefMaybeNull(
                         ref_index, null_succeeds ? kNullable : kNonNullable)};
    return BrOnCastImpl(decoder, rtt, config, object, value_on_branch, br_depth,
                        null_succeeds);
  }

  void BrOnCastAbstract(FullDecoder* decoder, const Value& object,
                        HeapType type, Value* value_on_branch,
                        uint32_t br_depth, bool null_succeeds) {
    V<Map> rtt = OpIndex::Invalid();
    compiler::WasmTypeCheckConfig config{
        object.type, ValueType::RefMaybeNull(
                         type, null_succeeds ? kNullable : kNonNullable)};
    return BrOnCastImpl(decoder, rtt, config, object, value_on_branch, br_depth,
                        null_succeeds);
  }

  void BrOnCastFail(FullDecoder* decoder, uint32_t ref_index,
                    const Value& object, Value* value_on_fallthrough,
                    uint32_t br_depth, bool null_succeeds) {
    V<Map> rtt = asm_.RttCanon(instance_node_, ref_index);
    compiler::WasmTypeCheckConfig config{
        object.type, ValueType::RefMaybeNull(
                         ref_index, null_succeeds ? kNullable : kNonNullable)};
    return BrOnCastFailImpl(decoder, rtt, config, object, value_on_fallthrough,
                            br_depth, null_succeeds);
  }

  void BrOnCastFailAbstract(FullDecoder* decoder, const Value& object,
                            HeapType type, Value* value_on_fallthrough,
                            uint32_t br_depth, bool null_succeeds) {
    V<Map> rtt = OpIndex::Invalid();
    compiler::WasmTypeCheckConfig config{
        object.type, ValueType::RefMaybeNull(
                         type, null_succeeds ? kNullable : kNonNullable)};
    return BrOnCastFailImpl(decoder, rtt, config, object, value_on_fallthrough,
                            br_depth, null_succeeds);
  }

  void RefIsStruct(FullDecoder* decoder, const Value& object, Value* result) {
    Bailout(decoder);  // Deprecated: Do not implement!
  }

  void RefAsStruct(FullDecoder* decoder, const Value& object, Value* result) {
    Bailout(decoder);  // Deprecated: Do not implement!
  }

  void BrOnStruct(FullDecoder* decoder, const Value& object,
                  Value* value_on_branch, uint32_t br_depth,
                  bool null_succeeds) {
    Bailout(decoder);  // Deprecated: Do not implement!
  }

  void BrOnNonStruct(FullDecoder* decoder, const Value& object,
                     Value* value_on_fallthrough, uint32_t br_depth,
                     bool null_succeeds) {
    Bailout(decoder);  // Deprecated: Do not implement!
  }

  void RefIsArray(FullDecoder* decoder, const Value& object, Value* result) {
    Bailout(decoder);  // Deprecated: Do not implement!
  }

  void RefAsArray(FullDecoder* decoder, const Value& object, Value* result) {
    Bailout(decoder);  // Deprecated: Do not implement!
  }

  void BrOnArray(FullDecoder* decoder, const Value& object,
                 Value* value_on_branch, uint32_t br_depth,
                 bool null_succeeds) {
    Bailout(decoder);  // Deprecated: Do not implement!
  }

  void BrOnNonArray(FullDecoder* decoder, const Value& object,
                    Value* value_on_fallthrough, uint32_t br_depth,
                    bool null_succeeds) {
    Bailout(decoder);  // Deprecated: Do not implement!
  }

  void RefIsI31(FullDecoder* decoder, const Value& object, Value* result) {
    Bailout(decoder);  // Deprecated: Do not implement!
  }

  void RefAsI31(FullDecoder* decoder, const Value& object, Value* result) {
    Bailout(decoder);  // Deprecated: Do not implement!
  }

  void BrOnI31(FullDecoder* decoder, const Value& object,
               Value* value_on_branch, uint32_t br_depth, bool null_succeeds) {
    Bailout(decoder);  // Deprecated: Do not implement!
  }

  void BrOnNonI31(FullDecoder* decoder, const Value& object,
                  Value* value_on_fallthrough, uint32_t br_depth,
                  bool null_succeeds) {
    Bailout(decoder);  // Deprecated: Do not implement!
  }

  void BrOnString(FullDecoder* decoder, const Value& object,
                  Value* value_on_branch, uint32_t br_depth,
                  bool null_succeeds) {
    Bailout(decoder);  // Deprecated: Do not implement!
  }

  void BrOnNonString(FullDecoder* decoder, const Value& object,
                     Value* value_on_fallthrough, uint32_t br_depth,
                     bool null_succeeds) {
    Bailout(decoder);  // Deprecated: Do not implement!
  }

  void StringNewWtf8(FullDecoder* decoder, const MemoryIndexImmediate& memory,
                     const unibrow::Utf8Variant variant, const Value& offset,
                     const Value& size, Value* result) {
    Bailout(decoder);
  }

  void StringNewWtf8Array(FullDecoder* decoder,
                          const unibrow::Utf8Variant variant,
                          const Value& array, const Value& start,
                          const Value& end, Value* result) {
    Bailout(decoder);
  }

  void StringNewWtf16(FullDecoder* decoder, const MemoryIndexImmediate& imm,
                      const Value& offset, const Value& size, Value* result) {
    Bailout(decoder);
  }

  void StringNewWtf16Array(FullDecoder* decoder, const Value& array,
                           const Value& start, const Value& end,
                           Value* result) {
    Bailout(decoder);
  }

  void StringConst(FullDecoder* decoder, const StringConstImmediate& imm,
                   Value* result) {
    Bailout(decoder);
  }

  void StringMeasureWtf8(FullDecoder* decoder,
                         const unibrow::Utf8Variant variant, const Value& str,
                         Value* result) {
    Bailout(decoder);
  }

  void StringMeasureWtf16(FullDecoder* decoder, const Value& str,
                          Value* result) {
    Bailout(decoder);
  }

  void StringEncodeWtf8(FullDecoder* decoder,
                        const MemoryIndexImmediate& memory,
                        const unibrow::Utf8Variant variant, const Value& str,
                        const Value& offset, Value* result) {
    Bailout(decoder);
  }

  void StringEncodeWtf8Array(FullDecoder* decoder,
                             const unibrow::Utf8Variant variant,
                             const Value& str, const Value& array,
                             const Value& start, Value* result) {
    Bailout(decoder);
  }

  void StringEncodeWtf16(FullDecoder* decoder, const MemoryIndexImmediate& imm,
                         const Value& str, const Value& offset, Value* result) {
    Bailout(decoder);
  }

  void StringEncodeWtf16Array(FullDecoder* decoder, const Value& str,
                              const Value& array, const Value& start,
                              Value* result) {
    Bailout(decoder);
  }

  void StringConcat(FullDecoder* decoder, const Value& head, const Value& tail,
                    Value* result) {
    Bailout(decoder);
  }

  void StringEq(FullDecoder* decoder, const Value& a, const Value& b,
                Value* result) {
    Bailout(decoder);
  }

  void StringIsUSVSequence(FullDecoder* decoder, const Value& str,
                           Value* result) {
    Bailout(decoder);
  }

  void StringAsWtf8(FullDecoder* decoder, const Value& str, Value* result) {
    Bailout(decoder);
  }

  void StringViewWtf8Advance(FullDecoder* decoder, const Value& view,
                             const Value& pos, const Value& bytes,
                             Value* result) {
    Bailout(decoder);
  }

  void StringViewWtf8Encode(FullDecoder* decoder,
                            const MemoryIndexImmediate& memory,
                            const unibrow::Utf8Variant variant,
                            const Value& view, const Value& addr,
                            const Value& pos, const Value& bytes,
                            Value* next_pos, Value* bytes_written) {
    Bailout(decoder);
  }

  void StringViewWtf8Slice(FullDecoder* decoder, const Value& view,
                           const Value& start, const Value& end,
                           Value* result) {
    Bailout(decoder);
  }

  void StringAsWtf16(FullDecoder* decoder, const Value& str, Value* result) {
    Bailout(decoder);
  }

  void StringViewWtf16GetCodeUnit(FullDecoder* decoder, const Value& view,
                                  const Value& pos, Value* result) {
    Bailout(decoder);
  }

  void StringViewWtf16Encode(FullDecoder* decoder,
                             const MemoryIndexImmediate& imm, const Value& view,
                             const Value& offset, const Value& pos,
                             const Value& codeunits, Value* result) {
    Bailout(decoder);
  }

  void StringViewWtf16Slice(FullDecoder* decoder, const Value& view,
                            const Value& start, const Value& end,
                            Value* result) {
    Bailout(decoder);
  }

  void StringAsIter(FullDecoder* decoder, const Value& str, Value* result) {
    Bailout(decoder);
  }

  void StringViewIterNext(FullDecoder* decoder, const Value& view,
                          Value* result) {
    Bailout(decoder);
  }

  void StringViewIterAdvance(FullDecoder* decoder, const Value& view,
                             const Value& codepoints, Value* result) {
    Bailout(decoder);
  }

  void StringViewIterRewind(FullDecoder* decoder, const Value& view,
                            const Value& codepoints, Value* result) {
    Bailout(decoder);
  }

  void StringViewIterSlice(FullDecoder* decoder, const Value& view,
                           const Value& codepoints, Value* result) {
    Bailout(decoder);
  }

  void StringCompare(FullDecoder* decoder, const Value& lhs, const Value& rhs,
                     Value* result) {
    Bailout(decoder);
  }

  void StringFromCodePoint(FullDecoder* decoder, const Value& code_point,
                           Value* result) {
    Bailout(decoder);
  }

  void StringHash(FullDecoder* decoder, const Value& string, Value* result) {
    Bailout(decoder);
  }

  void Forward(FullDecoder* decoder, const Value& from, Value* to) {
    to->op = from.op;
  }

  bool did_bailout() { return did_bailout_; }

 private:
  // Holds phi inputs for a specific block. These include SSA values as well as
  // stack merge values.
  struct BlockPhis {
    // The first vector corresponds to all inputs of the first phi etc.
    std::vector<std::vector<OpIndex>> phi_inputs;
    std::vector<ValueType> phi_types;
    std::vector<OpIndex> incoming_exception;

    explicit BlockPhis(int total_arity)
        : phi_inputs(total_arity), phi_types(total_arity) {}
  };

  enum class CheckForException { kNo, kYes };

  void Bailout(FullDecoder* decoder) {
    decoder->errorf("Unsupported Turboshaft operation: %s",
                    decoder->SafeOpcodeNameAt(decoder->pc()));
    did_bailout_ = true;
  }

  void BailoutWithoutOpcode(FullDecoder* decoder, const char* message) {
    decoder->errorf("Unsupported operation: %s", message);
    did_bailout_ = true;
  }

  // Creates a new block, initializes a {BlockPhis} for it, and registers it
  // with block_phis_. We pass a {merge} only if we later need to recover values
  // for that merge.
  TSBlock* NewBlock(FullDecoder* decoder, Merge<Value>* merge) {
    TSBlock* block = asm_.NewBlock();
    BlockPhis block_phis(decoder->num_locals() +
                         (merge != nullptr ? merge->arity : 0));
    for (uint32_t i = 0; i < decoder->num_locals(); i++) {
      block_phis.phi_types[i] = decoder->local_type(i);
    }
    if (merge != nullptr) {
      for (uint32_t i = 0; i < merge->arity; i++) {
        block_phis.phi_types[decoder->num_locals() + i] = (*merge)[i].type;
      }
    }
    block_phis_.emplace(block, std::move(block_phis));
    return block;
  }

  // Sets up a control flow edge from the current SSA environment and a stack to
  // {block}. The stack is {stack_values} if present, otherwise the current
  // decoder stack.
  void SetupControlFlowEdge(FullDecoder* decoder, TSBlock* block,
                            uint32_t drop_values = 0,
                            V<Tagged> exception = OpIndex::Invalid(),
                            Merge<Value>* stack_values = nullptr) {
    if (asm_.current_block() == nullptr) return;
    // It is guaranteed that this element exists.
    BlockPhis& phis_for_block = block_phis_.find(block)->second;
    uint32_t merge_arity =
        static_cast<uint32_t>(phis_for_block.phi_inputs.size()) -
        decoder->num_locals();
    for (size_t i = 0; i < ssa_env_.size(); i++) {
      phis_for_block.phi_inputs[i].emplace_back(ssa_env_[i]);
    }
    // We never drop values from an explicit merge.
    DCHECK_IMPLIES(stack_values != nullptr, drop_values == 0);
    Value* stack_base = merge_arity == 0 ? nullptr
                        : stack_values != nullptr
                            ? &(*stack_values)[0]
                            : decoder->stack_value(merge_arity + drop_values);
    for (size_t i = 0; i < merge_arity; i++) {
      DCHECK(stack_base[i].op.valid());
      phis_for_block.phi_inputs[decoder->num_locals() + i].emplace_back(
          stack_base[i].op);
    }
    if (exception.valid()) {
      phis_for_block.incoming_exception.push_back(exception);
    }
  }

  OpIndex MaybePhi(std::vector<OpIndex>& elements, ValueType type) {
    if (elements.empty()) return OpIndex::Invalid();
    for (size_t i = 1; i < elements.size(); i++) {
      if (elements[i] != elements[0]) {
        return asm_.Phi(base::VectorOf(elements), RepresentationFor(type));
      }
    }
    return elements[0];
  }

  // Binds a block, initializes phis for its SSA environment from its entry in
  // {block_phis_}, and sets values to its {merge} (if available) from the
  // its entry in {block_phis_}.
  void EnterBlock(FullDecoder* decoder, TSBlock* tsblock, Merge<Value>* merge,
                  OpIndex* exception = nullptr) {
    asm_.Bind(tsblock);
    BlockPhis& block_phis = block_phis_.at(tsblock);
    for (uint32_t i = 0; i < decoder->num_locals(); i++) {
      ssa_env_[i] = MaybePhi(block_phis.phi_inputs[i], block_phis.phi_types[i]);
    }
    DCHECK_EQ(decoder->num_locals() + (merge != nullptr ? merge->arity : 0),
              block_phis.phi_inputs.size());
    if (merge != nullptr) {
      for (uint32_t i = 0; i < merge->arity; i++) {
        (*merge)[i].op =
            MaybePhi(block_phis.phi_inputs[decoder->num_locals() + i],
                     block_phis.phi_types[decoder->num_locals() + i]);
      }
    }
    DCHECK_IMPLIES(exception == nullptr, block_phis.incoming_exception.empty());
    if (exception != nullptr && !exception->valid()) {
      *exception = MaybePhi(block_phis.incoming_exception, kWasmExternRef);
    }
    block_phis_.erase(tsblock);
  }

  OpIndex DefaultValue(ValueType type) {
    switch (type.kind()) {
      case kI8:
      case kI16:
      case kI32:
        return asm_.Word32Constant(int32_t{0});
      case kI64:
        return asm_.Word64Constant(int64_t{0});
      case kF32:
        return asm_.Float32Constant(0.0f);
      case kF64:
        return asm_.Float64Constant(0.0);
      case kRefNull:
        return asm_.Null(type);
      case kS128: {
        uint8_t value[kSimd128Size] = {};
        return asm_.Simd128Constant(value);
      }
      case kVoid:
      case kRtt:
      case kRef:
      case kBottom:
        UNREACHABLE();
    }
  }

  RegisterRepresentation RepresentationFor(ValueType type) {
    switch (type.kind()) {
      case kI8:
      case kI16:
      case kI32:
        return RegisterRepresentation::Word32();
      case kI64:
        return RegisterRepresentation::Word64();
      case kF32:
        return RegisterRepresentation::Float32();
      case kF64:
        return RegisterRepresentation::Float64();
      case kRefNull:
      case kRef:
        return RegisterRepresentation::Tagged();
      case kS128:
        return RegisterRepresentation::Simd128();
      case kVoid:
      case kRtt:
      case kBottom:
        UNREACHABLE();
    }
  }

  V<Word64> ExtractTruncationProjections(OpIndex truncated) {
    V<Word64> result =
        asm_.Projection(truncated, 0, RegisterRepresentation::Word64());
    V<Word32> check =
        asm_.Projection(truncated, 1, RegisterRepresentation::Word32());
    asm_.TrapIf(asm_.Word32Equal(check, 0), OpIndex::Invalid(),
                TrapId::kTrapFloatUnrepresentable);
    return result;
  }

  std::pair<OpIndex, V<Word32>> BuildCCallForFloatConversion(
      OpIndex arg, MemoryRepresentation float_type,
      ExternalReference ccall_ref) {
    uint8_t slot_size = MemoryRepresentation::Int64().SizeInBytes();
    V<WordPtr> stack_slot = __ StackSlot(slot_size, slot_size);
    __ Store(stack_slot, arg, StoreOp::Kind::RawAligned(), float_type,
             compiler::WriteBarrierKind::kNoWriteBarrier);
    MachineType reps[]{MachineType::Int32(), MachineType::Pointer()};
    MachineSignature sig(1, 1, reps);
    V<Word32> overflow = CallC(&sig, ccall_ref, stack_slot);
    return {stack_slot, overflow};
  }

  OpIndex BuildCcallConvertFloat(OpIndex arg, MemoryRepresentation float_type,
                                 ExternalReference ccall_ref) {
    auto [stack_slot, overflow] =
        BuildCCallForFloatConversion(arg, float_type, ccall_ref);
    __ TrapIf(__ Word32Equal(overflow, 0), OpIndex::Invalid(),
              compiler::TrapId::kTrapFloatUnrepresentable);
    MemoryRepresentation int64 = MemoryRepresentation::Int64();
    return __ Load(stack_slot, LoadOp::Kind::RawAligned(), int64);
  }

  OpIndex BuildCcallConvertFloatSat(OpIndex arg,
                                    MemoryRepresentation float_type,
                                    ExternalReference ccall_ref,
                                    bool is_signed) {
    MemoryRepresentation int64 = MemoryRepresentation::Int64();
    uint8_t slot_size = int64.SizeInBytes();
    V<WordPtr> stack_slot = __ StackSlot(slot_size, slot_size);
    __ Store(stack_slot, arg, StoreOp::Kind::RawAligned(), float_type,
             compiler::WriteBarrierKind::kNoWriteBarrier);
    MachineType reps[]{MachineType::Pointer()};
    MachineSignature sig(0, 1, reps);
    CallC(&sig, ccall_ref, stack_slot);
    return __ Load(stack_slot, LoadOp::Kind::RawAligned(), int64);
  }

  OpIndex BuildIntToFloatConversionInstruction(
      OpIndex input, ExternalReference ccall_ref,
      MemoryRepresentation input_representation,
      MemoryRepresentation result_representation) {
    uint8_t slot_size = std::max(input_representation.SizeInBytes(),
                                 result_representation.SizeInBytes());
    V<WordPtr> stack_slot = __ StackSlot(slot_size, slot_size);
    __ Store(stack_slot, input, StoreOp::Kind::RawAligned(),
             input_representation, compiler::WriteBarrierKind::kNoWriteBarrier);
    MachineType reps[]{MachineType::Pointer()};
    MachineSignature sig(0, 1, reps);
    CallC(&sig, ccall_ref, stack_slot);
    return __ Load(stack_slot, LoadOp::Kind::RawAligned(),
                   result_representation);
  }

  OpIndex BuildDiv64Call(OpIndex lhs, OpIndex rhs, ExternalReference ccall_ref,
                         wasm::TrapId trap_zero) {
    MemoryRepresentation int64_rep = MemoryRepresentation::Int64();
    V<WordPtr> stack_slot =
        asm_.StackSlot(2 * int64_rep.SizeInBytes(), int64_rep.SizeInBytes());
    asm_.Store(stack_slot, lhs, StoreOp::Kind::RawAligned(), int64_rep,
               compiler::WriteBarrierKind::kNoWriteBarrier);
    asm_.Store(stack_slot, rhs, StoreOp::Kind::RawAligned(), int64_rep,
               compiler::WriteBarrierKind::kNoWriteBarrier,
               int64_rep.SizeInBytes());

    MachineType sig_types[] = {MachineType::Int32(), MachineType::Pointer()};
    MachineSignature sig(1, 1, sig_types);
    OpIndex rc = CallC(&sig, ccall_ref, stack_slot);
    __ TrapIf(__ Word32Equal(rc, 0), OpIndex::Invalid(), trap_zero);
    __ TrapIf(__ Word32Equal(rc, -1), OpIndex::Invalid(),
              TrapId::kTrapDivUnrepresentable);
    return __ Load(stack_slot, LoadOp::Kind::RawAligned(), int64_rep);
  }

  // TODO(14108): Remove the decoder argument once we have no bailouts.
  OpIndex UnOpImpl(FullDecoder* decoder, WasmOpcode opcode, OpIndex arg,
                   ValueType input_type /* for ref.is_null only*/) {
    switch (opcode) {
      case kExprI32Eqz:
        return asm_.Word32Equal(arg, 0);
      case kExprF32Abs:
        return asm_.Float32Abs(arg);
      case kExprF32Neg:
        return asm_.Float32Negate(arg);
      case kExprF32Sqrt:
        return asm_.Float32Sqrt(arg);
      case kExprF64Abs:
        return asm_.Float64Abs(arg);
      case kExprF64Neg:
        return asm_.Float64Negate(arg);
      case kExprF64Sqrt:
        return asm_.Float64Sqrt(arg);
      case kExprI32SConvertF32: {
        V<Float32> truncated = UnOpImpl(decoder, kExprF32Trunc, arg, kWasmF32);
        V<Word32> result = asm_.TruncateFloat32ToInt32OverflowToMin(truncated);
        V<Float32> converted_back = asm_.ChangeInt32ToFloat32(result);
        asm_.TrapIf(
            asm_.Word32Equal(asm_.Float32Equal(converted_back, truncated), 0),
            OpIndex::Invalid(), TrapId::kTrapFloatUnrepresentable);
        return result;
      }
      case kExprI32UConvertF32: {
        V<Float32> truncated = UnOpImpl(decoder, kExprF32Trunc, arg, kWasmF32);
        V<Word32> result = asm_.TruncateFloat32ToUint32OverflowToMin(truncated);
        V<Float32> converted_back = asm_.ChangeUint32ToFloat32(result);
        asm_.TrapIf(
            asm_.Word32Equal(asm_.Float32Equal(converted_back, truncated), 0),
            OpIndex::Invalid(), TrapId::kTrapFloatUnrepresentable);
        return result;
      }
      case kExprI32SConvertF64: {
        V<Float64> truncated = UnOpImpl(decoder, kExprF64Trunc, arg, kWasmF64);
        V<Word32> result =
            asm_.TruncateFloat64ToInt32OverflowUndefined(truncated);
        V<Float64> converted_back = asm_.ChangeInt32ToFloat64(result);
        asm_.TrapIf(
            asm_.Word32Equal(asm_.Float64Equal(converted_back, truncated), 0),
            OpIndex::Invalid(), TrapId::kTrapFloatUnrepresentable);
        return result;
      }
      case kExprI32UConvertF64: {
        V<Float64> truncated = UnOpImpl(decoder, kExprF64Trunc, arg, kWasmF64);
        V<Word32> result = asm_.TruncateFloat64ToUint32OverflowToMin(truncated);
        V<Float64> converted_back = asm_.ChangeUint32ToFloat64(result);
        asm_.TrapIf(
            asm_.Word32Equal(asm_.Float64Equal(converted_back, truncated), 0),
            OpIndex::Invalid(), TrapId::kTrapFloatUnrepresentable);
        return result;
      }
      case kExprI64SConvertF32:
        return Is64() ? ExtractTruncationProjections(
                            asm_.TryTruncateFloat32ToInt64(arg))
                      : BuildCcallConvertFloat(
                            arg, MemoryRepresentation::Float32(),
                            ExternalReference::wasm_float32_to_int64());
      case kExprI64UConvertF32:
        return Is64() ? ExtractTruncationProjections(
                            asm_.TryTruncateFloat32ToUint64(arg))
                      : BuildCcallConvertFloat(
                            arg, MemoryRepresentation::Float32(),
                            ExternalReference::wasm_float32_to_uint64());
      case kExprI64SConvertF64:
        return Is64() ? ExtractTruncationProjections(
                            asm_.TryTruncateFloat64ToInt64(arg))
                      : BuildCcallConvertFloat(
                            arg, MemoryRepresentation::Float64(),
                            ExternalReference::wasm_float64_to_int64());
      case kExprI64UConvertF64:
        return Is64() ? ExtractTruncationProjections(
                            asm_.TryTruncateFloat64ToUint64(arg))
                      : BuildCcallConvertFloat(
                            arg, MemoryRepresentation::Float64(),
                            ExternalReference::wasm_float64_to_uint64());
      case kExprF64SConvertI32:
        return asm_.ChangeInt32ToFloat64(arg);
      case kExprF64UConvertI32:
        return asm_.ChangeUint32ToFloat64(arg);
      case kExprF32SConvertI32:
        return asm_.ChangeInt32ToFloat32(arg);
      case kExprF32UConvertI32:
        return asm_.ChangeUint32ToFloat32(arg);
      case kExprI32SConvertSatF32: {
        V<Float32> truncated = UnOpImpl(decoder, kExprF32Trunc, arg, kWasmF32);
        V<Word32> converted =
            asm_.TruncateFloat32ToInt32OverflowUndefined(truncated);
        V<Float32> converted_back = asm_.ChangeInt32ToFloat32(converted);

        Label<Word32> done(&asm_);

        IF (LIKELY(asm_.Float32Equal(truncated, converted_back))) {
          GOTO(done, converted);
        }
        ELSE {
          // Overflow.
          IF (asm_.Float32Equal(arg, arg)) {
            // Not NaN.
            IF (asm_.Float32LessThan(arg, 0)) {
              // Negative arg.
              GOTO(done,
                   asm_.Word32Constant(std::numeric_limits<int32_t>::min()));
            }
            ELSE {
              // Positive arg.
              GOTO(done,
                   asm_.Word32Constant(std::numeric_limits<int32_t>::max()));
            }
            END_IF
          }
          ELSE {
            // NaN.
            GOTO(done, asm_.Word32Constant(0));
          }
          END_IF
        }
        END_IF
        BIND(done, result);

        return result;
      }
      case kExprI32UConvertSatF32: {
        V<Float32> truncated = UnOpImpl(decoder, kExprF32Trunc, arg, kWasmF32);
        V<Word32> converted =
            asm_.TruncateFloat32ToUint32OverflowUndefined(truncated);
        V<Float32> converted_back = asm_.ChangeUint32ToFloat32(converted);

        Label<Word32> done(&asm_);

        IF (LIKELY(asm_.Float32Equal(truncated, converted_back))) {
          GOTO(done, converted);
        }
        ELSE {
          // Overflow.
          IF (asm_.Float32Equal(arg, arg)) {
            // Not NaN.
            IF (asm_.Float32LessThan(arg, 0)) {
              // Negative arg.
              GOTO(done, asm_.Word32Constant(0));
            }
            ELSE {
              // Positive arg.
              GOTO(done,
                   asm_.Word32Constant(std::numeric_limits<uint32_t>::max()));
            }
            END_IF
          }
          ELSE {
            // NaN.
            GOTO(done, asm_.Word32Constant(0));
          }
          END_IF
        }
        END_IF
        BIND(done, result);

        return result;
      }
      case kExprI32SConvertSatF64: {
        V<Float64> truncated = UnOpImpl(decoder, kExprF64Trunc, arg, kWasmF64);
        V<Word32> converted =
            asm_.TruncateFloat64ToInt32OverflowUndefined(truncated);
        V<Float64> converted_back = asm_.ChangeInt32ToFloat64(converted);

        Label<Word32> done(&asm_);

        IF (LIKELY(asm_.Float64Equal(truncated, converted_back))) {
          GOTO(done, converted);
        }
        ELSE {
          // Overflow.
          IF (asm_.Float64Equal(arg, arg)) {
            // Not NaN.
            IF (asm_.Float64LessThan(arg, 0)) {
              // Negative arg.
              GOTO(done,
                   asm_.Word32Constant(std::numeric_limits<int32_t>::min()));
            }
            ELSE {
              // Positive arg.
              GOTO(done,
                   asm_.Word32Constant(std::numeric_limits<int32_t>::max()));
            }
            END_IF
          }
          ELSE {
            // NaN.
            GOTO(done, asm_.Word32Constant(0));
          }
          END_IF
        }
        END_IF
        BIND(done, result);

        return result;
      }
      case kExprI32UConvertSatF64: {
        V<Float64> truncated = UnOpImpl(decoder, kExprF64Trunc, arg, kWasmF64);
        V<Word32> converted =
            asm_.TruncateFloat64ToUint32OverflowUndefined(truncated);
        V<Float64> converted_back = asm_.ChangeUint32ToFloat64(converted);

        Label<Word32> done(&asm_);

        IF (LIKELY(asm_.Float64Equal(truncated, converted_back))) {
          GOTO(done, converted);
        }
        ELSE {
          // Overflow.
          IF (asm_.Float64Equal(arg, arg)) {
            // Not NaN.
            IF (asm_.Float64LessThan(arg, 0)) {
              // Negative arg.
              GOTO(done, asm_.Word32Constant(0));
            }
            ELSE {
              // Positive arg.
              GOTO(done,
                   asm_.Word32Constant(std::numeric_limits<uint32_t>::max()));
            }
            END_IF
          }
          ELSE {
            // NaN.
            GOTO(done, asm_.Word32Constant(0));
          }
          END_IF
        }
        END_IF
        BIND(done, result);

        return result;
      }
      case kExprI64SConvertSatF32: {
        if (!Is64()) {
          bool is_signed = true;
          return BuildCcallConvertFloatSat(
              arg, MemoryRepresentation::Float32(),
              ExternalReference::wasm_float32_to_int64_sat(), is_signed);
        }
        V<Word64> converted = asm_.TryTruncateFloat32ToInt64(arg);
        Label<compiler::turboshaft::Word64> done(&asm_);

        if (SupportedOperations::sat_conversion_is_safe()) {
          return asm_.Projection(converted, 0,
                                 RegisterRepresentation::Word64());
        }
        IF (LIKELY(asm_.Projection(converted, 1,
                                   RegisterRepresentation::Word32()))) {
          GOTO(done,
               asm_.Projection(converted, 0, RegisterRepresentation::Word64()));
        }
        ELSE {
          // Overflow.
          IF (asm_.Float32Equal(arg, arg)) {
            // Not NaN.
            IF (asm_.Float32LessThan(arg, 0)) {
              // Negative arg.
              GOTO(done,
                   asm_.Word64Constant(std::numeric_limits<int64_t>::min()));
            }
            ELSE {
              // Positive arg.
              GOTO(done,
                   asm_.Word64Constant(std::numeric_limits<int64_t>::max()));
            }
            END_IF
          }
          ELSE {
            // NaN.
            GOTO(done, asm_.Word64Constant(int64_t{0}));
          }
          END_IF
        }
        END_IF
        BIND(done, result);

        return result;
      }
      case kExprI64UConvertSatF32: {
        if (!Is64()) {
          bool is_signed = false;
          return BuildCcallConvertFloatSat(
              arg, MemoryRepresentation::Float32(),
              ExternalReference::wasm_float32_to_uint64_sat(), is_signed);
        }
        V<Word64> converted = asm_.TryTruncateFloat32ToUint64(arg);
        Label<compiler::turboshaft::Word64> done(&asm_);

        if (SupportedOperations::sat_conversion_is_safe()) {
          return asm_.Projection(converted, 0,
                                 RegisterRepresentation::Word64());
        }

        IF (LIKELY(asm_.Projection(converted, 1,
                                   RegisterRepresentation::Word32()))) {
          GOTO(done,
               asm_.Projection(converted, 0, RegisterRepresentation::Word64()));
        }
        ELSE {
          // Overflow.
          IF (asm_.Float32Equal(arg, arg)) {
            // Not NaN.
            IF (asm_.Float32LessThan(arg, 0)) {
              // Negative arg.
              GOTO(done, asm_.Word64Constant(int64_t{0}));
            }
            ELSE {
              // Positive arg.
              GOTO(done,
                   asm_.Word64Constant(std::numeric_limits<uint64_t>::max()));
            }
            END_IF
          }
          ELSE {
            // NaN.
            GOTO(done, asm_.Word64Constant(int64_t{0}));
          }
          END_IF
        }
        END_IF
        BIND(done, result);

        return result;
      }
      case kExprI64SConvertSatF64: {
        if (!Is64()) {
          bool is_signed = true;
          return BuildCcallConvertFloatSat(
              arg, MemoryRepresentation::Float64(),
              ExternalReference::wasm_float64_to_int64_sat(), is_signed);
        }
        V<Word64> converted = asm_.TryTruncateFloat64ToInt64(arg);
        Label<compiler::turboshaft::Word64> done(&asm_);

        if (SupportedOperations::sat_conversion_is_safe()) {
          return asm_.Projection(converted, 0,
                                 RegisterRepresentation::Word64());
        }

        IF (LIKELY(asm_.Projection(converted, 1,
                                   RegisterRepresentation::Word32()))) {
          GOTO(done,
               asm_.Projection(converted, 0, RegisterRepresentation::Word64()));
        }
        ELSE {
          // Overflow.
          IF (asm_.Float64Equal(arg, arg)) {
            // Not NaN.
            IF (asm_.Float64LessThan(arg, 0)) {
              // Negative arg.
              GOTO(done,
                   asm_.Word64Constant(std::numeric_limits<int64_t>::min()));
            }
            ELSE {
              // Positive arg.
              GOTO(done,
                   asm_.Word64Constant(std::numeric_limits<int64_t>::max()));
            }
            END_IF
          }
          ELSE {
            // NaN.
            GOTO(done, asm_.Word64Constant(int64_t{0}));
          }
          END_IF
        }
        END_IF
        BIND(done, result);

        return result;
      }
      case kExprI64UConvertSatF64: {
        if (!Is64()) {
          bool is_signed = false;
          return BuildCcallConvertFloatSat(
              arg, MemoryRepresentation::Float64(),
              ExternalReference::wasm_float64_to_uint64_sat(), is_signed);
        }
        V<Word64> converted = asm_.TryTruncateFloat64ToUint64(arg);
        Label<compiler::turboshaft::Word64> done(&asm_);

        if (SupportedOperations::sat_conversion_is_safe()) {
          return asm_.Projection(converted, 0,
                                 RegisterRepresentation::Word64());
        }

        IF (LIKELY(asm_.Projection(converted, 1,
                                   RegisterRepresentation::Word32()))) {
          GOTO(done,
               asm_.Projection(converted, 0, RegisterRepresentation::Word64()));
        }
        ELSE {
          // Overflow.
          IF (asm_.Float64Equal(arg, arg)) {
            // Not NaN.
            IF (asm_.Float64LessThan(arg, 0)) {
              // Negative arg.
              GOTO(done, asm_.Word64Constant(int64_t{0}));
            }
            ELSE {
              // Positive arg.
              GOTO(done,
                   asm_.Word64Constant(std::numeric_limits<uint64_t>::max()));
            }
            END_IF
          }
          ELSE {
            // NaN.
            GOTO(done, asm_.Word64Constant(int64_t{0}));
          }
          END_IF
        }
        END_IF
        BIND(done, result);

        return result;
      }
      case kExprF32ConvertF64:
        return asm_.ChangeFloat64ToFloat32(arg);
      case kExprF64ConvertF32:
        return asm_.ChangeFloat32ToFloat64(arg);
      case kExprF32ReinterpretI32:
        return asm_.BitcastWord32ToFloat32(arg);
      case kExprI32ReinterpretF32:
        return asm_.BitcastFloat32ToWord32(arg);
      case kExprI32Clz:
        return asm_.Word32CountLeadingZeros(arg);
      case kExprI32Ctz:
        if (SupportedOperations::word32_ctz()) {
          return asm_.Word32CountTrailingZeros(arg);
        } else {
          // TODO(14108): Use reverse_bits if supported.
          return CallCStackSlotToInt32(arg,
                                       ExternalReference::wasm_word32_ctz(),
                                       MemoryRepresentation::Int32());
        }
      case kExprI32Popcnt:
        if (SupportedOperations::word32_popcnt()) {
          return asm_.Word32PopCount(arg);
        } else {
          return CallCStackSlotToInt32(arg,
                                       ExternalReference::wasm_word32_popcnt(),
                                       MemoryRepresentation::Int32());
        }
      case kExprF32Floor:
        if (SupportedOperations::float32_round_down()) {
          return asm_.Float32RoundDown(arg);
        } else {
          return CallCStackSlotToStackSlot(arg,
                                           ExternalReference::wasm_f32_floor(),
                                           MemoryRepresentation::Float32());
        }
      case kExprF32Ceil:
        if (SupportedOperations::float32_round_up()) {
          return asm_.Float32RoundUp(arg);
        } else {
          return CallCStackSlotToStackSlot(arg,
                                           ExternalReference::wasm_f32_ceil(),
                                           MemoryRepresentation::Float32());
        }
      case kExprF32Trunc:
        if (SupportedOperations::float32_round_to_zero()) {
          return asm_.Float32RoundToZero(arg);
        } else {
          return CallCStackSlotToStackSlot(arg,
                                           ExternalReference::wasm_f32_trunc(),
                                           MemoryRepresentation::Float32());
        }
      case kExprF32NearestInt:
        if (SupportedOperations::float32_round_ties_even()) {
          return asm_.Float32RoundTiesEven(arg);
        } else {
          return CallCStackSlotToStackSlot(
              arg, ExternalReference::wasm_f32_nearest_int(),
              MemoryRepresentation::Float32());
        }
      case kExprF64Floor:
        if (SupportedOperations::float64_round_down()) {
          return asm_.Float64RoundDown(arg);
        } else {
          return CallCStackSlotToStackSlot(arg,
                                           ExternalReference::wasm_f64_floor(),
                                           MemoryRepresentation::Float64());
        }
      case kExprF64Ceil:
        if (SupportedOperations::float64_round_up()) {
          return asm_.Float64RoundUp(arg);
        } else {
          return CallCStackSlotToStackSlot(arg,
                                           ExternalReference::wasm_f64_ceil(),
                                           MemoryRepresentation::Float64());
        }
      case kExprF64Trunc:
        if (SupportedOperations::float64_round_to_zero()) {
          return asm_.Float64RoundToZero(arg);
        } else {
          return CallCStackSlotToStackSlot(arg,
                                           ExternalReference::wasm_f64_trunc(),
                                           MemoryRepresentation::Float64());
        }
      case kExprF64NearestInt:
        if (SupportedOperations::float64_round_ties_even()) {
          return asm_.Float64RoundTiesEven(arg);
        } else {
          return CallCStackSlotToStackSlot(
              arg, ExternalReference::wasm_f64_nearest_int(),
              MemoryRepresentation::Float64());
        }
      case kExprF64Acos:
        return CallCStackSlotToStackSlot(
            arg, ExternalReference::f64_acos_wrapper_function(),
            MemoryRepresentation::Float64());
      case kExprF64Asin:
        return CallCStackSlotToStackSlot(
            arg, ExternalReference::f64_asin_wrapper_function(),
            MemoryRepresentation::Float64());
      case kExprF64Atan:
        return asm_.Float64Atan(arg);
      case kExprF64Cos:
        return asm_.Float64Cos(arg);
      case kExprF64Sin:
        return asm_.Float64Sin(arg);
      case kExprF64Tan:
        return asm_.Float64Tan(arg);
      case kExprF64Exp:
        return asm_.Float64Exp(arg);
      case kExprF64Log:
        return asm_.Float64Log(arg);
      case kExprI32ConvertI64:
        return asm_.TruncateWord64ToWord32(arg);
      case kExprI64SConvertI32:
        return asm_.ChangeInt32ToInt64(arg);
      case kExprI64UConvertI32:
        return asm_.ChangeUint32ToUint64(arg);
      case kExprF64ReinterpretI64:
        return asm_.BitcastWord64ToFloat64(arg);
      case kExprI64ReinterpretF64:
        return asm_.BitcastFloat64ToWord64(arg);
      case kExprI64Clz:
        return asm_.Word64CountLeadingZeros(arg);
      case kExprI64Ctz:
        if (SupportedOperations::word64_ctz() ||
            (!Is64() && SupportedOperations::word32_ctz())) {
          return asm_.Word64CountTrailingZeros(arg);
        } else {
          // TODO(14108): Use reverse_bits if supported.
          return asm_.ChangeUint32ToUint64(
              CallCStackSlotToInt32(arg, ExternalReference::wasm_word64_ctz(),
                                    MemoryRepresentation::Int64()));
        }
      case kExprI64Popcnt:
        if (SupportedOperations::word64_popcnt() ||
            (!Is64() && SupportedOperations::word32_popcnt())) {
          return asm_.Word64PopCount(arg);
        } else {
          return asm_.ChangeUint32ToUint64(CallCStackSlotToInt32(
              arg, ExternalReference::wasm_word64_popcnt(),
              MemoryRepresentation::Int64()));
        }
      case kExprI64Eqz:
        return asm_.Word64Equal(arg, 0);
      case kExprF32SConvertI64:
        if (!Is64()) {
          return BuildIntToFloatConversionInstruction(
              arg, ExternalReference::wasm_int64_to_float32(),
              MemoryRepresentation::Int64(), MemoryRepresentation::Float32());
        }
        return asm_.ChangeInt64ToFloat32(arg);
      case kExprF32UConvertI64:
        if (!Is64()) {
          return BuildIntToFloatConversionInstruction(
              arg, ExternalReference::wasm_uint64_to_float32(),
              MemoryRepresentation::Uint64(), MemoryRepresentation::Float32());
        }
        return asm_.ChangeUint64ToFloat32(arg);
      case kExprF64SConvertI64:
        if (!Is64()) {
          return BuildIntToFloatConversionInstruction(
              arg, ExternalReference::wasm_int64_to_float64(),
              MemoryRepresentation::Int64(), MemoryRepresentation::Float64());
        }
        return asm_.ChangeInt64ToFloat64(arg);
      case kExprF64UConvertI64:
        if (!Is64()) {
          return BuildIntToFloatConversionInstruction(
              arg, ExternalReference::wasm_uint64_to_float64(),
              MemoryRepresentation::Uint64(), MemoryRepresentation::Float64());
        }
        return asm_.ChangeUint64ToFloat64(arg);
      case kExprI32SExtendI8:
        return asm_.Word32SignExtend8(arg);
      case kExprI32SExtendI16:
        return asm_.Word32SignExtend16(arg);
      case kExprI64SExtendI8:
        return asm_.Word64SignExtend8(arg);
      case kExprI64SExtendI16:
        return asm_.Word64SignExtend16(arg);
      case kExprI64SExtendI32:
        // TODO(14108): Is this correct?
        return asm_.ChangeInt32ToInt64(asm_.TruncateWord64ToWord32(arg));
      case kExprRefIsNull:
        return asm_.IsNull(arg, input_type);
      case kExprI32AsmjsLoadMem8S:
        return AsmjsLoadMem(arg, MemoryRepresentation::Int8());
      case kExprI32AsmjsLoadMem8U:
        return AsmjsLoadMem(arg, MemoryRepresentation::Uint8());
      case kExprI32AsmjsLoadMem16S:
        return AsmjsLoadMem(arg, MemoryRepresentation::Int16());
      case kExprI32AsmjsLoadMem16U:
        return AsmjsLoadMem(arg, MemoryRepresentation::Uint16());
      case kExprI32AsmjsLoadMem:
        return AsmjsLoadMem(arg, MemoryRepresentation::Int32());
      case kExprF32AsmjsLoadMem:
        return AsmjsLoadMem(arg, MemoryRepresentation::Float32());
      case kExprF64AsmjsLoadMem:
        return AsmjsLoadMem(arg, MemoryRepresentation::Float64());
      case kExprI32AsmjsSConvertF32:
      case kExprI32AsmjsUConvertF32:
        return asm_.JSTruncateFloat64ToWord32(asm_.ChangeFloat32ToFloat64(arg));
      case kExprI32AsmjsSConvertF64:
      case kExprI32AsmjsUConvertF64:
        return asm_.JSTruncateFloat64ToWord32(arg);
      case kExprRefAsNonNull:
        // We abuse ref.as_non_null, which isn't otherwise used in this switch,
        // as a sentinel for the negation of ref.is_null.
        return asm_.Word32Equal(asm_.IsNull(arg, input_type), 0);
      case kExprExternInternalize:
      case kExprExternExternalize:
        Bailout(decoder);
        return OpIndex::Invalid();
      default:
        UNREACHABLE();
    }
  }

  // TODO(14108): Implement 64-bit divisions on 32-bit platforms.
  OpIndex BinOpImpl(WasmOpcode opcode, OpIndex lhs, OpIndex rhs) {
    switch (opcode) {
      case kExprI32Add:
        return asm_.Word32Add(lhs, rhs);
      case kExprI32Sub:
        return asm_.Word32Sub(lhs, rhs);
      case kExprI32Mul:
        return asm_.Word32Mul(lhs, rhs);
      case kExprI32DivS: {
        asm_.TrapIf(asm_.Word32Equal(rhs, 0), OpIndex::Invalid(),
                    TrapId::kTrapDivByZero);
        V<Word32> unrepresentable_condition = asm_.Word32BitwiseAnd(
            asm_.Word32Equal(rhs, -1), asm_.Word32Equal(lhs, kMinInt));
        asm_.TrapIf(unrepresentable_condition, OpIndex::Invalid(),
                    TrapId::kTrapDivUnrepresentable);
        return asm_.Int32Div(lhs, rhs);
      }
      case kExprI32DivU:
        asm_.TrapIf(asm_.Word32Equal(rhs, 0), OpIndex::Invalid(),
                    TrapId::kTrapDivByZero);
        return asm_.Uint32Div(lhs, rhs);
      case kExprI32RemS: {
        asm_.TrapIf(asm_.Word32Equal(rhs, 0), OpIndex::Invalid(),
                    TrapId::kTrapRemByZero);
        Label<Word32> done(&asm_);
        IF (UNLIKELY(asm_.Word32Equal(rhs, -1))) {
          GOTO(done, asm_.Word32Constant(0));
        }
        ELSE {
          GOTO(done, asm_.Int32Mod(lhs, rhs));
        }
        END_IF;

        BIND(done, result);
        return result;
      }
      case kExprI32RemU:
        asm_.TrapIf(asm_.Word32Equal(rhs, 0), OpIndex::Invalid(),
                    TrapId::kTrapRemByZero);
        return asm_.Uint32Mod(lhs, rhs);
      case kExprI32And:
        return asm_.Word32BitwiseAnd(lhs, rhs);
      case kExprI32Ior:
        return asm_.Word32BitwiseOr(lhs, rhs);
      case kExprI32Xor:
        return asm_.Word32BitwiseXor(lhs, rhs);
      case kExprI32Shl:
        // If possible, the bitwise-and gets optimized away later.
        return asm_.Word32ShiftLeft(lhs, asm_.Word32BitwiseAnd(rhs, 0x1f));
      case kExprI32ShrS:
        return asm_.Word32ShiftRightArithmetic(
            lhs, asm_.Word32BitwiseAnd(rhs, 0x1f));
      case kExprI32ShrU:
        return asm_.Word32ShiftRightLogical(lhs,
                                            asm_.Word32BitwiseAnd(rhs, 0x1f));
      case kExprI32Ror:
        return asm_.Word32RotateRight(lhs, asm_.Word32BitwiseAnd(rhs, 0x1f));
      case kExprI32Rol:
        if (SupportedOperations::word32_rol()) {
          return asm_.Word32RotateLeft(lhs, asm_.Word32BitwiseAnd(rhs, 0x1f));
        } else {
          return asm_.Word32RotateRight(
              lhs, asm_.Word32Sub(32, asm_.Word32BitwiseAnd(rhs, 0x1f)));
        }
      case kExprI32Eq:
        return asm_.Word32Equal(lhs, rhs);
      case kExprI32Ne:
        return asm_.Word32Equal(asm_.Word32Equal(lhs, rhs), 0);
      case kExprI32LtS:
        return asm_.Int32LessThan(lhs, rhs);
      case kExprI32LeS:
        return asm_.Int32LessThanOrEqual(lhs, rhs);
      case kExprI32LtU:
        return asm_.Uint32LessThan(lhs, rhs);
      case kExprI32LeU:
        return asm_.Uint32LessThanOrEqual(lhs, rhs);
      case kExprI32GtS:
        return asm_.Int32LessThan(rhs, lhs);
      case kExprI32GeS:
        return asm_.Int32LessThanOrEqual(rhs, lhs);
      case kExprI32GtU:
        return asm_.Uint32LessThan(rhs, lhs);
      case kExprI32GeU:
        return asm_.Uint32LessThanOrEqual(rhs, lhs);
      case kExprI64Add:
        return asm_.Word64Add(lhs, rhs);
      case kExprI64Sub:
        return asm_.Word64Sub(lhs, rhs);
      case kExprI64Mul:
        return asm_.Word64Mul(lhs, rhs);
      case kExprI64DivS: {
        if (!Is64()) {
          return BuildDiv64Call(lhs, rhs, ExternalReference::wasm_int64_div(),
                                wasm::TrapId::kTrapDivByZero);
        }
        asm_.TrapIf(asm_.Word64Equal(rhs, 0), OpIndex::Invalid(),
                    TrapId::kTrapDivByZero);
        V<Word32> unrepresentable_condition = asm_.Word32BitwiseAnd(
            asm_.Word64Equal(rhs, -1),
            asm_.Word64Equal(lhs, std::numeric_limits<int64_t>::min()));
        asm_.TrapIf(unrepresentable_condition, OpIndex::Invalid(),
                    TrapId::kTrapDivUnrepresentable);
        return asm_.Int64Div(lhs, rhs);
      }
      case kExprI64DivU:
        if (!Is64()) {
          return BuildDiv64Call(lhs, rhs, ExternalReference::wasm_uint64_div(),
                                wasm::TrapId::kTrapDivByZero);
        }
        asm_.TrapIf(asm_.Word64Equal(rhs, 0), OpIndex::Invalid(),
                    TrapId::kTrapDivByZero);
        return asm_.Uint64Div(lhs, rhs);
      case kExprI64RemS: {
        if (!Is64()) {
          return BuildDiv64Call(lhs, rhs, ExternalReference::wasm_int64_mod(),
                                wasm::TrapId::kTrapRemByZero);
        }
        asm_.TrapIf(asm_.Word64Equal(rhs, 0), OpIndex::Invalid(),
                    TrapId::kTrapRemByZero);
        Label<Word64> done(&asm_);
        IF (UNLIKELY(asm_.Word64Equal(rhs, -1))) {
          GOTO(done, asm_.Word64Constant(int64_t{0}));
        }
        ELSE {
          GOTO(done, asm_.Int64Mod(lhs, rhs));
        }
        END_IF;

        BIND(done, result);
        return result;
      }
      case kExprI64RemU:
        if (!Is64()) {
          return BuildDiv64Call(lhs, rhs, ExternalReference::wasm_uint64_mod(),
                                wasm::TrapId::kTrapRemByZero);
        }
        asm_.TrapIf(asm_.Word64Equal(rhs, 0), OpIndex::Invalid(),
                    TrapId::kTrapRemByZero);
        return asm_.Uint64Mod(lhs, rhs);
      case kExprI64And:
        return asm_.Word64BitwiseAnd(lhs, rhs);
      case kExprI64Ior:
        return asm_.Word64BitwiseOr(lhs, rhs);
      case kExprI64Xor:
        return asm_.Word64BitwiseXor(lhs, rhs);
      case kExprI64Shl:
        // If possible, the bitwise-and gets optimized away later.
        return asm_.Word64ShiftLeft(
            lhs, asm_.Word32BitwiseAnd(asm_.TruncateWord64ToWord32(rhs), 0x3f));
      case kExprI64ShrS:
        return asm_.Word64ShiftRightArithmetic(
            lhs, asm_.Word32BitwiseAnd(asm_.TruncateWord64ToWord32(rhs), 0x3f));
      case kExprI64ShrU:
        return asm_.Word64ShiftRightLogical(
            lhs, asm_.Word32BitwiseAnd(asm_.TruncateWord64ToWord32(rhs), 0x3f));
      case kExprI64Ror:
        return asm_.Word64RotateRight(
            lhs, asm_.Word32BitwiseAnd(asm_.TruncateWord64ToWord32(rhs), 0x3f));
      case kExprI64Rol:
        if (SupportedOperations::word64_rol()) {
          return asm_.Word64RotateLeft(
              lhs,
              asm_.Word32BitwiseAnd(asm_.TruncateWord64ToWord32(rhs), 0x3f));
        } else {
          return asm_.Word64RotateRight(
              lhs,
              asm_.Word32BitwiseAnd(
                  asm_.Word32Sub(64, asm_.TruncateWord64ToWord32(rhs)), 0x3f));
        }
      case kExprI64Eq:
        return asm_.Word64Equal(lhs, rhs);
      case kExprI64Ne:
        return asm_.Word32Equal(asm_.Word64Equal(lhs, rhs), 0);
      case kExprI64LtS:
        return asm_.Int64LessThan(lhs, rhs);
      case kExprI64LeS:
        return asm_.Int64LessThanOrEqual(lhs, rhs);
      case kExprI64LtU:
        return asm_.Uint64LessThan(lhs, rhs);
      case kExprI64LeU:
        return asm_.Uint64LessThanOrEqual(lhs, rhs);
      case kExprI64GtS:
        return asm_.Int64LessThan(rhs, lhs);
      case kExprI64GeS:
        return asm_.Int64LessThanOrEqual(rhs, lhs);
      case kExprI64GtU:
        return asm_.Uint64LessThan(rhs, lhs);
      case kExprI64GeU:
        return asm_.Uint64LessThanOrEqual(rhs, lhs);
      case kExprF32CopySign: {
        V<Word32> lhs_without_sign =
            asm_.Word32BitwiseAnd(asm_.BitcastFloat32ToWord32(lhs), 0x7fffffff);
        V<Word32> rhs_sign =
            asm_.Word32BitwiseAnd(asm_.BitcastFloat32ToWord32(rhs), 0x80000000);
        return asm_.BitcastWord32ToFloat32(
            asm_.Word32BitwiseOr(lhs_without_sign, rhs_sign));
      }
      case kExprF32Add:
        return asm_.Float32Add(lhs, rhs);
      case kExprF32Sub:
        return asm_.Float32Sub(lhs, rhs);
      case kExprF32Mul:
        return asm_.Float32Mul(lhs, rhs);
      case kExprF32Div:
        return asm_.Float32Div(lhs, rhs);
      case kExprF32Eq:
        return asm_.Float32Equal(lhs, rhs);
      case kExprF32Ne:
        return asm_.Word32Equal(asm_.Float32Equal(lhs, rhs), 0);
      case kExprF32Lt:
        return asm_.Float32LessThan(lhs, rhs);
      case kExprF32Le:
        return asm_.Float32LessThanOrEqual(lhs, rhs);
      case kExprF32Gt:
        return asm_.Float32LessThan(rhs, lhs);
      case kExprF32Ge:
        return asm_.Float32LessThanOrEqual(rhs, lhs);
      case kExprF32Min:
        return asm_.Float32Min(rhs, lhs);
      case kExprF32Max:
        return asm_.Float32Max(rhs, lhs);
      case kExprF64CopySign: {
        V<Word64> lhs_without_sign = asm_.Word64BitwiseAnd(
            asm_.BitcastFloat64ToWord64(lhs), 0x7fffffffffffffff);
        V<Word64> rhs_sign = asm_.Word64BitwiseAnd(
            asm_.BitcastFloat64ToWord64(rhs), 0x8000000000000000);
        return asm_.BitcastWord64ToFloat64(
            asm_.Word64BitwiseOr(lhs_without_sign, rhs_sign));
      }
      case kExprF64Add:
        return asm_.Float64Add(lhs, rhs);
      case kExprF64Sub:
        return asm_.Float64Sub(lhs, rhs);
      case kExprF64Mul:
        return asm_.Float64Mul(lhs, rhs);
      case kExprF64Div:
        return asm_.Float64Div(lhs, rhs);
      case kExprF64Eq:
        return asm_.Float64Equal(lhs, rhs);
      case kExprF64Ne:
        return asm_.Word32Equal(asm_.Float64Equal(lhs, rhs), 0);
      case kExprF64Lt:
        return asm_.Float64LessThan(lhs, rhs);
      case kExprF64Le:
        return asm_.Float64LessThanOrEqual(lhs, rhs);
      case kExprF64Gt:
        return asm_.Float64LessThan(rhs, lhs);
      case kExprF64Ge:
        return asm_.Float64LessThanOrEqual(rhs, lhs);
      case kExprF64Min:
        return asm_.Float64Min(lhs, rhs);
      case kExprF64Max:
        return asm_.Float64Max(lhs, rhs);
      case kExprF64Pow:
        return asm_.Float64Power(lhs, rhs);
      case kExprF64Atan2:
        return asm_.Float64Atan2(lhs, rhs);
      case kExprF64Mod:
        return CallCStackSlotToStackSlot(
            lhs, rhs, ExternalReference::f64_mod_wrapper_function(),
            MemoryRepresentation::Float64());
      case kExprRefEq:
        return asm_.TaggedEqual(lhs, rhs);
      case kExprI32AsmjsDivS: {
        // asmjs semantics return 0 when dividing by 0.
        if (SupportedOperations::int32_div_is_safe()) {
          return asm_.Int32Div(lhs, rhs);
        }
        Label<Word32> done(&asm_);
        IF (UNLIKELY(asm_.Word32Equal(rhs, 0))) {
          GOTO(done, asm_.Word32Constant(0));
        }
        ELSE {
          IF (UNLIKELY(asm_.Word32Equal(rhs, -1))) {
            GOTO(done, asm_.Word32Sub(0, lhs));
          }
          ELSE {
            GOTO(done, asm_.Int32Div(lhs, rhs));
          }
          END_IF
        }
        END_IF
        BIND(done, result);
        return result;
      }
      case kExprI32AsmjsDivU: {
        // asmjs semantics return 0 when dividing by 0.
        if (SupportedOperations::uint32_div_is_safe()) {
          return asm_.Uint32Div(lhs, rhs);
        }
        Label<Word32> done(&asm_);
        IF (UNLIKELY(asm_.Word32Equal(rhs, 0))) {
          GOTO(done, asm_.Word32Constant(0));
        }
        ELSE {
          GOTO(done, asm_.Uint32Div(lhs, rhs));
        }
        END_IF
        BIND(done, result);
        return result;
      }
      case kExprI32AsmjsRemS: {
        Label<Word32> done(&asm_);
        IF (UNLIKELY(asm_.Word32Equal(rhs, 0))) {
          GOTO(done, asm_.Word32Constant(0));
        }
        ELSE {
          IF (UNLIKELY(asm_.Word32Equal(rhs, -1))) {
            GOTO(done, asm_.Word32Constant(0));
          }
          ELSE {
            GOTO(done, asm_.Int32Mod(lhs, rhs));
          }
          END_IF
        }
        END_IF
        BIND(done, result);
        return result;
      }
      case kExprI32AsmjsRemU: {
        // asmjs semantics return 0 for mod with 0.
        Label<Word32> done(&asm_);
        IF (UNLIKELY(asm_.Word32Equal(rhs, 0))) {
          GOTO(done, asm_.Word32Constant(0));
        }
        ELSE {
          GOTO(done, asm_.Uint32Mod(lhs, rhs));
        }
        END_IF
        BIND(done, result);
        return result;
      }
      case kExprI32AsmjsStoreMem8:
        AsmjsStoreMem(lhs, rhs, MemoryRepresentation::Int8());
        return rhs;
      case kExprI32AsmjsStoreMem16:
        AsmjsStoreMem(lhs, rhs, MemoryRepresentation::Int16());
        return rhs;
      case kExprI32AsmjsStoreMem:
        AsmjsStoreMem(lhs, rhs, MemoryRepresentation::Int32());
        return rhs;
      case kExprF32AsmjsStoreMem:
        AsmjsStoreMem(lhs, rhs, MemoryRepresentation::Float32());
        return rhs;
      case kExprF64AsmjsStoreMem:
        AsmjsStoreMem(lhs, rhs, MemoryRepresentation::Float64());
        return rhs;
      default:
        UNREACHABLE();
    }
  }

  std::pair<V<WordPtr>, compiler::BoundsCheckResult> CheckBoundsAndAlignment(
      const wasm::WasmMemory* memory, MemoryRepresentation repr, OpIndex index,
      uintptr_t offset, wasm::WasmCodePosition position,
      compiler::EnforceBoundsCheck enforce_check) {
    // Atomic operations need bounds checks until the backend can emit protected
    // loads.
    compiler::BoundsCheckResult bounds_check_result;
    V<WordPtr> converted_index;
    std::tie(converted_index, bounds_check_result) =
        BoundsCheckMem(memory, repr, index, offset, enforce_check);

    const uintptr_t align_mask = repr.SizeInBytes() - 1;

    // TODO(14108): Optimize constant index as per wasm-compiler.cc.

    // Unlike regular memory accesses, atomic memory accesses should trap if
    // the effective offset is misaligned.
    // TODO(wasm): this addition is redundant with one inserted by {MemBuffer}.
    OpIndex effective_offset =
        asm_.WordPtrAdd(MemBuffer(memory->index, offset), converted_index);

    V<Word32> cond = asm_.TruncateWordPtrToWord32(asm_.WordPtrBitwiseAnd(
        effective_offset, asm_.IntPtrConstant(align_mask)));
    asm_.TrapIfNot(asm_.Word32Equal(cond, asm_.Word32Constant(0)),
                   OpIndex::Invalid(), TrapId::kTrapUnalignedAccess);
    return {converted_index, bounds_check_result};
  }

  std::pair<V<WordPtr>, compiler::BoundsCheckResult> BoundsCheckMem(
      const wasm::WasmMemory* memory, MemoryRepresentation repr, OpIndex index,
      uintptr_t offset, compiler::EnforceBoundsCheck enforce_bounds_check) {
    // The function body decoder already validated that the access is not
    // statically OOB.
    DCHECK(base::IsInBounds(offset, static_cast<uintptr_t>(repr.SizeInBytes()),
                            memory->max_memory_size));

    // Convert the index to uintptr.
    if (!memory->is_memory64) {
      index = asm_.ChangeUint32ToUintPtr(index);
    } else if (kSystemPointerSize == kInt32Size) {
      // In memory64 mode on 32-bit systems, the upper 32 bits need to be zero
      // to succeed the bounds check.
      DCHECK_NE(kTrapHandler, memory->bounds_checks);
      if (memory->bounds_checks == wasm::kExplicitBoundsChecks) {
        V<Word32> high_word = asm_.TruncateWord64ToWord32(
            asm_.Word64ShiftRightLogical(index, 32));
        asm_.TrapIf(high_word, OpIndex::Invalid(), TrapId::kTrapMemOutOfBounds);
      }
      // Truncate index to 32-bit.
      index = asm_.TruncateWord64ToWord32(index);
    }

    //  If no bounds checks should be performed (for testing), just return the
    // converted index and assume it to be in-bounds.
    if (memory->bounds_checks == wasm::kNoBoundsChecks) {
      return {index, compiler::BoundsCheckResult::kInBounds};
    }

    // TODO(14108): Optimize constant index as per wasm-compiler.cc.

    if (memory->bounds_checks == kTrapHandler &&
        enforce_bounds_check ==
            compiler::EnforceBoundsCheck::kCanOmitBoundsCheck) {
      return {index, compiler::BoundsCheckResult::kTrapHandler};
    }

    uintptr_t end_offset = offset + repr.SizeInBytes() - 1u;

    V<WordPtr> memory_size = MemSize(memory->index);
    if (end_offset > memory->min_memory_size) {
      // The end offset is larger than the smallest memory.
      // Dynamically check the end offset against the dynamic memory size.
      asm_.TrapIfNot(
          asm_.UintPtrLessThan(asm_.UintPtrConstant(end_offset), memory_size),
          OpIndex::Invalid(), TrapId::kTrapMemOutOfBounds);
    }

    // This produces a positive number since {end_offset <= min_size <=
    // mem_size}.
    V<WordPtr> effective_size = asm_.WordPtrSub(memory_size, end_offset);
    asm_.TrapIfNot(asm_.UintPtrLessThan(index, effective_size),
                   OpIndex::Invalid(), TrapId::kTrapMemOutOfBounds);
    return {index, compiler::BoundsCheckResult::kDynamicallyChecked};
  }

  V<WordPtr> MemStart(uint32_t index) {
    if (index == 0) {
      return LOAD_INSTANCE_FIELD(Memory0Start, kMaybeSandboxedPointer);
    } else {
      V<ByteArray> instance_memories = LOAD_INSTANCE_FIELD(
          MemoryBasesAndSizes, MemoryRepresentation::TaggedPointer());
      return asm_.Load(instance_memories, LoadOp::Kind::TaggedBase(),
                       kMaybeSandboxedPointer,
                       ByteArray::kHeaderSize +
                           2 * index * kMaybeSandboxedPointer.SizeInBytes());
    }
  }

  V<WordPtr> MemBuffer(uint32_t mem_index, uintptr_t offset) {
    V<WordPtr> mem_start = MemStart(mem_index);
    if (offset == 0) return mem_start;
    return asm_.WordPtrAdd(mem_start, offset);
  }

  V<WordPtr> MemSize(uint32_t index) {
    if (index == 0) {
      return LOAD_INSTANCE_FIELD(Memory0Size,
                                 MemoryRepresentation::PointerSized());
    } else {
      V<ByteArray> instance_memories = LOAD_INSTANCE_FIELD(
          MemoryBasesAndSizes, MemoryRepresentation::TaggedPointer());
      return asm_.Load(
          instance_memories, LoadOp::Kind::TaggedBase(),
          MemoryRepresentation::PointerSized(),
          ByteArray::kHeaderSize + (2 * index + 1) * kSystemPointerSize);
    }
  }

  LoadOp::Kind GetMemoryAccessKind(
      MemoryRepresentation repr,
      compiler::BoundsCheckResult bounds_check_result) {
    if (bounds_check_result == compiler::BoundsCheckResult::kTrapHandler) {
      DCHECK(repr == MemoryRepresentation::Int8() ||
             repr == MemoryRepresentation::Uint8() ||
             SupportedOperations::IsUnalignedLoadSupported(repr));
      return LoadOp::Kind::Protected();
    } else if (repr != MemoryRepresentation::Int8() &&
               repr != MemoryRepresentation::Uint8() &&
               !SupportedOperations::IsUnalignedLoadSupported(repr)) {
      return LoadOp::Kind::RawUnaligned();
    } else {
      return LoadOp::Kind::RawAligned();
    }
  }

  void TraceMemoryOperation(bool is_store, MemoryRepresentation repr,
                            V<WordPtr> index, uintptr_t offset) {
    int kAlign = 4;  // Ensure that the LSB is 0, like a Smi.
    V<WordPtr> info = asm_.StackSlot(sizeof(MemoryTracingInfo), kAlign);
    V<WordPtr> effective_offset = asm_.WordPtrAdd(index, offset);
    asm_.Store(info, effective_offset, StoreOp::Kind::RawAligned(),
               MemoryRepresentation::PointerSized(), compiler::kNoWriteBarrier,
               offsetof(MemoryTracingInfo, offset));
    asm_.Store(info, asm_.Word32Constant(is_store ? 1 : 0),
               StoreOp::Kind::RawAligned(), MemoryRepresentation::Uint8(),
               compiler::kNoWriteBarrier,
               offsetof(MemoryTracingInfo, is_store));
    V<Word32> rep_as_int = asm_.Word32Constant(
        static_cast<int>(repr.ToMachineType().representation()));
    asm_.Store(info, rep_as_int, StoreOp::Kind::RawAligned(),
               MemoryRepresentation::Uint8(), compiler::kNoWriteBarrier,
               offsetof(MemoryTracingInfo, mem_rep));
    CallRuntime(Runtime::kWasmTraceMemory, {info});
  }

  void StackCheck() {
    if (V8_UNLIKELY(!v8_flags.wasm_stack_checks)) return;
    V<WordPtr> limit_address = LOAD_INSTANCE_FIELD(
        StackLimitAddress, MemoryRepresentation::PointerSized());
    V<WordPtr> limit = asm_.Load(limit_address, LoadOp::Kind::RawAligned(),
                                 MemoryRepresentation::PointerSized(), 0);
    V<Word32> check =
        asm_.StackPointerGreaterThan(limit, compiler::StackCheckKind::kWasm);
    TSBlock* continuation = asm_.NewBlock();
    TSBlock* call_builtin = asm_.NewBlock();
    asm_.Branch(ConditionWithHint(check, BranchHint::kTrue), continuation,
                call_builtin);

    // TODO(14108): Cache descriptor.
    asm_.Bind(call_builtin);
    V<WordPtr> builtin = asm_.RelocatableConstant(WasmCode::kWasmStackGuard,
                                                  RelocInfo::WASM_STUB_CALL);
    const CallDescriptor* call_descriptor =
        compiler::Linkage::GetStubCallDescriptor(
            asm_.graph_zone(),                    // zone
            NoContextDescriptor{},                // descriptor
            0,                                    // stack parameter count
            CallDescriptor::kNoFlags,             // flags
            compiler::Operator::kNoProperties,    // properties
            StubCallMode::kCallWasmRuntimeStub);  // stub call mode
    const TSCallDescriptor* ts_call_descriptor = TSCallDescriptor::Create(
        call_descriptor, compiler::CanThrow::kNo, asm_.graph_zone());
    asm_.Call(builtin, {}, ts_call_descriptor);
    asm_.Goto(continuation);

    asm_.Bind(continuation);
  }

  std::pair<V<WordPtr>, V<HeapObject>> BuildImportedFunctionTargetAndRef(
      uint32_t function_index) {
    // Imported function.
    V<WordPtr> func_index = asm_.IntPtrConstant(function_index);
    V<FixedArray> imported_function_refs = LOAD_INSTANCE_FIELD(
        ImportedFunctionRefs, MemoryRepresentation::TaggedPointer());
    auto ref = V<HeapObject>::Cast(
        LoadFixedArrayElement(imported_function_refs, func_index));
    V<FixedAddressArray> imported_targets = LOAD_INSTANCE_FIELD(
        ImportedFunctionTargets, MemoryRepresentation::TaggedPointer());
    V<WordPtr> target =
        asm_.Load(imported_targets, func_index, LoadOp::Kind::TaggedBase(),
                  MemoryRepresentation::PointerSized(),
                  FixedAddressArray::kHeaderSize, kSystemPointerSizeLog2);
    return {target, ref};
  }

  std::pair<V<WordPtr>, V<HeapObject>> BuildIndirectCallTargetAndRef(
      FullDecoder* decoder, OpIndex index, CallIndirectImmediate imm) {
    uint32_t table_index = imm.table_imm.index;
    const WasmTable& table = decoder->module_->tables[table_index];
    V<WordPtr> index_intptr = asm_.ChangeInt32ToIntPtr(index);
    uint32_t sig_index = imm.sig_imm.index;

    /* Step 1: Load the indirect function tables for this table. */
    bool needs_dynamic_size =
        !(table.has_maximum_size && table.maximum_size == table.initial_size);
    V<Word32> ift_size;
    V<ByteArray> ift_sig_ids;
    V<ExternalPointerArray> ift_targets;
    V<FixedArray> ift_refs;
    if (table_index == 0) {
      ift_size = needs_dynamic_size
                     ? LOAD_INSTANCE_FIELD(IndirectFunctionTableSize,
                                           MemoryRepresentation::Uint32())
                     : asm_.Word32Constant(table.initial_size);
      ift_sig_ids = LOAD_INSTANCE_FIELD(IndirectFunctionTableSigIds,
                                        MemoryRepresentation::TaggedPointer());
      ift_targets = LOAD_INSTANCE_FIELD(IndirectFunctionTableTargets,
                                        MemoryRepresentation::TaggedPointer());
      ift_refs = LOAD_INSTANCE_FIELD(IndirectFunctionTableRefs,
                                     MemoryRepresentation::TaggedPointer());
    } else {
      V<FixedArray> ift_tables = LOAD_INSTANCE_FIELD(
          IndirectFunctionTables, MemoryRepresentation::TaggedPointer());
      OpIndex ift_table = LoadFixedArrayElement(ift_tables, table_index);
      ift_size = needs_dynamic_size
                     ? asm_.Load(ift_table, LoadOp::Kind::TaggedBase(),
                                 MemoryRepresentation::Uint32(),
                                 WasmIndirectFunctionTable::kSizeOffset)
                     : asm_.Word32Constant(table.initial_size);
      ift_sig_ids = asm_.Load(ift_table, LoadOp::Kind::TaggedBase(),
                              MemoryRepresentation::TaggedPointer(),
                              WasmIndirectFunctionTable::kSigIdsOffset);
      ift_targets = asm_.Load(ift_table, LoadOp::Kind::TaggedBase(),
                              MemoryRepresentation::TaggedPointer(),
                              WasmIndirectFunctionTable::kTargetsOffset);
      ift_refs = asm_.Load(ift_table, LoadOp::Kind::TaggedBase(),
                           MemoryRepresentation::TaggedPointer(),
                           WasmIndirectFunctionTable::kRefsOffset);
    }

    /* Step 2: Bounds check against the table size. */
    asm_.TrapIfNot(asm_.Uint32LessThan(index, ift_size), OpIndex::Invalid(),
                   TrapId::kTrapTableOutOfBounds);

    /* Step 3: Check the canonical real signature against the canonical declared
     * signature. */
    bool needs_type_check =
        !EquivalentTypes(table.type.AsNonNull(), ValueType::Ref(sig_index),
                         decoder->module_, decoder->module_);
    bool needs_null_check = table.type.is_nullable();

    if (needs_type_check) {
      V<WordPtr> isorecursive_canonical_types = LOAD_INSTANCE_FIELD(
          IsorecursiveCanonicalTypes, MemoryRepresentation::PointerSized());
      V<Word32> expected_sig_id =
          asm_.Load(isorecursive_canonical_types, LoadOp::Kind::RawAligned(),
                    MemoryRepresentation::Uint32(), sig_index * kUInt32Size);
      V<Word32> loaded_sig =
          asm_.Load(ift_sig_ids, index_intptr, LoadOp::Kind::TaggedBase(),
                    MemoryRepresentation::Uint32(), ByteArray::kHeaderSize,
                    2 /* kInt32SizeLog2 */);
      if (decoder->enabled_.has_gc() &&
          !decoder->module_->types[sig_index].is_final) {
        // In this case, a full null check and type check is needed.
        Bailout(decoder);
        return {};
      } else {
        // In this case, signatures must match exactly.
        asm_.TrapIfNot(asm_.Word32Equal(expected_sig_id, loaded_sig),
                       OpIndex::Invalid(), TrapId::kTrapFuncSigMismatch);
      }
    } else if (needs_null_check) {
      V<Word32> loaded_sig =
          asm_.Load(ift_sig_ids, index_intptr, LoadOp::Kind::TaggedBase(),
                    MemoryRepresentation::Uint32(), ByteArray::kHeaderSize,
                    2 /* kInt32SizeLog2 */);
      asm_.TrapIf(asm_.Word32Equal(-1, loaded_sig), OpIndex::Invalid(),
                  TrapId::kTrapFuncSigMismatch);
    }

    /* Step 4: Extract ref and target. */
#ifdef V8_ENABLE_SANDBOX
    V<Word32> external_pointer_handle = asm_.Load(
        ift_targets, index_intptr, LoadOp::Kind::TaggedBase(),
        MemoryRepresentation::Uint32(), ExternalPointerArray::kHeaderSize, 2);
    V<WordPtr> target = BuildDecodeExternalPointer(
        external_pointer_handle, kWasmIndirectFunctionTargetTag);
#else
    V<WordPtr> target =
        asm_.Load(ift_targets, index_intptr, LoadOp::Kind::TaggedBase(),
                  MemoryRepresentation::PointerSized(),
                  ExternalPointerArray::kHeaderSize, kSystemPointerSizeLog2);
#endif
    auto ref =
        V<HeapObject>::Cast(LoadFixedArrayElement(ift_refs, index_intptr));
    return {target, ref};
  }

  std::pair<V<WordPtr>, V<HeapObject>> BuildFunctionReferenceTargetAndRef(
      V<HeapObject> func_ref, ValueType type) {
    if (type.is_nullable() &&
        null_check_strategy_ == compiler::NullCheckStrategy::kExplicit) {
      func_ref = V<HeapObject>::Cast(
          asm_.AssertNotNull(func_ref, type, TrapId::kTrapNullDereference));
    }

    V<HeapObject> ref =
        type.is_nullable() && null_check_strategy_ ==
                                  compiler::NullCheckStrategy::kTrapHandler
            ? asm_.Load(func_ref, LoadOp::Kind::TrapOnNull(),
                        MemoryRepresentation::TaggedPointer(),
                        WasmInternalFunction::kRefOffset)
            : asm_.Load(func_ref, LoadOp::Kind::TaggedBase(),
                        MemoryRepresentation::TaggedPointer(),
                        WasmInternalFunction::kRefOffset);

#ifdef V8_ENABLE_SANDBOX
    V<Word32> target_handle = asm_.Load(
        func_ref, LoadOp::Kind::TaggedBase(), MemoryRepresentation::Uint32(),
        WasmInternalFunction::kCallTargetOffset);
    V<WordPtr> target = BuildDecodeExternalPointer(
        target_handle, kWasmInternalFunctionCallTargetTag);
#else
    V<WordPtr> target = asm_.Load(func_ref, LoadOp::Kind::TaggedBase(),
                                  MemoryRepresentation::PointerSized(),
                                  WasmInternalFunction::kCallTargetOffset);
#endif
    Label<WordPtr> done(&asm_);
    // For wasm functions, we have a cached handle to a pointer to off-heap
    // code. For WasmJSFunctions we used not to be able to cache this handle
    // because the code was on-heap, so we needed to fetch it every time from
    // the on-heap Code object. However now, when code pointer sandboxing is on,
    // the on-heap Code object also has a handle to off-heap code, so we could
    // probably also cache that somehow.
    // TODO(manoskouk): Figure out how to improve the situation.
    IF (UNLIKELY(asm_.WordPtrEqual(target, 0))) {
      V<Code> wrapper_code = asm_.Load(func_ref, LoadOp::Kind::TaggedBase(),
                                       MemoryRepresentation::TaggedPointer(),
                                       WasmInternalFunction::kCodeOffset);
#ifdef V8_CODE_POINTER_SANDBOXING
      V<Word32> call_target_handle = asm_.Load(
          wrapper_code, LoadOp::Kind::TaggedBase(),
          MemoryRepresentation::Uint32(), Code::kInstructionStartOffset);
      V<WordPtr> call_target =
          BuildDecodeExternalCodePointer(call_target_handle);
#else
      V<WordPtr> call_target = asm_.Load(
          wrapper_code, LoadOp::Kind::TaggedBase(),
          MemoryRepresentation::PointerSized(), Code::kInstructionStartOffset);
#endif
      GOTO(done, call_target);
    }
    ELSE {
      GOTO(done, target);
    }
    END_IF

    BIND(done, final_target);

    return {final_target, ref};
  }

  void BuildWasmCall(FullDecoder* decoder, const FunctionSig* sig,
                     V<WordPtr> callee, V<HeapObject> ref, const Value args[],
                     Value returns[]) {
    const TSCallDescriptor* descriptor = TSCallDescriptor::Create(
        compiler::GetWasmCallDescriptor(asm_.graph_zone(), sig),
        compiler::CanThrow::kYes, asm_.graph_zone());

    std::vector<OpIndex> arg_indices(sig->parameter_count() + 1);
    arg_indices[0] = ref;
    for (uint32_t i = 0; i < sig->parameter_count(); i++) {
      arg_indices[i + 1] = args[i].op;
    }

    OpIndex call = CallAndMaybeCatchException(
        decoder, callee, base::VectorOf(arg_indices), descriptor);

    if (sig->return_count() == 1) {
      returns[0].op = call;
    } else if (sig->return_count() > 1) {
      for (uint32_t i = 0; i < sig->return_count(); i++) {
        returns[i].op =
            asm_.Projection(call, i, RepresentationFor(sig->GetReturn(i)));
      }
    }
  }

  void BuildWasmReturnCall(const FunctionSig* sig, V<WordPtr> callee,
                           V<HeapObject> ref, const Value args[]) {
    const TSCallDescriptor* descriptor = TSCallDescriptor::Create(
        compiler::GetWasmCallDescriptor(asm_.graph_zone(), sig),
        compiler::CanThrow::kYes, asm_.graph_zone());

    base::SmallVector<OpIndex, 8> arg_indices(sig->parameter_count() + 1);
    arg_indices[0] = ref;
    for (uint32_t i = 0; i < sig->parameter_count(); i++) {
      arg_indices[i + 1] = args[i].op;
    }

    asm_.TailCall(callee, base::VectorOf(arg_indices), descriptor);
  }

  OpIndex CallBuiltinFromRuntimeStub(
      FullDecoder* decoder, WasmCode::RuntimeStubId stub_id,
      base::Vector<const OpIndex> args,
      CheckForException check_for_exception = CheckForException::kNo) {
    Builtin builtin_name = RuntimeStubIdToBuiltinName(stub_id);
    CallInterfaceDescriptor interface_descriptor =
        Builtins::CallInterfaceDescriptorFor(builtin_name);
    const CallDescriptor* call_descriptor =
        compiler::Linkage::GetStubCallDescriptor(
            asm_.graph_zone(), interface_descriptor,
            interface_descriptor.GetStackParameterCount(),
            CallDescriptor::kNoFlags, compiler::Operator::kNoProperties,
            StubCallMode::kCallWasmRuntimeStub);
    const TSCallDescriptor* ts_call_descriptor = TSCallDescriptor::Create(
        call_descriptor, compiler::CanThrow::kYes, asm_.graph_zone());
    V<WordPtr> call_target =
        asm_.RelocatableConstant(stub_id, RelocInfo::WASM_STUB_CALL);
    return check_for_exception == CheckForException::kYes
               ? CallAndMaybeCatchException(decoder, call_target, args,
                                            ts_call_descriptor)
               : asm_.Call(call_target, OpIndex::Invalid(), args,
                           ts_call_descriptor);
  }

  OpIndex CallBuiltinFromRuntimeStub(
      FullDecoder* decoder, WasmCode::RuntimeStubId stub_id,
      std::initializer_list<OpIndex> args,
      CheckForException check_for_exception = CheckForException::kNo) {
    return CallBuiltinFromRuntimeStub(decoder, stub_id, base::VectorOf(args),
                                      check_for_exception);
  }

  OpIndex CallAndMaybeCatchException(FullDecoder* decoder, V<WordPtr> callee,
                                     base::Vector<const OpIndex> args,
                                     const TSCallDescriptor* descriptor) {
    if (decoder->current_catch() == -1) {
      return asm_.Call(callee, OpIndex::Invalid(), args, descriptor);
    }

    Control* current_catch =
        decoder->control_at(decoder->control_depth_of_current_catch());
    TSBlock* catch_block = current_catch->false_or_loop_or_catch_block;
    TSBlock* success_block = asm_.NewBlock();
    TSBlock* exception_block = asm_.NewBlock();
    OpIndex call;
    {
      decltype(asm_)::CatchScope scope(asm_, exception_block);

      call = asm_.Call(callee, OpIndex::Invalid(), args, descriptor);
      asm_.Goto(success_block);
    }

    asm_.Bind(exception_block);
    OpIndex exception = asm_.CatchBlockBegin();
    SetupControlFlowEdge(decoder, catch_block, 0, exception);
    asm_.Goto(catch_block);

    asm_.Bind(success_block);
    return call;
  }

  OpIndex CallRuntime(Runtime::FunctionId f,
                      std::initializer_list<const OpIndex> args) {
    const Runtime::Function* fun = Runtime::FunctionForId(f);
    OpIndex isolate_root = asm_.LoadRootRegister();
    DCHECK_EQ(1, fun->result_size);
    int builtin_slot_offset = IsolateData::BuiltinSlotOffset(
        Builtin::kCEntry_Return1_ArgvOnStack_NoBuiltinExit);
    OpIndex centry_stub =
        asm_.Load(isolate_root, LoadOp::Kind::RawAligned(),
                  MemoryRepresentation::PointerSized(), builtin_slot_offset);
    base::SmallVector<OpIndex, 8> centry_args;
    for (OpIndex arg : args) centry_args.emplace_back(arg);
    centry_args.emplace_back(
        asm_.ExternalConstant(ExternalReference::Create(f)));
    centry_args.emplace_back(asm_.Word32Constant(fun->nargs));
    centry_args.emplace_back(asm_.NoContextConstant());  // js_context
    const CallDescriptor* call_descriptor =
        compiler::Linkage::GetRuntimeCallDescriptor(
            asm_.graph_zone(), f, fun->nargs, compiler::Operator::kNoProperties,
            CallDescriptor::kNoFlags);
    const TSCallDescriptor* ts_call_descriptor = TSCallDescriptor::Create(
        call_descriptor, compiler::CanThrow::kYes, asm_.graph_zone());
    return asm_.Call(centry_stub, OpIndex::Invalid(),
                     base::VectorOf(centry_args), ts_call_descriptor);
  }

  OpIndex CallC(const MachineSignature* sig, ExternalReference ref,
                std::initializer_list<OpIndex> args) {
    DCHECK_LE(sig->return_count(), 1);
    const CallDescriptor* call_descriptor =
        compiler::Linkage::GetSimplifiedCDescriptor(asm_.graph_zone(), sig);
    const TSCallDescriptor* ts_call_descriptor = TSCallDescriptor::Create(
        call_descriptor, compiler::CanThrow::kNo, asm_.graph_zone());
    return asm_.Call(asm_.ExternalConstant(ref), OpIndex::Invalid(),
                     base::VectorOf(args), ts_call_descriptor);
  }

  OpIndex CallC(const MachineSignature* sig, ExternalReference ref,
                OpIndex arg) {
    return CallC(sig, ref, {arg});
  }

  OpIndex CallCStackSlotToInt32(OpIndex arg, ExternalReference ref,
                                MemoryRepresentation arg_type) {
    OpIndex stack_slot_param =
        asm_.StackSlot(arg_type.SizeInBytes(), arg_type.SizeInBytes());
    asm_.Store(stack_slot_param, arg, StoreOp::Kind::RawAligned(), arg_type,
               compiler::WriteBarrierKind::kNoWriteBarrier);
    MachineType reps[]{MachineType::Int32(), MachineType::Pointer()};
    MachineSignature sig(1, 1, reps);
    return CallC(&sig, ref, stack_slot_param);
  }

  V<Word32> CallCStackSlotToInt32(
      ExternalReference ref,
      std::initializer_list<std::pair<OpIndex, MemoryRepresentation>> args) {
    int slot_size = 0;
    for (auto arg : args) slot_size += arg.second.SizeInBytes();
    // Since we are storing the arguments unaligned anyway, we do not need
    // alignment > 0.
    V<WordPtr> stack_slot_param = asm_.StackSlot(slot_size, 0);
    int offset = 0;
    for (auto arg : args) {
      asm_.Store(stack_slot_param, arg.first, StoreOp::Kind::RawUnaligned(),
                 arg.second, compiler::WriteBarrierKind::kNoWriteBarrier,
                 offset);
      offset += arg.second.SizeInBytes();
    }
    MachineType reps[]{MachineType::Int32(), MachineType::Pointer()};
    MachineSignature sig(1, 1, reps);
    return CallC(&sig, ref, stack_slot_param);
  }

  OpIndex CallCStackSlotToStackSlot(OpIndex arg, ExternalReference ref,
                                    MemoryRepresentation arg_type) {
    V<WordPtr> stack_slot =
        asm_.StackSlot(arg_type.SizeInBytes(), arg_type.SizeInBytes());
    asm_.Store(stack_slot, arg, StoreOp::Kind::RawAligned(), arg_type,
               compiler::WriteBarrierKind::kNoWriteBarrier);
    MachineType reps[]{MachineType::Pointer()};
    MachineSignature sig(0, 1, reps);
    CallC(&sig, ref, stack_slot);
    return asm_.Load(stack_slot, LoadOp::Kind::RawAligned(), arg_type);
  }

  OpIndex CallCStackSlotToStackSlot(OpIndex arg0, OpIndex arg1,
                                    ExternalReference ref,
                                    MemoryRepresentation arg_type) {
    V<WordPtr> stack_slot =
        asm_.StackSlot(2 * arg_type.SizeInBytes(), arg_type.SizeInBytes());
    asm_.Store(stack_slot, arg0, StoreOp::Kind::RawAligned(), arg_type,
               compiler::WriteBarrierKind::kNoWriteBarrier);
    asm_.Store(stack_slot, arg1, StoreOp::Kind::RawAligned(), arg_type,
               compiler::WriteBarrierKind::kNoWriteBarrier,
               arg_type.SizeInBytes());
    MachineType reps[]{MachineType::Pointer()};
    MachineSignature sig(0, 1, reps);
    CallC(&sig, ref, stack_slot);
    return asm_.Load(stack_slot, LoadOp::Kind::RawAligned(), arg_type);
  }

  V<WordPtr> MemoryIndexToUintPtrOrOOBTrap(bool memory_is_64, OpIndex index) {
    if (!memory_is_64) return asm_.ChangeUint32ToUintPtr(index);
    if (kSystemPointerSize == kInt64Size) return index;
    asm_.TrapIf(
        asm_.TruncateWord64ToWord32(asm_.Word64ShiftRightLogical(index, 32)),
        OpIndex::Invalid(), TrapId::kTrapMemOutOfBounds);
    return OpIndex(asm_.TruncateWord64ToWord32(index));
  }

  V<Smi> ChangeUint31ToSmi(V<Word32> value) {
    if constexpr (COMPRESS_POINTERS_BOOL) {
      return V<Smi>::Cast(
          asm_.Word32ShiftLeft(value, kSmiShiftSize + kSmiTagSize));
    } else {
      return V<Smi>::Cast(asm_.WordPtrShiftLeft(
          asm_.ChangeUint32ToUintPtr(value), kSmiShiftSize + kSmiTagSize));
    }
  }

  V<Word32> ChangeSmiToUint32(V<Smi> value) {
    if constexpr (COMPRESS_POINTERS_BOOL) {
      return asm_.Word32ShiftRightLogical(V<Word32>::Cast(value),
                                          kSmiShiftSize + kSmiTagSize);
    } else {
      return asm_.TruncateWordPtrToWord32(asm_.WordPtrShiftRightLogical(
          V<WordPtr>::Cast(value), kSmiShiftSize + kSmiTagSize));
    }
  }

  V<WordPtr> BuildDecodeExternalPointer(V<Word32> handle,
                                        ExternalPointerTag tag) {
#ifdef V8_ENABLE_SANDBOX
    // Decode loaded external pointer.
    V<WordPtr> isolate_root = asm_.LoadRootRegister();
    DCHECK(!IsSharedExternalPointerType(tag));
    V<WordPtr> table =
        asm_.Load(isolate_root, LoadOp::Kind::RawAligned(),
                  MemoryRepresentation::PointerSized(),
                  IsolateData::external_pointer_table_offset() +
                      Internals::kExternalPointerTableBasePointerOffset);
    V<Word32> index =
        asm_.Word32ShiftRightLogical(handle, kExternalPointerIndexShift);
    V<WordPtr> pointer =
        asm_.LoadOffHeap(table, asm_.ChangeUint32ToUint64(index), 0,
                         MemoryRepresentation::PointerSized());
    pointer = asm_.Word64BitwiseAnd(pointer, asm_.Word64Constant(~tag));
    return pointer;
#else   // V8_ENABLE_SANDBOX
    UNREACHABLE();
#endif  // V8_ENABLE_SANDBOX
  }

  V<WordPtr> BuildDecodeExternalCodePointer(V<Word32> handle) {
#ifdef V8_CODE_POINTER_SANDBOXING
    V<Word32> index =
        asm_.Word32ShiftRightLogical(handle, kCodePointerHandleShift);
    V<WordPtr> offset = asm_.ChangeUint32ToUintPtr(
        asm_.Word32ShiftLeft(index, kCodePointerTableEntrySizeLog2));
    V<WordPtr> table =
        asm_.ExternalConstant(ExternalReference::code_pointer_table_address());
    return asm_.Load(table, offset, LoadOp::Kind::RawAligned(),
                     MemoryRepresentation::PointerSized());
#else
    UNREACHABLE();
#endif
  }

  void BuildEncodeException32BitValue(V<FixedArray> values_array,
                                      uint32_t index, V<Word32> value) {
    V<Smi> upper_half =
        ChangeUint31ToSmi(asm_.Word32ShiftRightLogical(value, 16));
    StoreFixedArrayElement(values_array, index, upper_half,
                           compiler::kNoWriteBarrier);
    V<Smi> lower_half =
        ChangeUint31ToSmi(asm_.Word32BitwiseAnd(value, 0xffffu));
    StoreFixedArrayElement(values_array, index + 1, lower_half,
                           compiler::kNoWriteBarrier);
  }

  V<Word32> BuildDecodeException32BitValue(V<FixedArray> exception_values_array,
                                           int index) {
    V<Word32> upper_half = asm_.Word32ShiftLeft(
        ChangeSmiToUint32(
            V<Smi>::Cast(LoadFixedArrayElement(exception_values_array, index))),
        16);
    V<Word32> lower_half = ChangeSmiToUint32(
        V<Smi>::Cast(LoadFixedArrayElement(exception_values_array, index + 1)));
    return asm_.Word32BitwiseOr(upper_half, lower_half);
  }

  V<Word64> BuildDecodeException64BitValue(V<FixedArray> exception_values_array,
                                           int index) {
    V<Word64> upper_half = asm_.Word64ShiftLeft(
        asm_.ChangeUint32ToUint64(
            BuildDecodeException32BitValue(exception_values_array, index)),
        32);
    V<Word64> lower_half = asm_.ChangeUint32ToUint64(
        BuildDecodeException32BitValue(exception_values_array, index + 2));
    return asm_.Word64BitwiseOr(upper_half, lower_half);
  }

  void UnpackWasmException(FullDecoder* decoder, V<Tagged> exception,
                           base::Vector<Value> values) {
    V<FixedArray> exception_values_array = CallBuiltinFromRuntimeStub(
        decoder, WasmCode::kWasmGetOwnProperty,
        {exception, LOAD_ROOT(wasm_exception_values_symbol),
         LOAD_INSTANCE_FIELD(NativeContext,
                             MemoryRepresentation::TaggedPointer())});

    int index = 0;
    for (Value& value : values) {
      switch (value.type.kind()) {
        case kI32:
          value.op =
              BuildDecodeException32BitValue(exception_values_array, index);
          index += 2;
          break;
        case kI64:
          value.op =
              BuildDecodeException64BitValue(exception_values_array, index);
          index += 4;
          break;
        case kF32:
          value.op = asm_.BitcastWord32ToFloat32(
              BuildDecodeException32BitValue(exception_values_array, index));
          index += 2;
          break;
        case kF64:
          value.op = asm_.BitcastWord64ToFloat64(
              BuildDecodeException64BitValue(exception_values_array, index));
          index += 4;
          break;
        case kS128: {
          value.op = asm_.Simd128Splat(
              BuildDecodeException32BitValue(exception_values_array, index),
              compiler::turboshaft::Simd128SplatOp::Kind::kI32x4);
          index += 2;
          using Kind = compiler::turboshaft::Simd128ReplaceLaneOp::Kind;
          value.op = asm_.Simd128ReplaceLane(
              value.op,
              BuildDecodeException32BitValue(exception_values_array, index),
              Kind::kI32x4, 1);
          index += 2;
          value.op = asm_.Simd128ReplaceLane(
              value.op,
              BuildDecodeException32BitValue(exception_values_array, index),
              Kind::kI32x4, 2);
          index += 2;
          value.op = asm_.Simd128ReplaceLane(
              value.op,
              BuildDecodeException32BitValue(exception_values_array, index),
              Kind::kI32x4, 3);
          index += 2;
          break;
        }
        case kRtt:
        case kRef:
        case kRefNull:
          value.op = LoadFixedArrayElement(exception_values_array, index);
          index++;
          break;
        case kI8:
        case kI16:
        case kVoid:
        case kBottom:
          UNREACHABLE();
      }
    }
  }

  void AsmjsStoreMem(V<Word32> index, OpIndex value,
                     MemoryRepresentation repr) {
    // Since asmjs does not support unaligned accesses, we can bounds-check
    // ignoring the access size.
    IF (LIKELY(asm_.Uint32LessThan(index,
                                   asm_.TruncateWordPtrToWord32(MemSize(0))))) {
      asm_.Store(MemStart(0), asm_.ChangeUint32ToUintPtr(index), value,
                 StoreOp::Kind::RawAligned(), repr, compiler::kNoWriteBarrier,
                 0);
    }
    END_IF
  }

  OpIndex AsmjsLoadMem(V<Word32> index, MemoryRepresentation repr) {
    // Since asmjs does not support unaligned accesses, we can bounds-check
    // ignoring the access size.
    TSBlock* in_bounds = asm_.NewBlock();
    TSBlock* out_of_bounds = asm_.NewBlock();
    TSBlock* done = asm_.NewBlock();

    asm_.Branch(
        ConditionWithHint(asm_.Uint32LessThan(
                              index, asm_.TruncateWordPtrToWord32(MemSize(0))),
                          BranchHint::kTrue),
        in_bounds, out_of_bounds);

    asm_.Bind(in_bounds);
    OpIndex loaded_value =
        asm_.Load(MemStart(0), asm_.ChangeInt32ToIntPtr(index),
                  LoadOp::Kind::RawAligned(), repr);
    asm_.Goto(done);

    asm_.Bind(out_of_bounds);
    OpIndex default_value;
    switch (repr) {
      case MemoryRepresentation::Int8():
      case MemoryRepresentation::Int16():
      case MemoryRepresentation::Int32():
      case MemoryRepresentation::Uint8():
      case MemoryRepresentation::Uint16():
      case MemoryRepresentation::Uint32():
        default_value = asm_.Word32Constant(0);
        break;
      case MemoryRepresentation::Float32():
        default_value =
            asm_.Float32Constant(std::numeric_limits<float>::quiet_NaN());
        break;
      case MemoryRepresentation::Float64():
        default_value =
            asm_.Float64Constant(std::numeric_limits<double>::quiet_NaN());
        break;
      default:
        UNREACHABLE();
    }
    asm_.Goto(done);

    asm_.Bind(done);
    return asm_.Phi(base::VectorOf({loaded_value, default_value}),
                    repr.ToRegisterRepresentation());
  }

  void BoundsCheckArray(V<HeapObject> array, V<Word32> index,
                        ValueType array_type) {
    if (V8_UNLIKELY(v8_flags.experimental_wasm_skip_bounds_checks)) {
      if (array_type.is_nullable()) {
        asm_.AssertNotNull(array, array_type, TrapId::kTrapNullDereference);
      }
    } else {
      OpIndex length = asm_.ArrayLength(
          array, array_type.is_nullable() ? compiler::kWithNullCheck
                                          : compiler::kWithoutNullCheck);
      asm_.TrapIfNot(asm_.Uint32LessThan(index, length), OpIndex::Invalid(),
                     TrapId::kTrapArrayOutOfBounds);
    }
  }

  void BoundsCheckArrayWithLength(V<HeapObject> array, V<Word32> index,
                                  V<Word32> length,
                                  compiler::CheckForNull null_check) {
    if (V8_UNLIKELY(v8_flags.experimental_wasm_skip_bounds_checks)) return;
    V<Word32> array_length = asm_.ArrayLength(array, null_check);
    V<Word32> range_end = asm_.Word32Add(index, length);
    V<Word32> range_valid = asm_.Word32BitwiseAnd(
        // OOB if (index + length > array.len).
        asm_.Uint32LessThanOrEqual(range_end, array_length),
        // OOB if (index + length) overflows.
        asm_.Uint32LessThanOrEqual(index, range_end));
    asm_.TrapIfNot(range_valid, OpIndex::Invalid(),
                   TrapId::kTrapArrayOutOfBounds);
  }

  V<Tagged> LoadFixedArrayElement(V<FixedArray> array, int index) {
    return asm_.Load(array, LoadOp::Kind::TaggedBase(),
                     MemoryRepresentation::AnyTagged(),
                     FixedArray::kHeaderSize + index * kTaggedSize);
  }

  V<Tagged> LoadFixedArrayElement(V<FixedArray> array, V<WordPtr> index) {
    return asm_.Load(array, index, LoadOp::Kind::TaggedBase(),
                     MemoryRepresentation::AnyTagged(), FixedArray::kHeaderSize,
                     kTaggedSizeLog2);
  }

  void StoreFixedArrayElement(V<FixedArray> array, int index, V<Tagged> value,
                              compiler::WriteBarrierKind write_barrier) {
    asm_.Store(array, value, LoadOp::Kind::TaggedBase(),
               MemoryRepresentation::AnyTagged(), write_barrier,
               FixedArray::kHeaderSize + index * kTaggedSize);
  }

  void StoreFixedArrayElement(V<FixedArray> array, V<WordPtr> index,
                              V<Tagged> value,
                              compiler::WriteBarrierKind write_barrier) {
    asm_.Store(array, index, value, LoadOp::Kind::TaggedBase(),
               MemoryRepresentation::AnyTagged(), write_barrier,
               FixedArray::kHeaderSize, kTaggedSizeLog2);
  }

  void BrOnCastImpl(FullDecoder* decoder, V<Map> rtt,
                    compiler::WasmTypeCheckConfig config, const Value& object,
                    Value* value_on_branch, uint32_t br_depth,
                    bool null_succeeds) {
    OpIndex cast_succeeds = asm_.WasmTypeCheck(object.op, rtt, config);
    IF (cast_succeeds) {
      // Narrow type for the successful cast target branch.
      Forward(decoder, object, value_on_branch);
      BrOrRet(decoder, br_depth, 0);
    }
    END_IF
    // Note: Differently to below for br_on_cast_fail, we do not Forward
    // the value here to perform a TypeGuard. It can't be done here due to
    // asymmetric decoder code. A Forward here would be popped from the stack
    // and ignored by the decoder. Therefore the decoder has to call Forward
    // itself.
  }

  void BrOnCastFailImpl(FullDecoder* decoder, V<Map> rtt,
                        compiler::WasmTypeCheckConfig config,
                        const Value& object, Value* value_on_fallthrough,
                        uint32_t br_depth, bool null_succeeds) {
    OpIndex cast_succeeds = asm_.WasmTypeCheck(object.op, rtt, config);
    IF (__ Word32Equal(cast_succeeds, 0)) {
      // It is necessary in case of {null_succeeds} to forward the value.
      // This will add a TypeGuard to the non-null type (as in this case the
      // object is non-nullable).
      Forward(decoder, object, decoder->stack_value(1));
      BrOrRet(decoder, br_depth, 0);
    }
    END_IF
    // Narrow type for the successful cast fallthrough branch.
    Forward(decoder, object, value_on_fallthrough);
  }

  TrapId GetTrapIdForTrap(wasm::TrapReason reason) {
    switch (reason) {
#define TRAPREASON_TO_TRAPID(name)                                             \
  case wasm::k##name:                                                          \
    static_assert(                                                             \
        static_cast<int>(TrapId::k##name) == wasm::WasmCode::kThrowWasm##name, \
        "trap id mismatch");                                                   \
    return TrapId::k##name;
      FOREACH_WASM_TRAPREASON(TRAPREASON_TO_TRAPID)
#undef TRAPREASON_TO_TRAPID
      default:
        UNREACHABLE();
    }
  }

  OpIndex WasmPositionToOpIndex(WasmCodePosition position) {
    return OpIndex(sizeof(compiler::turboshaft::OperationStorageSlot) *
                   static_cast<int>(position));
  }

  WasmCodePosition OpIndexToWasmPosition(OpIndex index) {
    return index.valid()
               ? static_cast<WasmCodePosition>(
                     index.offset() /
                     sizeof(compiler::turboshaft::OperationStorageSlot))
               : kNoCodePosition;
  }

  Assembler& Asm() { return asm_; }

  V<WasmInstanceObject> instance_node_;
  std::unordered_map<TSBlock*, BlockPhis> block_phis_;
  Assembler asm_;
  std::vector<OpIndex> ssa_env_;
  bool did_bailout_ = false;
  compiler::NullCheckStrategy null_check_strategy_ =
      trap_handler::IsTrapHandlerEnabled() && V8_STATIC_ROOTS_BOOL
          ? compiler::NullCheckStrategy::kTrapHandler
          : compiler::NullCheckStrategy::kExplicit;
};

V8_EXPORT_PRIVATE bool BuildTSGraph(AccountingAllocator* allocator,
                                    const WasmFeatures& enabled,
                                    const WasmModule* module,
                                    WasmFeatures* detected,
                                    const FunctionBody& body, Graph& graph,
                                    compiler::NodeOriginTable* node_origins) {
  Zone zone(allocator, ZONE_NAME);
  WasmFullDecoder<Decoder::FullValidationTag, TurboshaftGraphBuildingInterface>
      decoder(&zone, module, enabled, detected, body, graph, &zone,
              node_origins);
  decoder.Decode();
  // Turboshaft runs with validation, but the function should already be
  // validated, so graph building must always succeed, unless we bailed out.
  DCHECK_IMPLIES(!decoder.ok(), decoder.interface().did_bailout());
  return decoder.ok();
}

#undef LOAD_INSTANCE_FIELD
#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::wasm
