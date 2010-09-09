// Copyright 2009 the V8 project authors. All rights reserved.
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

#include "v8.h"

#include "heap-profiler.h"
#include "frames-inl.h"
#include "global-handles.h"
#include "profile-generator.h"
#include "string-stream.h"

namespace v8 {
namespace internal {


#ifdef ENABLE_LOGGING_AND_PROFILING
namespace {

// Clusterizer is a set of helper functions for converting
// object references into clusters.
class Clusterizer : public AllStatic {
 public:
  static JSObjectsCluster Clusterize(HeapObject* obj) {
    return Clusterize(obj, true);
  }
  static void InsertIntoTree(JSObjectsClusterTree* tree,
                             HeapObject* obj, bool fine_grain);
  static void InsertReferenceIntoTree(JSObjectsClusterTree* tree,
                                      const JSObjectsCluster& cluster) {
    InsertIntoTree(tree, cluster, 0);
  }

 private:
  static JSObjectsCluster Clusterize(HeapObject* obj, bool fine_grain);
  static int CalculateNetworkSize(JSObject* obj);
  static int GetObjectSize(HeapObject* obj) {
    return obj->IsJSObject() ?
        CalculateNetworkSize(JSObject::cast(obj)) : obj->Size();
  }
  static void InsertIntoTree(JSObjectsClusterTree* tree,
                             const JSObjectsCluster& cluster, int size);
};


JSObjectsCluster Clusterizer::Clusterize(HeapObject* obj, bool fine_grain) {
  if (obj->IsJSObject()) {
    JSObject* js_obj = JSObject::cast(obj);
    String* constructor = JSObject::cast(js_obj)->constructor_name();
    // Differentiate Object and Array instances.
    if (fine_grain && (constructor == Heap::Object_symbol() ||
                       constructor == Heap::Array_symbol())) {
      return JSObjectsCluster(constructor, obj);
    } else {
      return JSObjectsCluster(constructor);
    }
  } else if (obj->IsString()) {
    return JSObjectsCluster(Heap::String_symbol());
  } else if (obj->IsJSGlobalPropertyCell()) {
    return JSObjectsCluster(JSObjectsCluster::GLOBAL_PROPERTY);
  } else if (obj->IsCode() || obj->IsSharedFunctionInfo() || obj->IsScript()) {
    return JSObjectsCluster(JSObjectsCluster::CODE);
  }
  return JSObjectsCluster();
}


void Clusterizer::InsertIntoTree(JSObjectsClusterTree* tree,
                                 HeapObject* obj, bool fine_grain) {
  JSObjectsCluster cluster = Clusterize(obj, fine_grain);
  if (cluster.is_null()) return;
  InsertIntoTree(tree, cluster, GetObjectSize(obj));
}


void Clusterizer::InsertIntoTree(JSObjectsClusterTree* tree,
                                 const JSObjectsCluster& cluster, int size) {
  JSObjectsClusterTree::Locator loc;
  tree->Insert(cluster, &loc);
  NumberAndSizeInfo number_and_size = loc.value();
  number_and_size.increment_number(1);
  number_and_size.increment_bytes(size);
  loc.set_value(number_and_size);
}


int Clusterizer::CalculateNetworkSize(JSObject* obj) {
  int size = obj->Size();
  // If 'properties' and 'elements' are non-empty (thus, non-shared),
  // take their size into account.
  if (obj->properties() != Heap::empty_fixed_array()) {
    size += obj->properties()->Size();
  }
  if (obj->elements() != Heap::empty_fixed_array()) {
    size += obj->elements()->Size();
  }
  // For functions, also account non-empty context and literals sizes.
  if (obj->IsJSFunction()) {
    JSFunction* f = JSFunction::cast(obj);
    if (f->unchecked_context()->IsContext()) {
      size += f->context()->Size();
    }
    if (f->literals()->length() != 0) {
      size += f->literals()->Size();
    }
  }
  return size;
}


// A helper class for recording back references.
class ReferencesExtractor : public ObjectVisitor {
 public:
  ReferencesExtractor(const JSObjectsCluster& cluster,
                      RetainerHeapProfile* profile)
      : cluster_(cluster),
        profile_(profile),
        inside_array_(false) {
  }

  void VisitPointer(Object** o) {
    if ((*o)->IsFixedArray() && !inside_array_) {
      // Traverse one level deep for data members that are fixed arrays.
      // This covers the case of 'elements' and 'properties' of JSObject,
      // and function contexts.
      inside_array_ = true;
      FixedArray::cast(*o)->Iterate(this);
      inside_array_ = false;
    } else if ((*o)->IsHeapObject()) {
      profile_->StoreReference(cluster_, HeapObject::cast(*o));
    }
  }

