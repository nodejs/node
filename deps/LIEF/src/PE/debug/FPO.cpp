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
#include "LIEF/PE/debug/FPO.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"

#include "logging.hpp"
#include "PE/Structures.hpp"

namespace LIEF::PE {

namespace details {
// From um/winnt.h -- typedef struct _FPO_DATA { ... } FPO_DATA, *PFPO_DATA
struct FPO_DATA {
    uint32_t  ulOffStart;             // offset 1st byte of function code
    uint32_t  cbProcSize;             // # bytes in function
    uint32_t  cdwLocals;              // # bytes in locals/4
    uint16_t  cdwParams;              // # bytes in params/4
    uint16_t  cbProlog : 8;           // # bytes in prolog
    uint16_t  cbRegs   : 3;           // # regs saved
    uint16_t  fHasSEH  : 1;           // TRUE if SEH in func
    uint16_t  fUseBP   : 1;           // TRUE if EBP has been allocated
    uint16_t  reserved : 1;           // reserved for future use
    uint16_t  cbFrame  : 2;           // frame type
};

static_assert(sizeof(FPO_DATA) == 16, "Wrong size");
}

std::unique_ptr<FPO> FPO::parse(
  const details::pe_debug& hdr, Section* section, span<uint8_t> payload)
{
  SpanStream stream(payload);

  auto fpo = std::make_unique<FPO>(hdr, section);

  while (stream) {
    auto entry = stream.read<details::FPO_DATA>();
    if (!entry) {
      break;
    }
    fpo->entries_.push_back({
      /*rva=*/ entry->ulOffStart,
      /*proc_size=*/ entry->cbProcSize,
      /*nb_locals=*/entry->cdwLocals * 4,
      /*parameters_size=*/(uint32_t)entry->cdwParams * 4,
      /*prolog_size=*/(uint16_t)entry->cbProcSize,
      /*nb_saved_regs=*/(uint16_t)entry->cbRegs,
      /*use_seh=*/(bool)entry->fHasSEH,
      /*use_bp=*/(bool)entry->fUseBP,
      /*reserved=*/(uint16_t)entry->reserved,
      /*type=*/(FRAME_TYPE)entry->cbFrame,
    });
  }
  return fpo;
}

std::string FPO::entry_t::to_string() const {
  using namespace fmt;
  std::ostringstream os;
  os << format("{:10} {:<13} {:<9} {:<8} {:<9} {:<5} {:<6} {:<8} {}",
    format("0x{:08x}", rva),
    proc_size, nb_locals, nb_saved_regs, prolog_size, use_bp ? 'Y' : 'N',
    use_seh ? 'Y' : 'N', PE::to_string(type), parameters_size
  );
  return os.str();
}

std::string FPO::to_string() const {
  using namespace fmt;
  std::ostringstream os;
  os << format("{:10} {:13} {:9} {:8} {:9} {:5} {:6} {:8} {}\n", "RVA", "Proc Size",
               "Locals", "Regs", "Prolog", "BP", "SEH", "Type", "Params");
  for (const entry_t& entry : entries()) {
    os << entry.to_string() << '\n';
  }
  return os.str();
}

const char* to_string(FPO::FRAME_TYPE e) {
  switch (e) {
    default: return "UNKNOWN";
    case FPO::FRAME_TYPE::FPO: return "FPO";
    case FPO::FRAME_TYPE::TRAP: return "TRAP";
    case FPO::FRAME_TYPE::TSS: return "TSS";
    case FPO::FRAME_TYPE::NON_FPO: return "NON_FPO";
  }

  return "UNKNOWN";
}

}
