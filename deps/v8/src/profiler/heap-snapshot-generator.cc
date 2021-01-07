// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/profiler/heap-snapshot-generator.h"

#include <utility>

#include "src/api/api-inl.h"
#include "src/base/optional.h"
#include "src/codegen/assembler-inl.h"
#include "src/common/globals.h"
#include "src/debug/debug.h"
#include "src/handles/global-handles.h"
#include "src/heap/combined-heap.h"
#include "src/heap/safepoint.h"
#include "src/numbers/conversions.h"
#include "src/objects/allocation-site-inl.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/cell-inl.h"
#include "src/objects/feedback-cell-inl.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-collection-inl.h"
#include "src/objects/js-generator-inl.h"
#include "src/objects/js-promise-inl.h"
#include "src/objects/js-regexp-inl.h"
#include "src/objects/layout-descriptor.h"
#include "src/objects/literal-objects-inl.h"
#include "src/objects/objects-body-descriptors.h"
#include "src/objects/objects-inl.h"
#include "src/objects/prototype.h"
#include "src/objects/slots-inl.h"
#include "src/objects/struct-inl.h"
#include "src/objects/transitions-inl.h"
#include "src/objects/visitors.h"
#include "src/profiler/allocation-tracker.h"
#include "src/profiler/heap-profiler.h"
#include "src/profiler/heap-snapshot-generator-inl.h"
#include "src/utils/vector.h"

