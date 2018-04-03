// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/baseline/liftoff-assembler.h"

#include "src/assembler-inl.h"
#include "src/base/optional.h"
#include "src/compiler/linkage.h"
#include "src/compiler/wasm-compiler.h"
#include "src/counters.h"
#include "src/macro-assembler-inl.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/memory-tracing.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {
namespace wasm {

constexpr auto kRegister = LiftoffAssembler::VarState::kRegister;
constexpr auto kI32Const = LiftoffAssembler::VarState::kI32Const;
constexpr auto kStack = LiftoffAssembler::VarState::kStack;

namespace {

#define __ asm_->

#define TRACE(...)                                            \
  do {                                                        \
    if (FLAG_trace_liftoff) PrintF("[liftoff] " __VA_ARGS__); \
  } while (false)

#if V8_TARGET_ARCH_ARM64
// On ARM64, the Assembler keeps track of pointers to Labels to resolve
// branches to distant targets. Moving labels would confuse the Assembler,
// thus store the label on the heap and keep a unique_ptr.
class MovableLabel {
 public:
  Label* get() { return label_.get(); }
  MovableLabel() : MovableLabel(new Label()) {}

  static MovableLabel None() { return MovableLabel(nullptr); }

 private:
  std::unique_ptr<Label> label_;
  explicit MovableLabel(Label* label) : label_(label) {}
};
#else
// On all other platforms, just store the Label directly.
class MovableLabel {
 public:
  Label* get() { return &label_; }

  static MovableLabel None() { return MovableLabel(); }

 private:
  Label label_;
};
#endif

class LiftoffCompiler {
 public:
  MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(LiftoffCompiler);

  // TODO(clemensh): Make this a template parameter.
  static constexpr wasm::Decoder::ValidateFlag validate =
      wasm::Decoder::kValidate;

  using Value = ValueBase;

  struct ElseState {
    MovableLabel label;
    LiftoffAssembler::CacheState state;
  };

  struct Control : public ControlWithNamedConstructors<Control, Value> {
    MOVE_ONLY_WITH_DEFAULT_CONSTRUCTORS(Control);

    std::unique_ptr<ElseState> else_state;
    LiftoffAssembler::CacheState label_state;
    MovableLabel label;
  };

  using Decoder = WasmFullDecoder<validate, LiftoffCompiler>;

  struct OutOfLineCode {
    MovableLabel label;
    MovableLabel continuation;
    Builtins::Name builtin;
    wasm::WasmCodePosition position;
    LiftoffRegList regs_to_save;
    uint32_t pc;  // for trap handler.

    // Named constructors:
    static OutOfLineCode Trap(Builtins::Name b, wasm::WasmCodePosition pos,
                              uint32_t pc) {
      return {{}, {}, b, pos, {}, pc};
    }
    static OutOfLineCode StackCheck(wasm::WasmCodePosition pos,
                                    LiftoffRegList regs) {
      return {{}, MovableLabel::None(), Builtins::kWasmStackGuard, pos, regs,
              0};
    }
  };

  LiftoffCompiler(LiftoffAssembler* liftoff_asm,
                  compiler::CallDescriptor* call_desc, compiler::ModuleEnv* env,
                  compiler::RuntimeExceptionSupport runtime_exception_support,
                  SourcePositionTableBuilder* source_position_table_builder,
                  std::vector<trap_handler::ProtectedInstructionData>*
                      protected_instructions,
                  Zone* compilation_zone, std::unique_ptr<Zone>* codegen_zone)
      : asm_(liftoff_asm),
        call_desc_(call_desc),
        env_(env),
        min_size_(env_->module->initial_pages * wasm::kWasmPageSize),
        max_size_((env_->module->has_maximum_pages
                       ? env_->module->maximum_pages
                       : wasm::kV8MaxWasmMemoryPages) *
                  wasm::kWasmPageSize),
        runtime_exception_support_(runtime_exception_support),
        source_position_table_builder_(source_position_table_builder),
        protected_instructions_(protected_instructions),
        compilation_zone_(compilation_zone),
        codegen_zone_(codegen_zone),
        safepoint_table_builder_(compilation_zone_) {
    // Check for overflow in max_size_.
    DCHECK_EQ(max_size_, uint64_t{env_->module->has_maximum_pages
                                      ? env_->module->maximum_pages
                                      : wasm::kV8MaxWasmMemoryPages} *
                             wasm::kWasmPageSize);
  }

  bool ok() const { return ok_; }

  void unsupported(Decoder* decoder, const char* reason) {
    ok_ = false;
    TRACE("unsupported: %s\n", reason);
    decoder->errorf(decoder->pc(), "unsupported liftoff operation: %s", reason);
    BindUnboundLabels(decoder);
  }

  int GetSafepointTableOffset() const {
    return safepoint_table_builder_.GetCodeOffset();
  }

  void BindUnboundLabels(Decoder* decoder) {
#ifdef DEBUG
    // Bind all labels now, otherwise their destructor will fire a DCHECK error
    // if they where referenced before.
    for (uint32_t i = 0, e = decoder->control_depth(); i < e; ++i) {
      Control* c = decoder->control_at(i);
      Label* label = c->label.get();
      if (!label->is_bound()) __ bind(label);
      if (c->else_state) {
        Label* else_label = c->else_state->label.get();
        if (!else_label->is_bound()) __ bind(else_label);
      }
    }
    for (auto& ool : out_of_line_code_) {
      if (!ool.label.get()->is_bound()) __ bind(ool.label.get());
    }
#endif
  }

