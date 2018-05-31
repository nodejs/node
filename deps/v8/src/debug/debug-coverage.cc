// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/debug-coverage.h"

#include "src/ast/ast.h"
#include "src/base/hashmap.h"
#include "src/debug/debug.h"
#include "src/deoptimizer.h"
#include "src/frames-inl.h"
#include "src/isolate.h"
#include "src/objects.h"
#include "src/objects/debug-objects-inl.h"

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
  if (start == kNoSourcePosition) start = info->StartPosition();
  return start;
}

bool CompareSharedFunctionInfo(SharedFunctionInfo* a, SharedFunctionInfo* b) {
  int a_start = StartPosition(a);
  int b_start = StartPosition(b);
  if (a_start == b_start) return a->EndPosition() > b->EndPosition();
  return a_start < b_start;
}

bool CompareCoverageBlock(const CoverageBlock& a, const CoverageBlock& b) {
  DCHECK_NE(kNoSourcePosition, a.start);
  DCHECK_NE(kNoSourcePosition, b.start);
  if (a.start == b.start) return a.end > b.end;
  return a.start < b.start;
}

void SortBlockData(std::vector<CoverageBlock>& v) {
  // Sort according to the block nesting structure.
  std::sort(v.begin(), v.end(), CompareCoverageBlock);
}

std::vector<CoverageBlock> GetSortedBlockData(Isolate* isolate,
                                              SharedFunctionInfo* shared) {
  DCHECK(shared->HasCoverageInfo());

  CoverageInfo* coverage_info =
      CoverageInfo::cast(shared->GetDebugInfo()->coverage_info());

  std::vector<CoverageBlock> result;
  if (coverage_info->SlotCount() == 0) return result;

  for (int i = 0; i < coverage_info->SlotCount(); i++) {
    const int start_pos = coverage_info->StartSourcePosition(i);
    const int until_pos = coverage_info->EndSourcePosition(i);
    const int count = coverage_info->BlockCount(i);

    DCHECK_NE(kNoSourcePosition, start_pos);
    result.emplace_back(start_pos, until_pos, count);
  }

  SortBlockData(result);

  return result;
}

// A utility class to simplify logic for performing passes over block coverage
// ranges. Provides access to the implicit tree structure of ranges (i.e. access
// to parent and sibling blocks), and supports efficient in-place editing and
// deletion. The underlying backing store is the array of CoverageBlocks stored
// on the CoverageFunction.
class CoverageBlockIterator final {
 public:
  explicit CoverageBlockIterator(CoverageFunction* function)
      : function_(function),
        ended_(false),
        delete_current_(false),
        read_index_(-1),
        write_index_(-1) {
    DCHECK(std::is_sorted(function_->blocks.begin(), function_->blocks.end(),
                          CompareCoverageBlock));
  }

  ~CoverageBlockIterator() {
    Finalize();
    DCHECK(std::is_sorted(function_->blocks.begin(), function_->blocks.end(),
                          CompareCoverageBlock));
  }

  bool HasNext() const {
    return read_index_ + 1 < static_cast<int>(function_->blocks.size());
  }

  bool Next() {
    if (!HasNext()) {
      if (!ended_) MaybeWriteCurrent();
      ended_ = true;
      return false;
    }

    // If a block has been deleted, subsequent iteration moves trailing blocks
    // to their updated position within the array.
    MaybeWriteCurrent();

    if (read_index_ == -1) {
      // Initialize the nesting stack with the function range.
      nesting_stack_.emplace_back(function_->start, function_->end,
                                  function_->count);
    } else if (!delete_current_) {
      nesting_stack_.emplace_back(GetBlock());
    }

    delete_current_ = false;
    read_index_++;

    DCHECK(IsActive());

    CoverageBlock& block = GetBlock();
    while (nesting_stack_.size() > 1 &&
           nesting_stack_.back().end <= block.start) {
      nesting_stack_.pop_back();
    }

    DCHECK_IMPLIES(block.start >= function_->end,
                   block.end == kNoSourcePosition);
    DCHECK_NE(block.start, kNoSourcePosition);
    DCHECK_LE(block.end, GetParent().end);

    return true;
  }

  CoverageBlock& GetBlock() {
    DCHECK(IsActive());
    return function_->blocks[read_index_];
  }

  CoverageBlock& GetNextBlock() {
    DCHECK(IsActive());
    DCHECK(HasNext());
    return function_->blocks[read_index_ + 1];
  }

