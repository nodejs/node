// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <utility>

#include "src/init/v8.h"

#include "src/api/api-inl.h"
#include "src/base/overflowing-math.h"
#include "src/builtins/accessors.h"
#include "src/codegen/compilation-cache.h"
#include "src/execution/execution.h"
#include "src/handles/global-handles.h"
#include "src/heap/factory.h"
#include "src/heap/heap-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/spaces.h"
#include "src/ic/ic.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/field-type.h"
#include "src/objects/heap-number-inl.h"
#include "src/objects/layout-descriptor.h"
#include "src/objects/objects-inl.h"
#include "src/objects/property.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {
namespace test_unboxed_doubles {

#if V8_DOUBLE_FIELDS_UNBOXING


//
// Helper functions.
//

static void InitializeVerifiedMapDescriptors(
    Isolate* isolate, Map map, DescriptorArray descriptors,
    LayoutDescriptor layout_descriptor) {
  map.InitializeDescriptors(isolate, descriptors, layout_descriptor);
  CHECK(layout_descriptor.IsConsistentWithMap(map, true));
}

static Handle<String> MakeString(const char* str) {
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  return factory->InternalizeUtf8String(str);
}


static Handle<String> MakeName(const char* str, int suffix) {
  EmbeddedVector<char, 128> buffer;
  SNPrintF(buffer, "%s%d", str, suffix);
  return MakeString(buffer.begin());
}


Handle<JSObject> GetObject(const char* name) {
  return Handle<JSObject>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Object>::Cast(
          CcTest::global()
              ->Get(v8::Isolate::GetCurrent()->GetCurrentContext(),
                    v8_str(name))
              .ToLocalChecked())));
}

static double GetDoubleFieldValue(JSObject obj, FieldIndex field_index) {
  if (obj.IsUnboxedDoubleField(field_index)) {
    return obj.RawFastDoublePropertyAt(field_index);
  } else {
    Object value = obj.RawFastPropertyAt(field_index);
    CHECK(value.IsMutableHeapNumber());
    return MutableHeapNumber::cast(value).value();
  }
}

void WriteToField(JSObject object, int descriptor, Object value) {
  DescriptorArray descriptors = object.map().instance_descriptors();
  PropertyDetails details = descriptors.GetDetails(descriptor);
  object.WriteToField(descriptor, details, value);
}

const int kNumberOfBits = 32;
const int kBitsInSmiLayout = SmiValuesAre32Bits() ? 32 : kSmiValueSize - 1;

enum TestPropertyKind {
  PROP_ACCESSOR_INFO,
  PROP_SMI,
  PROP_DOUBLE,
  PROP_TAGGED,
  PROP_KIND_NUMBER
};

static Representation representations[PROP_KIND_NUMBER] = {
    Representation::None(), Representation::Smi(), Representation::Double(),
    Representation::Tagged()};


static Handle<DescriptorArray> CreateDescriptorArray(Isolate* isolate,
                                                     TestPropertyKind* props,
                                                     int kPropsCount) {
  Factory* factory = isolate->factory();

  Handle<DescriptorArray> descriptors =
      DescriptorArray::Allocate(isolate, 0, kPropsCount);

  int next_field_offset = 0;
  for (int i = 0; i < kPropsCount; i++) {
    EmbeddedVector<char, 64> buffer;
    SNPrintF(buffer, "prop%d", i);
    Handle<String> name = factory->InternalizeUtf8String(buffer.begin());

    TestPropertyKind kind = props[i];

    Descriptor d;
    if (kind == PROP_ACCESSOR_INFO) {
      Handle<AccessorInfo> info =
          Accessors::MakeAccessor(isolate, name, nullptr, nullptr);
      d = Descriptor::AccessorConstant(name, info, NONE);

    } else {
      d = Descriptor::DataField(isolate, name, next_field_offset, NONE,
                                representations[kind]);
    }
    descriptors->Append(&d);
    PropertyDetails details = d.GetDetails();
    if (details.location() == kField) {
      next_field_offset += details.field_width_in_words();
    }
  }
  return descriptors;
}


TEST(LayoutDescriptorBasicFast) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  LayoutDescriptor layout_desc = LayoutDescriptor::FastPointerLayout();

  CHECK(!layout_desc.IsSlowLayout());
  CHECK(layout_desc.IsFastPointerLayout());
  CHECK_EQ(kBitsInSmiLayout, layout_desc.capacity());

  for (int i = 0; i < kBitsInSmiLayout + 13; i++) {
    CHECK(layout_desc.IsTagged(i));
  }
  CHECK(layout_desc.IsTagged(-1));
  CHECK(layout_desc.IsTagged(-12347));
  CHECK(layout_desc.IsTagged(15635));
  CHECK(layout_desc.IsFastPointerLayout());

  for (int i = 0; i < kBitsInSmiLayout; i++) {
    layout_desc = layout_desc.SetTaggedForTesting(i, false);
    CHECK(!layout_desc.IsTagged(i));
    layout_desc = layout_desc.SetTaggedForTesting(i, true);
    CHECK(layout_desc.IsTagged(i));
  }
  CHECK(layout_desc.IsFastPointerLayout());

  int sequence_length;
  CHECK_EQ(true, layout_desc.IsTagged(0, std::numeric_limits<int>::max(),
                                      &sequence_length));
  CHECK_EQ(std::numeric_limits<int>::max(), sequence_length);

  CHECK(layout_desc.IsTagged(0, 7, &sequence_length));
  CHECK_EQ(7, sequence_length);
}


TEST(LayoutDescriptorBasicSlow) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());

  Handle<LayoutDescriptor> layout_descriptor;
  const int kPropsCount = kBitsInSmiLayout * 3;
  TestPropertyKind props[kPropsCount];
  for (int i = 0; i < kPropsCount; i++) {
    // All properties tagged.
    props[i] = PROP_TAGGED;
  }

  {
    Handle<DescriptorArray> descriptors =
        CreateDescriptorArray(isolate, props, kPropsCount);

    Handle<Map> map = Map::Create(isolate, kPropsCount);

    layout_descriptor =
        LayoutDescriptor::New(isolate, map, descriptors, kPropsCount);
    CHECK_EQ(LayoutDescriptor::FastPointerLayout(), *layout_descriptor);
    CHECK_EQ(kBitsInSmiLayout, layout_descriptor->capacity());
    InitializeVerifiedMapDescriptors(isolate, *map, *descriptors,
                                     *layout_descriptor);
  }

  props[0] = PROP_DOUBLE;
  props[kPropsCount - 1] = PROP_DOUBLE;

  Handle<DescriptorArray> descriptors =
      CreateDescriptorArray(isolate, props, kPropsCount);

  {
    int inobject_properties = kPropsCount - 1;
    Handle<Map> map = Map::Create(isolate, inobject_properties);

    // Should be fast as the only double property is the first one.
    layout_descriptor =
        LayoutDescriptor::New(isolate, map, descriptors, kPropsCount);
    CHECK_NE(LayoutDescriptor::FastPointerLayout(), *layout_descriptor);
    CHECK(!layout_descriptor->IsSlowLayout());
    CHECK(!layout_descriptor->IsFastPointerLayout());

    CHECK(!layout_descriptor->IsTagged(0));
    for (int i = 1; i < kPropsCount; i++) {
      CHECK(layout_descriptor->IsTagged(i));
    }
    InitializeVerifiedMapDescriptors(isolate, *map, *descriptors,
                                     *layout_descriptor);
  }

  {
    int inobject_properties = kPropsCount;
    Handle<Map> map = Map::Create(isolate, inobject_properties);

    layout_descriptor =
        LayoutDescriptor::New(isolate, map, descriptors, kPropsCount);
    CHECK_NE(LayoutDescriptor::FastPointerLayout(), *layout_descriptor);
    CHECK(layout_descriptor->IsSlowLayout());
    CHECK(!layout_descriptor->IsFastPointerLayout());
    CHECK_GT(layout_descriptor->capacity(), kBitsInSmiLayout);

    CHECK(!layout_descriptor->IsTagged(0));
    CHECK(!layout_descriptor->IsTagged(kPropsCount - 1));
    for (int i = 1; i < kPropsCount - 1; i++) {
      CHECK(layout_descriptor->IsTagged(i));
    }

    InitializeVerifiedMapDescriptors(isolate, *map, *descriptors,
                                     *layout_descriptor);

    // Here we have truly slow layout descriptor, so play with the bits.
    CHECK(layout_descriptor->IsTagged(-1));
    CHECK(layout_descriptor->IsTagged(-12347));
    CHECK(layout_descriptor->IsTagged(15635));

    LayoutDescriptor layout_desc = *layout_descriptor;
    // Play with the bits but leave it in consistent state with map at the end.
    for (int i = 1; i < kPropsCount - 1; i++) {
      layout_desc = layout_desc.SetTaggedForTesting(i, false);
      CHECK(!layout_desc.IsTagged(i));
      layout_desc = layout_desc.SetTaggedForTesting(i, true);
      CHECK(layout_desc.IsTagged(i));
    }
    CHECK(layout_desc.IsSlowLayout());
    CHECK(!layout_desc.IsFastPointerLayout());
    CHECK(layout_descriptor->IsConsistentWithMap(*map, true));
  }
}