  void VisitPointers(Object** start, Object** end) {
    for (Object** p = start; p < end; p++) VisitPointer(p);
  }

 private:
  const JSObjectsCluster& cluster_;
  RetainerHeapProfile* profile_;
  bool inside_array_;
};


// A printer interface implementation for the Retainers profile.
class RetainersPrinter : public RetainerHeapProfile::Printer {
 public:
  void PrintRetainers(const JSObjectsCluster& cluster,
                      const StringStream& retainers) {
    HeapStringAllocator allocator;
    StringStream stream(&allocator);
    cluster.Print(&stream);
    LOG(HeapSampleJSRetainersEvent(
        *(stream.ToCString()), *(retainers.ToCString())));
  }
};


// Visitor for printing a cluster tree.
class ClusterTreePrinter BASE_EMBEDDED {
 public:
  explicit ClusterTreePrinter(StringStream* stream) : stream_(stream) {}
  void Call(const JSObjectsCluster& cluster,
            const NumberAndSizeInfo& number_and_size) {
    Print(stream_, cluster, number_and_size);
  }
  static void Print(StringStream* stream,
                    const JSObjectsCluster& cluster,
                    const NumberAndSizeInfo& number_and_size);

 private:
  StringStream* stream_;
};


void ClusterTreePrinter::Print(StringStream* stream,
                               const JSObjectsCluster& cluster,
                               const NumberAndSizeInfo& number_and_size) {
  stream->Put(',');
  cluster.Print(stream);
  stream->Add(";%d", number_and_size.number());
}


// Visitor for printing a retainer tree.
class SimpleRetainerTreePrinter BASE_EMBEDDED {
 public:
  explicit SimpleRetainerTreePrinter(RetainerHeapProfile::Printer* printer)
      : printer_(printer) {}
  void Call(const JSObjectsCluster& cluster, JSObjectsClusterTree* tree);

 private:
  RetainerHeapProfile::Printer* printer_;
};


void SimpleRetainerTreePrinter::Call(const JSObjectsCluster& cluster,
                                     JSObjectsClusterTree* tree) {
  HeapStringAllocator allocator;
  StringStream stream(&allocator);
  ClusterTreePrinter retainers_printer(&stream);
  tree->ForEach(&retainers_printer);
  printer_->PrintRetainers(cluster, stream);
}


// Visitor for aggregating references count of equivalent clusters.
class RetainersAggregator BASE_EMBEDDED {
 public:
  RetainersAggregator(ClustersCoarser* coarser, JSObjectsClusterTree* dest_tree)
      : coarser_(coarser), dest_tree_(dest_tree) {}
  void Call(const JSObjectsCluster& cluster,
            const NumberAndSizeInfo& number_and_size);

 private:
  ClustersCoarser* coarser_;
  JSObjectsClusterTree* dest_tree_;
};


void RetainersAggregator::Call(const JSObjectsCluster& cluster,
                               const NumberAndSizeInfo& number_and_size) {
  JSObjectsCluster eq = coarser_->GetCoarseEquivalent(cluster);
  if (eq.is_null()) eq = cluster;
  JSObjectsClusterTree::Locator loc;
  dest_tree_->Insert(eq, &loc);
  NumberAndSizeInfo aggregated_number = loc.value();
  aggregated_number.increment_number(number_and_size.number());
  loc.set_value(aggregated_number);
}


// Visitor for printing retainers tree. Aggregates equivalent retainer clusters.
class AggregatingRetainerTreePrinter BASE_EMBEDDED {
 public:
  AggregatingRetainerTreePrinter(ClustersCoarser* coarser,
                                 RetainerHeapProfile::Printer* printer)
      : coarser_(coarser), printer_(printer) {}
  void Call(const JSObjectsCluster& cluster, JSObjectsClusterTree* tree);

 private:
  ClustersCoarser* coarser_;
  RetainerHeapProfile::Printer* printer_;
};


void AggregatingRetainerTreePrinter::Call(const JSObjectsCluster& cluster,
                                          JSObjectsClusterTree* tree) {
  if (!coarser_->GetCoarseEquivalent(cluster).is_null()) return;
  JSObjectsClusterTree dest_tree_;
  RetainersAggregator retainers_aggregator(coarser_, &dest_tree_);
  tree->ForEach(&retainers_aggregator);
  HeapStringAllocator allocator;
  StringStream stream(&allocator);
  ClusterTreePrinter retainers_printer(&stream);
  dest_tree_.ForEach(&retainers_printer);
  printer_->PrintRetainers(cluster, stream);
}

}  // namespace


// A helper class for building a retainers tree, that aggregates
// all equivalent clusters.
class RetainerTreeAggregator {
 public:
  explicit RetainerTreeAggregator(ClustersCoarser* coarser)
      : coarser_(coarser) {}
  void Process(JSObjectsRetainerTree* input_tree) {
    input_tree->ForEach(this);
  }
  void Call(const JSObjectsCluster& cluster, JSObjectsClusterTree* tree);
  JSObjectsRetainerTree& output_tree() { return output_tree_; }

