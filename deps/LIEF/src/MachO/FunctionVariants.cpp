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
#include "spdlog/fmt/fmt.h"

#include "LIEF/utils.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"

#include "LIEF/MachO/FunctionVariants.hpp"

#include "MachO/Structures.hpp"

#include "logging.hpp"
#include "internal_utils.hpp"

namespace LIEF {
namespace MachO {

namespace details {
struct runtime_table_entry_t {
  uint32_t impl          : 31,
           another_table :  1;
  uint8_t  flag_bit_nums[4];
};
}

using FLAGS = FunctionVariants::RuntimeTableEntry::FLAGS;
using KIND = FunctionVariants::RuntimeTable::KIND;

static const char* pretty_name(FLAGS f) {
  switch (f) {
    #define FUNCTION_VARIANT_FLAG(name, _, pretty) case FLAGS::name: return pretty;
      #include  "LIEF/MachO/FunctionVariants/Arm64.def"
      #include  "LIEF/MachO/FunctionVariants/PerProcess.def"
      #include  "LIEF/MachO/FunctionVariants/SystemWide.def"
      #include  "LIEF/MachO/FunctionVariants/X86_64.def"
    #undef FUNCTION_VARIANT_FLAG
      case FLAGS::UNKNOWN:
        return "UNKNOWN";
  }
  return "UNKNOWN";
}

FunctionVariants::FunctionVariants(const details::linkedit_data_command& cmd) :
  LoadCommand::LoadCommand{LoadCommand::TYPE(cmd.cmd), cmd.cmdsize},
  data_offset_{cmd.dataoff},
  data_size_{cmd.datasize}
{}

std::ostream& FunctionVariants::print(std::ostream& os) const {
  LoadCommand::print(os) << '\n';
  auto runtime_tbl = runtime_table();

  for (size_t i = 0; i < runtime_tbl.size(); ++i) {
    const RuntimeTable& entry = runtime_tbl[i];
    os << fmt::format("Table #{}\n", i)
       << indent(entry.to_string(), 2) << '\n';
  }

  return os;
}

FunctionVariants::RuntimeTableEntry::RuntimeTableEntry(const details::runtime_table_entry_t& entry) :
  another_table_(entry.another_table),
  impl_(entry.impl)
{
  flag_bit_nums_.fill(0);
  std::copy(std::begin(entry.flag_bit_nums), std::end(entry.flag_bit_nums),
            flag_bit_nums_.begin());
}

span<const uint8_t> get_relevant_flags(span<const uint8_t> raw) {
  auto it = std::find_if(raw.begin(), raw.end(),
      [] (uint8_t v) { return v == 0; });
  if (it == raw.end()) {
    return raw;
  }
  size_t distance = std::distance(raw.begin(), it);

  return raw.subspan(0, distance);
}

std::vector<FLAGS> get_high_level_flags(span<const uint8_t> flags, KIND kind) {
  using RuntimeTableEntry = FunctionVariants::RuntimeTableEntry;

  std::vector<FLAGS> out;
  out.reserve(flags.size());

  for (uint8_t flag : flags) {
    switch (kind) {
      case KIND::UNKNOWN:
        out.push_back((FLAGS)flag);
        break;

      case KIND::PER_PROCESS:
        out.push_back((FLAGS)(flag | RuntimeTableEntry::F_PER_PROCESS));
        break;

      case KIND::SYSTEM_WIDE:
        out.push_back((FLAGS)(flag | RuntimeTableEntry::F_SYSTEM_WIDE));
        break;

      case KIND::ARM64:
        out.push_back((FLAGS)(flag | RuntimeTableEntry::F_ARM64));
        break;

      case KIND::X86_64:
        out.push_back((FLAGS)(flag | RuntimeTableEntry::F_X86_64));
        break;
    }
  }
  return out;
}

std::string FunctionVariants::RuntimeTableEntry::to_string() const {
  std::ostringstream oss;
  if (another_table()) {
    oss << fmt::format("Table: #{}", impl());
  } else {
    oss << fmt::format("Function: 0x{:08x}", impl());
  }
  std::vector<FLAGS> flags = this->flags();
  if (flags.empty()) {
    oss << " 00: default";
  } else {
    std::vector<uint8_t> raw;
    std::vector<const char*> formated;
    for (FLAGS f : flags) {
      raw.push_back(get_raw(f));
      formated.push_back(pretty_name(f));
    }
    oss << fmt::format(" {:02x}: {}", fmt::join(raw, " "), fmt::join(formated, ", "));
  }

  return oss.str();
}

std::string FunctionVariants::RuntimeTable::to_string() const {
  std::ostringstream oss;
  oss << fmt::format("Namespace: {} ({})", MachO::to_string(kind()), (uint32_t)kind()) << '\n';
  for (const RuntimeTableEntry& entry : entries()) {
    oss << "  " << entry << '\n';
  }
  return oss.str();
}

std::vector<FunctionVariants::RuntimeTable> FunctionVariants::parse_payload(SpanStream& stream) {
  std::vector<FunctionVariants::RuntimeTable> result;

  auto tableCount = stream.read<uint32_t>();
  if (!tableCount) {
    LIEF_DEBUG("Error: Failed to read FunctionVariants.OnDiskFormat.tableCount");
    return result;
  }

  for (size_t i = 0; i < *tableCount; ++i) {
    auto tableOffset = stream.read<uint32_t>();
    if (!tableOffset) {
      LIEF_DEBUG("Error: Failed to read FunctionVariants.OnDiskFormat.tableOffsets[{}]", i);
      return result;
    }

    {
      ScopedStream runtime_table_strm(stream, *tableOffset);
      if (auto entry = parse_entry(*runtime_table_strm)) {
        result.push_back(std::move(*entry));
      } else {
        LIEF_DEBUG("Failed to parse entry #{} at offset: 0x{:06x}", i, *tableOffset);
      }
    }
  }
  return result;
}

result<FunctionVariants::RuntimeTable> FunctionVariants::parse_entry(BinaryStream& stream) {
  auto kind = stream.read<uint32_t>();

  if (!kind) {
    LIEF_DEBUG("Error: Failed to read FunctionVariantsRuntimeTable.kind");
    return make_error_code(kind.error());
  }

  RuntimeTable runtime_table((RuntimeTable::KIND)*kind, stream.pos());

  auto count = stream.read<uint32_t>();

  if (!count) {
    LIEF_DEBUG("Error: Failed to read FunctionVariantsRuntimeTable.count");
    return make_error_code(count.error());
  }

  for (size_t i = 0; i < *count; ++i) {
    auto raw_entry = stream.read<details::runtime_table_entry_t>();
    if (!raw_entry) {
      LIEF_DEBUG("Error: Failed to read FunctionVariantsRuntimeTable.entries[{}]", i);
      return runtime_table;
    }

    RuntimeTableEntry entry(*raw_entry);

    span<const uint8_t> flags = get_relevant_flags(entry.flag_bit_nums());
    std::vector<FLAGS> hl_flags = get_high_level_flags(flags, runtime_table.kind());
    entry.set_flags(std::move(hl_flags));

    runtime_table.add(std::move(entry));
  }
  return runtime_table;
}

const char* to_string(FunctionVariants::RuntimeTable::KIND kind) {
  using KIND = FunctionVariants::RuntimeTable::KIND;
  switch (kind) {
    default:
    case KIND::UNKNOWN: return "UNKNOWN";
    case KIND::PER_PROCESS: return "PER_PROCESS";
    case KIND::SYSTEM_WIDE: return "SYSTEM_WIDE";
    case KIND::ARM64: return "ARM64";
    case KIND::X86_64: return "X86_64";
  }
}

const char* to_string(FLAGS f) {
  switch (f) {
    #define FUNCTION_VARIANT_FLAG(X, __, _) case FLAGS::X: return #X;
      #include  "LIEF/MachO/FunctionVariants/Arm64.def"
      #include  "LIEF/MachO/FunctionVariants/PerProcess.def"
      #include  "LIEF/MachO/FunctionVariants/SystemWide.def"
      #include  "LIEF/MachO/FunctionVariants/X86_64.def"
    #undef FUNCTION_VARIANT_FLAG
      case FLAGS::UNKNOWN:
        return "UNKNOWN";
  }
  return "UNKNOWN";
}

}
}
