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

#ifndef V8_VIRTUAL_FRAME_H_
#define V8_VIRTUAL_FRAME_H_

#include "macro-assembler.h"

namespace v8 { namespace internal {

// -------------------------------------------------------------------------
// Virtual frame elements
//
// The internal elements of the virtual frames.  There are several kinds of
// elements:
//   * Invalid: elements that are uninitialized or not actually part
//     of the virtual frame.  They should not be read.
//   * Memory: an element that resides in the actual frame.  Its address is
//     given by its position in the virtual frame.
//   * Register: an element that resides in a register.
//   * Constant: an element whose value is known at compile time.

class FrameElement BASE_EMBEDDED {
 public:
  enum SyncFlag {
    NOT_SYNCED,
    SYNCED
  };

  // The default constructor creates an invalid frame element.
  FrameElement()
      : static_type_(), type_(INVALID), copied_(false), synced_(false) {
    data_.reg_ = no_reg;
  }

  // Factory function to construct an invalid frame element.
  static FrameElement InvalidElement() {
    FrameElement result;
    return result;
  }

  // Factory function to construct an in-memory frame element.
  static FrameElement MemoryElement() {
    FrameElement result(MEMORY, no_reg, SYNCED);
    return result;
  }

  // Factory function to construct an in-register frame element.
  static FrameElement RegisterElement(Register reg,
                                      SyncFlag is_synced,
                                      StaticType static_type = StaticType()) {
    return FrameElement(REGISTER, reg, is_synced, static_type);
  }

  // Factory function to construct a frame element whose value is known at
  // compile time.
  static FrameElement ConstantElement(Handle<Object> value,
                                      SyncFlag is_synced) {
    FrameElement result(value, is_synced);
    return result;
  }

  bool is_synced() const { return synced_; }

  void set_sync() {
    ASSERT(type() != MEMORY);
    synced_ = true;
  }

  void clear_sync() {
    ASSERT(type() != MEMORY);
    synced_ = false;
  }

  bool is_valid() const { return type() != INVALID; }
  bool is_memory() const { return type() == MEMORY; }
  bool is_register() const { return type() == REGISTER; }
  bool is_constant() const { return type() == CONSTANT; }
  bool is_copy() const { return type() == COPY; }

  bool is_copied() const { return copied_; }
  void set_copied() { copied_ = true; }
  void clear_copied() { copied_ = false; }

  Register reg() const {
    ASSERT(is_register());
    return data_.reg_;
  }

  Handle<Object> handle() const {
    ASSERT(is_constant());
    return Handle<Object>(data_.handle_);
  }

  int index() const {
    ASSERT(is_copy());
    return data_.index_;
  }

  bool Equals(FrameElement other);

  StaticType static_type() { return static_type_; }

  void set_static_type(StaticType static_type) {
    // TODO(lrn): If it's s copy, it would be better to update the real one,
    // but we can't from here. The caller must handle this.
    static_type_ = static_type;
  }

 private:
  enum Type {
    INVALID,
    MEMORY,
    REGISTER,
    CONSTANT,
    COPY
  };

  Type type() const { return static_cast<Type>(type_); }

  StaticType static_type_;

  // The element's type.
  byte type_;

  bool copied_;

  // The element's dirty-bit. The dirty bit can be cleared
  // for non-memory elements to indicate that the element agrees with
  // the value in memory in the actual frame.
  bool synced_;

  union {
    Register reg_;
    Object** handle_;
    int index_;
  } data_;

  // Used to construct memory and register elements.
  FrameElement(Type type, Register reg, SyncFlag is_synced)
      : static_type_(),
        type_(type),
        copied_(false),
        synced_(is_synced  != NOT_SYNCED) {
    data_.reg_ = reg;
  }

  FrameElement(Type type, Register reg, SyncFlag is_synced, StaticType stype)
      : static_type_(stype),
        type_(type),
        copied_(false),
        synced_(is_synced != NOT_SYNCED) {
    data_.reg_ = reg;
  }

  // Used to construct constant elements.
  FrameElement(Handle<Object> value, SyncFlag is_synced)
      : static_type_(StaticType::TypeOf(*value)),
        type_(CONSTANT),
        copied_(false),
        synced_(is_synced != NOT_SYNCED) {
    data_.handle_ = value.location();
  }

  void set_index(int new_index) {
    ASSERT(is_copy());
    data_.index_ = new_index;
  }

  void set_reg(Register new_reg) {
    ASSERT(is_register());
    data_.reg_ = new_reg;
  }

  friend class VirtualFrame;
};


} }  // namespace v8::internal

#if V8_TARGET_ARCH_IA32
#include "ia32/virtual-frame-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "x64/virtual-frame-x64.h"
#elif V8_TARGET_ARCH_ARM
#include "arm/virtual-frame-arm.h"
#endif

#endif  // V8_VIRTUAL_FRAME_H_