 private:
  ClustersCoarser* coarser_;
  JSObjectsRetainerTree output_tree_;
};


void RetainerTreeAggregator::Call(const JSObjectsCluster& cluster,
                                  JSObjectsClusterTree* tree) {
  JSObjectsCluster eq = coarser_->GetCoarseEquivalent(cluster);
  if (eq.is_null()) return;
  JSObjectsRetainerTree::Locator loc;
  if (output_tree_.Insert(eq, &loc)) {
    loc.set_value(new JSObjectsClusterTree());
  }
  RetainersAggregator retainers_aggregator(coarser_, loc.value());
  tree->ForEach(&retainers_aggregator);
}


HeapProfiler* HeapProfiler::singleton_ = NULL;

HeapProfiler::HeapProfiler()
    : snapshots_(new HeapSnapshotsCollection()),
      next_snapshot_uid_(1) {
}


HeapProfiler::~HeapProfiler() {
  delete snapshots_;
}

#endif  // ENABLE_LOGGING_AND_PROFILING

void HeapProfiler::Setup() {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (singleton_ == NULL) {
    singleton_ = new HeapProfiler();
  }
#endif
}


void HeapProfiler::TearDown() {
#ifdef ENABLE_LOGGING_AND_PROFILING
  delete singleton_;
  singleton_ = NULL;
#endif
}


#ifdef ENABLE_LOGGING_AND_PROFILING

HeapSnapshot* HeapProfiler::TakeSnapshot(const char* name, int type) {
  ASSERT(singleton_ != NULL);
  return singleton_->TakeSnapshotImpl(name, type);
}


HeapSnapshot* HeapProfiler::TakeSnapshot(String* name, int type) {
  ASSERT(singleton_ != NULL);
  return singleton_->TakeSnapshotImpl(name, type);
}


HeapSnapshot* HeapProfiler::TakeSnapshotImpl(const char* name, int type) {
  Heap::CollectAllGarbage(true);
  HeapSnapshot::Type s_type = static_cast<HeapSnapshot::Type>(type);
  HeapSnapshot* result =
      snapshots_->NewSnapshot(s_type, name, next_snapshot_uid_++);
  switch (s_type) {
    case HeapSnapshot::kFull: {
      HeapSnapshotGenerator generator(result);
      generator.GenerateSnapshot();
      break;
    }
    case HeapSnapshot::kAggregated: {
      AggregatedHeapSnapshot agg_snapshot;
      AggregatedHeapSnapshotGenerator generator(&agg_snapshot);
      generator.GenerateSnapshot();
      generator.FillHeapSnapshot(result);
      break;
    }
    default:
      UNREACHABLE();
  }
  snapshots_->SnapshotGenerationFinished();
  return result;
}


HeapSnapshot* HeapProfiler::TakeSnapshotImpl(String* name, int type) {
  return TakeSnapshotImpl(snapshots_->GetName(name), type);
}


int HeapProfiler::GetSnapshotsCount() {
  ASSERT(singleton_ != NULL);
  return singleton_->snapshots_->snapshots()->length();
}


HeapSnapshot* HeapProfiler::GetSnapshot(int index) {
  ASSERT(singleton_ != NULL);
  return singleton_->snapshots_->snapshots()->at(index);
}


HeapSnapshot* HeapProfiler::FindSnapshot(unsigned uid) {
  ASSERT(singleton_ != NULL);
  return singleton_->snapshots_->GetSnapshot(uid);
}


void HeapProfiler::ObjectMoveEvent(Address from, Address to) {
  ASSERT(singleton_ != NULL);
  singleton_->snapshots_->ObjectMoveEvent(from, to);
}


const JSObjectsClusterTreeConfig::Key JSObjectsClusterTreeConfig::kNoKey;
const JSObjectsClusterTreeConfig::Value JSObjectsClusterTreeConfig::kNoValue;


ConstructorHeapProfile::ConstructorHeapProfile()
    : zscope_(DELETE_ON_EXIT) {
}


void ConstructorHeapProfile::Call(const JSObjectsCluster& cluster,
                                  const NumberAndSizeInfo& number_and_size) {
  HeapStringAllocator allocator;
  StringStream stream(&allocator);
  cluster.Print(&stream);
  LOG(HeapSampleJSConstructorEvent(*(stream.ToCString()),
                                   number_and_size.number(),
                                   number_and_size.bytes()));
}


void ConstructorHeapProfile::CollectStats(HeapObject* obj) {
  Clusterizer::InsertIntoTree(&js_objects_info_tree_, obj, false);
}


void ConstructorHeapProfile::PrintStats() {
  js_objects_info_tree_.ForEach(this);
}


static const char* GetConstructorName(const char* name) {
  return name[0] != '\0' ? name : "(anonymous)";
}


const char* JSObjectsCluster::GetSpecialCaseName() const {
  if (constructor_ == FromSpecialCase(ROOTS)) {
    return "(roots)";
  } else if (constructor_ == FromSpecialCase(GLOBAL_PROPERTY)) {
    return "(global property)";
  } else if (constructor_ == FromSpecialCase(CODE)) {
    return "(code)";
  } else if (constructor_ == FromSpecialCase(SELF)) {
    return "(self)";
  }
  return NULL;
}


void JSObjectsCluster::Print(StringStream* accumulator) const {
  ASSERT(!is_null());
  const char* special_case_name = GetSpecialCaseName();
  if (special_case_name != NULL) {
    accumulator->Add(special_case_name);
  } else {
    SmartPointer<char> s_name(
        constructor_->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL));
    accumulator->Add("%s", GetConstructorName(*s_name));
    if (instance_ != NULL) {
      accumulator->Add(":%p", static_cast<void*>(instance_));
    }
  }
}


void JSObjectsCluster::DebugPrint(StringStream* accumulator) const {
  if (!is_null()) {
    Print(accumulator);
  } else {
    accumulator->Add("(null cluster)");
  }
}


inline ClustersCoarser::ClusterBackRefs::ClusterBackRefs(
    const JSObjectsCluster& cluster_)
    : cluster(cluster_), refs(kInitialBackrefsListCapacity) {
}


inline ClustersCoarser::ClusterBackRefs::ClusterBackRefs(
    const ClustersCoarser::ClusterBackRefs& src)
    : cluster(src.cluster), refs(src.refs.capacity()) {
  refs.AddAll(src.refs);
}


inline ClustersCoarser::ClusterBackRefs&
    ClustersCoarser::ClusterBackRefs::operator=(
    const ClustersCoarser::ClusterBackRefs& src) {
  if (this == &src) return *this;
  cluster = src.cluster;
  refs.Clear();
  refs.AddAll(src.refs);
  return *this;
}


inline int ClustersCoarser::ClusterBackRefs::Compare(
    const ClustersCoarser::ClusterBackRefs& a,
    const ClustersCoarser::ClusterBackRefs& b) {
  int cmp = JSObjectsCluster::CompareConstructors(a.cluster, b.cluster);
  if (cmp != 0) return cmp;
  if (a.refs.length() < b.refs.length()) return -1;
  if (a.refs.length() > b.refs.length()) return 1;
  for (int i = 0; i < a.refs.length(); ++i) {
    int cmp = JSObjectsCluster::Compare(a.refs[i], b.refs[i]);
    if (cmp != 0) return cmp;
  }
  return 0;
}


ClustersCoarser::ClustersCoarser()
    : zscope_(DELETE_ON_EXIT),
      sim_list_(ClustersCoarser::kInitialSimilarityListCapacity),
      current_pair_(NULL),
      current_set_(NULL),
      self_(NULL) {
}


void ClustersCoarser::Call(const JSObjectsCluster& cluster,
                           JSObjectsClusterTree* tree) {
  if (!cluster.can_be_coarsed()) return;
  ClusterBackRefs pair(cluster);
  ASSERT(current_pair_ == NULL);
  current_pair_ = &pair;
  current_set_ = new JSObjectsRetainerTree();
  self_ = &cluster;
  tree->ForEach(this);
  sim_list_.Add(pair);
  current_pair_ = NULL;
  current_set_ = NULL;
  self_ = NULL;
}


void ClustersCoarser::Call(const JSObjectsCluster& cluster,
                           const NumberAndSizeInfo& number_and_size) {
  ASSERT(current_pair_ != NULL);
  ASSERT(current_set_ != NULL);
  ASSERT(self_ != NULL);
  JSObjectsRetainerTree::Locator loc;
  if (JSObjectsCluster::Compare(*self_, cluster) == 0) {
    current_pair_->refs.Add(JSObjectsCluster(JSObjectsCluster::SELF));
    return;
  }
  JSObjectsCluster eq = GetCoarseEquivalent(cluster);
  if (!eq.is_null()) {
    if (current_set_->Find(eq, &loc)) return;
    current_pair_->refs.Add(eq);
    current_set_->Insert(eq, &loc);
  } else {
    current_pair_->refs.Add(cluster);
  }
}


void ClustersCoarser::Process(JSObjectsRetainerTree* tree) {
  int last_eq_clusters = -1;
  for (int i = 0; i < kMaxPassesCount; ++i) {
    sim_list_.Clear();
    const int curr_eq_clusters = DoProcess(tree);
    // If no new cluster equivalents discovered, abort processing.
    if (last_eq_clusters == curr_eq_clusters) break;
    last_eq_clusters = curr_eq_clusters;
  }
}


int ClustersCoarser::DoProcess(JSObjectsRetainerTree* tree) {
  tree->ForEach(this);
  sim_list_.Iterate(ClusterBackRefs::SortRefsIterator);
  sim_list_.Sort(ClusterBackRefsCmp);
  return FillEqualityTree();
}


JSObjectsCluster ClustersCoarser::GetCoarseEquivalent(
    const JSObjectsCluster& cluster) {
  if (!cluster.can_be_coarsed()) return JSObjectsCluster();
  EqualityTree::Locator loc;
  return eq_tree_.Find(cluster, &loc) ? loc.value() : JSObjectsCluster();
}


bool ClustersCoarser::HasAnEquivalent(const JSObjectsCluster& cluster) {
  // Return true for coarsible clusters that have a non-identical equivalent.
  if (!cluster.can_be_coarsed()) return false;
  JSObjectsCluster eq = GetCoarseEquivalent(cluster);
  return !eq.is_null() && JSObjectsCluster::Compare(cluster, eq) != 0;
}


int ClustersCoarser::FillEqualityTree() {
  int eq_clusters_count = 0;
  int eq_to = 0;
  bool first_added = false;
  for (int i = 1; i < sim_list_.length(); ++i) {
    if (ClusterBackRefs::Compare(sim_list_[i], sim_list_[eq_to]) == 0) {
      EqualityTree::Locator loc;
      if (!first_added) {
        // Add self-equivalence, if we have more than one item in this
        // equivalence class.
        eq_tree_.Insert(sim_list_[eq_to].cluster, &loc);
        loc.set_value(sim_list_[eq_to].cluster);
        first_added = true;
      }
      eq_tree_.Insert(sim_list_[i].cluster, &loc);
      loc.set_value(sim_list_[eq_to].cluster);
      ++eq_clusters_count;
    } else {
      eq_to = i;
      first_added = false;
    }
  }
  return eq_clusters_count;
}


const JSObjectsCluster ClustersCoarser::ClusterEqualityConfig::kNoKey;
const JSObjectsCluster ClustersCoarser::ClusterEqualityConfig::kNoValue;
const JSObjectsRetainerTreeConfig::Key JSObjectsRetainerTreeConfig::kNoKey;
const JSObjectsRetainerTreeConfig::Value JSObjectsRetainerTreeConfig::kNoValue =
    NULL;


RetainerHeapProfile::RetainerHeapProfile()
    : zscope_(DELETE_ON_EXIT),
      aggregator_(NULL) {
  JSObjectsCluster roots(JSObjectsCluster::ROOTS);
  ReferencesExtractor extractor(roots, this);
  Heap::IterateRoots(&extractor, VISIT_ONLY_STRONG);
}


RetainerHeapProfile::~RetainerHeapProfile() {
  delete aggregator_;
}


void RetainerHeapProfile::StoreReference(const JSObjectsCluster& cluster,
                                         HeapObject* ref) {
  JSObjectsCluster ref_cluster = Clusterizer::Clusterize(ref);
  if (ref_cluster.is_null()) return;
  JSObjectsRetainerTree::Locator ref_loc;
  if (retainers_tree_.Insert(ref_cluster, &ref_loc)) {
    ref_loc.set_value(new JSObjectsClusterTree());
  }
  JSObjectsClusterTree* referenced_by = ref_loc.value();
  Clusterizer::InsertReferenceIntoTree(referenced_by, cluster);
}


void RetainerHeapProfile::CollectStats(HeapObject* obj) {
  const JSObjectsCluster cluster = Clusterizer::Clusterize(obj);
  if (cluster.is_null()) return;
  ReferencesExtractor extractor(cluster, this);
  obj->Iterate(&extractor);
}


void RetainerHeapProfile::CoarseAndAggregate() {
  coarser_.Process(&retainers_tree_);
  ASSERT(aggregator_ == NULL);
  aggregator_ = new RetainerTreeAggregator(&coarser_);
  aggregator_->Process(&retainers_tree_);
}


void RetainerHeapProfile::DebugPrintStats(
    RetainerHeapProfile::Printer* printer) {
  // Print clusters that have no equivalents, aggregating their retainers.
  AggregatingRetainerTreePrinter agg_printer(&coarser_, printer);
  retainers_tree_.ForEach(&agg_printer);
  // Print clusters that have equivalents.
  SimpleRetainerTreePrinter s_printer(printer);
  aggregator_->output_tree().ForEach(&s_printer);
}


void RetainerHeapProfile::PrintStats() {
  RetainersPrinter printer;
  DebugPrintStats(&printer);
}


//
// HeapProfiler class implementation.
//
static void StackWeakReferenceCallback(Persistent<Value> object,
                                       void* trace) {
  DeleteArray(static_cast<Address*>(trace));
  object.Dispose();
}


static void PrintProducerStackTrace(Object* obj, void* trace) {
  if (!obj->IsJSObject()) return;
  String* constructor = JSObject::cast(obj)->constructor_name();
  SmartPointer<char> s_name(
      constructor->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL));
  LOG(HeapSampleJSProducerEvent(GetConstructorName(*s_name),
                                reinterpret_cast<Address*>(trace)));
}


