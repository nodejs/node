// Copyright (c) 1994-2006 Sun Microsystems Inc.
// All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// - Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// - Redistribution in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the
// distribution.
//
// - Neither the name of Sun Microsystems or the names of contributors may
// be used to endorse or promote products derived from this software without
// specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.

// The original source code covered by the above license above has been
// modified significantly by Google Inc.
// Copyright 2014 the V8 project authors. All rights reserved.

// A light-weight PPC Assembler
// Generates user mode instructions for the PPC architecture up

#ifndef V8_PPC_ASSEMBLER_PPC_H_
#define V8_PPC_ASSEMBLER_PPC_H_

#include <stdio.h>
#include <vector>

#include "src/assembler.h"
#include "src/ppc/constants-ppc.h"
#include "src/serialize.h"

#define ABI_USES_FUNCTION_DESCRIPTORS \
  (V8_HOST_ARCH_PPC && (V8_OS_AIX ||    \
                      (V8_TARGET_ARCH_PPC64 && V8_TARGET_BIG_ENDIAN)))

#define ABI_PASSES_HANDLES_IN_REGS \
  (!V8_HOST_ARCH_PPC || V8_OS_AIX || V8_TARGET_ARCH_PPC64)

#define ABI_RETURNS_OBJECT_PAIRS_IN_REGS \
  (!V8_HOST_ARCH_PPC || !V8_TARGET_ARCH_PPC64 || V8_TARGET_LITTLE_ENDIAN)

#define ABI_TOC_ADDRESSABILITY_VIA_IP \
  (V8_HOST_ARCH_PPC && V8_TARGET_ARCH_PPC64 && V8_TARGET_LITTLE_ENDIAN)

#if !V8_HOST_ARCH_PPC || V8_OS_AIX || V8_TARGET_ARCH_PPC64
#define ABI_TOC_REGISTER kRegister_r2_Code
#else
#define ABI_TOC_REGISTER kRegister_r13_Code
#endif

#define INSTR_AND_DATA_CACHE_COHERENCY LWSYNC

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

// Core register
struct Register {
  static const int kNumRegisters = 32;
  static const int kSizeInBytes = kPointerSize;

#if V8_TARGET_LITTLE_ENDIAN
  static const int kMantissaOffset = 0;
  static const int kExponentOffset = 4;
#else
  static const int kMantissaOffset = 4;
  static const int kExponentOffset = 0;
#endif

  static const int kAllocatableLowRangeBegin = 3;
  static const int kAllocatableLowRangeEnd = 10;
  static const int kAllocatableHighRangeBegin = 14;
#if V8_OOL_CONSTANT_POOL
  static const int kAllocatableHighRangeEnd = 27;
#else
  static const int kAllocatableHighRangeEnd = 28;
#endif
  static const int kAllocatableContext = 30;

  static const int kNumAllocatableLow =
      kAllocatableLowRangeEnd - kAllocatableLowRangeBegin + 1;
  static const int kNumAllocatableHigh =
      kAllocatableHighRangeEnd - kAllocatableHighRangeBegin + 1;
  static const int kMaxNumAllocatableRegisters =
      kNumAllocatableLow + kNumAllocatableHigh + 1;  // cp

  static int NumAllocatableRegisters() { return kMaxNumAllocatableRegisters; }

  static int ToAllocationIndex(Register reg) {
    int index;
    int code = reg.code();
    if (code == kAllocatableContext) {
      // Context is the last index
      index = NumAllocatableRegisters() - 1;
    } else if (code <= kAllocatableLowRangeEnd) {
      // low range
      index = code - kAllocatableLowRangeBegin;
    } else {
      // high range
      index = code - kAllocatableHighRangeBegin + kNumAllocatableLow;
    }
    DCHECK(index >= 0 && index < kMaxNumAllocatableRegisters);
    return index;
  }

  static Register FromAllocationIndex(int index) {
    DCHECK(index >= 0 && index < kMaxNumAllocatableRegisters);
    // Last index is always the 'cp' register.
    if (index == kMaxNumAllocatableRegisters - 1) {
      return from_code(kAllocatableContext);
    }
    return (index < kNumAllocatableLow)
               ? from_code(index + kAllocatableLowRangeBegin)
               : from_code(index - kNumAllocatableLow +
                           kAllocatableHighRangeBegin);
  }

  static const char* AllocationIndexToString(int index) {
    DCHECK(index >= 0 && index < kMaxNumAllocatableRegisters);
    const char* const names[] = {
      "r3",
      "r4",
      "r5",
      "r6",
      "r7",
      "r8",
      "r9",
      "r10",
      "r14",
      "r15",
      "r16",
      "r17",
      "r18",
      "r19",
      "r20",
      "r21",
      "r22",
      "r23",
      "r24",
      "r25",
      "r26",
      "r27",
#if !V8_OOL_CONSTANT_POOL
      "r28",
#endif
      "cp",
    };
    return names[index];
  }

  static Register from_code(int code) {
    Register r = {code};
    return r;
  }

  bool is_valid() const { return 0 <= code_ && code_ < kNumRegisters; }
  bool is(Register reg) const { return code_ == reg.code_; }
  int code() const {
    DCHECK(is_valid());
    return code_;
  }
  int bit() const {
    DCHECK(is_valid());
    return 1 << code_;
  }

  void set_code(int code) {
    code_ = code;
    DCHECK(is_valid());
  }

  // Unfortunately we can't make this private in a struct.
  int code_;
};

// These constants are used in several locations, including static initializers
const int kRegister_no_reg_Code = -1;
const int kRegister_r0_Code = 0;  // general scratch
const int kRegister_sp_Code = 1;  // stack pointer
const int kRegister_r2_Code = 2;  // special on PowerPC
const int kRegister_r3_Code = 3;
const int kRegister_r4_Code = 4;
const int kRegister_r5_Code = 5;
const int kRegister_r6_Code = 6;
const int kRegister_r7_Code = 7;
const int kRegister_r8_Code = 8;
const int kRegister_r9_Code = 9;
const int kRegister_r10_Code = 10;
const int kRegister_r11_Code = 11;  // lithium scratch
const int kRegister_ip_Code = 12;   // ip (general scratch)
const int kRegister_r13_Code = 13;  // special on PowerPC
const int kRegister_r14_Code = 14;
const int kRegister_r15_Code = 15;

const int kRegister_r16_Code = 16;
const int kRegister_r17_Code = 17;
const int kRegister_r18_Code = 18;
const int kRegister_r19_Code = 19;
const int kRegister_r20_Code = 20;
const int kRegister_r21_Code = 21;
const int kRegister_r22_Code = 22;
const int kRegister_r23_Code = 23;
const int kRegister_r24_Code = 24;
const int kRegister_r25_Code = 25;
const int kRegister_r26_Code = 26;
const int kRegister_r27_Code = 27;
const int kRegister_r28_Code = 28;  // constant pool pointer
const int kRegister_r29_Code = 29;  // roots array pointer
const int kRegister_r30_Code = 30;  // context pointer
const int kRegister_fp_Code = 31;   // frame pointer