  CoverageBlock& GetParent() {
    DCHECK(IsActive());
    return nesting_stack_.back();
  }

  bool HasSiblingOrChild() {
    DCHECK(IsActive());
    return HasNext() && GetNextBlock().start < GetParent().end;
  }

  CoverageBlock& GetSiblingOrChild() {
    DCHECK(HasSiblingOrChild());
    DCHECK(IsActive());
    return GetNextBlock();
  }

  // A range is considered to be at top level if its parent range is the
  // function range.
  bool IsTopLevel() const { return nesting_stack_.size() == 1; }

  void DeleteBlock() {
    DCHECK(!delete_current_);
    DCHECK(IsActive());
    delete_current_ = true;
  }

 private:
  void MaybeWriteCurrent() {
    if (delete_current_) return;
    if (read_index_ >= 0 && write_index_ != read_index_) {
      function_->blocks[write_index_] = function_->blocks[read_index_];
    }
    write_index_++;
  }

  void Finalize() {
    while (Next()) {
      // Just iterate to the end.
    }
    function_->blocks.resize(write_index_);
  }

  bool IsActive() const { return read_index_ >= 0 && !ended_; }

  CoverageFunction* function_;
  std::vector<CoverageBlock> nesting_stack_;
  bool ended_;
  bool delete_current_;
  int read_index_;
  int write_index_;
};

bool HaveSameSourceRange(const CoverageBlock& lhs, const CoverageBlock& rhs) {
  return lhs.start == rhs.start && lhs.end == rhs.end;
}

void MergeDuplicateSingletons(CoverageFunction* function) {
  CoverageBlockIterator iter(function);

  while (iter.Next() && iter.HasNext()) {
    CoverageBlock& block = iter.GetBlock();
    CoverageBlock& next_block = iter.GetNextBlock();

    // Identical ranges should only occur through singleton ranges. Consider the
    // ranges for `for (.) break;`: continuation ranges for both the `break` and
    // `for` statements begin after the trailing semicolon.
    // Such ranges are merged and keep the maximal execution count.
    if (!HaveSameSourceRange(block, next_block)) continue;

    DCHECK_EQ(kNoSourcePosition, block.end);  // Singleton range.
    next_block.count = std::max(block.count, next_block.count);
    iter.DeleteBlock();
  }
}

void MergeDuplicateRanges(CoverageFunction* function) {
  CoverageBlockIterator iter(function);

  while (iter.Next() && iter.HasNext()) {
    CoverageBlock& block = iter.GetBlock();
    CoverageBlock& next_block = iter.GetNextBlock();

    if (!HaveSameSourceRange(block, next_block)) continue;

    DCHECK_NE(kNoSourcePosition, block.end);  // Non-singleton range.
    next_block.count = std::max(block.count, next_block.count);
    iter.DeleteBlock();
  }
}

// Rewrite position singletons (produced by unconditional control flow
// like return statements, and by continuation counters) into source
// ranges that end at the next sibling range or the end of the parent
// range, whichever comes first.
void RewritePositionSingletonsToRanges(CoverageFunction* function) {
  CoverageBlockIterator iter(function);

  while (iter.Next()) {
    CoverageBlock& block = iter.GetBlock();
    CoverageBlock& parent = iter.GetParent();

    if (block.start >= function->end) {
      DCHECK_EQ(block.end, kNoSourcePosition);
      iter.DeleteBlock();
    } else if (block.end == kNoSourcePosition) {
      // The current block ends at the next sibling block (if it exists) or the
      // end of the parent block otherwise.
      if (iter.HasSiblingOrChild()) {
        block.end = iter.GetSiblingOrChild().start;
      } else if (iter.IsTopLevel()) {
        // See https://crbug.com/v8/6661. Functions are special-cased because
        // we never want the closing brace to be uncovered. This is mainly to
        // avoid a noisy UI.
        block.end = parent.end - 1;
      } else {
        block.end = parent.end;
      }
    }
  }
}

void MergeConsecutiveRanges(CoverageFunction* function) {
  CoverageBlockIterator iter(function);

  while (iter.Next()) {
    CoverageBlock& block = iter.GetBlock();

    if (iter.HasSiblingOrChild()) {
      CoverageBlock& sibling = iter.GetSiblingOrChild();
      if (sibling.start == block.end && sibling.count == block.count) {
        // Best-effort: this pass may miss mergeable siblings in the presence of
        // child blocks.
        sibling.start = block.start;
        iter.DeleteBlock();
      }
    }
  }
}

