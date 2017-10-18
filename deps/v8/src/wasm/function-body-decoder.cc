// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/signature.h"

#include "src/base/platform/elapsed-timer.h"
#include "src/flags.h"
#include "src/handles.h"
#include "src/objects-inl.h"
#include "src/zone/zone-containers.h"

#include "src/wasm/decoder.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes.h"

#include "src/ostreams.h"

#include "src/compiler/wasm-compiler.h"

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
  TFNode** locals;

  bool go() { return state >= kReached; }
  void Kill(State new_state = kControlEnd) {
    state = new_state;
    locals = nullptr;
    control = nullptr;
    effect = nullptr;
  }
  void SetNotMerged() {
    if (state == kMerged) state = kReached;
  }
};

// Macros that build nodes only if there is a graph and the current SSA
// environment is reachable from start. This avoids problems with malformed
// TF graphs when decoding inputs that have unreachable code.
#define BUILD(func, ...)                                                    \
  (build(decoder) ? CheckForException(decoder, builder_->func(__VA_ARGS__)) \
                  : nullptr)

constexpr uint32_t kNullCatch = static_cast<uint32_t>(-1);

class WasmGraphBuildingInterface
    : public InterfaceTemplate<true, WasmGraphBuildingInterface> {
 public:
  struct IValue {
    TFNode* node;

    static IValue Unreachable() { return {nullptr}; }
    static IValue New() { return {nullptr}; }
  };

  struct TryInfo : public ZoneObject {
    SsaEnv* catch_env;
    TFNode* exception;

    explicit TryInfo(SsaEnv* c) : catch_env(c), exception(nullptr) {}
  };

  struct IControl {
    SsaEnv* end_env;         // end environment for the construct.
    SsaEnv* false_env;       // false environment (only for if).
    TryInfo* try_info;       // information used for compiling try statements.
    int32_t previous_catch;  // previous Control (on the stack) with a catch.

    static IControl Block() { return {}; }
    static IControl If() { return {}; }
    static IControl Loop() { return {}; }
    static IControl Try() { return {}; }
  };

  explicit WasmGraphBuildingInterface(TFBuilder* builder) : builder_(builder) {}

  void StartFunction(Decoder* decoder) {
    SsaEnv* ssa_env =
        reinterpret_cast<SsaEnv*>(decoder->zone()->New(sizeof(SsaEnv)));
    uint32_t env_count = decoder->NumLocals();
    size_t size = sizeof(TFNode*) * env_count;
    ssa_env->state = SsaEnv::kReached;
    ssa_env->locals =
        size > 0 ? reinterpret_cast<TFNode**>(decoder->zone()->New(size))
                 : nullptr;

    TFNode* start =
        builder_->Start(static_cast<int>(decoder->sig_->parameter_count() + 1));
    // Initialize local variables.
    uint32_t index = 0;
    for (; index < decoder->sig_->parameter_count(); ++index) {
      ssa_env->locals[index] = builder_->Param(index);
    }
    while (index < env_count) {
      ValueType type = decoder->GetLocalType(index);
      TFNode* node = DefaultValue(type);
      while (index < env_count && decoder->GetLocalType(index) == type) {
        // Do a whole run of like-typed locals at a time.
        ssa_env->locals[index++] = node;
      }
    }
    ssa_env->control = start;
    ssa_env->effect = start;
    SetEnv(ssa_env);
  }

  void StartFunctionBody(Decoder* decoder, Control* block) {
    SsaEnv* break_env = ssa_env_;
    SetEnv(Steal(decoder->zone(), break_env));
    block->interface_data.end_env = break_env;
  }

  void FinishFunction(Decoder* decoder) {
    builder_->PatchInStackCheckIfNeeded();
  }

  void Block(Decoder* decoder, Control* block) {
    // The break environment is the outer environment.
    block->interface_data.end_env = ssa_env_;
    SetEnv(Steal(decoder->zone(), ssa_env_));
  }

  void Loop(Decoder* decoder, Control* block) {
    SsaEnv* finish_try_env = Steal(decoder->zone(), ssa_env_);
    block->interface_data.end_env = finish_try_env;
    // The continue environment is the inner environment.
    SetEnv(PrepareForLoop(decoder, finish_try_env));
    ssa_env_->SetNotMerged();
  }

  void Try(Decoder* decoder, Control* block) {
    SsaEnv* outer_env = ssa_env_;
    SsaEnv* try_env = Steal(decoder->zone(), outer_env);
    SsaEnv* catch_env = UnreachableEnv(decoder->zone());
    SetEnv(try_env);
    TryInfo* try_info = new (decoder->zone()) TryInfo(catch_env);
    block->interface_data.end_env = outer_env;
    block->interface_data.try_info = try_info;
    block->interface_data.previous_catch = current_catch_;
    current_catch_ = static_cast<int32_t>(decoder->control_depth() - 1);
  }

  void If(Decoder* decoder, const Value& cond, Control* if_block) {
    TFNode* if_true = nullptr;
    TFNode* if_false = nullptr;
    BUILD(BranchNoHint, cond.interface_data.node, &if_true, &if_false);
    SsaEnv* end_env = ssa_env_;
    SsaEnv* false_env = Split(decoder, ssa_env_);
    false_env->control = if_false;
    SsaEnv* true_env = Steal(decoder->zone(), ssa_env_);
    true_env->control = if_true;
    if_block->interface_data.end_env = end_env;
    if_block->interface_data.false_env = false_env;
    SetEnv(true_env);
  }

  void FallThruTo(Decoder* decoder, Control* c) {
    MergeValuesInto(decoder, c);
    SetEnv(c->interface_data.end_env);
  }

  void PopControl(Decoder* decoder, Control& block) {
    if (block.is_onearmed_if()) {
      Goto(decoder, block.interface_data.false_env,
           block.interface_data.end_env);
    }
  }

  void EndControl(Decoder* decoder, Control* block) { ssa_env_->Kill(); }

  void UnOp(Decoder* decoder, WasmOpcode opcode, FunctionSig* sig,
            const Value& value, Value* result) {
    result->interface_data.node =
        BUILD(Unop, opcode, value.interface_data.node, decoder->position());
  }

  void BinOp(Decoder* decoder, WasmOpcode opcode, FunctionSig* sig,
             const Value& lhs, const Value& rhs, Value* result) {
    result->interface_data.node =
        BUILD(Binop, opcode, lhs.interface_data.node, rhs.interface_data.node,
              decoder->position());
  }

  void I32Const(Decoder* decoder, Value* result, int32_t value) {
    result->interface_data.node = builder_->Int32Constant(value);
  }

  void I64Const(Decoder* decoder, Value* result, int64_t value) {
    result->interface_data.node = builder_->Int64Constant(value);
  }

  void F32Const(Decoder* decoder, Value* result, float value) {
    result->interface_data.node = builder_->Float32Constant(value);
  }

  void F64Const(Decoder* decoder, Value* result, double value) {
    result->interface_data.node = builder_->Float64Constant(value);
  }

  void DoReturn(Decoder* decoder, Vector<Value> values) {
    size_t num_values = values.size();
    TFNode** buffer = GetNodes(values);
    for (size_t i = 0; i < num_values; ++i) {
      buffer[i] = values[i].interface_data.node;
    }
    BUILD(Return, static_cast<unsigned>(values.size()), buffer);
  }

  void GetLocal(Decoder* decoder, Value* result,
                const LocalIndexOperand<true>& operand) {
    if (!ssa_env_->locals) return;  // unreachable
    result->interface_data.node = ssa_env_->locals[operand.index];
  }

  void SetLocal(Decoder* decoder, const Value& value,
                const LocalIndexOperand<true>& operand) {
    if (!ssa_env_->locals) return;  // unreachable
    ssa_env_->locals[operand.index] = value.interface_data.node;
  }

  void TeeLocal(Decoder* decoder, const Value& value, Value* result,
                const LocalIndexOperand<true>& operand) {
    result->interface_data.node = value.interface_data.node;
    if (!ssa_env_->locals) return;  // unreachable
    ssa_env_->locals[operand.index] = value.interface_data.node;
  }

  void GetGlobal(Decoder* decoder, Value* result,
                 const GlobalIndexOperand<true>& operand) {
    result->interface_data.node = BUILD(GetGlobal, operand.index);
  }

  void SetGlobal(Decoder* decoder, const Value& value,
                 const GlobalIndexOperand<true>& operand) {
    BUILD(SetGlobal, operand.index, value.interface_data.node);
  }

  void Unreachable(Decoder* decoder) {
    BUILD(Unreachable, decoder->position());
  }

  void Select(Decoder* decoder, const Value& cond, const Value& fval,
              const Value& tval, Value* result) {
    TFNode* controls[2];
    BUILD(BranchNoHint, cond.interface_data.node, &controls[0], &controls[1]);
    TFNode* merge = BUILD(Merge, 2, controls);
    TFNode* vals[2] = {tval.interface_data.node, fval.interface_data.node};
    TFNode* phi = BUILD(Phi, tval.type, 2, vals, merge);
    result->interface_data.node = phi;
    ssa_env_->control = merge;
  }

  void BreakTo(Decoder* decoder, Control* block) {
    if (block->is_loop()) {
      Goto(decoder, ssa_env_, block->interface_data.end_env);
    } else {
      MergeValuesInto(decoder, block);
    }
  }

  void BrIf(Decoder* decoder, const Value& cond, Control* block) {
    SsaEnv* fenv = ssa_env_;
    SsaEnv* tenv = Split(decoder, fenv);
    fenv->SetNotMerged();
    BUILD(BranchNoHint, cond.interface_data.node, &tenv->control,
          &fenv->control);
    ssa_env_ = tenv;
    BreakTo(decoder, block);
    ssa_env_ = fenv;
  }

  void BrTable(Decoder* decoder, const BranchTableOperand<true>& operand,
               const Value& key) {
    SsaEnv* break_env = ssa_env_;
    // Build branches to the various blocks based on the table.
    TFNode* sw =
        BUILD(Switch, operand.table_count + 1, key.interface_data.node);

    SsaEnv* copy = Steal(decoder->zone(), break_env);
    ssa_env_ = copy;
    BranchTableIterator<true> iterator(decoder, operand);
    while (iterator.has_next()) {
      uint32_t i = iterator.cur_index();
      uint32_t target = iterator.next();
      ssa_env_ = Split(decoder, copy);
      ssa_env_->control = (i == operand.table_count) ? BUILD(IfDefault, sw)
                                                     : BUILD(IfValue, i, sw);
      BreakTo(decoder, decoder->control_at(target));
    }
    DCHECK(decoder->ok());
    ssa_env_ = break_env;
  }

  void Else(Decoder* decoder, Control* if_block) {
    SetEnv(if_block->interface_data.false_env);
  }

  void LoadMem(Decoder* decoder, ValueType type, MachineType mem_type,
               const MemoryAccessOperand<true>& operand, const Value& index,
               Value* result) {
    result->interface_data.node =
        BUILD(LoadMem, type, mem_type, index.interface_data.node,
              operand.offset, operand.alignment, decoder->position());
  }

  void StoreMem(Decoder* decoder, ValueType type, MachineType mem_type,
                const MemoryAccessOperand<true>& operand, const Value& index,
                const Value& value) {
    BUILD(StoreMem, mem_type, index.interface_data.node, operand.offset,
          operand.alignment, value.interface_data.node, decoder->position(),
          type);
  }

  void CurrentMemoryPages(Decoder* decoder, Value* result) {
    result->interface_data.node = BUILD(CurrentMemoryPages);
  }

  void GrowMemory(Decoder* decoder, const Value& value, Value* result) {
    result->interface_data.node = BUILD(GrowMemory, value.interface_data.node);
  }

  void CallDirect(Decoder* decoder, const CallFunctionOperand<true>& operand,
                  const Value args[], Value returns[]) {
    DoCall(decoder, nullptr, operand, args, returns, false);
  }

  void CallIndirect(Decoder* decoder, const Value& index,
                    const CallIndirectOperand<true>& operand,
                    const Value args[], Value returns[]) {
    DoCall(decoder, index.interface_data.node, operand, args, returns, true);
  }

  void SimdOp(Decoder* decoder, WasmOpcode opcode, Vector<Value> args,
              Value* result) {
    TFNode** inputs = GetNodes(args);
    TFNode* node = BUILD(SimdOp, opcode, inputs);
    if (result) result->interface_data.node = node;
  }

  void SimdLaneOp(Decoder* decoder, WasmOpcode opcode,
                  const SimdLaneOperand<true> operand, Vector<Value> inputs,
                  Value* result) {
    TFNode** nodes = GetNodes(inputs);
    result->interface_data.node =
        BUILD(SimdLaneOp, opcode, operand.lane, nodes);
  }

  void SimdShiftOp(Decoder* decoder, WasmOpcode opcode,
                   const SimdShiftOperand<true> operand, const Value& input,
                   Value* result) {
    TFNode* inputs[] = {input.interface_data.node};
    result->interface_data.node =
        BUILD(SimdShiftOp, opcode, operand.shift, inputs);
  }

  void Simd8x16ShuffleOp(Decoder* decoder,
                         const Simd8x16ShuffleOperand<true>& operand,
                         const Value& input0, const Value& input1,
                         Value* result) {
    TFNode* input_nodes[] = {input0.interface_data.node,
                             input1.interface_data.node};
    result->interface_data.node =
        BUILD(Simd8x16ShuffleOp, operand.shuffle, input_nodes);
  }

  TFNode* GetExceptionTag(Decoder* decoder,
                          const ExceptionIndexOperand<true>& operand) {
    // TODO(kschimpf): Need to get runtime exception tag values. This
    // code only handles non-imported/exported exceptions.
    return BUILD(Int32Constant, operand.index);
  }

  void Throw(Decoder* decoder, const ExceptionIndexOperand<true>& operand) {
    BUILD(Throw, GetExceptionTag(decoder, operand));
  }

  void Catch(Decoder* decoder, const ExceptionIndexOperand<true>& operand,
             Control* block) {
    DCHECK(block->is_try_catch());
    current_catch_ = block->interface_data.previous_catch;
    SsaEnv* catch_env = block->interface_data.try_info->catch_env;
    SetEnv(catch_env);

    // Get the exception and see if wanted exception.
    TFNode* exception_as_i32 = BUILD(
        Catch, block->interface_data.try_info->exception, decoder->position());
    TFNode* exception_tag = GetExceptionTag(decoder, operand);
    TFNode* compare_i32 = BUILD(Binop, kExprI32Eq, exception_as_i32,
                                exception_tag, decoder->position());
    TFNode* if_true = nullptr;
    TFNode* if_false = nullptr;
    BUILD(BranchNoHint, compare_i32, &if_true, &if_false);
    SsaEnv* false_env = Split(decoder, catch_env);
    false_env->control = if_false;
    SsaEnv* true_env = Steal(decoder->zone(), catch_env);
    true_env->control = if_true;
    block->interface_data.try_info->catch_env = false_env;

    // Generate code to re-throw the exception.
    DCHECK_NOT_NULL(block->interface_data.try_info->catch_env);
    SetEnv(false_env);
    BUILD(Rethrow);
    FallThruTo(decoder, block);

    SetEnv(true_env);
    // TODO(kschimpf): Add code to pop caught exception from isolate.
  }

  void AtomicOp(Decoder* decoder, WasmOpcode opcode, Vector<Value> args,
                Value* result) {
    TFNode** inputs = GetNodes(args);
    TFNode* node = BUILD(AtomicOp, opcode, inputs, decoder->position());
    if (result) result->interface_data.node = node;
  }

 private:
  SsaEnv* ssa_env_;
  TFBuilder* builder_;
  uint32_t current_catch_ = kNullCatch;

  bool build(Decoder* decoder) { return ssa_env_->go() && decoder->ok(); }

  TryInfo* current_try_info(Decoder* decoder) {
    return decoder->control_at(decoder->control_depth() - 1 - current_catch_)
        ->interface_data.try_info;
  }

  TFNode** GetNodes(Value* values, size_t count) {
    TFNode** nodes = builder_->Buffer(count);
    for (size_t i = 0; i < count; ++i) {
      nodes[i] = values[i].interface_data.node;
    }
    return nodes;
  }

  TFNode** GetNodes(Vector<Value> values) {
    return GetNodes(values.start(), values.size());
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
      PrintF("{set_env = %p, state = %c", static_cast<void*>(env), state);
      if (env && env->control) {
        PrintF(", control = ");
        compiler::WasmGraphBuilder::PrintDebugName(env->control);
      }
      PrintF("}\n");
    }
#endif
    ssa_env_ = env;
    builder_->set_control_ptr(&env->control);
    builder_->set_effect_ptr(&env->effect);
  }

  TFNode* CheckForException(Decoder* decoder, TFNode* node) {
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
      try_info->exception =
          CreateOrMergeIntoPhi(kWasmI32, try_info->catch_env->control,
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
      default:
        UNREACHABLE();
    }
  }

  void MergeValuesInto(Decoder* decoder, Control* c) {
    if (!ssa_env_->go()) return;

    SsaEnv* target = c->interface_data.end_env;
    const bool first = target->state == SsaEnv::kUnreachable;
    Goto(decoder, ssa_env_, target);

    size_t avail = decoder->stack_size() - decoder->control_at(0)->stack_depth;
    size_t start = avail >= c->merge.arity ? 0 : c->merge.arity - avail;
    for (size_t i = start; i < c->merge.arity; ++i) {
      auto& val = decoder->GetMergeValueFromStack(c, i);
      auto& old = c->merge[i];
      DCHECK_NOT_NULL(val.interface_data.node);
      // TODO(clemensh): Remove first.
      DCHECK_EQ(first, old.interface_data.node == nullptr);
      DCHECK(val.type == old.type || val.type == kWasmVar);
      old.interface_data.node =
          first ? val.interface_data.node
                : CreateOrMergeIntoPhi(old.type, target->control,
                                       old.interface_data.node,
                                       val.interface_data.node);
    }
  }

  void Goto(Decoder* decoder, SsaEnv* from, SsaEnv* to) {
    DCHECK_NOT_NULL(to);
    if (!from->go()) return;
    switch (to->state) {
      case SsaEnv::kUnreachable: {  // Overwrite destination.
        to->state = SsaEnv::kReached;
        to->locals = from->locals;
        to->control = from->control;
        to->effect = from->effect;
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
        for (int i = decoder->NumLocals() - 1; i >= 0; i--) {
          TFNode* a = to->locals[i];
          TFNode* b = from->locals[i];
          if (a != b) {
            TFNode* vals[] = {a, b};
            to->locals[i] =
                builder_->Phi(decoder->GetLocalType(i), 2, vals, merge);
          }
        }
        break;
      }
      case SsaEnv::kMerged: {
        TFNode* merge = to->control;
        // Extend the existing merge.
        builder_->AppendToMerge(merge, from->control);
        // Merge effects.
        if (builder_->IsPhiWithMerge(to->effect, merge)) {
          builder_->AppendToPhi(to->effect, from->effect);
        } else if (to->effect != from->effect) {
          uint32_t count = builder_->InputCount(merge);
          TFNode** effects = builder_->Buffer(count);
          for (uint32_t j = 0; j < count - 1; j++) {
            effects[j] = to->effect;
          }
          effects[count - 1] = from->effect;
          to->effect = builder_->EffectPhi(count, effects, merge);
        }
        // Merge locals.
        for (int i = decoder->NumLocals() - 1; i >= 0; i--) {
          TFNode* tnode = to->locals[i];
          TFNode* fnode = from->locals[i];
          if (builder_->IsPhiWithMerge(tnode, merge)) {
            builder_->AppendToPhi(tnode, fnode);
          } else if (tnode != fnode) {
            uint32_t count = builder_->InputCount(merge);
            TFNode** vals = builder_->Buffer(count);
            for (uint32_t j = 0; j < count - 1; j++) {
              vals[j] = tnode;
            }
            vals[count - 1] = fnode;
            to->locals[i] =
                builder_->Phi(decoder->GetLocalType(i), count, vals, merge);
          }
        }
        break;
      }
      default:
        UNREACHABLE();
    }
    return from->Kill();
  }

  TFNode* CreateOrMergeIntoPhi(ValueType type, TFNode* merge, TFNode* tnode,
                               TFNode* fnode) {
    if (builder_->IsPhiWithMerge(tnode, merge)) {
      builder_->AppendToPhi(tnode, fnode);
    } else if (tnode != fnode) {
      uint32_t count = builder_->InputCount(merge);
      TFNode** vals = builder_->Buffer(count);
      for (uint32_t j = 0; j < count - 1; j++) vals[j] = tnode;
      vals[count - 1] = fnode;
      return builder_->Phi(type, count, vals, merge);
    }
    return tnode;
  }

  SsaEnv* PrepareForLoop(Decoder* decoder, SsaEnv* env) {
    if (!env->go()) return Split(decoder, env);
    env->state = SsaEnv::kMerged;

    env->control = builder_->Loop(env->control);
    env->effect = builder_->EffectPhi(1, &env->effect, env->control);
    builder_->Terminate(env->effect, env->control);
    BitVector* assigned = WasmDecoder<true>::AnalyzeLoopAssignment(
        decoder, decoder->pc(), static_cast<int>(decoder->total_locals()),
        decoder->zone());
    if (decoder->failed()) return env;
    if (assigned != nullptr) {
      // Only introduce phis for variables assigned in this loop.
      for (int i = decoder->NumLocals() - 1; i >= 0; i--) {
        if (!assigned->Contains(i)) continue;
        env->locals[i] = builder_->Phi(decoder->GetLocalType(i), 1,
                                       &env->locals[i], env->control);
      }
      SsaEnv* loop_body_env = Split(decoder, env);
      builder_->StackCheck(decoder->position(), &(loop_body_env->effect),
                           &(loop_body_env->control));
      return loop_body_env;
    }

    // Conservatively introduce phis for all local variables.
    for (int i = decoder->NumLocals() - 1; i >= 0; i--) {
      env->locals[i] = builder_->Phi(decoder->GetLocalType(i), 1,
                                     &env->locals[i], env->control);
    }

    SsaEnv* loop_body_env = Split(decoder, env);
    builder_->StackCheck(decoder->position(), &loop_body_env->effect,
                         &loop_body_env->control);
    return loop_body_env;
  }

  // Create a complete copy of the {from}.
  SsaEnv* Split(Decoder* decoder, SsaEnv* from) {
    DCHECK_NOT_NULL(from);
    SsaEnv* result =
        reinterpret_cast<SsaEnv*>(decoder->zone()->New(sizeof(SsaEnv)));
    size_t size = sizeof(TFNode*) * decoder->NumLocals();
    result->control = from->control;
    result->effect = from->effect;

    if (from->go()) {
      result->state = SsaEnv::kReached;
      result->locals =
          size > 0 ? reinterpret_cast<TFNode**>(decoder->zone()->New(size))
                   : nullptr;
      memcpy(result->locals, from->locals, size);
    } else {
      result->state = SsaEnv::kUnreachable;
      result->locals = nullptr;
    }

    return result;
  }

  // Create a copy of {from} that steals its state and leaves {from}
  // unreachable.
  SsaEnv* Steal(Zone* zone, SsaEnv* from) {
    DCHECK_NOT_NULL(from);
    if (!from->go()) return UnreachableEnv(zone);
    SsaEnv* result = reinterpret_cast<SsaEnv*>(zone->New(sizeof(SsaEnv)));
    result->state = SsaEnv::kReached;
    result->locals = from->locals;
    result->control = from->control;
    result->effect = from->effect;
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
    return result;
  }

  template <typename Operand>
  void DoCall(WasmFullDecoder<true, WasmGraphBuildingInterface>* decoder,
              TFNode* index_node, const Operand& operand, const Value args[],
              Value returns[], bool is_indirect) {
    if (!build(decoder)) return;
    int param_count = static_cast<int>(operand.sig->parameter_count());
    TFNode** arg_nodes = builder_->Buffer(param_count + 1);
    TFNode** return_nodes = nullptr;
    arg_nodes[0] = index_node;
    for (int i = 0; i < param_count; ++i) {
      arg_nodes[i + 1] = args[i].interface_data.node;
    }
    if (is_indirect) {
      builder_->CallIndirect(operand.index, arg_nodes, &return_nodes,
                             decoder->position());
    } else {
      builder_->CallDirect(operand.index, arg_nodes, &return_nodes,
                           decoder->position());
    }
    int return_count = static_cast<int>(operand.sig->return_count());
    for (int i = 0; i < return_count; ++i) {
      returns[i].interface_data.node = return_nodes[i];
    }
  }
};

}  // namespace

