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

TEST_F(StringTableTest, InternalizeUtf8String) {
  static const char* const kTestStrings[] = {
      "abstract",   "boolean",      "break",      "byte",    "case",
      "catch",      "char",         "class",      "const",   "continue",
      "debugger",   "default",      "delete",     "do",      "double",
      "else",       "enum",         "export",     "extends", "false",
      "final",      "finally",      "float",      "for",     "function",
      "goto",       "if",           "implements", "import",  "in",
      "instanceof", "int",          "interface",  "long",    "native",
      "new",        "null",         "package",    "private", "protected",
      "public",     "return",       "short",      "static",  "super",
      "switch",     "synchronized", "this",       "throw",   "throws",
      "transient",  "true",         "try",        "typeof",  "var",
      "void",       "volatile",     "while",      "with",    nullptr};

  auto check_internalized_strings = [this](const char* const* strings) {
    for (const char* const* ptr = strings; *ptr != nullptr; ++ptr) {
      const char* string = *ptr;
      HandleScope scope(isolate());
      DirectHandle<String> a =
          factory()->InternalizeUtf8String(base::CStrVector(string));
      EXPECT_TRUE(IsInternalizedString(*a));
      DirectHandle<String> b = factory()->InternalizeUtf8String(string);
      EXPECT_EQ(*b, *a);
      EXPECT_TRUE(b->IsOneByteEqualTo(base::CStrVector(string)));
      b = factory()->InternalizeUtf8String(base::CStrVector(string));
      EXPECT_EQ(*b, *a);
      EXPECT_TRUE(b->IsOneByteEqualTo(base::CStrVector(string)));
    }
  };

  check_internalized_strings(kTestStrings);
  // Re-checking lookups of already internalized strings.
  check_internalized_strings(kTestStrings);
}

}  // namespace internal
}  // namespace v8
