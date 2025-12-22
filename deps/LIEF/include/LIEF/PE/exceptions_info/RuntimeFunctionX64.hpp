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
#ifndef LIEF_PE_RUNTIME_FUNCTION_X64_H
#define LIEF_PE_RUNTIME_FUNCTION_X64_H

#include <memory>

#include "LIEF/errors.hpp"
#include "LIEF/visibility.h"
#include "LIEF/enums.hpp"
#include "LIEF/optional.hpp"

#include "LIEF/PE/ExceptionInfo.hpp"

namespace LIEF {
class BinaryStream;

namespace PE {
class Parser;

namespace unwind_x64 {
class Code;
}

/// This class represents an entry in the exception table (`.pdata` section)
/// for the x86-64 architecture.
///
/// Reference: https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64
class LIEF_API RuntimeFunctionX64 : public ExceptionInfo {
  public:

  /// \private
  LIEF_LOCAL static std::unique_ptr<RuntimeFunctionX64>
    parse(Parser& ctx, BinaryStream& strm, bool skip_unwind = false);

  /// \private
  LIEF_LOCAL static ok_error_t
    parse_unwind(Parser& ctx, BinaryStream& strm, RuntimeFunctionX64& func);

  enum class UNWIND_FLAGS : uint8_t {
    /// The function has an exception handler that should be called when looking
    /// for functions that need to examine exceptions.
    EXCEPTION_HANDLER = 1,
    /// The function has a termination handler that should be called when
    /// unwinding an exception.
    TERMINATE_HANDLER = 2,

    /// The chained info payload references a previous `RUNTIME_FUNCTION`
    CHAIN_INFO = 4,
  };

  enum class UNWIND_OPCODES : uint32_t {
    /// Push a nonvolatile integer register, decrementing RSP by 8.
    /// The operation info is the number of the register. Because of the
    /// constraints on epilogs, `PUSH_NONVOL` unwind codes must appear first
    /// in the prolog and correspondingly, last in the unwind code array.
    /// This relative ordering applies to all other unwind codes except
    /// UNWIND_OPCODES::PUSH_MACHFRAME.
    PUSH_NONVOL = 0,

    /// Allocate a large-sized area on the stack.
    /// There are two forms. If the operation info equals 0,
    /// then the size of the allocation divided by 8 is recorded in the next slot,
    /// allowing an allocation up to 512K - 8. If the operation info equals 1,
    /// then the unscaled size of the allocation is recorded in the next two
    /// slots in little-endian format, allowing allocations up to 4GB - 8.
    ALLOC_LARGE = 1,

    /// Allocate a small-sized area on the stack. The size of the allocation is
    /// the operation info field * 8 + 8, allowing allocations from 8 to 128 bytes.
    ALLOC_SMALL = 2,

    /// Establish the frame pointer register by setting the register to some
    /// offset of the current RSP. The offset is equal to the Frame Register
    /// offset (scaled) field in the UNWIND_INFO * 16, allowing offsets from
    /// 0 to 240. The use of an offset permits establishing a frame pointer that
    /// points to the middle of the fixed stack allocation, helping code density
    /// by allowing more accesses to use short instruction forms. The operation
    /// info field is reserved and shouldn't be used.
    SET_FPREG = 3,

    /// Save a nonvolatile integer register on the stack using a MOV instead of a
    /// PUSH. This code is primarily used for shrink-wrapping, where a nonvolatile
    /// register is saved to the stack in a position that was previously allocated.
    /// The operation info is the number of the register. The scaled-by-8 stack
    /// offset is recorded in the next unwind operation code slot, as described
    /// in the note above.
    SAVE_NONVOL = 4,

    /// Save a nonvolatile integer register on the stack with a long offset,
    /// using a MOV instead of a PUSH. This code is primarily used for
    /// shrink-wrapping, where a nonvolatile register is saved to the stack in a
    /// position that was previously allocated. The operation info is the number
    /// of the register. The unscaled stack offset is recorded in the next two
    /// unwind operation code slots, as described in the note above.
    SAVE_NONVOL_FAR = 5,

    /// This entry is only revelant for version 2. It describes the function
    /// epilog.
    EPILOG = 6,

    /// Reserved
    /// Originally SAVE_XMM128_FAR in version 1, but deprecated and removed
    SPARE = 7,

    /// Save all 128 bits of a nonvolatile XMM register on the stack.
    /// The operation info is the number of the register. The scaled-by-16 stack
    /// offset is recorded in the next slot.
    SAVE_XMM128 = 8,

    /// Save all 128 bits of a nonvolatile XMM register on the stack with a
    /// long offset. The operation info is the number of the register.
    /// The unscaled stack offset is recorded in the next two slots.
    SAVE_XMM128_FAR = 9,

