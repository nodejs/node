// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/profiler/sampling-heap-profiler.h"

#include <stdint.h>

#include <memory>

#include "src/api/api-inl.h"
#include "src/base/ieee754.h"
#include "src/base/utils/random-number-generator.h"
#include "src/execution/frames-inl.h"
#include "src/execution/isolate.h"
#include "src/heap/heap-layout-inl.h"
#include "src/heap/heap.h"
#include "src/objects/js-collection-inl.h"
#include "src/objects/ordered-hash-table.h"
#include "src/profiler/heap-profiler.h"
#include "src/profiler/strings-storage.h"

namespace v8 {
namespace internal {

// We sample with a Poisson process, with constant average sampling interval.
// This follows the exponential probability distribution with parameter
// λ = 1/rate where rate is the average number of bytes between samples.
//
// Let u be a uniformly distributed random number between 0 and 1, then
// next_sample = (- ln u) / λ
intptr_t SamplingHeapProfiler::Observer::GetNextSampleInterval(uint64_t rate) {
  if (v8_flags.sampling_heap_profiler_suppress_randomness)
    return static_cast<intptr_t>(rate);
  double u = random_->NextDouble();
  double next = (-base::ieee754::log(u)) * rate;
  return next < kTaggedSize
             ? kTaggedSize
             : (next > INT_MAX ? INT_MAX : static_cast<intptr_t>(next));
}

// Samples were collected according to a poisson process. Since we have not
// recorded all allocations, we must approximate the shape of the underlying
// space of allocations based on the samples we have collected. Given that
// we sample at rate R, the probability that an allocation of size S will be
// sampled is 1-exp(-S/R). This function uses the above probability to
// approximate the true number of allocations with size *size* given that
// *count* samples were observed.
v8::AllocationProfile::Allocation SamplingHeapProfiler::ScaleSample(
    size_t size, unsigned int count) const {
  double scale = 1.0 / (1.0 - std::exp(-static_cast<double>(size) / rate_));
  // Round count instead of truncating.
  return {size, static_cast<unsigned int>(count * scale + 0.5)};
}

SamplingHeapProfiler::SamplingHeapProfiler(
    Heap* heap, StringsStorage* names, uint64_t rate, int stack_depth,
    v8::HeapProfiler::SamplingFlags flags)
    : isolate_(Isolate::FromHeap(heap)),
      heap_(heap),
      allocation_observer_(heap_, static_cast<intptr_t>(rate), rate, this,
                           isolate_->random_number_generator()),
      names_(names),
      profile_root_(nullptr, "(root)", v8::UnboundScript::kNoScriptId, 0,
                    next_node_id()),
      stack_depth_(stack_depth),
      rate_(rate),
      flags_(flags) {
  CHECK_GT(rate_, 0u);
  heap_->AddAllocationObserversToAllSpaces(&allocation_observer_,
                                           &allocation_observer_);
}

SamplingHeapProfiler::~SamplingHeapProfiler() {
  heap_->RemoveAllocationObserversFromAllSpaces(&allocation_observer_,
                                                &allocation_observer_);
}

void SamplingHeapProfiler::SampleObject(Address soon_object, size_t size) {
  DisallowGarbageCollection no_gc;

  // Check if the area is iterable by confirming that it starts with a map.
  DCHECK(IsMap(HeapObject::FromAddress(soon_object)->map(isolate_), isolate_));

  HandleScope scope(isolate_);
  Tagged<HeapObject> heap_object = HeapObject::FromAddress(soon_object);
  Handle<Object> obj(heap_object, isolate_);

  // Since soon_object can be in code space or trusted space we can't use
  // v8::Utils::ToLocal.
  DCHECK(obj.is_null() ||
         (IsSmi(*obj) ||
          (V8_EXTERNAL_CODE_SPACE_BOOL &&
           TrustedHeapLayout::InCodeSpace(heap_object)) ||
          TrustedHeapLayout::InTrustedSpace(heap_object) || !IsTheHole(*obj)));
  auto loc = Local<v8::Value>::FromSlot(obj.location());

  AllocationNode* node = AddStack();
  node->allocations_[size]++;
  auto sample =
      std::make_unique<Sample>(size, node, loc, this, next_sample_id());

#ifdef V8_HEAP_PROFILER_SAMPLE_LABELS
  // Extract the ALS value from the CPED Map at allocation time.
  // The CPED (ContinuationPreservedEmbedderData) is a JSMap containing
  // ALL ALS stores.  We look up only our ALS key via
  // OrderedHashMap::FindEntry (zero-allocation, GC-safe) and store just
  // the resulting value (the flat label array).  This avoids retaining
  // the entire CPED for the lifetime of each sample.
  // Global::Reset is safe inside DisallowGC (uses malloc, not V8 heap).
  {
    HeapProfiler* hp = isolate_->heap()->heap_profiler();
    if (hp->sample_labels_callback()) {
      const v8::Global<v8::Value>& als_key_global =
          hp->sample_labels_als_key();
      if (!als_key_global.IsEmpty()) {
        v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate_);
        v8::Local<v8::Value> context =
            v8_isolate->GetContinuationPreservedEmbedderData();
        if (!context.IsEmpty() && !context->IsUndefined()) {
          Tagged<Object> cped_obj = *Utils::OpenDirectHandle(*context);
          if (IsJSMap(cped_obj)) {
            Tagged<JSMap> js_map = Cast<JSMap>(cped_obj);
            Tagged<OrderedHashMap> table =
                Cast<OrderedHashMap>(js_map->table());
            v8::Local<v8::Value> als_key_local =
                als_key_global.Get(v8_isolate);
            Tagged<Object> key_obj = *Utils::OpenDirectHandle(*als_key_local);
            InternalIndex entry = table->FindEntry(isolate_, key_obj);
            if (entry.is_found()) {
              Tagged<Object> value = table->ValueAt(entry);
              v8::Local<v8::Value> value_local =
                  Utils::ToLocal(direct_handle(value, isolate_));
              sample->label_value.Reset(v8_isolate, value_local);
            }
          }
        }
      }
    }
  }
#endif  // V8_HEAP_PROFILER_SAMPLE_LABELS

