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
#include <sstream>
#include "LIEF/PE/exceptions_info/RuntimeFunctionX64.hpp"
#include "LIEF/PE/exceptions_info/UnwindCodeX64.hpp"

#include "LIEF/BinaryStream/BinaryStream.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"
#include "LIEF/PE/Parser.hpp"
#include "LIEF/PE/Binary.hpp"

#include "logging.hpp"
#include "frozen.hpp"
#include "internal_utils.hpp"

#include "LIEF/PE/exceptions_info/internal_x64.hpp"

namespace LIEF::PE {

inline bool should_have_handler(const RuntimeFunctionX64::unwind_info_t& info) {
  return info.has(RuntimeFunctionX64::UNWIND_FLAGS::EXCEPTION_HANDLER |
                  RuntimeFunctionX64::UNWIND_FLAGS::TERMINATE_HANDLER);
}

std::unique_ptr<RuntimeFunctionX64>
  RuntimeFunctionX64::parse(Parser& ctx, BinaryStream& strm, bool skip_unwind)
{
  // From the documentation:
  // > The RUNTIME_FUNCTION structure must be DWORD aligned in memory.
  strm.align(sizeof(uint32_t));

  auto rva_start = strm.read<uint32_t>();
  if (!rva_start) {
    LIEF_WARN("Can't read exception info RVA start (line: {})", __LINE__);
    return nullptr;
  }

  auto rva_end = strm.read<uint32_t>();
  if (!rva_end) {
    LIEF_WARN("Can't read exception info RVA end (line: {})", __LINE__);
    return nullptr;
  }

  auto unwind_rva = strm.read<uint32_t>();
  if (!unwind_rva) {
    LIEF_WARN("Can't read exception info unwind rva (line: {})", __LINE__);
    return nullptr;
  }

  auto rfunc =
    std::make_unique<RuntimeFunctionX64>(*rva_start, *rva_end, *unwind_rva);

  if (!skip_unwind) {
    ctx.memoize(*rfunc);
  }

  if (*unwind_rva == 0 || skip_unwind) {
    return rfunc;
  }

  uint64_t unwind_off = ctx.bin().rva_to_offset(*unwind_rva);
  {
    ScopedStream unwind_strm(ctx.stream(), unwind_off);
    if (auto is_ok = parse_unwind(ctx, *unwind_strm, *rfunc); !is_ok) {
      LIEF_DEBUG("Failed to parse unwind data for function: 0x{:06x}",
                 rfunc->rva_start());
      return rfunc;
    }
  }

  return rfunc;
}

ok_error_t RuntimeFunctionX64::parse_unwind(Parser& ctx, BinaryStream& strm,
                                            RuntimeFunctionX64& func)
{
  auto VersionFlags = strm.read<uint8_t>();
  if (!VersionFlags) {
    return make_error_code(VersionFlags.error());
  }

  auto SizeOfProlog = strm.read<uint8_t>();
  if (!SizeOfProlog) {
    return make_error_code(SizeOfProlog.error());
  }

  auto CountOfCodes = strm.read<uint8_t>();
  if (!CountOfCodes) {
    return make_error_code(CountOfCodes.error());
  }

  auto FrameInfo = strm.read<uint8_t>();
  if (!FrameInfo) {
    return make_error_code(FrameInfo.error());
  }


  const uint8_t version = *VersionFlags & 0x07;
  const uint8_t flags = (*VersionFlags >> 3) & 0x1f;
  const uint8_t frame_reg = *FrameInfo & 0x0f;
  const uint8_t frame_off = (*VersionFlags >> 4) & 0x0f;

  LIEF_DEBUG("Parsing unwind info for function 0x{:08x}", func.rva_start());
  LIEF_DEBUG("  Version: {}", version);
  LIEF_DEBUG("  Flags: {}", flags);
  LIEF_DEBUG("  SizeOfProlog: {}", *SizeOfProlog);
  LIEF_DEBUG("  Frame register: {}", frame_reg);
  LIEF_DEBUG("  Frame offset: {}", frame_off);
  LIEF_DEBUG("  CountOfCodes: {}", *CountOfCodes);

  std::vector<uint8_t> unwind_code;
  size_t count_aligned = *CountOfCodes + (int)(*CountOfCodes % 2 == 1);

  if (!strm.read_data(unwind_code, count_aligned * sizeof(uint16_t))) {
    return make_error_code(lief_errors::read_error);
  }

  unwind_info_t info;
  info.version = version;
  info.flags = flags;
  info.sizeof_prologue = *SizeOfProlog;
  info.count_opcodes = *CountOfCodes;
  info.frame_reg = frame_reg;
  info.frame_reg_offset = frame_off;
  info.raw_opcodes = std::move(unwind_code);


  if (should_have_handler(info)) {
    auto handler = strm.read<uint32_t>();
    if (!handler) {
      return make_error_code(handler.error());
    }
    LIEF_DEBUG("  Handler: 0x{:06x}", *handler);
    info.handler = *handler;
  }

  if (info.has(UNWIND_FLAGS::CHAIN_INFO)) {
    LIEF_DEBUG("  Chained!");
    std::unique_ptr<RuntimeFunctionX64> chained = parse(ctx, strm, /*skip_unwind=*/true);
    if (chained != nullptr) {
      LIEF_DEBUG("    chain: 0x{:08x} - 0x{:08x} - 0x{:08x}",
                 chained->rva_start(), chained->rva_end(), chained->unwind_rva());
      ExceptionInfo* link = ctx.find_exception_info(chained->rva_start());
      if (link != nullptr) {
        LIEF_DEBUG("    chain (found): 0x{:08x} - 0x{:08x} - 0x{:08x}",
                   link->rva_start(), link->as<RuntimeFunctionX64>()->rva_end(),
                   link->as<RuntimeFunctionX64>()->unwind_rva());
        assert(link->arch() == ExceptionInfo::ARCH::X86_64);
        info.chained = link->as<RuntimeFunctionX64>();
      } else {
        LIEF_DEBUG("RuntimeFunctionX64 0x{:06x}: Can't find linked chained info",
                   func.rva_start());
        ctx.add_non_resolved(func, chained->rva_start());
      }
    } else {
      LIEF_WARN("RuntimeFunctionX64 0x{:06x}: chained info corrupted",
                func.rva_start());
    }
  }

  func.unwind_info(std::move(info));
  return ok();
}

RuntimeFunctionX64::unwind_info_t::opcodes_t
  RuntimeFunctionX64::unwind_info_t::opcodes() const
{
  using namespace unwind_x64;
  opcodes_t out;

  SpanStream stream(raw_opcodes);

  const bool is_odd = count_opcodes % 2 == 1;
  while (stream) {
    if (is_odd && stream.pos() == stream.size() - sizeof(uint16_t)) {
      // Skip the padding opcode
      break;
    }
    std::unique_ptr<Code> op = Code::create_from(*this, stream);
    if (op == nullptr) {
      break;
    }

    out.push_back(std::move(op));
  }

  [[maybe_unused]] const int32_t delta = (int32_t)stream.size() - stream.pos();
  LIEF_DEBUG("Nb bytes left: {}", delta);
  return out;
}


std::string RuntimeFunctionX64::unwind_info_t::to_string() const {
  std::ostringstream oss;
  oss << "unwind_info_t {\n";
  oss << "  Version: " << (int)version << '\n'
      << "  Flags: " << (int)flags << '\n'
      << "  Size of prologue: " << (int)sizeof_prologue << '\n';

  opcodes_t opcodes =  this->opcodes();
  oss
    << fmt::format("  Nb opcodes: {} ({})\n", (int)count_opcodes, opcodes.size());

  if (!opcodes.empty()) {
    oss << "  Opcodes: [\n";
    for (const auto& opcode : opcodes) {
      oss << indent(opcode->to_string(), 4);
    }
    oss << "  ];\n";
  }

  if (chained != nullptr) {
    oss << "  Chained: {\n"
        << fmt::format("    RVA: [0x{:06x}, 0x{:06x}]\n", chained->rva_start(),
                       chained->rva_end())
        << fmt::format("    Unwind RVA: 0x{:06x}\n", chained->unwind_rva())
        << "  }\n";
  }

  if (handler) {
    oss << fmt::format("  Handler: 0x{:06x}\n", *handler);
  }
  oss << "}";
  return oss.str();
}

std::string RuntimeFunctionX64::to_string() const {
  std::ostringstream oss;
  oss << "RuntimeFunctionX64 {\n";
  oss << fmt::format("  RVA: [0x{:06x}, 0x{:06x}] ({} bytes)\n",
                     rva_start(), rva_end(), rva_end() - rva_start())
      << fmt::format("  Unwind info RVA: 0x{:06x}\n", unwind_rva());
  if (const unwind_info_t* info = unwind_info()) {
    oss << indent(info->to_string(), 2);
  }
  oss << "}";
  return oss.str();
}

const char* to_string(RuntimeFunctionX64::UNWIND_OPCODES e) {
  #define ENTRY(X) std::pair(RuntimeFunctionX64::UNWIND_OPCODES::X, #X)
  STRING_MAP enums2str {
    ENTRY(ALLOC_LARGE),
    ENTRY(ALLOC_SMALL),
    ENTRY(EPILOG),
    ENTRY(PUSH_MACHFRAME),
    ENTRY(PUSH_NONVOL),
    ENTRY(SAVE_NONVOL),
    ENTRY(SAVE_NONVOL_FAR),
    ENTRY(SAVE_XMM128),
    ENTRY(SAVE_XMM128_FAR),
    ENTRY(SET_FPREG),
    ENTRY(SPARE),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

const char* to_string(RuntimeFunctionX64::UNWIND_REG e) {
  #define ENTRY(X) std::pair(RuntimeFunctionX64::UNWIND_REG::X, #X)
  STRING_MAP enums2str {
    ENTRY(R10),
    ENTRY(R11),
    ENTRY(R12),
    ENTRY(R13),
    ENTRY(R14),
    ENTRY(R15),
    ENTRY(R8),
    ENTRY(R9),
    ENTRY(RAX),
    ENTRY(RBP),
    ENTRY(RBX),
    ENTRY(RCX),
    ENTRY(RDI),
    ENTRY(RDX),
    ENTRY(RSI),
    ENTRY(RSP),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}


const char* to_string(RuntimeFunctionX64::UNWIND_FLAGS op) {
  switch(op) {
    default:
      return "UNKNOWN";
    case RuntimeFunctionX64::UNWIND_FLAGS::CHAIN_INFO:
      return "CHAIN_INFO";
    case RuntimeFunctionX64::UNWIND_FLAGS::EXCEPTION_HANDLER:
      return "EXCEPTION_HANDLER";
    case RuntimeFunctionX64::UNWIND_FLAGS::TERMINATE_HANDLER:
      return "TERMINATE_HANDLER";
  }
  return "UNKNOWN";
}

}
