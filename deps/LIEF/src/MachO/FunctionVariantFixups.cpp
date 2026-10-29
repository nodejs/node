/* Copyright 2017 - 2026 R. Thomas
 * Copyright 2017 - 2026 Quarkslab
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
#include "spdlog/fmt/fmt.h"

#include "LIEF/BinaryStream/SpanStream.hpp"

#include "LIEF/MachO/FunctionVariantFixups.hpp"
#include "LIEF/MachO/SegmentCommand.hpp"
#include "MachO/Structures.hpp"

#include "logging.hpp"

namespace LIEF::MachO {

static_assert(sizeof(details::function_variant_fixup_t) == 8);

static const char* pac_key_name(uint8_t key) {
  switch (key) {
    case 0: return "IA";
    case 1: return "IB";
    case 2: return "DA";
    case 3: return "DB";
    default: return "??";
  }
}

FunctionVariantFixups::FunctionVariantFixups(
    const details::linkedit_data_command& cmd
) :
  LoadCommand::LoadCommand{LoadCommand::TYPE(cmd.cmd), cmd.cmdsize},
  data_offset_{cmd.dataoff},
  data_size_{cmd.datasize} {}

FunctionVariantFixups::Fixup::Fixup(const details::function_variant_fixup_t& raw) :
  seg_offset_(raw.seg_offset),
  seg_index_(raw.seg_index),
  variant_index_(raw.variant_index),
  pac_auth_(raw.pac_auth != 0),
  pac_address_(raw.pac_address != 0),
  pac_key_(raw.pac_key),
  pac_diversity_(raw.pac_diversity) {}

std::vector<FunctionVariantFixups::Fixup>
    FunctionVariantFixups::parse_payload(SpanStream& stream) {
  std::vector<Fixup> result;

  const size_t count = stream.size() / sizeof(details::function_variant_fixup_t);
  result.reserve(count);

  for (size_t i = 0; i < count; ++i) {
    auto raw = stream.read<details::function_variant_fixup_t>();
    if (!raw) {
      LIEF_DEBUG("Failed to read FunctionVariantFixups.fixups[{}]", i);
      break;
    }
    result.emplace_back(*raw);
  }

  return result;
}

std::string FunctionVariantFixups::Fixup::to_string() const {
  std::ostringstream oss;

  if (const SegmentCommand* seg = segment()) {
    oss << fmt::format("{}+{:#x} ({:#x})", seg->name(), seg_offset(),
                       seg->virtual_address() + seg_offset());
  } else {
    oss << fmt::format("seg[{}]+{:#x}", seg_index(), seg_offset());
  }

  oss << fmt::format(" -> variant #{}", variant_index());

  if (pac_auth()) {
    oss << fmt::format(" [PAC: key={} addr={} diversity={:#06x}]",
                       pac_key_name(pac_key()), pac_address(), pac_diversity());
  }

  return oss.str();
}

std::ostream& FunctionVariantFixups::print(std::ostream& os) const {
  LoadCommand::print(os) << '\n';
  auto entries = fixups();
  os << fmt::format("nb_fixups = {}\n", entries.size());
  for (size_t i = 0; i < entries.size(); ++i) {
    os << fmt::format("  [{:04d}] {}\n", i, entries[i].to_string());
  }
  return os;
}


}
