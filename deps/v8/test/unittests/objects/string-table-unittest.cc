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
          factory()->InternalizeUtf8String(std::string_view(string));
      EXPECT_TRUE(IsInternalizedString(*a));
      DirectHandle<String> b = factory()->InternalizeUtf8String(string);
      EXPECT_EQ(*b, *a);
      EXPECT_TRUE(b->IsOneByteEqualTo(string));
      b = factory()->InternalizeUtf8String(std::string_view(string));
      EXPECT_EQ(*b, *a);
      EXPECT_TRUE(b->IsOneByteEqualTo(string));
    }
  };

  check_internalized_strings(kTestStrings);
  // Re-checking lookups of already internalized strings.
  check_internalized_strings(kTestStrings);
}

namespace {

DirectHandle<SeqTwoByteString> NewTwoByteStringFromAscii(
    Isolate* isolate, std::string_view str,
    AllocationType allocation = AllocationType::kYoung) {
  int length = static_cast<int>(str.length());
  DirectHandle<SeqTwoByteString> result =
      isolate->factory()
          ->NewRawTwoByteString(length, allocation)
          .ToHandleChecked();
  DisallowGarbageCollection no_gc;
  CopyChars(result->GetChars(no_gc), str.data(), length);
  return result;
}

}  // namespace

TEST_F(StringTableTest, InternalizePreHashedYoungTwoByteWithOneByteContent) {
  DirectHandle<String> str =
      NewTwoByteStringFromAscii(isolate(), "prehashed_young_ascii");
  EXPECT_TRUE(str->IsTwoByteRepresentation());
  str->EnsureHash();
  EXPECT_TRUE(str->HasHashCode());

  DirectHandle<String> internalized = factory()->InternalizeString(str);
  EXPECT_TRUE(IsInternalizedString(*internalized));
  EXPECT_TRUE(internalized->IsOneByteRepresentation());
}

TEST_F(StringTableTest, InternalizePreHashedOldTwoByteWithOneByteContent) {
  // Old-generation SeqTwoByteString would be in-place internalizable if not
  // canonicalized to one-byte.
  DirectHandle<String> str = NewTwoByteStringFromAscii(
      isolate(), "prehashed_old_ascii", AllocationType::kOld);
  EXPECT_TRUE(str->IsTwoByteRepresentation());
  str->EnsureHash();
  EXPECT_TRUE(str->HasHashCode());

  DirectHandle<String> internalized = factory()->InternalizeString(str);
  EXPECT_TRUE(IsInternalizedString(*internalized));
  EXPECT_TRUE(internalized->IsOneByteRepresentation());
  EXPECT_TRUE(IsThinString(*str));
  EXPECT_EQ(Cast<ThinString>(*str)->actual(), *internalized);
}

TEST_F(StringTableTest, InternalizeHasStringHashedTwoByteWithOneByteContent) {
  DirectHandle<String> str =
      NewTwoByteStringFromAscii(isolate(), "hashed_via_has_string");
  EXPECT_FALSE(isolate()->string_table()->HasString(isolate(), str));
  EXPECT_TRUE(str->HasHashCode());

  DirectHandle<String> internalized = factory()->InternalizeString(str);
  EXPECT_TRUE(IsInternalizedString(*internalized));
  EXPECT_TRUE(internalized->IsOneByteRepresentation());
}

TEST_F(StringTableTest, InternalizeLongTwoByteWithOneByteContent) {
  // Strings longer than kMaxHashCalcLength use a trivial hash without scanning
  // content in EnsureRawHash.
  std::string long_ascii(String::kMaxHashCalcLength + 10, 'a');
  DirectHandle<String> str =
      NewTwoByteStringFromAscii(isolate(), long_ascii, AllocationType::kOld);
  EXPECT_TRUE(str->IsTwoByteRepresentation());

  DirectHandle<String> internalized = factory()->InternalizeString(str);
  EXPECT_TRUE(IsInternalizedString(*internalized));
  EXPECT_TRUE(internalized->IsOneByteRepresentation());
}

TEST_F(StringTableTest, InternalizeTwoByteIntegerIndex) {
  DirectHandle<String> unhashed_idx =
      NewTwoByteStringFromAscii(isolate(), "12345", AllocationType::kOld);
  EXPECT_TRUE(unhashed_idx->IsTwoByteRepresentation());
  DirectHandle<String> internalized_unhashed =
      factory()->InternalizeString(unhashed_idx);
  EXPECT_TRUE(IsInternalizedString(*internalized_unhashed));
  EXPECT_TRUE(internalized_unhashed->IsOneByteRepresentation());
  EXPECT_TRUE(Name::IsIntegerIndex(internalized_unhashed->raw_hash_field()));

  DirectHandle<String> prehashed_idx =
      NewTwoByteStringFromAscii(isolate(), "67890", AllocationType::kOld);
  EXPECT_TRUE(prehashed_idx->IsTwoByteRepresentation());
  prehashed_idx->EnsureHash();
  EXPECT_TRUE(Name::IsIntegerIndex(prehashed_idx->raw_hash_field()));
  DirectHandle<String> internalized_prehashed =
      factory()->InternalizeString(prehashed_idx);
  EXPECT_TRUE(IsInternalizedString(*internalized_prehashed));
  EXPECT_TRUE(internalized_prehashed->IsOneByteRepresentation());
  EXPECT_TRUE(Name::IsIntegerIndex(internalized_prehashed->raw_hash_field()));
}

TEST_F(StringTableTest, InternalizePreHashedSlicedTwoByteWithOneByteParent) {
  // When parent is internalized to 1-byte, slice->IsOneByteRepresentation()
  // remains false while slice->GetFlatContent().IsOneByte() becomes true.
  DirectHandle<String> parent = NewTwoByteStringFromAscii(
      isolate(), "0123456789abcdefghij0123456789", AllocationType::kOld);
  DirectHandle<String> slice = factory()->NewProperSubString(parent, 1, 25);
  EXPECT_TRUE(IsSlicedString(*slice));
  EXPECT_TRUE(slice->IsTwoByteRepresentation());
  slice->EnsureHash();

  DirectHandle<String> internalized_parent =
      factory()->InternalizeString(parent);
  EXPECT_TRUE(internalized_parent->IsOneByteRepresentation());
  EXPECT_FALSE(slice->IsOneByteRepresentation());

  DirectHandle<String> internalized_slice = factory()->InternalizeString(slice);
  EXPECT_TRUE(IsInternalizedString(*internalized_slice));
  EXPECT_TRUE(internalized_slice->IsOneByteRepresentation());
}

TEST_F(StringTableTest, InternalizePreHashedTwoByteWithTwoByteContent) {
  DirectHandle<SeqTwoByteString> str =
      factory()->NewRawTwoByteString(4, AllocationType::kOld).ToHandleChecked();
  {
    DisallowGarbageCollection no_gc;
    uint16_t* chars = str->GetChars(no_gc);
    chars[0] = 'a';
    chars[1] = 0x1234;
    chars[2] = 'b';
    chars[3] = 'c';
  }
  str->EnsureHash();

  DirectHandle<String> internalized = factory()->InternalizeString(str);
  EXPECT_TRUE(IsInternalizedString(*internalized));
  EXPECT_TRUE(internalized->IsTwoByteRepresentation());
}

}  // namespace internal
}  // namespace v8
