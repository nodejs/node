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

class StringTableKey : public HashTableKey {
 public:
  explicit inline StringTableKey(uint32_t hash_field);

  virtual Handle<String> AsHandle(Isolate* isolate) = 0;
  uint32_t HashField() const {
    DCHECK_NE(0, hash_field_);
    return hash_field_;
  }

 protected:
  inline void set_hash_field(uint32_t hash_field);

 private:
  uint32_t hash_field_ = 0;
};

class StringTableShape : public BaseShape<StringTableKey*> {
 public:
  static inline bool IsMatch(Key key, Object* value) {
    return key->IsMatch(value);
  }

  static inline uint32_t Hash(Isolate* isolate, Key key) { return key->Hash(); }

  static inline uint32_t HashForObject(Isolate* isolate, Object* object);

  static inline Handle<Object> AsHandle(Isolate* isolate, Key key);

  static inline RootIndex GetMapRootIndex();

  static const int kPrefixSize = 0;
  static const int kEntrySize = 1;
};

class SeqOneByteString;

// StringTable.
//
// No special elements in the prefix and the element size is 1
// because only the string itself (the key) needs to be stored.
class StringTable : public HashTable<StringTable, StringTableShape> {
 public:
  // Find string in the string table. If it is not there yet, it is
  // added. The return value is the string found.
  V8_EXPORT_PRIVATE static Handle<String> LookupString(Isolate* isolate,
                                                       Handle<String> key);
  static Handle<String> LookupKey(Isolate* isolate, StringTableKey* key);
  static Handle<String> AddKeyNoResize(Isolate* isolate, StringTableKey* key);
  static String* ForwardStringIfExists(Isolate* isolate, StringTableKey* key,
                                       String* string);

  // Shink the StringTable if it's very empty (kMaxEmptyFactor) to avoid the
  // performance overhead of re-allocating the StringTable over and over again.
  static Handle<StringTable> CautiousShrink(Isolate* isolate,
                                            Handle<StringTable> table);

  // Looks up a string that is equal to the given string and returns
  // string handle if it is found, or an empty handle otherwise.
  V8_WARN_UNUSED_RESULT static MaybeHandle<String> LookupTwoCharsStringIfExists(
      Isolate* isolate, uint16_t c1, uint16_t c2);
  static Object* LookupStringIfExists_NoAllocate(Isolate* isolate,
                                                 String* string);

  static void EnsureCapacityForDeserialization(Isolate* isolate, int expected);

  DECL_CAST(StringTable)

  static const int kMaxEmptyFactor = 4;
  static const int kMinCapacity = 2048;
  static const int kMinShrinkCapacity = kMinCapacity;

 private:
  template <bool seq_one_byte>
  friend class JsonParser;

  DISALLOW_IMPLICIT_CONSTRUCTORS(StringTable);
};

class StringSetShape : public BaseShape<String*> {
 public:
  static inline bool IsMatch(String* key, Object* value);
  static inline uint32_t Hash(Isolate* isolate, String* key);
  static inline uint32_t HashForObject(Isolate* isolate, Object* object);

  static const int kPrefixSize = 0;
  static const int kEntrySize = 1;
};

class StringSet : public HashTable<StringSet, StringSetShape> {
 public:
  static Handle<StringSet> New(Isolate* isolate);
  static Handle<StringSet> Add(Isolate* isolate, Handle<StringSet> blacklist,
                               Handle<String> name);
  bool Has(Isolate* isolate, Handle<String> name);

  DECL_CAST(StringSet)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_STRING_TABLE_H_
