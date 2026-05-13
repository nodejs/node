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
#include "src/heap/cppgc-js/cpp-heap.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap-visitor.h"
#include "src/heap/cppgc/visitor.h"
#include "src/heap/marking-worklist-inl.h"
#include "src/objects/cpp-heap-object-wrapper-inl.h"
#include "src/objects/js-objects.h"
#include "src/objects/objects-inl.h"
#include "src/profiler/heap-profiler.h"

namespace v8 {
namespace internal {

class CppGraphBuilderImpl;
class StateStorage;
class State;

using cppgc::internal::HeapObjectHeader;

// Node representing a C++ object on the heap.
class EmbedderNode : public v8::EmbedderGraph::Node {
 public:
  EmbedderNode(const HeapObjectHeader* header_address,
               cppgc::internal::HeapObjectName name, size_t size)
      : header_address_(header_address), name_(name.value), size_(size) {}
  ~EmbedderNode() override = default;

  const char* Name() final { return name_; }
  size_t SizeInBytes() final { return size_; }

  void SetWrapperNode(v8::EmbedderGraph::Node* wrapper_node) {
    // An embedder node may only be merged with a single wrapper node, as
    // consumers of the graph may merge a node and its wrapper node.
    //
    // TODO(chromium:1218404): Add a DCHECK() to avoid overriding an already
    // set `wrapper_node_`. This can currently happen with global proxies that
    // are rewired (and still kept alive) after reloading a page, see
    // `AddEdge`. We accept overriding the wrapper node in such cases,
    // leading to a random merged node and separated nodes for all other
    // proxies.
    wrapper_node_ = wrapper_node;
  }
  Node* WrapperNode() final { return wrapper_node_; }

  void SetDetachedness(Detachedness detachedness) {
    detachedness_ = detachedness;
  }
  Detachedness GetDetachedness() final { return detachedness_; }

  // Edge names are passed to V8 but are required to be held alive from the
  // embedder until the snapshot is compiled.
  const char* InternalizeEdgeName(std::string_view edge_name) {
    const size_t edge_name_len = edge_name.length();
    named_edges_.emplace_back(std::make_unique<char[]>(edge_name_len + 1));
    char* named_edge_str = named_edges_.back().get();
    snprintf(named_edge_str, edge_name_len + 1, "%s", edge_name.data());
    return named_edge_str;
  }

  const void* GetAddress() override { return header_address_; }

 private:
  const void* header_address_;
  const char* name_;
  size_t size_;
  Node* wrapper_node_ = nullptr;
  Detachedness detachedness_ = Detachedness::kUnknown;
  std::vector<std::unique_ptr<char[]>> named_edges_;
};

constexpr HeapObjectHeader* kNoNativeAddress = nullptr;

constexpr std::string_view kEphemeronEdgeName =
    "part of key -> value pair in ephemeron table";

// Node representing an artificial root group, e.g., set of Persistent handles.
class EmbedderRootNode final : public EmbedderNode {
 public:
  explicit EmbedderRootNode(const char* name)
      : EmbedderNode(kNoNativeAddress, {name, false}, 0) {}
  ~EmbedderRootNode() final = default;

  bool IsRootNode() final { return true; }
};

class ParentScope final {
 public:
  explicit ParentScope(EmbedderNode* node) : node_(node) {
    DCHECK_NOT_NULL(node_);
  }

  EmbedderNode* node() const { return node_; }
  bool IsRoot() const { return node_->IsRootNode(); }

 private:
  EmbedderNode* node_;
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
  CppGraphBuilderImpl(CppHeap& cpp_heap, v8::EmbedderGraph& graph,
                      CppHeapWrapperSet&& cpp_heap_wrappers);

  void Run();

  void AddRootEdge(EmbedderNode*, const HeapObjectHeader&,
                   cppgc::SourceLocation);
  void RecordTracedReferenceOnStack(EmbedderNode*, Tagged<HeapObject>);

  void RecordObjectReachableFromStack(const HeapObjectHeader&);
  void RecordWeakContainer(const HeapObjectHeader&, cppgc::TraceDescriptor);

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

