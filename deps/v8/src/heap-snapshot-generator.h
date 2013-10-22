// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_HEAP_SNAPSHOT_GENERATOR_H_
#define V8_HEAP_SNAPSHOT_GENERATOR_H_

#include "profile-generator-inl.h"

namespace v8 {
namespace internal {

class HeapEntry;
class HeapSnapshot;

class HeapGraphEdge BASE_EMBEDDED {
 public:
  enum Type {
    kContextVariable = v8::HeapGraphEdge::kContextVariable,
    kElement = v8::HeapGraphEdge::kElement,
    kProperty = v8::HeapGraphEdge::kProperty,
    kInternal = v8::HeapGraphEdge::kInternal,
    kHidden = v8::HeapGraphEdge::kHidden,
    kShortcut = v8::HeapGraphEdge::kShortcut,
    kWeak = v8::HeapGraphEdge::kWeak
  };

  HeapGraphEdge() { }
  HeapGraphEdge(Type type, const char* name, int from, int to);
  HeapGraphEdge(Type type, int index, int from, int to);
  void ReplaceToIndexWithEntry(HeapSnapshot* snapshot);

  Type type() const { return static_cast<Type>(type_); }
  int index() const {
    ASSERT(type_ == kElement || type_ == kHidden || type_ == kWeak);
    return index_;
  }
  const char* name() const {
    ASSERT(type_ == kContextVariable
        || type_ == kProperty
        || type_ == kInternal
        || type_ == kShortcut);
    return name_;
  }
  INLINE(HeapEntry* from() const);
  HeapEntry* to() const { return to_entry_; }

 private:
  INLINE(HeapSnapshot* snapshot() const);

  unsigned type_ : 3;
  int from_index_ : 29;
  union {
    // During entries population |to_index_| is used for storing the index,
    // afterwards it is replaced with a pointer to the entry.
    int to_index_;
    HeapEntry* to_entry_;
  };
  union {
    int index_;
    const char* name_;
  };
};


// HeapEntry instances represent an entity from the heap (or a special
// virtual node, e.g. root).
class HeapEntry BASE_EMBEDDED {
 public:
  enum Type {
    kHidden = v8::HeapGraphNode::kHidden,
    kArray = v8::HeapGraphNode::kArray,
    kString = v8::HeapGraphNode::kString,
    kObject = v8::HeapGraphNode::kObject,
    kCode = v8::HeapGraphNode::kCode,
    kClosure = v8::HeapGraphNode::kClosure,
    kRegExp = v8::HeapGraphNode::kRegExp,
    kHeapNumber = v8::HeapGraphNode::kHeapNumber,
    kNative = v8::HeapGraphNode::kNative,
    kSynthetic = v8::HeapGraphNode::kSynthetic,
    kConsString = v8::HeapGraphNode::kConsString,
    kSlicedString = v8::HeapGraphNode::kSlicedString
  };
  static const int kNoEntry;

  HeapEntry() { }
  HeapEntry(HeapSnapshot* snapshot,
            Type type,
            const char* name,
            SnapshotObjectId id,
            int self_size);

  HeapSnapshot* snapshot() { return snapshot_; }
  Type type() { return static_cast<Type>(type_); }
  const char* name() { return name_; }
  void set_name(const char* name) { name_ = name; }
  inline SnapshotObjectId id() { return id_; }
  int self_size() { return self_size_; }
  INLINE(int index() const);
  int children_count() const { return children_count_; }
  INLINE(int set_children_index(int index));
  void add_child(HeapGraphEdge* edge) {
    children_arr()[children_count_++] = edge;
  }
  Vector<HeapGraphEdge*> children() {
    return Vector<HeapGraphEdge*>(children_arr(), children_count_); }

  void SetIndexedReference(
      HeapGraphEdge::Type type, int index, HeapEntry* entry);
  void SetNamedReference(
      HeapGraphEdge::Type type, const char* name, HeapEntry* entry);

  void Print(
      const char* prefix, const char* edge_name, int max_depth, int indent);

