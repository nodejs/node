// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc-js/cpp-snapshot.h"

#include <memory>
#include <optional>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "include/cppgc/heap-consistency.h"
#include "include/cppgc/internal/name-trait.h"
#include "include/cppgc/trace-trait.h"
#include "include/cppgc/visitor.h"
#include "include/v8-cppgc.h"
#include "include/v8-internal.h"
#include "include/v8-profiler.h"
#include "src/api/api-inl.h"
#include "src/base/logging.h"
#include "src/execution/isolate.h"
#include "src/handles/traced-handles.h"
#include "src/heap/cppgc-internal/heap-object-header.h"
#include "src/heap/cppgc-internal/heap-visitor.h"
#include "src/heap/cppgc-internal/visitor.h"
#include "src/heap/cppgc-js/cpp-heap.h"
#include "src/heap/marking-worklist-inl.h"
#include "src/objects/cpp-heap-object-wrapper-inl.h"
#include "src/objects/js-objects.h"
#include "src/objects/objects-inl.h"
#include "src/profiler/heap-profiler.h"
#include "src/profiler/heap-snapshot-generator.h"

namespace v8 {
namespace internal {

class CppGraphBuilderImpl;
using cppgc::internal::HeapObjectHeader;

constexpr std::string_view kEphemeronEdgeName =
    "part of key -> value pair in ephemeron table";

class ParentScope final {
 public:
  enum class Type {
    kRegular,
    kRoot,
  };

  explicit ParentScope(HeapEntry* entry, Type type = Type::kRegular)
      : entry_(entry), type_(type) {
    DCHECK_NOT_NULL(entry_);
  }

  HeapEntry* entry() const { return entry_; }
  bool IsRoot() const { return type_ == Type::kRoot; }

 private:
  HeapEntry* entry_;
  Type type_;
};

void* ExtractEmbedderDataBackref(Isolate* isolate, CppHeap& cpp_heap,
                                 v8::Local<v8::Data> v8_value) {
  if (!(v8_value->IsValue() && v8_value.As<v8::Value>()->IsObject())) {
    return nullptr;
  }

  DirectHandle<Object> v8_object = Utils::OpenDirectHandle(*v8_value);
  if (!IsJSObject(*v8_object) ||
      !Cast<JSObject>(*v8_object)->MayHaveEmbedderFields()) {
    return nullptr;
  }

  Tagged<JSObject> js_object = Cast<JSObject>(*v8_object);
  // Not every object that can have embedder fields is actually a JSApiWrapper.
  if (!IsJSApiWrapperObject(js_object)) {
    return nullptr;
  }
  // Wrapper using cpp_heap_wrappable field.
  return CppHeapObjectWrapper(js_object).GetCppHeapWrappable(
      isolate, kAnyCppHeapPointer);
}

class CppGraphBuilderImpl final {
 public:
  struct WrapperInfo {
    HeapEntry* entry;
    Tagged<HeapObject> v8_object;
    v8::EmbedderGraph::Node::Detachedness detachedness;
  };

  CppGraphBuilderImpl(CppHeap& cpp_heap, HeapSnapshotGenerator* generator,
                      CppHeapWrapperSet&& cpp_heap_wrappers);

  void Run();

  void AddRootEdge(HeapEntry*, const HeapObjectHeader&, cppgc::SourceLocation);
  void RecordTracedReferenceOnStack(HeapEntry*, Tagged<HeapObject>);

  void RecordObjectReachableFromStack(const HeapObjectHeader&);
  void RecordWeakContainer(const HeapObjectHeader&, cppgc::TraceDescriptor);
  void RecordWrapper(const HeapObjectHeader* header, WrapperInfo info) {
    wrapper_map_.emplace(header, info);
  }

  const cppgc::TraceDescriptor* GetWeakContainerTraceDescriptor(
      const HeapObjectHeader& header) const {
    auto it = weak_containers_.find(&header);
    return it == weak_containers_.end() ? nullptr : &it->second;
  }

