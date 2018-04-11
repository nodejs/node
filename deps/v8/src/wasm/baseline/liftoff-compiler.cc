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
constexpr auto KIntConst = LiftoffAssembler::VarState::KIntConst;
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

  operator bool() const { return label_ != nullptr; }

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

  operator bool() const { return true; }

  static MovableLabel None() { return MovableLabel(); }

 private:
  Label label_;
};
#endif

wasm::WasmValue WasmPtrValue(uintptr_t ptr) {
  using int_t = std::conditional<kPointerSize == 8, uint64_t, uint32_t>::type;
  static_assert(sizeof(int_t) == sizeof(uintptr_t), "weird uintptr_t");
  return wasm::WasmValue(static_cast<int_t>(ptr));
}

wasm::WasmValue WasmPtrValue(void* ptr) {
  return WasmPtrValue(reinterpret_cast<uintptr_t>(ptr));
}

compiler::CallDescriptor* GetLoweredCallDescriptor(
    Zone* zone, compiler::CallDescriptor* call_desc) {
  return kPointerSize == 4 ? compiler::GetI32WasmCallDescriptor(zone, call_desc)
                           : call_desc;
}

constexpr ValueType kTypesArr_ilfd[] = {kWasmI32, kWasmI64, kWasmF32, kWasmF64};
constexpr Vector<const ValueType> kTypes_ilfd = ArrayVector(kTypesArr_ilfd);

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
                  compiler::CallDescriptor* call_descriptor,
                  compiler::ModuleEnv* env,
                  compiler::RuntimeExceptionSupport runtime_exception_support,
                  SourcePositionTableBuilder* source_position_table_builder,
                  std::vector<trap_handler::ProtectedInstructionData>*
                      protected_instructions,
                  Zone* compilation_zone, std::unique_ptr<Zone>* codegen_zone)
      : asm_(liftoff_asm),
        descriptor_(
            GetLoweredCallDescriptor(compilation_zone, call_descriptor)),
        env_(env),
        min_size_(uint64_t{env_->module->initial_pages} * wasm::kWasmPageSize),
        max_size_(uint64_t{env_->module->has_maximum_pages
                               ? env_->module->maximum_pages
                               : wasm::kV8MaxWasmMemoryPages} *
                  wasm::kWasmPageSize),
        runtime_exception_support_(runtime_exception_support),
        source_position_table_builder_(source_position_table_builder),
        protected_instructions_(protected_instructions),
        compilation_zone_(compilation_zone),
        codegen_zone_(codegen_zone),
        safepoint_table_builder_(compilation_zone_) {}

  ~LiftoffCompiler() { BindUnboundLabels(nullptr); }

  bool ok() const { return ok_; }

  void unsupported(Decoder* decoder, const char* reason) {
    ok_ = false;
    TRACE("unsupported: %s\n", reason);
    decoder->errorf(decoder->pc(), "unsupported liftoff operation: %s", reason);
    BindUnboundLabels(decoder);
  }

  bool DidAssemblerBailout(Decoder* decoder) {
    if (decoder->failed() || !asm_->did_bailout()) return false;
    unsupported(decoder, asm_->bailout_reason());
    return true;
  }

  bool CheckSupportedType(Decoder* decoder,
                          Vector<const ValueType> supported_types,
                          ValueType type, const char* context) {
    char buffer[128];
    // Check supported types.
    for (ValueType supported : supported_types) {
      if (type == supported) return true;
    }
    SNPrintF(ArrayVector(buffer), "%s %s", WasmOpcodes::TypeName(type),
             context);
    unsupported(decoder, buffer);
    return false;
  }

  int GetSafepointTableOffset() const {
    return safepoint_table_builder_.GetCodeOffset();
  }

  void BindUnboundLabels(Decoder* decoder) {
#ifdef DEBUG
    // Bind all labels now, otherwise their destructor will fire a DCHECK error
    // if they where referenced before.
    uint32_t control_depth = decoder ? decoder->control_depth() : 0;
    for (uint32_t i = 0; i < control_depth; ++i) {
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

  void StartFunction(Decoder* decoder) {
    int num_locals = decoder->NumLocals();
    __ set_num_locals(num_locals);
    for (int i = 0; i < num_locals; ++i) {
      __ set_local_type(i, decoder->GetLocalType(i));
    }
  }

  // Returns the number of inputs processed (1 or 2).
  uint32_t ProcessParameter(ValueType type, uint32_t input_idx) {
    const int num_lowered_params = 1 + (kNeedI64RegPair && type == kWasmI64);
    // Initialize to anything, will be set in the loop and used afterwards.
    LiftoffRegister reg = LiftoffRegister::from_code(kGpReg, 0);
    RegClass rc = num_lowered_params == 1 ? reg_class_for(type) : kGpReg;
    LiftoffRegList pinned;
    for (int pair_idx = 0; pair_idx < num_lowered_params; ++pair_idx) {
      compiler::LinkageLocation param_loc =
          descriptor_->GetInputLocation(input_idx + pair_idx);
      // Initialize to anything, will be set in both arms of the if.
      LiftoffRegister in_reg = LiftoffRegister::from_code(kGpReg, 0);
      if (param_loc.IsRegister()) {
        DCHECK(!param_loc.IsAnyRegister());
        int reg_code = param_loc.AsRegister();
        RegList cache_regs = rc == kGpReg ? kLiftoffAssemblerGpCacheRegs
                                          : kLiftoffAssemblerFpCacheRegs;
        if (cache_regs & (1 << reg_code)) {
          // This is a cache register, just use it.
          in_reg = LiftoffRegister::from_code(rc, reg_code);
        } else {
          // Move to a cache register (spill one if necessary).
          // Note that we cannot create a {LiftoffRegister} for reg_code, since
          // {LiftoffRegister} can only store cache regs.
          LiftoffRegister in_reg = __ GetUnusedRegister(rc, pinned);
          if (rc == kGpReg) {
            __ Move(in_reg.gp(), Register::from_code(reg_code), type);
          } else {
            __ Move(in_reg.fp(), DoubleRegister::from_code(reg_code), type);
          }
        }
      } else if (param_loc.IsCallerFrameSlot()) {
        in_reg = __ GetUnusedRegister(rc, pinned);
        ValueType lowered_type = num_lowered_params == 1 ? type : kWasmI32;
        __ LoadCallerFrameSlot(in_reg, -param_loc.AsCallerFrameSlot(),
                               lowered_type);
      }
      reg = pair_idx == 0 ? in_reg : LiftoffRegister::ForPair(reg, in_reg);
      pinned.set(reg);
    }
    __ PushRegister(type, reg);
    return num_lowered_params;
  }

  void StackCheck(wasm::WasmCodePosition position) {
    if (FLAG_wasm_no_stack_checks || !runtime_exception_support_) return;
    out_of_line_code_.push_back(
        OutOfLineCode::StackCheck(position, __ cache_state()->used_registers));
    OutOfLineCode& ool = out_of_line_code_.back();
    __ StackCheck(ool.label.get());
    if (ool.continuation) __ bind(ool.continuation.get());
  }

  void StartFunctionBody(Decoder* decoder, Control* block) {
    __ EnterFrame(StackFrame::WASM_COMPILED);
    __ set_has_frame(true);
    pc_offset_stack_frame_construction_ = __ PrepareStackFrame();
    // {PrepareStackFrame} is the first platform-specific assembler method.
    // If this failed, we can bail out immediately, avoiding runtime overhead
    // and potential failures because of other unimplemented methods.
    // A platform implementing {PrepareStackFrame} must ensure that we can
    // finish compilation without errors even if we hit unimplemented
    // LiftoffAssembler methods.
    if (DidAssemblerBailout(decoder)) return;
    // Parameter 0 is the wasm context.
    uint32_t num_params =
        static_cast<uint32_t>(decoder->sig_->parameter_count());
    for (uint32_t i = 0; i < __ num_locals(); ++i) {
      if (!CheckSupportedType(decoder, kTypes_ilfd, __ local_type(i), "param"))
        return;
    }
    // Input 0 is the call target, the context is at 1.
    constexpr int kContextParameterIndex = 1;
    // Store the context parameter to a special stack slot.
    compiler::LinkageLocation context_loc =
        descriptor_->GetInputLocation(kContextParameterIndex);
    DCHECK(context_loc.IsRegister());
    DCHECK(!context_loc.IsAnyRegister());
    Register context_reg = Register::from_code(context_loc.AsRegister());
    __ SpillContext(context_reg);
    // Input 0 is the code target, 1 is the context. First parameter at 2.
    uint32_t input_idx = kContextParameterIndex + 1;
    for (uint32_t param_idx = 0; param_idx < num_params; ++param_idx) {
      input_idx += ProcessParameter(__ local_type(param_idx), input_idx);
    }
    DCHECK_EQ(input_idx, descriptor_->InputCount());
    // Set to a gp register, to mark this uninitialized.
    LiftoffRegister zero_double_reg(Register::from_code<0>());
    DCHECK(zero_double_reg.is_gp());
    for (uint32_t param_idx = num_params; param_idx < __ num_locals();
         ++param_idx) {
      ValueType type = decoder->GetLocalType(param_idx);
      switch (type) {
        case kWasmI32:
          __ cache_state()->stack_state.emplace_back(kWasmI32, uint32_t{0});
          break;
        case kWasmI64:
          __ cache_state()->stack_state.emplace_back(kWasmI64, uint32_t{0});
          break;
        case kWasmF32:
        case kWasmF64:
          if (zero_double_reg.is_gp()) {
            // Note: This might spill one of the registers used to hold
            // parameters.
            zero_double_reg = __ GetUnusedRegister(kFpReg);
            // Zero is represented by the bit pattern 0 for both f32 and f64.
            __ LoadConstant(zero_double_reg, WasmValue(0.));
          }
          __ PushRegister(type, zero_double_reg);
          break;
        default:
          UNIMPLEMENTED();
      }
    }
    block->label_state.stack_base = __ num_locals();

    // The function-prologue stack check is associated with position 0, which
    // is never a position of any instruction in the function.
    StackCheck(0);

    DCHECK_EQ(__ num_locals(), __ cache_state()->stack_height());
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
    if (DidAssemblerBailout(decoder)) return;
    for (OutOfLineCode& ool : out_of_line_code_) {
      GenerateOutOfLineCode(ool);
    }
    safepoint_table_builder_.Emit(asm_, __ GetTotalFrameSlotCount());
    __ PatchPrepareStackFrame(pc_offset_stack_frame_construction_,
                              __ GetTotalFrameSlotCount());
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
    __ emit_cond_jump(kEqual, if_block->else_state->label.get(), kWasmI32,
                      value);

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
    auto call_descriptor =
        compiler::Linkage::GetSimplifiedCDescriptor(compilation_zone_, &sig);

    // Before making a call, spill all cache registers.
    __ SpillAllRegisters();

    // Store arguments on our stack, then align the stack for calling to C.
    uint32_t num_params =
        static_cast<uint32_t>(call_descriptor->ParameterCount());
    __ PrepareCCall(num_params, arg_regs);

    // Set parameters (in sp[0], sp[8], ...).
    uint32_t num_stack_params = 0;
    for (uint32_t param = 0; param < num_params; ++param) {
      constexpr size_t kInputShift = 1;  // Input 0 is the call target.

      compiler::LinkageLocation loc =
          call_descriptor->GetInputLocation(param + kInputShift);
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
    compiler::LinkageLocation return_loc =
        call_descriptor->GetReturnLocation(0);
    DCHECK(return_loc.IsRegister());
    Register return_reg = Register::from_code(return_loc.AsRegister());
    if (return_reg != res_reg) {
      DCHECK_EQ(MachineRepresentation::kWord32,
                sig.GetReturn(0).representation());
      __ Move(LiftoffRegister(res_reg), LiftoffRegister(return_reg), kWasmI32);
    }
  }

  template <ValueType type, class EmitFn>
  void EmitUnOp(EmitFn fn) {
    static RegClass rc = reg_class_for(type);
    LiftoffRegList pinned;
    LiftoffRegister dst = pinned.set(__ GetUnaryOpTargetRegister(rc));
    LiftoffRegister src = __ PopToRegister(rc, pinned);
    fn(dst, src);
    __ PushRegister(type, dst);
  }

  void EmitI32UnOpWithCFallback(bool (LiftoffAssembler::*emit_fn)(Register,
                                                                  Register),
                                ExternalReference (*fallback_fn)(Isolate*)) {
    auto emit_with_c_fallback = [=](LiftoffRegister dst, LiftoffRegister src) {
      if (emit_fn && (asm_->*emit_fn)(dst.gp(), src.gp())) return;
      ExternalReference ext_ref = fallback_fn(asm_->isolate());
      Register args[] = {src.gp()};
      GenerateCCall(dst.gp(), arraysize(args), args, ext_ref);
    };
    EmitUnOp<kWasmI32>(emit_with_c_fallback);
  }

  void UnOp(Decoder* decoder, WasmOpcode opcode, FunctionSig*,
            const Value& value, Value* result) {
#define CASE_I32_UNOP(opcode, fn)                                      \
  case WasmOpcode::kExpr##opcode:                                      \
    EmitUnOp<kWasmI32>([=](LiftoffRegister dst, LiftoffRegister src) { \
      __ emit_##fn(dst.gp(), src.gp());                                \
    });                                                                \
    break;
#define CASE_FLOAT_UNOP(opcode, type, fn)                                 \
  case WasmOpcode::kExpr##opcode:                                         \
    EmitUnOp<kWasm##type>([=](LiftoffRegister dst, LiftoffRegister src) { \
      __ emit_##fn(dst.fp(), src.fp());                                   \
    });                                                                   \
    break;
    switch (opcode) {
      CASE_I32_UNOP(I32Clz, i32_clz)
      CASE_I32_UNOP(I32Ctz, i32_ctz)
      case kExprI32Popcnt:
        EmitI32UnOpWithCFallback(&LiftoffAssembler::emit_i32_popcnt,
                                 &ExternalReference::wasm_word32_popcnt);
        break;
      case kExprI32Eqz:
        EmitUnOp<kWasmI32>([=](LiftoffRegister dst, LiftoffRegister src) {
          __ emit_i32_set_cond(kEqual, dst.gp(), src.gp());
        });
        break;
        CASE_FLOAT_UNOP(F32Neg, F32, f32_neg)
        CASE_FLOAT_UNOP(F64Neg, F64, f64_neg)
      default:
        return unsupported(decoder, WasmOpcodes::OpcodeName(opcode));
    }
#undef CASE_I32_UNOP
#undef CASE_FLOAT_UNOP
  }

  template <ValueType type, typename EmitFn>
  void EmitMonomorphicBinOp(EmitFn fn) {
    static constexpr RegClass rc = reg_class_for(type);
    LiftoffRegList pinned;
    LiftoffRegister dst = pinned.set(__ GetBinaryOpTargetRegister(rc));
    LiftoffRegister rhs = pinned.set(__ PopToRegister(rc, pinned));
    LiftoffRegister lhs = __ PopToRegister(rc, pinned);
    fn(dst, lhs, rhs);
    __ PushRegister(type, dst);
  }

  template <ValueType result_type, RegClass src_rc, typename EmitFn>
  void EmitBinOpWithDifferentResultType(EmitFn fn) {
    LiftoffRegList pinned;
    LiftoffRegister rhs = pinned.set(__ PopToRegister(src_rc, pinned));
    LiftoffRegister lhs = pinned.set(__ PopToRegister(src_rc, pinned));
    LiftoffRegister dst = __ GetUnusedRegister(reg_class_for(result_type));
    fn(dst, lhs, rhs);
    __ PushRegister(result_type, dst);
  }

  void BinOp(Decoder* decoder, WasmOpcode opcode, FunctionSig*,
             const Value& lhs, const Value& rhs, Value* result) {
#define CASE_I32_BINOP(opcode, fn)                                           \
  case WasmOpcode::kExpr##opcode:                                            \
    return EmitMonomorphicBinOp<kWasmI32>(                                   \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          __ emit_##fn(dst.gp(), lhs.gp(), rhs.gp());                        \
        });
#define CASE_FLOAT_BINOP(opcode, type, fn)                                   \
  case WasmOpcode::kExpr##opcode:                                            \
    return EmitMonomorphicBinOp<kWasm##type>(                                \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          __ emit_##fn(dst.fp(), lhs.fp(), rhs.fp());                        \
        });
