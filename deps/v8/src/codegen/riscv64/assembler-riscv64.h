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

#ifndef V8_CODEGEN_RISCV64_ASSEMBLER_RISCV64_H_
#define V8_CODEGEN_RISCV64_ASSEMBLER_RISCV64_H_

#include <stdio.h>

#include <memory>
#include <set>

#include "src/codegen/assembler.h"
#include "src/codegen/constant-pool.h"
#include "src/codegen/external-reference.h"
#include "src/codegen/label.h"
#include "src/codegen/riscv64/constants-riscv64.h"
#include "src/codegen/riscv64/register-riscv64.h"
#include "src/objects/contexts.h"
#include "src/objects/smi.h"

namespace v8 {
namespace internal {

#define DEBUG_PRINTF(...) \
  if (FLAG_debug_riscv) { \
    printf(__VA_ARGS__);  \
  }

class SafepointTableBuilder;

// -----------------------------------------------------------------------------
// Machine instruction Operands.
constexpr int kSmiShift = kSmiTagSize + kSmiShiftSize;
constexpr uint64_t kSmiShiftMask = (1UL << kSmiShift) - 1;
// Class Operand represents a shifter operand in data processing instructions.
class Operand {
 public:
  // Immediate.
  V8_INLINE explicit Operand(int64_t immediate,
                             RelocInfo::Mode rmode = RelocInfo::NONE)
      : rm_(no_reg), rmode_(rmode) {
    value_.immediate = immediate;
  }
  V8_INLINE explicit Operand(const ExternalReference& f)
      : rm_(no_reg), rmode_(RelocInfo::EXTERNAL_REFERENCE) {
    value_.immediate = static_cast<int64_t>(f.address());
  }
  V8_INLINE explicit Operand(const char* s);
  explicit Operand(Handle<HeapObject> handle);
  V8_INLINE explicit Operand(Smi value) : rm_(no_reg), rmode_(RelocInfo::NONE) {
    value_.immediate = static_cast<intptr_t>(value.ptr());
  }

  static Operand EmbeddedNumber(double number);  // Smi or HeapNumber.
  static Operand EmbeddedStringConstant(const StringConstantBase* str);

  // Register.
  V8_INLINE explicit Operand(Register rm) : rm_(rm) {}

  // Return true if this is a register operand.
  V8_INLINE bool is_reg() const;

  inline int64_t immediate() const;

  bool IsImmediate() const { return !rm_.is_valid(); }

  HeapObjectRequest heap_object_request() const {
    DCHECK(IsHeapObjectRequest());
    return value_.heap_object_request;
  }

  bool IsHeapObjectRequest() const {
    DCHECK_IMPLIES(is_heap_object_request_, IsImmediate());
    DCHECK_IMPLIES(is_heap_object_request_,
                   rmode_ == RelocInfo::FULL_EMBEDDED_OBJECT ||
                       rmode_ == RelocInfo::CODE_TARGET);
    return is_heap_object_request_;
  }

  Register rm() const { return rm_; }

  RelocInfo::Mode rmode() const { return rmode_; }

 private:
  Register rm_;
  union Value {
    Value() {}
    HeapObjectRequest heap_object_request;  // if is_heap_object_request_
    int64_t immediate;                      // otherwise
  } value_;                                 // valid if rm_ == no_reg
  bool is_heap_object_request_ = false;
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

  bool OffsetIsInt12Encodable() const { return is_int12(offset_); }

 private:
  int32_t offset_;

  friend class Assembler;
};

class V8_EXPORT_PRIVATE Assembler : public AssemblerBase {
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

  virtual ~Assembler() { CHECK(constpool_.IsEmpty()); }

  // GetCode emits any pending (non-emitted) code and fills the descriptor desc.
  static constexpr int kNoHandlerTable = 0;
  static constexpr SafepointTableBuilder* kNoSafepointTable = nullptr;
  void GetCode(Isolate* isolate, CodeDesc* desc,
               SafepointTableBuilder* safepoint_table_builder,
               int handler_table_offset);

  // Convenience wrapper for code without safepoint or handler tables.
  void GetCode(Isolate* isolate, CodeDesc* desc) {
    GetCode(isolate, desc, kNoSafepointTable, kNoHandlerTable);
  }

  // Unused on this architecture.
  void MaybeEmitOutOfLineConstantPool() {}

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

  enum OffsetSize : int {
    kOffset21 = 21,  // RISCV jal
    kOffset12 = 12,  // RISCV imm12
    kOffset20 = 20,  // RISCV imm20
    kOffset13 = 13,  // RISCV branch
    kOffset32 = 32,  // RISCV auipc + instr_I
    kOffset11 = 11   // RISCV C_J
  };

  // Determines if Label is bound and near enough so that branch instruction
  // can be used to reach it, instead of jump instruction.
  bool is_near(Label* L);
  bool is_near(Label* L, OffsetSize bits);
  bool is_near_branch(Label* L);

  // Get offset from instr.
  int BranchOffset(Instr instr);
  int BrachlongOffset(Instr auipc, Instr jalr);
  int JumpOffset(Instr instr);
  int CJumpOffset(Instr instr);
  static int LdOffset(Instr instr);
  static int AuipcOffset(Instr instr);

  // Returns the branch offset to the given label from the current code
  // position. Links the label to the current position if it is still unbound.
  // Manages the jump elimination optimization if the second parameter is true.
  int32_t branch_offset_helper(Label* L, OffsetSize bits);
  inline int32_t branch_offset(Label* L) {
    return branch_offset_helper(L, OffsetSize::kOffset13);
  }
  inline int32_t jump_offset(Label* L) {
    return branch_offset_helper(L, OffsetSize::kOffset21);
  }
  inline int16_t cjump_offset(Label* L) {
    return (int16_t)branch_offset_helper(L, OffsetSize::kOffset11);
  }

  uint64_t jump_address(Label* L);
  uint64_t branch_long_offset(Label* L);

  // Puts a labels target address at the given position.
  // The high 8 bits are set to zero.
  void label_at_put(Label* L, int at_offset);