  bool IsReachableFromStack(const HeapObjectHeader& header) const {
    return objects_reachable_from_stack_.find(&header) !=
           objects_reachable_from_stack_.end();
  }

  void AddEdgeForCppHeapWrapper(Tagged<CppHeapPointerWrapperObjectT>);

  HeapEntry* AddRootNode(const char* name) {
    SnapshotObjectId id = heap_object_map_->get_next_id();
    HeapEntry* entry = snapshot_->AddEntry(HeapEntry::kSynthetic,
                                           names_->GetCopy(name), id, 0, 0);
    snapshot_->gc_roots()->SetIndexedAutoIndexReference(
        HeapGraphEdge::kElement, entry, generator_, HeapEntry::kOffHeapPointer);
    return entry;
  }

  HeapEntry* AddNode(const HeapObjectHeader& header) {
    DCHECK(pre_pass_done_);
    size_t size = header.AllocatedSize();
    Address lookup_address = reinterpret_cast<Address>(&header);
    SnapshotObjectId id = heap_object_map_->FindOrAddEntry(
        lookup_address, 0, HeapObjectsMap::MarkEntryAccessed::kYes,
        HeapObjectsMap::IsNativeObject::kYes);
    const char* name = header.GetName().value;
    if (strcmp(name, "Window") == 0) {
      name = "Window (cppgc)";
    }
    return snapshot_->AddEntry(HeapEntry::kNative, names_->GetCopy(name), id,
                               size, 0);
  }

  HeapEntry* GetOrCreateNode(const HeapObjectHeader& header) {
    DCHECK(pre_pass_done_);
    auto it = nodes_.find(&header);
    if (it != nodes_.end()) return it->second;

    HeapEntry* entry = nullptr;
    auto w_it = wrapper_map_.find(&header);
    if (w_it != wrapper_map_.end()) {
      entry = w_it->second.entry;
      const char* cpp_name = names_->GetCopy(header.GetName().value);
      entry->set_name(
          HeapSnapshotGenerator::MergeNames(names_, cpp_name, entry->name()));
      entry->set_type(HeapEntry::kNative);
      entry->add_self_size(header.AllocatedSize());
      entry->set_detachedness(w_it->second.detachedness);
      heap_object_map_->AddMergedNativeEntry(
          reinterpret_cast<NativeObject>(
              const_cast<HeapObjectHeader*>(&header)),
          w_it->second.v8_object.address());
    } else {
      entry = AddNode(header);
    }
    nodes_[&header] = entry;
    return entry;
  }

  void AddEdge(const ParentScope& parent, const HeapObjectHeader& header,
               std::string_view edge_name) {
    HeapEntry* child_entry = GetOrCreateNode(header);

    if (!edge_name.empty()) {
      parent.entry()->SetNamedReference(
          HeapGraphEdge::kInternal,
          names_->GetCopy(std::string(edge_name).c_str()), child_entry,
          generator_, HeapEntry::kOffHeapPointer);
    } else {
      parent.entry()->SetIndexedAutoIndexReference(HeapGraphEdge::kElement,
                                                   child_entry, generator_,
                                                   HeapEntry::kOffHeapPointer);
    }
  }

  void AddWeakEdge(const ParentScope& parent, const HeapObjectHeader& header,
                   std::string_view edge_name) {
    HeapEntry* child_entry = GetOrCreateNode(header);

    if (!edge_name.empty()) {
      parent.entry()->SetNamedReference(
          HeapGraphEdge::kWeak, names_->GetCopy(std::string(edge_name).c_str()),
          child_entry, generator_, HeapEntry::kOffHeapPointer);
    } else {
      // Unlike kElement the edge type kWeak requires a name, so we use
      // SetNamedAutoIndexReference here to create a name for the index.
      parent.entry()->SetNamedAutoIndexReference(
          HeapGraphEdge::kWeak, nullptr, child_entry, names_, generator_,
          HeapEntry::kOffHeapPointer);
    }
  }