#define CASE_I32_CMPOP(opcode, cond)                                         \
  case WasmOpcode::kExpr##opcode:                                            \
    return EmitMonomorphicBinOp<kWasmI32>(                                   \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          __ emit_i32_set_cond(cond, dst.gp(), lhs.gp(), rhs.gp());          \
        });
#define CASE_F32_CMPOP(opcode, cond)                                         \
  case WasmOpcode::kExpr##opcode:                                            \
    return EmitBinOpWithDifferentResultType<kWasmI32, kFpReg>(               \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          __ emit_f32_set_cond(cond, dst.gp(), lhs.fp(), rhs.fp());          \
        });
#define CASE_SHIFTOP(opcode, fn)                                             \
  case WasmOpcode::kExpr##opcode:                                            \
    return EmitMonomorphicBinOp<kWasmI32>(                                   \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          __ emit_##fn(dst.gp(), lhs.gp(), rhs.gp(), {});                    \
        });
#define CASE_CCALL_BINOP(opcode, type, ext_ref_fn)                           \
  case WasmOpcode::kExpr##opcode:                                            \
    return EmitMonomorphicBinOp<kWasmI32>(                                   \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          Register args[] = {lhs.gp(), rhs.gp()};                            \
          auto ext_ref = ExternalReference::ext_ref_fn(__ isolate());        \
          GenerateCCall(dst.gp(), arraysize(args), args, ext_ref);           \
        });
    switch (opcode) {
      CASE_I32_BINOP(I32Add, i32_add)
      CASE_I32_BINOP(I32Sub, i32_sub)
      CASE_I32_BINOP(I32Mul, i32_mul)
      CASE_I32_BINOP(I32And, i32_and)
      CASE_I32_BINOP(I32Ior, i32_or)
      CASE_I32_BINOP(I32Xor, i32_xor)
      CASE_I32_CMPOP(I32Eq, kEqual)
      CASE_I32_CMPOP(I32Ne, kUnequal)
      CASE_I32_CMPOP(I32LtS, kSignedLessThan)
      CASE_I32_CMPOP(I32LtU, kUnsignedLessThan)
      CASE_I32_CMPOP(I32GtS, kSignedGreaterThan)
      CASE_I32_CMPOP(I32GtU, kUnsignedGreaterThan)
      CASE_I32_CMPOP(I32LeS, kSignedLessEqual)
      CASE_I32_CMPOP(I32LeU, kUnsignedLessEqual)
      CASE_I32_CMPOP(I32GeS, kSignedGreaterEqual)
      CASE_I32_CMPOP(I32GeU, kUnsignedGreaterEqual)
      CASE_F32_CMPOP(F32Eq, kEqual)
      CASE_F32_CMPOP(F32Ne, kUnequal)
      CASE_F32_CMPOP(F32Lt, kUnsignedLessThan)
      CASE_F32_CMPOP(F32Gt, kUnsignedGreaterThan)
      CASE_F32_CMPOP(F32Le, kUnsignedLessEqual)
      CASE_F32_CMPOP(F32Ge, kUnsignedGreaterEqual)
      CASE_SHIFTOP(I32Shl, i32_shl)
      CASE_SHIFTOP(I32ShrS, i32_sar)
      CASE_SHIFTOP(I32ShrU, i32_shr)
      CASE_CCALL_BINOP(I32Rol, I32, wasm_word32_rol)
      CASE_CCALL_BINOP(I32Ror, I32, wasm_word32_ror)
      CASE_FLOAT_BINOP(F32Add, F32, f32_add)
      CASE_FLOAT_BINOP(F32Sub, F32, f32_sub)
      CASE_FLOAT_BINOP(F32Mul, F32, f32_mul)
      CASE_FLOAT_BINOP(F64Add, F64, f64_add)
      CASE_FLOAT_BINOP(F64Sub, F64, f64_sub)
      CASE_FLOAT_BINOP(F64Mul, F64, f64_mul)
      default:
        return unsupported(decoder, WasmOpcodes::OpcodeName(opcode));
    }