  // Read/Modify the code target address in the branch/call instruction at pc.
  // The isolate argument is unused (and may be nullptr) when skipping flushing.
  static Address target_address_at(Address pc);
  V8_INLINE static void set_target_address_at(
      Address pc, Address target,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED) {
    set_target_value_at(pc, target, icache_flush_mode);
  }

  static Address target_address_at(Address pc, Address constant_pool);

  static void set_target_address_at(
      Address pc, Address constant_pool, Address target,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED);

  static bool IsConstantPoolAt(Instruction* instr);
  static int ConstantPoolSizeAt(Instruction* instr);
  // See Assembler::CheckConstPool for more info.
  void EmitPoolGuard();

  static void set_target_value_at(
      Address pc, uint64_t target,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED);

  static void JumpLabelToJumpRegister(Address pc);

  // This sets the branch destination (which gets loaded at the call address).
  // This is for calls and branches within generated code.  The serializer
  // has already deserialized the lui/ori instructions etc.
  inline static void deserialization_set_special_target_at(
      Address instruction_payload, Code code, Address target);

  // Get the size of the special target encoded at 'instruction_payload'.
  inline static int deserialization_special_target_size(
      Address instruction_payload);

  // This sets the internal reference at the pc.
  inline static void deserialization_set_target_internal_reference_at(
      Address pc, Address target,
      RelocInfo::Mode mode = RelocInfo::INTERNAL_REFERENCE);

  // Difference between address of current opcode and target address offset.
  static constexpr int kBranchPCOffset = kInstrSize;

  // Difference between address of current opcode and target address offset,
  // when we are generatinga sequence of instructions for long relative PC
  // branches
  static constexpr int kLongBranchPCOffset = 3 * kInstrSize;

  // Adjust ra register in branch delay slot of bal instruction so to skip
  // instructions not needed after optimization of PIC in
  // TurboAssembler::BranchAndLink method.

  static constexpr int kOptimizedBranchAndLinkLongReturnOffset = 4 * kInstrSize;

  // Here we are patching the address in the LUI/ADDI instruction pair.
  // These values are used in the serialization process and must be zero for
  // RISC-V platform, as Code, Embedded Object or External-reference pointers
  // are split across two consecutive instructions and don't exist separately
  // in the code, so the serializer should not step forwards in memory after
  // a target is resolved and written.
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

  // Max offset for b instructions with 12-bit offset field (multiple of 2)
  static constexpr int kMaxBranchOffset = (1 << (13 - 1)) - 1;

  // Max offset for jal instruction with 20-bit offset field (multiple of 2)
  static constexpr int kMaxJumpOffset = (1 << (21 - 1)) - 1;

  static constexpr int kTrampolineSlotsSize = 2 * kInstrSize;

  RegList* GetScratchRegisterList() { return &scratch_register_list_; }

  // ---------------------------------------------------------------------------
  // Code generation.

  // Insert the smallest number of nop instructions
  // possible to align the pc offset to a multiple
  // of m. m must be a power of 2 (>= 4).
  void Align(int m);
  // Insert the smallest number of zero bytes possible to align the pc offset
  // to a mulitple of m. m must be a power of 2 (>= 2).
  void DataAlign(int m);
  // Aligns code to something that's optimal for a jump target for the platform.
  void CodeTargetAlign();

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

  // RISC-V Instructions Emited to a buffer

  void lui(Register rd, int32_t imm20);
  void auipc(Register rd, int32_t imm20);

  // Jumps
  void jal(Register rd, int32_t imm20);
  void jalr(Register rd, Register rs1, int16_t imm12);

  // Branches
  void beq(Register rs1, Register rs2, int16_t imm12);
  inline void beq(Register rs1, Register rs2, Label* L) {
    beq(rs1, rs2, branch_offset(L));
  }
  void bne(Register rs1, Register rs2, int16_t imm12);
  inline void bne(Register rs1, Register rs2, Label* L) {
    bne(rs1, rs2, branch_offset(L));
  }
  void blt(Register rs1, Register rs2, int16_t imm12);
  inline void blt(Register rs1, Register rs2, Label* L) {
    blt(rs1, rs2, branch_offset(L));
  }
  void bge(Register rs1, Register rs2, int16_t imm12);
  inline void bge(Register rs1, Register rs2, Label* L) {
    bge(rs1, rs2, branch_offset(L));
  }
  void bltu(Register rs1, Register rs2, int16_t imm12);
  inline void bltu(Register rs1, Register rs2, Label* L) {
    bltu(rs1, rs2, branch_offset(L));
  }
  void bgeu(Register rs1, Register rs2, int16_t imm12);
  inline void bgeu(Register rs1, Register rs2, Label* L) {
    bgeu(rs1, rs2, branch_offset(L));
  }

  // Loads
  void lb(Register rd, Register rs1, int16_t imm12);
  void lh(Register rd, Register rs1, int16_t imm12);
  void lw(Register rd, Register rs1, int16_t imm12);
  void lbu(Register rd, Register rs1, int16_t imm12);
  void lhu(Register rd, Register rs1, int16_t imm12);

  // Stores
  void sb(Register source, Register base, int16_t imm12);
  void sh(Register source, Register base, int16_t imm12);
  void sw(Register source, Register base, int16_t imm12);

  // Arithmetic with immediate
  void addi(Register rd, Register rs1, int16_t imm12);
  void slti(Register rd, Register rs1, int16_t imm12);
  void sltiu(Register rd, Register rs1, int16_t imm12);
  void xori(Register rd, Register rs1, int16_t imm12);
  void ori(Register rd, Register rs1, int16_t imm12);
  void andi(Register rd, Register rs1, int16_t imm12);
  void slli(Register rd, Register rs1, uint8_t shamt);
  void srli(Register rd, Register rs1, uint8_t shamt);
  void srai(Register rd, Register rs1, uint8_t shamt);

  // Arithmetic
  void add(Register rd, Register rs1, Register rs2);
  void sub(Register rd, Register rs1, Register rs2);
  void sll(Register rd, Register rs1, Register rs2);
  void slt(Register rd, Register rs1, Register rs2);
  void sltu(Register rd, Register rs1, Register rs2);
  void xor_(Register rd, Register rs1, Register rs2);
  void srl(Register rd, Register rs1, Register rs2);
  void sra(Register rd, Register rs1, Register rs2);
  void or_(Register rd, Register rs1, Register rs2);
  void and_(Register rd, Register rs1, Register rs2);