  void CheckStackSizeLimit(Decoder* decoder) {
    DCHECK_GE(__ cache_state()->stack_height(), __ num_locals());
    int stack_height = __ cache_state()->stack_height() - __ num_locals();
    if (stack_height > LiftoffAssembler::kMaxValueStackHeight) {
      unsupported(decoder, "value stack grows too large");
    }
  }

  void StartFunction(Decoder* decoder) {
    int num_locals = decoder->NumLocals();
    __ set_num_locals(num_locals);
    for (int i = 0; i < num_locals; ++i) {
      __ set_local_type(i, decoder->GetLocalType(i));
    }
  }

  void ProcessParameter(uint32_t param_idx, uint32_t input_location) {
    ValueType type = __ local_type(param_idx);
    RegClass rc = reg_class_for(type);
    compiler::LinkageLocation param_loc =
        call_desc_->GetInputLocation(input_location);
    if (param_loc.IsRegister()) {
      DCHECK(!param_loc.IsAnyRegister());
      int reg_code = param_loc.AsRegister();
      LiftoffRegister reg =
          rc == kGpReg ? LiftoffRegister(Register::from_code(reg_code))
                       : LiftoffRegister(DoubleRegister::from_code(reg_code));
      LiftoffRegList cache_regs =
          rc == kGpReg ? kGpCacheRegList : kFpCacheRegList;
      if (cache_regs.has(reg)) {
        // This is a cache register, just use it.
        __ PushRegister(type, reg);
        return;
      }
      // Move to a cache register.
      LiftoffRegister cache_reg = __ GetUnusedRegister(rc);
      __ Move(cache_reg, reg);
      __ PushRegister(type, reg);
      return;
    }
    if (param_loc.IsCallerFrameSlot()) {
      LiftoffRegister tmp_reg = __ GetUnusedRegister(rc);
      __ LoadCallerFrameSlot(tmp_reg, -param_loc.AsCallerFrameSlot());
      __ PushRegister(type, tmp_reg);
      return;
    }
    UNREACHABLE();
  }

  void StackCheck(wasm::WasmCodePosition position) {
    if (FLAG_wasm_no_stack_checks || !runtime_exception_support_) return;
    out_of_line_code_.push_back(
        OutOfLineCode::StackCheck(position, __ cache_state()->used_registers));
    OutOfLineCode& ool = out_of_line_code_.back();
    __ StackCheck(ool.label.get());
    __ bind(ool.continuation.get());
  }

  void StartFunctionBody(Decoder* decoder, Control* block) {
    if (!kLiftoffAssemblerImplementedOnThisPlatform) {
      unsupported(decoder, "platform");
      return;
    }
    __ EnterFrame(StackFrame::WASM_COMPILED);
    __ set_has_frame(true);
    __ ReserveStackSpace(LiftoffAssembler::kStackSlotSize *
                         __ GetTotalFrameSlotCount());
    // Parameter 0 is the wasm context.
    uint32_t num_params =
        static_cast<uint32_t>(call_desc_->ParameterCount()) - 1;
    for (uint32_t i = 0; i < __ num_locals(); ++i) {
      switch (__ local_type(i)) {
        case kWasmI32:
        case kWasmF32:
          // supported.
          break;
        case kWasmI64:
          unsupported(decoder, "i64 param/local");
          return;
        case kWasmF64:
          unsupported(decoder, "f64 param/local");
          return;
        default:
          unsupported(decoder, "exotic param/local");
          return;
      }
    }
    // Input 0 is the call target, the context is at 1.
    constexpr int kContextParameterIndex = 1;
    // Store the context parameter to a special stack slot.
    compiler::LinkageLocation context_loc =
        call_desc_->GetInputLocation(kContextParameterIndex);
    DCHECK(context_loc.IsRegister());
    DCHECK(!context_loc.IsAnyRegister());
    Register context_reg = Register::from_code(context_loc.AsRegister());
    __ SpillContext(context_reg);
    uint32_t param_idx = 0;
    for (; param_idx < num_params; ++param_idx) {
      constexpr int kFirstActualParameterIndex = kContextParameterIndex + 1;
      ProcessParameter(param_idx, param_idx + kFirstActualParameterIndex);
    }
    // Set to a gp register, to mark this uninitialized.
    LiftoffRegister zero_double_reg(Register::from_code<0>());
    DCHECK(zero_double_reg.is_gp());
    for (; param_idx < __ num_locals(); ++param_idx) {
      ValueType type = decoder->GetLocalType(param_idx);
      switch (type) {
        case kWasmI32:
          __ cache_state()->stack_state.emplace_back(kWasmI32, uint32_t{0});
          break;
        case kWasmF32:
          if (zero_double_reg.is_gp()) {
            // Note: This might spill one of the registers used to hold
            // parameters.
            zero_double_reg = __ GetUnusedRegister(kFpReg);
            __ LoadConstant(zero_double_reg, WasmValue(0.f));
          }
          __ PushRegister(kWasmF32, zero_double_reg);
          break;
        default:
          UNIMPLEMENTED();
      }
    }
    block->label_state.stack_base = __ num_locals();

    // The function-prologue stack check is associated with position 0, which
    // is never a position of any instruction in the function.
    StackCheck(0);

    DCHECK_EQ(__ num_locals(), param_idx);
    DCHECK_EQ(__ num_locals(), __ cache_state()->stack_height());
    CheckStackSizeLimit(decoder);
  }

