// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/graph-builder-interface.h"

#include "src/compiler/wasm-compiler.h"
#include "src/flags/flags.h"
#include "src/handles/handles.h"
#include "src/objects/objects-inl.h"
#include "src/utils/ostreams.h"
#include "src/wasm/decoder.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/value-type.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-linkage.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes-inl.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace {

// An SsaEnv environment carries the current local variable renaming
// as well as the current effect and control dependency in the TF graph.
// It maintains a control state that tracks whether the environment
// is reachable, has reached a control end, or has been merged.
struct SsaEnv : public ZoneObject {
  enum State { kControlEnd, kUnreachable, kReached, kMerged };

  State state;
  TFNode* control;
  TFNode* effect;
  compiler::WasmInstanceCacheNodes instance_cache;
  ZoneVector<TFNode*> locals;

  SsaEnv(Zone* zone, State state, TFNode* control, TFNode* effect,
         uint32_t locals_size)
      : state(state), control(control), effect(effect), locals(zone) {
    if (locals_size > 0) locals.resize(locals_size);
  }

  SsaEnv(const SsaEnv& other) V8_NOEXCEPT = default;
  SsaEnv(SsaEnv&& other) V8_NOEXCEPT : state(other.state),
                                       control(other.control),
                                       effect(other.effect),
                                       instance_cache(other.instance_cache),
                                       locals(std::move(other.locals)) {
    other.Kill(kUnreachable);
  }

  void Kill(State new_state = kControlEnd) {
    state = new_state;
    locals.clear();
    control = nullptr;
    effect = nullptr;
    instance_cache = {};
  }
  void SetNotMerged() {
    if (state == kMerged) state = kReached;
  }
};

#define BUILD(func, ...)                                            \
  ([&] {                                                            \
    DCHECK(decoder->ok());                                          \
    return CheckForException(decoder, builder_->func(__VA_ARGS__)); \
  })()

constexpr uint32_t kNullCatch = static_cast<uint32_t>(-1);

class WasmGraphBuildingInterface {
 public:
  static constexpr Decoder::ValidateFlag validate = Decoder::kFullValidation;
  using FullDecoder = WasmFullDecoder<validate, WasmGraphBuildingInterface>;
  using CheckForNull = compiler::WasmGraphBuilder::CheckForNull;

  struct Value : public ValueBase<validate> {
    TFNode* node = nullptr;

    template <typename... Args>
    explicit Value(Args&&... args) V8_NOEXCEPT
        : ValueBase(std::forward<Args>(args)...) {}
  };

  struct TryInfo : public ZoneObject {
    SsaEnv* catch_env;
    TFNode* exception = nullptr;

    bool might_throw() const { return exception != nullptr; }

    MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(TryInfo);

    explicit TryInfo(SsaEnv* c) : catch_env(c) {}
  };

  struct Control : public ControlBase<Value, validate> {
    SsaEnv* end_env = nullptr;    // end environment for the construct.
    SsaEnv* false_env = nullptr;  // false environment (only for if).
    TryInfo* try_info = nullptr;  // information about try statements.
    int32_t previous_catch = -1;  // previous Control with a catch.

    MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(Control);

    template <typename... Args>
    explicit Control(Args&&... args) V8_NOEXCEPT
        : ControlBase(std::forward<Args>(args)...) {}
  };

  explicit WasmGraphBuildingInterface(compiler::WasmGraphBuilder* builder)
      : builder_(builder) {}

  void StartFunction(FullDecoder* decoder) {
    // The first '+ 1' is needed by TF Start node, the second '+ 1' is for the
    // instance parameter.
    TFNode* start = builder_->Start(
        static_cast<int>(decoder->sig_->parameter_count() + 1 + 1));
    uint32_t num_locals = decoder->num_locals();
    SsaEnv* ssa_env = decoder->zone()->New<SsaEnv>(
        decoder->zone(), SsaEnv::kReached, start, start, num_locals);

    // Initialize effect and control before initializing the locals default
    // values (which might require instance loads) or loading the context.
    builder_->SetEffectControl(start);
    // Initialize the instance parameter (index 0).
    builder_->set_instance_node(builder_->Param(kWasmInstanceParameterIndex));
    // Initialize local variables. Parameters are shifted by 1 because of the
    // the instance parameter.
    uint32_t index = 0;
    for (; index < decoder->sig_->parameter_count(); ++index) {
      ssa_env->locals[index] = builder_->Param(index + 1);
    }
    while (index < num_locals) {
      ValueType type = decoder->local_type(index);
      TFNode* node = DefaultValue(type);
      while (index < num_locals && decoder->local_type(index) == type) {
        // Do a whole run of like-typed locals at a time.
        ssa_env->locals[index++] = node;
      }
    }
    SetEnv(ssa_env);
    LoadContextIntoSsa(ssa_env);

    if (FLAG_trace_wasm) BUILD(TraceFunctionEntry, decoder->position());
  }

  // Reload the instance cache entries into the Ssa Environment.
  void LoadContextIntoSsa(SsaEnv* ssa_env) {
    if (ssa_env) builder_->InitInstanceCache(&ssa_env->instance_cache);
  }

  void StartFunctionBody(FullDecoder* decoder, Control* block) {}

  void FinishFunction(FullDecoder*) { builder_->PatchInStackCheckIfNeeded(); }

  void OnFirstError(FullDecoder*) {}

  void NextInstruction(FullDecoder*, WasmOpcode) {}

  void Block(FullDecoder* decoder, Control* block) {
    // The branch environment is the outer environment.
    block->end_env = ssa_env_;
    SetEnv(Steal(decoder->zone(), ssa_env_));
  }

  void Loop(FullDecoder* decoder, Control* block) {
    SsaEnv* finish_try_env = Steal(decoder->zone(), ssa_env_);
    block->end_env = finish_try_env;
    SetEnv(finish_try_env);
    // The continue environment is the inner environment.
    PrepareForLoop(decoder);
    ssa_env_->SetNotMerged();
    if (!decoder->ok()) return;
    // Wrap input merge into phis.
    for (uint32_t i = 0; i < block->start_merge.arity; ++i) {
      Value& val = block->start_merge[i];
      TFNode* inputs[] = {val.node, block->end_env->control};
      val.node = builder_->Phi(val.type, 1, inputs);
    }
  }

