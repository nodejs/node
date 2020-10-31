// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: ksroka@google.com (Krzysztof Sroka)

#include <google/protobuf/util/field_comparator.h>

#include <google/protobuf/unittest.pb.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/stubs/mathutil.h>
// This gtest header is put after mathutil.h intentionally. We have to do
// this because mathutil.h includes mathlimits.h which requires cmath not
// being included to compile on some versions of gcc:
//   https://github.com/protocolbuffers/protobuf/blob/818c5eee08840355d70d2f3bdf1a2f17986a5e70/src/google/protobuf/stubs/mathlimits.h#L48
// and the opensource version gtest.h header includes cmath transitively
// somehow.
#include <gtest/gtest.h>
namespace google {
namespace protobuf {
namespace util {
namespace {

using protobuf_unittest::TestAllTypes;

class DefaultFieldComparatorTest : public ::testing::Test {
 protected:
  void SetUp() { descriptor_ = TestAllTypes::descriptor(); }

  const Descriptor* descriptor_;
  DefaultFieldComparator comparator_;
  TestAllTypes message_1_;
  TestAllTypes message_2_;
};

TEST_F(DefaultFieldComparatorTest, RecursesIntoGroup) {
  const FieldDescriptor* field = descriptor_->FindFieldByName("optionalgroup");
  EXPECT_EQ(FieldComparator::RECURSE,
            comparator_.Compare(message_1_, message_2_, field, -1, -1, NULL));
}

TEST_F(DefaultFieldComparatorTest, RecursesIntoNestedMessage) {
  const FieldDescriptor* field =
      descriptor_->FindFieldByName("optional_nested_message");
  EXPECT_EQ(FieldComparator::RECURSE,
            comparator_.Compare(message_1_, message_2_, field, -1, -1, NULL));
}

TEST_F(DefaultFieldComparatorTest, RecursesIntoForeignMessage) {
  const FieldDescriptor* field =
      descriptor_->FindFieldByName("optional_foreign_message");
  EXPECT_EQ(FieldComparator::RECURSE,
            comparator_.Compare(message_1_, message_2_, field, -1, -1, NULL));
}

TEST_F(DefaultFieldComparatorTest, Int32Comparison) {
  const FieldDescriptor* field = descriptor_->FindFieldByName("optional_int32");
  message_1_.set_optional_int32(1);
  message_2_.set_optional_int32(1);

  EXPECT_EQ(FieldComparator::SAME,
            comparator_.Compare(message_1_, message_2_, field, -1, -1, NULL));

  message_2_.set_optional_int32(-1);
  EXPECT_EQ(FieldComparator::DIFFERENT,
            comparator_.Compare(message_1_, message_2_, field, -1, -1, NULL));
}

TEST_F(DefaultFieldComparatorTest, Int64Comparison) {
  const FieldDescriptor* field = descriptor_->FindFieldByName("optional_int64");
  message_1_.set_optional_int64(1L);
  message_2_.set_optional_int64(1L);

  EXPECT_EQ(FieldComparator::SAME,
            comparator_.Compare(message_1_, message_2_, field, -1, -1, NULL));

  message_2_.set_optional_int64(-1L);
  EXPECT_EQ(FieldComparator::DIFFERENT,
            comparator_.Compare(message_1_, message_2_, field, -1, -1, NULL));
}

TEST_F(DefaultFieldComparatorTest, UInt32Comparison) {
  const FieldDescriptor* field =
      descriptor_->FindFieldByName("optional_uint32");
  message_1_.set_optional_uint32(1);
  message_2_.set_optional_uint32(1);

  EXPECT_EQ(FieldComparator::SAME,
            comparator_.Compare(message_1_, message_2_, field, -1, -1, NULL));

  message_2_.set_optional_uint32(2);
  EXPECT_EQ(FieldComparator::DIFFERENT,
            comparator_.Compare(message_1_, message_2_, field, -1, -1, NULL));
}

TEST_F(DefaultFieldComparatorTest, UInt64Comparison) {
  const FieldDescriptor* field =
      descriptor_->FindFieldByName("optional_uint64");
  message_1_.set_optional_uint64(1L);
  message_2_.set_optional_uint64(1L);

  EXPECT_EQ(FieldComparator::SAME,
            comparator_.Compare(message_1_, message_2_, field, -1, -1, NULL));

  message_2_.set_optional_uint64(2L);
  EXPECT_EQ(FieldComparator::DIFFERENT,
            comparator_.Compare(message_1_, message_2_, field, -1, -1, NULL));
}

TEST_F(DefaultFieldComparatorTest, BooleanComparison) {
  const FieldDescriptor* field = descriptor_->FindFieldByName("optional_bool");
  message_1_.set_optional_bool(true);
  message_2_.set_optional_bool(true);

  EXPECT_EQ(FieldComparator::SAME,
            comparator_.Compare(message_1_, message_2_, field, -1, -1, NULL));

  message_2_.set_optional_bool(false);
  EXPECT_EQ(FieldComparator::DIFFERENT,
            comparator_.Compare(message_1_, message_2_, field, -1, -1, NULL));
}

TEST_F(DefaultFieldComparatorTest, EnumComparison) {
  const FieldDescriptor* field =
      descriptor_->FindFieldByName("optional_nested_enum");
  message_1_.set_optional_nested_enum(TestAllTypes::BAR);
  message_2_.set_optional_nested_enum(TestAllTypes::BAR);

  EXPECT_EQ(FieldComparator::SAME,
            comparator_.Compare(message_1_, message_2_, field, -1, -1, NULL));

  message_2_.set_optional_nested_enum(TestAllTypes::BAZ);
  EXPECT_EQ(FieldComparator::DIFFERENT,
            comparator_.Compare(message_1_, message_2_, field, -1, -1, NULL));
}

TEST_F(DefaultFieldComparatorTest, StringComparison) {
  const FieldDescriptor* field =
      descriptor_->FindFieldByName("optional_string");
  message_1_.set_optional_string("foo");
  message_2_.set_optional_string("foo");

  EXPECT_EQ(FieldComparator::SAME,
            comparator_.Compare(message_1_, message_2_, field, -1, -1, NULL));

  message_2_.set_optional_string("bar");
  EXPECT_EQ(FieldComparator::DIFFERENT,
            comparator_.Compare(message_1_, message_2_, field, -1, -1, NULL));
}

TEST_F(DefaultFieldComparatorTest, FloatingPointComparisonExact) {
  const FieldDescriptor* field_float =
      descriptor_->FindFieldByName("optional_float");
  const FieldDescriptor* field_double =
      descriptor_->FindFieldByName("optional_double");

  message_1_.set_optional_float(0.1f);
  message_2_.set_optional_float(0.1f);
  message_1_.set_optional_double(0.1);
  message_2_.set_optional_double(0.1);

  EXPECT_EQ(
      FieldComparator::SAME,
      comparator_.Compare(message_1_, message_2_, field_float, -1, -1, NULL));
  EXPECT_EQ(
      FieldComparator::SAME,
      comparator_.Compare(message_1_, message_2_, field_double, -1, -1, NULL));

  message_2_.set_optional_float(0.2f);
  message_2_.set_optional_double(0.2);

  EXPECT_EQ(
      FieldComparator::DIFFERENT,
      comparator_.Compare(message_1_, message_2_, field_float, -1, -1, NULL));
  EXPECT_EQ(
      FieldComparator::DIFFERENT,
      comparator_.Compare(message_1_, message_2_, field_double, -1, -1, NULL));
}

TEST_F(DefaultFieldComparatorTest, FloatingPointComparisonApproximate) {
  const FieldDescriptor* field_float =
      descriptor_->FindFieldByName("optional_float");
  const FieldDescriptor* field_double =
      descriptor_->FindFieldByName("optional_double");

  message_1_.set_optional_float(2.300005f);
  message_2_.set_optional_float(2.300006f);
  message_1_.set_optional_double(2.3000000000000003);
  message_2_.set_optional_double(2.3000000000000007);

  // Approximate comparison depends on MathUtil, so we assert on MathUtil
  // results first to check if that's where the failure was introduced.
  ASSERT_NE(message_1_.optional_float(), message_2_.optional_float());
  ASSERT_NE(message_1_.optional_double(), message_2_.optional_double());
  ASSERT_TRUE(MathUtil::AlmostEquals(message_1_.optional_float(),
                                     message_2_.optional_float()));
  ASSERT_TRUE(MathUtil::AlmostEquals(message_1_.optional_double(),
                                     message_2_.optional_double()));

  // DefaultFieldComparator's default float comparison mode is EXACT.
  ASSERT_EQ(DefaultFieldComparator::EXACT, comparator_.float_comparison());
  EXPECT_EQ(
      FieldComparator::DIFFERENT,
      comparator_.Compare(message_1_, message_2_, field_float, -1, -1, NULL));
  EXPECT_EQ(
      FieldComparator::DIFFERENT,
      comparator_.Compare(message_1_, message_2_, field_double, -1, -1, NULL));

  comparator_.set_float_comparison(DefaultFieldComparator::APPROXIMATE);

  EXPECT_EQ(
      FieldComparator::SAME,
      comparator_.Compare(message_1_, message_2_, field_float, -1, -1, NULL));
  EXPECT_EQ(
      FieldComparator::SAME,
      comparator_.Compare(message_1_, message_2_, field_double, -1, -1, NULL));
}

TEST_F(DefaultFieldComparatorTest, FloatingPointComparisonTreatNaNsAsEqual) {
  const FieldDescriptor* field_float =
      descriptor_->FindFieldByName("optional_float");
  const FieldDescriptor* field_double =
      descriptor_->FindFieldByName("optional_double");

  message_1_.set_optional_float(MathLimits<float>::kNaN);
  message_2_.set_optional_float(MathLimits<float>::kNaN);
  message_1_.set_optional_double(MathLimits<double>::kNaN);
  message_2_.set_optional_double(MathLimits<double>::kNaN);

  // DefaultFieldComparator's default float comparison mode is EXACT with
  // treating NaNs as different.
  ASSERT_EQ(DefaultFieldComparator::EXACT, comparator_.float_comparison());
  ASSERT_EQ(false, comparator_.treat_nan_as_equal());
  EXPECT_EQ(
      FieldComparator::DIFFERENT,
      comparator_.Compare(message_1_, message_2_, field_float, -1, -1, NULL));
  EXPECT_EQ(
      FieldComparator::DIFFERENT,
      comparator_.Compare(message_1_, message_2_, field_double, -1, -1, NULL));
  comparator_.set_float_comparison(DefaultFieldComparator::APPROXIMATE);
  EXPECT_EQ(
      FieldComparator::DIFFERENT,
      comparator_.Compare(message_1_, message_2_, field_float, -1, -1, NULL));
  EXPECT_EQ(
      FieldComparator::DIFFERENT,
      comparator_.Compare(message_1_, message_2_, field_double, -1, -1, NULL));

  comparator_.set_treat_nan_as_equal(true);
  ASSERT_EQ(true, comparator_.treat_nan_as_equal());
  comparator_.set_float_comparison(DefaultFieldComparator::EXACT);
  EXPECT_EQ(
      FieldComparator::SAME,
      comparator_.Compare(message_1_, message_2_, field_float, -1, -1, NULL));
  EXPECT_EQ(
      FieldComparator::SAME,
      comparator_.Compare(message_1_, message_2_, field_double, -1, -1, NULL));
  comparator_.set_float_comparison(DefaultFieldComparator::APPROXIMATE);
  EXPECT_EQ(
      FieldComparator::SAME,
      comparator_.Compare(message_1_, message_2_, field_float, -1, -1, NULL));
  EXPECT_EQ(
      FieldComparator::SAME,
      comparator_.Compare(message_1_, message_2_, field_double, -1, -1, NULL));
}

TEST_F(DefaultFieldComparatorTest,
       FloatingPointComparisonWithinFractionOrMargin) {
  const FieldDescriptor* field_float =
      descriptor_->FindFieldByName("optional_float");
  const FieldDescriptor* field_double =
      descriptor_->FindFieldByName("optional_double");

  message_1_.set_optional_float(100.0f);
  message_2_.set_optional_float(109.9f);
  message_1_.set_optional_double(100.0);
  message_2_.set_optional_double(109.9);

  comparator_.set_float_comparison(DefaultFieldComparator::APPROXIMATE);
  EXPECT_EQ(
      FieldComparator::DIFFERENT,
      comparator_.Compare(message_1_, message_2_, field_float, -1, -1, NULL));
  EXPECT_EQ(
      FieldComparator::DIFFERENT,
      comparator_.Compare(message_1_, message_2_, field_double, -1, -1, NULL));

  // Should fail since the fraction is too low.
  comparator_.SetFractionAndMargin(field_float, 0.01, 0.0);
  comparator_.SetFractionAndMargin(field_double, 0.01, 0.0);

  EXPECT_EQ(
      FieldComparator::DIFFERENT,
      comparator_.Compare(message_1_, message_2_, field_float, -1, -1, NULL));
  EXPECT_EQ(
      FieldComparator::DIFFERENT,
      comparator_.Compare(message_1_, message_2_, field_double, -1, -1, NULL));

  // Should fail since the margin is too low.
  comparator_.SetFractionAndMargin(field_float, 0.0, 9.0);
  comparator_.SetFractionAndMargin(field_double, 0.0, 9.0);
  EXPECT_EQ(
      FieldComparator::DIFFERENT,
      comparator_.Compare(message_1_, message_2_, field_float, -1, -1, NULL));
  EXPECT_EQ(
      FieldComparator::DIFFERENT,
      comparator_.Compare(message_1_, message_2_, field_double, -1, -1, NULL));

  // Should succeed since the fraction is high enough.
  comparator_.SetFractionAndMargin(field_float, 0.2, 0.0);
  comparator_.SetFractionAndMargin(field_double, 0.2, 0.0);
  EXPECT_EQ(
      FieldComparator::SAME,
      comparator_.Compare(message_1_, message_2_, field_float, -1, -1, NULL));
  EXPECT_EQ(
      FieldComparator::SAME,
      comparator_.Compare(message_1_, message_2_, field_double, -1, -1, NULL));

  // Should succeed since the margin is high enough.
  comparator_.SetFractionAndMargin(field_float, 0.0, 10.0);
  comparator_.SetFractionAndMargin(field_double, 0.0, 10.0);
  EXPECT_EQ(
      FieldComparator::SAME,
      comparator_.Compare(message_1_, message_2_, field_float, -1, -1, NULL));
  EXPECT_EQ(
      FieldComparator::SAME,
      comparator_.Compare(message_1_, message_2_, field_double, -1, -1, NULL));

  // Setting values for one of the fields should not affect the other.
  comparator_.SetFractionAndMargin(field_double, 0.0, 0.0);
  EXPECT_EQ(
      FieldComparator::SAME,
      comparator_.Compare(message_1_, message_2_, field_float, -1, -1, NULL));
  EXPECT_EQ(
      FieldComparator::DIFFERENT,
      comparator_.Compare(message_1_, message_2_, field_double, -1, -1, NULL));

  // +inf should be equal even though they are not technically within margin or
  // fraction.
  message_1_.set_optional_float(std::numeric_limits<float>::infinity());
  message_2_.set_optional_float(std::numeric_limits<float>::infinity());
  message_1_.set_optional_double(std::numeric_limits<double>::infinity());
  message_2_.set_optional_double(std::numeric_limits<double>::infinity());
  comparator_.SetFractionAndMargin(field_float, 0.0, 0.0);
  comparator_.SetFractionAndMargin(field_double, 0.0, 0.0);
  EXPECT_EQ(
      FieldComparator::SAME,
      comparator_.Compare(message_1_, message_2_, field_float, -1, -1, NULL));
  EXPECT_EQ(
      FieldComparator::SAME,
      comparator_.Compare(message_1_, message_2_, field_double, -1, -1, NULL));

  // -inf should be equal even though they are not technically within margin or
  // fraction.
  message_1_.set_optional_float(-std::numeric_limits<float>::infinity());
  message_2_.set_optional_float(-std::numeric_limits<float>::infinity());
  message_1_.set_optional_double(-std::numeric_limits<double>::infinity());
  message_2_.set_optional_double(-std::numeric_limits<double>::infinity());
  comparator_.SetFractionAndMargin(field_float, 0.0, 0.0);
  comparator_.SetFractionAndMargin(field_double, 0.0, 0.0);
  EXPECT_EQ(
      FieldComparator::SAME,
      comparator_.Compare(message_1_, message_2_, field_float, -1, -1, NULL));
  EXPECT_EQ(
      FieldComparator::SAME,
      comparator_.Compare(message_1_, message_2_, field_double, -1, -1, NULL));

  // Finite values and inf should not be equal, even for a positive fraction.
  message_1_.set_optional_float(std::numeric_limits<float>::infinity());
  message_2_.set_optional_float(0.0f);
  message_1_.set_optional_double(std::numeric_limits<double>::infinity());
  message_2_.set_optional_double(0.0);
  comparator_.SetFractionAndMargin(field_float, 0.1, 0.0);
  comparator_.SetFractionAndMargin(field_double, 0.1, 0.0);
  EXPECT_EQ(FieldComparator::DIFFERENT,
            comparator_.Compare(message_1_, message_2_, field_float, -1, -1,
                                nullptr));
  EXPECT_EQ(FieldComparator::DIFFERENT,
            comparator_.Compare(message_1_, message_2_, field_double, -1, -1,
                                nullptr));
}

TEST_F(DefaultFieldComparatorTest,
       FloatingPointComparisonWithinDefaultFractionOrMargin) {
  const FieldDescriptor* field_float =
      descriptor_->FindFieldByName("optional_float");
  const FieldDescriptor* field_double =
      descriptor_->FindFieldByName("optional_double");

  message_1_.set_optional_float(100.0f);
  message_2_.set_optional_float(109.9f);
  message_1_.set_optional_double(100.0);
  message_2_.set_optional_double(109.9);

  comparator_.set_float_comparison(DefaultFieldComparator::APPROXIMATE);
  EXPECT_EQ(
      FieldComparator::DIFFERENT,
      comparator_.Compare(message_1_, message_2_, field_float, -1, -1, NULL));
  EXPECT_EQ(
      FieldComparator::DIFFERENT,
      comparator_.Compare(message_1_, message_2_, field_double, -1, -1, NULL));

  // Set default fraction and margin.
  comparator_.SetDefaultFractionAndMargin(0.01, 0.0);

  // Float comparisons should fail since the fraction is too low.
  EXPECT_EQ(
      FieldComparator::DIFFERENT,
      comparator_.Compare(message_1_, message_2_, field_float, -1, -1, NULL));
  EXPECT_EQ(
      FieldComparator::DIFFERENT,
      comparator_.Compare(message_1_, message_2_, field_double, -1, -1, NULL));

  // Set field-specific fraction and margin for one field (field_float) but not
  // the other (field_double)
  comparator_.SetFractionAndMargin(field_float, 0.2, 0.0);

  // The field with the override should succeed, since its field-specific
  // fraction is high enough.
  EXPECT_EQ(
      FieldComparator::SAME,
      comparator_.Compare(message_1_, message_2_, field_float, -1, -1, NULL));
  // The field with no override should fail, since the default fraction is too
  // low
  EXPECT_EQ(
      FieldComparator::DIFFERENT,
      comparator_.Compare(message_1_, message_2_, field_double, -1, -1, NULL));

  // Set the default fraction and margin high enough so that fields that use
  // the default should succeed
  comparator_.SetDefaultFractionAndMargin(0.2, 0.0);
  EXPECT_EQ(
      FieldComparator::SAME,
      comparator_.Compare(message_1_, message_2_, field_double, -1, -1, NULL));

  // The field with an override should still be OK
  EXPECT_EQ(
      FieldComparator::SAME,
      comparator_.Compare(message_1_, message_2_, field_float, -1, -1, NULL));

  // Set fraction and margin for the field with an override to be too low
  comparator_.SetFractionAndMargin(field_float, 0.01, 0.0);

  // Now our default is high enough but field_float's override is too low.
  EXPECT_EQ(
      FieldComparator::DIFFERENT,
      comparator_.Compare(message_1_, message_2_, field_float, -1, -1, NULL));
  EXPECT_EQ(
      FieldComparator::SAME,
      comparator_.Compare(message_1_, message_2_, field_double, -1, -1, NULL));
}

// Simple test checking whether we compare values at correct indices.
TEST_F(DefaultFieldComparatorTest, RepeatedFieldComparison) {
  const FieldDescriptor* field =
      descriptor_->FindFieldByName("repeated_string");

  message_1_.add_repeated_string("foo");
  message_1_.add_repeated_string("bar");
  message_2_.add_repeated_string("bar");
  message_2_.add_repeated_string("baz");

  EXPECT_EQ(FieldComparator::DIFFERENT,
            comparator_.Compare(message_1_, message_2_, field, 0, 0, NULL));
  EXPECT_EQ(FieldComparator::DIFFERENT,
            comparator_.Compare(message_1_, message_2_, field, 1, 1, NULL));
  EXPECT_EQ(FieldComparator::SAME,
            comparator_.Compare(message_1_, message_2_, field, 1, 0, NULL));
}

}  // namespace
}  // namespace util
}  // namespace protobuf
}  // namespace google
