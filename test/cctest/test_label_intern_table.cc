// Tests for v8::internal::LabelInternTable.
// Exercises the refcounted, identity-hash-keyed intern table that the
// sampling heap profiler uses to dedup per-sample label values.

#include <cstdint>
#include <limits>
#include <thread>  // NOLINT(build/c++11)
#include <vector>

#include "gtest/gtest.h"
#include "node_test_fixture.h"
#include "src/profiler/label-intern-table.h"
#include "v8.h"

#ifdef V8_HEAP_PROFILER_SAMPLE_LABELS

class LabelInternTableTest : public NodeTestFixture {};

TEST_F(LabelInternTableTest, InternSameValueTwiceSameId) {
  const v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);
  v8::internal::LabelInternTable table(isolate_);

  v8::Local<v8::Object> obj = v8::Object::New(isolate_);
  uint32_t id1 = table.Intern(obj);
  uint32_t id2 = table.Intern(obj);
  EXPECT_NE(id1, v8::internal::LabelInternTable::kNoLabelId);
  EXPECT_EQ(id1, id2);
  EXPECT_EQ(1u, table.SizeForTesting());

  // Release once: still resolvable.
  table.Release(id1);
  EXPECT_EQ(1u, table.SizeForTesting());
  EXPECT_FALSE(table.Lookup(id1).IsEmpty());

  // Release twice: gone.
  table.Release(id2);
  EXPECT_EQ(0u, table.SizeForTesting());
  EXPECT_TRUE(table.Lookup(id1).IsEmpty());
}

TEST_F(LabelInternTableTest, DistinctValuesGetDistinctIds) {
  const v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);
  v8::internal::LabelInternTable table(isolate_);

  v8::Local<v8::Object> a = v8::Object::New(isolate_);
  v8::Local<v8::Object> b = v8::Object::New(isolate_);
  uint32_t id_a = table.Intern(a);
  uint32_t id_b = table.Intern(b);
  EXPECT_NE(id_a, id_b);
  EXPECT_EQ(2u, table.SizeForTesting());

  v8::Local<v8::Value> looked_up_a = table.Lookup(id_a).ToLocalChecked();
  v8::Local<v8::Value> looked_up_b = table.Lookup(id_b).ToLocalChecked();
  EXPECT_TRUE(looked_up_a->StrictEquals(a));
  EXPECT_TRUE(looked_up_b->StrictEquals(b));

  table.Release(id_a);
  table.Release(id_b);
  EXPECT_EQ(0u, table.SizeForTesting());
}

TEST_F(LabelInternTableTest, ManyDistinctValuesAllRetrievable) {
  // Identity hash is generated randomly so we cannot easily force a real
  // bucket collision. Instead exercise the chain-walk path indirectly by
  // interning many values and confirming Lookup correctness for each.
  const v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);
  v8::internal::LabelInternTable table(isolate_);

  constexpr int kN = 64;
  v8::LocalVector<v8::Object> objs(isolate_);
  std::vector<uint32_t> ids;
  for (int i = 0; i < kN; ++i) {
    v8::Local<v8::Object> obj = v8::Object::New(isolate_);
    objs.push_back(obj);
    ids.push_back(table.Intern(obj));
  }
  EXPECT_EQ(static_cast<size_t>(kN), table.SizeForTesting());

  for (int i = 0; i < kN; ++i) {
    for (int j = i + 1; j < kN; ++j) {
      EXPECT_NE(ids[i], ids[j]);
    }
  }
  for (int i = 0; i < kN; ++i) {
    v8::Local<v8::Value> got = table.Lookup(ids[i]).ToLocalChecked();
    EXPECT_TRUE(got->StrictEquals(objs[i]));
  }

  // Release in reverse order; table empty at end.
  for (int i = kN - 1; i >= 0; --i) {
    table.Release(ids[i]);
  }
  EXPECT_EQ(0u, table.SizeForTesting());
  for (int i = 0; i < kN; ++i) {
    EXPECT_TRUE(table.Lookup(ids[i]).IsEmpty());
  }
}

TEST_F(LabelInternTableTest, NoLabelIdAndNonReceiver) {
  const v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);
  v8::internal::LabelInternTable table(isolate_);

  // kNoLabelId is reserved.
  EXPECT_TRUE(table.Lookup(v8::internal::LabelInternTable::kNoLabelId)
                  .IsEmpty());
  table.Release(v8::internal::LabelInternTable::kNoLabelId);  // no-op

  // Smi (non-receiver) cannot be interned; returns kNoLabelId.
  v8::Local<v8::Value> smi = v8::Integer::New(isolate_, 42);
  EXPECT_EQ(v8::internal::LabelInternTable::kNoLabelId,
            table.Intern(smi));
  EXPECT_EQ(0u, table.SizeForTesting());
}