namespace v8 {
namespace internal {

HeapGraphEdge::HeapGraphEdge(Type type, const char* name, HeapEntry* from,
                             HeapEntry* to)
    : bit_field_(TypeField::encode(type) |
                 FromIndexField::encode(from->index())),
      to_entry_(to),
      name_(name) {
  DCHECK(type == kContextVariable
      || type == kProperty
      || type == kInternal
      || type == kShortcut
      || type == kWeak);
}

HeapGraphEdge::HeapGraphEdge(Type type, int index, HeapEntry* from,
                             HeapEntry* to)
    : bit_field_(TypeField::encode(type) |
                 FromIndexField::encode(from->index())),
      to_entry_(to),
      index_(index) {
  DCHECK(type == kElement || type == kHidden);
}

HeapEntry::HeapEntry(HeapSnapshot* snapshot, int index, Type type,
                     const char* name, SnapshotObjectId id, size_t self_size,
                     unsigned trace_node_id)
    : type_(type),
      index_(index),
      children_count_(0),
      self_size_(self_size),
      snapshot_(snapshot),
      name_(name),
      id_(id),
      trace_node_id_(trace_node_id) {
  DCHECK_GE(index, 0);
}

void HeapEntry::SetNamedReference(HeapGraphEdge::Type type,
                                  const char* name,
                                  HeapEntry* entry) {
  ++children_count_;
  snapshot_->edges().emplace_back(type, name, this, entry);
}

void HeapEntry::SetIndexedReference(HeapGraphEdge::Type type,
                                    int index,
                                    HeapEntry* entry) {
  ++children_count_;
  snapshot_->edges().emplace_back(type, index, this, entry);
}

void HeapEntry::SetNamedAutoIndexReference(HeapGraphEdge::Type type,
                                           const char* description,
                                           HeapEntry* child,
                                           StringsStorage* names) {
  int index = children_count_ + 1;
  const char* name = description
                         ? names->GetFormatted("%d / %s", index, description)
                         : names->GetName(index);
  SetNamedReference(type, name, child);
}

void HeapEntry::Print(const char* prefix, const char* edge_name, int max_depth,
                      int indent) const {
  STATIC_ASSERT(sizeof(unsigned) == sizeof(id()));
  base::OS::Print("%6zu @%6u %*c %s%s: ", self_size(), id(), indent, ' ',
                  prefix, edge_name);
  if (type() != kString) {
    base::OS::Print("%s %.40s\n", TypeAsString(), name_);
  } else {
    base::OS::Print("\"");
    const char* c = name_;
    while (*c && (c - name_) <= 40) {
      if (*c != '\n')
        base::OS::Print("%c", *c);
      else
        base::OS::Print("\\n");
      ++c;
    }
    base::OS::Print("\"\n");
  }
  if (--max_depth == 0) return;
  for (auto i = children_begin(); i != children_end(); ++i) {
    HeapGraphEdge& edge = **i;
    const char* edge_prefix = "";
    EmbeddedVector<char, 64> index;
    const char* edge_name = index.begin();
    switch (edge.type()) {
      case HeapGraphEdge::kContextVariable:
        edge_prefix = "#";
        edge_name = edge.name();
        break;
      case HeapGraphEdge::kElement:
        SNPrintF(index, "%d", edge.index());
        break;
      case HeapGraphEdge::kInternal:
        edge_prefix = "$";
        edge_name = edge.name();
        break;
      case HeapGraphEdge::kProperty:
        edge_name = edge.name();
        break;
      case HeapGraphEdge::kHidden:
        edge_prefix = "$";
        SNPrintF(index, "%d", edge.index());
        break;
      case HeapGraphEdge::kShortcut:
        edge_prefix = "^";
        edge_name = edge.name();
        break;
      case HeapGraphEdge::kWeak:
        edge_prefix = "w";
        edge_name = edge.name();
        break;
      default:
        SNPrintF(index, "!!! unknown edge type: %d ", edge.type());
    }
    edge.to()->Print(edge_prefix, edge_name, max_depth, indent + 2);
  }
}

const char* HeapEntry::TypeAsString() const {
  switch (type()) {
    case kHidden: return "/hidden/";
    case kObject: return "/object/";
    case kClosure: return "/closure/";
    case kString: return "/string/";
    case kCode: return "/code/";
    case kArray: return "/array/";
    case kRegExp: return "/regexp/";
    case kHeapNumber: return "/number/";
    case kNative: return "/native/";
    case kSynthetic: return "/synthetic/";
    case kConsString: return "/concatenated string/";
    case kSlicedString: return "/sliced string/";
    case kSymbol: return "/symbol/";
    case kBigInt:
      return "/bigint/";
    default: return "???";
  }
}

HeapSnapshot::HeapSnapshot(HeapProfiler* profiler, bool global_objects_as_roots)
    : profiler_(profiler),
      treat_global_objects_as_roots_(global_objects_as_roots) {
  // It is very important to keep objects that form a heap snapshot
  // as small as possible. Check assumptions about data structure sizes.
  STATIC_ASSERT(kSystemPointerSize != 4 || sizeof(HeapGraphEdge) == 12);
  STATIC_ASSERT(kSystemPointerSize != 8 || sizeof(HeapGraphEdge) == 24);
  STATIC_ASSERT(kSystemPointerSize != 4 || sizeof(HeapEntry) == 32);
#if V8_CC_MSVC
  STATIC_ASSERT(kSystemPointerSize != 8 || sizeof(HeapEntry) == 48);
#else   // !V8_CC_MSVC
  STATIC_ASSERT(kSystemPointerSize != 8 || sizeof(HeapEntry) == 40);
#endif  // !V8_CC_MSVC
  memset(&gc_subroot_entries_, 0, sizeof(gc_subroot_entries_));
}

void HeapSnapshot::Delete() {
  profiler_->RemoveSnapshot(this);
}

void HeapSnapshot::RememberLastJSObjectId() {
  max_snapshot_js_object_id_ = profiler_->heap_object_map()->last_assigned_id();
}

void HeapSnapshot::AddSyntheticRootEntries() {
  AddRootEntry();
  AddGcRootsEntry();
  SnapshotObjectId id = HeapObjectsMap::kGcRootsFirstSubrootId;
  for (int root = 0; root < static_cast<int>(Root::kNumberOfRoots); root++) {
    AddGcSubrootEntry(static_cast<Root>(root), id);
    id += HeapObjectsMap::kObjectIdStep;
  }
  DCHECK_EQ(HeapObjectsMap::kFirstAvailableObjectId, id);
}

void HeapSnapshot::AddRootEntry() {
  DCHECK_NULL(root_entry_);
  DCHECK(entries_.empty());  // Root entry must be the first one.
  root_entry_ = AddEntry(HeapEntry::kSynthetic, "",
                         HeapObjectsMap::kInternalRootObjectId, 0, 0);
  DCHECK_EQ(1u, entries_.size());
  DCHECK_EQ(root_entry_, &entries_.front());
}

void HeapSnapshot::AddGcRootsEntry() {
  DCHECK_NULL(gc_roots_entry_);
  gc_roots_entry_ = AddEntry(HeapEntry::kSynthetic, "(GC roots)",
                             HeapObjectsMap::kGcRootsObjectId, 0, 0);
}

void HeapSnapshot::AddGcSubrootEntry(Root root, SnapshotObjectId id) {
  DCHECK_NULL(gc_subroot_entries_[static_cast<int>(root)]);
  gc_subroot_entries_[static_cast<int>(root)] =
      AddEntry(HeapEntry::kSynthetic, RootVisitor::RootName(root), id, 0, 0);
}

void HeapSnapshot::AddLocation(HeapEntry* entry, int scriptId, int line,
                               int col) {
  locations_.emplace_back(entry->index(), scriptId, line, col);
}

HeapEntry* HeapSnapshot::AddEntry(HeapEntry::Type type,
                                  const char* name,
                                  SnapshotObjectId id,
                                  size_t size,
                                  unsigned trace_node_id) {
  DCHECK(!is_complete());
  entries_.emplace_back(this, static_cast<int>(entries_.size()), type, name, id,
                        size, trace_node_id);
  return &entries_.back();
}

void HeapSnapshot::FillChildren() {
  DCHECK(children().empty());
  int children_index = 0;
  for (HeapEntry& entry : entries()) {
    children_index = entry.set_children_index(children_index);
  }
  DCHECK_EQ(edges().size(), static_cast<size_t>(children_index));
  children().resize(edges().size());
  for (HeapGraphEdge& edge : edges()) {
    edge.from()->add_child(&edge);
  }
}

HeapEntry* HeapSnapshot::GetEntryById(SnapshotObjectId id) {
  if (entries_by_id_cache_.empty()) {
    CHECK(is_complete());
    entries_by_id_cache_.reserve(entries_.size());
    for (HeapEntry& entry : entries_) {
      entries_by_id_cache_.emplace(entry.id(), &entry);
    }
  }
  auto it = entries_by_id_cache_.find(id);
  return it != entries_by_id_cache_.end() ? it->second : nullptr;
}

void HeapSnapshot::Print(int max_depth) {
  root()->Print("", "", max_depth, 0);
}

// We split IDs on evens for embedder objects (see
// HeapObjectsMap::GenerateId) and odds for native objects.
const SnapshotObjectId HeapObjectsMap::kInternalRootObjectId = 1;
const SnapshotObjectId HeapObjectsMap::kGcRootsObjectId =
    HeapObjectsMap::kInternalRootObjectId + HeapObjectsMap::kObjectIdStep;
const SnapshotObjectId HeapObjectsMap::kGcRootsFirstSubrootId =
    HeapObjectsMap::kGcRootsObjectId + HeapObjectsMap::kObjectIdStep;
const SnapshotObjectId HeapObjectsMap::kFirstAvailableObjectId =
    HeapObjectsMap::kGcRootsFirstSubrootId +
    static_cast<int>(Root::kNumberOfRoots) * HeapObjectsMap::kObjectIdStep;

HeapObjectsMap::HeapObjectsMap(Heap* heap)
    : next_id_(kFirstAvailableObjectId), heap_(heap) {
  // The dummy element at zero index is needed as entries_map_ cannot hold
  // an entry with zero value. Otherwise it's impossible to tell if
  // LookupOrInsert has added a new item or just returning exisiting one
  // having the value of zero.
  entries_.emplace_back(0, kNullAddress, 0, true);
}

bool HeapObjectsMap::MoveObject(Address from, Address to, int object_size) {
  DCHECK_NE(kNullAddress, to);
  DCHECK_NE(kNullAddress, from);
  if (from == to) return false;
  void* from_value = entries_map_.Remove(reinterpret_cast<void*>(from),
                                         ComputeAddressHash(from));
  if (from_value == nullptr) {
    // It may occur that some untracked object moves to an address X and there
    // is a tracked object at that address. In this case we should remove the
    // entry as we know that the object has died.
    void* to_value = entries_map_.Remove(reinterpret_cast<void*>(to),
                                         ComputeAddressHash(to));
    if (to_value != nullptr) {
      int to_entry_info_index =
          static_cast<int>(reinterpret_cast<intptr_t>(to_value));
      entries_.at(to_entry_info_index).addr = kNullAddress;
    }
  } else {
    base::HashMap::Entry* to_entry = entries_map_.LookupOrInsert(
        reinterpret_cast<void*>(to), ComputeAddressHash(to));
    if (to_entry->value != nullptr) {
      // We found the existing entry with to address for an old object.
      // Without this operation we will have two EntryInfo's with the same
      // value in addr field. It is bad because later at RemoveDeadEntries
      // one of this entry will be removed with the corresponding entries_map_
      // entry.
      int to_entry_info_index =
          static_cast<int>(reinterpret_cast<intptr_t>(to_entry->value));
      entries_.at(to_entry_info_index).addr = kNullAddress;
    }
    int from_entry_info_index =
        static_cast<int>(reinterpret_cast<intptr_t>(from_value));
    entries_.at(from_entry_info_index).addr = to;
    // Size of an object can change during its life, so to keep information
    // about the object in entries_ consistent, we have to adjust size when the
    // object is migrated.
    if (FLAG_heap_profiler_trace_objects) {
      PrintF("Move object from %p to %p old size %6d new size %6d\n",
             reinterpret_cast<void*>(from), reinterpret_cast<void*>(to),
             entries_.at(from_entry_info_index).size, object_size);
    }
    entries_.at(from_entry_info_index).size = object_size;
    to_entry->value = from_value;
  }
  return from_value != nullptr;
}


void HeapObjectsMap::UpdateObjectSize(Address addr, int size) {
  FindOrAddEntry(addr, size, false);
}


SnapshotObjectId HeapObjectsMap::FindEntry(Address addr) {
  base::HashMap::Entry* entry = entries_map_.Lookup(
      reinterpret_cast<void*>(addr), ComputeAddressHash(addr));
  if (entry == nullptr) return v8::HeapProfiler::kUnknownObjectId;
  int entry_index = static_cast<int>(reinterpret_cast<intptr_t>(entry->value));
  EntryInfo& entry_info = entries_.at(entry_index);
  DCHECK(static_cast<uint32_t>(entries_.size()) > entries_map_.occupancy());
  return entry_info.id;
}


SnapshotObjectId HeapObjectsMap::FindOrAddEntry(Address addr,
                                                unsigned int size,
                                                bool accessed) {
  DCHECK(static_cast<uint32_t>(entries_.size()) > entries_map_.occupancy());
  base::HashMap::Entry* entry = entries_map_.LookupOrInsert(
      reinterpret_cast<void*>(addr), ComputeAddressHash(addr));
  if (entry->value != nullptr) {
    int entry_index =
        static_cast<int>(reinterpret_cast<intptr_t>(entry->value));
    EntryInfo& entry_info = entries_.at(entry_index);
    entry_info.accessed = accessed;
    if (FLAG_heap_profiler_trace_objects) {
      PrintF("Update object size : %p with old size %d and new size %d\n",
             reinterpret_cast<void*>(addr), entry_info.size, size);
    }
    entry_info.size = size;
    return entry_info.id;
  }
  entry->value = reinterpret_cast<void*>(entries_.size());
  SnapshotObjectId id = next_id_;
  next_id_ += kObjectIdStep;
  entries_.push_back(EntryInfo(id, addr, size, accessed));
  DCHECK(static_cast<uint32_t>(entries_.size()) > entries_map_.occupancy());
  return id;
}

SnapshotObjectId HeapObjectsMap::FindMergedNativeEntry(NativeObject addr) {
  auto it = merged_native_entries_map_.find(addr);
  if (it == merged_native_entries_map_.end())
    return v8::HeapProfiler::kUnknownObjectId;
  return entries_[it->second].id;
}

void HeapObjectsMap::AddMergedNativeEntry(NativeObject addr,
                                          Address canonical_addr) {
  base::HashMap::Entry* entry =
      entries_map_.Lookup(reinterpret_cast<void*>(canonical_addr),
                          ComputeAddressHash(canonical_addr));
  auto result = merged_native_entries_map_.insert(
      {addr, reinterpret_cast<size_t>(entry->value)});
  if (!result.second) {
    result.first->second = reinterpret_cast<size_t>(entry->value);
  }
}

void HeapObjectsMap::StopHeapObjectsTracking() { time_intervals_.clear(); }

void HeapObjectsMap::UpdateHeapObjectsMap() {
  if (FLAG_heap_profiler_trace_objects) {
    PrintF("Begin HeapObjectsMap::UpdateHeapObjectsMap. map has %d entries.\n",
           entries_map_.occupancy());
  }
  heap_->PreciseCollectAllGarbage(Heap::kNoGCFlags,
                                  GarbageCollectionReason::kHeapProfiler);
  CombinedHeapObjectIterator iterator(heap_);
  for (HeapObject obj = iterator.Next(); !obj.is_null();
       obj = iterator.Next()) {
    FindOrAddEntry(obj.address(), obj.Size());
    if (FLAG_heap_profiler_trace_objects) {
      PrintF("Update object      : %p %6d. Next address is %p\n",
             reinterpret_cast<void*>(obj.address()), obj.Size(),
             reinterpret_cast<void*>(obj.address() + obj.Size()));
    }
  }
  RemoveDeadEntries();
  if (FLAG_heap_profiler_trace_objects) {
    PrintF("End HeapObjectsMap::UpdateHeapObjectsMap. map has %d entries.\n",
           entries_map_.occupancy());
  }
}

SnapshotObjectId HeapObjectsMap::PushHeapObjectsStats(OutputStream* stream,
                                                      int64_t* timestamp_us) {
  UpdateHeapObjectsMap();
  time_intervals_.emplace_back(next_id_);
  int prefered_chunk_size = stream->GetChunkSize();
  std::vector<v8::HeapStatsUpdate> stats_buffer;
  DCHECK(!entries_.empty());
  EntryInfo* entry_info = &entries_.front();
  EntryInfo* end_entry_info = &entries_.back() + 1;
  for (size_t time_interval_index = 0;
       time_interval_index < time_intervals_.size(); ++time_interval_index) {
    TimeInterval& time_interval = time_intervals_[time_interval_index];
    SnapshotObjectId time_interval_id = time_interval.id;
    uint32_t entries_size = 0;
    EntryInfo* start_entry_info = entry_info;
    while (entry_info < end_entry_info && entry_info->id < time_interval_id) {
      entries_size += entry_info->size;
      ++entry_info;
    }
    uint32_t entries_count =
        static_cast<uint32_t>(entry_info - start_entry_info);
    if (time_interval.count != entries_count ||
        time_interval.size != entries_size) {
      stats_buffer.emplace_back(static_cast<uint32_t>(time_interval_index),
                                time_interval.count = entries_count,
                                time_interval.size = entries_size);
      if (static_cast<int>(stats_buffer.size()) >= prefered_chunk_size) {
        OutputStream::WriteResult result = stream->WriteHeapStatsChunk(
            &stats_buffer.front(), static_cast<int>(stats_buffer.size()));
        if (result == OutputStream::kAbort) return last_assigned_id();
        stats_buffer.clear();
      }
    }
  }
  DCHECK(entry_info == end_entry_info);
  if (!stats_buffer.empty()) {
    OutputStream::WriteResult result = stream->WriteHeapStatsChunk(
        &stats_buffer.front(), static_cast<int>(stats_buffer.size()));
    if (result == OutputStream::kAbort) return last_assigned_id();
  }
  stream->EndOfStream();
  if (timestamp_us) {
    *timestamp_us =
        (time_intervals_.back().timestamp - time_intervals_.front().timestamp)
            .InMicroseconds();
  }
  return last_assigned_id();
}


void HeapObjectsMap::RemoveDeadEntries() {
  DCHECK(entries_.size() > 0 && entries_.at(0).id == 0 &&
         entries_.at(0).addr == kNullAddress);

  // Build up temporary reverse map.
  std::unordered_map<size_t, NativeObject> reverse_merged_native_entries_map;
  for (const auto& it : merged_native_entries_map_) {
    auto result =
        reverse_merged_native_entries_map.emplace(it.second, it.first);
    DCHECK(result.second);
    USE(result);
  }

  size_t first_free_entry = 1;
  for (size_t i = 1; i < entries_.size(); ++i) {
    EntryInfo& entry_info = entries_.at(i);
    auto merged_reverse_it = reverse_merged_native_entries_map.find(i);
    if (entry_info.accessed) {
      if (first_free_entry != i) {
        entries_.at(first_free_entry) = entry_info;
      }
      entries_.at(first_free_entry).accessed = false;
      base::HashMap::Entry* entry =
          entries_map_.Lookup(reinterpret_cast<void*>(entry_info.addr),
                              ComputeAddressHash(entry_info.addr));
      DCHECK(entry);
      entry->value = reinterpret_cast<void*>(first_free_entry);
      if (merged_reverse_it != reverse_merged_native_entries_map.end()) {
        auto it = merged_native_entries_map_.find(merged_reverse_it->second);
        DCHECK_NE(merged_native_entries_map_.end(), it);
        it->second = first_free_entry;
      }
      ++first_free_entry;
    } else {
      if (entry_info.addr) {
        entries_map_.Remove(reinterpret_cast<void*>(entry_info.addr),
                            ComputeAddressHash(entry_info.addr));
        if (merged_reverse_it != reverse_merged_native_entries_map.end()) {
          merged_native_entries_map_.erase(merged_reverse_it->second);
        }
      }
    }
  }
  entries_.erase(entries_.begin() + first_free_entry, entries_.end());

  DCHECK(static_cast<uint32_t>(entries_.size()) - 1 ==
         entries_map_.occupancy());
}

V8HeapExplorer::V8HeapExplorer(HeapSnapshot* snapshot,
                               SnapshottingProgressReportingInterface* progress,
                               v8::HeapProfiler::ObjectNameResolver* resolver)
    : heap_(snapshot->profiler()->heap_object_map()->heap()),
      snapshot_(snapshot),
      names_(snapshot_->profiler()->names()),
      heap_object_map_(snapshot_->profiler()->heap_object_map()),
      progress_(progress),
      generator_(nullptr),
      global_object_name_resolver_(resolver) {}

HeapEntry* V8HeapExplorer::AllocateEntry(HeapThing ptr) {
  return AddEntry(HeapObject::cast(Object(reinterpret_cast<Address>(ptr))));
}

void V8HeapExplorer::ExtractLocation(HeapEntry* entry, HeapObject object) {
  if (object.IsJSFunction()) {
    JSFunction func = JSFunction::cast(object);
    ExtractLocationForJSFunction(entry, func);

  } else if (object.IsJSGeneratorObject()) {
    JSGeneratorObject gen = JSGeneratorObject::cast(object);
    ExtractLocationForJSFunction(entry, gen.function());

  } else if (object.IsJSObject()) {
    JSObject obj = JSObject::cast(object);
    JSFunction maybe_constructor = GetConstructor(obj);

    if (!maybe_constructor.is_null()) {
      ExtractLocationForJSFunction(entry, maybe_constructor);
    }
  }
}

void V8HeapExplorer::ExtractLocationForJSFunction(HeapEntry* entry,
                                                  JSFunction func) {
  if (!func.shared().script().IsScript()) return;
  Script script = Script::cast(func.shared().script());
  int scriptId = script.id();
  int start = func.shared().StartPosition();
  Script::PositionInfo info;
  script.GetPositionInfo(start, &info, Script::WITH_OFFSET);
  snapshot_->AddLocation(entry, scriptId, info.line, info.column);
}

HeapEntry* V8HeapExplorer::AddEntry(HeapObject object) {
  if (object.IsJSFunction()) {
    JSFunction func = JSFunction::cast(object);
    SharedFunctionInfo shared = func.shared();
    const char* name = names_->GetName(shared.Name());
    return AddEntry(object, HeapEntry::kClosure, name);
  } else if (object.IsJSBoundFunction()) {
    return AddEntry(object, HeapEntry::kClosure, "native_bind");
  } else if (object.IsJSRegExp()) {
    JSRegExp re = JSRegExp::cast(object);
    return AddEntry(object, HeapEntry::kRegExp, names_->GetName(re.Pattern()));
  } else if (object.IsJSObject()) {
    const char* name = names_->GetName(
        GetConstructorName(JSObject::cast(object)));
    if (object.IsJSGlobalObject()) {
      auto it = global_object_tag_map_.find(JSGlobalObject::cast(object));
      if (it != global_object_tag_map_.end()) {
        name = names_->GetFormatted("%s / %s", name, it->second);
      }
    }
    return AddEntry(object, HeapEntry::kObject, name);
  } else if (object.IsString()) {
    String string = String::cast(object);
    if (string.IsConsString()) {
      return AddEntry(object, HeapEntry::kConsString, "(concatenated string)");
    } else if (string.IsSlicedString()) {
      return AddEntry(object, HeapEntry::kSlicedString, "(sliced string)");
    } else {
      return AddEntry(object, HeapEntry::kString,
                      names_->GetName(String::cast(object)));
    }
  } else if (object.IsSymbol()) {
    if (Symbol::cast(object).is_private())
      return AddEntry(object, HeapEntry::kHidden, "private symbol");
    else
      return AddEntry(object, HeapEntry::kSymbol, "symbol");
  } else if (object.IsBigInt()) {
    return AddEntry(object, HeapEntry::kBigInt, "bigint");
  } else if (object.IsCode()) {
    return AddEntry(object, HeapEntry::kCode, "");
  } else if (object.IsSharedFunctionInfo()) {
    String name = SharedFunctionInfo::cast(object).Name();
    return AddEntry(object, HeapEntry::kCode, names_->GetName(name));
  } else if (object.IsScript()) {
    Object name = Script::cast(object).name();
    return AddEntry(object, HeapEntry::kCode,
                    name.IsString() ? names_->GetName(String::cast(name)) : "");
  } else if (object.IsNativeContext()) {
    return AddEntry(object, HeapEntry::kHidden, "system / NativeContext");
  } else if (object.IsContext()) {
    return AddEntry(object, HeapEntry::kObject, "system / Context");
  } else if (object.IsFixedArray() || object.IsFixedDoubleArray() ||
             object.IsByteArray()) {
    return AddEntry(object, HeapEntry::kArray, "");
  } else if (object.IsHeapNumber()) {
    return AddEntry(object, HeapEntry::kHeapNumber, "number");
  }
  return AddEntry(object, HeapEntry::kHidden, GetSystemEntryName(object));
}

HeapEntry* V8HeapExplorer::AddEntry(HeapObject object, HeapEntry::Type type,
                                    const char* name) {
  return AddEntry(object.address(), type, name, object.Size());
}

HeapEntry* V8HeapExplorer::AddEntry(Address address,
                                    HeapEntry::Type type,
                                    const char* name,
                                    size_t size) {
  SnapshotObjectId object_id = heap_object_map_->FindOrAddEntry(
      address, static_cast<unsigned int>(size));
  unsigned trace_node_id = 0;
  if (AllocationTracker* allocation_tracker =
      snapshot_->profiler()->allocation_tracker()) {
    trace_node_id =
        allocation_tracker->address_to_trace()->GetTraceNodeId(address);
  }
  return snapshot_->AddEntry(type, name, object_id, size, trace_node_id);
}

const char* V8HeapExplorer::GetSystemEntryName(HeapObject object) {
  switch (object.map().instance_type()) {
    case MAP_TYPE:
      switch (Map::cast(object).instance_type()) {
#define MAKE_STRING_MAP_CASE(instance_type, size, name, Name) \
        case instance_type: return "system / Map (" #Name ")";
      STRING_TYPE_LIST(MAKE_STRING_MAP_CASE)
#undef MAKE_STRING_MAP_CASE
        default: return "system / Map";
      }
    case CELL_TYPE: return "system / Cell";
    case PROPERTY_CELL_TYPE: return "system / PropertyCell";
    case FOREIGN_TYPE: return "system / Foreign";
    case ODDBALL_TYPE: return "system / Oddball";
    case ALLOCATION_SITE_TYPE:
      return "system / AllocationSite";
#define MAKE_STRUCT_CASE(TYPE, Name, name) \
  case TYPE:                               \
    return "system / " #Name;
      STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE
    default: return "system";
  }
}

int V8HeapExplorer::EstimateObjectsCount() {
  CombinedHeapObjectIterator it(heap_, HeapObjectIterator::kFilterUnreachable);
  int objects_count = 0;
  while (!it.Next().is_null()) ++objects_count;
  return objects_count;
}

class IndexedReferencesExtractor : public ObjectVisitor {
 public:
  IndexedReferencesExtractor(V8HeapExplorer* generator, HeapObject parent_obj,
                             HeapEntry* parent)
      : generator_(generator),
        parent_obj_(parent_obj),
        parent_start_(parent_obj_.RawMaybeWeakField(0)),
        parent_end_(parent_obj_.RawMaybeWeakField(parent_obj_.Size())),
        parent_(parent),
        next_index_(0) {}
  void VisitPointers(HeapObject host, ObjectSlot start,
                     ObjectSlot end) override {
    VisitPointers(host, MaybeObjectSlot(start), MaybeObjectSlot(end));
  }
  void VisitPointers(HeapObject host, MaybeObjectSlot start,
                     MaybeObjectSlot end) override {
    // [start,end) must be a sub-region of [parent_start_, parent_end), i.e.
    // all the slots must point inside the object.
    CHECK_LE(parent_start_, start);
    CHECK_LE(end, parent_end_);
    for (MaybeObjectSlot p = start; p < end; ++p) {
      int field_index = static_cast<int>(p - parent_start_);
      if (generator_->visited_fields_[field_index]) {
        generator_->visited_fields_[field_index] = false;
        continue;
      }
      HeapObject heap_object;
      if ((*p)->GetHeapObject(&heap_object)) {
        VisitHeapObjectImpl(heap_object, field_index);
      }
    }
  }