const Register no_reg = {kRegister_no_reg_Code};

const Register r0 = {kRegister_r0_Code};
const Register sp = {kRegister_sp_Code};
const Register r2 = {kRegister_r2_Code};
const Register r3 = {kRegister_r3_Code};
const Register r4 = {kRegister_r4_Code};
const Register r5 = {kRegister_r5_Code};
const Register r6 = {kRegister_r6_Code};
const Register r7 = {kRegister_r7_Code};
const Register r8 = {kRegister_r8_Code};
const Register r9 = {kRegister_r9_Code};
const Register r10 = {kRegister_r10_Code};
const Register r11 = {kRegister_r11_Code};
const Register ip = {kRegister_ip_Code};
const Register r13 = {kRegister_r13_Code};
const Register r14 = {kRegister_r14_Code};
const Register r15 = {kRegister_r15_Code};

const Register r16 = {kRegister_r16_Code};
const Register r17 = {kRegister_r17_Code};
const Register r18 = {kRegister_r18_Code};
const Register r19 = {kRegister_r19_Code};
const Register r20 = {kRegister_r20_Code};
const Register r21 = {kRegister_r21_Code};
const Register r22 = {kRegister_r22_Code};
const Register r23 = {kRegister_r23_Code};
const Register r24 = {kRegister_r24_Code};
const Register r25 = {kRegister_r25_Code};
const Register r26 = {kRegister_r26_Code};
const Register r27 = {kRegister_r27_Code};
const Register r28 = {kRegister_r28_Code};
const Register r29 = {kRegister_r29_Code};
const Register r30 = {kRegister_r30_Code};
const Register fp = {kRegister_fp_Code};

// Give alias names to registers
const Register cp = {kRegister_r30_Code};  // JavaScript context pointer
const Register kRootRegister = {kRegister_r29_Code};  // Roots array pointer.
#if V8_OOL_CONSTANT_POOL
const Register kConstantPoolRegister = {kRegister_r28_Code};  // Constant pool
#endif

// Double word FP register.
struct DoubleRegister {
  static const int kNumRegisters = 32;
  static const int kMaxNumRegisters = kNumRegisters;
  static const int kNumVolatileRegisters = 14;  // d0-d13
  static const int kSizeInBytes = 8;

  static const int kAllocatableLowRangeBegin = 1;
  static const int kAllocatableLowRangeEnd = 12;
  static const int kAllocatableHighRangeBegin = 15;
  static const int kAllocatableHighRangeEnd = 31;

  static const int kNumAllocatableLow =
      kAllocatableLowRangeEnd - kAllocatableLowRangeBegin + 1;
  static const int kNumAllocatableHigh =
      kAllocatableHighRangeEnd - kAllocatableHighRangeBegin + 1;
  static const int kMaxNumAllocatableRegisters =
      kNumAllocatableLow + kNumAllocatableHigh;
  static int NumAllocatableRegisters() { return kMaxNumAllocatableRegisters; }

  // TODO(turbofan)
  inline static int NumAllocatableAliasedRegisters() {
    return NumAllocatableRegisters();
  }

  static int ToAllocationIndex(DoubleRegister reg) {
    int code = reg.code();
    int index = (code <= kAllocatableLowRangeEnd)
                    ? code - kAllocatableLowRangeBegin
                    : code - kAllocatableHighRangeBegin + kNumAllocatableLow;
    DCHECK(index < kMaxNumAllocatableRegisters);
    return index;
  }

  static DoubleRegister FromAllocationIndex(int index) {
    DCHECK(index >= 0 && index < kMaxNumAllocatableRegisters);
    return (index < kNumAllocatableLow)
               ? from_code(index + kAllocatableLowRangeBegin)
               : from_code(index - kNumAllocatableLow +
                           kAllocatableHighRangeBegin);
  }

  static const char* AllocationIndexToString(int index);

  static DoubleRegister from_code(int code) {
    DoubleRegister r = {code};
    return r;
  }

  bool is_valid() const { return 0 <= code_ && code_ < kMaxNumRegisters; }
  bool is(DoubleRegister reg) const { return code_ == reg.code_; }

  int code() const {
    DCHECK(is_valid());
    return code_;
  }
  int bit() const {
    DCHECK(is_valid());
    return 1 << code_;
  }
  void split_code(int* vm, int* m) const {
    DCHECK(is_valid());
    *m = (code_ & 0x10) >> 4;
    *vm = code_ & 0x0F;
  }

  int code_;
};


const DoubleRegister no_dreg = {-1};
const DoubleRegister d0 = {0};
const DoubleRegister d1 = {1};
const DoubleRegister d2 = {2};
const DoubleRegister d3 = {3};
const DoubleRegister d4 = {4};
const DoubleRegister d5 = {5};
const DoubleRegister d6 = {6};
const DoubleRegister d7 = {7};
const DoubleRegister d8 = {8};
const DoubleRegister d9 = {9};
const DoubleRegister d10 = {10};
const DoubleRegister d11 = {11};
const DoubleRegister d12 = {12};
const DoubleRegister d13 = {13};
const DoubleRegister d14 = {14};
const DoubleRegister d15 = {15};
const DoubleRegister d16 = {16};
const DoubleRegister d17 = {17};
const DoubleRegister d18 = {18};
const DoubleRegister d19 = {19};
const DoubleRegister d20 = {20};
const DoubleRegister d21 = {21};
const DoubleRegister d22 = {22};
const DoubleRegister d23 = {23};
const DoubleRegister d24 = {24};
const DoubleRegister d25 = {25};
const DoubleRegister d26 = {26};
const DoubleRegister d27 = {27};
const DoubleRegister d28 = {28};
const DoubleRegister d29 = {29};
const DoubleRegister d30 = {30};
const DoubleRegister d31 = {31};

// Aliases for double registers.  Defined using #define instead of
// "static const DoubleRegister&" because Clang complains otherwise when a
// compilation unit that includes this header doesn't use the variables.
#define kFirstCalleeSavedDoubleReg d14
#define kLastCalleeSavedDoubleReg d31
#define kDoubleRegZero d14
#define kScratchDoubleReg d13

Register ToRegister(int num);

// Coprocessor register
struct CRegister {
  bool is_valid() const { return 0 <= code_ && code_ < 16; }
  bool is(CRegister creg) const { return code_ == creg.code_; }
  int code() const {
    DCHECK(is_valid());
    return code_;
  }
  int bit() const {
    DCHECK(is_valid());
    return 1 << code_;
  }

  // Unfortunately we can't make this private in a struct.
  int code_;
};


const CRegister no_creg = {-1};

const CRegister cr0 = {0};
const CRegister cr1 = {1};
const CRegister cr2 = {2};
const CRegister cr3 = {3};
const CRegister cr4 = {4};
const CRegister cr5 = {5};
const CRegister cr6 = {6};
const CRegister cr7 = {7};
const CRegister cr8 = {8};
const CRegister cr9 = {9};
const CRegister cr10 = {10};
const CRegister cr11 = {11};
const CRegister cr12 = {12};
const CRegister cr13 = {13};
const CRegister cr14 = {14};
const CRegister cr15 = {15};

