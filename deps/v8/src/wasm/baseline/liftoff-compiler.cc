// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/baseline/liftoff-compiler.h"

#include "src/base/optional.h"
#include "src/codegen/assembler-inl.h"
// TODO(clemensb): Remove dependences on compiler stuff.
#include "src/codegen/interface-descriptors.h"
#include "src/codegen/macro-assembler-inl.h"
#include "src/compiler/linkage.h"
#include "src/compiler/wasm-compiler.h"
#include "src/logging/counters.h"
#include "src/logging/log.h"
#include "src/objects/smi.h"
#include "src/tracing/trace-event.h"
#include "src/utils/ostreams.h"
#include "src/utils/utils.h"
#include "src/wasm/baseline/liftoff-assembler.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/memory-tracing.h"
#include "src/wasm/object-access.h"
#include "src/wasm/wasm-debug.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-linkage.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {
namespace wasm {

constexpr auto kRegister = LiftoffAssembler::VarState::kRegister;
constexpr auto kIntConst = LiftoffAssembler::VarState::kIntConst;
constexpr auto kStack = LiftoffAssembler::VarState::kStack;

namespace {

#define __ asm_.

#define TRACE(...)                                            \
  do {                                                        \
    if (FLAG_trace_liftoff) PrintF("[liftoff] " __VA_ARGS__); \
  } while (false)

#define WASM_INSTANCE_OBJECT_FIELD_OFFSET(name) \
  ObjectAccess::ToTagged(WasmInstanceObject::k##name##Offset)

template <int expected_size, int actual_size>
struct assert_field_size {
  static_assert(expected_size == actual_size,
                "field in WasmInstance does not have the expected size");
  static constexpr int size = actual_size;
};

#define WASM_INSTANCE_OBJECT_FIELD_SIZE(name) \
  FIELD_SIZE(WasmInstanceObject::k##name##Offset)

#define LOAD_INSTANCE_FIELD(dst, name, load_size)                              \
  __ LoadFromInstance(dst, WASM_INSTANCE_OBJECT_FIELD_OFFSET(name),            \
                      assert_field_size<WASM_INSTANCE_OBJECT_FIELD_SIZE(name), \
                                        load_size>::size);

#define LOAD_TAGGED_PTR_INSTANCE_FIELD(dst, name)                         \
  static_assert(WASM_INSTANCE_OBJECT_FIELD_SIZE(name) == kTaggedSize,     \
                "field in WasmInstance does not have the expected size"); \
  __ LoadTaggedPointerFromInstance(dst,                                   \
                                   WASM_INSTANCE_OBJECT_FIELD_OFFSET(name));

#ifdef DEBUG
#define DEBUG_CODE_COMMENT(str) \
  do {                          \
    __ RecordComment(str);      \
  } while (false)
#else
#define DEBUG_CODE_COMMENT(str) ((void)0)
#endif

constexpr LoadType::LoadTypeValue kPointerLoadType =
    kSystemPointerSize == 8 ? LoadType::kI64Load : LoadType::kI32Load;

#if V8_TARGET_ARCH_ARM64
// On ARM64, the Assembler keeps track of pointers to Labels to resolve
// branches to distant targets. Moving labels would confuse the Assembler,
// thus store the label on the heap and keep a unique_ptr.
class MovableLabel {
 public:
  MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(MovableLabel);
  MovableLabel() : label_(new Label()) {}

  Label* get() { return label_.get(); }

 private:
  std::unique_ptr<Label> label_;
};
#else
// On all other platforms, just store the Label directly.
class MovableLabel {
 public:
  MOVE_ONLY_WITH_DEFAULT_CONSTRUCTORS(MovableLabel);

  Label* get() { return &label_; }

 private:
  Label label_;
};
#endif

compiler::CallDescriptor* GetLoweredCallDescriptor(
    Zone* zone, compiler::CallDescriptor* call_desc) {
  return kSystemPointerSize == 4
             ? compiler::GetI32WasmCallDescriptor(zone, call_desc)
             : call_desc;
}

constexpr ValueType kSupportedTypesArr[] = {kWasmI32, kWasmI64, kWasmF32,
                                            kWasmF64, kWasmS128};
constexpr Vector<const ValueType> kSupportedTypes =
    ArrayVector(kSupportedTypesArr);

constexpr Condition GetCompareCondition(WasmOpcode opcode) {
  switch (opcode) {
    case kExprI32Eq:
      return kEqual;
    case kExprI32Ne:
      return kUnequal;
    case kExprI32LtS:
      return kSignedLessThan;
    case kExprI32LtU:
      return kUnsignedLessThan;
    case kExprI32GtS:
      return kSignedGreaterThan;
    case kExprI32GtU:
      return kUnsignedGreaterThan;
    case kExprI32LeS:
      return kSignedLessEqual;
    case kExprI32LeU:
      return kUnsignedLessEqual;
    case kExprI32GeS:
      return kSignedGreaterEqual;
    case kExprI32GeU:
      return kUnsignedGreaterEqual;
    default:
#if V8_HAS_CXX14_CONSTEXPR
      UNREACHABLE();
#else
      // We need to return something for old compilers here.
      return kEqual;
#endif
  }
}

// Builds a {DebugSideTable}.
class DebugSideTableBuilder {
 public:
  class EntryBuilder {
   public:
    explicit EntryBuilder(
        int pc_offset, std::vector<ValueType> stack_types,
        std::vector<int> stack_offsets,
        std::vector<DebugSideTable::Entry::Constant> constants)
        : pc_offset_(pc_offset),
          stack_types_(std::move(stack_types)),
          stack_offsets_(std::move(stack_offsets)),
          constants_(std::move(constants)) {}

    DebugSideTable::Entry ToTableEntry() const {
      return DebugSideTable::Entry{pc_offset_, std::move(stack_types_),
                                   std::move(stack_offsets_),
                                   std::move(constants_)};
    }

    int pc_offset() const { return pc_offset_; }
    void set_pc_offset(int new_pc_offset) { pc_offset_ = new_pc_offset; }

   private:
    int pc_offset_;
    std::vector<ValueType> stack_types_;
    std::vector<int> stack_offsets_;
    std::vector<DebugSideTable::Entry::Constant> constants_;
  };

  // Adds a new entry, and returns a pointer to a builder for modifying that
  // entry ({stack_height} includes {num_locals}).
  EntryBuilder* NewEntry(int pc_offset, int num_locals, int stack_height,
                         LiftoffAssembler::VarState* stack_state) {
    DCHECK_LE(num_locals, stack_height);
    // Record stack types.
    int stack_height_without_locals = stack_height - num_locals;
    std::vector<ValueType> stack_types(stack_height_without_locals);
    for (int i = 0; i < stack_height_without_locals; ++i) {
      stack_types[i] = stack_state[num_locals + i].type();
    }
    // Record stack offsets.
    std::vector<int> stack_offsets(stack_height_without_locals);
    for (int i = 0; i < stack_height_without_locals; ++i) {
      stack_offsets[i] = stack_state[num_locals + i].offset();
    }
    // Record all constants on the locals and stack.
    std::vector<DebugSideTable::Entry::Constant> constants;
    for (int idx = 0; idx < stack_height; ++idx) {
      auto& slot = stack_state[idx];
      if (slot.is_const()) constants.push_back({idx, slot.i32_const()});
    }
    entries_.emplace_back(pc_offset, std::move(stack_types),
                          std::move(stack_offsets), std::move(constants));
    return &entries_.back();
  }

  void AddLocalType(ValueType type, int stack_offset) {
    local_types_.push_back(type);
    local_stack_offsets_.push_back(stack_offset);
  }

  DebugSideTable GenerateDebugSideTable() {
    std::vector<DebugSideTable::Entry> table_entries;
    table_entries.reserve(entries_.size());
    for (auto& entry : entries_) table_entries.push_back(entry.ToTableEntry());
    std::sort(table_entries.begin(), table_entries.end(),
              [](DebugSideTable::Entry& a, DebugSideTable::Entry& b) {
                return a.pc_offset() < b.pc_offset();
              });
    return DebugSideTable{std::move(local_types_),
                          std::move(local_stack_offsets_),
                          std::move(table_entries)};
  }

 private:
  std::vector<ValueType> local_types_;
  std::vector<int> local_stack_offsets_;
  std::list<EntryBuilder> entries_;
};

class LiftoffCompiler {
 public:
  // TODO(clemensb): Make this a template parameter.
  static constexpr Decoder::ValidateFlag validate = Decoder::kValidate;

  using Value = ValueBase;

  struct ElseState {
    MovableLabel label;
    LiftoffAssembler::CacheState state;
  };

  struct Control : public ControlBase<Value> {
    std::unique_ptr<ElseState> else_state;
    LiftoffAssembler::CacheState label_state;
    MovableLabel label;

    MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(Control);

    template <typename... Args>
    explicit Control(Args&&... args) V8_NOEXCEPT
        : ControlBase(std::forward<Args>(args)...) {}
  };

  using FullDecoder = WasmFullDecoder<validate, LiftoffCompiler>;

  struct OutOfLineCode {
    MovableLabel label;
    MovableLabel continuation;
    WasmCode::RuntimeStubId stub;
    WasmCodePosition position;
    LiftoffRegList regs_to_save;
    uint32_t pc;  // for trap handler.
    DebugSideTableBuilder::EntryBuilder* debug_sidetable_entry_builder;

    // Named constructors:
    static OutOfLineCode Trap(
        WasmCode::RuntimeStubId s, WasmCodePosition pos, uint32_t pc,
        DebugSideTableBuilder::EntryBuilder* debug_sidetable_entry_builder) {
      DCHECK_LT(0, pos);
      return {{}, {}, s, pos, {}, pc, debug_sidetable_entry_builder};
    }
    static OutOfLineCode StackCheck(
        WasmCodePosition pos, LiftoffRegList regs,
        DebugSideTableBuilder::EntryBuilder* debug_sidetable_entry_builder) {
      return {{},   {}, WasmCode::kWasmStackGuard,    pos,
              regs, 0,  debug_sidetable_entry_builder};
    }
  };

  LiftoffCompiler(compiler::CallDescriptor* call_descriptor,
                  CompilationEnv* env, Zone* compilation_zone,
                  std::unique_ptr<AssemblerBuffer> buffer,
                  DebugSideTableBuilder* debug_sidetable_builder,
                  Vector<int> breakpoints = {})
      : asm_(std::move(buffer)),
        descriptor_(
            GetLoweredCallDescriptor(compilation_zone, call_descriptor)),
        env_(env),
        debug_sidetable_builder_(debug_sidetable_builder),
        compilation_zone_(compilation_zone),
        safepoint_table_builder_(compilation_zone_),
        next_breakpoint_ptr_(breakpoints.begin()),
        next_breakpoint_end_(breakpoints.end()) {}

  bool did_bailout() const { return bailout_reason_ != kSuccess; }
  LiftoffBailoutReason bailout_reason() const { return bailout_reason_; }

  void GetCode(CodeDesc* desc) {
    asm_.GetCode(nullptr, desc, &safepoint_table_builder_,
                 Assembler::kNoHandlerTable);
  }

  OwnedVector<uint8_t> GetSourcePositionTable() {
    return source_position_table_builder_.ToSourcePositionTableVector();
  }

  OwnedVector<trap_handler::ProtectedInstructionData> GetProtectedInstructions()
      const {
    return OwnedVector<trap_handler::ProtectedInstructionData>::Of(
        protected_instructions_);
  }

  uint32_t GetTotalFrameSlotCount() const {
    return __ GetTotalFrameSlotCount();
  }

  void unsupported(FullDecoder* decoder, LiftoffBailoutReason reason,
                   const char* detail) {
    DCHECK_NE(kSuccess, reason);
    if (did_bailout()) return;
    bailout_reason_ = reason;
    TRACE("unsupported: %s\n", detail);
    decoder->errorf(decoder->pc_offset(), "unsupported liftoff operation: %s",
                    detail);
    UnuseLabels(decoder);
  }

  bool DidAssemblerBailout(FullDecoder* decoder) {
    if (decoder->failed() || !__ did_bailout()) return false;
    unsupported(decoder, __ bailout_reason(), __ bailout_detail());
    return true;
  }

  LiftoffBailoutReason BailoutReasonForType(ValueType type) {
    switch (type) {
      case kWasmS128:
        return kSimd;
      case kWasmAnyRef:
      case kWasmFuncRef:
      case kWasmNullRef:
        return kAnyRef;
      case kWasmExnRef:
        return kExceptionHandling;
      case kWasmBottom:
        return kMultiValue;
      default:
        return kOtherReason;
    }
  }

  bool CheckSupportedType(FullDecoder* decoder,
                          Vector<const ValueType> supported_types,
                          ValueType type, const char* context) {
    // Special case for kWasm128 which requires specific hardware support.
    if (type == kWasmS128 && (!CpuFeatures::SupportsWasmSimd128())) {
      unsupported(decoder, kSimd, "simd");
      return false;
    }
    // Check supported types.
    for (ValueType supported : supported_types) {
      if (type == supported) return true;
    }
    LiftoffBailoutReason bailout_reason = BailoutReasonForType(type);
    EmbeddedVector<char, 128> buffer;
    SNPrintF(buffer, "%s %s", ValueTypes::TypeName(type), context);
    unsupported(decoder, bailout_reason, buffer.begin());
    return false;
  }

  int GetSafepointTableOffset() const {
    return safepoint_table_builder_.GetCodeOffset();
  }

  void UnuseLabels(FullDecoder* decoder) {
#ifdef DEBUG
    auto Unuse = [](Label* label) {
      label->Unuse();
      label->UnuseNear();
    };
    // Unuse all labels now, otherwise their destructor will fire a DCHECK error
    // if they where referenced before.
    uint32_t control_depth = decoder ? decoder->control_depth() : 0;
    for (uint32_t i = 0; i < control_depth; ++i) {
      Control* c = decoder->control_at(i);
      Unuse(c->label.get());
      if (c->else_state) Unuse(c->else_state->label.get());
    }
    for (auto& ool : out_of_line_code_) Unuse(ool.label.get());
#endif
  }

  void StartFunction(FullDecoder* decoder) {
    if (FLAG_trace_liftoff && !FLAG_trace_wasm_decoder) {
      StdoutStream{} << "hint: add --trace-wasm-decoder to also see the wasm "
                        "instructions being decoded\n";
    }
    int num_locals = decoder->num_locals();
    __ set_num_locals(num_locals);
    for (int i = 0; i < num_locals; ++i) {
      ValueType type = decoder->GetLocalType(i);
      __ set_local_type(i, type);
    }
  }

  // Returns the number of inputs processed (1 or 2).
  uint32_t ProcessParameter(ValueType type, uint32_t input_idx) {
    const int num_lowered_params = 1 + needs_gp_reg_pair(type);
    ValueType lowered_type = needs_gp_reg_pair(type) ? kWasmI32 : type;
    RegClass rc = reg_class_for(lowered_type);
    // Initialize to anything, will be set in the loop and used afterwards.
    LiftoffRegister reg = kGpCacheRegList.GetFirstRegSet();
    LiftoffRegList pinned;
    for (int pair_idx = 0; pair_idx < num_lowered_params; ++pair_idx) {
      compiler::LinkageLocation param_loc =
          descriptor_->GetInputLocation(input_idx + pair_idx);
      // Initialize to anything, will be set in both arms of the if.
      LiftoffRegister in_reg = kGpCacheRegList.GetFirstRegSet();
      if (param_loc.IsRegister()) {
        DCHECK(!param_loc.IsAnyRegister());
        int reg_code = param_loc.AsRegister();
        if (!kSimpleFPAliasing && type == kWasmF32) {
          // Liftoff assumes a one-to-one mapping between float registers and
          // double registers, and so does not distinguish between f32 and f64
          // registers. The f32 register code must therefore be halved in order
          // to pass the f64 code to Liftoff.
          DCHECK_EQ(0, reg_code % 2);
          reg_code /= 2;
        } else if (kNeedS128RegPair && type == kWasmS128) {
          // Similarly for double registers and SIMD registers, the SIMD code
          // needs to be doubled to pass the f64 code to Liftoff.
          reg_code *= 2;
        }
        RegList cache_regs = rc == kGpReg ? kLiftoffAssemblerGpCacheRegs
                                          : kLiftoffAssemblerFpCacheRegs;
        if (cache_regs & (1ULL << reg_code)) {
          // This is a cache register, just use it.
          if (kNeedS128RegPair && rc == kFpRegPair) {
            in_reg =
                LiftoffRegister::ForFpPair(DoubleRegister::from_code(reg_code));
          } else {
            in_reg = LiftoffRegister::from_code(rc, reg_code);
          }
        } else {
          // Move to a cache register (spill one if necessary).
          // Note that we cannot create a {LiftoffRegister} for reg_code, since
          // {LiftoffRegister} can only store cache regs.
          in_reg = __ GetUnusedRegister(rc, pinned);
          if (rc == kGpReg) {
            __ Move(in_reg.gp(), Register::from_code(reg_code), lowered_type);
          } else if (kNeedS128RegPair && rc == kFpRegPair) {
            __ Move(in_reg.low_fp(), DoubleRegister::from_code(reg_code),
                    lowered_type);
          } else {
            DCHECK_EQ(kFpReg, rc);
            __ Move(in_reg.fp(), DoubleRegister::from_code(reg_code),
                    lowered_type);
          }
        }
      } else if (param_loc.IsCallerFrameSlot()) {
        in_reg = __ GetUnusedRegister(rc, pinned);
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

  void StackCheck(WasmCodePosition position) {
    if (!FLAG_wasm_stack_checks || !env_->runtime_exception_support) return;
    out_of_line_code_.push_back(
        OutOfLineCode::StackCheck(position, __ cache_state()->used_registers,
                                  RegisterDebugSideTableEntry()));
    OutOfLineCode& ool = out_of_line_code_.back();
    Register limit_address = __ GetUnusedRegister(kGpReg).gp();
    LOAD_INSTANCE_FIELD(limit_address, StackLimitAddress, kSystemPointerSize);
    __ StackCheck(ool.label.get(), limit_address);
    __ bind(ool.continuation.get());
  }

  bool SpillLocalsInitially(FullDecoder* decoder, uint32_t num_params) {
    int actual_locals = __ num_locals() - num_params;
    DCHECK_LE(0, actual_locals);
    constexpr int kNumCacheRegisters = NumRegs(kLiftoffAssemblerGpCacheRegs);
    // If we have many locals, we put them on the stack initially. This avoids
    // having to spill them on merge points. Use of these initial values should
    // be rare anyway.
    if (actual_locals > kNumCacheRegisters / 2) return true;
    // If there are locals which are not i32 or i64, we also spill all locals,
    // because other types cannot be initialized to constants.
    for (uint32_t param_idx = num_params; param_idx < __ num_locals();
         ++param_idx) {
      ValueType type = decoder->GetLocalType(param_idx);
      if (type != kWasmI32 && type != kWasmI64) return true;
    }
    return false;
  }

  void StartFunctionBody(FullDecoder* decoder, Control* block) {
    for (uint32_t i = 0; i < __ num_locals(); ++i) {
      if (!CheckSupportedType(decoder, kSupportedTypes, __ local_type(i),
                              "param"))
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
    DCHECK_EQ(kWasmInstanceRegister, instance_reg);

    // Parameter 0 is the instance parameter.
    uint32_t num_params =
        static_cast<uint32_t>(decoder->sig_->parameter_count());

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

    // Process parameters.
    __ SpillInstance(instance_reg);
    // Input 0 is the code target, 1 is the instance. First parameter at 2.
    uint32_t input_idx = kInstanceParameterIndex + 1;
    for (uint32_t param_idx = 0; param_idx < num_params; ++param_idx) {
      input_idx += ProcessParameter(__ local_type(param_idx), input_idx);
    }
    int params_size = __ TopSpillOffset();
    DCHECK_EQ(input_idx, descriptor_->InputCount());

    // Initialize locals beyond parameters.
    if (SpillLocalsInitially(decoder, num_params)) {
      for (uint32_t param_idx = num_params; param_idx < __ num_locals();
           ++param_idx) {
        ValueType type = decoder->GetLocalType(param_idx);
        __ PushStack(type);
      }
      int spill_size = __ TopSpillOffset() - params_size;
      __ FillStackSlotsWithZero(params_size, spill_size);
    } else {
      for (uint32_t param_idx = num_params; param_idx < __ num_locals();
           ++param_idx) {
        ValueType type = decoder->GetLocalType(param_idx);
        __ PushConstant(type, int32_t{0});
      }
    }

    DCHECK_EQ(__ num_locals(), __ cache_state()->stack_height());

    // Register local types and stack offsets for the debug side table.
    if (V8_UNLIKELY(debug_sidetable_builder_)) {
      for (uint32_t i = 0; i < __ num_locals(); ++i) {
        debug_sidetable_builder_->AddLocalType(
            __ local_type(i), __ cache_state()->stack_state[i].offset());
      }
    }
    // The function-prologue stack check is associated with position 0, which
    // is never a position of any instruction in the function.
    StackCheck(0);
  }

  void GenerateOutOfLineCode(OutOfLineCode* ool) {
    __ bind(ool->label.get());
    const bool is_stack_check = ool->stub == WasmCode::kWasmStackGuard;
    const bool is_mem_out_of_bounds =
        ool->stub == WasmCode::kThrowWasmTrapMemOutOfBounds;

    if (is_mem_out_of_bounds && env_->use_trap_handler) {
      uint32_t pc = static_cast<uint32_t>(__ pc_offset());
      DCHECK_EQ(pc, __ pc_offset());
      protected_instructions_.emplace_back(
          trap_handler::ProtectedInstructionData{ool->pc, pc});
    }

    if (!env_->runtime_exception_support) {
      // We cannot test calls to the runtime in cctest/test-run-wasm.
      // Therefore we emit a call to C here instead of a call to the runtime.
      // In this mode, we never generate stack checks.
      DCHECK(!is_stack_check);
      __ CallTrapCallbackForTesting();
      __ LeaveFrame(StackFrame::WASM_COMPILED);
      __ DropStackSlotsAndRet(
          static_cast<uint32_t>(descriptor_->StackParameterCount()));
      return;
    }

    if (!ool->regs_to_save.is_empty()) __ PushRegisters(ool->regs_to_save);

    source_position_table_builder_.AddPosition(
        __ pc_offset(), SourcePosition(ool->position), false);
    __ CallRuntimeStub(ool->stub);
    DCHECK_EQ(!debug_sidetable_builder_, !ool->debug_sidetable_entry_builder);
    if (V8_UNLIKELY(ool->debug_sidetable_entry_builder)) {
      ool->debug_sidetable_entry_builder->set_pc_offset(__ pc_offset());
    }
    safepoint_table_builder_.DefineSafepoint(&asm_, Safepoint::kNoLazyDeopt);
    DCHECK_EQ(ool->continuation.get()->is_bound(), is_stack_check);
    if (!ool->regs_to_save.is_empty()) __ PopRegisters(ool->regs_to_save);
    if (is_stack_check) {
      __ emit_jump(ool->continuation.get());
    } else {
      __ AssertUnreachable(AbortReason::kUnexpectedReturnFromWasmTrap);
    }
  }

  void FinishFunction(FullDecoder* decoder) {
    // All breakpoints (if any) must be emitted by now.
    DCHECK_NULL(next_breakpoint_ptr_);
    if (DidAssemblerBailout(decoder)) return;
    for (OutOfLineCode& ool : out_of_line_code_) {
      GenerateOutOfLineCode(&ool);
    }
    __ PatchPrepareStackFrame(pc_offset_stack_frame_construction_,
                              __ GetTotalFrameSize());
    __ FinishCode();
    safepoint_table_builder_.Emit(&asm_, __ GetTotalFrameSlotCount());
    __ MaybeEmitOutOfLineConstantPool();
    // The previous calls may have also generated a bailout.
    DidAssemblerBailout(decoder);
  }

  void OnFirstError(FullDecoder* decoder) {
    if (!did_bailout()) bailout_reason_ = kDecodeError;
    UnuseLabels(decoder);
    asm_.AbortCompilation();
  }

  void NextInstruction(FullDecoder* decoder, WasmOpcode opcode) {
    if (V8_UNLIKELY(next_breakpoint_ptr_) &&
        *next_breakpoint_ptr_ == decoder->position()) {
      ++next_breakpoint_ptr_;
      if (next_breakpoint_ptr_ == next_breakpoint_end_) {
        next_breakpoint_ptr_ = next_breakpoint_end_ = nullptr;
      }
      EmitBreakpoint();
    }
    TraceCacheState(decoder);
    SLOW_DCHECK(__ ValidateCacheState());
    DEBUG_CODE_COMMENT(WasmOpcodes::OpcodeName(opcode));
  }

  void EmitBreakpoint() {
    DEBUG_CODE_COMMENT("breakpoint");
    // TODO(clemensb): Actually emit a breakpoint.
  }

  void Block(FullDecoder* decoder, Control* block) {}

  void Loop(FullDecoder* decoder, Control* loop) {
    if (loop->start_merge.arity > 0 || loop->end_merge.arity > 1) {
      return unsupported(decoder, kMultiValue, "multi-value loop");
    }

    // Before entering a loop, spill all locals to the stack, in order to free
    // the cache registers, and to avoid unnecessarily reloading stack values
    // into registers at branches.
    // TODO(clemensb): Come up with a better strategy here, involving
    // pre-analysis of the function.
    __ SpillLocals();

    // Loop labels bind at the beginning of the block.
    __ bind(loop->label.get());

    // Save the current cache state for the merge when jumping to this loop.
    loop->label_state.Split(*__ cache_state());

    // Execute a stack check in the loop header.
    StackCheck(decoder->position());
  }

  void Try(FullDecoder* decoder, Control* block) {
    unsupported(decoder, kExceptionHandling, "try");
  }

  void Catch(FullDecoder* decoder, Control* block, Value* exception) {
    unsupported(decoder, kExceptionHandling, "catch");
  }

  void If(FullDecoder* decoder, const Value& cond, Control* if_block) {
    DCHECK_EQ(if_block, decoder->control_at(0));
    DCHECK(if_block->is_if());

    if (if_block->start_merge.arity > 0 || if_block->end_merge.arity > 1) {
      return unsupported(decoder, kMultiValue, "multi-value if");
    }

    // Allocate the else state.
    if_block->else_state = std::make_unique<ElseState>();

    // Test the condition, jump to else if zero.
    Register value = __ PopToRegister().gp();
    __ emit_cond_jump(kEqual, if_block->else_state->label.get(), kWasmI32,
                      value);

    // Store the state (after popping the value) for executing the else branch.
    if_block->else_state->state.Split(*__ cache_state());
  }

  void FallThruTo(FullDecoder* decoder, Control* c) {
    if (c->end_merge.reached) {
      __ MergeFullStackWith(c->label_state, *__ cache_state());
    } else {
      c->label_state.Split(*__ cache_state());
    }
    TraceCacheState(decoder);
  }

  void FinishOneArmedIf(FullDecoder* decoder, Control* c) {
    DCHECK(c->is_onearmed_if());
    if (c->end_merge.reached) {
      // Someone already merged to the end of the if. Merge both arms into that.
      if (c->reachable()) {
        // Merge the if state into the end state.
        __ MergeFullStackWith(c->label_state, *__ cache_state());
        __ emit_jump(c->label.get());
      }
      // Merge the else state into the end state.
      __ bind(c->else_state->label.get());
      __ MergeFullStackWith(c->label_state, c->else_state->state);
      __ cache_state()->Steal(c->label_state);
    } else if (c->reachable()) {
      // No merge yet at the end of the if, but we need to create a merge for
      // the both arms of this if. Thus init the merge point from the else
      // state, then merge the if state into that.
      DCHECK_EQ(0, c->end_merge.arity);
      c->label_state.InitMerge(c->else_state->state, __ num_locals(), 0,
                               c->stack_depth);
      __ MergeFullStackWith(c->label_state, *__ cache_state());
      __ emit_jump(c->label.get());
      // Merge the else state into the end state.
      __ bind(c->else_state->label.get());
      __ MergeFullStackWith(c->label_state, c->else_state->state);
      __ cache_state()->Steal(c->label_state);
    } else {
      // No merge needed, just continue with the else state.
      __ bind(c->else_state->label.get());
      __ cache_state()->Steal(c->else_state->state);
    }
  }

  void PopControl(FullDecoder* decoder, Control* c) {
    if (c->is_loop()) return;  // A loop just falls through.
    if (c->is_onearmed_if()) {
      // Special handling for one-armed ifs.
      FinishOneArmedIf(decoder, c);
    } else if (c->end_merge.reached) {
      // There is a merge already. Merge our state into that, then continue with
      // that state.
      if (c->reachable()) {
        __ MergeFullStackWith(c->label_state, *__ cache_state());
      }
      __ cache_state()->Steal(c->label_state);
    } else {
      // No merge, just continue with our current state.
    }

    if (!c->label.get()->is_bound()) __ bind(c->label.get());
  }

  void EndControl(FullDecoder* decoder, Control* c) {}

  enum CCallReturn : bool { kHasReturn = true, kNoReturn = false };

  void GenerateCCall(const LiftoffRegister* result_regs, FunctionSig* sig,
                     ValueType out_argument_type,
                     const LiftoffRegister* arg_regs,
                     ExternalReference ext_ref) {
    // Before making a call, spill all cache registers.
    __ SpillAllRegisters();

    // Store arguments on our stack, then align the stack for calling to C.
    int param_bytes = 0;
    for (ValueType param_type : sig->parameters()) {
      param_bytes += ValueTypes::MemSize(param_type);
    }
    int out_arg_bytes = out_argument_type == kWasmStmt
                            ? 0
                            : ValueTypes::MemSize(out_argument_type);
    int stack_bytes = std::max(param_bytes, out_arg_bytes);
    __ CallC(sig, arg_regs, result_regs, out_argument_type, stack_bytes,
             ext_ref);
  }

  template <ValueType src_type, ValueType result_type, class EmitFn>
  void EmitUnOp(EmitFn fn) {
    constexpr RegClass src_rc = reg_class_for(src_type);
    constexpr RegClass result_rc = reg_class_for(result_type);
    LiftoffRegister src = __ PopToRegister();
    LiftoffRegister dst = src_rc == result_rc
                              ? __ GetUnusedRegister(result_rc, {src})
                              : __ GetUnusedRegister(result_rc);
    fn(dst, src);
    __ PushRegister(result_type, dst);
  }

  template <ValueType type>
  void EmitFloatUnOpWithCFallback(
      bool (LiftoffAssembler::*emit_fn)(DoubleRegister, DoubleRegister),
      ExternalReference (*fallback_fn)()) {
    auto emit_with_c_fallback = [=](LiftoffRegister dst, LiftoffRegister src) {
      if ((asm_.*emit_fn)(dst.fp(), src.fp())) return;
      ExternalReference ext_ref = fallback_fn();
      ValueType sig_reps[] = {type};
      FunctionSig sig(0, 1, sig_reps);
      GenerateCCall(&dst, &sig, type, &src, ext_ref);
    };
    EmitUnOp<type, type>(emit_with_c_fallback);
  }

  enum TypeConversionTrapping : bool { kCanTrap = true, kNoTrap = false };
  template <ValueType dst_type, ValueType src_type,
            TypeConversionTrapping can_trap>
  void EmitTypeConversion(WasmOpcode opcode, ExternalReference (*fallback_fn)(),
                          WasmCodePosition trap_position) {
    static constexpr RegClass src_rc = reg_class_for(src_type);
    static constexpr RegClass dst_rc = reg_class_for(dst_type);
    LiftoffRegister src = __ PopToRegister();
    LiftoffRegister dst = src_rc == dst_rc ? __ GetUnusedRegister(dst_rc, {src})
                                           : __ GetUnusedRegister(dst_rc);
    DCHECK_EQ(!!can_trap, trap_position > 0);
    Label* trap = can_trap ? AddOutOfLineTrap(
                                 trap_position,
                                 WasmCode::kThrowWasmTrapFloatUnrepresentable)
                           : nullptr;
    if (!__ emit_type_conversion(opcode, dst, src, trap)) {
      DCHECK_NOT_NULL(fallback_fn);
      ExternalReference ext_ref = fallback_fn();
      if (can_trap) {
        // External references for potentially trapping conversions return int.
        ValueType sig_reps[] = {kWasmI32, src_type};
        FunctionSig sig(1, 1, sig_reps);
        LiftoffRegister ret_reg =
            __ GetUnusedRegister(kGpReg, LiftoffRegList::ForRegs(dst));
        LiftoffRegister dst_regs[] = {ret_reg, dst};
        GenerateCCall(dst_regs, &sig, dst_type, &src, ext_ref);
        __ emit_cond_jump(kEqual, trap, kWasmI32, ret_reg.gp());
      } else {
        ValueType sig_reps[] = {src_type};
        FunctionSig sig(0, 1, sig_reps);
        GenerateCCall(&dst, &sig, dst_type, &src, ext_ref);
      }
    }
    __ PushRegister(dst_type, dst);
  }

  void UnOp(FullDecoder* decoder, WasmOpcode opcode, const Value& value,
            Value* result) {
#define CASE_I32_UNOP(opcode, fn)                       \
  case kExpr##opcode:                                   \
    EmitUnOp<kWasmI32, kWasmI32>(                       \
        [=](LiftoffRegister dst, LiftoffRegister src) { \
          __ emit_##fn(dst.gp(), src.gp());             \
        });                                             \
    break;
#define CASE_I64_UNOP(opcode, fn)                       \
  case kExpr##opcode:                                   \
    EmitUnOp<kWasmI64, kWasmI64>(                       \
        [=](LiftoffRegister dst, LiftoffRegister src) { \
          __ emit_##fn(dst, src);                       \
        });                                             \
    break;
#define CASE_FLOAT_UNOP(opcode, type, fn)               \
  case kExpr##opcode:                                   \
    EmitUnOp<kWasm##type, kWasm##type>(                 \
        [=](LiftoffRegister dst, LiftoffRegister src) { \
          __ emit_##fn(dst.fp(), src.fp());             \
        });                                             \
    break;
#define CASE_FLOAT_UNOP_WITH_CFALLBACK(opcode, type, fn)                    \
  case kExpr##opcode:                                                       \
    EmitFloatUnOpWithCFallback<kWasm##type>(&LiftoffAssembler::emit_##fn,   \
                                            &ExternalReference::wasm_##fn); \
    break;
#define CASE_TYPE_CONVERSION(opcode, dst_type, src_type, ext_ref, can_trap) \
  case kExpr##opcode:                                                       \
    EmitTypeConversion<kWasm##dst_type, kWasm##src_type, can_trap>(         \
        kExpr##opcode, ext_ref, can_trap ? decoder->position() : 0);        \
    break;
    switch (opcode) {
      CASE_I32_UNOP(I32Clz, i32_clz)
      CASE_I32_UNOP(I32Ctz, i32_ctz)
      CASE_FLOAT_UNOP(F32Abs, F32, f32_abs)
      CASE_FLOAT_UNOP(F32Neg, F32, f32_neg)
      CASE_FLOAT_UNOP_WITH_CFALLBACK(F32Ceil, F32, f32_ceil)
      CASE_FLOAT_UNOP_WITH_CFALLBACK(F32Floor, F32, f32_floor)
      CASE_FLOAT_UNOP_WITH_CFALLBACK(F32Trunc, F32, f32_trunc)
      CASE_FLOAT_UNOP_WITH_CFALLBACK(F32NearestInt, F32, f32_nearest_int)
      CASE_FLOAT_UNOP(F32Sqrt, F32, f32_sqrt)
      CASE_FLOAT_UNOP(F64Abs, F64, f64_abs)
      CASE_FLOAT_UNOP(F64Neg, F64, f64_neg)
      CASE_FLOAT_UNOP_WITH_CFALLBACK(F64Ceil, F64, f64_ceil)
      CASE_FLOAT_UNOP_WITH_CFALLBACK(F64Floor, F64, f64_floor)
      CASE_FLOAT_UNOP_WITH_CFALLBACK(F64Trunc, F64, f64_trunc)
      CASE_FLOAT_UNOP_WITH_CFALLBACK(F64NearestInt, F64, f64_nearest_int)
      CASE_FLOAT_UNOP(F64Sqrt, F64, f64_sqrt)
      CASE_TYPE_CONVERSION(I32ConvertI64, I32, I64, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(I32SConvertF32, I32, F32, nullptr, kCanTrap)
      CASE_TYPE_CONVERSION(I32UConvertF32, I32, F32, nullptr, kCanTrap)
      CASE_TYPE_CONVERSION(I32SConvertF64, I32, F64, nullptr, kCanTrap)
      CASE_TYPE_CONVERSION(I32UConvertF64, I32, F64, nullptr, kCanTrap)
      CASE_TYPE_CONVERSION(I32ReinterpretF32, I32, F32, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(I64SConvertI32, I64, I32, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(I64UConvertI32, I64, I32, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(I64SConvertF32, I64, F32,
                           &ExternalReference::wasm_float32_to_int64, kCanTrap)
      CASE_TYPE_CONVERSION(I64UConvertF32, I64, F32,
                           &ExternalReference::wasm_float32_to_uint64, kCanTrap)
      CASE_TYPE_CONVERSION(I64SConvertF64, I64, F64,
                           &ExternalReference::wasm_float64_to_int64, kCanTrap)
      CASE_TYPE_CONVERSION(I64UConvertF64, I64, F64,
                           &ExternalReference::wasm_float64_to_uint64, kCanTrap)
      CASE_TYPE_CONVERSION(I64ReinterpretF64, I64, F64, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(F32SConvertI32, F32, I32, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(F32UConvertI32, F32, I32, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(F32SConvertI64, F32, I64,
                           &ExternalReference::wasm_int64_to_float32, kNoTrap)
      CASE_TYPE_CONVERSION(F32UConvertI64, F32, I64,
                           &ExternalReference::wasm_uint64_to_float32, kNoTrap)
      CASE_TYPE_CONVERSION(F32ConvertF64, F32, F64, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(F32ReinterpretI32, F32, I32, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(F64SConvertI32, F64, I32, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(F64UConvertI32, F64, I32, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(F64SConvertI64, F64, I64,
                           &ExternalReference::wasm_int64_to_float64, kNoTrap)
      CASE_TYPE_CONVERSION(F64UConvertI64, F64, I64,
                           &ExternalReference::wasm_uint64_to_float64, kNoTrap)
      CASE_TYPE_CONVERSION(F64ConvertF32, F64, F32, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(F64ReinterpretI64, F64, I64, nullptr, kNoTrap)
      CASE_I32_UNOP(I32SExtendI8, i32_signextend_i8)
      CASE_I32_UNOP(I32SExtendI16, i32_signextend_i16)
      CASE_I64_UNOP(I64SExtendI8, i64_signextend_i8)
      CASE_I64_UNOP(I64SExtendI16, i64_signextend_i16)
      CASE_I64_UNOP(I64SExtendI32, i64_signextend_i32)
      CASE_I64_UNOP(I64Clz, i64_clz)
      CASE_I64_UNOP(I64Ctz, i64_ctz)
      case kExprI32Eqz:
        DCHECK(decoder->lookahead(0, kExprI32Eqz));
        if (decoder->lookahead(1, kExprBrIf)) {
          DCHECK(!has_outstanding_op());
          outstanding_op_ = kExprI32Eqz;
          break;
        }
        EmitUnOp<kWasmI32, kWasmI32>(
            [=](LiftoffRegister dst, LiftoffRegister src) {
              __ emit_i32_eqz(dst.gp(), src.gp());
            });
        break;
      case kExprI64Eqz:
        EmitUnOp<kWasmI64, kWasmI32>(
            [=](LiftoffRegister dst, LiftoffRegister src) {
              __ emit_i64_eqz(dst.gp(), src);
            });
        break;
      case kExprI32Popcnt:
        EmitUnOp<kWasmI32, kWasmI32>(
            [=](LiftoffRegister dst, LiftoffRegister src) {
              if (__ emit_i32_popcnt(dst.gp(), src.gp())) return;
              ValueType sig_i_i_reps[] = {kWasmI32, kWasmI32};
              FunctionSig sig_i_i(1, 1, sig_i_i_reps);
              GenerateCCall(&dst, &sig_i_i, kWasmStmt, &src,
                            ExternalReference::wasm_word32_popcnt());
            });
        break;
      case kExprI64Popcnt:
        EmitUnOp<kWasmI64, kWasmI64>(
            [=](LiftoffRegister dst, LiftoffRegister src) {
              if (__ emit_i64_popcnt(dst, src)) return;
              // The c function returns i32. We will zero-extend later.
              ValueType sig_i_l_reps[] = {kWasmI32, kWasmI64};
              FunctionSig sig_i_l(1, 1, sig_i_l_reps);
              LiftoffRegister c_call_dst = kNeedI64RegPair ? dst.low() : dst;
              GenerateCCall(&c_call_dst, &sig_i_l, kWasmStmt, &src,
                            ExternalReference::wasm_word64_popcnt());
              // Now zero-extend the result to i64.
              __ emit_type_conversion(kExprI64UConvertI32, dst, c_call_dst,
                                      nullptr);
            });
        break;
      case kExprI32SConvertSatF32:
      case kExprI32UConvertSatF32:
      case kExprI32SConvertSatF64:
      case kExprI32UConvertSatF64:
      case kExprI64SConvertSatF32:
      case kExprI64UConvertSatF32:
      case kExprI64SConvertSatF64:
      case kExprI64UConvertSatF64:
        return unsupported(decoder, kNonTrappingFloatToInt,
                           WasmOpcodes::OpcodeName(opcode));
      default:
        UNREACHABLE();
    }
#undef CASE_I32_UNOP
#undef CASE_I64_UNOP
#undef CASE_FLOAT_UNOP
#undef CASE_FLOAT_UNOP_WITH_CFALLBACK
#undef CASE_TYPE_CONVERSION
  }

  template <ValueType src_type, ValueType result_type, typename EmitFn,
            typename EmitFnImm>
  void EmitBinOpImm(EmitFn fn, EmitFnImm fnImm) {
    static constexpr RegClass src_rc = reg_class_for(src_type);
    static constexpr RegClass result_rc = reg_class_for(result_type);

    LiftoffAssembler::VarState rhs_slot = __ cache_state()->stack_state.back();
    // Check if the RHS is an immediate.
    if (rhs_slot.is_const()) {
      __ cache_state()->stack_state.pop_back();
      int32_t imm = rhs_slot.i32_const();

      LiftoffRegister lhs = __ PopToRegister();
      LiftoffRegister dst = src_rc == result_rc
                                ? __ GetUnusedRegister(result_rc, {lhs})
                                : __ GetUnusedRegister(result_rc);

      fnImm(dst, lhs, imm);
      __ PushRegister(result_type, dst);
    } else {
      // The RHS was not an immediate.
      LiftoffRegister rhs = __ PopToRegister();
      LiftoffRegister lhs = __ PopToRegister(LiftoffRegList::ForRegs(rhs));
      LiftoffRegister dst = src_rc == result_rc
                                ? __ GetUnusedRegister(result_rc, {lhs, rhs})
                                : __ GetUnusedRegister(result_rc);
      fn(dst, lhs, rhs);
      __ PushRegister(result_type, dst);
    }
  }

  template <ValueType src_type, ValueType result_type, typename EmitFn>
  void EmitBinOp(EmitFn fn) {
    static constexpr RegClass src_rc = reg_class_for(src_type);
    static constexpr RegClass result_rc = reg_class_for(result_type);
    LiftoffRegister rhs = __ PopToRegister();
    LiftoffRegister lhs = __ PopToRegister(LiftoffRegList::ForRegs(rhs));
    LiftoffRegister dst = src_rc == result_rc
                              ? __ GetUnusedRegister(result_rc, {lhs, rhs})
                              : __ GetUnusedRegister(result_rc);
    fn(dst, lhs, rhs);
    __ PushRegister(result_type, dst);
  }

  void EmitDivOrRem64CCall(LiftoffRegister dst, LiftoffRegister lhs,
                           LiftoffRegister rhs, ExternalReference ext_ref,
                           Label* trap_by_zero,
                           Label* trap_unrepresentable = nullptr) {
    // Cannot emit native instructions, build C call.
    LiftoffRegister ret =
        __ GetUnusedRegister(kGpReg, LiftoffRegList::ForRegs(dst));
    LiftoffRegister tmp =
        __ GetUnusedRegister(kGpReg, LiftoffRegList::ForRegs(dst, ret));
    LiftoffRegister arg_regs[] = {lhs, rhs};
    LiftoffRegister result_regs[] = {ret, dst};
    ValueType sig_types[] = {kWasmI32, kWasmI64, kWasmI64};
    // <i64, i64> -> i32 (with i64 output argument)
    FunctionSig sig(1, 2, sig_types);
    GenerateCCall(result_regs, &sig, kWasmI64, arg_regs, ext_ref);
    __ LoadConstant(tmp, WasmValue(int32_t{0}));
    __ emit_cond_jump(kEqual, trap_by_zero, kWasmI32, ret.gp(), tmp.gp());
    if (trap_unrepresentable) {
      __ LoadConstant(tmp, WasmValue(int32_t{-1}));
      __ emit_cond_jump(kEqual, trap_unrepresentable, kWasmI32, ret.gp(),
                        tmp.gp());
    }
  }

  void BinOp(FullDecoder* decoder, WasmOpcode opcode, const Value& lhs,
             const Value& rhs, Value* result) {
#define CASE_I32_BINOP(opcode, fn)                                           \
  case kExpr##opcode:                                                        \
    return EmitBinOp<kWasmI32, kWasmI32>(                                    \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          __ emit_##fn(dst.gp(), lhs.gp(), rhs.gp());                        \
        });
#define CASE_I32_BINOPI(opcode, fn)                                          \
  case kExpr##opcode:                                                        \
    return EmitBinOpImm<kWasmI32, kWasmI32>(                                 \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          __ emit_##fn(dst.gp(), lhs.gp(), rhs.gp());                        \
        },                                                                   \
        [=](LiftoffRegister dst, LiftoffRegister lhs, int32_t imm) {         \
          __ emit_##fn(dst.gp(), lhs.gp(), imm);                             \
        });
#define CASE_I64_BINOP(opcode, fn)                                           \
  case kExpr##opcode:                                                        \
    return EmitBinOp<kWasmI64, kWasmI64>(                                    \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          __ emit_##fn(dst, lhs, rhs);                                       \
        });
#define CASE_I64_BINOPI(opcode, fn)                                          \
  case kExpr##opcode:                                                        \
    return EmitBinOpImm<kWasmI64, kWasmI64>(                                 \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          __ emit_##fn(dst, lhs, rhs);                                       \
        },                                                                   \
        [=](LiftoffRegister dst, LiftoffRegister lhs, int32_t imm) {         \
          __ emit_##fn(dst, lhs, imm);                                       \
        });
#define CASE_FLOAT_BINOP(opcode, type, fn)                                   \
  case kExpr##opcode:                                                        \
    return EmitBinOp<kWasm##type, kWasm##type>(                              \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          __ emit_##fn(dst.fp(), lhs.fp(), rhs.fp());                        \
        });
#define CASE_I32_CMPOP(opcode)                                               \
  case kExpr##opcode:                                                        \
    DCHECK(decoder->lookahead(0, kExpr##opcode));                            \
    if (decoder->lookahead(1, kExprBrIf)) {                                  \
      DCHECK(!has_outstanding_op());                                         \
      outstanding_op_ = kExpr##opcode;                                       \
      break;                                                                 \
    }                                                                        \
    return EmitBinOp<kWasmI32, kWasmI32>(                                    \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          constexpr Condition cond = GetCompareCondition(kExpr##opcode);     \
          __ emit_i32_set_cond(cond, dst.gp(), lhs.gp(), rhs.gp());          \
        });
#define CASE_I64_CMPOP(opcode, cond)                                         \
  case kExpr##opcode:                                                        \
    return EmitBinOp<kWasmI64, kWasmI32>(                                    \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          __ emit_i64_set_cond(cond, dst.gp(), lhs, rhs);                    \
        });
#define CASE_F32_CMPOP(opcode, cond)                                         \
  case kExpr##opcode:                                                        \
    return EmitBinOp<kWasmF32, kWasmI32>(                                    \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          __ emit_f32_set_cond(cond, dst.gp(), lhs.fp(), rhs.fp());          \
        });
#define CASE_F64_CMPOP(opcode, cond)                                         \
  case kExpr##opcode:                                                        \
    return EmitBinOp<kWasmF64, kWasmI32>(                                    \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          __ emit_f64_set_cond(cond, dst.gp(), lhs.fp(), rhs.fp());          \
        });
#define CASE_I64_SHIFTOP(opcode, fn)                                         \
  case kExpr##opcode:                                                        \
    return EmitBinOpImm<kWasmI64, kWasmI64>(                                 \
        [=](LiftoffRegister dst, LiftoffRegister src,                        \
            LiftoffRegister amount) {                                        \
          __ emit_##fn(dst, src,                                             \
                       amount.is_gp_pair() ? amount.low_gp() : amount.gp()); \
        },                                                                   \
        [=](LiftoffRegister dst, LiftoffRegister src, int32_t amount) {      \
          __ emit_##fn(dst, src, amount);                                    \
        });
#define CASE_CCALL_BINOP(opcode, type, ext_ref_fn)                           \
  case kExpr##opcode:                                                        \
    return EmitBinOp<kWasm##type, kWasm##type>(                              \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          LiftoffRegister args[] = {lhs, rhs};                               \
          auto ext_ref = ExternalReference::ext_ref_fn();                    \
          ValueType sig_reps[] = {kWasm##type, kWasm##type, kWasm##type};    \
          const bool out_via_stack = kWasm##type == kWasmI64;                \
          FunctionSig sig(out_via_stack ? 0 : 1, 2, sig_reps);               \
          ValueType out_arg_type = out_via_stack ? kWasmI64 : kWasmStmt;     \
          GenerateCCall(&dst, &sig, out_arg_type, args, ext_ref);            \
        });
    switch (opcode) {
      CASE_I32_BINOPI(I32Add, i32_add)
      CASE_I32_BINOP(I32Sub, i32_sub)
      CASE_I32_BINOP(I32Mul, i32_mul)
      CASE_I32_BINOPI(I32And, i32_and)
      CASE_I32_BINOPI(I32Ior, i32_or)
      CASE_I32_BINOPI(I32Xor, i32_xor)
      CASE_I64_BINOPI(I64And, i64_and)
      CASE_I64_BINOPI(I64Ior, i64_or)
      CASE_I64_BINOPI(I64Xor, i64_xor)
      CASE_I32_CMPOP(I32Eq)
      CASE_I32_CMPOP(I32Ne)
      CASE_I32_CMPOP(I32LtS)
      CASE_I32_CMPOP(I32LtU)
      CASE_I32_CMPOP(I32GtS)
      CASE_I32_CMPOP(I32GtU)
      CASE_I32_CMPOP(I32LeS)
      CASE_I32_CMPOP(I32LeU)
      CASE_I32_CMPOP(I32GeS)
      CASE_I32_CMPOP(I32GeU)
      CASE_I64_BINOPI(I64Add, i64_add)
      CASE_I64_BINOP(I64Sub, i64_sub)
      CASE_I64_BINOP(I64Mul, i64_mul)
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
      CASE_I32_BINOPI(I32Shl, i32_shl)
      CASE_I32_BINOPI(I32ShrS, i32_sar)
      CASE_I32_BINOPI(I32ShrU, i32_shr)
      CASE_CCALL_BINOP(I32Rol, I32, wasm_word32_rol)
      CASE_CCALL_BINOP(I32Ror, I32, wasm_word32_ror)
      CASE_I64_SHIFTOP(I64Shl, i64_shl)
      CASE_I64_SHIFTOP(I64ShrS, i64_sar)
      CASE_I64_SHIFTOP(I64ShrU, i64_shr)
      CASE_CCALL_BINOP(I64Rol, I64, wasm_word64_rol)
      CASE_CCALL_BINOP(I64Ror, I64, wasm_word64_ror)
      CASE_FLOAT_BINOP(F32Add, F32, f32_add)
      CASE_FLOAT_BINOP(F32Sub, F32, f32_sub)
      CASE_FLOAT_BINOP(F32Mul, F32, f32_mul)
      CASE_FLOAT_BINOP(F32Div, F32, f32_div)
      CASE_FLOAT_BINOP(F32Min, F32, f32_min)
      CASE_FLOAT_BINOP(F32Max, F32, f32_max)
      CASE_FLOAT_BINOP(F32CopySign, F32, f32_copysign)
      CASE_FLOAT_BINOP(F64Add, F64, f64_add)
      CASE_FLOAT_BINOP(F64Sub, F64, f64_sub)
      CASE_FLOAT_BINOP(F64Mul, F64, f64_mul)
      CASE_FLOAT_BINOP(F64Div, F64, f64_div)
      CASE_FLOAT_BINOP(F64Min, F64, f64_min)
      CASE_FLOAT_BINOP(F64Max, F64, f64_max)
      CASE_FLOAT_BINOP(F64CopySign, F64, f64_copysign)
      case kExprI32DivS:
        EmitBinOp<kWasmI32, kWasmI32>([this, decoder](LiftoffRegister dst,
                                                      LiftoffRegister lhs,
                                                      LiftoffRegister rhs) {
          WasmCodePosition position = decoder->position();
          AddOutOfLineTrap(position, WasmCode::kThrowWasmTrapDivByZero);
          // Adding the second trap might invalidate the pointer returned for
          // the first one, thus get both pointers afterwards.
          AddOutOfLineTrap(position,
                           WasmCode::kThrowWasmTrapDivUnrepresentable);
          Label* div_by_zero = out_of_line_code_.end()[-2].label.get();
          Label* div_unrepresentable = out_of_line_code_.end()[-1].label.get();
          __ emit_i32_divs(dst.gp(), lhs.gp(), rhs.gp(), div_by_zero,
                           div_unrepresentable);
        });
        break;
      case kExprI32DivU:
        EmitBinOp<kWasmI32, kWasmI32>([this, decoder](LiftoffRegister dst,
                                                      LiftoffRegister lhs,
                                                      LiftoffRegister rhs) {
          Label* div_by_zero = AddOutOfLineTrap(
              decoder->position(), WasmCode::kThrowWasmTrapDivByZero);
          __ emit_i32_divu(dst.gp(), lhs.gp(), rhs.gp(), div_by_zero);
        });
        break;
      case kExprI32RemS:
        EmitBinOp<kWasmI32, kWasmI32>([this, decoder](LiftoffRegister dst,
                                                      LiftoffRegister lhs,
                                                      LiftoffRegister rhs) {
          Label* rem_by_zero = AddOutOfLineTrap(
              decoder->position(), WasmCode::kThrowWasmTrapRemByZero);
          __ emit_i32_rems(dst.gp(), lhs.gp(), rhs.gp(), rem_by_zero);
        });
        break;
      case kExprI32RemU:
        EmitBinOp<kWasmI32, kWasmI32>([this, decoder](LiftoffRegister dst,
                                                      LiftoffRegister lhs,
                                                      LiftoffRegister rhs) {
          Label* rem_by_zero = AddOutOfLineTrap(
              decoder->position(), WasmCode::kThrowWasmTrapRemByZero);
          __ emit_i32_remu(dst.gp(), lhs.gp(), rhs.gp(), rem_by_zero);
        });
        break;
      case kExprI64DivS:
        EmitBinOp<kWasmI64, kWasmI64>([this, decoder](LiftoffRegister dst,
                                                      LiftoffRegister lhs,
                                                      LiftoffRegister rhs) {
          WasmCodePosition position = decoder->position();
          AddOutOfLineTrap(position, WasmCode::kThrowWasmTrapDivByZero);
          // Adding the second trap might invalidate the pointer returned for
          // the first one, thus get both pointers afterwards.
          AddOutOfLineTrap(position,
                           WasmCode::kThrowWasmTrapDivUnrepresentable);
          Label* div_by_zero = out_of_line_code_.end()[-2].label.get();
          Label* div_unrepresentable = out_of_line_code_.end()[-1].label.get();
          if (!__ emit_i64_divs(dst, lhs, rhs, div_by_zero,
                                div_unrepresentable)) {
            ExternalReference ext_ref = ExternalReference::wasm_int64_div();
            EmitDivOrRem64CCall(dst, lhs, rhs, ext_ref, div_by_zero,
                                div_unrepresentable);
          }
        });
        break;
      case kExprI64DivU:
        EmitBinOp<kWasmI64, kWasmI64>([this, decoder](LiftoffRegister dst,
                                                      LiftoffRegister lhs,
                                                      LiftoffRegister rhs) {
          Label* div_by_zero = AddOutOfLineTrap(
              decoder->position(), WasmCode::kThrowWasmTrapDivByZero);
          if (!__ emit_i64_divu(dst, lhs, rhs, div_by_zero)) {
            ExternalReference ext_ref = ExternalReference::wasm_uint64_div();
            EmitDivOrRem64CCall(dst, lhs, rhs, ext_ref, div_by_zero);
          }
        });
        break;
      case kExprI64RemS:
        EmitBinOp<kWasmI64, kWasmI64>([this, decoder](LiftoffRegister dst,
                                                      LiftoffRegister lhs,
                                                      LiftoffRegister rhs) {
          Label* rem_by_zero = AddOutOfLineTrap(
              decoder->position(), WasmCode::kThrowWasmTrapRemByZero);
          if (!__ emit_i64_rems(dst, lhs, rhs, rem_by_zero)) {
            ExternalReference ext_ref = ExternalReference::wasm_int64_mod();
            EmitDivOrRem64CCall(dst, lhs, rhs, ext_ref, rem_by_zero);
          }
        });
        break;
      case kExprI64RemU:
        EmitBinOp<kWasmI64, kWasmI64>([this, decoder](LiftoffRegister dst,
                                                      LiftoffRegister lhs,
                                                      LiftoffRegister rhs) {
          Label* rem_by_zero = AddOutOfLineTrap(
              decoder->position(), WasmCode::kThrowWasmTrapRemByZero);
          if (!__ emit_i64_remu(dst, lhs, rhs, rem_by_zero)) {
            ExternalReference ext_ref = ExternalReference::wasm_uint64_mod();
            EmitDivOrRem64CCall(dst, lhs, rhs, ext_ref, rem_by_zero);
          }
        });
        break;
      default:
        UNREACHABLE();
    }
#undef CASE_I32_BINOP
#undef CASE_I32_BINOPI
#undef CASE_I64_BINOP
#undef CASE_I64_BINOPI
#undef CASE_FLOAT_BINOP
#undef CASE_I32_CMPOP
#undef CASE_I64_CMPOP
#undef CASE_F32_CMPOP
#undef CASE_F64_CMPOP
#undef CASE_I64_SHIFTOP
#undef CASE_CCALL_BINOP
  }

  void I32Const(FullDecoder* decoder, Value* result, int32_t value) {
    __ PushConstant(kWasmI32, value);
  }

  void I64Const(FullDecoder* decoder, Value* result, int64_t value) {
    // The {VarState} stores constant values as int32_t, thus we only store
    // 64-bit constants in this field if it fits in an int32_t. Larger values
    // cannot be used as immediate value anyway, so we can also just put them in
    // a register immediately.
    int32_t value_i32 = static_cast<int32_t>(value);
    if (value_i32 == value) {
      __ PushConstant(kWasmI64, value_i32);
    } else {
      LiftoffRegister reg = __ GetUnusedRegister(reg_class_for(kWasmI64));
      __ LoadConstant(reg, WasmValue(value));
      __ PushRegister(kWasmI64, reg);
    }
  }

  void F32Const(FullDecoder* decoder, Value* result, float value) {
    LiftoffRegister reg = __ GetUnusedRegister(kFpReg);
    __ LoadConstant(reg, WasmValue(value));
    __ PushRegister(kWasmF32, reg);
  }

  void F64Const(FullDecoder* decoder, Value* result, double value) {
    LiftoffRegister reg = __ GetUnusedRegister(kFpReg);
    __ LoadConstant(reg, WasmValue(value));
    __ PushRegister(kWasmF64, reg);
  }

  void RefNull(FullDecoder* decoder, Value* result) {
    unsupported(decoder, kAnyRef, "ref_null");
  }

  void RefFunc(FullDecoder* decoder, uint32_t function_index, Value* result) {
    unsupported(decoder, kAnyRef, "func");
  }

  void Drop(FullDecoder* decoder, const Value& value) {
    auto& slot = __ cache_state()->stack_state.back();
    // If the dropped slot contains a register, decrement it's use count.
    if (slot.is_reg()) __ cache_state()->dec_used(slot.reg());
    __ cache_state()->stack_state.pop_back();
  }

  void ReturnImpl(FullDecoder* decoder) {
    size_t num_returns = decoder->sig_->return_count();
    if (num_returns > 1) {
      return unsupported(decoder, kMultiValue, "multi-return");
    }
    if (num_returns > 0) __ MoveToReturnRegisters(decoder->sig_);
    __ LeaveFrame(StackFrame::WASM_COMPILED);
    __ DropStackSlotsAndRet(
        static_cast<uint32_t>(descriptor_->StackParameterCount()));
  }

  void DoReturn(FullDecoder* decoder, Vector<Value> /*values*/) {
    ReturnImpl(decoder);
  }

  void LocalGet(FullDecoder* decoder, Value* result,
                const LocalIndexImmediate<validate>& imm) {
    auto& slot = __ cache_state()->stack_state[imm.index];
    DCHECK_EQ(slot.type(), imm.type);
    switch (slot.loc()) {
      case kRegister:
        __ PushRegister(slot.type(), slot.reg());
        break;
      case kIntConst:
        __ PushConstant(imm.type, slot.i32_const());
        break;
      case kStack: {
        auto rc = reg_class_for(imm.type);
        LiftoffRegister reg = __ GetUnusedRegister(rc);
        __ Fill(reg, slot.offset(), imm.type);
        __ PushRegister(slot.type(), reg);
        break;
      }
    }
  }

  void LocalSetFromStackSlot(LiftoffAssembler::VarState* dst_slot,
                             uint32_t local_index) {
    auto& state = *__ cache_state();
    auto& src_slot = state.stack_state.back();
    ValueType type = dst_slot->type();
    if (dst_slot->is_reg()) {
      LiftoffRegister slot_reg = dst_slot->reg();
      if (state.get_use_count(slot_reg) == 1) {
        __ Fill(dst_slot->reg(), src_slot.offset(), type);
        return;
      }
      state.dec_used(slot_reg);
      dst_slot->MakeStack();
    }
    DCHECK_EQ(type, __ local_type(local_index));
    RegClass rc = reg_class_for(type);
    LiftoffRegister dst_reg = __ GetUnusedRegister(rc);
    __ Fill(dst_reg, src_slot.offset(), type);
    *dst_slot = LiftoffAssembler::VarState(type, dst_reg, dst_slot->offset());
    __ cache_state()->inc_used(dst_reg);
  }

  void LocalSet(uint32_t local_index, bool is_tee) {
    auto& state = *__ cache_state();
    auto& source_slot = state.stack_state.back();
    auto& target_slot = state.stack_state[local_index];
    switch (source_slot.loc()) {
      case kRegister:
        if (target_slot.is_reg()) state.dec_used(target_slot.reg());
        target_slot.Copy(source_slot);
        if (is_tee) state.inc_used(target_slot.reg());
        break;
      case kIntConst:
        if (target_slot.is_reg()) state.dec_used(target_slot.reg());
        target_slot.Copy(source_slot);
        break;
      case kStack:
        LocalSetFromStackSlot(&target_slot, local_index);
        break;
    }
    if (!is_tee) __ cache_state()->stack_state.pop_back();
  }

  void LocalSet(FullDecoder* decoder, const Value& value,
                const LocalIndexImmediate<validate>& imm) {
    LocalSet(imm.index, false);
  }

  void LocalTee(FullDecoder* decoder, const Value& value, Value* result,
                const LocalIndexImmediate<validate>& imm) {
    LocalSet(imm.index, true);
  }

  Register GetGlobalBaseAndOffset(const WasmGlobal* global,
                                  LiftoffRegList* pinned, uint32_t* offset) {
    Register addr = pinned->set(__ GetUnusedRegister(kGpReg)).gp();
    if (global->mutability && global->imported) {
      LOAD_INSTANCE_FIELD(addr, ImportedMutableGlobals, kSystemPointerSize);
      __ Load(LiftoffRegister(addr), addr, no_reg,
              global->index * sizeof(Address), kPointerLoadType, *pinned);
      *offset = 0;
    } else {
      LOAD_INSTANCE_FIELD(addr, GlobalsStart, kSystemPointerSize);
      *offset = global->offset;
    }
    return addr;
  }

  void GlobalGet(FullDecoder* decoder, Value* result,
                 const GlobalIndexImmediate<validate>& imm) {
    const auto* global = &env_->module->globals[imm.index];
    if (!CheckSupportedType(decoder, kSupportedTypes, global->type, "global"))
      return;
    LiftoffRegList pinned;
    uint32_t offset = 0;
    Register addr = GetGlobalBaseAndOffset(global, &pinned, &offset);
    LiftoffRegister value =
        pinned.set(__ GetUnusedRegister(reg_class_for(global->type), pinned));
    LoadType type = LoadType::ForValueType(global->type);
    __ Load(value, addr, no_reg, offset, type, pinned, nullptr, true);
    __ PushRegister(global->type, value);
  }

  void GlobalSet(FullDecoder* decoder, const Value& value,
                 const GlobalIndexImmediate<validate>& imm) {
    auto* global = &env_->module->globals[imm.index];
    if (!CheckSupportedType(decoder, kSupportedTypes, global->type, "global"))
      return;
    LiftoffRegList pinned;
    uint32_t offset = 0;
    Register addr = GetGlobalBaseAndOffset(global, &pinned, &offset);
    LiftoffRegister reg = pinned.set(__ PopToRegister(pinned));
    StoreType type = StoreType::ForValueType(global->type);
    __ Store(addr, no_reg, offset, reg, type, {}, nullptr, true);
  }

  void TableGet(FullDecoder* decoder, const Value& index, Value* result,
                const TableIndexImmediate<validate>& imm) {
    unsupported(decoder, kAnyRef, "table_get");
  }

  void TableSet(FullDecoder* decoder, const Value& index, const Value& value,
                const TableIndexImmediate<validate>& imm) {
    unsupported(decoder, kAnyRef, "table_set");
  }

  void Unreachable(FullDecoder* decoder) {
    Label* unreachable_label = AddOutOfLineTrap(
        decoder->position(), WasmCode::kThrowWasmTrapUnreachable);
    __ emit_jump(unreachable_label);
    __ AssertUnreachable(AbortReason::kUnexpectedReturnFromWasmTrap);
  }

  void Select(FullDecoder* decoder, const Value& cond, const Value& fval,
              const Value& tval, Value* result) {
    LiftoffRegList pinned;
    Register condition = pinned.set(__ PopToRegister()).gp();
    ValueType type = __ cache_state()->stack_state.end()[-1].type();
    DCHECK_EQ(type, __ cache_state()->stack_state.end()[-2].type());
    LiftoffRegister false_value = pinned.set(__ PopToRegister(pinned));
    LiftoffRegister true_value = __ PopToRegister(pinned);
    LiftoffRegister dst =
        __ GetUnusedRegister(true_value.reg_class(), {true_value, false_value});
    __ PushRegister(type, dst);

    // Now emit the actual code to move either {true_value} or {false_value}
    // into {dst}.
    Label cont;
    Label case_false;
    __ emit_cond_jump(kEqual, &case_false, kWasmI32, condition);
    if (dst != true_value) __ Move(dst, true_value, type);
    __ emit_jump(&cont);

    __ bind(&case_false);
    if (dst != false_value) __ Move(dst, false_value, type);
    __ bind(&cont);
  }

  void BrImpl(Control* target) {
    if (!target->br_merge()->reached) {
      target->label_state.InitMerge(*__ cache_state(), __ num_locals(),
                                    target->br_merge()->arity,
                                    target->stack_depth);
    }
    __ MergeStackWith(target->label_state, target->br_merge()->arity);
    __ jmp(target->label.get());
  }

  void Br(FullDecoder* decoder, Control* target) { BrImpl(target); }

  void BrOrRet(FullDecoder* decoder, uint32_t depth) {
    if (depth == decoder->control_depth() - 1) {
      ReturnImpl(decoder);
    } else {
      BrImpl(decoder->control_at(depth));
    }
  }

  void BrIf(FullDecoder* decoder, const Value& /* cond */, uint32_t depth) {
    Label cont_false;
    Register value = __ PopToRegister().gp();

    if (!has_outstanding_op()) {
      // Unary "equal" means "equals zero".
      __ emit_cond_jump(kEqual, &cont_false, kWasmI32, value);
    } else if (outstanding_op_ == kExprI32Eqz) {
      // Unary "unequal" means "not equals zero".
      __ emit_cond_jump(kUnequal, &cont_false, kWasmI32, value);
      outstanding_op_ = kNoOutstandingOp;
    } else {
      // Otherwise, it's an i32 compare opcode.
      Condition cond = NegateCondition(GetCompareCondition(outstanding_op_));
      Register rhs = value;
      Register lhs = __ PopToRegister(LiftoffRegList::ForRegs(rhs)).gp();
      __ emit_cond_jump(cond, &cont_false, kWasmI32, lhs, rhs);
      outstanding_op_ = kNoOutstandingOp;
    }

    BrOrRet(decoder, depth);
    __ bind(&cont_false);
  }

  // Generate a branch table case, potentially reusing previously generated
  // stack transfer code.
  void GenerateBrCase(FullDecoder* decoder, uint32_t br_depth,
                      std::map<uint32_t, MovableLabel>* br_targets) {
    MovableLabel& label = (*br_targets)[br_depth];
    if (label.get()->is_bound()) {
      __ jmp(label.get());
    } else {
      __ bind(label.get());
      BrOrRet(decoder, br_depth);
    }
  }

  // Generate a branch table for input in [min, max).
  // TODO(wasm): Generate a real branch table (like TF TableSwitch).
  void GenerateBrTable(FullDecoder* decoder, LiftoffRegister tmp,
                       LiftoffRegister value, uint32_t min, uint32_t max,
                       BranchTableIterator<validate>* table_iterator,
                       std::map<uint32_t, MovableLabel>* br_targets) {
    DCHECK_LT(min, max);
    // Check base case.
    if (max == min + 1) {
      DCHECK_EQ(min, table_iterator->cur_index());
      GenerateBrCase(decoder, table_iterator->next(), br_targets);
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
    // table_iterator will trigger a DCHECK if we don't stop decoding now.
    if (did_bailout()) return;
    // Emit br table for upper half:
    GenerateBrTable(decoder, tmp, value, split, max, table_iterator,
                    br_targets);
  }

  void BrTable(FullDecoder* decoder, const BranchTableImmediate<validate>& imm,
               const Value& key) {
    LiftoffRegList pinned;
    LiftoffRegister value = pinned.set(__ PopToRegister());
    BranchTableIterator<validate> table_iterator(decoder, imm);
    std::map<uint32_t, MovableLabel> br_targets;

    if (imm.table_count > 0) {
      LiftoffRegister tmp = __ GetUnusedRegister(kGpReg, pinned);
      __ LoadConstant(tmp, WasmValue(uint32_t{imm.table_count}));
      Label case_default;
      __ emit_cond_jump(kUnsignedGreaterEqual, &case_default, kWasmI32,
                        value.gp(), tmp.gp());

      GenerateBrTable(decoder, tmp, value, 0, imm.table_count, &table_iterator,
                      &br_targets);

      __ bind(&case_default);
      // table_iterator will trigger a DCHECK if we don't stop decoding now.
      if (did_bailout()) return;
    }

    // Generate the default case.
    GenerateBrCase(decoder, table_iterator.next(), &br_targets);
    DCHECK(!table_iterator.has_next());
  }

  void Else(FullDecoder* decoder, Control* c) {
    if (c->reachable()) {
      if (!c->end_merge.reached) {
        c->label_state.InitMerge(*__ cache_state(), __ num_locals(),
                                 c->end_merge.arity, c->stack_depth);
      }
      __ MergeFullStackWith(c->label_state, *__ cache_state());
      __ emit_jump(c->label.get());
    }
    __ bind(c->else_state->label.get());
    __ cache_state()->Steal(c->else_state->state);
  }

  Label* AddOutOfLineTrap(WasmCodePosition position,
                          WasmCode::RuntimeStubId stub, uint32_t pc = 0) {
    DCHECK(FLAG_wasm_bounds_checks);
    // The pc is needed for memory OOB trap with trap handler enabled. Other
    // callers should not even compute it.
    DCHECK_EQ(pc != 0, stub == WasmCode::kThrowWasmTrapMemOutOfBounds &&
                           env_->use_trap_handler);

    out_of_line_code_.push_back(
        OutOfLineCode::Trap(stub, position, pc, RegisterDebugSideTableEntry()));
    return out_of_line_code_.back().label.get();
  }

  enum ForceCheck : bool { kDoForceCheck = true, kDontForceCheck = false };

  // Returns true if the memory access is statically known to be out of bounds
  // (a jump to the trap was generated then); return false otherwise.
  bool BoundsCheckMem(FullDecoder* decoder, uint32_t access_size,
                      uint32_t offset, Register index, LiftoffRegList pinned,
                      ForceCheck force_check) {
    const bool statically_oob =
        !base::IsInBounds(offset, access_size, env_->max_memory_size);

    if (!force_check && !statically_oob &&
        (!FLAG_wasm_bounds_checks || env_->use_trap_handler)) {
      return false;
    }

    // TODO(wasm): This adds protected instruction information for the jump
    // instruction we are about to generate. It would be better to just not add
    // protected instruction info when the pc is 0.
    Label* trap_label = AddOutOfLineTrap(
        decoder->position(), WasmCode::kThrowWasmTrapMemOutOfBounds,
        env_->use_trap_handler ? __ pc_offset() : 0);

    if (statically_oob) {
      __ emit_jump(trap_label);
      Control* current_block = decoder->control_at(0);
      if (current_block->reachable()) {
        current_block->reachability = kSpecOnlyReachable;
      }
      return true;
    }

    uint64_t end_offset = uint64_t{offset} + access_size - 1u;

    // If the end offset is larger than the smallest memory, dynamically check
    // the end offset against the actual memory size, which is not known at
    // compile time. Otherwise, only one check is required (see below).
    LiftoffRegister end_offset_reg =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    Register mem_size = __ GetUnusedRegister(kGpReg, pinned).gp();
    LOAD_INSTANCE_FIELD(mem_size, MemorySize, kSystemPointerSize);

    if (kSystemPointerSize == 8) {
      __ LoadConstant(end_offset_reg, WasmValue(end_offset));
    } else {
      __ LoadConstant(end_offset_reg,
                      WasmValue(static_cast<uint32_t>(end_offset)));
    }

    if (end_offset >= env_->min_memory_size) {
      __ emit_cond_jump(kUnsignedGreaterEqual, trap_label,
                        LiftoffAssembler::kWasmIntPtr, end_offset_reg.gp(),
                        mem_size);
    }

    // Just reuse the end_offset register for computing the effective size.
    LiftoffRegister effective_size_reg = end_offset_reg;
    __ emit_ptrsize_sub(effective_size_reg.gp(), mem_size, end_offset_reg.gp());

    __ emit_u32_to_intptr(index, index);

    __ emit_cond_jump(kUnsignedGreaterEqual, trap_label,
                      LiftoffAssembler::kWasmIntPtr, index,
                      effective_size_reg.gp());
    return false;
  }

  void AlignmentCheckMem(FullDecoder* decoder, uint32_t access_size,
                         uint32_t offset, Register index,
                         LiftoffRegList pinned) {
    Label* trap_label = AddOutOfLineTrap(
        decoder->position(), WasmCode::kThrowWasmTrapUnalignedAccess, 0);
    Register address = __ GetUnusedRegister(kGpReg, pinned).gp();

    const uint32_t align_mask = access_size - 1;
    if ((offset & align_mask) == 0) {
      // If {offset} is aligned, we can produce faster code.

      // TODO(ahaas): On Intel, the "test" instruction implicitly computes the
      // AND of two operands. We could introduce a new variant of
      // {emit_cond_jump} to use the "test" instruction without the "and" here.
      // Then we can also avoid using the temp register here.
      __ emit_i32_and(address, index, align_mask);
      __ emit_cond_jump(kUnequal, trap_label, kWasmI32, address);
      return;
    }
    __ emit_i32_add(address, index, offset);
    __ emit_i32_and(address, address, align_mask);

    __ emit_cond_jump(kUnequal, trap_label, kWasmI32, address);
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

    // Get a register to hold the stack slot for MemoryTracingInfo.
    LiftoffRegister info = pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    // Allocate stack slot for MemoryTracingInfo.
    __ AllocateStackSlot(info.gp(), sizeof(MemoryTracingInfo));

    // Now store all information into the MemoryTracingInfo struct.
    __ Store(info.gp(), no_reg, offsetof(MemoryTracingInfo, address), address,
             StoreType::kI32Store, pinned);
    __ LoadConstant(address, WasmValue(is_store ? 1 : 0));
    __ Store(info.gp(), no_reg, offsetof(MemoryTracingInfo, is_store), address,
             StoreType::kI32Store8, pinned);
    __ LoadConstant(address, WasmValue(static_cast<int>(rep)));
    __ Store(info.gp(), no_reg, offsetof(MemoryTracingInfo, mem_rep), address,
             StoreType::kI32Store8, pinned);

    WasmTraceMemoryDescriptor descriptor;
    DCHECK_EQ(0, descriptor.GetStackParameterCount());
    DCHECK_EQ(1, descriptor.GetRegisterParameterCount());
    Register param_reg = descriptor.GetRegisterParameter(0);
    if (info.gp() != param_reg) {
      __ Move(param_reg, info.gp(), LiftoffAssembler::kWasmIntPtr);
    }

    source_position_table_builder_.AddPosition(__ pc_offset(),
                                               SourcePosition(position), false);
    __ CallRuntimeStub(WasmCode::kWasmTraceMemory);
    safepoint_table_builder_.DefineSafepoint(&asm_, Safepoint::kNoLazyDeopt);

    __ DeallocateStackSlot(sizeof(MemoryTracingInfo));
  }

  Register AddMemoryMasking(Register index, uint32_t* offset,
                            LiftoffRegList* pinned) {
    if (!FLAG_untrusted_code_mitigations || env_->use_trap_handler) {
      return index;
    }
    DEBUG_CODE_COMMENT("Mask memory index");
    // Make sure that we can overwrite {index}.
    if (__ cache_state()->is_used(LiftoffRegister(index))) {
      Register old_index = index;
      pinned->clear(LiftoffRegister(old_index));
      index = pinned->set(__ GetUnusedRegister(kGpReg, *pinned)).gp();
      if (index != old_index) __ Move(index, old_index, kWasmI32);
    }
    Register tmp = __ GetUnusedRegister(kGpReg, *pinned).gp();
    __ emit_ptrsize_add(index, index, *offset);
    LOAD_INSTANCE_FIELD(tmp, MemoryMask, kSystemPointerSize);
    __ emit_ptrsize_and(index, index, tmp);
    *offset = 0;
    return index;
  }

  void LoadMem(FullDecoder* decoder, LoadType type,
               const MemoryAccessImmediate<validate>& imm,
               const Value& index_val, Value* result) {
    ValueType value_type = type.value_type();
    if (!CheckSupportedType(decoder, kSupportedTypes, value_type, "load"))
      return;
    LiftoffRegList pinned;
    Register index = pinned.set(__ PopToRegister()).gp();
    if (BoundsCheckMem(decoder, type.size(), imm.offset, index, pinned,
                       kDontForceCheck)) {
      return;
    }
    uint32_t offset = imm.offset;
    index = AddMemoryMasking(index, &offset, &pinned);
    DEBUG_CODE_COMMENT("Load from memory");
    Register addr = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    LOAD_INSTANCE_FIELD(addr, MemoryStart, kSystemPointerSize);
    RegClass rc = reg_class_for(value_type);
    LiftoffRegister value = pinned.set(__ GetUnusedRegister(rc, pinned));
    uint32_t protected_load_pc = 0;
    __ Load(value, addr, index, offset, type, pinned, &protected_load_pc, true);
    if (env_->use_trap_handler) {
      AddOutOfLineTrap(decoder->position(),
                       WasmCode::kThrowWasmTrapMemOutOfBounds,
                       protected_load_pc);
    }
    __ PushRegister(value_type, value);

    if (FLAG_trace_wasm_memory) {
      TraceMemoryOperation(false, type.mem_type().representation(), index,
                           offset, decoder->position());
    }
  }
  void LoadTransform(FullDecoder* decoder, LoadType type,
                     LoadTransformationKind transform,
                     const MemoryAccessImmediate<validate>& imm,
                     const Value& index_val, Value* result) {
    unsupported(decoder, kSimd, "simd");
  }

  void StoreMem(FullDecoder* decoder, StoreType type,
                const MemoryAccessImmediate<validate>& imm,
                const Value& index_val, const Value& value_val) {
    ValueType value_type = type.value_type();
    if (!CheckSupportedType(decoder, kSupportedTypes, value_type, "store"))
      return;
    LiftoffRegList pinned;
    LiftoffRegister value = pinned.set(__ PopToRegister());
    Register index = pinned.set(__ PopToRegister(pinned)).gp();
    if (BoundsCheckMem(decoder, type.size(), imm.offset, index, pinned,
                       kDontForceCheck)) {
      return;
    }
    uint32_t offset = imm.offset;
    index = AddMemoryMasking(index, &offset, &pinned);
    DEBUG_CODE_COMMENT("Store to memory");
    Register addr = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    LOAD_INSTANCE_FIELD(addr, MemoryStart, kSystemPointerSize);
    uint32_t protected_store_pc = 0;
    LiftoffRegList outer_pinned;
    if (FLAG_trace_wasm_memory) outer_pinned.set(index);
    __ Store(addr, index, offset, value, type, outer_pinned,
             &protected_store_pc, true);
    if (env_->use_trap_handler) {
      AddOutOfLineTrap(decoder->position(),
                       WasmCode::kThrowWasmTrapMemOutOfBounds,
                       protected_store_pc);
    }
    if (FLAG_trace_wasm_memory) {
      TraceMemoryOperation(true, type.mem_rep(), index, offset,
                           decoder->position());
    }
  }

  void CurrentMemoryPages(FullDecoder* decoder, Value* result) {
    Register mem_size = __ GetUnusedRegister(kGpReg).gp();
    LOAD_INSTANCE_FIELD(mem_size, MemorySize, kSystemPointerSize);
    __ emit_ptrsize_shr(mem_size, mem_size, kWasmPageSizeLog2);
    __ PushRegister(kWasmI32, LiftoffRegister(mem_size));
  }

  void MemoryGrow(FullDecoder* decoder, const Value& value, Value* result_val) {
    // Pop the input, then spill all cache registers to make the runtime call.
    LiftoffRegList pinned;
    LiftoffRegister input = pinned.set(__ PopToRegister());
    __ SpillAllRegisters();

    constexpr Register kGpReturnReg = kGpReturnRegisters[0];
    static_assert(kLiftoffAssemblerGpCacheRegs & kGpReturnReg.bit(),
                  "first return register is a cache register (needs more "
                  "complex code here otherwise)");
    LiftoffRegister result = pinned.set(LiftoffRegister(kGpReturnReg));

    WasmMemoryGrowDescriptor descriptor;
    DCHECK_EQ(0, descriptor.GetStackParameterCount());
    DCHECK_EQ(1, descriptor.GetRegisterParameterCount());
    DCHECK_EQ(ValueTypes::MachineTypeFor(kWasmI32),
              descriptor.GetParameterType(0));

    Register param_reg = descriptor.GetRegisterParameter(0);
    if (input.gp() != param_reg) __ Move(param_reg, input.gp(), kWasmI32);

    __ CallRuntimeStub(WasmCode::kWasmMemoryGrow);
    RegisterDebugSideTableEntry();
    safepoint_table_builder_.DefineSafepoint(&asm_, Safepoint::kNoLazyDeopt);

    if (kReturnRegister0 != result.gp()) {
      __ Move(result.gp(), kReturnRegister0, kWasmI32);
    }

    __ PushRegister(kWasmI32, result);
  }

  DebugSideTableBuilder::EntryBuilder* RegisterDebugSideTableEntry() {
    if (V8_LIKELY(!debug_sidetable_builder_)) return nullptr;
    int stack_height = static_cast<int>(__ cache_state()->stack_height());
    return debug_sidetable_builder_->NewEntry(
        __ pc_offset(), __ num_locals(), stack_height,
        __ cache_state()->stack_state.begin());
  }

  void CallDirect(FullDecoder* decoder,
                  const CallFunctionImmediate<validate>& imm,
                  const Value args[], Value returns[]) {
    if (imm.sig->return_count() > 1) {
      return unsupported(decoder, kMultiValue, "multi-return");
    }
    if (imm.sig->return_count() == 1 &&
        !CheckSupportedType(decoder, kSupportedTypes, imm.sig->GetReturn(0),
                            "return")) {
      return;
    }

    auto call_descriptor =
        compiler::GetWasmCallDescriptor(compilation_zone_, imm.sig);
    call_descriptor =
        GetLoweredCallDescriptor(compilation_zone_, call_descriptor);

    if (imm.index < env_->module->num_imported_functions) {
      // A direct call to an imported function.
      LiftoffRegList pinned;
      Register tmp = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
      Register target = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();

      Register imported_targets = tmp;
      LOAD_INSTANCE_FIELD(imported_targets, ImportedFunctionTargets,
                          kSystemPointerSize);
      __ Load(LiftoffRegister(target), imported_targets, no_reg,
              imm.index * sizeof(Address), kPointerLoadType, pinned);

      Register imported_function_refs = tmp;
      LOAD_TAGGED_PTR_INSTANCE_FIELD(imported_function_refs,
                                     ImportedFunctionRefs);
      Register imported_function_ref = tmp;
      __ LoadTaggedPointer(
          imported_function_ref, imported_function_refs, no_reg,
          ObjectAccess::ElementOffsetInTaggedFixedArray(imm.index), pinned);

      Register* explicit_instance = &imported_function_ref;
      __ PrepareCall(imm.sig, call_descriptor, &target, explicit_instance);
      source_position_table_builder_.AddPosition(
          __ pc_offset(), SourcePosition(decoder->position()), false);

      __ CallIndirect(imm.sig, call_descriptor, target);
    } else {
      // A direct call within this module just gets the current instance.
      __ PrepareCall(imm.sig, call_descriptor);

      source_position_table_builder_.AddPosition(
          __ pc_offset(), SourcePosition(decoder->position()), false);

      // Just encode the function index. This will be patched at instantiation.
      Address addr = static_cast<Address>(imm.index);
      __ CallNativeWasmCode(addr);
    }

    RegisterDebugSideTableEntry();
    safepoint_table_builder_.DefineSafepoint(&asm_, Safepoint::kNoLazyDeopt);

    __ FinishCall(imm.sig, call_descriptor);
  }

  void CallIndirect(FullDecoder* decoder, const Value& index_val,
                    const CallIndirectImmediate<validate>& imm,
                    const Value args[], Value returns[]) {
    if (imm.sig->return_count() > 1) {
      return unsupported(decoder, kMultiValue, "multi-return");
    }
    if (imm.table_index != 0) {
      return unsupported(decoder, kAnyRef, "table index != 0");
    }
    if (imm.sig->return_count() == 1 &&
        !CheckSupportedType(decoder, kSupportedTypes, imm.sig->GetReturn(0),
                            "return")) {
      return;
    }

    // Pop the index.
    Register index = __ PopToRegister().gp();
    // If that register is still being used after popping, we move it to another
    // register, because we want to modify that register.
    if (__ cache_state()->is_used(LiftoffRegister(index))) {
      Register new_index =
          __ GetUnusedRegister(kGpReg, LiftoffRegList::ForRegs(index)).gp();
      __ Move(new_index, index, kWasmI32);
      index = new_index;
    }

    LiftoffRegList pinned = LiftoffRegList::ForRegs(index);
    // Get three temporary registers.
    Register table = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    Register tmp_const = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    Register scratch = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();

    // Bounds check against the table size.
    Label* invalid_func_label = AddOutOfLineTrap(
        decoder->position(), WasmCode::kThrowWasmTrapFuncInvalid);

    uint32_t canonical_sig_num = env_->module->signature_ids[imm.sig_index];
    DCHECK_GE(canonical_sig_num, 0);
    DCHECK_GE(kMaxInt, canonical_sig_num);

    // Compare against table size stored in
    // {instance->indirect_function_table_size}.
    LOAD_INSTANCE_FIELD(tmp_const, IndirectFunctionTableSize, kUInt32Size);
    __ emit_cond_jump(kUnsignedGreaterEqual, invalid_func_label, kWasmI32,
                      index, tmp_const);

    // Mask the index to prevent SSCA.
    if (FLAG_untrusted_code_mitigations) {
      DEBUG_CODE_COMMENT("Mask indirect call index");
      // mask = ((index - size) & ~index) >> 31
      // Reuse allocated registers; note: size is still stored in {tmp_const}.
      Register diff = table;
      Register neg_index = tmp_const;
      Register mask = scratch;
      // 1) diff = index - size
      __ emit_i32_sub(diff, index, tmp_const);
      // 2) neg_index = ~index
      __ LoadConstant(LiftoffRegister(neg_index), WasmValue(int32_t{-1}));
      __ emit_i32_xor(neg_index, neg_index, index);
      // 3) mask = diff & neg_index
      __ emit_i32_and(mask, diff, neg_index);
      // 4) mask = mask >> 31
      __ emit_i32_sar(mask, mask, 31);

      // Apply mask.
      __ emit_i32_and(index, index, mask);
    }

    DEBUG_CODE_COMMENT("Check indirect call signature");
    // Load the signature from {instance->ift_sig_ids[key]}
    LOAD_INSTANCE_FIELD(table, IndirectFunctionTableSigIds, kSystemPointerSize);
    // Shift {index} by 2 (multiply by 4) to represent kInt32Size items.
    STATIC_ASSERT((1 << 2) == kInt32Size);
    __ emit_i32_shl(index, index, 2);
    __ Load(LiftoffRegister(scratch), table, index, 0, LoadType::kI32Load,
            pinned);

    // Compare against expected signature.
    __ LoadConstant(LiftoffRegister(tmp_const), WasmValue(canonical_sig_num));

    Label* sig_mismatch_label = AddOutOfLineTrap(
        decoder->position(), WasmCode::kThrowWasmTrapFuncSigMismatch);
    __ emit_cond_jump(kUnequal, sig_mismatch_label,
                      LiftoffAssembler::kWasmIntPtr, scratch, tmp_const);

    // At this point {index} has already been multiplied by 4.
    DEBUG_CODE_COMMENT("Execute indirect call");
    if (kTaggedSize != kInt32Size) {
      DCHECK_EQ(kTaggedSize, kInt32Size * 2);
      // Multiply {index} by another 2 to represent kTaggedSize items.
      __ emit_i32_add(index, index, index);
    }
    // At this point {index} has already been multiplied by kTaggedSize.

    // Load the instance from {instance->ift_instances[key]}
    LOAD_TAGGED_PTR_INSTANCE_FIELD(table, IndirectFunctionTableRefs);
    __ LoadTaggedPointer(tmp_const, table, index,
                         ObjectAccess::ElementOffsetInTaggedFixedArray(0),
                         pinned);

    if (kTaggedSize != kSystemPointerSize) {
      DCHECK_EQ(kSystemPointerSize, kTaggedSize * 2);
      // Multiply {index} by another 2 to represent kSystemPointerSize items.
      __ emit_i32_add(index, index, index);
    }
    // At this point {index} has already been multiplied by kSystemPointerSize.

    Register* explicit_instance = &tmp_const;

    // Load the target from {instance->ift_targets[key]}
    LOAD_INSTANCE_FIELD(table, IndirectFunctionTableTargets,
                        kSystemPointerSize);
    __ Load(LiftoffRegister(scratch), table, index, 0, kPointerLoadType,
            pinned);

    source_position_table_builder_.AddPosition(
        __ pc_offset(), SourcePosition(decoder->position()), false);

    auto call_descriptor =
        compiler::GetWasmCallDescriptor(compilation_zone_, imm.sig);
    call_descriptor =
        GetLoweredCallDescriptor(compilation_zone_, call_descriptor);

    Register target = scratch;
    __ PrepareCall(imm.sig, call_descriptor, &target, explicit_instance);
    __ CallIndirect(imm.sig, call_descriptor, target);

    RegisterDebugSideTableEntry();
    safepoint_table_builder_.DefineSafepoint(&asm_, Safepoint::kNoLazyDeopt);

    __ FinishCall(imm.sig, call_descriptor);
  }

  void ReturnCall(FullDecoder* decoder,
                  const CallFunctionImmediate<validate>& imm,
                  const Value args[]) {
    unsupported(decoder, kTailCall, "return_call");
  }
  void ReturnCallIndirect(FullDecoder* decoder, const Value& index_val,
                          const CallIndirectImmediate<validate>& imm,
                          const Value args[]) {
    unsupported(decoder, kTailCall, "return_call_indirect");
  }

  void SimdOp(FullDecoder* decoder, WasmOpcode opcode, Vector<Value> args,
              Value* result) {
    if (!CpuFeatures::SupportsWasmSimd128()) {
      return unsupported(decoder, kSimd, "simd");
    }
    switch (opcode) {
      case wasm::kExprF32x4Splat:
        EmitUnOp<kWasmF32, kWasmS128>(
            [=](LiftoffRegister dst, LiftoffRegister src) {
              __ emit_f32x4_splat(dst, src);
            });
        break;
      default:
        unsupported(decoder, kSimd, "simd");
    }
  }

  void SimdLaneOp(FullDecoder* decoder, WasmOpcode opcode,
                  const SimdLaneImmediate<validate>& imm,
                  const Vector<Value> inputs, Value* result) {
    unsupported(decoder, kSimd, "simd");
  }
  void Simd8x16ShuffleOp(FullDecoder* decoder,
                         const Simd8x16ShuffleImmediate<validate>& imm,
                         const Value& input0, const Value& input1,
                         Value* result) {
    unsupported(decoder, kSimd, "simd");
  }
  void Throw(FullDecoder* decoder, const ExceptionIndexImmediate<validate>&,
             const Vector<Value>& args) {
    unsupported(decoder, kExceptionHandling, "throw");
  }
  void Rethrow(FullDecoder* decoder, const Value& exception) {
    unsupported(decoder, kExceptionHandling, "rethrow");
  }
  void BrOnException(FullDecoder* decoder, const Value& exception,
                     const ExceptionIndexImmediate<validate>& imm,
                     uint32_t depth, Vector<Value> values) {
    unsupported(decoder, kExceptionHandling, "br_on_exn");
  }

  void AtomicStoreMem(FullDecoder* decoder, StoreType type,
                      const MemoryAccessImmediate<validate>& imm) {
    LiftoffRegList pinned;
    LiftoffRegister value = pinned.set(__ PopToRegister());
    Register index = pinned.set(__ PopToRegister(pinned)).gp();
    if (BoundsCheckMem(decoder, type.size(), imm.offset, index, pinned,
                       kDoForceCheck)) {
      return;
    }
    AlignmentCheckMem(decoder, type.size(), imm.offset, index, pinned);
    uint32_t offset = imm.offset;
    index = AddMemoryMasking(index, &offset, &pinned);
    DEBUG_CODE_COMMENT("Atomic store to memory");
    Register addr = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    LOAD_INSTANCE_FIELD(addr, MemoryStart, kSystemPointerSize);
    LiftoffRegList outer_pinned;
    if (FLAG_trace_wasm_memory) outer_pinned.set(index);
    __ AtomicStore(addr, index, offset, value, type, outer_pinned);
    if (FLAG_trace_wasm_memory) {
      TraceMemoryOperation(true, type.mem_rep(), index, offset,
                           decoder->position());
    }
  }

  void AtomicLoadMem(FullDecoder* decoder, LoadType type,
                     const MemoryAccessImmediate<validate>& imm) {
    ValueType value_type = type.value_type();
    LiftoffRegList pinned;
    Register index = pinned.set(__ PopToRegister()).gp();
    if (BoundsCheckMem(decoder, type.size(), imm.offset, index, pinned,
                       kDoForceCheck)) {
      return;
    }
    AlignmentCheckMem(decoder, type.size(), imm.offset, index, pinned);
    uint32_t offset = imm.offset;
    index = AddMemoryMasking(index, &offset, &pinned);
    DEBUG_CODE_COMMENT("Atomic load from memory");
    Register addr = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    LOAD_INSTANCE_FIELD(addr, MemoryStart, kSystemPointerSize);
    RegClass rc = reg_class_for(value_type);
    LiftoffRegister value = pinned.set(__ GetUnusedRegister(rc, pinned));
    __ AtomicLoad(value, addr, index, offset, type, pinned);
    __ PushRegister(value_type, value);

    if (FLAG_trace_wasm_memory) {
      TraceMemoryOperation(false, type.mem_type().representation(), index,
                           offset, decoder->position());
    }
  }

  void AtomicBinop(FullDecoder* decoder, StoreType type,
                   const MemoryAccessImmediate<validate>& imm,
                   void (LiftoffAssembler::*emit_fn)(Register, Register,
                                                     uint32_t, LiftoffRegister,
                                                     StoreType)) {
    ValueType result_type = type.value_type();
    LiftoffRegList pinned;
    LiftoffRegister value = pinned.set(__ PopToRegister());
    // We have to reuse the value register as the result register so that we
    //  don't run out of registers on ia32. For this we use the value register
    //  as the result register if it has no other uses. Otherwise  we allocate
    //  a new register and let go of the value register to get spilled.
    LiftoffRegister result = value;
    if (__ cache_state()->is_used(value)) {
      result = pinned.set(__ GetUnusedRegister(value.reg_class(), pinned));
      __ Move(result, value, result_type);
      pinned.clear(value);
    }
    Register index = pinned.set(__ PopToRegister(pinned)).gp();
    if (BoundsCheckMem(decoder, type.size(), imm.offset, index, pinned,
                       kDoForceCheck)) {
      return;
    }
    AlignmentCheckMem(decoder, type.size(), imm.offset, index, pinned);

    uint32_t offset = imm.offset;
    index = AddMemoryMasking(index, &offset, &pinned);
    Register addr = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    LOAD_INSTANCE_FIELD(addr, MemoryStart, kSystemPointerSize);

    (asm_.*emit_fn)(addr, index, offset, result, type);
    __ PushRegister(result_type, result);
  }

#define ATOMIC_STORE_LIST(V)        \
  V(I32AtomicStore, kI32Store)      \
  V(I64AtomicStore, kI64Store)      \
  V(I32AtomicStore8U, kI32Store8)   \
  V(I32AtomicStore16U, kI32Store16) \
  V(I64AtomicStore8U, kI64Store8)   \
  V(I64AtomicStore16U, kI64Store16) \
  V(I64AtomicStore32U, kI64Store32)

#define ATOMIC_LOAD_LIST(V)        \
  V(I32AtomicLoad, kI32Load)       \
  V(I64AtomicLoad, kI64Load)       \
  V(I32AtomicLoad8U, kI32Load8U)   \
  V(I32AtomicLoad16U, kI32Load16U) \
  V(I64AtomicLoad8U, kI64Load8U)   \
  V(I64AtomicLoad16U, kI64Load16U) \
  V(I64AtomicLoad32U, kI64Load32U)

#define ATOMIC_BINOP_INSTRUCTION_LIST(V) \
  V(Add, I32AtomicAdd, kI32Store)        \
  V(Add, I64AtomicAdd, kI64Store)        \
  V(Add, I32AtomicAdd8U, kI32Store8)     \
  V(Add, I32AtomicAdd16U, kI32Store16)   \
  V(Add, I64AtomicAdd8U, kI64Store8)     \
  V(Add, I64AtomicAdd16U, kI64Store16)   \
  V(Add, I64AtomicAdd32U, kI64Store32)   \
  V(Sub, I32AtomicSub, kI32Store)        \
  V(Sub, I64AtomicSub, kI64Store)        \
  V(Sub, I32AtomicSub8U, kI32Store8)     \
  V(Sub, I32AtomicSub16U, kI32Store16)   \
  V(Sub, I64AtomicSub8U, kI64Store8)     \
  V(Sub, I64AtomicSub16U, kI64Store16)   \
  V(Sub, I64AtomicSub32U, kI64Store32)   \
  V(And, I32AtomicAnd, kI32Store)        \
  V(And, I64AtomicAnd, kI64Store)        \
  V(And, I32AtomicAnd8U, kI32Store8)     \
  V(And, I32AtomicAnd16U, kI32Store16)   \
  V(And, I64AtomicAnd8U, kI64Store8)     \
  V(And, I64AtomicAnd16U, kI64Store16)   \
  V(And, I64AtomicAnd32U, kI64Store32)   \
  V(Or, I32AtomicOr, kI32Store)          \
  V(Or, I64AtomicOr, kI64Store)          \
  V(Or, I32AtomicOr8U, kI32Store8)       \
  V(Or, I32AtomicOr16U, kI32Store16)     \
  V(Or, I64AtomicOr8U, kI64Store8)       \
  V(Or, I64AtomicOr16U, kI64Store16)     \
  V(Or, I64AtomicOr32U, kI64Store32)     \
  V(Xor, I32AtomicXor, kI32Store)        \
  V(Xor, I64AtomicXor, kI64Store)        \
  V(Xor, I32AtomicXor8U, kI32Store8)     \
  V(Xor, I32AtomicXor16U, kI32Store16)   \
  V(Xor, I64AtomicXor8U, kI64Store8)     \
  V(Xor, I64AtomicXor16U, kI64Store16)   \
  V(Xor, I64AtomicXor32U, kI64Store32)

  void AtomicOp(FullDecoder* decoder, WasmOpcode opcode, Vector<Value> args,
                const MemoryAccessImmediate<validate>& imm, Value* result) {
    switch (opcode) {
#define ATOMIC_STORE_OP(name, type)                \
  case wasm::kExpr##name:                          \
    AtomicStoreMem(decoder, StoreType::type, imm); \
    break;

      ATOMIC_STORE_LIST(ATOMIC_STORE_OP)
#undef ATOMIC_STORE_OP

#define ATOMIC_LOAD_OP(name, type)               \
  case wasm::kExpr##name:                        \
    AtomicLoadMem(decoder, LoadType::type, imm); \
    break;

      ATOMIC_LOAD_LIST(ATOMIC_LOAD_OP)
#undef ATOMIC_LOAD_OP

#define ATOMIC_BINOP_OP(op, name, type)                                        \
  case wasm::kExpr##name:                                                      \
    AtomicBinop(decoder, StoreType::type, imm, &LiftoffAssembler::Atomic##op); \
    break;

      ATOMIC_BINOP_INSTRUCTION_LIST(ATOMIC_BINOP_OP)
#undef ATOMIC_BINOP_OP
      default:
        unsupported(decoder, kAtomics, "atomicop");
    }
  }

#undef ATOMIC_STORE_LIST
#undef ATOMIC_LOAD_LIST
#undef ATOMIC_BINOP_INSTRUCTION_LIST

  void AtomicFence(FullDecoder* decoder) {
    unsupported(decoder, kAtomics, "atomic.fence");
  }
  void MemoryInit(FullDecoder* decoder,
                  const MemoryInitImmediate<validate>& imm, const Value& dst,
                  const Value& src, const Value& size) {
    unsupported(decoder, kBulkMemory, "memory.init");
  }
  void DataDrop(FullDecoder* decoder, const DataDropImmediate<validate>& imm) {
    unsupported(decoder, kBulkMemory, "data.drop");
  }
  void MemoryCopy(FullDecoder* decoder,
                  const MemoryCopyImmediate<validate>& imm, const Value& dst,
                  const Value& src, const Value& size) {
    unsupported(decoder, kBulkMemory, "memory.copy");
  }
  void MemoryFill(FullDecoder* decoder,
                  const MemoryIndexImmediate<validate>& imm, const Value& dst,
                  const Value& value, const Value& size) {
    unsupported(decoder, kBulkMemory, "memory.fill");
  }
  void TableInit(FullDecoder* decoder, const TableInitImmediate<validate>& imm,
                 Vector<Value> args) {
    unsupported(decoder, kBulkMemory, "table.init");
  }
  void ElemDrop(FullDecoder* decoder, const ElemDropImmediate<validate>& imm) {
    unsupported(decoder, kBulkMemory, "elem.drop");
  }
  void TableCopy(FullDecoder* decoder, const TableCopyImmediate<validate>& imm,
                 Vector<Value> args) {
    unsupported(decoder, kBulkMemory, "table.copy");
  }
  void TableGrow(FullDecoder* decoder, const TableIndexImmediate<validate>& imm,
                 const Value& value, const Value& delta, Value* result) {
    unsupported(decoder, kAnyRef, "table.grow");
  }
  void TableSize(FullDecoder* decoder, const TableIndexImmediate<validate>& imm,
                 Value* result) {
    unsupported(decoder, kAnyRef, "table.size");
  }
  void TableFill(FullDecoder* decoder, const TableIndexImmediate<validate>& imm,
                 const Value& start, const Value& value, const Value& count) {
    unsupported(decoder, kAnyRef, "table.fill");
  }

 private:
  static constexpr WasmOpcode kNoOutstandingOp = kExprUnreachable;

  LiftoffAssembler asm_;

  // Used for merging code generation of subsequent operations (via look-ahead).
  // Set by the first opcode, reset by the second.
  WasmOpcode outstanding_op_ = kNoOutstandingOp;

  compiler::CallDescriptor* const descriptor_;
  CompilationEnv* const env_;
  DebugSideTableBuilder* const debug_sidetable_builder_;
  LiftoffBailoutReason bailout_reason_ = kSuccess;
  std::vector<OutOfLineCode> out_of_line_code_;
  SourcePositionTableBuilder source_position_table_builder_;
  std::vector<trap_handler::ProtectedInstructionData> protected_instructions_;
  // Zone used to store information during compilation. The result will be
  // stored independently, such that this zone can die together with the
  // LiftoffCompiler after compilation.
  Zone* compilation_zone_;
  SafepointTableBuilder safepoint_table_builder_;
  // The pc offset of the instructions to reserve the stack frame. Needed to
  // patch the actually needed stack size in the end.
  uint32_t pc_offset_stack_frame_construction_ = 0;
  // For emitting breakpoint, we store a pointer to the position of the next
  // breakpoint, and a pointer after the list of breakpoints as end marker.
  int* next_breakpoint_ptr_ = nullptr;
  int* next_breakpoint_end_ = nullptr;

  bool has_outstanding_op() const {
    return outstanding_op_ != kNoOutstandingOp;
  }

  void TraceCacheState(FullDecoder* decoder) const {
    if (!FLAG_trace_liftoff) return;
    StdoutStream os;
    for (int control_depth = decoder->control_depth() - 1; control_depth >= -1;
         --control_depth) {
      auto* cache_state =
          control_depth == -1 ? __ cache_state()
                              : &decoder->control_at(control_depth)
                                     ->label_state;
      os << PrintCollection(cache_state->stack_state);
      if (control_depth != -1) PrintF("; ");
    }
    os << "\n";
  }

  DISALLOW_IMPLICIT_CONSTRUCTORS(LiftoffCompiler);
};

}  // namespace

WasmCompilationResult ExecuteLiftoffCompilation(
    AccountingAllocator* allocator, CompilationEnv* env,
    const FunctionBody& func_body, int func_index, Counters* counters,
    WasmFeatures* detected, Vector<int> breakpoints) {
  int func_body_size = static_cast<int>(func_body.end - func_body.start);
  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("v8.wasm"),
               "ExecuteLiftoffCompilation", "func_index", func_index,
               "body_size", func_body_size);

  Zone zone(allocator, "LiftoffCompilationZone");
  auto call_descriptor = compiler::GetWasmCallDescriptor(&zone, func_body.sig);
  base::Optional<TimedHistogramScope> liftoff_compile_time_scope;
  if (counters) {
    liftoff_compile_time_scope.emplace(counters->liftoff_compile_time());
  }
  size_t code_size_estimate =
      WasmCodeManager::EstimateLiftoffCodeSize(func_body_size);
  // Allocate the initial buffer a bit bigger to avoid reallocation during code
  // generation.
  std::unique_ptr<wasm::WasmInstructionBuffer> instruction_buffer =
      wasm::WasmInstructionBuffer::New(128 + code_size_estimate * 4 / 3);
  DebugSideTableBuilder* const kNoDebugSideTable = nullptr;
  WasmFullDecoder<Decoder::kValidate, LiftoffCompiler> decoder(
      &zone, env->module, env->enabled_features, detected, func_body,
      call_descriptor, env, &zone, instruction_buffer->CreateView(),
      kNoDebugSideTable, breakpoints);
  decoder.Decode();
  liftoff_compile_time_scope.reset();
  LiftoffCompiler* compiler = &decoder.interface();
  if (decoder.failed()) compiler->OnFirstError(&decoder);

  if (counters) {
    // Check that the histogram for the bailout reasons has the correct size.
    DCHECK_EQ(0, counters->liftoff_bailout_reasons()->min());
    DCHECK_EQ(kNumBailoutReasons - 1,
              counters->liftoff_bailout_reasons()->max());
    DCHECK_EQ(kNumBailoutReasons,
              counters->liftoff_bailout_reasons()->num_buckets());
    // Register the bailout reason (can also be {kSuccess}).
    counters->liftoff_bailout_reasons()->AddSample(
        static_cast<int>(compiler->bailout_reason()));
    if (compiler->did_bailout()) {
      // Liftoff compilation failed.
      counters->liftoff_unsupported_functions()->Increment();
      return WasmCompilationResult{};
    }

    counters->liftoff_compiled_functions()->Increment();
  }

  WasmCompilationResult result;
  compiler->GetCode(&result.code_desc);
  result.instr_buffer = instruction_buffer->ReleaseBuffer();
  result.source_positions = compiler->GetSourcePositionTable();
  result.protected_instructions = compiler->GetProtectedInstructions();
  result.frame_slot_count = compiler->GetTotalFrameSlotCount();
  result.tagged_parameter_slots = call_descriptor->GetTaggedParameterSlots();
  result.result_tier = ExecutionTier::kLiftoff;

  DCHECK(result.succeeded());
  return result;
}

DebugSideTable GenerateLiftoffDebugSideTable(AccountingAllocator* allocator,
                                             CompilationEnv* env,
                                             const FunctionBody& func_body) {
  Zone zone(allocator, "LiftoffDebugSideTableZone");
  auto call_descriptor = compiler::GetWasmCallDescriptor(&zone, func_body.sig);
  DebugSideTableBuilder debug_sidetable_builder;
  WasmFeatures detected;
  WasmFullDecoder<Decoder::kValidate, LiftoffCompiler> decoder(
      &zone, env->module, env->enabled_features, &detected, func_body,
      call_descriptor, env, &zone,
      NewAssemblerBuffer(AssemblerBase::kDefaultBufferSize),
      &debug_sidetable_builder);
  decoder.Decode();
  DCHECK(decoder.ok());
  DCHECK(!decoder.interface().did_bailout());
  return debug_sidetable_builder.GenerateDebugSideTable();
}

#undef __
#undef TRACE
#undef WASM_INSTANCE_OBJECT_FIELD_OFFSET
#undef WASM_INSTANCE_OBJECT_FIELD_SIZE
#undef LOAD_INSTANCE_FIELD
#undef LOAD_TAGGED_PTR_INSTANCE_FIELD
#undef DEBUG_CODE_COMMENT

}  // namespace wasm
}  // namespace internal
}  // namespace v8
