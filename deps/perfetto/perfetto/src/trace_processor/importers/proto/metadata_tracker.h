/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_METADATA_TRACKER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_METADATA_TRACKER_H_

#include "src/trace_processor/storage/trace_storage.h"

namespace perfetto {
namespace trace_processor {

class TraceProcessorContext;

// Tracks information in the metadata table.
class MetadataTracker {
 public:
  MetadataTracker(TraceProcessorContext* context);

  // Example usage:
  // SetMetadata(metadata::benchmark_name,
  //             Variadic::String(storage->InternString("foo"));
  // Returns the id of the new entry.
  MetadataId SetMetadata(metadata::KeyIDs key, Variadic value);

  // Example usage:
  // AppendMetadata(metadata::benchmark_story_tags,
  //                Variadic::String(storage->InternString("bar"));
  // Returns the id of the new entry.
  MetadataId AppendMetadata(metadata::KeyIDs key, Variadic value);

 private:
  static constexpr size_t kNumKeys =
      static_cast<size_t>(metadata::KeyIDs::kNumKeys);
  static constexpr size_t kNumKeyTypes =
      static_cast<size_t>(metadata::KeyType::kNumKeyTypes);

  void WriteValue(uint32_t row, Variadic value);

  std::array<StringId, kNumKeys> key_ids_;
  std::array<StringId, kNumKeyTypes> key_type_ids_;

  TraceProcessorContext* context_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_METADATA_TRACKER_H_
