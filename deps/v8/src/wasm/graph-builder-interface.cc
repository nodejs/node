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
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-linkage.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace {

// An SsaEnv environment carries the current local variable renaming
// as well as the current effect and control dependency in the TF graph.
// It maintains a control state that tracks whether the environment
// is reachable, has reached a control end, or has been merged.
struct SsaEnv {
  enum State { kControlEnd, kUnreachable, kReached, kMerged };

  State state;
  TFNode* control;
  TFNode* effect;
  compiler::WasmInstanceCacheNodes instance_cache;
  TFNode** locals;

  void Kill(State new_state = kControlEnd) {
    state = new_state;
    locals = nullptr;
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
  static constexpr Decoder::ValidateFlag validate = Decoder::kValidate;
  using FullDecoder = WasmFullDecoder<validate, WasmGraphBuildingInterface>;

  struct Value : public ValueBase {
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

  struct Control : public ControlBase<Value> {
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
    SsaEnv* ssa_env =
        reinterpret_cast<SsaEnv*>(decoder->zone()->New(sizeof(SsaEnv)));
    uint32_t num_locals = decoder->num_locals();
    uint32_t env_count = num_locals;
    size_t size = sizeof(TFNode*) * env_count;
    ssa_env->state = SsaEnv::kReached;
    ssa_env->locals =
        size > 0 ? reinterpret_cast<TFNode**>(decoder->zone()->New(size))
                 : nullptr;

    // The first '+ 1' is needed by TF Start node, the second '+ 1' is for the
    // instance parameter.
    TFNode* start = builder_->Start(
        static_cast<int>(decoder->sig_->parameter_count() + 1 + 1));
    ssa_env->effect = start;
    ssa_env->control = start;
    // Initialize effect and control before initializing the locals default
    // values (which might require instance loads) or loading the context.
    builder_->set_effect_ptr(&ssa_env->effect);
    builder_->set_control_ptr(&ssa_env->control);
    // Initialize the instance parameter (index 0).
    builder_->set_instance_node(builder_->Param(kWasmInstanceParameterIndex));
    // Initialize local variables. Parameters are shifted by 1 because of the
    // the instance parameter.
    uint32_t index = 0;
    for (; index < decoder->sig_->parameter_count(); ++index) {
      ssa_env->locals[index] = builder_->Param(index + 1);
    }
    while (index < num_locals) {
      ValueType type = decoder->GetLocalType(index);
      TFNode* node = DefaultValue(type);
      while (index < num_locals && decoder->GetLocalType(index) == type) {
        // Do a whole run of like-typed locals at a time.
        ssa_env->locals[index++] = node;
      }
    }
    LoadContextIntoSsa(ssa_env);
    SetEnv(ssa_env);
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
    // The continue environment is the inner environment.
    SetEnv(PrepareForLoop(decoder, finish_try_env));
    ssa_env_->SetNotMerged();
    if (!decoder->ok()) return;
    // Wrap input merge into phis.
    for (uint32_t i = 0; i < block->start_merge.arity; ++i) {
      Value& val = block->start_merge[i];
      val.node = builder_->Phi(val.type, 1, &val.node, block->end_env->control);
    }
  }

  void Try(FullDecoder* decoder, Control* block) {
    SsaEnv* outer_env = ssa_env_;
    SsaEnv* catch_env = Split(decoder, outer_env);
    // Mark catch environment as unreachable, since only accessable
    // through catch unwinding (i.e. landing pads).
    catch_env->state = SsaEnv::kUnreachable;
    SsaEnv* try_env = Steal(decoder->zone(), outer_env);
    SetEnv(try_env);
    TryInfo* try_info = new (decoder->zone()) TryInfo(catch_env);
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
    SsaEnv* false_env = Split(decoder, ssa_env_);
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
      MergeValuesInto(decoder, block, &block->end_merge);
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
    auto node = BUILD(Binop, opcode, lhs.node, rhs.node, decoder->position());
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

  void RefNull(FullDecoder* decoder, Value* result) {
    result->node = builder_->RefNull();
  }

  void RefFunc(FullDecoder* decoder, uint32_t function_index, Value* result) {
    result->node = BUILD(RefFunc, function_index);
  }

  void Drop(FullDecoder* decoder, const Value& value) {}

  void DoReturn(FullDecoder* decoder, Vector<Value> values) {
    Vector<TFNode*> nodes = GetNodes(values);
    BUILD(Return, nodes);
  }

  void GetLocal(FullDecoder* decoder, Value* result,
                const LocalIndexImmediate<validate>& imm) {
    if (!ssa_env_->locals) return;  // unreachable
    result->node = ssa_env_->locals[imm.index];
  }

  void SetLocal(FullDecoder* decoder, const Value& value,
                const LocalIndexImmediate<validate>& imm) {
    if (!ssa_env_->locals) return;  // unreachable
    ssa_env_->locals[imm.index] = value.node;
  }

  void TeeLocal(FullDecoder* decoder, const Value& value, Value* result,
                const LocalIndexImmediate<validate>& imm) {
    result->node = value.node;
    if (!ssa_env_->locals) return;  // unreachable
    ssa_env_->locals[imm.index] = value.node;
  }

  void GetGlobal(FullDecoder* decoder, Value* result,
                 const GlobalIndexImmediate<validate>& imm) {
    result->node = BUILD(GetGlobal, imm.index);
  }

  void SetGlobal(FullDecoder* decoder, const Value& value,
                 const GlobalIndexImmediate<validate>& imm) {
    BUILD(SetGlobal, imm.index, value.node);
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
    BUILD(Unreachable, decoder->position());
  }

  void Select(FullDecoder* decoder, const Value& cond, const Value& fval,
              const Value& tval, Value* result) {
    TFNode* controls[2];
    BUILD(BranchNoHint, cond.node, &controls[0], &controls[1]);
    TFNode* merge = BUILD(Merge, 2, controls);
    TFNode* vals[2] = {tval.node, fval.node};
    TFNode* phi = BUILD(Phi, tval.type, 2, vals, merge);
    result->node = phi;
    ssa_env_->control = merge;
  }

  void BrOrRet(FullDecoder* decoder, uint32_t depth) {
    if (depth == decoder->control_depth() - 1) {
      uint32_t ret_count = static_cast<uint32_t>(decoder->sig_->return_count());
      Vector<TFNode*> values =
          ret_count == 0 ? Vector<TFNode*>{}
                         : GetNodes(decoder->stack_value(ret_count), ret_count);
      BUILD(Return, values);
    } else {
      Br(decoder, decoder->control_at(depth));
    }
  }

  void Br(FullDecoder* decoder, Control* target) {
    MergeValuesInto(decoder, target, target->br_merge());
  }

  void BrIf(FullDecoder* decoder, const Value& cond, uint32_t depth) {
    SsaEnv* fenv = ssa_env_;
    SsaEnv* tenv = Split(decoder, fenv);
    fenv->SetNotMerged();
    BUILD(BranchNoHint, cond.node, &tenv->control, &fenv->control);
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
      SetEnv(Split(decoder, copy));
      ssa_env_->control =
          (i == imm.table_count) ? BUILD(IfDefault, sw) : BUILD(IfValue, i, sw);
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

  void StoreMem(FullDecoder* decoder, StoreType type,
                const MemoryAccessImmediate<validate>& imm, const Value& index,
                const Value& value) {
    BUILD(StoreMem, type.mem_rep(), index.node, imm.offset, imm.alignment,
          value.node, decoder->position(), type.value_type());
  }

  void CurrentMemoryPages(FullDecoder* decoder, Value* result) {
    result->node = BUILD(CurrentMemoryPages);
  }

  void MemoryGrow(FullDecoder* decoder, const Value& value, Value* result) {
    result->node = BUILD(MemoryGrow, value.node);
    // Always reload the instance cache after growing memory.
    LoadContextIntoSsa(ssa_env_);
  }

  void CallDirect(FullDecoder* decoder,
                  const CallFunctionImmediate<validate>& imm,
                  const Value args[], Value returns[]) {
    DoCall(decoder, 0, nullptr, imm.sig, imm.index, args, returns);
  }

  void ReturnCall(FullDecoder* decoder,
                  const CallFunctionImmediate<validate>& imm,
                  const Value args[]) {
    DoReturnCall(decoder, 0, nullptr, imm.sig, imm.index, args);
  }

  void CallIndirect(FullDecoder* decoder, const Value& index,
                    const CallIndirectImmediate<validate>& imm,
                    const Value args[], Value returns[]) {
    DoCall(decoder, imm.table_index, index.node, imm.sig, imm.sig_index, args,
           returns);
  }

  void ReturnCallIndirect(FullDecoder* decoder, const Value& index,
                          const CallIndirectImmediate<validate>& imm,
                          const Value args[]) {
    DoReturnCall(decoder, imm.table_index, index.node, imm.sig, imm.sig_index,
                 args);
  }

  void SimdOp(FullDecoder* decoder, WasmOpcode opcode, Vector<Value> args,
              Value* result) {
    Vector<TFNode*> inputs = GetNodes(args);
    TFNode* node = BUILD(SimdOp, opcode, inputs.begin());
    if (result) result->node = node;
  }

  void SimdLaneOp(FullDecoder* decoder, WasmOpcode opcode,
                  const SimdLaneImmediate<validate> imm, Vector<Value> inputs,
                  Value* result) {
    Vector<TFNode*> nodes = GetNodes(inputs);
    result->node = BUILD(SimdLaneOp, opcode, imm.lane, nodes.begin());
  }

  void Simd8x16ShuffleOp(FullDecoder* decoder,
                         const Simd8x16ShuffleImmediate<validate>& imm,
                         const Value& input0, const Value& input1,
                         Value* result) {
    TFNode* input_nodes[] = {input0.node, input1.node};
    result->node = BUILD(Simd8x16ShuffleOp, imm.shuffle, input_nodes);
  }

  void Throw(FullDecoder* decoder, const ExceptionIndexImmediate<validate>& imm,
             const Vector<Value>& value_args) {
    int count = value_args.length();
    ZoneVector<TFNode*> args(count, decoder->zone());
    for (int i = 0; i < count; ++i) {
      args[i] = value_args[i].node;
    }
    BUILD(Throw, imm.index, imm.exception, VectorOf(args), decoder->position());
    builder_->TerminateThrow(ssa_env_->effect, ssa_env_->control);
  }

  void Rethrow(FullDecoder* decoder, const Value& exception) {
    BUILD(Rethrow, exception.node);
    builder_->TerminateThrow(ssa_env_->effect, ssa_env_->control);
  }

  void BrOnException(FullDecoder* decoder, const Value& exception,
                     const ExceptionIndexImmediate<validate>& imm,
                     uint32_t depth, Vector<Value> values) {
    TFNode* if_match = nullptr;
    TFNode* if_no_match = nullptr;

    // Get the exception tag and see if it matches the expected one.
    TFNode* caught_tag = BUILD(GetExceptionTag, exception.node);
    TFNode* exception_tag = BUILD(LoadExceptionTagFromTable, imm.index);
    TFNode* compare = BUILD(ExceptionTagEqual, caught_tag, exception_tag);
    BUILD(BranchNoHint, compare, &if_match, &if_no_match);
    SsaEnv* if_no_match_env = Split(decoder, ssa_env_);
    SsaEnv* if_match_env = Steal(decoder->zone(), ssa_env_);
    if_no_match_env->control = if_no_match;
    if_match_env->control = if_match;

    // If the tags match we extract the values from the exception object and
    // push them onto the operand stack using the passed {values} vector.
    SetEnv(if_match_env);
    // TODO(mstarzinger): Can't use BUILD() here, GetExceptionValues() returns
    // TFNode** rather than TFNode*. Fix to add landing pads.
    Vector<TFNode*> caught_values =
        builder_->GetExceptionValues(exception.node, imm.exception);
    for (size_t i = 0, e = values.size(); i < e; ++i) {
      values[i].node = caught_values[i];
    }
    BrOrRet(decoder, depth);

    // If the tags don't match we fall-through here.
    SetEnv(if_no_match_env);
  }

  void Catch(FullDecoder* decoder, Control* block, Value* exception) {
    DCHECK(block->is_try_catch());

    current_catch_ = block->previous_catch;  // Pop try scope.

    // The catch block is unreachable if no possible throws in the try block
    // exist. We only build a landing pad if some node in the try block can
    // (possibly) throw. Otherwise the catch environments remain empty.
    if (!block->try_info->might_throw()) {
      block->reachability = kSpecOnlyReachable;
      return;
    }

    SetEnv(block->try_info->catch_env);
    DCHECK_NOT_NULL(block->try_info->exception);
    exception->node = block->try_info->exception;
  }

  void AtomicOp(FullDecoder* decoder, WasmOpcode opcode, Vector<Value> args,
                const MemoryAccessImmediate<validate>& imm, Value* result) {
    Vector<TFNode*> inputs = GetNodes(args);
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
                 Value& value, Value& delta, Value* result) {
    result->node = BUILD(TableGrow, imm.index, value.node, delta.node);
  }

  void TableSize(FullDecoder* decoder, const TableIndexImmediate<validate>& imm,
                 Value* result) {
    result->node = BUILD(TableSize, imm.index);
  }

  void TableFill(FullDecoder* decoder, const TableIndexImmediate<validate>& imm,
                 Value& start, Value& value, Value& count) {
    BUILD(TableFill, imm.index, start.node, value.node, count.node);
  }

 private:
  SsaEnv* ssa_env_;
  compiler::WasmGraphBuilder* builder_;
  uint32_t current_catch_ = kNullCatch;

  TryInfo* current_try_info(FullDecoder* decoder) {
    return decoder->control_at(decoder->control_depth() - 1 - current_catch_)
        ->try_info;
  }

  Vector<TFNode*> GetNodes(Value* values, size_t count) {
    Vector<TFNode*> nodes = builder_->Buffer(count);
    for (size_t i = 0; i < count; ++i) {
      nodes[i] = values[i].node;
    }
    return nodes;
  }

  Vector<TFNode*> GetNodes(Vector<Value> values) {
    return GetNodes(values.begin(), values.size());
  }

  void SetEnv(SsaEnv* env) {
#if DEBUG
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
#endif
    ssa_env_ = env;
    // TODO(wasm): combine the control and effect pointers with instance cache.
    builder_->set_control_ptr(&env->control);
    builder_->set_effect_ptr(&env->effect);
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

    SsaEnv* exception_env = Split(decoder, success_env);
    exception_env->control = if_exception;
    TryInfo* try_info = current_try_info(decoder);
    Goto(decoder, exception_env, try_info->catch_env);
    TFNode* exception = try_info->exception;
    if (exception == nullptr) {
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
    switch (type) {
      case kWasmI32:
        return builder_->Int32Constant(0);
      case kWasmI64:
        return builder_->Int64Constant(0);
      case kWasmF32:
        return builder_->Float32Constant(0);
      case kWasmF64:
        return builder_->Float64Constant(0);
      case kWasmS128:
        return builder_->S128Zero();
      case kWasmAnyRef:
      case kWasmFuncRef:
      case kWasmExnRef:
        return builder_->RefNull();
      default:
        UNREACHABLE();
    }
  }

  void MergeValuesInto(FullDecoder* decoder, Control* c, Merge<Value>* merge) {
    DCHECK(merge == &c->start_merge || merge == &c->end_merge);

    SsaEnv* target = c->end_env;
    const bool first = target->state == SsaEnv::kUnreachable;
    Goto(decoder, ssa_env_, target);

    if (merge->arity == 0) return;

    uint32_t avail =
        decoder->stack_size() - decoder->control_at(0)->stack_depth;
    DCHECK_GE(avail, merge->arity);
    uint32_t start = avail >= merge->arity ? 0 : merge->arity - avail;
    Value* stack_values = decoder->stack_value(merge->arity);
    for (uint32_t i = start; i < merge->arity; ++i) {
      Value& val = stack_values[i];
      Value& old = (*merge)[i];
      DCHECK_NOT_NULL(val.node);
      DCHECK(val.type == kWasmBottom ||
             ValueTypes::MachineRepresentationFor(val.type) ==
                 ValueTypes::MachineRepresentationFor(old.type));
      old.node = first ? val.node
                       : builder_->CreateOrMergeIntoPhi(
                             ValueTypes::MachineRepresentationFor(old.type),
                             target->control, old.node, val.node);
    }
  }

  void Goto(FullDecoder* decoder, SsaEnv* from, SsaEnv* to) {
    DCHECK_NOT_NULL(to);
    switch (to->state) {
      case SsaEnv::kUnreachable: {  // Overwrite destination.
        to->state = SsaEnv::kReached;
        to->locals = from->locals;
        to->control = from->control;
        to->effect = from->effect;
        to->instance_cache = from->instance_cache;
        break;
      }
      case SsaEnv::kReached: {  // Create a new merge.
        to->state = SsaEnv::kMerged;
        // Merge control.
        TFNode* controls[] = {to->control, from->control};
        TFNode* merge = builder_->Merge(2, controls);
        to->control = merge;
        // Merge effects.
        if (from->effect != to->effect) {
          TFNode* effects[] = {to->effect, from->effect, merge};
          to->effect = builder_->EffectPhi(2, effects, merge);
        }
        // Merge SSA values.
        for (int i = decoder->num_locals() - 1; i >= 0; i--) {
          TFNode* a = to->locals[i];
          TFNode* b = from->locals[i];
          if (a != b) {
            TFNode* vals[] = {a, b};
            to->locals[i] =
                builder_->Phi(decoder->GetLocalType(i), 2, vals, merge);
          }
        }
        // Start a new merge from the instance cache.
        builder_->NewInstanceCacheMerge(&to->instance_cache,
                                        &from->instance_cache, merge);
        break;
      }
      case SsaEnv::kMerged: {
        TFNode* merge = to->control;
        // Extend the existing merge control node.
        builder_->AppendToMerge(merge, from->control);
        // Merge effects.
        to->effect = builder_->CreateOrMergeIntoEffectPhi(merge, to->effect,
                                                          from->effect);
        // Merge locals.
        for (int i = decoder->num_locals() - 1; i >= 0; i--) {
          to->locals[i] = builder_->CreateOrMergeIntoPhi(
              ValueTypes::MachineRepresentationFor(decoder->GetLocalType(i)),
              merge, to->locals[i], from->locals[i]);
        }
        // Merge the instance caches.
        builder_->MergeInstanceCacheInto(&to->instance_cache,
                                         &from->instance_cache, merge);
        break;
      }
      default:
        UNREACHABLE();
    }
    return from->Kill();
  }

  SsaEnv* PrepareForLoop(FullDecoder* decoder, SsaEnv* env) {
    env->state = SsaEnv::kMerged;

    env->control = builder_->Loop(env->control);
    env->effect = builder_->EffectPhi(1, &env->effect, env->control);
    builder_->TerminateLoop(env->effect, env->control);
    // The '+ 1' here is to be able to set the instance cache as assigned.
    BitVector* assigned = WasmDecoder<validate>::AnalyzeLoopAssignment(
        decoder, decoder->pc(), decoder->total_locals() + 1, decoder->zone());
    if (decoder->failed()) return env;
    if (assigned != nullptr) {
      // Only introduce phis for variables assigned in this loop.
      int instance_cache_index = decoder->total_locals();
      for (int i = decoder->num_locals() - 1; i >= 0; i--) {
        if (!assigned->Contains(i)) continue;
        env->locals[i] = builder_->Phi(decoder->GetLocalType(i), 1,
                                       &env->locals[i], env->control);
      }
      // Introduce phis for instance cache pointers if necessary.
      if (assigned->Contains(instance_cache_index)) {
        builder_->PrepareInstanceCacheForLoop(&env->instance_cache,
                                              env->control);
      }

      SsaEnv* loop_body_env = Split(decoder, env);
      builder_->StackCheck(decoder->position(), &(loop_body_env->effect),
                           &(loop_body_env->control));
      return loop_body_env;
    }

    // Conservatively introduce phis for all local variables.
    for (int i = decoder->num_locals() - 1; i >= 0; i--) {
      env->locals[i] = builder_->Phi(decoder->GetLocalType(i), 1,
                                     &env->locals[i], env->control);
    }

    // Conservatively introduce phis for instance cache.
    builder_->PrepareInstanceCacheForLoop(&env->instance_cache, env->control);

    SsaEnv* loop_body_env = Split(decoder, env);
    builder_->StackCheck(decoder->position(), &loop_body_env->effect,
                         &loop_body_env->control);
    return loop_body_env;
  }

  // Create a complete copy of {from}.
  SsaEnv* Split(FullDecoder* decoder, SsaEnv* from) {
    DCHECK_NOT_NULL(from);
    SsaEnv* result =
        reinterpret_cast<SsaEnv*>(decoder->zone()->New(sizeof(SsaEnv)));
    size_t size = sizeof(TFNode*) * decoder->num_locals();
    result->control = from->control;
    result->effect = from->effect;

    result->state = SsaEnv::kReached;
    if (size > 0) {
      result->locals = reinterpret_cast<TFNode**>(decoder->zone()->New(size));
      memcpy(result->locals, from->locals, size);
    } else {
      result->locals = nullptr;
    }
    result->instance_cache = from->instance_cache;

    return result;
  }

  // Create a copy of {from} that steals its state and leaves {from}
  // unreachable.
  SsaEnv* Steal(Zone* zone, SsaEnv* from) {
    DCHECK_NOT_NULL(from);
    SsaEnv* result = reinterpret_cast<SsaEnv*>(zone->New(sizeof(SsaEnv)));
    result->state = SsaEnv::kReached;
    result->locals = from->locals;
    result->control = from->control;
    result->effect = from->effect;
    result->instance_cache = from->instance_cache;
    from->Kill(SsaEnv::kUnreachable);
    return result;
  }

  // Create an unreachable environment.
  SsaEnv* UnreachableEnv(Zone* zone) {
    SsaEnv* result = reinterpret_cast<SsaEnv*>(zone->New(sizeof(SsaEnv)));
    result->state = SsaEnv::kUnreachable;
    result->control = nullptr;
    result->effect = nullptr;
    result->locals = nullptr;
    result->instance_cache = {};
    return result;
  }

  void DoCall(FullDecoder* decoder, uint32_t table_index, TFNode* index_node,
              FunctionSig* sig, uint32_t sig_index, const Value args[],
              Value returns[]) {
    int param_count = static_cast<int>(sig->parameter_count());
    Vector<TFNode*> arg_nodes = builder_->Buffer(param_count + 1);
    TFNode** return_nodes = nullptr;
    arg_nodes[0] = index_node;
    for (int i = 0; i < param_count; ++i) {
      arg_nodes[i + 1] = args[i].node;
    }
    if (index_node) {
      BUILD(CallIndirect, table_index, sig_index, arg_nodes.begin(),
            &return_nodes, decoder->position());
    } else {
      BUILD(CallDirect, sig_index, arg_nodes.begin(), &return_nodes,
            decoder->position());
    }
    int return_count = static_cast<int>(sig->return_count());
    for (int i = 0; i < return_count; ++i) {
      returns[i].node = return_nodes[i];
    }
    // The invoked function could have used grow_memory, so we need to
    // reload mem_size and mem_start.
    LoadContextIntoSsa(ssa_env_);
  }

  void DoReturnCall(FullDecoder* decoder, uint32_t table_index,
                    TFNode* index_node, FunctionSig* sig, uint32_t sig_index,
                    const Value args[]) {
    int arg_count = static_cast<int>(sig->parameter_count());
    Vector<TFNode*> arg_nodes = builder_->Buffer(arg_count + 1);
    arg_nodes[0] = index_node;
    for (int i = 0; i < arg_count; ++i) {
      arg_nodes[i + 1] = args[i].node;
    }
    if (index_node) {
      BUILD(ReturnCallIndirect, table_index, sig_index, arg_nodes.begin(),
            decoder->position());
    } else {
      BUILD(ReturnCall, sig_index, arg_nodes.begin(), decoder->position());
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
  WasmFullDecoder<Decoder::kValidate, WasmGraphBuildingInterface> decoder(
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
