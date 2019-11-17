// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <iostream>
#include <limits>

#include "src/deoptimizer/deoptimizer.h"
#include "src/objects/objects-inl.h"
#include "src/objects/objects.h"
#include "src/objects/osr-optimized-code-cache.h"
#include "test/unittests/test-utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

namespace {

const char* code_template_string =
    "function f%d() { return 0; };"
    "%%PrepareFunctionForOptimization(f%d);"
    "f%d(); f%d();"
    "%%OptimizeFunctionOnNextCall(f%d);"
    "f%d(); f%d;";

void GetSource(i::ScopedVector<char>* source, int index) {
  i::SNPrintF(*source, code_template_string, index, index, index, index, index,
              index, index);
}

const int kInitialLength = OSROptimizedCodeCache::kInitialLength;
const int kInitialEntries =
    kInitialLength / OSROptimizedCodeCache::kEntryLength;
const int kMaxLength = OSROptimizedCodeCache::kMaxLength;
const int kMaxEntries = kMaxLength / OSROptimizedCodeCache::kEntryLength;

}  // namespace

TEST_F(TestWithNativeContext, AddCodeToEmptyCache) {
  if (!i::FLAG_opt) return;

  i::FLAG_allow_natives_syntax = true;

  i::ScopedVector<char> source(1024);
  GetSource(&source, 0);
  Handle<JSFunction> function = RunJS<JSFunction>(source.begin());
  Isolate* isolate = function->GetIsolate();
  Handle<NativeContext> native_context(function->native_context(), isolate);
  Handle<SharedFunctionInfo> shared(function->shared(), isolate);
  Handle<Code> code(function->code(), isolate);
  BailoutId bailout_id(1);
  OSROptimizedCodeCache::AddOptimizedCode(native_context, shared, code,
                                          bailout_id);

  Handle<OSROptimizedCodeCache> osr_cache(
      native_context->GetOSROptimizedCodeCache(), isolate);
  EXPECT_EQ(osr_cache->length(), kInitialLength);

  HeapObject sfi_entry;
  osr_cache->Get(OSROptimizedCodeCache::kSharedOffset)
      ->GetHeapObject(&sfi_entry);
  EXPECT_EQ(sfi_entry, *shared);
  HeapObject code_entry;
  osr_cache->Get(OSROptimizedCodeCache::kCachedCodeOffset)
      ->GetHeapObject(&code_entry);
  EXPECT_EQ(code_entry, *code);
  Smi osr_offset_entry;
  osr_cache->Get(OSROptimizedCodeCache::kOsrIdOffset)->ToSmi(&osr_offset_entry);
  EXPECT_EQ(osr_offset_entry.value(), bailout_id.ToInt());
}

TEST_F(TestWithNativeContext, GrowCodeCache) {
  if (!i::FLAG_opt) return;

  i::FLAG_allow_natives_syntax = true;

  i::ScopedVector<char> source(1024);
  GetSource(&source, 0);
  Handle<JSFunction> function = RunJS<JSFunction>(source.begin());
  Isolate* isolate = function->GetIsolate();
  Handle<NativeContext> native_context(function->native_context(), isolate);
  Handle<SharedFunctionInfo> shared(function->shared(), isolate);
  Handle<Code> code(function->code(), isolate);

  int bailout_id = 0;
  for (bailout_id = 0; bailout_id < kInitialEntries; bailout_id++) {
    OSROptimizedCodeCache::AddOptimizedCode(native_context, shared, code,
                                            BailoutId(bailout_id));
  }
  Handle<OSROptimizedCodeCache> osr_cache(
      native_context->GetOSROptimizedCodeCache(), isolate);
  EXPECT_EQ(osr_cache->length(), kInitialLength);

  OSROptimizedCodeCache::AddOptimizedCode(native_context, shared, code,
                                          BailoutId(bailout_id));
  osr_cache = Handle<OSROptimizedCodeCache>(
      native_context->GetOSROptimizedCodeCache(), isolate);
  EXPECT_EQ(osr_cache->length(), kInitialLength * 2);

  int index = kInitialLength;
  HeapObject sfi_entry;
  osr_cache->Get(index + OSROptimizedCodeCache::kSharedOffset)
      ->GetHeapObject(&sfi_entry);
  EXPECT_EQ(sfi_entry, *shared);
  HeapObject code_entry;
  osr_cache->Get(index + OSROptimizedCodeCache::kCachedCodeOffset)
      ->GetHeapObject(&code_entry);
  EXPECT_EQ(code_entry, *code);
  Smi osr_offset_entry;
  osr_cache->Get(index + OSROptimizedCodeCache::kOsrIdOffset)
      ->ToSmi(&osr_offset_entry);
  EXPECT_EQ(osr_offset_entry.value(), bailout_id);
}