void MergeNestedRanges(CoverageFunction* function) {
  CoverageBlockIterator iter(function);

  while (iter.Next()) {
    CoverageBlock& block = iter.GetBlock();
    CoverageBlock& parent = iter.GetParent();

    if (parent.count == block.count) {
      // Transformation may not be valid if sibling blocks exist with a
      // differing count.
      iter.DeleteBlock();
    }
  }
}

void FilterUncoveredRanges(CoverageFunction* function) {
  CoverageBlockIterator iter(function);

  while (iter.Next()) {
    CoverageBlock& block = iter.GetBlock();
    CoverageBlock& parent = iter.GetParent();
    if (block.count == 0 && parent.count == 0) iter.DeleteBlock();
  }
}

void FilterEmptyRanges(CoverageFunction* function) {
  CoverageBlockIterator iter(function);

  while (iter.Next()) {
    CoverageBlock& block = iter.GetBlock();
    if (block.start == block.end) iter.DeleteBlock();
  }
}

void ClampToBinary(CoverageFunction* function) {
  CoverageBlockIterator iter(function);

  while (iter.Next()) {
    CoverageBlock& block = iter.GetBlock();
    if (block.count > 0) block.count = 1;
  }
}

void ResetAllBlockCounts(SharedFunctionInfo* shared) {
  DCHECK(shared->HasCoverageInfo());

  CoverageInfo* coverage_info =
      CoverageInfo::cast(shared->GetDebugInfo()->coverage_info());

  for (int i = 0; i < coverage_info->SlotCount(); i++) {
    coverage_info->ResetBlockCount(i);
  }
}

bool IsBlockMode(debug::Coverage::Mode mode) {
  switch (mode) {
    case debug::Coverage::kBlockBinary:
    case debug::Coverage::kBlockCount:
      return true;
    default:
      return false;
  }
}

bool IsBinaryMode(debug::Coverage::Mode mode) {
  switch (mode) {
    case debug::Coverage::kBlockBinary:
    case debug::Coverage::kPreciseBinary:
      return true;
    default:
      return false;
  }
}

void CollectBlockCoverage(Isolate* isolate, CoverageFunction* function,
                          SharedFunctionInfo* info,
                          debug::Coverage::Mode mode) {
  DCHECK(IsBlockMode(mode));

  function->has_block_coverage = true;
  function->blocks = GetSortedBlockData(isolate, info);

  // If in binary mode, only report counts of 0/1.
  if (mode == debug::Coverage::kBlockBinary) ClampToBinary(function);

  // Remove duplicate singleton ranges, keeping the max count.
  MergeDuplicateSingletons(function);

  // Rewrite all singletons (created e.g. by continuations and unconditional
  // control flow) to ranges.
  RewritePositionSingletonsToRanges(function);

  // Merge nested and consecutive ranges with identical counts.
  // Note that it's necessary to merge duplicate ranges prior to merging nested
  // changes in order to avoid invalid transformations. See crbug.com/827530.
  MergeConsecutiveRanges(function);

  SortBlockData(function->blocks);
  MergeDuplicateRanges(function);
  MergeNestedRanges(function);

  MergeConsecutiveRanges(function);

  // Filter out ranges with count == 0 unless the immediate parent range has
  // a count != 0.
  FilterUncoveredRanges(function);

  // Filter out ranges of zero length.
  FilterEmptyRanges(function);

  // Reset all counters on the DebugInfo to zero.
  ResetAllBlockCounts(info);
}
}  // anonymous namespace

std::unique_ptr<Coverage> Coverage::CollectPrecise(Isolate* isolate) {
  DCHECK(!isolate->is_best_effort_code_coverage());
  std::unique_ptr<Coverage> result =
      Collect(isolate, isolate->code_coverage_mode());
  if (!isolate->is_collecting_type_profile() &&
      (isolate->is_precise_binary_code_coverage() ||
       isolate->is_block_binary_code_coverage())) {
    // We do not have to hold onto feedback vectors for invocations we already
    // reported. So we can reset the list.
    isolate->SetFeedbackVectorsForProfilingTools(*ArrayList::New(isolate, 0));
  }
  return result;
}

