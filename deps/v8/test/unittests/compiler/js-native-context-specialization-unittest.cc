// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-native-context-specialization.h"

#include "src/compiler/js-operator.h"
#include "test/unittests/compiler/graph-unittest.h"

namespace v8 {
namespace internal {
namespace compiler {
namespace js_native_context_specialization_unittest {

class JSNativeContextSpecializationTest : public GraphTest {
 public:
  explicit JSNativeContextSpecializationTest(int num_parameters = 1)
      : GraphTest(num_parameters), javascript_(zone()) {}
  ~JSNativeContextSpecializationTest() override {}

 protected:
  JSOperatorBuilder* javascript() { return &javascript_; }

 private:
  JSOperatorBuilder javascript_;
};

TEST_F(JSNativeContextSpecializationTest, GetMaxStringLengthOfString) {
  const size_t str_len = 3;
  const size_t num_len = kMaxDoubleStringLength;

  Node* const str_node = graph()->NewNode(
      common()->HeapConstant(factory()->InternalizeUtf8String("str")));
  EXPECT_EQ(
      JSNativeContextSpecialization::GetMaxStringLength(broker(), str_node),
      str_len);

  Node* const num_node = graph()->NewNode(common()->NumberConstant(10.0 / 3));
  EXPECT_EQ(
      JSNativeContextSpecialization::GetMaxStringLength(broker(), num_node),
      num_len);
}

}  // namespace js_native_context_specialization_unittest
}  // namespace compiler
}  // namespace internal
}  // namespace v8
