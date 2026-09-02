// Tests for Sample::label_id and ResolveLabelValue API.
// Validates that label ids are captured at allocation time and resolvable
// via the public HeapProfiler::ResolveLabelValue API.

#include <memory>
#include <string>

#include "gtest/gtest.h"
#include "node_test_fixture.h"
#include "v8-profiler.h"
#include "v8.h"

#ifdef V8_HEAP_PROFILER_SAMPLE_LABELS

// Sets up the ALS key on the heap profiler and stores als_value in a CPED Map.
static void SetupAlsContext(v8::Isolate* isolate, v8::Local<v8::Context> ctx,
                            v8::HeapProfiler* hp,
                            v8::Local<v8::Value> als_value) {
  v8::Local<v8::String> als_key =
      v8::String::NewFromUtf8Literal(isolate, "node-heap-profiler");
  hp->SetHeapProfileSampleLabelsKey(als_key);
  v8::Local<v8::Map> cped_map = v8::Map::New(isolate);
  cped_map->Set(ctx, als_key, als_value).ToLocalChecked();
  isolate->SetContinuationPreservedEmbedderDataV2(cped_map);
}

// Returns a flat V8 Array ["route", route_val] as the ALS label value.
static v8::Local<v8::Array> MakeLabelArray(v8::Isolate* isolate,
                                           v8::Local<v8::Context> ctx,
                                           const char* route_val) {
  v8::Local<v8::Array> arr = v8::Array::New(isolate, 2);
  arr->Set(ctx, 0, v8::String::NewFromUtf8Literal(isolate, "route")).Check();
  arr->Set(ctx, 1, v8::String::NewFromUtf8(isolate, route_val).ToLocalChecked())
      .Check();
  return arr;
}

class HeapProfileLabelsTest : public NodeTestFixture {};

// Test: register callback + set ALS key, allocate, verify label_id on samples.
TEST_F(HeapProfileLabelsTest, CallbackReturnsLabels) {
  const v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);

  v8::HeapProfiler* heap_profiler = isolate_->GetHeapProfiler();

  v8::Local<v8::Array> label_arr =
      MakeLabelArray(isolate_, context, "/api/test");
  SetupAlsContext(isolate_, context, heap_profiler, label_arr);

  heap_profiler->StartSamplingHeapProfiler(256);

  // Allocate enough objects to get samples.
  for (int i = 0; i < 8 * 1024; ++i) v8::Object::New(isolate_);

  std::unique_ptr<v8::AllocationProfile> profile(
      heap_profiler->GetAllocationProfile());
  ASSERT_NE(profile, nullptr);

  bool found_labeled = false;
  for (const auto& sample : profile->GetSamples()) {
    if (sample.label_id != 0) {
      v8::HandleScope hs(isolate_);
      v8::Local<v8::Value> resolved;
      ASSERT_TRUE(heap_profiler->ResolveLabelValue(sample.label_id)
                      .ToLocal(&resolved));
      ASSERT_TRUE(resolved->IsArray());
      v8::Local<v8::Array> arr = resolved.As<v8::Array>();
      ASSERT_GE(arr->Length(), 2u);
      v8::String::Utf8Value key(
          isolate_, arr->Get(context, 0).ToLocalChecked());
      v8::String::Utf8Value val(
          isolate_, arr->Get(context, 1).ToLocalChecked());
      EXPECT_EQ(std::string(*key), "route");
      EXPECT_EQ(std::string(*val), "/api/test");
      found_labeled = true;
    }
  }
  EXPECT_TRUE(found_labeled);

  heap_profiler->StopSamplingHeapProfiler();
}

// Test: no ALS key set — internment gate closed — label_id must be 0.
TEST_F(HeapProfileLabelsTest, NoAlsKeySetEmptyLabels) {
  const v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);

  v8::HeapProfiler* heap_profiler = isolate_->GetHeapProfiler();

  heap_profiler->StartSamplingHeapProfiler(256);

  for (int i = 0; i < 8 * 1024; ++i) v8::Object::New(isolate_);

  std::unique_ptr<v8::AllocationProfile> profile(
      heap_profiler->GetAllocationProfile());
  ASSERT_NE(profile, nullptr);

  for (const auto& sample : profile->GetSamples()) {
    EXPECT_EQ(sample.label_id, 0u);
  }

  heap_profiler->StopSamplingHeapProfiler();
}

