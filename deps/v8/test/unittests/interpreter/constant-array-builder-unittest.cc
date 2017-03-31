// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/factory.h"
#include "src/handles-inl.h"
#include "src/interpreter/constant-array-builder.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

class ConstantArrayBuilderTest : public TestWithIsolateAndZone {
 public:
  ConstantArrayBuilderTest() {}
  ~ConstantArrayBuilderTest() override {}

  static const size_t k8BitCapacity = ConstantArrayBuilder::k8BitCapacity;
  static const size_t k16BitCapacity = ConstantArrayBuilder::k16BitCapacity;
};

STATIC_CONST_MEMBER_DEFINITION const size_t
    ConstantArrayBuilderTest::k16BitCapacity;
STATIC_CONST_MEMBER_DEFINITION const size_t
    ConstantArrayBuilderTest::k8BitCapacity;

TEST_F(ConstantArrayBuilderTest, AllocateAllEntries) {
  CanonicalHandleScope canonical(isolate());
  ConstantArrayBuilder builder(zone(), isolate()->factory()->the_hole_value());
  for (size_t i = 0; i < k16BitCapacity; i++) {
    builder.Insert(handle(Smi::FromInt(static_cast<int>(i)), isolate()));
  }
  CHECK_EQ(builder.size(), k16BitCapacity);
  for (size_t i = 0; i < k16BitCapacity; i++) {
    CHECK_EQ(Handle<Smi>::cast(builder.At(i))->value(), static_cast<int>(i));
  }
}

TEST_F(ConstantArrayBuilderTest, ToFixedArray) {
  CanonicalHandleScope canonical(isolate());
  ConstantArrayBuilder builder(zone(), isolate()->factory()->the_hole_value());
  static const size_t kNumberOfElements = 37;
  for (size_t i = 0; i < kNumberOfElements; i++) {
    Handle<Object> object = isolate()->factory()->NewNumberFromSize(i);
    builder.Insert(object);
    CHECK(builder.At(i)->SameValue(*object));
  }
  Handle<FixedArray> constant_array = builder.ToFixedArray(isolate());
  CHECK_EQ(constant_array->length(), static_cast<int>(kNumberOfElements));
  for (size_t i = 0; i < kNumberOfElements; i++) {
    CHECK(constant_array->get(static_cast<int>(i))->SameValue(*builder.At(i)));
  }
}

TEST_F(ConstantArrayBuilderTest, ToLargeFixedArray) {
  CanonicalHandleScope canonical(isolate());
  ConstantArrayBuilder builder(zone(), isolate()->factory()->the_hole_value());
  static const size_t kNumberOfElements = 37373;
  for (size_t i = 0; i < kNumberOfElements; i++) {
    Handle<Object> object = isolate()->factory()->NewNumberFromSize(i);
    builder.Insert(object);
    CHECK(builder.At(i)->SameValue(*object));
  }
  Handle<FixedArray> constant_array = builder.ToFixedArray(isolate());
  CHECK_EQ(constant_array->length(), static_cast<int>(kNumberOfElements));
  for (size_t i = 0; i < kNumberOfElements; i++) {
    CHECK(constant_array->get(static_cast<int>(i))->SameValue(*builder.At(i)));
  }
}

TEST_F(ConstantArrayBuilderTest, ToLargeFixedArrayWithReservations) {
  CanonicalHandleScope canonical(isolate());
  ConstantArrayBuilder builder(zone(), isolate()->factory()->the_hole_value());
  static const size_t kNumberOfElements = 37373;
  for (size_t i = 0; i < kNumberOfElements; i++) {
    builder.CommitReservedEntry(builder.CreateReservedEntry(),
                                Smi::FromInt(static_cast<int>(i)));
  }
  Handle<FixedArray> constant_array = builder.ToFixedArray(isolate());
  CHECK_EQ(constant_array->length(), static_cast<int>(kNumberOfElements));
  for (size_t i = 0; i < kNumberOfElements; i++) {
    CHECK(constant_array->get(static_cast<int>(i))->SameValue(*builder.At(i)));
  }
}