  void Try(FullDecoder* decoder, Control* block) {
    SsaEnv* outer_env = ssa_env_;
    SsaEnv* catch_env = Split(decoder->zone(), outer_env);
    // Mark catch environment as unreachable, since only accessable
    // through catch unwinding (i.e. landing pads).
    catch_env->state = SsaEnv::kUnreachable;
    SsaEnv* try_env = Steal(decoder->zone(), outer_env);
    SetEnv(try_env);
    TryInfo* try_info = decoder->zone()->New<TryInfo>(catch_env);
    block->end_env = outer_env;
    block->try_info = try_info;
    block->previous_catch = current_catch_;
    current_catch_ = static_cast<int32_t>(decoder->control_depth() - 1);
  }

  void If(FullDecoder* decoder, const Value& cond, Control* if_block) {
    TFNode* if_true = nullptr;
    TFNode* if_false = nullptr;
    BUILD(BranchNoHint, cond.node, &if_true, &if_false);
    SsaEnv* end_env = ssa_env_;
    SsaEnv* false_env = Split(decoder->zone(), ssa_env_);
    false_env->control = if_false;
    SsaEnv* true_env = Steal(decoder->zone(), ssa_env_);
    true_env->control = if_true;
    if_block->end_env = end_env;
    if_block->false_env = false_env;
    SetEnv(true_env);
  }

  void FallThruTo(FullDecoder* decoder, Control* c) {
    DCHECK(!c->is_loop());
    MergeValuesInto(decoder, c, &c->end_merge);
  }

  void PopControl(FullDecoder* decoder, Control* block) {
    // A loop just continues with the end environment. There is no merge.
    if (block->is_loop()) return;
    // Any other block falls through to the parent block.
    if (block->reachable()) FallThruTo(decoder, block);
    if (block->is_onearmed_if()) {
      // Merge the else branch into the end merge.
      SetEnv(block->false_env);
      DCHECK_EQ(block->start_merge.arity, block->end_merge.arity);
      Value* values =
          block->start_merge.arity > 0 ? &block->start_merge[0] : nullptr;
      MergeValuesInto(decoder, block, &block->end_merge, values);
    }
    // Now continue with the merged environment.
    SetEnv(block->end_env);
  }

  void EndControl(FullDecoder* decoder, Control* block) { ssa_env_->Kill(); }

  void UnOp(FullDecoder* decoder, WasmOpcode opcode, const Value& value,
            Value* result) {
    result->node = BUILD(Unop, opcode, value.node, decoder->position());
  }

  void BinOp(FullDecoder* decoder, WasmOpcode opcode, const Value& lhs,
             const Value& rhs, Value* result) {
    TFNode* node =
        BUILD(Binop, opcode, lhs.node, rhs.node, decoder->position());
    if (result) result->node = node;
  }

  void I32Const(FullDecoder* decoder, Value* result, int32_t value) {
    result->node = builder_->Int32Constant(value);
  }

  void I64Const(FullDecoder* decoder, Value* result, int64_t value) {
    result->node = builder_->Int64Constant(value);
  }

  void F32Const(FullDecoder* decoder, Value* result, float value) {
    result->node = builder_->Float32Constant(value);
  }

  void F64Const(FullDecoder* decoder, Value* result, double value) {
    result->node = builder_->Float64Constant(value);
  }

  void S128Const(FullDecoder* decoder, const Simd128Immediate<validate>& imm,
                 Value* result) {
    result->node = builder_->Simd128Constant(imm.value);
  }

  void RefNull(FullDecoder* decoder, ValueType type, Value* result) {
    result->node = builder_->RefNull();
  }

  void RefFunc(FullDecoder* decoder, uint32_t function_index, Value* result) {
    result->node = BUILD(RefFunc, function_index);
  }

  void RefAsNonNull(FullDecoder* decoder, const Value& arg, Value* result) {
    result->node = BUILD(RefAsNonNull, arg.node, decoder->position());
  }

  void Drop(FullDecoder* decoder, const Value& value) {}

  void DoReturn(FullDecoder* decoder, Vector<Value> values) {
    base::SmallVector<TFNode*, 8> nodes(values.size());
    GetNodes(nodes.begin(), values);
    if (FLAG_trace_wasm) {
      BUILD(TraceFunctionExit, VectorOf(nodes), decoder->position());
    }
    BUILD(Return, VectorOf(nodes));
  }

  void LocalGet(FullDecoder* decoder, Value* result,
                const LocalIndexImmediate<validate>& imm) {
    result->node = ssa_env_->locals[imm.index];
  }

  void LocalSet(FullDecoder* decoder, const Value& value,
                const LocalIndexImmediate<validate>& imm) {
    ssa_env_->locals[imm.index] = value.node;
  }

  void LocalTee(FullDecoder* decoder, const Value& value, Value* result,
                const LocalIndexImmediate<validate>& imm) {
    result->node = value.node;
    ssa_env_->locals[imm.index] = value.node;
  }

  void AllocateLocals(FullDecoder* decoder, Vector<Value> local_values) {
    ZoneVector<TFNode*>* locals = &ssa_env_->locals;
    locals->insert(locals->begin(), local_values.size(), nullptr);
    for (uint32_t i = 0; i < local_values.size(); i++) {
      (*locals)[i] = local_values[i].node;
    }
  }

  void DeallocateLocals(FullDecoder* decoder, uint32_t count) {
    ZoneVector<TFNode*>* locals = &ssa_env_->locals;
    locals->erase(locals->begin(), locals->begin() + count);
  }

  void GlobalGet(FullDecoder* decoder, Value* result,
                 const GlobalIndexImmediate<validate>& imm) {
    result->node = BUILD(GlobalGet, imm.index);
  }

  void GlobalSet(FullDecoder* decoder, const Value& value,
                 const GlobalIndexImmediate<validate>& imm) {
    BUILD(GlobalSet, imm.index, value.node);
  }

