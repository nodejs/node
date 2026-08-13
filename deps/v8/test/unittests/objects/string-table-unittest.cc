// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/string-table.h"

#include "src/handles/handles-inl.h"
#include "src/objects/internal-index.h"
#include "src/objects/string-inl.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {

using StringTableTest = TestWithIsolate;

TEST_F(StringTableTest, HasString) {
  DirectHandle<String> foo = factory()->NewStringFromStaticChars("foo");
  DirectHandle<String> foo2 = factory()->NewStringFromStaticChars("foo");
  DirectHandle<String> bar = factory()->NewStringFromStaticChars("bar");

  StringTable* string_table = isolate()->string_table();

  // Initially, neither is in the table.
  EXPECT_FALSE(string_table->HasString(isolate(), foo));
  EXPECT_FALSE(string_table->HasString(isolate(), foo2));
  EXPECT_FALSE(string_table->HasString(isolate(), bar));

  // Internalize 'foo'.
  DirectHandle<String> foo_internalized = factory()->InternalizeString(foo);
  EXPECT_TRUE(IsInternalizedString(*foo_internalized));

  // Now 'foo' should be in the table, but 'bar' should not.
  EXPECT_TRUE(string_table->HasString(isolate(), foo));
  EXPECT_TRUE(string_table->HasString(isolate(), foo2));
  EXPECT_FALSE(string_table->HasString(isolate(), bar));

  // Check with the internalized handle itself.
  EXPECT_TRUE(string_table->HasString(isolate(), foo_internalized));

  // Internalize 'bar'.
  DirectHandle<String> bar_internalized = factory()->InternalizeString(bar);
  EXPECT_TRUE(IsInternalizedString(*bar_internalized));

  // Both should be in the table now.
  EXPECT_TRUE(string_table->HasString(isolate(), foo));
  EXPECT_TRUE(string_table->HasString(isolate(), bar));
}

}  // namespace internal
}  // namespace v8