void HeapProfiler::WriteSample() {
  LOG(HeapSampleBeginEvent("Heap", "allocated"));
  LOG(HeapSampleStats(
      "Heap", "allocated", Heap::CommittedMemory(), Heap::SizeOfObjects()));

  AggregatedHeapSnapshot snapshot;
  AggregatedHeapSnapshotGenerator generator(&snapshot);
  generator.GenerateSnapshot();

  HistogramInfo* info = snapshot.info();
  for (int i = FIRST_NONSTRING_TYPE;
       i <= AggregatedHeapSnapshotGenerator::kAllStringsType;
       ++i) {
    if (info[i].bytes() > 0) {
      LOG(HeapSampleItemEvent(info[i].name(), info[i].number(),
                              info[i].bytes()));
    }
  }

  snapshot.js_cons_profile()->PrintStats();
  snapshot.js_retainer_profile()->PrintStats();

  GlobalHandles::IterateWeakRoots(PrintProducerStackTrace,
                                  StackWeakReferenceCallback);

  LOG(HeapSampleEndEvent("Heap", "allocated"));
}


AggregatedHeapSnapshot::AggregatedHeapSnapshot()
    : info_(NewArray<HistogramInfo>(
        AggregatedHeapSnapshotGenerator::kAllStringsType + 1)) {
#define DEF_TYPE_NAME(name) info_[name].set_name(#name);
  INSTANCE_TYPE_LIST(DEF_TYPE_NAME);
#undef DEF_TYPE_NAME
  info_[AggregatedHeapSnapshotGenerator::kAllStringsType].set_name(
      "STRING_TYPE");
}