  void TableGet(FullDecoder* decoder, const Value& index, Value* result,
                const TableIndexImmediate<validate>& imm) {
    result->node = BUILD(TableGet, imm.index, index.node, decoder->position());
  }

  void TableSet(FullDecoder* decoder, const Value& index, const Value& value,
                const TableIndexImmediate<validate>& imm) {
    BUILD(TableSet, imm.index, index.node, value.node, decoder->position());
  }

  void Unreachable(FullDecoder* decoder) {
    BUILD(Trap, wasm::TrapReason::kTrapUnreachable, decoder->position());
  }

  void Select(FullDecoder* decoder, const Value& cond, const Value& fval,
              const Value& tval, Value* result) {
    TFNode* controls[2];
    BUILD(BranchNoHint, cond.node, &controls[0], &controls[1]);
    TFNode* merge = BUILD(Merge, 2, controls);
    TFNode* inputs[] = {tval.node, fval.node, merge};
    TFNode* phi = BUILD(Phi, tval.type, 2, inputs);
    result->node = phi;
    builder_->SetControl(merge);
  }

  void BrOrRet(FullDecoder* decoder, uint32_t depth) {
    if (depth == decoder->control_depth() - 1) {
      uint32_t ret_count = static_cast<uint32_t>(decoder->sig_->return_count());
      base::SmallVector<TFNode*, 8> values(ret_count);
      if (ret_count > 0) {
        GetNodes(values.begin(), decoder->stack_value(ret_count), ret_count);
      }
      BUILD(Return, VectorOf(values));
    } else {
      Br(decoder, decoder->control_at(depth));
    }
  }

  void Br(FullDecoder* decoder, Control* target) {
    MergeValuesInto(decoder, target, target->br_merge());
  }

  void BrIf(FullDecoder* decoder, const Value& cond, uint32_t depth) {
    SsaEnv* fenv = ssa_env_;
    SsaEnv* tenv = Split(decoder->zone(), fenv);
    fenv->SetNotMerged();
    BUILD(BranchNoHint, cond.node, &tenv->control, &fenv->control);
    builder_->SetControl(fenv->control);
    SetEnv(tenv);
    BrOrRet(decoder, depth);
    SetEnv(fenv);
  }

  void BrTable(FullDecoder* decoder, const BranchTableImmediate<validate>& imm,
               const Value& key) {
    if (imm.table_count == 0) {
      // Only a default target. Do the equivalent of br.
      uint32_t target = BranchTableIterator<validate>(decoder, imm).next();
      BrOrRet(decoder, target);
      return;
    }

    SsaEnv* branch_env = ssa_env_;
    // Build branches to the various blocks based on the table.
    TFNode* sw = BUILD(Switch, imm.table_count + 1, key.node);

    SsaEnv* copy = Steal(decoder->zone(), branch_env);
    SetEnv(copy);
    BranchTableIterator<validate> iterator(decoder, imm);
    while (iterator.has_next()) {
      uint32_t i = iterator.cur_index();
      uint32_t target = iterator.next();
      SetEnv(Split(decoder->zone(), copy));
      builder_->SetControl(i == imm.table_count ? BUILD(IfDefault, sw)
                                                : BUILD(IfValue, i, sw));
      BrOrRet(decoder, target);
    }
    DCHECK(decoder->ok());
    SetEnv(branch_env);
  }

  void Else(FullDecoder* decoder, Control* if_block) {
    if (if_block->reachable()) {
      // Merge the if branch into the end merge.
      MergeValuesInto(decoder, if_block, &if_block->end_merge);
    }
    SetEnv(if_block->false_env);
  }

  void LoadMem(FullDecoder* decoder, LoadType type,
               const MemoryAccessImmediate<validate>& imm, const Value& index,
               Value* result) {
    result->node =
        BUILD(LoadMem, type.value_type(), type.mem_type(), index.node,
              imm.offset, imm.alignment, decoder->position());
  }

  void LoadTransform(FullDecoder* decoder, LoadType type,
                     LoadTransformationKind transform,
                     const MemoryAccessImmediate<validate>& imm,
                     const Value& index, Value* result) {
    result->node =
        BUILD(LoadTransform, type.value_type(), type.mem_type(), transform,
              index.node, imm.offset, imm.alignment, decoder->position());
  }

  void LoadLane(FullDecoder* decoder, LoadType type, const Value& value,
                const Value& index, const MemoryAccessImmediate<validate>& imm,
                const uint8_t laneidx, Value* result) {
    result->node = BUILD(LoadLane, type.mem_type(), value.node, index.node,
                         imm.offset, laneidx, decoder->position());
  }

  void StoreMem(FullDecoder* decoder, StoreType type,
                const MemoryAccessImmediate<validate>& imm, const Value& index,
                const Value& value) {
    BUILD(StoreMem, type.mem_rep(), index.node, imm.offset, imm.alignment,
          value.node, decoder->position(), type.value_type());
  }

  void StoreLane(FullDecoder* decoder, StoreType type,
                 const MemoryAccessImmediate<validate>& imm, const Value& index,
                 const Value& value, const uint8_t laneidx) {
    BUILD(StoreLane, type.mem_rep(), index.node, imm.offset, imm.alignment,
          value.node, laneidx, decoder->position(), type.value_type());
  }

  void CurrentMemoryPages(FullDecoder* decoder, Value* result) {
    result->node = BUILD(CurrentMemoryPages);
  }

  void MemoryGrow(FullDecoder* decoder, const Value& value, Value* result) {
    result->node = BUILD(MemoryGrow, value.node);
    // Always reload the instance cache after growing memory.
    LoadContextIntoSsa(ssa_env_);
  }

  enum CallMode { kDirect, kIndirect, kRef };

  void CallDirect(FullDecoder* decoder,
                  const CallFunctionImmediate<validate>& imm,
                  const Value args[], Value returns[]) {
    DoCall(decoder, kDirect, 0, CheckForNull::kWithoutNullCheck, nullptr,
           imm.sig, imm.index, args, returns);
  }

