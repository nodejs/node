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

#include "absl/profiling/hashtable.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/config.h"
#include "absl/container/internal/hashtablez_sampler.h"
#include "absl/profiling/internal/profile_builder.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

StatusOr<std::string> MarshalHashtableProfile() {
  return debugging_internal::MarshalHashtableProfile(
      container_internal::GlobalHashtablezSampler(), Now());
}

namespace debugging_internal {

StatusOr<std::string> MarshalHashtableProfile(
    container_internal::HashtablezSampler& sampler, Time now) {
  static constexpr absl::string_view kDropFrames =
      "(::)?absl::container_internal::.*|"
      "(::)?absl::(flat|node)_hash_(map|set).*";

  ProfileBuilder builder;
  StringId drop_frames_id = builder.InternString(kDropFrames);
  builder.set_drop_frames_id(drop_frames_id);
  builder.AddSampleType(builder.InternString("capacity"),
                        builder.InternString("count"));
  builder.set_default_sample_type_id(builder.InternString("capacity"));

  const auto capacity_id = builder.InternString("capacity");
  const auto size_id = builder.InternString("size");
  const auto num_erases_id = builder.InternString("num_erases");
  const auto num_rehashes_id = builder.InternString("num_rehashes");
  const auto max_probe_length_id = builder.InternString("max_probe_length");
  const auto total_probe_length_id = builder.InternString("total_probe_length");
  const auto stuck_bits_id = builder.InternString("stuck_bits");
  const auto inline_element_size_id =
      builder.InternString("inline_element_size");
  const auto key_size_id = builder.InternString("key_size");
  const auto value_size_id = builder.InternString("value_size");
  const auto soo_capacity_id = builder.InternString("soo_capacity");
  const auto checksum_id = builder.InternString("checksum");
  const auto table_age_id = builder.InternString("table_age");
  const auto max_reserve_id = builder.InternString("max_reserve");

  size_t dropped =
      sampler.Iterate([&](const container_internal::HashtablezInfo& info) {
        const size_t capacity = info.capacity.load(std::memory_order_relaxed);
        std::vector<std::pair<StringId, int64_t>> labels;

        auto add_label = [&](StringId tag, uint64_t value) {
          if (value == 0) {
            return;
          }
          labels.emplace_back(tag, static_cast<int64_t>(value));
        };

        add_label(capacity_id, capacity);
        add_label(size_id, info.size.load(std::memory_order_relaxed));
        add_label(num_erases_id,
                  info.num_erases.load(std::memory_order_relaxed));
        add_label(num_rehashes_id,
                  info.num_rehashes.load(std::memory_order_relaxed));
        add_label(max_probe_length_id,
                  info.max_probe_length.load(std::memory_order_relaxed));
        add_label(total_probe_length_id,
                  info.total_probe_length.load(std::memory_order_relaxed));
        add_label(stuck_bits_id,
                  (info.hashes_bitwise_and.load(std::memory_order_relaxed) |
                   ~info.hashes_bitwise_or.load(std::memory_order_relaxed)));
        add_label(inline_element_size_id, info.inline_element_size);
        add_label(key_size_id, info.key_size);
        add_label(value_size_id, info.value_size);
        add_label(soo_capacity_id, info.soo_capacity);
        add_label(checksum_id,
                  info.hashes_bitwise_xor.load(std::memory_order_relaxed));
        add_label(
            table_age_id,
            static_cast<uint64_t>(ToInt64Microseconds(now - info.create_time)));
        add_label(max_reserve_id,
                  info.max_reserve.load(std::memory_order_relaxed));
        builder.AddSample(static_cast<int64_t>(capacity) * info.weight,
                          MakeSpan(info.stack, info.depth), labels);
      });

  // TODO(b/262310142): Make this more structured data.
  StringId comment_id =
      builder.InternString(StrCat("dropped_samples: ", dropped));
  builder.set_comment_id(comment_id);
  builder.AddCurrentMappings();
  return std::move(builder).Emit();
}

}  // namespace debugging_internal
ABSL_NAMESPACE_END
}  // namespace absl