TEST_F(LabelInternTableTest, RefcountReleaseOrderIndependent) {
  const v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);
  v8::internal::LabelInternTable table(isolate_);

  v8::Local<v8::Object> obj = v8::Object::New(isolate_);
  uint32_t id_a = table.Intern(obj);
  uint32_t id_b = table.Intern(obj);
  uint32_t id_c = table.Intern(obj);
  EXPECT_EQ(id_a, id_b);
  EXPECT_EQ(id_a, id_c);
  EXPECT_EQ(1u, table.SizeForTesting());

  table.Release(id_a);
  EXPECT_FALSE(table.Lookup(id_a).IsEmpty());
  table.Release(id_b);
  EXPECT_FALSE(table.Lookup(id_a).IsEmpty());
  table.Release(id_c);
  EXPECT_TRUE(table.Lookup(id_a).IsEmpty());
  EXPECT_EQ(0u, table.SizeForTesting());
}

// Concurrent Release() from multiple threads must not corrupt the
// table. Production: ProfilingArrayBufferAllocator::TrackFree() runs on
// V8's ArrayBufferSweeper background worker thread and calls
// Release() while the main thread may also call Intern()/Lookup().
//
// This test pre-bumps the refcount on a single id from the main thread,
// spawns four worker threads that each issue 10000 Release() calls,
// then asserts the refcount drained to exactly one (held by the main
// thread) and the table is internally consistent. The final Release()
// happens on the main thread to avoid racing the underlying
// Global<Value>::Reset() against the isolate.
TEST_F(LabelInternTableTest, ConcurrentReleaseFromManyThreads) {
  const v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);
  v8::internal::LabelInternTable table(isolate_);

  constexpr int kThreads = 4;
  constexpr int kIters = 10000;
  constexpr int kReleasesByWorkers = kThreads * kIters;

  v8::Local<v8::Object> obj = v8::Object::New(isolate_);
  uint32_t id = table.Intern(obj);
  ASSERT_NE(id, v8::internal::LabelInternTable::kNoLabelId);

  // Bump refcount to (kReleasesByWorkers + 1). Workers will drain
  // kReleasesByWorkers; main thread does the final Release.
  for (int i = 0; i < kReleasesByWorkers; ++i) {
    uint32_t bumped = table.Intern(obj);
    ASSERT_EQ(id, bumped);
  }
  EXPECT_EQ(1u, table.SizeForTesting());

  std::vector<std::thread> workers;
  workers.reserve(kThreads);
  for (int t = 0; t < kThreads; ++t) {
    workers.emplace_back([&table, id]() {
      for (int i = 0; i < kIters; ++i) table.Release(id);
    });
  }
  for (auto& w : workers) w.join();

  // Workers drained kReleasesByWorkers refs; one ref remains.
  EXPECT_EQ(1u, table.SizeForTesting());
  EXPECT_FALSE(table.Lookup(id).IsEmpty());

  table.Release(id);
  EXPECT_EQ(0u, table.SizeForTesting());
  EXPECT_TRUE(table.Lookup(id).IsEmpty());
}

// Revival race against the drain queue. Off-thread Release(N) queues
// the id; main-thread Intern(V) must find the still-bucketed entry and
// revive it (refcount==0 -> 1) BEFORE the drain pass runs in the same
// call. The drain skip-when-non-zero guard then leaves the revived
// entry alone, preserving id N and the underlying Global<V>.
//
// Failure modes this catches:
//   * Drain ordered before chain walk -> entry freed -> fresh id
//     allocated, original id N becomes unresolvable, Global Reset.
//   * Drain skip guard missing -> revived entry freed in same call,
//     Lookup(N) returns empty.
//   * Off-thread Release calling Global::Reset() -> use-after-free or
//     GlobalHandles CHECK on isolate's main thread.
TEST_F(LabelInternTableTest, RevivalRaceQueueSafety) {
  const v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);
  v8::internal::LabelInternTable table(isolate_);

  v8::Local<v8::Object> v = v8::Object::New(isolate_);
  uint32_t n = table.Intern(v);
  ASSERT_NE(n, v8::internal::LabelInternTable::kNoLabelId);
  EXPECT_EQ(1u, table.SizeForTesting());

  // Worker drops the only refcount off-thread. The id is queued for
  // free; the entry stays in the bucket at refcount==0 until the next
  // main-thread Intern/Lookup drains it.
  std::thread worker([&table, n]() { table.Release(n); });
  worker.join();

  // Bucket invariants pre-revival: SizeForTesting excludes refcount==0
  // entries (so it reports 0), but Lookup(N) routed through Intern's
  // chain walk should still find the entry because Release only queued
  // it. Note: a direct Lookup() here would drain and free first; that
  // is tested separately. We jump straight to revival via Intern.
  EXPECT_EQ(0u, table.SizeForTesting());

  uint32_t revived = table.Intern(v);
  EXPECT_EQ(n, revived) << "revival must return the original id";
  EXPECT_EQ(1u, table.SizeForTesting());

  v8::Local<v8::Value> looked_up = table.Lookup(n).ToLocalChecked();
  EXPECT_TRUE(looked_up->StrictEquals(v))
      << "Global<V> must not have been Reset across the race";

  // Now drop the revived refcount and force a flush. After this the
  // entry should be properly freed (not just queued forever).
  table.Release(revived);
  EXPECT_EQ(0u, table.SizeForTesting());
  // A second Intern of an unrelated value drains the queue, freeing
  // the entry for v.
  v8::Local<v8::Object> other = v8::Object::New(isolate_);
  uint32_t other_id = table.Intern(other);
  EXPECT_EQ(1u, table.SizeForTesting());
  EXPECT_TRUE(table.Lookup(n).IsEmpty())
      << "drained entry must not be resolvable";
  table.Release(other_id);
  EXPECT_EQ(0u, table.SizeForTesting());
}

