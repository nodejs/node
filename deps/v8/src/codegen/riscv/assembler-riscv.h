// Copyright (c) 1994-2006 Sun Microsystems Inc.
// All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// - Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// - Redistribution in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// - Neither the name of Sun Microsystems or the names of contributors may
// be used to endorse or promote products derived from this software without
// specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// The original source code covered by the above license above has been
// modified significantly by Google Inc.
// Copyright 2021 the V8 project authors. All rights reserved.

#ifndef V8_CODEGEN_RISCV_ASSEMBLER_RISCV_H_
#define V8_CODEGEN_RISCV_ASSEMBLER_RISCV_H_

#include <stdio.h>

#include <memory>
#include <set>

#include "src/codegen/assembler.h"
#include "src/codegen/constant-pool.h"
#include "src/codegen/constants-arch.h"
#include "src/codegen/external-reference.h"
#include "src/codegen/flush-instruction-cache.h"
#include "src/codegen/label.h"
#include "src/codegen/riscv/base-assembler-riscv.h"
#include "src/codegen/riscv/base-riscv-i.h"
#include "src/codegen/riscv/extension-riscv-a.h"
#include "src/codegen/riscv/extension-riscv-b.h"
#include "src/codegen/riscv/extension-riscv-c.h"
#include "src/codegen/riscv/extension-riscv-d.h"
#include "src/codegen/riscv/extension-riscv-f.h"
#include "src/codegen/riscv/extension-riscv-m.h"
#include "src/codegen/riscv/extension-riscv-v.h"
#include "src/codegen/riscv/extension-riscv-zfh.h"
#include "src/codegen/riscv/extension-riscv-zicond.h"
#include "src/codegen/riscv/extension-riscv-zicsr.h"
#include "src/codegen/riscv/extension-riscv-zifencei.h"
#include "src/codegen/riscv/extension-riscv-zimop.h"
#include "src/codegen/riscv/register-riscv.h"
#include "src/common/code-memory-access.h"
#include "src/objects/contexts.h"
#include "src/objects/smi.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// Machine instruction Operands.
constexpr int kSmiShift = kSmiTagSize + kSmiShiftSize;
constexpr uintptr_t kSmiShiftMask = (1UL << kSmiShift) - 1;
// Class Operand represents a shifter operand in data processing instructions.
class Operand {
 public:
  // Immediate.
  V8_INLINE explicit Operand(intptr_t immediate,
                             RelocInfo::Mode rmode = RelocInfo::NO_INFO)
      : rm_(no_reg), rmode_(rmode) {
    value_.immediate = immediate;
  }

  V8_INLINE explicit Operand(Tagged<Smi> value)
      : Operand(static_cast<intptr_t>(value.ptr())) {}

  V8_INLINE explicit Operand(const ExternalReference& f)
      : rm_(no_reg), rmode_(RelocInfo::EXTERNAL_REFERENCE) {
    value_.immediate = static_cast<intptr_t>(f.address());
  }

  explicit Operand(Handle<HeapObject> handle,
                   RelocInfo::Mode rmode = RelocInfo::FULL_EMBEDDED_OBJECT);

  static Operand EmbeddedNumber(double number);  // Smi or HeapNumber.

  // Register.
  V8_INLINE explicit Operand(Register rm) : rm_(rm) {}

  // Return true if this is a register operand.
  V8_INLINE bool is_reg() const { return rm_.is_valid(); }

  inline intptr_t immediate_for_heap_number_request() const {
    DCHECK(rmode() == RelocInfo::FULL_EMBEDDED_OBJECT);
    return value_.immediate;
  }

  inline intptr_t immediate() const {
    DCHECK(!is_reg());
    DCHECK(!IsHeapNumberRequest());
    return value_.immediate;
  }

  bool IsImmediate() const { return !rm_.is_valid(); }

  HeapNumberRequest heap_number_request() const {
    DCHECK(IsHeapNumberRequest());
    return value_.heap_number_request;
  }

  bool IsHeapNumberRequest() const {
    DCHECK_IMPLIES(is_heap_number_request_, IsImmediate());
    DCHECK_IMPLIES(is_heap_number_request_,
                   rmode_ == RelocInfo::FULL_EMBEDDED_OBJECT ||
                       rmode_ == RelocInfo::CODE_TARGET);
    return is_heap_number_request_;
  }

  Register rm() const { return rm_; }

  RelocInfo::Mode rmode() const { return rmode_; }