AggregatedHeapSnapshot::~AggregatedHeapSnapshot() {
  DeleteArray(info_);
}


AggregatedHeapSnapshotGenerator::AggregatedHeapSnapshotGenerator(
    AggregatedHeapSnapshot* agg_snapshot)
    : agg_snapshot_(agg_snapshot) {
}


void AggregatedHeapSnapshotGenerator::CalculateStringsStats() {
  HistogramInfo* info = agg_snapshot_->info();
  HistogramInfo& strings = info[kAllStringsType];
  // Lump all the string types together.
#define INCREMENT_SIZE(type, size, name, camel_name)   \
  strings.increment_number(info[type].number());       \
  strings.increment_bytes(info[type].bytes());
  STRING_TYPE_LIST(INCREMENT_SIZE);
#undef INCREMENT_SIZE
}


void AggregatedHeapSnapshotGenerator::CollectStats(HeapObject* obj) {
  InstanceType type = obj->map()->instance_type();
  ASSERT(0 <= type && type <= LAST_TYPE);
  if (!FreeListNode::IsFreeListNode(obj)) {
    agg_snapshot_->info()[type].increment_number(1);
    agg_snapshot_->info()[type].increment_bytes(obj->Size());
  }
}


void AggregatedHeapSnapshotGenerator::GenerateSnapshot() {
  HeapIterator iterator;
  for (HeapObject* obj = iterator.next(); obj != NULL; obj = iterator.next()) {
    CollectStats(obj);
    agg_snapshot_->js_cons_profile()->CollectStats(obj);
    agg_snapshot_->js_retainer_profile()->CollectStats(obj);
  }
  CalculateStringsStats();
  agg_snapshot_->js_retainer_profile()->CoarseAndAggregate();
}