  void AddEphemeronEdge(const ParentScope& table_scope,
                        const ParentScope& key_scope,
                        const HeapObjectHeader& value_header) {
    HeapEntry* key = key_scope.entry();
    HeapEntry* table = table_scope.entry();
    HeapEntry* value = GetOrCreateNode(value_header);

    generator_->CreateEphemeronEdges(table, key, value);
  }

  void AddEdge(const ParentScope& parent, const TracedReferenceBase& ref,
               std::string_view edge_name) {
    v8::Local<v8::Data> v8_data =
        ref.Get(reinterpret_cast<v8::Isolate*>(cpp_heap_.isolate()));
    if (v8_data.IsEmpty()) return;

    DirectHandle<Object> object = v8::Utils::OpenDirectHandle(*v8_data);
    if (object.is_null() || IsSmi(*object)) return;

    HeapEntry* v8_entry =
        generator_->FindEntry(reinterpret_cast<void*>((*object).ptr()));
    if (!v8_entry) return;

    if (!edge_name.empty()) {
      parent.entry()->SetNamedReference(
          HeapGraphEdge::kInternal,
          names_->GetCopy(std::string(edge_name).c_str()), v8_entry, generator_,
          HeapEntry::kOffHeapPointer);
    } else {
      parent.entry()->SetIndexedAutoIndexReference(HeapGraphEdge::kElement,
                                                   v8_entry, generator_,
                                                   HeapEntry::kOffHeapPointer);
    }
  }

  void AddRootEdge(HeapEntry* root, const HeapObjectHeader& child_header,
                   std::string edge_name) {
    HeapEntry* child_entry = GetOrCreateNode(child_header);

    if (!edge_name.empty()) {
      root->SetNamedReference(HeapGraphEdge::kInternal,
                              names_->GetCopy(edge_name.c_str()), child_entry,
                              generator_, HeapEntry::kOffHeapPointer);
    } else {
      root->SetIndexedAutoIndexReference(HeapGraphEdge::kElement, child_entry,
                                         generator_,
                                         HeapEntry::kOffHeapPointer);
    }
  }

  CppHeap& cpp_heap() { return cpp_heap_; }
  HeapSnapshotGenerator* generator() { return generator_; }

 private:
  friend class GraphBuildingVisitor;
  friend class LiveObjectsIterator;

  CppHeap& cpp_heap_;
  HeapSnapshotGenerator* generator_;
  HeapSnapshot* snapshot_;
  StringsStorage* names_;
  HeapObjectsMap* heap_object_map_;
  absl::flat_hash_map<const HeapObjectHeader*, HeapEntry*> nodes_;
  absl::flat_hash_map<const HeapObjectHeader*, cppgc::TraceDescriptor>
      weak_containers_;
  absl::flat_hash_set<const HeapObjectHeader*> objects_reachable_from_stack_;
  CppHeapWrapperSet cpp_heap_wrappers_;
  absl::flat_hash_map<const HeapObjectHeader*, WrapperInfo> wrapper_map_;
  bool pre_pass_done_ = false;
};

class GraphBuildingVisitor final : public JSVisitor {
 public:
  GraphBuildingVisitor(CppGraphBuilderImpl& graph_builder,
                       const ParentScope& parent_scope)
      : JSVisitor(cppgc::internal::VisitorFactory::CreateKey()),
        graph_builder_(graph_builder),
        parent_scope_(parent_scope) {}

  // C++ handling.
  void Visit(const void*, cppgc::TraceDescriptor desc) final {
    graph_builder_.AddEdge(
        parent_scope_, HeapObjectHeader::FromObject(desc.base_object_payload),
        edge_name_);
  }

  void VisitWeak(const void*, cppgc::TraceDescriptor desc, cppgc::WeakCallback,
                 const void*) final {
    graph_builder_.AddWeakEdge(
        parent_scope_, HeapObjectHeader::FromObject(desc.base_object_payload),
        edge_name_);
  }

