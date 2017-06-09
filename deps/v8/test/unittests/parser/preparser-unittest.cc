// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api.h"
#include "src/objects-inl.h"
#include "test/unittests/test-helpers.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

class PreParserTest : public TestWithContext {
 public:
  PreParserTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PreParserTest);
};

TEST_F(PreParserTest, LazyFunctionLength) {
  const char* script_source = "function lazy(a, b, c) { } lazy";

  Handle<Object> lazy_object = test::RunJS(isolate(), script_source);

  Handle<SharedFunctionInfo> shared(
      Handle<JSFunction>::cast(lazy_object)->shared(), i_isolate());
  CHECK_EQ(shared->length(), SharedFunctionInfo::kInvalidLength);

  const char* get_length_source = "lazy.length";

  Handle<Object> length = test::RunJS(isolate(), get_length_source);
  CHECK(length->IsSmi());
  int32_t value;
  CHECK(length->ToInt32(&value));
  CHECK_EQ(3, value);
}

}  // namespace internal
}  // namespace v8