  void ReturnCall(FullDecoder* decoder,
                  const CallFunctionImmediate<validate>& imm,
                  const Value args[]) {
    DoReturnCall(decoder, kDirect, 0, CheckForNull::kWithoutNullCheck, nullptr,
                 imm.sig, imm.index, args);
  }

  void CallIndirect(FullDecoder* decoder, const Value& index,
                    const CallIndirectImmediate<validate>& imm,
                    const Value args[], Value returns[]) {
    DoCall(decoder, kIndirect, imm.table_index, CheckForNull::kWithoutNullCheck,
           index.node, imm.sig, imm.sig_index, args, returns);
  }

  void ReturnCallIndirect(FullDecoder* decoder, const Value& index,
                          const CallIndirectImmediate<validate>& imm,
                          const Value args[]) {
    DoReturnCall(decoder, kIndirect, imm.table_index,
                 CheckForNull::kWithoutNullCheck, index.node, imm.sig,
                 imm.sig_index, args);
  }

  void CallRef(FullDecoder* decoder, const Value& func_ref,
               const FunctionSig* sig, uint32_t sig_index, const Value args[],
               Value returns[]) {
    CheckForNull null_check = func_ref.type.is_nullable()
                                  ? CheckForNull::kWithNullCheck
                                  : CheckForNull::kWithoutNullCheck;
    DoCall(decoder, kRef, 0, null_check, func_ref.node, sig, sig_index, args,
           returns);
  }

  void ReturnCallRef(FullDecoder* decoder, const Value& func_ref,
                     const FunctionSig* sig, uint32_t sig_index,
                     const Value args[]) {
    CheckForNull null_check = func_ref.type.is_nullable()
                                  ? CheckForNull::kWithNullCheck
                                  : CheckForNull::kWithoutNullCheck;
    DoReturnCall(decoder, kRef, 0, null_check, func_ref.node, sig, sig_index,
                 args);
  }

  void BrOnNull(FullDecoder* decoder, const Value& ref_object, uint32_t depth) {
    SsaEnv* non_null_env = ssa_env_;
    SsaEnv* null_env = Split(decoder->zone(), non_null_env);
    non_null_env->SetNotMerged();
    BUILD(BrOnNull, ref_object.node, &null_env->control,
          &non_null_env->control);
    builder_->SetControl(non_null_env->control);
    SetEnv(null_env);
    BrOrRet(decoder, depth);
    SetEnv(non_null_env);
  }

  void SimdOp(FullDecoder* decoder, WasmOpcode opcode, Vector<Value> args,
              Value* result) {
    base::SmallVector<TFNode*, 8> inputs(args.size());
    GetNodes(inputs.begin(), args);
    TFNode* node = BUILD(SimdOp, opcode, inputs.begin());
    if (result) result->node = node;
  }

  void SimdLaneOp(FullDecoder* decoder, WasmOpcode opcode,
                  const SimdLaneImmediate<validate>& imm, Vector<Value> inputs,
                  Value* result) {
    base::SmallVector<TFNode*, 8> nodes(inputs.size());
    GetNodes(nodes.begin(), inputs);
    result->node = BUILD(SimdLaneOp, opcode, imm.lane, nodes.begin());
  }

  void Simd8x16ShuffleOp(FullDecoder* decoder,
                         const Simd128Immediate<validate>& imm,
                         const Value& input0, const Value& input1,
                         Value* result) {
    TFNode* input_nodes[] = {input0.node, input1.node};
    result->node = BUILD(Simd8x16ShuffleOp, imm.value, input_nodes);
  }

  void Throw(FullDecoder* decoder, const ExceptionIndexImmediate<validate>& imm,
             const Vector<Value>& value_args) {
    int count = value_args.length();
    ZoneVector<TFNode*> args(count, decoder->zone());
    for (int i = 0; i < count; ++i) {
      args[i] = value_args[i].node;
    }
    BUILD(Throw, imm.index, imm.exception, VectorOf(args), decoder->position());
    builder_->TerminateThrow(effect(), control());
  }

  void Rethrow(FullDecoder* decoder, const Value& exception) {
    BUILD(Rethrow, exception.node);
    builder_->TerminateThrow(effect(), control());
  }

  void BrOnException(FullDecoder* decoder, const Value& exception,
                     const ExceptionIndexImmediate<validate>& imm,
                     uint32_t depth, Vector<Value> values) {
    TFNode* if_match = nullptr;
    TFNode* if_no_match = nullptr;

    // Get the exception tag and see if it matches the expected one.
    TFNode* caught_tag =
        BUILD(GetExceptionTag, exception.node, decoder->position());
    TFNode* exception_tag = BUILD(LoadExceptionTagFromTable, imm.index);
    TFNode* compare = BUILD(ExceptionTagEqual, caught_tag, exception_tag);
    BUILD(BranchNoHint, compare, &if_match, &if_no_match);
    SsaEnv* if_no_match_env = Split(decoder->zone(), ssa_env_);
    SsaEnv* if_match_env = Steal(decoder->zone(), ssa_env_);
    if_no_match_env->control = if_no_match;
    if_match_env->control = if_match;

    // If the tags match we extract the values from the exception object and
    // push them onto the operand stack using the passed {values} vector.
    SetEnv(if_match_env);
    base::SmallVector<TFNode*, 8> caught_values(values.size());
    Vector<TFNode*> caught_vector = VectorOf(caught_values);
    BUILD(GetExceptionValues, exception.node, imm.exception, caught_vector);
    for (size_t i = 0, e = values.size(); i < e; ++i) {
      values[i].node = caught_vector[i];
    }
    BrOrRet(decoder, depth);

    // If the tags don't match we fall-through here.
    SetEnv(if_no_match_env);
  }

  void Catch(FullDecoder* decoder, Control* block, Value* exception) {
    DCHECK(block->is_try_catch());
    DCHECK_EQ(decoder->control_at(0), block);

    current_catch_ = block->previous_catch;  // Pop try scope.

    // The catch block is unreachable if no possible throws in the try block
    // exist. We only build a landing pad if some node in the try block can
    // (possibly) throw. Otherwise the catch environments remain empty.
    if (!block->try_info->might_throw()) {
      decoder->SetSucceedingCodeDynamicallyUnreachable();
      return;
    }

    SetEnv(block->try_info->catch_env);
    DCHECK_NOT_NULL(block->try_info->exception);
    exception->node = block->try_info->exception;
  }

