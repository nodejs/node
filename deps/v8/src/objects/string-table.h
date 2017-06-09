// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_STRING_TABLE_H_
#define V8_OBJECTS_STRING_TABLE_H_

#include "src/objects/hash-table.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class StringTableShape : public BaseShape<HashTableKey*> {
 public:
  static inline bool IsMatch(HashTableKey* key, Object* value) {
    return key->IsMatch(value);
  }

  static inline uint32_t Hash(HashTableKey* key) { return key->Hash(); }

  static inline uint32_t HashForObject(HashTableKey* key, Object* object) {
    return key->HashForObject(object);
  }

  static inline Handle<Object> AsHandle(Isolate* isolate, HashTableKey* key);

  static const int kPrefixSize = 0;
  static const int kEntrySize = 1;
};

class SeqOneByteString;

// StringTable.
//
// No special elements in the prefix and the element size is 1
// because only the string itself (the key) needs to be stored.
class StringTable
    : public HashTable<StringTable, StringTableShape, HashTableKey*> {
 public:
  // Find string in the string table. If it is not there yet, it is
  // added. The return value is the string found.
  V8_EXPORT_PRIVATE static Handle<String> LookupString(Isolate* isolate,
                                                       Handle<String> key);
  static Handle<String> LookupKey(Isolate* isolate, HashTableKey* key);
  static String* LookupKeyIfExists(Isolate* isolate, HashTableKey* key);

  // Tries to internalize given string and returns string handle on success
  // or an empty handle otherwise.
  MUST_USE_RESULT static MaybeHandle<String> InternalizeStringIfExists(
      Isolate* isolate, Handle<String> string);

  // Looks up a string that is equal to the given string and returns
  // string handle if it is found, or an empty handle otherwise.
  MUST_USE_RESULT static MaybeHandle<String> LookupStringIfExists(
      Isolate* isolate, Handle<String> str);
  MUST_USE_RESULT static MaybeHandle<String> LookupTwoCharsStringIfExists(
      Isolate* isolate, uint16_t c1, uint16_t c2);
  static Object* LookupStringIfExists_NoAllocate(String* string);

  static void EnsureCapacityForDeserialization(Isolate* isolate, int expected);

  DECLARE_CAST(StringTable)

 private:
  template <bool seq_one_byte>
  friend class JsonParser;

  DISALLOW_IMPLICIT_CONSTRUCTORS(StringTable);
};

class StringSetShape : public BaseShape<String*> {
 public:
  static inline bool IsMatch(String* key, Object* value);
  static inline uint32_t Hash(String* key);
  static inline uint32_t HashForObject(String* key, Object* object);

  static const int kPrefixSize = 0;
  static const int kEntrySize = 1;
};

class StringSet : public HashTable<StringSet, StringSetShape, String*> {
 public:
  static Handle<StringSet> New(Isolate* isolate);
  static Handle<StringSet> Add(Handle<StringSet> blacklist,
                               Handle<String> name);
  bool Has(Handle<String> name);

  DECLARE_CAST(StringSet)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_STRING_TABLE_H_