  void GenerateOutOfLineCode(OutOfLineCode& ool) {
    __ bind(ool.label.get());
    const bool is_stack_check = ool.builtin == Builtins::kWasmStackGuard;
    if (!runtime_exception_support_) {
      // We cannot test calls to the runtime in cctest/test-run-wasm.
      // Therefore we emit a call to C here instead of a call to the runtime.
      // In this mode, we never generate stack checks.
      DCHECK(!is_stack_check);
      __ CallTrapCallbackForTesting();
      __ LeaveFrame(StackFrame::WASM_COMPILED);
      __ Ret();
      return;
    }

    if (!is_stack_check && env_->use_trap_handler) {
      uint32_t pc = static_cast<uint32_t>(__ pc_offset());
      DCHECK_EQ(pc, __ pc_offset());
      protected_instructions_->emplace_back(
          trap_handler::ProtectedInstructionData{ool.pc, pc});
    }

    if (!ool.regs_to_save.is_empty()) __ PushRegisters(ool.regs_to_save);

    source_position_table_builder_->AddPosition(
        __ pc_offset(), SourcePosition(ool.position), false);
    __ Call(__ isolate()->builtins()->builtin_handle(ool.builtin),
            RelocInfo::CODE_TARGET);
    safepoint_table_builder_.DefineSafepoint(asm_, Safepoint::kSimple, 0,
                                             Safepoint::kNoLazyDeopt);
    DCHECK_EQ(ool.continuation.get()->is_bound(), is_stack_check);
    if (!ool.regs_to_save.is_empty()) __ PopRegisters(ool.regs_to_save);
    if (is_stack_check) {
      __ emit_jump(ool.continuation.get());
    } else {
      __ AssertUnreachable(AbortReason::kUnexpectedReturnFromWasmTrap);
    }
  }

  void FinishFunction(Decoder* decoder) {
    for (OutOfLineCode& ool : out_of_line_code_) {
      GenerateOutOfLineCode(ool);
    }
    safepoint_table_builder_.Emit(asm_, __ GetTotalFrameSlotCount());
  }

  void OnFirstError(Decoder* decoder) {
    ok_ = false;
    BindUnboundLabels(decoder);
  }

  void NextInstruction(Decoder* decoder, WasmOpcode) {
    TraceCacheState(decoder);
  }

  void Block(Decoder* decoder, Control* block) {
    block->label_state.stack_base = __ cache_state()->stack_height();
  }

  void Loop(Decoder* decoder, Control* loop) {
    loop->label_state.stack_base = __ cache_state()->stack_height();

    // Before entering a loop, spill all locals to the stack, in order to free
    // the cache registers, and to avoid unnecessarily reloading stack values
    // into registers at branches.
    // TODO(clemensh): Come up with a better strategy here, involving
    // pre-analysis of the function.
    __ SpillLocals();

    // Loop labels bind at the beginning of the block.
    __ bind(loop->label.get());

    // Save the current cache state for the merge when jumping to this loop.
    loop->label_state.Split(*__ cache_state());

    // Execute a stack check in the loop header.
    StackCheck(decoder->position());
  }

  void Try(Decoder* decoder, Control* block) { unsupported(decoder, "try"); }

  void If(Decoder* decoder, const Value& cond, Control* if_block) {
    DCHECK_EQ(if_block, decoder->control_at(0));
    DCHECK(if_block->is_if());

    if (if_block->start_merge.arity > 0 || if_block->end_merge.arity > 1)
      return unsupported(decoder, "multi-value if");

    // Allocate the else state.
    if_block->else_state = base::make_unique<ElseState>();

    // Test the condition, jump to else if zero.
    Register value = __ PopToRegister(kGpReg).gp();
    __ emit_i32_test(value);
    __ emit_cond_jump(kEqual, if_block->else_state->label.get());

    if_block->label_state.stack_base = __ cache_state()->stack_height();
    // Store the state (after popping the value) for executing the else branch.
    if_block->else_state->state.Split(*__ cache_state());
  }

  void FallThruTo(Decoder* decoder, Control* c) {
    if (c->end_merge.reached) {
      __ MergeFullStackWith(c->label_state);
    } else if (c->is_onearmed_if()) {
      c->label_state.InitMerge(*__ cache_state(), __ num_locals(),
                               c->br_merge()->arity);
      __ MergeFullStackWith(c->label_state);
    } else {
      c->label_state.Split(*__ cache_state());
    }
    TraceCacheState(decoder);
  }

  void PopControl(Decoder* decoder, Control* c) {
    if (!c->is_loop() && c->end_merge.reached) {
      __ cache_state()->Steal(c->label_state);
    }
    if (!c->label.get()->is_bound()) {
      __ bind(c->label.get());
    }
  }

  void EndControl(Decoder* decoder, Control* c) {}

  void GenerateCCall(Register res_reg, uint32_t num_args,
                     const Register* arg_regs, ExternalReference ext_ref) {
    static constexpr int kNumReturns = 1;
    static constexpr int kMaxArgs = 2;
    static constexpr MachineType kReps[]{
        MachineType::Uint32(), MachineType::Pointer(), MachineType::Pointer()};
    static_assert(arraysize(kReps) == kNumReturns + kMaxArgs, "mismatch");
    DCHECK_LE(num_args, kMaxArgs);

    MachineSignature sig(kNumReturns, num_args, kReps);
    compiler::CallDescriptor* desc =
        compiler::Linkage::GetSimplifiedCDescriptor(compilation_zone_, &sig);

    // Before making a call, spill all cache registers.
    __ SpillAllRegisters();

    // Store arguments on our stack, then align the stack for calling to C.
    uint32_t num_params = static_cast<uint32_t>(desc->ParameterCount());
    __ PrepareCCall(num_params, arg_regs);

    // Set parameters (in sp[0], sp[8], ...).
    uint32_t num_stack_params = 0;
    for (uint32_t param = 0; param < num_params; ++param) {
      constexpr size_t kInputShift = 1;  // Input 0 is the call target.

      compiler::LinkageLocation loc =
          desc->GetInputLocation(param + kInputShift);
      if (loc.IsRegister()) {
        Register reg = Register::from_code(loc.AsRegister());
        // Load address of that parameter to the register.
        __ SetCCallRegParamAddr(reg, param, num_params);
      } else {
        DCHECK(loc.IsCallerFrameSlot());
        __ SetCCallStackParamAddr(num_stack_params, param, num_params);
        ++num_stack_params;
      }
    }

    // Now execute the call.
    __ CallC(ext_ref, num_params);

    // Load return value.
    compiler::LinkageLocation return_loc = desc->GetReturnLocation(0);
    DCHECK(return_loc.IsRegister());
    Register return_reg = Register::from_code(return_loc.AsRegister());
    if (return_reg != res_reg) {
      __ Move(LiftoffRegister(res_reg), LiftoffRegister(return_reg));
    }
  }

