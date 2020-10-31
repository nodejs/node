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

// Author: jschorr@google.com (Joseph Schorr)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.
//
// TODO(ksroka): Move some of these tests to field_comparator_test.cc.

#include <algorithm>
#include <random>
#include <string>
#include <vector>

#include <google/protobuf/stubs/strutil.h>

#include <google/protobuf/any_test.pb.h>
#include <google/protobuf/map_test_util.h>
#include <google/protobuf/map_unittest.pb.h>
#include <google/protobuf/test_util.h>
#include <google/protobuf/unittest.pb.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/util/message_differencer_unittest.pb.h>
#include <google/protobuf/util/field_comparator.h>
#include <google/protobuf/util/message_differencer.h>

#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {

namespace {


const FieldDescriptor* GetFieldDescriptor(const Message& message,
                                          const std::string& field_name) {
  std::vector<std::string> field_path =
      Split(field_name, ".", true);
  const Descriptor* descriptor = message.GetDescriptor();
  const FieldDescriptor* field = NULL;
  for (int i = 0; i < field_path.size(); i++) {
    field = descriptor->FindFieldByName(field_path[i]);
    descriptor = field->message_type();
  }
  return field;
}

void ExpectEqualsWithDifferencer(util::MessageDifferencer* differencer,
                                 const Message& msg1, const Message& msg2) {
  differencer->set_scope(util::MessageDifferencer::FULL);
  EXPECT_TRUE(differencer->Compare(msg1, msg2));

  differencer->set_scope(util::MessageDifferencer::PARTIAL);
  EXPECT_TRUE(differencer->Compare(msg1, msg2));
}

TEST(MessageDifferencerTest, BasicEqualityTest) {
  // Create the testing protos
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  TestUtil::SetAllFields(&msg1);
  TestUtil::SetAllFields(&msg2);

  // Compare
  EXPECT_TRUE(util::MessageDifferencer::Equals(msg1, msg2));
}

TEST(MessageDifferencerTest, BasicInequalityTest) {
  // Create the testing protos
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  TestUtil::SetAllFields(&msg1);
  TestUtil::SetAllFields(&msg2);

  msg1.set_optional_int32(-1);

  // Compare
  EXPECT_FALSE(util::MessageDifferencer::Equals(msg1, msg2));
}

TEST(MessageDifferencerTest, RepeatedFieldInequalityTest) {
  // Create the testing protos
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  TestUtil::SetAllFields(&msg1);
  TestUtil::SetAllFields(&msg2);

  msg1.add_repeated_int32(-1);

  // Compare
  EXPECT_FALSE(util::MessageDifferencer::Equals(msg1, msg2));
}

TEST(MessageDifferencerTest, RepeatedFieldSetOptimizationTest) {
  util::MessageDifferencer differencer;
  protobuf_unittest::TestDiffMessage msg1;
  protobuf_unittest::TestDiffMessage msg2;
  protobuf_unittest::TestDiffMessage::Item* item1 = msg1.add_item();
  protobuf_unittest::TestDiffMessage::Item* item2 = msg2.add_item();
  differencer.TreatAsSet(item1->GetDescriptor()->FindFieldByName("ra"));
  differencer.TreatAsSet(item2->GetDescriptor()->FindFieldByName("ra"));
  for (int i = 0; i < 1000; i++) {
    item1->add_ra(i);
    item2->add_ra(i);
  }
  EXPECT_TRUE(differencer.Compare(msg1, msg2));
  item2->add_ra(1001);
  EXPECT_FALSE(differencer.Compare(msg1, msg2));
  item1->add_ra(1001);
  EXPECT_TRUE(differencer.Compare(msg1, msg2));
  item1->add_ra(1002);
  EXPECT_FALSE(differencer.Compare(msg1, msg2));
}

TEST(MessageDifferencerTest, MapFieldEqualityTest) {
  // Create the testing protos
  unittest::TestMap msg1;
  unittest::TestMap msg2;

  MapReflectionTester tester(unittest::TestMap::descriptor());
  tester.SetMapFieldsViaReflection(&msg1);
  tester.SetMapFieldsViaReflection(&msg2);
  tester.SwapMapsViaReflection(&msg1);

  // Compare
  EXPECT_TRUE(util::MessageDifferencer::Equals(msg1, msg2));
}

TEST(MessageDifferencerTest, BasicPartialEqualityTest) {
  // Create the testing protos
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  TestUtil::SetAllFields(&msg1);
  TestUtil::SetAllFields(&msg2);

  // Compare
  util::MessageDifferencer differencer;
  differencer.set_scope(util::MessageDifferencer::PARTIAL);
  EXPECT_TRUE(differencer.Compare(msg1, msg2));
}

TEST(MessageDifferencerTest, PartialEqualityTestExtraField) {
  // Create the testing protos
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  TestUtil::SetAllFields(&msg1);
  TestUtil::SetAllFields(&msg2);

  msg1.clear_optional_int32();

  // Compare
  util::MessageDifferencer differencer;
  differencer.set_scope(util::MessageDifferencer::PARTIAL);
  EXPECT_TRUE(differencer.Compare(msg1, msg2));
}

TEST(MessageDifferencerTest, PartialEqualityTestSkipRequiredField) {
  // Create the testing protos
  unittest::TestRequired msg1;
  unittest::TestRequired msg2;

  msg1.set_a(401);
  msg2.set_a(401);
  msg2.set_b(402);

  // Compare
  util::MessageDifferencer differencer;
  differencer.set_scope(util::MessageDifferencer::PARTIAL);
  EXPECT_TRUE(differencer.Compare(msg1, msg2));
}

TEST(MessageDifferencerTest, BasicPartialInequalityTest) {
  // Create the testing protos
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  TestUtil::SetAllFields(&msg1);
  TestUtil::SetAllFields(&msg2);

  msg1.set_optional_int32(-1);

  // Compare
  util::MessageDifferencer differencer;
  differencer.set_scope(util::MessageDifferencer::PARTIAL);
  EXPECT_FALSE(differencer.Compare(msg1, msg2));
}

TEST(MessageDifferencerTest, PartialInequalityMissingFieldTest) {
  // Create the testing protos
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  TestUtil::SetAllFields(&msg1);
  TestUtil::SetAllFields(&msg2);

  msg2.clear_optional_int32();

  // Compare
  util::MessageDifferencer differencer;
  differencer.set_scope(util::MessageDifferencer::PARTIAL);
  EXPECT_FALSE(differencer.Compare(msg1, msg2));
}

TEST(MessageDifferencerTest, RepeatedFieldPartialInequalityTest) {
  // Create the testing protos
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  TestUtil::SetAllFields(&msg1);
  TestUtil::SetAllFields(&msg2);

  msg1.add_repeated_int32(-1);

  // Compare
  util::MessageDifferencer differencer;
  differencer.set_scope(util::MessageDifferencer::PARTIAL);
  EXPECT_FALSE(differencer.Compare(msg1, msg2));
}

TEST(MessageDifferencerTest, BasicEquivalencyTest) {
  // Create the testing protos
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  TestUtil::SetAllFields(&msg1);
  TestUtil::SetAllFields(&msg2);

  // Compare
  EXPECT_TRUE(util::MessageDifferencer::Equivalent(msg1, msg2));
}

TEST(MessageDifferencerTest, EquivalencyNotEqualTest) {
  // Create the testing protos
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  TestUtil::SetAllFields(&msg1);
  TestUtil::SetAllFields(&msg2);

  msg1.clear_optional_int32();
  msg2.set_optional_int32(0);

  // Compare
  EXPECT_FALSE(util::MessageDifferencer::Equals(msg1, msg2));
  EXPECT_TRUE(util::MessageDifferencer::Equivalent(msg1, msg2));
}

TEST(MessageDifferencerTest, BasicInequivalencyTest) {
  // Create the testing protos
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  TestUtil::SetAllFields(&msg1);
  TestUtil::SetAllFields(&msg2);

  msg1.set_optional_int32(-1);

  // Compare
  EXPECT_FALSE(util::MessageDifferencer::Equivalent(msg1, msg2));
}

TEST(MessageDifferencerTest, BasicEquivalencyNonSetTest) {
  // Create the testing protos
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  // Compare
  EXPECT_TRUE(util::MessageDifferencer::Equivalent(msg1, msg2));
}

TEST(MessageDifferencerTest, BasicInequivalencyNonSetTest) {
  // Create the testing protos
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  msg1.set_optional_int32(-1);

  // Compare
  EXPECT_FALSE(util::MessageDifferencer::Equivalent(msg1, msg2));
}

TEST(MessageDifferencerTest, BasicPartialEquivalencyTest) {
  // Create the testing protos
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  TestUtil::SetAllFields(&msg1);
  TestUtil::SetAllFields(&msg2);

  // Compare
  util::MessageDifferencer differencer;
  differencer.set_message_field_comparison(
      util::MessageDifferencer::EQUIVALENT);
  differencer.set_scope(util::MessageDifferencer::PARTIAL);
  EXPECT_TRUE(differencer.Compare(msg1, msg2));
}

TEST(MessageDifferencerTest, PartialEquivalencyNotEqualTest) {
  // Create the testing protos
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  TestUtil::SetAllFields(&msg1);
  TestUtil::SetAllFields(&msg2);

  msg1.set_optional_int32(0);
  msg2.clear_optional_int32();

  // Compare
  EXPECT_FALSE(util::MessageDifferencer::Equals(msg1, msg2));
  util::MessageDifferencer differencer;
  differencer.set_message_field_comparison(
      util::MessageDifferencer::EQUIVALENT);
  differencer.set_scope(util::MessageDifferencer::PARTIAL);
  EXPECT_TRUE(differencer.Compare(msg1, msg2));
}

TEST(MessageDifferencerTest, PartialEquivalencyTestExtraField) {
  // Create the testing protos
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  TestUtil::SetAllFields(&msg1);
  TestUtil::SetAllFields(&msg2);

  msg1.clear_optional_int32();

  // Compare
  util::MessageDifferencer differencer;
  differencer.set_message_field_comparison(
      util::MessageDifferencer::EQUIVALENT);
  differencer.set_scope(util::MessageDifferencer::PARTIAL);
  EXPECT_TRUE(differencer.Compare(msg1, msg2));
}

TEST(MessageDifferencerTest, PartialEquivalencyTestSkipRequiredField) {
  // Create the testing protos
  unittest::TestRequired msg1;
  unittest::TestRequired msg2;

  msg1.set_a(401);
  msg2.set_a(401);
  msg2.set_b(402);

  // Compare
  util::MessageDifferencer differencer;
  differencer.set_message_field_comparison(
      util::MessageDifferencer::EQUIVALENT);
  differencer.set_scope(util::MessageDifferencer::PARTIAL);
  EXPECT_TRUE(differencer.Compare(msg1, msg2));
}

TEST(MessageDifferencerTest, BasicPartialInequivalencyTest) {
  // Create the testing protos
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  TestUtil::SetAllFields(&msg1);
  TestUtil::SetAllFields(&msg2);

  msg1.set_optional_int32(-1);

  // Compare
  util::MessageDifferencer differencer;
  differencer.set_message_field_comparison(
      util::MessageDifferencer::EQUIVALENT);
  differencer.set_scope(util::MessageDifferencer::PARTIAL);
  EXPECT_FALSE(differencer.Compare(msg1, msg2));
}

TEST(MessageDifferencerTest, BasicPartialEquivalencyNonSetTest) {
  // Create the testing protos
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  // Compare
  util::MessageDifferencer differencer;
  differencer.set_message_field_comparison(
      util::MessageDifferencer::EQUIVALENT);
  differencer.set_scope(util::MessageDifferencer::PARTIAL);
  EXPECT_TRUE(differencer.Compare(msg1, msg2));
}

TEST(MessageDifferencerTest, BasicPartialInequivalencyNonSetTest) {
  // Create the testing protos
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  msg1.set_optional_int32(-1);

  // Compare
  util::MessageDifferencer differencer;
  differencer.set_message_field_comparison(
      util::MessageDifferencer::EQUIVALENT);
  differencer.set_scope(util::MessageDifferencer::PARTIAL);
  EXPECT_FALSE(differencer.Compare(msg1, msg2));
}

TEST(MessageDifferencerTest, ApproximateEqualityTest) {
  // Create the testing protos
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  TestUtil::SetAllFields(&msg1);
  TestUtil::SetAllFields(&msg2);

  // Compare
  EXPECT_TRUE(util::MessageDifferencer::ApproximatelyEquals(msg1, msg2));
}

TEST(MessageDifferencerTest, ApproximateModifiedEqualityTest) {
  // Create the testing protos
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  TestUtil::SetAllFields(&msg1);
  TestUtil::SetAllFields(&msg2);

  const float v1 = 2.300005f;
  const float v2 = 2.300006f;
  msg1.set_optional_float(v1);
  msg2.set_optional_float(v2);

  // Compare
  ASSERT_NE(v1, v2) << "Should not be the same: " << v1 << ", " << v2;
  ASSERT_FLOAT_EQ(v1, v2) << "Should be approx. equal: " << v1 << ", " << v2;
  EXPECT_FALSE(util::MessageDifferencer::Equals(msg1, msg2));
  EXPECT_TRUE(util::MessageDifferencer::ApproximatelyEquals(msg1, msg2));
}

TEST(MessageDifferencerTest, ApproximateEquivalencyTest) {
  // Create the testing protos
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  TestUtil::SetAllFields(&msg1);
  TestUtil::SetAllFields(&msg2);

  // Compare
  EXPECT_TRUE(util::MessageDifferencer::ApproximatelyEquivalent(msg1, msg2));
}

TEST(MessageDifferencerTest, ApproximateModifiedEquivalencyTest) {
  // Create the testing protos
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  TestUtil::SetAllFields(&msg1);
  TestUtil::SetAllFields(&msg2);

  // Modify the approximateness requirement
  const float v1 = 2.300005f;
  const float v2 = 2.300006f;
  msg1.set_optional_float(v1);
  msg2.set_optional_float(v2);

  // Compare
  ASSERT_NE(v1, v2) << "Should not be the same: " << v1 << ", " << v2;
  ASSERT_FLOAT_EQ(v1, v2) << "Should be approx. equal: " << v1 << ", " << v2;
  EXPECT_FALSE(util::MessageDifferencer::Equals(msg1, msg2));
  EXPECT_TRUE(util::MessageDifferencer::ApproximatelyEquivalent(msg1, msg2));

  // Modify the equivalency requirement too
  msg1.clear_optional_int32();
  msg2.set_optional_int32(0);

  // Compare. Now should only pass on ApproximatelyEquivalent
  EXPECT_FALSE(util::MessageDifferencer::Equals(msg1, msg2));
  EXPECT_FALSE(util::MessageDifferencer::Equivalent(msg1, msg2));
  EXPECT_FALSE(util::MessageDifferencer::ApproximatelyEquals(msg1, msg2));
  EXPECT_TRUE(util::MessageDifferencer::ApproximatelyEquivalent(msg1, msg2));
}

TEST(MessageDifferencerTest, ApproximateInequivalencyTest) {
  // Create the testing protos
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  TestUtil::SetAllFields(&msg1);
  TestUtil::SetAllFields(&msg2);

  // Should fail on equivalency
  msg1.set_optional_int32(-1);
  EXPECT_FALSE(util::MessageDifferencer::ApproximatelyEquivalent(msg1, msg2));

  // Make these fields the same again.
  msg1.set_optional_int32(0);
  msg2.set_optional_int32(0);
  EXPECT_TRUE(util::MessageDifferencer::ApproximatelyEquivalent(msg1, msg2));

  // Should fail on approximate equality check
  const float v1 = 2.3f;
  const float v2 = 9.3f;
  msg1.set_optional_float(v1);
  msg2.set_optional_float(v2);
  EXPECT_FALSE(util::MessageDifferencer::ApproximatelyEquivalent(msg1, msg2));
}

TEST(MessageDifferencerTest, WithinFractionOrMarginFloatTest) {
  // Create the testing protos
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  TestUtil::SetAllFields(&msg1);
  TestUtil::SetAllFields(&msg2);

  // Should fail on approximate equality check
  const float v1 = 100.0f;
  const float v2 = 109.9f;
  msg1.set_optional_float(v1);
  msg2.set_optional_float(v2);

  // Compare
  util::MessageDifferencer differencer;
  EXPECT_FALSE(differencer.Compare(msg1, msg2));
  const FieldDescriptor* fd =
      msg1.GetDescriptor()->FindFieldByName("optional_float");

  // Set float comparison to exact, margin and fraction value should not matter.
  differencer.set_float_comparison(util::MessageDifferencer::EXACT);
  // Set margin for float comparison.
  differencer.SetFractionAndMargin(fd, 0.0, 10.0);
  EXPECT_FALSE(differencer.Compare(msg1, msg2));

  // Margin and fraction float comparison is activated when float comparison is
  // set to approximate.
  differencer.set_float_comparison(util::MessageDifferencer::APPROXIMATE);
  EXPECT_TRUE(differencer.Compare(msg1, msg2));

  // Test out float comparison with fraction.
  differencer.SetFractionAndMargin(fd, 0.2, 0.0);
  EXPECT_TRUE(differencer.Compare(msg1, msg2));

  // Should fail since the fraction is smaller than error.
  differencer.SetFractionAndMargin(fd, 0.01, 0.0);
  EXPECT_FALSE(differencer.Compare(msg1, msg2));

  // Should pass if either fraction or margin are satisfied.
  differencer.SetFractionAndMargin(fd, 0.01, 10.0);
  EXPECT_TRUE(differencer.Compare(msg1, msg2));

  // Make sure that the margin and fraction only affects the field that it was
  // set for.
  msg1.set_default_float(v1);
  msg2.set_default_float(v2);
  EXPECT_FALSE(differencer.Compare(msg1, msg2));
  msg1.set_default_float(v1);
  msg2.set_default_float(v1);
  EXPECT_TRUE(differencer.Compare(msg1, msg2));
}

TEST(MessageDifferencerTest, WithinFractionOrMarginDoubleTest) {
  // Create the testing protos
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  TestUtil::SetAllFields(&msg1);
  TestUtil::SetAllFields(&msg2);

  // Should fail on approximate equality check
  const double v1 = 100.0;
  const double v2 = 109.9;
  msg1.set_optional_double(v1);
  msg2.set_optional_double(v2);

  // Compare
  util::MessageDifferencer differencer;
  EXPECT_FALSE(differencer.Compare(msg1, msg2));

  // Set comparison to exact, margin and fraction value should not matter.
  differencer.set_float_comparison(util::MessageDifferencer::EXACT);
  // Set margin for float comparison.
  const FieldDescriptor* fd =
      msg1.GetDescriptor()->FindFieldByName("optional_double");
  differencer.SetFractionAndMargin(fd, 0.0, 10.0);
  EXPECT_FALSE(differencer.Compare(msg1, msg2));

  // Margin and fraction comparison is activated when float comparison is
  // set to approximate.
  differencer.set_float_comparison(util::MessageDifferencer::APPROXIMATE);
  EXPECT_TRUE(differencer.Compare(msg1, msg2));

  // Test out comparison with fraction.
  differencer.SetFractionAndMargin(fd, 0.2, 0.0);
  EXPECT_TRUE(differencer.Compare(msg1, msg2));

  // Should fail since the fraction is smaller than error.
  differencer.SetFractionAndMargin(fd, 0.01, 0.0);
  EXPECT_FALSE(differencer.Compare(msg1, msg2));

  // Should pass if either fraction or margin are satisfied.
  differencer.SetFractionAndMargin(fd, 0.01, 10.0);
  EXPECT_TRUE(differencer.Compare(msg1, msg2));

  // Make sure that the margin and fraction only affects the field that it was
  // set for.
  msg1.set_default_double(v1);
  msg2.set_default_double(v2);
  EXPECT_FALSE(differencer.Compare(msg1, msg2));
  msg1.set_default_double(v1);
  msg2.set_default_double(v1);
  EXPECT_TRUE(differencer.Compare(msg1, msg2));
}

TEST(MessageDifferencerTest, WithinDefaultFractionOrMarginDoubleTest) {
  // Create the testing protos
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  TestUtil::SetAllFields(&msg1);
  TestUtil::SetAllFields(&msg2);

  // Should fail on approximate equality check
  const double v1 = 100.0;
  const double v2 = 109.9;
  msg1.set_optional_double(v1);
  msg2.set_optional_double(v2);

  util::MessageDifferencer differencer;

  // Compare
  EXPECT_FALSE(differencer.Compare(msg1, msg2));

  // Set up a custom field comparitor, with a default fraction and margin for
  // float and double comparison.
  util::DefaultFieldComparator field_comparitor;
  field_comparitor.SetDefaultFractionAndMargin(0.0, 10.0);
  differencer.set_field_comparator(&field_comparitor);

  // Set comparison to exact, margin and fraction value should not matter.
  field_comparitor.set_float_comparison(util::DefaultFieldComparator::EXACT);
  EXPECT_FALSE(differencer.Compare(msg1, msg2));

  // Margin and fraction comparison is activated when float comparison is
  // set to approximate.
  field_comparitor.set_float_comparison(
      util::DefaultFieldComparator::APPROXIMATE);
  EXPECT_TRUE(differencer.Compare(msg1, msg2));

  // Test out comparison with fraction.
  field_comparitor.SetDefaultFractionAndMargin(0.2, 0.0);
  EXPECT_TRUE(differencer.Compare(msg1, msg2));

  // Should fail since the fraction is smaller than error.
  field_comparitor.SetDefaultFractionAndMargin(0.01, 0.0);
  EXPECT_FALSE(differencer.Compare(msg1, msg2));

  // Should pass if either fraction or margin are satisfied.
  field_comparitor.SetDefaultFractionAndMargin(0.01, 10.0);
  EXPECT_TRUE(differencer.Compare(msg1, msg2));

  // Make sure that the default margin and fraction affects all fields
  msg1.set_default_double(v1);
  msg2.set_default_double(v2);
  EXPECT_TRUE(differencer.Compare(msg1, msg2));
}

TEST(MessageDifferencerTest, BasicFieldOrderingsTest) {
  // Create the testing protos
  unittest::TestFieldOrderings msg1;
  unittest::TestFieldOrderings msg2;

  TestUtil::SetAllFieldsAndExtensions(&msg1);
  TestUtil::SetAllFieldsAndExtensions(&msg2);

  // Compare
  EXPECT_TRUE(util::MessageDifferencer::Equals(msg1, msg2));
}

TEST(MessageDifferencerTest, BasicFieldOrderingInequalityTest) {
  // Create the testing protos
  unittest::TestFieldOrderings msg1;
  unittest::TestFieldOrderings msg2;

  TestUtil::SetAllFieldsAndExtensions(&msg1);
  TestUtil::SetAllFieldsAndExtensions(&msg2);

  msg1.set_my_float(15.00);
  msg2.set_my_float(16.00);

  // Compare
  EXPECT_FALSE(util::MessageDifferencer::Equals(msg1, msg2));
}

TEST(MessageDifferencerTest, BasicExtensionTest) {
  // Create the testing protos
  unittest::TestAllExtensions msg1;
  unittest::TestAllExtensions msg2;

  TestUtil::SetAllExtensions(&msg1);
  TestUtil::SetAllExtensions(&msg2);

  // Compare
  EXPECT_TRUE(util::MessageDifferencer::Equals(msg1, msg2));
}

TEST(MessageDifferencerTest, BasicExtensionInequalityTest) {
  // Create the testing protos
  unittest::TestAllExtensions msg1;
  unittest::TestAllExtensions msg2;

  TestUtil::SetAllExtensions(&msg1);
  TestUtil::SetAllExtensions(&msg2);

  msg1.SetExtension(unittest::optional_int32_extension, 101);
  msg2.SetExtension(unittest::optional_int32_extension, 102);

  // Compare
  EXPECT_FALSE(util::MessageDifferencer::Equals(msg1, msg2));
}

TEST(MessageDifferencerTest, OneofTest) {
  // Create the testing protos
  unittest::TestOneof2 msg1;
  unittest::TestOneof2 msg2;

  TestUtil::SetOneof1(&msg1);
  TestUtil::SetOneof1(&msg2);

  // Compare
  EXPECT_TRUE(util::MessageDifferencer::Equals(msg1, msg2));
}

TEST(MessageDifferencerTest, OneofInequalityTest) {
  // Create the testing protos
  unittest::TestOneof2 msg1;
  unittest::TestOneof2 msg2;

  TestUtil::SetOneof1(&msg1);
  TestUtil::SetOneof2(&msg2);

  // Compare
  EXPECT_FALSE(util::MessageDifferencer::Equals(msg1, msg2));
}

TEST(MessageDifferencerTest, UnknownFieldPartialEqualTest) {
  unittest::TestEmptyMessage empty1;
  unittest::TestEmptyMessage empty2;

  UnknownFieldSet* unknown1 = empty1.mutable_unknown_fields();
  UnknownFieldSet* unknown2 = empty2.mutable_unknown_fields();

  unknown1->AddVarint(243, 122);
  unknown1->AddLengthDelimited(245, "abc");
  unknown1->AddGroup(246)->AddFixed32(248, 1);
  unknown1->mutable_field(2)->mutable_group()->AddFixed32(248, 2);

  unknown2->AddVarint(243, 122);
  unknown2->AddLengthDelimited(245, "abc");
  unknown2->AddGroup(246)->AddFixed32(248, 1);
  unknown2->mutable_field(2)->mutable_group()->AddFixed32(248, 2);

  util::MessageDifferencer differencer;
  differencer.set_scope(util::MessageDifferencer::PARTIAL);
  EXPECT_TRUE(differencer.Compare(empty1, empty2));
}

TEST(MessageDifferencerTest, SpecifiedFieldsEqualityAllTest) {
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  TestUtil::SetAllFields(&msg1);
  TestUtil::SetAllFields(&msg2);

  std::vector<const FieldDescriptor*> fields1;
  std::vector<const FieldDescriptor*> fields2;
  msg1.GetReflection()->ListFields(msg1, &fields1);
  msg2.GetReflection()->ListFields(msg2, &fields2);

  util::MessageDifferencer differencer;
  EXPECT_TRUE(differencer.CompareWithFields(msg1, msg2, fields1, fields2));
}

TEST(MessageDifferencerTest, SpecifiedFieldsInequalityAllTest) {
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  TestUtil::SetAllFields(&msg1);

  std::vector<const FieldDescriptor*> fields1;
  std::vector<const FieldDescriptor*> fields2;
  msg1.GetReflection()->ListFields(msg1, &fields1);
  msg2.GetReflection()->ListFields(msg2, &fields2);

  util::MessageDifferencer differencer;
  EXPECT_FALSE(differencer.CompareWithFields(msg1, msg2, fields1, fields2));
}

TEST(MessageDifferencerTest, SpecifiedFieldsEmptyListAlwaysSucceeds) {
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  TestUtil::SetAllFields(&msg1);

  std::vector<const FieldDescriptor*> empty_fields;

  util::MessageDifferencer differencer;
  EXPECT_TRUE(
      differencer.CompareWithFields(msg1, msg2, empty_fields, empty_fields));

  TestUtil::SetAllFields(&msg2);
  EXPECT_TRUE(
      differencer.CompareWithFields(msg1, msg2, empty_fields, empty_fields));
}

TEST(MessageDifferencerTest, SpecifiedFieldsCompareWithSelf) {
  unittest::TestAllTypes msg1;
  TestUtil::SetAllFields(&msg1);

  std::vector<const FieldDescriptor*> fields;
  msg1.GetReflection()->ListFields(msg1, &fields);

  util::MessageDifferencer differencer;
  EXPECT_TRUE(differencer.CompareWithFields(msg1, msg1, fields, fields));

  {
    // Compare with a subset of fields.
    std::vector<const FieldDescriptor*> compare_fields;
    for (int i = 0; i < fields.size(); ++i) {
      if (i % 2 == 0) {
        compare_fields.push_back(fields[i]);
      }
    }
    EXPECT_TRUE(differencer.CompareWithFields(msg1, msg1, compare_fields,
                                              compare_fields));
  }
  {
    // Specify a different set of fields to compare, even though we're using the
    // same message. This should fail, since we are explicitly saying that the
    // set of fields are different.
    std::vector<const FieldDescriptor*> compare_fields1;
    std::vector<const FieldDescriptor*> compare_fields2;
    for (int i = 0; i < fields.size(); ++i) {
      if (i % 2 == 0) {
        compare_fields1.push_back(fields[i]);
      } else {
        compare_fields2.push_back(fields[i]);
      }
    }
    EXPECT_FALSE(differencer.CompareWithFields(msg1, msg1, compare_fields1,
                                               compare_fields2));
  }
}

TEST(MessageDifferencerTest, SpecifiedFieldsEqualityAllShuffledTest) {
  // This is a public function, so make sure there are no assumptions about the
  // list of fields. Randomly shuffle them to make sure that they are properly
  // ordered for comparison.
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  TestUtil::SetAllFields(&msg1);
  TestUtil::SetAllFields(&msg2);

  std::vector<const FieldDescriptor*> fields1;
  std::vector<const FieldDescriptor*> fields2;
  msg1.GetReflection()->ListFields(msg1, &fields1);
  msg2.GetReflection()->ListFields(msg2, &fields2);

  std::default_random_engine rng;
  std::shuffle(fields1.begin(), fields1.end(), rng);
  std::shuffle(fields2.begin(), fields2.end(), rng);

  util::MessageDifferencer differencer;
  EXPECT_TRUE(differencer.CompareWithFields(msg1, msg2, fields1, fields2));
}

TEST(MessageDifferencerTest, SpecifiedFieldsSubsetEqualityTest) {
  // Specify a set of fields to compare. All the fields are equal.
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;
  TestUtil::SetAllFields(&msg1);
  TestUtil::SetAllFields(&msg2);

  std::vector<const FieldDescriptor*> fields1;
  msg1.GetReflection()->ListFields(msg1, &fields1);

  std::vector<const FieldDescriptor*> compare_fields;
  // Only compare the field descriptors with even indices.
  for (int i = 0; i < fields1.size(); ++i) {
    if (i % 2 == 0) {
      compare_fields.push_back(fields1[i]);
    }
  }

  util::MessageDifferencer differencer;
  EXPECT_TRUE(differencer.CompareWithFields(msg1, msg2, compare_fields,
                                            compare_fields));
}

TEST(MessageDifferencerTest,
     SpecifiedFieldsSubsetIgnoresOtherFieldDifferencesTest) {
  // Specify a set of fields to compare, but clear all the other fields in one
  // of the messages. This should fail a regular compare, but CompareWithFields
  // should succeed.
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;
  TestUtil::SetAllFields(&msg1);
  TestUtil::SetAllFields(&msg2);

  std::vector<const FieldDescriptor*> fields1;
  const Reflection* reflection = msg1.GetReflection();
  reflection->ListFields(msg1, &fields1);

  std::vector<const FieldDescriptor*> compare_fields;
  // Only compare the field descriptors with even indices.
  for (int i = 0; i < fields1.size(); ++i) {
    if (i % 2 == 0) {
      compare_fields.push_back(fields1[i]);
    } else {
      reflection->ClearField(&msg2, fields1[i]);
    }
  }

  util::MessageDifferencer differencer;
  EXPECT_FALSE(differencer.Compare(msg1, msg2));
  EXPECT_TRUE(differencer.CompareWithFields(msg1, msg2, compare_fields,
                                            compare_fields));
}

TEST(MessageDifferencerTest, SpecifiedFieldsDetectsDifferencesTest) {
  // Change all of the repeated fields in one of the messages, and use only
  // those fields for comparison.
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;
  TestUtil::SetAllFields(&msg1);
  TestUtil::SetAllFields(&msg2);
  TestUtil::ModifyRepeatedFields(&msg2);

  std::vector<const FieldDescriptor*> fields1;
  msg1.GetReflection()->ListFields(msg1, &fields1);

  std::vector<const FieldDescriptor*> compare_fields;
  // Only compare the repeated field descriptors.
  for (int i = 0; i < fields1.size(); ++i) {
    if (fields1[i]->is_repeated()) {
      compare_fields.push_back(fields1[i]);
    }
  }

  util::MessageDifferencer differencer;
  EXPECT_FALSE(differencer.CompareWithFields(msg1, msg2, compare_fields,
                                             compare_fields));
}

TEST(MessageDifferencerTest, SpecifiedFieldsEquivalenceAllTest) {
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;

  TestUtil::SetAllFields(&msg1);
  TestUtil::SetAllFields(&msg2);

  std::vector<const FieldDescriptor*> fields1;
  std::vector<const FieldDescriptor*> fields2;
  msg1.GetReflection()->ListFields(msg1, &fields1);
  msg2.GetReflection()->ListFields(msg2, &fields2);

  util::MessageDifferencer differencer;
  differencer.set_message_field_comparison(
      util::MessageDifferencer::EQUIVALENT);
  EXPECT_TRUE(differencer.CompareWithFields(msg1, msg2, fields1, fields2));
}

TEST(MessageDifferencerTest,
     SpecifiedFieldsEquivalenceIgnoresOtherFieldDifferencesTest) {
  unittest::TestAllTypes msg1;
  unittest::TestAllTypes msg2;
  const Descriptor* desc = msg1.GetDescriptor();

  const FieldDescriptor* optional_int32_desc =
      desc->FindFieldByName("optional_int32");
  const FieldDescriptor* optional_int64_desc =
      desc->FindFieldByName("optional_int64");
  const FieldDescriptor* default_int64_desc =
      desc->FindFieldByName("default_int64");
  ASSERT_TRUE(optional_int32_desc != NULL);
  ASSERT_TRUE(optional_int64_desc != NULL);
  ASSERT_TRUE(default_int64_desc != NULL);
  msg1.set_optional_int32(0);
  msg2.set_optional_int64(0);
  msg1.set_default_int64(default_int64_desc->default_value_int64());

  // Set a field to a non-default value so we know that field selection is
  // actually doing something.
  msg2.set_optional_uint64(23);

  std::vector<const FieldDescriptor*> fields1;
  std::vector<const FieldDescriptor*> fields2;
  fields1.push_back(optional_int32_desc);
  fields1.push_back(default_int64_desc);

  fields2.push_back(optional_int64_desc);

  util::MessageDifferencer differencer;
  EXPECT_FALSE(differencer.CompareWithFields(msg1, msg2, fields1, fields2));
  differencer.set_message_field_comparison(
      util::MessageDifferencer::EQUIVALENT);
  EXPECT_FALSE(differencer.Compare(msg1, msg2));
  EXPECT_TRUE(differencer.CompareWithFields(msg1, msg2, fields1, fields2));
}

TEST(MessageDifferencerTest, RepeatedFieldSmartListTest) {
  // Create the testing protos
  protobuf_unittest::TestDiffMessage msg1;
  protobuf_unittest::TestDiffMessage msg2;

  msg1.add_rv(1);
  msg1.add_rv(2);
  msg1.add_rv(3);
  msg1.add_rv(9);
  msg1.add_rv(4);
  msg1.add_rv(5);
  msg1.add_rv(7);
  msg1.add_rv(2);
  msg2.add_rv(9);
  msg2.add_rv(0);
  msg2.add_rv(2);
  msg2.add_rv(7);
  msg2.add_rv(3);
  msg2.add_rv(4);
  msg2.add_rv(5);
  msg2.add_rv(6);
  msg2.add_rv(2);
  // Compare
  // a: 1,      2,    3, 9, 4, 5, 7,   2
  // b:   9, 0, 2, 7, 3,    4, 5,   6, 2
  std::string diff_report;
  util::MessageDifferencer differencer;
  differencer.ReportDifferencesToString(&diff_report);
  differencer.set_repeated_field_comparison(
      util::MessageDifferencer::AS_SMART_LIST);
  EXPECT_FALSE(differencer.Compare(msg1, msg2));
  EXPECT_EQ(
      "deleted: rv[0]: 1\n"
      "added: rv[0]: 9\n"
      "added: rv[1]: 0\n"
      "moved: rv[1] -> rv[2] : 2\n"
      "added: rv[3]: 7\n"
      "moved: rv[2] -> rv[4] : 3\n"
      "deleted: rv[3]: 9\n"
      "moved: rv[4] -> rv[5] : 4\n"
      "moved: rv[5] -> rv[6] : 5\n"
      "deleted: rv[6]: 7\n"
      "added: rv[7]: 6\n"
      "moved: rv[7] -> rv[8] : 2\n",
      diff_report);

  // Compare two sub messages
  // a: 1, 2, 3,    4, 5
  // b:    2,    6, 4,
  msg1.Clear();
  msg2.Clear();
  msg1.add_rm()->set_a(1);
  msg1.add_rm()->set_a(2);
  msg1.add_rm()->set_a(3);
  msg1.add_rm()->set_a(4);
  msg1.add_rm()->set_a(5);
  msg2.add_rm()->set_a(2);
  msg2.add_rm()->set_a(6);
  msg2.add_rm()->set_a(4);
  differencer.ReportDifferencesToString(&diff_report);
  differencer.set_repeated_field_comparison(
      util::MessageDifferencer::AS_SMART_LIST);
  EXPECT_FALSE(differencer.Compare(msg1, msg2));
  EXPECT_EQ(
      "deleted: rm[0]: { a: 1 }\n"
      "moved: rm[1] -> rm[0] : { a: 2 }\n"
      "deleted: rm[2]: { a: 3 }\n"
      "added: rm[1]: { a: 6 }\n"
      "moved: rm[3] -> rm[2] : { a: 4 }\n"
      "deleted: rm[4]: { a: 5 }\n",
      diff_report);
}

TEST(MessageDifferencerTest, RepeatedFieldSmartSetTest) {
  // Create the testing protos
  protobuf_unittest::TestDiffMessage msg1;
  protobuf_unittest::TestDiffMessage msg2;
  protobuf_unittest::TestField elem1_1, elem2_1, elem3_1;
  protobuf_unittest::TestField elem1_2, elem2_2, elem3_2;

  // Only one field is different for each pair of elememts
  elem1_1.set_a(1);
  elem1_2.set_a(0);
  elem1_1.set_b(1);
  elem1_2.set_b(1);
  elem1_1.set_c(1);
  elem1_2.set_c(1);
  elem2_1.set_a(2);
  elem2_2.set_a(2);
  elem2_1.set_b(2);
  elem2_2.set_b(0);
  elem2_1.set_c(2);
  elem2_2.set_c(2);
  elem3_1.set_a(3);
  elem3_2.set_a(3);
  elem3_1.set_b(3);
  elem3_2.set_b(0);
  elem3_1.set_c(3);
  elem3_2.set_c(3);

  *msg1.add_rm() = elem1_1;
  *msg1.add_rm() = elem2_1;
  *msg1.add_rm() = elem3_1;
  // Change the order of those elements for the second message.
  *msg2.add_rm() = elem3_2;
  *msg2.add_rm() = elem1_2;
  *msg2.add_rm() = elem2_2;

  std::string diff_report;
  util::MessageDifferencer differencer;
  differencer.ReportDifferencesToString(&diff_report);
  differencer.set_repeated_field_comparison(
      util::MessageDifferencer::AS_SMART_SET);
  EXPECT_FALSE(differencer.Compare(msg1, msg2));
  EXPECT_EQ(
      "modified: rm[0].a -> rm[1].a: 1 -> 0\n"
      "modified: rm[1].b -> rm[2].b: 2 -> 0\n"
      "modified: rm[2].b -> rm[0].b: 3 -> 0\n",
      diff_report);
}

TEST(MessageDifferencerTest, RepeatedFieldSmartSetTest_IdenticalElements) {
  // Create the testing protos
  protobuf_unittest::TestDiffMessage msg1;
  protobuf_unittest::TestDiffMessage msg2;
  protobuf_unittest::TestField elem;

  elem.set_a(1);
  elem.set_b(1);
  elem.set_c(1);

  *msg1.add_rm() = elem;
  *msg1.add_rm() = elem;
  *msg2.add_rm() = elem;
  *msg2.add_rm() = elem;

  util::MessageDifferencer differencer;
  differencer.set_repeated_field_comparison(
      util::MessageDifferencer::AS_SMART_SET);
  EXPECT_TRUE(differencer.Compare(msg1, msg2));
}

TEST(MessageDifferencerTest, RepeatedFieldSmartSetTest_PreviouslyMatch) {
  // Create the testing protos
  protobuf_unittest::TestDiffMessage msg1;
  protobuf_unittest::TestDiffMessage msg2;
  protobuf_unittest::TestField elem1_1, elem1_2;
  protobuf_unittest::TestField elem2_1, elem2_2;

  elem1_1.set_a(1);
  elem1_1.set_b(1);
  elem1_1.set_c(1);
  elem1_2.set_a(1);
  elem1_2.set_b(1);
  elem1_2.set_c(0);

  elem2_1.set_a(1);
  elem2_1.set_b(1);
  elem2_1.set_c(1);
  elem2_2.set_a(1);
  elem2_2.set_b(0);
  elem2_2.set_c(1);

  *msg1.add_rm() = elem1_1;
  *msg1.add_rm() = elem2_1;
  *msg2.add_rm() = elem1_2;
  *msg2.add_rm() = elem2_2;

  string diff_report;
  util::MessageDifferencer differencer;
  differencer.ReportDifferencesToString(&diff_report);
  differencer.set_repeated_field_comparison(
      util::MessageDifferencer::AS_SMART_SET);
  EXPECT_FALSE(differencer.Compare(msg1, msg2));
  EXPECT_EQ(
      "modified: rm[0].c: 1 -> 0\n"
      "modified: rm[1].b: 1 -> 0\n",
      diff_report);
}

TEST(MessageDifferencerTest, RepeatedFieldSmartSet_MultipleMatches) {
  // Create the testing protos
  protobuf_unittest::TestDiffMessage msg1;
  protobuf_unittest::TestDiffMessage msg2;
  protobuf_unittest::TestField elem1_1, elem2_1, elem3_1;
  protobuf_unittest::TestField elem2_2, elem3_2;

  // Only one field is different for each pair of elememts
  elem1_1.set_a(1);
  elem1_1.set_b(1);
  elem1_1.set_c(1);
  elem2_1.set_a(2);
  elem2_2.set_a(2);
  elem2_1.set_b(2);
  elem2_2.set_b(0);
  elem2_1.set_c(2);
  elem2_2.set_c(2);
  elem3_1.set_a(3);
  elem3_2.set_a(3);
  elem3_1.set_b(3);
  elem3_2.set_b(0);
  elem3_1.set_c(3);
  elem3_2.set_c(3);

  // In this testcase, elem1_1 will match with elem2_2 first and then get
  // reverted because elem2_1 matches with elem2_2 later.
  *msg1.add_rm() = elem1_1;
  *msg1.add_rm() = elem2_1;
  *msg1.add_rm() = elem3_1;
  *msg2.add_rm() = elem2_2;
  *msg2.add_rm() = elem3_2;

  std::string diff_report;
  util::MessageDifferencer differencer;
  differencer.ReportDifferencesToString(&diff_report);
  differencer.set_repeated_field_comparison(
      util::MessageDifferencer::AS_SMART_SET);
  EXPECT_FALSE(differencer.Compare(msg1, msg2));
  EXPECT_EQ(
      "modified: rm[1].b -> rm[0].b: 2 -> 0\n"
      "modified: rm[2].b -> rm[1].b: 3 -> 0\n"
      "deleted: rm[0]: { c: 1 a: 1 b: 1 }\n",
      diff_report);
}

TEST(MessageDifferencerTest, RepeatedFieldSmartSet_MultipleMatchesNoReporter) {
  protobuf_unittest::TestDiffMessage msg1;
  protobuf_unittest::TestDiffMessage msg2;
  protobuf_unittest::TestField elem1, elem2, elem3, elem4;
  elem1.set_a(1);
  elem2.set_a(2);
  elem3.set_a(3);
  elem4.set_a(4);

  *msg1.add_rm() = elem1;
  *msg1.add_rm() = elem2;
  *msg1.add_rm() = elem3;
  *msg2.add_rm() = elem2;
  *msg2.add_rm() = elem3;
  *msg2.add_rm() = elem4;

  util::MessageDifferencer differencer;
  differencer.set_repeated_field_comparison(
      util::MessageDifferencer::AS_SMART_SET);
  EXPECT_FALSE(differencer.Compare(msg1, msg2));
}

TEST(MessageDifferencerTest, RepeatedFieldSmartSet_NonMessageTypeTest) {
  // Create the testing protos
  protobuf_unittest::TestDiffMessage msg1;
  protobuf_unittest::TestDiffMessage msg2;

  // Create 3 elements, but in different order.
  msg1.add_rw("b");
  msg2.add_rw("a");
  msg1.add_rw("x");
  msg2.add_rw("x");
  msg1.add_rw("a");
  msg2.add_rw("b");

  std::string diff_report;
  util::MessageDifferencer differencer;
  differencer.ReportDifferencesToString(&diff_report);
  differencer.set_repeated_field_comparison(
      util::MessageDifferencer::AS_SMART_SET);
  EXPECT_TRUE(differencer.Compare(msg1, msg2));
  EXPECT_EQ(
      "moved: rw[0] -> rw[2] : \"b\"\n"
      "moved: rw[2] -> rw[0] : \"a\"\n",
      diff_report);
}

TEST(MessageDifferencerTest, RepeatedFieldSetTest_SetOfSet) {
  // Create the testing protos
  protobuf_unittest::TestDiffMessage msg1;
  protobuf_unittest::TestDiffMessage msg2;

  protobuf_unittest::TestDiffMessage::Item* item = msg1.add_item();
  item->add_ra(1);
  item->add_ra(2);
  item->add_ra(3);
  item = msg1.add_item();
  item->add_ra(5);
  item->add_ra(6);
  item = msg1.add_item();
  item->add_ra(1);
  item->add_ra(3);
  item = msg1.add_item();
  item->add_ra(6);
  item->add_ra(7);
  item->add_ra(8);

  item = msg2.add_item();
  item->add_ra(6);
  item->add_ra(5);
  item = msg2.add_item();
  item->add_ra(6);
  item->add_ra(8);
  item->add_ra(7);
  item = msg2.add_item();
  item->add_ra(1);
  item->add_ra(3);
  item = msg2.add_item();
  item->add_ra(3);
  item->add_ra(2);
  item->add_ra(1);

  // Compare
  util::MessageDifferencer differencer;
  differencer.set_repeated_field_comparison(util::MessageDifferencer::AS_SET);
  EXPECT_TRUE(differencer.Compare(msg1, msg2));
}

TEST(MessageDifferencerTest, RepeatedFieldSetTest_Combination) {
  // Create the testing protos
  protobuf_unittest::TestDiffMessage msg1;
  protobuf_unittest::TestDiffMessage msg2;
  // Treat "item" as Map, with key = "a"
  // Treat "item.ra" also as Set
  // Treat "rv" as Set
  // Treat "rw" as List
  protobuf_unittest::TestDiffMessage::Item* item = msg1.add_item();
  item->set_a(3);
  item->add_ra(1);
  item->add_ra(2);
  item->add_ra(3);
  item = msg1.add_item();
  item->set_a(4);
  item->add_ra(5);
  item->add_ra(6);
  item = msg1.add_item();
  item->set_a(1);
  item->add_ra(1);
  item->add_ra(3);
  item = msg1.add_item();
  item->set_a(2);
  item->add_ra(6);
  item->add_ra(7);
  item->add_ra(8);

  item = msg2.add_item();
  item->set_a(4);
  item->add_ra(6);
  item->add_ra(5);
  item = msg2.add_item();
  item->set_a(2);
  item->add_ra(6);
  item->add_ra(8);
  item->add_ra(7);
  item = msg2.add_item();
  item->set_a(1);
  item->add_ra(1);
  item->add_ra(3);
  item = msg2.add_item();
  item->set_a(3);
  item->add_ra(3);
  item->add_ra(2);
  item->add_ra(1);

  msg1.add_rv(3);
  msg1.add_rv(4);
  msg1.add_rv(7);
  msg1.add_rv(0);
  msg2.add_rv(4);
  msg2.add_rv(3);
  msg2.add_rv(0);
  msg2.add_rv(7);

  msg1.add_rw("nothing");
  msg2.add_rw("nothing");
  msg1.add_rw("should");
  msg2.add_rw("should");
  msg1.add_rw("change");
  msg2.add_rw("change");

  // Compare
  util::MessageDifferencer differencer1;
  differencer1.TreatAsMap(msg1.GetDescriptor()->FindFieldByName("item"),
                          item->GetDescriptor()->FindFieldByName("a"));
  differencer1.TreatAsSet(msg1.GetDescriptor()->FindFieldByName("rv"));
  differencer1.TreatAsSet(item->GetDescriptor()->FindFieldByName("ra"));
  EXPECT_TRUE(differencer1.Compare(msg1, msg2));

  util::MessageDifferencer differencer2;
  differencer2.TreatAsMap(msg1.GetDescriptor()->FindFieldByName("item"),
                          item->GetDescriptor()->FindFieldByName("a"));
  differencer2.set_repeated_field_comparison(util::MessageDifferencer::AS_SET);
  differencer2.TreatAsList(msg1.GetDescriptor()->FindFieldByName("rw"));
  EXPECT_TRUE(differencer2.Compare(msg1, msg2));
}

TEST(MessageDifferencerTest, RepeatedFieldMapTest_Partial) {
  protobuf_unittest::TestDiffMessage msg1;
  // message msg1 {
  //   item { a: 1; b: "11" }
  // }
  protobuf_unittest::TestDiffMessage::Item* item = msg1.add_item();
  item->set_a(1);
  item->set_b("11");

  protobuf_unittest::TestDiffMessage msg2;
  // message msg2 {
  //   item { a: 2; b: "22" }
  //   item { a: 1; b: "11" }
  // }
  item = msg2.add_item();
  item->set_a(2);
  item->set_b("22");
  item = msg2.add_item();
  item->set_a(1);
  item->set_b("11");

  // Compare
  util::MessageDifferencer differencer;
  differencer.TreatAsMap(GetFieldDescriptor(msg1, "item"),
                         GetFieldDescriptor(msg1, "item.a"));
  differencer.set_scope(util::MessageDifferencer::PARTIAL);
  EXPECT_TRUE(differencer.Compare(msg1, msg2));
}

TEST(MessageDifferencerTest, RepeatedFieldSetTest_Duplicates) {
  protobuf_unittest::TestDiffMessage a, b, c;
  // message a: {
  //   rv: 0
  //   rv: 1
  //   rv: 0
  // }
  a.add_rv(0);
  a.add_rv(1);
  a.add_rv(0);
  // message b: {
  //   rv: 0
  //   rv: 0
  //   rv: 1
  // }
  b.add_rv(0);
  b.add_rv(0);
  b.add_rv(1);
  // message c: {
  //   rv: 0
  //   rv: 1
  // }
  c.add_rv(0);
  c.add_rv(1);
  util::MessageDifferencer differencer;
  differencer.TreatAsSet(GetFieldDescriptor(a, "rv"));
  EXPECT_TRUE(differencer.Compare(b, a));
  EXPECT_FALSE(differencer.Compare(c, a));

  util::MessageDifferencer differencer1;
  differencer1.set_repeated_field_comparison(util::MessageDifferencer::AS_SET);
  EXPECT_TRUE(differencer1.Compare(b, a));
  EXPECT_FALSE(differencer1.Compare(c, a));
}

TEST(MessageDifferencerTest, RepeatedFieldSetTest_PartialSimple) {
  protobuf_unittest::TestDiffMessage a, b, c;
  // message a: {
  //   rm { c: 1 }
  //   rm { c: 0 }
  // }
  a.add_rm()->set_c(1);
  a.add_rm()->set_c(0);
  // message b: {
  //   rm { c: 1 }
  //   rm {}
  // }
  b.add_rm()->set_c(1);
  b.add_rm();
  // message c: {
  //   rm {}
  //   rm { c: 1 }
  // }
  c.add_rm();
  c.add_rm()->set_c(1);
  util::MessageDifferencer differencer;
  differencer.set_scope(util::MessageDifferencer::PARTIAL);
  differencer.TreatAsSet(GetFieldDescriptor(a, "rm"));
  EXPECT_TRUE(differencer.Compare(b, a));
  EXPECT_TRUE(differencer.Compare(c, a));
}

TEST(MessageDifferencerTest, RepeatedFieldSetTest_Partial) {
  protobuf_unittest::TestDiffMessage msg1, msg2;
  // message msg1: {
  //   rm { a: 1 }
  //   rm { b: 2 }
  //   rm { c: 3 }
  // }
  msg1.add_rm()->set_a(1);
  msg1.add_rm()->set_b(2);
  msg1.add_rm()->set_c(3);
  // message msg2: {
  //   rm { a: 1; c: 3 }
  //   rm { b: 2; c: 3 }
  //   rm { b: 2 }
  // }
  protobuf_unittest::TestField* field = msg2.add_rm();
  field->set_a(1);
  field->set_c(3);
  field = msg2.add_rm();
  field->set_b(2);
  field->set_c(3);
  field = msg2.add_rm();
  field->set_b(2);

  util::MessageDifferencer differencer;
  differencer.set_scope(util::MessageDifferencer::PARTIAL);
  differencer.TreatAsSet(GetFieldDescriptor(msg1, "rm"));
  EXPECT_TRUE(differencer.Compare(msg1, msg2));
}

TEST(MessageDifferencerTest, RepeatedFieldMapTest_MultipleFieldsAsKey) {
  protobuf_unittest::TestDiffMessage msg1;
  protobuf_unittest::TestDiffMessage msg2;
  // Treat "item" as Map, with key = ("a", "ra")
  // Treat "item.ra" as Set
  protobuf_unittest::TestDiffMessage::Item* item = msg1.add_item();
  // key => value: (1, {2, 3}) => "a"
  item->set_a(1);
  item->add_ra(2);
  item->add_ra(3);
  item->set_b("a");
  item = msg1.add_item();
  // key => value: (2, {1, 3}) => "b"
  item->set_a(2);
  item->add_ra(1);
  item->add_ra(3);
  item->set_b("b");
  item = msg1.add_item();
  // key => value: (1, {1, 3}) => "c"
  item->set_a(1);
  item->add_ra(1);
  item->add_ra(3);
  item->set_b("c");

  item = msg2.add_item();
  // key => value: (1, {1, 3}) => "c"
  item->set_a(1);
  item->add_ra(3);
  item->add_ra(1);
  item->set_b("c");
  item = msg2.add_item();
  // key => value: (1, {2, 3}) => "a"
  item->set_a(1);
  item->add_ra(3);
  item->add_ra(2);
  item->set_b("a");
  item = msg2.add_item();
  // key => value: (2, {1, 3}) => "b"
  item->set_a(2);
  item->add_ra(3);
  item->add_ra(1);
  item->set_b("b");

  // Compare
  util::MessageDifferencer differencer;
  differencer.TreatAsSet(GetFieldDescriptor(msg1, "item.ra"));
  EXPECT_FALSE(differencer.Compare(msg1, msg2));
  std::vector<const FieldDescriptor*> key_fields;
  key_fields.push_back(GetFieldDescriptor(msg1, "item.a"));
  key_fields.push_back(GetFieldDescriptor(msg1, "item.ra"));
  differencer.TreatAsMapWithMultipleFieldsAsKey(
      GetFieldDescriptor(msg1, "item"), key_fields);
  EXPECT_TRUE(differencer.Compare(msg1, msg2));

  // Introduce some differences.
  msg1.clear_item();
  msg2.clear_item();
  item = msg1.add_item();
  item->set_a(4);
  item->add_ra(5);
  item->add_ra(6);
  item->set_b("hello");
  item = msg2.add_item();
  item->set_a(4);
  item->add_ra(6);
  item->add_ra(5);
  item->set_b("world");
  std::string output;
  differencer.ReportDifferencesToString(&output);
  EXPECT_FALSE(differencer.Compare(msg1, msg2));
  EXPECT_EQ(
      "moved: item[0].ra[0] -> item[0].ra[1] : 5\n"
      "moved: item[0].ra[1] -> item[0].ra[0] : 6\n"
      "modified: item[0].b: \"hello\" -> \"world\"\n",
      output);
}

TEST(MessageDifferencerTest, RepeatedFieldMapTest_MultipleFieldPathsAsKey) {
  protobuf_unittest::TestDiffMessage msg1;
  protobuf_unittest::TestDiffMessage msg2;
  // Treat "item" as Map, with key = ("m.a", "m.rc")
  // Treat "item.m.rc" as Set
  protobuf_unittest::TestDiffMessage::Item* item = msg1.add_item();
  // key => value: (1, {2, 3}) => "a"
  item->mutable_m()->set_a(1);
  item->mutable_m()->add_rc(2);
  item->mutable_m()->add_rc(3);
  item->set_b("a");
  item = msg1.add_item();
  // key => value: (2, {1, 3}) => "b"
  item->mutable_m()->set_a(2);
  item->mutable_m()->add_rc(1);
  item->mutable_m()->add_rc(3);
  item->set_b("b");
  item = msg1.add_item();
  // key => value: (1, {1, 3}) => "c"
  item->mutable_m()->set_a(1);
  item->mutable_m()->add_rc(1);
  item->mutable_m()->add_rc(3);
  item->set_b("c");

  item = msg2.add_item();
  // key => value: (1, {1, 3}) => "c"
  item->mutable_m()->set_a(1);
  item->mutable_m()->add_rc(3);
  item->mutable_m()->add_rc(1);
  item->set_b("c");
  item = msg2.add_item();
  // key => value: (1, {2, 3}) => "a"
  item->mutable_m()->set_a(1);
  item->mutable_m()->add_rc(3);
  item->mutable_m()->add_rc(2);
  item->set_b("a");
  item = msg2.add_item();
  // key => value: (2, {1, 3}) => "b"
  item->mutable_m()->set_a(2);
  item->mutable_m()->add_rc(3);
  item->mutable_m()->add_rc(1);
  item->set_b("b");

  // Compare
  util::MessageDifferencer differencer;
  differencer.TreatAsSet(GetFieldDescriptor(msg1, "item.m.rc"));
  EXPECT_FALSE(differencer.Compare(msg1, msg2));
  std::vector<std::vector<const FieldDescriptor*> > key_field_paths;
  std::vector<const FieldDescriptor*> key_field_path1;
  key_field_path1.push_back(GetFieldDescriptor(msg1, "item.m"));
  key_field_path1.push_back(GetFieldDescriptor(msg1, "item.m.a"));
  std::vector<const FieldDescriptor*> key_field_path2;
  key_field_path2.push_back(GetFieldDescriptor(msg1, "item.m"));
  key_field_path2.push_back(GetFieldDescriptor(msg1, "item.m.rc"));
  key_field_paths.push_back(key_field_path1);
  key_field_paths.push_back(key_field_path2);
  differencer.TreatAsMapWithMultipleFieldPathsAsKey(
      GetFieldDescriptor(msg1, "item"), key_field_paths);
  EXPECT_TRUE(differencer.Compare(msg1, msg2));

  // Introduce some differences.
  msg1.clear_item();
  msg2.clear_item();
  item = msg1.add_item();
  item->mutable_m()->set_a(4);
  item->mutable_m()->add_rc(5);
  item->mutable_m()->add_rc(6);
  item->set_b("hello");
  item = msg2.add_item();
  item->mutable_m()->set_a(4);
  item->mutable_m()->add_rc(6);
  item->mutable_m()->add_rc(5);
  item->set_b("world");
  std::string output;
  differencer.ReportDifferencesToString(&output);
  EXPECT_FALSE(differencer.Compare(msg1, msg2));
  EXPECT_EQ(
      "modified: item[0].b: \"hello\" -> \"world\"\n"
      "moved: item[0].m.rc[0] -> item[0].m.rc[1] : 5\n"
      "moved: item[0].m.rc[1] -> item[0].m.rc[0] : 6\n",
      output);
}

TEST(MessageDifferencerTest, RepeatedFieldMapTest_IgnoredKeyFields) {
  protobuf_unittest::TestDiffMessage msg1;
  protobuf_unittest::TestDiffMessage msg2;
  // Treat "item" as Map, with key = ("a", "ra")
  protobuf_unittest::TestDiffMessage::Item* item = msg1.add_item();
  item->set_a(1);
  item->add_ra(2);
  item->set_b("hello");
  item = msg2.add_item();
  item->set_a(1);
  item->add_ra(3);
  item->set_b("world");
  // Compare
  util::MessageDifferencer differencer;
  std::vector<const FieldDescriptor*> key_fields;
  key_fields.push_back(GetFieldDescriptor(msg1, "item.a"));
  key_fields.push_back(GetFieldDescriptor(msg1, "item.ra"));
  differencer.TreatAsMapWithMultipleFieldsAsKey(
      GetFieldDescriptor(msg1, "item"), key_fields);
  std::string output;
  differencer.ReportDifferencesToString(&output);
  EXPECT_FALSE(differencer.Compare(msg1, msg2));
  EXPECT_EQ(
      "added: item[0]: { a: 1 ra: 3 b: \"world\" }\n"
      "deleted: item[0]: { a: 1 ra: 2 b: \"hello\" }\n",
      output);
  // Ignored fields that are listed as parts of the key are still used
  // in key comparison, but they're not used in value comparison.
  differencer.IgnoreField(GetFieldDescriptor(msg1, "item.ra"));
  output.clear();
  EXPECT_FALSE(differencer.Compare(msg1, msg2));
  EXPECT_EQ(
      "added: item[0]: { a: 1 ra: 3 b: \"world\" }\n"
      "deleted: item[0]: { a: 1 ra: 2 b: \"hello\" }\n",
      output);
  // Ignoring a field in the key is different from treating the left fields
  // as key. That is:
  //   (key = ("a", "ra") && ignore "ra") != (key = ("a") && ignore "ra")
  util::MessageDifferencer differencer2;
  differencer2.TreatAsMap(GetFieldDescriptor(msg1, "item"),
                          GetFieldDescriptor(msg1, "item.a"));
  differencer2.IgnoreField(GetFieldDescriptor(msg1, "item.ra"));
  output.clear();
  differencer2.ReportDifferencesToString(&output);
  EXPECT_FALSE(differencer2.Compare(msg1, msg2));
  EXPECT_EQ(
      "ignored: item[0].ra\n"
      "modified: item[0].b: \"hello\" -> \"world\"\n",
      output);
}

static const char* const kIgnoredFields[] = {"rm.b", "rm.m.b"};

class TestIgnorer : public util::MessageDifferencer::IgnoreCriteria {
 public:
  virtual bool IsIgnored(
      const Message& message1, const Message& message2,
      const FieldDescriptor* field,
      const std::vector<util::MessageDifferencer::SpecificField>&
          parent_fields) {
    std::string name = "";
    for (int i = 0; i < parent_fields.size(); ++i) {
      name += parent_fields[i].field->name() + ".";
    }
    name += field->name();
    for (int i = 0; i < GOOGLE_ARRAYSIZE(kIgnoredFields); ++i) {
      if (name.compare(kIgnoredFields[i]) == 0) {
        return true;
      }
    }
    return false;
  }
};

TEST(MessageDifferencerTest, TreatRepeatedFieldAsSetWithIgnoredFields) {
  protobuf_unittest::TestDiffMessage msg1;
  protobuf_unittest::TestDiffMessage msg2;
  TextFormat::MergeFromString("rm { a: 11\n b: 12 }", &msg1);
  TextFormat::MergeFromString("rm { a: 11\n b: 13 }", &msg2);
  util::MessageDifferencer differ;
  differ.TreatAsSet(GetFieldDescriptor(msg1, "rm"));
  differ.AddIgnoreCriteria(new TestIgnorer);
  EXPECT_TRUE(differ.Compare(msg1, msg2));
}

TEST(MessageDifferencerTest, TreatRepeatedFieldAsMapWithIgnoredKeyFields) {
  protobuf_unittest::TestDiffMessage msg1;
  protobuf_unittest::TestDiffMessage msg2;
  TextFormat::MergeFromString("rm { a: 11\n m { a: 12\n b: 13\n } }", &msg1);
  TextFormat::MergeFromString("rm { a: 11\n m { a: 12\n b: 14\n } }", &msg2);
  util::MessageDifferencer differ;
  differ.TreatAsMap(GetFieldDescriptor(msg1, "rm"),
                    GetFieldDescriptor(msg1, "rm.m"));
  differ.AddIgnoreCriteria(new TestIgnorer);
  EXPECT_TRUE(differ.Compare(msg1, msg2));
}

// Takes the product of all elements of item.ra as the key for key comparison.
class ValueProductMapKeyComparator
    : public util::MessageDifferencer::MapKeyComparator {
 public:
  typedef util::MessageDifferencer::SpecificField SpecificField;
  virtual bool IsMatch(const Message& message1, const Message& message2,
                       const std::vector<SpecificField>& parent_fields) const {
    const Reflection* reflection1 = message1.GetReflection();
    const Reflection* reflection2 = message2.GetReflection();
    // FieldDescriptor for item.ra
    const FieldDescriptor* ra_field =
        message1.GetDescriptor()->FindFieldByName("ra");
    // Get the product of all elements in item.ra
    int result1 = 1, result2 = 1;
    for (int i = 0; i < reflection1->FieldSize(message1, ra_field); ++i) {
      result1 *= reflection1->GetRepeatedInt32(message1, ra_field, i);
    }
    for (int i = 0; i < reflection2->FieldSize(message2, ra_field); ++i) {
      result2 *= reflection2->GetRepeatedInt32(message2, ra_field, i);
    }
    return result1 == result2;
  }
};

TEST(MessageDifferencerTest, RepeatedFieldMapTest_CustomMapKeyComparator) {
  protobuf_unittest::TestDiffMessage msg1;
  protobuf_unittest::TestDiffMessage msg2;
  // Treat "item" as Map, using custom key comparator to determine if two
  // elements have the same key.
  protobuf_unittest::TestDiffMessage::Item* item = msg1.add_item();
  item->add_ra(6);
  item->add_ra(35);
  item->set_b("hello");
  item = msg2.add_item();
  item->add_ra(10);
  item->add_ra(21);
  item->set_b("hello");
  util::MessageDifferencer differencer;
  ValueProductMapKeyComparator key_comparator;
  differencer.TreatAsMapUsingKeyComparator(GetFieldDescriptor(msg1, "item"),
                                           &key_comparator);
  std::string output;
  differencer.ReportDifferencesToString(&output);
  // Though the above two messages have different values for item.ra, they
  // are regarded as having the same key because 6 * 35 == 10 * 21. That's
  // how the key comparator determines if the two have the same key.
  // However, in value comparison, all fields of the message are taken into
  // consideration, so they are different because their item.ra fields have
  // different values using normal value comparison.
  EXPECT_FALSE(differencer.Compare(msg1, msg2));
  EXPECT_EQ(
      "modified: item[0].ra[0]: 6 -> 10\n"
      "modified: item[0].ra[1]: 35 -> 21\n",
      output);
  differencer.IgnoreField(GetFieldDescriptor(msg1, "item.ra"));
  output.clear();
  // item.ra is ignored in value comparison, so the two messages equal.
  EXPECT_TRUE(differencer.Compare(msg1, msg2));
  EXPECT_EQ("ignored: item[0].ra\n", output);
}

// Compares fields by their index offset by one, so index 0 matches with 1, etc.
class OffsetByOneMapKeyComparator
    : public util::MessageDifferencer::MapKeyComparator {
 public:
  typedef util::MessageDifferencer::SpecificField SpecificField;
  virtual bool IsMatch(const Message& message1, const Message& message2,
                       const std::vector<SpecificField>& parent_fields) const {
    return parent_fields.back().index + 1 == parent_fields.back().new_index;
  }
};

TEST(MessageDifferencerTest, RepeatedFieldMapTest_CustomIndexMapKeyComparator) {
  protobuf_unittest::TestDiffMessage msg1;
  protobuf_unittest::TestDiffMessage msg2;
  // Treat "item" as Map, using custom key comparator to determine if two
  // elements have the same key.
  protobuf_unittest::TestDiffMessage::Item* item = msg1.add_item();
  item->set_b("one");
  item = msg2.add_item();
  item->set_b("zero");
  item = msg2.add_item();
  item->set_b("one");
  util::MessageDifferencer differencer;
  OffsetByOneMapKeyComparator key_comparator;
  differencer.TreatAsMapUsingKeyComparator(GetFieldDescriptor(msg1, "item"),
                                           &key_comparator);
  std::string output;
  differencer.ReportDifferencesToString(&output);
  // With the offset by one comparator msg1.item[0] should be compared to
  // msg2.item[1] and thus be moved, msg2.item[0] should be marked as added.
  EXPECT_FALSE(differencer.Compare(msg1, msg2));
  EXPECT_EQ(
      "moved: item[0] -> item[1] : { b: \"one\" }\n"
      "added: item[0]: { b: \"zero\" }\n",
      output);
}

TEST(MessageDifferencerTest, RepeatedFieldSetTest_Subset) {
  protobuf_unittest::TestDiffMessage msg1;
  protobuf_unittest::TestDiffMessage msg2;

  msg1.add_rv(3);
  msg1.add_rv(8);
  msg1.add_rv(2);
  msg2.add_rv(2);
  msg2.add_rv(3);
  msg2.add_rv(5);
  msg2.add_rv(8);

  util::MessageDifferencer differencer;

  // Fail with only partial scope set.
  differencer.set_scope(util::MessageDifferencer::PARTIAL);
  differencer.set_repeated_field_comparison(util::MessageDifferencer::AS_LIST);
  EXPECT_FALSE(differencer.Compare(msg1, msg2));

  // Fail with only set-like comparison set.
  differencer.set_scope(util::MessageDifferencer::FULL);
  differencer.set_repeated_field_comparison(util::MessageDifferencer::AS_SET);
  EXPECT_FALSE(differencer.Compare(msg1, msg2));

  // Succeed with scope and repeated field comparison set properly.
  differencer.set_scope(util::MessageDifferencer::PARTIAL);
  differencer.set_repeated_field_comparison(util::MessageDifferencer::AS_SET);
  EXPECT_TRUE(differencer.Compare(msg1, msg2));
}

TEST(MessageDifferencerTest, IgnoreField_Single) {
  protobuf_unittest::TestField msg1;
  protobuf_unittest::TestField msg2;

  msg1.set_c(3);
  msg1.add_rc(1);

  msg2.set_c(5);
  msg2.add_rc(1);

  util::MessageDifferencer differencer;
  differencer.IgnoreField(GetFieldDescriptor(msg1, "c"));

  ExpectEqualsWithDifferencer(&differencer, msg1, msg2);
}

TEST(MessageDifferencerTest, IgnoreField_Repeated) {
  protobuf_unittest::TestField msg1;
  protobuf_unittest::TestField msg2;

  msg1.set_c(3);
  msg1.add_rc(1);
  msg1.add_rc(2);

  msg2.set_c(3);
  msg2.add_rc(1);
  msg2.add_rc(3);

  util::MessageDifferencer differencer;
  differencer.IgnoreField(GetFieldDescriptor(msg1, "rc"));

  ExpectEqualsWithDifferencer(&differencer, msg1, msg2);
}

TEST(MessageDifferencerTest, IgnoreField_Message) {
  protobuf_unittest::TestDiffMessage msg1;
  protobuf_unittest::TestDiffMessage msg2;

  protobuf_unittest::TestField* field;

  field = msg1.add_rm();
  field->set_c(3);

  field = msg2.add_rm();
  field->set_c(4);

  util::MessageDifferencer differencer;
  differencer.IgnoreField(GetFieldDescriptor(msg1, "rm"));

  ExpectEqualsWithDifferencer(&differencer, msg1, msg2);
}

TEST(MessageDifferencerTest, IgnoreField_Group) {
  protobuf_unittest::TestDiffMessage msg1;
  protobuf_unittest::TestDiffMessage msg2;

  protobuf_unittest::TestDiffMessage::Item* item;

  item = msg1.add_item();
  item->set_a(3);

  item = msg2.add_item();
  item->set_a(4);

  util::MessageDifferencer differencer;
  differencer.IgnoreField(GetFieldDescriptor(msg1, "item"));

  ExpectEqualsWithDifferencer(&differencer, msg1, msg2);
}

TEST(MessageDifferencerTest, IgnoreField_Missing) {
  protobuf_unittest::TestField msg1;
  protobuf_unittest::TestField msg2;

  msg1.set_c(3);
  msg1.add_rc(1);

  msg2.add_rc(1);

  util::MessageDifferencer differencer;
  differencer.IgnoreField(GetFieldDescriptor(msg1, "c"));

  ExpectEqualsWithDifferencer(&differencer, msg1, msg2);
  ExpectEqualsWithDifferencer(&differencer, msg2, msg1);
}

TEST(MessageDifferencerTest, IgnoreField_Multiple) {
  protobuf_unittest::TestField msg1;
  protobuf_unittest::TestField msg2;

  msg1.set_c(3);
  msg1.add_rc(1);
  msg1.add_rc(2);

  msg2.set_c(5);
  msg2.add_rc(1);
  msg2.add_rc(3);

  const FieldDescriptor* c = GetFieldDescriptor(msg1, "c");
  const FieldDescriptor* rc = GetFieldDescriptor(msg1, "rc");

  {  // Ignore c
    util::MessageDifferencer differencer;
    differencer.IgnoreField(c);

    EXPECT_FALSE(differencer.Compare(msg1, msg2));
  }
  {  // Ignore rc
    util::MessageDifferencer differencer;
    differencer.IgnoreField(rc);

    EXPECT_FALSE(differencer.Compare(msg1, msg2));
  }
  {  // Ignore both
    util::MessageDifferencer differencer;
    differencer.IgnoreField(c);
    differencer.IgnoreField(rc);

    ExpectEqualsWithDifferencer(&differencer, msg1, msg2);
  }
}

TEST(MessageDifferencerTest, IgnoreField_NestedMessage) {
  protobuf_unittest::TestDiffMessage msg1;
  protobuf_unittest::TestDiffMessage msg2;

  protobuf_unittest::TestField* field;

  field = msg1.add_rm();
  field->set_c(3);
  field->add_rc(1);

  field = msg2.add_rm();
  field->set_c(4);
  field->add_rc(1);

  util::MessageDifferencer differencer;
  differencer.IgnoreField(GetFieldDescriptor(msg1, "rm.c"));

  ExpectEqualsWithDifferencer(&differencer, msg1, msg2);
}

TEST(MessageDifferencerTest, IgnoreField_NestedGroup) {
  protobuf_unittest::TestDiffMessage msg1;
  protobuf_unittest::TestDiffMessage msg2;

  protobuf_unittest::TestDiffMessage::Item* item;

  item = msg1.add_item();
  item->set_a(3);
  item->set_b("foo");

  item = msg2.add_item();
  item->set_a(4);
  item->set_b("foo");

  util::MessageDifferencer differencer;
  differencer.IgnoreField(GetFieldDescriptor(msg1, "item.a"));

  ExpectEqualsWithDifferencer(&differencer, msg1, msg2);
}

TEST(MessageDifferencerTest, IgnoreField_InsideSet) {
  protobuf_unittest::TestDiffMessage msg1;
  protobuf_unittest::TestDiffMessage msg2;

  protobuf_unittest::TestDiffMessage::Item* item;

  item = msg1.add_item();
  item->set_a(1);
  item->set_b("foo");
  item->add_ra(1);

  item = msg1.add_item();
  item->set_a(2);
  item->set_b("bar");
  item->add_ra(2);

  item = msg2.add_item();
  item->set_a(2);
  item->set_b("bar");
  item->add_ra(2);

  item = msg2.add_item();
  item->set_a(1);
  item->set_b("baz");
  item->add_ra(1);

  const FieldDescriptor* item_desc = GetFieldDescriptor(msg1, "item");
  const FieldDescriptor* b = GetFieldDescriptor(msg1, "item.b");

  util::MessageDifferencer differencer;
  differencer.IgnoreField(b);
  differencer.TreatAsSet(item_desc);

  ExpectEqualsWithDifferencer(&differencer, msg1, msg2);
}

TEST(MessageDifferencerTest, IgnoreField_InsideMap) {
  protobuf_unittest::TestDiffMessage msg1;
  protobuf_unittest::TestDiffMessage msg2;

  protobuf_unittest::TestDiffMessage::Item* item;

  item = msg1.add_item();
  item->set_a(1);
  item->set_b("foo");
  item->add_ra(1);

  item = msg1.add_item();
  item->set_a(2);
  item->set_b("bar");
  item->add_ra(2);

  item = msg2.add_item();
  item->set_a(2);
  item->set_b("bar");
  item->add_ra(2);

  item = msg2.add_item();
  item->set_a(1);
  item->set_b("baz");
  item->add_ra(1);

  const FieldDescriptor* item_desc = GetFieldDescriptor(msg1, "item");
  const FieldDescriptor* a = GetFieldDescriptor(msg1, "item.a");
  const FieldDescriptor* b = GetFieldDescriptor(msg1, "item.b");

  util::MessageDifferencer differencer;
  differencer.IgnoreField(b);
  differencer.TreatAsMap(item_desc, a);

  ExpectEqualsWithDifferencer(&differencer, msg1, msg2);
}

TEST(MessageDifferencerTest, IgnoreField_DoesNotIgnoreKey) {
  protobuf_unittest::TestDiffMessage msg1;
  protobuf_unittest::TestDiffMessage msg2;

  protobuf_unittest::TestDiffMessage::Item* item;

  item = msg1.add_item();
  item->set_a(1);
  item->set_b("foo");
  item->add_ra(1);

  item = msg2.add_item();
  item->set_a(2);
  item->set_b("foo");
  item->add_ra(1);

  const FieldDescriptor* item_desc = GetFieldDescriptor(msg1, "item");
  const FieldDescriptor* a = GetFieldDescriptor(msg1, "item.a");

  util::MessageDifferencer differencer;
  differencer.IgnoreField(a);
  differencer.TreatAsMap(item_desc, a);

  EXPECT_FALSE(differencer.Compare(msg1, msg2));
}

TEST(MessageDifferencerTest, IgnoreField_TrumpsCompareWithFields) {
  protobuf_unittest::TestField msg1;
  protobuf_unittest::TestField msg2;

  msg1.set_c(3);
  msg1.add_rc(1);
  msg1.add_rc(2);

  msg2.set_c(3);
  msg2.add_rc(1);
  msg2.add_rc(3);

  const FieldDescriptor* c = GetFieldDescriptor(msg1, "c");
  const FieldDescriptor* rc = GetFieldDescriptor(msg1, "rc");

  std::vector<const FieldDescriptor*> fields;
  fields.push_back(c);
  fields.push_back(rc);

  util::MessageDifferencer differencer;
  differencer.IgnoreField(rc);

  differencer.set_scope(util::MessageDifferencer::FULL);
  EXPECT_TRUE(differencer.CompareWithFields(msg1, msg2, fields, fields));

  differencer.set_scope(util::MessageDifferencer::PARTIAL);
  EXPECT_TRUE(differencer.CompareWithFields(msg1, msg2, fields, fields));
}

TEST(MessageDifferencerTest, IgnoreField_SetReportIgnoresFalse) {
  protobuf_unittest::TestField msg1;
  protobuf_unittest::TestField msg2;

  msg1.set_a(1);
  msg1.set_b(2);
  msg1.set_c(3);
  msg1.add_rc(1);
  msg1.add_rc(2);

  msg2.set_a(1);
  msg2.set_b(1);

  const FieldDescriptor* a = GetFieldDescriptor(msg1, "a");
  const FieldDescriptor* b = GetFieldDescriptor(msg1, "b");
  const FieldDescriptor* c = GetFieldDescriptor(msg1, "c");
  const FieldDescriptor* rc = GetFieldDescriptor(msg1, "rc");

  std::vector<const FieldDescriptor*> fields;
  fields.push_back(a);
  fields.push_back(b);
  fields.push_back(c);
  fields.push_back(rc);

  std::string diff_report;
  util::MessageDifferencer differencer;
  differencer.set_report_ignores(false);
  differencer.set_report_matches(true);
  differencer.ReportDifferencesToString(&diff_report);
  differencer.IgnoreField(c);
  differencer.IgnoreField(rc);
  differencer.set_scope(util::MessageDifferencer::FULL);
  EXPECT_FALSE(differencer.CompareWithFields(msg1, msg2, fields, fields));

  EXPECT_EQ(diff_report,
            "matched: a : 1\n"
            "modified: b: 2 -> 1\n");
}


// Test class to save a copy of the last field_context.parent_fields() vector
// passed to the comparison function.
class ParentSavingFieldComparator : public util::FieldComparator {
 public:
  ParentSavingFieldComparator() {}

