// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_SNAPSHOT_GENERATOR_H_
#define V8_HEAP_SNAPSHOT_GENERATOR_H_

#include "src/profile-generator-inl.h"

namespace v8 {
namespace internal {

class AllocationTracker;
class AllocationTraceNode;
class HeapEntry;
class HeapSnapshot;
class SnapshotFiller;

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
    DCHECK(type_ == kElement || type_ == kHidden);
    return index_;
  }
  const char* name() const {
    DCHECK(type_ == kContextVariable
        || type_ == kProperty
        || type_ == kInternal
        || type_ == kShortcut
        || type_ == kWeak);
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
    kSlicedString = v8::HeapGraphNode::kSlicedString,
    kSymbol = v8::HeapGraphNode::kSymbol
  };
  static const int kNoEntry;

  HeapEntry() { }
  HeapEntry(HeapSnapshot* snapshot,
            Type type,
            const char* name,
            SnapshotObjectId id,
            size_t self_size,
            unsigned trace_node_id);

  HeapSnapshot* snapshot() { return snapshot_; }
  Type type() { return static_cast<Type>(type_); }
  const char* name() { return name_; }
  void set_name(const char* name) { name_ = name; }
  SnapshotObjectId id() { return id_; }
  size_t self_size() { return self_size_; }
  unsigned trace_node_id() const { return trace_node_id_; }
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

 private:
  INLINE(HeapGraphEdge** children_arr());
  const char* TypeAsString();

  unsigned type_: 4;
  int children_count_: 28;
  int children_index_;
  size_t self_size_;
  HeapSnapshot* snapshot_;
  const char* name_;
  SnapshotObjectId id_;
  // id of allocation stack trace top node
  unsigned trace_node_id_;
};


// HeapSnapshot represents a single heap snapshot. It is stored in
// HeapProfiler, which is also a factory for
// HeapSnapshots. All HeapSnapshots share strings copied from JS heap
// to be able to return them even if they were collected.
// HeapSnapshotGenerator fills in a HeapSnapshot.
class HeapSnapshot {
 public:
  HeapSnapshot(HeapProfiler* profiler,
               const char* title,
               unsigned uid);
  void Delete();