  void I32UnOp(bool (LiftoffAssembler::*emit_fn)(Register, Register),
               ExternalReference (*fallback_fn)(Isolate*)) {
    LiftoffRegList pinned;
    LiftoffRegister dst_reg = pinned.set(__ GetUnaryOpTargetRegister(kGpReg));
    LiftoffRegister src_reg = pinned.set(__ PopToRegister(kGpReg, pinned));
    if (!emit_fn || !(asm_->*emit_fn)(dst_reg.gp(), src_reg.gp())) {
      ExternalReference ext_ref = fallback_fn(asm_->isolate());
      Register args[] = {src_reg.gp()};
      GenerateCCall(dst_reg.gp(), arraysize(args), args, ext_ref);
    }
    __ PushRegister(kWasmI32, dst_reg);
  }

  void UnOp(Decoder* decoder, WasmOpcode opcode, FunctionSig*,
            const Value& value, Value* result) {
#define CASE_UNOP(opcode, type, fn, ext_ref_fn)           \
  case WasmOpcode::kExpr##opcode:                         \
    type##UnOp(&LiftoffAssembler::emit_##fn, ext_ref_fn); \
    break;
    switch (opcode) {
      CASE_UNOP(I32Eqz, I32, i32_eqz, nullptr)
      CASE_UNOP(I32Clz, I32, i32_clz, nullptr)
      CASE_UNOP(I32Ctz, I32, i32_ctz, nullptr)
      CASE_UNOP(I32Popcnt, I32, i32_popcnt,
                &ExternalReference::wasm_word32_popcnt)
      default:
        return unsupported(decoder, WasmOpcodes::OpcodeName(opcode));
    }
#undef CASE_UNOP
  }

  void I32BinOp(void (LiftoffAssembler::*emit_fn)(Register, Register,
                                                  Register)) {
    LiftoffRegList pinned;
    LiftoffRegister dst_reg = pinned.set(__ GetBinaryOpTargetRegister(kGpReg));
    LiftoffRegister rhs_reg = pinned.set(__ PopToRegister(kGpReg, pinned));
    LiftoffRegister lhs_reg = __ PopToRegister(kGpReg, pinned);
    (asm_->*emit_fn)(dst_reg.gp(), lhs_reg.gp(), rhs_reg.gp());
    __ PushRegister(kWasmI32, dst_reg);
  }

  void I32CCallBinOp(ExternalReference ext_ref) {
    LiftoffRegList pinned;
    LiftoffRegister dst_reg = pinned.set(__ GetBinaryOpTargetRegister(kGpReg));
    LiftoffRegister rhs_reg = pinned.set(__ PopToRegister(kGpReg, pinned));
    LiftoffRegister lhs_reg = __ PopToRegister(kGpReg, pinned);
    Register args[] = {lhs_reg.gp(), rhs_reg.gp()};
    GenerateCCall(dst_reg.gp(), arraysize(args), args, ext_ref);
    __ PushRegister(kWasmI32, dst_reg);
  }

  void F32BinOp(void (LiftoffAssembler::*emit_fn)(DoubleRegister,
                                                  DoubleRegister,
                                                  DoubleRegister)) {
    LiftoffRegList pinned;
    LiftoffRegister target_reg =
        pinned.set(__ GetBinaryOpTargetRegister(kFpReg));
    LiftoffRegister rhs_reg = pinned.set(__ PopToRegister(kFpReg, pinned));
    LiftoffRegister lhs_reg = __ PopToRegister(kFpReg, pinned);
    (asm_->*emit_fn)(target_reg.fp(), lhs_reg.fp(), rhs_reg.fp());
    __ PushRegister(kWasmF32, target_reg);
  }

  void BinOp(Decoder* decoder, WasmOpcode opcode, FunctionSig*,
             const Value& lhs, const Value& rhs, Value* result) {
#define CASE_BINOP(opcode, type, fn) \
  case WasmOpcode::kExpr##opcode:    \
    return type##BinOp(&LiftoffAssembler::emit_##fn);
#define CASE_CCALL_BINOP(opcode, type, ext_ref_fn)                    \
  case WasmOpcode::kExpr##opcode:                                     \
    type##CCallBinOp(ExternalReference::ext_ref_fn(asm_->isolate())); \
    break;
    switch (opcode) {
      CASE_BINOP(I32Add, I32, i32_add)
      CASE_BINOP(I32Sub, I32, i32_sub)
      CASE_BINOP(I32Mul, I32, i32_mul)
      CASE_BINOP(I32And, I32, i32_and)
      CASE_BINOP(I32Ior, I32, i32_or)
      CASE_BINOP(I32Xor, I32, i32_xor)
      CASE_BINOP(I32Shl, I32, i32_shl)
      CASE_BINOP(I32ShrS, I32, i32_sar)
      CASE_BINOP(I32ShrU, I32, i32_shr)
      CASE_CCALL_BINOP(I32Rol, I32, wasm_word32_rol)
      CASE_CCALL_BINOP(I32Ror, I32, wasm_word32_ror)
      CASE_BINOP(F32Add, F32, f32_add)
      CASE_BINOP(F32Sub, F32, f32_sub)
      CASE_BINOP(F32Mul, F32, f32_mul)
      default:
        return unsupported(decoder, WasmOpcodes::OpcodeName(opcode));
    }
#undef CASE_BINOP
#undef CASE_CCALL_BINOP
  }