  sample->global.SetWeak(sample.get(), OnWeakCallback,
                         WeakCallbackType::kParameter);
  samples_.emplace(sample.get(), std::move(sample));
}

void SamplingHeapProfiler::OnWeakCallback(
    const WeakCallbackInfo<Sample>& data) {
  Sample* sample = data.GetParameter();
  Heap* heap = reinterpret_cast<Isolate*>(data.GetIsolate())->heap();
  bool is_minor_gc = Heap::IsYoungGenerationCollector(
      heap->current_or_last_garbage_collector());
  bool should_keep_sample =
      is_minor_gc
          ? (sample->profiler->flags_ &
             v8::HeapProfiler::kSamplingIncludeObjectsCollectedByMinorGC)
          : (sample->profiler->flags_ &
             v8::HeapProfiler::kSamplingIncludeObjectsCollectedByMajorGC);
  if (should_keep_sample) {
    sample->global.Reset();
    return;
  }
  AllocationNode* node = sample->owner;
  DCHECK_GT(node->allocations_[sample->size], 0);
  node->allocations_[sample->size]--;
  if (node->allocations_[sample->size] == 0) {
    node->allocations_.erase(sample->size);
    while (node->allocations_.empty() && node->children_.empty() &&
           node->parent_ && !node->parent_->pinned_) {
      AllocationNode* parent = node->parent_;
      AllocationNode::FunctionId id = AllocationNode::function_id(
          node->script_id_, node->script_position_, node->name_);
      parent->children_.erase(id);
      node = parent;
    }
  }
  sample->profiler->samples_.erase(sample);
  // sample is deleted because its unique ptr was erased from samples_.
}

SamplingHeapProfiler::AllocationNode* SamplingHeapProfiler::FindOrAddChildNode(
    AllocationNode* parent, const char* name, int script_id,
    int start_position) {
  AllocationNode::FunctionId id =
      AllocationNode::function_id(script_id, start_position, name);
  AllocationNode* child = parent->FindChildNode(id);
  if (child) {
    DCHECK_EQ(strcmp(child->name_, name), 0);
    return child;
  }
  auto new_child = std::make_unique<AllocationNode>(
      parent, name, script_id, start_position, next_node_id());
  return parent->AddChildNode(id, std::move(new_child));
}

SamplingHeapProfiler::AllocationNode* SamplingHeapProfiler::AddStack() {
  AllocationNode* node = &profile_root_;

  std::vector<Tagged<SharedFunctionInfo>> stack;
  JavaScriptStackFrameIterator frame_it(isolate_);
  int frames_captured = 0;
  bool found_arguments_marker_frames = false;
  while (!frame_it.done() && frames_captured < stack_depth_) {
    JavaScriptFrame* frame = frame_it.frame();
    // If we are materializing objects during deoptimization, inlined
    // closures may not yet be materialized, and this includes the
    // closure on the stack. Skip over any such frames (they'll be
    // in the top frames of the stack). The allocations made in this
    // sensitive moment belong to the formerly optimized frame anyway.
    if (IsJSFunction(frame->unchecked_function())) {
      Tagged<SharedFunctionInfo> shared = frame->function()->shared();
      stack.push_back(shared);
      frames_captured++;
    } else {
      found_arguments_marker_frames = true;
    }
    frame_it.Advance();
  }

  if (frames_captured == 0) {
    const char* name = nullptr;
    switch (isolate_->current_vm_state()) {
      case GC:
        name = "(GC)";
        break;
      case PARSER:
        name = "(PARSER)";
        break;
      case COMPILER:
        name = "(COMPILER)";
        break;
      case BYTECODE_COMPILER:
        name = "(BYTECODE_COMPILER)";
        break;
      case OTHER:
        name = "(V8 API)";
        break;
      case EXTERNAL:
        name = "(EXTERNAL)";
        break;
      case LOGGING:
        name = "(LOGGING)";
        break;
      case IDLE_EXTERNAL:
        name = "(IDLE_EXTERNAL)";
        break;
      case IDLE:
        name = "(IDLE)";
        break;
      // Treat atomics wait as a normal JS event; we don't care about the
      // difference for allocations.
      case ATOMICS_WAIT:
      case JS:
        name = "(JS)";
        break;
    }
    return FindOrAddChildNode(node, name, v8::UnboundScript::kNoScriptId, 0);
  }

  // We need to process the stack in reverse order as the top of the stack is
  // the first element in the list.
  for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
    Tagged<SharedFunctionInfo> shared = *it;
    const char* name = this->names()->GetCopy(shared->DebugNameCStr().get());
    int script_id = v8::UnboundScript::kNoScriptId;
    if (IsScript(shared->script())) {
      Tagged<Script> script = Cast<Script>(shared->script());
      script_id = script->id();
    }
    node = FindOrAddChildNode(node, name, script_id, shared->StartPosition());
  }