  void VisitWeakContainer(const void*, cppgc::TraceDescriptor strong_desc,
                          cppgc::TraceDescriptor, cppgc::WeakCallback,
                          const void*) final {
    const auto& container_header =
        HeapObjectHeader::FromObject(strong_desc.base_object_payload);

    // Add an edge from the object holding the weak container to the weak
    // container itself.
    graph_builder_.AddEdge(parent_scope_, container_header, edge_name_);
  }

  void VisitEphemeron(const void* key, const void* value,
                      cppgc::TraceDescriptor value_desc) final {
    auto& key_header = HeapObjectHeader::FromObject(key);

    HeapEntry* key_entry = graph_builder_.GetOrCreateNode(key_header);
    ParentScope key_scope(key_entry);

    if (!value_desc.base_object_payload) {
      // Value does not represent an actual GarbageCollected object but rather
      // should be traced eagerly.
      GraphBuildingVisitor key_visitor(graph_builder_, key_scope);
      key_visitor.set_edge_name(kEphemeronEdgeName);
      value_desc.callback(&key_visitor, value);
      return;
    }

    // Regular path where both key and value are GarbageCollected objects.
    auto& value_header = HeapObjectHeader::FromObject(value);

    graph_builder_.AddEphemeronEdge(parent_scope_, key_scope, value_header);
  }

  // JS handling.
  void Visit(const TracedReferenceBase& ref) final {
    graph_builder_.AddEdge(parent_scope_, ref, edge_name_);
  }

  void set_edge_name(std::string_view edge_name) { edge_name_ = edge_name; }

 private:
  CppGraphBuilderImpl& graph_builder_;
  const ParentScope& parent_scope_;
  std::string edge_name_;
};

class PrePassVisitor final : public JSVisitor {
 public:
  PrePassVisitor(CppGraphBuilderImpl& graph_builder, Isolate* isolate)
      : JSVisitor(cppgc::internal::VisitorFactory::CreateKey()),
        graph_builder_(graph_builder),
        isolate_(isolate) {}

  void set_current_header(const HeapObjectHeader* header) {
    current_header_ = header;
  }

  void VisitWeakContainer(const void*, cppgc::TraceDescriptor strong_desc,
                          cppgc::TraceDescriptor weak_desc, cppgc::WeakCallback,
                          const void*) final {
    if (!strong_desc.base_object_payload) return;
    graph_builder_.RecordWeakContainer(
        HeapObjectHeader::FromObject(strong_desc.base_object_payload),
        weak_desc);
  }

  void Visit(const TracedReferenceBase& ref) final {
    DCHECK_NOT_NULL(current_header_);
    v8::Local<v8::Data> v8_data =
        ref.Get(reinterpret_cast<v8::Isolate*>(isolate_));
    if (v8_data.IsEmpty()) return;

    void* back_ref = ExtractEmbedderDataBackref(
        isolate_, graph_builder_.cpp_heap(), v8_data);
    if (!back_ref) return;

    auto& back_header = HeapObjectHeader::FromObject(back_ref);
    if (&back_header != current_header_) return;

    DirectHandle<Object> object = v8::Utils::OpenDirectHandle(*v8_data);
    if (object.is_null() || IsSmi(*object)) return;

    HeapEntry* v8_entry = graph_builder_.generator()->FindEntry(
        reinterpret_cast<void*>((*object).ptr()));
    if (!v8_entry) return;

    v8::EmbedderGraph::Node::Detachedness detachedness =
        v8::EmbedderGraph::Node::Detachedness::kUnknown;
    auto* profiler = isolate_->heap()->heap_profiler();
    if (profiler->HasGetDetachednessCallback() && v8_data->IsValue()) {
      detachedness = profiler->GetDetachedness(v8_data.As<v8::Value>(), 0);
    }

    graph_builder_.RecordWrapper(
        current_header_, {v8_entry, Cast<HeapObject>(*object), detachedness});
  }

