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
#include "LIEF/PE/exceptions_info/UnwindCodeX64.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"

#include "LIEF/PE/exceptions_info/internal_x64.hpp"

#include "logging.hpp"

namespace LIEF::PE::unwind_x64 {

std::unique_ptr<Code> Code::create_from(
  const RuntimeFunctionX64::unwind_info_t& info, SpanStream& stream)
{
  auto op = stream.read<details::unwind_code_t>();
  if (!op) {
    return nullptr;
  }
  size_t pos = op->opcode.code_offset;

  const auto opcode = OPCODE(op->op());
  switch (opcode) {
    case OPCODE::PUSH_NONVOL:
      return std::make_unique<PushNonVol>(REG(op->op_info()), pos);

    case OPCODE::ALLOC_LARGE:
      {
        if (op->op_info() == 0) {
          auto size = stream.read<uint16_t>();
          if (!size) {
            return nullptr;
          }
          return std::make_unique<Alloc>(opcode, pos, *size * 8);
        }

        auto size = stream.read<uint32_t>();
        if (!size) {
          return nullptr;
        }
        return std::make_unique<Alloc>(opcode, pos, *size);
      }

    case OPCODE::ALLOC_SMALL:
      return std::make_unique<Alloc>(opcode, pos, (op->op_info() + 1) * 8);

    case OPCODE::SET_FPREG:
      return std::make_unique<SetFPReg>(REG(info.frame_reg), pos);

    case OPCODE::SAVE_NONVOL:
      {
        auto offset = stream.read<uint16_t>();
        if (!offset) {
          return nullptr;
        }
        return std::make_unique<SaveNonVolatile>(opcode, REG(op->op_info()),
                                                 pos, *offset * 8);
      }

    case OPCODE::SAVE_NONVOL_FAR:
      {
        auto offset = stream.read<uint32_t>();
        if (!offset) {
          return nullptr;
        }
        return std::make_unique<SaveNonVolatile>(opcode, REG(op->op_info()),
                                                 pos, *offset);
      }

    case OPCODE::SAVE_XMM128:
      {
        auto offset = stream.read<uint16_t>();
        if (!offset) {
          return nullptr;
        }
        return std::make_unique<SaveXMM128>(opcode, op->op_info(), pos, *offset * 16);
      }

    case OPCODE::SAVE_XMM128_FAR:
      {
        auto offset = stream.read<uint32_t>();
        if (!offset) {
          return nullptr;
        }
        return std::make_unique<SaveXMM128>(opcode, op->op_info(), pos, *offset);
      }

    case OPCODE::PUSH_MACHFRAME:
      return std::make_unique<PushMachFrame>(op->op_info(), pos);

    case OPCODE::EPILOG:
      {
        auto offset = stream.read<uint16_t>();
        if (!offset) {
          return nullptr;
        }

        if (info.version == 1) {
          return std::make_unique<SaveXMM128>(OPCODE::SAVE_XMM128, op->op_info(),
                                              pos, *offset * 16);
        }
        return std::make_unique<Epilog>(op->op_info(), op->opcode.code_offset);
      }

    case OPCODE::SPARE:
      {
        auto offset = stream.read<uint32_t>();
        if (!offset) {
          return nullptr;
        }

        if (info.version == 1) {
          return std::make_unique<SaveXMM128>(opcode, op->op_info(), pos, *offset);
        }
        return std::make_unique<Spare>();
      }

    default:
      LIEF_WARN("Unknown opcode: {}", op->op());
      return nullptr;
  }
  return nullptr;
}

std::string Code::to_string() const {
  return fmt::format("0x{:02x} {}", pos_, LIEF::PE::to_string(opcode_));
}

std::string Alloc::to_string() const {
  return fmt::format("{} size={}", Code::to_string(), size());
}

std::string PushNonVol::to_string() const {
  return fmt::format("{} reg={}", Code::to_string(),
                     LIEF::PE::to_string(reg()));
}

std::string PushMachFrame::to_string() const {
  return fmt::format("{} value={}", Code::to_string(),
                     (bool)value());
}

std::string SetFPReg::to_string() const {
  return fmt::format("{} reg={} ", Code::to_string(),
                     LIEF::PE::to_string(reg()));
}

std::string SaveNonVolatile::to_string() const {
  return fmt::format("{} reg={}, offset=0x{:06x}", Code::to_string(),
                     LIEF::PE::to_string(reg()), offset());
}

std::string SaveXMM128::to_string() const {
  return fmt::format("{} reg=XMM{}, offset=0x{:06x}", Code::to_string(),
                     num(), offset());
}

std::string Epilog::to_string() const {
  return fmt::format("EPILOG flags={}, size={}",
                     flags(), size());
}

}
