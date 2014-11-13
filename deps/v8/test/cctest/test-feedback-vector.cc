// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/api.h"
#include "src/debug.h"
#include "src/execution.h"
#include "src/factory.h"
#include "src/global-handles.h"
#include "src/macro-assembler.h"
#include "src/objects.h"

using namespace v8::internal;

namespace {

TEST(VectorStructure) {
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  // Empty vectors are the empty fixed array.
  Handle<TypeFeedbackVector> vector = factory->NewTypeFeedbackVector(0, 0);
  CHECK(Handle<FixedArray>::cast(vector)
            .is_identical_to(factory->empty_fixed_array()));
  // Which can nonetheless be queried.
  CHECK_EQ(0, vector->ic_with_type_info_count());
  CHECK_EQ(0, vector->ic_generic_count());
  CHECK_EQ(0, vector->Slots());
  CHECK_EQ(0, vector->ICSlots());

  vector = factory->NewTypeFeedbackVector(1, 0);
  CHECK_EQ(1, vector->Slots());
  CHECK_EQ(0, vector->ICSlots());

  vector = factory->NewTypeFeedbackVector(0, 1);
  CHECK_EQ(0, vector->Slots());
  CHECK_EQ(1, vector->ICSlots());

  vector = factory->NewTypeFeedbackVector(3, 5);
  CHECK_EQ(3, vector->Slots());
  CHECK_EQ(5, vector->ICSlots());

  int metadata_length = vector->ic_metadata_length();
  if (!FLAG_vector_ics) {
    CHECK_EQ(0, metadata_length);
  } else {
    CHECK(metadata_length > 0);
  }

  int index = vector->GetIndex(FeedbackVectorSlot(0));
  CHECK_EQ(TypeFeedbackVector::kReservedIndexCount + metadata_length, index);
  CHECK(FeedbackVectorSlot(0) == vector->ToSlot(index));

  index = vector->GetIndex(FeedbackVectorICSlot(0));
  CHECK_EQ(index,
           TypeFeedbackVector::kReservedIndexCount + metadata_length + 3);
  CHECK(FeedbackVectorICSlot(0) == vector->ToICSlot(index));

  CHECK_EQ(TypeFeedbackVector::kReservedIndexCount + metadata_length + 3 + 5,
           vector->length());
}


// IC slots need an encoding to recognize what is in there.
TEST(VectorICMetadata) {
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  if (!FLAG_vector_ics) {
    // If FLAG_vector_ics is false, we only store CALL_ICs in the vector, so
    // there is no need for metadata to describe the slots.
    return;
  }
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  Handle<TypeFeedbackVector> vector =
      factory->NewTypeFeedbackVector(10, 3 * 10);
  CHECK_EQ(10, vector->Slots());
  CHECK_EQ(3 * 10, vector->ICSlots());

  // Set metadata.
  for (int i = 0; i < 30; i++) {
    Code::Kind kind;
    if (i % 3 == 0) {
      kind = Code::CALL_IC;
    } else if (i % 3 == 1) {
      kind = Code::LOAD_IC;
    } else {
      kind = Code::KEYED_LOAD_IC;
    }
    vector->SetKind(FeedbackVectorICSlot(i), kind);
  }

  // Meanwhile set some feedback values and type feedback values to
  // verify the data structure remains intact.
  vector->change_ic_with_type_info_count(100);
  vector->change_ic_generic_count(3333);
  vector->Set(FeedbackVectorSlot(0), *vector);

  // Verify the metadata remains the same.
  for (int i = 0; i < 30; i++) {
    Code::Kind kind = vector->GetKind(FeedbackVectorICSlot(i));
    if (i % 3 == 0) {
      CHECK_EQ(Code::CALL_IC, kind);
    } else if (i % 3 == 1) {
      CHECK_EQ(Code::LOAD_IC, kind);
    } else {
      CHECK_EQ(Code::KEYED_LOAD_IC, kind);
    }
  }
}


TEST(VectorSlotClearing) {
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  // We only test clearing FeedbackVectorSlots, not FeedbackVectorICSlots.
  // The reason is that FeedbackVectorICSlots need a full code environment
  // to fully test (See VectorICProfilerStatistics test below).
  Handle<TypeFeedbackVector> vector = factory->NewTypeFeedbackVector(5, 0);

  // Fill with information
  vector->Set(FeedbackVectorSlot(0), Smi::FromInt(1));
  vector->Set(FeedbackVectorSlot(1), *factory->fixed_array_map());
  Handle<AllocationSite> site = factory->NewAllocationSite();
  vector->Set(FeedbackVectorSlot(2), *site);

  vector->ClearSlots(NULL);

  // The feedback vector slots are cleared. AllocationSites are granted
  // an exemption from clearing, as are smis.
  CHECK_EQ(Smi::FromInt(1), vector->Get(FeedbackVectorSlot(0)));
  CHECK_EQ(*TypeFeedbackVector::UninitializedSentinel(isolate),
           vector->Get(FeedbackVectorSlot(1)));
  CHECK(vector->Get(FeedbackVectorSlot(2))->IsAllocationSite());
}


TEST(VectorICProfilerStatistics) {
  if (i::FLAG_always_opt) return;
  CcTest::InitializeVM();
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();

  // Make sure function f has a call that uses a type feedback slot.
  CompileRun(
      "function fun() {};"
      "function f(a) { a(); } f(fun);");
  Handle<JSFunction> f = v8::Utils::OpenHandle(
      *v8::Handle<v8::Function>::Cast(CcTest::global()->Get(v8_str("f"))));
  // There should be one IC.
  Code* code = f->shared()->code();
  TypeFeedbackInfo* feedback_info =
      TypeFeedbackInfo::cast(code->type_feedback_info());
  CHECK_EQ(1, feedback_info->ic_total_count());
  CHECK_EQ(0, feedback_info->ic_with_type_info_count());
  CHECK_EQ(0, feedback_info->ic_generic_count());
  TypeFeedbackVector* feedback_vector = f->shared()->feedback_vector();
  CHECK_EQ(1, feedback_vector->ic_with_type_info_count());
  CHECK_EQ(0, feedback_vector->ic_generic_count());

  // Now send the information generic.
  CompileRun("f(Object);");
  feedback_vector = f->shared()->feedback_vector();
  CHECK_EQ(0, feedback_vector->ic_with_type_info_count());
  CHECK_EQ(1, feedback_vector->ic_generic_count());

  // A collection will make the site uninitialized again.
  heap->CollectAllGarbage(i::Heap::kNoGCFlags);
  feedback_vector = f->shared()->feedback_vector();
  CHECK_EQ(0, feedback_vector->ic_with_type_info_count());
  CHECK_EQ(0, feedback_vector->ic_generic_count());

  // The Array function is special. A call to array remains monomorphic
  // and isn't cleared by gc because an AllocationSite is being held.
  CompileRun("f(Array);");
  feedback_vector = f->shared()->feedback_vector();
  CHECK_EQ(1, feedback_vector->ic_with_type_info_count());
  CHECK_EQ(0, feedback_vector->ic_generic_count());

  int ic_slot = FLAG_vector_ics ? 1 : 0;
  CHECK(
      feedback_vector->Get(FeedbackVectorICSlot(ic_slot))->IsAllocationSite());
  heap->CollectAllGarbage(i::Heap::kNoGCFlags);
  feedback_vector = f->shared()->feedback_vector();
  CHECK_EQ(1, feedback_vector->ic_with_type_info_count());
  CHECK_EQ(0, feedback_vector->ic_generic_count());
  CHECK(
      feedback_vector->Get(FeedbackVectorICSlot(ic_slot))->IsAllocationSite());
}


TEST(VectorCallICStates) {
  if (i::FLAG_always_opt) return;
  CcTest::InitializeVM();
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();

  // Make sure function f has a call that uses a type feedback slot.
  CompileRun(
      "function foo() { return 17; }"
      "function f(a) { a(); } f(foo);");
  Handle<JSFunction> f = v8::Utils::OpenHandle(
      *v8::Handle<v8::Function>::Cast(CcTest::global()->Get(v8_str("f"))));
  // There should be one IC.
  Handle<TypeFeedbackVector> feedback_vector =
      Handle<TypeFeedbackVector>(f->shared()->feedback_vector(), isolate);
  FeedbackVectorICSlot slot(FLAG_vector_ics ? 1 : 0);
  CallICNexus nexus(feedback_vector, slot);
  CHECK_EQ(MONOMORPHIC, nexus.StateFromFeedback());
  // CallIC doesn't return map feedback.
  CHECK_EQ(NULL, nexus.FindFirstMap());

  CompileRun("f(function() { return 16; })");
  CHECK_EQ(GENERIC, nexus.StateFromFeedback());

  // After a collection, state should be reset to UNINITIALIZED.
  heap->CollectAllGarbage(i::Heap::kNoGCFlags);
  CHECK_EQ(UNINITIALIZED, nexus.StateFromFeedback());

  // Array is special. It will remain monomorphic across gcs and it contains an
  // AllocationSite.
  CompileRun("f(Array)");
  CHECK_EQ(MONOMORPHIC, nexus.StateFromFeedback());
  CHECK(feedback_vector->Get(FeedbackVectorICSlot(slot))->IsAllocationSite());

  heap->CollectAllGarbage(i::Heap::kNoGCFlags);
  CHECK_EQ(MONOMORPHIC, nexus.StateFromFeedback());
}
}