 private:
  Register rm_;
  union Value {
    Value() {}
    HeapNumberRequest heap_number_request;  // if is_heap_number_request_
    intptr_t immediate;                     // otherwise
  } value_;                                 // valid if rm_ == no_reg
  bool is_heap_number_request_ = false;
  RelocInfo::Mode rmode_;

  friend class Assembler;
  friend class MacroAssembler;
};

// On RISC-V we have only one addressing mode with base_reg + offset.
// Class MemOperand represents a memory operand in load and store instructions.
class V8_EXPORT_PRIVATE MemOperand : public Operand {
 public:
  // Immediate value attached to offset.
  enum OffsetAddend { offset_minus_one = -1, offset_zero = 0 };

  explicit MemOperand(Register rn, int32_t offset = 0);
  explicit MemOperand(Register rn, int32_t unit, int32_t multiplier,
                      OffsetAddend offset_addend = offset_zero);
  int32_t offset() const { return offset_; }

  void set_offset(int32_t offset) { offset_ = offset; }

  bool OffsetIsInt12Encodable() const { return is_int12(offset_); }

 private:
  int32_t offset_;

  friend class Assembler;
};

class V8_EXPORT_PRIVATE Assembler : public AssemblerBase,
                                    public AssemblerRISCVI,
                                    public AssemblerRISCVA,
                                    public AssemblerRISCVB,
                                    public AssemblerRISCVF,
                                    public AssemblerRISCVD,
                                    public AssemblerRISCVM,
                                    public AssemblerRISCVC,
                                    public AssemblerRISCVZifencei,
                                    public AssemblerRISCVZicsr,
                                    public AssemblerRISCVZicond,
                                    public AssemblerRISCVZicfiss,
                                    public AssemblerRISCVZfh,
                                    public AssemblerRISCVV {
 public:
  // Create an assembler. Instructions and relocation information are emitted
  // into a buffer, with the instructions starting from the beginning and the
  // relocation information starting from the end of the buffer. See CodeDesc
  // for a detailed comment on the layout (globals.h).
  //
  // If the provided buffer is nullptr, the assembler allocates and grows its
  // own buffer. Otherwise it takes ownership of the provided buffer.
  explicit Assembler(const AssemblerOptions&,
                     std::unique_ptr<AssemblerBuffer> = {});
  // For compatibility with assemblers that require a zone.
  Assembler(const MaybeAssemblerZone&, const AssemblerOptions& options,
            std::unique_ptr<AssemblerBuffer> buffer = {})
      : Assembler(options, std::move(buffer)) {}

  virtual ~Assembler();

  static RegList DefaultTmpList();
  static DoubleRegList DefaultFPTmpList();

  void AbortedCodeGeneration() override;

  // GetCode emits any pending (non-emitted) code and fills the descriptor desc.
  static constexpr int kNoHandlerTable = 0;
  static constexpr SafepointTableBuilderBase* kNoSafepointTable = nullptr;
  void GetCode(LocalIsolate* isolate, CodeDesc* desc,
               SafepointTableBuilderBase* safepoint_table_builder,
               int handler_table_offset);

  // Convenience wrapper for allocating with an Isolate.
  void GetCode(Isolate* isolate, CodeDesc* desc);
  // Convenience wrapper for code without safepoint or handler tables.
  void GetCode(LocalIsolate* isolate, CodeDesc* desc) {
    GetCode(isolate, desc, kNoSafepointTable, kNoHandlerTable);
  }

  // On RISC-V, we sometimes need to emit branch trampolines between emitting
  // a call instruction (jal/jalr) and recording a safepoint. This means that
  // we have to be careful to make sure the safepoint is recorded at the right
  // position. So we record the pc right after emitting the call instruction
  // and use this for the safepoint.
  int pc_offset_for_safepoint() const { return pc_offset_for_safepoint_; }

  // Unused on this architecture.
  void MaybeEmitOutOfLineConstantPool() {}

  // Clear any internal state to avoid check failures if we drop
  // the assembly code.
  void ClearInternalState() { constpool_.Clear(); }

  // Label operations & relative jumps (PPUM Appendix D).
  //
  // Takes a branch opcode (cc) and a label (L) and generates
  // either a backward branch or a forward branch and links it
  // to the label fixup chain. Usage:
  //
  // Label L;    // unbound label
  // j(cc, &L);  // forward branch to unbound label
  // bind(&L);   // bind label to the current pc
  // j(cc, &L);  // backward branch to bound label
  // bind(&L);   // illegal: a label may be bound only once
  //
  // Note: The same Label can be used for forward and backward branches
  // but it may be bound only once.
  void bind(Label* L);  // Binds an unbound label L to current code position.

  // Determines if Label is bound and near enough so that branch instruction
  // can be used to reach it, instead of jump instruction.
  bool is_near(Label* L);
  bool is_near(Label* L, OffsetSize bits);
  bool is_near_branch(Label* L);

  // Get offset from instr.
  int BranchOffset(Instr instr);
  static int BranchLongOffset(Instr auipc, Instr jalr);
  static int PatchBranchLongOffset(
      Address pc, Instr auipc, Instr instr_I, int32_t offset,
      WritableJitAllocation* jit_allocation = nullptr);

  // Returns the branch offset to the given label from the current code
  // position. Links the label to the current position if it is still unbound.
  int32_t branch_offset_helper(Label* L, OffsetSize bits) override;
  int32_t branch_long_offset(Label* L);

  // During code generation builtin targets in PC-relative call/jump
  // instructions are temporarily encoded as builtin ID until the generated
  // code is moved into the code space.
  static inline Builtin target_builtin_at(Address pc);

  // Read/Modify the code target address in the branch/call instruction at pc.
  // The isolate argument is unused (and may be nullptr) when skipping flushing.
  static Address target_constant_address_at(Address pc);

  static Address target_address_at(Address pc, Address constant_pool);

  static void set_target_address_at(
      Address pc, Address constant_pool, Address target,
      WritableJitAllocation* jit_allocation = nullptr,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED);

  // Read/Modify the code target address in the branch/call instruction at pc.
  inline static Tagged_t target_compressed_address_at(Address pc,
                                                      Address constant_pool);
  inline static void set_target_compressed_address_at(
      Address pc, Address constant_pool, Tagged_t target,
      WritableJitAllocation* jit_allocation = nullptr,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED);

  inline Handle<Object> code_target_object_handle_at(Address pc,
                                                     Address constant_pool);
  inline Handle<HeapObject> compressed_embedded_object_handle_at(
      Address pc, Address constant_pool);

  inline Handle<HeapObject> embedded_object_handle_at(Address pc);

#ifdef V8_TARGET_ARCH_RISCV64
  inline void set_embedded_object_index_referenced_from(
      Address p, EmbeddedObjectIndex index);
#endif

  static bool IsConstantPoolAt(Instruction* instr);
  static int ConstantPoolSizeAt(Instruction* instr);
  void EmitPoolGuard();

  bool pools_blocked() const { return pools_blocked_nesting_ > 0; }
  void StartBlockPools(int margin);
  void EndBlockPools();

  void FinishCode() { constpool_.Emit(); }

#if defined(V8_TARGET_ARCH_RISCV64)
  static void set_target_value_at(
      Address pc, uint64_t target,
      WritableJitAllocation* jit_allocation = nullptr,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED);
#elif defined(V8_TARGET_ARCH_RISCV32)
  static void set_target_value_at(
      Address pc, uint32_t target,
      WritableJitAllocation* jit_allocation = nullptr,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED);
#endif

  static inline int32_t target_constant32_at(Address pc);
  static inline void set_target_constant32_at(
      Address pc, uint32_t target, WritableJitAllocation* jit_allocation,
      ICacheFlushMode icache_flush_mode);

  static void JumpLabelToJumpRegister(Address pc);

  // This sets the branch destination (which gets loaded at the call address).
  // This is for calls and branches within generated code.  The serializer
  // has already deserialized the lui/ori instructions etc.
  inline static void deserialization_set_special_target_at(Address location,
                                                           Tagged<Code> code,
                                                           Address target);

  // Get the size of the special target encoded at 'instruction_payload'.
  inline static int deserialization_special_target_size(
      Address instruction_payload);

  // This sets the internal reference at the pc.
  inline static void deserialization_set_target_internal_reference_at(
      Address pc, Address target, WritableJitAllocation& jit_allocation,
      RelocInfo::Mode mode = RelocInfo::INTERNAL_REFERENCE);

  // Read/modify the uint32 constant used at pc.
  static inline uint32_t uint32_constant_at(Address pc, Address constant_pool);
  static inline void set_uint32_constant_at(
      Address pc, Address constant_pool, uint32_t new_constant,
      WritableJitAllocation* jit_allocation,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED);

  // Here we are patching the address in the LUI/ADDI instruction pair.
  // These values are used in the serialization process and must be zero for
  // RISC-V platform, as InstructionStream, Embedded Object or
  // External-reference pointers are split across two consecutive instructions
  // and don't exist separately in the code, so the serializer should not step
  // forwards in memory after a target is resolved and written.
  static constexpr int kSpecialTargetSize = 0;

  // Number of consecutive instructions used to store 32bit/64bit constant.
  // This constant was used in RelocInfo::target_address_address() function
  // to tell serializer address of the instruction that follows
  // LUI/ADDI instruction pair.
  static constexpr int kInstructionsFor32BitConstant = 2;
  static constexpr int kInstructionsFor64BitConstant = 8;

  // Difference between address of current opcode and value read from pc
  // register.
  static constexpr int kPcLoadDelta = 4;

  // Bits available for offset field in branches
  static constexpr int kBranchOffsetBits = 13;

  // Bits available for offset field in jump
  static constexpr int kJumpOffsetBits = 21;

  // Bits available for offset field in compresed jump
  static constexpr int kCJalOffsetBits = 12;

  // Bits available for offset field in compressed branch
  static constexpr int kCBranchOffsetBits = 9;

  // Max offset for b instructions with 12-bit offset field (multiple of 2)
  static constexpr int kMaxBranchOffset = (1 << (13 - 1)) - 1;

  // Max offset for jal instruction with 20-bit offset field (multiple of 2)
  static constexpr int kMaxJumpOffset = (1 << (21 - 1)) - 1;

  // The size of the an entry in the trampoline pool.
  static constexpr int kTrampolineSlotsSize = 2 * kInstrSize;

  // The size of the overhead for a trampoline pool. The overhead covers
  // the jump around the entries.
  static constexpr int kTrampolinePoolOverhead = 1 * kInstrSize;

  RegList* GetScratchRegisterList() { return &scratch_register_list_; }
  DoubleRegList* GetScratchDoubleRegisterList() {
    return &scratch_double_register_list_;
  }

  // ---------------------------------------------------------------------------
  // InstructionStream generation.

  // Insert the smallest number of nop instructions
  // possible to align the pc offset to a multiple
  // of m. m must be a power of 2 (>= 4).
  void Align(int m);
  // Insert the smallest number of zero bytes possible to align the pc offset
  // to a mulitple of m. m must be a power of 2 (>= 2).
  void DataAlign(int m);
  // Aligns code to something that's optimal for a jump target for the platform.
  void CodeTargetAlign();
  void SwitchTargetAlign() { CodeTargetAlign(); }
  void BranchTargetAlign() {}
  void LoopHeaderAlign() { CodeTargetAlign(); }

  // Different nop operations are used by the code generator to detect certain
  // states of the generated code.
  enum NopMarkerTypes {
    NON_MARKING_NOP = 0,
    DEBUG_BREAK_NOP,
    // IC markers.
    PROPERTY_ACCESS_INLINED,
    PROPERTY_ACCESS_INLINED_CONTEXT,
    PROPERTY_ACCESS_INLINED_CONTEXT_DONT_DELETE,
    // Helper values.
    LAST_CODE_MARKER,
    FIRST_IC_MARKER = PROPERTY_ACCESS_INLINED,
  };

  void NOP();
  void EBREAK();

  // Assembler Pseudo Instructions (Tables 25.2, 25.3, RISC-V Unprivileged ISA)
  void nop();
#if defined(V8_TARGET_ARCH_RISCV64)
  void RecursiveLiImpl(Register rd, int64_t imm);
  void RecursiveLi(Register rd, int64_t imm);
  static int RecursiveLiCount(int64_t imm);
  static int RecursiveLiImplCount(int64_t imm);
  void RV_li(Register rd, int64_t imm);
  static int RV_li_count(int64_t imm, bool is_get_temp_reg = false);
  // Returns the number of instructions required to load the immediate
  void GeneralLi(Register rd, int64_t imm);
  static int GeneralLiCount(int64_t imm, bool is_get_temp_reg = false);
  // Loads an immediate, always using 8 instructions, regardless of the value,
  // so that it can be modified later.
  void li_constant(Register rd, int64_t imm);
  void li_constant32(Register rd, int32_t imm);
  void li_ptr(Register rd, int64_t imm);
#endif
#if defined(V8_TARGET_ARCH_RISCV32)
  void RV_li(Register rd, int32_t imm);
  static int RV_li_count(int32_t imm, bool is_get_temp_reg = false);

  void li_constant(Register rd, int32_t imm);
  void li_ptr(Register rd, int32_t imm);
#endif

  void break_(uint32_t code, bool break_as_stop = false);
  void stop(uint32_t code = kMaxStopCode);

  // Check the code size generated from label to here.
  int SizeOfCodeGeneratedSince(Label* label) {
    return pc_offset() - label->pos();
  }

  // Check the number of instructions generated from label to here.
  int InstructionsGeneratedSince(Label* label) {
    return SizeOfCodeGeneratedSince(label) / kInstrSize;
  }

  // Blocks the trampoline pool and constant pools emissions. Emits pools if
  // necessary to ensure that {margin} more bytes can be emitted without
  // triggering pool emission.
  class V8_NODISCARD BlockPoolsScope {
   public:
    // We leave space for a number of trampoline pool slots, so we do not
    // have to pass in an explicit margin for all scopes.
    static constexpr int kGap = kTrampolineSlotsSize * 16;

    explicit BlockPoolsScope(Assembler* assem, int margin = 0)
        : assem_(assem), margin_(margin) {
      assem->StartBlockPools(margin);
      start_offset_ = assem->pc_offset();
    }

    ~BlockPoolsScope() {
      int generated = assem_->pc_offset() - start_offset_;
      USE(generated);  // Only used in DCHECK.
      int allowed = margin_;
      if (allowed == 0) allowed = kGap - kTrampolinePoolOverhead;
      DCHECK_GE(generated, 0);
      DCHECK_LE(generated, allowed);
      assem_->EndBlockPools();
    }

   private:
    Assembler* const assem_;
    const int margin_;
    int start_offset_;

    DISALLOW_IMPLICIT_CONSTRUCTORS(BlockPoolsScope);
  };

  // Record a deoptimization reason that can be used by a log or cpu profiler.
  // Use --trace-deopt to enable.
  void RecordDeoptReason(DeoptimizeReason reason, uint32_t node_id,
                         SourcePosition position, int id);
  static int RelocateInternalReference(
      RelocInfo::Mode rmode, Address pc, intptr_t pc_delta,
      WritableJitAllocation* jit_allocation = nullptr);
  static void RelocateRelativeReference(
      RelocInfo::Mode rmode, Address pc, intptr_t pc_delta,
      WritableJitAllocation* jit_allocation = nullptr);

  // Writes a single byte or word of data in the code stream.  Used for
  // inline tables, e.g., jump-tables.
  void db(uint8_t data);
  void dd(uint32_t data);
  void dq(uint64_t data);

#if defined(V8_TARGET_ARCH_RISCV64)
  void dp(uintptr_t data) { dq(data); }
  void dq(Label* label);
#elif defined(V8_TARGET_ARCH_RISCV32)
  void dp(uintptr_t data) { dd(data); }
  void dd(Label* label);
#endif

  Instruction* pc() const { return reinterpret_cast<Instruction*>(pc_); }

  Instruction* InstructionAt(ptrdiff_t offset) const {
    return reinterpret_cast<Instruction*>(buffer_start_ + offset);
  }

  // Check if there is less than kGap bytes available in the buffer.
  // If this is the case, we need to grow the buffer before emitting
  // an instruction or relocation information.
  inline bool overflow() const { return pc_ >= reloc_info_writer.pos() - kGap; }

  // Get the number of bytes available in the buffer.
  inline intptr_t available_space() const {
    return reloc_info_writer.pos() - pc_;
  }

  // Read/patch instructions.
  static Instr instr_at(Address pc) { return *reinterpret_cast<Instr*>(pc); }
  static void instr_at_put(Address pc, Instr instr,
                           WritableJitAllocation* jit_allocation = nullptr);

  Instr instr_at(int pos) {
    return *reinterpret_cast<Instr*>(buffer_start_ + pos);
  }
  void instr_at_put(int pos, Instr instr,
                    WritableJitAllocation* jit_allocation = nullptr);

  void instr_at_put(int pos, ShortInstr instr,
                    WritableJitAllocation* jit_allocation = nullptr);

  // Get the code target object for a pc-relative call or jump.
  inline Handle<Code> relative_code_target_object_handle_at(Address pc) const;

  inline int UnboundLabelsCount() const { return unbound_labels_count_; }

  friend class VectorUnit;
  class VectorUnit {
   public:
    inline VSew sew() const { return sew_; }
    inline int32_t sew_bits() const { return 1 << (sew_ + 3); }

    inline int32_t vlmax() const {
      if ((lmul_ & 0b100) != 0) {
        return (CpuFeatures::vlen() / sew_bits()) >> (lmul_ & 0b11);
      } else {
        return ((CpuFeatures::vlen() << lmul_) / sew_bits());
      }
    }

    explicit VectorUnit(Assembler* assm) : assm_(assm) {}

    // Sets the floating-point rounding mode.
    // Updating the rounding mode can be expensive, and therefore isn't done
    // for every basic block. Instead, we assume that the rounding mode is
    // RNE. Any instruction sequence that changes the rounding mode must
    // change it back to RNE before it finishes.
    void set(FPURoundingMode mode) {
      if (mode_ != mode) {
        if (mode == 0) {
          assm_->fsrm(zero_reg);
        } else {
          assm_->fsrm(mode);
        }
        mode_ = mode;
      }
    }

    void set(int32_t avl, VSew sew, Vlmul lmul, TailAgnosticType tail = ta) {
      DCHECK(is_uint5(avl));
      if (avl != avl_ || sew != sew_ || lmul != lmul_) {
        avl_ = avl;
        sew_ = sew;
        lmul_ = lmul;
        assm_->vsetivli(zero_reg, static_cast<uint8_t>(avl), sew_, lmul_, tail);
      }
    }

    void set(Register vd, Register avl, VSew sew, Vlmul lmul,
             TailAgnosticType tail = ta) {
      assm_->vsetvli(vd, avl, sew, lmul, tail);
      avl_ = -1;
      sew_ = sew;
      lmul_ = lmul;
    }

    bool IsConfiguredForSimd128() const {
      int32_t expected_avl = 128 / sew_bits();
      return (avl_ == expected_avl);
    }

    void SetSimd128(VSew sew, TailAgnosticType tail = ta) {
      Vlmul lmul;
      switch (CpuFeatures::vlen()) {
        case 128:
          lmul = m1;
          break;
        case 256:
          lmul = mf2;
          break;
        case 512:
          lmul = mf4;
          break;
        default:
          static_assert(kMaxRvvVLEN <= 512, "Unsupported VLEN");
          UNIMPLEMENTED();
      }
      if (sew == E8) {
        set(16, sew, lmul, tail);
      } else if (sew == E16) {
        set(8, sew, lmul, tail);
      } else if (sew == E32) {
        set(4, sew, lmul, tail);
      } else if (sew == E64) {
        set(2, sew, lmul, tail);
      } else {
        UNREACHABLE();
      }
    }

    void SetSimd128Half(VSew sew, TailAgnosticType tail = ta) {
      Vlmul lmul;
      switch (CpuFeatures::vlen()) {
        case 128:
          lmul = mf2;
          break;
        case 256:
          lmul = mf4;
          break;
        case 512:
          lmul = mf8;
          break;
        default:
          static_assert(kMaxRvvVLEN <= 512, "Unsupported VLEN");
          UNIMPLEMENTED();
      }
      if (sew == E8) {
        set(8, sew, lmul, tail);
      } else if (sew == E16) {
        set(4, sew, lmul, tail);
      } else if (sew == E32) {
        set(2, sew, lmul, tail);
      } else if (sew == E64) {
        set(1, sew, lmul, tail);
      } else {
        UNREACHABLE();
      }
    }

    void SetSimd128x2(VSew sew, TailAgnosticType tail = ta) {
      Vlmul lmul;
      switch (CpuFeatures::vlen()) {
        case 128:
          lmul = m2;
          break;
        case 256:
          lmul = m1;
          break;
        case 512:
          lmul = mf2;
          break;
        default:
          static_assert(kMaxRvvVLEN <= 512, "Unsupported VLEN");
          UNIMPLEMENTED();
      }
      if (sew == E8) {
        set(32, sew, lmul, tail);
      } else if (sew == E16) {
        set(16, sew, lmul, tail);
      } else if (sew == E32) {
        set(8, sew, lmul, tail);
      } else if (sew == E64) {
        set(4, sew, lmul, tail);
      } else {
        UNREACHABLE();
      }
    }

    void clear() {
      // If the rounding mode isn't RNE, then we forgot to change it back.
      DCHECK_EQ(RNE, mode_);
      avl_ = -1;
      sew_ = kVsInvalid;
      lmul_ = kVlInvalid;
    }

   private:
    int32_t avl_ = -1;
    VSew sew_ = kVsInvalid;
    Vlmul lmul_ = kVlInvalid;
    Assembler* assm_;
    FPURoundingMode mode_ = RNE;
  };

  VectorUnit VU;

  void ClearVectorUnit() override { VU.clear(); }

 protected:
  // Readable constants for base and offset adjustment helper, these indicate if
  // aside from offset, another value like offset + 4 should fit into int16.
  enum class OffsetAccessType : bool {
    SINGLE_ACCESS = false,
    TWO_ACCESSES = true
  };

  // Determine whether need to adjust base and offset of memroy load/store
  bool NeedAdjustBaseAndOffset(
      const MemOperand& src, OffsetAccessType = OffsetAccessType::SINGLE_ACCESS,
      int second_Access_add_to_offset = 4);

  // Helper function for memory load/store using base register and offset.
  void AdjustBaseAndOffset(
      MemOperand* src, Register scratch,
      OffsetAccessType access_type = OffsetAccessType::SINGLE_ACCESS,
      int second_access_add_to_offset = 4);

  inline static void set_target_internal_reference_encoded_at(Address pc,
                                                              Address target);

  intptr_t buffer_space() const { return reloc_info_writer.pos() - pc_; }

  // Decode branch instruction at pos and return branch target pos.
  int target_at(int pos, bool is_internal);

  // Patch branch instruction at pos to branch to given branch target pos.
  void target_at_put(int pos, int target_pos, bool is_internal);

  // Say if we need to relocate with this mode.
  bool MustUseReg(RelocInfo::Mode rmode);

  // Record reloc info for current pc_.
  void RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data = 0);

  // Record the current pc for the next safepoint.
  void RecordPcForSafepoint() override {
    set_pc_offset_for_safepoint(pc_offset());
  }

  void set_pc_offset_for_safepoint(int pc_offset) {
    pc_offset_for_safepoint_ = pc_offset;
  }

  RelocInfoStatus RecordEntry64(uint64_t data, RelocInfo::Mode rmode) {
    return constpool_.RecordEntry64(data, rmode);
  }

  void RecordConstPool(int size, const BlockPoolsScope& scope);

  bool is_trampoline_emitted() const { return trampoline_check_ == kMaxInt; }

 private:
  // Avoid overflows for displacements etc.
  static const int kMaximalBufferSize = 512 * MB;

  // The relocation writer's position is at least kGap bytes below the end of
  // the generated instructions. This is so that multi-instruction sequences do
  // not have to check for overflow. The same is true for writes of large
  // relocation info entries.
  static constexpr int kGap = 64;
  static_assert(AssemblerBase::kMinimalBufferSize >= 2 * kGap);

  // Emission of the pools may be blocked in some code sequences. The
  // nesting is zero when the pools aren't blocked.
  int pools_blocked_nesting_ = 0;

  // Relocation information generation.
  // Each relocation is encoded as a variable size value.
  static constexpr int kMaxRelocSize = RelocInfoWriter::kMaxSize;
  RelocInfoWriter reloc_info_writer;

  // Keep track of the last call instruction (jal/jalr) position to ensure that
  // we can generate a correct safepoint even in the presence of a branch
  // trampoline between emitting the call and recording the safepoint.
  int pc_offset_for_safepoint_ = -1;

  // InstructionStream emission.
  inline void CheckBuffer();
  void GrowBuffer();
  void emit(Instr x) override;
  void emit(ShortInstr x) override;
  template <typename T>
  inline void EmitHelper(T x, bool disassemble);

  inline void DisassembleInstruction(uint8_t* pc);
  static void DisassembleInstructionHelper(uint8_t* pc);

  // Labels.
  void print(const Label* L);
  void bind_to(Label* L, int pos);
  void next(Label* L, bool is_internal);

  // One trampoline consists of:
  // - space for trampoline slots,
  // - space for labels.
  //
  // Space for trampoline slots is equal to slot_count * 2 * kInstrSize.
  // Space for trampoline slots precedes space for labels. Each label is of one
  // instruction size, so total amount for labels is equal to
  // label_count *  kInstrSize.
  class Trampoline {
   public:
    Trampoline() {
      start_ = 0;
      next_slot_ = 0;
      free_slot_count_ = 0;
      end_ = 0;
    }

    Trampoline(int start, int slot_count) {
      start_ = start;
      next_slot_ = start;
      free_slot_count_ = slot_count;
      end_ = start + slot_count * kTrampolineSlotsSize;
    }

    int start() const { return start_; }
    int end() const { return end_; }

    int take_slot() {
      if (free_slot_count_ <= 0) return kInvalidSlotPos;
      int trampoline_slot = next_slot_;
      free_slot_count_--;
      next_slot_ += kTrampolineSlotsSize;
      DEBUG_PRINTF("\ttrampoline slot %d next %d free %d\n", trampoline_slot,
                   next_slot_, free_slot_count_)
      return trampoline_slot;
    }

   private:
    int start_;
    int end_;
    int next_slot_;
    int free_slot_count_;
  };

  static constexpr int kInvalidSlotPos = -1;
  Trampoline trampoline_;

  int unbound_labels_count_ = 0;
  int trampoline_check_;  // The pc offset of next trampoline pool check.

  void CheckTrampolinePool();
  inline void CheckTrampolinePoolQuick(int margin);
  int32_t GetTrampolineEntry(int32_t pos);

  // We keep track of the position of all internal reference uses of labels,
  // so we can distinguish the use site from other kinds of uses. The other
  // uses can be recognized by looking at the generated code at the position,
  // but internal references are just data (like jump table entries), so we
  // need something extra to tell them apart from other kinds of uses.
  std::set<intptr_t> internal_reference_positions_;
  bool is_internal_reference(Label* L) const {
    DCHECK(L->is_linked());
    return internal_reference_positions_.contains(L->pos());
  }

  RegList scratch_register_list_;
  DoubleRegList scratch_double_register_list_;
  ConstantPool constpool_;

  void PatchInHeapNumberRequest(Address pc, Handle<HeapNumber> object) override;

  int WriteCodeComments();

  friend class EnsureSpace;
  friend class ConstantPool;
  friend class RelocInfo;
  friend class RegExpMacroAssemblerRISCV;
};

