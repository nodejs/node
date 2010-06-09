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
// Copyright 2010 the V8 project authors. All rights reserved.


#ifndef V8_MIPS_ASSEMBLER_MIPS_H_
#define V8_MIPS_ASSEMBLER_MIPS_H_

#include <stdio.h>
#include "assembler.h"
#include "constants-mips.h"
#include "serialize.h"

using namespace assembler::mips;

namespace v8 {
namespace internal {

// CPU Registers.
//
// 1) We would prefer to use an enum, but enum values are assignment-
// compatible with int, which has caused code-generation bugs.
//
// 2) We would prefer to use a class instead of a struct but we don't like
// the register initialization to depend on the particular initialization
// order (which appears to be different on OS X, Linux, and Windows for the
// installed versions of C++ we tried). Using a struct permits C-style
// "initialization". Also, the Register objects cannot be const as this
// forces initialization stubs in MSVC, making us dependent on initialization
// order.
//
// 3) By not using an enum, we are possibly preventing the compiler from
// doing certain constant folds, which may significantly reduce the
// code generated for some assembly instructions (because they boil down
// to a few constants). If this is a problem, we could change the code
// such that we use an enum in optimized mode, and the struct in debug
// mode. This way we get the compile-time error checking in debug mode
// and best performance in optimized code.


// -----------------------------------------------------------------------------
// Implementation of Register and FPURegister

// Core register.
struct Register {
  bool is_valid() const  { return 0 <= code_ && code_ < kNumRegisters; }
  bool is(Register reg) const  { return code_ == reg.code_; }
  int code() const  {
    ASSERT(is_valid());
    return code_;
  }
  int bit() const  {
    ASSERT(is_valid());
    return 1 << code_;
  }

  // Unfortunately we can't make this private in a struct.
  int code_;
};

extern const Register no_reg;

extern const Register zero_reg;
extern const Register at;
extern const Register v0;
extern const Register v1;
extern const Register a0;
extern const Register a1;
extern const Register a2;
extern const Register a3;
extern const Register t0;
extern const Register t1;
extern const Register t2;
extern const Register t3;
extern const Register t4;
extern const Register t5;
extern const Register t6;
extern const Register t7;
extern const Register s0;
extern const Register s1;
extern const Register s2;
extern const Register s3;
extern const Register s4;
extern const Register s5;
extern const Register s6;
extern const Register s7;
extern const Register t8;
extern const Register t9;
extern const Register k0;
extern const Register k1;
extern const Register gp;
extern const Register sp;
extern const Register s8_fp;
extern const Register ra;

int ToNumber(Register reg);

Register ToRegister(int num);

// Coprocessor register.
struct FPURegister {
  bool is_valid() const  { return 0 <= code_ && code_ < kNumFPURegister ; }
  bool is(FPURegister creg) const  { return code_ == creg.code_; }
  int code() const  {
    ASSERT(is_valid());
    return code_;
  }
  int bit() const  {
    ASSERT(is_valid());
    return 1 << code_;
  }

