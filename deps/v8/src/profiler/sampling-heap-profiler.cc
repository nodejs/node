// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/profiler/sampling-heap-profiler.h"

#include <stdint.h>
#include <memory>
#include "src/api.h"
#include "src/base/ieee754.h"
#include "src/base/utils/random-number-generator.h"
#include "src/frames-inl.h"
#include "src/heap/heap.h"
#include "src/isolate.h"
#include "src/profiler/strings-storage.h"

namespace v8 {
namespace internal {

// We sample with a Poisson process, with constant average sampling interval.
// This follows the exponential probability distribution with parameter
// λ = 1/rate where rate is the average number of bytes between samples.
//
// Let u be a uniformly distributed random number between 0 and 1, then
// next_sample = (- ln u) / λ
intptr_t SamplingAllocationObserver::GetNextSampleInterval(uint64_t rate) {
  if (FLAG_sampling_heap_profiler_suppress_randomness) {
    return static_cast<intptr_t>(rate);
  }
  double u = random_->NextDouble();
  double next = (-base::ieee754::log(u)) * rate;
  return next < kPointerSize
             ? kPointerSize
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
    size_t size, unsigned int count) {
  double scale = 1.0 / (1.0 - std::exp(-static_cast<double>(size) / rate_));
  // Round count instead of truncating.
  return {size, static_cast<unsigned int>(count * scale + 0.5)};
}

SamplingHeapProfiler::SamplingHeapProfiler(
    Heap* heap, StringsStorage* names, uint64_t rate, int stack_depth,
    v8::HeapProfiler::SamplingFlags flags)
    : isolate_(heap->isolate()),
      heap_(heap),
      new_space_observer_(new SamplingAllocationObserver(
          heap_, static_cast<intptr_t>(rate), rate, this,
          heap->isolate()->random_number_generator())),
      other_spaces_observer_(new SamplingAllocationObserver(
          heap_, static_cast<intptr_t>(rate), rate, this,
          heap->isolate()->random_number_generator())),
      names_(names),
      profile_root_(nullptr, "(root)", v8::UnboundScript::kNoScriptId, 0),
      samples_(),
      stack_depth_(stack_depth),
      rate_(rate),
      flags_(flags) {
  CHECK_GT(rate_, 0u);
  heap->new_space()->AddAllocationObserver(new_space_observer_.get());
  AllSpaces spaces(heap);
  for (Space* space = spaces.next(); space != nullptr; space = spaces.next()) {
    if (space != heap->new_space()) {
      space->AddAllocationObserver(other_spaces_observer_.get());
    }
  }
}


SamplingHeapProfiler::~SamplingHeapProfiler() {
  heap_->new_space()->RemoveAllocationObserver(new_space_observer_.get());
  AllSpaces spaces(heap_);
  for (Space* space = spaces.next(); space != nullptr; space = spaces.next()) {
    if (space != heap_->new_space()) {
      space->RemoveAllocationObserver(other_spaces_observer_.get());
    }
  }

  for (auto sample : samples_) {
    delete sample;
  }
  std::set<Sample*> empty;
  samples_.swap(empty);
}


void SamplingHeapProfiler::SampleObject(Address soon_object, size_t size) {
  DisallowHeapAllocation no_allocation;

  HandleScope scope(isolate_);
  HeapObject* heap_object = HeapObject::FromAddress(soon_object);
  Handle<Object> obj(heap_object, isolate_);

  // Mark the new block as FreeSpace to make sure the heap is iterable while we
  // are taking the sample.
  heap()->CreateFillerObjectAt(soon_object, static_cast<int>(size),
                               ClearRecordedSlots::kNo);

  Local<v8::Value> loc = v8::Utils::ToLocal(obj);

  AllocationNode* node = AddStack();
  node->allocations_[size]++;
  Sample* sample = new Sample(size, node, loc, this);
  samples_.insert(sample);
  sample->global.SetWeak(sample, OnWeakCallback, WeakCallbackType::kParameter);
  sample->global.MarkIndependent();
}

void SamplingHeapProfiler::OnWeakCallback(
    const WeakCallbackInfo<Sample>& data) {
  Sample* sample = data.GetParameter();
  AllocationNode* node = sample->owner;
  DCHECK(node->allocations_[sample->size] > 0);
  node->allocations_[sample->size]--;
  if (node->allocations_[sample->size] == 0) {
    node->allocations_.erase(sample->size);
    while (node->allocations_.empty() && node->children_.empty() &&
           node->parent_ && !node->parent_->pinned_) {
      AllocationNode* parent = node->parent_;
      AllocationNode::FunctionId id = AllocationNode::function_id(
          node->script_id_, node->script_position_, node->name_);
      parent->children_.erase(id);
      delete node;
      node = parent;
    }
  }
  sample->profiler->samples_.erase(sample);
  delete sample;
}

SamplingHeapProfiler::AllocationNode*
SamplingHeapProfiler::AllocationNode::FindOrAddChildNode(const char* name,
                                                         int script_id,
                                                         int start_position) {
  FunctionId id = function_id(script_id, start_position, name);
  auto it = children_.find(id);
  if (it != children_.end()) {
    DCHECK(strcmp(it->second->name_, name) == 0);
    return it->second;
  }
  auto child = new AllocationNode(this, name, script_id, start_position);
  children_.insert(std::make_pair(id, child));
  return child;
}

SamplingHeapProfiler::AllocationNode* SamplingHeapProfiler::AddStack() {
  AllocationNode* node = &profile_root_;

  std::vector<SharedFunctionInfo*> stack;
  JavaScriptFrameIterator it(isolate_);
  int frames_captured = 0;
  while (!it.done() && frames_captured < stack_depth_) {
    JavaScriptFrame* frame = it.frame();
    SharedFunctionInfo* shared = frame->function()->shared();
    stack.push_back(shared);

    frames_captured++;
    it.Advance();
  }

  if (frames_captured == 0) {
    const char* name = nullptr;
    switch (isolate_->current_vm_state()) {
      case GC:
        name = "(GC)";
        break;
      case COMPILER:
        name = "(COMPILER)";
        break;
      case OTHER:
        name = "(V8 API)";
        break;
      case EXTERNAL:
        name = "(EXTERNAL)";
        break;
      case IDLE:
        name = "(IDLE)";
        break;
      case JS:
        name = "(JS)";
        break;
    }
    return node->FindOrAddChildNode(name, v8::UnboundScript::kNoScriptId, 0);
  }

  // We need to process the stack in reverse order as the top of the stack is
  // the first element in the list.
  for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
    SharedFunctionInfo* shared = *it;
    const char* name = this->names()->GetFunctionName(shared->DebugName());
    int script_id = v8::UnboundScript::kNoScriptId;
    if (shared->script()->IsScript()) {
      Script* script = Script::cast(shared->script());
      script_id = script->id();
    }
    node = node->FindOrAddChildNode(name, script_id, shared->start_position());
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
  if (node->script_id_ != v8::UnboundScript::kNoScriptId &&
      scripts.find(node->script_id_) != scripts.end()) {
    // Cannot use std::map<T>::at because it is not available on android.
    auto non_const_scripts =
        const_cast<std::map<int, Handle<Script>>&>(scripts);
    Handle<Script> script = non_const_scripts[node->script_id_];
    if (!script.is_null()) {
      if (script->name()->IsName()) {
        Name* name = Name::cast(script->name());
        script_name = ToApiHandle<v8::String>(
            isolate_->factory()->InternalizeUtf8String(names_->GetName(name)));
      }
      line = 1 + Script::GetLineNumber(script, node->script_position_);
      column = 1 + Script::GetColumnNumber(script, node->script_position_);
    }
  }
  for (auto alloc : node->allocations_) {
    allocations.push_back(ScaleSample(alloc.first, alloc.second));
  }

  profile->nodes().push_back(v8::AllocationProfile::Node(
      {ToApiHandle<v8::String>(
           isolate_->factory()->InternalizeUtf8String(node->name_)),
       script_name, node->script_id_, node->script_position_, line, column,
       std::vector<v8::AllocationProfile::Node*>(), allocations}));
  v8::AllocationProfile::Node* current = &profile->nodes().back();
  // The children map may have nodes inserted into it during translation
  // because the translation may allocate strings on the JS heap that have
  // the potential to be sampled. That's ok since map iterators are not
  // invalidated upon std::map insertion.
  for (auto it : node->children_) {
    current->children.push_back(
        TranslateAllocationNode(profile, it.second, scripts));
  }
  node->pinned_ = false;
  return current;
}

v8::AllocationProfile* SamplingHeapProfiler::GetAllocationProfile() {
  if (flags_ & v8::HeapProfiler::kSamplingForceGC) {
    isolate_->heap()->CollectAllGarbage(
        Heap::kNoGCFlags, GarbageCollectionReason::kSamplingProfiler);
  }
  // To resolve positions to line/column numbers, we will need to look up
  // scripts. Build a map to allow fast mapping from script id to script.
  std::map<int, Handle<Script>> scripts;
  {
    Script::Iterator iterator(isolate_);
    while (Script* script = iterator.Next()) {
      scripts[script->id()] = handle(script);
    }
  }
  auto profile = new v8::internal::AllocationProfile();
  TranslateAllocationNode(profile, &profile_root_, scripts);
  return profile;
}


}  // namespace internal
}  // namespace v8