  void VisitCodeTarget(Code host, RelocInfo* rinfo) override {
    Code target = Code::GetCodeFromTargetAddress(rinfo->target_address());
    VisitHeapObjectImpl(target, -1);
  }

  void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) override {
    VisitHeapObjectImpl(rinfo->target_object(), -1);
  }

 private:
  V8_INLINE void VisitHeapObjectImpl(HeapObject heap_object, int field_index) {
    DCHECK_LE(-1, field_index);
    // The last parameter {field_offset} is only used to check some well-known
    // skipped references, so passing -1 * kTaggedSize for objects embedded
    // into code is fine.
    generator_->SetHiddenReference(parent_obj_, parent_, next_index_++,
                                   heap_object, field_index * kTaggedSize);
  }

  V8HeapExplorer* generator_;
  HeapObject parent_obj_;
  MaybeObjectSlot parent_start_;
  MaybeObjectSlot parent_end_;
  HeapEntry* parent_;
  int next_index_;
};

void V8HeapExplorer::ExtractReferences(HeapEntry* entry, HeapObject obj) {
  if (obj.IsJSGlobalProxy()) {
    ExtractJSGlobalProxyReferences(entry, JSGlobalProxy::cast(obj));
  } else if (obj.IsJSArrayBuffer()) {
    ExtractJSArrayBufferReferences(entry, JSArrayBuffer::cast(obj));
  } else if (obj.IsJSObject()) {
    if (obj.IsJSWeakSet()) {
      ExtractJSWeakCollectionReferences(entry, JSWeakSet::cast(obj));
    } else if (obj.IsJSWeakMap()) {
      ExtractJSWeakCollectionReferences(entry, JSWeakMap::cast(obj));
    } else if (obj.IsJSSet()) {
      ExtractJSCollectionReferences(entry, JSSet::cast(obj));
    } else if (obj.IsJSMap()) {
      ExtractJSCollectionReferences(entry, JSMap::cast(obj));
    } else if (obj.IsJSPromise()) {
      ExtractJSPromiseReferences(entry, JSPromise::cast(obj));
    } else if (obj.IsJSGeneratorObject()) {
      ExtractJSGeneratorObjectReferences(entry, JSGeneratorObject::cast(obj));
    }
    ExtractJSObjectReferences(entry, JSObject::cast(obj));
  } else if (obj.IsString()) {
    ExtractStringReferences(entry, String::cast(obj));
  } else if (obj.IsSymbol()) {
    ExtractSymbolReferences(entry, Symbol::cast(obj));
  } else if (obj.IsMap()) {
    ExtractMapReferences(entry, Map::cast(obj));
  } else if (obj.IsSharedFunctionInfo()) {
    ExtractSharedFunctionInfoReferences(entry, SharedFunctionInfo::cast(obj));
  } else if (obj.IsScript()) {
    ExtractScriptReferences(entry, Script::cast(obj));
  } else if (obj.IsAccessorInfo()) {
    ExtractAccessorInfoReferences(entry, AccessorInfo::cast(obj));
  } else if (obj.IsAccessorPair()) {
    ExtractAccessorPairReferences(entry, AccessorPair::cast(obj));
  } else if (obj.IsCode()) {
    ExtractCodeReferences(entry, Code::cast(obj));
  } else if (obj.IsCell()) {
    ExtractCellReferences(entry, Cell::cast(obj));
  } else if (obj.IsFeedbackCell()) {
    ExtractFeedbackCellReferences(entry, FeedbackCell::cast(obj));
  } else if (obj.IsPropertyCell()) {
    ExtractPropertyCellReferences(entry, PropertyCell::cast(obj));
  } else if (obj.IsAllocationSite()) {
    ExtractAllocationSiteReferences(entry, AllocationSite::cast(obj));
  } else if (obj.IsArrayBoilerplateDescription()) {
    ExtractArrayBoilerplateDescriptionReferences(
        entry, ArrayBoilerplateDescription::cast(obj));
  } else if (obj.IsFeedbackVector()) {
    ExtractFeedbackVectorReferences(entry, FeedbackVector::cast(obj));
  } else if (obj.IsDescriptorArray()) {
    ExtractDescriptorArrayReferences(entry, DescriptorArray::cast(obj));
  } else if (obj.IsWeakFixedArray()) {
    ExtractWeakArrayReferences(WeakFixedArray::kHeaderSize, entry,
                               WeakFixedArray::cast(obj));
  } else if (obj.IsWeakArrayList()) {
    ExtractWeakArrayReferences(WeakArrayList::kHeaderSize, entry,
                               WeakArrayList::cast(obj));
  } else if (obj.IsContext()) {
    ExtractContextReferences(entry, Context::cast(obj));
  } else if (obj.IsEphemeronHashTable()) {
    ExtractEphemeronHashTableReferences(entry, EphemeronHashTable::cast(obj));
  } else if (obj.IsFixedArray()) {
    ExtractFixedArrayReferences(entry, FixedArray::cast(obj));
  }
}

void V8HeapExplorer::ExtractJSGlobalProxyReferences(HeapEntry* entry,
                                                    JSGlobalProxy proxy) {
  SetInternalReference(entry, "native_context", proxy.native_context(),
                       JSGlobalProxy::kNativeContextOffset);
}

void V8HeapExplorer::ExtractJSObjectReferences(HeapEntry* entry,
                                               JSObject js_obj) {
  HeapObject obj = js_obj;
  ExtractPropertyReferences(js_obj, entry);
  ExtractElementReferences(js_obj, entry);
  ExtractInternalReferences(js_obj, entry);
  Isolate* isolate = Isolate::FromHeap(heap_);
  PrototypeIterator iter(isolate, js_obj);
  ReadOnlyRoots roots(isolate);
  SetPropertyReference(entry, roots.proto_string(), iter.GetCurrent());
  if (obj.IsJSBoundFunction()) {
    JSBoundFunction js_fun = JSBoundFunction::cast(obj);
    TagObject(js_fun.bound_arguments(), "(bound arguments)");
    SetInternalReference(entry, "bindings", js_fun.bound_arguments(),
                         JSBoundFunction::kBoundArgumentsOffset);
    SetInternalReference(entry, "bound_this", js_fun.bound_this(),
                         JSBoundFunction::kBoundThisOffset);
    SetInternalReference(entry, "bound_function",
                         js_fun.bound_target_function(),
                         JSBoundFunction::kBoundTargetFunctionOffset);
    FixedArray bindings = js_fun.bound_arguments();
    for (int i = 0; i < bindings.length(); i++) {
      const char* reference_name = names_->GetFormatted("bound_argument_%d", i);
      SetNativeBindReference(entry, reference_name, bindings.get(i));
    }
  } else if (obj.IsJSFunction()) {
    JSFunction js_fun = JSFunction::cast(js_obj);
    if (js_fun.has_prototype_slot()) {
      Object proto_or_map = js_fun.prototype_or_initial_map();
      if (!proto_or_map.IsTheHole(isolate)) {
        if (!proto_or_map.IsMap()) {
          SetPropertyReference(entry, roots.prototype_string(), proto_or_map,
                               nullptr,
                               JSFunction::kPrototypeOrInitialMapOffset);
        } else {
          SetPropertyReference(entry, roots.prototype_string(),
                               js_fun.prototype());
          SetInternalReference(entry, "initial_map", proto_or_map,
                               JSFunction::kPrototypeOrInitialMapOffset);
        }
      }
    }
    SharedFunctionInfo shared_info = js_fun.shared();
    TagObject(js_fun.raw_feedback_cell(), "(function feedback cell)");
    SetInternalReference(entry, "feedback_cell", js_fun.raw_feedback_cell(),
                         JSFunction::kFeedbackCellOffset);
    TagObject(shared_info, "(shared function info)");
    SetInternalReference(entry, "shared", shared_info,
                         JSFunction::kSharedFunctionInfoOffset);
    TagObject(js_fun.context(), "(context)");
    SetInternalReference(entry, "context", js_fun.context(),
                         JSFunction::kContextOffset);
    SetInternalReference(entry, "code", js_fun.code(), JSFunction::kCodeOffset);
  } else if (obj.IsJSGlobalObject()) {
    JSGlobalObject global_obj = JSGlobalObject::cast(obj);
    SetInternalReference(entry, "native_context", global_obj.native_context(),
                         JSGlobalObject::kNativeContextOffset);
    SetInternalReference(entry, "global_proxy", global_obj.global_proxy(),
                         JSGlobalObject::kGlobalProxyOffset);
    STATIC_ASSERT(JSGlobalObject::kHeaderSize - JSObject::kHeaderSize ==
                  2 * kTaggedSize);
  } else if (obj.IsJSArrayBufferView()) {
    JSArrayBufferView view = JSArrayBufferView::cast(obj);
    SetInternalReference(entry, "buffer", view.buffer(),
                         JSArrayBufferView::kBufferOffset);
  }

  TagObject(js_obj.raw_properties_or_hash(), "(object properties)");
  SetInternalReference(entry, "properties", js_obj.raw_properties_or_hash(),
                       JSObject::kPropertiesOrHashOffset);

  TagObject(js_obj.elements(), "(object elements)");
  SetInternalReference(entry, "elements", js_obj.elements(),
                       JSObject::kElementsOffset);
}

void V8HeapExplorer::ExtractStringReferences(HeapEntry* entry, String string) {
  if (string.IsConsString()) {
    ConsString cs = ConsString::cast(string);
    SetInternalReference(entry, "first", cs.first(), ConsString::kFirstOffset);
    SetInternalReference(entry, "second", cs.second(),
                         ConsString::kSecondOffset);
  } else if (string.IsSlicedString()) {
    SlicedString ss = SlicedString::cast(string);
    SetInternalReference(entry, "parent", ss.parent(),
                         SlicedString::kParentOffset);
  } else if (string.IsThinString()) {
    ThinString ts = ThinString::cast(string);
    SetInternalReference(entry, "actual", ts.actual(),
                         ThinString::kActualOffset);
  }
}

void V8HeapExplorer::ExtractSymbolReferences(HeapEntry* entry, Symbol symbol) {
  SetInternalReference(entry, "name", symbol.description(),
                       Symbol::kDescriptionOffset);
}

void V8HeapExplorer::ExtractJSCollectionReferences(HeapEntry* entry,
                                                   JSCollection collection) {
  SetInternalReference(entry, "table", collection.table(),
                       JSCollection::kTableOffset);
}

void V8HeapExplorer::ExtractJSWeakCollectionReferences(HeapEntry* entry,
                                                       JSWeakCollection obj) {
  SetInternalReference(entry, "table", obj.table(),
                       JSWeakCollection::kTableOffset);
}

void V8HeapExplorer::ExtractEphemeronHashTableReferences(
    HeapEntry* entry, EphemeronHashTable table) {
  for (InternalIndex i : table.IterateEntries()) {
    int key_index = EphemeronHashTable::EntryToIndex(i) +
                    EphemeronHashTable::kEntryKeyIndex;
    int value_index = EphemeronHashTable::EntryToValueIndex(i);
    Object key = table.get(key_index);
    Object value = table.get(value_index);
    SetWeakReference(entry, key_index, key, table.OffsetOfElementAt(key_index));
    SetWeakReference(entry, value_index, value,
                     table.OffsetOfElementAt(value_index));
    HeapEntry* key_entry = GetEntry(key);
    HeapEntry* value_entry = GetEntry(value);
    HeapEntry* table_entry = GetEntry(table);
    if (key_entry && value_entry && !key.IsUndefined()) {
      const char* edge_name = names_->GetFormatted(
          "part of key (%s @%u) -> value (%s @%u) pair in WeakMap (table @%u)",
          key_entry->name(), key_entry->id(), value_entry->name(),
          value_entry->id(), table_entry->id());
      key_entry->SetNamedAutoIndexReference(HeapGraphEdge::kInternal, edge_name,
                                            value_entry, names_);
      table_entry->SetNamedAutoIndexReference(HeapGraphEdge::kInternal,
                                              edge_name, value_entry, names_);
    }
  }
}

// These static arrays are used to prevent excessive code-size in
// ExtractContextReferences below, which would happen if we called
// SetInternalReference for every native context field in a macro.
static const struct {
  int index;
  const char* name;
} native_context_names[] = {
#define CONTEXT_FIELD_INDEX_NAME(index, _, name) {Context::index, #name},
    NATIVE_CONTEXT_FIELDS(CONTEXT_FIELD_INDEX_NAME)
#undef CONTEXT_FIELD_INDEX_NAME
};

void V8HeapExplorer::ExtractContextReferences(HeapEntry* entry,
                                              Context context) {
  if (!context.IsNativeContext() && context.is_declaration_context()) {
    ScopeInfo scope_info = context.scope_info();
    // Add context allocated locals.
    int context_locals = scope_info.ContextLocalCount();
    for (int i = 0; i < context_locals; ++i) {
      String local_name = scope_info.ContextLocalName(i);
      int idx = scope_info.ContextHeaderLength() + i;
      SetContextReference(entry, local_name, context.get(idx),
                          Context::OffsetOfElementAt(idx));
    }
    if (scope_info.HasFunctionName()) {
      String name = String::cast(scope_info.FunctionName());
      int idx = scope_info.FunctionContextSlotIndex(name);
      if (idx >= 0) {
        SetContextReference(entry, name, context.get(idx),
                            Context::OffsetOfElementAt(idx));
      }
    }
  }

  SetInternalReference(
      entry, "scope_info", context.get(Context::SCOPE_INFO_INDEX),
      FixedArray::OffsetOfElementAt(Context::SCOPE_INFO_INDEX));
  SetInternalReference(entry, "previous", context.get(Context::PREVIOUS_INDEX),
                       FixedArray::OffsetOfElementAt(Context::PREVIOUS_INDEX));
  if (context.has_extension()) {
    SetInternalReference(
        entry, "extension", context.get(Context::EXTENSION_INDEX),
        FixedArray::OffsetOfElementAt(Context::EXTENSION_INDEX));
  }

  if (context.IsNativeContext()) {
    TagObject(context.normalized_map_cache(), "(context norm. map cache)");
    TagObject(context.embedder_data(), "(context data)");
    for (size_t i = 0; i < arraysize(native_context_names); i++) {
      int index = native_context_names[i].index;
      const char* name = native_context_names[i].name;
      SetInternalReference(entry, name, context.get(index),
                           FixedArray::OffsetOfElementAt(index));
    }

    SetWeakReference(
        entry, "optimized_code_list", context.get(Context::OPTIMIZED_CODE_LIST),
        FixedArray::OffsetOfElementAt(Context::OPTIMIZED_CODE_LIST));
    SetWeakReference(
        entry, "deoptimized_code_list",
        context.get(Context::DEOPTIMIZED_CODE_LIST),
        FixedArray::OffsetOfElementAt(Context::DEOPTIMIZED_CODE_LIST));
    STATIC_ASSERT(Context::OPTIMIZED_CODE_LIST == Context::FIRST_WEAK_SLOT);
    STATIC_ASSERT(Context::NEXT_CONTEXT_LINK + 1 ==
                  Context::NATIVE_CONTEXT_SLOTS);
    STATIC_ASSERT(Context::FIRST_WEAK_SLOT + 3 ==
                  Context::NATIVE_CONTEXT_SLOTS);
  }
}

void V8HeapExplorer::ExtractMapReferences(HeapEntry* entry, Map map) {
  MaybeObject maybe_raw_transitions_or_prototype_info = map.raw_transitions();
  HeapObject raw_transitions_or_prototype_info;
  if (maybe_raw_transitions_or_prototype_info->GetHeapObjectIfWeak(
          &raw_transitions_or_prototype_info)) {
    DCHECK(raw_transitions_or_prototype_info.IsMap());
    SetWeakReference(entry, "transition", raw_transitions_or_prototype_info,
                     Map::kTransitionsOrPrototypeInfoOffset);
  } else if (maybe_raw_transitions_or_prototype_info->GetHeapObjectIfStrong(
                 &raw_transitions_or_prototype_info)) {
    if (raw_transitions_or_prototype_info.IsTransitionArray()) {
      TransitionArray transitions =
          TransitionArray::cast(raw_transitions_or_prototype_info);
      if (map.CanTransition() && transitions.HasPrototypeTransitions()) {
        TagObject(transitions.GetPrototypeTransitions(),
                  "(prototype transitions)");
      }
      TagObject(transitions, "(transition array)");
      SetInternalReference(entry, "transitions", transitions,
                           Map::kTransitionsOrPrototypeInfoOffset);
    } else if (raw_transitions_or_prototype_info.IsFixedArray()) {
      TagObject(raw_transitions_or_prototype_info, "(transition)");
      SetInternalReference(entry, "transition",
                           raw_transitions_or_prototype_info,
                           Map::kTransitionsOrPrototypeInfoOffset);
    } else if (map.is_prototype_map()) {
      TagObject(raw_transitions_or_prototype_info, "prototype_info");
      SetInternalReference(entry, "prototype_info",
                           raw_transitions_or_prototype_info,
                           Map::kTransitionsOrPrototypeInfoOffset);
    }
  }
  DescriptorArray descriptors = map.instance_descriptors(kRelaxedLoad);
  TagObject(descriptors, "(map descriptors)");
  SetInternalReference(entry, "descriptors", descriptors,
                       Map::kInstanceDescriptorsOffset);
  SetInternalReference(entry, "prototype", map.prototype(),
                       Map::kPrototypeOffset);
  if (FLAG_unbox_double_fields) {
    SetInternalReference(entry, "layout_descriptor",
                         map.layout_descriptor(kAcquireLoad),
                         Map::kLayoutDescriptorOffset);
  }
  if (map.IsContextMap()) {
    Object native_context = map.native_context();
    TagObject(native_context, "(native context)");
    SetInternalReference(entry, "native_context", native_context,
                         Map::kConstructorOrBackPointerOrNativeContextOffset);
  } else {
    Object constructor_or_backpointer = map.constructor_or_backpointer();
    if (constructor_or_backpointer.IsMap()) {
      TagObject(constructor_or_backpointer, "(back pointer)");
      SetInternalReference(entry, "back_pointer", constructor_or_backpointer,
                           Map::kConstructorOrBackPointerOrNativeContextOffset);
    } else if (constructor_or_backpointer.IsFunctionTemplateInfo()) {
      TagObject(constructor_or_backpointer, "(constructor function data)");
      SetInternalReference(entry, "constructor_function_data",
                           constructor_or_backpointer,
                           Map::kConstructorOrBackPointerOrNativeContextOffset);
    } else {
      SetInternalReference(entry, "constructor", constructor_or_backpointer,
                           Map::kConstructorOrBackPointerOrNativeContextOffset);
    }
  }
  TagObject(map.dependent_code(), "(dependent code)");
  SetInternalReference(entry, "dependent_code", map.dependent_code(),
                       Map::kDependentCodeOffset);
}

void V8HeapExplorer::ExtractSharedFunctionInfoReferences(
    HeapEntry* entry, SharedFunctionInfo shared) {
  std::unique_ptr<char[]> name = shared.DebugNameCStr();
  if (name[0] != '\0') {
    TagObject(shared.GetCode(),
              names_->GetFormatted("(code for %s)", name.get()));
  } else {
    TagObject(shared.GetCode(),
              names_->GetFormatted("(%s code)",
                                   CodeKindToString(shared.GetCode().kind())));
  }

  Object name_or_scope_info = shared.name_or_scope_info(kAcquireLoad);
  if (name_or_scope_info.IsScopeInfo()) {
    TagObject(name_or_scope_info, "(function scope info)");
  }
  SetInternalReference(entry, "name_or_scope_info", name_or_scope_info,
                       SharedFunctionInfo::kNameOrScopeInfoOffset);
  SetInternalReference(entry, "script_or_debug_info",
                       shared.script_or_debug_info(kAcquireLoad),
                       SharedFunctionInfo::kScriptOrDebugInfoOffset);
  SetInternalReference(entry, "function_data",
                       shared.function_data(kAcquireLoad),
                       SharedFunctionInfo::kFunctionDataOffset);
  SetInternalReference(
      entry, "raw_outer_scope_info_or_feedback_metadata",
      shared.raw_outer_scope_info_or_feedback_metadata(),
      SharedFunctionInfo::kOuterScopeInfoOrFeedbackMetadataOffset);
}

void V8HeapExplorer::ExtractScriptReferences(HeapEntry* entry, Script script) {
  SetInternalReference(entry, "source", script.source(), Script::kSourceOffset);
  SetInternalReference(entry, "name", script.name(), Script::kNameOffset);
  SetInternalReference(entry, "context_data", script.context_data(),
                       Script::kContextDataOffset);
  TagObject(script.line_ends(), "(script line ends)");
  SetInternalReference(entry, "line_ends", script.line_ends(),
                       Script::kLineEndsOffset);
}

void V8HeapExplorer::ExtractAccessorInfoReferences(HeapEntry* entry,
                                                   AccessorInfo accessor_info) {
  SetInternalReference(entry, "name", accessor_info.name(),
                       AccessorInfo::kNameOffset);
  SetInternalReference(entry, "expected_receiver_type",
                       accessor_info.expected_receiver_type(),
                       AccessorInfo::kExpectedReceiverTypeOffset);
  SetInternalReference(entry, "getter", accessor_info.getter(),
                       AccessorInfo::kGetterOffset);
  SetInternalReference(entry, "setter", accessor_info.setter(),
                       AccessorInfo::kSetterOffset);
  SetInternalReference(entry, "data", accessor_info.data(),
                       AccessorInfo::kDataOffset);
}

void V8HeapExplorer::ExtractAccessorPairReferences(HeapEntry* entry,
                                                   AccessorPair accessors) {
  SetInternalReference(entry, "getter", accessors.getter(),
                       AccessorPair::kGetterOffset);
  SetInternalReference(entry, "setter", accessors.setter(),
                       AccessorPair::kSetterOffset);
}

void V8HeapExplorer::TagBuiltinCodeObject(Code code, const char* name) {
  TagObject(code, names_->GetFormatted("(%s builtin)", name));
}

void V8HeapExplorer::ExtractCodeReferences(HeapEntry* entry, Code code) {
  TagObject(code.relocation_info(), "(code relocation info)");
  SetInternalReference(entry, "relocation_info", code.relocation_info(),
                       Code::kRelocationInfoOffset);
  TagObject(code.deoptimization_data(), "(code deopt data)");
  SetInternalReference(entry, "deoptimization_data", code.deoptimization_data(),
                       Code::kDeoptimizationDataOffset);
  TagObject(code.source_position_table(), "(source position table)");
  SetInternalReference(entry, "source_position_table",
                       code.source_position_table(),
                       Code::kSourcePositionTableOffset);
}

void V8HeapExplorer::ExtractCellReferences(HeapEntry* entry, Cell cell) {
  SetInternalReference(entry, "value", cell.value(), Cell::kValueOffset);
}

void V8HeapExplorer::ExtractFeedbackCellReferences(HeapEntry* entry,
                                                   FeedbackCell feedback_cell) {
  TagObject(feedback_cell, "(feedback cell)");
  SetInternalReference(entry, "value", feedback_cell.value(),
                       FeedbackCell::kValueOffset);
}

void V8HeapExplorer::ExtractPropertyCellReferences(HeapEntry* entry,
                                                   PropertyCell cell) {
  SetInternalReference(entry, "value", cell.value(),
                       PropertyCell::kValueOffset);
  TagObject(cell.dependent_code(), "(dependent code)");
  SetInternalReference(entry, "dependent_code", cell.dependent_code(),
                       PropertyCell::kDependentCodeOffset);
}

void V8HeapExplorer::ExtractAllocationSiteReferences(HeapEntry* entry,
                                                     AllocationSite site) {
  SetInternalReference(entry, "transition_info",
                       site.transition_info_or_boilerplate(),
                       AllocationSite::kTransitionInfoOrBoilerplateOffset);
  SetInternalReference(entry, "nested_site", site.nested_site(),
                       AllocationSite::kNestedSiteOffset);
  TagObject(site.dependent_code(), "(dependent code)");
  SetInternalReference(entry, "dependent_code", site.dependent_code(),
                       AllocationSite::kDependentCodeOffset);
}

void V8HeapExplorer::ExtractArrayBoilerplateDescriptionReferences(
    HeapEntry* entry, ArrayBoilerplateDescription value) {
  SetInternalReference(entry, "constant_elements", value.constant_elements(),
                       ArrayBoilerplateDescription::kConstantElementsOffset);
}

class JSArrayBufferDataEntryAllocator : public HeapEntriesAllocator {
 public:
  JSArrayBufferDataEntryAllocator(size_t size, V8HeapExplorer* explorer)
      : size_(size)
      , explorer_(explorer) {
  }
  HeapEntry* AllocateEntry(HeapThing ptr) override {
    return explorer_->AddEntry(reinterpret_cast<Address>(ptr),
                               HeapEntry::kNative, "system / JSArrayBufferData",
                               size_);
  }
 private:
  size_t size_;
  V8HeapExplorer* explorer_;
};

void V8HeapExplorer::ExtractJSArrayBufferReferences(HeapEntry* entry,
                                                    JSArrayBuffer buffer) {
  // Setup a reference to a native memory backing_store object.
  if (!buffer.backing_store()) return;
  size_t data_size = buffer.byte_length();
  JSArrayBufferDataEntryAllocator allocator(data_size, this);
  HeapEntry* data_entry =
      generator_->FindOrAddEntry(buffer.backing_store(), &allocator);
  entry->SetNamedReference(HeapGraphEdge::kInternal, "backing_store",
                           data_entry);
}

void V8HeapExplorer::ExtractJSPromiseReferences(HeapEntry* entry,
                                                JSPromise promise) {
  SetInternalReference(entry, "reactions_or_result",
                       promise.reactions_or_result(),
                       JSPromise::kReactionsOrResultOffset);
}

void V8HeapExplorer::ExtractJSGeneratorObjectReferences(
    HeapEntry* entry, JSGeneratorObject generator) {
  SetInternalReference(entry, "function", generator.function(),
                       JSGeneratorObject::kFunctionOffset);
  SetInternalReference(entry, "context", generator.context(),
                       JSGeneratorObject::kContextOffset);
  SetInternalReference(entry, "receiver", generator.receiver(),
                       JSGeneratorObject::kReceiverOffset);
  SetInternalReference(entry, "parameters_and_registers",
                       generator.parameters_and_registers(),
                       JSGeneratorObject::kParametersAndRegistersOffset);
}

void V8HeapExplorer::ExtractFixedArrayReferences(HeapEntry* entry,
                                                 FixedArray array) {
  for (int i = 0, l = array.length(); i < l; ++i) {
    DCHECK(!HasWeakHeapObjectTag(array.get(i)));
    SetInternalReference(entry, i, array.get(i), array.OffsetOfElementAt(i));
  }
}

void V8HeapExplorer::ExtractFeedbackVectorReferences(
    HeapEntry* entry, FeedbackVector feedback_vector) {
  MaybeObject code = feedback_vector.maybe_optimized_code();
  HeapObject code_heap_object;
  if (code->GetHeapObjectIfWeak(&code_heap_object)) {
    SetWeakReference(entry, "optimized code", code_heap_object,
                     FeedbackVector::kMaybeOptimizedCodeOffset);
  }
}

void V8HeapExplorer::ExtractDescriptorArrayReferences(HeapEntry* entry,
                                                      DescriptorArray array) {
  SetInternalReference(entry, "enum_cache", array.enum_cache(),
                       DescriptorArray::kEnumCacheOffset);
  MaybeObjectSlot start = MaybeObjectSlot(array.GetDescriptorSlot(0));
  MaybeObjectSlot end = MaybeObjectSlot(
      array.GetDescriptorSlot(array.number_of_all_descriptors()));
  for (int i = 0; start + i < end; ++i) {
    MaybeObjectSlot slot = start + i;
    int offset = static_cast<int>(slot.address() - array.address());
    MaybeObject object = *slot;
    HeapObject heap_object;
    if (object->GetHeapObjectIfWeak(&heap_object)) {
      SetWeakReference(entry, i, heap_object, offset);
    } else if (object->GetHeapObjectIfStrong(&heap_object)) {
      SetInternalReference(entry, i, heap_object, offset);
    }
  }
}

template <typename T>
void V8HeapExplorer::ExtractWeakArrayReferences(int header_size,
                                                HeapEntry* entry, T array) {
  for (int i = 0; i < array.length(); ++i) {
    MaybeObject object = array.Get(i);
    HeapObject heap_object;
    if (object->GetHeapObjectIfWeak(&heap_object)) {
      SetWeakReference(entry, i, heap_object, header_size + i * kTaggedSize);
    } else if (object->GetHeapObjectIfStrong(&heap_object)) {
      SetInternalReference(entry, i, heap_object,
                           header_size + i * kTaggedSize);
    }
  }
}

void V8HeapExplorer::ExtractPropertyReferences(JSObject js_obj,
                                               HeapEntry* entry) {
  Isolate* isolate = js_obj.GetIsolate();
  if (js_obj.HasFastProperties()) {
    DescriptorArray descs = js_obj.map().instance_descriptors(kRelaxedLoad);
    for (InternalIndex i : js_obj.map().IterateOwnDescriptors()) {
      PropertyDetails details = descs.GetDetails(i);
      switch (details.location()) {
        case kField: {
          Representation r = details.representation();
          if (r.IsSmi() || r.IsDouble()) break;

          Name k = descs.GetKey(i);
          FieldIndex field_index = FieldIndex::ForDescriptor(js_obj.map(), i);
          Object value = js_obj.RawFastPropertyAt(field_index);
          int field_offset =
              field_index.is_inobject() ? field_index.offset() : -1;

          SetDataOrAccessorPropertyReference(details.kind(), entry, k, value,
                                             nullptr, field_offset);
          break;
        }
        case kDescriptor:
          SetDataOrAccessorPropertyReference(
              details.kind(), entry, descs.GetKey(i), descs.GetStrongValue(i));
          break;
      }
    }
  } else if (js_obj.IsJSGlobalObject()) {
    // We assume that global objects can only have slow properties.
    GlobalDictionary dictionary =
        JSGlobalObject::cast(js_obj).global_dictionary(kAcquireLoad);
    ReadOnlyRoots roots(isolate);
    for (InternalIndex i : dictionary.IterateEntries()) {
      if (!dictionary.IsKey(roots, dictionary.KeyAt(i))) continue;
      PropertyCell cell = dictionary.CellAt(i);
      Name name = cell.name();
      Object value = cell.value();
      PropertyDetails details = cell.property_details();
      SetDataOrAccessorPropertyReference(details.kind(), entry, name, value);
    }
  } else if (V8_DICT_MODE_PROTOTYPES_BOOL) {
    OrderedNameDictionary dictionary = js_obj.property_dictionary_ordered();
    ReadOnlyRoots roots(isolate);
    for (InternalIndex i : dictionary.IterateEntries()) {
      Object k = dictionary.KeyAt(i);
      if (!dictionary.IsKey(roots, k)) continue;
      Object value = dictionary.ValueAt(i);
      PropertyDetails details = dictionary.DetailsAt(i);
      SetDataOrAccessorPropertyReference(details.kind(), entry, Name::cast(k),
                                         value);
    }
  } else {
    NameDictionary dictionary = js_obj.property_dictionary();
    ReadOnlyRoots roots(isolate);
    for (InternalIndex i : dictionary.IterateEntries()) {
      Object k = dictionary.KeyAt(i);
      if (!dictionary.IsKey(roots, k)) continue;
      Object value = dictionary.ValueAt(i);
      PropertyDetails details = dictionary.DetailsAt(i);
      SetDataOrAccessorPropertyReference(details.kind(), entry, Name::cast(k),
                                         value);
    }
  }
}

void V8HeapExplorer::ExtractAccessorPairProperty(HeapEntry* entry, Name key,
                                                 Object callback_obj,
                                                 int field_offset) {
  if (!callback_obj.IsAccessorPair()) return;
  AccessorPair accessors = AccessorPair::cast(callback_obj);
  SetPropertyReference(entry, key, accessors, nullptr, field_offset);
  Object getter = accessors.getter();
  if (!getter.IsOddball()) {
    SetPropertyReference(entry, key, getter, "get %s");
  }
  Object setter = accessors.setter();
  if (!setter.IsOddball()) {
    SetPropertyReference(entry, key, setter, "set %s");
  }
}

void V8HeapExplorer::ExtractElementReferences(JSObject js_obj,
                                              HeapEntry* entry) {
  ReadOnlyRoots roots = js_obj.GetReadOnlyRoots();
  if (js_obj.HasObjectElements()) {
    FixedArray elements = FixedArray::cast(js_obj.elements());
    int length = js_obj.IsJSArray() ? Smi::ToInt(JSArray::cast(js_obj).length())
                                    : elements.length();
    for (int i = 0; i < length; ++i) {
      if (!elements.get(i).IsTheHole(roots)) {
        SetElementReference(entry, i, elements.get(i));
      }
    }
  } else if (js_obj.HasDictionaryElements()) {
    NumberDictionary dictionary = js_obj.element_dictionary();
    for (InternalIndex i : dictionary.IterateEntries()) {
      Object k = dictionary.KeyAt(i);
      if (!dictionary.IsKey(roots, k)) continue;
      DCHECK(k.IsNumber());
      uint32_t index = static_cast<uint32_t>(k.Number());
      SetElementReference(entry, index, dictionary.ValueAt(i));
    }
  }
}

void V8HeapExplorer::ExtractInternalReferences(JSObject js_obj,
                                               HeapEntry* entry) {
  int length = js_obj.GetEmbedderFieldCount();
  for (int i = 0; i < length; ++i) {
    Object o = js_obj.GetEmbedderField(i);
    SetInternalReference(entry, i, o, js_obj.GetEmbedderFieldOffset(i));
  }
}

JSFunction V8HeapExplorer::GetConstructor(JSReceiver receiver) {
  Isolate* isolate = receiver.GetIsolate();
  DisallowGarbageCollection no_gc;
  HandleScope scope(isolate);
  MaybeHandle<JSFunction> maybe_constructor =
      JSReceiver::GetConstructor(handle(receiver, isolate));

  if (maybe_constructor.is_null()) return JSFunction();

  return *maybe_constructor.ToHandleChecked();
}

String V8HeapExplorer::GetConstructorName(JSObject object) {
  Isolate* isolate = object.GetIsolate();
  if (object.IsJSFunction()) return ReadOnlyRoots(isolate).closure_string();
  DisallowGarbageCollection no_gc;
  HandleScope scope(isolate);
  return *JSReceiver::GetConstructorName(handle(object, isolate));
}

HeapEntry* V8HeapExplorer::GetEntry(Object obj) {
  return obj.IsHeapObject() ? generator_->FindOrAddEntry(
                                  reinterpret_cast<void*>(obj.ptr()), this)
                            : nullptr;
}

class RootsReferencesExtractor : public RootVisitor {
 public:
  explicit RootsReferencesExtractor(V8HeapExplorer* explorer)
      : explorer_(explorer), visiting_weak_roots_(false) {}