  Handle<HeapObject> GetHeapObject();

 private:
  INLINE(HeapGraphEdge** children_arr());
  const char* TypeAsString();

  unsigned type_: 4;
  int children_count_: 28;
  int children_index_;
  int self_size_;
  SnapshotObjectId id_;
  HeapSnapshot* snapshot_;
  const char* name_;
};


class HeapSnapshotsCollection;

// HeapSnapshot represents a single heap snapshot. It is stored in
// HeapSnapshotsCollection, which is also a factory for
// HeapSnapshots. All HeapSnapshots share strings copied from JS heap
// to be able to return them even if they were collected.
// HeapSnapshotGenerator fills in a HeapSnapshot.
class HeapSnapshot {
 public:
  HeapSnapshot(HeapSnapshotsCollection* collection,
               const char* title,
               unsigned uid);
  void Delete();

  HeapSnapshotsCollection* collection() { return collection_; }
  const char* title() { return title_; }
  unsigned uid() { return uid_; }
  size_t RawSnapshotSize() const;
  HeapEntry* root() { return &entries_[root_index_]; }
  HeapEntry* gc_roots() { return &entries_[gc_roots_index_]; }
  HeapEntry* natives_root() { return &entries_[natives_root_index_]; }
  HeapEntry* gc_subroot(int index) {
    return &entries_[gc_subroot_indexes_[index]];
  }
  List<HeapEntry>& entries() { return entries_; }
  List<HeapGraphEdge>& edges() { return edges_; }
  List<HeapGraphEdge*>& children() { return children_; }
  void RememberLastJSObjectId();
  SnapshotObjectId max_snapshot_js_object_id() const {
    return max_snapshot_js_object_id_;
  }

  HeapEntry* AddEntry(HeapEntry::Type type,
                      const char* name,
                      SnapshotObjectId id,
                      int size);
  HeapEntry* AddRootEntry();
  HeapEntry* AddGcRootsEntry();
  HeapEntry* AddGcSubrootEntry(int tag);
  HeapEntry* AddNativesRootEntry();
  HeapEntry* GetEntryById(SnapshotObjectId id);
  List<HeapEntry*>* GetSortedEntriesList();
  void FillChildren();

  void Print(int max_depth);
  void PrintEntriesSize();

 private:
  HeapSnapshotsCollection* collection_;
  const char* title_;
  unsigned uid_;
  int root_index_;
  int gc_roots_index_;
  int natives_root_index_;
  int gc_subroot_indexes_[VisitorSynchronization::kNumberOfSyncTags];
  List<HeapEntry> entries_;
  List<HeapGraphEdge> edges_;
  List<HeapGraphEdge*> children_;
  List<HeapEntry*> sorted_entries_;
  SnapshotObjectId max_snapshot_js_object_id_;

  friend class HeapSnapshotTester;

  DISALLOW_COPY_AND_ASSIGN(HeapSnapshot);
};


class HeapObjectsMap {
 public:
  explicit HeapObjectsMap(Heap* heap);

  Heap* heap() const { return heap_; }

  void SnapshotGenerationFinished();
  SnapshotObjectId FindEntry(Address addr);
  SnapshotObjectId FindOrAddEntry(Address addr, unsigned int size);
  void MoveObject(Address from, Address to);
  SnapshotObjectId last_assigned_id() const {
    return next_id_ - kObjectIdStep;
  }

  void StopHeapObjectsTracking();
  SnapshotObjectId PushHeapObjectsStats(OutputStream* stream);
  size_t GetUsedMemorySize() const;

  static SnapshotObjectId GenerateId(Heap* heap, v8::RetainedObjectInfo* info);
  static inline SnapshotObjectId GetNthGcSubrootId(int delta);

  static const int kObjectIdStep = 2;
  static const SnapshotObjectId kInternalRootObjectId;
  static const SnapshotObjectId kGcRootsObjectId;
  static const SnapshotObjectId kNativesRootObjectId;
  static const SnapshotObjectId kGcRootsFirstSubrootId;
  static const SnapshotObjectId kFirstAvailableObjectId;