class EnsureSpace {
 public:
  explicit inline EnsureSpace(Assembler* assembler);
};

// This scope utility allows scratch registers to be managed safely. The
// Assembler's {GetScratchRegisterList()}/{GetScratchDoubleRegisterList()}
// are used as pools of general-purpose/double scratch registers.
// These registers can be allocated on demand, and will be returned
// at the end of the scope.
//
// When the scope ends, the Assembler's lists will be restored to their original
// states, even if the lists are modified by some other means. Note that this
// scope can be nested but the destructors need to run in the opposite order as
// the constructors. We do not have assertions for this.
class V8_EXPORT_PRIVATE UseScratchRegisterScope {
 public:
  explicit UseScratchRegisterScope(Assembler* assembler)
      : assembler_(assembler),
        old_available_(*assembler->GetScratchRegisterList()),
        old_available_double_(*assembler->GetScratchDoubleRegisterList()) {}

  ~UseScratchRegisterScope() {
    RegList* available = assembler_->GetScratchRegisterList();
    DoubleRegList* available_double =
        assembler_->GetScratchDoubleRegisterList();
    *available = old_available_;
    *available_double = old_available_double_;
  }

  Register Acquire() {
    RegList* available = assembler_->GetScratchRegisterList();
    return available->PopFirst();
  }

