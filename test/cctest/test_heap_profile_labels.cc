// Tests for V8 HeapProfileSampleLabelsCallback API.
// Validates the label callback feature at the V8 public API level.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "node_test_fixture.h"
#include "v8-profiler.h"
#include "v8.h"

#ifdef V8_HEAP_PROFILER_SAMPLE_LABELS

// Helper: a label callback that writes fixed labels via output parameter.
static bool FixedLabelsCallback(
    void* data, v8::Local<v8::Value> context,
    std::vector<std::pair<std::string, std::string>>* out_labels) {
  auto* labels =
      static_cast<std::vector<std::pair<std::string, std::string>>*>(data);
  *out_labels = *labels;
  return true;
}

// Helper: a label callback that returns false (no labels).
static bool EmptyLabelsCallback(
    void* data, v8::Local<v8::Value> context,
    std::vector<std::pair<std::string, std::string>>* out_labels) {
  return false;
}

// Helper: a label callback that resolves labels based on the CPED string value.
// Used to verify that different CPED values produce different labels.
struct ContextLabelState {
  v8::Isolate* isolate;
};

static bool ContextBasedLabelsCallback(
    void* data, v8::Local<v8::Value> context,
    std::vector<std::pair<std::string, std::string>>* out_labels) {
  if (context.IsEmpty() || !context->IsString()) return false;
  auto* state = static_cast<ContextLabelState*>(data);
  v8::String::Utf8Value utf8(state->isolate, context);
  std::string cped_str(*utf8, utf8.length());
  if (cped_str == "first") {
    out_labels->push_back({"route", "/api/first"});
  } else if (cped_str == "second") {
    out_labels->push_back({"route", "/api/second"});
  }
  return !out_labels->empty();
}

class HeapProfileLabelsTest : public NodeTestFixture {};

// Test: register callback, allocate, verify labels on samples.
TEST_F(HeapProfileLabelsTest, CallbackReturnsLabels) {
  const v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);

  v8::HeapProfiler* heap_profiler = isolate_->GetHeapProfiler();

  std::vector<std::pair<std::string, std::string>> labels = {
      {"route", "/api/test"}};

  // Set CPED so the callback gets invoked (requires non-empty context).
  isolate_->SetContinuationPreservedEmbedderData(
      v8::String::NewFromUtf8Literal(isolate_, "test-context"));

  heap_profiler->SetHeapProfileSampleLabelsCallback(FixedLabelsCallback,
                                                    &labels);

  heap_profiler->StartSamplingHeapProfiler(256);

  // Allocate enough objects to get samples.
  for (int i = 0; i < 8 * 1024; ++i) v8::Object::New(isolate_);

  std::unique_ptr<v8::AllocationProfile> profile(
      heap_profiler->GetAllocationProfile());
  ASSERT_NE(profile, nullptr);

  bool found_labeled = false;
  for (const auto& sample : profile->GetSamples()) {
    if (!sample.labels.empty()) {
      EXPECT_EQ(sample.labels.size(), 1u);
      EXPECT_EQ(sample.labels[0].first, "route");
      EXPECT_EQ(sample.labels[0].second, "/api/test");
      found_labeled = true;
    }
  }
  EXPECT_TRUE(found_labeled);

  heap_profiler->StopSamplingHeapProfiler();
  heap_profiler->SetHeapProfileSampleLabelsCallback(nullptr, nullptr);
}

// Test: no callback registered — samples have empty labels.
TEST_F(HeapProfileLabelsTest, NoCallbackEmptyLabels) {
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
    EXPECT_TRUE(sample.labels.empty());
  }

  heap_profiler->StopSamplingHeapProfiler();
}

// Test: callback returns empty vector — samples have empty labels.
TEST_F(HeapProfileLabelsTest, EmptyCallbackEmptyLabels) {
  const v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);

  v8::HeapProfiler* heap_profiler = isolate_->GetHeapProfiler();

  // Set CPED so callback is invoked.
  isolate_->SetContinuationPreservedEmbedderData(
      v8::String::NewFromUtf8Literal(isolate_, "test-context"));

  heap_profiler->SetHeapProfileSampleLabelsCallback(EmptyLabelsCallback,
                                                    nullptr);

  heap_profiler->StartSamplingHeapProfiler(256);

  for (int i = 0; i < 8 * 1024; ++i) v8::Object::New(isolate_);

  std::unique_ptr<v8::AllocationProfile> profile(
      heap_profiler->GetAllocationProfile());
  ASSERT_NE(profile, nullptr);

  for (const auto& sample : profile->GetSamples()) {
    EXPECT_TRUE(sample.labels.empty());
  }

  heap_profiler->StopSamplingHeapProfiler();
  heap_profiler->SetHeapProfileSampleLabelsCallback(nullptr, nullptr);
}

// Test: multiple distinct label sets resolved from different CPED values.
// Labels are resolved at read time (BuildSamples) from stored CPED.
TEST_F(HeapProfileLabelsTest, MultipleDistinctLabels) {
  const v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);

  v8::HeapProfiler* heap_profiler = isolate_->GetHeapProfiler();

  ContextLabelState state{isolate_};
  heap_profiler->SetHeapProfileSampleLabelsCallback(ContextBasedLabelsCallback,
                                                    &state);

  heap_profiler->StartSamplingHeapProfiler(256);

  // Allocate with first CPED value.
  isolate_->SetContinuationPreservedEmbedderData(
      v8::String::NewFromUtf8Literal(isolate_, "first"));
  for (int i = 0; i < 4 * 1024; ++i) v8::Object::New(isolate_);

  // Switch to second CPED value.
  isolate_->SetContinuationPreservedEmbedderData(
      v8::String::NewFromUtf8Literal(isolate_, "second"));
  for (int i = 0; i < 4 * 1024; ++i) v8::Object::New(isolate_);

  std::unique_ptr<v8::AllocationProfile> profile(
      heap_profiler->GetAllocationProfile());
  ASSERT_NE(profile, nullptr);

  bool found_first = false;
  bool found_second = false;
  for (const auto& sample : profile->GetSamples()) {
    if (!sample.labels.empty()) {
      EXPECT_EQ(sample.labels.size(), 1u);
      EXPECT_EQ(sample.labels[0].first, "route");
      if (sample.labels[0].second == "/api/first") found_first = true;
      if (sample.labels[0].second == "/api/second") found_second = true;
    }
  }
  EXPECT_TRUE(found_first);
  EXPECT_TRUE(found_second);

  heap_profiler->StopSamplingHeapProfiler();
  heap_profiler->SetHeapProfileSampleLabelsCallback(nullptr, nullptr);
}