  void I32Const(Decoder* decoder, Value* result, int32_t value) {
    __ cache_state()->stack_state.emplace_back(kWasmI32, value);
    CheckStackSizeLimit(decoder);
  }

  void I64Const(Decoder* decoder, Value* result, int64_t value) {
    unsupported(decoder, "i64.const");
  }

  void F32Const(Decoder* decoder, Value* result, float value) {
    LiftoffRegister reg = __ GetUnusedRegister(kFpReg);
    __ LoadConstant(reg, WasmValue(value));
    __ PushRegister(kWasmF32, reg);
    CheckStackSizeLimit(decoder);
  }

  void F64Const(Decoder* decoder, Value* result, double value) {
    unsupported(decoder, "f64.const");
  }

  void Drop(Decoder* decoder, const Value& value) {
    __ DropStackSlot(&__ cache_state()->stack_state.back());
    __ cache_state()->stack_state.pop_back();
  }

  void DoReturn(Decoder* decoder, Vector<Value> values, bool implicit) {
    if (implicit) {
      DCHECK_EQ(1, decoder->control_depth());
      Control* func_block = decoder->control_at(0);
      __ bind(func_block->label.get());
      __ cache_state()->Steal(func_block->label_state);
    }
    if (!values.is_empty()) {
      if (values.size() > 1) return unsupported(decoder, "multi-return");
      RegClass rc = reg_class_for(values[0].type);
      LiftoffRegister reg = __ PopToRegister(rc);
      __ MoveToReturnRegister(reg);
    }
    __ LeaveFrame(StackFrame::WASM_COMPILED);
    __ DropStackSlotsAndRet(
        static_cast<uint32_t>(call_desc_->StackParameterCount()));
  }

  void GetLocal(Decoder* decoder, Value* result,
                const LocalIndexOperand<validate>& operand) {
    auto& slot = __ cache_state()->stack_state[operand.index];
    DCHECK_EQ(slot.type(), operand.type);
    switch (slot.loc()) {
      case kRegister:
        __ PushRegister(slot.type(), slot.reg());
        break;
      case kI32Const:
        __ cache_state()->stack_state.emplace_back(operand.type,
                                                   slot.i32_const());
        break;
      case kStack: {
        auto rc = reg_class_for(operand.type);
        LiftoffRegister reg = __ GetUnusedRegister(rc);
        __ Fill(reg, operand.index);
        __ PushRegister(slot.type(), reg);
        break;
      }
    }
    CheckStackSizeLimit(decoder);
  }

  void SetLocalFromStackSlot(LiftoffAssembler::VarState& dst_slot,
                             uint32_t local_index) {
    auto& state = *__ cache_state();
    if (dst_slot.is_reg()) {
      LiftoffRegister slot_reg = dst_slot.reg();
      if (state.get_use_count(slot_reg) == 1) {
        __ Fill(dst_slot.reg(), state.stack_height() - 1);
        return;
      }
      state.dec_used(slot_reg);
    }
    ValueType type = dst_slot.type();
    DCHECK_EQ(type, __ local_type(local_index));
    RegClass rc = reg_class_for(type);
    LiftoffRegister dst_reg = __ GetUnusedRegister(rc);
    __ Fill(dst_reg, __ cache_state()->stack_height() - 1);
    dst_slot = LiftoffAssembler::VarState(type, dst_reg);
    __ cache_state()->inc_used(dst_reg);
  }

  void SetLocal(uint32_t local_index, bool is_tee) {
    auto& state = *__ cache_state();
    auto& source_slot = state.stack_state.back();
    auto& target_slot = state.stack_state[local_index];
    switch (source_slot.loc()) {
      case kRegister:
        __ DropStackSlot(&target_slot);
        target_slot = source_slot;
        if (is_tee) state.inc_used(target_slot.reg());
        break;
      case kI32Const:
        __ DropStackSlot(&target_slot);
        target_slot = source_slot;
        break;
      case kStack:
        SetLocalFromStackSlot(target_slot, local_index);
        break;
    }
    if (!is_tee) __ cache_state()->stack_state.pop_back();
  }

  void SetLocal(Decoder* decoder, const Value& value,
                const LocalIndexOperand<validate>& operand) {
    SetLocal(operand.index, false);
  }

  void TeeLocal(Decoder* decoder, const Value& value, Value* result,
                const LocalIndexOperand<validate>& operand) {
    SetLocal(operand.index, true);
  }

  void GetGlobal(Decoder* decoder, Value* result,
                 const GlobalIndexOperand<validate>& operand) {
    const auto* global = &env_->module->globals[operand.index];
    if (global->type != kWasmI32 && global->type != kWasmI64)
      return unsupported(decoder, "non-int global");
    LiftoffRegList pinned;
    Register addr = pinned.set(__ GetUnusedRegister(kGpReg)).gp();
    __ LoadFromContext(addr, offsetof(WasmContext, globals_start),
                       kPointerSize);
    LiftoffRegister value =
        pinned.set(__ GetUnusedRegister(reg_class_for(global->type), pinned));
    LoadType type =
        global->type == kWasmI32 ? LoadType::kI32Load : LoadType::kI64Load;
    if (type.size() > kPointerSize)
      return unsupported(decoder, "global > kPointerSize");
    __ Load(value, addr, no_reg, global->offset, type, pinned);
    __ PushRegister(global->type, value);
    CheckStackSizeLimit(decoder);
  }