 private:
  CppGraphBuilderImpl& graph_builder_;
  Isolate* isolate_;
  const HeapObjectHeader* current_header_ = nullptr;
};

// Pre-pass to discover wrappers and weak containers before building edges.
class PrePassIterator final
    : public cppgc::internal::HeapVisitor<PrePassIterator> {
  friend class cppgc::internal::HeapVisitor<PrePassIterator>;

 public:
  PrePassIterator(CppGraphBuilderImpl& graph_builder, Isolate* isolate)
      : visitor_(graph_builder, isolate) {}

 private:
  bool VisitHeapObjectHeader(HeapObjectHeader& header) {
    if (header.IsFree()) return true;
    if (header.IsInConstruction()) return true;

    visitor_.set_current_header(&header);
    header.TraceImpl(&visitor_);
    visitor_.set_current_header(nullptr);
    return true;
  }

  PrePassVisitor visitor_;
};

// Iterating live objects to create nodes and add edges.
class LiveObjectsIterator final
    : public cppgc::internal::HeapVisitor<LiveObjectsIterator> {
  friend class cppgc::internal::HeapVisitor<LiveObjectsIterator>;

 public:
  explicit LiveObjectsIterator(CppGraphBuilderImpl& graph_builder)
      : graph_builder_(graph_builder) {}

 private:
  bool VisitHeapObjectHeader(HeapObjectHeader& header) {
    if (header.IsFree()) return true;
    if (header.IsInConstruction()) {
      // TODO(mlippautz): Handle in-construction objects.
      return true;
    }

    HeapEntry* entry = graph_builder_.GetOrCreateNode(header);
    ParentScope parent_scope(entry);
    GraphBuildingVisitor object_visitor(graph_builder_, parent_scope);
    const cppgc::TraceDescriptor* weak_desc =
        graph_builder_.GetWeakContainerTraceDescriptor(header);
    if (weak_desc && !graph_builder_.IsReachableFromStack(header)) {
      // For WeakContainers we call the weak tracing method if available.
      // WeakContainer do not need to have a callback, see
      // ProcessWeakContainer() in cppgc/marking-state.h.
      if (weak_desc->callback) {
        weak_desc->callback(&object_visitor, weak_desc->base_object_payload);
      }
    } else {
      // For all other cases we call the regular Trace() method on that object.
      // This is the default case.
      header.TraceImpl(&object_visitor);
    }
    return true;
  }

  CppGraphBuilderImpl& graph_builder_;
};

class GraphBuildingRootVisitor final : public cppgc::internal::RootVisitorBase {
 public:
  GraphBuildingRootVisitor(CppGraphBuilderImpl& graph_builder,
                           const ParentScope& parent_scope)
      : graph_builder_(graph_builder), parent_scope_(parent_scope) {}

  void VisitRoot(const void*, cppgc::TraceDescriptor desc,
                 cppgc::SourceLocation loc) final {
    graph_builder_.AddRootEdge(
        parent_scope_.entry(),
        HeapObjectHeader::FromObject(desc.base_object_payload), loc);
  }