  HeapProfiler* profiler() { return profiler_; }
  const char* title() { return title_; }
  unsigned uid() { return uid_; }
  size_t RawSnapshotSize() const;
  HeapEntry* root() { return &entries_[root_index_]; }
  HeapEntry* gc_roots() { return &entries_[gc_roots_index_]; }
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
                      size_t size,
                      unsigned trace_node_id);
  void AddSyntheticRootEntries();
  HeapEntry* GetEntryById(SnapshotObjectId id);
  List<HeapEntry*>* GetSortedEntriesList();
  void FillChildren();

  void Print(int max_depth);
  void PrintEntriesSize();

 private:
  HeapEntry* AddRootEntry();
  HeapEntry* AddGcRootsEntry();
  HeapEntry* AddGcSubrootEntry(int tag, SnapshotObjectId id);

  HeapProfiler* profiler_;
  const char* title_;
  unsigned uid_;
  int root_index_;
  int gc_roots_index_;
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

  SnapshotObjectId FindEntry(Address addr);
  SnapshotObjectId FindOrAddEntry(Address addr,
                                  unsigned int size,
                                  bool accessed = true);
  bool MoveObject(Address from, Address to, int size);
  void UpdateObjectSize(Address addr, int size);
  SnapshotObjectId last_assigned_id() const {
    return next_id_ - kObjectIdStep;
  }

  void StopHeapObjectsTracking();
  SnapshotObjectId PushHeapObjectsStats(OutputStream* stream);
  size_t GetUsedMemorySize() const;

  SnapshotObjectId GenerateId(v8::RetainedObjectInfo* info);

  static const int kObjectIdStep = 2;
  static const SnapshotObjectId kInternalRootObjectId;
  static const SnapshotObjectId kGcRootsObjectId;
  static const SnapshotObjectId kGcRootsFirstSubrootId;
  static const SnapshotObjectId kFirstAvailableObjectId;

  int FindUntrackedObjects();

  void UpdateHeapObjectsMap();
  void RemoveDeadEntries();

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

  SnapshotObjectId next_id_;
  HashMap entries_map_;
  List<EntryInfo> entries_;
  List<TimeInterval> time_intervals_;
  Heap* heap_;

  DISALLOW_COPY_AND_ASSIGN(HeapObjectsMap);
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
  void AddRootEntries(SnapshotFiller* filler);
  int EstimateObjectsCount(HeapIterator* iterator);
  bool IterateAndExtractReferences(SnapshotFiller* filler);
  void TagGlobalObjects();
  void TagCodeObject(Code* code);
  void TagBuiltinCodeObject(Code* code, const char* name);
  HeapEntry* AddEntry(Address address,
                      HeapEntry::Type type,
                      const char* name,
                      size_t size);

  static String* GetConstructorName(JSObject* object);

 private:
  typedef bool (V8HeapExplorer::*ExtractReferencesMethod)(int entry,
                                                          HeapObject* object);

  HeapEntry* AddEntry(HeapObject* object);
  HeapEntry* AddEntry(HeapObject* object,
                      HeapEntry::Type type,
                      const char* name);

  const char* GetSystemEntryName(HeapObject* object);

  template<V8HeapExplorer::ExtractReferencesMethod extractor>
  bool IterateAndExtractSinglePass();

  bool ExtractReferencesPass1(int entry, HeapObject* obj);
  bool ExtractReferencesPass2(int entry, HeapObject* obj);
  void ExtractJSGlobalProxyReferences(int entry, JSGlobalProxy* proxy);
  void ExtractJSObjectReferences(int entry, JSObject* js_obj);
  void ExtractStringReferences(int entry, String* obj);
  void ExtractSymbolReferences(int entry, Symbol* symbol);
  void ExtractJSCollectionReferences(int entry, JSCollection* collection);
  void ExtractJSWeakCollectionReferences(int entry,
                                         JSWeakCollection* collection);
  void ExtractContextReferences(int entry, Context* context);
  void ExtractMapReferences(int entry, Map* map);
  void ExtractSharedFunctionInfoReferences(int entry,
                                           SharedFunctionInfo* shared);
  void ExtractScriptReferences(int entry, Script* script);
  void ExtractAccessorInfoReferences(int entry, AccessorInfo* accessor_info);
  void ExtractAccessorPairReferences(int entry, AccessorPair* accessors);
  void ExtractCodeCacheReferences(int entry, CodeCache* code_cache);
  void ExtractCodeReferences(int entry, Code* code);
  void ExtractBoxReferences(int entry, Box* box);
  void ExtractCellReferences(int entry, Cell* cell);
  void ExtractPropertyCellReferences(int entry, PropertyCell* cell);
  void ExtractAllocationSiteReferences(int entry, AllocationSite* site);
  void ExtractJSArrayBufferReferences(int entry, JSArrayBuffer* buffer);
  void ExtractFixedArrayReferences(int entry, FixedArray* array);
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
                        const char* reference_name,
                        Object* child_obj,
                        int field_offset);
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
  void MarkAsWeakContainer(Object* object);

  HeapEntry* GetEntry(Object* obj);

  Heap* heap_;
  HeapSnapshot* snapshot_;
  StringsStorage* names_;
  HeapObjectsMap* heap_object_map_;
  SnapshottingProgressReportingInterface* progress_;
  SnapshotFiller* filler_;
  HeapObjectsSet objects_tags_;
  HeapObjectsSet strong_gc_subroot_names_;
  HeapObjectsSet user_roots_;
  HeapObjectsSet weak_containers_;
  v8::HeapProfiler::ObjectNameResolver* global_object_name_resolver_;

  friend class IndexedReferencesExtractor;
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
  void AddRootEntries(SnapshotFiller* filler);
  int EstimateObjectsCount();
  bool IterateAndExtractReferences(SnapshotFiller* filler);

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
  StringsStorage* names_;
  SnapshottingProgressReportingInterface* progress_;
  bool embedder_queried_;
  HeapObjectsSet in_groups_;
  // RetainedObjectInfo* -> List<HeapObject*>*
  HashMap objects_by_info_;
  HashMap native_groups_;
  HeapEntriesAllocator* synthetic_entries_allocator_;
  HeapEntriesAllocator* native_entries_allocator_;
  // Used during references extraction.
  SnapshotFiller* filler_;

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
        strings_(StringsMatch),
        next_node_id_(1),
        next_string_id_(1),
        writer_(NULL) {
  }
  void Serialize(v8::OutputStream* stream);

 private:
  INLINE(static bool StringsMatch(void* key1, void* key2)) {
    return strcmp(reinterpret_cast<char*>(key1),
                  reinterpret_cast<char*>(key2)) == 0;
  }

  INLINE(static uint32_t StringHash(const void* string)) {
    const char* s = reinterpret_cast<const char*>(string);
    int len = static_cast<int>(strlen(s));
    return StringHasher::HashSequentialString(
        s, len, v8::internal::kZeroHashSeed);
  }

  int GetStringId(const char* s);
  int entry_index(HeapEntry* e) { return e->index() * kNodeFieldsCount; }
  void SerializeEdge(HeapGraphEdge* edge, bool first_edge);
  void SerializeEdges();
  void SerializeImpl();
  void SerializeNode(HeapEntry* entry);
  void SerializeNodes();
  void SerializeSnapshot();
  void SerializeTraceTree();
  void SerializeTraceNode(AllocationTraceNode* node);
  void SerializeTraceNodeInfos();
  void SerializeString(const unsigned char* s);
  void SerializeStrings();

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