  void SetGlobal(Decoder* decoder, const Value& value,
                 const GlobalIndexOperand<validate>& operand) {
    auto* global = &env_->module->globals[operand.index];
    if (global->type != kWasmI32) return unsupported(decoder, "non-i32 global");
    LiftoffRegList pinned;
    Register addr = pinned.set(__ GetUnusedRegister(kGpReg)).gp();
    __ LoadFromContext(addr, offsetof(WasmContext, globals_start),
                       kPointerSize);
    LiftoffRegister reg =
        pinned.set(__ PopToRegister(reg_class_for(global->type), pinned));
    StoreType type =
        global->type == kWasmI32 ? StoreType::kI32Store : StoreType::kI64Store;
    __ Store(addr, no_reg, global->offset, reg, type, pinned);
  }

  void Unreachable(Decoder* decoder) { unsupported(decoder, "unreachable"); }

  void Select(Decoder* decoder, const Value& cond, const Value& fval,
              const Value& tval, Value* result) {
    unsupported(decoder, "select");
  }

  void Br(Control* target) {
    if (!target->br_merge()->reached) {
      target->label_state.InitMerge(*__ cache_state(), __ num_locals(),
                                    target->br_merge()->arity);
    }
    __ MergeStackWith(target->label_state, target->br_merge()->arity);
    __ jmp(target->label.get());
  }

  void Br(Decoder* decoder, Control* target) {
    Br(target);
  }

  void BrIf(Decoder* decoder, const Value& cond, Control* target) {
    Label cont_false;
    Register value = __ PopToRegister(kGpReg).gp();
    __ emit_i32_test(value);
    __ emit_cond_jump(kEqual, &cont_false);

    Br(target);
    __ bind(&cont_false);
  }

  void BrTable(Decoder* decoder, const BranchTableOperand<validate>& operand,
               const Value& key) {
    unsupported(decoder, "br_table");
  }

  void Else(Decoder* decoder, Control* if_block) {
    if (if_block->reachable()) __ emit_jump(if_block->label.get());
    __ bind(if_block->else_state->label.get());
    __ cache_state()->Steal(if_block->else_state->state);
  }

  Label* AddOutOfLineTrap(wasm::WasmCodePosition position, uint32_t pc = 0) {
    DCHECK(!FLAG_wasm_no_bounds_checks);
    // The pc is needed exactly if trap handlers are enabled.
    DCHECK_EQ(pc != 0, env_->use_trap_handler);

    out_of_line_code_.push_back(OutOfLineCode::Trap(
        Builtins::kThrowWasmTrapMemOutOfBounds, position, pc));
    return out_of_line_code_.back().label.get();
  }

  void BoundsCheckMem(uint32_t access_size, uint32_t offset, Register index,
                      wasm::WasmCodePosition position, LiftoffRegList pinned) {
    DCHECK(!env_->use_trap_handler);
    if (FLAG_wasm_no_bounds_checks) return;

    Label* trap_label = AddOutOfLineTrap(position);

    if (access_size > max_size_ || offset > max_size_ - access_size) {
      // The access will be out of bounds, even for the largest memory.
      __ emit_jump(trap_label);
      return;
    }
    uint32_t end_offset = offset + access_size - 1;

    // If the end offset is larger than the smallest memory, dynamically check
    // the end offset against the actual memory size, which is not known at
    // compile time. Otherwise, only one check is required (see below).
    LiftoffRegister end_offset_reg =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LiftoffRegister mem_size = __ GetUnusedRegister(kGpReg, pinned);
    __ LoadFromContext(mem_size.gp(), offsetof(WasmContext, mem_size), 4);
    __ LoadConstant(end_offset_reg, WasmValue(end_offset));
    if (end_offset >= min_size_) {
      __ emit_i32_compare(end_offset_reg.gp(), mem_size.gp());
      __ emit_cond_jump(kUnsignedGreaterEqual, trap_label);
    }

    // Just reuse the end_offset register for computing the effective size.
    LiftoffRegister effective_size_reg = end_offset_reg;
    __ emit_i32_sub(effective_size_reg.gp(), mem_size.gp(),
                    end_offset_reg.gp());

    __ emit_i32_compare(index, effective_size_reg.gp());
    __ emit_cond_jump(kUnsignedGreaterEqual, trap_label);
  }

  void TraceMemoryOperation(bool is_store, MachineRepresentation rep,
                            Register index, uint32_t offset,
                            WasmCodePosition position) {
    // Before making the runtime call, spill all cache registers.
    __ SpillAllRegisters();

    LiftoffRegList pinned = LiftoffRegList::ForRegs(index);
    // Get one register for computing the address (offset + index).
    LiftoffRegister address = pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    // Compute offset+index in address.
    __ LoadConstant(address, WasmValue(offset));
    __ emit_i32_add(address.gp(), address.gp(), index);

    // Get a register to hold the stack slot for wasm::MemoryTracingInfo.
    LiftoffRegister info = pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    // Allocate stack slot for wasm::MemoryTracingInfo.
    __ AllocateStackSlot(info.gp(), sizeof(wasm::MemoryTracingInfo));

    // Now store all information into the wasm::MemoryTracingInfo struct.
    __ Store(info.gp(), no_reg, offsetof(wasm::MemoryTracingInfo, address),
             address, StoreType::kI32Store, pinned);
    __ LoadConstant(address, WasmValue(is_store ? 1 : 0));
    __ Store(info.gp(), no_reg, offsetof(wasm::MemoryTracingInfo, is_store),
             address, StoreType::kI32Store8, pinned);
    __ LoadConstant(address, WasmValue(static_cast<int>(rep)));
    __ Store(info.gp(), no_reg, offsetof(wasm::MemoryTracingInfo, mem_rep),
             address, StoreType::kI32Store8, pinned);

    source_position_table_builder_->AddPosition(
        __ pc_offset(), SourcePosition(position), false);

    Register args[] = {info.gp()};
    GenerateRuntimeCall(arraysize(args), args);
  }