  // Memory fences
  void fence(uint8_t pred, uint8_t succ);
  void fence_tso();

  // Environment call / break
  void ecall();
  void ebreak();

  // This is a de facto standard (as set by GNU binutils) 32-bit unimplemented
  // instruction (i.e., it should always trap, if your implementation has
  // invalid instruction traps).
  void unimp();

  // CSR
  void csrrw(Register rd, ControlStatusReg csr, Register rs1);
  void csrrs(Register rd, ControlStatusReg csr, Register rs1);
  void csrrc(Register rd, ControlStatusReg csr, Register rs1);
  void csrrwi(Register rd, ControlStatusReg csr, uint8_t imm5);
  void csrrsi(Register rd, ControlStatusReg csr, uint8_t imm5);
  void csrrci(Register rd, ControlStatusReg csr, uint8_t imm5);

  // RV64I
  void lwu(Register rd, Register rs1, int16_t imm12);
  void ld(Register rd, Register rs1, int16_t imm12);
  void sd(Register source, Register base, int16_t imm12);
  void addiw(Register rd, Register rs1, int16_t imm12);
  void slliw(Register rd, Register rs1, uint8_t shamt);
  void srliw(Register rd, Register rs1, uint8_t shamt);
  void sraiw(Register rd, Register rs1, uint8_t shamt);
  void addw(Register rd, Register rs1, Register rs2);
  void subw(Register rd, Register rs1, Register rs2);
  void sllw(Register rd, Register rs1, Register rs2);
  void srlw(Register rd, Register rs1, Register rs2);
  void sraw(Register rd, Register rs1, Register rs2);

  // RV32M Standard Extension
  void mul(Register rd, Register rs1, Register rs2);
  void mulh(Register rd, Register rs1, Register rs2);
  void mulhsu(Register rd, Register rs1, Register rs2);
  void mulhu(Register rd, Register rs1, Register rs2);
  void div(Register rd, Register rs1, Register rs2);
  void divu(Register rd, Register rs1, Register rs2);
  void rem(Register rd, Register rs1, Register rs2);
  void remu(Register rd, Register rs1, Register rs2);

  // RV64M Standard Extension (in addition to RV32M)
  void mulw(Register rd, Register rs1, Register rs2);
  void divw(Register rd, Register rs1, Register rs2);
  void divuw(Register rd, Register rs1, Register rs2);
  void remw(Register rd, Register rs1, Register rs2);
  void remuw(Register rd, Register rs1, Register rs2);

