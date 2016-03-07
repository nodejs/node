// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/factory.h"
#include "src/handles-inl.h"
#include "src/interpreter/constant-array-builder.h"
#include "src/isolate.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

class ConstantArrayBuilderTest : public TestWithIsolateAndZone {
 public:
  ConstantArrayBuilderTest() {}
  ~ConstantArrayBuilderTest() override {}

  static const size_t kLowCapacity = ConstantArrayBuilder::kLowCapacity;
  static const size_t kMaxCapacity = ConstantArrayBuilder::kMaxCapacity;
};


STATIC_CONST_MEMBER_DEFINITION const size_t
    ConstantArrayBuilderTest::kMaxCapacity;
STATIC_CONST_MEMBER_DEFINITION const size_t
    ConstantArrayBuilderTest::kLowCapacity;


TEST_F(ConstantArrayBuilderTest, AllocateAllEntries) {
  ConstantArrayBuilder builder(isolate(), zone());
  for (size_t i = 0; i < kMaxCapacity; i++) {
    builder.Insert(handle(Smi::FromInt(static_cast<int>(i)), isolate()));
  }
  CHECK_EQ(builder.size(), kMaxCapacity);
  for (size_t i = 0; i < kMaxCapacity; i++) {
    CHECK_EQ(Handle<Smi>::cast(builder.At(i))->value(), i);
  }
}


TEST_F(ConstantArrayBuilderTest, AllocateEntriesWithIdx8Reservations) {
  for (size_t reserved = 1; reserved < kLowCapacity; reserved *= 3) {
    ConstantArrayBuilder builder(isolate(), zone());
    for (size_t i = 0; i < reserved; i++) {
      OperandSize operand_size = builder.CreateReservedEntry();
      CHECK(operand_size == OperandSize::kByte);
    }
    for (size_t i = 0; i < 2 * kLowCapacity; i++) {
      Handle<Object> object = isolate()->factory()->NewNumberFromSize(i);
      builder.Insert(object);
      if (i + reserved < kLowCapacity) {
        CHECK_LE(builder.size(), kLowCapacity);
        CHECK_EQ(builder.size(), i + 1);
        CHECK(builder.At(i)->SameValue(*object));
      } else {
        CHECK_GE(builder.size(), kLowCapacity);
        CHECK_EQ(builder.size(), i + reserved + 1);
        CHECK(builder.At(i + reserved)->SameValue(*object));
      }
    }
    CHECK_EQ(builder.size(), 2 * kLowCapacity + reserved);

    // Check reserved values represented by the hole.
    for (size_t i = 0; i < reserved; i++) {
      Handle<Object> empty = builder.At(kLowCapacity - reserved + i);
      CHECK(empty->SameValue(isolate()->heap()->the_hole_value()));
    }

    // Commmit reserved entries with duplicates and check size does not change.
    DCHECK_EQ(reserved + 2 * kLowCapacity, builder.size());
    size_t duplicates_in_idx8_space =
        std::min(reserved, kLowCapacity - reserved);
    for (size_t i = 0; i < duplicates_in_idx8_space; i++) {
      builder.CommitReservedEntry(OperandSize::kByte,
                                  isolate()->factory()->NewNumberFromSize(i));
      DCHECK_EQ(reserved + 2 * kLowCapacity, builder.size());
    }

    // Check all committed values match expected (holes where
    // duplicates_in_idx8_space allocated).
    for (size_t i = 0; i < kLowCapacity - reserved; i++) {
      Smi* smi = Smi::FromInt(static_cast<int>(i));
      CHECK(Handle<Smi>::cast(builder.At(i))->SameValue(smi));
    }
    for (size_t i = kLowCapacity; i < 2 * kLowCapacity + reserved; i++) {
      Smi* smi = Smi::FromInt(static_cast<int>(i - reserved));
      CHECK(Handle<Smi>::cast(builder.At(i))->SameValue(smi));
    }
    for (size_t i = 0; i < reserved; i++) {
      size_t index = kLowCapacity - reserved + i;
      CHECK(builder.At(index)->IsTheHole());
    }

    // Now make reservations, and commit them with unique entries.
    for (size_t i = 0; i < duplicates_in_idx8_space; i++) {
      OperandSize operand_size = builder.CreateReservedEntry();
      CHECK(operand_size == OperandSize::kByte);
    }
    for (size_t i = 0; i < duplicates_in_idx8_space; i++) {
      Handle<Object> object =
          isolate()->factory()->NewNumberFromSize(2 * kLowCapacity + i);
      size_t index = builder.CommitReservedEntry(OperandSize::kByte, object);
      CHECK_EQ(static_cast<int>(index), kLowCapacity - reserved + i);
      CHECK(builder.At(static_cast<int>(index))->SameValue(*object));
    }
    CHECK_EQ(builder.size(), 2 * kLowCapacity + reserved);
  }
}