bool DecodeLocalDecls(BodyLocalDecls* decls, const byte* start,
                      const byte* end) {
  Decoder decoder(start, end);
  if (WasmDecoder<true>::DecodeLocals(&decoder, nullptr, &decls->type_list)) {
    DCHECK(decoder.ok());
    decls->encoded_size = decoder.pc_offset();
    return true;
  }
  return false;
}

BytecodeIterator::BytecodeIterator(const byte* start, const byte* end,
                                   BodyLocalDecls* decls)
    : Decoder(start, end) {
  if (decls != nullptr) {
    if (DecodeLocalDecls(decls, start, end)) {
      pc_ += decls->encoded_size;
      if (pc_ > end_) pc_ = end_;
    }
  }
}

DecodeResult VerifyWasmCode(AccountingAllocator* allocator,
                            const wasm::WasmModule* module,
                            FunctionBody& body) {
  Zone zone(allocator, ZONE_NAME);
  WasmFullDecoder<true, EmptyInterface> decoder(&zone, module, body);
  decoder.Decode();
  return decoder.toResult(nullptr);
}

DecodeResult VerifyWasmCodeWithStats(AccountingAllocator* allocator,
                                     const wasm::WasmModule* module,
                                     FunctionBody& body, bool is_wasm,
                                     Counters* counters) {
  auto size_histogram = is_wasm ? counters->wasm_wasm_function_size_bytes()
                                : counters->wasm_asm_function_size_bytes();
  // TODO(bradnelson): Improve histogram handling of ptrdiff_t.
  CHECK((body.end - body.start) >= 0);
  size_histogram->AddSample(static_cast<int>(body.end - body.start));
  auto time_counter = is_wasm ? counters->wasm_decode_wasm_function_time()
                              : counters->wasm_decode_asm_function_time();
  TimedHistogramScope wasm_decode_function_time_scope(time_counter);
  return VerifyWasmCode(allocator, module, body);
}