class CountingConstructorHeapProfileIterator {
 public:
  CountingConstructorHeapProfileIterator()
      : entities_count_(0), children_count_(0) {
  }

  void Call(const JSObjectsCluster& cluster,
            const NumberAndSizeInfo& number_and_size) {
    ++entities_count_;
    children_count_ += number_and_size.number();
  }

  int entities_count() { return entities_count_; }
  int children_count() { return children_count_; }

 private:
  int entities_count_;
  int children_count_;
};


static HeapEntry* AddEntryFromAggregatedSnapshot(HeapSnapshot* snapshot,
                                                 int* root_child_index,
                                                 HeapEntry::Type type,
                                                 const char* name,
                                                 int count,
                                                 int size,
                                                 int children_count,
                                                 int retainers_count) {
  HeapEntry* entry = snapshot->AddEntry(
      type, name, count, size, children_count, retainers_count);
  ASSERT(entry != NULL);
  snapshot->root()->SetUnidirElementReference(*root_child_index,
                                              *root_child_index + 1,
                                              entry);
  *root_child_index = *root_child_index + 1;
  return entry;
}


class AllocatingConstructorHeapProfileIterator {
 public:
  AllocatingConstructorHeapProfileIterator(HeapSnapshot* snapshot,
                                  int* root_child_index)
      : snapshot_(snapshot),
        root_child_index_(root_child_index) {
  }