static void TestLayoutDescriptorQueries(int layout_descriptor_length,
                                        int* bit_flip_positions,
                                        int max_sequence_length) {
  Handle<LayoutDescriptor> layout_descriptor = LayoutDescriptor::NewForTesting(
      CcTest::i_isolate(), layout_descriptor_length);
  layout_descriptor_length = layout_descriptor->capacity();
  LayoutDescriptor layout_desc = *layout_descriptor;

  {
    // Fill in the layout descriptor.
    int cur_bit_flip_index = 0;
    bool tagged = true;
    for (int i = 0; i < layout_descriptor_length; i++) {
      if (i == bit_flip_positions[cur_bit_flip_index]) {
        tagged = !tagged;
        ++cur_bit_flip_index;
        CHECK(i < bit_flip_positions[cur_bit_flip_index]);  // check test data
      }
      layout_desc = layout_desc.SetTaggedForTesting(i, tagged);
    }
  }

  if (layout_desc.IsFastPointerLayout()) {
    return;
  }

  {
    // Check queries.
    int cur_bit_flip_index = 0;
    bool tagged = true;
    for (int i = 0; i < layout_descriptor_length; i++) {
      if (i == bit_flip_positions[cur_bit_flip_index]) {
        tagged = !tagged;
        ++cur_bit_flip_index;
      }
      CHECK_EQ(tagged, layout_desc.IsTagged(i));

      int next_bit_flip_position = bit_flip_positions[cur_bit_flip_index];
      int expected_sequence_length;
      if (next_bit_flip_position < layout_desc.capacity()) {
        expected_sequence_length = next_bit_flip_position - i;
      } else {
        expected_sequence_length = tagged ? std::numeric_limits<int>::max()
                                          : (layout_desc.capacity() - i);
      }
      expected_sequence_length =
          Min(expected_sequence_length, max_sequence_length);
      int sequence_length;
      CHECK_EQ(tagged,
               layout_desc.IsTagged(i, max_sequence_length, &sequence_length));
      CHECK_GT(sequence_length, 0);

      CHECK_EQ(expected_sequence_length, sequence_length);
    }

    int sequence_length;
    CHECK_EQ(true, layout_desc.IsTagged(layout_descriptor_length,
                                        max_sequence_length, &sequence_length));
    CHECK_EQ(max_sequence_length, sequence_length);
  }
}


static void TestLayoutDescriptorQueriesFast(int max_sequence_length) {
  {
    LayoutDescriptor layout_desc = LayoutDescriptor::FastPointerLayout();
    int sequence_length;
    for (int i = 0; i < kNumberOfBits; i++) {
      CHECK_EQ(true,
               layout_desc.IsTagged(i, max_sequence_length, &sequence_length));
      CHECK_GT(sequence_length, 0);
      CHECK_EQ(max_sequence_length, sequence_length);
    }
  }

  {
    int bit_flip_positions[] = {1000};
    TestLayoutDescriptorQueries(kBitsInSmiLayout, bit_flip_positions,
                                max_sequence_length);
  }

  {
    int bit_flip_positions[] = {0, 1000};
    TestLayoutDescriptorQueries(kBitsInSmiLayout, bit_flip_positions,
                                max_sequence_length);
  }

  {
    int bit_flip_positions[kNumberOfBits + 1];
    for (int i = 0; i <= kNumberOfBits; i++) {
      bit_flip_positions[i] = i;
    }
    TestLayoutDescriptorQueries(kBitsInSmiLayout, bit_flip_positions,
                                max_sequence_length);
  }

  {
    int bit_flip_positions[] = {3, 7, 8, 10, 15, 21, 30, 1000};
    TestLayoutDescriptorQueries(kBitsInSmiLayout, bit_flip_positions,
                                max_sequence_length);
  }

  {
    int bit_flip_positions[] = {0,  1,  2,  3,  5,  7,  9,
                                12, 15, 18, 22, 26, 29, 1000};
    TestLayoutDescriptorQueries(kBitsInSmiLayout, bit_flip_positions,
                                max_sequence_length);
  }
}


TEST(LayoutDescriptorQueriesFastLimited7) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  TestLayoutDescriptorQueriesFast(7);
}


TEST(LayoutDescriptorQueriesFastLimited13) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  TestLayoutDescriptorQueriesFast(13);
}


TEST(LayoutDescriptorQueriesFastUnlimited) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  TestLayoutDescriptorQueriesFast(std::numeric_limits<int>::max());
}


static void TestLayoutDescriptorQueriesSlow(int max_sequence_length) {
  {
    int bit_flip_positions[] = {10000};
    TestLayoutDescriptorQueries(kMaxNumberOfDescriptors, bit_flip_positions,
                                max_sequence_length);
  }

  {
    int bit_flip_positions[] = {0, 10000};
    TestLayoutDescriptorQueries(kMaxNumberOfDescriptors, bit_flip_positions,
                                max_sequence_length);
  }

  {
    int bit_flip_positions[kMaxNumberOfDescriptors + 1];
    for (int i = 0; i < kMaxNumberOfDescriptors; i++) {
      bit_flip_positions[i] = i;
    }
    bit_flip_positions[kMaxNumberOfDescriptors] = 10000;
    TestLayoutDescriptorQueries(kMaxNumberOfDescriptors, bit_flip_positions,
                                max_sequence_length);
  }

  {
    int bit_flip_positions[] = {3,  7,  8,  10, 15,  21,   30,
                                37, 54, 80, 99, 383, 10000};
    TestLayoutDescriptorQueries(kMaxNumberOfDescriptors, bit_flip_positions,
                                max_sequence_length);
  }

  {
    int bit_flip_positions[] = {0,   10,  20,  30,  50,  70,  90,
                                120, 150, 180, 220, 260, 290, 10000};
    TestLayoutDescriptorQueries(kMaxNumberOfDescriptors, bit_flip_positions,
                                max_sequence_length);
  }

  {
    int bit_flip_positions[kMaxNumberOfDescriptors + 1];
    int cur = 0;
    for (int i = 0; i < kMaxNumberOfDescriptors; i++) {
      bit_flip_positions[i] = cur;
      cur = base::MulWithWraparound((cur + 1), 2);
    }
    CHECK_LT(cur, 10000);
    bit_flip_positions[kMaxNumberOfDescriptors] = 10000;
    TestLayoutDescriptorQueries(kMaxNumberOfDescriptors, bit_flip_positions,
                                max_sequence_length);
  }

  {
    int bit_flip_positions[kMaxNumberOfDescriptors + 1];
    int cur = 3;
    for (int i = 0; i < kMaxNumberOfDescriptors; i++) {
      bit_flip_positions[i] = cur;
      cur = base::MulWithWraparound((cur + 1), 2);
    }
    CHECK_LT(cur, 10000);
    bit_flip_positions[kMaxNumberOfDescriptors] = 10000;
    TestLayoutDescriptorQueries(kMaxNumberOfDescriptors, bit_flip_positions,
                                max_sequence_length);
  }
}