DecodeResult BuildTFGraph(AccountingAllocator* allocator, TFBuilder* builder,
                          FunctionBody& body) {
  Zone zone(allocator, ZONE_NAME);
  WasmFullDecoder<true, WasmGraphBuildingInterface> decoder(
      &zone, builder->module(), body, builder);
  decoder.Decode();
  return decoder.toResult(nullptr);
}

unsigned OpcodeLength(const byte* pc, const byte* end) {
  Decoder decoder(pc, end);
  return WasmDecoder<false>::OpcodeLength(&decoder, pc);
}

std::pair<uint32_t, uint32_t> StackEffect(const WasmModule* module,
                                          FunctionSig* sig, const byte* pc,
                                          const byte* end) {
  WasmDecoder<false> decoder(module, sig, pc, end);
  return decoder.StackEffect(pc);
}

void PrintRawWasmCode(const byte* start, const byte* end) {
  AccountingAllocator allocator;
  PrintRawWasmCode(&allocator, FunctionBodyForTesting(start, end), nullptr);
}

namespace {
const char* RawOpcodeName(WasmOpcode opcode) {
  switch (opcode) {
#define DECLARE_NAME_CASE(name, opcode, sig) \
  case kExpr##name:                          \
    return "kExpr" #name;
    FOREACH_OPCODE(DECLARE_NAME_CASE)
#undef DECLARE_NAME_CASE
    default:
      break;
  }
  return "Unknown";
}
}  // namespace