  void SetVisitingWeakRoots() { visiting_weak_roots_ = true; }

  void VisitRootPointer(Root root, const char* description,
                        FullObjectSlot object) override {
    if (root == Root::kBuiltins) {
      explorer_->TagBuiltinCodeObject(Code::cast(*object), description);
    }
    explorer_->SetGcSubrootReference(root, description, visiting_weak_roots_,
                                     *object);
  }

  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override {
    for (FullObjectSlot p = start; p < end; ++p) {
      VisitRootPointer(root, description, p);
    }
  }

  void VisitRootPointers(Root root, const char* description,
                         OffHeapObjectSlot start,
                         OffHeapObjectSlot end) override {
    DCHECK_EQ(root, Root::kStringTable);
    IsolateRoot isolate = Isolate::FromHeap(explorer_->heap_);
    for (OffHeapObjectSlot p = start; p < end; ++p) {
      explorer_->SetGcSubrootReference(root, description, visiting_weak_roots_,
                                       p.load(isolate));
    }
  }

 private:
  V8HeapExplorer* explorer_;
  bool visiting_weak_roots_;
};

bool V8HeapExplorer::IterateAndExtractReferences(
    HeapSnapshotGenerator* generator) {
  generator_ = generator;

  // Create references to the synthetic roots.
  SetRootGcRootsReference();
  for (int root = 0; root < static_cast<int>(Root::kNumberOfRoots); root++) {
    SetGcRootsReference(static_cast<Root>(root));
  }

  // Make sure builtin code objects get their builtin tags
  // first. Otherwise a particular JSFunction object could set
  // its custom name to a generic builtin.
  RootsReferencesExtractor extractor(this);
  ReadOnlyRoots(heap_).Iterate(&extractor);
  heap_->IterateRoots(&extractor, base::EnumSet<SkipRoot>{SkipRoot::kWeak});
  // TODO(ulan): The heap snapshot generator incorrectly considers the weak
  // string tables as strong retainers. Move IterateWeakRoots after
  // SetVisitingWeakRoots.
  heap_->IterateWeakRoots(&extractor, {});
  extractor.SetVisitingWeakRoots();
  heap_->IterateWeakGlobalHandles(&extractor);

  bool interrupted = false;

  CombinedHeapObjectIterator iterator(heap_,
                                      HeapObjectIterator::kFilterUnreachable);
  // Heap iteration with filtering must be finished in any case.
  for (HeapObject obj = iterator.Next(); !obj.is_null();
       obj = iterator.Next(), progress_->ProgressStep()) {
    if (interrupted) continue;

    size_t max_pointer = obj.Size() / kTaggedSize;
    if (max_pointer > visited_fields_.size()) {
      // Clear the current bits.
      std::vector<bool>().swap(visited_fields_);
      // Reallocate to right size.
      visited_fields_.resize(max_pointer, false);
    }

    HeapEntry* entry = GetEntry(obj);
    ExtractReferences(entry, obj);
    SetInternalReference(entry, "map", obj.map(), HeapObject::kMapOffset);
    // Extract unvisited fields as hidden references and restore tags
    // of visited fields.
    IndexedReferencesExtractor refs_extractor(this, obj, entry);
    obj.Iterate(&refs_extractor);

    // Ensure visited_fields_ doesn't leak to the next object.
    for (size_t i = 0; i < max_pointer; ++i) {
      DCHECK(!visited_fields_[i]);
    }

    // Extract location for specific object types
    ExtractLocation(entry, obj);

    if (!progress_->ProgressReport(false)) interrupted = true;
  }

  generator_ = nullptr;
  return interrupted ? false : progress_->ProgressReport(true);
}

bool V8HeapExplorer::IsEssentialObject(Object object) {
  ReadOnlyRoots roots(heap_);
  return object.IsHeapObject() && !object.IsOddball() &&
         object != roots.empty_byte_array() &&
         object != roots.empty_fixed_array() &&
         object != roots.empty_weak_fixed_array() &&
         object != roots.empty_descriptor_array() &&
         object != roots.fixed_array_map() && object != roots.cell_map() &&
         object != roots.global_property_cell_map() &&
         object != roots.shared_function_info_map() &&
         object != roots.free_space_map() &&
         object != roots.one_pointer_filler_map() &&
         object != roots.two_pointer_filler_map();
}

bool V8HeapExplorer::IsEssentialHiddenReference(Object parent,
                                                int field_offset) {
  if (parent.IsAllocationSite() &&
      field_offset == AllocationSite::kWeakNextOffset)
    return false;
  if (parent.IsCodeDataContainer() &&
      field_offset == CodeDataContainer::kNextCodeLinkOffset)
    return false;
  if (parent.IsContext() &&
      field_offset == Context::OffsetOfElementAt(Context::NEXT_CONTEXT_LINK))
    return false;
  return true;
}

void V8HeapExplorer::SetContextReference(HeapEntry* parent_entry,
                                         String reference_name,
                                         Object child_obj, int field_offset) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry == nullptr) return;
  parent_entry->SetNamedReference(HeapGraphEdge::kContextVariable,
                                  names_->GetName(reference_name), child_entry);
  MarkVisitedField(field_offset);
}