#undef CASE_I32_BINOP
#undef CASE_FLOAT_BINOP
#undef CASE_I32_CMPOP
#undef CASE_F32_CMPOP
#undef CASE_SHIFTOP
#undef CASE_CCALL_BINOP
  }

  void I32Const(Decoder* decoder, Value* result, int32_t value) {
    __ cache_state()->stack_state.emplace_back(kWasmI32, value);
  }

  void I64Const(Decoder* decoder, Value* result, int64_t value) {
    // The {VarState} stores constant values as int32_t, thus we only store
    // 64-bit constants in this field if it fits in an int32_t. Larger values
    // cannot be used as immediate value anyway, so we can also just put them in
    // a register immediately.
    int32_t value_i32 = static_cast<int32_t>(value);
    if (value_i32 == value) {
      __ cache_state()->stack_state.emplace_back(kWasmI64, value_i32);
    } else {
      LiftoffRegister reg = __ GetUnusedRegister(reg_class_for(kWasmI64));
      __ LoadConstant(reg, WasmValue(value));
      __ PushRegister(kWasmI64, reg);
    }
  }

  void F32Const(Decoder* decoder, Value* result, float value) {
    LiftoffRegister reg = __ GetUnusedRegister(kFpReg);
    __ LoadConstant(reg, WasmValue(value));
    __ PushRegister(kWasmF32, reg);
  }

  void F64Const(Decoder* decoder, Value* result, double value) {
    LiftoffRegister reg = __ GetUnusedRegister(kFpReg);
    __ LoadConstant(reg, WasmValue(value));
    __ PushRegister(kWasmF64, reg);
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
      __ MoveToReturnRegister(reg, values[0].type);
    }
    __ LeaveFrame(StackFrame::WASM_COMPILED);
    __ DropStackSlotsAndRet(
        static_cast<uint32_t>(descriptor_->StackParameterCount()));
  }

  void GetLocal(Decoder* decoder, Value* result,
                const LocalIndexOperand<validate>& operand) {
    auto& slot = __ cache_state()->stack_state[operand.index];
    DCHECK_EQ(slot.type(), operand.type);
    switch (slot.loc()) {
      case kRegister:
        __ PushRegister(slot.type(), slot.reg());
        break;
      case KIntConst:
        __ cache_state()->stack_state.emplace_back(operand.type,
                                                   slot.i32_const());
        break;
      case kStack: {
        auto rc = reg_class_for(operand.type);
        LiftoffRegister reg = __ GetUnusedRegister(rc);
        __ Fill(reg, operand.index, operand.type);
        __ PushRegister(slot.type(), reg);
        break;
      }
    }
  }

  void SetLocalFromStackSlot(LiftoffAssembler::VarState& dst_slot,
                             uint32_t local_index) {
    auto& state = *__ cache_state();
    ValueType type = dst_slot.type();
    if (dst_slot.is_reg()) {
      LiftoffRegister slot_reg = dst_slot.reg();
      if (state.get_use_count(slot_reg) == 1) {
        __ Fill(dst_slot.reg(), state.stack_height() - 1, type);
        return;
      }
      state.dec_used(slot_reg);
    }
    DCHECK_EQ(type, __ local_type(local_index));
    RegClass rc = reg_class_for(type);
    LiftoffRegister dst_reg = __ GetUnusedRegister(rc);
    __ Fill(dst_reg, __ cache_state()->stack_height() - 1, type);
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
      case KIntConst:
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
    __ emit_cond_jump(kEqual, &cont_false, kWasmI32, value);

    Br(target);
    __ bind(&cont_false);
  }

  // Generate a branch table case, potentially reusing previously generated
  // stack transfer code.
  void GenerateBrCase(Decoder* decoder, uint32_t br_depth,
                      std::map<uint32_t, MovableLabel>& br_targets) {
    MovableLabel& label = br_targets[br_depth];
    if (label.get()->is_bound()) {
      __ jmp(label.get());
    } else {
      __ bind(label.get());
      Br(decoder->control_at(br_depth));
    }
  }

  // Generate a branch table for input in [min, max).
  // TODO(wasm): Generate a real branch table (like TF TableSwitch).
  void GenerateBrTable(Decoder* decoder, LiftoffRegister tmp,
                       LiftoffRegister value, uint32_t min, uint32_t max,
                       BranchTableIterator<validate>& table_iterator,
                       std::map<uint32_t, MovableLabel>& br_targets) {
    DCHECK_LT(min, max);
    // Check base case.
    if (max == min + 1) {
      DCHECK_EQ(min, table_iterator.cur_index());
      GenerateBrCase(decoder, table_iterator.next(), br_targets);
      return;
    }

    uint32_t split = min + (max - min) / 2;
    Label upper_half;
    __ LoadConstant(tmp, WasmValue(split));
    __ emit_cond_jump(kUnsignedGreaterEqual, &upper_half, kWasmI32, value.gp(),
                      tmp.gp());
    // Emit br table for lower half:
    GenerateBrTable(decoder, tmp, value, min, split, table_iterator,
                    br_targets);
    __ bind(&upper_half);
    // Emit br table for upper half:
    GenerateBrTable(decoder, tmp, value, split, max, table_iterator,
                    br_targets);
  }

  void BrTable(Decoder* decoder, const BranchTableOperand<validate>& operand,
               const Value& key) {
    LiftoffRegList pinned;
    LiftoffRegister value = pinned.set(__ PopToRegister(kGpReg));
    BranchTableIterator<validate> table_iterator(decoder, operand);
    std::map<uint32_t, MovableLabel> br_targets;

    if (operand.table_count > 0) {
      LiftoffRegister tmp = __ GetUnusedRegister(kGpReg, pinned);
      __ LoadConstant(tmp, WasmValue(uint32_t{operand.table_count}));
      Label case_default;
      __ emit_cond_jump(kUnsignedGreaterEqual, &case_default, kWasmI32,
                        value.gp(), tmp.gp());

      GenerateBrTable(decoder, tmp, value, 0, operand.table_count,
                      table_iterator, br_targets);

      __ bind(&case_default);
    }

    // Generate the default case.
    GenerateBrCase(decoder, table_iterator.next(), br_targets);
    DCHECK(!table_iterator.has_next());
  }

  void Else(Decoder* decoder, Control* if_block) {
    if (if_block->reachable()) __ emit_jump(if_block->label.get());
    __ bind(if_block->else_state->label.get());
    __ cache_state()->Steal(if_block->else_state->state);
  }

  Label* AddOutOfLineTrap(wasm::WasmCodePosition position,
                          Builtins::Name builtin, uint32_t pc = 0) {
    DCHECK(!FLAG_wasm_no_bounds_checks);
    // The pc is needed for memory OOB trap with trap handler enabled. Other
    // callers should not even compute it.
    DCHECK_EQ(pc != 0, builtin == Builtins::kThrowWasmTrapMemOutOfBounds &&
                           env_->use_trap_handler);

    out_of_line_code_.push_back(OutOfLineCode::Trap(builtin, position, pc));
    return out_of_line_code_.back().label.get();
  }

  // Returns true if the memory access is statically known to be out of bounds
  // (a jump to the trap was generated then); return false otherwise.
  bool BoundsCheckMem(Decoder* decoder, uint32_t access_size, uint32_t offset,
                      Register index, LiftoffRegList pinned) {
    const bool statically_oob =
        access_size > max_size_ || offset > max_size_ - access_size;

    if (!statically_oob &&
        (FLAG_wasm_no_bounds_checks || env_->use_trap_handler)) {
      return false;
    }

    Label* trap_label = AddOutOfLineTrap(
        decoder->position(), Builtins::kThrowWasmTrapMemOutOfBounds);

    if (statically_oob) {
      __ emit_jump(trap_label);
      Control* current_block = decoder->control_at(0);
      if (current_block->reachable()) {
        current_block->reachability = kSpecOnlyReachable;
      }
      return true;
    }

    DCHECK(!env_->use_trap_handler);
    DCHECK(!FLAG_wasm_no_bounds_checks);

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
      __ emit_cond_jump(kUnsignedGreaterEqual, trap_label, kWasmI32,
                        end_offset_reg.gp(), mem_size.gp());
    }

    // Just reuse the end_offset register for computing the effective size.
    LiftoffRegister effective_size_reg = end_offset_reg;
    __ emit_i32_sub(effective_size_reg.gp(), mem_size.gp(),
                    end_offset_reg.gp());

    __ emit_cond_jump(kUnsignedGreaterEqual, trap_label, kWasmI32, index,
                      effective_size_reg.gp());
    return false;
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
    auto call_descriptor = compiler::Linkage::GetRuntimeCallDescriptor(
        compilation_zone_, Runtime::kWasmTraceMemory, num_args,
        compiler::Operator::kNoProperties, compiler::CallDescriptor::kNoFlags);
    // Currently, only one argument is supported. More arguments require some
    // caution for the parallel register moves (reuse StackTransferRecipe).
    DCHECK_EQ(1, num_args);
    constexpr size_t kInputShift = 1;  // Input 0 is the call target.
    compiler::LinkageLocation param_loc =
        call_descriptor->GetInputLocation(kInputShift);
    if (param_loc.IsRegister()) {
      Register reg = Register::from_code(param_loc.AsRegister());
      __ Move(LiftoffRegister(reg), LiftoffRegister(args[0]),
              LiftoffAssembler::kWasmIntPtr);
    } else {
      DCHECK(param_loc.IsCallerFrameSlot());
      __ PushCallerFrameSlot(LiftoffRegister(args[0]),
                             LiftoffAssembler::kWasmIntPtr);
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
    if (!CheckSupportedType(decoder, kTypes_ilfd, value_type, "load")) return;
    LiftoffRegList pinned;
    Register index = pinned.set(__ PopToRegister(kGpReg)).gp();
    if (BoundsCheckMem(decoder, type.size(), operand.offset, index, pinned)) {
      return;
    }
    Register addr = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    __ LoadFromContext(addr, offsetof(WasmContext, mem_start), kPointerSize);
    RegClass rc = reg_class_for(value_type);
    LiftoffRegister value = pinned.set(__ GetUnusedRegister(rc, pinned));
    uint32_t protected_load_pc = 0;
    __ Load(value, addr, index, operand.offset, type, pinned,
            &protected_load_pc);
    if (env_->use_trap_handler) {
      AddOutOfLineTrap(decoder->position(),
                       Builtins::kThrowWasmTrapMemOutOfBounds,
                       protected_load_pc);
    }
    __ PushRegister(value_type, value);

    if (FLAG_wasm_trace_memory) {
      TraceMemoryOperation(false, type.mem_type().representation(), index,
                           operand.offset, decoder->position());
    }
  }

  void StoreMem(Decoder* decoder, StoreType type,
                const MemoryAccessOperand<validate>& operand,
                const Value& index_val, const Value& value_val) {
    ValueType value_type = type.value_type();
    if (!CheckSupportedType(decoder, kTypes_ilfd, value_type, "store")) return;
    RegClass rc = reg_class_for(value_type);
    LiftoffRegList pinned;
    LiftoffRegister value = pinned.set(__ PopToRegister(rc));
    Register index = pinned.set(__ PopToRegister(kGpReg, pinned)).gp();
    if (BoundsCheckMem(decoder, type.size(), operand.offset, index, pinned)) {
      return;
    }
    Register addr = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    __ LoadFromContext(addr, offsetof(WasmContext, mem_start), kPointerSize);
    uint32_t protected_store_pc = 0;
    __ Store(addr, index, operand.offset, value, type, pinned,
             &protected_store_pc);
    if (env_->use_trap_handler) {
      AddOutOfLineTrap(decoder->position(),
                       Builtins::kThrowWasmTrapMemOutOfBounds,
                       protected_store_pc);
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
    if (operand.sig->return_count() == 1 &&
        !CheckSupportedType(decoder, kTypes_ilfd, operand.sig->GetReturn(0),
                            "return"))
      return;

    auto call_descriptor =
        compiler::GetWasmCallDescriptor(compilation_zone_, operand.sig);
    call_descriptor =
        GetLoweredCallDescriptor(compilation_zone_, call_descriptor);

    __ PrepareCall(operand.sig, call_descriptor);

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

    __ FinishCall(operand.sig, call_descriptor);
  }

  void CallIndirect(Decoder* decoder, const Value& index_val,
                    const CallIndirectOperand<validate>& operand,
                    const Value args[], Value returns[]) {
    if (operand.sig->return_count() > 1) {
      return unsupported(decoder, "multi-return");
    }
    if (operand.sig->return_count() == 1 &&
        !CheckSupportedType(decoder, kTypes_ilfd, operand.sig->GetReturn(0),
                            "return")) {
      return;
    }

    // Assume only one table for now.
    uint32_t table_index = 0;

    // Pop the index.
    LiftoffRegister index = __ PopToRegister(kGpReg);
    // If that register is still being used after popping, we move it to another
    // register, because we want to modify that register.
    if (__ cache_state()->is_used(index)) {
      LiftoffRegister new_index =
          __ GetUnusedRegister(kGpReg, LiftoffRegList::ForRegs(index));
      __ Move(new_index, index, kWasmI32);
      index = new_index;
    }

    LiftoffRegList pinned = LiftoffRegList::ForRegs(index);
    // Get three temporary registers.
    LiftoffRegister table = pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LiftoffRegister tmp_const =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LiftoffRegister scratch = pinned.set(__ GetUnusedRegister(kGpReg, pinned));

    LiftoffRegister* explicit_context = nullptr;

    // Bounds check against the table size.
    Label* invalid_func_label = AddOutOfLineTrap(
        decoder->position(), Builtins::kThrowWasmTrapFuncInvalid);

    static constexpr LoadType kPointerLoadType =
        kPointerSize == 8 ? LoadType::kI64Load : LoadType::kI32Load;
    static constexpr int kFixedArrayOffset =
        FixedArray::kHeaderSize - kHeapObjectTag;

    uint32_t canonical_sig_num = env_->module->signature_ids[operand.sig_index];
    DCHECK_GE(canonical_sig_num, 0);
    DCHECK_GE(kMaxInt, canonical_sig_num);

    if (WASM_CONTEXT_TABLES) {
      // Compare against table size stored in {wasm_context->table_size}.
      __ LoadFromContext(tmp_const.gp(), offsetof(WasmContext, table_size),
                         sizeof(uint32_t));
      __ emit_cond_jump(kUnsignedGreaterEqual, invalid_func_label, kWasmI32,
                        index.gp(), tmp_const.gp());
      // Load the table from {wasm_context->table}
      __ LoadFromContext(table.gp(), offsetof(WasmContext, table),
                         kPointerSize);
      // Load the signature from {wasm_context->table[$index].sig_id}
      // == wasm_context.table + $index * #sizeof(IndirectionFunctionTableEntry)
      //    + #offsetof(sig_id)
      __ LoadConstant(
          tmp_const,
          WasmValue(static_cast<uint32_t>(sizeof(IndirectFunctionTableEntry))));
      __ emit_i32_mul(index.gp(), index.gp(), tmp_const.gp());
      __ Load(scratch, table.gp(), index.gp(),
              offsetof(IndirectFunctionTableEntry, sig_id), LoadType::kI32Load,
              pinned);

      __ LoadConstant(tmp_const, WasmValue(canonical_sig_num));

      Label* sig_mismatch_label = AddOutOfLineTrap(
          decoder->position(), Builtins::kThrowWasmTrapFuncSigMismatch);
      __ emit_cond_jump(kUnequal, sig_mismatch_label,
                        LiftoffAssembler::kWasmIntPtr, scratch.gp(),
                        tmp_const.gp());

      // Load the target address from {wasm_context->table[$index].target}
      __ Load(scratch, table.gp(), index.gp(),
              offsetof(IndirectFunctionTableEntry, target), kPointerLoadType,
              pinned);

      // Load the context from {wasm_context->table[$index].context}
      // TODO(wasm): directly allocate the correct context register to avoid
      // any potential moves.
      __ Load(tmp_const, table.gp(), index.gp(),
              offsetof(IndirectFunctionTableEntry, context), kPointerLoadType,
              pinned);
      explicit_context = &tmp_const;
    } else {
      // Compare against table size, which is a patchable constant.
      uint32_t table_size =
          env_->module->function_tables[table_index].initial_size;

      __ LoadConstant(tmp_const, WasmValue(table_size),
                      RelocInfo::WASM_FUNCTION_TABLE_SIZE_REFERENCE);

      __ emit_cond_jump(kUnsignedGreaterEqual, invalid_func_label, kWasmI32,
                        index.gp(), tmp_const.gp());

      wasm::GlobalHandleAddress function_table_handle_address =
          env_->function_tables[table_index];
      __ LoadConstant(table, WasmPtrValue(function_table_handle_address),
                      RelocInfo::WASM_GLOBAL_HANDLE);
      __ Load(table, table.gp(), no_reg, 0, kPointerLoadType, pinned);

      // Load signature from the table and check.
      // The table is a FixedArray; signatures are encoded as SMIs.
      // [sig1, code1, sig2, code2, sig3, code3, ...]
      static_assert(compiler::kFunctionTableEntrySize == 2, "consistency");
      static_assert(compiler::kFunctionTableSignatureOffset == 0,
                    "consistency");
      static_assert(compiler::kFunctionTableCodeOffset == 1, "consistency");
      __ LoadConstant(tmp_const, WasmValue(kPointerSizeLog2 + 1));
      // Shift index such that it's the offset of the signature in the
      // FixedArray.
      __ emit_i32_shl(index.gp(), index.gp(), tmp_const.gp(), pinned);

      // Load the signature.
      __ Load(scratch, table.gp(), index.gp(), kFixedArrayOffset,
              kPointerLoadType, pinned);

      __ LoadConstant(tmp_const, WasmPtrValue(Smi::FromInt(canonical_sig_num)));

      Label* sig_mismatch_label = AddOutOfLineTrap(
          decoder->position(), Builtins::kThrowWasmTrapFuncSigMismatch);
      __ emit_cond_jump(kUnequal, sig_mismatch_label,
                        LiftoffAssembler::kWasmIntPtr, scratch.gp(),
                        tmp_const.gp());

      // Load code object.
      __ Load(scratch, table.gp(), index.gp(), kFixedArrayOffset + kPointerSize,
              kPointerLoadType, pinned);

      // Move the pointer from the Code object to the instruction start.
      __ LoadConstant(tmp_const,
                      WasmPtrValue(Code::kHeaderSize - kHeapObjectTag));
      __ emit_ptrsize_add(scratch.gp(), scratch.gp(), tmp_const.gp());
    }

    source_position_table_builder_->AddPosition(
        __ pc_offset(), SourcePosition(decoder->position()), false);

    auto call_descriptor =
        compiler::GetWasmCallDescriptor(compilation_zone_, operand.sig);
    call_descriptor =
        GetLoweredCallDescriptor(compilation_zone_, call_descriptor);

    Register target = scratch.gp();
    __ PrepareCall(operand.sig, call_descriptor, &target, explicit_context);
    __ CallIndirect(operand.sig, call_descriptor, target);

    safepoint_table_builder_.DefineSafepoint(asm_, Safepoint::kSimple, 0,
                                             Safepoint::kNoLazyDeopt);

    __ FinishCall(operand.sig, call_descriptor);
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
  compiler::CallDescriptor* const descriptor_;
  compiler::ModuleEnv* const env_;
  // {min_size_} and {max_size_} are cached values computed from the ModuleEnv.
  const uint64_t min_size_;
  const uint64_t max_size_;
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
  // The pc offset of the instructions to reserve the stack frame. Needed to
  // patch the actually needed stack size in the end.
  uint32_t pc_offset_stack_frame_construction_ = 0;

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
  auto call_descriptor = compiler::GetWasmCallDescriptor(&zone, func_body_.sig);
  base::Optional<TimedHistogramScope> liftoff_compile_time_scope(
      base::in_place, counters()->liftoff_compile_time());
  wasm::WasmFullDecoder<wasm::Decoder::kValidate, wasm::LiftoffCompiler>
      decoder(&zone, module, func_body_, &liftoff_.asm_, call_descriptor, env_,
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