TEST(LayoutDescriptorQueriesSlowLimited7) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  TestLayoutDescriptorQueriesSlow(7);
}


TEST(LayoutDescriptorQueriesSlowLimited13) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  TestLayoutDescriptorQueriesSlow(13);
}


TEST(LayoutDescriptorQueriesSlowLimited42) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  TestLayoutDescriptorQueriesSlow(42);
}


TEST(LayoutDescriptorQueriesSlowUnlimited) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  TestLayoutDescriptorQueriesSlow(std::numeric_limits<int>::max());
}


TEST(LayoutDescriptorCreateNewFast) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());

  Handle<LayoutDescriptor> layout_descriptor;
  TestPropertyKind props[] = {
      PROP_ACCESSOR_INFO,
      PROP_TAGGED,  // field #0
      PROP_ACCESSOR_INFO,
      PROP_DOUBLE,  // field #1
      PROP_ACCESSOR_INFO,
      PROP_TAGGED,  // field #2
      PROP_ACCESSOR_INFO,
  };
  const int kPropsCount = arraysize(props);

  Handle<DescriptorArray> descriptors =
      CreateDescriptorArray(isolate, props, kPropsCount);

  {
    Handle<Map> map = Map::Create(isolate, 0);
    layout_descriptor =
        LayoutDescriptor::New(isolate, map, descriptors, kPropsCount);
    CHECK_EQ(LayoutDescriptor::FastPointerLayout(), *layout_descriptor);
    InitializeVerifiedMapDescriptors(isolate, *map, *descriptors,
                                     *layout_descriptor);
  }

  {
    Handle<Map> map = Map::Create(isolate, 1);
    layout_descriptor =
        LayoutDescriptor::New(isolate, map, descriptors, kPropsCount);
    CHECK_EQ(LayoutDescriptor::FastPointerLayout(), *layout_descriptor);
    InitializeVerifiedMapDescriptors(isolate, *map, *descriptors,
                                     *layout_descriptor);
  }

  {
    Handle<Map> map = Map::Create(isolate, 2);
    layout_descriptor =
        LayoutDescriptor::New(isolate, map, descriptors, kPropsCount);
    CHECK_NE(LayoutDescriptor::FastPointerLayout(), *layout_descriptor);
    CHECK(!layout_descriptor->IsSlowLayout());
    CHECK(layout_descriptor->IsTagged(0));
    CHECK(!layout_descriptor->IsTagged(1));
    CHECK(layout_descriptor->IsTagged(2));
    CHECK(layout_descriptor->IsTagged(125));
    InitializeVerifiedMapDescriptors(isolate, *map, *descriptors,
                                     *layout_descriptor);
  }
}


TEST(LayoutDescriptorCreateNewSlow) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());

  Handle<LayoutDescriptor> layout_descriptor;
  const int kPropsCount = kBitsInSmiLayout * 3;
  TestPropertyKind props[kPropsCount];
  for (int i = 0; i < kPropsCount; i++) {
    props[i] = static_cast<TestPropertyKind>(i % PROP_KIND_NUMBER);
  }

  Handle<DescriptorArray> descriptors =
      CreateDescriptorArray(isolate, props, kPropsCount);

  {
    Handle<Map> map = Map::Create(isolate, 0);
    layout_descriptor =
        LayoutDescriptor::New(isolate, map, descriptors, kPropsCount);
    CHECK_EQ(LayoutDescriptor::FastPointerLayout(), *layout_descriptor);
    InitializeVerifiedMapDescriptors(isolate, *map, *descriptors,
                                     *layout_descriptor);
  }

  {
    Handle<Map> map = Map::Create(isolate, 1);
    layout_descriptor =
        LayoutDescriptor::New(isolate, map, descriptors, kPropsCount);
    CHECK_EQ(LayoutDescriptor::FastPointerLayout(), *layout_descriptor);
    InitializeVerifiedMapDescriptors(isolate, *map, *descriptors,
                                     *layout_descriptor);
  }

  {
    Handle<Map> map = Map::Create(isolate, 2);
    layout_descriptor =
        LayoutDescriptor::New(isolate, map, descriptors, kPropsCount);
    CHECK_NE(LayoutDescriptor::FastPointerLayout(), *layout_descriptor);
    CHECK(!layout_descriptor->IsSlowLayout());
    CHECK(layout_descriptor->IsTagged(0));
    CHECK(!layout_descriptor->IsTagged(1));
    CHECK(layout_descriptor->IsTagged(2));
    CHECK(layout_descriptor->IsTagged(125));
    InitializeVerifiedMapDescriptors(isolate, *map, *descriptors,
                                     *layout_descriptor);
  }

  {
    int inobject_properties = kPropsCount / 2;
    Handle<Map> map = Map::Create(isolate, inobject_properties);
    layout_descriptor =
        LayoutDescriptor::New(isolate, map, descriptors, kPropsCount);
    CHECK_NE(LayoutDescriptor::FastPointerLayout(), *layout_descriptor);
    CHECK(layout_descriptor->IsSlowLayout());
    for (int i = 0; i < inobject_properties; i++) {
      // PROP_DOUBLE has index 1 among DATA properties.
      const bool tagged = (i % (PROP_KIND_NUMBER - 1)) != 1;
      CHECK_EQ(tagged, layout_descriptor->IsTagged(i));
    }
    // Every property after inobject_properties must be tagged.
    for (int i = inobject_properties; i < kPropsCount; i++) {
      CHECK(layout_descriptor->IsTagged(i));
    }
    InitializeVerifiedMapDescriptors(isolate, *map, *descriptors,
                                     *layout_descriptor);

    // Now test LayoutDescriptor::cast_gc_safe().
    Handle<LayoutDescriptor> layout_descriptor_copy =
        LayoutDescriptor::New(isolate, map, descriptors, kPropsCount);

    LayoutDescriptor layout_desc = *layout_descriptor;
    CHECK_EQ(layout_desc, LayoutDescriptor::cast(layout_desc));
    CHECK_EQ(layout_desc, LayoutDescriptor::cast_gc_safe(layout_desc));
    CHECK(layout_desc.IsSlowLayout());
    // Now make it look like a forwarding pointer to layout_descriptor_copy.
    MapWord map_word = layout_desc.map_word();
    CHECK(!map_word.IsForwardingAddress());
    layout_desc.set_map_word(
        MapWord::FromForwardingAddress(*layout_descriptor_copy));
    CHECK(layout_desc.map_word().IsForwardingAddress());
    CHECK_EQ(layout_desc, LayoutDescriptor::cast_gc_safe(layout_desc));

    // Restore it back.
    layout_desc.set_map_word(map_word);
    CHECK_EQ(layout_desc, LayoutDescriptor::cast(layout_desc));
  }
}


static Handle<LayoutDescriptor> TestLayoutDescriptorAppend(
    Isolate* isolate, int inobject_properties, TestPropertyKind* props,
    int kPropsCount) {
  Factory* factory = isolate->factory();

  Handle<DescriptorArray> descriptors =
      DescriptorArray::Allocate(isolate, 0, kPropsCount);

  Handle<Map> map = Map::Create(isolate, inobject_properties);
  map->InitializeDescriptors(isolate, *descriptors,
                             LayoutDescriptor::FastPointerLayout());

  int next_field_offset = 0;
  for (int i = 0; i < kPropsCount; i++) {
    EmbeddedVector<char, 64> buffer;
    SNPrintF(buffer, "prop%d", i);
    Handle<String> name = factory->InternalizeUtf8String(buffer.begin());

    Handle<LayoutDescriptor> layout_descriptor;
    TestPropertyKind kind = props[i];
    Descriptor d;
    if (kind == PROP_ACCESSOR_INFO) {
      Handle<AccessorInfo> info =
          Accessors::MakeAccessor(isolate, name, nullptr, nullptr);
      d = Descriptor::AccessorConstant(name, info, NONE);

    } else {
      d = Descriptor::DataField(isolate, name, next_field_offset, NONE,
                                representations[kind]);
    }
    PropertyDetails details = d.GetDetails();
    layout_descriptor = LayoutDescriptor::ShareAppend(isolate, map, details);
    descriptors->Append(&d);
    if (details.location() == kField) {
      int field_width_in_words = details.field_width_in_words();
      next_field_offset += field_width_in_words;

      int field_index = details.field_index();
      bool is_inobject = field_index < map->GetInObjectProperties();
      for (int bit = 0; bit < field_width_in_words; bit++) {
        CHECK_EQ(is_inobject && (kind == PROP_DOUBLE),
                 !layout_descriptor->IsTagged(field_index + bit));
      }
      CHECK(layout_descriptor->IsTagged(next_field_offset));
    }
    map->InitializeDescriptors(isolate, *descriptors, *layout_descriptor);
  }
  Handle<LayoutDescriptor> layout_descriptor(map->layout_descriptor(), isolate);
  CHECK(layout_descriptor->IsConsistentWithMap(*map, true));
  return layout_descriptor;
}