// -----------------------------------------------------------------------------
// Machine instruction Operands

#if V8_TARGET_ARCH_PPC64
const RelocInfo::Mode kRelocInfo_NONEPTR = RelocInfo::NONE64;
#else
const RelocInfo::Mode kRelocInfo_NONEPTR = RelocInfo::NONE32;
#endif

// Class Operand represents a shifter operand in data processing instructions
class Operand BASE_EMBEDDED {
 public:
  // immediate
  INLINE(explicit Operand(intptr_t immediate,
                          RelocInfo::Mode rmode = kRelocInfo_NONEPTR));
  INLINE(static Operand Zero()) { return Operand(static_cast<intptr_t>(0)); }
  INLINE(explicit Operand(const ExternalReference& f));
  explicit Operand(Handle<Object> handle);
  INLINE(explicit Operand(Smi* value));

  // rm
  INLINE(explicit Operand(Register rm));

  // Return true if this is a register operand.
  INLINE(bool is_reg() const);

  // For mov.  Return the number of actual instructions required to
  // load the operand into a register.  This can be anywhere from
  // one (constant pool small section) to five instructions (full
  // 64-bit sequence).
  //
  // The value returned is only valid as long as no entries are added to the
  // constant pool between this call and the actual instruction being emitted.
  bool must_output_reloc_info(const Assembler* assembler) const;

  inline intptr_t immediate() const {
    DCHECK(!rm_.is_valid());
    return imm_;
  }

  Register rm() const { return rm_; }

 private:
  Register rm_;
  intptr_t imm_;  // valid if rm_ == no_reg
  RelocInfo::Mode rmode_;

  friend class Assembler;
  friend class MacroAssembler;
};


// Class MemOperand represents a memory operand in load and store instructions
// On PowerPC we have base register + 16bit signed value
// Alternatively we can have a 16bit signed value immediate
class MemOperand BASE_EMBEDDED {
 public:
  explicit MemOperand(Register rn, int32_t offset = 0);

  explicit MemOperand(Register ra, Register rb);

  int32_t offset() const {
    DCHECK(rb_.is(no_reg));
    return offset_;
  }

  // PowerPC - base register
  Register ra() const {
    DCHECK(!ra_.is(no_reg));
    return ra_;
  }

  Register rb() const {
    DCHECK(offset_ == 0 && !rb_.is(no_reg));
    return rb_;
  }

 private:
  Register ra_;     // base
  int32_t offset_;  // offset
  Register rb_;     // index

  friend class Assembler;
};


#if V8_OOL_CONSTANT_POOL
// Class used to build a constant pool.
class ConstantPoolBuilder BASE_EMBEDDED {
 public:
  ConstantPoolBuilder();
  ConstantPoolArray::LayoutSection AddEntry(Assembler* assm,
                                            const RelocInfo& rinfo);
  void Relocate(intptr_t pc_delta);
  bool IsEmpty();
  Handle<ConstantPoolArray> New(Isolate* isolate);
  void Populate(Assembler* assm, ConstantPoolArray* constant_pool);

  inline ConstantPoolArray::LayoutSection current_section() const {
    return current_section_;
  }

  // Rather than increasing the capacity of the ConstantPoolArray's
  // small section to match the longer (16-bit) reach of PPC's load
  // instruction (at the expense of a larger header to describe the
  // layout), the PPC implementation utilizes the extended section to
  // satisfy that reach.  I.e. all entries (regardless of their
  // section) are reachable with a single load instruction.
  //
  // This implementation does not support an unlimited constant pool
  // size (which would require a multi-instruction sequence).  [See
  // ARM commit e27ab337 for a reference on the changes required to
  // support the longer instruction sequence.]  Note, however, that
  // going down that path will necessarily generate that longer
  // sequence for all extended section accesses since the placement of
  // a given entry within the section is not known at the time of
  // code generation.
  //
  // TODO(mbrandy): Determine whether there is a benefit to supporting
  // the longer sequence given that nops could be used for those
  // entries which are reachable with a single instruction.
  inline bool is_full() const { return !is_int16(size_); }

  inline ConstantPoolArray::NumberOfEntries* number_of_entries(
      ConstantPoolArray::LayoutSection section) {
    return &number_of_entries_[section];
  }

  inline ConstantPoolArray::NumberOfEntries* small_entries() {
    return number_of_entries(ConstantPoolArray::SMALL_SECTION);
  }

  inline ConstantPoolArray::NumberOfEntries* extended_entries() {
    return number_of_entries(ConstantPoolArray::EXTENDED_SECTION);
  }

 private:
  struct ConstantPoolEntry {
    ConstantPoolEntry(RelocInfo rinfo, ConstantPoolArray::LayoutSection section,
                      int merged_index)
        : rinfo_(rinfo), section_(section), merged_index_(merged_index) {}

    RelocInfo rinfo_;
    ConstantPoolArray::LayoutSection section_;
    int merged_index_;
  };

  ConstantPoolArray::Type GetConstantPoolType(RelocInfo::Mode rmode);

  uint32_t size_;
  std::vector<ConstantPoolEntry> entries_;
  ConstantPoolArray::LayoutSection current_section_;
  ConstantPoolArray::NumberOfEntries number_of_entries_[2];
};
#endif


class Assembler : public AssemblerBase {
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
  Assembler(Isolate* isolate, void* buffer, int buffer_size);
  virtual ~Assembler() {}

  // GetCode emits any pending (non-emitted) code and fills the descriptor
  // desc. GetCode() is idempotent; it returns the same result if no other
  // Assembler functions are invoked in between GetCode() calls.
  void GetCode(CodeDesc* desc);

  // Label operations & relative jumps (PPUM Appendix D)
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
  // Determines if Label is bound and near enough so that a single
  // branch instruction can be used to reach it.
  bool is_near(Label* L, Condition cond);

  // Returns the branch offset to the given label from the current code position
  // Links the label to the current position if it is still unbound
  // Manages the jump elimination optimization if the second parameter is true.
  int branch_offset(Label* L, bool jump_elimination_allowed);

  // Puts a labels target address at the given position.
  // The high 8 bits are set to zero.
  void label_at_put(Label* L, int at_offset);

#if V8_OOL_CONSTANT_POOL
  INLINE(static bool IsConstantPoolLoadStart(Address pc));
  INLINE(static bool IsConstantPoolLoadEnd(Address pc));
  INLINE(static int GetConstantPoolOffset(Address pc));
  INLINE(static void SetConstantPoolOffset(Address pc, int offset));

  // Return the address in the constant pool of the code target address used by
  // the branch/call instruction at pc, or the object in a mov.
  INLINE(static Address target_constant_pool_address_at(
      Address pc, ConstantPoolArray* constant_pool));
#endif