 private:
  struct EntryInfo {
  EntryInfo(SnapshotObjectId id, Address addr, unsigned int size)
      : id(id), addr(addr), size(size), accessed(true) { }
  EntryInfo(SnapshotObjectId id, Address addr, unsigned int size, bool accessed)
      : id(id), addr(addr), size(size), accessed(accessed) { }
    SnapshotObjectId id;
    Address addr;
    unsigned int size;
    bool accessed;
  };
  struct TimeInterval {
    explicit TimeInterval(SnapshotObjectId id) : id(id), size(0), count(0) { }
    SnapshotObjectId id;
    uint32_t size;
    uint32_t count;
  };

  void UpdateHeapObjectsMap();
  void RemoveDeadEntries();

  SnapshotObjectId next_id_;
  HashMap entries_map_;
  List<EntryInfo> entries_;
  List<TimeInterval> time_intervals_;
  Heap* heap_;

  DISALLOW_COPY_AND_ASSIGN(HeapObjectsMap);
};


class HeapSnapshotsCollection {
 public:
  explicit HeapSnapshotsCollection(Heap* heap);
  ~HeapSnapshotsCollection();

  Heap* heap() const { return ids_.heap(); }

  bool is_tracking_objects() { return is_tracking_objects_; }
  SnapshotObjectId PushHeapObjectsStats(OutputStream* stream) {
    return ids_.PushHeapObjectsStats(stream);
  }
  void StartHeapObjectsTracking() { is_tracking_objects_ = true; }
  void StopHeapObjectsTracking() { ids_.StopHeapObjectsTracking(); }

  HeapSnapshot* NewSnapshot(const char* name, unsigned uid);
  void SnapshotGenerationFinished(HeapSnapshot* snapshot);
  List<HeapSnapshot*>* snapshots() { return &snapshots_; }
  void RemoveSnapshot(HeapSnapshot* snapshot);

  StringsStorage* names() { return &names_; }

  SnapshotObjectId FindObjectId(Address object_addr) {
    return ids_.FindEntry(object_addr);
  }
  SnapshotObjectId GetObjectId(Address object_addr, int object_size) {
    return ids_.FindOrAddEntry(object_addr, object_size);
  }
  Handle<HeapObject> FindHeapObjectById(SnapshotObjectId id);
  void ObjectMoveEvent(Address from, Address to) { ids_.MoveObject(from, to); }
  SnapshotObjectId last_assigned_id() const {
    return ids_.last_assigned_id();
  }
  size_t GetUsedMemorySize() const;

 private:
  bool is_tracking_objects_;  // Whether tracking object moves is needed.
  List<HeapSnapshot*> snapshots_;
  StringsStorage names_;
  // Mapping from HeapObject addresses to objects' uids.
  HeapObjectsMap ids_;

  DISALLOW_COPY_AND_ASSIGN(HeapSnapshotsCollection);
};


// A typedef for referencing anything that can be snapshotted living
// in any kind of heap memory.
typedef void* HeapThing;


// An interface that creates HeapEntries by HeapThings.
class HeapEntriesAllocator {
 public:
  virtual ~HeapEntriesAllocator() { }
  virtual HeapEntry* AllocateEntry(HeapThing ptr) = 0;
};


// The HeapEntriesMap instance is used to track a mapping between
// real heap objects and their representations in heap snapshots.
class HeapEntriesMap {
 public:
  HeapEntriesMap();

  int Map(HeapThing thing);
  void Pair(HeapThing thing, int entry);

 private:
  static uint32_t Hash(HeapThing thing) {
    return ComputeIntegerHash(
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(thing)),
        v8::internal::kZeroHashSeed);
  }
  static bool HeapThingsMatch(HeapThing key1, HeapThing key2) {
    return key1 == key2;
  }

  HashMap entries_;

  friend class HeapObjectsSet;