TEST(LayoutDescriptorAppend) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());

  Handle<LayoutDescriptor> layout_descriptor;
  const int kPropsCount = kBitsInSmiLayout * 3;
  TestPropertyKind props[kPropsCount];
  for (int i = 0; i < kPropsCount; i++) {
    props[i] = static_cast<TestPropertyKind>(i % PROP_KIND_NUMBER);
  }

  layout_descriptor =
      TestLayoutDescriptorAppend(isolate, 0, props, kPropsCount);
  CHECK(!layout_descriptor->IsSlowLayout());

  layout_descriptor =
      TestLayoutDescriptorAppend(isolate, 13, props, kPropsCount);
  CHECK(!layout_descriptor->IsSlowLayout());

  layout_descriptor =
      TestLayoutDescriptorAppend(isolate, kBitsInSmiLayout, props, kPropsCount);
  CHECK(!layout_descriptor->IsSlowLayout());

  layout_descriptor = TestLayoutDescriptorAppend(isolate, kBitsInSmiLayout * 2,
                                                 props, kPropsCount);
  CHECK(layout_descriptor->IsSlowLayout());

  layout_descriptor =
      TestLayoutDescriptorAppend(isolate, kPropsCount, props, kPropsCount);
  CHECK(layout_descriptor->IsSlowLayout());
}


TEST(LayoutDescriptorAppendAllDoubles) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());

  Handle<LayoutDescriptor> layout_descriptor;
  const int kPropsCount = kBitsInSmiLayout * 3;
  TestPropertyKind props[kPropsCount];
  for (int i = 0; i < kPropsCount; i++) {
    props[i] = PROP_DOUBLE;
  }

  layout_descriptor =
      TestLayoutDescriptorAppend(isolate, 0, props, kPropsCount);
  CHECK(!layout_descriptor->IsSlowLayout());

  layout_descriptor =
      TestLayoutDescriptorAppend(isolate, 13, props, kPropsCount);
  CHECK(!layout_descriptor->IsSlowLayout());

  layout_descriptor =
      TestLayoutDescriptorAppend(isolate, kBitsInSmiLayout, props, kPropsCount);
  CHECK(!layout_descriptor->IsSlowLayout());

  layout_descriptor = TestLayoutDescriptorAppend(isolate, kBitsInSmiLayout + 1,
                                                 props, kPropsCount);
  CHECK(layout_descriptor->IsSlowLayout());

  layout_descriptor = TestLayoutDescriptorAppend(isolate, kBitsInSmiLayout * 2,
                                                 props, kPropsCount);
  CHECK(layout_descriptor->IsSlowLayout());

  layout_descriptor =
      TestLayoutDescriptorAppend(isolate, kPropsCount, props, kPropsCount);
  CHECK(layout_descriptor->IsSlowLayout());

  {
    // Ensure layout descriptor switches into slow mode at the right moment.
    layout_descriptor = TestLayoutDescriptorAppend(isolate, kPropsCount, props,
                                                   kBitsInSmiLayout);
    CHECK(!layout_descriptor->IsSlowLayout());

    layout_descriptor = TestLayoutDescriptorAppend(isolate, kPropsCount, props,
                                                   kBitsInSmiLayout + 1);
    CHECK(layout_descriptor->IsSlowLayout());
  }
}


static Handle<LayoutDescriptor> TestLayoutDescriptorAppendIfFastOrUseFull(
    Isolate* isolate, int inobject_properties,
    Handle<DescriptorArray> descriptors, int number_of_descriptors) {
  Handle<Map> initial_map = Map::Create(isolate, inobject_properties);

  Handle<LayoutDescriptor> full_layout_descriptor = LayoutDescriptor::New(
      isolate, initial_map, descriptors, descriptors->number_of_descriptors());

  int nof = 0;
  bool switched_to_slow_mode = false;

  // This method calls LayoutDescriptor::AppendIfFastOrUseFull() internally
  // and does all the required map-descriptors related book keeping.
  Handle<Map> last_map = Map::AddMissingTransitionsForTesting(
      isolate, initial_map, descriptors, full_layout_descriptor);

  // Follow back pointers to construct a sequence of maps from |map|
  // to |last_map|.
  int descriptors_length = descriptors->number_of_descriptors();
  std::vector<Handle<Map>> maps(descriptors_length);
  {
    CHECK(last_map->is_stable());
    Map map = *last_map;
    for (int i = 0; i < descriptors_length; i++) {
      maps[descriptors_length - 1 - i] = handle(map, isolate);
      Object maybe_map = map.GetBackPointer();
      CHECK(maybe_map.IsMap());
      map = Map::cast(maybe_map);
      CHECK(!map.is_stable());
    }
    CHECK_EQ(1, maps[0]->NumberOfOwnDescriptors());
  }

  Handle<Map> map;
  // Now check layout descriptors of all intermediate maps.
  for (int i = 0; i < number_of_descriptors; i++) {
    PropertyDetails details = descriptors->GetDetails(i);
    map = maps[i];
    LayoutDescriptor layout_desc = map->layout_descriptor();

    if (layout_desc.IsSlowLayout()) {
      switched_to_slow_mode = true;
      CHECK_EQ(*full_layout_descriptor, layout_desc);
    } else {
      CHECK(!switched_to_slow_mode);
      if (details.location() == kField) {
        nof++;
        int field_index = details.field_index();
        int field_width_in_words = details.field_width_in_words();

        bool is_inobject = field_index < map->GetInObjectProperties();
        for (int bit = 0; bit < field_width_in_words; bit++) {
          CHECK_EQ(is_inobject && details.representation().IsDouble(),
                   !layout_desc.IsTagged(field_index + bit));
        }
        CHECK(layout_desc.IsTagged(field_index + field_width_in_words));
      }
    }
    CHECK(map->layout_descriptor().IsConsistentWithMap(*map));
  }

  Handle<LayoutDescriptor> layout_descriptor(map->GetLayoutDescriptor(),
                                             isolate);
  CHECK(layout_descriptor->IsConsistentWithMap(*map));
  return layout_descriptor;
}


TEST(LayoutDescriptorAppendIfFastOrUseFull) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());

  Handle<LayoutDescriptor> layout_descriptor;
  const int kPropsCount = kBitsInSmiLayout * 3;
  TestPropertyKind props[kPropsCount];
  for (int i = 0; i < kPropsCount; i++) {
    props[i] = static_cast<TestPropertyKind>(i % PROP_KIND_NUMBER);
  }
  Handle<DescriptorArray> descriptors =
      CreateDescriptorArray(isolate, props, kPropsCount);

  layout_descriptor = TestLayoutDescriptorAppendIfFastOrUseFull(
      isolate, 0, descriptors, kPropsCount);
  CHECK(!layout_descriptor->IsSlowLayout());

  layout_descriptor = TestLayoutDescriptorAppendIfFastOrUseFull(
      isolate, 13, descriptors, kPropsCount);
  CHECK(!layout_descriptor->IsSlowLayout());

  layout_descriptor = TestLayoutDescriptorAppendIfFastOrUseFull(
      isolate, kBitsInSmiLayout, descriptors, kPropsCount);
  CHECK(!layout_descriptor->IsSlowLayout());

  layout_descriptor = TestLayoutDescriptorAppendIfFastOrUseFull(
      isolate, kBitsInSmiLayout * 2, descriptors, kPropsCount);
  CHECK(layout_descriptor->IsSlowLayout());

  layout_descriptor = TestLayoutDescriptorAppendIfFastOrUseFull(
      isolate, kPropsCount, descriptors, kPropsCount);
  CHECK(layout_descriptor->IsSlowLayout());
}