  void GenerateRuntimeCall(int num_args, Register* args) {
    compiler::CallDescriptor* desc =
        compiler::Linkage::GetRuntimeCallDescriptor(
            compilation_zone_, Runtime::kWasmTraceMemory, num_args,
            compiler::Operator::kNoProperties,
            compiler::CallDescriptor::kNoFlags);
    // Currently, only one argument is supported. More arguments require some
    // caution for the parallel register moves (reuse StackTransferRecipe).
    DCHECK_EQ(1, num_args);
    constexpr size_t kInputShift = 1;  // Input 0 is the call target.
    compiler::LinkageLocation param_loc = desc->GetInputLocation(kInputShift);
    if (param_loc.IsRegister()) {
      Register reg = Register::from_code(param_loc.AsRegister());
      __ Move(LiftoffRegister(reg), LiftoffRegister(args[0]));
    } else {
      DCHECK(param_loc.IsCallerFrameSlot());
      __ PushCallerFrameSlot(LiftoffRegister(args[0]));
    }

    // Allocate the codegen zone if not done before.
    if (!*codegen_zone_) {
      codegen_zone_->reset(
          new Zone(__ isolate()->allocator(), "LiftoffCodegenZone"));
    }
    __ CallRuntime(codegen_zone_->get(), Runtime::kWasmTraceMemory);
    __ DeallocateStackSlot(sizeof(wasm::MemoryTracingInfo));
  }

  void LoadMem(Decoder* decoder, LoadType type,
               const MemoryAccessOperand<validate>& operand,
               const Value& index_val, Value* result) {
    ValueType value_type = type.value_type();
    if (value_type != kWasmI32 && value_type != kWasmF32)
      return unsupported(decoder, "unsupported load type");
    LiftoffRegList pinned;
    Register index = pinned.set(__ PopToRegister(kGpReg)).gp();
    if (!env_->use_trap_handler) {
      // Emit an explicit bounds check.
      BoundsCheckMem(type.size(), operand.offset, index, decoder->position(),
                     pinned);
    }
    Register addr = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    __ LoadFromContext(addr, offsetof(WasmContext, mem_start), kPointerSize);
    RegClass rc = reg_class_for(value_type);
    LiftoffRegister value = pinned.set(__ GetUnusedRegister(rc, pinned));
    uint32_t protected_load_pc = 0;
    __ Load(value, addr, index, operand.offset, type, pinned,
            &protected_load_pc);
    if (env_->use_trap_handler) {
      AddOutOfLineTrap(decoder->position(), protected_load_pc);
    }
    __ PushRegister(value_type, value);
    CheckStackSizeLimit(decoder);

    if (FLAG_wasm_trace_memory) {
      TraceMemoryOperation(false, type.mem_type().representation(), index,
                           operand.offset, decoder->position());
    }
  }

  void StoreMem(Decoder* decoder, StoreType type,
                const MemoryAccessOperand<validate>& operand,
                const Value& index_val, const Value& value_val) {
    ValueType value_type = type.value_type();
    if (value_type != kWasmI32 && value_type != kWasmF32)
      return unsupported(decoder, "unsupported store type");
    RegClass rc = reg_class_for(value_type);
    LiftoffRegList pinned;
    LiftoffRegister value = pinned.set(__ PopToRegister(rc));
    Register index = pinned.set(__ PopToRegister(kGpReg, pinned)).gp();
    if (!env_->use_trap_handler) {
      // Emit an explicit bounds check.
      BoundsCheckMem(type.size(), operand.offset, index, decoder->position(),
                     pinned);
    }
    Register addr = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    __ LoadFromContext(addr, offsetof(WasmContext, mem_start), kPointerSize);
    uint32_t protected_store_pc = 0;
    __ Store(addr, index, operand.offset, value, type, pinned,
             &protected_store_pc);
    if (env_->use_trap_handler) {
      AddOutOfLineTrap(decoder->position(), protected_store_pc);
    }
    if (FLAG_wasm_trace_memory) {
      TraceMemoryOperation(true, type.mem_rep(), index, operand.offset,
                           decoder->position());
    }
  }

  void CurrentMemoryPages(Decoder* decoder, Value* result) {
    unsupported(decoder, "current_memory");
  }
  void GrowMemory(Decoder* decoder, const Value& value, Value* result) {
    unsupported(decoder, "grow_memory");
  }

  void CallDirect(Decoder* decoder,
                  const CallFunctionOperand<validate>& operand,
                  const Value args[], Value returns[]) {
    if (operand.sig->return_count() > 1)
      return unsupported(decoder, "multi-return");

    compiler::CallDescriptor* call_desc =
        compiler::GetWasmCallDescriptor(compilation_zone_, operand.sig);

    __ PrepareCall(operand.sig, call_desc);

    source_position_table_builder_->AddPosition(
        __ pc_offset(), SourcePosition(decoder->position()), false);

    if (FLAG_wasm_jit_to_native) {
      // Just encode the function index. This will be patched at instantiation.
      Address addr = reinterpret_cast<Address>(operand.index);
      __ CallNativeWasmCode(addr);
    } else {
      Handle<Code> target = operand.index < env_->function_code.size()
                                ? env_->function_code[operand.index]
                                : env_->default_function_code;
      __ Call(target, RelocInfo::CODE_TARGET);
    }

    safepoint_table_builder_.DefineSafepoint(asm_, Safepoint::kSimple, 0,
                                             Safepoint::kNoLazyDeopt);

    __ FinishCall(operand.sig, call_desc);
  }