  DISALLOW_COPY_AND_ASSIGN(HeapEntriesMap);
};


class HeapObjectsSet {
 public:
  HeapObjectsSet();
  void Clear();
  bool Contains(Object* object);
  void Insert(Object* obj);
  const char* GetTag(Object* obj);
  void SetTag(Object* obj, const char* tag);
  bool is_empty() const { return entries_.occupancy() == 0; }

 private:
  HashMap entries_;

  DISALLOW_COPY_AND_ASSIGN(HeapObjectsSet);
};


// An interface used to populate a snapshot with nodes and edges.
class SnapshotFillerInterface {
 public:
  virtual ~SnapshotFillerInterface() { }
  virtual HeapEntry* AddEntry(HeapThing ptr,
                              HeapEntriesAllocator* allocator) = 0;
  virtual HeapEntry* FindEntry(HeapThing ptr) = 0;
  virtual HeapEntry* FindOrAddEntry(HeapThing ptr,
                                    HeapEntriesAllocator* allocator) = 0;
  virtual void SetIndexedReference(HeapGraphEdge::Type type,
                                   int parent_entry,
                                   int index,
                                   HeapEntry* child_entry) = 0;
  virtual void SetIndexedAutoIndexReference(HeapGraphEdge::Type type,
                                            int parent_entry,
                                            HeapEntry* child_entry) = 0;
  virtual void SetNamedReference(HeapGraphEdge::Type type,
                                 int parent_entry,
                                 const char* reference_name,
                                 HeapEntry* child_entry) = 0;
  virtual void SetNamedAutoIndexReference(HeapGraphEdge::Type type,
                                          int parent_entry,
                                          HeapEntry* child_entry) = 0;
};


class SnapshottingProgressReportingInterface {
 public:
  virtual ~SnapshottingProgressReportingInterface() { }
  virtual void ProgressStep() = 0;
  virtual bool ProgressReport(bool force) = 0;
};


// An implementation of V8 heap graph extractor.
class V8HeapExplorer : public HeapEntriesAllocator {
 public:
  V8HeapExplorer(HeapSnapshot* snapshot,
                 SnapshottingProgressReportingInterface* progress,
                 v8::HeapProfiler::ObjectNameResolver* resolver);
  virtual ~V8HeapExplorer();
  virtual HeapEntry* AllocateEntry(HeapThing ptr);
  void AddRootEntries(SnapshotFillerInterface* filler);
  int EstimateObjectsCount(HeapIterator* iterator);
  bool IterateAndExtractReferences(SnapshotFillerInterface* filler);
  void TagGlobalObjects();

  static String* GetConstructorName(JSObject* object);

  static HeapObject* const kInternalRootObject;

 private:
  HeapEntry* AddEntry(HeapObject* object);
  HeapEntry* AddEntry(HeapObject* object,
                      HeapEntry::Type type,
                      const char* name);
  const char* GetSystemEntryName(HeapObject* object);