  // Unfortunately we can't make this private in a struct.
  int code_;
};

extern const FPURegister no_creg;

extern const FPURegister f0;
extern const FPURegister f1;
extern const FPURegister f2;
extern const FPURegister f3;
extern const FPURegister f4;
extern const FPURegister f5;
extern const FPURegister f6;
extern const FPURegister f7;
extern const FPURegister f8;
extern const FPURegister f9;
extern const FPURegister f10;
extern const FPURegister f11;
extern const FPURegister f12;  // arg
extern const FPURegister f13;
extern const FPURegister f14;  // arg
extern const FPURegister f15;
extern const FPURegister f16;
extern const FPURegister f17;
extern const FPURegister f18;
extern const FPURegister f19;
extern const FPURegister f20;
extern const FPURegister f21;
extern const FPURegister f22;
extern const FPURegister f23;
extern const FPURegister f24;
extern const FPURegister f25;
extern const FPURegister f26;
extern const FPURegister f27;
extern const FPURegister f28;
extern const FPURegister f29;
extern const FPURegister f30;
extern const FPURegister f31;


// Returns the equivalent of !cc.
// Negation of the default no_condition (-1) results in a non-default
// no_condition value (-2). As long as tests for no_condition check
// for condition < 0, this will work as expected.
inline Condition NegateCondition(Condition cc);

inline Condition ReverseCondition(Condition cc) {
  switch (cc) {
    case Uless:
      return Ugreater;
    case Ugreater:
      return Uless;
    case Ugreater_equal:
      return Uless_equal;
    case Uless_equal:
      return Ugreater_equal;
    case less:
      return greater;
    case greater:
      return less;
    case greater_equal:
      return less_equal;
    case less_equal:
      return greater_equal;
    default:
      return cc;
  };
}


enum Hint {
  no_hint = 0
};

inline Hint NegateHint(Hint hint) {
  return no_hint;
}


// -----------------------------------------------------------------------------
// Machine instruction Operands.

// Class Operand represents a shifter operand in data processing instructions.
class Operand BASE_EMBEDDED {
 public:
  // Immediate.
  INLINE(explicit Operand(int32_t immediate,
         RelocInfo::Mode rmode = RelocInfo::NONE));
  INLINE(explicit Operand(const ExternalReference& f));
  INLINE(explicit Operand(const char* s));
  INLINE(explicit Operand(Object** opp));
  INLINE(explicit Operand(Context** cpp));
  explicit Operand(Handle<Object> handle);
  INLINE(explicit Operand(Smi* value));

  // Register.
  INLINE(explicit Operand(Register rm));

  // Return true if this is a register operand.
  INLINE(bool is_reg() const);

  Register rm() const { return rm_; }

 private:
  Register rm_;
  int32_t imm32_;  // Valid if rm_ == no_reg
  RelocInfo::Mode rmode_;

  friend class Assembler;
  friend class MacroAssembler;
};


// On MIPS we have only one adressing mode with base_reg + offset.
// Class MemOperand represents a memory operand in load and store instructions.
class MemOperand : public Operand {
 public:

  explicit MemOperand(Register rn, int16_t offset = 0);

 private:
  int16_t offset_;

  friend class Assembler;
};


class Assembler : public Malloced {
 public:
  // Create an assembler. Instructions and relocation information are emitted
  // into a buffer, with the instructions starting from the beginning and the
  // relocation information starting from the end of the buffer. See CodeDesc
  // for a detailed comment on the layout (globals.h).
  //
  // If the provided buffer is NULL, the assembler allocates and grows its own
  // buffer, and buffer_size determines the initial buffer size. The buffer is
  // owned by the assembler and deallocated upon destruction of the assembler.
  //
  // If the provided buffer is not NULL, the assembler uses the provided buffer
  // for code generation and assumes its size to be buffer_size. If the buffer
  // is too small, a fatal error occurs. No deallocation of the buffer is done
  // upon destruction of the assembler.
  Assembler(void* buffer, int buffer_size);
  ~Assembler();

  // GetCode emits any pending (non-emitted) code and fills the descriptor
  // desc. GetCode() is idempotent; it returns the same result if no other
  // Assembler functions are invoked in between GetCode() calls.
  void GetCode(CodeDesc* desc);

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
  void bind(Label* L);  // binds an unbound label L to the current code position

  // Returns the branch offset to the given label from the current code position
  // Links the label to the current position if it is still unbound
  // Manages the jump elimination optimization if the second parameter is true.
  int32_t branch_offset(Label* L, bool jump_elimination_allowed);
  int32_t shifted_branch_offset(Label* L, bool jump_elimination_allowed) {
    int32_t o = branch_offset(L, jump_elimination_allowed);
    ASSERT((o & 3) == 0);   // Assert the offset is aligned.
    return o >> 2;
  }

  // Puts a labels target address at the given position.
  // The high 8 bits are set to zero.
  void label_at_put(Label* L, int at_offset);

  // Size of an instruction.
  static const int kInstrSize = sizeof(Instr);

  // Difference between address of current opcode and target address offset.
  static const int kBranchPCOffset = 4;