void V8HeapExplorer::MarkVisitedField(int offset) {
  if (offset < 0) return;
  int index = offset / kTaggedSize;
  DCHECK(!visited_fields_[index]);
  visited_fields_[index] = true;
}

void V8HeapExplorer::SetNativeBindReference(HeapEntry* parent_entry,
                                            const char* reference_name,
                                            Object child_obj) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry == nullptr) return;
  parent_entry->SetNamedReference(HeapGraphEdge::kShortcut, reference_name,
                                  child_entry);
}

void V8HeapExplorer::SetElementReference(HeapEntry* parent_entry, int index,
                                         Object child_obj) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry == nullptr) return;
  parent_entry->SetIndexedReference(HeapGraphEdge::kElement, index,
                                    child_entry);
}

void V8HeapExplorer::SetInternalReference(HeapEntry* parent_entry,
                                          const char* reference_name,
                                          Object child_obj, int field_offset) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry == nullptr) return;
  if (IsEssentialObject(child_obj)) {
    parent_entry->SetNamedReference(HeapGraphEdge::kInternal, reference_name,
                                    child_entry);
  }
  MarkVisitedField(field_offset);
}

void V8HeapExplorer::SetInternalReference(HeapEntry* parent_entry, int index,
                                          Object child_obj, int field_offset) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry == nullptr) return;
  if (IsEssentialObject(child_obj)) {
    parent_entry->SetNamedReference(HeapGraphEdge::kInternal,
                                    names_->GetName(index), child_entry);
  }
  MarkVisitedField(field_offset);
}

