// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROFILER_SAMPLING_HEAP_PROFILER_H_
#define V8_PROFILER_SAMPLING_HEAP_PROFILER_H_

#include <deque>
#include <map>
#include <memory>
#include <unordered_map>

#include "include/v8-profiler.h"
#include "src/heap/heap.h"
#include "src/profiler/strings-storage.h"

namespace v8 {

namespace base {
class RandomNumberGenerator;
}  // namespace base

namespace internal {

class AllocationProfile : public v8::AllocationProfile {
 public:
  AllocationProfile() = default;
  AllocationProfile(const AllocationProfile&) = delete;
  AllocationProfile& operator=(const AllocationProfile&) = delete;

  v8::AllocationProfile::Node* GetRootNode() override {
    return nodes_.size() == 0 ? nullptr : &nodes_.front();
  }

  const std::vector<v8::AllocationProfile::Sample>& GetSamples() override {
    return samples_;
  }

 private:
  std::deque<v8::AllocationProfile::Node> nodes_;
  std::vector<v8::AllocationProfile::Sample> samples_;

  friend class SamplingHeapProfiler;
};

class SamplingHeapProfiler {
 public:
  class AllocationNode {
   public:
    using FunctionId = uint64_t;
    AllocationNode(AllocationNode* parent, const char* name, int script_id,
                   int start_position, uint32_t id)
        : parent_(parent),
          script_id_(script_id),
          script_position_(start_position),
          name_(name),
          id_(id) {}
    AllocationNode(const AllocationNode&) = delete;
    AllocationNode& operator=(const AllocationNode&) = delete;

    AllocationNode* FindChildNode(FunctionId id) {
      auto it = children_.find(id);
      return it != children_.end() ? it->second.get() : nullptr;
    }

    AllocationNode* AddChildNode(FunctionId id,
                                 std::unique_ptr<AllocationNode> node) {
      return children_.emplace(id, std::move(node)).first->second.get();
    }

    static FunctionId function_id(int script_id, int start_position,
                                  const char* name) {
      // script_id == kNoScriptId case:
      //   Use function name pointer as an id. Names derived from VM state
      //   must not collide with the builtin names. The least significant bit
      //   of the id is set to 1.
      if (script_id == v8::UnboundScript::kNoScriptId) {
        return reinterpret_cast<intptr_t>(name) | 1;
      }
      // script_id != kNoScriptId case:
      //   Use script_id, start_position pair to uniquelly identify the node.
      //   The least significant bit of the id is set to 0.
      DCHECK(static_cast<unsigned>(start_position) < (1u << 31));
      return (static_cast<uint64_t>(script_id) << 32) + (start_position << 1);
    }

   private:
    // TODO(alph): make use of unordered_map's here. Pay attention to
    // iterator invalidation during TranslateAllocationNode.
    std::map<size_t, unsigned int> allocations_;
    std::map<FunctionId, std::unique_ptr<AllocationNode>> children_;
    AllocationNode* const parent_;
    const int script_id_;
    const int script_position_;
    const char* const name_;
    uint32_t id_;
    bool pinned_ = false;

    friend class SamplingHeapProfiler;
  };

  struct Sample {
    Sample(size_t size_, AllocationNode* owner_, Local<Value> local_,
           SamplingHeapProfiler* profiler_, uint64_t sample_id)
        : size(size_),
          owner(owner_),
          global(reinterpret_cast<v8::Isolate*>(profiler_->isolate_), local_),
          profiler(profiler_),
          sample_id(sample_id) {}
    Sample(const Sample&) = delete;
    Sample& operator=(const Sample&) = delete;
    const size_t size;
    AllocationNode* const owner;
    Global<Value> global;
    SamplingHeapProfiler* const profiler;
    const uint64_t sample_id;
  };

  SamplingHeapProfiler(Heap* heap, StringsStorage* names, uint64_t rate,
                       int stack_depth, v8::HeapProfiler::SamplingFlags flags);
  ~SamplingHeapProfiler();
  SamplingHeapProfiler(const SamplingHeapProfiler&) = delete;
  SamplingHeapProfiler& operator=(const SamplingHeapProfiler&) = delete;

  v8::AllocationProfile* GetAllocationProfile();
  StringsStorage* names() const { return names_; }

 private:
  class Observer : public AllocationObserver {
   public:
    Observer(Heap* heap, intptr_t step_size, uint64_t rate,
             SamplingHeapProfiler* profiler,
             base::RandomNumberGenerator* random)
        : AllocationObserver(step_size),
          profiler_(profiler),
          heap_(heap),
          random_(random),
          rate_(rate) {}

   protected:
    void Step(int bytes_allocated, Address soon_object, size_t size) override {
      USE(heap_);
      DCHECK(heap_->gc_state() == Heap::NOT_IN_GC);
      if (soon_object) {
        // TODO(ofrobots): it would be better to sample the next object rather
        // than skipping this sample epoch if soon_object happens to be null.
        profiler_->SampleObject(soon_object, size);
      }
    }

    intptr_t GetNextStepSize() override { return GetNextSampleInterval(rate_); }

   private:
    intptr_t GetNextSampleInterval(uint64_t rate);
    SamplingHeapProfiler* const profiler_;
    Heap* const heap_;
    base::RandomNumberGenerator* const random_;
    uint64_t const rate_;
  };

  void SampleObject(Address soon_object, size_t size);

  const std::vector<v8::AllocationProfile::Sample> BuildSamples() const;

  AllocationNode* FindOrAddChildNode(AllocationNode* parent, const char* name,
                                     int script_id, int start_position);
  static void OnWeakCallback(const WeakCallbackInfo<Sample>& data);

  uint32_t next_node_id() { return ++last_node_id_; }
  uint64_t next_sample_id() { return ++last_sample_id_; }

  // Methods that construct v8::AllocationProfile.

  // Translates the provided AllocationNode *node* returning an equivalent
  // AllocationProfile::Node. The newly created AllocationProfile::Node is added
  // to the provided AllocationProfile *profile*. Line numbers, column numbers,
  // and script names are resolved using *scripts* which maps all currently
  // loaded scripts keyed by their script id.
  v8::AllocationProfile::Node* TranslateAllocationNode(
      AllocationProfile* profile, SamplingHeapProfiler::AllocationNode* node,
      const std::map<int, Handle<Script>>& scripts);
  v8::AllocationProfile::Allocation ScaleSample(size_t size,
                                                unsigned int count) const;
  AllocationNode* AddStack();

  Isolate* const isolate_;
  Heap* const heap_;
  uint64_t last_sample_id_ = 0;
  uint32_t last_node_id_ = 0;
  Observer allocation_observer_;
  StringsStorage* const names_;
  AllocationNode profile_root_;
  std::unordered_map<Sample*, std::unique_ptr<Sample>> samples_;
  const int stack_depth_;
  const uint64_t rate_;
  v8::HeapProfiler::SamplingFlags flags_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PROFILER_SAMPLING_HEAP_PROFILER_H_