  virtual ComparisonResult Compare(const Message& message_1,
                                   const Message& message_2,
                                   const FieldDescriptor* field, int index_1,
                                   int index_2,
                                   const util::FieldContext* field_context) {
    if (field_context) parent_fields_ = *(field_context->parent_fields());
    if (field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) {
      return RECURSE;
    } else {
      return SAME;
    }
  }

  std::vector<util::MessageDifferencer::SpecificField> parent_fields() {
    return parent_fields_;
  }

 private:
  std::vector<util::MessageDifferencer::SpecificField> parent_fields_;
};

// Tests if MessageDifferencer sends the parent fields in the FieldContext
// parameter.
TEST(MessageDifferencerTest, FieldContextParentFieldsTest) {
  protobuf_unittest::TestDiffMessage msg1;
  msg1.add_rm()->set_c(1);
  protobuf_unittest::TestDiffMessage msg2;
  msg2.add_rm()->set_c(1);

  ParentSavingFieldComparator field_comparator;
  util::MessageDifferencer differencer;
  differencer.set_field_comparator(&field_comparator);
  differencer.Compare(msg1, msg2);

  // We want only one parent with the name "rm"
  ASSERT_EQ(1, field_comparator.parent_fields().size());
  EXPECT_EQ("rm", field_comparator.parent_fields()[0].field->name());
}


class ComparisonTest : public testing::Test {
 protected:
  ComparisonTest() : use_equivalency_(false), repeated_field_as_set_(false) {
    // Setup the test.
    TestUtil::SetAllFields(&proto1_);
    TestUtil::SetAllFields(&proto2_);

    TestUtil::SetAllExtensions(&proto1ex_);
    TestUtil::SetAllExtensions(&proto2ex_);

    TestUtil::SetAllFieldsAndExtensions(&orderings_proto1_);
    TestUtil::SetAllFieldsAndExtensions(&orderings_proto2_);

    unknown1_ = empty1_.mutable_unknown_fields();
    unknown2_ = empty2_.mutable_unknown_fields();
  }

