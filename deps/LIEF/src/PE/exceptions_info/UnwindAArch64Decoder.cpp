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
#include "PE/exceptions_info/UnwindAArch64Decoder.hpp"
#include "LIEF/BinaryStream/BinaryStream.hpp"

#include "logging.hpp"

namespace LIEF::PE::unwind_aarch64 {
using namespace fmt;

template<>
bool Decoder::decode<OPCODES::ALLOC_S>(bool is_prologue) {
  auto num_bytes = (read_u8() & 0x1F) << 4;
  log("{} sp, #{}", is_prologue ? "sub" : "add", num_bytes);
  return false;
}

template<>
bool Decoder::decode<OPCODES::SAVE_R19R20_X>(bool is_prologue) {
  auto offset = (read_u8() & 0x1F) << 3;
  is_prologue ? log("stp x19, x20, [sp, #-{}]!", offset) :
                log("ldp x19, x20, [sp], #{}", offset);
  return false;
}

template<>
bool Decoder::decode<OPCODES::SAVE_FPLR>(bool is_prologue) {
  auto offset = (read_u8() & 0x3F) << 3;
  log("{} x29, x30, [sp, #{}]", is_prologue ? "stp" : "ldp", offset);
  return false;
}

template<>
bool Decoder::decode<OPCODES::SAVE_FPLR_X>(bool is_prologue) {
  auto offset = ((read_u8() & 0x3F) + 1) << 3;
  is_prologue ? log("stp x29, x30, [sp, #-{}]!", offset) :
                log("ldp x29, x30, [sp], #{}", offset);
  return false;
}

template<>
bool Decoder::decode<OPCODES::ALLOC_M>(bool is_prologue) {
  auto chunk_1 = read_u8();
  auto chunk_2 = read_u8();

  uint32_t nb_bytes = ((chunk_1 & 0x07) << 8 | (chunk_2 & 0xFF)) << 4;
  log("{} sp, #{}", is_prologue ? "sub" : "add", nb_bytes);
  return false;
}

template<>
bool Decoder::decode<OPCODES::SAVE_REGP>(bool is_prologue) {
  auto chunk_1 = read_u8();
  auto chunk_2 = read_u8();

  uint32_t reg = ((((chunk_1 & 0x03) << 8) | (chunk_2 & 0xC0)) >> 6) + 19;
  uint32_t off = (chunk_2 & 0x3F) << 3;

  log("{} x{}, x{}, [sp, #{}]", is_prologue ? "stp" : "ldp",
      reg, reg + 1, off);

  return false;
}

template<>
bool Decoder::decode<OPCODES::SAVE_REGP_X>(bool is_prologue) {
  auto chunk_1 = read_u8();
  auto chunk_2 = read_u8();

  uint32_t reg = ((((chunk_1 & 0x03) << 8) | (chunk_2 & 0xC0)) >> 6) + 19;
  uint32_t off = ((chunk_2 & 0x3F) + 1) << 3;

  is_prologue ? log("stp x{}, x{}, [sp, #-{}]!", reg, reg + 1, off) :
                log("ldp x{}, x{}, [sp], #{}", reg, reg + 1, off);

  return false;
}

template<>
bool Decoder::decode<OPCODES::SAVE_REG>(bool is_prologue) {
  auto chunk_1 = read_u8();
  auto chunk_2 = read_u8();

  uint32_t reg = ((((chunk_1 & 0x03) << 8) | (chunk_2 & 0xC0)) >> 6) + 19;
  uint32_t off = (chunk_2 & 0x3F) << 3;
  log("{} x{}, [sp, #{}]", is_prologue ? "str" : "ldr", reg, off);
  return false;
}

template<>
bool Decoder::decode<OPCODES::SAVE_REG_X>(bool is_prologue) {
  auto chunk_1 = read_u8();
  auto chunk_2 = read_u8();

  uint32_t reg = ((((chunk_1 & 0x01) << 8) | (chunk_2 & 0xE0)) >> 5) + 19;
  uint32_t off = ((chunk_2 & 0x1F) + 1) << 3;

  is_prologue ? log("str x{}, [sp, #-{}]!", reg, off) :
                log("ldr x{}, [sp], #{}", reg, off);
  return false;
}

template<>
bool Decoder::decode<OPCODES::SAVE_LRPAIR>(bool is_prologue) {
  auto chunk_1 = read_u8();
  auto chunk_2 = read_u8();

  uint32_t reg = (((((chunk_1 & 0x01) << 8) | (chunk_2 & 0xE0)) >> 6) * 2) + 19;
  uint32_t off = (chunk_2 & 0x3F) << 3;

  log("{} x{}, lr, [sp, #{}]", is_prologue ? "stp" : "ldp", reg, off);

  return false;
}

template<>
bool Decoder::decode<OPCODES::SAVE_FREGP>(bool is_prologue) {
  auto chunk_1 = read_u8();
  auto chunk_2 = read_u8();

  uint32_t reg = ((((chunk_1 & 0x01) << 8) | (chunk_2 & 0xC0)) >> 6) + 8;
  uint32_t off = (chunk_2 & 0x3F) << 3;

  log("{} d{}, d{}, [sp, #{}]", is_prologue ? "stp" : "ldp", reg, reg + 1, off);

  return false;
}

template<>
bool Decoder::decode<OPCODES::SAVE_FREGP_X>(bool is_prologue) {
  auto chunk_1 = read_u8();
  auto chunk_2 = read_u8();

  uint32_t reg = ((((chunk_1 & 0x01) << 8) | (chunk_2 & 0xC0)) >> 6) + 8;
  uint32_t off = ((chunk_2 & 0x3F) + 1) << 3;

  is_prologue ? log("stp d{}, d{}, [sp, #-{}]!", reg, reg + 1, off) :
                log("ldp d{}, d{}, [sp], #{}", reg, reg + 1, off);
  return false;
}

template<>
bool Decoder::decode<OPCODES::SAVE_FREG>(bool is_prologue) {
  auto chunk_1 = read_u8();
  auto chunk_2 = read_u8();

  uint32_t reg = ((((chunk_1 & 0x01) << 8) | (chunk_2 & 0xC0)) >> 6) + 8;
  uint32_t off = (chunk_2 & 0x3F) << 3;

  log("{} d{}, [sp, #{}]", is_prologue ? "str" : "ldr", reg, off);

  return false;
}

template<>
bool Decoder::decode<OPCODES::SAVE_FREG_X>(bool is_prologue) {
  [[maybe_unused]] auto _ = read_u8();
  auto chunk_2 = read_u8();

  uint32_t reg = (chunk_2 & 0xE0 >> 5) + 8;
  uint32_t off = ((chunk_2 & 0x1F) + 1) << 3;

  is_prologue ? log("str d{}, [sp, #-{}]!", reg, off) :
                log("ldr d{}, [sp], #{}", reg, off);
  return false;
}

template<>
bool Decoder::decode<OPCODES::ALLOC_Z>(bool is_prologue) {
  // The logic of this function is taken from LLVM:
  // <llvm_root>/llvm/tools/llvm-readobj/ARMWinEHPrinter.cpp
  //
  // Commit: 312d6b48 by Eli Friedman <efriedma@quicinc.com>
  [[maybe_unused]] auto _ = read_u8();
  auto CH1 = read_u8();

  uint32_t offset = CH1;
  log("addvl sp, #{}", is_prologue ? -(int32_t)offset : (int32_t)offset);
  return false;
}

template<>
bool Decoder::decode<OPCODES::ALLOC_L>(bool is_prologue) {
  [[maybe_unused]] auto _ = read_u8();
  auto CH1 = read_u8();
  auto CH2 = read_u8();
  auto CH3 = read_u8();

  uint32_t offset = ((CH1 << 16) | (CH2 << 8) | (CH3 << 0)) << 4;
  log("{} sp, #{}", is_prologue ? "sub" : "add", offset);
  return false;
}

template<>
bool Decoder::decode<OPCODES::SETFP>(bool is_prologue) {
  [[maybe_unused]] auto _ = read_u8();
  log("mov {}, {}", is_prologue ? "fp" : "sp",
                    is_prologue ? "sp" : "fp");
  return false;
}

template<>
bool Decoder::decode<OPCODES::ADDFP>(bool is_prologue) {
  [[maybe_unused]] auto _ = read_u8();
  auto CH1 = read_u8();
  uint32_t nb_bytes = CH1 << 3;

  log("{} {}, {}, #{}", is_prologue ? "add" : "sub",
                        is_prologue ? "fp" : "sp",
                        is_prologue ? "sp" : "fp", nb_bytes);
  return false;
}

template<>
bool Decoder::decode<OPCODES::NOP>(bool/*is_prologue*/) {
  [[maybe_unused]] auto _ = read_u8();
  log("nop");
  return false;
}

template<>
bool Decoder::decode<OPCODES::END>(bool/*is_prologue*/) {
  [[maybe_unused]] auto _ = read_u8();
  log("end");
  return true;
}

template<>
bool Decoder::decode<OPCODES::END_C>(bool /*is_prologue*/) {
  [[maybe_unused]] auto _ = read_u8();
  log("end_c");
  return false;
}

template<>
bool Decoder::decode<OPCODES::SAVE_NEXT>(bool is_prologue) {
  [[maybe_unused]] auto _ = read_u8();
  is_prologue ? log("save next") : log("restore next");
  return false;
}


template<>
bool Decoder::decode<OPCODES::SAVE_ANY_REG>(bool is_prologue) {
  // The logic of this function is taken from LLVM:
  // <llvm_root>/llvm/tools/llvm-readobj/ARMWinEHPrinter.cpp
  //
  // It is based on reverse engineering since this encoding is marked as
  // "reserved" in the official documentation.
  //
  // Kudos to Eli Friedman[1] and Martin Storsjö[2] for their reverse
  // engineering work on this decoding
  //
  // [1] https://github.com/efriedma-quic
  // [2] https://github.com/mstorsjo
  //
  // References
  // https://github.com/llvm/llvm-project/commit/0b61db423bfec26f110c27b7282c3f51aa0f3006
  // https://reviews.llvm.org/D135196
  [[maybe_unused]] auto _ = read_u8();
  auto CH1 = read_u8();
  auto CH2 = read_u8();

  const bool write_back = (CH1 & 0x20) == 0x20;
  const bool is_paired = (CH1 & 0x40) == 0x40;
  int reg_kind = (CH2 & 0xC0) >> 6;
  int reg = CH1 & 0x1F;
  int stack_off = CH2 & 0x3F;

  if (write_back) {
    ++stack_off;
  }

  if (!write_back && !is_paired && reg_kind != 2) {
    stack_off *= 8;
  } else {
    stack_off *= 16;
  }

  int max_reg = 0x1F;

  if (is_paired) {
    --max_reg;
  }

  if (reg_kind == 0) {
    --max_reg;
  }

  if ((CH1 & 0x80) == 0x80 || reg_kind == 3 || reg > max_reg) {
    log("invalid save_any_reg encoding");
    return false;
  }

  if (is_paired) {
    lognf("{} ", is_paired ? (is_prologue ? "stp" : "ldp") :
                            (is_prologue ? "str" : "ldr"));
  }

  char reg_prefix = 'x';
  if (reg_kind == 1) {
    reg_prefix = 'd';
  }
  else if (reg_kind == 2) {
    reg_prefix = 'q';
  }

  if (is_paired) {
    lognf("{}{}, {}{}, ", reg_prefix, reg, reg_prefix, reg + 1);
  } else {
    lognf("{}{}, ", reg_prefix, reg, reg_prefix, reg + 1);
  }

  if (write_back) {
    is_prologue ? log("[sp, #-{}]!", stack_off) :
                  log("[sp], #{}", stack_off);
  } else {
    log("[sp, #{}]", stack_off);
  }

  return false;
}


template<>
bool Decoder::decode<OPCODES::SAVE_ZREG>(bool is_prologue) {
  // The logic of this function is taken from LLVM:
  // <llvm_root>/llvm/tools/llvm-readobj/ARMWinEHPrinter.cpp
  //
  // Commit: 312d6b48 by Eli Friedman <efriedma@quicinc.com>

  [[maybe_unused]] auto _ = read_u8();
  auto CH1 = read_u8();
  auto CH2 = read_u8();

  const uint32_t reg = (CH1 & 0x0F) + 8;
  const uint32_t off = ((CH1 & 0x60) << 1) | (CH2 & 0x3f);

  log("{} z{}, [sp, #{}, mul vl]", is_prologue ? "str" : "ldr", reg, off);
  return false;
}

template<>
bool Decoder::decode<OPCODES::SAVE_PREG>(bool is_prologue) {
  // The logic of this function is taken from LLVM:
  // <llvm_root>/llvm/tools/llvm-readobj/ARMWinEHPrinter.cpp
  //
  // Commit: 312d6b48 by Eli Friedman <efriedma@quicinc.com>

  [[maybe_unused]] auto _ = read_u8();
  auto CH1 = read_u8();
  auto CH2 = read_u8();

  const uint32_t reg = (CH1 & 0x0F) + 8;
  const uint32_t off = ((CH1 & 0x60) << 1) | (CH2 & 0x3f);

  log("{} p{}, [sp, #{}, mul vl]", is_prologue ? "str" : "ldr", reg, off);
  return false;
}

template<>
bool Decoder::decode<OPCODES::E7>(bool is_prologue) {
  // The logic of this function is taken from LLVM:
  // <llvm_root>/llvm/tools/llvm-readobj/ARMWinEHPrinter.cpp
  //
  // Commit: 312d6b48 by Eli Friedman <efriedma@quicinc.com>

  [[maybe_unused]] auto _ = read_u8();
  auto CH1 = read_u8();
  auto CH2 = read_u8();

  if ((CH1 & 0x80) == 0x80) {
    log("reserved encondig");
    return false;
  }

  rewind(3);

  if ((CH2 & 0xC0) == 0xC0) {
    if ((CH1 & 0x10) == 0) {
      return decode<OPCODES::SAVE_ZREG>(is_prologue);
    }
    return decode<OPCODES::SAVE_PREG>(is_prologue);
  }

  return decode<OPCODES::SAVE_ANY_REG>(is_prologue);
}

template<>
bool Decoder::decode<OPCODES::TRAP_FRAME>(bool /*is_prologue*/) {
  [[maybe_unused]] auto _ = read_u8();
  log("trap frame");
  return false;
}

template<>
bool Decoder::decode<OPCODES::MACHINE_FRAME>(bool /*is_prologue*/) {
  [[maybe_unused]] auto _ = read_u8();
  log("machine frame");
  return false;
}

template<>
bool Decoder::decode<OPCODES::CONTEXT>(bool /*is_prologue*/) {
  [[maybe_unused]] auto _ = read_u8();
  log("context");
  return false;
}

template<>
bool Decoder::decode<OPCODES::EC_CONTEXT>(bool /*is_prologue*/) {
  [[maybe_unused]] auto _ = read_u8();
  log("EC context");
  return false;
}

template<>
bool Decoder::decode<OPCODES::CLEAR_UNWOUND_TO_CALL>(bool /*is_prologue*/) {
  [[maybe_unused]] auto _ = read_u8();
  log("clear unwound to call");
  return false;
}

template<>
bool Decoder::decode<OPCODES::PAC_SIGN_LR>(bool is_prologue) {
  [[maybe_unused]] auto _ = read_u8();
  is_prologue ? log("pacibsp") : log("autibsp");
  return false;
}

static constexpr auto HANDLERS = {
  std::make_tuple(0xe0, 0x00, 1, &Decoder::decode<OPCODES::ALLOC_S>),
  std::make_tuple(0xe0, 0x20, 1, &Decoder::decode<OPCODES::SAVE_R19R20_X>),
  std::make_tuple(0xc0, 0x40, 1, &Decoder::decode<OPCODES::SAVE_FPLR>),
  std::make_tuple(0xc0, 0x80, 1, &Decoder::decode<OPCODES::SAVE_FPLR_X>),
  std::make_tuple(0xf8, 0xc0, 2, &Decoder::decode<OPCODES::ALLOC_M>),
  std::make_tuple(0xfc, 0xc8, 2, &Decoder::decode<OPCODES::SAVE_REGP>),
  std::make_tuple(0xfc, 0xcc, 2, &Decoder::decode<OPCODES::SAVE_REGP_X>),
  std::make_tuple(0xfc, 0xd0, 2, &Decoder::decode<OPCODES::SAVE_REG>),
  std::make_tuple(0xfe, 0xd4, 2, &Decoder::decode<OPCODES::SAVE_REG_X>),
  std::make_tuple(0xfe, 0xd6, 2, &Decoder::decode<OPCODES::SAVE_LRPAIR>),
  std::make_tuple(0xfe, 0xd8, 2, &Decoder::decode<OPCODES::SAVE_FREGP>),
  std::make_tuple(0xfe, 0xda, 2, &Decoder::decode<OPCODES::SAVE_FREGP_X>),
  std::make_tuple(0xfe, 0xdc, 2, &Decoder::decode<OPCODES::SAVE_FREG>),
  std::make_tuple(0xff, 0xde, 2, &Decoder::decode<OPCODES::SAVE_FREG_X>),
  std::make_tuple(0xff, 0xdf, 2, &Decoder::decode<OPCODES::ALLOC_Z>),
  std::make_tuple(0xff, 0xe0, 4, &Decoder::decode<OPCODES::ALLOC_L>),
  std::make_tuple(0xff, 0xe1, 1, &Decoder::decode<OPCODES::SETFP>),
  std::make_tuple(0xff, 0xe2, 2, &Decoder::decode<OPCODES::ADDFP>),
  std::make_tuple(0xff, 0xe3, 1, &Decoder::decode<OPCODES::NOP>),
  std::make_tuple(0xff, 0xe4, 1, &Decoder::decode<OPCODES::END>),
  std::make_tuple(0xff, 0xe5, 1, &Decoder::decode<OPCODES::END_C>),
  std::make_tuple(0xff, 0xe6, 1, &Decoder::decode<OPCODES::SAVE_NEXT>),
  std::make_tuple(0xff, 0xe7, 3, &Decoder::decode<OPCODES::E7>),
  std::make_tuple(0xff, 0xe8, 1, &Decoder::decode<OPCODES::TRAP_FRAME>),
  std::make_tuple(0xff, 0xe9, 1, &Decoder::decode<OPCODES::MACHINE_FRAME>),
  std::make_tuple(0xff, 0xea, 1, &Decoder::decode<OPCODES::CONTEXT>),
  std::make_tuple(0xff, 0xeb, 1, &Decoder::decode<OPCODES::EC_CONTEXT>),
  std::make_tuple(0xff, 0xec, 1, &Decoder::decode<OPCODES::CLEAR_UNWOUND_TO_CALL>),
  std::make_tuple(0xff, 0xfc, 1, &Decoder::decode<OPCODES::PAC_SIGN_LR>),
};

ok_error_t Decoder::run(bool prologue) {
  while (stream_) {
    bool found = false;
    for (const auto& [mask, value, len, handler] : HANDLERS) {
      const size_t start_pos = stream_->pos();

      auto word = stream_->peek<uint8_t>();
      if (!word) {
        LIEF_DEBUG("{}:{}", __FUNCTION__, __LINE__);
        return make_error_code(lief_errors::read_error);
      }

      if ((*word & mask) != value) {
        continue;
      }

      found = true;

      lognf("0x{:04x} ", start_pos);
      {
        ScopedStream scoped(*stream_);
        std::vector<uint8_t> bytes;
        bytes.reserve(len);
        for (int i = 0; i < len; ++i) {
          auto byte = scoped->read<uint8_t>();
          if (!byte) {
            break;
          }
          bytes.push_back(*byte);
        }

        lognf("{:02x}", fmt::join(bytes, ""));
        for (int i = 0; i < (4 - len); ++i) {
          lognf("..");
        }
        lognf(" ");
      }

      bool is_done = std::mem_fn(handler)(this, prologue);
      if (is_done) {
        return ok();
      }

      if ((int)(stream_->pos() - start_pos) != len) {
        LIEF_DEBUG("{}:{}", __FUNCTION__, __LINE__);
        return make_error_code(lief_errors::read_error);
      }

      break;
    }

    if (!found) {
      LIEF_DEBUG("{}:{}", __FUNCTION__, __LINE__);
      return make_error_code(lief_errors::read_error);
    }

  }
  return ok();
}
}