void V8HeapExplorer::SetHiddenReference(HeapObject parent_obj,
                                        HeapEntry* parent_entry, int index,
                                        Object child_obj, int field_offset) {
  DCHECK_EQ(parent_entry, GetEntry(parent_obj));
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry != nullptr && IsEssentialObject(child_obj) &&
      IsEssentialHiddenReference(parent_obj, field_offset)) {
    parent_entry->SetIndexedReference(HeapGraphEdge::kHidden, index,
                                      child_entry);
  }
}

void V8HeapExplorer::SetWeakReference(HeapEntry* parent_entry,
                                      const char* reference_name,
                                      Object child_obj, int field_offset) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry == nullptr) return;
  if (IsEssentialObject(child_obj)) {
    parent_entry->SetNamedReference(HeapGraphEdge::kWeak, reference_name,
                                    child_entry);
  }
  MarkVisitedField(field_offset);
}

void V8HeapExplorer::SetWeakReference(HeapEntry* parent_entry, int index,
                                      Object child_obj, int field_offset) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry == nullptr) return;
  if (IsEssentialObject(child_obj)) {
    parent_entry->SetNamedReference(
        HeapGraphEdge::kWeak, names_->GetFormatted("%d", index), child_entry);
  }
  MarkVisitedField(field_offset);
}

void V8HeapExplorer::SetDataOrAccessorPropertyReference(
    PropertyKind kind, HeapEntry* parent_entry, Name reference_name,
    Object child_obj, const char* name_format_string, int field_offset) {
  if (kind == kAccessor) {
    ExtractAccessorPairProperty(parent_entry, reference_name, child_obj,
                                field_offset);
  } else {
    SetPropertyReference(parent_entry, reference_name, child_obj,
                         name_format_string, field_offset);
  }
}

void V8HeapExplorer::SetPropertyReference(HeapEntry* parent_entry,
                                          Name reference_name, Object child_obj,
                                          const char* name_format_string,
                                          int field_offset) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry == nullptr) return;
  HeapGraphEdge::Type type =
      reference_name.IsSymbol() || String::cast(reference_name).length() > 0
          ? HeapGraphEdge::kProperty
          : HeapGraphEdge::kInternal;
  const char* name =
      name_format_string != nullptr && reference_name.IsString()
          ? names_->GetFormatted(
                name_format_string,
                String::cast(reference_name)
                    .ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL)
                    .get())
          : names_->GetName(reference_name);

  parent_entry->SetNamedReference(type, name, child_entry);
  MarkVisitedField(field_offset);
}

void V8HeapExplorer::SetRootGcRootsReference() {
  snapshot_->root()->SetIndexedAutoIndexReference(HeapGraphEdge::kElement,
                                                  snapshot_->gc_roots());
}

void V8HeapExplorer::SetUserGlobalReference(Object child_obj) {
  HeapEntry* child_entry = GetEntry(child_obj);
  DCHECK_NOT_NULL(child_entry);
  snapshot_->root()->SetNamedAutoIndexReference(HeapGraphEdge::kShortcut,
                                                nullptr, child_entry, names_);
}

void V8HeapExplorer::SetGcRootsReference(Root root) {
  snapshot_->gc_roots()->SetIndexedAutoIndexReference(
      HeapGraphEdge::kElement, snapshot_->gc_subroot(root));
}

void V8HeapExplorer::SetGcSubrootReference(Root root, const char* description,
                                           bool is_weak, Object child_obj) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry == nullptr) return;
  const char* name = GetStrongGcSubrootName(child_obj);
  HeapGraphEdge::Type edge_type =
      is_weak ? HeapGraphEdge::kWeak : HeapGraphEdge::kInternal;
  if (name != nullptr) {
    snapshot_->gc_subroot(root)->SetNamedReference(edge_type, name,
                                                   child_entry);
  } else {
    snapshot_->gc_subroot(root)->SetNamedAutoIndexReference(
        edge_type, description, child_entry, names_);
  }

  // For full heap snapshots we do not emit user roots but rather rely on
  // regular GC roots to retain objects.
  if (!snapshot_->treat_global_objects_as_roots()) return;

  // Add a shortcut to JS global object reference at snapshot root.
  // That allows the user to easily find global objects. They are
  // also used as starting points in distance calculations.
  if (is_weak || !child_obj.IsNativeContext()) return;

  JSGlobalObject global = Context::cast(child_obj).global_object();
  if (!global.IsJSGlobalObject()) return;

  if (!user_roots_.insert(global).second) return;

  SetUserGlobalReference(global);
}

const char* V8HeapExplorer::GetStrongGcSubrootName(Object object) {
  if (strong_gc_subroot_names_.empty()) {
    Isolate* isolate = Isolate::FromHeap(heap_);
    for (RootIndex root_index = RootIndex::kFirstStrongOrReadOnlyRoot;
         root_index <= RootIndex::kLastStrongOrReadOnlyRoot; ++root_index) {
      const char* name = RootsTable::name(root_index);
      strong_gc_subroot_names_.emplace(isolate->root(root_index), name);
    }
    CHECK(!strong_gc_subroot_names_.empty());
  }
  auto it = strong_gc_subroot_names_.find(object);
  return it != strong_gc_subroot_names_.end() ? it->second : nullptr;
}

void V8HeapExplorer::TagObject(Object obj, const char* tag) {
  if (IsEssentialObject(obj)) {
    HeapEntry* entry = GetEntry(obj);
    if (entry->name()[0] == '\0') {
      entry->set_name(tag);
    }
  }
}

class GlobalObjectsEnumerator : public RootVisitor {
 public:
  explicit GlobalObjectsEnumerator(Isolate* isolate) : isolate_(isolate) {}

  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override {
    VisitRootPointersImpl(root, description, start, end);
  }

  void VisitRootPointers(Root root, const char* description,
                         OffHeapObjectSlot start,
                         OffHeapObjectSlot end) override {
    VisitRootPointersImpl(root, description, start, end);
  }

  int count() const { return static_cast<int>(objects_.size()); }
  Handle<JSGlobalObject>& at(int i) { return objects_[i]; }

 private:
  template <typename TSlot>
  void VisitRootPointersImpl(Root root, const char* description, TSlot start,
                             TSlot end) {
    for (TSlot p = start; p < end; ++p) {
      Object o = p.load(isolate_);
      if (!o.IsNativeContext(isolate_)) continue;
      JSObject proxy = Context::cast(o).global_proxy();
      if (!proxy.IsJSGlobalProxy(isolate_)) continue;
      Object global = proxy.map(isolate_).prototype(isolate_);
      if (!global.IsJSGlobalObject(isolate_)) continue;
      objects_.push_back(handle(JSGlobalObject::cast(global), isolate_));
    }
  }

  Isolate* isolate_;
  std::vector<Handle<JSGlobalObject>> objects_;
};


// Modifies heap. Must not be run during heap traversal.
void V8HeapExplorer::CollectGlobalObjectsTags() {
  if (!global_object_name_resolver_) return;

  Isolate* isolate = Isolate::FromHeap(heap_);
  GlobalObjectsEnumerator enumerator(isolate);
  isolate->global_handles()->IterateAllRoots(&enumerator);
  for (int i = 0, l = enumerator.count(); i < l; ++i) {
    Handle<JSGlobalObject> obj = enumerator.at(i);
    const char* tag = global_object_name_resolver_->GetName(
        Utils::ToLocal(Handle<JSObject>::cast(obj)));
    if (tag) {
      global_object_tag_pairs_.emplace_back(obj, tag);
    }
  }
}

void V8HeapExplorer::MakeGlobalObjectTagMap(
    const SafepointScope& safepoint_scope) {
  for (const auto& pair : global_object_tag_pairs_) {
    global_object_tag_map_.emplace(*pair.first, pair.second);
  }
}

class EmbedderGraphImpl : public EmbedderGraph {
 public:
  struct Edge {
    Node* from;
    Node* to;
    const char* name;
  };

  class V8NodeImpl : public Node {
   public:
    explicit V8NodeImpl(Object object) : object_(object) {}
    Object GetObject() { return object_; }

    // Node overrides.
    bool IsEmbedderNode() override { return false; }
    const char* Name() override {
      // The name should be retrieved via GetObject().
      UNREACHABLE();
      return "";
    }
    size_t SizeInBytes() override {
      // The size should be retrieved via GetObject().
      UNREACHABLE();
      return 0;
    }

   private:
    Object object_;
  };

  Node* V8Node(const v8::Local<v8::Value>& value) final {
    Handle<Object> object = v8::Utils::OpenHandle(*value);
    DCHECK(!object.is_null());
    return AddNode(std::unique_ptr<Node>(new V8NodeImpl(*object)));
  }

  Node* AddNode(std::unique_ptr<Node> node) final {
    Node* result = node.get();
    nodes_.push_back(std::move(node));
    return result;
  }

  void AddEdge(Node* from, Node* to, const char* name) final {
    edges_.push_back({from, to, name});
  }

  const std::vector<std::unique_ptr<Node>>& nodes() { return nodes_; }
  const std::vector<Edge>& edges() { return edges_; }

 private:
  std::vector<std::unique_ptr<Node>> nodes_;
  std::vector<Edge> edges_;
};

class EmbedderGraphEntriesAllocator : public HeapEntriesAllocator {
 public:
  explicit EmbedderGraphEntriesAllocator(HeapSnapshot* snapshot)
      : snapshot_(snapshot),
        names_(snapshot_->profiler()->names()),
        heap_object_map_(snapshot_->profiler()->heap_object_map()) {}
  HeapEntry* AllocateEntry(HeapThing ptr) override;

 private:
  HeapSnapshot* snapshot_;
  StringsStorage* names_;
  HeapObjectsMap* heap_object_map_;
};

namespace {

const char* EmbedderGraphNodeName(StringsStorage* names,
                                  EmbedderGraphImpl::Node* node) {
  const char* prefix = node->NamePrefix();
  return prefix ? names->GetFormatted("%s %s", prefix, node->Name())
                : names->GetCopy(node->Name());
}

HeapEntry::Type EmbedderGraphNodeType(EmbedderGraphImpl::Node* node) {
  return node->IsRootNode() ? HeapEntry::kSynthetic : HeapEntry::kNative;
}

// Merges the names of an embedder node and its wrapper node.
// If the wrapper node name contains a tag suffix (part after '/') then the
// result is the embedder node name concatenated with the tag suffix.
// Otherwise, the result is the embedder node name.
const char* MergeNames(StringsStorage* names, const char* embedder_name,
                       const char* wrapper_name) {
  const char* suffix = strchr(wrapper_name, '/');
  return suffix ? names->GetFormatted("%s %s", embedder_name, suffix)
                : embedder_name;
}

}  // anonymous namespace

HeapEntry* EmbedderGraphEntriesAllocator::AllocateEntry(HeapThing ptr) {
  EmbedderGraphImpl::Node* node =
      reinterpret_cast<EmbedderGraphImpl::Node*>(ptr);
  DCHECK(node->IsEmbedderNode());
  size_t size = node->SizeInBytes();
  Address lookup_address = reinterpret_cast<Address>(node->GetNativeObject());
  SnapshotObjectId id =
      (lookup_address) ? heap_object_map_->FindOrAddEntry(lookup_address, 0)
                       : static_cast<SnapshotObjectId>(
                             reinterpret_cast<uintptr_t>(node) << 1);
  auto* heap_entry = snapshot_->AddEntry(EmbedderGraphNodeType(node),
                                         EmbedderGraphNodeName(names_, node),
                                         id, static_cast<int>(size), 0);
  heap_entry->set_detachedness(node->GetDetachedness());
  return heap_entry;
}

NativeObjectsExplorer::NativeObjectsExplorer(
    HeapSnapshot* snapshot, SnapshottingProgressReportingInterface* progress)
    : isolate_(
          Isolate::FromHeap(snapshot->profiler()->heap_object_map()->heap())),
      snapshot_(snapshot),
      names_(snapshot_->profiler()->names()),
      heap_object_map_(snapshot_->profiler()->heap_object_map()),
      embedder_graph_entries_allocator_(
          new EmbedderGraphEntriesAllocator(snapshot)) {}

void NativeObjectsExplorer::MergeNodeIntoEntry(
    HeapEntry* entry, EmbedderGraph::Node* original_node,
    EmbedderGraph::Node* wrapper_node) {
  // The wrapper node may be an embedder node (for testing purposes) or a V8
  // node (production code).
  if (!wrapper_node->IsEmbedderNode()) {
    // For V8 nodes only we can add a lookup.
    EmbedderGraphImpl::V8NodeImpl* v8_node =
        static_cast<EmbedderGraphImpl::V8NodeImpl*>(wrapper_node);
    Object object = v8_node->GetObject();
    DCHECK(!object.IsSmi());
    if (original_node->GetNativeObject()) {
      HeapObject heap_object = HeapObject::cast(object);
      heap_object_map_->AddMergedNativeEntry(original_node->GetNativeObject(),
                                             heap_object.address());
      DCHECK_EQ(entry->id(), heap_object_map_->FindMergedNativeEntry(
                                 original_node->GetNativeObject()));
    }
  }
  entry->set_detachedness(original_node->GetDetachedness());
  entry->set_name(MergeNames(
      names_, EmbedderGraphNodeName(names_, original_node), entry->name()));
  entry->set_type(EmbedderGraphNodeType(original_node));
}