  ~ComparisonTest() {}

  void SetSpecialFieldOption(const Message& message,
                             util::MessageDifferencer* d) {
    if (!ignored_field_.empty()) {
      d->IgnoreField(GetFieldDescriptor(message, ignored_field_));
    }

    if (repeated_field_as_set_) {
      d->set_repeated_field_comparison(util::MessageDifferencer::AS_SET);
    }

    if (!set_field_.empty()) {
      d->TreatAsSet(GetFieldDescriptor(message, set_field_));
    }

    if (!map_field_.empty() && !map_key_.empty()) {
      d->TreatAsMap(GetFieldDescriptor(message, map_field_),
                    GetFieldDescriptor(message, map_field_ + "." + map_key_));
    }
  }

  std::string Run(const Message& msg1, const Message& msg2) {
    std::string output;

    // Setup the comparison.
    util::MessageDifferencer differencer;
    differencer.ReportDifferencesToString(&output);

    if (use_equivalency_) {
      differencer.set_message_field_comparison(
          util::MessageDifferencer::EQUIVALENT);
    }

    SetSpecialFieldOption(msg1, &differencer);

    // Conduct the comparison.
    EXPECT_FALSE(differencer.Compare(msg1, msg2));

    return output;
  }

  std::string Run() { return Run(proto1_, proto2_); }