 private:
  CppGraphBuilderImpl& graph_builder_;
  const ParentScope& parent_scope_;
};

CppGraphBuilderImpl::CppGraphBuilderImpl(CppHeap& cpp_heap,
                                         HeapSnapshotGenerator* generator,
                                         CppHeapWrapperSet&& cpp_heap_wrappers)
    : cpp_heap_(cpp_heap),
      generator_(generator),
      snapshot_(generator->snapshot()),
      names_(snapshot_->profiler()->names()),
      heap_object_map_(snapshot_->profiler()->heap_object_map()),
      cpp_heap_wrappers_(std::move(cpp_heap_wrappers)) {}

void CppGraphBuilderImpl::RecordObjectReachableFromStack(
    const HeapObjectHeader& header) {
  objects_reachable_from_stack_.insert(&header);
}

void CppGraphBuilderImpl::RecordWeakContainer(
    const HeapObjectHeader& header, cppgc::TraceDescriptor weak_desc) {
  weak_containers_.emplace(&header, weak_desc);
}

void CppGraphBuilderImpl::AddRootEdge(HeapEntry* root,
                                      const HeapObjectHeader& header,
                                      cppgc::SourceLocation loc) {
  AddRootEdge(root, header, loc.ToString());
}

void CppGraphBuilderImpl::RecordTracedReferenceOnStack(
    HeapEntry* root, Tagged<HeapObject> object) {
  HeapEntry* v8_entry =
      generator_->FindEntry(reinterpret_cast<void*>(object.ptr()));
  if (!v8_entry) return;
  root->SetIndexedAutoIndexReference(HeapGraphEdge::kElement, v8_entry,
                                     generator_, HeapEntry::kOffHeapPointer);
}

void CppGraphBuilderImpl::AddEdgeForCppHeapWrapper(
    Tagged<CppHeapPointerWrapperObjectT> object) {
  void* cpp_object = CppHeapObjectWrapper::From(object).GetCppHeapWrappable(
      cpp_heap_.isolate(), kAnyCppHeapPointer);
  if (!cpp_object) {
    return;
  }

  auto& header = HeapObjectHeader::FromObject(cpp_object);

  auto it = wrapper_map_.find(&header);
  if (it != wrapper_map_.end() && it->second.v8_object == object) {
    // The C++ object is already merged into a wrapper node. Avoid adding a
    // bogus self-edge on the wrapper node.
    return;
  }

  HeapEntry* cpp_entry = GetOrCreateNode(header);

  HeapEntry* v8_entry =
      generator_->FindEntry(reinterpret_cast<void*>(object.ptr()));
  if (!v8_entry) return;

  v8_entry->SetNamedReference(HeapGraphEdge::kInternal, "cppgc_object",
                              cpp_entry, generator_,
                              HeapEntry::kOffHeapPointer);
}

namespace {

// Visitor adds edges from native stack roots to objects.
class GraphBuildingStackVisitor
    : public cppgc::internal::ConservativeTracingVisitor,
      public ::heap::base::StackVisitor,
      public cppgc::Visitor {
 public:
  GraphBuildingStackVisitor(CppGraphBuilderImpl& graph_builder, CppHeap& heap,
                            GraphBuildingRootVisitor& root_visitor,
                            const ParentScope& traced_handles_parent_scope)
      : cppgc::internal::ConservativeTracingVisitor(heap, *heap.page_backend(),
                                                    *this),
        cppgc::Visitor(cppgc::internal::VisitorFactory::CreateKey()),
        graph_builder_(graph_builder),
        root_visitor_(root_visitor),
        traced_handles_parent_scope_(traced_handles_parent_scope),
        isolate_(heap.isolate()),
        traced_handles_scanner_(isolate_) {}

  void VisitPointer(const void* address) final {
    // Entry point for stack walk. The conservative visitor dispatches as
    // follows:
    // - Fully constructed objects: VisitFullyConstructedConservatively()
    // - Objects in construction: VisitInConstructionConservatively()
    TraceConservativelyIfNeeded(address);

    if (TracedNode* node = traced_handles_scanner_.TryFindNode(address)) {
      Tagged<Object> object = node->object();
      if (IsHeapObject(object)) {
        graph_builder_.RecordTracedReferenceOnStack(
            traced_handles_parent_scope_.entry(), Cast<HeapObject>(object));
      }
    }
  }

  void VisitFullyConstructedConservatively(HeapObjectHeader& header) final {
    VisitConservatively(header);
  }

  void VisitInConstructionConservatively(HeapObjectHeader& header,
                                         TraceConservativelyCallback) final {
    VisitConservatively(header);
  }

 private:
  void VisitConservatively(const HeapObjectHeader& header) {
    root_visitor_.VisitRoot(header.ObjectStart(),
                            {header.ObjectStart(), nullptr},
                            cppgc::SourceLocation());
    graph_builder_.RecordObjectReachableFromStack(header);
  }

  CppGraphBuilderImpl& graph_builder_;
  GraphBuildingRootVisitor& root_visitor_;
  const ParentScope& traced_handles_parent_scope_;
  Isolate* isolate_;
  ConservativeTracedHandlesNodeScanner traced_handles_scanner_;
};

}  // namespace

void CppGraphBuilderImpl::Run() {
  // Sweeping from a previous GC might still be running, in which case not all
  // pages have been returned to spaces yet.
  cpp_heap_.sweeper().FinishIfRunning();
  cppgc::subtle::DisallowGarbageCollectionScope no_gc(
      cpp_heap_.GetHeapHandle());

  // (1) Discover all wrappers and weak containers first. This iterates over all
  // objects in the heap to find all objects traced through TraceWeakContainer()
  // and TracedReferences with back references.
  PrePassIterator pre_pass_iterator(
      *this, reinterpret_cast<Isolate*>(cpp_heap_.isolate()));
  pre_pass_iterator.Traverse(cpp_heap_.raw_heap());

  // Only once we identified all wrappers we need to merge, we are allowed to
  // create HeapEntries for cppgc objects.
  pre_pass_done_ = true;

  // (2a) Visit the stack before iterating the heap. When iterating the heap we
  // need to know for weak containers whether they were reachable from stack or
  // not.
  //
  // Only add stack roots in case the callback is not run from generating a
  // snapshot without stack. This avoids adding false-positive edges when
  // conservatively scanning the stack.
  if (cpp_heap_.isolate()->heap()->IsGCWithMainThreadStack()) {
    HeapEntry* native_stack_root = AddRootNode("C++ native stack roots");
    ParentScope native_stack_parent_scope(native_stack_root,
                                          ParentScope::Type::kRoot);

    HeapEntry* traced_handles_root =
        AddRootNode("C++ native stack traced handles");
    ParentScope traced_handles_parent_scope(traced_handles_root,
                                            ParentScope::Type::kRoot);

    GraphBuildingRootVisitor root_object_visitor(*this,
                                                 native_stack_parent_scope);
    GraphBuildingStackVisitor stack_visitor(
        *this, cpp_heap_, root_object_visitor, traced_handles_parent_scope);
    cpp_heap_.stack()->IteratePointersUntilMarker(&stack_visitor);
  }

  // (2b) Add persistent roots.
  {
    HeapEntry* root_node = AddRootNode("C++ Persistent roots");
    ParentScope parent_scope(root_node, ParentScope::Type::kRoot);
    GraphBuildingRootVisitor root_object_visitor(*this, parent_scope);
    cpp_heap_.GetStrongPersistentRegion().Iterate(root_object_visitor);
  }
  {
    HeapEntry* root_node = AddRootNode("C++ CrossThreadPersistent roots");
    ParentScope parent_scope(root_node, ParentScope::Type::kRoot);
    GraphBuildingRootVisitor root_object_visitor(*this, parent_scope);
    cppgc::internal::PersistentRegionLock guard;
    cpp_heap_.GetStrongCrossThreadPersistentRegion().Iterate(
        root_object_visitor);
  }

  // (3) Iterate over all objects in the heap to create nodes and add edges.
  // Because of (1) and (2) we know which objects are weak containers and/or
  // reachable from stack.
  LiveObjectsIterator visitor(*this);
  visitor.Traverse(cpp_heap_.raw_heap());

  // Connect each cppgc wrapper object to its corresponding cpp object.
  for (Tagged<CppHeapPointerWrapperObjectT> object : cpp_heap_wrappers_) {
    AddEdgeForCppHeapWrapper(object);
  }
}

// static
void CppGraphBuilder::Run(CppHeap* cpp_heap, HeapSnapshotGenerator* generator,
                          CppHeapWrapperSet&& cpp_heap_wrappers) {
  CHECK_NOT_NULL(cpp_heap);
  CHECK_NOT_NULL(generator);
  CppGraphBuilderImpl graph_builder(*cpp_heap, generator,
                                    std::move(cpp_heap_wrappers));
  graph_builder.Run();
}

}  // namespace internal
}  // namespace v8