// Test: multiple distinct label sets resolved from different ALS values.
TEST_F(HeapProfileLabelsTest, MultipleDistinctLabels) {
  const v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);

  v8::HeapProfiler* heap_profiler = isolate_->GetHeapProfiler();

  v8::Local<v8::String> als_key =
      v8::String::NewFromUtf8Literal(isolate_, "node-heap-profiler");
  heap_profiler->SetHeapProfileSampleLabelsKey(als_key);

  heap_profiler->StartSamplingHeapProfiler(256);

  // Phase 1: allocate under "/api/first".
  v8::Local<v8::Array> arr1 = MakeLabelArray(isolate_, context, "/api/first");
  {
    v8::Local<v8::Map> cped = v8::Map::New(isolate_);
    cped->Set(context, als_key, arr1).ToLocalChecked();
    isolate_->SetContinuationPreservedEmbedderDataV2(cped);
  }
  for (int i = 0; i < 4 * 1024; ++i) v8::Object::New(isolate_);

  // Phase 2: allocate under "/api/second" (different array object).
  v8::Local<v8::Array> arr2 = MakeLabelArray(isolate_, context, "/api/second");
  {
    v8::Local<v8::Map> cped = v8::Map::New(isolate_);
    cped->Set(context, als_key, arr2).ToLocalChecked();
    isolate_->SetContinuationPreservedEmbedderDataV2(cped);
  }
  for (int i = 0; i < 4 * 1024; ++i) v8::Object::New(isolate_);

  std::unique_ptr<v8::AllocationProfile> profile(
      heap_profiler->GetAllocationProfile());
  ASSERT_NE(profile, nullptr);

  bool found_first = false;
  bool found_second = false;
  for (const auto& sample : profile->GetSamples()) {
    if (sample.label_id == 0) continue;
    v8::HandleScope hs(isolate_);
    v8::Local<v8::Value> resolved;
    if (!heap_profiler->ResolveLabelValue(sample.label_id).ToLocal(&resolved))
      continue;
    if (!resolved->IsArray()) continue;
    v8::Local<v8::Array> arr = resolved.As<v8::Array>();
    if (arr->Length() < 2) continue;
    v8::String::Utf8Value val(isolate_,
                              arr->Get(context, 1).ToLocalChecked());
    if (std::string(*val) == "/api/first") found_first = true;
    if (std::string(*val) == "/api/second") found_second = true;
  }
  EXPECT_TRUE(found_first);
  EXPECT_TRUE(found_second);

  heap_profiler->StopSamplingHeapProfiler();
}

// Test: label_id survives GC when
// kSamplingIncludeObjectsCollectedByMajorGC is set.
TEST_F(HeapProfileLabelsTest, LabelsSurviveGCWithRetainFlags) {
  const v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);

  v8::HeapProfiler* heap_profiler = isolate_->GetHeapProfiler();

  v8::Local<v8::Array> label_arr =
      MakeLabelArray(isolate_, context, "/api/gc-test");
  SetupAlsContext(isolate_, context, heap_profiler, label_arr);

  // Start with GC retain flags — GC'd samples should survive.
  heap_profiler->StartSamplingHeapProfiler(
      256, 128,
      static_cast<v8::HeapProfiler::SamplingFlags>(
          v8::HeapProfiler::kSamplingIncludeObjectsCollectedByMajorGC |
          v8::HeapProfiler::kSamplingIncludeObjectsCollectedByMinorGC));

  // Allocate short-lived objects via JS (no reference retained).
  v8::Local<v8::String> source =
      v8::String::NewFromUtf8Literal(isolate_,
          "for (var i = 0; i < 4096; i++) { new Array(64); }");
  v8::Local<v8::Script> script =
      v8::Script::Compile(context, source).ToLocalChecked();
  script->Run(context).ToLocalChecked();

  // Force GC to collect the short-lived objects.
  v8::V8::SetFlagsFromString("--expose-gc");
  isolate_->RequestGarbageCollectionForTesting(
      v8::Isolate::kFullGarbageCollection);

  std::unique_ptr<v8::AllocationProfile> profile(
      heap_profiler->GetAllocationProfile());
  ASSERT_NE(profile, nullptr);

  // Retained samples must still have a resolvable label_id.
  bool found_labeled = false;
  for (const auto& sample : profile->GetSamples()) {
    if (sample.label_id != 0) {
      v8::HandleScope hs(isolate_);
      v8::Local<v8::Value> resolved;
      EXPECT_TRUE(heap_profiler->ResolveLabelValue(sample.label_id)
                      .ToLocal(&resolved));
      EXPECT_TRUE(resolved->IsArray());
      found_labeled = true;
    }
  }
  EXPECT_TRUE(found_labeled);

  heap_profiler->StopSamplingHeapProfiler();
}

