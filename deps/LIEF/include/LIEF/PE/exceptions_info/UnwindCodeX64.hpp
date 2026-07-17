/* Copyright 2017 - 2025 R. Thomas
 * Copyright 2017 - 2025 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIEF_PE_UNWIND_CODE_X64_H
#define LIEF_PE_UNWIND_CODE_X64_H
#include <ostream>
#include <memory>
#include <string>
#include "LIEF/PE/exceptions_info/RuntimeFunctionX64.hpp"

namespace LIEF {
class SpanStream;

namespace PE {
/// This namespace wraps code related to PE-x64 unwinding code
namespace unwind_x64 {

/// Base class for all unwind operations
class LIEF_API Code {
  public:
  using OPCODE = RuntimeFunctionX64::UNWIND_OPCODES;
  using REG = RuntimeFunctionX64::UNWIND_REG;

  /// \private
  static std::unique_ptr<Code>
    create_from(const RuntimeFunctionX64::unwind_info_t& info, SpanStream& stream);

  Code() = delete;
  Code(const Code&) = default;
  Code& operator=(const Code&) = default;

  Code(Code&&) = default;
  Code& operator=(Code&&) = default;

  virtual ~Code() = default;

  Code(OPCODE opcode, uint32_t pos) :
    pos_(pos),
    opcode_(opcode)
  {}

  Code(OPCODE opcode) :
    Code(opcode, 0)
  {}

  /// The original opcode
  OPCODE opcode() const {
    return opcode_;
  }

  /// Offset in the prolog
  uint32_t position() const {
    return pos_;
  }

  /// Pretty representation
  virtual std::string to_string() const;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Code& code) {
    os << code.to_string();
    return os;
  }

  protected:
  uint32_t pos_ = 0;
  OPCODE opcode_;
};

/// This class represents a stack-allocation operation
/// (UNWIND_OPCODES::ALLOC_SMALL or UNWIND_OPCODES::ALLOC_LARGE).
class LIEF_API Alloc : public Code {
  public:
  Alloc(OPCODE op, size_t pos, uint32_t size) :
    Code(op, pos),
    size_(size)
  {}

  /// The size allocated
  uint32_t size() const {
    return size_;
  }

  std::string to_string() const override;

  ~Alloc() override = default;

  static bool classof(const Code* code) {
    return code->opcode() == OPCODE::ALLOC_LARGE ||
           code->opcode() == OPCODE::ALLOC_SMALL;
  }
  protected:
  uint32_t size_ = 0;
};

/// Push a nonvolatile integer register, decrementing RSP by 8
class LIEF_API PushNonVol : public Code {
  public:
  PushNonVol() = delete;
  PushNonVol(REG reg, size_t pos) :
    Code(OPCODE::PUSH_NONVOL, pos),
    reg_(reg)
  {}

  std::string to_string() const override;

  /// The register pushed
  REG reg() const {
    return reg_;
  }

  ~PushNonVol() override = default;

  static bool classof(const Code* code) {
    return code->opcode() == OPCODE::PUSH_NONVOL;
  }

  protected:
  REG reg_;
};

/// Push a machine frame
class LIEF_API PushMachFrame : public Code {
  public:
  PushMachFrame() = delete;
  PushMachFrame(uint8_t value, size_t pos) :
    Code(OPCODE::PUSH_MACHFRAME, pos),
    value_(value)
  {}

  /// 0 or 1
  uint8_t value() const {
    return value_;
  }

  std::string to_string() const override;

  ~PushMachFrame() override = default;

  static bool classof(const Code* code) {
    return code->opcode() == OPCODE::PUSH_MACHFRAME;
  }

  protected:
  uint8_t value_;
};

/// Establish the frame pointer register by setting the register to some offset
/// of the current RSP
class LIEF_API SetFPReg : public Code {
  public:
  SetFPReg() = delete;
  SetFPReg(REG value, size_t pos) :
    Code(OPCODE::SET_FPREG, pos),
    reg_(value)
  {}

  /// Frame pointer register
  REG reg() const {
    return reg_;
  }

  std::string to_string() const override;

  ~SetFPReg() override = default;

  static bool classof(const Code* code) {
    return code->opcode() == OPCODE::SET_FPREG;
  }

  protected:
  REG reg_;
};

/// Save a nonvolatile integer register on the stack using a `MOV` instead of a
/// `PUSH`.
class LIEF_API SaveNonVolatile : public Code {
  public:
  SaveNonVolatile() = delete;
  SaveNonVolatile(OPCODE op, REG value, size_t pos, uint32_t offset) :
    Code(op, pos),
    reg_(value),
    offset_(offset)
  {}

  REG reg() const {
    return reg_;
  }

  uint32_t offset() const {
    return offset_;
  }

  std::string to_string() const override;

  ~SaveNonVolatile() override = default;

  static bool classof(const Code* code) {
    return code->opcode() == OPCODE::SAVE_NONVOL ||
           code->opcode() == OPCODE::SAVE_NONVOL_FAR;
  }

  protected:
  REG reg_;
  uint32_t offset_ = 0;
};

class LIEF_API SaveXMM128 : public Code {
  public:
  SaveXMM128() = delete;
  SaveXMM128(OPCODE op, uint8_t num, size_t pos, uint32_t offset) :
    Code(op, pos),
    num_(num),
    offset_(offset)
  {}

  uint8_t num() const {
    return num_;
  }

  uint32_t offset() const {
    return offset_;
  }

  std::string to_string() const override;

  ~SaveXMM128() override = default;

  static bool classof(const Code* code) {
    return code->opcode() == OPCODE::SAVE_XMM128 ||
           code->opcode() == OPCODE::SAVE_XMM128_FAR;
  }

  protected:
  uint8_t num_ = 0;
  uint32_t offset_ = 0;
};

/// Describes the function's epilog
class LIEF_API Epilog : public Code {
  public:
  Epilog() = delete;

  Epilog(uint8_t flags, uint8_t size) :
    Code(OPCODE::EPILOG, 0),
    flags_(flags),
    size_(size)
  {}

  uint8_t flags() const {
    return flags_;
  }

  /// Size of the epilog
  uint32_t size() const {
    return size_;
  }

  std::string to_string() const override;

  ~Epilog() override = default;

  static bool classof(const Code* code) {
    return code->opcode() == OPCODE::EPILOG;
  }

  protected:
  uint8_t flags_ = 0;
  uint8_t size_ = 0;
};

class LIEF_API Spare : public Code {
  public:
  Spare() :
    Code(OPCODE::SPARE, 0)
  {}

  std::string to_string() const override {
    return "Noop";
  }

  ~Spare() override = default;

  static bool classof(const Code* code) {
    return code->opcode() == OPCODE::SPARE;
  }
};

}

}
}
#endif


