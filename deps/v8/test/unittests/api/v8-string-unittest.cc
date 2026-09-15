// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-primitive.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace {

using StringApiTest = TestWithContext;

TEST_F(StringApiTest, IsInStringTable) {
  HandleScope handle_scope(isolate());
  Local<String> foo = String::NewFromUtf8Literal(isolate(), "foo");
  Local<String> foo2 = String::NewFromUtf8Literal(isolate(), "foo");
  Local<String> bar = String::NewFromUtf8Literal(isolate(), "bar");

  // Initially, neither is in the table if they were created with kNormal
  // (default).
  EXPECT_FALSE(foo->IsInStringTable(isolate()));
  EXPECT_FALSE(foo2->IsInStringTable(isolate()));
  EXPECT_FALSE(bar->IsInStringTable(isolate()));

  // Internalize 'foo'.
  Local<String> foo_internalized = foo->InternalizeString(isolate());
  EXPECT_TRUE(foo_internalized->IsInStringTable(isolate()));

  // Also the original string and the alternative string should now be found in
  // the table.
  EXPECT_TRUE(foo->IsInStringTable(isolate()));
  EXPECT_TRUE(foo2->IsInStringTable(isolate()));

  EXPECT_FALSE(bar->IsInStringTable(isolate()));

  // Internalize 'bar'.
  Local<String> bar_internalized = bar->InternalizeString(isolate());
  EXPECT_TRUE(bar_internalized->IsInStringTable(isolate()));
  EXPECT_TRUE(bar->IsInStringTable(isolate()));
}

}  // namespace
}  // namespace v8
