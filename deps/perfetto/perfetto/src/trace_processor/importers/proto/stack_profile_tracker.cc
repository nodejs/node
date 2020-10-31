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

#include "src/trace_processor/importers/proto/stack_profile_tracker.h"

#include "src/trace_processor/types/trace_processor_context.h"

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/string_utils.h"

namespace perfetto {
namespace trace_processor {

StackProfileTracker::InternLookup::~InternLookup() = default;

StackProfileTracker::StackProfileTracker(TraceProcessorContext* context)
    : context_(context), empty_(kNullStringId) {}

StackProfileTracker::~StackProfileTracker() = default;

StringId StackProfileTracker::GetEmptyStringId() {
  if (empty_ == kNullStringId) {
    empty_ = context_->storage->InternString({"", 0});
  }

  return empty_;
}

void StackProfileTracker::AddString(SourceStringId id, base::StringView str) {
  string_map_.emplace(id, str.ToStdString());
}

base::Optional<MappingId> StackProfileTracker::AddMapping(
    SourceMappingId id,
    const SourceMapping& mapping,
    const InternLookup* intern_lookup) {
  std::string path;
  for (SourceStringId str_id : mapping.name_ids) {
    auto opt_str = FindOrInsertString(str_id, intern_lookup,
                                      InternedStringType::kMappingPath);
    if (!opt_str)
      break;
    path += "/" + *opt_str;
  }

  auto opt_build_id = FindAndInternString(mapping.build_id, intern_lookup,
                                          InternedStringType::kBuildId);
  if (!opt_build_id) {
    context_->storage->IncrementStats(stats::stackprofile_invalid_string_id);
    PERFETTO_DLOG("Invalid string.");
    return base::nullopt;
  }
  const StringId raw_build_id = opt_build_id.value();
  NullTermStringView raw_build_id_str =
      context_->storage->GetString(raw_build_id);
  StringId build_id = GetEmptyStringId();
  if (raw_build_id_str.size() > 0) {
    // If the build_id is 33 characters long, we assume it's a Breakpad debug
    // identifier which is already in Hex and doesn't need conversion.
    // TODO(b/148109467): Remove workaround once all active Chrome versions
    // write raw bytes instead of a string as build_id.
    if (raw_build_id_str.size() == 33) {
      build_id = raw_build_id;
    } else {
      std::string hex_build_id =
          base::ToHex(raw_build_id_str.c_str(), raw_build_id_str.size());
      build_id =
          context_->storage->InternString(base::StringView(hex_build_id));
    }
  }

  tables::StackProfileMappingTable::Row row{
      build_id,
      static_cast<int64_t>(mapping.exact_offset),
      static_cast<int64_t>(mapping.start_offset),
      static_cast<int64_t>(mapping.start),
      static_cast<int64_t>(mapping.end),
      static_cast<int64_t>(mapping.load_bias),
      context_->storage->InternString(base::StringView(path))};

  tables::StackProfileMappingTable* mappings =
      context_->storage->mutable_stack_profile_mapping_table();
  base::Optional<MappingId> cur_id;
  auto it = mapping_idx_.find(row);
  if (it != mapping_idx_.end()) {
    cur_id = it->second;
  } else {
    std::vector<MappingId> db_mappings =
        context_->storage->FindMappingRow(row.name, row.build_id);
    for (const MappingId preexisting_mapping : db_mappings) {
      uint32_t preexisting_row = *mappings->id().IndexOf(preexisting_mapping);
      tables::StackProfileMappingTable::Row preexisting_data{
          mappings->build_id()[preexisting_row],
          mappings->exact_offset()[preexisting_row],
          mappings->start_offset()[preexisting_row],
          mappings->start()[preexisting_row],
          mappings->end()[preexisting_row],
          mappings->load_bias()[preexisting_row],
          mappings->name()[preexisting_row]};

      if (row == preexisting_data) {
        cur_id = preexisting_mapping;
      }
    }
    if (!cur_id) {
      MappingId mapping_id = mappings->Insert(row).id;
      context_->storage->InsertMappingId(row.name, row.build_id, mapping_id);
      cur_id = mapping_id;
    }
    mapping_idx_.emplace(row, *cur_id);
  }
  mapping_ids_.emplace(id, *cur_id);
  return cur_id;
}

base::Optional<FrameId> StackProfileTracker::AddFrame(
    SourceFrameId id,
    const SourceFrame& frame,
    const InternLookup* intern_lookup) {
  auto opt_str_id = FindAndInternString(frame.name_id, intern_lookup,
                                        InternedStringType::kFunctionName);
  if (!opt_str_id) {
    context_->storage->IncrementStats(stats::stackprofile_invalid_string_id);
    PERFETTO_DLOG("Invalid string.");
    return base::nullopt;
  }
  const StringId& str_id = opt_str_id.value();

  auto opt_mapping = FindOrInsertMapping(frame.mapping_id, intern_lookup);
  if (!opt_mapping) {
    context_->storage->IncrementStats(stats::stackprofile_invalid_mapping_id);
    PERFETTO_ELOG("Invalid mapping for frame %" PRIu64, id);
    return base::nullopt;
  }
  MappingId mapping_id = *opt_mapping;

  tables::StackProfileFrameTable::Row row{str_id, mapping_id,
                                          static_cast<int64_t>(frame.rel_pc)};

  auto* frames = context_->storage->mutable_stack_profile_frame_table();

  base::Optional<FrameId> cur_id;
  auto it = frame_idx_.find(row);
  if (it != frame_idx_.end()) {
    cur_id = it->second;
  } else {
    std::vector<FrameId> db_frames =
        context_->storage->FindFrameIds(mapping_id, frame.rel_pc);
    for (const FrameId preexisting_frame : db_frames) {
      uint32_t preexisting_row_id = preexisting_frame.value;
      tables::StackProfileFrameTable::Row preexisting_row{
          frames->name()[preexisting_row_id],
          frames->mapping()[preexisting_row_id],
          frames->rel_pc()[preexisting_row_id]};

      if (row == preexisting_row) {
        cur_id = preexisting_frame;
      }
    }
    if (!cur_id) {
      cur_id = frames->Insert(row).id;
      context_->storage->InsertFrameRow(
          mapping_id, static_cast<uint64_t>(row.rel_pc), *cur_id);
    }
    frame_idx_.emplace(row, *cur_id);
  }
  frame_ids_.emplace(id, *cur_id);
  return cur_id;
}

base::Optional<CallsiteId> StackProfileTracker::AddCallstack(
    SourceCallstackId id,
    const SourceCallstack& frame_ids,
    const InternLookup* intern_lookup) {
  if (frame_ids.size() == 0)
    return base::nullopt;

  base::Optional<CallsiteId> parent_id;
  for (uint32_t depth = 0; depth < frame_ids.size(); ++depth) {
    auto opt_frame_id = FindOrInsertFrame(frame_ids[depth], intern_lookup);
    if (!opt_frame_id) {
      context_->storage->IncrementStats(stats::stackprofile_invalid_frame_id);
      PERFETTO_ELOG("Unknown frame in callstack; ignoring.");
      return base::nullopt;
    }
    FrameId frame_id = *opt_frame_id;

    tables::StackProfileCallsiteTable::Row row{depth, parent_id, frame_id};
    CallsiteId self_id;
    auto callsite_it = callsite_idx_.find(row);
    if (callsite_it != callsite_idx_.end()) {
      self_id = callsite_it->second;
    } else {
      auto* callsite =
          context_->storage->mutable_stack_profile_callsite_table();
      self_id = callsite->Insert(row).id;
      callsite_idx_.emplace(row, self_id);
    }
    parent_id = self_id;
  }
  PERFETTO_DCHECK(parent_id);  // The loop ran at least once.
  callstack_ids_.emplace(id, *parent_id);
  return parent_id;
}

FrameId StackProfileTracker::GetDatabaseFrameIdForTesting(
    SourceFrameId frame_id) {
  auto it = frame_ids_.find(frame_id);
  if (it == frame_ids_.end()) {
    PERFETTO_DLOG("Invalid frame.");
    return {};
  }
  return it->second;
}

base::Optional<StringId> StackProfileTracker::FindAndInternString(
    SourceStringId id,
    const InternLookup* intern_lookup,
    StackProfileTracker::InternedStringType type) {
  if (id == 0)
    return GetEmptyStringId();

  auto opt_str = FindOrInsertString(id, intern_lookup, type);
  if (!opt_str)
    return GetEmptyStringId();

  return context_->storage->InternString(base::StringView(*opt_str));
}

base::Optional<std::string> StackProfileTracker::FindOrInsertString(
    SourceStringId id,
    const InternLookup* intern_lookup,
    StackProfileTracker::InternedStringType type) {
  if (id == 0)
    return "";

  auto it = string_map_.find(id);
  if (it == string_map_.end()) {
    if (intern_lookup) {
      auto str = intern_lookup->GetString(id, type);
      if (!str) {
        context_->storage->IncrementStats(
            stats::stackprofile_invalid_string_id);
        PERFETTO_DLOG("Invalid string.");
        return base::nullopt;
      }
      return str->ToStdString();
    }
    return base::nullopt;
  }

  return it->second;
}

base::Optional<MappingId> StackProfileTracker::FindOrInsertMapping(
    SourceMappingId mapping_id,
    const InternLookup* intern_lookup) {
  base::Optional<MappingId> res;
  auto it = mapping_ids_.find(mapping_id);
  if (it == mapping_ids_.end()) {
    if (intern_lookup) {
      auto interned_mapping = intern_lookup->GetMapping(mapping_id);
      if (interned_mapping) {
        res = AddMapping(mapping_id, *interned_mapping, intern_lookup);
        return res;
      }
    }
    context_->storage->IncrementStats(stats::stackprofile_invalid_mapping_id);
    PERFETTO_ELOG("Unknown mapping %" PRIu64 " : %zu", mapping_id,
                  mapping_ids_.size());
    return res;
  }
  res = it->second;
  return res;
}

base::Optional<FrameId> StackProfileTracker::FindOrInsertFrame(
    SourceFrameId frame_id,
    const InternLookup* intern_lookup) {
  base::Optional<FrameId> res;
  auto it = frame_ids_.find(frame_id);
  if (it == frame_ids_.end()) {
    if (intern_lookup) {
      auto interned_frame = intern_lookup->GetFrame(frame_id);
      if (interned_frame) {
        res = AddFrame(frame_id, *interned_frame, intern_lookup);
        return res;
      }
    }
    context_->storage->IncrementStats(stats::stackprofile_invalid_frame_id);
    PERFETTO_DLOG("Unknown frame %" PRIu64 " : %zu", frame_id,
                  frame_ids_.size());
    return res;
  }
  res = it->second;
  return res;
}

base::Optional<CallsiteId> StackProfileTracker::FindOrInsertCallstack(
    SourceCallstackId callstack_id,
    const InternLookup* intern_lookup) {
  base::Optional<CallsiteId> res;
  auto it = callstack_ids_.find(callstack_id);
  if (it == callstack_ids_.end()) {
    auto interned_callstack = intern_lookup->GetCallstack(callstack_id);
    if (interned_callstack) {
      res = AddCallstack(callstack_id, *interned_callstack, intern_lookup);
      return res;
    }
    context_->storage->IncrementStats(stats::stackprofile_invalid_callstack_id);
    PERFETTO_DLOG("Unknown callstack %" PRIu64 " : %zu", callstack_id,
                  callstack_ids_.size());
    return res;
  }
  res = it->second;
  return res;
}

void StackProfileTracker::ClearIndices() {
  string_map_.clear();
  mapping_ids_.clear();
  callstack_ids_.clear();
  frame_ids_.clear();
}

}  // namespace trace_processor
}  // namespace perfetto