TEST_F(TestWithNativeContext, FindCachedEntry) {
  if (!i::FLAG_opt) return;

  i::FLAG_allow_natives_syntax = true;

  i::ScopedVector<char> source(1024);
  GetSource(&source, 0);
  Handle<JSFunction> function = RunJS<JSFunction>(source.begin());
  Isolate* isolate = function->GetIsolate();
  Handle<NativeContext> native_context(function->native_context(), isolate);
  Handle<SharedFunctionInfo> shared(function->shared(), isolate);
  Handle<Code> code(function->code(), isolate);

  int bailout_id = 0;
  for (bailout_id = 0; bailout_id < kInitialEntries; bailout_id++) {
    OSROptimizedCodeCache::AddOptimizedCode(native_context, shared, code,
                                            BailoutId(bailout_id));
  }

  i::ScopedVector<char> source1(1024);
  GetSource(&source1, 1);
  Handle<JSFunction> function1 = RunJS<JSFunction>(source1.begin());
  Handle<SharedFunctionInfo> shared1(function1->shared(), isolate);
  Handle<Code> code1(function1->code(), isolate);
  OSROptimizedCodeCache::AddOptimizedCode(native_context, shared1, code1,
                                          BailoutId(bailout_id));

  Handle<OSROptimizedCodeCache> osr_cache(
      native_context->GetOSROptimizedCodeCache(), isolate);
  EXPECT_EQ(osr_cache->GetOptimizedCode(shared, BailoutId(0), isolate), *code);
  EXPECT_EQ(
      osr_cache->GetOptimizedCode(shared1, BailoutId(bailout_id), isolate),
      *code1);

  RunJS("%DeoptimizeFunction(f1)");
  EXPECT_TRUE(
      osr_cache->GetOptimizedCode(shared1, BailoutId(bailout_id), isolate)
          .is_null());

  osr_cache->Set(OSROptimizedCodeCache::kCachedCodeOffset,
                 HeapObjectReference::ClearedValue(isolate));
  EXPECT_TRUE(
      osr_cache->GetOptimizedCode(shared, BailoutId(0), isolate).is_null());
}