  // Read/Modify the code target address in the branch/call instruction at pc.
  static Address target_address_at(Address pc);
  static void set_target_address_at(Address pc, Address target);

  // This sets the branch destination (which gets loaded at the call address).
  // This is for calls and branches within generated code.
  inline static void set_target_at(Address instruction_payload,
                                   Address target) {
    set_target_address_at(instruction_payload, target);
  }

  // This sets the branch destination.
  // This is for calls and branches to runtime code.
  inline static void set_external_target_at(Address instruction_payload,
                                            Address target) {
    set_target_address_at(instruction_payload, target);
  }

  static const int kCallTargetSize = 3 * kPointerSize;
  static const int kExternalTargetSize = 3 * kPointerSize;

  // Distance between the instruction referring to the address of the call
  // target and the return address.
  static const int kCallTargetAddressOffset = 4 * kInstrSize;

  // Distance between start of patched return sequence and the emitted address
  // to jump to.
  static const int kPatchReturnSequenceAddressOffset = kInstrSize;

  // Distance between start of patched debug break slot and the emitted address
  // to jump to.
  static const int kPatchDebugBreakSlotAddressOffset = kInstrSize;

  // ---------------------------------------------------------------------------
  // Code generation.

  void nop() { sll(zero_reg, zero_reg, 0); }


  //------- Branch and jump  instructions --------
  // We don't use likely variant of instructions.
  void b(int16_t offset);
  void b(Label* L) { b(branch_offset(L, false)>>2); }
  void bal(int16_t offset);
  void bal(Label* L) { bal(branch_offset(L, false)>>2); }

  void beq(Register rs, Register rt, int16_t offset);
  void beq(Register rs, Register rt, Label* L) {
    beq(rs, rt, branch_offset(L, false) >> 2);
  }
  void bgez(Register rs, int16_t offset);
  void bgezal(Register rs, int16_t offset);
  void bgtz(Register rs, int16_t offset);
  void blez(Register rs, int16_t offset);
  void bltz(Register rs, int16_t offset);
  void bltzal(Register rs, int16_t offset);
  void bne(Register rs, Register rt, int16_t offset);
  void bne(Register rs, Register rt, Label* L) {
    bne(rs, rt, branch_offset(L, false)>>2);
  }

  // Never use the int16_t b(l)cond version with a branch offset
  // instead of using the Label* version. See Twiki for infos.

  // Jump targets must be in the current 256 MB-aligned region. ie 28 bits.
  void j(int32_t target);
  void jal(int32_t target);
  void jalr(Register rs, Register rd = ra);
  void jr(Register target);


  //-------Data-processing-instructions---------

  // Arithmetic.
  void add(Register rd, Register rs, Register rt);
  void addu(Register rd, Register rs, Register rt);
  void sub(Register rd, Register rs, Register rt);
  void subu(Register rd, Register rs, Register rt);
  void mult(Register rs, Register rt);
  void multu(Register rs, Register rt);
  void div(Register rs, Register rt);
  void divu(Register rs, Register rt);
  void mul(Register rd, Register rs, Register rt);

  void addi(Register rd, Register rs, int32_t j);
  void addiu(Register rd, Register rs, int32_t j);

  // Logical.
  void and_(Register rd, Register rs, Register rt);
  void or_(Register rd, Register rs, Register rt);
  void xor_(Register rd, Register rs, Register rt);
  void nor(Register rd, Register rs, Register rt);

  void andi(Register rd, Register rs, int32_t j);
  void ori(Register rd, Register rs, int32_t j);
  void xori(Register rd, Register rs, int32_t j);
  void lui(Register rd, int32_t j);

  // Shifts.
  void sll(Register rd, Register rt, uint16_t sa);
  void sllv(Register rd, Register rt, Register rs);
  void srl(Register rd, Register rt, uint16_t sa);
  void srlv(Register rd, Register rt, Register rs);
  void sra(Register rt, Register rd, uint16_t sa);
  void srav(Register rt, Register rd, Register rs);