  // Read/Modify the code target address in the branch/call instruction at pc.
  INLINE(static Address target_address_at(Address pc,
                                          ConstantPoolArray* constant_pool));
  INLINE(static void set_target_address_at(
      Address pc, ConstantPoolArray* constant_pool, Address target,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED));
  INLINE(static Address target_address_at(Address pc, Code* code)) {
    ConstantPoolArray* constant_pool = code ? code->constant_pool() : NULL;
    return target_address_at(pc, constant_pool);
  }
  INLINE(static void set_target_address_at(
      Address pc, Code* code, Address target,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED)) {
    ConstantPoolArray* constant_pool = code ? code->constant_pool() : NULL;
    set_target_address_at(pc, constant_pool, target, icache_flush_mode);
  }

  // Return the code target address at a call site from the return address
  // of that call in the instruction stream.
  inline static Address target_address_from_return_address(Address pc);

  // Given the address of the beginning of a call, return the address
  // in the instruction stream that the call will return to.
  INLINE(static Address return_address_from_call_start(Address pc));

  // Return the code target address of the patch debug break slot
  INLINE(static Address break_address_from_return_address(Address pc));

  // This sets the branch destination.
  // This is for calls and branches within generated code.
  inline static void deserialization_set_special_target_at(
      Address instruction_payload, Code* code, Address target);

  // Size of an instruction.
  static const int kInstrSize = sizeof(Instr);

  // Here we are patching the address in the LUI/ORI instruction pair.
  // These values are used in the serialization process and must be zero for
  // PPC platform, as Code, Embedded Object or External-reference pointers
  // are split across two consecutive instructions and don't exist separately
  // in the code, so the serializer should not step forwards in memory after
  // a target is resolved and written.
  static const int kSpecialTargetSize = 0;

// Number of instructions to load an address via a mov sequence.
#if V8_TARGET_ARCH_PPC64
  static const int kMovInstructionsConstantPool = 2;
  static const int kMovInstructionsNoConstantPool = 5;
#else
  static const int kMovInstructionsConstantPool = 1;
  static const int kMovInstructionsNoConstantPool = 2;
#endif
#if V8_OOL_CONSTANT_POOL
  static const int kMovInstructions = kMovInstructionsConstantPool;
#else
  static const int kMovInstructions = kMovInstructionsNoConstantPool;
#endif

  // Distance between the instruction referring to the address of the call
  // target and the return address.

  // Call sequence is a FIXED_SEQUENCE:
  // mov     r8, @ call address
  // mtlr    r8
  // blrl
  //                      @ return address
  static const int kCallTargetAddressOffset =
      (kMovInstructions + 2) * kInstrSize;

  // Distance between start of patched return sequence and the emitted address
  // to jump to.
  // Patched return sequence is a FIXED_SEQUENCE:
  //   mov r0, <address>
  //   mtlr r0
  //   blrl
  static const int kPatchReturnSequenceAddressOffset = 0 * kInstrSize;

  // Distance between start of patched debug break slot and the emitted address
  // to jump to.
  // Patched debug break slot code is a FIXED_SEQUENCE:
  //   mov r0, <address>
  //   mtlr r0
  //   blrl
  static const int kPatchDebugBreakSlotAddressOffset = 0 * kInstrSize;

  // This is the length of the BreakLocationIterator::SetDebugBreakAtReturn()
  // code patch FIXED_SEQUENCE
  static const int kJSReturnSequenceInstructions =
      kMovInstructionsNoConstantPool + 3;

  // This is the length of the code sequence from SetDebugBreakAtSlot()
  // FIXED_SEQUENCE
  static const int kDebugBreakSlotInstructions =
      kMovInstructionsNoConstantPool + 2;
  static const int kDebugBreakSlotLength =
      kDebugBreakSlotInstructions * kInstrSize;

  static inline int encode_crbit(const CRegister& cr, enum CRBit crbit) {
    return ((cr.code() * CRWIDTH) + crbit);
  }

  // ---------------------------------------------------------------------------
  // Code generation

  // Insert the smallest number of nop instructions
  // possible to align the pc offset to a multiple
  // of m. m must be a power of 2 (>= 4).
  void Align(int m);
  // Aligns code to something that's optimal for a jump target for the platform.
  void CodeTargetAlign();

  // Branch instructions
  void bclr(BOfield bo, LKBit lk);
  void blr();
  void bc(int branch_offset, BOfield bo, int condition_bit, LKBit lk = LeaveLK);
  void b(int branch_offset, LKBit lk);

  void bcctr(BOfield bo, LKBit lk);
  void bctr();
  void bctrl();

  // Convenience branch instructions using labels
  void b(Label* L, LKBit lk = LeaveLK) { b(branch_offset(L, false), lk); }

  void bc_short(Condition cond, Label* L, CRegister cr = cr7,
                LKBit lk = LeaveLK) {
    DCHECK(cond != al);
    DCHECK(cr.code() >= 0 && cr.code() <= 7);

    int b_offset = branch_offset(L, false);

    switch (cond) {
      case eq:
        bc(b_offset, BT, encode_crbit(cr, CR_EQ), lk);
        break;
      case ne:
        bc(b_offset, BF, encode_crbit(cr, CR_EQ), lk);
        break;
      case gt:
        bc(b_offset, BT, encode_crbit(cr, CR_GT), lk);
        break;
      case le:
        bc(b_offset, BF, encode_crbit(cr, CR_GT), lk);
        break;
      case lt:
        bc(b_offset, BT, encode_crbit(cr, CR_LT), lk);
        break;
      case ge:
        bc(b_offset, BF, encode_crbit(cr, CR_LT), lk);
        break;
      case unordered:
        bc(b_offset, BT, encode_crbit(cr, CR_FU), lk);
        break;
      case ordered:
        bc(b_offset, BF, encode_crbit(cr, CR_FU), lk);
        break;
      case overflow:
        bc(b_offset, BT, encode_crbit(cr, CR_SO), lk);
        break;
      case nooverflow:
        bc(b_offset, BF, encode_crbit(cr, CR_SO), lk);
        break;
      default:
        UNIMPLEMENTED();
    }
  }

  void isel(Register rt, Register ra, Register rb, int cb);
  void isel(Condition cond, Register rt, Register ra, Register rb,
            CRegister cr = cr7) {
    DCHECK(cond != al);
    DCHECK(cr.code() >= 0 && cr.code() <= 7);

    switch (cond) {
      case eq:
        isel(rt, ra, rb, encode_crbit(cr, CR_EQ));
        break;
      case ne:
        isel(rt, rb, ra, encode_crbit(cr, CR_EQ));
        break;
      case gt:
        isel(rt, ra, rb, encode_crbit(cr, CR_GT));
        break;
      case le:
        isel(rt, rb, ra, encode_crbit(cr, CR_GT));
        break;
      case lt:
        isel(rt, ra, rb, encode_crbit(cr, CR_LT));
        break;
      case ge:
        isel(rt, rb, ra, encode_crbit(cr, CR_LT));
        break;
      case unordered:
        isel(rt, ra, rb, encode_crbit(cr, CR_FU));
        break;
      case ordered:
        isel(rt, rb, ra, encode_crbit(cr, CR_FU));
        break;
      case overflow:
        isel(rt, ra, rb, encode_crbit(cr, CR_SO));
        break;
      case nooverflow:
        isel(rt, rb, ra, encode_crbit(cr, CR_SO));
        break;
      default:
        UNIMPLEMENTED();
    }
  }