  std::string RunOrder() { return Run(orderings_proto1_, orderings_proto2_); }

  std::string RunEx() { return Run(proto1ex_, proto2ex_); }

  std::string RunDiff() { return Run(proto1diff_, proto2diff_); }

  std::string RunUn() { return Run(empty1_, empty2_); }

  void use_equivalency() { use_equivalency_ = true; }

  void repeated_field_as_set() { repeated_field_as_set_ = true; }

  void field_as_set(const std::string& field) { set_field_ = field; }

  void field_as_map(const string& field, const std::string& key) {
    map_field_ = field;
    map_key_ = key;
  }

  void ignore_field(const std::string& field) { ignored_field_ = field; }

  unittest::TestAllTypes proto1_;
  unittest::TestAllTypes proto2_;

  unittest::TestFieldOrderings orderings_proto1_;
  unittest::TestFieldOrderings orderings_proto2_;

  unittest::TestAllExtensions proto1ex_;
  unittest::TestAllExtensions proto2ex_;

  unittest::TestDiffMessage proto1diff_;
  unittest::TestDiffMessage proto2diff_;

  unittest::TestEmptyMessage empty1_;
  unittest::TestEmptyMessage empty2_;

  unittest::TestMap map_proto1_;
  unittest::TestMap map_proto2_;

