// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/marking-worklist.h"

#include <algorithm>
#include <map>

#include "src/objects/heap-object-inl.h"
#include "src/objects/heap-object.h"
#include "src/objects/instance-type-inl.h"
#include "src/objects/instance-type.h"
#include "src/objects/map.h"
#include "src/objects/objects-definitions.h"

namespace v8 {
namespace internal {

MarkingWorklistsHolder::~MarkingWorklistsHolder() {
  DCHECK(worklists_.empty());
  DCHECK(context_worklists_.empty());
}

void MarkingWorklistsHolder::Clear() {
  shared_.Clear();
  on_hold_.Clear();
  embedder_.Clear();
  for (auto cw : context_worklists_) {
    cw.worklist->Clear();
  }
  ReleaseContextWorklists();
}

void MarkingWorklistsHolder::Print() {
  PrintWorklist("shared", &shared_);
  PrintWorklist("on_hold", &on_hold_);
}

void MarkingWorklistsHolder::CreateContextWorklists(
    const std::vector<Address>& contexts) {
  DCHECK(worklists_.empty());
  DCHECK(context_worklists_.empty());
  if (contexts.empty()) return;
  worklists_.reserve(contexts.size());
  context_worklists_.reserve(contexts.size() + 2);
  context_worklists_.push_back({kSharedContext, &shared_});
  context_worklists_.push_back({kOtherContext, &other_});
  for (Address context : contexts) {
    MarkingWorklist* worklist = new MarkingWorklist();
    worklists_.push_back(std::unique_ptr<MarkingWorklist>(worklist));
    context_worklists_.push_back({context, worklist});
  }
}

void MarkingWorklistsHolder::ReleaseContextWorklists() {
  context_worklists_.clear();
  worklists_.clear();
}

void MarkingWorklistsHolder::PrintWorklist(const char* worklist_name,
                                           MarkingWorklist* worklist) {
#ifdef DEBUG
  std::map<InstanceType, int> count;
  int total_count = 0;
  worklist->IterateGlobalPool([&count, &total_count](HeapObject obj) {
    ++total_count;
    count[obj.map().instance_type()]++;
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

MarkingWorklists::MarkingWorklists(int task_id, MarkingWorklistsHolder* holder)
    : shared_(holder->shared()),
      on_hold_(holder->on_hold()),
      embedder_(holder->embedder()),
      active_(shared_),
      active_context_(kSharedContext),
      task_id_(task_id),
      is_per_context_mode_(false),
      context_worklists_(holder->context_worklists()) {
  if (!context_worklists_.empty()) {
    is_per_context_mode_ = true;
    worklist_by_context_.reserve(context_worklists_.size());
    for (auto& cw : context_worklists_) {
      worklist_by_context_[cw.context] = cw.worklist;
    }
  }
}

void MarkingWorklists::FlushToGlobal() {
  shared_->FlushToGlobal(task_id_);
  on_hold_->FlushToGlobal(task_id_);
  embedder_->FlushToGlobal(task_id_);
  if (is_per_context_mode_) {
    for (auto& cw : context_worklists_) {
      cw.worklist->FlushToGlobal(task_id_);
    }
  }
}

bool MarkingWorklists::IsEmpty() {
  // This function checks the on_hold_ worklist, so it works only for the main
  // thread.
  DCHECK_EQ(kMainThreadTask, task_id_);
  if (!active_->IsLocalEmpty(task_id_) || !on_hold_->IsLocalEmpty(task_id_) ||
      !active_->IsGlobalPoolEmpty() || !on_hold_->IsGlobalPoolEmpty()) {
    return false;
  }
  if (!is_per_context_mode_) {
    DCHECK_EQ(active_, shared_);
    return true;
  }
  for (auto& cw : context_worklists_) {
    if (!cw.worklist->IsLocalEmpty(task_id_) ||
        !cw.worklist->IsGlobalPoolEmpty()) {
      active_ = cw.worklist;
      active_context_ = cw.context;
      return false;
    }
  }
  return true;
}

bool MarkingWorklists::IsEmbedderEmpty() {
  return embedder_->IsLocalEmpty(task_id_) && embedder_->IsGlobalPoolEmpty();
}

void MarkingWorklists::ShareWorkIfGlobalPoolIsEmpty() {
  if (!shared_->IsLocalEmpty(task_id_) && shared_->IsGlobalPoolEmpty()) {
    shared_->FlushToGlobal(task_id_);
  }
  if (is_per_context_mode_ && shared_ != active_) {
    if (!active_->IsLocalEmpty(task_id_) && active_->IsGlobalPoolEmpty()) {
      active_->FlushToGlobal(task_id_);
    }
  }
}

void MarkingWorklists::MergeOnHold() {
  DCHECK_EQ(kMainThreadTask, task_id_);
  shared_->MergeGlobalPool(on_hold_);
}

bool MarkingWorklists::PopContext(HeapObject* object) {
  DCHECK(is_per_context_mode_);
  // As an optimization we first check only the local segments to avoid locks.
  for (auto& cw : context_worklists_) {
    if (!cw.worklist->IsLocalEmpty(task_id_)) {
      active_ = cw.worklist;
      active_context_ = cw.context;
      return active_->Pop(task_id_, object);
    }
  }
  // All local segments are empty. Check global segments.
  for (auto& cw : context_worklists_) {
    if (cw.worklist->Pop(task_id_, object)) {
      active_ = cw.worklist;
      active_context_ = cw.context;
      return true;
    }
  }
  // All worklists are empty. Switch to the default shared worklist.
  SwitchToShared();
  return false;
}

Address MarkingWorklists::SwitchToContextSlow(Address context) {
  const auto& it = worklist_by_context_.find(context);
  if (V8_UNLIKELY(it == worklist_by_context_.end())) {
    // This context was created during marking or is not being measured,
    // so we don't have a specific worklist for it.
    active_context_ = kOtherContext;
    active_ = worklist_by_context_[active_context_];
  } else {
    active_ = it->second;
    active_context_ = context;
  }
  return active_context_;
}

}  // namespace internal
}  // namespace v8