  EmbedderRootNode* AddRootNode(const char* name) {
    return static_cast<EmbedderRootNode*>(graph_.AddNode(
        std::unique_ptr<v8::EmbedderGraph::Node>{new EmbedderRootNode(name)}));
  }

  EmbedderNode* AddNode(const HeapObjectHeader& header) {
    size_t size = header.AllocatedSize();
    EmbedderNode* node = static_cast<EmbedderNode*>(
        graph_.AddNode(std::unique_ptr<v8::EmbedderGraph::Node>{
            new EmbedderNode(&header, header.GetName(), size)}));
    DCHECK_EQ(node->SizeInBytes(), size);
    return node;
  }

  EmbedderNode* GetOrCreateNode(const HeapObjectHeader& header) {
    auto it = nodes_.find(&header);
    if (it != nodes_.end()) return it->second;

    EmbedderNode* node = AddNode(header);
    nodes_[&header] = node;
    return node;
  }

  void AddEdge(const ParentScope& parent, const HeapObjectHeader& header,
               std::string_view edge_name) {
    EmbedderNode* child_node = GetOrCreateNode(header);

    if (!edge_name.empty()) {
      graph_.AddEdge(parent.node(), child_node,
                     parent.node()->InternalizeEdgeName(edge_name));
    } else {
      graph_.AddEdge(parent.node(), child_node);
    }
  }

  void AddEdge(const ParentScope& parent, const TracedReferenceBase& ref,
               std::string_view edge_name) {
    v8::Local<v8::Data> v8_data =
        ref.Get(reinterpret_cast<v8::Isolate*>(cpp_heap_.isolate()));
    if (v8_data.IsEmpty()) return;

    auto* v8_node = graph_.V8Node(v8_data);
    if (!edge_name.empty()) {
      graph_.AddEdge(parent.node(), v8_node,
                     parent.node()->InternalizeEdgeName(edge_name));
    } else {
      graph_.AddEdge(parent.node(), v8_node);
    }

    // Don't merge nodes if edges have a name.
    if (!edge_name.empty()) return;

    // Try to extract the back reference. If the back reference matches
    // `parent`, then the nodes are merged.
    void* back_reference_object = ExtractEmbedderDataBackref(
        reinterpret_cast<v8::internal::Isolate*>(cpp_heap_.isolate()),
        cpp_heap_, v8_data);
    if (!back_reference_object) return;
    // Only JS objects have back references.
    DCHECK(v8_data->IsValue() && v8_data.As<v8::Value>()->IsObject());

    auto& back_header = HeapObjectHeader::FromObject(back_reference_object);

    // If the back reference doesn't point to the same header, just return. In
    // such a case we have stand-alone references to a wrapper.
    if (parent.IsRoot() || parent.node()->GetAddress() != &back_header) return;

    // Back reference points to parents header. In this case, the nodes should
    // be merged and query the detachedness state of the embedder.
    auto it = nodes_.find(&back_header);
    CHECK(it != nodes_.end());
    EmbedderNode* back_node = it->second;

    back_node->SetWrapperNode(v8_node);

    auto* profiler = reinterpret_cast<Isolate*>(cpp_heap_.isolate())
                         ->heap()
                         ->heap_profiler();
    if (profiler->HasGetDetachednessCallback()) {
      back_node->SetDetachedness(
          profiler->GetDetachedness(v8_data.As<v8::Value>(), 0));
    }
  }

  void AddRootEdge(EmbedderRootNode* root, const HeapObjectHeader& child_header,
                   std::string edge_name) {
    EmbedderNode* child_node = GetOrCreateNode(child_header);

    if (!edge_name.empty()) {
      graph_.AddEdge(root, child_node, root->InternalizeEdgeName(edge_name));
    } else {
      graph_.AddEdge(root, child_node);
    }
  }

 private:
  friend class GraphBuildingVisitor;
  friend class LiveObjectsIterator;

  struct MergedNodeItem {
    EmbedderGraph::Node* node_;
    v8::Local<v8::Value> value_;
  };