// Tests that the revival path in Intern() does not hold a reference into
// buckets_ across DrainPendingFreeLocked(). Three entries A (chain[0]),
// B (chain[1]), C (chain[2]) all share one bucket via SetHashMaskForTesting.
// Off-thread Release(id_a) queues A for drain. Main-thread Intern(B) finds
// Reviving B drains A and shifts the bucket chain. Intern must preserve B's
// id across that mutation rather than reading an invalidated entry.
TEST_F(LabelInternTableTest, DrainDuringRevivalDoesNotInvalidateEntry) {
  const v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);
  v8::internal::LabelInternTable table(isolate_);

  // Force all values into a single bucket: A at chain[0], B at chain[1],
  // C at chain[2]. C acts as the witness: after erasing A, C shifts to
  // chain[1] (where `entry` points), making the wrong return visible.
  table.SetHashMaskForTesting(0);

  v8::Local<v8::Object> a = v8::Object::New(isolate_);
  v8::Local<v8::Object> b = v8::Object::New(isolate_);
  v8::Local<v8::Object> c = v8::Object::New(isolate_);

  uint32_t id_a = table.Intern(a);
  uint32_t id_b = table.Intern(b);
  uint32_t id_c = table.Intern(c);
  ASSERT_NE(id_a, v8::internal::LabelInternTable::kNoLabelId);
  ASSERT_NE(id_b, v8::internal::LabelInternTable::kNoLabelId);
  ASSERT_NE(id_c, v8::internal::LabelInternTable::kNoLabelId);
  ASSERT_NE(id_a, id_b);
  ASSERT_NE(id_b, id_c);
  EXPECT_EQ(3u, table.SizeForTesting());

  // Off-thread: Release A -> refcount 1->0, id_a queued on pending_free_.
  // A stays in the bucket at chain[0] (Release does not erase; no drain
  // has run). B and C retain refcount 1.
  std::thread worker([&table, id_a]() { table.Release(id_a); });
  worker.join();

  EXPECT_EQ(2u, table.SizeForTesting());  // A dead; B and C alive

  // Reviving B drains A and shifts the bucket chain. Intern must return B's
  // saved id rather than reading through an invalidated entry reference.
  uint32_t revived = table.Intern(b);
  EXPECT_EQ(id_b, revived)
      << "revival must return id_b after the bucket chain shifts";
  EXPECT_EQ(2u, table.SizeForTesting());  // B (refcount 2) and C (1) alive

  v8::Local<v8::Value> looked_up = table.Lookup(id_b).ToLocalChecked();
  EXPECT_TRUE(looked_up->StrictEquals(b))
      << "Global<Value> for B must survive the drain";

  // Clean up. B was interned once and revived once (refcount 2), so it
  // needs two releases. Lookup drains the queue for both.
  table.Release(id_b);  // refcount 2 -> 1
  table.Release(id_b);  // refcount 1 -> 0
  table.Release(id_c);  // refcount 1 -> 0
  EXPECT_EQ(0u, table.SizeForTesting());
  EXPECT_TRUE(table.Lookup(id_b).IsEmpty());
  EXPECT_TRUE(table.Lookup(id_c).IsEmpty());
}