  void AtomicOp(FullDecoder* decoder, WasmOpcode opcode, Vector<Value> args,
                const MemoryAccessImmediate<validate>& imm, Value* result) {
    base::SmallVector<TFNode*, 8> inputs(args.size());
    GetNodes(inputs.begin(), args);
    TFNode* node = BUILD(AtomicOp, opcode, inputs.begin(), imm.alignment,
                         imm.offset, decoder->position());
    if (result) result->node = node;
  }

  void AtomicFence(FullDecoder* decoder) { BUILD(AtomicFence); }

  void MemoryInit(FullDecoder* decoder,
                  const MemoryInitImmediate<validate>& imm, const Value& dst,
                  const Value& src, const Value& size) {
    BUILD(MemoryInit, imm.data_segment_index, dst.node, src.node, size.node,
          decoder->position());
  }

  void DataDrop(FullDecoder* decoder, const DataDropImmediate<validate>& imm) {
    BUILD(DataDrop, imm.index, decoder->position());
  }

  void MemoryCopy(FullDecoder* decoder,
                  const MemoryCopyImmediate<validate>& imm, const Value& dst,
                  const Value& src, const Value& size) {
    BUILD(MemoryCopy, dst.node, src.node, size.node, decoder->position());
  }

  void MemoryFill(FullDecoder* decoder,
                  const MemoryIndexImmediate<validate>& imm, const Value& dst,
                  const Value& value, const Value& size) {
    BUILD(MemoryFill, dst.node, value.node, size.node, decoder->position());
  }

  void TableInit(FullDecoder* decoder, const TableInitImmediate<validate>& imm,
                 Vector<Value> args) {
    BUILD(TableInit, imm.table.index, imm.elem_segment_index, args[0].node,
          args[1].node, args[2].node, decoder->position());
  }

  void ElemDrop(FullDecoder* decoder, const ElemDropImmediate<validate>& imm) {
    BUILD(ElemDrop, imm.index, decoder->position());
  }

  void TableCopy(FullDecoder* decoder, const TableCopyImmediate<validate>& imm,
                 Vector<Value> args) {
    BUILD(TableCopy, imm.table_dst.index, imm.table_src.index, args[0].node,
          args[1].node, args[2].node, decoder->position());
  }

  void TableGrow(FullDecoder* decoder, const TableIndexImmediate<validate>& imm,
                 const Value& value, const Value& delta, Value* result) {
    result->node = BUILD(TableGrow, imm.index, value.node, delta.node);
  }

  void TableSize(FullDecoder* decoder, const TableIndexImmediate<validate>& imm,
                 Value* result) {
    result->node = BUILD(TableSize, imm.index);
  }

  void TableFill(FullDecoder* decoder, const TableIndexImmediate<validate>& imm,
                 const Value& start, const Value& value, const Value& count) {
    BUILD(TableFill, imm.index, start.node, value.node, count.node);
  }

  void StructNewWithRtt(FullDecoder* decoder,
                        const StructIndexImmediate<validate>& imm,
                        const Value& rtt, const Value args[], Value* result) {
    uint32_t field_count = imm.struct_type->field_count();
    base::SmallVector<TFNode*, 16> arg_nodes(field_count);
    for (uint32_t i = 0; i < field_count; i++) {
      arg_nodes[i] = args[i].node;
    }
    result->node = BUILD(StructNewWithRtt, imm.index, imm.struct_type, rtt.node,
                         VectorOf(arg_nodes));
  }
  void StructNewDefault(FullDecoder* decoder,
                        const StructIndexImmediate<validate>& imm,
                        const Value& rtt, Value* result) {
    uint32_t field_count = imm.struct_type->field_count();
    base::SmallVector<TFNode*, 16> arg_nodes(field_count);
    for (uint32_t i = 0; i < field_count; i++) {
      arg_nodes[i] = DefaultValue(imm.struct_type->field(i));
    }
    result->node = BUILD(StructNewWithRtt, imm.index, imm.struct_type, rtt.node,
                         VectorOf(arg_nodes));
  }

  void StructGet(FullDecoder* decoder, const Value& struct_object,
                 const FieldIndexImmediate<validate>& field, bool is_signed,
                 Value* result) {
    CheckForNull null_check = struct_object.type.is_nullable()
                                  ? CheckForNull::kWithNullCheck
                                  : CheckForNull::kWithoutNullCheck;
    result->node =
        BUILD(StructGet, struct_object.node, field.struct_index.struct_type,
              field.index, null_check, is_signed, decoder->position());
  }

  void StructSet(FullDecoder* decoder, const Value& struct_object,
                 const FieldIndexImmediate<validate>& field,
                 const Value& field_value) {
    CheckForNull null_check = struct_object.type.is_nullable()
                                  ? CheckForNull::kWithNullCheck
                                  : CheckForNull::kWithoutNullCheck;
    BUILD(StructSet, struct_object.node, field.struct_index.struct_type,
          field.index, field_value.node, null_check, decoder->position());
  }

  void ArrayNewWithRtt(FullDecoder* decoder,
                       const ArrayIndexImmediate<validate>& imm,
                       const Value& length, const Value& initial_value,
                       const Value& rtt, Value* result) {
    result->node = BUILD(ArrayNewWithRtt, imm.index, imm.array_type,
                         length.node, initial_value.node, rtt.node);
  }

  void ArrayNewDefault(FullDecoder* decoder,
                       const ArrayIndexImmediate<validate>& imm,
                       const Value& length, const Value& rtt, Value* result) {
    TFNode* initial_value = DefaultValue(imm.array_type->element_type());
    result->node = BUILD(ArrayNewWithRtt, imm.index, imm.array_type,
                         length.node, initial_value, rtt.node);
  }

  void ArrayGet(FullDecoder* decoder, const Value& array_obj,
                const ArrayIndexImmediate<validate>& imm, const Value& index,
                bool is_signed, Value* result) {
    CheckForNull null_check = array_obj.type.is_nullable()
                                  ? CheckForNull::kWithNullCheck
                                  : CheckForNull::kWithoutNullCheck;
    result->node = BUILD(ArrayGet, array_obj.node, imm.array_type, index.node,
                         null_check, is_signed, decoder->position());
  }

