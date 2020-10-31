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

#include "src/trace_processor/importers/proto/heap_graph_module.h"

#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/importers/proto/heap_graph_tracker.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/types/trace_processor_context.h"

#include "protos/perfetto/trace/profiling/heap_graph.pbzero.h"

namespace perfetto {
namespace trace_processor {

namespace {

const char* HeapGraphRootTypeToString(int32_t type) {
  switch (type) {
    case protos::pbzero::HeapGraphRoot::ROOT_UNKNOWN:
      return "ROOT_UNKNOWN";
    case protos::pbzero::HeapGraphRoot::ROOT_JNI_GLOBAL:
      return "ROOT_JNI_GLOBAL";
    case protos::pbzero::HeapGraphRoot::ROOT_JNI_LOCAL:
      return "ROOT_JNI_LOCAL";
    case protos::pbzero::HeapGraphRoot::ROOT_JAVA_FRAME:
      return "ROOT_JAVA_FRAME";
    case protos::pbzero::HeapGraphRoot::ROOT_NATIVE_STACK:
      return "ROOT_NATIVE_STACK";
    case protos::pbzero::HeapGraphRoot::ROOT_STICKY_CLASS:
      return "ROOT_STICKY_CLASS";
    case protos::pbzero::HeapGraphRoot::ROOT_THREAD_BLOCK:
      return "ROOT_THREAD_BLOCK";
    case protos::pbzero::HeapGraphRoot::ROOT_MONITOR_USED:
      return "ROOT_MONITOR_USED";
    case protos::pbzero::HeapGraphRoot::ROOT_THREAD_OBJECT:
      return "ROOT_THREAD_OBJECT";
    case protos::pbzero::HeapGraphRoot::ROOT_INTERNED_STRING:
      return "ROOT_INTERNED_STRING";
    case protos::pbzero::HeapGraphRoot::ROOT_FINALIZING:
      return "ROOT_FINALIZING";
    case protos::pbzero::HeapGraphRoot::ROOT_DEBUGGER:
      return "ROOT_DEBUGGER";
    case protos::pbzero::HeapGraphRoot::ROOT_REFERENCE_CLEANUP:
      return "ROOT_REFERENCE_CLEANUP";
    case protos::pbzero::HeapGraphRoot::ROOT_VM_INTERNAL:
      return "ROOT_VM_INTERNAL";
    case protos::pbzero::HeapGraphRoot::ROOT_JNI_MONITOR:
      return "ROOT_JNI_MONITOR";
    default:
      return "ROOT_UNKNOWN";
  }
}

// Iterate over a repeated field of varints, independent of whether it is
// packed or not.
template <int32_t field_no, typename T, typename F>
bool ForEachVarInt(const T& decoder, F fn) {
  auto field = decoder.template at<field_no>();
  bool parse_error = false;
  if (field.type() == protozero::proto_utils::ProtoWireType::kLengthDelimited) {
    // packed repeated
    auto it = decoder.template GetPackedRepeated<
        ::protozero::proto_utils::ProtoWireType::kVarInt, uint64_t>(
        field_no, &parse_error);
    for (; it; ++it)
      fn(*it);
  } else {
    // non-packed repeated
    auto it = decoder.template GetRepeated<uint64_t>(field_no);
    for (; it; ++it)
      fn(*it);
  }
  return parse_error;
}

}  // namespace

using perfetto::protos::pbzero::TracePacket;

HeapGraphModule::HeapGraphModule(TraceProcessorContext* context)
    : context_(context) {
  RegisterForField(TracePacket::kHeapGraphFieldNumber, context);
  RegisterForField(TracePacket::kDeobfuscationMappingFieldNumber, context);
}

void HeapGraphModule::ParsePacket(
    const protos::pbzero::TracePacket::Decoder& decoder,
    const TimestampedTracePiece& ttp,
    uint32_t field_id) {
  switch (field_id) {
    case TracePacket::kHeapGraphFieldNumber:
      ParseHeapGraph(decoder.trusted_packet_sequence_id(), ttp.timestamp,
                     decoder.heap_graph());
      return;
    case TracePacket::kDeobfuscationMappingFieldNumber:
      ParseDeobfuscationMapping(decoder.deobfuscation_mapping());
      return;
  }
}

void HeapGraphModule::ParseHeapGraph(uint32_t seq_id,
                                     int64_t ts,
                                     protozero::ConstBytes blob) {
  auto* heap_graph_tracker = HeapGraphTracker::GetOrCreate(context_);
  protos::pbzero::HeapGraph::Decoder heap_graph(blob.data, blob.size);
  UniquePid upid = context_->process_tracker->GetOrCreateProcess(
      static_cast<uint32_t>(heap_graph.pid()));
  heap_graph_tracker->SetPacketIndex(seq_id, heap_graph.index());
  for (auto it = heap_graph.objects(); it; ++it) {
    protos::pbzero::HeapGraphObject::Decoder object(*it);
    HeapGraphTracker::SourceObject obj;
    obj.object_id = object.id();
    obj.self_size = object.self_size();
    obj.type_id = object.type_id();

    std::vector<uint64_t> field_ids;
    std::vector<uint64_t> object_ids;

    bool parse_error = ForEachVarInt<
        protos::pbzero::HeapGraphObject::kReferenceFieldIdFieldNumber>(
        object, [&field_ids](uint64_t value) { field_ids.push_back(value); });

    if (!parse_error) {
      parse_error = ForEachVarInt<
          protos::pbzero::HeapGraphObject::kReferenceObjectIdFieldNumber>(
          object,
          [&object_ids](uint64_t value) { object_ids.push_back(value); });
    }

    if (parse_error) {
      context_->storage->IncrementIndexedStats(
          stats::heap_graph_malformed_packet, static_cast<int>(upid));
      break;
    }
    if (field_ids.size() != object_ids.size()) {
      context_->storage->IncrementIndexedStats(
          stats::heap_graph_malformed_packet, static_cast<int>(upid));
      continue;
    }
    for (size_t i = 0; i < field_ids.size(); ++i) {
      HeapGraphTracker::SourceObject::Reference ref;
      ref.field_name_id = field_ids[i];
      ref.owned_object_id = object_ids[i];
      obj.references.emplace_back(std::move(ref));
    }
    heap_graph_tracker->AddObject(seq_id, upid, ts, std::move(obj));
  }
  for (auto it = heap_graph.types(); it; ++it) {
    protos::pbzero::HeapGraphType::Decoder entry(*it);
    const char* str = reinterpret_cast<const char*>(entry.class_name().data);
    auto str_view = base::StringView(str, entry.class_name().size);

    heap_graph_tracker->AddInternedType(
        seq_id, entry.id(), context_->storage->InternString(str_view),
        entry.location_id());
  }
  for (auto it = heap_graph.type_names(); it; ++it) {
    protos::pbzero::InternedString::Decoder entry(*it);
    const char* str = reinterpret_cast<const char*>(entry.str().data);
    auto str_view = base::StringView(str, entry.str().size);

    heap_graph_tracker->AddInternedTypeName(
        seq_id, entry.iid(), context_->storage->InternString(str_view));
  }
  for (auto it = heap_graph.field_names(); it; ++it) {
    protos::pbzero::InternedString::Decoder entry(*it);
    const char* str = reinterpret_cast<const char*>(entry.str().data);
    auto str_view = base::StringView(str, entry.str().size);

    heap_graph_tracker->AddInternedFieldName(seq_id, entry.iid(), str_view);
  }
  for (auto it = heap_graph.location_names(); it; ++it) {
    protos::pbzero::InternedString::Decoder entry(*it);
    const char* str = reinterpret_cast<const char*>(entry.str().data);
    auto str_view = base::StringView(str, entry.str().size);

    heap_graph_tracker->AddInternedLocationName(
        seq_id, entry.iid(), context_->storage->InternString(str_view));
  }
  for (auto it = heap_graph.roots(); it; ++it) {
    protos::pbzero::HeapGraphRoot::Decoder entry(*it);
    const char* str = HeapGraphRootTypeToString(entry.root_type());
    auto str_view = base::StringView(str);

    HeapGraphTracker::SourceRoot src_root;
    src_root.root_type = context_->storage->InternString(str_view);
    bool parse_error =
        ForEachVarInt<protos::pbzero::HeapGraphRoot::kObjectIdsFieldNumber>(
            entry, [&src_root](uint64_t value) {
              src_root.object_ids.emplace_back(value);
            });
    if (parse_error) {
      context_->storage->IncrementIndexedStats(
          stats::heap_graph_malformed_packet, static_cast<int>(upid));
      break;
    }
    heap_graph_tracker->AddRoot(seq_id, upid, ts, std::move(src_root));
  }
  if (!heap_graph.continued()) {
    heap_graph_tracker->FinalizeProfile(seq_id);
  }
}

void HeapGraphModule::DeobfuscateClass(
    base::Optional<StringPool::Id> package_name_id,
    StringPool::Id obfuscated_class_name_id,
    const protos::pbzero::ObfuscatedClass::Decoder& cls) {
  auto* heap_graph_tracker = HeapGraphTracker::GetOrCreate(context_);
  const std::vector<tables::HeapGraphClassTable::Id>* cls_objects =
      heap_graph_tracker->RowsForType(package_name_id,
                                      obfuscated_class_name_id);

  if (cls_objects) {
    heap_graph_tracker->AddDeobfuscationMapping(
        package_name_id, obfuscated_class_name_id,
        context_->storage->InternString(
            base::StringView(cls.deobfuscated_name())));

    for (tables::HeapGraphClassTable::Id id : *cls_objects) {
      uint32_t row =
          *context_->storage->heap_graph_class_table().id().IndexOf(id);
      const StringPool::Id obfuscated_type_name =
          context_->storage->heap_graph_class_table().name()[row];
      StringPool::Id deobfuscated_type_name =
          heap_graph_tracker->MaybeDeobfuscate(package_name_id,
                                               obfuscated_type_name);
      PERFETTO_CHECK(!deobfuscated_type_name.is_null());
      context_->storage->mutable_heap_graph_class_table()
          ->mutable_deobfuscated_name()
          ->Set(row, deobfuscated_type_name);
    }
  } else {
    PERFETTO_DLOG("Class %s not found",
                  cls.obfuscated_name().ToStdString().c_str());
  }
}

void HeapGraphModule::ParseDeobfuscationMapping(protozero::ConstBytes blob) {
  auto* heap_graph_tracker = HeapGraphTracker::GetOrCreate(context_);
  protos::pbzero::DeobfuscationMapping::Decoder deobfuscation_mapping(
      blob.data, blob.size);
  base::Optional<StringPool::Id> package_name_id;
  if (deobfuscation_mapping.package_name().size > 0) {
    package_name_id = context_->storage->string_pool().GetId(
        deobfuscation_mapping.package_name());
  }

  for (auto class_it = deobfuscation_mapping.obfuscated_classes(); class_it;
       ++class_it) {
    protos::pbzero::ObfuscatedClass::Decoder cls(*class_it);
    auto obfuscated_class_name_id =
        context_->storage->string_pool().GetId(cls.obfuscated_name());
    if (!obfuscated_class_name_id) {
      PERFETTO_DLOG("Class string %s not found",
                    cls.obfuscated_name().ToStdString().c_str());
    } else {
      // TODO(b/153552977): Remove this work-around for legacy traces.
      // For traces without location information, deobfuscate all matching
      // classes.
      DeobfuscateClass(base::nullopt, *obfuscated_class_name_id, cls);
      if (package_name_id) {
        DeobfuscateClass(package_name_id, *obfuscated_class_name_id, cls);
      }
    }
    for (auto member_it = cls.obfuscated_members(); member_it; ++member_it) {
      protos::pbzero::ObfuscatedMember::Decoder member(*member_it);

      std::string merged_obfuscated = cls.obfuscated_name().ToStdString() +
                                      "." +
                                      member.obfuscated_name().ToStdString();
      std::string merged_deobfuscated;
      std::string member_deobfuscated_name =
          member.deobfuscated_name().ToStdString();
      if (member_deobfuscated_name.find('.') == std::string::npos) {
        // Name relative to class.
        merged_deobfuscated = cls.deobfuscated_name().ToStdString() + "." +
                              member_deobfuscated_name;
      } else {
        // Fully qualified name.
        merged_deobfuscated = std::move(member_deobfuscated_name);
      }

      auto obfuscated_field_name_id = context_->storage->string_pool().GetId(
          base::StringView(merged_obfuscated));
      if (!obfuscated_field_name_id) {
        PERFETTO_DLOG("Field string %s not found", merged_obfuscated.c_str());
        continue;
      }

      const std::vector<int64_t>* field_references =
          heap_graph_tracker->RowsForField(*obfuscated_field_name_id);
      if (field_references) {
        auto interned_deobfuscated_name = context_->storage->InternString(
            base::StringView(merged_deobfuscated));
        for (int64_t row : *field_references) {
          context_->storage->mutable_heap_graph_reference_table()
              ->mutable_deobfuscated_field_name()
              ->Set(static_cast<uint32_t>(row), interned_deobfuscated_name);
        }
      } else {
        PERFETTO_DLOG("Field %s not found", merged_obfuscated.c_str());
      }
    }
  }
}

void HeapGraphModule::NotifyEndOfFile() {
  auto* heap_graph_tracker = HeapGraphTracker::GetOrCreate(context_);
  heap_graph_tracker->NotifyEndOfFile();
}

}  // namespace trace_processor
}  // namespace perfetto