std::unique_ptr<Coverage> Coverage::CollectBestEffort(Isolate* isolate) {
  return Collect(isolate, v8::debug::Coverage::kBestEffort);
}

std::unique_ptr<Coverage> Coverage::Collect(
    Isolate* isolate, v8::debug::Coverage::Mode collectionMode) {
  SharedToCounterMap counter_map;

  const bool reset_count = collectionMode != v8::debug::Coverage::kBestEffort;

  switch (isolate->code_coverage_mode()) {
    case v8::debug::Coverage::kBlockBinary:
    case v8::debug::Coverage::kBlockCount:
    case v8::debug::Coverage::kPreciseBinary:
    case v8::debug::Coverage::kPreciseCount: {
      // Feedback vectors are already listed to prevent losing them to GC.
      DCHECK(isolate->factory()
                 ->feedback_vectors_for_profiling_tools()
                 ->IsArrayList());
      Handle<ArrayList> list = Handle<ArrayList>::cast(
          isolate->factory()->feedback_vectors_for_profiling_tools());
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
      DCHECK(!isolate->factory()
                  ->feedback_vectors_for_profiling_tools()
                  ->IsArrayList());
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
  std::unique_ptr<Coverage> result(new Coverage());
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
      int end = info->EndPosition();
      uint32_t count = counter_map.Get(info);
      // Find the correct outer function based on start position.
      while (!nesting.empty() && functions->at(nesting.back()).end <= start) {
        nesting.pop_back();
      }
      if (count != 0) {
        switch (collectionMode) {
          case v8::debug::Coverage::kBlockCount:
          case v8::debug::Coverage::kPreciseCount:
            break;
          case v8::debug::Coverage::kBlockBinary:
          case v8::debug::Coverage::kPreciseBinary:
            count = info->has_reported_binary_coverage() ? 0 : 1;
            info->set_has_reported_binary_coverage(true);
            break;
          case v8::debug::Coverage::kBestEffort:
            count = 1;
            break;
        }
      }

      Handle<String> name(info->DebugName(), isolate);
      CoverageFunction function(start, end, count, name);

      if (IsBlockMode(collectionMode) && info->HasCoverageInfo()) {
        CollectBlockCoverage(isolate, &function, info, collectionMode);
      }

      // Only include a function range if itself or its parent function is
      // covered, or if it contains non-trivial block coverage.
      bool is_covered = (count != 0);
      bool parent_is_covered =
          (!nesting.empty() && functions->at(nesting.back()).count != 0);
      bool has_block_coverage = !function.blocks.empty();
      if (is_covered || parent_is_covered || has_block_coverage) {
        nesting.push_back(functions->size());
        functions->emplace_back(function);
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
      // Note that DevTools switches back to best-effort coverage once the
      // recording is stopped. Since we delete coverage infos at that point, any
      // following coverage recording (without reloads) will be at function
      // granularity.
      isolate->debug()->RemoveAllCoverageInfos();
      if (!isolate->is_collecting_type_profile()) {
        isolate->SetFeedbackVectorsForProfilingTools(
            isolate->heap()->undefined_value());
      }
      break;
    case debug::Coverage::kBlockBinary:
    case debug::Coverage::kBlockCount:
    case debug::Coverage::kPreciseBinary:
    case debug::Coverage::kPreciseCount: {
      HandleScope scope(isolate);

      // Remove all optimized function. Optimized and inlined functions do not
      // increment invocation count.
      Deoptimizer::DeoptimizeAll(isolate);

      // Root all feedback vectors to avoid early collection.
      isolate->MaybeInitializeVectorListFromHeap();

      HeapIterator heap_iterator(isolate->heap());
      while (HeapObject* o = heap_iterator.next()) {
        if (IsBinaryMode(mode) && o->IsSharedFunctionInfo()) {
          // If collecting binary coverage, reset
          // SFI::has_reported_binary_coverage to avoid optimizing / inlining
          // functions before they have reported coverage.
          SharedFunctionInfo* shared = SharedFunctionInfo::cast(o);
          shared->set_has_reported_binary_coverage(false);
        } else if (o->IsFeedbackVector()) {
          // In any case, clear any collected invocation counts.
          FeedbackVector* vector = FeedbackVector::cast(o);
          vector->clear_invocation_count();
        }
      }

      break;
    }
  }
  isolate->set_code_coverage_mode(mode);
}

}  // namespace internal
}  // namespace v8
