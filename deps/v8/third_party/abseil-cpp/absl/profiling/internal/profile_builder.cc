// Copyright 2025 The Abseil Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may
// obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/profiling/internal/profile_builder.h"

#ifdef __linux__
#include <elf.h>
#include <link.h>
#endif  // __linux__

#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/casts.h"
#include "absl/base/config.h"
#include "absl/base/internal/raw_logging.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace debugging_internal {

namespace {

// This file contains a simplified implementation of the pprof profile builder,
// which avoids a dependency on protobuf.
//
// The canonical profile proto definition is at
// https://github.com/google/pprof/blob/master/proto/profile.proto
//
// Wire-format encoding is a simple sequence of (tag, value) pairs. The tag
// is a varint-encoded integer, where the low 3 bits are the wire type, and the
// high bits are the field number.
//
// For the fields we care about, we'll be using the following wire types:
//
// Wire Type 0: Varint-encoded integer.
// Wire Type 2: Length-delimited. Used for strings and sub-messages.
enum class WireType {
  kVarint = 0,
  kLengthDelimited = 2,
};

#ifdef __linux__
// Returns the Phdr of the first segment of the given type.
const ElfW(Phdr) * GetFirstSegment(const dl_phdr_info* const info,
                                   const ElfW(Word) segment_type) {
  for (int i = 0; i < info->dlpi_phnum; ++i) {
    if (info->dlpi_phdr[i].p_type == segment_type) {
      return &info->dlpi_phdr[i];
    }
  }
  return nullptr;
}

// Return DT_SONAME for the given image.  If there is no PT_DYNAMIC or if
// PT_DYNAMIC does not contain DT_SONAME, return nullptr.
static const char* GetSoName(const dl_phdr_info* const info) {
  const ElfW(Phdr)* const pt_dynamic = GetFirstSegment(info, PT_DYNAMIC);
  if (pt_dynamic == nullptr) {
    return nullptr;
  }
  const ElfW(Dyn)* dyn =
      reinterpret_cast<ElfW(Dyn)*>(info->dlpi_addr + pt_dynamic->p_vaddr);
  const ElfW(Dyn)* dt_strtab = nullptr;
  const ElfW(Dyn)* dt_strsz = nullptr;
  const ElfW(Dyn)* dt_soname = nullptr;
  for (; dyn->d_tag != DT_NULL; ++dyn) {
    if (dyn->d_tag == DT_SONAME) {
      dt_soname = dyn;
    } else if (dyn->d_tag == DT_STRTAB) {
      dt_strtab = dyn;
    } else if (dyn->d_tag == DT_STRSZ) {
      dt_strsz = dyn;
    }
  }
  if (dt_soname == nullptr) {
    return nullptr;
  }
  ABSL_RAW_CHECK(dt_strtab != nullptr, "Unexpected nullptr");
  ABSL_RAW_CHECK(dt_strsz != nullptr, "Unexpected nullptr");
  const char* const strtab = reinterpret_cast<char*>(
      info->dlpi_addr + static_cast<ElfW(Word)>(dt_strtab->d_un.d_val));
  ABSL_RAW_CHECK(dt_soname->d_un.d_val < dt_strsz->d_un.d_val,
                 "Unexpected order");
  return strtab + dt_soname->d_un.d_val;
}

// Helper function to get the build ID of a shared object.
std::string GetBuildId(const dl_phdr_info* const info) {
  std::string result;

  // pt_note contains entries (of type ElfW(Nhdr)) starting at
  //   info->dlpi_addr + pt_note->p_vaddr
  // with length
  //   pt_note->p_memsz
  //
  // The length of each entry is given by
  //   Align(sizeof(ElfW(Nhdr)) + nhdr->n_namesz) + Align(nhdr->n_descsz)
  for (int i = 0; i < info->dlpi_phnum; ++i) {
    const ElfW(Phdr)* pt_note = &info->dlpi_phdr[i];
    if (pt_note->p_type != PT_NOTE) continue;

    const char* note =
        reinterpret_cast<char*>(info->dlpi_addr + pt_note->p_vaddr);
    const char* const last = note + pt_note->p_filesz;
    const ElfW(Xword) align = pt_note->p_align;
    while (note < last) {
      const ElfW(Nhdr)* const nhdr = reinterpret_cast<const ElfW(Nhdr)*>(note);
      if (note + sizeof(*nhdr) > last) {
        // Corrupt PT_NOTE
        break;
      }

      // Both the start and end of the descriptor are aligned by sh_addralign
      // (= p_align).
      const ElfW(Xword) desc_start =
          (sizeof(*nhdr) + nhdr->n_namesz + align - 1) & -align;
      const ElfW(Xword) size =
          desc_start + ((nhdr->n_descsz + align - 1) & -align);

      // Beware of wrap-around.
      if (nhdr->n_namesz >= static_cast<ElfW(Word)>(-align) ||
          nhdr->n_descsz >= static_cast<ElfW(Word)>(-align) ||
          desc_start < sizeof(*nhdr) || size < desc_start ||
          size > static_cast<ElfW(Xword)>(last - note)) {
        // Corrupt PT_NOTE
        break;
      }

      if (nhdr->n_type == NT_GNU_BUILD_ID) {
        const char* const note_name = note + sizeof(*nhdr);
        // n_namesz is the length of note_name.
        if (nhdr->n_namesz == 4 && memcmp(note_name, "GNU\0", 4) == 0) {
          if (!result.empty()) {
            // Repeated build-ids.  Ignore them.
            return "";
          }
          result = absl::BytesToHexString(
              absl::string_view(note + desc_start, nhdr->n_descsz));
        }
      }
      note += size;
    }
  }

  return result;
}
#endif  // __linux__

// A varint-encoded integer.
struct Varint {
  explicit Varint(uint64_t v) : value(v) {}
  explicit Varint(StringId v) : value(static_cast<uint64_t>(v)) {}
  explicit Varint(LocationId v) : value(static_cast<uint64_t>(v)) {}
  explicit Varint(MappingId v) : value(static_cast<uint64_t>(v)) {}

