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

#ifndef ABSL_PROFILING_INTERNAL_PROFILE_BUILDER_H_
#define ABSL_PROFILING_INTERNAL_PROFILE_BUILDER_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace debugging_internal {

// Field numbers for perftools.profiles.Profile.
// https://github.com/google/pprof/blob/master/proto/profile.proto
struct ProfileProto {
  static constexpr int kSampleType = 1;
  static constexpr int kSample = 2;
  static constexpr int kMapping = 3;
  static constexpr int kLocation = 4;
  static constexpr int kStringTable = 6;
  static constexpr int kDropFrames = 7;
  static constexpr int kComment = 13;
  static constexpr int kDefaultSampleType = 14;
};

struct ValueTypeProto {
  static constexpr int kType = 1;
  static constexpr int kUnit = 2;
};

struct SampleProto {
  static constexpr int kLocationId = 1;
  static constexpr int kValue = 2;
  static constexpr int kLabel = 3;
};

struct LabelProto {
  static constexpr int kKey = 1;
  static constexpr int kStr = 2;
  static constexpr int kNum = 3;
  static constexpr int kNumUnit = 4;
};

struct MappingProto {
  static constexpr int kId = 1;
  static constexpr int kMemoryStart = 2;
  static constexpr int kMemoryLimit = 3;
  static constexpr int kFileOffset = 4;
  static constexpr int kFilename = 5;
  static constexpr int kBuildId = 6;
};

struct LocationProto {
  static constexpr int kId = 1;
  static constexpr int kMappingId = 2;
  static constexpr int kAddress = 3;
};

enum class StringId : size_t {};
enum class LocationId : size_t {};
enum class MappingId : size_t {};

// A helper class to build a profile protocol buffer.
class ProfileBuilder {
 public:
  struct Mapping {
    uint64_t start;
    uint64_t limit;
    uint64_t offset;
    std::string filename;
    std::string build_id;
  };

  StringId InternString(absl::string_view str);

  LocationId InternLocation(const void* address);

  void AddSample(int64_t value, absl::Span<const void* const> stack,
                 absl::Span<const std::pair<StringId, int64_t>> labels);

  void AddSampleType(StringId type, StringId unit);

  // Adds the current process mappings to the profile.
  void AddCurrentMappings();

  // Adds a single mapping to the profile and to lookup cache and returns the
  // resulting ID.
  MappingId AddMapping(uintptr_t memory_start, uintptr_t memory_limit,
                       uintptr_t file_offset, absl::string_view filename,
                       absl::string_view build_id);

  std::string Emit() &&;

  void set_drop_frames_id(StringId drop_frames_id);
  void set_comment_id(StringId comment_id);
  void set_default_sample_type_id(StringId default_sample_type_id);

 private:
  absl::flat_hash_map<std::string, StringId> string_table_{{"", StringId(0)}};
  absl::flat_hash_map<uintptr_t, LocationId> location_table_;
  // mapping_table_ stores the start address of each mapping in mapping_
  // to its index.
  absl::btree_map<uintptr_t, size_t> mapping_table_;
  std::vector<Mapping> mappings_;

  std::vector<std::string> sample_types_;
  std::vector<std::string> samples_;

  StringId drop_frames_id_{};
  StringId comment_id_{};
  StringId default_sample_type_id_{};
};

}  // namespace debugging_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_PROFILING_INTERNAL_PROFILE_BUILDER_H_