TEST(LayoutDescriptorAppendIfFastOrUseFullAllDoubles) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());

  Handle<LayoutDescriptor> layout_descriptor;
  const int kPropsCount = kBitsInSmiLayout * 3;
  TestPropertyKind props[kPropsCount];
  for (int i = 0; i < kPropsCount; i++) {
    props[i] = PROP_DOUBLE;
  }
  Handle<DescriptorArray> descriptors =
      CreateDescriptorArray(isolate, props, kPropsCount);

  layout_descriptor = TestLayoutDescriptorAppendIfFastOrUseFull(
      isolate, 0, descriptors, kPropsCount);
  CHECK(!layout_descriptor->IsSlowLayout());

  layout_descriptor = TestLayoutDescriptorAppendIfFastOrUseFull(
      isolate, 13, descriptors, kPropsCount);
  CHECK(!layout_descriptor->IsSlowLayout());

  layout_descriptor = TestLayoutDescriptorAppendIfFastOrUseFull(
      isolate, kBitsInSmiLayout, descriptors, kPropsCount);
  CHECK(!layout_descriptor->IsSlowLayout());

  layout_descriptor = TestLayoutDescriptorAppendIfFastOrUseFull(
      isolate, kBitsInSmiLayout + 1, descriptors, kPropsCount);
  CHECK(layout_descriptor->IsSlowLayout());

  layout_descriptor = TestLayoutDescriptorAppendIfFastOrUseFull(
      isolate, kBitsInSmiLayout * 2, descriptors, kPropsCount);
  CHECK(layout_descriptor->IsSlowLayout());

  layout_descriptor = TestLayoutDescriptorAppendIfFastOrUseFull(
      isolate, kPropsCount, descriptors, kPropsCount);
  CHECK(layout_descriptor->IsSlowLayout());

  {
    // Ensure layout descriptor switches into slow mode at the right moment.
    layout_descriptor = TestLayoutDescriptorAppendIfFastOrUseFull(
        isolate, kPropsCount, descriptors, kBitsInSmiLayout);
    CHECK(!layout_descriptor->IsSlowLayout());

    layout_descriptor = TestLayoutDescriptorAppendIfFastOrUseFull(
        isolate, kPropsCount, descriptors, kBitsInSmiLayout + 1);
    CHECK(layout_descriptor->IsSlowLayout());
  }
}


TEST(Regress436816) {
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(CcTest::isolate());

  // Force a GC to free up space before we allocate objects whose
  // mid-test states would fail heap verification.
  CcTest::CollectAllGarbage();

  const int kPropsCount = kBitsInSmiLayout * 3;
  TestPropertyKind props[kPropsCount];
  for (int i = 0; i < kPropsCount; i++) {
    props[i] = PROP_DOUBLE;
  }
  Handle<DescriptorArray> descriptors =
      CreateDescriptorArray(isolate, props, kPropsCount);

  Handle<Map> map = Map::Create(isolate, kPropsCount);
  Handle<LayoutDescriptor> layout_descriptor =
      LayoutDescriptor::New(isolate, map, descriptors, kPropsCount);
  map->InitializeDescriptors(isolate, *descriptors, *layout_descriptor);

  Handle<JSObject> object =
      factory->NewJSObjectFromMap(map, AllocationType::kOld);

  Address fake_address = static_cast<Address>(~kHeapObjectTagMask);
  HeapObject fake_object = HeapObject::FromAddress(fake_address);
  CHECK(fake_object.IsHeapObject());

  uint64_t boom_value = bit_cast<uint64_t>(fake_object);
  for (int i = 0; i < kPropsCount; i++) {
    FieldIndex index = FieldIndex::ForDescriptor(*map, i);
    CHECK(map->IsUnboxedDoubleField(index));
    object->RawFastDoublePropertyAsBitsAtPut(index, boom_value);
  }
  CHECK(object->HasFastProperties());
  CHECK(!object->map().HasFastPointerLayout());

  Handle<Map> normalized_map =
      Map::Normalize(isolate, map, KEEP_INOBJECT_PROPERTIES, "testing");
  JSObject::MigrateToMap(object, normalized_map);
  CHECK(!object->HasFastProperties());
  CHECK(object->map().HasFastPointerLayout());

  // Trigger GCs and heap verification.
  CcTest::CollectAllGarbage();
}


TEST(DescriptorArrayTrimming) {
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();

  const int kFieldCount = 128;
  const int kSplitFieldIndex = 32;
  const int kTrimmedLayoutDescriptorLength = 64;

  Handle<FieldType> any_type = FieldType::Any(isolate);
  Handle<Map> map = Map::Create(isolate, kFieldCount);
  for (int i = 0; i < kSplitFieldIndex; i++) {
    map = Map::CopyWithField(isolate, map, MakeName("prop", i), any_type, NONE,
                             PropertyConstness::kMutable, Representation::Smi(),
                             INSERT_TRANSITION)
              .ToHandleChecked();
  }
  map = Map::CopyWithField(isolate, map, MakeName("dbl", kSplitFieldIndex),
                           any_type, NONE, PropertyConstness::kMutable,
                           Representation::Double(), INSERT_TRANSITION)
            .ToHandleChecked();
  CHECK(map->layout_descriptor().IsConsistentWithMap(*map, true));
  CHECK(map->layout_descriptor().IsSlowLayout());
  CHECK(map->owns_descriptors());
  CHECK_EQ(8, map->layout_descriptor().length());

  {
    // Add transitions to double fields.
    v8::HandleScope scope(CcTest::isolate());

    Handle<Map> tmp_map = map;
    for (int i = kSplitFieldIndex + 1; i < kFieldCount; i++) {
      tmp_map = Map::CopyWithField(isolate, tmp_map, MakeName("dbl", i),
                                   any_type, NONE, PropertyConstness::kMutable,
                                   Representation::Double(), INSERT_TRANSITION)
                    .ToHandleChecked();
      CHECK(tmp_map->layout_descriptor().IsConsistentWithMap(*tmp_map, true));
    }
    // Check that descriptors are shared.
    CHECK(tmp_map->owns_descriptors());
    CHECK_EQ(map->instance_descriptors(), tmp_map->instance_descriptors());
    CHECK_EQ(map->layout_descriptor(), tmp_map->layout_descriptor());
  }
  CHECK(map->layout_descriptor().IsSlowLayout());
  CHECK_EQ(16, map->layout_descriptor().length());

  // The unused tail of the layout descriptor is now "durty" because of sharing.
  CHECK(map->layout_descriptor().IsConsistentWithMap(*map));
  for (int i = kSplitFieldIndex + 1; i < kTrimmedLayoutDescriptorLength; i++) {
    CHECK(!map->layout_descriptor().IsTagged(i));
  }
  CHECK_LT(map->NumberOfOwnDescriptors(),
           map->instance_descriptors().number_of_descriptors());

  // Call GC that should trim both |map|'s descriptor array and layout
  // descriptor.
  CcTest::CollectAllGarbage();

  // The unused tail of the layout descriptor is now "clean" again.
  CHECK(map->layout_descriptor().IsConsistentWithMap(*map, true));
  CHECK(map->owns_descriptors());
  CHECK_EQ(map->NumberOfOwnDescriptors(),
           map->instance_descriptors().number_of_descriptors());
  CHECK(map->layout_descriptor().IsSlowLayout());
  CHECK_EQ(8, map->layout_descriptor().length());

  {
    // Add transitions to tagged fields.
    v8::HandleScope scope(CcTest::isolate());

    Handle<Map> tmp_map = map;
    for (int i = kSplitFieldIndex + 1; i < kFieldCount - 1; i++) {
      tmp_map = Map::CopyWithField(isolate, tmp_map, MakeName("tagged", i),
                                   any_type, NONE, PropertyConstness::kMutable,
                                   Representation::Tagged(), INSERT_TRANSITION)
                    .ToHandleChecked();
      CHECK(tmp_map->layout_descriptor().IsConsistentWithMap(*tmp_map, true));
    }
    tmp_map = Map::CopyWithField(isolate, tmp_map, MakeString("dbl"), any_type,
                                 NONE, PropertyConstness::kMutable,
                                 Representation::Double(), INSERT_TRANSITION)
                  .ToHandleChecked();
    CHECK(tmp_map->layout_descriptor().IsConsistentWithMap(*tmp_map, true));
    // Check that descriptors are shared.
    CHECK(tmp_map->owns_descriptors());
    CHECK_EQ(map->instance_descriptors(), tmp_map->instance_descriptors());
  }
  CHECK(map->layout_descriptor().IsSlowLayout());
}