  void CallIndirect(Decoder* decoder, const Value& index,
                    const CallIndirectOperand<validate>& operand,
                    const Value args[], Value returns[]) {
    unsupported(decoder, "call_indirect");
  }
  void SimdOp(Decoder* decoder, WasmOpcode opcode, Vector<Value> args,
              Value* result) {
    unsupported(decoder, "simd");
  }
  void SimdLaneOp(Decoder* decoder, WasmOpcode opcode,
                  const SimdLaneOperand<validate>& operand,
                  const Vector<Value> inputs, Value* result) {
    unsupported(decoder, "simd");
  }
  void SimdShiftOp(Decoder* decoder, WasmOpcode opcode,
                   const SimdShiftOperand<validate>& operand,
                   const Value& input, Value* result) {
    unsupported(decoder, "simd");
  }
  void Simd8x16ShuffleOp(Decoder* decoder,
                         const Simd8x16ShuffleOperand<validate>& operand,
                         const Value& input0, const Value& input1,
                         Value* result) {
    unsupported(decoder, "simd");
  }
  void Throw(Decoder* decoder, const ExceptionIndexOperand<validate>&,
             Control* block, const Vector<Value>& args) {
    unsupported(decoder, "throw");
  }
  void CatchException(Decoder* decoder,
                      const ExceptionIndexOperand<validate>& operand,
                      Control* block, Vector<Value> caught_values) {
    unsupported(decoder, "catch");
  }
  void AtomicOp(Decoder* decoder, WasmOpcode opcode, Vector<Value> args,
                const MemoryAccessOperand<validate>& operand, Value* result) {
    unsupported(decoder, "atomicop");
  }

 private:
  LiftoffAssembler* const asm_;
  compiler::CallDescriptor* const call_desc_;
  compiler::ModuleEnv* const env_;
  // {min_size_} and {max_size_} are cached values computed from the ModuleEnv.
  const uint32_t min_size_;
  const uint32_t max_size_;
  const compiler::RuntimeExceptionSupport runtime_exception_support_;
  bool ok_ = true;
  std::vector<OutOfLineCode> out_of_line_code_;
  SourcePositionTableBuilder* const source_position_table_builder_;
  std::vector<trap_handler::ProtectedInstructionData>* protected_instructions_;
  // Zone used to store information during compilation. The result will be
  // stored independently, such that this zone can die together with the
  // LiftoffCompiler after compilation.
  Zone* compilation_zone_;
  // This zone is allocated when needed, held externally, and survives until
  // code generation (in FinishCompilation).
  std::unique_ptr<Zone>* codegen_zone_;
  SafepointTableBuilder safepoint_table_builder_;

  void TraceCacheState(Decoder* decoder) const {
#ifdef DEBUG
    if (!FLAG_trace_liftoff || !FLAG_trace_wasm_decoder) return;
    OFStream os(stdout);
    for (int control_depth = decoder->control_depth() - 1; control_depth >= -1;
         --control_depth) {
      LiftoffAssembler::CacheState* cache_state =
          control_depth == -1
              ? asm_->cache_state()
              : &decoder->control_at(control_depth)->label_state;
      bool first = true;
      for (LiftoffAssembler::VarState& slot : cache_state->stack_state) {
        os << (first ? "" : "-") << slot;
        first = false;
      }
      if (control_depth != -1) PrintF("; ");
    }
    os << "\n";
#endif
  }
};

}  // namespace
}  // namespace wasm

bool compiler::WasmCompilationUnit::ExecuteLiftoffCompilation() {
  base::ElapsedTimer compile_timer;
  if (FLAG_trace_wasm_decode_time) {
    compile_timer.Start();
  }

  Zone zone(isolate_->allocator(), "LiftoffCompilationZone");
  const wasm::WasmModule* module = env_ ? env_->module : nullptr;
  auto* call_desc = compiler::GetWasmCallDescriptor(&zone, func_body_.sig);
  base::Optional<TimedHistogramScope> liftoff_compile_time_scope(
      base::in_place, counters()->liftoff_compile_time());
  wasm::WasmFullDecoder<wasm::Decoder::kValidate, wasm::LiftoffCompiler>
      decoder(&zone, module, func_body_, &liftoff_.asm_, call_desc, env_,
              runtime_exception_support_,
              &liftoff_.source_position_table_builder_,
              protected_instructions_.get(), &zone, &liftoff_.codegen_zone_);
  decoder.Decode();
  liftoff_compile_time_scope.reset();
  if (!decoder.interface().ok()) {
    // Liftoff compilation failed.
    isolate_->counters()->liftoff_unsupported_functions()->Increment();
    return false;
  }
  if (decoder.failed()) return false;  // Validation error

  if (FLAG_trace_wasm_decode_time) {
    double compile_ms = compile_timer.Elapsed().InMillisecondsF();
    PrintF(
        "wasm-compilation liftoff phase 1 ok: %u bytes, %0.3f ms decode and "
        "compile\n",
        static_cast<unsigned>(func_body_.end - func_body_.start), compile_ms);
  }

  // Record the memory cost this unit places on the system until
  // it is finalized.
  memory_cost_ = liftoff_.asm_.pc_offset();
  liftoff_.safepoint_table_offset_ =
      decoder.interface().GetSafepointTableOffset();
  isolate_->counters()->liftoff_compiled_functions()->Increment();
  return true;
}

#undef __
#undef TRACE

}  // namespace internal
}  // namespace v8
