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

using WasmCompilationData = compiler::WasmCompilationData;

constexpr auto kRegister = LiftoffAssembler::VarState::kRegister;
constexpr auto KIntConst = LiftoffAssembler::VarState::KIntConst;
constexpr auto kStack = LiftoffAssembler::VarState::kStack;

namespace {

#define __ asm_->

#define TRACE(...)                                            \
  do {                                                        \
    if (FLAG_trace_liftoff) PrintF("[liftoff] " __VA_ARGS__); \
  } while (false)

#define WASM_INSTANCE_OBJECT_OFFSET(name) \
  (WasmInstanceObject::k##name##Offset - kHeapObjectTag)

#define LOAD_INSTANCE_FIELD(dst, name, type)                       \
  __ LoadFromInstance(dst.gp(), WASM_INSTANCE_OBJECT_OFFSET(name), \
                      LoadType(type).size());

constexpr LoadType::LoadTypeValue kPointerLoadType =
    kPointerSize == 8 ? LoadType::kI64Load : LoadType::kI32Load;

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
                  SourcePositionTableBuilder* source_position_table_builder,
                  WasmCompilationData* wasm_compilation_data,
                  Zone* compilation_zone, std::unique_ptr<Zone>* codegen_zone,
                  WasmCode* const* code_table_entry)
      : asm_(liftoff_asm),
        descriptor_(
            GetLoweredCallDescriptor(compilation_zone, call_descriptor)),
        env_(env),
        min_size_(uint64_t{env_->module->initial_pages} * wasm::kWasmPageSize),
        max_size_(uint64_t{env_->module->has_maximum_pages
                               ? env_->module->maximum_pages
                               : wasm::kV8MaxWasmMemoryPages} *
                  wasm::kWasmPageSize),
        source_position_table_builder_(source_position_table_builder),
        wasm_compilation_data_(wasm_compilation_data),
        compilation_zone_(compilation_zone),
        codegen_zone_(codegen_zone),
        safepoint_table_builder_(compilation_zone_),
        code_table_entry_(code_table_entry) {}

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
    const int num_lowered_params = 1 + needs_reg_pair(type);
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
      reg = pair_idx == 0 ? in_reg
                          : LiftoffRegister::ForPair(reg.gp(), in_reg.gp());
      pinned.set(reg);
    }
    __ PushRegister(type, reg);
    return num_lowered_params;
  }

  void StackCheck(wasm::WasmCodePosition position) {
    if (FLAG_wasm_no_stack_checks ||
        !wasm_compilation_data_->runtime_exception_support()) {
      return;
    }
    out_of_line_code_.push_back(
        OutOfLineCode::StackCheck(position, __ cache_state()->used_registers));
    OutOfLineCode& ool = out_of_line_code_.back();
    __ StackCheck(ool.label.get());
    if (ool.continuation) __ bind(ool.continuation.get());
  }

  // Inserts a check whether the optimized version of this code already exists.
  // If so, it redirects execution to the optimized code.
  void JumpToOptimizedCodeIfExisting() {
    // Check whether we have an optimized function before
    // continuing to execute the Liftoff-compiled code.
    // TODO(clemensh): Reduce number of temporary registers.
    LiftoffRegList pinned;
    LiftoffRegister wasm_code_addr =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LiftoffRegister target_code_addr =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LiftoffRegister code_start_address =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));

    // Get the current code's target address ({instructions_.start()}).
    __ ComputeCodeStartAddress(code_start_address.gp());

    static LoadType kPointerLoadType =
        LoadType::ForValueType(LiftoffAssembler::kWasmIntPtr);
    using int_t = std::conditional<kPointerSize == 8, uint64_t, uint32_t>::type;
    static_assert(sizeof(int_t) == sizeof(uintptr_t), "weird uintptr_t");
    // Get the address of the WasmCode* currently stored in the code table.
    __ LoadConstant(target_code_addr,
                    WasmValue(reinterpret_cast<int_t>(code_table_entry_)),
                    RelocInfo::WASM_CODE_TABLE_ENTRY);
    // Load the corresponding WasmCode*.
    __ Load(wasm_code_addr, target_code_addr.gp(), Register::no_reg(), 0,
            kPointerLoadType, pinned);
    // Load its target address ({instuctions_.start()}).
    __ Load(target_code_addr, wasm_code_addr.gp(), Register::no_reg(),
            WasmCode::kInstructionStartOffset, kPointerLoadType, pinned);

    // If the current code's target address is the same as the
    // target address of the stored WasmCode, then continue executing, otherwise
    // jump to the updated WasmCode.
    Label cont;
    __ emit_cond_jump(kEqual, &cont, LiftoffAssembler::kWasmIntPtr,
                      target_code_addr.gp(), code_start_address.gp());
    __ LeaveFrame(StackFrame::WASM_COMPILED);
    __ emit_jump(target_code_addr.gp());
    __ bind(&cont);
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
    // Parameter 0 is the instance parameter.
    uint32_t num_params =
        static_cast<uint32_t>(decoder->sig_->parameter_count());
    for (uint32_t i = 0; i < __ num_locals(); ++i) {
      if (!CheckSupportedType(decoder, kTypes_ilfd, __ local_type(i), "param"))
        return;
    }
    // Input 0 is the call target, the instance is at 1.
    constexpr int kInstanceParameterIndex = 1;
    // Store the instance parameter to a special stack slot.
    compiler::LinkageLocation instance_loc =
        descriptor_->GetInputLocation(kInstanceParameterIndex);
    DCHECK(instance_loc.IsRegister());
    DCHECK(!instance_loc.IsAnyRegister());
    Register instance_reg = Register::from_code(instance_loc.AsRegister());
    __ SpillInstance(instance_reg);
    // Input 0 is the code target, 1 is the instance. First parameter at 2.
    uint32_t input_idx = kInstanceParameterIndex + 1;
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

    // TODO(kimanh): if possible, we want to move this check further up,
    // in order to avoid unnecessary overhead each time we enter
    // a Liftoff-compiled function that will jump to a Turbofan-compiled
    // function.
    if (FLAG_wasm_tier_up) {
      JumpToOptimizedCodeIfExisting();
    }
  }

  void GenerateOutOfLineCode(OutOfLineCode& ool) {
    __ bind(ool.label.get());
    const bool is_stack_check = ool.builtin == Builtins::kWasmStackGuard;
    const bool is_mem_out_of_bounds =
        ool.builtin == Builtins::kThrowWasmTrapMemOutOfBounds;

    if (is_mem_out_of_bounds && env_->use_trap_handler) {
      uint32_t pc = static_cast<uint32_t>(__ pc_offset());
      DCHECK_EQ(pc, __ pc_offset());
      wasm_compilation_data_->AddProtectedInstruction(ool.pc, pc);
    }

    if (!wasm_compilation_data_->runtime_exception_support()) {
      // We cannot test calls to the runtime in cctest/test-run-wasm.
      // Therefore we emit a call to C here instead of a call to the runtime.
      // In this mode, we never generate stack checks.
      DCHECK(!is_stack_check);
      __ CallTrapCallbackForTesting();
      __ LeaveFrame(StackFrame::WASM_COMPILED);
      __ Ret();
      return;
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
    Register value = __ PopToRegister().gp();
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

  enum CCallReturn : bool { kHasReturn = true, kNoReturn = false };

  void GenerateCCall(const LiftoffRegister* result_regs, FunctionSig* sig,
                     ValueType out_argument_type,
                     const LiftoffRegister* arg_regs,
                     ExternalReference ext_ref) {
    static constexpr int kMaxReturns = 1;
    static constexpr int kMaxArgs = 2;
    static constexpr MachineType kReps[]{
        MachineType::Uint32(), MachineType::Pointer(), MachineType::Pointer()};
    static_assert(arraysize(kReps) == kMaxReturns + kMaxArgs, "mismatch");

    const bool has_out_argument = out_argument_type != kWasmStmt;
    const uint32_t num_returns = static_cast<uint32_t>(sig->return_count());
    // {total_num_args} is {num_args + 1} if the return value is stored in an
    // out parameter, or {num_args} otherwise.
    const uint32_t num_args = static_cast<uint32_t>(sig->parameter_count());
    const uint32_t total_num_args = num_args + has_out_argument;
    DCHECK_LE(num_args, kMaxArgs);
    DCHECK_LE(num_returns, kMaxReturns);

    MachineSignature machine_sig(num_returns, total_num_args,
                                 kReps + (kMaxReturns - num_returns));
    auto* call_descriptor = compiler::Linkage::GetSimplifiedCDescriptor(
        compilation_zone_, &machine_sig);

    // Before making a call, spill all cache registers.
    __ SpillAllRegisters();

    // Store arguments on our stack, then align the stack for calling to C.
    __ PrepareCCall(sig, arg_regs, out_argument_type);

    // The arguments to the c function are pointers to the stack slots we just
    // pushed.
    int num_stack_params = 0;   // Number of stack parameters.
    int input_idx = 1;          // Input 0 is the call target.
    int param_byte_offset = 0;  // Byte offset into the pushed arguments.
    auto add_argument = [&](ValueType arg_type) {
      compiler::LinkageLocation loc =
          call_descriptor->GetInputLocation(input_idx);
      param_byte_offset +=
          RoundUp<kPointerSize>(WasmOpcodes::MemSize(arg_type));
      ++input_idx;
      if (loc.IsRegister()) {
        Register reg = Register::from_code(loc.AsRegister());
        // Load address of that parameter to the register.
        __ SetCCallRegParamAddr(reg, param_byte_offset, arg_type);
      } else {
        DCHECK(loc.IsCallerFrameSlot());
        __ SetCCallStackParamAddr(num_stack_params, param_byte_offset,
                                  arg_type);
        ++num_stack_params;
      }
    };
    for (ValueType arg_type : sig->parameters()) {
      add_argument(arg_type);
    }
    if (has_out_argument) {
      add_argument(out_argument_type);
    }
    DCHECK_EQ(input_idx, call_descriptor->InputCount());

    // Now execute the call.
    uint32_t c_call_arg_count =
        static_cast<uint32_t>(sig->parameter_count()) + has_out_argument;
    __ CallC(ext_ref, c_call_arg_count);

    // Reset the stack pointer.
    __ FinishCCall();

    // Load return value.
    const LiftoffRegister* next_result_reg = result_regs;
    if (sig->return_count() > 0) {
      DCHECK_EQ(1, sig->return_count());
      compiler::LinkageLocation return_loc =
          call_descriptor->GetReturnLocation(0);
      DCHECK(return_loc.IsRegister());
      Register return_reg = Register::from_code(return_loc.AsRegister());
      if (return_reg != next_result_reg->gp()) {
        __ Move(*next_result_reg, LiftoffRegister(return_reg),
                sig->GetReturn(0));
      }
      ++next_result_reg;
    }

    // Load potential return value from output argument.
    if (has_out_argument) {
      __ LoadCCallOutArgument(*next_result_reg, out_argument_type,
                              param_byte_offset);
    }
  }

  template <ValueType src_type, ValueType result_type, class EmitFn>
  void EmitUnOp(EmitFn fn) {
    static RegClass src_rc = reg_class_for(src_type);
    static RegClass result_rc = reg_class_for(result_type);
    LiftoffRegList pinned;
    LiftoffRegister src = pinned.set(__ PopToRegister(pinned));
    LiftoffRegister dst = src_rc == result_rc
                              ? __ GetUnusedRegister(result_rc, {src}, pinned)
                              : __ GetUnusedRegister(result_rc, pinned);
    fn(dst, src);
    __ PushRegister(result_type, dst);
  }

  void EmitI32UnOpWithCFallback(bool (LiftoffAssembler::*emit_fn)(Register,
                                                                  Register),
                                ExternalReference (*fallback_fn)(Isolate*)) {
    auto emit_with_c_fallback = [=](LiftoffRegister dst, LiftoffRegister src) {
      if (emit_fn && (asm_->*emit_fn)(dst.gp(), src.gp())) return;
      ExternalReference ext_ref = fallback_fn(asm_->isolate());
      ValueType sig_i_i_reps[] = {kWasmI32, kWasmI32};
      FunctionSig sig_i_i(1, 1, sig_i_i_reps);
      GenerateCCall(&dst, &sig_i_i, kWasmStmt, &src, ext_ref);
    };
    EmitUnOp<kWasmI32, kWasmI32>(emit_with_c_fallback);
  }

  void EmitTypeConversion(WasmOpcode opcode, ValueType dst_type,
                          ValueType src_type,
                          ExternalReference (*fallback_fn)(Isolate*)) {
    RegClass src_rc = reg_class_for(src_type);
    RegClass dst_rc = reg_class_for(dst_type);
    LiftoffRegList pinned;
    LiftoffRegister src = pinned.set(__ PopToRegister());
    LiftoffRegister dst = src_rc == dst_rc
                              ? __ GetUnusedRegister(dst_rc, {src}, pinned)
                              : __ GetUnusedRegister(dst_rc, pinned);
    if (!__ emit_type_conversion(opcode, dst, src)) {
      DCHECK_NOT_NULL(fallback_fn);
      ExternalReference ext_ref = fallback_fn(asm_->isolate());
      ValueType sig_reps[] = {src_type};
      FunctionSig sig(0, 1, sig_reps);
      GenerateCCall(&dst, &sig, dst_type, &src, ext_ref);
    }
    __ PushRegister(dst_type, dst);
  }

  void UnOp(Decoder* decoder, WasmOpcode opcode, FunctionSig*,
            const Value& value, Value* result) {
#define CASE_I32_UNOP(opcode, fn)                       \
  case WasmOpcode::kExpr##opcode:                       \
    EmitUnOp<kWasmI32, kWasmI32>(                       \
        [=](LiftoffRegister dst, LiftoffRegister src) { \
          __ emit_##fn(dst.gp(), src.gp());             \
        });                                             \
    break;
#define CASE_FLOAT_UNOP(opcode, type, fn)               \
  case WasmOpcode::kExpr##opcode:                       \
    EmitUnOp<kWasm##type, kWasm##type>(                 \
        [=](LiftoffRegister dst, LiftoffRegister src) { \
          __ emit_##fn(dst.fp(), src.fp());             \
        });                                             \
    break;
#define CASE_TYPE_CONVERSION(opcode, dst_type, src_type, ext_ref)       \
  case WasmOpcode::kExpr##opcode:                                       \
    EmitTypeConversion(kExpr##opcode, kWasm##dst_type, kWasm##src_type, \
                       ext_ref);                                        \
    break;
    switch (opcode) {
      CASE_I32_UNOP(I32Eqz, i32_eqz)
      CASE_I32_UNOP(I32Clz, i32_clz)
      CASE_I32_UNOP(I32Ctz, i32_ctz)
      CASE_FLOAT_UNOP(F32Abs, F32, f32_abs)
      CASE_FLOAT_UNOP(F32Neg, F32, f32_neg)
      CASE_FLOAT_UNOP(F32Ceil, F32, f32_ceil)
      CASE_FLOAT_UNOP(F32Floor, F32, f32_floor)
      CASE_FLOAT_UNOP(F32Trunc, F32, f32_trunc)
      CASE_FLOAT_UNOP(F32NearestInt, F32, f32_nearest_int)
      CASE_FLOAT_UNOP(F32Sqrt, F32, f32_sqrt)
      CASE_FLOAT_UNOP(F64Abs, F64, f64_abs)
      CASE_FLOAT_UNOP(F64Neg, F64, f64_neg)
      CASE_FLOAT_UNOP(F64Ceil, F64, f64_ceil)
      CASE_FLOAT_UNOP(F64Floor, F64, f64_floor)
      CASE_FLOAT_UNOP(F64Trunc, F64, f64_trunc)
      CASE_FLOAT_UNOP(F64NearestInt, F64, f64_nearest_int)
      CASE_FLOAT_UNOP(F64Sqrt, F64, f64_sqrt)
      CASE_TYPE_CONVERSION(I32ConvertI64, I32, I64, nullptr)
      CASE_TYPE_CONVERSION(I32ReinterpretF32, I32, F32, nullptr)
      CASE_TYPE_CONVERSION(I64SConvertI32, I64, I32, nullptr)
      CASE_TYPE_CONVERSION(I64UConvertI32, I64, I32, nullptr)
      CASE_TYPE_CONVERSION(I64ReinterpretF64, I64, F64, nullptr)
      CASE_TYPE_CONVERSION(F32SConvertI32, F32, I32, nullptr)
      CASE_TYPE_CONVERSION(F32UConvertI32, F32, I32, nullptr)
      CASE_TYPE_CONVERSION(F32SConvertI64, F32, I64,
                           &ExternalReference::wasm_int64_to_float32)
      CASE_TYPE_CONVERSION(F32UConvertI64, F32, I64,
                           &ExternalReference::wasm_uint64_to_float32)
      CASE_TYPE_CONVERSION(F32ConvertF64, F32, F64, nullptr)
      CASE_TYPE_CONVERSION(F32ReinterpretI32, F32, I32, nullptr)
      CASE_TYPE_CONVERSION(F64SConvertI32, F64, I32, nullptr)
      CASE_TYPE_CONVERSION(F64UConvertI32, F64, I32, nullptr)
      CASE_TYPE_CONVERSION(F64SConvertI64, F64, I64,
                           &ExternalReference::wasm_int64_to_float64)
      CASE_TYPE_CONVERSION(F64UConvertI64, F64, I64,
                           &ExternalReference::wasm_uint64_to_float64)
      CASE_TYPE_CONVERSION(F64ConvertF32, F64, F32, nullptr)
      CASE_TYPE_CONVERSION(F64ReinterpretI64, F64, I64, nullptr)
      case kExprI32Popcnt:
        EmitI32UnOpWithCFallback(&LiftoffAssembler::emit_i32_popcnt,
                                 &ExternalReference::wasm_word32_popcnt);
        break;
      case WasmOpcode::kExprI64Eqz:
        EmitUnOp<kWasmI64, kWasmI32>(
            [=](LiftoffRegister dst, LiftoffRegister src) {
              __ emit_i64_eqz(dst.gp(), src);
            });
        break;
      default:
        return unsupported(decoder, WasmOpcodes::OpcodeName(opcode));
    }
#undef CASE_I32_UNOP
#undef CASE_FLOAT_UNOP
#undef CASE_TYPE_CONVERSION
  }

  template <ValueType src_type, ValueType result_type, typename EmitFn>
  void EmitBinOp(EmitFn fn) {
    static constexpr RegClass src_rc = reg_class_for(src_type);
    static constexpr RegClass result_rc = reg_class_for(result_type);
    LiftoffRegList pinned;
    LiftoffRegister rhs = pinned.set(__ PopToRegister(pinned));
    LiftoffRegister lhs = pinned.set(__ PopToRegister(pinned));
    LiftoffRegister dst =
        src_rc == result_rc
            ? __ GetUnusedRegister(result_rc, {lhs, rhs}, pinned)
            : __ GetUnusedRegister(result_rc);
    fn(dst, lhs, rhs);
    __ PushRegister(result_type, dst);
  }

  void BinOp(Decoder* decoder, WasmOpcode opcode, FunctionSig*,
             const Value& lhs, const Value& rhs, Value* result) {
#define CASE_I32_BINOP(opcode, fn)                                           \
  case WasmOpcode::kExpr##opcode:                                            \
    return EmitBinOp<kWasmI32, kWasmI32>(                                    \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          __ emit_##fn(dst.gp(), lhs.gp(), rhs.gp());                        \
        });
#define CASE_I64_BINOP(opcode, fn)                                           \
  case WasmOpcode::kExpr##opcode:                                            \
    return EmitBinOp<kWasmI64, kWasmI64>(                                    \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          __ emit_##fn(dst, lhs, rhs);                                       \
        });