  void Call(const JSObjectsCluster& cluster,
            const NumberAndSizeInfo& number_and_size) {
    const char* name = cluster.GetSpecialCaseName();
    if (name == NULL) {
      name = snapshot_->collection()->GetFunctionName(cluster.constructor());
    }
    AddEntryFromAggregatedSnapshot(snapshot_,
                                   root_child_index_,
                                   HeapEntry::kObject,
                                   name,
                                   number_and_size.number(),
                                   number_and_size.bytes(),
                                   0,
                                   0);
  }

 private:
  HeapSnapshot* snapshot_;
  int* root_child_index_;
};


static HeapObject* ClusterAsHeapObject(const JSObjectsCluster& cluster) {
  return cluster.can_be_coarsed() ?
      reinterpret_cast<HeapObject*>(cluster.instance()) : cluster.constructor();
}


static JSObjectsCluster HeapObjectAsCluster(HeapObject* object) {
  if (object->IsString()) {
    return JSObjectsCluster(String::cast(object));
  } else {
    JSObject* js_obj = JSObject::cast(object);
    String* constructor = JSObject::cast(js_obj)->constructor_name();
    return JSObjectsCluster(constructor, object);
  }
}


class CountingRetainersIterator {
 public:
  CountingRetainersIterator(const JSObjectsCluster& child_cluster,
                            HeapEntriesMap* map)
      : child_(ClusterAsHeapObject(child_cluster)), map_(map) {
    if (map_->Map(child_) == NULL)
      map_->Pair(child_, HeapEntriesMap::kHeapEntryPlaceholder);
  }

  void Call(const JSObjectsCluster& cluster,
            const NumberAndSizeInfo& number_and_size) {
    if (map_->Map(ClusterAsHeapObject(cluster)) == NULL)
      map_->Pair(ClusterAsHeapObject(cluster),
                 HeapEntriesMap::kHeapEntryPlaceholder);
    map_->CountReference(ClusterAsHeapObject(cluster), child_);
  }

 private:
  HeapObject* child_;
  HeapEntriesMap* map_;
};


class AllocatingRetainersIterator {
 public:
  AllocatingRetainersIterator(const JSObjectsCluster& child_cluster,
                              HeapEntriesMap* map)
      : child_(ClusterAsHeapObject(child_cluster)), map_(map) {
    child_entry_ = map_->Map(child_);
    ASSERT(child_entry_ != NULL);
  }

  void Call(const JSObjectsCluster& cluster,
            const NumberAndSizeInfo& number_and_size) {
    int child_index, retainer_index;
    map_->CountReference(ClusterAsHeapObject(cluster), child_,
                         &child_index, &retainer_index);
    map_->Map(ClusterAsHeapObject(cluster))->SetElementReference(
        child_index, number_and_size.number(), child_entry_, retainer_index);
  }

 private:
  HeapObject* child_;
  HeapEntriesMap* map_;
  HeapEntry* child_entry_;
};


template<class RetainersIterator>
class AggregatingRetainerTreeIterator {
 public:
  explicit AggregatingRetainerTreeIterator(ClustersCoarser* coarser,
                                           HeapEntriesMap* map)
      : coarser_(coarser), map_(map) {
  }

  void Call(const JSObjectsCluster& cluster, JSObjectsClusterTree* tree) {
    if (coarser_ != NULL &&
        !coarser_->GetCoarseEquivalent(cluster).is_null()) return;
    JSObjectsClusterTree* tree_to_iterate = tree;
    ZoneScope zs(DELETE_ON_EXIT);
    JSObjectsClusterTree dest_tree_;
    if (coarser_ != NULL) {
      RetainersAggregator retainers_aggregator(coarser_, &dest_tree_);
      tree->ForEach(&retainers_aggregator);
      tree_to_iterate = &dest_tree_;
    }
    RetainersIterator iterator(cluster, map_);
    tree_to_iterate->ForEach(&iterator);
  }

 private:
  ClustersCoarser* coarser_;
  HeapEntriesMap* map_;
};


class AggregatedRetainerTreeAllocator {
 public:
  AggregatedRetainerTreeAllocator(HeapSnapshot* snapshot,
                                  int* root_child_index)
      : snapshot_(snapshot), root_child_index_(root_child_index) {
  }