  //------------Memory-instructions-------------

  void lb(Register rd, const MemOperand& rs);
  void lbu(Register rd, const MemOperand& rs);
  void lw(Register rd, const MemOperand& rs);
  void sb(Register rd, const MemOperand& rs);
  void sw(Register rd, const MemOperand& rs);


  //-------------Misc-instructions--------------

  // Break / Trap instructions.
  void break_(uint32_t code);
  void tge(Register rs, Register rt, uint16_t code);
  void tgeu(Register rs, Register rt, uint16_t code);
  void tlt(Register rs, Register rt, uint16_t code);
  void tltu(Register rs, Register rt, uint16_t code);
  void teq(Register rs, Register rt, uint16_t code);
  void tne(Register rs, Register rt, uint16_t code);

  // Move from HI/LO register.
  void mfhi(Register rd);
  void mflo(Register rd);

  // Set on less than.
  void slt(Register rd, Register rs, Register rt);
  void sltu(Register rd, Register rs, Register rt);
  void slti(Register rd, Register rs, int32_t j);
  void sltiu(Register rd, Register rs, int32_t j);


  //--------Coprocessor-instructions----------------

  // Load, store, and move.
  void lwc1(FPURegister fd, const MemOperand& src);
  void ldc1(FPURegister fd, const MemOperand& src);

  void swc1(FPURegister fs, const MemOperand& dst);
  void sdc1(FPURegister fs, const MemOperand& dst);

  // When paired with MTC1 to write a value to a 64-bit FPR, the MTC1 must be
  // executed first, followed by the MTHC1.
  void mtc1(FPURegister fs, Register rt);
  void mthc1(FPURegister fs, Register rt);
  void mfc1(FPURegister fs, Register rt);
  void mfhc1(FPURegister fs, Register rt);

  // Conversion.
  void cvt_w_s(FPURegister fd, FPURegister fs);
  void cvt_w_d(FPURegister fd, FPURegister fs);

  void cvt_l_s(FPURegister fd, FPURegister fs);
  void cvt_l_d(FPURegister fd, FPURegister fs);

  void cvt_s_w(FPURegister fd, FPURegister fs);
  void cvt_s_l(FPURegister fd, FPURegister fs);
  void cvt_s_d(FPURegister fd, FPURegister fs);

  void cvt_d_w(FPURegister fd, FPURegister fs);
  void cvt_d_l(FPURegister fd, FPURegister fs);
  void cvt_d_s(FPURegister fd, FPURegister fs);

  // Conditions and branches.
  void c(FPUCondition cond, SecondaryField fmt,
         FPURegister ft, FPURegister fs, uint16_t cc = 0);

  void bc1f(int16_t offset, uint16_t cc = 0);
  void bc1f(Label* L, uint16_t cc = 0) { bc1f(branch_offset(L, false)>>2, cc); }
  void bc1t(int16_t offset, uint16_t cc = 0);
  void bc1t(Label* L, uint16_t cc = 0) { bc1t(branch_offset(L, false)>>2, cc); }


  // Check the code size generated from label to here.
  int InstructionsGeneratedSince(Label* l) {
    return (pc_offset() - l->pos()) / kInstrSize;
  }

  // Debugging.

  // Mark address of the ExitJSFrame code.
  void RecordJSReturn();

  // Record a comment relocation entry that can be used by a disassembler.
  // Use --debug_code to enable.
  void RecordComment(const char* msg);

  void RecordPosition(int pos);
  void RecordStatementPosition(int pos);
  bool WriteRecordedPositions();

  int32_t pc_offset() const { return pc_ - buffer_; }
  int32_t current_position() const { return current_position_; }
  int32_t current_statement_position() const {
    return current_statement_position_;
  }

  // Check if there is less than kGap bytes available in the buffer.
  // If this is the case, we need to grow the buffer before emitting
  // an instruction or relocation information.
  inline bool overflow() const { return pc_ >= reloc_info_writer.pos() - kGap; }

  // Get the number of bytes available in the buffer.
  inline int available_space() const { return reloc_info_writer.pos() - pc_; }