// Test: samples removed by GC (no retain flags) — labelled count must drop.
TEST_F(HeapProfileLabelsTest, SamplesRemovedByGCWithoutFlags) {
  const v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);

  v8::HeapProfiler* heap_profiler = isolate_->GetHeapProfiler();

  v8::Local<v8::Array> label_arr =
      MakeLabelArray(isolate_, context, "/api/gc-remove");
  SetupAlsContext(isolate_, context, heap_profiler, label_arr);

  // Start WITHOUT GC retain flags — GC'd samples should be removed.
  heap_profiler->StartSamplingHeapProfiler(256);

  // Allocate short-lived objects via JS (no reference retained).
  v8::Local<v8::String> source =
      v8::String::NewFromUtf8Literal(isolate_,
          "for (var i = 0; i < 4096; i++) { new Array(64); }");
  v8::Local<v8::Script> script =
      v8::Script::Compile(context, source).ToLocalChecked();
  script->Run(context).ToLocalChecked();

  // Count labelled samples before GC — most of the 4096 short-lived arrays
  // should be represented since sampling is probabilistic over 256-byte steps.
  std::unique_ptr<v8::AllocationProfile> pre_gc(
      heap_profiler->GetAllocationProfile());
  ASSERT_NE(pre_gc, nullptr);
  size_t labeled_before = 0;
  for (const auto& s : pre_gc->GetSamples()) {
    if (s.label_id != 0) labeled_before++;
  }
  ASSERT_GT(labeled_before, 0u) << "need labelled samples before GC";

  // Force GC to collect the short-lived objects.
  v8::V8::SetFlagsFromString("--expose-gc");
  isolate_->RequestGarbageCollectionForTesting(
      v8::Isolate::kFullGarbageCollection);

  std::unique_ptr<v8::AllocationProfile> profile(
      heap_profiler->GetAllocationProfile());
  ASSERT_NE(profile, nullptr);
  EXPECT_NE(profile->GetRootNode(), nullptr);

  // Without GC retain flags, samples for collected objects are removed.
  // The labelled count must be strictly less than before the GC.
  size_t labeled_count = 0;
  for (const auto& sample : profile->GetSamples()) {
    if (sample.label_id != 0) labeled_count++;
  }
  EXPECT_LT(labeled_count, labeled_before);

  heap_profiler->StopSamplingHeapProfiler();
}

// Test: after StopSamplingHeapProfiler, ResolveLabelValue returns empty
// and ReleaseLabelValue does not crash for ids from the stopped session.
// This verifies the post-stop contract: Clear() empties the table so old
// ids are stale but safe.
TEST_F(HeapProfileLabelsTest, ReleaseAfterStopIsNoOp) {
  const v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);

  v8::HeapProfiler* heap_profiler = isolate_->GetHeapProfiler();

  v8::Local<v8::Array> label_arr =
      MakeLabelArray(isolate_, context, "/api/stop-test");
  SetupAlsContext(isolate_, context, heap_profiler, label_arr);

  heap_profiler->StartSamplingHeapProfiler(256);

  for (int i = 0; i < 8 * 1024; ++i) v8::Object::New(isolate_);

  std::unique_ptr<v8::AllocationProfile> profile(
      heap_profiler->GetAllocationProfile());
  ASSERT_NE(profile, nullptr);

  uint32_t saved_id = 0;
  for (const auto& sample : profile->GetSamples()) {
    if (sample.label_id != 0) {
      saved_id = sample.label_id;
      break;
    }
  }
  ASSERT_NE(saved_id, 0u) << "need at least one labeled sample";

  heap_profiler->StopSamplingHeapProfiler();

  // After stop: the table is cleared, so Resolve returns empty.
  EXPECT_TRUE(heap_profiler->ResolveLabelValue(saved_id).IsEmpty());

  // After stop: Release must not crash (silent no-op).
  heap_profiler->ReleaseLabelValue(saved_id);
}