  HeapEntry* GetEntry(
      HeapObject* obj, int children_count, int retainers_count) {
    JSObjectsCluster cluster = HeapObjectAsCluster(obj);
    const char* name = cluster.GetSpecialCaseName();
    if (name == NULL) {
      name = snapshot_->collection()->GetFunctionName(cluster.constructor());
    }
    return AddEntryFromAggregatedSnapshot(
        snapshot_, root_child_index_, HeapEntry::kObject, name,
        0, 0, children_count, retainers_count);
  }

 private:
  HeapSnapshot* snapshot_;
  int* root_child_index_;
};


template<class Iterator>
void AggregatedHeapSnapshotGenerator::IterateRetainers(
    HeapEntriesMap* entries_map) {
  RetainerHeapProfile* p = agg_snapshot_->js_retainer_profile();
  AggregatingRetainerTreeIterator<Iterator> agg_ret_iter_1(
      p->coarser(), entries_map);
  p->retainers_tree()->ForEach(&agg_ret_iter_1);
  AggregatingRetainerTreeIterator<Iterator> agg_ret_iter_2(NULL, entries_map);
  p->aggregator()->output_tree().ForEach(&agg_ret_iter_2);
}


void AggregatedHeapSnapshotGenerator::FillHeapSnapshot(HeapSnapshot* snapshot) {
  // Count the number of entities.
  int histogram_entities_count = 0;
  int histogram_children_count = 0;
  int histogram_retainers_count = 0;
  for (int i = FIRST_NONSTRING_TYPE; i <= kAllStringsType; ++i) {
    if (agg_snapshot_->info()[i].bytes() > 0) {
      ++histogram_entities_count;
    }
  }
  CountingConstructorHeapProfileIterator counting_cons_iter;
  agg_snapshot_->js_cons_profile()->ForEach(&counting_cons_iter);
  histogram_entities_count += counting_cons_iter.entities_count();
  HeapEntriesMap entries_map;
  IterateRetainers<CountingRetainersIterator>(&entries_map);
  histogram_entities_count += entries_map.entries_count();
  histogram_children_count += entries_map.total_children_count();
  histogram_retainers_count += entries_map.total_retainers_count();

  // Root entry references all other entries.
  histogram_children_count += histogram_entities_count;
  int root_children_count = histogram_entities_count;
  ++histogram_entities_count;

  // Allocate and fill entries in the snapshot, allocate references.
  snapshot->AllocateEntries(histogram_entities_count,
                            histogram_children_count,
                            histogram_retainers_count);
  snapshot->AddEntry(HeapSnapshot::kInternalRootObject,
                     root_children_count,
                     0);
  int root_child_index = 0;
  for (int i = FIRST_NONSTRING_TYPE; i <= kAllStringsType; ++i) {
    if (agg_snapshot_->info()[i].bytes() > 0) {
      AddEntryFromAggregatedSnapshot(snapshot,
                                     &root_child_index,
                                     HeapEntry::kInternal,
                                     agg_snapshot_->info()[i].name(),
                                     agg_snapshot_->info()[i].number(),
                                     agg_snapshot_->info()[i].bytes(),
                                     0,
                                     0);
    }
  }
  AllocatingConstructorHeapProfileIterator alloc_cons_iter(
      snapshot, &root_child_index);
  agg_snapshot_->js_cons_profile()->ForEach(&alloc_cons_iter);
  AggregatedRetainerTreeAllocator allocator(snapshot, &root_child_index);
  entries_map.UpdateEntries(&allocator);

  // Fill up references.
  IterateRetainers<AllocatingRetainersIterator>(&entries_map);
}


bool ProducerHeapProfile::can_log_ = false;

void ProducerHeapProfile::Setup() {
  can_log_ = true;
}

void ProducerHeapProfile::DoRecordJSObjectAllocation(Object* obj) {
  ASSERT(FLAG_log_producers);
  if (!can_log_) return;
  int framesCount = 0;
  for (JavaScriptFrameIterator it; !it.done(); it.Advance()) {
    ++framesCount;
  }
  if (framesCount == 0) return;
  ++framesCount;  // Reserve place for the terminator item.
  Vector<Address> stack(NewArray<Address>(framesCount), framesCount);
  int i = 0;
  for (JavaScriptFrameIterator it; !it.done(); it.Advance()) {
    stack[i++] = it.frame()->pc();
  }
  stack[i] = NULL;
  Handle<Object> handle = GlobalHandles::Create(obj);
  GlobalHandles::MakeWeak(handle.location(),
                          static_cast<void*>(stack.start()),
                          StackWeakReferenceCallback);
}


#endif  // ENABLE_LOGGING_AND_PROFILING


} }  // namespace v8::internal