  void b(Condition cond, Label* L, CRegister cr = cr7, LKBit lk = LeaveLK) {
    if (cond == al) {
      b(L, lk);
      return;
    }

    if ((L->is_bound() && is_near(L, cond)) || !is_trampoline_emitted()) {
      bc_short(cond, L, cr, lk);
      return;
    }

    Label skip;
    Condition neg_cond = NegateCondition(cond);
    bc_short(neg_cond, &skip, cr);
    b(L, lk);
    bind(&skip);
  }

  void bne(Label* L, CRegister cr = cr7, LKBit lk = LeaveLK) {
    b(ne, L, cr, lk);
  }
  void beq(Label* L, CRegister cr = cr7, LKBit lk = LeaveLK) {
    b(eq, L, cr, lk);
  }
  void blt(Label* L, CRegister cr = cr7, LKBit lk = LeaveLK) {
    b(lt, L, cr, lk);
  }
  void bge(Label* L, CRegister cr = cr7, LKBit lk = LeaveLK) {
    b(ge, L, cr, lk);
  }
  void ble(Label* L, CRegister cr = cr7, LKBit lk = LeaveLK) {
    b(le, L, cr, lk);
  }
  void bgt(Label* L, CRegister cr = cr7, LKBit lk = LeaveLK) {
    b(gt, L, cr, lk);
  }
  void bunordered(Label* L, CRegister cr = cr7, LKBit lk = LeaveLK) {
    b(unordered, L, cr, lk);
  }
  void bordered(Label* L, CRegister cr = cr7, LKBit lk = LeaveLK) {
    b(ordered, L, cr, lk);
  }
  void boverflow(Label* L, CRegister cr = cr0, LKBit lk = LeaveLK) {
    b(overflow, L, cr, lk);
  }
  void bnooverflow(Label* L, CRegister cr = cr0, LKBit lk = LeaveLK) {
    b(nooverflow, L, cr, lk);
  }

  // Decrement CTR; branch if CTR != 0
  void bdnz(Label* L, LKBit lk = LeaveLK) {
    bc(branch_offset(L, false), DCBNZ, 0, lk);
  }

  // Data-processing instructions

  void sub(Register dst, Register src1, Register src2, OEBit s = LeaveOE,
           RCBit r = LeaveRC);

  void subfic(Register dst, Register src, const Operand& imm);

  void subfc(Register dst, Register src1, Register src2, OEBit s = LeaveOE,
             RCBit r = LeaveRC);

  void add(Register dst, Register src1, Register src2, OEBit s = LeaveOE,
           RCBit r = LeaveRC);

  void addc(Register dst, Register src1, Register src2, OEBit o = LeaveOE,
            RCBit r = LeaveRC);

  void addze(Register dst, Register src1, OEBit o, RCBit r);

  void mullw(Register dst, Register src1, Register src2, OEBit o = LeaveOE,
             RCBit r = LeaveRC);

  void mulhw(Register dst, Register src1, Register src2, RCBit r = LeaveRC);
  void mulhwu(Register dst, Register src1, Register src2, RCBit r = LeaveRC);

  void divw(Register dst, Register src1, Register src2, OEBit o = LeaveOE,
            RCBit r = LeaveRC);
  void divwu(Register dst, Register src1, Register src2, OEBit o = LeaveOE,
             RCBit r = LeaveRC);

  void addi(Register dst, Register src, const Operand& imm);
  void addis(Register dst, Register src, const Operand& imm);
  void addic(Register dst, Register src, const Operand& imm);

  void and_(Register dst, Register src1, Register src2, RCBit rc = LeaveRC);
  void andc(Register dst, Register src1, Register src2, RCBit rc = LeaveRC);
  void andi(Register ra, Register rs, const Operand& imm);
  void andis(Register ra, Register rs, const Operand& imm);
  void nor(Register dst, Register src1, Register src2, RCBit r = LeaveRC);
  void notx(Register dst, Register src, RCBit r = LeaveRC);
  void ori(Register dst, Register src, const Operand& imm);
  void oris(Register dst, Register src, const Operand& imm);
  void orx(Register dst, Register src1, Register src2, RCBit rc = LeaveRC);
  void orc(Register dst, Register src1, Register src2, RCBit rc = LeaveRC);
  void xori(Register dst, Register src, const Operand& imm);
  void xoris(Register ra, Register rs, const Operand& imm);
  void xor_(Register dst, Register src1, Register src2, RCBit rc = LeaveRC);
  void cmpi(Register src1, const Operand& src2, CRegister cr = cr7);
  void cmpli(Register src1, const Operand& src2, CRegister cr = cr7);
  void cmpwi(Register src1, const Operand& src2, CRegister cr = cr7);
  void cmplwi(Register src1, const Operand& src2, CRegister cr = cr7);
  void li(Register dst, const Operand& src);
  void lis(Register dst, const Operand& imm);
  void mr(Register dst, Register src);

  void lbz(Register dst, const MemOperand& src);
  void lbzx(Register dst, const MemOperand& src);
  void lbzux(Register dst, const MemOperand& src);
  void lhz(Register dst, const MemOperand& src);
  void lhzx(Register dst, const MemOperand& src);
  void lhzux(Register dst, const MemOperand& src);
  void lha(Register dst, const MemOperand& src);
  void lhax(Register dst, const MemOperand& src);
  void lwz(Register dst, const MemOperand& src);
  void lwzu(Register dst, const MemOperand& src);
  void lwzx(Register dst, const MemOperand& src);
  void lwzux(Register dst, const MemOperand& src);
  void lwa(Register dst, const MemOperand& src);
  void lwax(Register dst, const MemOperand& src);
  void stb(Register dst, const MemOperand& src);
  void stbx(Register dst, const MemOperand& src);
  void stbux(Register dst, const MemOperand& src);
  void sth(Register dst, const MemOperand& src);
  void sthx(Register dst, const MemOperand& src);
  void sthux(Register dst, const MemOperand& src);
  void stw(Register dst, const MemOperand& src);
  void stwu(Register dst, const MemOperand& src);
  void stwx(Register rs, const MemOperand& src);
  void stwux(Register rs, const MemOperand& src);

  void extsb(Register rs, Register ra, RCBit r = LeaveRC);
  void extsh(Register rs, Register ra, RCBit r = LeaveRC);
  void extsw(Register rs, Register ra, RCBit r = LeaveRC);

