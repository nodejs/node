// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ARM64_ASSEMBLER_ARM64_INL_H_
#define V8_ARM64_ASSEMBLER_ARM64_INL_H_

#include "src/arm64/assembler-arm64.h"
#include "src/assembler.h"
#include "src/debug.h"


namespace v8 {
namespace internal {


bool CpuFeatures::SupportsCrankshaft() { return true; }


void RelocInfo::apply(intptr_t delta, ICacheFlushMode icache_flush_mode) {
  UNIMPLEMENTED();
}


void RelocInfo::set_target_address(Address target,
                                   WriteBarrierMode write_barrier_mode,
                                   ICacheFlushMode icache_flush_mode) {
  DCHECK(IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_));
  Assembler::set_target_address_at(pc_, host_, target, icache_flush_mode);
  if (write_barrier_mode == UPDATE_WRITE_BARRIER && host() != NULL &&
      IsCodeTarget(rmode_)) {
    Object* target_code = Code::GetCodeFromTargetAddress(target);
    host()->GetHeap()->incremental_marking()->RecordWriteIntoCode(
        host(), this, HeapObject::cast(target_code));
  }
}


inline unsigned CPURegister::code() const {
  DCHECK(IsValid());
  return reg_code;
}


inline CPURegister::RegisterType CPURegister::type() const {
  DCHECK(IsValidOrNone());
  return reg_type;
}


inline RegList CPURegister::Bit() const {
  DCHECK(reg_code < (sizeof(RegList) * kBitsPerByte));
  return IsValid() ? 1UL << reg_code : 0;
}


inline unsigned CPURegister::SizeInBits() const {
  DCHECK(IsValid());
  return reg_size;
}


inline int CPURegister::SizeInBytes() const {
  DCHECK(IsValid());
  DCHECK(SizeInBits() % 8 == 0);
  return reg_size / 8;
}


inline bool CPURegister::Is32Bits() const {
  DCHECK(IsValid());
  return reg_size == 32;
}


inline bool CPURegister::Is64Bits() const {
  DCHECK(IsValid());
  return reg_size == 64;
}


inline bool CPURegister::IsValid() const {
  if (IsValidRegister() || IsValidFPRegister()) {
    DCHECK(!IsNone());
    return true;
  } else {
    DCHECK(IsNone());
    return false;
  }
}


inline bool CPURegister::IsValidRegister() const {
  return IsRegister() &&
         ((reg_size == kWRegSizeInBits) || (reg_size == kXRegSizeInBits)) &&
         ((reg_code < kNumberOfRegisters) || (reg_code == kSPRegInternalCode));
}


inline bool CPURegister::IsValidFPRegister() const {
  return IsFPRegister() &&
         ((reg_size == kSRegSizeInBits) || (reg_size == kDRegSizeInBits)) &&
         (reg_code < kNumberOfFPRegisters);
}


inline bool CPURegister::IsNone() const {
  // kNoRegister types should always have size 0 and code 0.
  DCHECK((reg_type != kNoRegister) || (reg_code == 0));
  DCHECK((reg_type != kNoRegister) || (reg_size == 0));

  return reg_type == kNoRegister;
}


inline bool CPURegister::Is(const CPURegister& other) const {
  DCHECK(IsValidOrNone() && other.IsValidOrNone());
  return Aliases(other) && (reg_size == other.reg_size);
}


inline bool CPURegister::Aliases(const CPURegister& other) const {
  DCHECK(IsValidOrNone() && other.IsValidOrNone());
  return (reg_code == other.reg_code) && (reg_type == other.reg_type);
}


inline bool CPURegister::IsRegister() const {
  return reg_type == kRegister;
}


inline bool CPURegister::IsFPRegister() const {
  return reg_type == kFPRegister;
}


inline bool CPURegister::IsSameSizeAndType(const CPURegister& other) const {
  return (reg_size == other.reg_size) && (reg_type == other.reg_type);
}


inline bool CPURegister::IsValidOrNone() const {
  return IsValid() || IsNone();
}


inline bool CPURegister::IsZero() const {
  DCHECK(IsValid());
  return IsRegister() && (reg_code == kZeroRegCode);
}


inline bool CPURegister::IsSP() const {
  DCHECK(IsValid());
  return IsRegister() && (reg_code == kSPRegInternalCode);
}


inline void CPURegList::Combine(const CPURegList& other) {
  DCHECK(IsValid());
  DCHECK(other.type() == type_);
  DCHECK(other.RegisterSizeInBits() == size_);
  list_ |= other.list();
}


inline void CPURegList::Remove(const CPURegList& other) {
  DCHECK(IsValid());
  if (other.type() == type_) {
    list_ &= ~other.list();
  }
}


inline void CPURegList::Combine(const CPURegister& other) {
  DCHECK(other.type() == type_);
  DCHECK(other.SizeInBits() == size_);
  Combine(other.code());
}


inline void CPURegList::Remove(const CPURegister& other1,
                               const CPURegister& other2,
                               const CPURegister& other3,
                               const CPURegister& other4) {
  if (!other1.IsNone() && (other1.type() == type_)) Remove(other1.code());
  if (!other2.IsNone() && (other2.type() == type_)) Remove(other2.code());
  if (!other3.IsNone() && (other3.type() == type_)) Remove(other3.code());
  if (!other4.IsNone() && (other4.type() == type_)) Remove(other4.code());
}


inline void CPURegList::Combine(int code) {
  DCHECK(IsValid());
  DCHECK(CPURegister::Create(code, size_, type_).IsValid());
  list_ |= (1UL << code);
}


inline void CPURegList::Remove(int code) {
  DCHECK(IsValid());
  DCHECK(CPURegister::Create(code, size_, type_).IsValid());
  list_ &= ~(1UL << code);
}


inline Register Register::XRegFromCode(unsigned code) {
  if (code == kSPRegInternalCode) {
    return csp;
  } else {
    DCHECK(code < kNumberOfRegisters);
    return Register::Create(code, kXRegSizeInBits);
  }
}


inline Register Register::WRegFromCode(unsigned code) {
  if (code == kSPRegInternalCode) {
    return wcsp;
  } else {
    DCHECK(code < kNumberOfRegisters);
    return Register::Create(code, kWRegSizeInBits);
  }
}


inline FPRegister FPRegister::SRegFromCode(unsigned code) {
  DCHECK(code < kNumberOfFPRegisters);
  return FPRegister::Create(code, kSRegSizeInBits);
}


inline FPRegister FPRegister::DRegFromCode(unsigned code) {
  DCHECK(code < kNumberOfFPRegisters);
  return FPRegister::Create(code, kDRegSizeInBits);
}


inline Register CPURegister::W() const {
  DCHECK(IsValidRegister());
  return Register::WRegFromCode(reg_code);
}


inline Register CPURegister::X() const {
  DCHECK(IsValidRegister());
  return Register::XRegFromCode(reg_code);
}


inline FPRegister CPURegister::S() const {
  DCHECK(IsValidFPRegister());
  return FPRegister::SRegFromCode(reg_code);
}


inline FPRegister CPURegister::D() const {
  DCHECK(IsValidFPRegister());
  return FPRegister::DRegFromCode(reg_code);
}


// Immediate.
// Default initializer is for int types
template<typename T>
struct ImmediateInitializer {
  static const bool kIsIntType = true;
  static inline RelocInfo::Mode rmode_for(T) {
    return sizeof(T) == 8 ? RelocInfo::NONE64 : RelocInfo::NONE32;
  }
  static inline int64_t immediate_for(T t) {
    STATIC_ASSERT(sizeof(T) <= 8);
    return t;
  }
};


template<>
struct ImmediateInitializer<Smi*> {
  static const bool kIsIntType = false;
  static inline RelocInfo::Mode rmode_for(Smi* t) {
    return RelocInfo::NONE64;
  }
  static inline int64_t immediate_for(Smi* t) {;
    return reinterpret_cast<int64_t>(t);
  }
};


template<>
struct ImmediateInitializer<ExternalReference> {
  static const bool kIsIntType = false;
  static inline RelocInfo::Mode rmode_for(ExternalReference t) {
    return RelocInfo::EXTERNAL_REFERENCE;
  }
  static inline int64_t immediate_for(ExternalReference t) {;
    return reinterpret_cast<int64_t>(t.address());
  }
};


template<typename T>
Immediate::Immediate(Handle<T> value) {
  InitializeHandle(value);
}


template<typename T>
Immediate::Immediate(T t)
    : value_(ImmediateInitializer<T>::immediate_for(t)),
      rmode_(ImmediateInitializer<T>::rmode_for(t)) {}


template<typename T>
Immediate::Immediate(T t, RelocInfo::Mode rmode)
    : value_(ImmediateInitializer<T>::immediate_for(t)),
      rmode_(rmode) {
  STATIC_ASSERT(ImmediateInitializer<T>::kIsIntType);
}


// Operand.
template<typename T>
Operand::Operand(Handle<T> value) : immediate_(value), reg_(NoReg) {}


template<typename T>
Operand::Operand(T t) : immediate_(t), reg_(NoReg) {}


template<typename T>
Operand::Operand(T t, RelocInfo::Mode rmode)
    : immediate_(t, rmode),
      reg_(NoReg) {}


Operand::Operand(Register reg, Shift shift, unsigned shift_amount)
    : immediate_(0),
      reg_(reg),
      shift_(shift),
      extend_(NO_EXTEND),
      shift_amount_(shift_amount) {
  DCHECK(reg.Is64Bits() || (shift_amount < kWRegSizeInBits));
  DCHECK(reg.Is32Bits() || (shift_amount < kXRegSizeInBits));
  DCHECK(!reg.IsSP());
}


Operand::Operand(Register reg, Extend extend, unsigned shift_amount)
    : immediate_(0),
      reg_(reg),
      shift_(NO_SHIFT),
      extend_(extend),
      shift_amount_(shift_amount) {
  DCHECK(reg.IsValid());
  DCHECK(shift_amount <= 4);
  DCHECK(!reg.IsSP());

  // Extend modes SXTX and UXTX require a 64-bit register.
  DCHECK(reg.Is64Bits() || ((extend != SXTX) && (extend != UXTX)));
}


bool Operand::IsImmediate() const {
  return reg_.Is(NoReg);
}


bool Operand::IsShiftedRegister() const {
  return reg_.IsValid() && (shift_ != NO_SHIFT);
}


bool Operand::IsExtendedRegister() const {
  return reg_.IsValid() && (extend_ != NO_EXTEND);
}


bool Operand::IsZero() const {
  if (IsImmediate()) {
    return ImmediateValue() == 0;
  } else {
    return reg().IsZero();
  }
}


Operand Operand::ToExtendedRegister() const {
  DCHECK(IsShiftedRegister());
  DCHECK((shift_ == LSL) && (shift_amount_ <= 4));
  return Operand(reg_, reg_.Is64Bits() ? UXTX : UXTW, shift_amount_);
}


Immediate Operand::immediate() const {
  DCHECK(IsImmediate());
  return immediate_;
}


int64_t Operand::ImmediateValue() const {
  DCHECK(IsImmediate());
  return immediate_.value();
}


Register Operand::reg() const {
  DCHECK(IsShiftedRegister() || IsExtendedRegister());
  return reg_;
}


Shift Operand::shift() const {
  DCHECK(IsShiftedRegister());
  return shift_;
}


Extend Operand::extend() const {
  DCHECK(IsExtendedRegister());
  return extend_;
}


unsigned Operand::shift_amount() const {
  DCHECK(IsShiftedRegister() || IsExtendedRegister());
  return shift_amount_;
}


Operand Operand::UntagSmi(Register smi) {
  STATIC_ASSERT(kXRegSizeInBits == static_cast<unsigned>(kSmiShift +
                                                         kSmiValueSize));
  DCHECK(smi.Is64Bits());
  return Operand(smi, ASR, kSmiShift);
}


Operand Operand::UntagSmiAndScale(Register smi, int scale) {
  STATIC_ASSERT(kXRegSizeInBits == static_cast<unsigned>(kSmiShift +
                                                         kSmiValueSize));
  DCHECK(smi.Is64Bits());
  DCHECK((scale >= 0) && (scale <= (64 - kSmiValueSize)));
  if (scale > kSmiShift) {
    return Operand(smi, LSL, scale - kSmiShift);
  } else if (scale < kSmiShift) {
    return Operand(smi, ASR, kSmiShift - scale);
  }
  return Operand(smi);
}


MemOperand::MemOperand()
  : base_(NoReg), regoffset_(NoReg), offset_(0), addrmode_(Offset),
    shift_(NO_SHIFT), extend_(NO_EXTEND), shift_amount_(0) {
}


MemOperand::MemOperand(Register base, ptrdiff_t offset, AddrMode addrmode)
  : base_(base), regoffset_(NoReg), offset_(offset), addrmode_(addrmode),
    shift_(NO_SHIFT), extend_(NO_EXTEND), shift_amount_(0) {
  DCHECK(base.Is64Bits() && !base.IsZero());
}


MemOperand::MemOperand(Register base,
                       Register regoffset,
                       Extend extend,
                       unsigned shift_amount)
  : base_(base), regoffset_(regoffset), offset_(0), addrmode_(Offset),
    shift_(NO_SHIFT), extend_(extend), shift_amount_(shift_amount) {
  DCHECK(base.Is64Bits() && !base.IsZero());
  DCHECK(!regoffset.IsSP());
  DCHECK((extend == UXTW) || (extend == SXTW) || (extend == SXTX));

  // SXTX extend mode requires a 64-bit offset register.
  DCHECK(regoffset.Is64Bits() || (extend != SXTX));
}


MemOperand::MemOperand(Register base,
                       Register regoffset,
                       Shift shift,
                       unsigned shift_amount)
  : base_(base), regoffset_(regoffset), offset_(0), addrmode_(Offset),
    shift_(shift), extend_(NO_EXTEND), shift_amount_(shift_amount) {
  DCHECK(base.Is64Bits() && !base.IsZero());
  DCHECK(regoffset.Is64Bits() && !regoffset.IsSP());
  DCHECK(shift == LSL);
}


MemOperand::MemOperand(Register base, const Operand& offset, AddrMode addrmode)
  : base_(base), addrmode_(addrmode) {
  DCHECK(base.Is64Bits() && !base.IsZero());

  if (offset.IsImmediate()) {
    offset_ = offset.ImmediateValue();

    regoffset_ = NoReg;
  } else if (offset.IsShiftedRegister()) {
    DCHECK(addrmode == Offset);

    regoffset_ = offset.reg();
    shift_= offset.shift();
    shift_amount_ = offset.shift_amount();

    extend_ = NO_EXTEND;
    offset_ = 0;

    // These assertions match those in the shifted-register constructor.
    DCHECK(regoffset_.Is64Bits() && !regoffset_.IsSP());
    DCHECK(shift_ == LSL);
  } else {
    DCHECK(offset.IsExtendedRegister());
    DCHECK(addrmode == Offset);

    regoffset_ = offset.reg();
    extend_ = offset.extend();
    shift_amount_ = offset.shift_amount();

    shift_= NO_SHIFT;
    offset_ = 0;

    // These assertions match those in the extended-register constructor.
    DCHECK(!regoffset_.IsSP());
    DCHECK((extend_ == UXTW) || (extend_ == SXTW) || (extend_ == SXTX));
    DCHECK((regoffset_.Is64Bits() || (extend_ != SXTX)));
  }
}

bool MemOperand::IsImmediateOffset() const {
  return (addrmode_ == Offset) && regoffset_.Is(NoReg);
}


bool MemOperand::IsRegisterOffset() const {
  return (addrmode_ == Offset) && !regoffset_.Is(NoReg);
}


bool MemOperand::IsPreIndex() const {
  return addrmode_ == PreIndex;
}


bool MemOperand::IsPostIndex() const {
  return addrmode_ == PostIndex;
}

Operand MemOperand::OffsetAsOperand() const {
  if (IsImmediateOffset()) {
    return offset();
  } else {
    DCHECK(IsRegisterOffset());
    if (extend() == NO_EXTEND) {
      return Operand(regoffset(), shift(), shift_amount());
    } else {
      return Operand(regoffset(), extend(), shift_amount());
    }
  }
}


void Assembler::Unreachable() {
#ifdef USE_SIMULATOR
  debug("UNREACHABLE", __LINE__, BREAK);
#else
  // Crash by branching to 0. lr now points near the fault.
  Emit(BLR | Rn(xzr));
#endif
}


Address Assembler::target_pointer_address_at(Address pc) {
  Instruction* instr = reinterpret_cast<Instruction*>(pc);
  DCHECK(instr->IsLdrLiteralX());
  return reinterpret_cast<Address>(instr->ImmPCOffsetTarget());
}


// Read/Modify the code target address in the branch/call instruction at pc.
Address Assembler::target_address_at(Address pc,
                                     ConstantPoolArray* constant_pool) {
  return Memory::Address_at(target_pointer_address_at(pc));
}


Address Assembler::target_address_at(Address pc, Code* code) {
  ConstantPoolArray* constant_pool = code ? code->constant_pool() : NULL;
  return target_address_at(pc, constant_pool);
}


Address Assembler::target_address_from_return_address(Address pc) {
  // Returns the address of the call target from the return address that will
  // be returned to after a call.
  // Call sequence on ARM64 is:
  //  ldr ip0, #... @ load from literal pool
  //  blr ip0
  Address candidate = pc - 2 * kInstructionSize;
  Instruction* instr = reinterpret_cast<Instruction*>(candidate);
  USE(instr);
  DCHECK(instr->IsLdrLiteralX());
  return candidate;
}


Address Assembler::break_address_from_return_address(Address pc) {
  return pc - Assembler::kPatchDebugBreakSlotReturnOffset;
}


Address Assembler::return_address_from_call_start(Address pc) {
  // The call, generated by MacroAssembler::Call, is one of two possible
  // sequences:
  //
  // Without relocation:
  //  movz  temp, #(target & 0x000000000000ffff)
  //  movk  temp, #(target & 0x00000000ffff0000)
  //  movk  temp, #(target & 0x0000ffff00000000)
  //  blr   temp
  //
  // With relocation:
  //  ldr   temp, =target
  //  blr   temp
  //
  // The return address is immediately after the blr instruction in both cases,
  // so it can be found by adding the call size to the address at the start of
  // the call sequence.
  STATIC_ASSERT(Assembler::kCallSizeWithoutRelocation == 4 * kInstructionSize);
  STATIC_ASSERT(Assembler::kCallSizeWithRelocation == 2 * kInstructionSize);

  Instruction* instr = reinterpret_cast<Instruction*>(pc);
  if (instr->IsMovz()) {
    // Verify the instruction sequence.
    DCHECK(instr->following(1)->IsMovk());
    DCHECK(instr->following(2)->IsMovk());
    DCHECK(instr->following(3)->IsBranchAndLinkToRegister());
    return pc + Assembler::kCallSizeWithoutRelocation;
  } else {
    // Verify the instruction sequence.
    DCHECK(instr->IsLdrLiteralX());
    DCHECK(instr->following(1)->IsBranchAndLinkToRegister());
    return pc + Assembler::kCallSizeWithRelocation;
  }
}


void Assembler::deserialization_set_special_target_at(
    Address constant_pool_entry, Code* code, Address target) {
  Memory::Address_at(constant_pool_entry) = target;
}


void Assembler::set_target_address_at(Address pc,
                                      ConstantPoolArray* constant_pool,
                                      Address target,
                                      ICacheFlushMode icache_flush_mode) {
  Memory::Address_at(target_pointer_address_at(pc)) = target;
  // Intuitively, we would think it is necessary to always flush the
  // instruction cache after patching a target address in the code as follows:
  //   CpuFeatures::FlushICache(pc, sizeof(target));
  // However, on ARM, an instruction is actually patched in the case of
  // embedded constants of the form:
  // ldr   ip, [pc, #...]
  // since the instruction accessing this address in the constant pool remains
  // unchanged, a flush is not required.
}


void Assembler::set_target_address_at(Address pc,
                                      Code* code,
                                      Address target,
                                      ICacheFlushMode icache_flush_mode) {
  ConstantPoolArray* constant_pool = code ? code->constant_pool() : NULL;
  set_target_address_at(pc, constant_pool, target, icache_flush_mode);
}


int RelocInfo::target_address_size() {
  return kPointerSize;
}


Address RelocInfo::target_address() {
  DCHECK(IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_));
  return Assembler::target_address_at(pc_, host_);
}