  void ExtractReferences(HeapObject* obj);
  void ExtractJSGlobalProxyReferences(int entry, JSGlobalProxy* proxy);
  void ExtractJSObjectReferences(int entry, JSObject* js_obj);
  void ExtractStringReferences(int entry, String* obj);
  void ExtractContextReferences(int entry, Context* context);
  void ExtractMapReferences(int entry, Map* map);
  void ExtractSharedFunctionInfoReferences(int entry,
                                           SharedFunctionInfo* shared);
  void ExtractScriptReferences(int entry, Script* script);
  void ExtractAccessorPairReferences(int entry, AccessorPair* accessors);
  void ExtractCodeCacheReferences(int entry, CodeCache* code_cache);
  void ExtractCodeReferences(int entry, Code* code);
  void ExtractCellReferences(int entry, Cell* cell);
  void ExtractPropertyCellReferences(int entry, PropertyCell* cell);
  void ExtractAllocationSiteReferences(int entry, AllocationSite* site);
  void ExtractClosureReferences(JSObject* js_obj, int entry);
  void ExtractPropertyReferences(JSObject* js_obj, int entry);
  bool ExtractAccessorPairProperty(JSObject* js_obj, int entry,
                                   Object* key, Object* callback_obj);
  void ExtractElementReferences(JSObject* js_obj, int entry);
  void ExtractInternalReferences(JSObject* js_obj, int entry);
  bool IsEssentialObject(Object* object);
  void SetContextReference(HeapObject* parent_obj,
                           int parent,
                           String* reference_name,
                           Object* child,
                           int field_offset);
  void SetNativeBindReference(HeapObject* parent_obj,
                              int parent,
                              const char* reference_name,
                              Object* child);
  void SetElementReference(HeapObject* parent_obj,
                           int parent,
                           int index,
                           Object* child);
  void SetInternalReference(HeapObject* parent_obj,
                            int parent,
                            const char* reference_name,
                            Object* child,
                            int field_offset = -1);
  void SetInternalReference(HeapObject* parent_obj,
                            int parent,
                            int index,
                            Object* child,
                            int field_offset = -1);
  void SetHiddenReference(HeapObject* parent_obj,
                          int parent,
                          int index,
                          Object* child);
  void SetWeakReference(HeapObject* parent_obj,
                        int parent,
                        int index,
                        Object* child_obj,
                        int field_offset);
  void SetPropertyReference(HeapObject* parent_obj,
                            int parent,
                            Name* reference_name,
                            Object* child,
                            const char* name_format_string = NULL,
                            int field_offset = -1);
  void SetUserGlobalReference(Object* user_global);
  void SetRootGcRootsReference();
  void SetGcRootsReference(VisitorSynchronization::SyncTag tag);
  void SetGcSubrootReference(
      VisitorSynchronization::SyncTag tag, bool is_weak, Object* child);
  const char* GetStrongGcSubrootName(Object* object);
  void TagObject(Object* obj, const char* tag);

  HeapEntry* GetEntry(Object* obj);

  static inline HeapObject* GetNthGcSubrootObject(int delta);
  static inline int GetGcSubrootOrder(HeapObject* subroot);

  Heap* heap_;
  HeapSnapshot* snapshot_;
  HeapSnapshotsCollection* collection_;
  SnapshottingProgressReportingInterface* progress_;
  SnapshotFillerInterface* filler_;
  HeapObjectsSet objects_tags_;
  HeapObjectsSet strong_gc_subroot_names_;
  HeapObjectsSet user_roots_;
  v8::HeapProfiler::ObjectNameResolver* global_object_name_resolver_;

  static HeapObject* const kGcRootsObject;
  static HeapObject* const kFirstGcSubrootObject;
  static HeapObject* const kLastGcSubrootObject;

  friend class IndexedReferencesExtractor;
  friend class GcSubrootsEnumerator;
  friend class RootsReferencesExtractor;

  DISALLOW_COPY_AND_ASSIGN(V8HeapExplorer);
};


class NativeGroupRetainedObjectInfo;


// An implementation of retained native objects extractor.
class NativeObjectsExplorer {
 public:
  NativeObjectsExplorer(HeapSnapshot* snapshot,
                        SnapshottingProgressReportingInterface* progress);
  virtual ~NativeObjectsExplorer();
  void AddRootEntries(SnapshotFillerInterface* filler);
  int EstimateObjectsCount();
  bool IterateAndExtractReferences(SnapshotFillerInterface* filler);

 private:
  void FillRetainedObjects();
  void FillImplicitReferences();
  List<HeapObject*>* GetListMaybeDisposeInfo(v8::RetainedObjectInfo* info);
  void SetNativeRootReference(v8::RetainedObjectInfo* info);
  void SetRootNativeRootsReference();
  void SetWrapperNativeReferences(HeapObject* wrapper,
                                      v8::RetainedObjectInfo* info);
  void VisitSubtreeWrapper(Object** p, uint16_t class_id);