  UnknownFieldSet* unknown1_;
  UnknownFieldSet* unknown2_;

  bool use_equivalency_;
  bool repeated_field_as_set_;

  std::string set_field_;
  std::string map_field_;
  std::string map_key_;
  std::string ignored_field_;
};

// Basic tests.
TEST_F(ComparisonTest, AdditionTest) {
  proto1_.clear_optional_int32();

  EXPECT_EQ("added: optional_int32: 101\n", Run());
}

TEST_F(ComparisonTest, Addition_OrderTest) {
  orderings_proto1_.clear_my_int();

  EXPECT_EQ("added: my_int: 1\n", RunOrder());
}

TEST_F(ComparisonTest, DeletionTest) {
  proto2_.clear_optional_int32();

  EXPECT_EQ("deleted: optional_int32: 101\n", Run());
}

TEST_F(ComparisonTest, Deletion_OrderTest) {
  orderings_proto2_.clear_my_string();

  EXPECT_EQ("deleted: my_string: \"foo\"\n", RunOrder());
}

TEST_F(ComparisonTest, RepeatedDeletionTest) {
  proto2_.clear_repeated_int32();

  EXPECT_EQ(
      "deleted: repeated_int32[0]: 201\n"
      "deleted: repeated_int32[1]: 301\n",
      Run());
}

TEST_F(ComparisonTest, ModificationTest) {
  proto1_.set_optional_int32(-1);

  EXPECT_EQ("modified: optional_int32: -1 -> 101\n", Run());
}

// Basic equivalency tests.
TEST_F(ComparisonTest, EquivalencyAdditionTest) {
  use_equivalency();

  proto1_.clear_optional_int32();

  EXPECT_EQ("modified: optional_int32: 0 -> 101\n", Run());
}

TEST_F(ComparisonTest, EquivalencyDeletionTest) {
  use_equivalency();

  proto2_.clear_optional_int32();

  EXPECT_EQ("modified: optional_int32: 101 -> 0\n", Run());
}

// Group tests.
TEST_F(ComparisonTest, GroupAdditionTest) {
  proto1_.mutable_optionalgroup()->clear_a();

  EXPECT_EQ("added: optionalgroup.a: 117\n", Run());
}

TEST_F(ComparisonTest, GroupDeletionTest) {
  proto2_.mutable_optionalgroup()->clear_a();

  EXPECT_EQ("deleted: optionalgroup.a: 117\n", Run());
}

TEST_F(ComparisonTest, GroupModificationTest) {
  proto1_.mutable_optionalgroup()->set_a(2);

  EXPECT_EQ("modified: optionalgroup.a: 2 -> 117\n", Run());
}

TEST_F(ComparisonTest, GroupFullAdditionTest) {
  proto1_.clear_optionalgroup();

  // Note the difference in the output between this and GroupAdditionTest.
  EXPECT_EQ("added: optionalgroup: { a: 117 }\n", Run());
}

TEST_F(ComparisonTest, GroupFullDeletionTest) {
  proto2_.clear_optionalgroup();

  EXPECT_EQ("deleted: optionalgroup: { a: 117 }\n", Run());
}

TEST_F(ComparisonTest, RepeatedSetOptionTest) {
  repeated_field_as_set();

  proto2_.clear_repeatedgroup();
  proto1_.clear_repeatedgroup();
  proto1_.add_repeatedgroup()->set_a(317);
  proto2_.add_repeatedgroup()->set_a(909);
  proto2_.add_repeatedgroup()->set_a(907);
  proto1_.add_repeatedgroup()->set_a(904);
  proto1_.add_repeatedgroup()->set_a(907);
  proto1_.add_repeatedgroup()->set_a(909);

  EXPECT_EQ(
      "moved: repeatedgroup[2] -> repeatedgroup[1] : { a: 907 }\n"
      "moved: repeatedgroup[3] -> repeatedgroup[0] : { a: 909 }\n"
      "deleted: repeatedgroup[0]: { a: 317 }\n"
      "deleted: repeatedgroup[1]: { a: 904 }\n",
      Run());
}

TEST_F(ComparisonTest, RepeatedSetOptionTest_Ex) {
  repeated_field_as_set();

  proto1ex_.ClearExtension(protobuf_unittest::repeated_nested_message_extension);
  proto2ex_.ClearExtension(protobuf_unittest::repeated_nested_message_extension);
  proto2ex_.AddExtension(protobuf_unittest::repeated_nested_message_extension)
      ->set_bb(909);
  proto2ex_.AddExtension(protobuf_unittest::repeated_nested_message_extension)
      ->set_bb(907);
  proto1ex_.AddExtension(protobuf_unittest::repeated_nested_message_extension)
      ->set_bb(904);
  proto1ex_.AddExtension(protobuf_unittest::repeated_nested_message_extension)
      ->set_bb(907);
  proto1ex_.AddExtension(protobuf_unittest::repeated_nested_message_extension)
      ->set_bb(909);

  EXPECT_EQ(
      "moved: (protobuf_unittest.repeated_nested_message_extension)[2] ->"
      " (protobuf_unittest.repeated_nested_message_extension)[0] :"
      " { bb: 909 }\n"
      "deleted: (protobuf_unittest.repeated_nested_message_extension)[0]:"
      " { bb: 904 }\n",
      RunEx());
}

TEST_F(ComparisonTest, RepeatedMapFieldTest_Group) {
  field_as_map("repeatedgroup", "a");
  proto1_.clear_repeatedgroup();
  proto2_.clear_repeatedgroup();

  proto1_.add_repeatedgroup()->set_a(317);  // deleted
  proto1_.add_repeatedgroup()->set_a(904);  // deleted
  proto1_.add_repeatedgroup()->set_a(907);  // moved from
  proto1_.add_repeatedgroup()->set_a(909);  // moved from

  proto2_.add_repeatedgroup()->set_a(909);  // moved to
  proto2_.add_repeatedgroup()->set_a(318);  // added
  proto2_.add_repeatedgroup()->set_a(907);  // moved to

  EXPECT_EQ(
      "moved: repeatedgroup[3] -> repeatedgroup[0] : { a: 909 }\n"
      "added: repeatedgroup[1]: { a: 318 }\n"
      "deleted: repeatedgroup[0]: { a: 317 }\n"
      "deleted: repeatedgroup[1]: { a: 904 }\n",
      Run());
}

TEST_F(ComparisonTest, RepeatedMapFieldTest_MessageKey) {
  // Use m as key, but use b as value.
  field_as_map("item", "m");

  protobuf_unittest::TestDiffMessage msg1;
  protobuf_unittest::TestDiffMessage msg2;
  protobuf_unittest::TestDiffMessage::Item* item = msg1.add_item();

  // The following code creates one deletion, one addition and two moved fields
  // on the messages.
  item->mutable_m()->set_c(0);
  item->set_b("first");
  item = msg1.add_item();
  item->mutable_m()->set_c(2);
  item->set_b("second");
  item = msg1.add_item();
  item->set_b("null");  // empty key moved
  item = msg1.add_item();
  item->mutable_m()->set_c(3);
  item->set_b("third");  // deletion
  item = msg1.add_item();
  item->mutable_m()->set_c(2);
  item->set_b("second");  // duplicated key ( deletion )
  item = msg2.add_item();
  item->mutable_m()->set_c(2);
  item->set_b("second");  // modification
  item = msg2.add_item();
  item->mutable_m()->set_c(4);
  item->set_b("fourth");  // addition
  item = msg2.add_item();
  item->mutable_m()->set_c(0);
  item->set_b("fist");  // move with change
  item = msg2.add_item();
  item->set_b("null");

  EXPECT_EQ(
      "modified: item[0].b -> item[2].b: \"first\" -> \"fist\"\n"
      "moved: item[1] -> item[0] : { b: \"second\" m { c: 2 } }\n"
      "moved: item[2] -> item[3] : { b: \"null\" }\n"
      "added: item[1]: { b: \"fourth\" m { c: 4 } }\n"
      "deleted: item[3]: { b: \"third\" m { c: 3 } }\n"
      "deleted: item[4]: { b: \"second\" m { c: 2 } }\n",
      Run(msg1, msg2));
}

TEST_F(ComparisonTest, RepeatedFieldSetTest_SetOfSet) {
  repeated_field_as_set();
  // Create the testing protos
  protobuf_unittest::TestDiffMessage msg1;
  protobuf_unittest::TestDiffMessage msg2;

  protobuf_unittest::TestDiffMessage::Item* item = msg1.add_item();
  item->add_ra(1);
  item->add_ra(2);
  item->add_ra(3);
  item = msg1.add_item();
  item->add_ra(5);
  item->add_ra(6);
  item = msg1.add_item();
  item->add_ra(1);
  item->add_ra(3);
  item = msg1.add_item();
  item->add_ra(6);
  item->add_ra(7);
  item->add_ra(8);

  item = msg2.add_item();
  item->add_ra(6);
  item->add_ra(5);
  item = msg2.add_item();
  item->add_ra(6);
  item->add_ra(8);
  item = msg2.add_item();
  item->add_ra(1);
  item->add_ra(3);
  item = msg2.add_item();
  item->add_ra(3);
  item->add_ra(2);
  item->add_ra(1);

  // Compare
  EXPECT_EQ(
      "moved: item[0].ra[0] -> item[3].ra[2] : 1\n"
      "moved: item[0].ra[2] -> item[3].ra[0] : 3\n"
      "moved: item[0] -> item[3] : { ra: 1 ra: 2 ra: 3 }\n"
      "moved: item[1].ra[0] -> item[0].ra[1] : 5\n"
      "moved: item[1].ra[1] -> item[0].ra[0] : 6\n"
      "moved: item[1] -> item[0] : { ra: 5 ra: 6 }\n"
      "added: item[1]: { ra: 6 ra: 8 }\n"
      "deleted: item[3]: { ra: 6 ra: 7 ra: 8 }\n",
      Run(msg1, msg2));
}

TEST_F(ComparisonTest, RepeatedMapFieldTest_RepeatedKey) {
  // used rb as a key, but b is the value.
  repeated_field_as_set();
  field_as_map("item", "rb");

  protobuf_unittest::TestDiffMessage msg1;
  protobuf_unittest::TestDiffMessage msg2;
  protobuf_unittest::TestDiffMessage::Item* item = msg1.add_item();
  item->add_rb("a");
  item->add_rb("b");
  item->set_b("first");

  item = msg2.add_item();
  item->add_rb("c");
  item->set_b("second");

  item = msg2.add_item();
  item->add_rb("b");
  item->add_rb("a");
  item->set_b("fist");

  EXPECT_EQ(
      "modified: item[0].b -> item[1].b: \"first\" -> \"fist\"\n"
      "moved: item[0].rb[0] -> item[1].rb[1] : \"a\"\n"
      "moved: item[0].rb[1] -> item[1].rb[0] : \"b\"\n"
      "added: item[0]: { b: \"second\" rb: \"c\" }\n",
      Run(msg1, msg2));
}

TEST_F(ComparisonTest, RepeatedMapFieldTest_RepeatedMessageKey) {
  field_as_map("item", "rm");

  protobuf_unittest::TestDiffMessage msg1;
  protobuf_unittest::TestDiffMessage msg2;
  protobuf_unittest::TestDiffMessage::Item* item = msg1.add_item();
  protobuf_unittest::TestField* key = item->add_rm();
  key->set_c(2);
  key->add_rc(10);
  key->add_rc(10);
  item = msg1.add_item();
  key = item->add_rm();
  key->set_c(0);
  key->add_rc(1);
  key->add_rc(2);
  key = item->add_rm();
  key->set_c(0);
  item->add_rb("first");

  item = msg2.add_item();
  item->CopyFrom(msg1.item(1));
  item->add_rb("second");

  EXPECT_EQ(
      "added: item[0].rb[1]: \"second\"\n"
      "deleted: item[0]: { rm { c: 2 rc: 10 rc: 10 } }\n",
      Run(msg1, msg2));
}

TEST_F(ComparisonTest, RepeatedSetOptionTest_Unknown) {
  // Currently, as_set option doesn't have affects on unknown field.
  // If needed, this feature will be added by request.
  repeated_field_as_set();
  unknown1_->AddGroup(245)->AddFixed32(248, 1);
  unknown2_->AddGroup(245)->AddFixed32(248, 3);
  unknown2_->AddGroup(245)->AddFixed32(248, 1);

  // We expect it behaves the same as normal comparison.
  EXPECT_EQ(
      "modified: 245[0].248[0]: 0x00000001 -> 0x00000003\n"
      "added: 245[1]: { ... }\n",
      RunUn());
}

TEST_F(ComparisonTest, Matching_Unknown) {
  unknown1_->AddGroup(245)->AddFixed32(248, 1);
  unknown2_->AddGroup(245)->AddFixed32(248, 1);
  unknown1_->AddGroup(245)->AddFixed32(248, 3);
  unknown2_->AddGroup(245)->AddFixed32(248, 3);
  unknown2_->AddLengthDelimited(242, "cat");
  unknown2_->AddGroup(246)->AddFixed32(248, 4);

  // report_match is false so only added/modified fields are expected.
  EXPECT_EQ(
      "added: 242[0]: \"cat\"\n"
      "added: 246[0]: { ... }\n",
      RunUn());
}

TEST_F(ComparisonTest, RepeatedSetFieldTest) {
  field_as_set("repeatedgroup");

  proto1_.clear_repeatedgroup();
  proto2_.clear_repeatedgroup();
  proto2_.add_repeatedgroup()->set_a(909);
  proto2_.add_repeatedgroup()->set_a(907);
  proto1_.add_repeatedgroup()->set_a(317);
  proto1_.add_repeatedgroup()->set_a(904);
  proto1_.add_repeatedgroup()->set_a(907);
  proto1_.add_repeatedgroup()->set_a(909);

  EXPECT_EQ(
      "moved: repeatedgroup[2] -> repeatedgroup[1] : { a: 907 }\n"
      "moved: repeatedgroup[3] -> repeatedgroup[0] : { a: 909 }\n"
      "deleted: repeatedgroup[0]: { a: 317 }\n"
      "deleted: repeatedgroup[1]: { a: 904 }\n",
      Run());
}

// Embedded message tests.
TEST_F(ComparisonTest, EmbeddedAdditionTest) {
  proto1_.mutable_optional_nested_message()->clear_bb();

  EXPECT_EQ("added: optional_nested_message.bb: 118\n", Run());
}

TEST_F(ComparisonTest, EmbeddedDeletionTest) {
  proto2_.mutable_optional_nested_message()->clear_bb();

  EXPECT_EQ("deleted: optional_nested_message.bb: 118\n", Run());
}

TEST_F(ComparisonTest, EmbeddedModificationTest) {
  proto1_.mutable_optional_nested_message()->set_bb(2);

  EXPECT_EQ("modified: optional_nested_message.bb: 2 -> 118\n", Run());
}

TEST_F(ComparisonTest, EmbeddedFullAdditionTest) {
  proto1_.clear_optional_nested_message();

  EXPECT_EQ("added: optional_nested_message: { bb: 118 }\n", Run());
}

TEST_F(ComparisonTest, EmbeddedPartialAdditionTest) {
  proto1_.clear_optional_nested_message();
  proto2_.mutable_optional_nested_message()->clear_bb();

  EXPECT_EQ("added: optional_nested_message: { }\n", Run());
}

TEST_F(ComparisonTest, EmbeddedFullDeletionTest) {
  proto2_.clear_optional_nested_message();

  EXPECT_EQ("deleted: optional_nested_message: { bb: 118 }\n", Run());
}

// Repeated element tests.
TEST_F(ComparisonTest, BasicRepeatedTest) {
  proto1_.clear_repeated_int32();
  proto2_.clear_repeated_int32();

  proto1_.add_repeated_int32(500);
  proto1_.add_repeated_int32(501);
  proto1_.add_repeated_int32(502);
  proto1_.add_repeated_int32(503);
  proto1_.add_repeated_int32(500);

  proto2_.add_repeated_int32(500);
  proto2_.add_repeated_int32(509);
  proto2_.add_repeated_int32(502);
  proto2_.add_repeated_int32(504);

  EXPECT_EQ(
      "modified: repeated_int32[1]: 501 -> 509\n"
      "modified: repeated_int32[3]: 503 -> 504\n"
      "deleted: repeated_int32[4]: 500\n",
      Run());
}

TEST_F(ComparisonTest, BasicRepeatedTest_SetOption) {
  repeated_field_as_set();
  proto1_.clear_repeated_int32();
  proto2_.clear_repeated_int32();

  proto1_.add_repeated_int32(501);
  proto1_.add_repeated_int32(502);
  proto1_.add_repeated_int32(503);
  proto1_.add_repeated_int32(500);
  proto1_.add_repeated_int32(500);

  proto2_.add_repeated_int32(500);
  proto2_.add_repeated_int32(509);
  proto2_.add_repeated_int32(503);
  proto2_.add_repeated_int32(502);
  proto2_.add_repeated_int32(504);

  EXPECT_EQ(
      "moved: repeated_int32[1] -> repeated_int32[3] : 502\n"
      "moved: repeated_int32[3] -> repeated_int32[0] : 500\n"
      "added: repeated_int32[1]: 509\n"
      "added: repeated_int32[4]: 504\n"
      "deleted: repeated_int32[0]: 501\n"
      "deleted: repeated_int32[4]: 500\n",
      Run());
}

TEST_F(ComparisonTest, BasicRepeatedTest_SetField) {
  field_as_set("repeated_int32");
  proto1_.clear_repeated_int32();
  proto2_.clear_repeated_int32();

  proto1_.add_repeated_int32(501);
  proto1_.add_repeated_int32(502);
  proto1_.add_repeated_int32(503);
  proto1_.add_repeated_int32(500);
  proto1_.add_repeated_int32(500);

  proto2_.add_repeated_int32(500);
  proto2_.add_repeated_int32(509);
  proto2_.add_repeated_int32(503);
  proto2_.add_repeated_int32(502);
  proto2_.add_repeated_int32(504);

  EXPECT_EQ(
      "moved: repeated_int32[1] -> repeated_int32[3] : 502\n"
      "moved: repeated_int32[3] -> repeated_int32[0] : 500\n"
      "added: repeated_int32[1]: 509\n"
      "added: repeated_int32[4]: 504\n"
      "deleted: repeated_int32[0]: 501\n"
      "deleted: repeated_int32[4]: 500\n",
      Run());
}

// Multiple action tests.
TEST_F(ComparisonTest, AddDeleteTest) {
  proto1_.clear_optional_int32();
  proto2_.clear_optional_int64();

  EXPECT_EQ(
      "added: optional_int32: 101\n"
      "deleted: optional_int64: 102\n",
      Run());
}

TEST_F(ComparisonTest, AddDelete_FieldOrderingTest) {
  orderings_proto1_.ClearExtension(unittest::my_extension_string);
  orderings_proto2_.clear_my_int();

  EXPECT_EQ(
      "deleted: my_int: 1\n"
      "added: (protobuf_unittest.my_extension_string): \"bar\"\n",
      RunOrder());
}

TEST_F(ComparisonTest, AllThreeTest) {
  proto1_.clear_optional_int32();
  proto2_.clear_optional_float();
  proto2_.set_optional_string("hello world!");

  EXPECT_EQ(
      "added: optional_int32: 101\n"
      "deleted: optional_float: 111\n"
      "modified: optional_string: \"115\" -> \"hello world!\"\n",
      Run());
}

TEST_F(ComparisonTest, SandwhichTest) {
  proto1_.clear_optional_int64();
  proto1_.clear_optional_uint32();

  proto2_.clear_optional_uint64();

  EXPECT_EQ(
      "added: optional_int64: 102\n"
      "added: optional_uint32: 103\n"
      "deleted: optional_uint64: 104\n",
      Run());
}

TEST_F(ComparisonTest, IgnoredNoChangeTest) {
  proto1diff_.set_v(3);
  proto2diff_.set_v(3);
  proto2diff_.set_w("foo");

  ignore_field("v");

  EXPECT_EQ(
      "ignored: v\n"
      "added: w: \"foo\"\n",
      RunDiff());
}

TEST_F(ComparisonTest, IgnoredAddTest) {
  proto2diff_.set_v(3);
  proto2diff_.set_w("foo");

  ignore_field("v");

  EXPECT_EQ(
      "ignored: v\n"
      "added: w: \"foo\"\n",
      RunDiff());
}

TEST_F(ComparisonTest, IgnoredDeleteTest) {
  proto1diff_.set_v(3);
  proto2diff_.set_w("foo");

  ignore_field("v");

  EXPECT_EQ(
      "ignored: v\n"
      "added: w: \"foo\"\n",
      RunDiff());
}

TEST_F(ComparisonTest, IgnoredModifyTest) {
  proto1diff_.set_v(3);
  proto2diff_.set_v(4);
  proto2diff_.set_w("foo");

  ignore_field("v");

  EXPECT_EQ(
      "ignored: v\n"
      "added: w: \"foo\"\n",
      RunDiff());
}

TEST_F(ComparisonTest, IgnoredRepeatedAddTest) {
  proto1diff_.add_rv(3);
  proto1diff_.add_rv(4);

  proto2diff_.add_rv(3);
  proto2diff_.add_rv(4);
  proto2diff_.add_rv(5);

  proto2diff_.set_w("foo");

  ignore_field("rv");

  EXPECT_EQ(
      "ignored: rv\n"
      "added: w: \"foo\"\n",
      RunDiff());
}

TEST_F(ComparisonTest, IgnoredRepeatedDeleteTest) {
  proto1diff_.add_rv(3);
  proto1diff_.add_rv(4);
  proto1diff_.add_rv(5);

  proto2diff_.add_rv(3);
  proto2diff_.add_rv(4);

  proto2diff_.set_w("foo");

  ignore_field("rv");

  EXPECT_EQ(
      "ignored: rv\n"
      "added: w: \"foo\"\n",
      RunDiff());
}

TEST_F(ComparisonTest, IgnoredRepeatedModifyTest) {
  proto1diff_.add_rv(3);
  proto1diff_.add_rv(4);

  proto2diff_.add_rv(3);
  proto2diff_.add_rv(5);

  proto2diff_.set_w("foo");

  ignore_field("rv");

  EXPECT_EQ(
      "ignored: rv\n"
      "added: w: \"foo\"\n",
      RunDiff());
}

TEST_F(ComparisonTest, IgnoredWholeNestedMessage) {
  proto1diff_.mutable_m()->set_c(3);
  proto2diff_.mutable_m()->set_c(4);

  proto2diff_.set_w("foo");

  ignore_field("m");

  EXPECT_EQ(
      "added: w: \"foo\"\n"
      "ignored: m\n",
      RunDiff());
}

TEST_F(ComparisonTest, IgnoredNestedField) {
  proto1diff_.mutable_m()->set_c(3);
  proto2diff_.mutable_m()->set_c(4);

  proto2diff_.set_w("foo");

  ignore_field("m.c");

  EXPECT_EQ(
      "added: w: \"foo\"\n"
      "ignored: m.c\n",
      RunDiff());
}

TEST_F(ComparisonTest, IgnoredRepeatedNested) {
  proto1diff_.add_rm()->set_c(0);
  proto1diff_.add_rm()->set_c(1);
  proto2diff_.add_rm()->set_c(2);
  proto2diff_.add_rm()->set_c(3);

  proto2diff_.set_w("foo");

  ignore_field("rm.c");

  EXPECT_EQ(
      "ignored: rm[0].c\n"
      "ignored: rm[1].c\n"
      "added: w: \"foo\"\n",
      RunDiff());
}

TEST_F(ComparisonTest, IgnoredNestedRepeated) {
  proto1diff_.mutable_m()->add_rc(23);
  proto1diff_.mutable_m()->add_rc(24);
  proto2diff_.mutable_m()->add_rc(25);

  proto2diff_.set_w("foo");

  ignore_field("m.rc");

  EXPECT_EQ(
      "added: w: \"foo\"\n"
      "ignored: m.rc\n",
      RunDiff());
}

TEST_F(ComparisonTest, ExtensionTest) {
  proto1ex_.SetExtension(unittest::optional_int32_extension, 401);
  proto2ex_.SetExtension(unittest::optional_int32_extension, 402);

  proto1ex_.ClearExtension(unittest::optional_int64_extension);
  proto2ex_.SetExtension(unittest::optional_int64_extension, 403);

  EXPECT_EQ(
      "modified: (protobuf_unittest.optional_int32_extension): 401 -> 402\n"
      "added: (protobuf_unittest.optional_int64_extension): 403\n",
      RunEx());
}

TEST_F(ComparisonTest, MatchedUnknownFieldTagTest) {
  unknown1_->AddVarint(240, 122);
  unknown2_->AddVarint(240, 121);
  unknown1_->AddFixed32(241, 1);
  unknown2_->AddFixed64(241, 2);
  unknown1_->AddLengthDelimited(242, "cat");
  unknown2_->AddLengthDelimited(242, "dog");

  EXPECT_EQ(
      "modified: 240[0]: 122 -> 121\n"
      "deleted: 241[0]: 0x00000001\n"
      "added: 241[0]: 0x0000000000000002\n"
      "modified: 242[0]: \"cat\" -> \"dog\"\n",
      RunUn());
}

TEST_F(ComparisonTest, UnmatchedUnknownFieldTagTest) {
  unknown1_->AddFixed32(243, 1);
  unknown2_->AddVarint(244, 2);
  unknown2_->AddVarint(244, 4);

  EXPECT_EQ(
      "deleted: 243[0]: 0x00000001\n"
      "added: 244[0]: 2\n"
      "added: 244[1]: 4\n",
      RunUn());
}

TEST_F(ComparisonTest, DifferentSizedUnknownFieldTest) {
  unknown1_->AddVarint(240, 1);
  unknown1_->AddVarint(240, 3);
  unknown1_->AddVarint(240, 4);
  unknown2_->AddVarint(240, 2);
  unknown2_->AddVarint(240, 3);
  unknown2_->AddVarint(240, 2);
  unknown2_->AddVarint(240, 5);

  EXPECT_EQ(
      "modified: 240[0]: 1 -> 2\n"
      "modified: 240[2]: 4 -> 2\n"
      "added: 240[3]: 5\n",
      RunUn());
}

TEST_F(ComparisonTest, UnknownFieldsAll) {
  unknown1_->AddVarint(243, 122);
  unknown1_->AddFixed64(244, 0x0172356);
  unknown1_->AddFixed64(244, 0x098);
  unknown1_->AddGroup(245)->AddFixed32(248, 1);
  unknown1_->mutable_field(3)->mutable_group()->AddFixed32(248, 2);
  unknown1_->AddGroup(249)->AddFixed64(250, 1);

  unknown2_->AddVarint(243, 121);
  unknown2_->AddLengthDelimited(73882, "test 123");
  unknown2_->AddGroup(245)->AddFixed32(248, 3);
  unknown2_->AddGroup(247);

  EXPECT_EQ(
      "modified: 243[0]: 122 -> 121\n"
      "deleted: 244[0]: 0x0000000000172356\n"
      "deleted: 244[1]: 0x0000000000000098\n"
      "modified: 245[0].248[0]: 0x00000001 -> 0x00000003\n"
      "deleted: 245[0].248[1]: 0x00000002\n"
      "added: 247[0]: { ... }\n"
      "deleted: 249[0]: { ... }\n"
      "added: 73882[0]: \"test 123\"\n",
      RunUn());
}

TEST_F(ComparisonTest, EquivalentIgnoresUnknown) {
  unittest::ForeignMessage message1, message2;

  message1.set_c(5);
  message1.mutable_unknown_fields()->AddVarint(123, 456);
  message2.set_c(5);
  message2.mutable_unknown_fields()->AddVarint(321, 654);

  EXPECT_FALSE(util::MessageDifferencer::Equals(message1, message2));
  EXPECT_TRUE(util::MessageDifferencer::Equivalent(message1, message2));
}

TEST_F(ComparisonTest, MapTest) {
  Map<string, std::string>& map1 = *map_proto1_.mutable_map_string_string();
  map1["key1"] = "1";
  map1["key2"] = "2";
  map1["key3"] = "3";
  Map<string, std::string>& map2 = *map_proto2_.mutable_map_string_string();
  map2["key3"] = "0";
  map2["key2"] = "2";
  map2["key1"] = "1";

  EXPECT_EQ("modified: map_string_string.value: \"3\" -> \"0\"\n",
            Run(map_proto1_, map_proto2_));
}

TEST_F(ComparisonTest, MapIgnoreKeyTest) {
  Map<string, std::string>& map1 = *map_proto1_.mutable_map_string_string();
  map1["key1"] = "1";
  map1["key2"] = "2";
  map1["key3"] = "3";
  Map<string, std::string>& map2 = *map_proto2_.mutable_map_string_string();
  map2["key4"] = "2";
  map2["key5"] = "3";
  map2["key6"] = "1";

  util::MessageDifferencer differencer;
  differencer.IgnoreField(
      GetFieldDescriptor(map_proto1_, "map_string_string.key"));
  EXPECT_TRUE(differencer.Compare(map_proto1_, map_proto2_));
}

TEST_F(ComparisonTest, MapRoundTripSyncTest) {
  TextFormat::Parser parser;
  unittest::TestMap map_reflection1;

  // By setting via reflection, data exists in repeated field.
  ASSERT_TRUE(parser.ParseFromString("map_int32_foreign_message { key: 1 }",
                                     &map_reflection1));

  // During copy, data is synced from repeated field to map.
  unittest::TestMap map_reflection2 = map_reflection1;

  // During comparison, data is synced from map to repeated field.
  EXPECT_TRUE(
      util::MessageDifferencer::Equals(map_reflection1, map_reflection2));
}

TEST_F(ComparisonTest, MapEntryPartialTest) {
  TextFormat::Parser parser;
  unittest::TestMap map1;
  unittest::TestMap map2;

  std::string output;
  util::MessageDifferencer differencer;
  differencer.set_scope(util::MessageDifferencer::PARTIAL);
  differencer.ReportDifferencesToString(&output);

  ASSERT_TRUE(parser.ParseFromString(
      "map_int32_foreign_message { key: 1 value { c: 1 } }", &map1));
  ASSERT_TRUE(parser.ParseFromString(
      "map_int32_foreign_message { key: 1 value { c: 2 }}", &map2));
  EXPECT_FALSE(differencer.Compare(map1, map2));
  EXPECT_EQ("modified: map_int32_foreign_message.value.c: 1 -> 2\n", output);

  ASSERT_TRUE(
      parser.ParseFromString("map_int32_foreign_message { key: 1 }", &map1));
  EXPECT_TRUE(differencer.Compare(map1, map2));
}

TEST_F(ComparisonTest, MapEntryPartialEmptyKeyTest) {
  TextFormat::Parser parser;
  unittest::TestMap map1;
  unittest::TestMap map2;
  ASSERT_TRUE(parser.ParseFromString("map_int32_foreign_message {}", &map1));
  ASSERT_TRUE(
      parser.ParseFromString("map_int32_foreign_message { key: 1 }", &map2));

  util::MessageDifferencer differencer;
  differencer.set_scope(util::MessageDifferencer::PARTIAL);
  EXPECT_TRUE(differencer.Compare(map1, map2));
}

TEST_F(ComparisonTest, MapEntryMissingEmptyFieldIsOkTest) {
  TextFormat::Parser parser;
  protobuf_unittest::TestMap msg1;
  protobuf_unittest::TestMap msg2;

  ASSERT_TRUE(parser.ParseFromString(
      "map_string_foreign_message { key: 'key1' value {}}", &msg1));
  ASSERT_TRUE(parser.ParseFromString(
      "map_string_foreign_message { key: 'key1' }", &msg2));

  util::MessageDifferencer differencer;
  differencer.set_scope(util::MessageDifferencer::PARTIAL);

  ASSERT_TRUE(differencer.Compare(msg1, msg2));
}

// Considers strings keys as equal if they have equal lengths.
class LengthMapKeyComparator
    : public util::MessageDifferencer::MapKeyComparator {
 public:
  typedef util::MessageDifferencer::SpecificField SpecificField;
  virtual bool IsMatch(const Message& message1, const Message& message2,
                       const std::vector<SpecificField>& parent_fields) const {
    const Reflection* reflection1 = message1.GetReflection();
    const Reflection* reflection2 = message2.GetReflection();
    const FieldDescriptor* key_field =
        message1.GetDescriptor()->FindFieldByName("key");
    return reflection1->GetString(message1, key_field).size() ==
           reflection2->GetString(message2, key_field).size();
  }
};

TEST_F(ComparisonTest, MapEntryCustomMapKeyComparator) {
  TextFormat::Parser parser;
  protobuf_unittest::TestMap msg1;
  protobuf_unittest::TestMap msg2;

  ASSERT_TRUE(parser.ParseFromString(
      "map_string_foreign_message { key: 'key1' value { c: 1 }}", &msg1));
  ASSERT_TRUE(parser.ParseFromString(
      "map_string_foreign_message { key: 'key2' value { c: 1 }}", &msg2));

  util::MessageDifferencer differencer;
  LengthMapKeyComparator key_comparator;
  differencer.TreatAsMapUsingKeyComparator(
      GetFieldDescriptor(msg1, "map_string_foreign_message"), &key_comparator);
  std::string output;
  differencer.ReportDifferencesToString(&output);
  // Though the above two messages have different keys for their map entries,
  // they are considered the same by key_comparator because their lengths are
  // equal.  However, in value comparison, all fields of the message are taken
  // into consideration, so they are reported as different.
  EXPECT_FALSE(differencer.Compare(msg1, msg2));
  EXPECT_EQ("modified: map_string_foreign_message.key: \"key1\" -> \"key2\"\n",
            output);
  differencer.IgnoreField(
      GetFieldDescriptor(msg1, "map_string_foreign_message.key"));
  output.clear();
  EXPECT_TRUE(differencer.Compare(msg1, msg2));
  EXPECT_EQ("ignored: map_string_foreign_message.key\n", output);
}

class MatchingTest : public testing::Test {
 public:
  typedef util::MessageDifferencer MessageDifferencer;