#define CASE_FLOAT_BINOP(opcode, type, fn)                                   \
  case WasmOpcode::kExpr##opcode:                                            \
    return EmitBinOp<kWasm##type, kWasm##type>(                              \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          __ emit_##fn(dst.fp(), lhs.fp(), rhs.fp());                        \
        });
#define CASE_I32_CMPOP(opcode, cond)                                         \
  case WasmOpcode::kExpr##opcode:                                            \
    return EmitBinOp<kWasmI32, kWasmI32>(                                    \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          __ emit_i32_set_cond(cond, dst.gp(), lhs.gp(), rhs.gp());          \
        });
#define CASE_I64_CMPOP(opcode, cond)                                         \
  case WasmOpcode::kExpr##opcode:                                            \
    return EmitBinOp<kWasmI64, kWasmI32>(                                    \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          __ emit_i64_set_cond(cond, dst.gp(), lhs, rhs);                    \
        });
#define CASE_F32_CMPOP(opcode, cond)                                         \
  case WasmOpcode::kExpr##opcode:                                            \
    return EmitBinOp<kWasmF32, kWasmI32>(                                    \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          __ emit_f32_set_cond(cond, dst.gp(), lhs.fp(), rhs.fp());          \
        });
#define CASE_F64_CMPOP(opcode, cond)                                         \
  case WasmOpcode::kExpr##opcode:                                            \
    return EmitBinOp<kWasmF64, kWasmI32>(                                    \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          __ emit_f64_set_cond(cond, dst.gp(), lhs.fp(), rhs.fp());          \
        });