  void neg(Register rt, Register ra, OEBit o = LeaveOE, RCBit c = LeaveRC);

#if V8_TARGET_ARCH_PPC64
  void ld(Register rd, const MemOperand& src);
  void ldx(Register rd, const MemOperand& src);
  void ldu(Register rd, const MemOperand& src);
  void ldux(Register rd, const MemOperand& src);
  void std(Register rs, const MemOperand& src);
  void stdx(Register rs, const MemOperand& src);
  void stdu(Register rs, const MemOperand& src);
  void stdux(Register rs, const MemOperand& src);
  void rldic(Register dst, Register src, int sh, int mb, RCBit r = LeaveRC);
  void rldicl(Register dst, Register src, int sh, int mb, RCBit r = LeaveRC);
  void rldcl(Register ra, Register rs, Register rb, int mb, RCBit r = LeaveRC);
  void rldicr(Register dst, Register src, int sh, int me, RCBit r = LeaveRC);
  void rldimi(Register dst, Register src, int sh, int mb, RCBit r = LeaveRC);
  void sldi(Register dst, Register src, const Operand& val, RCBit rc = LeaveRC);
  void srdi(Register dst, Register src, const Operand& val, RCBit rc = LeaveRC);
  void clrrdi(Register dst, Register src, const Operand& val,
              RCBit rc = LeaveRC);
  void clrldi(Register dst, Register src, const Operand& val,
              RCBit rc = LeaveRC);
  void sradi(Register ra, Register rs, int sh, RCBit r = LeaveRC);
  void srd(Register dst, Register src1, Register src2, RCBit r = LeaveRC);
  void sld(Register dst, Register src1, Register src2, RCBit r = LeaveRC);
  void srad(Register dst, Register src1, Register src2, RCBit r = LeaveRC);
  void rotld(Register ra, Register rs, Register rb, RCBit r = LeaveRC);
  void rotldi(Register ra, Register rs, int sh, RCBit r = LeaveRC);
  void rotrdi(Register ra, Register rs, int sh, RCBit r = LeaveRC);
  void cntlzd_(Register dst, Register src, RCBit rc = LeaveRC);
  void mulld(Register dst, Register src1, Register src2, OEBit o = LeaveOE,
             RCBit r = LeaveRC);
  void divd(Register dst, Register src1, Register src2, OEBit o = LeaveOE,
            RCBit r = LeaveRC);
  void divdu(Register dst, Register src1, Register src2, OEBit o = LeaveOE,
             RCBit r = LeaveRC);
#endif

  void rlwinm(Register ra, Register rs, int sh, int mb, int me,
              RCBit rc = LeaveRC);
  void rlwimi(Register ra, Register rs, int sh, int mb, int me,
              RCBit rc = LeaveRC);
  void rlwnm(Register ra, Register rs, Register rb, int mb, int me,
             RCBit rc = LeaveRC);
  void slwi(Register dst, Register src, const Operand& val, RCBit rc = LeaveRC);
  void srwi(Register dst, Register src, const Operand& val, RCBit rc = LeaveRC);
  void clrrwi(Register dst, Register src, const Operand& val,
              RCBit rc = LeaveRC);
  void clrlwi(Register dst, Register src, const Operand& val,
              RCBit rc = LeaveRC);
  void srawi(Register ra, Register rs, int sh, RCBit r = LeaveRC);
  void srw(Register dst, Register src1, Register src2, RCBit r = LeaveRC);
  void slw(Register dst, Register src1, Register src2, RCBit r = LeaveRC);
  void sraw(Register dst, Register src1, Register src2, RCBit r = LeaveRC);
  void rotlw(Register ra, Register rs, Register rb, RCBit r = LeaveRC);
  void rotlwi(Register ra, Register rs, int sh, RCBit r = LeaveRC);
  void rotrwi(Register ra, Register rs, int sh, RCBit r = LeaveRC);

  void cntlzw_(Register dst, Register src, RCBit rc = LeaveRC);

  void subi(Register dst, Register src1, const Operand& src2);

  void cmp(Register src1, Register src2, CRegister cr = cr7);
  void cmpl(Register src1, Register src2, CRegister cr = cr7);
  void cmpw(Register src1, Register src2, CRegister cr = cr7);
  void cmplw(Register src1, Register src2, CRegister cr = cr7);

  void mov(Register dst, const Operand& src);

  // Load the position of the label relative to the generated code object
  // pointer in a register.
  void mov_label_offset(Register dst, Label* label);

  // Multiply instructions
  void mul(Register dst, Register src1, Register src2, OEBit s = LeaveOE,
           RCBit r = LeaveRC);

  // Miscellaneous arithmetic instructions

  // Special register access
  void crxor(int bt, int ba, int bb);
  void crclr(int bt) { crxor(bt, bt, bt); }
  void creqv(int bt, int ba, int bb);
  void crset(int bt) { creqv(bt, bt, bt); }
  void mflr(Register dst);
  void mtlr(Register src);
  void mtctr(Register src);
  void mtxer(Register src);
  void mcrfs(int bf, int bfa);
  void mfcr(Register dst);
#if V8_TARGET_ARCH_PPC64
  void mffprd(Register dst, DoubleRegister src);
  void mffprwz(Register dst, DoubleRegister src);
  void mtfprd(DoubleRegister dst, Register src);
  void mtfprwz(DoubleRegister dst, Register src);
  void mtfprwa(DoubleRegister dst, Register src);
#endif

  void function_descriptor();

  // Exception-generating instructions and debugging support
  void stop(const char* msg, Condition cond = al,
            int32_t code = kDefaultStopCode, CRegister cr = cr7);

  void bkpt(uint32_t imm16);  // v5 and above

  void dcbf(Register ra, Register rb);
  void sync();
  void lwsync();
  void icbi(Register ra, Register rb);
  void isync();

  // Support for floating point
  void lfd(const DoubleRegister frt, const MemOperand& src);
  void lfdu(const DoubleRegister frt, const MemOperand& src);
  void lfdx(const DoubleRegister frt, const MemOperand& src);
  void lfdux(const DoubleRegister frt, const MemOperand& src);
  void lfs(const DoubleRegister frt, const MemOperand& src);
  void lfsu(const DoubleRegister frt, const MemOperand& src);
  void lfsx(const DoubleRegister frt, const MemOperand& src);
  void lfsux(const DoubleRegister frt, const MemOperand& src);
  void stfd(const DoubleRegister frs, const MemOperand& src);
  void stfdu(const DoubleRegister frs, const MemOperand& src);
  void stfdx(const DoubleRegister frs, const MemOperand& src);
  void stfdux(const DoubleRegister frs, const MemOperand& src);
  void stfs(const DoubleRegister frs, const MemOperand& src);
  void stfsu(const DoubleRegister frs, const MemOperand& src);
  void stfsx(const DoubleRegister frs, const MemOperand& src);
  void stfsux(const DoubleRegister frs, const MemOperand& src);