TEST_F(TestWithNativeContext, MaxCapacityCache) {
  if (!i::FLAG_opt) return;

  i::FLAG_allow_natives_syntax = true;

  i::ScopedVector<char> source(1024);
  GetSource(&source, 0);
  Handle<JSFunction> function = RunJS<JSFunction>(source.begin());
  Isolate* isolate = function->GetIsolate();
  Handle<NativeContext> native_context(function->native_context(), isolate);
  Handle<SharedFunctionInfo> shared(function->shared(), isolate);
  Handle<Code> code(function->code(), isolate);

  int bailout_id = 0;
  // Add max_capacity - 1 entries.
  for (bailout_id = 0; bailout_id < kMaxEntries - 1; bailout_id++) {
    OSROptimizedCodeCache::AddOptimizedCode(native_context, shared, code,
                                            BailoutId(bailout_id));
  }
  Handle<OSROptimizedCodeCache> osr_cache(
      native_context->GetOSROptimizedCodeCache(), isolate);
  EXPECT_EQ(osr_cache->length(), kMaxLength);

  // Add an entry to reach max capacity.
  i::ScopedVector<char> source1(1024);
  GetSource(&source1, 1);
  Handle<JSFunction> function1 = RunJS<JSFunction>(source1.begin());
  Handle<SharedFunctionInfo> shared1(function1->shared(), isolate);
  Handle<Code> code1(function1->code(), isolate);
  OSROptimizedCodeCache::AddOptimizedCode(native_context, shared1, code1,
                                          BailoutId(bailout_id));
  osr_cache = Handle<OSROptimizedCodeCache>(
      native_context->GetOSROptimizedCodeCache(), isolate);
  EXPECT_EQ(osr_cache->length(), kMaxLength);

  int index = (kMaxEntries - 1) * OSROptimizedCodeCache::kEntryLength;
  HeapObject object;
  Smi smi;
  osr_cache->Get(index + OSROptimizedCodeCache::kSharedOffset)
      ->GetHeapObject(&object);
  EXPECT_EQ(object, *shared1);
  osr_cache->Get(index + OSROptimizedCodeCache::kCachedCodeOffset)
      ->GetHeapObject(&object);
  EXPECT_EQ(object, *code1);
  osr_cache->Get(index + OSROptimizedCodeCache::kOsrIdOffset)->ToSmi(&smi);
  EXPECT_EQ(smi.value(), bailout_id);

  // Add an entry beyond max capacity.
  i::ScopedVector<char> source2(1024);
  GetSource(&source2, 2);
  Handle<JSFunction> function2 = RunJS<JSFunction>(source2.begin());
  Handle<SharedFunctionInfo> shared2(function2->shared(), isolate);
  Handle<Code> code2(function2->code(), isolate);
  bailout_id++;
  OSROptimizedCodeCache::AddOptimizedCode(native_context, shared2, code2,
                                          BailoutId(bailout_id));
  osr_cache = Handle<OSROptimizedCodeCache>(
      native_context->GetOSROptimizedCodeCache(), isolate);
  EXPECT_EQ(osr_cache->length(), kMaxLength);

  index = 0;
  osr_cache->Get(index + OSROptimizedCodeCache::kSharedOffset)
      ->GetHeapObject(&object);
  EXPECT_EQ(object, *shared2);
  osr_cache->Get(index + OSROptimizedCodeCache::kCachedCodeOffset)
      ->GetHeapObject(&object);
  EXPECT_EQ(object, *code2);
  osr_cache->Get(index + OSROptimizedCodeCache::kOsrIdOffset)->ToSmi(&smi);
  EXPECT_EQ(smi.value(), bailout_id);
}

