// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NODE-LOCAL PATCH: heap profile sample labels feature, do not remove on V8
// update. This whole file is part of the Node.js floating patch set; see the
// comment at the top of include/v8-profiler.h.
#include "src/profiler/label-intern-table.h"

#ifdef V8_HEAP_PROFILER_SAMPLE_LABELS

#include <limits>

#include "include/v8-isolate.h"
#include "src/api/api-inl.h"
#include "src/execution/isolate.h"
#include "src/objects/js-objects-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/smi.h"

namespace v8 {
namespace internal {

LabelInternTable::LabelInternTable(v8::Isolate* isolate) : isolate_(isolate) {}

LabelInternTable::~LabelInternTable() {
  // Draining before the walk avoids double-Reset on entries about to be freed.
  base::MutexGuard guard(&mutex_);
  DrainPendingFreeLocked();
  for (auto& bucket : buckets_) {
    for (auto& entry : bucket.second) {
      entry.global.Reset();
    }
  }
  buckets_.clear();
  id_to_hash_.clear();
}

void LabelInternTable::DrainPendingFreeLocked() {
  // A duplicate id (queued, revived, released again before any drain) is
  // harmless: the first occurrence erases id_to_hash_[id] and the rest miss.
  for (uint32_t id : pending_free_) {
    auto map_it = id_to_hash_.find(id);
    if (map_it == id_to_hash_.end()) continue;
    uint32_t hash = map_it->second;
    auto bucket_it = buckets_.find(hash);
    if (bucket_it == buckets_.end()) continue;
    auto& chain = bucket_it->second;
    for (auto entry_it = chain.begin(); entry_it != chain.end(); ++entry_it) {
      if (entry_it->id != id) continue;
      if (entry_it->refcount > 0) break;  // revived; leave alone
      entry_it->global.Reset();
      chain.erase(entry_it);
      id_to_hash_.erase(map_it);
      if (chain.empty()) buckets_.erase(bucket_it);
      break;
    }
  }
  pending_free_.clear();
}

uint32_t LabelInternTable::Intern(v8::Local<v8::Value> value) {
  DCHECK(!value.IsEmpty());
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate_);

  DisallowGarbageCollection no_gc;
  Tagged<Object> value_obj = *Utils::OpenDirectHandle(*value);
  // Identity hash is only defined for JSReceiver.
  if (!IsJSReceiver(value_obj)) return kNoLabelId;

  Tagged<JSReceiver> receiver = Cast<JSReceiver>(value_obj);
  uint32_t hash = static_cast<uint32_t>(
      receiver->GetOrCreateIdentityHash(i_isolate).value());
  hash &= hash_mask_;

  Address candidate_ptr = value_obj.ptr();
  base::MutexGuard guard(&mutex_);
  auto& chain = buckets_[hash];
  for (Entry& entry : chain) {
    Tagged<Object> existing =
        *Utils::OpenDirectHandle(*entry.global.Get(isolate_));
    if (existing.ptr() == candidate_ptr) {
      // Bumping an entry at refcount 0 revives it: the drain below skips
      // queued ids that are live again, so the Global is never Reset across
      // the Release/Intern race and identity is preserved.
      // Saturate rather than wrap: an overflowed refcount would later
      // underflow on Release and free a still-live label.
      if (entry.refcount != std::numeric_limits<uint32_t>::max()) {
        ++entry.refcount;
      }
      // Copy the id out first. The drain can erase earlier entries in this
      // chain, which invalidates `entry`.
      uint32_t revived_id = entry.id;
      DrainPendingFreeLocked();
      return revived_id;
    }
  }

  // Mint a fresh id. In steady state next_id_ has never been issued, so the
  // first candidate is free. After ~2^32 interns the counter wraps; skip
  // kNoLabelId and any id still mapped to a live entry. If no free id turns
  // up within the probe bound, fail closed (drop the label) rather than
  // aliasing a live id, which would corrupt refcounts and attribution.
  //
  // The probe treats ids queued in pending_free_ as occupied (they are still
  // in id_to_hash_ until drained), so on wraparound it may fail closed
  // before a pending drain would free an id. Draining first is not done
  // here: DrainPendingFreeLocked() can erase from `chain`, invalidating the
  // `chain` reference taken above. Wraparound requires roughly 2^32 interns
  // in one isolate.
  uint32_t id = kNoLabelId;
  for (uint32_t probe = 0; probe <= kIdProbeLimit; ++probe) {
    uint32_t candidate = ++next_id_;
    if (candidate == kNoLabelId) continue;
    if (id_to_hash_.count(candidate) == 0) {
      id = candidate;
      break;
    }
  }
  if (id == kNoLabelId) {
    // Do not leave an empty bucket behind from buckets_[hash] above.
    if (chain.empty()) buckets_.erase(hash);
    DrainPendingFreeLocked();
    return kNoLabelId;
  }
  chain.push_back(Entry{v8::Global<v8::Value>(isolate_, value), 1, id});
  id_to_hash_[id] = hash;
  // `chain` must not be used below: the drain can erase from it.
  DrainPendingFreeLocked();
  return id;
}

void LabelInternTable::Release(uint32_t id) {
  if (id == kNoLabelId) return;
  base::MutexGuard guard(&mutex_);
  auto map_it = id_to_hash_.find(id);
  if (map_it == id_to_hash_.end()) return;

  auto bucket_it = buckets_.find(map_it->second);
  DCHECK(bucket_it != buckets_.end());
  for (Entry& entry : bucket_it->second) {
    if (entry.id != id) continue;
    DCHECK_GT(entry.refcount, 0u);
    --entry.refcount;
    // Queue only: Global::Reset() is main-thread only, and this runs on the
    // sweeper thread. The entry stays in its bucket until drained, so a
    // racing Intern can still revive it.
    if (entry.refcount == 0) pending_free_.push_back(id);
    return;
  }
  DCHECK(false);  // id_to_hash_ pointed at a bucket with no such entry
}

void LabelInternTable::Clear() {
  // Emptying id_to_hash_ is what makes a later Release() of a stale id a
  // no-op rather than a decrement of an unrelated entry.
  base::MutexGuard guard(&mutex_);
  DrainPendingFreeLocked();
  for (auto& bucket : buckets_) {
    for (auto& entry : bucket.second) {
      entry.global.Reset();
    }
  }
  buckets_.clear();
  id_to_hash_.clear();
}

v8::MaybeLocal<v8::Value> LabelInternTable::Lookup(uint32_t id) {
  if (id == kNoLabelId) return v8::MaybeLocal<v8::Value>();
  base::MutexGuard guard(&mutex_);
  // Drain first, unlike Intern: there is no revival path here, and every
  // reference into buckets_ below is taken after the drain.
  DrainPendingFreeLocked();
  auto map_it = id_to_hash_.find(id);
  if (map_it == id_to_hash_.end()) return v8::MaybeLocal<v8::Value>();

  auto bucket_it = buckets_.find(map_it->second);
  DCHECK(bucket_it != buckets_.end());
  for (Entry& entry : bucket_it->second) {
    if (entry.id != id) continue;
    if (entry.refcount == 0) return v8::MaybeLocal<v8::Value>();
    return entry.global.Get(isolate_);
  }
  return v8::MaybeLocal<v8::Value>();
}

size_t LabelInternTable::SizeForTesting() const {
  base::MutexGuard guard(&mutex_);
  size_t live = 0;
  for (const auto& bucket : buckets_) {
    for (const auto& entry : bucket.second) {
      if (entry.refcount > 0) ++live;
    }
  }
  return live;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_PROFILER_SAMPLE_LABELS
