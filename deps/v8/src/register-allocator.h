// Copyright 2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_REGISTER_ALLOCATOR_H_
#define V8_REGISTER_ALLOCATOR_H_

#include "macro-assembler.h"

namespace v8 { namespace internal {


// -------------------------------------------------------------------------
// StaticType
//
// StaticType represent the type of an expression or a word at runtime.
// The types are ordered by knowledge, so that if a value can come about
// in more than one way, and there are different static types inferred
// for the different ways, the types can be combined to a type that we
// are still certain of (possibly just "unknown").

class StaticType BASE_EMBEDDED {
 public:
  StaticType() : static_type_(UNKNOWN_TYPE) {}

  static StaticType unknown() { return StaticType(); }
  static StaticType smi() { return StaticType(SMI_TYPE); }
  static StaticType jsstring() { return StaticType(STRING_TYPE); }
  static StaticType heap_object() { return StaticType(HEAP_OBJECT_TYPE); }

  // Accessors
  bool is_unknown() { return static_type_ == UNKNOWN_TYPE; }
  bool is_smi() { return static_type_ == SMI_TYPE; }
  bool is_heap_object() { return (static_type_ & HEAP_OBJECT_TYPE) != 0; }
  bool is_jsstring() { return static_type_ == STRING_TYPE; }

  bool operator==(StaticType other) const {
    return static_type_ == other.static_type_;
  }

  // Find the best approximating type for a value.
  // The argument must not be NULL.
  static StaticType TypeOf(Object* object) {
    // Remember to make the most specific tests first. A string is also a heap
    // object, so test for string-ness first.
    if (object->IsSmi()) return smi();
    if (object->IsString()) return jsstring();
    if (object->IsHeapObject()) return heap_object();
    return unknown();
  }

  // Merges two static types to a type that combines the knowledge
  // of both. If there is no way to combine (e.g., being a string *and*
  // being a smi), the resulting type is unknown.
  StaticType merge(StaticType other) {
    StaticType x(
        static_cast<StaticTypeEnum>(static_type_ & other.static_type_));
    return x;
  }

 private:
  enum StaticTypeEnum {
    // Numbers are chosen so that least upper bound of the following
    // partial order is implemented by bitwise "and":
    //
    //    string
    //       |
    //    heap-object    smi
    //           \       /
    //            unknown
    //
    UNKNOWN_TYPE     = 0x00,
    SMI_TYPE         = 0x01,
    HEAP_OBJECT_TYPE = 0x02,
    STRING_TYPE      = 0x04 | HEAP_OBJECT_TYPE
  };
  explicit StaticType(StaticTypeEnum static_type) : static_type_(static_type) {}

  // StaticTypeEnum static_type_;
  byte static_type_;
};


// -------------------------------------------------------------------------
// Results
//
// Results encapsulate the compile-time values manipulated by the code
// generator.  They can represent registers or constants.

class Result BASE_EMBEDDED {
 public:
  enum Type {
    INVALID,
    REGISTER,
    CONSTANT
  };

  // Construct an invalid result.
  explicit Result(CodeGenerator* cgen)
      : static_type_(),
        type_(INVALID),
        cgen_(cgen) {}

  // Construct a register Result.
  Result(Register reg,
         CodeGenerator* cgen);

  // Construct a register Result with a known static type.
  Result(Register reg,
         CodeGenerator* cgen,
         StaticType static_type);

  // Construct a Result whose value is a compile-time constant.
  Result(Handle<Object> value, CodeGenerator * cgen)
      : static_type_(StaticType::TypeOf(*value)),
        type_(CONSTANT),
        cgen_(cgen) {
    data_.handle_ = value.location();
  }

  // The copy constructor and assignment operators could each create a new
  // register reference.
  Result(const Result& other) {
    other.CopyTo(this);
  }

  Result& operator=(const Result& other) {
    if (this != &other) {
      Unuse();
      other.CopyTo(this);
    }
    return *this;
  }

  inline ~Result();

  inline void Unuse();

  StaticType static_type() const { return static_type_; }
  void set_static_type(StaticType static_type) { static_type_ = static_type; }

  Type type() const { return static_cast<Type>(type_); }

  bool is_valid() const { return type() != INVALID; }
  bool is_register() const { return type() == REGISTER; }
  bool is_constant() const { return type() == CONSTANT; }

  Register reg() const {
    ASSERT(type() == REGISTER);
    return data_.reg_;
  }

  Handle<Object> handle() const {
    ASSERT(type() == CONSTANT);
    return Handle<Object>(data_.handle_);
  }

  // Move this result to an arbitrary register.  The register is not
  // necessarily spilled from the frame or even singly-referenced outside
  // it.
  void ToRegister();