Address RelocInfo::target_address_address() {
  DCHECK(IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_)
                              || rmode_ == EMBEDDED_OBJECT
                              || rmode_ == EXTERNAL_REFERENCE);
  return Assembler::target_pointer_address_at(pc_);
}


Address RelocInfo::constant_pool_entry_address() {
  DCHECK(IsInConstantPool());
  return Assembler::target_pointer_address_at(pc_);
}


Object* RelocInfo::target_object() {
  DCHECK(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  return reinterpret_cast<Object*>(Assembler::target_address_at(pc_, host_));
}


Handle<Object> RelocInfo::target_object_handle(Assembler* origin) {
  DCHECK(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  return Handle<Object>(reinterpret_cast<Object**>(
      Assembler::target_address_at(pc_, host_)));
}


void RelocInfo::set_target_object(Object* target,
                                  WriteBarrierMode write_barrier_mode,
                                  ICacheFlushMode icache_flush_mode) {
  DCHECK(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  Assembler::set_target_address_at(pc_, host_,
                                   reinterpret_cast<Address>(target),
                                   icache_flush_mode);
  if (write_barrier_mode == UPDATE_WRITE_BARRIER &&
      host() != NULL &&
      target->IsHeapObject()) {
    host()->GetHeap()->incremental_marking()->RecordWrite(
        host(), &Memory::Object_at(pc_), HeapObject::cast(target));
  }
}


Address RelocInfo::target_reference() {
  DCHECK(rmode_ == EXTERNAL_REFERENCE);
  return Assembler::target_address_at(pc_, host_);
}


Address RelocInfo::target_runtime_entry(Assembler* origin) {
  DCHECK(IsRuntimeEntry(rmode_));
  return target_address();
}


void RelocInfo::set_target_runtime_entry(Address target,
                                         WriteBarrierMode write_barrier_mode,
                                         ICacheFlushMode icache_flush_mode) {
  DCHECK(IsRuntimeEntry(rmode_));
  if (target_address() != target) {
    set_target_address(target, write_barrier_mode, icache_flush_mode);
  }
}


Handle<Cell> RelocInfo::target_cell_handle() {
  UNIMPLEMENTED();
  Cell *null_cell = NULL;
  return Handle<Cell>(null_cell);
}


Cell* RelocInfo::target_cell() {
  DCHECK(rmode_ == RelocInfo::CELL);
  return Cell::FromValueAddress(Memory::Address_at(pc_));
}


void RelocInfo::set_target_cell(Cell* cell,
                                WriteBarrierMode write_barrier_mode,
                                ICacheFlushMode icache_flush_mode) {
  UNIMPLEMENTED();
}


static const int kNoCodeAgeSequenceLength = 5 * kInstructionSize;
static const int kCodeAgeStubEntryOffset = 3 * kInstructionSize;


Handle<Object> RelocInfo::code_age_stub_handle(Assembler* origin) {
  UNREACHABLE();  // This should never be reached on ARM64.
  return Handle<Object>();
}


Code* RelocInfo::code_age_stub() {
  DCHECK(rmode_ == RelocInfo::CODE_AGE_SEQUENCE);
  // Read the stub entry point from the code age sequence.
  Address stub_entry_address = pc_ + kCodeAgeStubEntryOffset;
  return Code::GetCodeFromTargetAddress(Memory::Address_at(stub_entry_address));
}


void RelocInfo::set_code_age_stub(Code* stub,
                                  ICacheFlushMode icache_flush_mode) {
  DCHECK(rmode_ == RelocInfo::CODE_AGE_SEQUENCE);
  DCHECK(!Code::IsYoungSequence(stub->GetIsolate(), pc_));
  // Overwrite the stub entry point in the code age sequence. This is loaded as
  // a literal so there is no need to call FlushICache here.
  Address stub_entry_address = pc_ + kCodeAgeStubEntryOffset;
  Memory::Address_at(stub_entry_address) = stub->instruction_start();
}


Address RelocInfo::call_address() {
  DCHECK((IsJSReturn(rmode()) && IsPatchedReturnSequence()) ||
         (IsDebugBreakSlot(rmode()) && IsPatchedDebugBreakSlotSequence()));
  // For the above sequences the Relocinfo points to the load literal loading
  // the call address.
  return Assembler::target_address_at(pc_, host_);
}


void RelocInfo::set_call_address(Address target) {
  DCHECK((IsJSReturn(rmode()) && IsPatchedReturnSequence()) ||
         (IsDebugBreakSlot(rmode()) && IsPatchedDebugBreakSlotSequence()));
  Assembler::set_target_address_at(pc_, host_, target);
  if (host() != NULL) {
    Object* target_code = Code::GetCodeFromTargetAddress(target);
    host()->GetHeap()->incremental_marking()->RecordWriteIntoCode(
        host(), this, HeapObject::cast(target_code));
  }
}


void RelocInfo::WipeOut() {
  DCHECK(IsEmbeddedObject(rmode_) ||
         IsCodeTarget(rmode_) ||
         IsRuntimeEntry(rmode_) ||
         IsExternalReference(rmode_));
  Assembler::set_target_address_at(pc_, host_, NULL);
}


bool RelocInfo::IsPatchedReturnSequence() {
  // The sequence must be:
  //   ldr ip0, [pc, #offset]
  //   blr ip0
  // See arm64/debug-arm64.cc BreakLocationIterator::SetDebugBreakAtReturn().
  Instruction* i1 = reinterpret_cast<Instruction*>(pc_);
  Instruction* i2 = i1->following();
  return i1->IsLdrLiteralX() && (i1->Rt() == ip0.code()) &&
         i2->IsBranchAndLinkToRegister() && (i2->Rn() == ip0.code());
}


bool RelocInfo::IsPatchedDebugBreakSlotSequence() {
  Instruction* current_instr = reinterpret_cast<Instruction*>(pc_);
  return !current_instr->IsNop(Assembler::DEBUG_BREAK_NOP);
}


void RelocInfo::Visit(Isolate* isolate, ObjectVisitor* visitor) {
  RelocInfo::Mode mode = rmode();
  if (mode == RelocInfo::EMBEDDED_OBJECT) {
    visitor->VisitEmbeddedPointer(this);
  } else if (RelocInfo::IsCodeTarget(mode)) {
    visitor->VisitCodeTarget(this);
  } else if (mode == RelocInfo::CELL) {
    visitor->VisitCell(this);
  } else if (mode == RelocInfo::EXTERNAL_REFERENCE) {
    visitor->VisitExternalReference(this);
  } else if (((RelocInfo::IsJSReturn(mode) &&
              IsPatchedReturnSequence()) ||
             (RelocInfo::IsDebugBreakSlot(mode) &&
              IsPatchedDebugBreakSlotSequence())) &&
             isolate->debug()->has_break_points()) {
    visitor->VisitDebugTarget(this);
  } else if (RelocInfo::IsRuntimeEntry(mode)) {
    visitor->VisitRuntimeEntry(this);
  }
}


template<typename StaticVisitor>
void RelocInfo::Visit(Heap* heap) {
  RelocInfo::Mode mode = rmode();
  if (mode == RelocInfo::EMBEDDED_OBJECT) {
    StaticVisitor::VisitEmbeddedPointer(heap, this);
  } else if (RelocInfo::IsCodeTarget(mode)) {
    StaticVisitor::VisitCodeTarget(heap, this);
  } else if (mode == RelocInfo::CELL) {
    StaticVisitor::VisitCell(heap, this);
  } else if (mode == RelocInfo::EXTERNAL_REFERENCE) {
    StaticVisitor::VisitExternalReference(this);
  } else if (heap->isolate()->debug()->has_break_points() &&
             ((RelocInfo::IsJSReturn(mode) &&
              IsPatchedReturnSequence()) ||
             (RelocInfo::IsDebugBreakSlot(mode) &&
              IsPatchedDebugBreakSlotSequence()))) {
    StaticVisitor::VisitDebugTarget(heap, this);
  } else if (RelocInfo::IsRuntimeEntry(mode)) {
    StaticVisitor::VisitRuntimeEntry(this);
  }
}


LoadStoreOp Assembler::LoadOpFor(const CPURegister& rt) {
  DCHECK(rt.IsValid());
  if (rt.IsRegister()) {
    return rt.Is64Bits() ? LDR_x : LDR_w;
  } else {
    DCHECK(rt.IsFPRegister());
    return rt.Is64Bits() ? LDR_d : LDR_s;
  }
}


LoadStorePairOp Assembler::LoadPairOpFor(const CPURegister& rt,
                                         const CPURegister& rt2) {
  DCHECK(AreSameSizeAndType(rt, rt2));
  USE(rt2);
  if (rt.IsRegister()) {
    return rt.Is64Bits() ? LDP_x : LDP_w;
  } else {
    DCHECK(rt.IsFPRegister());
    return rt.Is64Bits() ? LDP_d : LDP_s;
  }
}


LoadStoreOp Assembler::StoreOpFor(const CPURegister& rt) {
  DCHECK(rt.IsValid());
  if (rt.IsRegister()) {
    return rt.Is64Bits() ? STR_x : STR_w;
  } else {
    DCHECK(rt.IsFPRegister());
    return rt.Is64Bits() ? STR_d : STR_s;
  }
}


LoadStorePairOp Assembler::StorePairOpFor(const CPURegister& rt,
                                          const CPURegister& rt2) {
  DCHECK(AreSameSizeAndType(rt, rt2));
  USE(rt2);
  if (rt.IsRegister()) {
    return rt.Is64Bits() ? STP_x : STP_w;
  } else {
    DCHECK(rt.IsFPRegister());
    return rt.Is64Bits() ? STP_d : STP_s;
  }
}


LoadStorePairNonTemporalOp Assembler::LoadPairNonTemporalOpFor(
    const CPURegister& rt, const CPURegister& rt2) {
  DCHECK(AreSameSizeAndType(rt, rt2));
  USE(rt2);
  if (rt.IsRegister()) {
    return rt.Is64Bits() ? LDNP_x : LDNP_w;
  } else {
    DCHECK(rt.IsFPRegister());
    return rt.Is64Bits() ? LDNP_d : LDNP_s;
  }
}


LoadStorePairNonTemporalOp Assembler::StorePairNonTemporalOpFor(
    const CPURegister& rt, const CPURegister& rt2) {
  DCHECK(AreSameSizeAndType(rt, rt2));
  USE(rt2);
  if (rt.IsRegister()) {
    return rt.Is64Bits() ? STNP_x : STNP_w;
  } else {
    DCHECK(rt.IsFPRegister());
    return rt.Is64Bits() ? STNP_d : STNP_s;
  }
}


LoadLiteralOp Assembler::LoadLiteralOpFor(const CPURegister& rt) {
  if (rt.IsRegister()) {
    return rt.Is64Bits() ? LDR_x_lit : LDR_w_lit;
  } else {
    DCHECK(rt.IsFPRegister());
    return rt.Is64Bits() ? LDR_d_lit : LDR_s_lit;
  }
}


int Assembler::LinkAndGetInstructionOffsetTo(Label* label) {
  DCHECK(kStartOfLabelLinkChain == 0);
  int offset = LinkAndGetByteOffsetTo(label);
  DCHECK(IsAligned(offset, kInstructionSize));
  return offset >> kInstructionSizeLog2;
}


Instr Assembler::Flags(FlagsUpdate S) {
  if (S == SetFlags) {
    return 1 << FlagsUpdate_offset;
  } else if (S == LeaveFlags) {
    return 0 << FlagsUpdate_offset;
  }
  UNREACHABLE();
  return 0;
}


Instr Assembler::Cond(Condition cond) {
  return cond << Condition_offset;
}


Instr Assembler::ImmPCRelAddress(int imm21) {
  CHECK(is_int21(imm21));
  Instr imm = static_cast<Instr>(truncate_to_int21(imm21));
  Instr immhi = (imm >> ImmPCRelLo_width) << ImmPCRelHi_offset;
  Instr immlo = imm << ImmPCRelLo_offset;
  return (immhi & ImmPCRelHi_mask) | (immlo & ImmPCRelLo_mask);
}


Instr Assembler::ImmUncondBranch(int imm26) {
  CHECK(is_int26(imm26));
  return truncate_to_int26(imm26) << ImmUncondBranch_offset;
}


Instr Assembler::ImmCondBranch(int imm19) {
  CHECK(is_int19(imm19));
  return truncate_to_int19(imm19) << ImmCondBranch_offset;
}


Instr Assembler::ImmCmpBranch(int imm19) {
  CHECK(is_int19(imm19));
  return truncate_to_int19(imm19) << ImmCmpBranch_offset;
}


Instr Assembler::ImmTestBranch(int imm14) {
  CHECK(is_int14(imm14));
  return truncate_to_int14(imm14) << ImmTestBranch_offset;
}


Instr Assembler::ImmTestBranchBit(unsigned bit_pos) {
  DCHECK(is_uint6(bit_pos));
  // Subtract five from the shift offset, as we need bit 5 from bit_pos.
  unsigned b5 = bit_pos << (ImmTestBranchBit5_offset - 5);
  unsigned b40 = bit_pos << ImmTestBranchBit40_offset;
  b5 &= ImmTestBranchBit5_mask;
  b40 &= ImmTestBranchBit40_mask;
  return b5 | b40;
}


Instr Assembler::SF(Register rd) {
    return rd.Is64Bits() ? SixtyFourBits : ThirtyTwoBits;
}


Instr Assembler::ImmAddSub(int64_t imm) {
  DCHECK(IsImmAddSub(imm));
  if (is_uint12(imm)) {  // No shift required.
    return imm << ImmAddSub_offset;
  } else {
    return ((imm >> 12) << ImmAddSub_offset) | (1 << ShiftAddSub_offset);
  }
}


Instr Assembler::ImmS(unsigned imms, unsigned reg_size) {
  DCHECK(((reg_size == kXRegSizeInBits) && is_uint6(imms)) ||
         ((reg_size == kWRegSizeInBits) && is_uint5(imms)));
  USE(reg_size);
  return imms << ImmS_offset;
}


Instr Assembler::ImmR(unsigned immr, unsigned reg_size) {
  DCHECK(((reg_size == kXRegSizeInBits) && is_uint6(immr)) ||
         ((reg_size == kWRegSizeInBits) && is_uint5(immr)));
  USE(reg_size);
  DCHECK(is_uint6(immr));
  return immr << ImmR_offset;
}


Instr Assembler::ImmSetBits(unsigned imms, unsigned reg_size) {
  DCHECK((reg_size == kWRegSizeInBits) || (reg_size == kXRegSizeInBits));
  DCHECK(is_uint6(imms));
  DCHECK((reg_size == kXRegSizeInBits) || is_uint6(imms + 3));
  USE(reg_size);
  return imms << ImmSetBits_offset;
}


Instr Assembler::ImmRotate(unsigned immr, unsigned reg_size) {
  DCHECK((reg_size == kWRegSizeInBits) || (reg_size == kXRegSizeInBits));
  DCHECK(((reg_size == kXRegSizeInBits) && is_uint6(immr)) ||
         ((reg_size == kWRegSizeInBits) && is_uint5(immr)));
  USE(reg_size);
  return immr << ImmRotate_offset;
}


Instr Assembler::ImmLLiteral(int imm19) {
  CHECK(is_int19(imm19));
  return truncate_to_int19(imm19) << ImmLLiteral_offset;
}


Instr Assembler::BitN(unsigned bitn, unsigned reg_size) {
  DCHECK((reg_size == kWRegSizeInBits) || (reg_size == kXRegSizeInBits));
  DCHECK((reg_size == kXRegSizeInBits) || (bitn == 0));
  USE(reg_size);
  return bitn << BitN_offset;
}


Instr Assembler::ShiftDP(Shift shift) {
  DCHECK(shift == LSL || shift == LSR || shift == ASR || shift == ROR);
  return shift << ShiftDP_offset;
}


Instr Assembler::ImmDPShift(unsigned amount) {
  DCHECK(is_uint6(amount));
  return amount << ImmDPShift_offset;
}


Instr Assembler::ExtendMode(Extend extend) {
  return extend << ExtendMode_offset;
}


Instr Assembler::ImmExtendShift(unsigned left_shift) {
  DCHECK(left_shift <= 4);
  return left_shift << ImmExtendShift_offset;
}


Instr Assembler::ImmCondCmp(unsigned imm) {
  DCHECK(is_uint5(imm));
  return imm << ImmCondCmp_offset;
}


Instr Assembler::Nzcv(StatusFlags nzcv) {
  return ((nzcv >> Flags_offset) & 0xf) << Nzcv_offset;
}


Instr Assembler::ImmLSUnsigned(int imm12) {
  DCHECK(is_uint12(imm12));
  return imm12 << ImmLSUnsigned_offset;
}


Instr Assembler::ImmLS(int imm9) {
  DCHECK(is_int9(imm9));
  return truncate_to_int9(imm9) << ImmLS_offset;
}


Instr Assembler::ImmLSPair(int imm7, LSDataSize size) {
  DCHECK(((imm7 >> size) << size) == imm7);
  int scaled_imm7 = imm7 >> size;
  DCHECK(is_int7(scaled_imm7));
  return truncate_to_int7(scaled_imm7) << ImmLSPair_offset;
}


Instr Assembler::ImmShiftLS(unsigned shift_amount) {
  DCHECK(is_uint1(shift_amount));
  return shift_amount << ImmShiftLS_offset;
}


Instr Assembler::ImmException(int imm16) {
  DCHECK(is_uint16(imm16));
  return imm16 << ImmException_offset;
}


Instr Assembler::ImmSystemRegister(int imm15) {
  DCHECK(is_uint15(imm15));
  return imm15 << ImmSystemRegister_offset;
}


Instr Assembler::ImmHint(int imm7) {
  DCHECK(is_uint7(imm7));
  return imm7 << ImmHint_offset;
}


Instr Assembler::ImmBarrierDomain(int imm2) {
  DCHECK(is_uint2(imm2));
  return imm2 << ImmBarrierDomain_offset;
}


Instr Assembler::ImmBarrierType(int imm2) {
  DCHECK(is_uint2(imm2));
  return imm2 << ImmBarrierType_offset;
}


LSDataSize Assembler::CalcLSDataSize(LoadStoreOp op) {
  DCHECK((SizeLS_offset + SizeLS_width) == (kInstructionSize * 8));
  return static_cast<LSDataSize>(op >> SizeLS_offset);
}


Instr Assembler::ImmMoveWide(uint64_t imm) {
  DCHECK(is_uint16(imm));
  return imm << ImmMoveWide_offset;
}


Instr Assembler::ShiftMoveWide(int64_t shift) {
  DCHECK(is_uint2(shift));
  return shift << ShiftMoveWide_offset;
}


Instr Assembler::FPType(FPRegister fd) {
  return fd.Is64Bits() ? FP64 : FP32;
}


Instr Assembler::FPScale(unsigned scale) {
  DCHECK(is_uint6(scale));
  return scale << FPScale_offset;
}


const Register& Assembler::AppropriateZeroRegFor(const CPURegister& reg) const {
  return reg.Is64Bits() ? xzr : wzr;
}


inline void Assembler::CheckBufferSpace() {
  DCHECK(pc_ < (buffer_ + buffer_size_));
  if (buffer_space() < kGap) {
    GrowBuffer();
  }
}


inline void Assembler::CheckBuffer() {
  CheckBufferSpace();
  if (pc_offset() >= next_veneer_pool_check_) {
    CheckVeneerPool(false, true);
  }
  if (pc_offset() >= next_constant_pool_check_) {
    CheckConstPool(false, true);
  }
}


TypeFeedbackId Assembler::RecordedAstId() {
  DCHECK(!recorded_ast_id_.IsNone());
  return recorded_ast_id_;
}


void Assembler::ClearRecordedAstId() {
  recorded_ast_id_ = TypeFeedbackId::None();
}


} }  // namespace v8::internal

#endif  // V8_ARM64_ASSEMBLER_ARM64_INL_H_