  uint64_t value;

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const Varint& v) {
    char buf[10];
    char* p = buf;
    uint64_t u = v.value;
    while (u >= 0x80) {
      *p++ = static_cast<char>((u & 0x7f) | 0x80);
      u >>= 7;
    }
    *p++ = static_cast<char>(u);
    sink.Append(absl::string_view(buf, static_cast<size_t>(p - buf)));
  }
};

struct Tag {
  int field_number;
  WireType wire_type;

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const Tag& t) {
    absl::Format(&sink, "%v",
                 Varint((static_cast<uint64_t>(t.field_number) << 3) |
                        static_cast<uint64_t>(t.wire_type)));
  }
};

struct LengthDelimited {
  int field_number;
  absl::string_view value;

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const LengthDelimited& ld) {
    absl::Format(&sink, "%v%v%v",
                 Tag{ld.field_number, WireType::kLengthDelimited},
                 Varint(ld.value.size()), ld.value);
  }
};

struct VarintField {
  int field_number;
  Varint value;

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const VarintField& vf) {
    absl::Format(&sink, "%v%v", Tag{vf.field_number, WireType::kVarint},
                 vf.value);
  }
};

}  // namespace

StringId ProfileBuilder::InternString(absl::string_view str) {
  if (str.empty()) return StringId(0);
  return string_table_.emplace(str, StringId(string_table_.size()))
      .first->second;
}

LocationId ProfileBuilder::InternLocation(const void* address) {
  return location_table_
      .emplace(absl::bit_cast<uintptr_t>(address),
               LocationId(location_table_.size() + 1))
      .first->second;
}