    /// Push a machine frame. This unwind code is used to record the effect of a
    /// hardware interrupt or exception.
    PUSH_MACHFRAME = 10,
  };

  enum class UNWIND_REG : uint32_t {
    RAX = 0,
    RCX, RDX, RBX, RSP, RBP, RSI, RDI, R8, R9, R10, R11, R12, R13, R14, R15,
  };

  /// This structure represents the `UNWIND_INFO` which records the effects
  /// a function has on the stack pointer, and where the nonvolatile registers
  /// are saved on the stack.
  struct LIEF_API unwind_info_t {
    using opcodes_t = std::vector<std::unique_ptr<unwind_x64::Code>>;

    /// Version number of the unwind data, currently 1 or 2.
    uint8_t version = 0;

    /// See: UNWIND_FLAGS
    uint8_t flags = 0;

    /// Length of the function prolog in bytes.
    uint8_t sizeof_prologue = 0;

    /// The number of slots in the unwind codes array. Some unwind codes, for
    /// example, UNWIND_OPCODES::SAVE_NONVOL, require more than one slot in the
    /// array.
    uint8_t count_opcodes = 0;

    /// If nonzero, then the function uses a frame pointer (FP), and this field
    /// is the number of the nonvolatile register used as the frame pointer,
    /// using the same encoding for the operation info field of UNWIND_OPCODES
    /// nodes.
    uint8_t frame_reg = 0;

    /// If the frame register field is nonzero, this field is the scaled offset
    /// from RSP that is applied to the FP register when it's established
    uint8_t frame_reg_offset = 0;

    /// An array of items that explains the effect of the prolog on the
    /// nonvolatile registers and RSP
    std::vector<uint8_t> raw_opcodes;

    /// An image-relative pointer to either the function's language-specific
    /// exception or termination handler. This value is set if one of these
    /// flags is set: UNWIND_FLAGS::EXCEPTION_HANDLER,
    /// UNWIND_FLAGS::TERMINATE_HANDLER
    optional<uint32_t> handler;

    /// If UNWIND_FLAGS::CHAIN_INFO is set, this attributes references the
    /// chained runtime function.
    RuntimeFunctionX64* chained = nullptr;

    /// Check if the given flag is used
    bool has(UNWIND_FLAGS flag) const {
      return (flags & (int)flag) != 0;
    }

    /// Enhanced representation of the unwind code
    opcodes_t opcodes() const;

    /// Pretty representation of this structure as a string
    std::string to_string() const;

    friend LIEF_API
      std::ostream& operator<<(std::ostream& os, const unwind_info_t& info)
    {
      os << info.to_string();
      return os;
    }
  };

  RuntimeFunctionX64(uint32_t rva_start, uint32_t rva_end, uint32_t unwind_rva) :
    ExceptionInfo(ARCH::X86_64, rva_start),
    rva_end_(rva_end),
    unwind_rva_(unwind_rva)
  {}

  RuntimeFunctionX64(const RuntimeFunctionX64&) = default;
  RuntimeFunctionX64& operator=(const RuntimeFunctionX64&) = default;

  RuntimeFunctionX64(RuntimeFunctionX64&&) = default;
  RuntimeFunctionX64& operator=(RuntimeFunctionX64&&) = default;

  std::unique_ptr<ExceptionInfo> clone() const override {
    return std::unique_ptr<RuntimeFunctionX64>(new RuntimeFunctionX64(*this));
  }

  std::string to_string() const override;

  /// Function end address
  uint32_t rva_end() const {
    return rva_end_;
  }

  /// Unwind info address
  uint32_t unwind_rva() const {
    return unwind_rva_;
  }

  /// Size of the function (in bytes)
  uint32_t size() const {
    return rva_end() - rva_start();
  }

  /// Detailed unwind information
  const unwind_info_t* unwind_info() const {
    return unwind_info_.has_value() ? &*unwind_info_ : nullptr;
  }

  unwind_info_t* unwind_info() {
    return unwind_info_.has_value() ? &*unwind_info_ : nullptr;
  }

  void unwind_info(unwind_info_t info) {
    unwind_info_ = std::move(info);
  }

  static bool classof(const ExceptionInfo* info) {
    return info->arch() == ExceptionInfo::ARCH::X86_64;
  }

  ~RuntimeFunctionX64() = default;

  private:
  uint32_t rva_end_ = 0;
  uint32_t unwind_rva_ = 0;
  optional<unwind_info_t> unwind_info_;
};

LIEF_API const char* to_string(RuntimeFunctionX64::UNWIND_OPCODES op);
LIEF_API const char* to_string(RuntimeFunctionX64::UNWIND_FLAGS op);
LIEF_API const char* to_string(RuntimeFunctionX64::UNWIND_REG op);

}
}

ENABLE_BITMASK_OPERATORS(LIEF::PE::RuntimeFunctionX64::UNWIND_FLAGS);

#endif
