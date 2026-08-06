// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NODE-LOCAL PATCH: heap profile sample labels feature, do not remove on V8
// update. This whole file is part of the Node.js floating patch set; see the
// comment at the top of include/v8-profiler.h.
#ifndef V8_PROFILER_LABEL_INTERN_TABLE_H_
#define V8_PROFILER_LABEL_INTERN_TABLE_H_

#ifdef V8_HEAP_PROFILER_SAMPLE_LABELS

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "include/v8-local-handle.h"
#include "include/v8-persistent-handle.h"
#include "src/base/macros.h"
#include "src/base/platform/mutex.h"

namespace v8 {

class Isolate;
class Value;

namespace internal {

// Refcounted table mapping JS values to uint32_t ids, so a sample can hold a
// 4-byte id instead of a Global<Value> and its GlobalHandles::Node. Keyed on
// JSReceiver::GetOrCreateIdentityHash, which is stable across GC moves;
// collisions walk a per-bucket vector comparing object addresses.
//
// Ids are drawn from a per-table counter and are never reused, so an id minted
// by a stopped session resolves to empty rather than to an unrelated value.
//
// Release() runs on any thread, because V8's ArrayBufferSweeper frees backing
// stores off-thread, but Global::Reset() is main-thread only. Release()
// therefore only does refcount math and queues the id on pending_free_;
// Intern() and Lookup() drain the queue, and so does the destructor if neither
// is ever called again. Intern() drains after inserting, so that an entry
// revived by a racing Intern outlives the drain its own 1->0 transition
// queued; DrainPendingFreeLocked() skipping revived entries is what makes that
// safe.
class V8_EXPORT_PRIVATE LabelInternTable {
 public:
  // Reserved id meaning "no label".
  static constexpr uint32_t kNoLabelId = 0;

  explicit LabelInternTable(v8::Isolate* isolate);
  ~LabelInternTable();
  LabelInternTable(const LabelInternTable&) = delete;
  LabelInternTable& operator=(const LabelInternTable&) = delete;

  // Returns an id for value, bumping the refcount if it is already interned.
  // Non-receiver values return kNoLabelId. Main thread only.
  uint32_t Intern(v8::Local<v8::Value> value);

  // Decrements the refcount for id, queueing it for drain on the 1->0
  // transition. No-op for kNoLabelId or an id the table does not hold.
  // Safe to call from any thread.
  void Release(uint32_t id);

  // Empties the table so a stopped session stops pinning JS values. The table
  // outlives the session: a later session interns into it again. Main thread
  // only.
  void Clear();

  // Returns the interned value for id, or empty if it has been released.
  // Main thread only.
  v8::MaybeLocal<v8::Value> Lookup(uint32_t id);

  // Counts entries with refcount > 0, excluding those pending free.
  size_t SizeForTesting() const;

  // Masks every identity hash, so mask 0 forces all entries into one bucket
  // and makes collision handling testable. Call before the first Intern().
  void SetHashMaskForTesting(uint32_t mask) { hash_mask_ = mask; }

  // Seeds the id counter so the wraparound path can be reached in a test
  // without minting 2^32 ids. The next id issued is next + 1 (skipping
  // kNoLabelId). Call before the Intern() under test.
  void SetNextIdForTesting(uint32_t next) { next_id_ = next; }

  // The bound Intern() probes before failing closed on wraparound.
  static constexpr uint32_t ProbeLimitForTesting() { return kIdProbeLimit; }

 private:
  // Upper bound on the linear probe used to find a free id after the counter
  // wraps. Live ids are a small fraction of the 2^32 space, so a free id is
  // typically found in a few probes; the bound limits the search if no free
  // id is present in that window.
  static constexpr uint32_t kIdProbeLimit = 4096;
  struct Entry {
    v8::Global<v8::Value> global;
    uint32_t refcount;
    uint32_t id;
  };

  // Frees each queued id that is still present and still at refcount 0,
  // skipping any revived since queueing. Caller must hold mutex_.
  void DrainPendingFreeLocked();

  v8::Isolate* const isolate_;
  // Guards every field below. Held across the body of each public method, so
  // that a Release() from the ArrayBufferSweeper thread cannot corrupt the
  // table.
  mutable base::Mutex mutex_;
  uint32_t hash_mask_ = 0xffffffff;
  // kNoLabelId is reserved, so the first id issued is 1. Wraparound needs 4
  // billion interns in one isolate; Intern() then probes past kNoLabelId and
  // any still-live id, and fails closed rather than aliasing a live id.
  uint32_t next_id_ = 0;
  std::unordered_map<uint32_t, std::vector<Entry>> buckets_;
  // id -> hash, so Release() and Lookup() find their bucket in O(1).
  std::unordered_map<uint32_t, uint32_t> id_to_hash_;
  std::vector<uint32_t> pending_free_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_PROFILER_SAMPLE_LABELS

#endif  // V8_PROFILER_LABEL_INTERN_TABLE_H_