  CppHeap& cpp_heap_;
  v8::EmbedderGraph& graph_;
  absl::flat_hash_map<const HeapObjectHeader*, EmbedderNode*> nodes_;
  absl::flat_hash_map<const HeapObjectHeader*, cppgc::TraceDescriptor>
      weak_containers_;
  absl::flat_hash_set<const HeapObjectHeader*> objects_reachable_from_stack_;
  CppHeapWrapperSet cpp_heap_wrappers_;
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

    EmbedderNode* key_node = graph_builder_.GetOrCreateNode(key_header);
    ParentScope key_scope(key_node);

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

    graph_builder_.AddEdge(key_scope, value_header, kEphemeronEdgeName);
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

class WeakContainerDiscoveryVisitor final : public JSVisitor {
 public:
  explicit WeakContainerDiscoveryVisitor(CppGraphBuilderImpl& graph_builder)
      : JSVisitor(cppgc::internal::VisitorFactory::CreateKey()),
        graph_builder_(graph_builder) {}

  void VisitWeakContainer(const void*, cppgc::TraceDescriptor strong_desc,
                          cppgc::TraceDescriptor weak_desc, cppgc::WeakCallback,
                          const void*) final {
    if (!strong_desc.base_object_payload) return;
    graph_builder_.RecordWeakContainer(
        HeapObjectHeader::FromObject(strong_desc.base_object_payload),
        weak_desc);
  }

 private:
  CppGraphBuilderImpl& graph_builder_;
};

// Iterating live objects to discover weak containers before building edges.
class WeakContainerDiscoveryIterator final
    : public cppgc::internal::HeapVisitor<WeakContainerDiscoveryIterator> {
  friend class cppgc::internal::HeapVisitor<WeakContainerDiscoveryIterator>;

 public:
  explicit WeakContainerDiscoveryIterator(CppGraphBuilderImpl& graph_builder)
      : weak_container_discovery_visitor_(graph_builder) {}

 private:
  bool VisitHeapObjectHeader(HeapObjectHeader& header) {
    if (header.IsFree()) return true;
    if (header.IsInConstruction()) return true;

    header.TraceImpl(&weak_container_discovery_visitor_);
    return true;
  }

  WeakContainerDiscoveryVisitor weak_container_discovery_visitor_;
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

    EmbedderNode* node = graph_builder_.GetOrCreateNode(header);
    ParentScope parent_scope(node);
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
        parent_scope_.node(),
        HeapObjectHeader::FromObject(desc.base_object_payload), loc);
  }