  void ArraySet(FullDecoder* decoder, const Value& array_obj,
                const ArrayIndexImmediate<validate>& imm, const Value& index,
                const Value& value) {
    CheckForNull null_check = array_obj.type.is_nullable()
                                  ? CheckForNull::kWithNullCheck
                                  : CheckForNull::kWithoutNullCheck;
    BUILD(ArraySet, array_obj.node, imm.array_type, index.node, value.node,
          null_check, decoder->position());
  }

  void ArrayLen(FullDecoder* decoder, const Value& array_obj, Value* result) {
    result->node = BUILD(ArrayLen, array_obj.node, decoder->position());
  }

  void I31New(FullDecoder* decoder, const Value& input, Value* result) {
    result->node = BUILD(I31New, input.node);
  }

  void I31GetS(FullDecoder* decoder, const Value& input, Value* result) {
    result->node = BUILD(I31GetS, input.node);
  }

  void I31GetU(FullDecoder* decoder, const Value& input, Value* result) {
    result->node = BUILD(I31GetU, input.node);
  }

  void RttCanon(FullDecoder* decoder, const HeapTypeImmediate<validate>& imm,
                Value* result) {
    result->node = BUILD(RttCanon, imm.type);
  }

  void RttSub(FullDecoder* decoder, const HeapTypeImmediate<validate>& imm,
              const Value& parent, Value* result) {
    result->node = BUILD(RttSub, imm.type, parent.node);
  }

  void RefTest(FullDecoder* decoder, const Value& object, const Value& rtt,
               Value* result) {
    using CheckForI31 = compiler::WasmGraphBuilder::CheckForI31;
    using RttIsI31 = compiler::WasmGraphBuilder::RttIsI31;
    CheckForNull null_check = object.type.is_nullable()
                                  ? CheckForNull::kWithNullCheck
                                  : CheckForNull::kWithoutNullCheck;
    CheckForI31 i31_check =
        IsSubtypeOf(kWasmI31Ref, object.type, decoder->module_)
            ? CheckForI31::kWithI31Check
            : CheckForI31::kNoI31Check;
    RttIsI31 rtt_is_i31 = rtt.type.heap_representation() == HeapType::kI31
                              ? RttIsI31::kRttIsI31
                              : RttIsI31::kRttIsNotI31;
    result->node = BUILD(RefTest, object.node, rtt.node, null_check, i31_check,
                         rtt_is_i31);
  }

  void RefCast(FullDecoder* decoder, const Value& object, const Value& rtt,
               Value* result) {
    using CheckForI31 = compiler::WasmGraphBuilder::CheckForI31;
    using RttIsI31 = compiler::WasmGraphBuilder::RttIsI31;
    CheckForNull null_check = object.type.is_nullable()
                                  ? CheckForNull::kWithNullCheck
                                  : CheckForNull::kWithoutNullCheck;
    CheckForI31 i31_check =
        IsSubtypeOf(kWasmI31Ref, object.type, decoder->module_)
            ? CheckForI31::kWithI31Check
            : CheckForI31::kNoI31Check;
    RttIsI31 rtt_is_i31 = rtt.type.heap_representation() == HeapType::kI31
                              ? RttIsI31::kRttIsI31
                              : RttIsI31::kRttIsNotI31;
    result->node = BUILD(RefCast, object.node, rtt.node, null_check, i31_check,
                         rtt_is_i31, decoder->position());
  }

  void BrOnCast(FullDecoder* decoder, const Value& object, const Value& rtt,
                Value* value_on_branch, uint32_t depth) {
    using CheckForI31 = compiler::WasmGraphBuilder::CheckForI31;
    using RttIsI31 = compiler::WasmGraphBuilder::RttIsI31;
    CheckForNull null_check = object.type.is_nullable()
                                  ? CheckForNull::kWithNullCheck
                                  : CheckForNull::kWithoutNullCheck;
    CheckForI31 i31_check =
        IsSubtypeOf(kWasmI31Ref, object.type, decoder->module_)
            ? CheckForI31::kWithI31Check
            : CheckForI31::kNoI31Check;
    RttIsI31 rtt_is_i31 = rtt.type.heap_representation() == HeapType::kI31
                              ? RttIsI31::kRttIsI31
                              : RttIsI31::kRttIsNotI31;
    SsaEnv* match_env = Split(decoder->zone(), ssa_env_);
    SsaEnv* no_match_env = Steal(decoder->zone(), ssa_env_);
    no_match_env->SetNotMerged();
    BUILD(BrOnCast, object.node, rtt.node, null_check, i31_check, rtt_is_i31,
          &match_env->control, &match_env->effect, &no_match_env->control,
          &no_match_env->effect);
    builder_->SetControl(no_match_env->control);
    SetEnv(match_env);
    value_on_branch->node = object.node;
    BrOrRet(decoder, depth);
    SetEnv(no_match_env);
  }

  void PassThrough(FullDecoder* decoder, const Value& from, Value* to) {
    to->node = from.node;
  }

 private:
  SsaEnv* ssa_env_ = nullptr;
  compiler::WasmGraphBuilder* builder_;
  uint32_t current_catch_ = kNullCatch;

  TFNode* effect() { return builder_->effect(); }

  TFNode* control() { return builder_->control(); }

  TryInfo* current_try_info(FullDecoder* decoder) {
    return decoder->control_at(decoder->control_depth() - 1 - current_catch_)
        ->try_info;
  }

  void GetNodes(TFNode** nodes, Value* values, size_t count) {
    for (size_t i = 0; i < count; ++i) {
      nodes[i] = values[i].node;
    }
  }

  void GetNodes(TFNode** nodes, Vector<Value> values) {
    GetNodes(nodes, values.begin(), values.size());
  }

