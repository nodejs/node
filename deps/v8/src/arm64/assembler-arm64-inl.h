// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ARM64_ASSEMBLER_ARM64_INL_H_
#define V8_ARM64_ASSEMBLER_ARM64_INL_H_

#include "src/arm64/assembler-arm64.h"
#include "src/assembler.h"
#include "src/debug/debug.h"
#include "src/objects-inl.h"
#include "src/objects/smi.h"

namespace v8 {
namespace internal {

bool CpuFeatures::SupportsOptimizer() { return true; }

bool CpuFeatures::SupportsWasmSimd128() { return true; }

void RelocInfo::apply(intptr_t delta) {
  // On arm64 only internal references and immediate branches need extra work.
  if (RelocInfo::IsInternalReference(rmode_)) {
    // Absolute code pointer inside code object moves with the code object.
    intptr_t* p = reinterpret_cast<intptr_t*>(pc_);
    *p += delta;  // Relocate entry.
  } else {
    Instruction* instr = reinterpret_cast<Instruction*>(pc_);
    if (instr->IsBranchAndLink() || instr->IsUnconditionalBranch()) {
      Address old_target =
          reinterpret_cast<Address>(instr->ImmPCOffsetTarget());
      Address new_target = old_target - delta;
      instr->SetBranchImmTarget(reinterpret_cast<Instruction*>(new_target));
    }
  }
}


inline bool CPURegister::IsSameSizeAndType(const CPURegister& other) const {
  return (reg_size_ == other.reg_size_) && (reg_type_ == other.reg_type_);
}


inline bool CPURegister::IsZero() const {
  DCHECK(IsValid());
  return IsRegister() && (reg_code_ == kZeroRegCode);
}


inline bool CPURegister::IsSP() const {
  DCHECK(IsValid());
  return IsRegister() && (reg_code_ == kSPRegInternalCode);
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
  list_ |= (1ULL << code);
}


inline void CPURegList::Remove(int code) {
  DCHECK(IsValid());
  DCHECK(CPURegister::Create(code, size_, type_).IsValid());
  list_ &= ~(1ULL << code);
}


inline Register Register::XRegFromCode(unsigned code) {
  if (code == kSPRegInternalCode) {
    return sp;
  } else {
    DCHECK_LT(code, static_cast<unsigned>(kNumberOfRegisters));
    return Register::Create(code, kXRegSizeInBits);
  }
}


inline Register Register::WRegFromCode(unsigned code) {
  if (code == kSPRegInternalCode) {
    return wsp;
  } else {
    DCHECK_LT(code, static_cast<unsigned>(kNumberOfRegisters));
    return Register::Create(code, kWRegSizeInBits);
  }
}

inline VRegister VRegister::BRegFromCode(unsigned code) {
  DCHECK_LT(code, static_cast<unsigned>(kNumberOfVRegisters));
  return VRegister::Create(code, kBRegSizeInBits);
}

inline VRegister VRegister::HRegFromCode(unsigned code) {
  DCHECK_LT(code, static_cast<unsigned>(kNumberOfVRegisters));
  return VRegister::Create(code, kHRegSizeInBits);
}

inline VRegister VRegister::SRegFromCode(unsigned code) {
  DCHECK_LT(code, static_cast<unsigned>(kNumberOfVRegisters));
  return VRegister::Create(code, kSRegSizeInBits);
}

inline VRegister VRegister::DRegFromCode(unsigned code) {
  DCHECK_LT(code, static_cast<unsigned>(kNumberOfVRegisters));
  return VRegister::Create(code, kDRegSizeInBits);
}

inline VRegister VRegister::QRegFromCode(unsigned code) {
  DCHECK_LT(code, static_cast<unsigned>(kNumberOfVRegisters));
  return VRegister::Create(code, kQRegSizeInBits);
}

inline VRegister VRegister::VRegFromCode(unsigned code) {
  DCHECK_LT(code, static_cast<unsigned>(kNumberOfVRegisters));
  return VRegister::Create(code, kVRegSizeInBits);
}

inline Register CPURegister::W() const {
  DCHECK(IsRegister());
  return Register::WRegFromCode(reg_code_);
}

inline Register CPURegister::Reg() const {
  DCHECK(IsRegister());
  return Register::Create(reg_code_, reg_size_);
}

inline VRegister CPURegister::VReg() const {
  DCHECK(IsVRegister());
  return VRegister::Create(reg_code_, reg_size_);
}

inline Register CPURegister::X() const {
  DCHECK(IsRegister());
  return Register::XRegFromCode(reg_code_);
}

inline VRegister CPURegister::V() const {
  DCHECK(IsVRegister());
  return VRegister::VRegFromCode(reg_code_);
}

inline VRegister CPURegister::B() const {
  DCHECK(IsVRegister());
  return VRegister::BRegFromCode(reg_code_);
}

inline VRegister CPURegister::H() const {
  DCHECK(IsVRegister());
  return VRegister::HRegFromCode(reg_code_);
}

inline VRegister CPURegister::S() const {
  DCHECK(IsVRegister());
  return VRegister::SRegFromCode(reg_code_);
}

inline VRegister CPURegister::D() const {
  DCHECK(IsVRegister());
  return VRegister::DRegFromCode(reg_code_);
}

inline VRegister CPURegister::Q() const {
  DCHECK(IsVRegister());
  return VRegister::QRegFromCode(reg_code_);
}


// Immediate.
// Default initializer is for int types
template<typename T>
struct ImmediateInitializer {
  static const bool kIsIntType = true;
  static inline RelocInfo::Mode rmode_for(T) { return RelocInfo::NONE; }
  static inline int64_t immediate_for(T t) {
    STATIC_ASSERT(sizeof(T) <= 8);
    return t;
  }
};

template <>
struct ImmediateInitializer<Smi> {
  static const bool kIsIntType = false;
  static inline RelocInfo::Mode rmode_for(Smi t) { return RelocInfo::NONE; }
  static inline int64_t immediate_for(Smi t) {
    return static_cast<int64_t>(t.ptr());
  }
};


template<>
struct ImmediateInitializer<ExternalReference> {
  static const bool kIsIntType = false;
  static inline RelocInfo::Mode rmode_for(ExternalReference t) {
    return RelocInfo::EXTERNAL_REFERENCE;
  }
  static inline int64_t immediate_for(ExternalReference t) {;
    return static_cast<int64_t>(t.address());
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
  DCHECK_IMPLIES(reg.IsSP(), shift_amount == 0);
}


Operand::Operand(Register reg, Extend extend, unsigned shift_amount)
    : immediate_(0),
      reg_(reg),
      shift_(NO_SHIFT),
      extend_(extend),
      shift_amount_(shift_amount) {
  DCHECK(reg.IsValid());
  DCHECK_LE(shift_amount, 4);
  DCHECK(!reg.IsSP());

  // Extend modes SXTX and UXTX require a 64-bit register.
  DCHECK(reg.Is64Bits() || ((extend != SXTX) && (extend != UXTX)));
}

bool Operand::IsHeapObjectRequest() const {
  DCHECK_IMPLIES(heap_object_request_.has_value(), reg_.Is(NoReg));
  DCHECK_IMPLIES(heap_object_request_.has_value(),
                 immediate_.rmode() == RelocInfo::EMBEDDED_OBJECT ||
                     immediate_.rmode() == RelocInfo::CODE_TARGET);
  return heap_object_request_.has_value();
}

HeapObjectRequest Operand::heap_object_request() const {
  DCHECK(IsHeapObjectRequest());
  return *heap_object_request_;
}

bool Operand::IsImmediate() const {
  return reg_.Is(NoReg) && !IsHeapObjectRequest();
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

Immediate Operand::immediate_for_heap_object_request() const {
  DCHECK((heap_object_request().kind() == HeapObjectRequest::kHeapNumber &&
          immediate_.rmode() == RelocInfo::EMBEDDED_OBJECT) ||
         (heap_object_request().kind() == HeapObjectRequest::kStringConstant &&
          immediate_.rmode() == RelocInfo::EMBEDDED_OBJECT));
  return immediate_;
}

Immediate Operand::immediate() const {
  DCHECK(IsImmediate());
  return immediate_;
}


int64_t Operand::ImmediateValue() const {
  DCHECK(IsImmediate());
  return immediate_.value();
}

RelocInfo::Mode Operand::ImmediateRMode() const {
  DCHECK(IsImmediate() || IsHeapObjectRequest());
  return immediate_.rmode();
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
  DCHECK(smi.Is64Bits());
  DCHECK(SmiValuesAre32Bits() || SmiValuesAre31Bits());
  return Operand(smi, ASR, kSmiShift);
}


Operand Operand::UntagSmiAndScale(Register smi, int scale) {
  DCHECK(smi.Is64Bits());
  DCHECK((scale >= 0) && (scale <= (64 - kSmiValueSize)));
  DCHECK(SmiValuesAre32Bits() || SmiValuesAre31Bits());
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


MemOperand::MemOperand(Register base, int64_t offset, AddrMode addrmode)
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
    : base_(base), regoffset_(NoReg), addrmode_(addrmode) {
  DCHECK(base.Is64Bits() && !base.IsZero());

  if (offset.IsImmediate()) {
    offset_ = offset.ImmediateValue();
  } else if (offset.IsShiftedRegister()) {
    DCHECK((addrmode == Offset) || (addrmode == PostIndex));

    regoffset_ = offset.reg();
    shift_ = offset.shift();
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

    shift_ = NO_SHIFT;
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
Address Assembler::target_address_at(Address pc, Address constant_pool) {
  Instruction* instr = reinterpret_cast<Instruction*>(pc);
  if (instr->IsLdrLiteralX()) {
    return Memory<Address>(target_pointer_address_at(pc));
  } else {
    DCHECK(instr->IsBranchAndLink() || instr->IsUnconditionalBranch());
    return reinterpret_cast<Address>(instr->ImmPCOffsetTarget());
  }
}

Handle<Code> Assembler::code_target_object_handle_at(Address pc) {
  Instruction* instr = reinterpret_cast<Instruction*>(pc);
  if (instr->IsLdrLiteralX()) {
    return Handle<Code>(reinterpret_cast<Address*>(
        Assembler::target_address_at(pc, 0 /* unused */)));
  } else {
    DCHECK(instr->IsBranchAndLink() || instr->IsUnconditionalBranch());
    DCHECK_EQ(instr->ImmPCOffset() % kInstrSize, 0);
    return GetCodeTarget(instr->ImmPCOffset() >> kInstrSizeLog2);
  }
}

Address Assembler::runtime_entry_at(Address pc) {
  Instruction* instr = reinterpret_cast<Instruction*>(pc);
  if (instr->IsLdrLiteralX()) {
    return Assembler::target_address_at(pc, 0 /* unused */);
  } else {
    DCHECK(instr->IsBranchAndLink() || instr->IsUnconditionalBranch());
    return instr->ImmPCOffset() + options().code_range_start;
  }
}

Address Assembler::target_address_from_return_address(Address pc) {
  // Returns the address of the call target from the return address that will
  // be returned to after a call.
  // Call sequence on ARM64 is:
  //  ldr ip0, #... @ load from literal pool
  //  blr ip0
  Address candidate = pc - 2 * kInstrSize;
  Instruction* instr = reinterpret_cast<Instruction*>(candidate);
  USE(instr);
  DCHECK(instr->IsLdrLiteralX());
  return candidate;
}

int Assembler::deserialization_special_target_size(Address location) {
  Instruction* instr = reinterpret_cast<Instruction*>(location);
  if (instr->IsBranchAndLink() || instr->IsUnconditionalBranch()) {
    return kSpecialTargetSize;
  } else {
    DCHECK_EQ(instr->InstructionBits(), 0);
    return kSystemPointerSize;
  }
}

void Assembler::deserialization_set_special_target_at(Address location,
                                                      Code code,
                                                      Address target) {
  Instruction* instr = reinterpret_cast<Instruction*>(location);
  if (instr->IsBranchAndLink() || instr->IsUnconditionalBranch()) {
    if (target == 0) {
      // We are simply wiping the target out for serialization. Set the offset
      // to zero instead.
      target = location;
    }
    instr->SetBranchImmTarget(reinterpret_cast<Instruction*>(target));
    FlushInstructionCache(location, kInstrSize);
  } else {
    DCHECK_EQ(instr->InstructionBits(), 0);
    Memory<Address>(location) = target;
    // Intuitively, we would think it is necessary to always flush the
    // instruction cache after patching a target address in the code. However,
    // in this case, only the constant pool contents change. The instruction
    // accessing the constant pool remains unchanged, so a flush is not
    // required.
  }
}

void Assembler::deserialization_set_target_internal_reference_at(
    Address pc, Address target, RelocInfo::Mode mode) {
  Memory<Address>(pc) = target;
}

void Assembler::set_target_address_at(Address pc, Address constant_pool,
                                      Address target,
                                      ICacheFlushMode icache_flush_mode) {
  Instruction* instr = reinterpret_cast<Instruction*>(pc);
  if (instr->IsLdrLiteralX()) {
    Memory<Address>(target_pointer_address_at(pc)) = target;
    // Intuitively, we would think it is necessary to always flush the
    // instruction cache after patching a target address in the code. However,
    // in this case, only the constant pool contents change. The instruction
    // accessing the constant pool remains unchanged, so a flush is not
    // required.
  } else {
    DCHECK(instr->IsBranchAndLink() || instr->IsUnconditionalBranch());
    if (target == 0) {
      // We are simply wiping the target out for serialization. Set the offset
      // to zero instead.
      target = pc;
    }
    instr->SetBranchImmTarget(reinterpret_cast<Instruction*>(target));
    if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
      FlushInstructionCache(pc, kInstrSize);
    }
  }
}

int RelocInfo::target_address_size() {
  if (IsCodedSpecially()) {
    return Assembler::kSpecialTargetSize;
  } else {
    DCHECK(reinterpret_cast<Instruction*>(pc_)->IsLdrLiteralX());
    return kSystemPointerSize;
  }
}


Address RelocInfo::target_address() {
  DCHECK(IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_) || IsWasmCall(rmode_));
  return Assembler::target_address_at(pc_, constant_pool_);
}

Address RelocInfo::target_address_address() {
  DCHECK(HasTargetAddressAddress());
  Instruction* instr = reinterpret_cast<Instruction*>(pc_);
  // Read the address of the word containing the target_address in an
  // instruction stream.
  // The only architecture-independent user of this function is the serializer.
  // The serializer uses it to find out how many raw bytes of instruction to
  // output before the next target.
  // For an instruction like B/BL, where the target bits are mixed into the
  // instruction bits, the size of the target will be zero, indicating that the
  // serializer should not step forward in memory after a target is resolved
  // and written.
  // For LDR literal instructions, we can skip up to the constant pool entry
  // address. We make sure that RelocInfo is ordered by the
  // target_address_address so that we do not skip over any relocatable
  // instruction sequences.
  if (instr->IsLdrLiteralX()) {
    return constant_pool_entry_address();
  } else {
    DCHECK(instr->IsBranchAndLink() || instr->IsUnconditionalBranch());
    return pc_;
  }
}


Address RelocInfo::constant_pool_entry_address() {
  DCHECK(IsInConstantPool());
  return Assembler::target_pointer_address_at(pc_);
}

HeapObject RelocInfo::target_object() {
  DCHECK(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  return HeapObject::cast(
      Object(Assembler::target_address_at(pc_, constant_pool_)));
}

Handle<HeapObject> RelocInfo::target_object_handle(Assembler* origin) {
  if (rmode_ == EMBEDDED_OBJECT) {
    return Handle<HeapObject>(reinterpret_cast<Address*>(
        Assembler::target_address_at(pc_, constant_pool_)));
  } else {
    DCHECK(IsCodeTarget(rmode_));
    return origin->code_target_object_handle_at(pc_);
  }
}

void RelocInfo::set_target_object(Heap* heap, HeapObject target,
                                  WriteBarrierMode write_barrier_mode,
                                  ICacheFlushMode icache_flush_mode) {
  DCHECK(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  Assembler::set_target_address_at(pc_, constant_pool_, target->ptr(),
                                   icache_flush_mode);
  if (write_barrier_mode == UPDATE_WRITE_BARRIER && !host().is_null()) {
    WriteBarrierForCode(host(), this, target);
  }
}

Address RelocInfo::target_external_reference() {
  DCHECK(rmode_ == EXTERNAL_REFERENCE);
  return Assembler::target_address_at(pc_, constant_pool_);
}

void RelocInfo::set_target_external_reference(
    Address target, ICacheFlushMode icache_flush_mode) {
  DCHECK(rmode_ == RelocInfo::EXTERNAL_REFERENCE);
  Assembler::set_target_address_at(pc_, constant_pool_, target,
                                   icache_flush_mode);
}

Address RelocInfo::target_internal_reference() {
  DCHECK(rmode_ == INTERNAL_REFERENCE);
  return Memory<Address>(pc_);
}


Address RelocInfo::target_internal_reference_address() {
  DCHECK(rmode_ == INTERNAL_REFERENCE);
  return pc_;
}

Address RelocInfo::target_runtime_entry(Assembler* origin) {
  DCHECK(IsRuntimeEntry(rmode_));
  return origin->runtime_entry_at(pc_);
}

void RelocInfo::set_target_runtime_entry(Address target,
                                         WriteBarrierMode write_barrier_mode,
                                         ICacheFlushMode icache_flush_mode) {
  DCHECK(IsRuntimeEntry(rmode_));
  if (target_address() != target) {
    set_target_address(target, write_barrier_mode, icache_flush_mode);
  }
}

Address RelocInfo::target_off_heap_target() {
  DCHECK(IsOffHeapTarget(rmode_));
  return Assembler::target_address_at(pc_, constant_pool_);
}

void RelocInfo::WipeOut() {
  DCHECK(IsEmbeddedObject(rmode_) || IsCodeTarget(rmode_) ||
         IsRuntimeEntry(rmode_) || IsExternalReference(rmode_) ||
         IsInternalReference(rmode_) || IsOffHeapTarget(rmode_));
  if (IsInternalReference(rmode_)) {
    Memory<Address>(pc_) = kNullAddress;
  } else {
    Assembler::set_target_address_at(pc_, constant_pool_, kNullAddress);
  }
}

LoadStoreOp Assembler::LoadOpFor(const CPURegister& rt) {
  DCHECK(rt.IsValid());
  if (rt.IsRegister()) {
    return rt.Is64Bits() ? LDR_x : LDR_w;
  } else {
    DCHECK(rt.IsVRegister());
    switch (rt.SizeInBits()) {
      case kBRegSizeInBits:
        return LDR_b;
      case kHRegSizeInBits:
        return LDR_h;
      case kSRegSizeInBits:
        return LDR_s;
      case kDRegSizeInBits:
        return LDR_d;
      default:
        DCHECK(rt.IsQ());
        return LDR_q;
    }
  }
}


LoadStoreOp Assembler::StoreOpFor(const CPURegister& rt) {
  DCHECK(rt.IsValid());
  if (rt.IsRegister()) {
    return rt.Is64Bits() ? STR_x : STR_w;
  } else {
    DCHECK(rt.IsVRegister());
    switch (rt.SizeInBits()) {
      case kBRegSizeInBits:
        return STR_b;
      case kHRegSizeInBits:
        return STR_h;
      case kSRegSizeInBits:
        return STR_s;
      case kDRegSizeInBits:
        return STR_d;
      default:
        DCHECK(rt.IsQ());
        return STR_q;
    }
  }
}

LoadStorePairOp Assembler::LoadPairOpFor(const CPURegister& rt,
                                         const CPURegister& rt2) {
  DCHECK_EQ(STP_w | LoadStorePairLBit, LDP_w);
  return static_cast<LoadStorePairOp>(StorePairOpFor(rt, rt2) |
                                      LoadStorePairLBit);
}

LoadStorePairOp Assembler::StorePairOpFor(const CPURegister& rt,
                                          const CPURegister& rt2) {
  DCHECK(AreSameSizeAndType(rt, rt2));
  USE(rt2);
  if (rt.IsRegister()) {
    return rt.Is64Bits() ? STP_x : STP_w;
  } else {
    DCHECK(rt.IsVRegister());
    switch (rt.SizeInBits()) {
      case kSRegSizeInBits:
        return STP_s;
      case kDRegSizeInBits:
        return STP_d;
      default:
        DCHECK(rt.IsQ());
        return STP_q;
    }
  }
}


LoadLiteralOp Assembler::LoadLiteralOpFor(const CPURegister& rt) {
  if (rt.IsRegister()) {
    return rt.Is64Bits() ? LDR_x_lit : LDR_w_lit;
  } else {
    DCHECK(rt.IsVRegister());
    return rt.Is64Bits() ? LDR_d_lit : LDR_s_lit;
  }
}


int Assembler::LinkAndGetInstructionOffsetTo(Label* label) {
  DCHECK_EQ(kStartOfLabelLinkChain, 0);
  int offset = LinkAndGetByteOffsetTo(label);
  DCHECK(IsAligned(offset, kInstrSize));
  return offset >> kInstrSizeLog2;
}


Instr Assembler::Flags(FlagsUpdate S) {
  if (S == SetFlags) {
    return 1 << FlagsUpdate_offset;
  } else if (S == LeaveFlags) {
    return 0 << FlagsUpdate_offset;
  }
  UNREACHABLE();
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


Instr Assembler::ImmAddSub(int imm) {
  DCHECK(IsImmAddSub(imm));
  if (is_uint12(imm)) {  // No shift required.
    imm <<= ImmAddSub_offset;
  } else {
    imm = ((imm >> 12) << ImmAddSub_offset) | (1 << ShiftAddSub_offset);
  }
  return imm;
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
  DCHECK_LE(left_shift, 4);
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

Instr Assembler::ImmLSPair(int imm7, unsigned size) {
  DCHECK_EQ((imm7 >> size) << size, imm7);
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

unsigned Assembler::CalcLSDataSize(LoadStoreOp op) {
  DCHECK((LSSize_offset + LSSize_width) == (kInstrSize * 8));
  unsigned size = static_cast<Instr>(op >> LSSize_offset);
  if ((op & LSVector_mask) != 0) {
    // Vector register memory operations encode the access size in the "size"
    // and "opc" fields.
    if ((size == 0) && ((op & LSOpc_mask) >> LSOpc_offset) >= 2) {
      size = kQRegSizeLog2;
    }
  }
  return size;
}


Instr Assembler::ImmMoveWide(int imm) {
  DCHECK(is_uint16(imm));
  return imm << ImmMoveWide_offset;
}


Instr Assembler::ShiftMoveWide(int shift) {
  DCHECK(is_uint2(shift));
  return shift << ShiftMoveWide_offset;
}

Instr Assembler::FPType(VRegister fd) { return fd.Is64Bits() ? FP64 : FP32; }

Instr Assembler::FPScale(unsigned scale) {
  DCHECK(is_uint6(scale));
  return scale << FPScale_offset;
}


const Register& Assembler::AppropriateZeroRegFor(const CPURegister& reg) const {
  return reg.Is64Bits() ? xzr : wzr;
}


inline void Assembler::CheckBufferSpace() {
  DCHECK_LT(pc_, buffer_start_ + buffer_->size());
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

}  // namespace internal
}  // namespace v8

#endif  // V8_ARM64_ASSEMBLER_ARM64_INL_H_