TEST(DoScavenge) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  // The plan: create |obj| with double field in new space, do scanvenge so
  // that |obj| is moved to old space, construct a double value that looks like
  // a pointer to "from space" pointer. Do scavenge one more time and ensure
  // that it didn't crash or corrupt the double value stored in the object.

  Handle<FieldType> any_type = FieldType::Any(isolate);
  Handle<Map> map = Map::Create(isolate, 10);
  map = Map::CopyWithField(isolate, map, MakeName("prop", 0), any_type, NONE,
                           PropertyConstness::kMutable,
                           Representation::Double(), INSERT_TRANSITION)
            .ToHandleChecked();

  // Create object in new space.
  Handle<JSObject> obj =
      factory->NewJSObjectFromMap(map, AllocationType::kYoung);

  Handle<HeapNumber> heap_number = factory->NewHeapNumber(42.5);
  WriteToField(*obj, 0, *heap_number);

  {
    // Ensure the object is properly set up.
    FieldIndex field_index = FieldIndex::ForDescriptor(*map, 0);
    CHECK(field_index.is_inobject() && field_index.is_double());
    CHECK_EQ(FLAG_unbox_double_fields, map->IsUnboxedDoubleField(field_index));
    CHECK_EQ(42.5, GetDoubleFieldValue(*obj, field_index));
  }
  CHECK(isolate->heap()->new_space()->Contains(*obj));

  // Do scavenge so that |obj| is moved to survivor space.
  CcTest::CollectGarbage(i::NEW_SPACE);

  // Create temp object in the new space.
  Handle<JSArray> temp = factory->NewJSArray(0, PACKED_ELEMENTS);
  CHECK(isolate->heap()->new_space()->Contains(*temp));

  // Construct a double value that looks like a pointer to the new space object
  // and store it into the obj.
  Address fake_object = temp->ptr() + kSystemPointerSize;
  double boom_value = bit_cast<double>(fake_object);

  FieldIndex field_index = FieldIndex::ForDescriptor(obj->map(), 0);
  auto boom_number = factory->NewMutableHeapNumber(boom_value);
  obj->FastPropertyAtPut(field_index, *boom_number);

  // Now |obj| moves to old gen and it has a double field that looks like
  // a pointer to a from semi-space.
  CcTest::CollectGarbage(i::NEW_SPACE);

  CHECK(isolate->heap()->old_space()->Contains(*obj));

  CHECK_EQ(boom_value, GetDoubleFieldValue(*obj, field_index));
}


TEST(DoScavengeWithIncrementalWriteBarrier) {
  if (FLAG_never_compact || !FLAG_incremental_marking) return;
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = CcTest::heap();
  PagedSpace* old_space = heap->old_space();

  // The plan: create |obj_value| in old space and ensure that it is allocated
  // on evacuation candidate page, create |obj| with double and tagged fields
  // in new space and write |obj_value| to tagged field of |obj|, do two
  // scavenges to promote |obj| to old space, a GC in old space and ensure that
  // the tagged value was properly updated after candidates evacuation.

  Handle<FieldType> any_type = FieldType::Any(isolate);
  Handle<Map> map = Map::Create(isolate, 10);
  map = Map::CopyWithField(isolate, map, MakeName("prop", 0), any_type, NONE,
                           PropertyConstness::kMutable,
                           Representation::Double(), INSERT_TRANSITION)
            .ToHandleChecked();
  map = Map::CopyWithField(isolate, map, MakeName("prop", 1), any_type, NONE,
                           PropertyConstness::kMutable,
                           Representation::Tagged(), INSERT_TRANSITION)
            .ToHandleChecked();

  // Create |obj_value| in old space.
  Handle<HeapObject> obj_value;
  Page* ec_page;
  {
    AlwaysAllocateScope always_allocate(isolate);
    // Make sure |obj_value| is placed on an old-space evacuation candidate.
    heap::SimulateFullSpace(old_space);
    obj_value =
        factory->NewJSArray(32 * KB, HOLEY_ELEMENTS, AllocationType::kOld);
    ec_page = Page::FromHeapObject(*obj_value);
  }

  // Create object in new space.
  Handle<JSObject> obj =
      factory->NewJSObjectFromMap(map, AllocationType::kYoung);

  Handle<HeapNumber> heap_number = factory->NewHeapNumber(42.5);
  WriteToField(*obj, 0, *heap_number);
  WriteToField(*obj, 1, *obj_value);

  {
    // Ensure the object is properly set up.
    FieldIndex field_index = FieldIndex::ForDescriptor(*map, 0);
    CHECK(field_index.is_inobject() && field_index.is_double());
    CHECK_EQ(FLAG_unbox_double_fields, map->IsUnboxedDoubleField(field_index));
    CHECK_EQ(42.5, GetDoubleFieldValue(*obj, field_index));

    field_index = FieldIndex::ForDescriptor(*map, 1);
    CHECK(field_index.is_inobject() && !field_index.is_double());
    CHECK(!map->IsUnboxedDoubleField(field_index));
  }
  CHECK(isolate->heap()->new_space()->Contains(*obj));

  // Heap is ready, force |ec_page| to become an evacuation candidate and
  // simulate incremental marking.
  FLAG_stress_compaction = true;
  FLAG_manual_evacuation_candidates_selection = true;
  heap::ForceEvacuationCandidate(ec_page);
  heap::SimulateIncrementalMarking(heap);
  // Disable stress compaction mode in order to let GC do scavenge.
  FLAG_stress_compaction = false;

  // Check that everything is ready for triggering incremental write barrier
  // during scavenge (i.e. that |obj| is black and incremental marking is
  // in compacting mode and |obj_value|'s page is an evacuation candidate).
  IncrementalMarking* marking = heap->incremental_marking();
  CHECK(marking->IsCompacting());
  IncrementalMarking::MarkingState* marking_state =
      heap->incremental_marking()->marking_state();
  CHECK(marking_state->IsBlack(*obj));
  CHECK(MarkCompactCollector::IsOnEvacuationCandidate(*obj_value));

  // Trigger GCs so that |obj| moves to old gen.
  CcTest::CollectGarbage(i::NEW_SPACE);  // in survivor space now
  CcTest::CollectGarbage(i::NEW_SPACE);  // in old gen now

  CHECK(isolate->heap()->old_space()->Contains(*obj));
  CHECK(isolate->heap()->old_space()->Contains(*obj_value));
  CHECK(MarkCompactCollector::IsOnEvacuationCandidate(*obj_value));

  CcTest::CollectGarbage(i::OLD_SPACE);

  // |obj_value| must be evacuated.
  CHECK(!MarkCompactCollector::IsOnEvacuationCandidate(*obj_value));

  FieldIndex field_index = FieldIndex::ForDescriptor(*map, 1);
  CHECK_EQ(*obj_value, obj->RawFastPropertyAt(field_index));
}


