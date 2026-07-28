// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/marking-worklist.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <string>

#include "src/heap/cppgc-js/cpp-heap.h"
#include "src/heap/cppgc-js/cpp-marking-state.h"
#include "src/heap/marking-worklist-inl.h"
#include "src/objects/heap-object-inl.h"
#include "src/objects/heap-object.h"
#include "src/objects/instance-type-inl.h"
#include "src/objects/instance-type.h"
#include "src/objects/map.h"
#include "src/objects/objects-definitions.h"

namespace v8 {
namespace internal {

void MarkingWorklists::Clear() {
  default_.Clear();
  on_hold_.Clear();
  other_.Clear();
  for (auto& cw : context_worklists_) {
    cw.worklist->Clear();
  }
  ReleaseContextWorklists();
}

void MarkingWorklists::Print() {
  PrintWorklist("default", &default_);
  PrintWorklist("on_hold", &on_hold_);
  if (IsPerContextMode()) {
    PrintWorklist("other", &other_);
    std::for_each(
        context_worklists_.begin(), context_worklists_.end(),
        [](auto& context_worklist) {
          PrintWorklist(
              ("context" + std::to_string(context_worklist.context)).c_str(),
              context_worklist.worklist.get());
        });
  }
}

bool MarkingWorklists::IsEmpty() const {
  if (!default_.IsEmpty()) {
    return false;
  }
  if (V8_LIKELY(!IsPerContextMode())) {
    return true;
  }
  if (!other_.IsEmpty()) {
    return false;
  }
  return std::all_of(context_worklists_.begin(), context_worklists_.end(),
                     [](auto& context_worklist) {
                       return context_worklist.worklist->IsEmpty();
                     });
}

void MarkingWorklists::CreateContextWorklists(
    const std::vector<Address>& contexts) {
  DCHECK(context_worklists_.empty());
  if (contexts.empty()) return;

  const size_t num_contexts = contexts.size();
  context_worklists_.reserve(num_contexts);
  worklist_by_context_.reserve(num_contexts);
  uint32_t index = 0;
  for (Address context : contexts) {
    context_worklists_.push_back(
        {context, std::make_unique<MarkingWorklist>()});
    worklist_by_context_.emplace(context, index++);
  }
}

void MarkingWorklists::ReleaseContextWorklists() {
  context_worklists_.clear();
  worklist_by_context_.clear();
}

// static
void MarkingWorklists::PrintWorklist(const char* worklist_name,
                                     MarkingWorklist* worklist) {
#ifdef DEBUG
  std::map<InstanceType, int> count;
  int total_count = 0;
  worklist->Iterate([&count, &total_count](Tagged<HeapObject> obj) {
    ++total_count;
    count[obj->map()->instance_type()]++;
  });
  std::vector<std::pair<int, InstanceType>> rank;
  rank.reserve(count.size());
  for (const auto& i : count) {
    rank.emplace_back(i.second, i.first);
  }
  std::map<InstanceType, std::string> instance_type_name;
#define INSTANCE_TYPE_NAME(name) instance_type_name[name] = #name;
  INSTANCE_TYPE_LIST(INSTANCE_TYPE_NAME)
#undef INSTANCE_TYPE_NAME
  std::sort(rank.begin(), rank.end(),
            std::greater<std::pair<int, InstanceType>>());
  PrintF("Worklist %s: %d\n", worklist_name, total_count);
  for (auto i : rank) {
    PrintF("  [%s]: %d\n", instance_type_name[i.second].c_str(), i.first);
  }
#endif
}

constexpr Address MarkingWorklists::Local::kSharedContext;
constexpr Address MarkingWorklists::Local::kOtherContext;
constexpr std::nullptr_t MarkingWorklists::Local::kNoCppMarkingState;

MarkingWorklists::Local::Local(
    MarkingWorklists* global,
    std::unique_ptr<CppMarkingState> cpp_marking_state)
    : active_(&default_),
      default_(*global->default_worklist()),
      on_hold_(*global->on_hold()),
      other_(*global->other()),
      active_context_(kSharedContext),
      is_per_context_mode_(global->IsPerContextMode()),
      worklist_by_context_(global->worklist_by_context_),
      cpp_marking_state_(std::move(cpp_marking_state)) {
  if (!is_per_context_mode_) {
    return;
  }
  const auto& context_worklists = global->context_worklists();
  context_worklists_.reserve(context_worklists.size());
  std::for_each(context_worklists.begin(), context_worklists.end(),
                [this](const ContextWorklistPair& cw) {
                  DCHECK_EQ(context_worklists_.size(),
                            worklist_by_context_.at(cw.context));
                  context_worklists_.emplace_back(*cw.worklist);
                });
}

void MarkingWorklists::Local::Publish() {
  default_.Publish();
  on_hold_.Publish();
  other_.Publish();
  if (is_per_context_mode_) {
    for (const auto& entry : worklist_by_context_) {
      context_worklists_[entry.second].Publish();
    }
  }
  PublishCppHeapObjects();
}

bool MarkingWorklists::Local::IsEmpty() {
  // This function checks the on_hold_ worklist, so it works only for the main
  // thread.
  if (!active_->IsLocalEmpty() || !on_hold_.IsLocalEmpty() ||
      !active_->IsGlobalEmpty() || !on_hold_.IsGlobalEmpty()) {
    return false;
  }
  if (!is_per_context_mode_) {
    return true;
  }
  if (!default_.IsLocalEmpty() || !other_.IsLocalEmpty() ||
      !default_.IsGlobalEmpty() || !other_.IsGlobalEmpty()) {
    return false;
  }
  for (const auto& entry : worklist_by_context_) {
    auto& worklist = context_worklists_[entry.second];
    if (entry.first != active_context_ &&
        !(worklist.IsLocalEmpty() && worklist.IsGlobalEmpty())) {
      SwitchToContextImpl(entry.first, &worklist);
      return false;
    }
  }
  return true;
}

bool MarkingWorklists::Local::IsWrapperEmpty() const {
  return !cpp_marking_state_ || cpp_marking_state_->IsLocalEmpty();
}

void MarkingWorklists::Local::ShareWork() {
  if (!default_.IsLocalEmpty() && default_.IsGlobalEmpty()) {
    default_.Publish();
  }
  if (V8_LIKELY(!is_per_context_mode_)) {
    DCHECK_EQ(active_, &default_);
    DCHECK(other_.IsLocalEmpty());
    return;
  }
  if (!other_.IsLocalEmpty() && other_.IsGlobalEmpty()) {
    other_.Publish();
  }
  std::for_each(context_worklists_.begin(), context_worklists_.end(),
                [](auto& worklist) {
                  if (!worklist.IsLocalEmpty() && worklist.IsGlobalEmpty()) {
                    worklist.Publish();
                  }
                });
}

void MarkingWorklists::Local::PublishWork() {
  DCHECK(!is_per_context_mode_);
  DCHECK_EQ(active_, &default_);
  default_.Publish();
}

void MarkingWorklists::Local::MergeOnHold() { default_.Merge(on_hold_); }

bool MarkingWorklists::Local::PopContext(Tagged<HeapObject>* object) {
  DCHECK(is_per_context_mode_);
  // As an optimization we first check only the local segments to avoid locks.
  auto check_worklist_local = [this, object](Address context,
                                             MarkingWorklist::Local& worklist) {
    if (context != active_context_ && !worklist.IsLocalEmpty()) {
      SwitchToContextImpl(context, &worklist);
      const bool res = active_->Pop(object);
      DCHECK(res);
      return res;
    }
    return false;
  };
  if (check_worklist_local(kSharedContext, default_) ||
      check_worklist_local(kOtherContext, other_) ||
      std::any_of(worklist_by_context_.begin(), worklist_by_context_.end(),
                  [this, check_worklist_local](auto& entry) {
                    return check_worklist_local(
                        entry.first, context_worklists_[entry.second]);
                  })) {
    return true;
  }
  // All local segments are empty. Check global segments.
  auto check_worklist_global =
      [this, object](Address context, MarkingWorklist::Local& worklist) {
        if (context != active_context_ && worklist.Pop(object)) {
          SwitchToContextImpl(context, &worklist);
          return true;
        }
        return false;
      };
  if (check_worklist_global(kSharedContext, default_) ||
      check_worklist_global(kOtherContext, other_) ||
      std::any_of(worklist_by_context_.begin(), worklist_by_context_.end(),
                  [this, check_worklist_global](auto& entry) {
                    return check_worklist_global(
                        entry.first, context_worklists_[entry.second]);
                  })) {
    return true;
  }
  // All worklists are empty. Switch to the default shared worklist.
  SwitchToContext(kSharedContext);
  return false;
}

Address MarkingWorklists::Local::SwitchToContextSlow(Address context) {
  ContextToIndexMap::const_iterator it;
  if (V8_UNLIKELY((context == kSharedContext) ||
                  ((it = worklist_by_context_.find(context)) ==
                   worklist_by_context_.end()))) {
    // The context passed is not an actual context:
    // - Shared context that should use the explicit worklist.
    // - This context was created during marking and should use the other
    // bucket.
    if (context == kSharedContext) {
      SwitchToContextImpl(kSharedContext, &default_);
    } else {
      SwitchToContextImpl(kOtherContext, &other_);
    }
  } else {
    DCHECK_NE(it, worklist_by_context_.end());
    SwitchToContextImpl(context, &(context_worklists_[it->second]));
  }
  return active_context_;
}

Address MarkingWorklists::Local::SwitchToSharedForTesting() {
  return SwitchToContext(kSharedContext);
}

}  // namespace internal
}  // namespace v8
