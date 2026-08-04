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
#include <algorithm>

#include "LIEF/Visitor.hpp"

#include "LIEF/PE/EnumToString.hpp"
#include "LIEF/PE/Header.hpp"

#include "PE/Structures.hpp"
#include "frozen.hpp"
#include "internal_utils.hpp"

#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/ranges.h>

namespace LIEF {
namespace PE {

static constexpr std::array CHARACTERISTICS_LIST = {
  Header::CHARACTERISTICS::RELOCS_STRIPPED,
  Header::CHARACTERISTICS::EXECUTABLE_IMAGE,
  Header::CHARACTERISTICS::LINE_NUMS_STRIPPED,
  Header::CHARACTERISTICS::LOCAL_SYMS_STRIPPED,
  Header::CHARACTERISTICS::AGGRESSIVE_WS_TRIM,
  Header::CHARACTERISTICS::LARGE_ADDRESS_AWARE,
  Header::CHARACTERISTICS::BYTES_REVERSED_LO,
  Header::CHARACTERISTICS::NEED_32BIT_MACHINE,
  Header::CHARACTERISTICS::DEBUG_STRIPPED,
  Header::CHARACTERISTICS::REMOVABLE_RUN_FROM_SWAP,
  Header::CHARACTERISTICS::NET_RUN_FROM_SWAP,
  Header::CHARACTERISTICS::SYSTEM,
  Header::CHARACTERISTICS::DLL,
  Header::CHARACTERISTICS::UP_SYSTEM_ONLY,
  Header::CHARACTERISTICS::BYTES_REVERSED_HI
};

bool Header::is_known_machine(uint16_t machine) {
  return to_string((MACHINE_TYPES)machine) != std::string("UNKNOWN");
}

Header Header::create(PE_TYPE type) {
  Header hdr;
  const size_t sizeof_dirs = details::DEFAULT_NUMBER_DATA_DIRECTORIES *
                             sizeof(details::pe_data_directory);
  hdr.signature({ 'P', 'E', '\0', '\0' });
  hdr.sizeof_optional_header(type == PE_TYPE::PE32 ?
                             sizeof(details::pe32_optional_header) :
                             sizeof(details::pe64_optional_header) + sizeof_dirs);
  hdr.machine(MACHINE_TYPES::AMD64);
  return hdr;
}

Header::Header(const details::pe_header& header) :
  machine_(static_cast<MACHINE_TYPES>(header.Machine)),
  nb_sections_(header.NumberOfSections),
  timedatestamp_(header.TimeDateStamp),
  pointerto_symtab_(header.PointerToSymbolTable),
  nb_symbols_(header.NumberOfSymbols),
  sizeof_opt_header_(header.SizeOfOptionalHeader),
  characteristics_(header.Characteristics)

{
  std::copy(std::begin(header.signature), std::end(header.signature),
            std::begin(signature_));
}

std::vector<Header::CHARACTERISTICS> Header::characteristics_list() const {
  std::vector<Header::CHARACTERISTICS> list;
  list.reserve(3);
  std::copy_if(CHARACTERISTICS_LIST.begin(), CHARACTERISTICS_LIST.end(),
               std::back_inserter(list),
               [this] (CHARACTERISTICS c) { return has_characteristic(c); });

  return list;
}

void Header::accept(LIEF::Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& operator<<(std::ostream& os, const Header& entry) {
  const Header::signature_t& sig = entry.signature();
  const std::string& signature_str =
    fmt::format("{:02x} {:02x} {:02x} {:02x}",
                sig[0], sig[1], sig[2], sig[3]);

  const auto& list = entry.characteristics_list();
  std::vector<std::string> list_str;
  list_str.reserve(list.size());
  std::transform(list.begin(), list.end(), std::back_inserter(list_str),
                 [] (const auto c) { return to_string(c); });

  os << fmt::format("Signature:               {}\n", signature_str)
     << fmt::format("Machine:                 {}\n", to_string(entry.machine()))
     << fmt::format("Number of sections:      {}\n", entry.numberof_sections())
     << fmt::format("Pointer to symbol table: 0x{:x}\n", entry.pointerto_symbol_table())
     << fmt::format("Number of symbols:       {}\n", entry.numberof_symbols())
     << fmt::format("Size of optional header: 0x{:x}\n", entry.sizeof_optional_header())
     << fmt::format("Characteristics:         {}\n", fmt::join(list_str, ", "))
     << fmt::format("Timtestamp:              {} ({})\n",
                    entry.time_date_stamp(), ts_to_str(entry.time_date_stamp()));

  return os;

}

const char* to_string(Header::MACHINE_TYPES e) {
  #define ENTRY(X) std::pair(Header::MACHINE_TYPES::X, #X)
  STRING_MAP enums2str {
    ENTRY(UNKNOWN),
    ENTRY(ALPHA),
    ENTRY(ALPHA64),
    ENTRY(AM33),
    ENTRY(AMD64),
    ENTRY(ARM),
    ENTRY(ARMNT),
    ENTRY(ARM64),
    ENTRY(EBC),
    ENTRY(I386),
    ENTRY(IA64),
    ENTRY(LOONGARCH32),
    ENTRY(LOONGARCH64),
    ENTRY(M32R),
    ENTRY(MIPS16),
    ENTRY(MIPSFPU),
    ENTRY(MIPSFPU16),
    ENTRY(POWERPC),
    ENTRY(POWERPCFP),
    ENTRY(POWERPCBE),
    ENTRY(R4000),
    ENTRY(RISCV32),
    ENTRY(RISCV64),
    ENTRY(RISCV128),
    ENTRY(SH3),
    ENTRY(SH3DSP),
    ENTRY(SH4),
    ENTRY(SH5),
    ENTRY(THUMB),
    ENTRY(WCEMIPSV2),
    ENTRY(ARM64EC),
    ENTRY(ARM64X),
    ENTRY(CHPE_X86),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

const char* to_string(Header::CHARACTERISTICS e) {
  #define ENTRY(X) std::pair(Header::CHARACTERISTICS::X, #X)
  STRING_MAP enums2str {
    ENTRY(NONE),
    ENTRY(RELOCS_STRIPPED),
    ENTRY(EXECUTABLE_IMAGE),
    ENTRY(LINE_NUMS_STRIPPED),
    ENTRY(LOCAL_SYMS_STRIPPED),
    ENTRY(AGGRESSIVE_WS_TRIM),
    ENTRY(LARGE_ADDRESS_AWARE),
    ENTRY(BYTES_REVERSED_LO),
    ENTRY(NEED_32BIT_MACHINE),
    ENTRY(DEBUG_STRIPPED),
    ENTRY(REMOVABLE_RUN_FROM_SWAP),
    ENTRY(NET_RUN_FROM_SWAP),
    ENTRY(SYSTEM),
    ENTRY(DLL),
    ENTRY(UP_SYSTEM_ONLY),
    ENTRY(BYTES_REVERSED_HI),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "NONE";
}




}
}