TEST_F(ConstantArrayBuilderTest, AllocateEntriesWithIdx8Reservations) {
  CanonicalHandleScope canonical(isolate());
  for (size_t reserved = 1; reserved < k8BitCapacity; reserved *= 3) {
    ConstantArrayBuilder builder(zone(),
                                 isolate()->factory()->the_hole_value());
    for (size_t i = 0; i < reserved; i++) {
      OperandSize operand_size = builder.CreateReservedEntry();
      CHECK(operand_size == OperandSize::kByte);
    }
    for (size_t i = 0; i < 2 * k8BitCapacity; i++) {
      builder.CommitReservedEntry(builder.CreateReservedEntry(),
                                  Smi::FromInt(static_cast<int>(i)));
      if (i + reserved < k8BitCapacity) {
        CHECK_LE(builder.size(), k8BitCapacity);
        CHECK_EQ(builder.size(), i + 1);
      } else {
        CHECK_GE(builder.size(), k8BitCapacity);
        CHECK_EQ(builder.size(), i + reserved + 1);
      }
    }
    CHECK_EQ(builder.size(), 2 * k8BitCapacity + reserved);

    // Check reserved values represented by the hole.
    for (size_t i = 0; i < reserved; i++) {
      Handle<Object> empty = builder.At(k8BitCapacity - reserved + i);
      CHECK(empty->SameValue(isolate()->heap()->the_hole_value()));
    }

    // Commit reserved entries with duplicates and check size does not change.
    DCHECK_EQ(reserved + 2 * k8BitCapacity, builder.size());
    size_t duplicates_in_idx8_space =
        std::min(reserved, k8BitCapacity - reserved);
    for (size_t i = 0; i < duplicates_in_idx8_space; i++) {
      builder.CommitReservedEntry(OperandSize::kByte,
                                  Smi::FromInt(static_cast<int>(i)));
      DCHECK_EQ(reserved + 2 * k8BitCapacity, builder.size());
    }

    // Now make reservations, and commit them with unique entries.
    for (size_t i = 0; i < duplicates_in_idx8_space; i++) {
      OperandSize operand_size = builder.CreateReservedEntry();
      CHECK(operand_size == OperandSize::kByte);
    }
    for (size_t i = 0; i < duplicates_in_idx8_space; i++) {
      Smi* value = Smi::FromInt(static_cast<int>(2 * k8BitCapacity + i));
      size_t index = builder.CommitReservedEntry(OperandSize::kByte, value);
      CHECK_EQ(index, k8BitCapacity - reserved + i);
    }

    Handle<FixedArray> constant_array = builder.ToFixedArray(isolate());
    CHECK_EQ(constant_array->length(),
             static_cast<int>(2 * k8BitCapacity + reserved));

    // Check all committed values match expected
    for (size_t i = 0; i < k8BitCapacity - reserved; i++) {
      Object* value = constant_array->get(static_cast<int>(i));
      Smi* smi = Smi::FromInt(static_cast<int>(i));
      CHECK(value->SameValue(smi));
    }
    for (size_t i = k8BitCapacity; i < 2 * k8BitCapacity + reserved; i++) {
      Object* value = constant_array->get(static_cast<int>(i));
      Smi* smi = Smi::FromInt(static_cast<int>(i - reserved));
      CHECK(value->SameValue(smi));
    }
  }
}

TEST_F(ConstantArrayBuilderTest, AllocateEntriesWithWideReservations) {
  CanonicalHandleScope canonical(isolate());
  for (size_t reserved = 1; reserved < k8BitCapacity; reserved *= 3) {
    ConstantArrayBuilder builder(zone(),
                                 isolate()->factory()->the_hole_value());
    for (size_t i = 0; i < k8BitCapacity; i++) {
      builder.CommitReservedEntry(builder.CreateReservedEntry(),
                                  Smi::FromInt(static_cast<int>(i)));
      CHECK_EQ(builder.size(), i + 1);
    }
    for (size_t i = 0; i < reserved; i++) {
      OperandSize operand_size = builder.CreateReservedEntry();
      CHECK(operand_size == OperandSize::kShort);
      CHECK_EQ(builder.size(), k8BitCapacity);
    }
    for (size_t i = 0; i < reserved; i++) {
      builder.DiscardReservedEntry(OperandSize::kShort);
      CHECK_EQ(builder.size(), k8BitCapacity);
    }
    for (size_t i = 0; i < reserved; i++) {
      OperandSize operand_size = builder.CreateReservedEntry();
      CHECK(operand_size == OperandSize::kShort);
      builder.CommitReservedEntry(operand_size,
                                  Smi::FromInt(static_cast<int>(i)));
      CHECK_EQ(builder.size(), k8BitCapacity);
    }
    for (size_t i = k8BitCapacity; i < k8BitCapacity + reserved; i++) {
      OperandSize operand_size = builder.CreateReservedEntry();
      CHECK(operand_size == OperandSize::kShort);
      builder.CommitReservedEntry(operand_size,
                                  Smi::FromInt(static_cast<int>(i)));
      CHECK_EQ(builder.size(), i + 1);
    }

    Handle<FixedArray> constant_array = builder.ToFixedArray(isolate());
    CHECK_EQ(constant_array->length(),
             static_cast<int>(k8BitCapacity + reserved));
    for (size_t i = 0; i < k8BitCapacity + reserved; i++) {
      Object* value = constant_array->get(static_cast<int>(i));
      CHECK(value->SameValue(*isolate()->factory()->NewNumberFromSize(i)));
    }
  }
}

