/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "src/trace_processor/importers/proto/metadata_tracker.h"

#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/types/trace_processor_context.h"

namespace perfetto {
namespace trace_processor {

MetadataTracker::MetadataTracker(TraceProcessorContext* context)
    : context_(context) {
  for (uint32_t i = 0; i < kNumKeys; ++i) {
    key_ids_[i] = context->storage->InternString(metadata::kNames[i]);
  }
  for (uint32_t i = 0; i < kNumKeyTypes; ++i) {
    key_type_ids_[i] =
        context->storage->InternString(metadata::kKeyTypeNames[i]);
  }
}

MetadataId MetadataTracker::SetMetadata(metadata::KeyIDs key, Variadic value) {
  PERFETTO_DCHECK(metadata::kKeyTypes[key] == metadata::KeyType::kSingle);
  PERFETTO_DCHECK(value.type == metadata::kValueTypes[key]);

  auto* metadata_table = context_->storage->mutable_metadata_table();
  uint32_t key_idx = static_cast<uint32_t>(key);
  base::Optional<uint32_t> opt_row =
      metadata_table->name().IndexOf(metadata::kNames[key_idx]);
  if (opt_row) {
    WriteValue(*opt_row, value);
    return metadata_table->id()[*opt_row];
  }

  tables::MetadataTable::Row row;
  row.name = key_ids_[key_idx];
  row.key_type = key_type_ids_[static_cast<size_t>(metadata::KeyType::kSingle)];

  auto id_and_row = metadata_table->Insert(row);
  WriteValue(id_and_row.row, value);
  return id_and_row.id;
}

MetadataId MetadataTracker::AppendMetadata(metadata::KeyIDs key,
                                           Variadic value) {
  PERFETTO_DCHECK(key < metadata::kNumKeys);
  PERFETTO_DCHECK(metadata::kKeyTypes[key] == metadata::KeyType::kMulti);
  PERFETTO_DCHECK(value.type == metadata::kValueTypes[key]);

  uint32_t key_idx = static_cast<uint32_t>(key);
  tables::MetadataTable::Row row;
  row.name = key_ids_[key_idx];
  row.key_type = key_type_ids_[static_cast<size_t>(metadata::KeyType::kMulti)];

  auto* metadata_table = context_->storage->mutable_metadata_table();
  auto id_and_row = metadata_table->Insert(row);
  WriteValue(id_and_row.row, value);
  return id_and_row.id;
}

void MetadataTracker::WriteValue(uint32_t row, Variadic value) {
  auto* metadata_table = context_->storage->mutable_metadata_table();
  switch (value.type) {
    case Variadic::Type::kInt:
      metadata_table->mutable_int_value()->Set(row, value.int_value);
      break;
    case Variadic::Type::kString:
      metadata_table->mutable_str_value()->Set(row, value.string_value);
      break;
    case Variadic::Type::kJson:
    case Variadic::Type::kBool:
    case Variadic::Type::kPointer:
    case Variadic::Type::kUint:
    case Variadic::Type::kReal:
      PERFETTO_FATAL("Unsupported value type");
  }
}

}  // namespace trace_processor
}  // namespace perfetto