  if (found_arguments_marker_frames) {
    node =
        FindOrAddChildNode(node, "(deopt)", v8::UnboundScript::kNoScriptId, 0);
  }

  return node;
}

v8::AllocationProfile::Node* SamplingHeapProfiler::TranslateAllocationNode(
    AllocationProfile* profile, SamplingHeapProfiler::AllocationNode* node,
    const std::map<int, Handle<Script>>& scripts) {
  // By pinning the node we make sure its children won't get disposed if
  // a GC kicks in during the tree retrieval.
  node->pinned_ = true;
  Local<v8::String> script_name =
      ToApiHandle<v8::String>(isolate_->factory()->InternalizeUtf8String(""));
  int line = v8::AllocationProfile::kNoLineNumberInfo;
  int column = v8::AllocationProfile::kNoColumnNumberInfo;
  std::vector<v8::AllocationProfile::Allocation> allocations;
  allocations.reserve(node->allocations_.size());
  if (node->script_id_ != v8::UnboundScript::kNoScriptId) {
    auto script_iterator = scripts.find(node->script_id_);
    if (script_iterator != scripts.end()) {
      DirectHandle<Script> script = script_iterator->second;
      if (IsName(script->name())) {
        Tagged<Name> name = Cast<Name>(script->name());
        script_name = ToApiHandle<v8::String>(
            isolate_->factory()->InternalizeUtf8String(names_->GetName(name)));
      }
      Script::PositionInfo pos_info;
      Script::GetPositionInfo(script, node->script_position_, &pos_info);
      line = pos_info.line + 1;
      column = pos_info.column + 1;
    }
  }
  for (auto alloc : node->allocations_) {
    allocations.push_back(ScaleSample(alloc.first, alloc.second));
  }

  profile->nodes_.push_back(v8::AllocationProfile::Node{
      ToApiHandle<v8::String>(
          isolate_->factory()->InternalizeUtf8String(node->name_)),
      script_name, node->script_id_, node->script_position_, line, column,
      node->id_, std::vector<v8::AllocationProfile::Node*>(), allocations});
  v8::AllocationProfile::Node* current = &profile->nodes_.back();
  // The |children_| map may have nodes inserted into it during translation
  // because the translation may allocate strings on the JS heap that have
  // the potential to be sampled. That's ok since map iterators are not
  // invalidated upon std::map insertion.
  for (const auto& it : node->children_) {
    current->children.push_back(
        TranslateAllocationNode(profile, it.second.get(), scripts));
  }
  node->pinned_ = false;
  return current;
}