TEST_F(ConstantArrayBuilderTest, GapFilledWhenLowReservationCommitted) {
  CanonicalHandleScope canonical(isolate());
  ConstantArrayBuilder builder(zone(), isolate()->factory()->the_hole_value());
  for (size_t i = 0; i < k8BitCapacity; i++) {
    OperandSize operand_size = builder.CreateReservedEntry();
    CHECK(OperandSize::kByte == operand_size);
    CHECK_EQ(builder.size(), 0u);
  }
  for (size_t i = 0; i < k8BitCapacity; i++) {
    builder.CommitReservedEntry(builder.CreateReservedEntry(),
                                Smi::FromInt(static_cast<int>(i)));
    CHECK_EQ(builder.size(), i + k8BitCapacity + 1);
  }
  for (size_t i = 0; i < k8BitCapacity; i++) {
    builder.CommitReservedEntry(OperandSize::kByte,
                                Smi::FromInt(static_cast<int>(i)));
    CHECK_EQ(builder.size(), 2 * k8BitCapacity);
  }
  Handle<FixedArray> constant_array = builder.ToFixedArray(isolate());
  CHECK_EQ(constant_array->length(), static_cast<int>(2 * k8BitCapacity));
  for (size_t i = 0; i < k8BitCapacity; i++) {
    Object* original = constant_array->get(static_cast<int>(k8BitCapacity + i));
    Object* duplicate = constant_array->get(static_cast<int>(i));
    CHECK(original->SameValue(duplicate));
    Handle<Object> reference = isolate()->factory()->NewNumberFromSize(i);
    CHECK(original->SameValue(*reference));
  }
}

TEST_F(ConstantArrayBuilderTest, GapNotFilledWhenLowReservationDiscarded) {
  CanonicalHandleScope canonical(isolate());
  ConstantArrayBuilder builder(zone(), isolate()->factory()->the_hole_value());
  for (size_t i = 0; i < k8BitCapacity; i++) {
    OperandSize operand_size = builder.CreateReservedEntry();
    CHECK(OperandSize::kByte == operand_size);
    CHECK_EQ(builder.size(), 0u);
  }
  for (size_t i = 0; i < k8BitCapacity; i++) {
    Handle<Object> object = isolate()->factory()->NewNumberFromSize(i);
    builder.Insert(object);
    CHECK_EQ(builder.size(), i + k8BitCapacity + 1);
  }
  for (size_t i = 0; i < k8BitCapacity; i++) {
    builder.DiscardReservedEntry(OperandSize::kByte);
    builder.Insert(builder.At(i + k8BitCapacity));
    CHECK_EQ(builder.size(), 2 * k8BitCapacity);
  }
  for (size_t i = 0; i < k8BitCapacity; i++) {
    Handle<Object> reference = isolate()->factory()->NewNumberFromSize(i);
    Handle<Object> original = builder.At(k8BitCapacity + i);
    CHECK(original->SameValue(*reference));
    Handle<Object> duplicate = builder.At(i);
    CHECK(duplicate->SameValue(*isolate()->factory()->the_hole_value()));
  }
}