 protected:
  MatchingTest() {}

  ~MatchingTest() {}

  std::string RunWithResult(MessageDifferencer* differencer,
                            const Message& msg1, const Message& msg2,
                            bool result) {
    std::string output;
    {
      // Before we return the "output" string, we must make sure the
      // StreamReporter is destructored because its destructor will
      // flush the stream.
      io::StringOutputStream output_stream(&output);
      MessageDifferencer::StreamReporter reporter(&output_stream);
      reporter.set_report_modified_aggregates(true);
      differencer->set_report_matches(true);
      differencer->ReportDifferencesTo(&reporter);
      if (result) {
        EXPECT_TRUE(differencer->Compare(msg1, msg2));
      } else {
        EXPECT_FALSE(differencer->Compare(msg1, msg2));
      }
    }
    return output;
  }

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(MatchingTest);
};

TEST_F(MatchingTest, StreamReporterMatching) {
  protobuf_unittest::TestField msg1, msg2;
  msg1.set_c(72);
  msg2.set_c(72);
  msg1.add_rc(13);
  msg2.add_rc(13);
  msg1.add_rc(17);
  msg2.add_rc(17);
  std::string output;
  MessageDifferencer differencer;
  differencer.set_report_matches(true);
  differencer.ReportDifferencesToString(&output);
  EXPECT_TRUE(differencer.Compare(msg1, msg2));
  EXPECT_EQ(
      "matched: c : 72\n"
      "matched: rc[0] : 13\n"
      "matched: rc[1] : 17\n",
      output);
}

TEST_F(MatchingTest, DontReportMatchedWhenIgnoring) {
  protobuf_unittest::TestField msg1, msg2;
  msg1.set_c(72);
  msg2.set_c(72);
  msg1.add_rc(13);
  msg2.add_rc(13);
  msg1.add_rc(17);
  msg2.add_rc(17);
  std::string output;
  MessageDifferencer differencer;
  differencer.set_report_matches(true);
  differencer.ReportDifferencesToString(&output);

  differencer.IgnoreField(msg1.GetDescriptor()->FindFieldByName("c"));

  EXPECT_TRUE(differencer.Compare(msg1, msg2));
  EXPECT_EQ(
      "ignored: c\n"
      "matched: rc[0] : 13\n"
      "matched: rc[1] : 17\n",
      output);
}

TEST_F(MatchingTest, ReportMatchedForMovedFields) {
  protobuf_unittest::TestDiffMessage msg1, msg2;
  protobuf_unittest::TestDiffMessage::Item* item = msg1.add_item();
  item->set_a(53);
  item->set_b("hello");
  item = msg2.add_item();
  item->set_a(27);
  item = msg2.add_item();
  item->set_a(53);
  item->set_b("hello");
  item = msg1.add_item();
  item->set_a(27);
  MessageDifferencer differencer;
  const FieldDescriptor* desc;
  desc = msg1.GetDescriptor()->FindFieldByName("item");
  differencer.TreatAsSet(desc);

  EXPECT_EQ(
      "matched: item[0].a -> item[1].a : 53\n"
      "matched: item[0].b -> item[1].b : \"hello\"\n"
      "moved: item[0] -> item[1] : { a: 53 b: \"hello\" }\n"
      "matched: item[1].a -> item[0].a : 27\n"
      "moved: item[1] -> item[0] : { a: 27 }\n",
      RunWithResult(&differencer, msg1, msg2, true));
}

TEST_F(MatchingTest, MatchesAppearInPostTraversalOrderForMovedFields) {
  protobuf_unittest::TestDiffMessage msg1, msg2;
  protobuf_unittest::TestDiffMessage::Item* item;
  protobuf_unittest::TestField* field;

  const FieldDescriptor* desc;
  const FieldDescriptor* nested_desc;
  const FieldDescriptor* double_nested_desc;
  desc = msg1.GetDescriptor()->FindFieldByName("item");
  nested_desc = desc->message_type()->FindFieldByName("rm");
  double_nested_desc = nested_desc->message_type()->FindFieldByName("rc");
  MessageDifferencer differencer;
  differencer.TreatAsSet(desc);
  differencer.TreatAsSet(nested_desc);
  differencer.TreatAsSet(double_nested_desc);

  item = msg1.add_item();
  field = item->add_rm();
  field->set_c(1);
  field->add_rc(2);
  field->add_rc(3);
  field = item->add_rm();
  field->set_c(4);
  field->add_rc(5);
  field->add_rc(6);
  field->add_rc(7);
  item = msg2.add_item();
  field = item->add_rm();
  field->set_c(4);
  field->add_rc(7);
  field->add_rc(6);
  field->add_rc(5);
  field = item->add_rm();
  field->set_c(1);
  field->add_rc(3);
  field->add_rc(2);
  item = msg1.add_item();
  field = item->add_rm();
  field->set_c(8);
  field->add_rc(10);
  field->add_rc(11);
  field->add_rc(9);
  item = msg2.add_item();
  field = item->add_rm();
  field->set_c(8);
  field->add_rc(9);
  field->add_rc(10);
  field->add_rc(11);

  EXPECT_EQ(
      "matched: item[0].rm[0].c -> item[0].rm[1].c : 1\n"
      "moved: item[0].rm[0].rc[0] -> item[0].rm[1].rc[1] : 2\n"
      "moved: item[0].rm[0].rc[1] -> item[0].rm[1].rc[0] : 3\n"
      "moved: item[0].rm[0] -> item[0].rm[1] : { c: 1 rc: 2 rc: 3 }\n"
      "matched: item[0].rm[1].c -> item[0].rm[0].c : 4\n"
      "moved: item[0].rm[1].rc[0] -> item[0].rm[0].rc[2] : 5\n"
      "matched: item[0].rm[1].rc[1] -> item[0].rm[0].rc[1] : 6\n"
      "moved: item[0].rm[1].rc[2] -> item[0].rm[0].rc[0] : 7\n"
      "moved: item[0].rm[1] -> item[0].rm[0] : { c: 4 rc: 5 rc: 6 rc: 7 }\n"
      "matched: item[0] : { rm { c: 1 rc: 2 rc: 3 }"
      " rm { c: 4 rc: 5 rc: 6 rc: 7 } }\n"
      "matched: item[1].rm[0].c : 8\n"
      "moved: item[1].rm[0].rc[0] -> item[1].rm[0].rc[1] : 10\n"
      "moved: item[1].rm[0].rc[1] -> item[1].rm[0].rc[2] : 11\n"
      "moved: item[1].rm[0].rc[2] -> item[1].rm[0].rc[0] : 9\n"
      "matched: item[1].rm[0] : { c: 8 rc: 10 rc: 11 rc: 9 }\n"
      "matched: item[1] : { rm { c: 8 rc: 10 rc: 11 rc: 9 } }\n",
      RunWithResult(&differencer, msg1, msg2, true));
}

TEST_F(MatchingTest, MatchAndModifiedInterleaveProperly) {
  protobuf_unittest::TestDiffMessage msg1, msg2;
  protobuf_unittest::TestDiffMessage::Item* item;
  protobuf_unittest::TestField* field;

  const FieldDescriptor* desc;
  const FieldDescriptor* nested_key;
  const FieldDescriptor* nested_desc;
  const FieldDescriptor* double_nested_key;
  const FieldDescriptor* double_nested_desc;
  desc = msg1.GetDescriptor()->FindFieldByName("item");
  nested_key = desc->message_type()->FindFieldByName("a");
  nested_desc = desc->message_type()->FindFieldByName("rm");
  double_nested_key = nested_desc->message_type()->FindFieldByName("c");
  double_nested_desc = nested_desc->message_type()->FindFieldByName("rc");

  MessageDifferencer differencer;
  differencer.TreatAsMap(desc, nested_key);
  differencer.TreatAsMap(nested_desc, double_nested_key);
  differencer.TreatAsSet(double_nested_desc);

  item = msg1.add_item();
  item->set_a(1);
  field = item->add_rm();
  field->set_c(2);
  field->add_rc(3);
  field->add_rc(4);
  field = item->add_rm();
  field->set_c(5);
  field->add_rc(6);
  field->add_rc(7);
  field->add_rc(8);
  item = msg1.add_item();
  item->set_a(9);
  field = item->add_rm();
  field->set_c(10);
  field->add_rc(11);
  field->add_rc(12);
  field = item->add_rm();
  field->set_c(13);

  item = msg2.add_item();
  item->set_a(1);
  field = item->add_rm();
  field->set_c(5);
  field->add_rc(8);
  field->add_rc(8);
  field->add_rc(6);
  field = item->add_rm();
  field->set_c(3);
  field->add_rc(2);
  field->add_rc(4);
  item = msg2.add_item();
  item->set_a(9);
  field = item->add_rm();
  field->set_c(10);
  field->add_rc(12);
  field->add_rc(11);
  field = item->add_rm();
  field->set_c(13);

  EXPECT_EQ(
      "matched: item[0].a : 1\n"
      "matched: item[0].rm[1].c -> item[0].rm[0].c : 5\n"
      "moved: item[0].rm[1].rc[0] -> item[0].rm[0].rc[2] : 6\n"
      "moved: item[0].rm[1].rc[2] -> item[0].rm[0].rc[0] : 8\n"
      "added: item[0].rm[0].rc[1]: 8\n"
      "deleted: item[0].rm[1].rc[1]: 7\n"
      "modified: item[0].rm[1] -> item[0].rm[0]: { c: 5 rc: 6 rc: 7 rc: 8 } ->"
      " { c: 5 rc: 8 rc: 8 rc: 6 }\n"
      "added: item[0].rm[1]: { c: 3 rc: 2 rc: 4 }\n"
      "deleted: item[0].rm[0]: { c: 2 rc: 3 rc: 4 }\n"
      "modified: item[0]: { a: 1 rm { c: 2 rc: 3 rc: 4 }"
      " rm { c: 5 rc: 6 rc: 7 rc: 8 } } ->"
      " { a: 1 rm { c: 5 rc: 8 rc: 8 rc: 6 }"
      " rm { c: 3 rc: 2 rc: 4 } }\n"
      "matched: item[1].a : 9\n"
      "matched: item[1].rm[0].c : 10\n"
      "moved: item[1].rm[0].rc[0] -> item[1].rm[0].rc[1] : 11\n"
      "moved: item[1].rm[0].rc[1] -> item[1].rm[0].rc[0] : 12\n"
      "matched: item[1].rm[0] : { c: 10 rc: 11 rc: 12 }\n"
      "matched: item[1].rm[1].c : 13\n"
      "matched: item[1].rm[1] : { c: 13 }\n"
      "matched: item[1] : { a: 9 rm { c: 10 rc: 11 rc: 12 } rm { c: 13 } }\n",
      RunWithResult(&differencer, msg1, msg2, false));
}

TEST_F(MatchingTest, MatchingWorksWithExtensions) {
  protobuf_unittest::TestAllExtensions msg1, msg2;
  protobuf_unittest::TestAllTypes::NestedMessage* nested;
  using protobuf_unittest::repeated_nested_message_extension;

  const FileDescriptor* descriptor;
  const FieldDescriptor* desc;
  const FieldDescriptor* nested_key;
  descriptor = msg1.GetDescriptor()->file();
  desc = descriptor->FindExtensionByName("repeated_nested_message_extension");
  ASSERT_FALSE(desc == NULL);
  nested_key = desc->message_type()->FindFieldByName("bb");

  MessageDifferencer differencer;
  differencer.TreatAsMap(desc, nested_key);

  nested = msg1.AddExtension(repeated_nested_message_extension);
  nested->set_bb(7);
  nested = msg1.AddExtension(repeated_nested_message_extension);
  nested->set_bb(13);
  nested = msg1.AddExtension(repeated_nested_message_extension);
  nested->set_bb(11);
  nested = msg2.AddExtension(repeated_nested_message_extension);
  nested->set_bb(11);
  nested = msg2.AddExtension(repeated_nested_message_extension);
  nested->set_bb(13);
  nested = msg2.AddExtension(repeated_nested_message_extension);
  nested->set_bb(7);

  EXPECT_EQ(
      "matched: (protobuf_unittest.repeated_nested_message_extension)[0].bb ->"
      " (protobuf_unittest.repeated_nested_message_extension)[2].bb : 7\n"
      "moved: (protobuf_unittest.repeated_nested_message_extension)[0] ->"
      " (protobuf_unittest.repeated_nested_message_extension)[2] :"
      " { bb: 7 }\n"
      "matched: (protobuf_unittest.repeated_nested_message_extension)[1].bb :"
      " 13\n"
      "matched: (protobuf_unittest.repeated_nested_message_extension)[1] :"
      " { bb: 13 }\n"
      "matched: (protobuf_unittest.repeated_nested_message_extension)[2].bb ->"
      " (protobuf_unittest.repeated_nested_message_extension)[0].bb :"
      " 11\n"
      "moved: (protobuf_unittest.repeated_nested_message_extension)[2] ->"
      " (protobuf_unittest.repeated_nested_message_extension)[0] :"
      " { bb: 11 }\n",
      RunWithResult(&differencer, msg1, msg2, true));
}

TEST(AnyTest, Simple) {
  protobuf_unittest::TestField value1, value2;
  value1.set_a(20);
  value2.set_a(21);

  protobuf_unittest::TestAny m1, m2;
  m1.mutable_any_value()->PackFrom(value1);
  m2.mutable_any_value()->PackFrom(value2);
  util::MessageDifferencer message_differencer;
  std::string difference_string;
  message_differencer.ReportDifferencesToString(&difference_string);
  EXPECT_FALSE(message_differencer.Compare(m1, m2));
  EXPECT_EQ("modified: any_value.a: 20 -> 21\n", difference_string);
}

TEST(Anytest, TreatAsSet) {
  protobuf_unittest::TestField value1, value2;
  value1.set_a(20);
  value1.set_b(30);
  value2.set_a(20);
  value2.set_b(31);

  protobuf_unittest::TestAny m1, m2;
  m1.add_repeated_any_value()->PackFrom(value1);
  m1.add_repeated_any_value()->PackFrom(value2);
  m2.add_repeated_any_value()->PackFrom(value2);
  m2.add_repeated_any_value()->PackFrom(value1);

  util::MessageDifferencer message_differencer;
  message_differencer.TreatAsSet(GetFieldDescriptor(m1, "repeated_any_value"));
  EXPECT_TRUE(message_differencer.Compare(m1, m2));
}

TEST(Anytest, TreatAsSet_DifferentType) {
  protobuf_unittest::TestField value1;
  value1.set_a(20);
  value1.set_b(30);
  protobuf_unittest::TestDiffMessage value2;
  value2.add_rv(40);

  protobuf_unittest::TestAny m1, m2;
  m1.add_repeated_any_value()->PackFrom(value1);
  m1.add_repeated_any_value()->PackFrom(value2);
  m2.add_repeated_any_value()->PackFrom(value2);
  m2.add_repeated_any_value()->PackFrom(value1);

  util::MessageDifferencer message_differencer;
  message_differencer.TreatAsSet(GetFieldDescriptor(m1, "repeated_any_value"));
  EXPECT_TRUE(message_differencer.Compare(m1, m2));
}


}  // namespace
}  // namespace protobuf
}  // namespace google