 protected:
  int32_t buffer_space() const { return reloc_info_writer.pos() - pc_; }

  // Read/patch instructions.
  static Instr instr_at(byte* pc) { return *reinterpret_cast<Instr*>(pc); }
  void instr_at_put(byte* pc, Instr instr) {
    *reinterpret_cast<Instr*>(pc) = instr;
  }
  Instr instr_at(int pos) { return *reinterpret_cast<Instr*>(buffer_ + pos); }
  void instr_at_put(int pos, Instr instr) {
    *reinterpret_cast<Instr*>(buffer_ + pos) = instr;
  }

  // Check if an instruction is a branch of some kind.
  bool is_branch(Instr instr);

  // Decode branch instruction at pos and return branch target pos.
  int target_at(int32_t pos);

  // Patch branch instruction at pos to branch to given branch target pos.
  void target_at_put(int32_t pos, int32_t target_pos);

  // Say if we need to relocate with this mode.
  bool MustUseAt(RelocInfo::Mode rmode);

  // Record reloc info for current pc_.
  void RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data = 0);

 private:
  // Code buffer:
  // The buffer into which code and relocation info are generated.
  byte* buffer_;
  int buffer_size_;
  // True if the assembler owns the buffer, false if buffer is external.
  bool own_buffer_;

  // Buffer size and constant pool distance are checked together at regular
  // intervals of kBufferCheckInterval emitted bytes.
  static const int kBufferCheckInterval = 1*KB/2;

  // Code generation.
  // The relocation writer's position is at least kGap bytes below the end of
  // the generated instructions. This is so that multi-instruction sequences do
  // not have to check for overflow. The same is true for writes of large
  // relocation info entries.
  static const int kGap = 32;
  byte* pc_;  // The program counter - moves forward.

  // Relocation information generation.
  // Each relocation is encoded as a variable size value.
  static const int kMaxRelocSize = RelocInfoWriter::kMaxSize;
  RelocInfoWriter reloc_info_writer;

  // The bound position, before this we cannot do instruction elimination.
  int last_bound_pos_;

  // Source position information.
  int current_position_;
  int current_statement_position_;
  int written_position_;
  int written_statement_position_;

  // Code emission.
  inline void CheckBuffer();
  void GrowBuffer();
  inline void emit(Instr x);

  // Instruction generation.
  // We have 3 different kind of encoding layout on MIPS.
  // However due to many different types of objects encoded in the same fields
  // we have quite a few aliases for each mode.
  // Using the same structure to refer to Register and FPURegister would spare a
  // few aliases, but mixing both does not look clean to me.
  // Anyway we could surely implement this differently.

  void GenInstrRegister(Opcode opcode,
                        Register rs,
                        Register rt,
                        Register rd,
                        uint16_t sa = 0,
                        SecondaryField func = NULLSF);

  void GenInstrRegister(Opcode opcode,
                        SecondaryField fmt,
                        FPURegister ft,
                        FPURegister fs,
                        FPURegister fd,
                        SecondaryField func = NULLSF);

  void GenInstrRegister(Opcode opcode,
                        SecondaryField fmt,
                        Register rt,
                        FPURegister fs,
                        FPURegister fd,
                        SecondaryField func = NULLSF);


  void GenInstrImmediate(Opcode opcode,
                         Register rs,
                         Register rt,
                         int32_t  j);
  void GenInstrImmediate(Opcode opcode,
                         Register rs,
                         SecondaryField SF,
                         int32_t  j);
  void GenInstrImmediate(Opcode opcode,
                         Register r1,
                         FPURegister r2,
                         int32_t  j);


  void GenInstrJump(Opcode opcode,
                     uint32_t address);


  // Labels.
  void print(Label* L);
  void bind_to(Label* L, int pos);
  void link_to(Label* L, Label* appendix);
  void next(Label* L);

  friend class RegExpMacroAssemblerMIPS;
  friend class RelocInfo;
};

} }  // namespace v8::internal

#endif  // V8_ARM_ASSEMBLER_MIPS_H_