// Within one table, Clear() must not make previously issued ids reusable.
TEST_F(LabelInternTableTest, IdsAreNotReusedAfterClear) {
  const v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);
  v8::internal::LabelInternTable table(isolate_);

  // Intern several values before the clear.
  constexpr int kPreClear = 4;
  std::vector<uint32_t> pre_ids;
  for (int i = 0; i < kPreClear; ++i) {
    v8::Local<v8::Object> obj = v8::Object::New(isolate_);
    uint32_t id = table.Intern(obj);
    ASSERT_NE(id, v8::internal::LabelInternTable::kNoLabelId);
    pre_ids.push_back(id);
    table.Release(id);
  }
  EXPECT_EQ(0u, table.SizeForTesting());

  // Simulate stopping the sampling session.
  table.Clear();

  // Intern new values. Their ids must not collide with any pre-Clear id.
  constexpr int kPostClear = 4;
  for (int i = 0; i < kPostClear; ++i) {
    v8::Local<v8::Object> obj = v8::Object::New(isolate_);
    uint32_t post_id = table.Intern(obj);
    ASSERT_NE(post_id, v8::internal::LabelInternTable::kNoLabelId);
    for (uint32_t pre_id : pre_ids) {
      EXPECT_NE(pre_id, post_id)
          << "id " << pre_id << " was reused after Clear()";
    }
    table.Release(post_id);
  }
  EXPECT_EQ(0u, table.SizeForTesting());
}

// When the id counter wraps, Intern() must skip kNoLabelId and any id still
// mapped to a live entry instead of aliasing it. Seed next_id_ just below the
// wrap so the next mints roll over 2^32 - 1 -> 0 (skipped) -> 1, 2, ...
TEST_F(LabelInternTableTest, IdWraparoundDoesNotAliasLiveId) {
  const v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);
  v8::internal::LabelInternTable table(isolate_);

  // Hold a live entry whose id is 1 (the first id issued after the wrap), so
  // the post-wrap mint would collide with it if the guard were absent.
  v8::Local<v8::Object> live = v8::Object::New(isolate_);
  uint32_t live_id = table.Intern(live);
  ASSERT_EQ(live_id, 1u);  // first id ever issued by this table

  // Drive the counter to the top of the range.
  table.SetNextIdForTesting(std::numeric_limits<uint32_t>::max() - 1);

  // max-1 -> max: a normal fresh id.
  v8::Local<v8::Object> a = v8::Object::New(isolate_);
  uint32_t id_a = table.Intern(a);
  EXPECT_EQ(id_a, std::numeric_limits<uint32_t>::max());

  // max -> 0 (kNoLabelId, skipped) -> 1 (live_id, in use, skipped) -> 2.
  v8::Local<v8::Object> b = v8::Object::New(isolate_);
  uint32_t id_b = table.Intern(b);
  EXPECT_NE(id_b, v8::internal::LabelInternTable::kNoLabelId);
  EXPECT_NE(id_b, live_id) << "wrapped id aliased a still-live id";
  EXPECT_EQ(id_b, 2u);

  // The live entry is intact and still resolves to its original value.
  v8::Local<v8::Value> looked_up = table.Lookup(live_id).ToLocalChecked();
  EXPECT_TRUE(looked_up->StrictEquals(live));

  table.Release(id_a);
  table.Release(id_b);
  table.Release(live_id);
  EXPECT_EQ(0u, table.SizeForTesting());
}

// If a full probe sweep after a wrap finds no free id, Intern() must fail
// closed (return kNoLabelId) and leave every existing live entry intact,
// rather than aliasing one.
TEST_F(LabelInternTableTest, IdWraparoundFailsClosedWhenNoFreeId) {
  const v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);
  v8::internal::LabelInternTable table(isolate_);

  // Occupy ids 1..probe+1 with live entries (the table's own Global keeps each
  // object alive), so a full sweep of candidates after a rewind hits no gap.
  const uint32_t probe =
      v8::internal::LabelInternTable::ProbeLimitForTesting();
  std::vector<uint32_t> ids;
  ids.reserve(probe + 1);
  for (uint32_t i = 0; i < probe + 1; ++i) {
    v8::Local<v8::Object> obj = v8::Object::New(isolate_);
    uint32_t id = table.Intern(obj);
    ASSERT_NE(id, v8::internal::LabelInternTable::kNoLabelId);
    ids.push_back(id);
  }
  EXPECT_EQ(static_cast<size_t>(probe + 1), table.SizeForTesting());

  // Rewind so the next mint sweeps candidates 1..probe+1, all occupied.
  table.SetNextIdForTesting(0);
  v8::Local<v8::Object> extra = v8::Object::New(isolate_);
  uint32_t failed = table.Intern(extra);
  EXPECT_EQ(failed, v8::internal::LabelInternTable::kNoLabelId)
      << "Intern must fail closed when no free id is available";

  // The pre-existing live entries are untouched.
  EXPECT_EQ(static_cast<size_t>(probe + 1), table.SizeForTesting());
  for (uint32_t id : ids) table.Release(id);
  EXPECT_EQ(0u, table.SizeForTesting());
}

#endif  // V8_HEAP_PROFILER_SAMPLE_LABELS