#define CASE_I32_SHIFTOP(opcode, fn)                                         \
  case WasmOpcode::kExpr##opcode:                                            \
    return EmitBinOp<kWasmI32, kWasmI32>(                                    \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          __ emit_##fn(dst.gp(), lhs.gp(), rhs.gp(), {});                    \
        });
#define CASE_I64_SHIFTOP(opcode, fn)                                           \
  case WasmOpcode::kExpr##opcode:                                              \
    return EmitBinOp<kWasmI64, kWasmI64>([=](LiftoffRegister dst,              \
                                             LiftoffRegister src,              \
                                             LiftoffRegister amount) {         \
      __ emit_##fn(dst, src, amount.is_pair() ? amount.low_gp() : amount.gp(), \
                   {});                                                        \
    });
#define CASE_CCALL_BINOP(opcode, type, ext_ref_fn)                           \
  case WasmOpcode::kExpr##opcode:                                            \
    return EmitBinOp<kWasmI32, kWasmI32>(                                    \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          LiftoffRegister args[] = {lhs, rhs};                               \
          auto ext_ref = ExternalReference::ext_ref_fn(__ isolate());        \
          ValueType sig_i_ii_reps[] = {kWasmI32, kWasmI32, kWasmI32};        \
          FunctionSig sig_i_ii(1, 2, sig_i_ii_reps);                         \
          GenerateCCall(&dst, &sig_i_ii, kWasmStmt, args, ext_ref);          \
        });
    switch (opcode) {
      CASE_I32_BINOP(I32Add, i32_add)
      CASE_I32_BINOP(I32Sub, i32_sub)
      CASE_I32_BINOP(I32Mul, i32_mul)
      CASE_I32_BINOP(I32And, i32_and)
      CASE_I32_BINOP(I32Ior, i32_or)
      CASE_I32_BINOP(I32Xor, i32_xor)
      CASE_I64_BINOP(I64And, i64_and)
      CASE_I64_BINOP(I64Ior, i64_or)
      CASE_I64_BINOP(I64Xor, i64_xor)
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
      CASE_I64_BINOP(I64Add, i64_add)
      CASE_I64_BINOP(I64Sub, i64_sub)
      CASE_I64_CMPOP(I64Eq, kEqual)
      CASE_I64_CMPOP(I64Ne, kUnequal)
      CASE_I64_CMPOP(I64LtS, kSignedLessThan)
      CASE_I64_CMPOP(I64LtU, kUnsignedLessThan)
      CASE_I64_CMPOP(I64GtS, kSignedGreaterThan)
      CASE_I64_CMPOP(I64GtU, kUnsignedGreaterThan)
      CASE_I64_CMPOP(I64LeS, kSignedLessEqual)
      CASE_I64_CMPOP(I64LeU, kUnsignedLessEqual)
      CASE_I64_CMPOP(I64GeS, kSignedGreaterEqual)
      CASE_I64_CMPOP(I64GeU, kUnsignedGreaterEqual)
      CASE_F32_CMPOP(F32Eq, kEqual)
      CASE_F32_CMPOP(F32Ne, kUnequal)
      CASE_F32_CMPOP(F32Lt, kUnsignedLessThan)
      CASE_F32_CMPOP(F32Gt, kUnsignedGreaterThan)
      CASE_F32_CMPOP(F32Le, kUnsignedLessEqual)
      CASE_F32_CMPOP(F32Ge, kUnsignedGreaterEqual)
      CASE_F64_CMPOP(F64Eq, kEqual)
      CASE_F64_CMPOP(F64Ne, kUnequal)
      CASE_F64_CMPOP(F64Lt, kUnsignedLessThan)
      CASE_F64_CMPOP(F64Gt, kUnsignedGreaterThan)
      CASE_F64_CMPOP(F64Le, kUnsignedLessEqual)
      CASE_F64_CMPOP(F64Ge, kUnsignedGreaterEqual)
      CASE_I32_SHIFTOP(I32Shl, i32_shl)
      CASE_I32_SHIFTOP(I32ShrS, i32_sar)
      CASE_I32_SHIFTOP(I32ShrU, i32_shr)
      CASE_I64_SHIFTOP(I64Shl, i64_shl)
      CASE_I64_SHIFTOP(I64ShrS, i64_sar)
      CASE_I64_SHIFTOP(I64ShrU, i64_shr)
      CASE_CCALL_BINOP(I32Rol, I32, wasm_word32_rol)
      CASE_CCALL_BINOP(I32Ror, I32, wasm_word32_ror)
      CASE_FLOAT_BINOP(F32Add, F32, f32_add)
      CASE_FLOAT_BINOP(F32Sub, F32, f32_sub)
      CASE_FLOAT_BINOP(F32Mul, F32, f32_mul)
      CASE_FLOAT_BINOP(F32Div, F32, f32_div)
      CASE_FLOAT_BINOP(F64Add, F64, f64_add)
      CASE_FLOAT_BINOP(F64Sub, F64, f64_sub)
      CASE_FLOAT_BINOP(F64Mul, F64, f64_mul)
      CASE_FLOAT_BINOP(F64Div, F64, f64_div)
      default:
        return unsupported(decoder, WasmOpcodes::OpcodeName(opcode));
    }