v8::AllocationProfile* SamplingHeapProfiler::GetAllocationProfile() {
  if (flags_ & v8::HeapProfiler::kSamplingForceGC) {
    isolate_->heap()->CollectAllGarbage(
        GCFlag::kNoFlags, GarbageCollectionReason::kSamplingProfiler);
  }
  // To resolve positions to line/column numbers, we will need to look up
  // scripts. Build a map to allow fast mapping from script id to script.
  std::map<int, Handle<Script>> scripts;
  {
    Script::Iterator iterator(isolate_);
    for (Tagged<Script> script = iterator.Next(); !script.is_null();
         script = iterator.Next()) {
      scripts[script->id()] = handle(script, isolate_);
    }
  }

#ifdef V8_HEAP_PROFILER_SAMPLE_LABELS
  // Pre-resolve labels while GC is still allowed.
  // The labels callback may allocate on the V8 heap. These MUST NOT run
  // during BuildSamples() iteration — GC would fire weak callbacks that
  // erase from samples_, causing iterator invalidation / UAF.
  //
  // Two-phase approach:
  //   Phase 1: snapshot sample IDs and ALS values (Global::Get + Global
  //            ctor use malloc, not V8 heap — no GC risk during iteration).
  //   Phase 2: invoke the callback for each snapshot entry. GC may fire
  //            here but we iterate our snapshot vector, not samples_.
  ResolvedLabelsMap resolved_labels;
  {
    HeapProfiler* hp = heap_->heap_profiler();
    auto callback = hp->sample_labels_callback();
    void* callback_data = hp->sample_labels_data();
    if (callback) {
      v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate_);

      // Phase 1: snapshot ALS values keyed by sample_id.
      // Global ctor (GlobalizeReference) and Global::Get use global handle
      // storage (malloc), not the V8 heap — safe under DisallowGarbageCollection.
      // Locals from Get() are immediately consumed by Global ctor, so a single
      // HandleScope outside the loop suffices (no per-iteration scope churn).
      struct LabelValueSnapshot {
        uint64_t sample_id;
        v8::Global<v8::Value> label_value;
      };
      std::vector<LabelValueSnapshot> snapshot;
      snapshot.reserve(samples_.size());
      {
        HandleScope handle_scope(isolate_);
        DisallowGarbageCollection no_gc;
        for (const auto& [ptr, sample] : samples_) {
          if (!sample->label_value.IsEmpty()) {
            snapshot.push_back(
                {sample->sample_id,
                 v8::Global<v8::Value>(
                     v8_isolate, sample->label_value.Get(v8_isolate))});
          }
        }
      }

      // Phase 2: resolve labels via callback (GC allowed).
      // The callback receives the ALS value (flat label array) directly —
      // the CPED Map lookup was already done at allocation time.
      //
      // Per-call cache: samples sharing the same ALS value (e.g. all
      // allocations under one withHeapProfileLabels scope) resolve to the
      // same labels, so we dedup callback invocations within this call.
      // Key: raw object Address (cheap identity check). If GC moves the
      // object between iterations we get a cache miss and re-call the
      // callback — correctness preserved, just less optimal. Negative
      // results (callback returned false or empty labels) are also cached
      // so we don't re-call for ALS values that resolve to nothing.
      std::unordered_map<Address,
                         std::vector<std::pair<std::string, std::string>>>
          label_cache;
      for (auto& entry : snapshot) {
        HandleScope scope(isolate_);
        v8::Local<v8::Value> value_local = entry.label_value.Get(v8_isolate);
        Address key = (*Utils::OpenDirectHandle(*value_local)).ptr();
        auto [cache_it, inserted] = label_cache.try_emplace(key);
        if (inserted) {
          std::vector<std::pair<std::string, std::string>> labels;
          if (callback(callback_data, value_local, &labels) &&
              !labels.empty()) {
            cache_it->second = std::move(labels);
          }
        }
        if (!cache_it->second.empty()) {
          resolved_labels[entry.sample_id] = cache_it->second;
        }
      }
    }
  }
#endif

  auto profile = new v8::internal::AllocationProfile();
  TranslateAllocationNode(profile, &profile_root_, scripts);

#ifdef V8_HEAP_PROFILER_SAMPLE_LABELS
  profile->samples_ = BuildSamples(std::move(resolved_labels));
#else
  profile->samples_ = BuildSamples();
#endif

  return profile;
}

#ifdef V8_HEAP_PROFILER_SAMPLE_LABELS
const std::vector<v8::AllocationProfile::Sample>
SamplingHeapProfiler::BuildSamples(ResolvedLabelsMap resolved_labels) const {
#else
const std::vector<v8::AllocationProfile::Sample>
SamplingHeapProfiler::BuildSamples() const {
#endif
  std::vector<v8::AllocationProfile::Sample> samples;
  samples.reserve(samples_.size());

  for (const auto& it : samples_) {
    const Sample* sample = it.second.get();
#ifdef V8_HEAP_PROFILER_SAMPLE_LABELS
    std::vector<std::pair<std::string, std::string>> labels;
    auto label_it = resolved_labels.find(sample->sample_id);
    if (label_it != resolved_labels.end()) {
      labels = std::move(label_it->second);
    }
    samples.emplace_back(sample->owner->id_, sample->size,
                         ScaleSample(sample->size, 1).count,
                         sample->sample_id, std::move(labels));
#else
    samples.emplace_back(sample->owner->id_, sample->size,
                         ScaleSample(sample->size, 1).count,
                         sample->sample_id);
#endif
  }
  return samples;
}

}  // namespace internal
}  // namespace v8