static void TestLayoutDescriptorHelper(Isolate* isolate,
                                       int inobject_properties,
                                       Handle<DescriptorArray> descriptors,
                                       int number_of_descriptors) {
  Handle<Map> map = Map::Create(isolate, inobject_properties);

  Handle<LayoutDescriptor> layout_descriptor = LayoutDescriptor::New(
      isolate, map, descriptors, descriptors->number_of_descriptors());
  InitializeVerifiedMapDescriptors(isolate, *map, *descriptors,
                                   *layout_descriptor);

  LayoutDescriptorHelper helper(*map);
  bool all_fields_tagged = true;

  int instance_size = map->instance_size();

  int end_offset = instance_size * 2;
  int first_non_tagged_field_offset = end_offset;
  for (int i = 0; i < number_of_descriptors; i++) {
    PropertyDetails details = descriptors->GetDetails(i);
    if (details.location() != kField) continue;
    FieldIndex index = FieldIndex::ForDescriptor(*map, i);
    if (!index.is_inobject()) continue;
    all_fields_tagged &= !details.representation().IsDouble();
    bool expected_tagged = !index.is_double();
    if (!expected_tagged) {
      first_non_tagged_field_offset =
          Min(first_non_tagged_field_offset, index.offset());
    }

    int end_of_region_offset;
    CHECK_EQ(expected_tagged, helper.IsTagged(index.offset()));
    CHECK_EQ(expected_tagged, helper.IsTagged(index.offset(), instance_size,
                                              &end_of_region_offset));
    CHECK_GT(end_of_region_offset, 0);
    CHECK_EQ(end_of_region_offset % kTaggedSize, 0);
    CHECK(end_of_region_offset <= instance_size);

    for (int offset = index.offset(); offset < end_of_region_offset;
         offset += kTaggedSize) {
      CHECK_EQ(expected_tagged, helper.IsTagged(index.offset()));
    }
    if (end_of_region_offset < instance_size) {
      CHECK_EQ(!expected_tagged, helper.IsTagged(end_of_region_offset));
    } else {
      CHECK(helper.IsTagged(end_of_region_offset));
    }
  }

  for (int offset = 0; offset < JSObject::kHeaderSize; offset += kTaggedSize) {
    // Header queries
    CHECK(helper.IsTagged(offset));
    int end_of_region_offset;
    CHECK(helper.IsTagged(offset, end_offset, &end_of_region_offset));
    CHECK_EQ(first_non_tagged_field_offset, end_of_region_offset);

    // Out of bounds queries
    CHECK(helper.IsTagged(offset + instance_size));
  }

  CHECK_EQ(all_fields_tagged, helper.all_fields_tagged());
}


TEST(LayoutDescriptorHelperMixed) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());

  Handle<LayoutDescriptor> layout_descriptor;
  const int kPropsCount = kBitsInSmiLayout * 3;
  TestPropertyKind props[kPropsCount];
  for (int i = 0; i < kPropsCount; i++) {
    props[i] = static_cast<TestPropertyKind>(i % PROP_KIND_NUMBER);
  }
  Handle<DescriptorArray> descriptors =
      CreateDescriptorArray(isolate, props, kPropsCount);

  TestLayoutDescriptorHelper(isolate, 0, descriptors, kPropsCount);

  TestLayoutDescriptorHelper(isolate, 13, descriptors, kPropsCount);

  TestLayoutDescriptorHelper(isolate, kBitsInSmiLayout, descriptors,
                             kPropsCount);

  TestLayoutDescriptorHelper(isolate, kBitsInSmiLayout * 2, descriptors,
                             kPropsCount);

  TestLayoutDescriptorHelper(isolate, kPropsCount, descriptors, kPropsCount);
}


TEST(LayoutDescriptorHelperAllTagged) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());

  Handle<LayoutDescriptor> layout_descriptor;
  const int kPropsCount = kBitsInSmiLayout * 3;
  TestPropertyKind props[kPropsCount];
  for (int i = 0; i < kPropsCount; i++) {
    props[i] = PROP_TAGGED;
  }
  Handle<DescriptorArray> descriptors =
      CreateDescriptorArray(isolate, props, kPropsCount);

  TestLayoutDescriptorHelper(isolate, 0, descriptors, kPropsCount);

  TestLayoutDescriptorHelper(isolate, 13, descriptors, kPropsCount);

  TestLayoutDescriptorHelper(isolate, kBitsInSmiLayout, descriptors,
                             kPropsCount);

  TestLayoutDescriptorHelper(isolate, kBitsInSmiLayout * 2, descriptors,
                             kPropsCount);

  TestLayoutDescriptorHelper(isolate, kPropsCount, descriptors, kPropsCount);
}


TEST(LayoutDescriptorHelperAllDoubles) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());

  Handle<LayoutDescriptor> layout_descriptor;
  const int kPropsCount = kBitsInSmiLayout * 3;
  TestPropertyKind props[kPropsCount];
  for (int i = 0; i < kPropsCount; i++) {
    props[i] = PROP_DOUBLE;
  }
  Handle<DescriptorArray> descriptors =
      CreateDescriptorArray(isolate, props, kPropsCount);

  TestLayoutDescriptorHelper(isolate, 0, descriptors, kPropsCount);

  TestLayoutDescriptorHelper(isolate, 13, descriptors, kPropsCount);

  TestLayoutDescriptorHelper(isolate, kBitsInSmiLayout, descriptors,
                             kPropsCount);

  TestLayoutDescriptorHelper(isolate, kBitsInSmiLayout * 2, descriptors,
                             kPropsCount);

  TestLayoutDescriptorHelper(isolate, kPropsCount, descriptors, kPropsCount);
}


TEST(LayoutDescriptorSharing) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<FieldType> any_type = FieldType::Any(isolate);

  Handle<Map> split_map;
  {
    Handle<Map> map = Map::Create(isolate, 64);
    for (int i = 0; i < 32; i++) {
      Handle<String> name = MakeName("prop", i);
      map = Map::CopyWithField(isolate, map, name, any_type, NONE,
                               PropertyConstness::kMutable,
                               Representation::Smi(), INSERT_TRANSITION)
                .ToHandleChecked();
    }
    split_map = Map::CopyWithField(isolate, map, MakeString("dbl"), any_type,
                                   NONE, PropertyConstness::kMutable,
                                   Representation::Double(), INSERT_TRANSITION)
                    .ToHandleChecked();
  }
  Handle<LayoutDescriptor> split_layout_descriptor(
      split_map->layout_descriptor(), isolate);
  CHECK(split_layout_descriptor->IsConsistentWithMap(*split_map, true));
  CHECK(split_layout_descriptor->IsSlowLayout());
  CHECK(split_map->owns_descriptors());

  Handle<Map> map1 =
      Map::CopyWithField(isolate, split_map, MakeString("foo"), any_type, NONE,
                         PropertyConstness::kMutable, Representation::Double(),
                         INSERT_TRANSITION)
          .ToHandleChecked();
  CHECK(!split_map->owns_descriptors());
  CHECK_EQ(*split_layout_descriptor, split_map->layout_descriptor());

  // Layout descriptors should be shared with |split_map|.
  CHECK(map1->owns_descriptors());
  CHECK_EQ(*split_layout_descriptor, map1->layout_descriptor());
  CHECK(map1->layout_descriptor().IsConsistentWithMap(*map1, true));

  Handle<Map> map2 =
      Map::CopyWithField(isolate, split_map, MakeString("bar"), any_type, NONE,
                         PropertyConstness::kMutable, Representation::Tagged(),
                         INSERT_TRANSITION)
          .ToHandleChecked();

  // Layout descriptors should not be shared with |split_map|.
  CHECK(map2->owns_descriptors());
  CHECK_NE(*split_layout_descriptor, map2->layout_descriptor());
  CHECK(map2->layout_descriptor().IsConsistentWithMap(*map2, true));
}