HeapEntry* NativeObjectsExplorer::EntryForEmbedderGraphNode(
    EmbedderGraphImpl::Node* node) {
  // Return the entry for the wrapper node if present.
  if (node->WrapperNode()) {
    node = node->WrapperNode();
  }
  // Node is EmbedderNode.
  if (node->IsEmbedderNode()) {
    return generator_->FindOrAddEntry(node,
                                      embedder_graph_entries_allocator_.get());
  }
  // Node is V8NodeImpl.
  Object object =
      static_cast<EmbedderGraphImpl::V8NodeImpl*>(node)->GetObject();
  if (object.IsSmi()) return nullptr;
  auto* entry = generator_->FindEntry(
      reinterpret_cast<void*>(Object::cast(object).ptr()));
  return entry;
}

bool NativeObjectsExplorer::IterateAndExtractReferences(
    HeapSnapshotGenerator* generator) {
  generator_ = generator;

  if (FLAG_heap_profiler_use_embedder_graph &&
      snapshot_->profiler()->HasBuildEmbedderGraphCallback()) {
    v8::HandleScope scope(reinterpret_cast<v8::Isolate*>(isolate_));
    DisallowGarbageCollection no_gc;
    EmbedderGraphImpl graph;
    snapshot_->profiler()->BuildEmbedderGraph(isolate_, &graph);
    for (const auto& node : graph.nodes()) {
      // Only add embedder nodes as V8 nodes have been added already by the
      // V8HeapExplorer.
      if (!node->IsEmbedderNode()) continue;

      if (auto* entry = EntryForEmbedderGraphNode(node.get())) {
        if (node->IsRootNode()) {
          snapshot_->root()->SetIndexedAutoIndexReference(
              HeapGraphEdge::kElement, entry);
        }
        if (node->WrapperNode()) {
          MergeNodeIntoEntry(entry, node.get(), node->WrapperNode());
        }
      }
    }
    // Fill edges of the graph.
    for (const auto& edge : graph.edges()) {
      // |from| and |to| can be nullptr if the corresponding node is a V8 node
      // pointing to a Smi.
      HeapEntry* from = EntryForEmbedderGraphNode(edge.from);
      if (!from) continue;
      HeapEntry* to = EntryForEmbedderGraphNode(edge.to);
      if (!to) continue;
      if (edge.name == nullptr) {
        from->SetIndexedAutoIndexReference(HeapGraphEdge::kElement, to);
      } else {
        from->SetNamedReference(HeapGraphEdge::kInternal,
                                names_->GetCopy(edge.name), to);
      }
    }
  }
  generator_ = nullptr;
  return true;
}

HeapSnapshotGenerator::HeapSnapshotGenerator(
    HeapSnapshot* snapshot,
    v8::ActivityControl* control,
    v8::HeapProfiler::ObjectNameResolver* resolver,
    Heap* heap)
    : snapshot_(snapshot),
      control_(control),
      v8_heap_explorer_(snapshot_, this, resolver),
      dom_explorer_(snapshot_, this),
      heap_(heap) {
}

namespace {
class V8_NODISCARD NullContextForSnapshotScope {
 public:
  explicit NullContextForSnapshotScope(Isolate* isolate)
      : isolate_(isolate), prev_(isolate->context()) {
    isolate_->set_context(Context());
  }
  ~NullContextForSnapshotScope() { isolate_->set_context(prev_); }

 private:
  Isolate* isolate_;
  Context prev_;
};
}  // namespace

bool HeapSnapshotGenerator::GenerateSnapshot() {
  Isolate* isolate = Isolate::FromHeap(heap_);
  base::Optional<HandleScope> handle_scope(base::in_place, isolate);
  v8_heap_explorer_.CollectGlobalObjectsTags();

  heap_->CollectAllAvailableGarbage(GarbageCollectionReason::kHeapProfiler);

  NullContextForSnapshotScope null_context_scope(isolate);
  SafepointScope scope(heap_);
  v8_heap_explorer_.MakeGlobalObjectTagMap(scope);
  handle_scope.reset();

#ifdef VERIFY_HEAP
  Heap* debug_heap = heap_;
  if (FLAG_verify_heap) {
    debug_heap->Verify();
  }
#endif

  InitProgressCounter();

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    debug_heap->Verify();
  }
#endif

  snapshot_->AddSyntheticRootEntries();

  if (!FillReferences()) return false;

  snapshot_->FillChildren();
  snapshot_->RememberLastJSObjectId();

  progress_counter_ = progress_total_;
  if (!ProgressReport(true)) return false;
  return true;
}

void HeapSnapshotGenerator::ProgressStep() {
  ++progress_counter_;
}

bool HeapSnapshotGenerator::ProgressReport(bool force) {
  const int kProgressReportGranularity = 10000;
  if (control_ != nullptr &&
      (force || progress_counter_ % kProgressReportGranularity == 0)) {
    return control_->ReportProgressValue(progress_counter_, progress_total_) ==
           v8::ActivityControl::kContinue;
  }
  return true;
}

void HeapSnapshotGenerator::InitProgressCounter() {
  if (control_ == nullptr) return;
  // The +1 ensures that intermediate ProgressReport calls will never signal
  // that the work is finished (i.e. progress_counter_ == progress_total_).
  // Only the forced ProgressReport() at the end of GenerateSnapshot()
  // should signal that the work is finished because signalling finished twice
  // breaks the DevTools frontend.
  progress_total_ = v8_heap_explorer_.EstimateObjectsCount() + 1;
  progress_counter_ = 0;
}

bool HeapSnapshotGenerator::FillReferences() {
  return v8_heap_explorer_.IterateAndExtractReferences(this) &&
         dom_explorer_.IterateAndExtractReferences(this);
}

template<int bytes> struct MaxDecimalDigitsIn;
template <>
struct MaxDecimalDigitsIn<1> {
  static const int kSigned = 3;
  static const int kUnsigned = 3;
};
template<> struct MaxDecimalDigitsIn<4> {
  static const int kSigned = 11;
  static const int kUnsigned = 10;
};
template<> struct MaxDecimalDigitsIn<8> {
  static const int kSigned = 20;
  static const int kUnsigned = 20;
};

class OutputStreamWriter {
 public:
  explicit OutputStreamWriter(v8::OutputStream* stream)
      : stream_(stream),
        chunk_size_(stream->GetChunkSize()),
        chunk_(chunk_size_),
        chunk_pos_(0),
        aborted_(false) {
    DCHECK_GT(chunk_size_, 0);
  }
  bool aborted() { return aborted_; }
  void AddCharacter(char c) {
    DCHECK_NE(c, '\0');
    DCHECK(chunk_pos_ < chunk_size_);
    chunk_[chunk_pos_++] = c;
    MaybeWriteChunk();
  }
  void AddString(const char* s) {
    size_t len = strlen(s);
    DCHECK_GE(kMaxInt, len);
    AddSubstring(s, static_cast<int>(len));
  }
  void AddSubstring(const char* s, int n) {
    if (n <= 0) return;
    DCHECK_LE(n, strlen(s));
    const char* s_end = s + n;
    while (s < s_end) {
      int s_chunk_size =
          std::min(chunk_size_ - chunk_pos_, static_cast<int>(s_end - s));
      DCHECK_GT(s_chunk_size, 0);
      MemCopy(chunk_.begin() + chunk_pos_, s, s_chunk_size);
      s += s_chunk_size;
      chunk_pos_ += s_chunk_size;
      MaybeWriteChunk();
    }
  }
  void AddNumber(unsigned n) { AddNumberImpl<unsigned>(n, "%u"); }
  void Finalize() {
    if (aborted_) return;
    DCHECK(chunk_pos_ < chunk_size_);
    if (chunk_pos_ != 0) {
      WriteChunk();
    }
    stream_->EndOfStream();
  }

 private:
  template<typename T>
  void AddNumberImpl(T n, const char* format) {
    // Buffer for the longest value plus trailing \0
    static const int kMaxNumberSize =
        MaxDecimalDigitsIn<sizeof(T)>::kUnsigned + 1;
    if (chunk_size_ - chunk_pos_ >= kMaxNumberSize) {
      int result = SNPrintF(
          chunk_.SubVector(chunk_pos_, chunk_size_), format, n);
      DCHECK_NE(result, -1);
      chunk_pos_ += result;
      MaybeWriteChunk();
    } else {
      EmbeddedVector<char, kMaxNumberSize> buffer;
      int result = SNPrintF(buffer, format, n);
      USE(result);
      DCHECK_NE(result, -1);
      AddString(buffer.begin());
    }
  }
  void MaybeWriteChunk() {
    DCHECK(chunk_pos_ <= chunk_size_);
    if (chunk_pos_ == chunk_size_) {
      WriteChunk();
    }
  }
  void WriteChunk() {
    if (aborted_) return;
    if (stream_->WriteAsciiChunk(chunk_.begin(), chunk_pos_) ==
        v8::OutputStream::kAbort)
      aborted_ = true;
    chunk_pos_ = 0;
  }

  v8::OutputStream* stream_;
  int chunk_size_;
  ScopedVector<char> chunk_;
  int chunk_pos_;
  bool aborted_;
};


// type, name|index, to_node.
const int HeapSnapshotJSONSerializer::kEdgeFieldsCount = 3;
// type, name, id, self_size, edge_count, trace_node_id, detachedness.
const int HeapSnapshotJSONSerializer::kNodeFieldsCount = 7;

void HeapSnapshotJSONSerializer::Serialize(v8::OutputStream* stream) {
  if (AllocationTracker* allocation_tracker =
      snapshot_->profiler()->allocation_tracker()) {
    allocation_tracker->PrepareForSerialization();
  }
  DCHECK_NULL(writer_);
  writer_ = new OutputStreamWriter(stream);
  SerializeImpl();
  delete writer_;
  writer_ = nullptr;
}


void HeapSnapshotJSONSerializer::SerializeImpl() {
  DCHECK_EQ(0, snapshot_->root()->index());
  writer_->AddCharacter('{');
  writer_->AddString("\"snapshot\":{");
  SerializeSnapshot();
  if (writer_->aborted()) return;
  writer_->AddString("},\n");
  writer_->AddString("\"nodes\":[");
  SerializeNodes();
  if (writer_->aborted()) return;
  writer_->AddString("],\n");
  writer_->AddString("\"edges\":[");
  SerializeEdges();
  if (writer_->aborted()) return;
  writer_->AddString("],\n");

  writer_->AddString("\"trace_function_infos\":[");
  SerializeTraceNodeInfos();
  if (writer_->aborted()) return;
  writer_->AddString("],\n");
  writer_->AddString("\"trace_tree\":[");
  SerializeTraceTree();
  if (writer_->aborted()) return;
  writer_->AddString("],\n");

  writer_->AddString("\"samples\":[");
  SerializeSamples();
  if (writer_->aborted()) return;
  writer_->AddString("],\n");

  writer_->AddString("\"locations\":[");
  SerializeLocations();
  if (writer_->aborted()) return;
  writer_->AddString("],\n");

  writer_->AddString("\"strings\":[");
  SerializeStrings();
  if (writer_->aborted()) return;
  writer_->AddCharacter(']');
  writer_->AddCharacter('}');
  writer_->Finalize();
}


int HeapSnapshotJSONSerializer::GetStringId(const char* s) {
  base::HashMap::Entry* cache_entry =
      strings_.LookupOrInsert(const_cast<char*>(s), StringHash(s));
  if (cache_entry->value == nullptr) {
    cache_entry->value = reinterpret_cast<void*>(next_string_id_++);
  }
  return static_cast<int>(reinterpret_cast<intptr_t>(cache_entry->value));
}


namespace {

template<size_t size> struct ToUnsigned;

template <>
struct ToUnsigned<1> {
  using Type = uint8_t;
};

template<> struct ToUnsigned<4> {
  using Type = uint32_t;
};

template<> struct ToUnsigned<8> {
  using Type = uint64_t;
};

}  // namespace


template<typename T>
static int utoa_impl(T value, const Vector<char>& buffer, int buffer_pos) {
  STATIC_ASSERT(static_cast<T>(-1) > 0);  // Check that T is unsigned
  int number_of_digits = 0;
  T t = value;
  do {
    ++number_of_digits;
  } while (t /= 10);

  buffer_pos += number_of_digits;
  int result = buffer_pos;
  do {
    int last_digit = static_cast<int>(value % 10);
    buffer[--buffer_pos] = '0' + last_digit;
    value /= 10;
  } while (value);
  return result;
}


template<typename T>
static int utoa(T value, const Vector<char>& buffer, int buffer_pos) {
  typename ToUnsigned<sizeof(value)>::Type unsigned_value = value;
  STATIC_ASSERT(sizeof(value) == sizeof(unsigned_value));
  return utoa_impl(unsigned_value, buffer, buffer_pos);
}


void HeapSnapshotJSONSerializer::SerializeEdge(HeapGraphEdge* edge,
                                               bool first_edge) {
  // The buffer needs space for 3 unsigned ints, 3 commas, \n and \0
  static const int kBufferSize =
      MaxDecimalDigitsIn<sizeof(unsigned)>::kUnsigned * 3 + 3 + 2;  // NOLINT
  EmbeddedVector<char, kBufferSize> buffer;
  int edge_name_or_index = edge->type() == HeapGraphEdge::kElement
      || edge->type() == HeapGraphEdge::kHidden
      ? edge->index() : GetStringId(edge->name());
  int buffer_pos = 0;
  if (!first_edge) {
    buffer[buffer_pos++] = ',';
  }
  buffer_pos = utoa(edge->type(), buffer, buffer_pos);
  buffer[buffer_pos++] = ',';
  buffer_pos = utoa(edge_name_or_index, buffer, buffer_pos);
  buffer[buffer_pos++] = ',';
  buffer_pos = utoa(to_node_index(edge->to()), buffer, buffer_pos);
  buffer[buffer_pos++] = '\n';
  buffer[buffer_pos++] = '\0';
  writer_->AddString(buffer.begin());
}

void HeapSnapshotJSONSerializer::SerializeEdges() {
  std::vector<HeapGraphEdge*>& edges = snapshot_->children();
  for (size_t i = 0; i < edges.size(); ++i) {
    DCHECK(i == 0 ||
           edges[i - 1]->from()->index() <= edges[i]->from()->index());
    SerializeEdge(edges[i], i == 0);
    if (writer_->aborted()) return;
  }
}