TEST_F(ConstantArrayBuilderTest, HolesWithUnusedReservations) {
  CanonicalHandleScope canonical(isolate());
  static int kNumberOfHoles = 128;
  ConstantArrayBuilder builder(zone(), isolate()->factory()->the_hole_value());
  for (int i = 0; i < kNumberOfHoles; ++i) {
    CHECK_EQ(builder.CreateReservedEntry(), OperandSize::kByte);
  }
  for (int i = 0; i < 128; ++i) {
    CHECK_EQ(builder.Insert(isolate()->factory()->NewNumber(i)),
             static_cast<size_t>(i));
  }
  CHECK_EQ(builder.Insert(isolate()->factory()->NewNumber(256)), 256u);

  Handle<FixedArray> constant_array = builder.ToFixedArray(isolate());
  CHECK_EQ(constant_array->length(), 257);
  for (int i = 128; i < 256; i++) {
    CHECK(constant_array->get(i)->SameValue(
        *isolate()->factory()->the_hole_value()));
  }
  CHECK(!constant_array->get(127)->SameValue(
      *isolate()->factory()->the_hole_value()));
  CHECK(!constant_array->get(256)->SameValue(
      *isolate()->factory()->the_hole_value()));
}

TEST_F(ConstantArrayBuilderTest, ReservationsAtAllScales) {
  CanonicalHandleScope canonical(isolate());
  ConstantArrayBuilder builder(zone(), isolate()->factory()->the_hole_value());
  for (int i = 0; i < 256; i++) {
    CHECK_EQ(builder.CreateReservedEntry(), OperandSize::kByte);
  }
  for (int i = 256; i < 65536; ++i) {
    CHECK_EQ(builder.CreateReservedEntry(), OperandSize::kShort);
  }
  for (int i = 65536; i < 131072; ++i) {
    CHECK_EQ(builder.CreateReservedEntry(), OperandSize::kQuad);
  }
  CHECK_EQ(builder.CommitReservedEntry(OperandSize::kByte, Smi::FromInt(1)),
           0u);
  CHECK_EQ(builder.CommitReservedEntry(OperandSize::kShort, Smi::FromInt(2)),
           256u);
  CHECK_EQ(builder.CommitReservedEntry(OperandSize::kQuad, Smi::FromInt(3)),
           65536u);
  Handle<FixedArray> constant_array = builder.ToFixedArray(isolate());
  CHECK_EQ(constant_array->length(), 65537);
  int count = 1;
  for (int i = 0; i < constant_array->length(); ++i) {
    Handle<Object> expected;
    if (i == 0 || i == 256 || i == 65536) {
      expected = isolate()->factory()->NewNumber(count++);
    } else {
      expected = isolate()->factory()->the_hole_value();
    }
    CHECK(constant_array->get(i)->SameValue(*expected));
  }
}

TEST_F(ConstantArrayBuilderTest, AllocateEntriesWithFixedReservations) {
  CanonicalHandleScope canonical(isolate());
  ConstantArrayBuilder builder(zone(), isolate()->factory()->the_hole_value());
  for (size_t i = 0; i < k16BitCapacity; i++) {
    if ((i % 2) == 0) {
      CHECK_EQ(i, builder.AllocateEntry());
    } else {
      builder.Insert(handle(Smi::FromInt(static_cast<int>(i)), isolate()));
    }
  }
  CHECK_EQ(builder.size(), k16BitCapacity);

  // Check values before reserved entries are inserted.
  for (size_t i = 0; i < k16BitCapacity; i++) {
    if ((i % 2) == 0) {
      // Check reserved values are the hole.
      Handle<Object> empty = builder.At(i);
      CHECK(empty->SameValue(isolate()->heap()->the_hole_value()));
    } else {
      CHECK_EQ(Handle<Smi>::cast(builder.At(i))->value(), static_cast<int>(i));
    }
  }

  // Insert reserved entries.
  for (size_t i = 0; i < k16BitCapacity; i += 2) {
    builder.InsertAllocatedEntry(
        i, handle(Smi::FromInt(static_cast<int>(i)), isolate()));
  }

  // Check values after reserved entries are inserted.
  for (size_t i = 0; i < k16BitCapacity; i++) {
    CHECK_EQ(Handle<Smi>::cast(builder.At(i))->value(), static_cast<int>(i));
  }
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
