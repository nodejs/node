// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/debug-coverage.h"

#include "src/base/hashmap.h"
#include "src/deoptimizer.h"
#include "src/frames-inl.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

class SharedToCounterMap
    : public base::TemplateHashMapImpl<SharedFunctionInfo*, uint32_t,
                                       base::KeyEqualityMatcher<void*>,
                                       base::DefaultAllocationPolicy> {
 public:
  typedef base::TemplateHashMapEntry<SharedFunctionInfo*, uint32_t> Entry;
  inline void Add(SharedFunctionInfo* key, uint32_t count) {
    Entry* entry = LookupOrInsert(key, Hash(key), []() { return 0; });
    uint32_t old_count = entry->value;
    if (UINT32_MAX - count < old_count) {
      entry->value = UINT32_MAX;
    } else {
      entry->value = old_count + count;
    }
  }

  inline uint32_t Get(SharedFunctionInfo* key) {
    Entry* entry = Lookup(key, Hash(key));
    if (entry == nullptr) return 0;
    return entry->value;
  }

 private:
  static uint32_t Hash(SharedFunctionInfo* key) {
    return static_cast<uint32_t>(reinterpret_cast<intptr_t>(key));
  }

  DisallowHeapAllocation no_gc;
};

namespace {
int StartPosition(SharedFunctionInfo* info) {
  int start = info->function_token_position();
  if (start == kNoSourcePosition) start = info->start_position();
  return start;
}

bool CompareSharedFunctionInfo(SharedFunctionInfo* a, SharedFunctionInfo* b) {
  int a_start = StartPosition(a);
  int b_start = StartPosition(b);
  if (a_start == b_start) return a->end_position() > b->end_position();
  return a_start < b_start;
}
}  // anonymous namespace

Coverage* Coverage::CollectPrecise(Isolate* isolate) {
  DCHECK(!isolate->is_best_effort_code_coverage());
  Coverage* result = Collect(isolate, isolate->code_coverage_mode());
  if (isolate->is_precise_binary_code_coverage()) {
    // We do not have to hold onto feedback vectors for invocations we already
    // reported. So we can reset the list.
    isolate->SetCodeCoverageList(*ArrayList::New(isolate, 0));
  }
  return result;
}

Coverage* Coverage::CollectBestEffort(Isolate* isolate) {
  return Collect(isolate, v8::debug::Coverage::kBestEffort);
}