// ReleaseLabelValue must remain safe across session stop/start transitions
// and when called from a background thread.
TEST_F(HeapProfileLabelsTest, ReleaseLabelValueCrossThread) {
  const v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);

  v8::HeapProfiler* heap_profiler = isolate_->GetHeapProfiler();

  v8::Local<v8::Array> label_arr =
      MakeLabelArray(isolate_, context, "/api/thread-test");
  SetupAlsContext(isolate_, context, heap_profiler, label_arr);

  // First session: capture a stale id.
  heap_profiler->StartSamplingHeapProfiler(256);
  for (int i = 0; i < 4 * 1024; ++i) v8::Object::New(isolate_);
  std::unique_ptr<v8::AllocationProfile> prof1(
      heap_profiler->GetAllocationProfile());
  ASSERT_NE(prof1, nullptr);
  uint32_t stale_id = 0;
  for (const auto& s : prof1->GetSamples()) {
    if (s.label_id != 0) {
      stale_id = s.label_id;
      break;
    }
  }
  heap_profiler->StopSamplingHeapProfiler();
  // stale_id is now cleared; ReleaseLabelValue(stale_id) must be a no-op.
  ASSERT_NE(stale_id, 0u) << "need at least one labeled sample";

  // Worker calls ReleaseLabelValue with the stale id while the main thread
  // starts and stops a new session. Both calls must not crash.
  std::atomic<bool> done{false};
  std::thread worker([heap_profiler, stale_id, &done]() {
    while (!done.load(std::memory_order_relaxed)) {
      heap_profiler->ReleaseLabelValue(stale_id);
      heap_profiler->ReleaseLabelValue(0);  // kNoLabelId, always a no-op
    }
  });

  heap_profiler->StartSamplingHeapProfiler(256);
  for (int i = 0; i < 4 * 1024; ++i) v8::Object::New(isolate_);
  heap_profiler->StopSamplingHeapProfiler();

  done.store(true, std::memory_order_relaxed);
  worker.join();
}

// Test: clearing the ALS key closes the internment gate — new samples
// get label_id == 0.
TEST_F(HeapProfileLabelsTest, ClearAlsKeyStopsLabels) {
  const v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);

  v8::HeapProfiler* heap_profiler = isolate_->GetHeapProfiler();

  v8::Local<v8::Array> label_arr =
      MakeLabelArray(isolate_, context, "/api/before-clear");
  SetupAlsContext(isolate_, context, heap_profiler, label_arr);

  heap_profiler->StartSamplingHeapProfiler(256);

  // Allocate with ALS key set — label_id will be non-zero.
  for (int i = 0; i < 4 * 1024; ++i) v8::Object::New(isolate_);

  // Clear ALS key — gate closes, new samples get label_id == 0.
  heap_profiler->SetHeapProfileSampleLabelsKey(v8::Local<v8::Value>());

  // Allocate more — no internment since gate is closed.
  for (int i = 0; i < 4 * 1024; ++i) v8::Object::New(isolate_);

  std::unique_ptr<v8::AllocationProfile> profile(
      heap_profiler->GetAllocationProfile());
  ASSERT_NE(profile, nullptr);

  bool found_labeled = false;
  bool found_unlabeled = false;
  for (const auto& sample : profile->GetSamples()) {
    if (sample.label_id != 0) {
      found_labeled = true;
    } else {
      found_unlabeled = true;
    }
  }
  EXPECT_TRUE(found_labeled);
  EXPECT_TRUE(found_unlabeled);

  heap_profiler->StopSamplingHeapProfiler();
}