void ProfileBuilder::AddSample(
    int64_t value, absl::Span<const void* const> stack,
    absl::Span<const std::pair<StringId, int64_t>> labels) {
  std::string sample_proto;
  absl::StrAppend(
      &sample_proto,
      VarintField{SampleProto::kValue, Varint(static_cast<uint64_t>(value))});

  for (const void* addr : stack) {
    // Profile addresses are raw stack unwind addresses, so they should be
    // adjusted by -1 to land inside the call instruction (although potentially
    // misaligned).
    absl::StrAppend(
        &sample_proto,
        VarintField{SampleProto::kLocationId,
                    Varint(InternLocation(absl::bit_cast<const void*>(
                        absl::bit_cast<uintptr_t>(addr) - 1)))});
  }

  for (const auto& label : labels) {
    std::string label_proto =
        absl::StrCat(VarintField{LabelProto::kKey, Varint(label.first)},
                     VarintField{LabelProto::kNum,
                                 Varint(static_cast<uint64_t>(label.second))});
    absl::StrAppend(&sample_proto,
                    LengthDelimited{SampleProto::kLabel, label_proto});
  }
  samples_.push_back(std::move(sample_proto));
}

void ProfileBuilder::AddSampleType(StringId type, StringId unit) {
  std::string sample_type_proto =
      absl::StrCat(VarintField{ValueTypeProto::kType, Varint(type)},
                   VarintField{ValueTypeProto::kUnit, Varint(unit)});
  sample_types_.push_back(std::move(sample_type_proto));
}

MappingId ProfileBuilder::AddMapping(uintptr_t memory_start,
                                     uintptr_t memory_limit,
                                     uintptr_t file_offset,
                                     absl::string_view filename,
                                     absl::string_view build_id) {
  size_t index = mappings_.size() + 1;
  auto [it, inserted] = mapping_table_.emplace(memory_start, index);
  if (!inserted) {
    return static_cast<MappingId>(it->second);
  }

  Mapping m;
  m.start = memory_start;
  m.limit = memory_limit;
  m.offset = file_offset;
  m.filename = std::string(filename);
  m.build_id = std::string(build_id);

  mappings_.push_back(std::move(m));
  return static_cast<MappingId>(index);
}

std::string ProfileBuilder::Emit() && {
  std::string profile_proto;
  for (const auto& sample_type : sample_types_) {
    absl::StrAppend(&profile_proto,
                    LengthDelimited{ProfileProto::kSampleType, sample_type});
  }
  for (const auto& sample : samples_) {
    absl::StrAppend(&profile_proto,
                    LengthDelimited{ProfileProto::kSample, sample});
  }

  // Build mapping table.
  for (size_t i = 0, n = mappings_.size(); i < n; ++i) {
    const auto& mapping = mappings_[i];
    std::string mapping_proto = absl::StrCat(
        VarintField{MappingProto::kId, Varint(static_cast<uint64_t>(i + 1))},
        VarintField{MappingProto::kMemoryStart, Varint(mapping.start)},
        VarintField{MappingProto::kMemoryLimit, Varint(mapping.limit)},
        VarintField{MappingProto::kFileOffset, Varint(mapping.offset)},
        VarintField{MappingProto::kFilename,
                    Varint(InternString(mapping.filename))},
        VarintField{MappingProto::kBuildId,
                    Varint(InternString(mapping.build_id))});

    absl::StrAppend(&profile_proto,
                    LengthDelimited{ProfileProto::kMapping, mapping_proto});
  }

  // Build location table.
  for (const auto& [address, id] : location_table_) {
    std::string location =
        absl::StrCat(VarintField{LocationProto::kId, Varint(id)},
                     VarintField{LocationProto::kAddress, Varint(address)});

    if (!mappings_.empty()) {
      // Find the mapping ID.
      auto it = mapping_table_.upper_bound(address);
      if (it != mapping_table_.begin()) {
        --it;
      }

      // If *it contains address, add mapping to location.
      const size_t mapping_index = it->second;
      const Mapping& mapping = mappings_[mapping_index - 1];

      if (it->first <= address && address < mapping.limit) {
        absl::StrAppend(
            &location,
            VarintField{LocationProto::kMappingId,
                        Varint(static_cast<uint64_t>(mapping_index))});
      }
    }

    absl::StrAppend(&profile_proto,
                    LengthDelimited{ProfileProto::kLocation, location});
  }

  std::string string_table_proto;
  std::vector<absl::string_view> sorted_strings(string_table_.size());
  for (const auto& p : string_table_) {
    sorted_strings[static_cast<size_t>(p.second)] = p.first;
  }
  for (const auto& s : sorted_strings) {
    absl::StrAppend(&string_table_proto,
                    LengthDelimited{ProfileProto::kStringTable, s});
  }
  absl::StrAppend(&profile_proto, VarintField{ProfileProto::kDropFrames,
                                              Varint(drop_frames_id_)});
  absl::StrAppend(&profile_proto,
                  VarintField{ProfileProto::kComment, Varint(comment_id_)});
  absl::StrAppend(&profile_proto, VarintField{ProfileProto::kDefaultSampleType,
                                              Varint(default_sample_type_id_)});
  return absl::StrCat(string_table_proto, profile_proto);
}