  void SetEnv(SsaEnv* env) {
    if (FLAG_trace_wasm_decoder) {
      char state = 'X';
      if (env) {
        switch (env->state) {
          case SsaEnv::kReached:
            state = 'R';
            break;
          case SsaEnv::kUnreachable:
            state = 'U';
            break;
          case SsaEnv::kMerged:
            state = 'M';
            break;
          case SsaEnv::kControlEnd:
            state = 'E';
            break;
        }
      }
      PrintF("{set_env = %p, state = %c", env, state);
      if (env && env->control) {
        PrintF(", control = ");
        compiler::WasmGraphBuilder::PrintDebugName(env->control);
      }
      PrintF("}\n");
    }
    if (ssa_env_) {
      ssa_env_->control = control();
      ssa_env_->effect = effect();
    }
    ssa_env_ = env;
    builder_->SetEffectControl(env->effect, env->control);
    builder_->set_instance_cache(&env->instance_cache);
  }

  TFNode* CheckForException(FullDecoder* decoder, TFNode* node) {
    if (node == nullptr) return nullptr;

    const bool inside_try_scope = current_catch_ != kNullCatch;

    if (!inside_try_scope) return node;

    TFNode* if_success = nullptr;
    TFNode* if_exception = nullptr;
    if (!builder_->ThrowsException(node, &if_success, &if_exception)) {
      return node;
    }

    SsaEnv* success_env = Steal(decoder->zone(), ssa_env_);
    success_env->control = if_success;

    SsaEnv* exception_env = Split(decoder->zone(), success_env);
    exception_env->control = if_exception;
    exception_env->effect = if_exception;
    SetEnv(exception_env);
    TryInfo* try_info = current_try_info(decoder);
    Goto(decoder, try_info->catch_env);
    if (try_info->exception == nullptr) {
      DCHECK_EQ(SsaEnv::kReached, try_info->catch_env->state);
      try_info->exception = if_exception;
    } else {
      DCHECK_EQ(SsaEnv::kMerged, try_info->catch_env->state);
      try_info->exception = builder_->CreateOrMergeIntoPhi(
          MachineRepresentation::kWord32, try_info->catch_env->control,
          try_info->exception, if_exception);
    }

    SetEnv(success_env);
    return node;
  }

  TFNode* DefaultValue(ValueType type) {
    DCHECK(type.is_defaultable());
    switch (type.kind()) {
      case ValueType::kI8:
      case ValueType::kI16:
      case ValueType::kI32:
        return builder_->Int32Constant(0);
      case ValueType::kI64:
        return builder_->Int64Constant(0);
      case ValueType::kF32:
        return builder_->Float32Constant(0);
      case ValueType::kF64:
        return builder_->Float64Constant(0);
      case ValueType::kS128:
        return builder_->S128Zero();
      case ValueType::kOptRef:
        return builder_->RefNull();
      case ValueType::kRtt:
      case ValueType::kStmt:
      case ValueType::kBottom:
      case ValueType::kRef:
        UNREACHABLE();
    }
  }

  void MergeValuesInto(FullDecoder* decoder, Control* c, Merge<Value>* merge,
                       Value* values) {
    DCHECK(merge == &c->start_merge || merge == &c->end_merge);

    SsaEnv* target = c->end_env;
    const bool first = target->state == SsaEnv::kUnreachable;
    Goto(decoder, target);

    if (merge->arity == 0) return;

    for (uint32_t i = 0; i < merge->arity; ++i) {
      Value& val = values[i];
      Value& old = (*merge)[i];
      DCHECK_NOT_NULL(val.node);
      DCHECK(val.type == kWasmBottom || val.type.machine_representation() ==
                                            old.type.machine_representation());
      old.node = first ? val.node
                       : builder_->CreateOrMergeIntoPhi(
                             old.type.machine_representation(), target->control,
                             old.node, val.node);
    }
  }

  void MergeValuesInto(FullDecoder* decoder, Control* c, Merge<Value>* merge) {
#ifdef DEBUG
    uint32_t avail =
        decoder->stack_size() - decoder->control_at(0)->stack_depth;
    DCHECK_GE(avail, merge->arity);
#endif
    Value* stack_values =
        merge->arity > 0 ? decoder->stack_value(merge->arity) : nullptr;
    MergeValuesInto(decoder, c, merge, stack_values);
  }

  void Goto(FullDecoder* decoder, SsaEnv* to) {
    DCHECK_NOT_NULL(to);
    switch (to->state) {
      case SsaEnv::kUnreachable: {  // Overwrite destination.
        to->state = SsaEnv::kReached;
        to->locals = ssa_env_->locals;
        to->control = control();
        to->effect = effect();
        to->instance_cache = ssa_env_->instance_cache;
        break;
      }
      case SsaEnv::kReached: {  // Create a new merge.
        to->state = SsaEnv::kMerged;
        // Merge control.
        TFNode* controls[] = {to->control, control()};
        TFNode* merge = builder_->Merge(2, controls);
        to->control = merge;
        // Merge effects.
        TFNode* old_effect = effect();
        if (old_effect != to->effect) {
          TFNode* inputs[] = {to->effect, old_effect, merge};
          to->effect = builder_->EffectPhi(2, inputs);
        }
        // Merge SSA values.
        for (int i = decoder->num_locals() - 1; i >= 0; i--) {
          TFNode* a = to->locals[i];
          TFNode* b = ssa_env_->locals[i];
          if (a != b) {
            TFNode* inputs[] = {a, b, merge};
            to->locals[i] = builder_->Phi(decoder->local_type(i), 2, inputs);
          }
        }
        // Start a new merge from the instance cache.
        builder_->NewInstanceCacheMerge(&to->instance_cache,
                                        &ssa_env_->instance_cache, merge);
        break;
      }
      case SsaEnv::kMerged: {
        TFNode* merge = to->control;
        // Extend the existing merge control node.
        builder_->AppendToMerge(merge, control());
        // Merge effects.
        to->effect =
            builder_->CreateOrMergeIntoEffectPhi(merge, to->effect, effect());
        // Merge locals.
        for (int i = decoder->num_locals() - 1; i >= 0; i--) {
          to->locals[i] = builder_->CreateOrMergeIntoPhi(
              decoder->local_type(i).machine_representation(), merge,
              to->locals[i], ssa_env_->locals[i]);
        }
        // Merge the instance caches.
        builder_->MergeInstanceCacheInto(&to->instance_cache,
                                         &ssa_env_->instance_cache, merge);
        break;
      }
      default:
        UNREACHABLE();
    }
    return ssa_env_->Kill();
  }