  DoubleRegister AcquireDouble() {
    DoubleRegList* available_double =
        assembler_->GetScratchDoubleRegisterList();
    return available_double->PopFirst();
  }

  // Check if we have registers available to acquire.
  bool CanAcquire() const {
    RegList* available = assembler_->GetScratchRegisterList();
    return !available->is_empty();
  }

  void Include(const Register& reg1, const Register& reg2) {
    Include(reg1);
    Include(reg2);
  }
  void Include(const Register& reg) {
    DCHECK_NE(reg, no_reg);
    RegList* available = assembler_->GetScratchRegisterList();
    DCHECK_NOT_NULL(available);
    DCHECK(!available->has(reg));
    available->set(reg);
  }
  void Include(RegList list) {
    RegList* available = assembler_->GetScratchRegisterList();
    DCHECK_NOT_NULL(available);
    *available = *available | list;
  }
  void Exclude(const RegList& list) {
    RegList* available = assembler_->GetScratchRegisterList();
    DCHECK_NOT_NULL(available);
    available->clear(list);
  }
  void Exclude(const Register& reg1, const Register& reg2) {
    Exclude(reg1);
    Exclude(reg2);
  }
  void Exclude(const Register& reg) {
    DCHECK_NE(reg, no_reg);
    RegList list({reg});
    Exclude(list);
  }

  void Include(DoubleRegList list) {
    DoubleRegList* available_double =
        assembler_->GetScratchDoubleRegisterList();
    DCHECK_NOT_NULL(available_double);
    DCHECK_EQ((*available_double & list).bits(), 0x0);
    *available_double = *available_double | list;
  }

  RegList Available() { return *assembler_->GetScratchRegisterList(); }
  void SetAvailable(RegList available) {
    *assembler_->GetScratchRegisterList() = available;
  }
  DoubleRegList AvailableDouble() {
    return *assembler_->GetScratchDoubleRegisterList();
  }
  void SetAvailableDouble(DoubleRegList available_double) {
    *assembler_->GetScratchDoubleRegisterList() = available_double;
  }

 private:
  friend class Assembler;
  friend class MacroAssembler;

  Assembler* assembler_;
  RegList old_available_;
  DoubleRegList old_available_double_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_RISCV_ASSEMBLER_RISCV_H_