  // RV32A Standard Extension
  void lr_w(bool aq, bool rl, Register rd, Register rs1);
  void sc_w(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amoswap_w(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amoadd_w(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amoxor_w(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amoand_w(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amoor_w(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amomin_w(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amomax_w(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amominu_w(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amomaxu_w(bool aq, bool rl, Register rd, Register rs1, Register rs2);

  // RV64A Standard Extension (in addition to RV32A)
  void lr_d(bool aq, bool rl, Register rd, Register rs1);
  void sc_d(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amoswap_d(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amoadd_d(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amoxor_d(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amoand_d(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amoor_d(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amomin_d(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amomax_d(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amominu_d(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amomaxu_d(bool aq, bool rl, Register rd, Register rs1, Register rs2);

  // RV32F Standard Extension
  void flw(FPURegister rd, Register rs1, int16_t imm12);
  void fsw(FPURegister source, Register base, int16_t imm12);
  void fmadd_s(FPURegister rd, FPURegister rs1, FPURegister rs2,
               FPURegister rs3, RoundingMode frm = RNE);
  void fmsub_s(FPURegister rd, FPURegister rs1, FPURegister rs2,
               FPURegister rs3, RoundingMode frm = RNE);
  void fnmsub_s(FPURegister rd, FPURegister rs1, FPURegister rs2,
                FPURegister rs3, RoundingMode frm = RNE);
  void fnmadd_s(FPURegister rd, FPURegister rs1, FPURegister rs2,
                FPURegister rs3, RoundingMode frm = RNE);
  void fadd_s(FPURegister rd, FPURegister rs1, FPURegister rs2,
              RoundingMode frm = RNE);
  void fsub_s(FPURegister rd, FPURegister rs1, FPURegister rs2,
              RoundingMode frm = RNE);
  void fmul_s(FPURegister rd, FPURegister rs1, FPURegister rs2,
              RoundingMode frm = RNE);
  void fdiv_s(FPURegister rd, FPURegister rs1, FPURegister rs2,
              RoundingMode frm = RNE);
  void fsqrt_s(FPURegister rd, FPURegister rs1, RoundingMode frm = RNE);
  void fsgnj_s(FPURegister rd, FPURegister rs1, FPURegister rs2);
  void fsgnjn_s(FPURegister rd, FPURegister rs1, FPURegister rs2);
  void fsgnjx_s(FPURegister rd, FPURegister rs1, FPURegister rs2);
  void fmin_s(FPURegister rd, FPURegister rs1, FPURegister rs2);
  void fmax_s(FPURegister rd, FPURegister rs1, FPURegister rs2);
  void fcvt_w_s(Register rd, FPURegister rs1, RoundingMode frm = RNE);
  void fcvt_wu_s(Register rd, FPURegister rs1, RoundingMode frm = RNE);
  void fmv_x_w(Register rd, FPURegister rs1);
  void feq_s(Register rd, FPURegister rs1, FPURegister rs2);
  void flt_s(Register rd, FPURegister rs1, FPURegister rs2);
  void fle_s(Register rd, FPURegister rs1, FPURegister rs2);
  void fclass_s(Register rd, FPURegister rs1);
  void fcvt_s_w(FPURegister rd, Register rs1, RoundingMode frm = RNE);
  void fcvt_s_wu(FPURegister rd, Register rs1, RoundingMode frm = RNE);
  void fmv_w_x(FPURegister rd, Register rs1);

  // RV64F Standard Extension (in addition to RV32F)
  void fcvt_l_s(Register rd, FPURegister rs1, RoundingMode frm = RNE);
  void fcvt_lu_s(Register rd, FPURegister rs1, RoundingMode frm = RNE);
  void fcvt_s_l(FPURegister rd, Register rs1, RoundingMode frm = RNE);
  void fcvt_s_lu(FPURegister rd, Register rs1, RoundingMode frm = RNE);

  // RV32D Standard Extension
  void fld(FPURegister rd, Register rs1, int16_t imm12);
  void fsd(FPURegister source, Register base, int16_t imm12);
  void fmadd_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
               FPURegister rs3, RoundingMode frm = RNE);
  void fmsub_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
               FPURegister rs3, RoundingMode frm = RNE);
  void fnmsub_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
                FPURegister rs3, RoundingMode frm = RNE);
  void fnmadd_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
                FPURegister rs3, RoundingMode frm = RNE);
  void fadd_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
              RoundingMode frm = RNE);
  void fsub_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
              RoundingMode frm = RNE);
  void fmul_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
              RoundingMode frm = RNE);
  void fdiv_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
              RoundingMode frm = RNE);
  void fsqrt_d(FPURegister rd, FPURegister rs1, RoundingMode frm = RNE);
  void fsgnj_d(FPURegister rd, FPURegister rs1, FPURegister rs2);
  void fsgnjn_d(FPURegister rd, FPURegister rs1, FPURegister rs2);
  void fsgnjx_d(FPURegister rd, FPURegister rs1, FPURegister rs2);
  void fmin_d(FPURegister rd, FPURegister rs1, FPURegister rs2);
  void fmax_d(FPURegister rd, FPURegister rs1, FPURegister rs2);
  void fcvt_s_d(FPURegister rd, FPURegister rs1, RoundingMode frm = RNE);
  void fcvt_d_s(FPURegister rd, FPURegister rs1, RoundingMode frm = RNE);
  void feq_d(Register rd, FPURegister rs1, FPURegister rs2);
  void flt_d(Register rd, FPURegister rs1, FPURegister rs2);
  void fle_d(Register rd, FPURegister rs1, FPURegister rs2);
  void fclass_d(Register rd, FPURegister rs1);
  void fcvt_w_d(Register rd, FPURegister rs1, RoundingMode frm = RNE);
  void fcvt_wu_d(Register rd, FPURegister rs1, RoundingMode frm = RNE);
  void fcvt_d_w(FPURegister rd, Register rs1, RoundingMode frm = RNE);
  void fcvt_d_wu(FPURegister rd, Register rs1, RoundingMode frm = RNE);

  // RV64D Standard Extension (in addition to RV32D)
  void fcvt_l_d(Register rd, FPURegister rs1, RoundingMode frm = RNE);
  void fcvt_lu_d(Register rd, FPURegister rs1, RoundingMode frm = RNE);
  void fmv_x_d(Register rd, FPURegister rs1);
  void fcvt_d_l(FPURegister rd, Register rs1, RoundingMode frm = RNE);
  void fcvt_d_lu(FPURegister rd, Register rs1, RoundingMode frm = RNE);
  void fmv_d_x(FPURegister rd, Register rs1);

  // RV64C Standard Extension
  void c_nop();
  void c_addi(Register rd, int8_t imm6);
  void c_addiw(Register rd, int8_t imm6);
  void c_addi16sp(int16_t imm10);
  void c_addi4spn(Register rd, int16_t uimm10);
  void c_li(Register rd, int8_t imm6);
  void c_lui(Register rd, int8_t imm6);
  void c_slli(Register rd, uint8_t uimm6);
  void c_fldsp(FPURegister rd, uint16_t uimm9);
  void c_lwsp(Register rd, uint16_t uimm8);
  void c_ldsp(Register rd, uint16_t uimm9);
  void c_jr(Register rs1);
  void c_mv(Register rd, Register rs2);
  void c_ebreak();
  void c_jalr(Register rs1);
  void c_j(int16_t imm12);
  inline void c_j(Label* L) { c_j(cjump_offset(L)); }
  void c_add(Register rd, Register rs2);
  void c_sub(Register rd, Register rs2);
  void c_and(Register rd, Register rs2);
  void c_xor(Register rd, Register rs2);
  void c_or(Register rd, Register rs2);
  void c_subw(Register rd, Register rs2);
  void c_addw(Register rd, Register rs2);
  void c_swsp(Register rs2, uint16_t uimm8);
  void c_sdsp(Register rs2, uint16_t uimm9);
  void c_fsdsp(FPURegister rs2, uint16_t uimm9);
  void c_lw(Register rd, Register rs1, uint16_t uimm7);
  void c_ld(Register rd, Register rs1, uint16_t uimm8);
  void c_fld(FPURegister rd, Register rs1, uint16_t uimm8);
  void c_sw(Register rs2, Register rs1, uint16_t uimm7);
  void c_sd(Register rs2, Register rs1, uint16_t uimm8);
  void c_fsd(FPURegister rs2, Register rs1, uint16_t uimm8);

  // Privileged
  void uret();
  void sret();
  void mret();
  void wfi();
  void sfence_vma(Register rs1, Register rs2);

  // Assembler Pseudo Instructions (Tables 25.2, 25.3, RISC-V Unprivileged ISA)
  void nop();
  void RV_li(Register rd, int64_t imm);
  // Returns the number of instructions required to load the immediate
  static int li_estimate(int64_t imm, bool is_get_temp_reg = false);
  // Loads an immediate, always using 8 instructions, regardless of the value,
  // so that it can be modified later.
  void li_constant(Register rd, int64_t imm);
  void li_ptr(Register rd, int64_t imm);

  void mv(Register rd, Register rs) { addi(rd, rs, 0); }
  void not_(Register rd, Register rs) { xori(rd, rs, -1); }
  void neg(Register rd, Register rs) { sub(rd, zero_reg, rs); }
  void negw(Register rd, Register rs) { subw(rd, zero_reg, rs); }
  void sext_w(Register rd, Register rs) { addiw(rd, rs, 0); }
  void seqz(Register rd, Register rs) { sltiu(rd, rs, 1); }
  void snez(Register rd, Register rs) { sltu(rd, zero_reg, rs); }
  void sltz(Register rd, Register rs) { slt(rd, rs, zero_reg); }
  void sgtz(Register rd, Register rs) { slt(rd, zero_reg, rs); }

  void fmv_s(FPURegister rd, FPURegister rs) { fsgnj_s(rd, rs, rs); }
  void fabs_s(FPURegister rd, FPURegister rs) { fsgnjx_s(rd, rs, rs); }
  void fneg_s(FPURegister rd, FPURegister rs) { fsgnjn_s(rd, rs, rs); }
  void fmv_d(FPURegister rd, FPURegister rs) { fsgnj_d(rd, rs, rs); }
  void fabs_d(FPURegister rd, FPURegister rs) { fsgnjx_d(rd, rs, rs); }
  void fneg_d(FPURegister rd, FPURegister rs) { fsgnjn_d(rd, rs, rs); }

  void beqz(Register rs, int16_t imm13) { beq(rs, zero_reg, imm13); }
  inline void beqz(Register rs1, Label* L) { beqz(rs1, branch_offset(L)); }
  void bnez(Register rs, int16_t imm13) { bne(rs, zero_reg, imm13); }
  inline void bnez(Register rs1, Label* L) { bnez(rs1, branch_offset(L)); }
  void blez(Register rs, int16_t imm13) { bge(zero_reg, rs, imm13); }
  inline void blez(Register rs1, Label* L) { blez(rs1, branch_offset(L)); }
  void bgez(Register rs, int16_t imm13) { bge(rs, zero_reg, imm13); }
  inline void bgez(Register rs1, Label* L) { bgez(rs1, branch_offset(L)); }
  void bltz(Register rs, int16_t imm13) { blt(rs, zero_reg, imm13); }
  inline void bltz(Register rs1, Label* L) { bltz(rs1, branch_offset(L)); }
  void bgtz(Register rs, int16_t imm13) { blt(zero_reg, rs, imm13); }

  inline void bgtz(Register rs1, Label* L) { bgtz(rs1, branch_offset(L)); }
  void bgt(Register rs1, Register rs2, int16_t imm13) { blt(rs2, rs1, imm13); }
  inline void bgt(Register rs1, Register rs2, Label* L) {
    bgt(rs1, rs2, branch_offset(L));
  }
  void ble(Register rs1, Register rs2, int16_t imm13) { bge(rs2, rs1, imm13); }
  inline void ble(Register rs1, Register rs2, Label* L) {
    ble(rs1, rs2, branch_offset(L));
  }
  void bgtu(Register rs1, Register rs2, int16_t imm13) {
    bltu(rs2, rs1, imm13);
  }
  inline void bgtu(Register rs1, Register rs2, Label* L) {
    bgtu(rs1, rs2, branch_offset(L));
  }
  void bleu(Register rs1, Register rs2, int16_t imm13) {
    bgeu(rs2, rs1, imm13);
  }
  inline void bleu(Register rs1, Register rs2, Label* L) {
    bleu(rs1, rs2, branch_offset(L));
  }

  void j(int32_t imm21) { jal(zero_reg, imm21); }
  inline void j(Label* L) { j(jump_offset(L)); }
  inline void b(Label* L) { j(L); }
  void jal(int32_t imm21) { jal(ra, imm21); }
  inline void jal(Label* L) { jal(jump_offset(L)); }
  void jr(Register rs) { jalr(zero_reg, rs, 0); }
  void jr(Register rs, int32_t imm12) { jalr(zero_reg, rs, imm12); }
  void jalr(Register rs, int32_t imm12) { jalr(ra, rs, imm12); }
  void jalr(Register rs) { jalr(ra, rs, 0); }
  void ret() { jalr(zero_reg, ra, 0); }
  void call(int32_t offset) {
    auipc(ra, (offset >> 12) + ((offset & 0x800) >> 11));
    jalr(ra, ra, offset << 20 >> 20);
  }

  // Read instructions-retired counter
  void rdinstret(Register rd) { csrrs(rd, csr_instret, zero_reg); }
  void rdinstreth(Register rd) { csrrs(rd, csr_instreth, zero_reg); }
  void rdcycle(Register rd) { csrrs(rd, csr_cycle, zero_reg); }
  void rdcycleh(Register rd) { csrrs(rd, csr_cycleh, zero_reg); }
  void rdtime(Register rd) { csrrs(rd, csr_time, zero_reg); }
  void rdtimeh(Register rd) { csrrs(rd, csr_timeh, zero_reg); }

  void csrr(Register rd, ControlStatusReg csr) { csrrs(rd, csr, zero_reg); }
  void csrw(ControlStatusReg csr, Register rs) { csrrw(zero_reg, csr, rs); }
  void csrs(ControlStatusReg csr, Register rs) { csrrs(zero_reg, csr, rs); }
  void csrc(ControlStatusReg csr, Register rs) { csrrc(zero_reg, csr, rs); }

  void csrwi(ControlStatusReg csr, uint8_t imm) { csrrwi(zero_reg, csr, imm); }
  void csrsi(ControlStatusReg csr, uint8_t imm) { csrrsi(zero_reg, csr, imm); }
  void csrci(ControlStatusReg csr, uint8_t imm) { csrrci(zero_reg, csr, imm); }

  void frcsr(Register rd) { csrrs(rd, csr_fcsr, zero_reg); }
  void fscsr(Register rd, Register rs) { csrrw(rd, csr_fcsr, rs); }
  void fscsr(Register rs) { csrrw(zero_reg, csr_fcsr, rs); }

  void frrm(Register rd) { csrrs(rd, csr_frm, zero_reg); }
  void fsrm(Register rd, Register rs) { csrrw(rd, csr_frm, rs); }
  void fsrm(Register rs) { csrrw(zero_reg, csr_frm, rs); }

  void frflags(Register rd) { csrrs(rd, csr_fflags, zero_reg); }
  void fsflags(Register rd, Register rs) { csrrw(rd, csr_fflags, rs); }
  void fsflags(Register rs) { csrrw(zero_reg, csr_fflags, rs); }

  // Other pseudo instructions that are not part of RISCV pseudo assemly
  void nor(Register rd, Register rs, Register rt) {
    or_(rd, rs, rt);
    not_(rd, rd);
  }

  void sync() { fence(0b1111, 0b1111); }
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

  using BlockConstPoolScope = ConstantPool::BlockScope;
  // Class for scoping postponing the trampoline pool generation.
  class BlockTrampolinePoolScope {
   public:
    explicit BlockTrampolinePoolScope(Assembler* assem, int margin = 0)
        : assem_(assem) {
      assem_->StartBlockTrampolinePool();
    }

    explicit BlockTrampolinePoolScope(Assembler* assem, PoolEmissionCheck check)
        : assem_(assem) {
      assem_->StartBlockTrampolinePool();
    }
    ~BlockTrampolinePoolScope() { assem_->EndBlockTrampolinePool(); }

   private:
    Assembler* assem_;
    DISALLOW_IMPLICIT_CONSTRUCTORS(BlockTrampolinePoolScope);
  };

  // Class for postponing the assembly buffer growth. Typically used for
  // sequences of instructions that must be emitted as a unit, before
  // buffer growth (and relocation) can occur.
  // This blocking scope is not nestable.
  class BlockGrowBufferScope {
   public:
    explicit BlockGrowBufferScope(Assembler* assem) : assem_(assem) {
      assem_->StartBlockGrowBuffer();
    }
    ~BlockGrowBufferScope() { assem_->EndBlockGrowBuffer(); }

   private:
    Assembler* assem_;

    DISALLOW_IMPLICIT_CONSTRUCTORS(BlockGrowBufferScope);
  };

  // Record a deoptimization reason that can be used by a log or cpu profiler.
  // Use --trace-deopt to enable.
  void RecordDeoptReason(DeoptimizeReason reason, SourcePosition position,
                         int id);

  static int RelocateInternalReference(RelocInfo::Mode rmode, Address pc,
                                       intptr_t pc_delta);

  // Writes a single byte or word of data in the code stream.  Used for
  // inline tables, e.g., jump-tables.
  void db(uint8_t data);
  void dd(uint32_t data, RelocInfo::Mode rmode = RelocInfo::NONE);
  void dq(uint64_t data, RelocInfo::Mode rmode = RelocInfo::NONE);
  void dp(uintptr_t data, RelocInfo::Mode rmode = RelocInfo::NONE) {
    dq(data, rmode);
  }
  void dd(Label* label);

  Instruction* pc() const { return reinterpret_cast<Instruction*>(pc_); }

  // Postpone the generation of the trampoline pool for the specified number of
  // instructions.
  void BlockTrampolinePoolFor(int instructions);

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
  static void instr_at_put(Address pc, Instr instr) {
    *reinterpret_cast<Instr*>(pc) = instr;
  }
  Instr instr_at(int pos) {
    return *reinterpret_cast<Instr*>(buffer_start_ + pos);
  }
  void instr_at_put(int pos, Instr instr) {
    *reinterpret_cast<Instr*>(buffer_start_ + pos) = instr;
  }

  void instr_at_put(int pos, ShortInstr instr) {
    *reinterpret_cast<ShortInstr*>(buffer_start_ + pos) = instr;
  }

  Address toAddress(int pos) {
    return reinterpret_cast<Address>(buffer_start_ + pos);
  }

  // Check if an instruction is a branch of some kind.
  static bool IsBranch(Instr instr);
  static bool IsJump(Instr instr);
  static bool IsJal(Instr instr);
  static bool IsCJal(Instr instr);
  static bool IsJalr(Instr instr);
  static bool IsLui(Instr instr);
  static bool IsAuipc(Instr instr);
  static bool IsAddiw(Instr instr);
  static bool IsAddi(Instr instr);
  static bool IsOri(Instr instr);
  static bool IsSlli(Instr instr);
  static bool IsLd(Instr instr);
  void CheckTrampolinePool();

  inline int UnboundLabelsCount() { return unbound_labels_count_; }

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

  int64_t buffer_space() const { return reloc_info_writer.pos() - pc_; }

  // Decode branch instruction at pos and return branch target pos.
  int target_at(int pos, bool is_internal);

  // Patch branch instruction at pos to branch to given branch target pos.
  void target_at_put(int pos, int target_pos, bool is_internal);

  // Say if we need to relocate with this mode.
  bool MustUseReg(RelocInfo::Mode rmode);

  // Record reloc info for current pc_.
  void RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data = 0);

  // Block the emission of the trampoline pool before pc_offset.
  void BlockTrampolinePoolBefore(int pc_offset) {
    if (no_trampoline_pool_before_ < pc_offset)
      no_trampoline_pool_before_ = pc_offset;
  }

  void StartBlockTrampolinePool() {
    DEBUG_PRINTF("\tStartBlockTrampolinePool\n");
    trampoline_pool_blocked_nesting_++;
  }

  void EndBlockTrampolinePool() {
    trampoline_pool_blocked_nesting_--;
    DEBUG_PRINTF("\ttrampoline_pool_blocked_nesting:%d\n",
                 trampoline_pool_blocked_nesting_);
    if (trampoline_pool_blocked_nesting_ == 0) {
      CheckTrampolinePoolQuick(1);
    }
  }

  bool is_trampoline_pool_blocked() const {
    return trampoline_pool_blocked_nesting_ > 0;
  }

  bool has_exception() const { return internal_trampoline_exception_; }

  bool is_trampoline_emitted() const { return trampoline_emitted_; }

  // Temporarily block automatic assembly buffer growth.
  void StartBlockGrowBuffer() {
    DCHECK(!block_buffer_growth_);
    block_buffer_growth_ = true;
  }

  void EndBlockGrowBuffer() {
    DCHECK(block_buffer_growth_);
    block_buffer_growth_ = false;
  }

  bool is_buffer_growth_blocked() const { return block_buffer_growth_; }

  void CheckTrampolinePoolQuick(int extra_instructions = 0) {
    DEBUG_PRINTF("\tpc_offset:%d %d\n", pc_offset(),
                 next_buffer_check_ - extra_instructions * kInstrSize);
    if (pc_offset() >= next_buffer_check_ - extra_instructions * kInstrSize) {
      CheckTrampolinePool();
    }
  }

  using BlockPoolsScope = BlockTrampolinePoolScope;

  void RecordConstPool(int size);

  void ForceConstantPoolEmissionWithoutJump() {
    constpool_.Check(Emission::kForced, Jump::kOmitted);
  }
  void ForceConstantPoolEmissionWithJump() {
    constpool_.Check(Emission::kForced, Jump::kRequired);
  }
  // Check if the const pool needs to be emitted while pretending that {margin}
  // more bytes of instructions have already been emitted.
  void EmitConstPoolWithJumpIfNeeded(size_t margin = 0) {
    constpool_.Check(Emission::kIfNeeded, Jump::kRequired, margin);
  }

  void EmitConstPoolWithoutJumpIfNeeded(size_t margin = 0) {
    constpool_.Check(Emission::kIfNeeded, Jump::kOmitted, margin);
  }

  void RecordEntry(uint32_t data, RelocInfo::Mode rmode) {
    constpool_.RecordEntry(data, rmode);
  }

  void RecordEntry(uint64_t data, RelocInfo::Mode rmode) {
    constpool_.RecordEntry(data, rmode);
  }

 private:
  // Avoid overflows for displacements etc.
  static const int kMaximalBufferSize = 512 * MB;

  // Buffer size and constant pool distance are checked together at regular
  // intervals of kBufferCheckInterval emitted bytes.
  static constexpr int kBufferCheckInterval = 1 * KB / 2;

  // Code generation.
  // The relocation writer's position is at least kGap bytes below the end of
  // the generated instructions. This is so that multi-instruction sequences do
  // not have to check for overflow. The same is true for writes of large
  // relocation info entries.
  static constexpr int kGap = 64;
  STATIC_ASSERT(AssemblerBase::kMinimalBufferSize >= 2 * kGap);

  // Repeated checking whether the trampoline pool should be emitted is rather
  // expensive. By default we only check again once a number of instructions
  // has been generated.
  static constexpr int kCheckConstIntervalInst = 32;
  static constexpr int kCheckConstInterval =
      kCheckConstIntervalInst * kInstrSize;

  int next_buffer_check_;  // pc offset of next buffer check.

  // Emission of the trampoline pool may be blocked in some code sequences.
  int trampoline_pool_blocked_nesting_;  // Block emission if this is not zero.
  int no_trampoline_pool_before_;  // Block emission before this pc offset.

  // Keep track of the last emitted pool to guarantee a maximal distance.
  int last_trampoline_pool_end_;  // pc offset of the end of the last pool.

  // Automatic growth of the assembly buffer may be blocked for some sequences.
  bool block_buffer_growth_;  // Block growth when true.

  // Relocation information generation.
  // Each relocation is encoded as a variable size value.
  static constexpr int kMaxRelocSize = RelocInfoWriter::kMaxSize;
  RelocInfoWriter reloc_info_writer;

  // The bound position, before this we cannot do instruction elimination.
  int last_bound_pos_;

  // Code emission.
  inline void CheckBuffer();
  void GrowBuffer();
  inline void emit(Instr x);
  inline void emit(ShortInstr x);
  inline void emit(uint64_t x);
  template <typename T>
  inline void EmitHelper(T x);

  static void disassembleInstr(Instr instr);

  // Instruction generation.

  // ----- Top-level instruction formats match those in the ISA manual
  // (R, I, S, B, U, J). These match the formats defined in LLVM's
  // RISCVInstrFormats.td.
  void GenInstrR(uint8_t funct7, uint8_t funct3, Opcode opcode, Register rd,
                 Register rs1, Register rs2);
  void GenInstrR(uint8_t funct7, uint8_t funct3, Opcode opcode, FPURegister rd,
                 FPURegister rs1, FPURegister rs2);
  void GenInstrR(uint8_t funct7, uint8_t funct3, Opcode opcode, Register rd,
                 FPURegister rs1, Register rs2);
  void GenInstrR(uint8_t funct7, uint8_t funct3, Opcode opcode, FPURegister rd,
                 Register rs1, Register rs2);
  void GenInstrR(uint8_t funct7, uint8_t funct3, Opcode opcode, FPURegister rd,
                 FPURegister rs1, Register rs2);
  void GenInstrR(uint8_t funct7, uint8_t funct3, Opcode opcode, Register rd,
                 FPURegister rs1, FPURegister rs2);
  void GenInstrR4(uint8_t funct2, Opcode opcode, Register rd, Register rs1,
                  Register rs2, Register rs3, RoundingMode frm);
  void GenInstrR4(uint8_t funct2, Opcode opcode, FPURegister rd,
                  FPURegister rs1, FPURegister rs2, FPURegister rs3,
                  RoundingMode frm);
  void GenInstrRAtomic(uint8_t funct5, bool aq, bool rl, uint8_t funct3,
                       Register rd, Register rs1, Register rs2);
  void GenInstrRFrm(uint8_t funct7, Opcode opcode, Register rd, Register rs1,
                    Register rs2, RoundingMode frm);
  void GenInstrI(uint8_t funct3, Opcode opcode, Register rd, Register rs1,
                 int16_t imm12);
  void GenInstrI(uint8_t funct3, Opcode opcode, FPURegister rd, Register rs1,
                 int16_t imm12);
  void GenInstrIShift(bool arithshift, uint8_t funct3, Opcode opcode,
                      Register rd, Register rs1, uint8_t shamt);
  void GenInstrIShiftW(bool arithshift, uint8_t funct3, Opcode opcode,
                       Register rd, Register rs1, uint8_t shamt);
  void GenInstrS(uint8_t funct3, Opcode opcode, Register rs1, Register rs2,
                 int16_t imm12);
  void GenInstrS(uint8_t funct3, Opcode opcode, Register rs1, FPURegister rs2,
                 int16_t imm12);
  void GenInstrB(uint8_t funct3, Opcode opcode, Register rs1, Register rs2,
                 int16_t imm12);
  void GenInstrU(Opcode opcode, Register rd, int32_t imm20);
  void GenInstrJ(Opcode opcode, Register rd, int32_t imm20);
  void GenInstrCR(uint8_t funct4, Opcode opcode, Register rd, Register rs2);
  void GenInstrCA(uint8_t funct6, Opcode opcode, Register rd, uint8_t funct,
                  Register rs2);
  void GenInstrCI(uint8_t funct3, Opcode opcode, Register rd, int8_t imm6);
  void GenInstrCIU(uint8_t funct3, Opcode opcode, Register rd, uint8_t uimm6);
  void GenInstrCIU(uint8_t funct3, Opcode opcode, FPURegister rd,
                   uint8_t uimm6);
  void GenInstrCIW(uint8_t funct3, Opcode opcode, Register rd, uint8_t uimm8);
  void GenInstrCSS(uint8_t funct3, Opcode opcode, FPURegister rs2,
                   uint8_t uimm6);
  void GenInstrCSS(uint8_t funct3, Opcode opcode, Register rs2, uint8_t uimm6);
  void GenInstrCL(uint8_t funct3, Opcode opcode, Register rd, Register rs1,
                  uint8_t uimm5);
  void GenInstrCL(uint8_t funct3, Opcode opcode, FPURegister rd, Register rs1,
                  uint8_t uimm5);
  void GenInstrCS(uint8_t funct3, Opcode opcode, Register rs2, Register rs1,
                  uint8_t uimm5);
  void GenInstrCS(uint8_t funct3, Opcode opcode, FPURegister rs2, Register rs1,
                  uint8_t uimm5);
  void GenInstrCJ(uint8_t funct3, Opcode opcode, uint16_t uint11);

  // ----- Instruction class templates match those in LLVM's RISCVInstrInfo.td
  void GenInstrBranchCC_rri(uint8_t funct3, Register rs1, Register rs2,
                            int16_t imm12);
  void GenInstrLoad_ri(uint8_t funct3, Register rd, Register rs1,
                       int16_t imm12);
  void GenInstrStore_rri(uint8_t funct3, Register rs1, Register rs2,
                         int16_t imm12);
  void GenInstrALU_ri(uint8_t funct3, Register rd, Register rs1, int16_t imm12);
  void GenInstrShift_ri(bool arithshift, uint8_t funct3, Register rd,
                        Register rs1, uint8_t shamt);
  void GenInstrALU_rr(uint8_t funct7, uint8_t funct3, Register rd, Register rs1,
                      Register rs2);
  void GenInstrCSR_ir(uint8_t funct3, Register rd, ControlStatusReg csr,
                      Register rs1);
  void GenInstrCSR_ii(uint8_t funct3, Register rd, ControlStatusReg csr,
                      uint8_t rs1);
  void GenInstrShiftW_ri(bool arithshift, uint8_t funct3, Register rd,
                         Register rs1, uint8_t shamt);
  void GenInstrALUW_rr(uint8_t funct7, uint8_t funct3, Register rd,
                       Register rs1, Register rs2);
  void GenInstrPriv(uint8_t funct7, Register rs1, Register rs2);
  void GenInstrLoadFP_ri(uint8_t funct3, FPURegister rd, Register rs1,
                         int16_t imm12);
  void GenInstrStoreFP_rri(uint8_t funct3, Register rs1, FPURegister rs2,
                           int16_t imm12);
  void GenInstrALUFP_rr(uint8_t funct7, uint8_t funct3, FPURegister rd,
                        FPURegister rs1, FPURegister rs2);
  void GenInstrALUFP_rr(uint8_t funct7, uint8_t funct3, FPURegister rd,
                        Register rs1, Register rs2);
  void GenInstrALUFP_rr(uint8_t funct7, uint8_t funct3, FPURegister rd,
                        FPURegister rs1, Register rs2);
  void GenInstrALUFP_rr(uint8_t funct7, uint8_t funct3, Register rd,
                        FPURegister rs1, Register rs2);
  void GenInstrALUFP_rr(uint8_t funct7, uint8_t funct3, Register rd,
                        FPURegister rs1, FPURegister rs2);

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
    int start() { return start_; }
    int end() { return end_; }
    int take_slot() {
      int trampoline_slot = kInvalidSlotPos;
      if (free_slot_count_ <= 0) {
        // We have run out of space on trampolines.
        // Make sure we fail in debug mode, so we become aware of each case
        // when this happens.
        DCHECK(0);
        // Internal exception will be caught.
      } else {
        trampoline_slot = next_slot_;
        free_slot_count_--;
        next_slot_ += kTrampolineSlotsSize;
      }
      return trampoline_slot;
    }

   private:
    int start_;
    int end_;
    int next_slot_;
    int free_slot_count_;
  };

  int32_t get_trampoline_entry(int32_t pos);
  int unbound_labels_count_;
  // After trampoline is emitted, long branches are used in generated code for
  // the forward branches whose target offsets could be beyond reach of branch
  // instruction. We use this information to trigger different mode of
  // branch instruction generation, where we use jump instructions rather
  // than regular branch instructions.
  bool trampoline_emitted_ = false;
  static constexpr int kInvalidSlotPos = -1;

  // Internal reference positions, required for unbounded internal reference
  // labels.
  std::set<int64_t> internal_reference_positions_;
  bool is_internal_reference(Label* L) {
    return internal_reference_positions_.find(L->pos()) !=
           internal_reference_positions_.end();
  }

  Trampoline trampoline_;
  bool internal_trampoline_exception_;

  RegList scratch_register_list_;

 private:
  ConstantPool constpool_;

  void AllocateAndInstallRequestedHeapObjects(Isolate* isolate);

  int WriteCodeComments();

  friend class RegExpMacroAssemblerRISCV;
  friend class RelocInfo;
  friend class BlockTrampolinePoolScope;
  friend class EnsureSpace;
  friend class ConstantPool;
};

class EnsureSpace {
 public:
  explicit inline EnsureSpace(Assembler* assembler);
};

class V8_EXPORT_PRIVATE UseScratchRegisterScope {
 public:
  explicit UseScratchRegisterScope(Assembler* assembler);
  ~UseScratchRegisterScope();

  Register Acquire();
  bool hasAvailable() const;

 private:
  RegList* available_;
  RegList old_available_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_RISCV64_ASSEMBLER_RISCV64_H_