  void fadd(const DoubleRegister frt, const DoubleRegister fra,
            const DoubleRegister frb, RCBit rc = LeaveRC);
  void fsub(const DoubleRegister frt, const DoubleRegister fra,
            const DoubleRegister frb, RCBit rc = LeaveRC);
  void fdiv(const DoubleRegister frt, const DoubleRegister fra,
            const DoubleRegister frb, RCBit rc = LeaveRC);
  void fmul(const DoubleRegister frt, const DoubleRegister fra,
            const DoubleRegister frc, RCBit rc = LeaveRC);
  void fcmpu(const DoubleRegister fra, const DoubleRegister frb,
             CRegister cr = cr7);
  void fmr(const DoubleRegister frt, const DoubleRegister frb,
           RCBit rc = LeaveRC);
  void fctiwz(const DoubleRegister frt, const DoubleRegister frb);
  void fctiw(const DoubleRegister frt, const DoubleRegister frb);
  void frin(const DoubleRegister frt, const DoubleRegister frb,
            RCBit rc = LeaveRC);
  void friz(const DoubleRegister frt, const DoubleRegister frb,
            RCBit rc = LeaveRC);
  void frip(const DoubleRegister frt, const DoubleRegister frb,
            RCBit rc = LeaveRC);
  void frim(const DoubleRegister frt, const DoubleRegister frb,
            RCBit rc = LeaveRC);
  void frsp(const DoubleRegister frt, const DoubleRegister frb,
            RCBit rc = LeaveRC);
  void fcfid(const DoubleRegister frt, const DoubleRegister frb,
             RCBit rc = LeaveRC);
  void fctid(const DoubleRegister frt, const DoubleRegister frb,
             RCBit rc = LeaveRC);
  void fctidz(const DoubleRegister frt, const DoubleRegister frb,
              RCBit rc = LeaveRC);
  void fsel(const DoubleRegister frt, const DoubleRegister fra,
            const DoubleRegister frc, const DoubleRegister frb,
            RCBit rc = LeaveRC);
  void fneg(const DoubleRegister frt, const DoubleRegister frb,
            RCBit rc = LeaveRC);
  void mtfsfi(int bf, int immediate, RCBit rc = LeaveRC);
  void mffs(const DoubleRegister frt, RCBit rc = LeaveRC);
  void mtfsf(const DoubleRegister frb, bool L = 1, int FLM = 0, bool W = 0,
             RCBit rc = LeaveRC);
  void fsqrt(const DoubleRegister frt, const DoubleRegister frb,
             RCBit rc = LeaveRC);
  void fabs(const DoubleRegister frt, const DoubleRegister frb,
            RCBit rc = LeaveRC);
  void fmadd(const DoubleRegister frt, const DoubleRegister fra,
             const DoubleRegister frc, const DoubleRegister frb,
             RCBit rc = LeaveRC);
  void fmsub(const DoubleRegister frt, const DoubleRegister fra,
             const DoubleRegister frc, const DoubleRegister frb,
             RCBit rc = LeaveRC);

  // Pseudo instructions

  // Different nop operations are used by the code generator to detect certain
  // states of the generated code.
  enum NopMarkerTypes {
    NON_MARKING_NOP = 0,
    GROUP_ENDING_NOP,
    DEBUG_BREAK_NOP,
    // IC markers.
    PROPERTY_ACCESS_INLINED,
    PROPERTY_ACCESS_INLINED_CONTEXT,
    PROPERTY_ACCESS_INLINED_CONTEXT_DONT_DELETE,
    // Helper values.
    LAST_CODE_MARKER,
    FIRST_IC_MARKER = PROPERTY_ACCESS_INLINED
  };

  void nop(int type = 0);  // 0 is the default non-marking type.

  void push(Register src) {
#if V8_TARGET_ARCH_PPC64
    stdu(src, MemOperand(sp, -kPointerSize));
#else
    stwu(src, MemOperand(sp, -kPointerSize));
#endif
  }

  void pop(Register dst) {
#if V8_TARGET_ARCH_PPC64
    ld(dst, MemOperand(sp));
#else
    lwz(dst, MemOperand(sp));
#endif
    addi(sp, sp, Operand(kPointerSize));
  }

  void pop() { addi(sp, sp, Operand(kPointerSize)); }

  // Jump unconditionally to given label.
  void jmp(Label* L) { b(L); }

  // Check the code size generated from label to here.
  int SizeOfCodeGeneratedSince(Label* label) {
    return pc_offset() - label->pos();
  }

  // Check the number of instructions generated from label to here.
  int InstructionsGeneratedSince(Label* label) {
    return SizeOfCodeGeneratedSince(label) / kInstrSize;
  }

  // Class for scoping postponing the trampoline pool generation.
  class BlockTrampolinePoolScope {
   public:
    explicit BlockTrampolinePoolScope(Assembler* assem) : assem_(assem) {
      assem_->StartBlockTrampolinePool();
    }
    ~BlockTrampolinePoolScope() { assem_->EndBlockTrampolinePool(); }

   private:
    Assembler* assem_;

    DISALLOW_IMPLICIT_CONSTRUCTORS(BlockTrampolinePoolScope);
  };

  // Debugging

  // Mark address of the ExitJSFrame code.
  void RecordJSReturn();

  // Mark address of a debug break slot.
  void RecordDebugBreakSlot();

  // Record the AST id of the CallIC being compiled, so that it can be placed
  // in the relocation information.
  void SetRecordedAstId(TypeFeedbackId ast_id) {
    // Causes compiler to fail
    // DCHECK(recorded_ast_id_.IsNone());
    recorded_ast_id_ = ast_id;
  }

  TypeFeedbackId RecordedAstId() {
    // Causes compiler to fail
    // DCHECK(!recorded_ast_id_.IsNone());
    return recorded_ast_id_;
  }

  void ClearRecordedAstId() { recorded_ast_id_ = TypeFeedbackId::None(); }

  // Record a comment relocation entry that can be used by a disassembler.
  // Use --code-comments to enable.
  void RecordComment(const char* msg);

  // Record a deoptimization reason that can be used by a log or cpu profiler.
  // Use --trace-deopt to enable.
  void RecordDeoptReason(const int reason, const int raw_position);

  // Writes a single byte or word of data in the code stream.  Used
  // for inline tables, e.g., jump-tables.
  void db(uint8_t data);
  void dd(uint32_t data);
  void emit_ptr(uintptr_t data);

  PositionsRecorder* positions_recorder() { return &positions_recorder_; }

  // Read/patch instructions
  Instr instr_at(int pos) { return *reinterpret_cast<Instr*>(buffer_ + pos); }
  void instr_at_put(int pos, Instr instr) {
    *reinterpret_cast<Instr*>(buffer_ + pos) = instr;
  }
  static Instr instr_at(byte* pc) { return *reinterpret_cast<Instr*>(pc); }
  static void instr_at_put(byte* pc, Instr instr) {
    *reinterpret_cast<Instr*>(pc) = instr;
  }
  static Condition GetCondition(Instr instr);

  static bool IsLis(Instr instr);
  static bool IsLi(Instr instr);
  static bool IsAddic(Instr instr);
  static bool IsOri(Instr instr);

  static bool IsBranch(Instr instr);
  static Register GetRA(Instr instr);
  static Register GetRB(Instr instr);
#if V8_TARGET_ARCH_PPC64
  static bool Is64BitLoadIntoR12(Instr instr1, Instr instr2, Instr instr3,
                                 Instr instr4, Instr instr5);
#else
  static bool Is32BitLoadIntoR12(Instr instr1, Instr instr2);
#endif

