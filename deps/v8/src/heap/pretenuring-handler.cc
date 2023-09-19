// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/pretenuring-handler.h"

#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/flags/flags.h"
#include "src/handles/global-handles-inl.h"
#include "src/heap/new-spaces.h"
#include "src/objects/allocation-site-inl.h"

namespace v8 {
namespace internal {

PretenuringHandler::PretenuringHandler(Heap* heap)
    : heap_(heap), global_pretenuring_feedback_(kInitialFeedbackCapacity) {}

PretenuringHandler::~PretenuringHandler() = default;

void PretenuringHandler::MergeAllocationSitePretenuringFeedback(
    const PretenuringFeedbackMap& local_pretenuring_feedback) {
  PtrComprCageBase cage_base(heap_->isolate());
  AllocationSite site;
  for (auto& site_and_count : local_pretenuring_feedback) {
    site = site_and_count.first;
    MapWord map_word = site.map_word(cage_base, kRelaxedLoad);
    if (map_word.IsForwardingAddress()) {
      site = AllocationSite::cast(map_word.ToForwardingAddress(site));
    }

    // We have not validated the allocation site yet, since we have not
    // dereferenced the site during collecting information.
    // This is an inlined check of AllocationMemento::IsValid.
    if (!site.IsAllocationSite() || site.IsZombie()) continue;

    const int value = static_cast<int>(site_and_count.second);
    DCHECK_LT(0, value);
    if (site.IncrementMementoFoundCount(value)) {
      // For sites in the global map the count is accessed through the site.
      global_pretenuring_feedback_.insert(std::make_pair(site, 0));
    }
  }
}

bool PretenuringHandler::DeoptMaybeTenuredAllocationSites() const {
  NewSpace* new_space = heap_->new_space();
  if (heap_->tracer()->GetCurrentCollector() ==
      GarbageCollector::MINOR_MARK_COMPACTOR) {
    DCHECK(v8_flags.minor_mc);
    DCHECK_NOT_NULL(new_space);
    return heap_->IsFirstMaximumSizeMinorGC();
  }
  return new_space && new_space->IsAtMaximumCapacity() &&
         !heap_->MaximumSizeMinorGC();
}

namespace {

inline bool MakePretenureDecision(
    AllocationSite site, AllocationSite::PretenureDecision current_decision,
    double ratio, bool maximum_size_scavenge) {
  // Here we just allow state transitions from undecided or maybe tenure
  // to don't tenure, maybe tenure, or tenure.
  if ((current_decision == AllocationSite::kUndecided ||
       current_decision == AllocationSite::kMaybeTenure)) {
    if (ratio >= AllocationSite::kPretenureRatio) {
      // We just transition into tenure state when the semi-space was at
      // maximum capacity.
      if (maximum_size_scavenge) {
        site.set_deopt_dependent_code(true);
        site.set_pretenure_decision(AllocationSite::kTenure);
        // Currently we just need to deopt when we make a state transition to
        // tenure.
        return true;
      }
      site.set_pretenure_decision(AllocationSite::kMaybeTenure);
    } else {
      site.set_pretenure_decision(AllocationSite::kDontTenure);
    }
  }
  return false;
}

// Clear feedback calculation fields until the next gc.
inline void ResetPretenuringFeedback(AllocationSite site) {
  site.set_memento_found_count(0);
  site.set_memento_create_count(0);
}

inline bool DigestPretenuringFeedback(Isolate* isolate, AllocationSite site,
                                      bool maximum_size_scavenge) {
  bool deopt = false;
  int create_count = site.memento_create_count();
  int found_count = site.memento_found_count();
  bool minimum_mementos_created =
      create_count >= AllocationSite::kPretenureMinimumCreated;
  double ratio =
      minimum_mementos_created || v8_flags.trace_pretenuring_statistics
          ? static_cast<double>(found_count) / create_count
          : 0.0;
  AllocationSite::PretenureDecision current_decision =
      site.pretenure_decision();

  if (minimum_mementos_created) {
    deopt = MakePretenureDecision(site, current_decision, ratio,
                                  maximum_size_scavenge);
  }

  if (v8_flags.trace_pretenuring_statistics) {
    PrintIsolate(isolate,
                 "pretenuring: AllocationSite(%p): (created, found, ratio) "
                 "(%d, %d, %f) %s => %s\n",
                 reinterpret_cast<void*>(site.ptr()), create_count, found_count,
                 ratio, site.PretenureDecisionName(current_decision),
                 site.PretenureDecisionName(site.pretenure_decision()));
  }

  ResetPretenuringFeedback(site);
  return deopt;
}

bool PretenureAllocationSiteManually(Isolate* isolate, AllocationSite site) {
  AllocationSite::PretenureDecision current_decision =
      site.pretenure_decision();
  bool deopt = true;
  if (current_decision == AllocationSite::kUndecided ||
      current_decision == AllocationSite::kMaybeTenure) {
    site.set_deopt_dependent_code(true);
    site.set_pretenure_decision(AllocationSite::kTenure);
  } else {
    deopt = false;
  }
  if (v8_flags.trace_pretenuring_statistics) {
    PrintIsolate(isolate,
                 "pretenuring manually requested: AllocationSite(%p): "
                 "%s => %s\n",
                 reinterpret_cast<void*>(site.ptr()),
                 site.PretenureDecisionName(current_decision),
                 site.PretenureDecisionName(site.pretenure_decision()));
  }

  ResetPretenuringFeedback(site);
  return deopt;
}

}  // namespace

void PretenuringHandler::RemoveAllocationSitePretenuringFeedback(
    AllocationSite site) {
  global_pretenuring_feedback_.erase(site);
}

void PretenuringHandler::ProcessPretenuringFeedback() {
  bool trigger_deoptimization = false;
  if (v8_flags.allocation_site_pretenuring) {
    int tenure_decisions = 0;
    int dont_tenure_decisions = 0;
    int allocation_mementos_found = 0;
    int allocation_sites = 0;
    int active_allocation_sites = 0;

    AllocationSite site;

    // Step 1: Digest feedback for recorded allocation sites.
    bool maximum_size_scavenge = heap_->MaximumSizeMinorGC();
    for (auto& site_and_count : global_pretenuring_feedback_) {
      allocation_sites++;
      site = site_and_count.first;
      // Count is always access through the site.
      DCHECK_EQ(0, site_and_count.second);
      int found_count = site.memento_found_count();
      // An entry in the storage does not imply that the count is > 0 because
      // allocation sites might have been reset due to too many objects dying
      // in old space.
      if (found_count > 0) {
        DCHECK(site.IsAllocationSite());
        active_allocation_sites++;
        allocation_mementos_found += found_count;
        if (DigestPretenuringFeedback(heap_->isolate(), site,
                                      maximum_size_scavenge)) {
          trigger_deoptimization = true;
        }
        if (site.GetAllocationType() == AllocationType::kOld) {
          tenure_decisions++;
        } else {
          dont_tenure_decisions++;
        }
      }
    }

    // Step 2: Pretenure allocation sites for manual requests.
    if (allocation_sites_to_pretenure_) {
      while (!allocation_sites_to_pretenure_->empty()) {
        auto pretenure_site = allocation_sites_to_pretenure_->Pop();
        if (PretenureAllocationSiteManually(heap_->isolate(), pretenure_site)) {
          trigger_deoptimization = true;
        }
      }
      allocation_sites_to_pretenure_.reset();
    }

    // Step 3: Deopt maybe tenured allocation sites if necessary.
    bool deopt_maybe_tenured = DeoptMaybeTenuredAllocationSites();
    if (deopt_maybe_tenured) {
      heap_->ForeachAllocationSite(
          heap_->allocation_sites_list(),
          [&allocation_sites, &trigger_deoptimization](AllocationSite site) {
            DCHECK(site.IsAllocationSite());
            allocation_sites++;
            if (site.IsMaybeTenure()) {
              site.set_deopt_dependent_code(true);
              trigger_deoptimization = true;
            }
          });
    }

    if (trigger_deoptimization) {
      heap_->isolate()->stack_guard()->RequestDeoptMarkedAllocationSites();
    }

    if (v8_flags.trace_pretenuring_statistics &&
        (allocation_mementos_found > 0 || tenure_decisions > 0 ||
         dont_tenure_decisions > 0)) {
      PrintIsolate(heap_->isolate(),
                   "pretenuring: deopt_maybe_tenured=%d visited_sites=%d "
                   "active_sites=%d "
                   "mementos=%d tenured=%d not_tenured=%d\n",
                   deopt_maybe_tenured ? 1 : 0, allocation_sites,
                   active_allocation_sites, allocation_mementos_found,
                   tenure_decisions, dont_tenure_decisions);
    }

    global_pretenuring_feedback_.clear();
    global_pretenuring_feedback_.reserve(kInitialFeedbackCapacity);
  }
}

void PretenuringHandler::PretenureAllocationSiteOnNextCollection(
    AllocationSite site) {
  if (!allocation_sites_to_pretenure_) {
    allocation_sites_to_pretenure_.reset(
        new GlobalHandleVector<AllocationSite>(heap_));
  }
  allocation_sites_to_pretenure_->Push(site);
}

void PretenuringHandler::reset() { allocation_sites_to_pretenure_.reset(); }

}  // namespace internal
}  // namespace v8