TEST_F(TestWithNativeContext, ReuseClearedEntry) {
  if (!i::FLAG_opt) return;

  i::FLAG_allow_natives_syntax = true;

  i::ScopedVector<char> source(1024);
  GetSource(&source, 0);
  Handle<JSFunction> function = RunJS<JSFunction>(source.begin());
  Isolate* isolate = function->GetIsolate();
  Handle<NativeContext> native_context(function->native_context(), isolate);
  Handle<SharedFunctionInfo> shared(function->shared(), isolate);
  Handle<Code> code(function->code(), isolate);

  int num_entries = kInitialEntries * 2;
  int expected_length = kInitialLength * 2;
  int bailout_id = 0;
  for (bailout_id = 0; bailout_id < num_entries; bailout_id++) {
    OSROptimizedCodeCache::AddOptimizedCode(native_context, shared, code,
                                            BailoutId(bailout_id));
  }
  Handle<OSROptimizedCodeCache> osr_cache(
      native_context->GetOSROptimizedCodeCache(), isolate);
  EXPECT_EQ(osr_cache->length(), expected_length);

  int clear_index1 = 0;
  int clear_index2 = (num_entries - 1) * OSROptimizedCodeCache::kEntryLength;
  osr_cache->Set(clear_index1 + OSROptimizedCodeCache::kSharedOffset,
                 HeapObjectReference::ClearedValue(isolate));
  osr_cache->Set(clear_index2 + OSROptimizedCodeCache::kCachedCodeOffset,
                 HeapObjectReference::ClearedValue(isolate));

  i::ScopedVector<char> source1(1024);
  GetSource(&source1, 1);
  Handle<JSFunction> function1 = RunJS<JSFunction>(source1.begin());
  Handle<SharedFunctionInfo> shared1(function1->shared(), isolate);
  Handle<Code> code1(function1->code(), isolate);
  OSROptimizedCodeCache::AddOptimizedCode(native_context, shared1, code1,
                                          BailoutId(bailout_id));
  osr_cache = Handle<OSROptimizedCodeCache>(
      native_context->GetOSROptimizedCodeCache(), isolate);
  EXPECT_EQ(osr_cache->length(), expected_length);

  int index = clear_index1;
  HeapObject object;
  Smi smi;
  osr_cache->Get(index + OSROptimizedCodeCache::kSharedOffset)
      ->GetHeapObject(&object);
  EXPECT_EQ(object, *shared1);
  osr_cache->Get(index + OSROptimizedCodeCache::kCachedCodeOffset)
      ->GetHeapObject(&object);
  EXPECT_EQ(object, *code1);
  osr_cache->Get(index + OSROptimizedCodeCache::kOsrIdOffset)->ToSmi(&smi);
  EXPECT_EQ(smi.value(), bailout_id);

  i::ScopedVector<char> source2(1024);
  GetSource(&source2, 2);
  Handle<JSFunction> function2 = RunJS<JSFunction>(source2.begin());
  Handle<SharedFunctionInfo> shared2(function2->shared(), isolate);
  Handle<Code> code2(function2->code(), isolate);
  bailout_id++;
  OSROptimizedCodeCache::AddOptimizedCode(native_context, shared2, code2,
                                          BailoutId(bailout_id));
  osr_cache = Handle<OSROptimizedCodeCache>(
      native_context->GetOSROptimizedCodeCache(), isolate);
  EXPECT_EQ(osr_cache->length(), expected_length);

  index = clear_index2;
  osr_cache->Get(index + OSROptimizedCodeCache::kSharedOffset)
      ->GetHeapObject(&object);
  EXPECT_EQ(object, *shared2);
  osr_cache->Get(index + OSROptimizedCodeCache::kCachedCodeOffset)
      ->GetHeapObject(&object);
  EXPECT_EQ(object, *code2);
  osr_cache->Get(index + OSROptimizedCodeCache::kOsrIdOffset)->ToSmi(&smi);
  EXPECT_EQ(smi.value(), bailout_id);
}