TEST_F(ConstantArrayBuilderTest, AllocateEntriesWithIdx16Reservations) {
  for (size_t reserved = 1; reserved < kLowCapacity; reserved *= 3) {
    ConstantArrayBuilder builder(isolate(), zone());
    for (size_t i = 0; i < kLowCapacity; i++) {
      Handle<Object> object = isolate()->factory()->NewNumberFromSize(i);
      builder.Insert(object);
      CHECK(builder.At(i)->SameValue(*object));
      CHECK_EQ(builder.size(), i + 1);
    }
    for (size_t i = 0; i < reserved; i++) {
      OperandSize operand_size = builder.CreateReservedEntry();
      CHECK(operand_size == OperandSize::kShort);
      CHECK_EQ(builder.size(), kLowCapacity);
    }
    for (size_t i = 0; i < reserved; i++) {
      builder.DiscardReservedEntry(OperandSize::kShort);
      CHECK_EQ(builder.size(), kLowCapacity);
    }
    for (size_t i = 0; i < reserved; i++) {
      OperandSize operand_size = builder.CreateReservedEntry();
      CHECK(operand_size == OperandSize::kShort);
      Handle<Object> object = isolate()->factory()->NewNumberFromSize(i);
      builder.CommitReservedEntry(operand_size, object);
      CHECK_EQ(builder.size(), kLowCapacity);
    }
    for (size_t i = kLowCapacity; i < kLowCapacity + reserved; i++) {
      OperandSize operand_size = builder.CreateReservedEntry();
      CHECK(operand_size == OperandSize::kShort);
      Handle<Object> object = isolate()->factory()->NewNumberFromSize(i);
      builder.CommitReservedEntry(operand_size, object);
      CHECK_EQ(builder.size(), i + 1);
    }
  }
}


TEST_F(ConstantArrayBuilderTest, ToFixedArray) {
  ConstantArrayBuilder builder(isolate(), zone());
  static const size_t kNumberOfElements = 37;
  for (size_t i = 0; i < kNumberOfElements; i++) {
    Handle<Object> object = isolate()->factory()->NewNumberFromSize(i);
    builder.Insert(object);
    CHECK(builder.At(i)->SameValue(*object));
  }
  Handle<FixedArray> constant_array = builder.ToFixedArray();
  CHECK_EQ(constant_array->length(), kNumberOfElements);
  for (size_t i = 0; i < kNumberOfElements; i++) {
    CHECK(constant_array->get(static_cast<int>(i))->SameValue(*builder.At(i)));
  }
}


TEST_F(ConstantArrayBuilderTest, GapFilledWhenLowReservationCommitted) {
  ConstantArrayBuilder builder(isolate(), zone());
  for (size_t i = 0; i < kLowCapacity; i++) {
    OperandSize operand_size = builder.CreateReservedEntry();
    CHECK(OperandSize::kByte == operand_size);
    CHECK_EQ(builder.size(), 0);
  }
  for (size_t i = 0; i < kLowCapacity; i++) {
    Handle<Object> object = isolate()->factory()->NewNumberFromSize(i);
    builder.Insert(object);
    CHECK_EQ(builder.size(), i + kLowCapacity + 1);
  }
  for (size_t i = 0; i < kLowCapacity; i++) {
    builder.CommitReservedEntry(OperandSize::kByte,
                                builder.At(i + kLowCapacity));
    CHECK_EQ(builder.size(), 2 * kLowCapacity);
  }
  for (size_t i = 0; i < kLowCapacity; i++) {
    Handle<Object> original = builder.At(kLowCapacity + i);
    Handle<Object> duplicate = builder.At(i);
    CHECK(original->SameValue(*duplicate));
    Handle<Object> reference = isolate()->factory()->NewNumberFromSize(i);
    CHECK(original->SameValue(*reference));
  }
}


TEST_F(ConstantArrayBuilderTest, GapNotFilledWhenLowReservationDiscarded) {
  ConstantArrayBuilder builder(isolate(), zone());
  for (size_t i = 0; i < kLowCapacity; i++) {
    OperandSize operand_size = builder.CreateReservedEntry();
    CHECK(OperandSize::kByte == operand_size);
    CHECK_EQ(builder.size(), 0);
  }
  for (size_t i = 0; i < kLowCapacity; i++) {
    Handle<Object> object = isolate()->factory()->NewNumberFromSize(i);
    builder.Insert(object);
    CHECK_EQ(builder.size(), i + kLowCapacity + 1);
  }
  for (size_t i = 0; i < kLowCapacity; i++) {
    builder.DiscardReservedEntry(OperandSize::kByte);
    builder.Insert(builder.At(i + kLowCapacity));
    CHECK_EQ(builder.size(), 2 * kLowCapacity);
  }
  for (size_t i = 0; i < kLowCapacity; i++) {
    Handle<Object> reference = isolate()->factory()->NewNumberFromSize(i);
    Handle<Object> original = builder.At(kLowCapacity + i);
    CHECK(original->SameValue(*reference));
    Handle<Object> duplicate = builder.At(i);
    CHECK(duplicate->SameValue(*isolate()->factory()->the_hole_value()));
  }
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