 private:
  CppGraphBuilderImpl& graph_builder_;
  const ParentScope& parent_scope_;
};

CppGraphBuilderImpl::CppGraphBuilderImpl(CppHeap& cpp_heap,
                                         v8::EmbedderGraph& graph,
                                         CppHeapWrapperSet&& cpp_heap_wrappers)
    : cpp_heap_(cpp_heap),
      graph_(graph),
      cpp_heap_wrappers_(std::move(cpp_heap_wrappers)) {}

void CppGraphBuilderImpl::RecordObjectReachableFromStack(
    const HeapObjectHeader& header) {
  objects_reachable_from_stack_.insert(&header);
}

void CppGraphBuilderImpl::RecordWeakContainer(
    const HeapObjectHeader& header, cppgc::TraceDescriptor weak_desc) {
  weak_containers_.emplace(&header, weak_desc);
}

void CppGraphBuilderImpl::AddRootEdge(EmbedderNode* root,
                                      const HeapObjectHeader& header,
                                      cppgc::SourceLocation loc) {
  AddRootEdge(static_cast<EmbedderRootNode*>(root), header, loc.ToString());
}

void CppGraphBuilderImpl::RecordTracedReferenceOnStack(
    EmbedderNode* root, Tagged<HeapObject> object) {
  v8::Local<v8::Data> v8_data =
      Utils::ToLocal(handle(object, cpp_heap_.isolate()));
  auto* v8_node = graph_.V8Node(v8_data);
  graph_.AddEdge(root, v8_node);
}

void CppGraphBuilderImpl::AddEdgeForCppHeapWrapper(
    Tagged<CppHeapPointerWrapperObjectT> object) {
  void* cpp_object = CppHeapObjectWrapper::From(object).GetCppHeapWrappable(
      cpp_heap_.isolate(), kAnyCppHeapPointer);
  if (!cpp_object) {
    return;
  }

  auto& header = HeapObjectHeader::FromObject(cpp_object);
  EmbedderNode* cpp_node = GetOrCreateNode(header);

  v8::Local<v8::Data> v8_value;
  if (IsJSObject(object)) {
    v8_value =
        Utils::ToLocal(handle(Cast<JSObject>(object), cpp_heap_.isolate()));
  } else {
    v8_value = Utils::CppHeapExternalToLocal(
        handle(Cast<CppHeapExternalObject>(object), cpp_heap_.isolate()));
  }

  if (cpp_node->WrapperNode()) {
    // The C++ object is already merged into a wrapper node. Avoid adding a
    // bogus self-edge on the wrapper node.
    return;
  }

  auto* v8_node = graph_.V8Node(v8_value);
  graph_.AddEdge(v8_node, cpp_node, "cppgc_object");
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
            traced_handles_parent_scope_.node(), Cast<HeapObject>(object));
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
  // (1a) Visit the stack before iterating the heap. When iterating the heap we
  // need to know for weak containers whether they were reachable from stack or
  // not.
  //
  // Only add stack roots in case the callback is not run from generating a
  // snapshot without stack. This avoids adding false-positive edges when
  // conservatively scanning the stack.
  if (cpp_heap_.isolate()->heap()->IsGCWithMainThreadStack()) {
    EmbedderRootNode* native_stack_root = AddRootNode("C++ native stack roots");
    ParentScope native_stack_parent_scope(native_stack_root);

    EmbedderRootNode* traced_handles_root =
        AddRootNode("C++ native stack traced handles");
    ParentScope traced_handles_parent_scope(traced_handles_root);

    GraphBuildingRootVisitor root_object_visitor(*this,
                                                 native_stack_parent_scope);
    GraphBuildingStackVisitor stack_visitor(
        *this, cpp_heap_, root_object_visitor, traced_handles_parent_scope);
    cpp_heap_.stack()->IteratePointersUntilMarker(&stack_visitor);
  }

  // (1b) Add persistent roots.
  {
    EmbedderRootNode* root_node = AddRootNode("C++ Persistent roots");
    ParentScope parent_scope(root_node);
    GraphBuildingRootVisitor root_object_visitor(*this, parent_scope);
    cpp_heap_.GetStrongPersistentRegion().Iterate(root_object_visitor);
  }
  {
    EmbedderRootNode* root_node =
        AddRootNode("C++ CrossThreadPersistent roots");
    ParentScope parent_scope(root_node);
    GraphBuildingRootVisitor root_object_visitor(*this, parent_scope);
    cppgc::internal::PersistentRegionLock guard;
    cpp_heap_.GetStrongCrossThreadPersistentRegion().Iterate(
        root_object_visitor);
  }

  // (2) Discover all weak containers first. This iterates over all objects in
  // the heap to find all objects traced through TraceWeakContainer().
  WeakContainerDiscoveryIterator weak_container_discovery_visitor(*this);
  weak_container_discovery_visitor.Traverse(cpp_heap_.raw_heap());

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
void CppGraphBuilder::Run(v8::Isolate* isolate, v8::EmbedderGraph* graph,
                          void* data, CppHeapWrapperSet&& cpp_heap_wrappers) {
  CppHeap* cpp_heap = static_cast<CppHeap*>(data);
  CHECK_NOT_NULL(cpp_heap);
  CHECK_NOT_NULL(graph);
  CppGraphBuilderImpl graph_builder(*cpp_heap, *graph,
                                    std::move(cpp_heap_wrappers));
  graph_builder.Run();
}

}  // namespace internal
}  // namespace v8