  static uint32_t InfoHash(v8::RetainedObjectInfo* info) {
    return ComputeIntegerHash(static_cast<uint32_t>(info->GetHash()),
                              v8::internal::kZeroHashSeed);
  }
  static bool RetainedInfosMatch(void* key1, void* key2) {
    return key1 == key2 ||
        (reinterpret_cast<v8::RetainedObjectInfo*>(key1))->IsEquivalent(
            reinterpret_cast<v8::RetainedObjectInfo*>(key2));
  }
  INLINE(static bool StringsMatch(void* key1, void* key2)) {
    return strcmp(reinterpret_cast<char*>(key1),
                  reinterpret_cast<char*>(key2)) == 0;
  }

  NativeGroupRetainedObjectInfo* FindOrAddGroupInfo(const char* label);

  Isolate* isolate_;
  HeapSnapshot* snapshot_;
  HeapSnapshotsCollection* collection_;
  SnapshottingProgressReportingInterface* progress_;
  bool embedder_queried_;
  HeapObjectsSet in_groups_;
  // RetainedObjectInfo* -> List<HeapObject*>*
  HashMap objects_by_info_;
  HashMap native_groups_;
  HeapEntriesAllocator* synthetic_entries_allocator_;
  HeapEntriesAllocator* native_entries_allocator_;
  // Used during references extraction.
  SnapshotFillerInterface* filler_;

  static HeapThing const kNativesRootObject;

  friend class GlobalHandlesExtractor;

  DISALLOW_COPY_AND_ASSIGN(NativeObjectsExplorer);
};


class HeapSnapshotGenerator : public SnapshottingProgressReportingInterface {
 public:
  HeapSnapshotGenerator(HeapSnapshot* snapshot,
                        v8::ActivityControl* control,
                        v8::HeapProfiler::ObjectNameResolver* resolver,
                        Heap* heap);
  bool GenerateSnapshot();

 private:
  bool FillReferences();
  void ProgressStep();
  bool ProgressReport(bool force = false);
  void SetProgressTotal(int iterations_count);

  HeapSnapshot* snapshot_;
  v8::ActivityControl* control_;
  V8HeapExplorer v8_heap_explorer_;
  NativeObjectsExplorer dom_explorer_;
  // Mapping from HeapThing pointers to HeapEntry* pointers.
  HeapEntriesMap entries_;
  // Used during snapshot generation.
  int progress_counter_;
  int progress_total_;
  Heap* heap_;

  DISALLOW_COPY_AND_ASSIGN(HeapSnapshotGenerator);
};

class OutputStreamWriter;

class HeapSnapshotJSONSerializer {
 public:
  explicit HeapSnapshotJSONSerializer(HeapSnapshot* snapshot)
      : snapshot_(snapshot),
        strings_(ObjectsMatch),
        next_node_id_(1),
        next_string_id_(1),
        writer_(NULL) {
  }
  void Serialize(v8::OutputStream* stream);

 private:
  INLINE(static bool ObjectsMatch(void* key1, void* key2)) {
    return key1 == key2;
  }

  INLINE(static uint32_t ObjectHash(const void* key)) {
    return ComputeIntegerHash(
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(key)),
        v8::internal::kZeroHashSeed);
  }

  int GetStringId(const char* s);
  int entry_index(HeapEntry* e) { return e->index() * kNodeFieldsCount; }
  void SerializeEdge(HeapGraphEdge* edge, bool first_edge);
  void SerializeEdges();
  void SerializeImpl();
  void SerializeNode(HeapEntry* entry);
  void SerializeNodes();
  void SerializeSnapshot();
  void SerializeString(const unsigned char* s);
  void SerializeStrings();
  void SortHashMap(HashMap* map, List<HashMap::Entry*>* sorted_entries);

  static const int kEdgeFieldsCount;
  static const int kNodeFieldsCount;

  HeapSnapshot* snapshot_;
  HashMap strings_;
  int next_node_id_;
  int next_string_id_;
  OutputStreamWriter* writer_;

  friend class HeapSnapshotJSONSerializerEnumerator;
  friend class HeapSnapshotJSONSerializerIterator;

  DISALLOW_COPY_AND_ASSIGN(HeapSnapshotJSONSerializer);
};


} }  // namespace v8::internal

#endif  // V8_HEAP_SNAPSHOT_GENERATOR_H_

