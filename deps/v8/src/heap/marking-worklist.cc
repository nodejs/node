// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/marking-worklist.h"

#include <algorithm>
#include <map>

#include "src/heap/marking-worklist-inl.h"
#include "src/objects/heap-object-inl.h"
#include "src/objects/heap-object.h"
#include "src/objects/instance-type-inl.h"
#include "src/objects/instance-type.h"
#include "src/objects/map.h"
#include "src/objects/objects-definitions.h"

namespace v8 {
namespace internal {

MarkingWorklists::~MarkingWorklists() {
  DCHECK(shared_.IsEmpty());
  DCHECK(on_hold_.IsEmpty());
  DCHECK(other_.IsEmpty());
  DCHECK(worklists_.empty());
  DCHECK(context_worklists_.empty());
}

void MarkingWorklists::Clear() {
  shared_.Clear();
  on_hold_.Clear();
  embedder_.Clear();
  other_.Clear();
  for (auto cw : context_worklists_) {
    if (cw.context == kSharedContext || cw.context == kOtherContext) {
      // These contexts were cleared above.
      continue;
    }
    cw.worklist->Clear();
  }
  ReleaseContextWorklists();
}

void MarkingWorklists::Print() {
  PrintWorklist("shared", &shared_);
  PrintWorklist("on_hold", &on_hold_);
}

void MarkingWorklists::CreateContextWorklists(
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

void MarkingWorklists::ReleaseContextWorklists() {
  context_worklists_.clear();
  worklists_.clear();
}

void MarkingWorklists::PrintWorklist(const char* worklist_name,
                                     MarkingWorklist* worklist) {
#ifdef DEBUG
  std::map<InstanceType, int> count;
  int total_count = 0;
  worklist->Iterate([&count, &total_count](HeapObject obj) {
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

const Address MarkingWorklists::Local::kSharedContext;
const Address MarkingWorklists::Local::kOtherContext;

MarkingWorklists::Local::Local(MarkingWorklists* global)
    : on_hold_(global->on_hold()),
      embedder_(global->embedder()),
      is_per_context_mode_(false) {
  if (global->context_worklists().empty()) {
    MarkingWorklist::Local shared(global->shared());
    active_ = std::move(shared);
    active_context_ = kSharedContext;
    active_owner_ = nullptr;
  } else {
    is_per_context_mode_ = true;
    worklist_by_context_.reserve(global->context_worklists().size());
    for (auto& cw : global->context_worklists()) {
      worklist_by_context_[cw.context] =
          std::make_unique<MarkingWorklist::Local>(cw.worklist);
    }
    active_owner_ = worklist_by_context_[kSharedContext].get();
    active_ = std::move(*active_owner_);
    active_context_ = kSharedContext;
  }
}

MarkingWorklists::Local::~Local() {
  DCHECK(active_.IsLocalEmpty());
  if (is_per_context_mode_) {
    for (auto& cw : worklist_by_context_) {
      if (cw.first != active_context_) {
        DCHECK(cw.second->IsLocalEmpty());
      }
    }
  }
}

void MarkingWorklists::Local::Publish() {
  active_.Publish();
  on_hold_.Publish();
  embedder_.Publish();
  if (is_per_context_mode_) {
    for (auto& cw : worklist_by_context_) {
      if (cw.first != active_context_) {
        cw.second->Publish();
      }
    }
  }
}

bool MarkingWorklists::Local::IsEmpty() {
  // This function checks the on_hold_ worklist, so it works only for the main
  // thread.
  if (!active_.IsLocalEmpty() || !on_hold_.IsLocalEmpty() ||
      !active_.IsGlobalEmpty() || !on_hold_.IsGlobalEmpty()) {
    return false;
  }
  if (!is_per_context_mode_) {
    return true;
  }
  for (auto& cw : worklist_by_context_) {
    if (cw.first != active_context_ &&
        !(cw.second->IsLocalEmpty() && cw.second->IsGlobalEmpty())) {
      SwitchToContext(cw.first, cw.second.get());
      return false;
    }
  }
  return true;
}

bool MarkingWorklists::Local::IsEmbedderEmpty() const {
  return embedder_.IsLocalEmpty() && embedder_.IsGlobalEmpty();
}

void MarkingWorklists::Local::ShareWork() {
  if (!active_.IsLocalEmpty() && active_.IsGlobalEmpty()) {
    active_.Publish();
  }
  if (is_per_context_mode_ && active_context_ != kSharedContext) {
    MarkingWorklist::Local* shared = worklist_by_context_[kSharedContext].get();
    if (!shared->IsLocalEmpty() && shared->IsGlobalEmpty()) {
      shared->Publish();
    }
  }
}

void MarkingWorklists::Local::MergeOnHold() {
  MarkingWorklist::Local* shared =
      active_context_ == kSharedContext
          ? &active_
          : worklist_by_context_[kSharedContext].get();
  shared->Merge(&on_hold_);
}

bool MarkingWorklists::Local::PopContext(HeapObject* object) {
  DCHECK(is_per_context_mode_);
  // As an optimization we first check only the local segments to avoid locks.
  for (auto& cw : worklist_by_context_) {
    if (cw.first != active_context_ && !cw.second->IsLocalEmpty()) {
      SwitchToContext(cw.first, cw.second.get());
      return active_.Pop(object);
    }
  }
  // All local segments are empty. Check global segments.
  for (auto& cw : worklist_by_context_) {
    if (cw.first != active_context_ && cw.second->Pop(object)) {
      SwitchToContext(cw.first, cw.second.get());
      return true;
    }
  }
  // All worklists are empty. Switch to the default shared worklist.
  SwitchToContext(kSharedContext);
  return false;
}

Address MarkingWorklists::Local::SwitchToContextSlow(Address context) {
  const auto& it = worklist_by_context_.find(context);
  if (V8_UNLIKELY(it == worklist_by_context_.end())) {
    // This context was created during marking or is not being measured,
    // so we don't have a specific worklist for it.
    SwitchToContext(kOtherContext, worklist_by_context_[kOtherContext].get());
  } else {
    SwitchToContext(it->first, it->second.get());
  }
  return active_context_;
}

}  // namespace internal
}  // namespace v8