#undef CASE_I32_BINOP
#undef CASE_I64_BINOP
#undef CASE_FLOAT_BINOP
#undef CASE_I32_CMPOP
#undef CASE_I64_CMPOP
#undef CASE_F32_CMPOP
#undef CASE_F64_CMPOP
#undef CASE_I32_SHIFTOP
#undef CASE_I64_SHIFTOP
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

  void RefNull(Decoder* decoder, Value* result) {
    unsupported(decoder, "ref_null");
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
      LiftoffRegister reg = __ PopToRegister();
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
    if (!CheckSupportedType(decoder, kTypes_ilfd, global->type, "global"))
      return;
    LiftoffRegList pinned;
    LiftoffRegister addr = pinned.set(__ GetUnusedRegister(kGpReg));
    LOAD_INSTANCE_FIELD(addr, GlobalsStart, kPointerLoadType);
    LiftoffRegister value =
        pinned.set(__ GetUnusedRegister(reg_class_for(global->type), pinned));
    LoadType type = LoadType::ForValueType(global->type);
    __ Load(value, addr.gp(), no_reg, global->offset, type, pinned);
    __ PushRegister(global->type, value);
  }

  void SetGlobal(Decoder* decoder, const Value& value,
                 const GlobalIndexOperand<validate>& operand) {
    auto* global = &env_->module->globals[operand.index];
    if (!CheckSupportedType(decoder, kTypes_ilfd, global->type, "global"))
      return;
    LiftoffRegList pinned;
    LiftoffRegister addr = pinned.set(__ GetUnusedRegister(kGpReg));
    LOAD_INSTANCE_FIELD(addr, GlobalsStart, kPointerLoadType);
    LiftoffRegister reg = pinned.set(__ PopToRegister(pinned));
    StoreType type = StoreType::ForValueType(global->type);
    __ Store(addr.gp(), no_reg, global->offset, reg, type, pinned);
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
    Register value = __ PopToRegister().gp();
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
    LiftoffRegister value = pinned.set(__ PopToRegister());
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

    // TODO(eholk): This adds protected instruction information for the jump
    // instruction we are about to generate. It would be better to just not add
    // protected instruction info when the pc is 0.
    Label* trap_label = AddOutOfLineTrap(
        decoder->position(), Builtins::kThrowWasmTrapMemOutOfBounds,
        env_->use_trap_handler ? __ pc_offset() : 0);

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
    LOAD_INSTANCE_FIELD(mem_size, MemorySize, LoadType::kI32Load);
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
    Register index = pinned.set(__ PopToRegister()).gp();
    if (BoundsCheckMem(decoder, type.size(), operand.offset, index, pinned)) {
      return;
    }
    LiftoffRegister addr = pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LOAD_INSTANCE_FIELD(addr, MemoryStart, kPointerLoadType);
    RegClass rc = reg_class_for(value_type);
    LiftoffRegister value = pinned.set(__ GetUnusedRegister(rc, pinned));
    uint32_t protected_load_pc = 0;
    __ Load(value, addr.gp(), index, operand.offset, type, pinned,
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
    LiftoffRegList pinned;
    LiftoffRegister value = pinned.set(__ PopToRegister());
    Register index = pinned.set(__ PopToRegister(pinned)).gp();
    if (BoundsCheckMem(decoder, type.size(), operand.offset, index, pinned)) {
      return;
    }
    LiftoffRegister addr = pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LOAD_INSTANCE_FIELD(addr, MemoryStart, kPointerLoadType);
    uint32_t protected_store_pc = 0;
    __ Store(addr.gp(), index, operand.offset, value, type, pinned,
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

    if (operand.index < env_->module->num_imported_functions) {
      // A direct call to an imported function.
      LiftoffRegList pinned;
      LiftoffRegister tmp = pinned.set(__ GetUnusedRegister(kGpReg, pinned));
      LiftoffRegister target = pinned.set(__ GetUnusedRegister(kGpReg, pinned));

      LiftoffRegister imported_targets = tmp;
      LOAD_INSTANCE_FIELD(imported_targets, ImportedFunctionTargets,
                          kPointerLoadType);
      __ Load(target, imported_targets.gp(), no_reg,
              operand.index * sizeof(Address), kPointerLoadType, pinned);

      LiftoffRegister imported_instances = tmp;
      LOAD_INSTANCE_FIELD(imported_instances, ImportedFunctionInstances,
                          kPointerLoadType);
      LiftoffRegister target_instance = tmp;
      __ Load(target_instance, imported_instances.gp(), no_reg,
              compiler::FixedArrayOffsetMinusTag(operand.index),
              kPointerLoadType, pinned);

      LiftoffRegister* explicit_instance = &target_instance;
      Register target_reg = target.gp();
      __ PrepareCall(operand.sig, call_descriptor, &target_reg,
                     explicit_instance);
      source_position_table_builder_->AddPosition(
          __ pc_offset(), SourcePosition(decoder->position()), false);

      __ CallIndirect(operand.sig, call_descriptor, target_reg);

      safepoint_table_builder_.DefineSafepoint(asm_, Safepoint::kSimple, 0,
                                               Safepoint::kNoLazyDeopt);

      __ FinishCall(operand.sig, call_descriptor);
    } else {
      // A direct call within this module just gets the current instance.
      __ PrepareCall(operand.sig, call_descriptor);

      source_position_table_builder_->AddPosition(
          __ pc_offset(), SourcePosition(decoder->position()), false);

      // Just encode the function index. This will be patched at instantiation.
      Address addr = reinterpret_cast<Address>(operand.index);
      __ CallNativeWasmCode(addr);

      safepoint_table_builder_.DefineSafepoint(asm_, Safepoint::kSimple, 0,
                                               Safepoint::kNoLazyDeopt);

      __ FinishCall(operand.sig, call_descriptor);
    }
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

    // Pop the index.
    LiftoffRegister index = __ PopToRegister();
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

    // Bounds check against the table size.
    Label* invalid_func_label = AddOutOfLineTrap(
        decoder->position(), Builtins::kThrowWasmTrapFuncInvalid);

    uint32_t canonical_sig_num = env_->module->signature_ids[operand.sig_index];
    DCHECK_GE(canonical_sig_num, 0);
    DCHECK_GE(kMaxInt, canonical_sig_num);

    // Compare against table size stored in
    // {instance->indirect_function_table_size}.
    LOAD_INSTANCE_FIELD(tmp_const, IndirectFunctionTableSize,
                        LoadType::kI32Load);
    __ emit_cond_jump(kUnsignedGreaterEqual, invalid_func_label, kWasmI32,
                      index.gp(), tmp_const.gp());

    // Load the signature from {instance->ift_sig_ids[key]}
    LOAD_INSTANCE_FIELD(table, IndirectFunctionTableSigIds, kPointerLoadType);
    __ LoadConstant(tmp_const,
                    WasmValue(static_cast<uint32_t>(sizeof(uint32_t))));
    // TODO(wasm): use a emit_i32_shli() instead of a multiply.
    // (currently cannot use shl on ia32/x64 because it clobbers %rcx).
    __ emit_i32_mul(index.gp(), index.gp(), tmp_const.gp());
    __ Load(scratch, table.gp(), index.gp(), 0, LoadType::kI32Load, pinned);

    // Compare against expected signature.
    __ LoadConstant(tmp_const, WasmValue(canonical_sig_num));

    Label* sig_mismatch_label = AddOutOfLineTrap(
        decoder->position(), Builtins::kThrowWasmTrapFuncSigMismatch);
    __ emit_cond_jump(kUnequal, sig_mismatch_label,
                      LiftoffAssembler::kWasmIntPtr, scratch.gp(),
                      tmp_const.gp());

    if (kPointerSize == 8) {
      // {index} has already been multiplied by 4. Multiply by another 2.
      __ LoadConstant(tmp_const, WasmValue(2));
      __ emit_i32_mul(index.gp(), index.gp(), tmp_const.gp());
    }

    // Load the target from {instance->ift_targets[key]}
    LOAD_INSTANCE_FIELD(table, IndirectFunctionTableTargets, kPointerLoadType);
    __ Load(scratch, table.gp(), index.gp(), 0, kPointerLoadType, pinned);

    // Load the instance from {instance->ift_instances[key]}
    LOAD_INSTANCE_FIELD(table, IndirectFunctionTableInstances,
                        kPointerLoadType);
    __ Load(tmp_const, table.gp(), index.gp(),
            (FixedArray::kHeaderSize - kHeapObjectTag), kPointerLoadType,
            pinned);
    LiftoffRegister* explicit_instance = &tmp_const;

    source_position_table_builder_->AddPosition(
        __ pc_offset(), SourcePosition(decoder->position()), false);

    auto call_descriptor =
        compiler::GetWasmCallDescriptor(compilation_zone_, operand.sig);
    call_descriptor =
        GetLoweredCallDescriptor(compilation_zone_, call_descriptor);

    Register target = scratch.gp();
    __ PrepareCall(operand.sig, call_descriptor, &target, explicit_instance);
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
  bool ok_ = true;
  std::vector<OutOfLineCode> out_of_line_code_;
  SourcePositionTableBuilder* const source_position_table_builder_;
  WasmCompilationData* wasm_compilation_data_;
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

  // Points to the cell within the {code_table_} of the NativeModule,
  // which  corresponds to the currently compiled function
  WasmCode* const* code_table_entry_ = nullptr;

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
  wasm::WasmCode* const* code_table_entry =
      native_module_->code_table().data() + func_index_;
  wasm::WasmFullDecoder<wasm::Decoder::kValidate, wasm::LiftoffCompiler>
      decoder(&zone, module, func_body_, &liftoff_.asm_, call_descriptor, env_,
              &liftoff_.source_position_table_builder_, &wasm_compilation_data_,
              &zone, &liftoff_.codegen_zone_, code_table_entry);
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
#undef WASM_INSTANCE_OBJECT_OFFSET
#undef LOAD_INSTANCE_FIELD

}  // namespace internal
}  // namespace v8
