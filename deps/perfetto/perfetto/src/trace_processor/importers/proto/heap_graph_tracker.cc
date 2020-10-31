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

#include "src/trace_processor/importers/proto/heap_graph_tracker.h"

#include "perfetto/ext/base/string_splitter.h"
#include "perfetto/ext/base/string_utils.h"

namespace perfetto {
namespace trace_processor {

namespace {
base::Optional<base::StringView> PackageFromApp(base::StringView location) {
  location = location.substr(base::StringView("/data/app/").size());
  size_t slash = location.find('/');
  if (slash == std::string::npos) {
    return base::nullopt;
  }
  size_t second_slash = location.find('/', slash + 1);
  if (second_slash == std::string::npos) {
    return base::nullopt;
  }
  location = location.substr(slash + 1, second_slash - slash);
  size_t minus = location.find('-');
  if (minus == std::string::npos) {
    return base::nullopt;
  }
  return location.substr(0, minus);
}
}  // namespace

base::Optional<base::StringView> GetStaticClassTypeName(base::StringView type) {
  static const base::StringView kJavaClassTemplate("java.lang.Class<");
  if (!type.empty() && type.at(type.size() - 1) == '>' &&
      type.substr(0, kJavaClassTemplate.size()) == kJavaClassTemplate) {
    return type.substr(kJavaClassTemplate.size(),
                       type.size() - kJavaClassTemplate.size() - 1);
  }
  return {};
}

size_t NumberOfArrays(base::StringView type) {
  if (type.size() < 2)
    return 0;

  size_t arrays = 0;
  while (type.size() >= 2 * (arrays + 1) &&
         memcmp(type.end() - 2 * (arrays + 1), "[]", 2) == 0) {
    arrays++;
  }

  return arrays;
}

NormalizedType GetNormalizedType(base::StringView type) {
  auto static_class_type_name = GetStaticClassTypeName(type);
  if (static_class_type_name.has_value()) {
    type = static_class_type_name.value();
  }
  size_t number_of_arrays = NumberOfArrays(type);
  return {base::StringView(type.data(), type.size() - number_of_arrays * 2),
          static_class_type_name.has_value(), number_of_arrays};
}

base::StringView NormalizeTypeName(base::StringView type) {
  return GetNormalizedType(type).name;
}

std::string DenormalizeTypeName(NormalizedType normalized,
                                base::StringView deobfuscated_type_name) {
  std::string result = deobfuscated_type_name.ToStdString();
  for (size_t i = 0; i < normalized.number_of_arrays; ++i) {
    result += "[]";
  }
  if (normalized.is_static_class) {
    result = "java.lang.Class<" + result + ">";
  }
  return result;
}

HeapGraphTracker::HeapGraphTracker(TraceProcessorContext* context)
    : context_(context) {}

base::Optional<std::string> HeapGraphTracker::PackageFromLocation(
    base::StringView location) {
  // List of some hardcoded apps that do not follow the scheme used in
  // PackageFromApp. Ask for yours to be added.
  //
  // TODO(b/153632336): Get rid of the hardcoded list of system apps.
  base::StringView sysui(
      "/system_ext/priv-app/SystemUIGoogle/SystemUIGoogle.apk");
  if (location.size() >= sysui.size() &&
      location.substr(0, sysui.size()) == sysui) {
    return "com.android.systemui";
  }

  base::StringView phonesky("/product/priv-app/Phonesky/Phonesky.apk");
  if (location.size() >= phonesky.size() &&
      location.substr(0, phonesky.size()) == phonesky) {
    return "com.android.vending";
  }

  base::StringView maps("/product/app/Maps/Maps.apk");
  if (location.size() >= maps.size() &&
      location.substr(0, maps.size()) == maps) {
    return "com.google.android.apps.maps";
  }

  base::StringView launcher(
      "/system_ext/priv-app/NexusLauncherRelease/NexusLauncherRelease.apk");
  if (location.size() >= launcher.size() &&
      location.substr(0, launcher.size()) == launcher) {
    return "com.google.android.apps.nexuslauncher";
  }

  base::StringView photos("/product/app/Photos/Photos.apk");
  if (location.size() >= photos.size() &&
      location.substr(0, photos.size()) == photos) {
    return "com.google.android.apps.photos";
  }

  base::StringView wellbeing(
      "/product/priv-app/WellbeingPrebuilt/WellbeingPrebuilt.apk");
  if (location.size() >= wellbeing.size() &&
      location.substr(0, wellbeing.size()) == wellbeing) {
    return "com.google.android.apps.wellbeing";
  }

  base::StringView matchmaker("MatchMaker");
  if (location.size() >= matchmaker.size() &&
      location.find(matchmaker) != base::StringView::npos) {
    return "com.google.android.as";
  }

  base::StringView gm("/product/app/PrebuiltGmail/PrebuiltGmail.apk");
  if (location.size() >= gm.size() && location.substr(0, gm.size()) == gm) {
    return "com.google.android.gm";
  }

  base::StringView gmscore("/product/priv-app/PrebuiltGmsCore/PrebuiltGmsCore");
  if (location.size() >= gmscore.size() &&
      location.substr(0, gmscore.size()) == gmscore) {
    return "com.google.android.gms";
  }

  base::StringView velvet("/product/priv-app/Velvet/Velvet.apk");
  if (location.size() >= velvet.size() &&
      location.substr(0, velvet.size()) == velvet) {
    return "com.google.android.googlequicksearchbox";
  }

  base::StringView inputmethod(
      "/product/app/LatinIMEGooglePrebuilt/LatinIMEGooglePrebuilt.apk");
  if (location.size() >= inputmethod.size() &&
      location.substr(0, inputmethod.size()) == inputmethod) {
    return "com.google.android.inputmethod.latin";
  }

  base::StringView data_app("/data/app/");
  if (location.substr(0, data_app.size()) == data_app) {
    auto package = PackageFromApp(location);
    if (!package) {
      context_->storage->IncrementStats(stats::heap_graph_location_parse_error);
      return base::nullopt;
    }
    return package->ToStdString();
  }
  return base::nullopt;
}

HeapGraphTracker::SequenceState& HeapGraphTracker::GetOrCreateSequence(
    uint32_t seq_id) {
  auto seq_it = sequence_state_.find(seq_id);
  if (seq_it == sequence_state_.end()) {
    std::tie(seq_it, std::ignore) = sequence_state_.emplace(seq_id, this);
  }
  return seq_it->second;
}

bool HeapGraphTracker::SetPidAndTimestamp(SequenceState* sequence_state,
                                          UniquePid upid,
                                          int64_t ts) {
  if (sequence_state->current_upid != 0 &&
      sequence_state->current_upid != upid) {
    context_->storage->IncrementStats(stats::heap_graph_non_finalized_graph);
    return false;
  }
  if (sequence_state->current_ts != 0 && sequence_state->current_ts != ts) {
    context_->storage->IncrementStats(stats::heap_graph_non_finalized_graph);
    return false;
  }
  sequence_state->current_upid = upid;
  sequence_state->current_ts = ts;
  return true;
}

void HeapGraphTracker::AddObject(uint32_t seq_id,
                                 UniquePid upid,
                                 int64_t ts,
                                 SourceObject obj) {
  SequenceState& sequence_state = GetOrCreateSequence(seq_id);

  if (!SetPidAndTimestamp(&sequence_state, upid, ts))
    return;

  sequence_state.current_objects.emplace_back(std::move(obj));
}

void HeapGraphTracker::AddRoot(uint32_t seq_id,
                               UniquePid upid,
                               int64_t ts,
                               SourceRoot root) {
  SequenceState& sequence_state = GetOrCreateSequence(seq_id);
  if (!SetPidAndTimestamp(&sequence_state, upid, ts))
    return;

  sequence_state.current_roots.emplace_back(std::move(root));
}

void HeapGraphTracker::AddInternedLocationName(uint32_t seq_id,
                                               uint64_t intern_id,
                                               StringPool::Id strid) {
  SequenceState& sequence_state = GetOrCreateSequence(seq_id);
  sequence_state.interned_location_names.emplace(intern_id, strid);
}

void HeapGraphTracker::AddInternedTypeName(uint32_t seq_id,
                                           uint64_t intern_id,
                                           StringPool::Id strid) {
  SequenceState& sequence_state = GetOrCreateSequence(seq_id);
  sequence_state.interned_types[intern_id].name = strid;
}

void HeapGraphTracker::AddInternedType(uint32_t seq_id,
                                       uint64_t intern_id,
                                       StringPool::Id strid,
                                       uint64_t location_id) {
  SequenceState& sequence_state = GetOrCreateSequence(seq_id);
  sequence_state.interned_types[intern_id].name = strid;
  sequence_state.interned_types[intern_id].location_id = location_id;
}

void HeapGraphTracker::AddInternedFieldName(uint32_t seq_id,
                                            uint64_t intern_id,
                                            base::StringView str) {
  SequenceState& sequence_state = GetOrCreateSequence(seq_id);
  size_t space = str.find(' ');
  base::StringView type_name;
  if (space != base::StringView::npos) {
    type_name = str.substr(0, space);
    str = str.substr(space + 1);
  }
  sequence_state.interned_fields.emplace(
      intern_id, InternedField{context_->storage->InternString(str),
                               context_->storage->InternString(type_name)});
}

void HeapGraphTracker::SetPacketIndex(uint32_t seq_id, uint64_t index) {
  SequenceState& sequence_state = GetOrCreateSequence(seq_id);
  bool dropped_packet = false;
  // perfetto_hprof starts counting at index = 0.
  if (!sequence_state.prev_index && index != 0) {
    dropped_packet = true;
  }

  if (sequence_state.prev_index && *sequence_state.prev_index + 1 != index) {
    dropped_packet = true;
  }

  if (dropped_packet) {
    if (sequence_state.prev_index) {
      PERFETTO_ELOG("Missing packets between %" PRIu64 " and %" PRIu64,
                    *sequence_state.prev_index, index);
    } else {
      PERFETTO_ELOG("Invalid first packet index %" PRIu64 " (!= 0)", index);
    }

    context_->storage->IncrementIndexedStats(
        stats::heap_graph_missing_packet,
        static_cast<int>(sequence_state.current_upid));
  }
  sequence_state.prev_index = index;
}

void HeapGraphTracker::FinalizeProfile(uint32_t seq_id) {
  SequenceState& sequence_state = GetOrCreateSequence(seq_id);

  std::map<uint64_t, tables::HeapGraphClassTable::Id> type_id_to_db;
  for (const auto& p : sequence_state.interned_types) {
    uint64_t id = p.first;
    const InternedType& interned_type = p.second;
    base::Optional<StringPool::Id> location_name;
    if (interned_type.location_id) {
      auto it = sequence_state.interned_location_names.find(
          *interned_type.location_id);
      if (it == sequence_state.interned_location_names.end()) {
        context_->storage->IncrementIndexedStats(
            stats::heap_graph_invalid_string_id,
            static_cast<int>(sequence_state.current_upid));

      } else {
        location_name = it->second;
      }
    }
    auto id_and_row =
        context_->storage->mutable_heap_graph_class_table()->Insert(
            {interned_type.name, base::nullopt, location_name});

    type_id_to_db[id] = id_and_row.id;
    base::StringView normalized_type =
        NormalizeTypeName(context_->storage->GetString(interned_type.name));

    // Annoyingly, some apps have a relative path to base.apk. We take this to
    // mean the main package, so we treat it as if the location was unknown.
    bool is_base_apk = false;
    if (location_name) {
      base::StringView base_apk("base.apk");
      is_base_apk = context_->storage->GetString(*location_name)
                        .substr(0, base_apk.size()) == base_apk;
    }

    if (location_name && !is_base_apk) {
      base::Optional<std::string> package_name =
          PackageFromLocation(context_->storage->GetString(*location_name));
      if (package_name) {
        class_to_rows_[std::make_pair(
                           context_->storage->InternString(
                               base::StringView(*package_name)),
                           context_->storage->InternString(normalized_type))]
            .emplace_back(id_and_row.id);
      }
    } else {
      // TODO(b/153552977): Remove this workaround.
      // For profiles collected for old versions of perfetto_hprof, we do not
      // have any location information. We store them using the nullopt
      // location, and assume they are all part of the main APK.
      //
      // This is to keep ingestion of old profiles working (especially
      // important for the UI).
      class_to_rows_[std::make_pair(
                         base::nullopt,
                         context_->storage->InternString(normalized_type))]
          .emplace_back(id_and_row.id);
    }
  }

  for (const SourceObject& obj : sequence_state.current_objects) {
    auto type_it = type_id_to_db.find(obj.type_id);
    if (type_it == type_id_to_db.end()) {
      context_->storage->IncrementIndexedStats(
          stats::heap_graph_malformed_packet,
          static_cast<int>(sequence_state.current_upid));
      continue;
    }
    tables::HeapGraphClassTable::Id db_id = type_it->second;
    auto id_and_row =
        context_->storage->mutable_heap_graph_object_table()->Insert(
            {sequence_state.current_upid, sequence_state.current_ts,
             static_cast<int64_t>(obj.object_id),
             static_cast<int64_t>(obj.self_size), /*retained_size=*/-1,
             /*unique_retained_size=*/-1, /*reference_set_id=*/base::nullopt,
             /*reachable=*/0, db_id,
             /*root_type=*/base::nullopt});
    int64_t row = id_and_row.row;
    sequence_state.object_id_to_row.emplace(obj.object_id, row);
    int64_t type_row =
        *context_->storage->heap_graph_class_table().id().IndexOf(db_id);
    // Still using raw rows for the HeapGraphWalker to not tie it to the
    // data base and make it useable independently.
    // We will eventually want to use that client-side for summarization.
    sequence_state.walker.AddNode(row, obj.self_size, type_row);
  }

  for (const SourceObject& obj : sequence_state.current_objects) {
    auto it = sequence_state.object_id_to_row.find(obj.object_id);
    if (it == sequence_state.object_id_to_row.end())
      continue;
    uint32_t owner_row = it->second;

    uint32_t reference_set_id =
        context_->storage->heap_graph_reference_table().row_count();
    std::set<int64_t> seen_owned;
    for (const SourceObject::Reference& ref : obj.references) {
      // This is true for unset reference fields.
      if (ref.owned_object_id == 0)
        continue;

      it = sequence_state.object_id_to_row.find(ref.owned_object_id);
      // This can only happen for an invalid type string id, which is already
      // reported as an error. Silently continue here.
      if (it == sequence_state.object_id_to_row.end())
        continue;

      int64_t owned_row = it->second;
      bool inserted;
      std::tie(std::ignore, inserted) = seen_owned.emplace(owned_row);
      if (inserted)
        sequence_state.walker.AddEdge(owner_row, owned_row);

      auto field_it = sequence_state.interned_fields.find(ref.field_name_id);
      if (field_it == sequence_state.interned_fields.end()) {
        context_->storage->IncrementIndexedStats(
            stats::heap_graph_invalid_string_id,
            static_cast<int>(sequence_state.current_upid));
        continue;
      }
      const InternedField& interned_field = field_it->second;
      StringPool::Id field_name = interned_field.name;
      context_->storage->mutable_heap_graph_reference_table()->Insert(
          {reference_set_id, owner_row, owned_row, interned_field.name,
           interned_field.type_name,
           /*deobfuscated_field_name=*/base::nullopt});
      uint32_t row =
          context_->storage->heap_graph_reference_table().row_count() - 1;
      field_to_rows_[field_name].emplace_back(row);
    }
    context_->storage->mutable_heap_graph_object_table()
        ->mutable_reference_set_id()
        ->Set(owner_row, reference_set_id);
  }

  for (const SourceRoot& root : sequence_state.current_roots) {
    for (uint64_t obj_id : root.object_ids) {
      auto it = sequence_state.object_id_to_row.find(obj_id);
      // This can only happen for an invalid type string id, which is already
      // reported as an error. Silently continue here.
      if (it == sequence_state.object_id_to_row.end())
        continue;

      int64_t obj_row = it->second;
      sequence_state.walker.MarkRoot(obj_row);
      context_->storage->mutable_heap_graph_object_table()
          ->mutable_root_type()
          ->Set(static_cast<uint32_t>(obj_row), root.root_type);
    }
  }

  auto paths = sequence_state.walker.FindPathsFromRoot();
  walkers_.emplace(
      std::make_pair(sequence_state.current_upid, sequence_state.current_ts),
      std::move(sequence_state.walker));

  sequence_state_.erase(seq_id);
}

std::unique_ptr<tables::ExperimentalFlamegraphNodesTable>
HeapGraphTracker::BuildFlamegraph(const int64_t current_ts,
                                  const UniquePid current_upid) {
  auto it = walkers_.find(std::make_pair(current_upid, current_ts));
  if (it == walkers_.end())
    return nullptr;

  std::unique_ptr<tables::ExperimentalFlamegraphNodesTable> tbl(
      new tables::ExperimentalFlamegraphNodesTable(
          context_->storage->mutable_string_pool(), nullptr));

  HeapGraphWalker::PathFromRoot init_path = it->second.FindPathsFromRoot();
  auto profile_type = context_->storage->InternString("graph");
  auto java_mapping = context_->storage->InternString("JAVA");

  std::vector<int32_t> node_to_cumulative_size(init_path.nodes.size());
  std::vector<int32_t> node_to_cumulative_count(init_path.nodes.size());
  // i > 0 is to skip the artifical root node.
  for (size_t i = init_path.nodes.size() - 1; i > 0; --i) {
    const HeapGraphWalker::PathFromRoot::Node& node = init_path.nodes[i];

    node_to_cumulative_size[i] += node.size;
    node_to_cumulative_count[i] += node.count;
    node_to_cumulative_size[node.parent_id] += node_to_cumulative_size[i];
    node_to_cumulative_count[node.parent_id] += node_to_cumulative_count[i];
  }

  std::vector<FlamegraphId> node_to_id(init_path.nodes.size());
  // i = 1 is to skip the artifical root node.
  for (size_t i = 1; i < init_path.nodes.size(); ++i) {
    const HeapGraphWalker::PathFromRoot::Node& node = init_path.nodes[i];
    PERFETTO_CHECK(node.parent_id < i);
    base::Optional<FlamegraphId> parent_id;
    if (node.parent_id != 0)
      parent_id = node_to_id[node.parent_id];
    const uint32_t depth = node.depth;

    base::Optional<StringPool::Id> name =
        context_->storage->heap_graph_class_table()
            .deobfuscated_name()[static_cast<uint32_t>(node.class_name)];
    if (!name) {
      name = context_->storage->heap_graph_class_table()
                 .name()[static_cast<uint32_t>(node.class_name)];
    }
    PERFETTO_CHECK(name);

    tables::ExperimentalFlamegraphNodesTable::Row alloc_row{};
    alloc_row.ts = current_ts;
    alloc_row.upid = current_upid;
    alloc_row.profile_type = profile_type;
    alloc_row.depth = depth;
    alloc_row.name = *name;
    alloc_row.map_name = java_mapping;
    alloc_row.count = static_cast<int64_t>(node.count);
    alloc_row.cumulative_count =
        static_cast<int64_t>(node_to_cumulative_count[i]);
    alloc_row.size = static_cast<int64_t>(node.size);
    alloc_row.cumulative_size =
        static_cast<int64_t>(node_to_cumulative_size[i]);
    alloc_row.parent_id = parent_id;
    node_to_id[i] = tbl->Insert(alloc_row).id;
  }
  return tbl;
}

void HeapGraphTracker::MarkReachable(int64_t row) {
  context_->storage->mutable_heap_graph_object_table()
      ->mutable_reachable()
      ->Set(static_cast<uint32_t>(row), 1);
}

void HeapGraphTracker::SetRetained(int64_t row,
                                   int64_t retained,
                                   int64_t unique_retained) {
  context_->storage->mutable_heap_graph_object_table()
      ->mutable_retained_size()
      ->Set(static_cast<uint32_t>(row), retained);
  context_->storage->mutable_heap_graph_object_table()
      ->mutable_unique_retained_size()
      ->Set(static_cast<uint32_t>(row), unique_retained);
}

void HeapGraphTracker::NotifyEndOfFile() {
  if (!sequence_state_.empty()) {
    context_->storage->IncrementStats(stats::heap_graph_non_finalized_graph);
  }
}

StringPool::Id HeapGraphTracker::MaybeDeobfuscate(
    base::Optional<StringPool::Id> package_name,
    StringPool::Id id) {
  base::StringView type_name = context_->storage->GetString(id);
  auto normalized_type = GetNormalizedType(type_name);
  auto it = deobfuscation_mapping_.find(std::make_pair(
      package_name, context_->storage->InternString(normalized_type.name)));
  if (it == deobfuscation_mapping_.end())
    return id;

  base::StringView normalized_deobfuscated_name =
      context_->storage->GetString(it->second);
  std::string result =
      DenormalizeTypeName(normalized_type, normalized_deobfuscated_name);
  return context_->storage->InternString(base::StringView(result));
}

void HeapGraphTracker::AddDeobfuscationMapping(
    base::Optional<StringPool::Id> package_name,
    StringPool::Id obfuscated_name,
    StringPool::Id deobfuscated_name) {
  deobfuscation_mapping_.emplace(std::make_pair(package_name, obfuscated_name),
                                 deobfuscated_name);
}

}  // namespace trace_processor
}  // namespace perfetto