bool PrintRawWasmCode(AccountingAllocator* allocator, const FunctionBody& body,
                      const wasm::WasmModule* module) {
  OFStream os(stdout);
  Zone zone(allocator, ZONE_NAME);
  WasmDecoder<false> decoder(module, body.sig, body.start, body.end);
  int line_nr = 0;

  // Print the function signature.
  if (body.sig) {
    os << "// signature: " << *body.sig << std::endl;
    ++line_nr;
  }

  // Print the local declarations.
  BodyLocalDecls decls(&zone);
  BytecodeIterator i(body.start, body.end, &decls);
  if (body.start != i.pc() && !FLAG_wasm_code_fuzzer_gen_test) {
    os << "// locals: ";
    if (!decls.type_list.empty()) {
      ValueType type = decls.type_list[0];
      uint32_t count = 0;
      for (size_t pos = 0; pos < decls.type_list.size(); ++pos) {
        if (decls.type_list[pos] == type) {
          ++count;
        } else {
          os << " " << count << " " << WasmOpcodes::TypeName(type);
          type = decls.type_list[pos];
          count = 1;
        }
      }
    }
    os << std::endl;
    ++line_nr;

    for (const byte* locals = body.start; locals < i.pc(); locals++) {
      os << (locals == body.start ? "0x" : " 0x") << AsHex(*locals, 2) << ",";
    }
    os << std::endl;
    ++line_nr;
  }

  os << "// body: " << std::endl;
  ++line_nr;
  unsigned control_depth = 0;
  for (; i.has_next(); i.next()) {
    unsigned length = WasmDecoder<false>::OpcodeLength(&decoder, i.pc());

    WasmOpcode opcode = i.current();
    if (opcode == kExprElse) control_depth--;

    int num_whitespaces = control_depth < 32 ? 2 * control_depth : 64;

    // 64 whitespaces
    const char* padding =
        "                                                                ";
    os.write(padding, num_whitespaces);

    os << RawOpcodeName(opcode) << ",";

    for (size_t j = 1; j < length; ++j) {
      os << " 0x" << AsHex(i.pc()[j], 2) << ",";
    }

    switch (opcode) {
      case kExprElse:
        os << "   // @" << i.pc_offset();
        control_depth++;
        break;
      case kExprLoop:
      case kExprIf:
      case kExprBlock:
      case kExprTry: {
        BlockTypeOperand<false> operand(&i, i.pc());
        os << "   // @" << i.pc_offset();
        for (unsigned i = 0; i < operand.arity; i++) {
          os << " " << WasmOpcodes::TypeName(operand.read_entry(i));
        }
        control_depth++;
        break;
      }
      case kExprEnd:
        os << "   // @" << i.pc_offset();
        control_depth--;
        break;
      case kExprBr: {
        BreakDepthOperand<false> operand(&i, i.pc());
        os << "   // depth=" << operand.depth;
        break;
      }
      case kExprBrIf: {
        BreakDepthOperand<false> operand(&i, i.pc());
        os << "   // depth=" << operand.depth;
        break;
      }
      case kExprBrTable: {
        BranchTableOperand<false> operand(&i, i.pc());
        os << " // entries=" << operand.table_count;
        break;
      }
      case kExprCallIndirect: {
        CallIndirectOperand<false> operand(&i, i.pc());
        os << "   // sig #" << operand.index;
        if (decoder.Complete(i.pc(), operand)) {
          os << ": " << *operand.sig;
        }
        break;
      }
      case kExprCallFunction: {
        CallFunctionOperand<false> operand(&i, i.pc());
        os << " // function #" << operand.index;
        if (decoder.Complete(i.pc(), operand)) {
          os << ": " << *operand.sig;
        }
        break;
      }
      default:
        break;
    }
    os << std::endl;
    ++line_nr;
  }

  return decoder.ok();
}

BitVector* AnalyzeLoopAssignmentForTesting(Zone* zone, size_t num_locals,
                                           const byte* start, const byte* end) {
  Decoder decoder(start, end);
  return WasmDecoder<true>::AnalyzeLoopAssignment(
      &decoder, start, static_cast<int>(num_locals), zone);
}

#undef TRACE

}  // namespace wasm
}  // namespace internal
}  // namespace v8