  // Move this result to a specified register.  The register is spilled from
  // the frame, and the register is singly-referenced (by this result)
  // outside the frame.
  void ToRegister(Register reg);

 private:
  StaticType static_type_;
  byte type_;

  union {
    Register reg_;
    Object** handle_;
  } data_;

  CodeGenerator* cgen_;

  void CopyTo(Result* destination) const;
};


// -------------------------------------------------------------------------
// Register file
//
// The register file tracks reference counts for the processor registers.
// It is used by both the register allocator and the virtual frame.

class RegisterFile BASE_EMBEDDED {
 public:
  RegisterFile() { Reset(); }

  void Reset() {
    for (int i = 0; i < kNumRegisters; i++) {
      ref_counts_[i] = 0;
    }
  }

  // Predicates and accessors for the reference counts.  The versions
  // that take a register code rather than a register are for
  // convenience in loops over the register codes.
  bool is_used(int reg_code) const { return ref_counts_[reg_code] > 0; }
  bool is_used(Register reg) const { return is_used(reg.code()); }
  int count(int reg_code) const { return ref_counts_[reg_code]; }
  int count(Register reg) const { return count(reg.code()); }

  // Record a use of a register by incrementing its reference count.
  void Use(Register reg) {
    ref_counts_[reg.code()]++;
  }

  // Record that a register will no longer be used by decrementing its
  // reference count.
  void Unuse(Register reg) {
    ASSERT(!reg.is(no_reg));
    ASSERT(is_used(reg.code()));
    ref_counts_[reg.code()]--;
  }

  // Copy the reference counts from this register file to the other.
  void CopyTo(RegisterFile* other);

 private:
  int ref_counts_[kNumRegisters];

  // Very fast inlined loop to find a free register.
  // Used in RegisterAllocator::AllocateWithoutSpilling.
  // Returns kNumRegisters if no free register found.
  inline int ScanForFreeRegister() {
    int i = 0;
    for (; i < kNumRegisters ; ++i) {
      if (ref_counts_[i] == 0) break;
    }
    return i;
  }

  friend class RegisterAllocator;
};


// -------------------------------------------------------------------------
// Register allocator
//

class RegisterAllocator BASE_EMBEDDED {
 public:
  explicit RegisterAllocator(CodeGenerator* cgen) : cgen_(cgen) {}

  // A register file with each of the reserved registers counted once.
  static RegisterFile Reserved();

  // Unuse all the reserved registers in a register file.
  static void UnuseReserved(RegisterFile* register_file);

  // True if the register is reserved by the code generator, false if it
  // can be freely used by the allocator.
  static bool IsReserved(int reg_code);
  static bool IsReserved(Register reg) { return IsReserved(reg); }

  // Predicates and accessors for the registers' reference counts.
  bool is_used(int reg_code) const { return registers_.is_used(reg_code); }
  bool is_used(Register reg) const { return registers_.is_used(reg.code()); }
  int count(int reg_code) const { return registers_.count(reg_code); }
  int count(Register reg) const { return registers_.count(reg.code()); }

  // Explicitly record a reference to a register.
  void Use(Register reg) { registers_.Use(reg); }

  // Explicitly record that a register will no longer be used.
  void Unuse(Register reg) { registers_.Unuse(reg); }

  // Initialize the register allocator for entry to a JS function.  On
  // entry, the registers used by the JS calling convention are
  // externally referenced (ie, outside the virtual frame); and the
  // other registers are free.
  void Initialize();

  // Reset the register reference counts to free all non-reserved registers.
  // A frame-external reference is kept to each of the reserved registers.
  void Reset();

  // Allocate a free register and return a register result if possible or
  // fail and return an invalid result.
  Result Allocate();

  // Allocate a specific register if possible, spilling it from the frame if
  // necessary, or else fail and return an invalid result.
  Result Allocate(Register target);

  // Allocate a free register without spilling any from the current frame or
  // fail and return an invalid result.
  Result AllocateWithoutSpilling();

  // Allocate a free byte register without spilling any from the
  // current frame or fail and return an invalid result.
  Result AllocateByteRegisterWithoutSpilling();

  // Copy the internal state to a register file, to be restored later by
  // RestoreFrom.
  void SaveTo(RegisterFile* register_file) {
    registers_.CopyTo(register_file);
  }

  void RestoreFrom(RegisterFile* register_file) {
    register_file->CopyTo(&registers_);
  }

 private:
  CodeGenerator* cgen_;
  RegisterFile registers_;
};

} }  // namespace v8::internal

#endif  // V8_REGISTER_ALLOCATOR_H_