  static bool IsCmpRegister(Instr instr);
  static bool IsCmpImmediate(Instr instr);
  static bool IsRlwinm(Instr instr);
#if V8_TARGET_ARCH_PPC64
  static bool IsRldicl(Instr instr);
#endif
  static bool IsCrSet(Instr instr);
  static Register GetCmpImmediateRegister(Instr instr);
  static int GetCmpImmediateRawImmediate(Instr instr);
  static bool IsNop(Instr instr, int type = NON_MARKING_NOP);

  // Postpone the generation of the trampoline pool for the specified number of
  // instructions.
  void BlockTrampolinePoolFor(int instructions);
  void CheckTrampolinePool();

  int instructions_required_for_mov(const Operand& x) const;

#if V8_OOL_CONSTANT_POOL
  // Decide between using the constant pool vs. a mov immediate sequence.
  bool use_constant_pool_for_mov(const Operand& x, bool canOptimize) const;

  // The code currently calls CheckBuffer() too often. This has the side
  // effect of randomly growing the buffer in the middle of multi-instruction
  // sequences.
  // MacroAssembler::LoadConstantPoolPointerRegister() includes a relocation
  // and multiple instructions. We cannot grow the buffer until the
  // relocation and all of the instructions are written.
  //
  // This function allows outside callers to check and grow the buffer
  void EnsureSpaceFor(int space_needed);
#endif

  // Allocate a constant pool of the correct size for the generated code.
  Handle<ConstantPoolArray> NewConstantPool(Isolate* isolate);

  // Generate the constant pool for the generated code.
  void PopulateConstantPool(ConstantPoolArray* constant_pool);

#if V8_OOL_CONSTANT_POOL
  bool is_constant_pool_full() const {
    return constant_pool_builder_.is_full();
  }

  bool use_extended_constant_pool() const {
    return constant_pool_builder_.current_section() ==
           ConstantPoolArray::EXTENDED_SECTION;
  }
#endif

#if ABI_USES_FUNCTION_DESCRIPTORS || V8_OOL_CONSTANT_POOL
  static void RelocateInternalReference(
      Address pc, intptr_t delta, Address code_start,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED);
  static int DecodeInternalReference(Vector<char> buffer, Address pc);
#endif

 protected:
  // Relocation for a type-recording IC has the AST id added to it.  This
  // member variable is a way to pass the information from the call site to
  // the relocation info.
  TypeFeedbackId recorded_ast_id_;

  int buffer_space() const { return reloc_info_writer.pos() - pc_; }

  // Decode branch instruction at pos and return branch target pos
  int target_at(int pos);

  // Patch branch instruction at pos to branch to given branch target pos
  void target_at_put(int pos, int target_pos);

  // Record reloc info for current pc_
  void RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data = 0);
  void RecordRelocInfo(const RelocInfo& rinfo);
#if V8_OOL_CONSTANT_POOL
  ConstantPoolArray::LayoutSection ConstantPoolAddEntry(
      const RelocInfo& rinfo) {
    return constant_pool_builder_.AddEntry(this, rinfo);
  }
#endif

  // Block the emission of the trampoline pool before pc_offset.
  void BlockTrampolinePoolBefore(int pc_offset) {
    if (no_trampoline_pool_before_ < pc_offset)
      no_trampoline_pool_before_ = pc_offset;
  }

  void StartBlockTrampolinePool() { trampoline_pool_blocked_nesting_++; }

  void EndBlockTrampolinePool() { trampoline_pool_blocked_nesting_--; }

  bool is_trampoline_pool_blocked() const {
    return trampoline_pool_blocked_nesting_ > 0;
  }

  bool has_exception() const { return internal_trampoline_exception_; }

  bool is_trampoline_emitted() const { return trampoline_emitted_; }

 private:
  // Code generation
  // The relocation writer's position is at least kGap bytes below the end of
  // the generated instructions. This is so that multi-instruction sequences do
  // not have to check for overflow. The same is true for writes of large
  // relocation info entries.
  static const int kGap = 32;

  // Repeated checking whether the trampoline pool should be emitted is rather
  // expensive. By default we only check again once a number of instructions
  // has been generated.
  int next_buffer_check_;  // pc offset of next buffer check.

  // Emission of the trampoline pool may be blocked in some code sequences.
  int trampoline_pool_blocked_nesting_;  // Block emission if this is not zero.
  int no_trampoline_pool_before_;  // Block emission before this pc offset.

  // Relocation info generation
  // Each relocation is encoded as a variable size value
  static const int kMaxRelocSize = RelocInfoWriter::kMaxSize;
  RelocInfoWriter reloc_info_writer;

  // The bound position, before this we cannot do instruction elimination.
  int last_bound_pos_;

#if V8_OOL_CONSTANT_POOL
  ConstantPoolBuilder constant_pool_builder_;
#endif

  // Code emission
  inline void CheckBuffer();
  void GrowBuffer();
  inline void emit(Instr x);
  inline void CheckTrampolinePoolQuick();

  // Instruction generation
  void a_form(Instr instr, DoubleRegister frt, DoubleRegister fra,
              DoubleRegister frb, RCBit r);
  void d_form(Instr instr, Register rt, Register ra, const intptr_t val,
              bool signed_disp);
  void x_form(Instr instr, Register ra, Register rs, Register rb, RCBit r);
  void xo_form(Instr instr, Register rt, Register ra, Register rb, OEBit o,
               RCBit r);
  void md_form(Instr instr, Register ra, Register rs, int shift, int maskbit,
               RCBit r);
  void mds_form(Instr instr, Register ra, Register rs, Register rb, int maskbit,
                RCBit r);

  // Labels
  void print(Label* L);
  int max_reach_from(int pos);
  void bind_to(Label* L, int pos);
  void next(Label* L);

  class Trampoline {
   public:
    Trampoline() {
      next_slot_ = 0;
      free_slot_count_ = 0;
    }
    Trampoline(int start, int slot_count) {
      next_slot_ = start;
      free_slot_count_ = slot_count;
    }
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
    int next_slot_;
    int free_slot_count_;
  };

  int32_t get_trampoline_entry();
  int unbound_labels_count_;
  // If trampoline is emitted, generated code is becoming large. As
  // this is already a slow case which can possibly break our code
  // generation for the extreme case, we use this information to
  // trigger different mode of branch instruction generation, where we
  // no longer use a single branch instruction.
  bool trampoline_emitted_;
  static const int kTrampolineSlotsSize = kInstrSize;
  static const int kMaxCondBranchReach = (1 << (16 - 1)) - 1;
  static const int kMaxBlockTrampolineSectionSize = 64 * kInstrSize;
  static const int kInvalidSlotPos = -1;

  Trampoline trampoline_;
  bool internal_trampoline_exception_;

  friend class RegExpMacroAssemblerPPC;
  friend class RelocInfo;
  friend class CodePatcher;
  friend class BlockTrampolinePoolScope;
  PositionsRecorder positions_recorder_;
  friend class PositionsRecorder;
  friend class EnsureSpace;
};


class EnsureSpace BASE_EMBEDDED {
 public:
  explicit EnsureSpace(Assembler* assembler) { assembler->CheckBuffer(); }
};
}
}  // namespace v8::internal

#endif  // V8_PPC_ASSEMBLER_PPC_H_