void ProfileBuilder::set_drop_frames_id(StringId drop_frames_id) {
  drop_frames_id_ = drop_frames_id;
}

void ProfileBuilder::set_comment_id(StringId comment_id) {
  comment_id_ = comment_id;
}

void ProfileBuilder::set_default_sample_type_id(
    StringId default_sample_type_id) {
  default_sample_type_id_ = default_sample_type_id;
}

void ProfileBuilder::AddCurrentMappings() {
#ifdef __linux__
  dl_iterate_phdr(
      +[](dl_phdr_info* info, size_t, void* data) {
        auto& builder = *reinterpret_cast<ProfileBuilder*>(data);

        // Skip dummy entry introduced since glibc 2.18.
        if (info->dlpi_phdr == nullptr && info->dlpi_phnum == 0) {
          return 0;
        }

        const bool is_main_executable = builder.mappings_.empty();

        // Evaluate all the loadable segments.
        for (int i = 0; i < info->dlpi_phnum; ++i) {
          if (info->dlpi_phdr[i].p_type != PT_LOAD) {
            continue;
          }
          const ElfW(Phdr)* pt_load = &info->dlpi_phdr[i];

          ABSL_RAW_CHECK(pt_load != nullptr, "Unexpected nullptr");

          // Extract data.
          const size_t memory_start = info->dlpi_addr + pt_load->p_vaddr;
          const size_t memory_limit = memory_start + pt_load->p_memsz;
          const size_t file_offset = pt_load->p_offset;

          // Storage for path to executable as dlpi_name isn't populated for the
          // main executable.  +1 to allow for the null terminator that readlink
          // does not add.
          char self_filename[PATH_MAX + 1];
          const char* filename = info->dlpi_name;
          if (filename == nullptr || filename[0] == '\0') {
            // This is either the main executable or the VDSO.  The main
            // executable is always the first entry processed by callbacks.
            if (is_main_executable) {
              // This is the main executable.
              ssize_t ret = readlink("/proc/self/exe", self_filename,
                                     sizeof(self_filename) - 1);
              if (ret >= 0 &&
                  static_cast<size_t>(ret) < sizeof(self_filename)) {
                self_filename[ret] = '\0';
                filename = self_filename;
              }
            } else {
              // This is the VDSO.
              filename = GetSoName(info);
            }
          }

          char resolved_path[PATH_MAX];
          absl::string_view resolved_filename;
          if (realpath(filename, resolved_path)) {
            resolved_filename = resolved_path;
          } else {
            resolved_filename = filename;
          }

          const std::string build_id = GetBuildId(info);

          // Add to profile.
          builder.AddMapping(memory_start, memory_limit, file_offset,
                             resolved_filename, build_id);
        }
        // Keep going.
        return 0;
      },
      this);
#endif  // __linux__
}

}  // namespace debugging_internal
ABSL_NAMESPACE_END
}  // namespace absl