Coverage* Coverage::Collect(Isolate* isolate,
                            v8::debug::Coverage::Mode collectionMode) {
  SharedToCounterMap counter_map;

  switch (isolate->code_coverage_mode()) {
    case v8::debug::Coverage::kPreciseBinary:
    case v8::debug::Coverage::kPreciseCount: {
      bool reset_count = collectionMode != v8::debug::Coverage::kBestEffort;
      // Feedback vectors are already listed to prevent losing them to GC.
      DCHECK(isolate->factory()->code_coverage_list()->IsArrayList());
      Handle<ArrayList> list =
          Handle<ArrayList>::cast(isolate->factory()->code_coverage_list());
      for (int i = 0; i < list->Length(); i++) {
        FeedbackVector* vector = FeedbackVector::cast(list->Get(i));
        SharedFunctionInfo* shared = vector->shared_function_info();
        DCHECK(shared->IsSubjectToDebugging());
        uint32_t count = static_cast<uint32_t>(vector->invocation_count());
        if (reset_count) vector->clear_invocation_count();
        counter_map.Add(shared, count);
      }
      break;
    }
    case v8::debug::Coverage::kBestEffort: {
      DCHECK(!isolate->factory()->code_coverage_list()->IsArrayList());
      DCHECK_EQ(v8::debug::Coverage::kBestEffort, collectionMode);
      HeapIterator heap_iterator(isolate->heap());
      while (HeapObject* current_obj = heap_iterator.next()) {
        if (!current_obj->IsFeedbackVector()) continue;
        FeedbackVector* vector = FeedbackVector::cast(current_obj);
        SharedFunctionInfo* shared = vector->shared_function_info();
        if (!shared->IsSubjectToDebugging()) continue;
        uint32_t count = static_cast<uint32_t>(vector->invocation_count());
        counter_map.Add(shared, count);
      }
      break;
    }
  }

  // Iterate shared function infos of every script and build a mapping
  // between source ranges and invocation counts.
  Coverage* result = new Coverage();
  Script::Iterator scripts(isolate);
  while (Script* script = scripts.Next()) {
    if (!script->IsUserJavaScript()) continue;

    // Create and add new script data.
    Handle<Script> script_handle(script, isolate);
    result->emplace_back(script_handle);
    std::vector<CoverageFunction>* functions = &result->back().functions;

    std::vector<SharedFunctionInfo*> sorted;

    {
      // Sort functions by start position, from outer to inner functions.
      SharedFunctionInfo::ScriptIterator infos(script_handle);
      while (SharedFunctionInfo* info = infos.Next()) {
        sorted.push_back(info);
      }
      std::sort(sorted.begin(), sorted.end(), CompareSharedFunctionInfo);
    }

    // Stack to track nested functions, referring function by index.
    std::vector<size_t> nesting;

    // Use sorted list to reconstruct function nesting.
    for (SharedFunctionInfo* info : sorted) {
      int start = StartPosition(info);
      int end = info->end_position();
      uint32_t count = counter_map.Get(info);
      // Find the correct outer function based on start position.
      while (!nesting.empty() && functions->at(nesting.back()).end <= start) {
        nesting.pop_back();
      }
      if (count != 0) {
        switch (collectionMode) {
          case v8::debug::Coverage::kPreciseCount:
            break;
          case v8::debug::Coverage::kPreciseBinary:
            count = info->has_reported_binary_coverage() ? 0 : 1;
            info->set_has_reported_binary_coverage(true);
            break;
          case v8::debug::Coverage::kBestEffort:
            count = 1;
            break;
        }
      }
      // Only include a function range if it has a non-0 count, or
      // if it is directly nested inside a function with non-0 count.
      if (count != 0 ||
          (!nesting.empty() && functions->at(nesting.back()).count != 0)) {
        Handle<String> name(info->DebugName(), isolate);
        nesting.push_back(functions->size());
        functions->emplace_back(start, end, count, name);
      }
    }

    // Remove entries for scripts that have no coverage.
    if (functions->empty()) result->pop_back();
  }
  return result;
}

void Coverage::SelectMode(Isolate* isolate, debug::Coverage::Mode mode) {
  switch (mode) {
    case debug::Coverage::kBestEffort:
      isolate->SetCodeCoverageList(isolate->heap()->undefined_value());
      break;
    case debug::Coverage::kPreciseBinary:
    case debug::Coverage::kPreciseCount: {
      HandleScope scope(isolate);
      // Remove all optimized function. Optimized and inlined functions do not
      // increment invocation count.
      Deoptimizer::DeoptimizeAll(isolate);
      // Collect existing feedback vectors.
      std::vector<Handle<FeedbackVector>> vectors;
      {
        HeapIterator heap_iterator(isolate->heap());
        while (HeapObject* current_obj = heap_iterator.next()) {
          if (current_obj->IsSharedFunctionInfo()) {
            SharedFunctionInfo* shared = SharedFunctionInfo::cast(current_obj);
            shared->set_has_reported_binary_coverage(false);
          } else if (current_obj->IsFeedbackVector()) {
            FeedbackVector* vector = FeedbackVector::cast(current_obj);
            SharedFunctionInfo* shared = vector->shared_function_info();
            if (!shared->IsSubjectToDebugging()) continue;
            vectors.emplace_back(vector, isolate);
          }
        }
      }
      // Add collected feedback vectors to the root list lest we lose them to
      // GC.
      Handle<ArrayList> list =
          ArrayList::New(isolate, static_cast<int>(vectors.size()));
      for (const auto& vector : vectors) list = ArrayList::Add(list, vector);
      isolate->SetCodeCoverageList(*list);
      break;
    }
  }
  isolate->set_code_coverage_mode(mode);
}

}  // namespace internal
}  // namespace v8