static void TestWriteBarrier(Handle<Map> map, Handle<Map> new_map,
                             int tagged_descriptor, int double_descriptor,
                             bool check_tagged_value = true) {
  FLAG_stress_compaction = true;
  FLAG_manual_evacuation_candidates_selection = true;
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = CcTest::heap();
  PagedSpace* old_space = heap->old_space();

  // The plan: create |obj| by |map| in old space, create |obj_value| in
  // new space and ensure that write barrier is triggered when |obj_value| is
  // written to property |tagged_descriptor| of |obj|.
  // Then migrate object to |new_map| and set proper value for property
  // |double_descriptor|. Call GC and ensure that it did not crash during
  // store buffer entries updating.

  Handle<JSObject> obj;
  Handle<HeapObject> obj_value;
  {
    AlwaysAllocateScope always_allocate(isolate);
    obj = factory->NewJSObjectFromMap(map, AllocationType::kOld);
    CHECK(old_space->Contains(*obj));

    obj_value = factory->NewHeapNumber(0.);
  }

  CHECK(Heap::InYoungGeneration(*obj_value));

  {
    FieldIndex index = FieldIndex::ForDescriptor(*map, tagged_descriptor);
    const int n = 153;
    for (int i = 0; i < n; i++) {
      obj->FastPropertyAtPut(index, *obj_value);
    }
  }

  // Migrate |obj| to |new_map| which should shift fields and put the
  // |boom_value| to the slot that was earlier recorded by write barrier.
  JSObject::MigrateToMap(obj, new_map);

  Address fake_object = obj_value->ptr() + kTaggedSize;
  uint64_t boom_value = bit_cast<uint64_t>(fake_object);

  FieldIndex double_field_index =
      FieldIndex::ForDescriptor(*new_map, double_descriptor);
  CHECK(obj->IsUnboxedDoubleField(double_field_index));
  obj->RawFastDoublePropertyAsBitsAtPut(double_field_index, boom_value);

  // Trigger GC to evacuate all candidates.
  CcTest::CollectGarbage(NEW_SPACE);

  if (check_tagged_value) {
    FieldIndex tagged_field_index =
        FieldIndex::ForDescriptor(*new_map, tagged_descriptor);
    CHECK_EQ(*obj_value, obj->RawFastPropertyAt(tagged_field_index));
  }
  CHECK_EQ(boom_value, obj->RawFastDoublePropertyAsBitsAt(double_field_index));
}


static void TestIncrementalWriteBarrier(Handle<Map> map, Handle<Map> new_map,
                                        int tagged_descriptor,
                                        int double_descriptor,
                                        bool check_tagged_value = true) {
  if (FLAG_never_compact || !FLAG_incremental_marking) return;
  ManualGCScope manual_gc_scope;
  FLAG_manual_evacuation_candidates_selection = true;
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = CcTest::heap();
  PagedSpace* old_space = heap->old_space();

  // The plan: create |obj| by |map| in old space, create |obj_value| in
  // old space and ensure it end up in evacuation candidate page. Start
  // incremental marking and ensure that incremental write barrier is triggered
  // when |obj_value| is written to property |tagged_descriptor| of |obj|.
  // Then migrate object to |new_map| and set proper value for property
  // |double_descriptor|. Call GC and ensure that it did not crash during
  // slots buffer entries updating.

  Handle<JSObject> obj;
  Handle<HeapObject> obj_value;
  Page* ec_page;
  {
    AlwaysAllocateScope always_allocate(isolate);
    obj = factory->NewJSObjectFromMap(map, AllocationType::kOld);
    CHECK(old_space->Contains(*obj));

    // Make sure |obj_value| is placed on an old-space evacuation candidate.
    heap::SimulateFullSpace(old_space);
    obj_value =
        factory->NewJSArray(32 * KB, HOLEY_ELEMENTS, AllocationType::kOld);
    ec_page = Page::FromHeapObject(*obj_value);
    CHECK_NE(ec_page, Page::FromHeapObject(*obj));
  }

  // Heap is ready, force |ec_page| to become an evacuation candidate and
  // simulate incremental marking.
  heap::ForceEvacuationCandidate(ec_page);
  heap::SimulateIncrementalMarking(heap);

  // Check that everything is ready for triggering incremental write barrier
  // (i.e. that both |obj| and |obj_value| are black and the marking phase is
  // still active and |obj_value|'s page is indeed an evacuation candidate).
  IncrementalMarking* marking = heap->incremental_marking();
  CHECK(marking->IsMarking());
  IncrementalMarking::MarkingState* marking_state = marking->marking_state();
  CHECK(marking_state->IsBlack(*obj));
  CHECK(marking_state->IsBlack(*obj_value));
  CHECK(MarkCompactCollector::IsOnEvacuationCandidate(*obj_value));

  // Trigger incremental write barrier, which should add a slot to remembered
  // set.
  {
    FieldIndex index = FieldIndex::ForDescriptor(*map, tagged_descriptor);
    obj->FastPropertyAtPut(index, *obj_value);
  }

  // Migrate |obj| to |new_map| which should shift fields and put the
  // |boom_value| to the slot that was earlier recorded by incremental write
  // barrier.
  JSObject::MigrateToMap(obj, new_map);

  uint64_t boom_value = UINT64_C(0xBAAD0176A37C28E1);

  FieldIndex double_field_index =
      FieldIndex::ForDescriptor(*new_map, double_descriptor);
  CHECK(obj->IsUnboxedDoubleField(double_field_index));
  obj->RawFastDoublePropertyAsBitsAtPut(double_field_index, boom_value);

  // Trigger GC to evacuate all candidates.
  CcTest::CollectGarbage(OLD_SPACE);

  // Ensure that the values are still there and correct.
  CHECK(!MarkCompactCollector::IsOnEvacuationCandidate(*obj_value));

  if (check_tagged_value) {
    FieldIndex tagged_field_index =
        FieldIndex::ForDescriptor(*new_map, tagged_descriptor);
    CHECK_EQ(*obj_value, obj->RawFastPropertyAt(tagged_field_index));
  }
  CHECK_EQ(boom_value, obj->RawFastDoublePropertyAsBitsAt(double_field_index));
}

enum OldToWriteBarrierKind {
  OLD_TO_OLD_WRITE_BARRIER,
  OLD_TO_NEW_WRITE_BARRIER
};
static void TestWriteBarrierObjectShiftFieldsRight(
    OldToWriteBarrierKind write_barrier_kind) {
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());

  Handle<FieldType> any_type = FieldType::Any(isolate);

  CompileRun("function func() { return 1; }");

  Handle<JSObject> func = GetObject("func");

  Handle<Map> map = Map::Create(isolate, 10);
  map = Map::CopyWithConstant(isolate, map, MakeName("prop", 0), func, NONE,
                              INSERT_TRANSITION)
            .ToHandleChecked();
  map = Map::CopyWithField(isolate, map, MakeName("prop", 1), any_type, NONE,
                           PropertyConstness::kMutable,
                           Representation::Double(), INSERT_TRANSITION)
            .ToHandleChecked();
  map = Map::CopyWithField(isolate, map, MakeName("prop", 2), any_type, NONE,
                           PropertyConstness::kMutable,
                           Representation::Tagged(), INSERT_TRANSITION)
            .ToHandleChecked();

  // Shift fields right by turning constant property to a field.
  Handle<Map> new_map = Map::ReconfigureProperty(
      isolate, map, 0, kData, NONE, Representation::Tagged(), any_type);

  if (write_barrier_kind == OLD_TO_NEW_WRITE_BARRIER) {
    TestWriteBarrier(map, new_map, 2, 1);
  } else {
    CHECK_EQ(OLD_TO_OLD_WRITE_BARRIER, write_barrier_kind);
    TestIncrementalWriteBarrier(map, new_map, 2, 1);
  }
}

TEST(WriteBarrierObjectShiftFieldsRight) {
  TestWriteBarrierObjectShiftFieldsRight(OLD_TO_NEW_WRITE_BARRIER);
}


TEST(IncrementalWriteBarrierObjectShiftFieldsRight) {
  TestWriteBarrierObjectShiftFieldsRight(OLD_TO_OLD_WRITE_BARRIER);
}


// TODO(ishell): add respective tests for property kind reconfiguring from
// accessor field to double, once accessor fields are supported by
// Map::ReconfigureProperty().


// TODO(ishell): add respective tests for fast property removal case once
// Map::ReconfigureProperty() supports that.

#endif

}  // namespace test_unboxed_doubles
}  // namespace internal
}  // namespace v8