  void PrepareForLoop(FullDecoder* decoder) {
    ssa_env_->state = SsaEnv::kMerged;

    builder_->SetControl(builder_->Loop(control()));
    TFNode* effect_inputs[] = {effect(), control()};
    builder_->SetEffect(builder_->EffectPhi(1, effect_inputs));
    builder_->TerminateLoop(effect(), control());
    // The '+ 1' here is to be able to set the instance cache as assigned.
    BitVector* assigned = WasmDecoder<validate>::AnalyzeLoopAssignment(
        decoder, decoder->pc(), decoder->num_locals() + 1, decoder->zone());
    if (decoder->failed()) return;
    DCHECK_NOT_NULL(assigned);

    // Only introduce phis for variables assigned in this loop.
    int instance_cache_index = decoder->num_locals();
    for (int i = decoder->num_locals() - 1; i >= 0; i--) {
      if (!assigned->Contains(i)) continue;
      TFNode* inputs[] = {ssa_env_->locals[i], control()};
      ssa_env_->locals[i] = builder_->Phi(decoder->local_type(i), 1, inputs);
    }
    // Introduce phis for instance cache pointers if necessary.
    if (assigned->Contains(instance_cache_index)) {
      builder_->PrepareInstanceCacheForLoop(&ssa_env_->instance_cache,
                                            control());
    }

    SetEnv(Split(decoder->zone(), ssa_env_));
    builder_->StackCheck(decoder->position());
  }

  // Create a complete copy of {from}.
  SsaEnv* Split(Zone* zone, SsaEnv* from) {
    DCHECK_NOT_NULL(from);
    if (from == ssa_env_) {
      ssa_env_->control = control();
      ssa_env_->effect = effect();
    }
    SsaEnv* result = zone->New<SsaEnv>(*from);
    result->state = SsaEnv::kReached;
    return result;
  }

  // Create a copy of {from} that steals its state and leaves {from}
  // unreachable.
  SsaEnv* Steal(Zone* zone, SsaEnv* from) {
    DCHECK_NOT_NULL(from);
    if (from == ssa_env_) {
      ssa_env_->control = control();
      ssa_env_->effect = effect();
    }
    SsaEnv* result = zone->New<SsaEnv>(std::move(*from));
    result->state = SsaEnv::kReached;
    return result;
  }

  // Create an unreachable environment.
  SsaEnv* UnreachableEnv(Zone* zone) {
    return zone->New<SsaEnv>(zone, SsaEnv::kUnreachable, nullptr, nullptr, 0);
  }

  void DoCall(FullDecoder* decoder, CallMode call_mode, uint32_t table_index,
              CheckForNull null_check, TFNode* caller_node,
              const FunctionSig* sig, uint32_t sig_index, const Value args[],
              Value returns[]) {
    size_t param_count = sig->parameter_count();
    size_t return_count = sig->return_count();
    base::SmallVector<TFNode*, 16> arg_nodes(param_count + 1);
    base::SmallVector<TFNode*, 1> return_nodes(return_count);
    arg_nodes[0] = caller_node;
    for (size_t i = 0; i < param_count; ++i) {
      arg_nodes[i + 1] = args[i].node;
    }
    switch (call_mode) {
      case kIndirect:
        BUILD(CallIndirect, table_index, sig_index, VectorOf(arg_nodes),
              VectorOf(return_nodes), decoder->position());
        break;
      case kDirect:
        BUILD(CallDirect, sig_index, VectorOf(arg_nodes),
              VectorOf(return_nodes), decoder->position());
        break;
      case kRef:
        BUILD(CallRef, sig_index, VectorOf(arg_nodes), VectorOf(return_nodes),
              null_check, decoder->position());
        break;
    }
    for (size_t i = 0; i < return_count; ++i) {
      returns[i].node = return_nodes[i];
    }
    // The invoked function could have used grow_memory, so we need to
    // reload mem_size and mem_start.
    LoadContextIntoSsa(ssa_env_);
  }

  void DoReturnCall(FullDecoder* decoder, CallMode call_mode,
                    uint32_t table_index, CheckForNull null_check,
                    TFNode* index_node, const FunctionSig* sig,
                    uint32_t sig_index, const Value args[]) {
    size_t arg_count = sig->parameter_count();
    base::SmallVector<TFNode*, 16> arg_nodes(arg_count + 1);
    arg_nodes[0] = index_node;
    for (size_t i = 0; i < arg_count; ++i) {
      arg_nodes[i + 1] = args[i].node;
    }
    switch (call_mode) {
      case kIndirect:
        BUILD(ReturnCallIndirect, table_index, sig_index, VectorOf(arg_nodes),
              decoder->position());
        break;
      case kDirect:
        BUILD(ReturnCall, sig_index, VectorOf(arg_nodes), decoder->position());
        break;
      case kRef:
        BUILD(ReturnCallRef, sig_index, VectorOf(arg_nodes), null_check,
              decoder->position());
    }
  }
};

}  // namespace

DecodeResult BuildTFGraph(AccountingAllocator* allocator,
                          const WasmFeatures& enabled, const WasmModule* module,
                          compiler::WasmGraphBuilder* builder,
                          WasmFeatures* detected, const FunctionBody& body,
                          compiler::NodeOriginTable* node_origins) {
  Zone zone(allocator, ZONE_NAME);
  WasmFullDecoder<Decoder::kFullValidation, WasmGraphBuildingInterface> decoder(
      &zone, module, enabled, detected, body, builder);
  if (node_origins) {
    builder->AddBytecodePositionDecorator(node_origins, &decoder);
  }
  decoder.Decode();
  if (node_origins) {
    builder->RemoveBytecodePositionDecorator();
  }
  return decoder.toResult(nullptr);
}

#undef BUILD

}  // namespace wasm
}  // namespace internal
}  // namespace v8