TEST_F(TestWithNativeContext, EvictDeoptedEntriesNoCompact) {
  if (!i::FLAG_opt) return;

  i::FLAG_allow_natives_syntax = true;

  i::ScopedVector<char> source(1024);
  GetSource(&source, 0);
  Handle<JSFunction> function = RunJS<JSFunction>(source.begin());
  Isolate* isolate = function->GetIsolate();
  Handle<NativeContext> native_context(function->native_context(), isolate);
  Handle<SharedFunctionInfo> shared(function->shared(), isolate);
  Handle<Code> code(function->code(), isolate);

  i::ScopedVector<char> source1(1024);
  GetSource(&source1, 1);
  Handle<JSFunction> deopt_function = RunJS<JSFunction>(source1.begin());
  Handle<SharedFunctionInfo> deopt_shared(deopt_function->shared(), isolate);
  Handle<Code> deopt_code(deopt_function->code(), isolate);

  int num_entries = kInitialEntries * 2;
  int expected_length = kInitialLength * 2;
  int deopt_id1 = num_entries - 2;
  int deopt_id2 = 0;
  int bailout_id = 0;
  for (bailout_id = 0; bailout_id < num_entries; bailout_id++) {
    if (bailout_id == deopt_id1 || bailout_id == deopt_id2) {
      OSROptimizedCodeCache::AddOptimizedCode(
          native_context, deopt_shared, deopt_code, BailoutId(bailout_id));
    } else {
      OSROptimizedCodeCache::AddOptimizedCode(native_context, shared, code,
                                              BailoutId(bailout_id));
    }
  }
  Handle<OSROptimizedCodeCache> osr_cache(
      native_context->GetOSROptimizedCodeCache(), isolate);
  EXPECT_EQ(osr_cache->length(), expected_length);

  RunJS("%DeoptimizeFunction(f1)");
  osr_cache = Handle<OSROptimizedCodeCache>(
      native_context->GetOSROptimizedCodeCache(), isolate);
  EXPECT_EQ(osr_cache->length(), expected_length);

  int index = (num_entries - 2) * OSROptimizedCodeCache::kEntryLength;
  EXPECT_TRUE(osr_cache->Get(index + OSROptimizedCodeCache::kSharedOffset)
                  ->IsCleared());
  EXPECT_TRUE(osr_cache->Get(index + OSROptimizedCodeCache::kCachedCodeOffset)
                  ->IsCleared());
  EXPECT_TRUE(
      osr_cache->Get(index + OSROptimizedCodeCache::kOsrIdOffset)->IsCleared());

  index = (num_entries - 1) * OSROptimizedCodeCache::kEntryLength;
  EXPECT_TRUE(osr_cache->Get(index + OSROptimizedCodeCache::kSharedOffset)
                  ->IsCleared());
  EXPECT_TRUE(osr_cache->Get(index + OSROptimizedCodeCache::kCachedCodeOffset)
                  ->IsCleared());
  EXPECT_TRUE(
      osr_cache->Get(index + OSROptimizedCodeCache::kOsrIdOffset)->IsCleared());
}

TEST_F(TestWithNativeContext, EvictDeoptedEntriesCompact) {
  if (!i::FLAG_opt) return;

  i::FLAG_allow_natives_syntax = true;

  i::ScopedVector<char> source(1024);
  GetSource(&source, 0);
  Handle<JSFunction> function = RunJS<JSFunction>(source.begin());
  Isolate* isolate = function->GetIsolate();
  Handle<NativeContext> native_context(function->native_context(), isolate);
  Handle<SharedFunctionInfo> shared(function->shared(), isolate);
  Handle<Code> code(function->code(), isolate);

  i::ScopedVector<char> source1(1024);
  GetSource(&source1, 1);
  Handle<JSFunction> deopt_function = RunJS<JSFunction>(source1.begin());
  Handle<SharedFunctionInfo> deopt_shared(deopt_function->shared(), isolate);
  Handle<Code> deopt_code(deopt_function->code(), isolate);

  int num_entries = kInitialEntries + 1;
  int expected_length = kInitialLength * 2;
  int bailout_id = 0;
  for (bailout_id = 0; bailout_id < num_entries; bailout_id++) {
    if (bailout_id % 2 == 0) {
      OSROptimizedCodeCache::AddOptimizedCode(
          native_context, deopt_shared, deopt_code, BailoutId(bailout_id));
    } else {
      OSROptimizedCodeCache::AddOptimizedCode(native_context, shared, code,
                                              BailoutId(bailout_id));
    }
  }
  Handle<OSROptimizedCodeCache> osr_cache(
      native_context->GetOSROptimizedCodeCache(), isolate);
  EXPECT_EQ(osr_cache->length(), expected_length);

  RunJS("%DeoptimizeFunction(f1)");
  osr_cache = Handle<OSROptimizedCodeCache>(
      native_context->GetOSROptimizedCodeCache(), isolate);
  EXPECT_EQ(osr_cache->length(), kInitialLength);
}

}  // namespace internal
}  // namespace v8
