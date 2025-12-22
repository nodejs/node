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
#include "LIEF/span.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"
#include "LIEF/BinaryStream/BinaryStream.hpp"
#include "LIEF/PE/exceptions_info/AArch64/UnpackedFunction.hpp"

#include "LIEF/PE/exceptions_info/internal_arm64.hpp"
#include "PE/exceptions_info/UnwindAArch64Decoder.hpp"

#include "logging.hpp"
#include "internal_utils.hpp"

namespace LIEF::PE::unwind_aarch64 {

using epilog_scope_t = UnpackedFunction::epilog_scope_t;

std::unique_ptr<UnpackedFunction> UnpackedFunction::parse(
  Parser& /*ctx*/, BinaryStream& strm, uint32_t xdata_rva, uint32_t rva
) {
  static constexpr auto WIDTH = 20;
  details::arm64_unpacked_t unpacked;

  LIEF_DEBUG("Parsing unpacked function 0x{:08x}", rva);

  const uint64_t strm_offset = strm.pos();

  auto word1 = strm.read<uint32_t>();
  if (!word1) {
    LIEF_WARN("Can't read unpacked exception info (word1, line: {})", __LINE__);
    return nullptr;
  }

  unpacked.data[0] = *word1;

  if (unpacked.is_extended()) {
    auto word2 = strm.read<uint32_t>();
    if (!word2) {
      LIEF_WARN("Can't read unpacked exception info (word2, line: {})", __LINE__);
      return nullptr;
    }
    unpacked.data[1] = *word2;
  }

  auto func = std::make_unique<UnpackedFunction>(rva, unpacked.function_length());

  (*func)
    .xdata_rva(xdata_rva)
    .version(unpacked.version())
    .X(unpacked.X())
    .E(unpacked.E())
    .epilog_cnt_offset(unpacked.epilog_count())
    .code_words(unpacked.code_words())
    .is_extended(unpacked.is_extended())
  ;

  LIEF_DEBUG("  {:{}}: {}", "Extended", WIDTH, unpacked.is_extended());
  LIEF_DEBUG("  {:{}}: 0x{:04x}", "Function RVA", WIDTH, rva);
  LIEF_DEBUG("  {:{}}: 0x{:04x}", "Function Length", WIDTH, unpacked.function_length());
  LIEF_DEBUG("  {:{}}: 0x{:04x}", "Version", WIDTH, unpacked.version());
  LIEF_DEBUG("  {:{}}: {}", "Exception Data", WIDTH, unpacked.X());
  LIEF_DEBUG("  {:{}}: {}", "Epilog Packed", WIDTH, unpacked.E());
  LIEF_DEBUG("  {:{}}: {}", "Code Words", WIDTH, unpacked.code_words());
  LIEF_DEBUG("  {:{}}: {}", unpacked.E() ? "Epilog Offset" : "Epilog Scopes",
             WIDTH, unpacked.epilog_count());
  LIEF_DEBUG("  {:{}}: {}", "Bytecode length", WIDTH, unpacked.code_words() * 4);

  const uint16_t ecount = unpacked.epilog_count();

  /// If E is 0, it specifies the count of the total number of epilog scopes
  /// described in section 2. If more than 31 scopes exist in the function, then
  /// the Code Words field must be set to 0 to indicate that an extension word
  /// is required.
  std::vector<uint32_t> scopes;
  if (unpacked.E() == 0) {
    func->epilog_scopes_offset_ = strm.pos() - strm_offset;
    if (!strm.read_objects(scopes, ecount)) {
      LIEF_DEBUG("Can't read #{} epilog scopes", ecount);
      return func;
    }
    LIEF_DEBUG("  {:{}}: {}", "Number of scopes (packed)", WIDTH, ecount);
    func->epilog_scopes_.reserve(ecount);
    std::transform(scopes.begin(), scopes.end(),
                   std::back_inserter(func->epilog_scopes_),
      [] (uint32_t raw) { return epilog_scope_t::from_raw(raw); }
    );
  }

  {
    func->unwind_code_offset_ = strm.pos() - strm_offset;
    std::vector<uint8_t> unwind_bytecode;
    if (!strm.read_data(unwind_bytecode, unpacked.code_words() * sizeof(uint32_t))) {
      LIEF_DEBUG("Can't read unwind bytecode");
      return func;
    }
    func->unwind_code(std::move(unwind_bytecode));
  }

  if (unpacked.X()) {
    func->exception_handler_offset_ = strm.pos() - strm_offset;
    auto ehandler_rva = strm.read<uint32_t>();
    if (!ehandler_rva) {
      LIEF_DEBUG("Can't read Exception Handler RVA");
      return func;
    }

    LIEF_DEBUG("  {:{}}: 0x{:08x}", "Exception Handler", WIDTH, *ehandler_rva);
    func->exception_handler(*ehandler_rva);
  }

  return func;
}

epilog_scope_t epilog_scope_t::from_raw(uint32_t raw) {
  details::arm64_epilog_scope_t scope{raw};

  return {
    /* .start_offset = */ scope.start_offset(),
    /* .start_index = */scope.start_index(),
    /* .reserved = */scope.reserved(),
  };
}

std::string UnpackedFunction::to_string() const {
  using namespace fmt;
  std::ostringstream oss;
  oss << "Runtime Unpacked AArch64 Function {\n";
  oss << format("  Range(RVA): 0x{:08x} - 0x{:08x}\n", rva_start(), rva_end());
  oss << format("  Unwind location (RVA): 0x{:08x}\n", xdata_rva());
  oss << format("  Length={} Vers={} X={} E={}, CodeWords={}\n",
                length(), version(), X(), E(), code_words());

  if (X() == 1) {
    oss << format("  Exception Handler: 0x{:08x}\n", exception_handler());
  }
  if (E() == 0) {
    oss << format("  Epilogs={}\n", epilog_count());
  }
  if (E() == 1) {
    oss << format("  Epilogs (offset)=0x{:06x}\n", epilog_offset());
  }

  if (E() == 0) {
    span<const uint8_t> code = unwind_code();
    std::ostringstream pretty_code;
    SpanStream unwind_stream(code);
    unwind_aarch64::Decoder decoder(unwind_stream, pretty_code);
    decoder.run(/*prologue=*/true);
    oss << "  Prolog unwind:\n";
    oss << indent(pretty_code.str(), 4);

    for (size_t i = 0; i < epilog_scopes_.size(); ++i) {
      const epilog_scope_t& scope = epilog_scopes_[i];
      oss << format("  Epilog #{} unwind:  (Offset={}, Index={}, Reserved={})\n", i + 1,
                    scope.start_offset, scope.start_index, scope.reserved);

      if (uint32_t offset = scope.start_index; offset > 0 && offset < code.size()) {
        span<const uint8_t> epilog_code = code.subspan(scope.start_index);
        SpanStream unwind_stream(epilog_code);
        std::ostringstream pretty_code;
        unwind_aarch64::Decoder decoder(unwind_stream, pretty_code);
        decoder.run(/*prologue=*/false);
        oss << indent(pretty_code.str(), 4);
      }
    }
  }

  if (E() == 1) {
    span<const uint8_t> code = unwind_code();
    {
      SpanStream unwind_stream(code);
      std::ostringstream pretty_code;
      unwind_aarch64::Decoder decoder(unwind_stream, pretty_code);
      decoder.run(/*prologue=*/true);
      oss << "  Prolog unwind:\n";
      oss << indent(pretty_code.str(), 4);
    }

    if (uint32_t offset = epilog_offset(); offset > 0 && offset < code.size()) {
      span<const uint8_t> epilog_code = code.subspan(epilog_offset());
      SpanStream unwind_stream(epilog_code);
      std::ostringstream pretty_code;
      unwind_aarch64::Decoder decoder(unwind_stream, pretty_code);
      decoder.run(/*prologue=*/false);
      oss << "  Epilog unwind:\n";
      oss << indent(pretty_code.str(), 4);
    }
  }

  oss << '}';
  return oss.str();
}
}