// Test: labels survive GC when kSamplingIncludeObjectsCollectedByMajorGC enabled.
TEST_F(HeapProfileLabelsTest, LabelsSurviveGCWithRetainFlags) {
  const v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);

  v8::HeapProfiler* heap_profiler = isolate_->GetHeapProfiler();

  // Set CPED so callback is invoked.
  isolate_->SetContinuationPreservedEmbedderData(
      v8::String::NewFromUtf8Literal(isolate_, "test-context"));

  std::vector<std::pair<std::string, std::string>> labels = {
      {"route", "/api/gc-test"}};
  heap_profiler->SetHeapProfileSampleLabelsCallback(FixedLabelsCallback,
                                                    &labels);

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

  // With GC retain flags, samples for collected objects should still exist
  // with their labels intact.
  bool found_labeled = false;
  for (const auto& sample : profile->GetSamples()) {
    if (!sample.labels.empty()) {
      EXPECT_EQ(sample.labels[0].first, "route");
      EXPECT_EQ(sample.labels[0].second, "/api/gc-test");
      found_labeled = true;
    }
  }
  EXPECT_TRUE(found_labeled);

  heap_profiler->StopSamplingHeapProfiler();
  heap_profiler->SetHeapProfileSampleLabelsCallback(nullptr, nullptr);
}

// Test: labels removed with samples when GC flags disabled and objects collected.
TEST_F(HeapProfileLabelsTest, SamplesRemovedByGCWithoutFlags) {
  const v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);

  v8::HeapProfiler* heap_profiler = isolate_->GetHeapProfiler();

  // Set CPED so callback is invoked.
  isolate_->SetContinuationPreservedEmbedderData(
      v8::String::NewFromUtf8Literal(isolate_, "test-context"));

  std::vector<std::pair<std::string, std::string>> labels = {
      {"route", "/api/gc-remove"}};
  heap_profiler->SetHeapProfileSampleLabelsCallback(FixedLabelsCallback,
                                                    &labels);

  // Start WITHOUT GC retain flags — GC'd samples should be removed.
  heap_profiler->StartSamplingHeapProfiler(256);

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

  // Without GC retain flags, samples for collected objects should be removed.
  // The profile should still be valid (no crash).
  EXPECT_NE(profile->GetRootNode(), nullptr);

  heap_profiler->StopSamplingHeapProfiler();
  heap_profiler->SetHeapProfileSampleLabelsCallback(nullptr, nullptr);
}

// Test: unregister callback — samples allocated after have no stored CPED.
// In the new architecture, CPED is only captured when a callback is registered
// at allocation time. Labels are resolved at read time. Samples without CPED
// get no labels even when the callback is re-registered for reading.
TEST_F(HeapProfileLabelsTest, UnregisterCallbackStopsLabels) {
  const v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);

  v8::HeapProfiler* heap_profiler = isolate_->GetHeapProfiler();

  // Set CPED so it's available during allocation.
  isolate_->SetContinuationPreservedEmbedderData(
      v8::String::NewFromUtf8Literal(isolate_, "test-context"));

  std::vector<std::pair<std::string, std::string>> labels = {
      {"route", "/api/before-unregister"}};
  heap_profiler->SetHeapProfileSampleLabelsCallback(FixedLabelsCallback,
                                                    &labels);

  heap_profiler->StartSamplingHeapProfiler(256);

  // Allocate with callback active — CPED is captured on these samples.
  for (int i = 0; i < 4 * 1024; ++i) v8::Object::New(isolate_);

  // Unregister callback — new samples won't have CPED captured.
  heap_profiler->SetHeapProfileSampleLabelsCallback(nullptr, nullptr);

  // Allocate more — no CPED captured since callback is null at allocation time.
  for (int i = 0; i < 4 * 1024; ++i) v8::Object::New(isolate_);

  // Re-register callback before reading profile. Labels are resolved at
  // read time, so only samples with stored CPED will get labels.
  heap_profiler->SetHeapProfileSampleLabelsCallback(FixedLabelsCallback,
                                                    &labels);

  std::unique_ptr<v8::AllocationProfile> profile(
      heap_profiler->GetAllocationProfile());
  ASSERT_NE(profile, nullptr);

  // Should have labeled samples (CPED captured before unregister) and
  // unlabeled samples (no CPED captured after unregister).
  bool found_labeled = false;
  bool found_unlabeled = false;
  for (const auto& sample : profile->GetSamples()) {
    if (!sample.labels.empty()) {
      found_labeled = true;
    } else {
      found_unlabeled = true;
    }
  }
  EXPECT_TRUE(found_labeled);
  EXPECT_TRUE(found_unlabeled);

  heap_profiler->StopSamplingHeapProfiler();
  heap_profiler->SetHeapProfileSampleLabelsCallback(nullptr, nullptr);
}

#endif  // V8_HEAP_PROFILER_SAMPLE_LABELS