// Stopping the profiler must release labels held by collected samples before
// clearing the intern table.
TEST_F(HeapProfileLabelsTest, RetainedSampleLabelReleasedOnStop) {
  const v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);

  v8::HeapProfiler* hp = isolate_->GetHeapProfiler();

  v8::Local<v8::Array> label_arr =
      MakeLabelArray(isolate_, context, "/api/retained-stop");
  SetupAlsContext(isolate_, context, hp, label_arr);

  // Use GC retain flag: collected samples keep their label_ids alive until
  // profiler teardown, exercising the destructor release loop.
  hp->StartSamplingHeapProfiler(
      256, 128,
      static_cast<v8::HeapProfiler::SamplingFlags>(
          v8::HeapProfiler::kSamplingIncludeObjectsCollectedByMajorGC |
          v8::HeapProfiler::kSamplingIncludeObjectsCollectedByMinorGC));

  // Allocate short-lived objects; force GC so they become retained samples.
  v8::Local<v8::String> source = v8::String::NewFromUtf8Literal(
      isolate_, "for (var i = 0; i < 4096; i++) { new Array(64); }");
  v8::Local<v8::Script> script =
      v8::Script::Compile(context, source).ToLocalChecked();
  script->Run(context).ToLocalChecked();
  isolate_->RequestGarbageCollectionForTesting(
      v8::Isolate::kFullGarbageCollection);

  // Verify at least one retained labelled sample exists before stop.
  {
    std::unique_ptr<v8::AllocationProfile> profile(hp->GetAllocationProfile());
    ASSERT_NE(profile, nullptr);
    bool has_label = false;
    for (const auto& s : profile->GetSamples()) {
      if (s.label_id != 0) {
        has_label = true;
        break;
      }
    }
    EXPECT_TRUE(has_label) << "need at least one retained labelled sample";
  }

  // This runs the retained-sample label release path.
  hp->StopSamplingHeapProfiler();

  // After stop, stale ids must not resolve.
  hp->ReleaseLabelValue(1u);  // stale, must be a silent no-op
  EXPECT_TRUE(hp->ResolveLabelValue(1u).IsEmpty());
}

// TrackFree must balance its label reference before allocator disable. TSAN
// covers the concurrent TrackFree/Disable interleaving.
TEST_F(HeapProfileLabelsTest, TrackFreeReleasesLabelBeforeDisable) {
  const v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> ctx = v8::Context::New(isolate_);
  v8::Context::Scope ctx_scope(ctx);
  v8::HeapProfiler* hp = isolate_->GetHeapProfiler();

  v8::Local<v8::Array> label_arr =
      MakeLabelArray(isolate_, ctx, "/trackfree-race");
  SetupAlsContext(isolate_, ctx, hp, label_arr);
  hp->StartSamplingHeapProfiler(256);

  node::ProfilingArrayBufferAllocator profiling;
  profiling.Enable(isolate_);

  // Use a sentinel pointer that is unique and non-null.
  void* fake_ptr = reinterpret_cast<void*>(static_cast<uintptr_t>(0x8000));
  profiling.TrackAllocate(fake_ptr, 512);

  // TrackAllocate must have interned the ALS label and added the entry.
  auto entries_before = profiling.GetPerLabelBytes();
  ASSERT_EQ(entries_before.size(), 1u)
      << "TrackAllocate must produce one labeled entry";
  uint32_t label_id = entries_before[0].first;

  // The id is resolvable before TrackFree.
  EXPECT_FALSE(hp->ResolveLabelValue(label_id).IsEmpty());

  // TrackFree erases the entry and releases its label reference.
  profiling.TrackFree(fake_ptr);

  // allocations_ must be empty now.
  EXPECT_TRUE(profiling.GetPerLabelBytes().empty());

  // ResolveLabelValue triggers a drain of the pending_free_ queue; the id
  // must be gone (refcount reached 0 via TrackFree's ReleaseLabelValue).
  EXPECT_TRUE(hp->ResolveLabelValue(label_id).IsEmpty());

  // Disable finds an empty map; it releases nothing.  No double-release.
  profiling.Disable();

  hp->StopSamplingHeapProfiler();
}

#endif  // V8_HEAP_PROFILER_SAMPLE_LABELS