void HeapSnapshotJSONSerializer::SerializeNode(const HeapEntry* entry) {
  // The buffer needs space for 5 unsigned ints, 1 size_t, 1 uint8_t, 7 commas,
  // \n and \0
  static const int kBufferSize =
      5 * MaxDecimalDigitsIn<sizeof(unsigned)>::kUnsigned  // NOLINT
      + MaxDecimalDigitsIn<sizeof(size_t)>::kUnsigned      // NOLINT
      + MaxDecimalDigitsIn<sizeof(uint8_t)>::kUnsigned + 7 + 1 + 1;
  EmbeddedVector<char, kBufferSize> buffer;
  int buffer_pos = 0;
  if (to_node_index(entry) != 0) {
    buffer[buffer_pos++] = ',';
  }
  buffer_pos = utoa(entry->type(), buffer, buffer_pos);
  buffer[buffer_pos++] = ',';
  buffer_pos = utoa(GetStringId(entry->name()), buffer, buffer_pos);
  buffer[buffer_pos++] = ',';
  buffer_pos = utoa(entry->id(), buffer, buffer_pos);
  buffer[buffer_pos++] = ',';
  buffer_pos = utoa(entry->self_size(), buffer, buffer_pos);
  buffer[buffer_pos++] = ',';
  buffer_pos = utoa(entry->children_count(), buffer, buffer_pos);
  buffer[buffer_pos++] = ',';
  buffer_pos = utoa(entry->trace_node_id(), buffer, buffer_pos);
  buffer[buffer_pos++] = ',';
  buffer_pos = utoa(entry->detachedness(), buffer, buffer_pos);
  buffer[buffer_pos++] = '\n';
  buffer[buffer_pos++] = '\0';
  writer_->AddString(buffer.begin());
}

void HeapSnapshotJSONSerializer::SerializeNodes() {
  const std::deque<HeapEntry>& entries = snapshot_->entries();
  for (const HeapEntry& entry : entries) {
    SerializeNode(&entry);
    if (writer_->aborted()) return;
  }
}

void HeapSnapshotJSONSerializer::SerializeSnapshot() {
  writer_->AddString("\"meta\":");
  // The object describing node serialization layout.
  // We use a set of macros to improve readability.

// clang-format off
#define JSON_A(s) "[" s "]"
#define JSON_O(s) "{" s "}"
#define JSON_S(s) "\"" s "\""
  writer_->AddString(JSON_O(
    JSON_S("node_fields") ":" JSON_A(
        JSON_S("type") ","
        JSON_S("name") ","
        JSON_S("id") ","
        JSON_S("self_size") ","
        JSON_S("edge_count") ","
        JSON_S("trace_node_id") ","
        JSON_S("detachedness")) ","
    JSON_S("node_types") ":" JSON_A(
        JSON_A(
            JSON_S("hidden") ","
            JSON_S("array") ","
            JSON_S("string") ","
            JSON_S("object") ","
            JSON_S("code") ","
            JSON_S("closure") ","
            JSON_S("regexp") ","
            JSON_S("number") ","
            JSON_S("native") ","
            JSON_S("synthetic") ","
            JSON_S("concatenated string") ","
            JSON_S("sliced string") ","
            JSON_S("symbol") ","
            JSON_S("bigint")) ","
        JSON_S("string") ","
        JSON_S("number") ","
        JSON_S("number") ","
        JSON_S("number") ","
        JSON_S("number") ","
        JSON_S("number")) ","
    JSON_S("edge_fields") ":" JSON_A(
        JSON_S("type") ","
        JSON_S("name_or_index") ","
        JSON_S("to_node")) ","
    JSON_S("edge_types") ":" JSON_A(
        JSON_A(
            JSON_S("context") ","
            JSON_S("element") ","
            JSON_S("property") ","
            JSON_S("internal") ","
            JSON_S("hidden") ","
            JSON_S("shortcut") ","
            JSON_S("weak")) ","
        JSON_S("string_or_number") ","
        JSON_S("node")) ","
    JSON_S("trace_function_info_fields") ":" JSON_A(
        JSON_S("function_id") ","
        JSON_S("name") ","
        JSON_S("script_name") ","
        JSON_S("script_id") ","
        JSON_S("line") ","
        JSON_S("column")) ","
    JSON_S("trace_node_fields") ":" JSON_A(
        JSON_S("id") ","
        JSON_S("function_info_index") ","
        JSON_S("count") ","
        JSON_S("size") ","
        JSON_S("children")) ","
    JSON_S("sample_fields") ":" JSON_A(
        JSON_S("timestamp_us") ","
        JSON_S("last_assigned_id")) ","
    JSON_S("location_fields") ":" JSON_A(
        JSON_S("object_index") ","
        JSON_S("script_id") ","
        JSON_S("line") ","
        JSON_S("column"))));
// clang-format on
#undef JSON_S
#undef JSON_O
#undef JSON_A
  writer_->AddString(",\"node_count\":");
  writer_->AddNumber(static_cast<unsigned>(snapshot_->entries().size()));
  writer_->AddString(",\"edge_count\":");
  writer_->AddNumber(static_cast<double>(snapshot_->edges().size()));
  writer_->AddString(",\"trace_function_count\":");
  uint32_t count = 0;
  AllocationTracker* tracker = snapshot_->profiler()->allocation_tracker();
  if (tracker) {
    count = static_cast<uint32_t>(tracker->function_info_list().size());
  }
  writer_->AddNumber(count);
}


static void WriteUChar(OutputStreamWriter* w, unibrow::uchar u) {
  static const char hex_chars[] = "0123456789ABCDEF";
  w->AddString("\\u");
  w->AddCharacter(hex_chars[(u >> 12) & 0xF]);
  w->AddCharacter(hex_chars[(u >> 8) & 0xF]);
  w->AddCharacter(hex_chars[(u >> 4) & 0xF]);
  w->AddCharacter(hex_chars[u & 0xF]);
}


void HeapSnapshotJSONSerializer::SerializeTraceTree() {
  AllocationTracker* tracker = snapshot_->profiler()->allocation_tracker();
  if (!tracker) return;
  AllocationTraceTree* traces = tracker->trace_tree();
  SerializeTraceNode(traces->root());
}


void HeapSnapshotJSONSerializer::SerializeTraceNode(AllocationTraceNode* node) {
  // The buffer needs space for 4 unsigned ints, 4 commas, [ and \0
  const int kBufferSize =
      4 * MaxDecimalDigitsIn<sizeof(unsigned)>::kUnsigned  // NOLINT
      + 4 + 1 + 1;
  EmbeddedVector<char, kBufferSize> buffer;
  int buffer_pos = 0;
  buffer_pos = utoa(node->id(), buffer, buffer_pos);
  buffer[buffer_pos++] = ',';
  buffer_pos = utoa(node->function_info_index(), buffer, buffer_pos);
  buffer[buffer_pos++] = ',';
  buffer_pos = utoa(node->allocation_count(), buffer, buffer_pos);
  buffer[buffer_pos++] = ',';
  buffer_pos = utoa(node->allocation_size(), buffer, buffer_pos);
  buffer[buffer_pos++] = ',';
  buffer[buffer_pos++] = '[';
  buffer[buffer_pos++] = '\0';
  writer_->AddString(buffer.begin());

  int i = 0;
  for (AllocationTraceNode* child : node->children()) {
    if (i++ > 0) {
      writer_->AddCharacter(',');
    }
    SerializeTraceNode(child);
  }
  writer_->AddCharacter(']');
}


// 0-based position is converted to 1-based during the serialization.
static int SerializePosition(int position, const Vector<char>& buffer,
                             int buffer_pos) {
  if (position == -1) {
    buffer[buffer_pos++] = '0';
  } else {
    DCHECK_GE(position, 0);
    buffer_pos = utoa(static_cast<unsigned>(position + 1), buffer, buffer_pos);
  }
  return buffer_pos;
}


void HeapSnapshotJSONSerializer::SerializeTraceNodeInfos() {
  AllocationTracker* tracker = snapshot_->profiler()->allocation_tracker();
  if (!tracker) return;
  // The buffer needs space for 6 unsigned ints, 6 commas, \n and \0
  const int kBufferSize =
      6 * MaxDecimalDigitsIn<sizeof(unsigned)>::kUnsigned  // NOLINT
      + 6 + 1 + 1;
  EmbeddedVector<char, kBufferSize> buffer;
  int i = 0;
  for (AllocationTracker::FunctionInfo* info : tracker->function_info_list()) {
    int buffer_pos = 0;
    if (i++ > 0) {
      buffer[buffer_pos++] = ',';
    }
    buffer_pos = utoa(info->function_id, buffer, buffer_pos);
    buffer[buffer_pos++] = ',';
    buffer_pos = utoa(GetStringId(info->name), buffer, buffer_pos);
    buffer[buffer_pos++] = ',';
    buffer_pos = utoa(GetStringId(info->script_name), buffer, buffer_pos);
    buffer[buffer_pos++] = ',';
    // The cast is safe because script id is a non-negative Smi.
    buffer_pos = utoa(static_cast<unsigned>(info->script_id), buffer,
        buffer_pos);
    buffer[buffer_pos++] = ',';
    buffer_pos = SerializePosition(info->line, buffer, buffer_pos);
    buffer[buffer_pos++] = ',';
    buffer_pos = SerializePosition(info->column, buffer, buffer_pos);
    buffer[buffer_pos++] = '\n';
    buffer[buffer_pos++] = '\0';
    writer_->AddString(buffer.begin());
  }
}


void HeapSnapshotJSONSerializer::SerializeSamples() {
  const std::vector<HeapObjectsMap::TimeInterval>& samples =
      snapshot_->profiler()->heap_object_map()->samples();
  if (samples.empty()) return;
  base::TimeTicks start_time = samples[0].timestamp;
  // The buffer needs space for 2 unsigned ints, 2 commas, \n and \0
  const int kBufferSize = MaxDecimalDigitsIn<sizeof(
                              base::TimeDelta().InMicroseconds())>::kUnsigned +
                          MaxDecimalDigitsIn<sizeof(samples[0].id)>::kUnsigned +
                          2 + 1 + 1;
  EmbeddedVector<char, kBufferSize> buffer;
  int i = 0;
  for (const HeapObjectsMap::TimeInterval& sample : samples) {
    int buffer_pos = 0;
    if (i++ > 0) {
      buffer[buffer_pos++] = ',';
    }
    base::TimeDelta time_delta = sample.timestamp - start_time;
    buffer_pos = utoa(time_delta.InMicroseconds(), buffer, buffer_pos);
    buffer[buffer_pos++] = ',';
    buffer_pos = utoa(sample.last_assigned_id(), buffer, buffer_pos);
    buffer[buffer_pos++] = '\n';
    buffer[buffer_pos++] = '\0';
    writer_->AddString(buffer.begin());
  }
}


void HeapSnapshotJSONSerializer::SerializeString(const unsigned char* s) {
  writer_->AddCharacter('\n');
  writer_->AddCharacter('\"');
  for ( ; *s != '\0'; ++s) {
    switch (*s) {
      case '\b':
        writer_->AddString("\\b");
        continue;
      case '\f':
        writer_->AddString("\\f");
        continue;
      case '\n':
        writer_->AddString("\\n");
        continue;
      case '\r':
        writer_->AddString("\\r");
        continue;
      case '\t':
        writer_->AddString("\\t");
        continue;
      case '\"':
      case '\\':
        writer_->AddCharacter('\\');
        writer_->AddCharacter(*s);
        continue;
      default:
        if (*s > 31 && *s < 128) {
          writer_->AddCharacter(*s);
        } else if (*s <= 31) {
          // Special character with no dedicated literal.
          WriteUChar(writer_, *s);
        } else {
          // Convert UTF-8 into \u UTF-16 literal.
          size_t length = 1, cursor = 0;
          for ( ; length <= 4 && *(s + length) != '\0'; ++length) { }
          unibrow::uchar c = unibrow::Utf8::CalculateValue(s, length, &cursor);
          if (c != unibrow::Utf8::kBadChar) {
            WriteUChar(writer_, c);
            DCHECK_NE(cursor, 0);
            s += cursor - 1;
          } else {
            writer_->AddCharacter('?');
          }
        }
    }
  }
  writer_->AddCharacter('\"');
}


void HeapSnapshotJSONSerializer::SerializeStrings() {
  ScopedVector<const unsigned char*> sorted_strings(
      strings_.occupancy() + 1);
  for (base::HashMap::Entry* entry = strings_.Start(); entry != nullptr;
       entry = strings_.Next(entry)) {
    int index = static_cast<int>(reinterpret_cast<uintptr_t>(entry->value));
    sorted_strings[index] = reinterpret_cast<const unsigned char*>(entry->key);
  }
  writer_->AddString("\"<dummy>\"");
  for (int i = 1; i < sorted_strings.length(); ++i) {
    writer_->AddCharacter(',');
    SerializeString(sorted_strings[i]);
    if (writer_->aborted()) return;
  }
}

void HeapSnapshotJSONSerializer::SerializeLocation(
    const SourceLocation& location) {
  // The buffer needs space for 4 unsigned ints, 3 commas, \n and \0
  static const int kBufferSize =
      MaxDecimalDigitsIn<sizeof(unsigned)>::kUnsigned * 4 + 3 + 2;
  EmbeddedVector<char, kBufferSize> buffer;
  int buffer_pos = 0;
  buffer_pos = utoa(to_node_index(location.entry_index), buffer, buffer_pos);
  buffer[buffer_pos++] = ',';
  buffer_pos = utoa(location.scriptId, buffer, buffer_pos);
  buffer[buffer_pos++] = ',';
  buffer_pos = utoa(location.line, buffer, buffer_pos);
  buffer[buffer_pos++] = ',';
  buffer_pos = utoa(location.col, buffer, buffer_pos);
  buffer[buffer_pos++] = '\n';
  buffer[buffer_pos++] = '\0';
  writer_->AddString(buffer.begin());
}

void HeapSnapshotJSONSerializer::SerializeLocations() {
  const std::vector<SourceLocation>& locations = snapshot_->locations();
  for (size_t i = 0; i < locations.size(); i++) {
    if (i > 0) writer_->AddCharacter(',');
    SerializeLocation(locations[i]);
    if (writer_->aborted()) return;
  }
}

}  // namespace internal
}  // namespace v8
