// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_STRING_TABLE_H_
#define V8_OBJECTS_STRING_TABLE_H_

#include "src/objects/hash-table.h"
#include "src/roots/roots.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class StringTableKey {
 public:
  virtual ~StringTableKey() {}
  inline StringTableKey(uint32_t hash_field, int length);

  virtual Handle<String> AsHandle(Isolate* isolate) = 0;
  uint32_t hash_field() const {
    DCHECK_NE(0, hash_field_);
    return hash_field_;
  }

  virtual bool IsMatch(String string) = 0;
  inline uint32_t hash() const;
  int length() const { return length_; }

 protected:
  inline void set_hash_field(uint32_t hash_field);

 private:
  uint32_t hash_field_ = 0;
  int length_;
};

class V8_EXPORT_PRIVATE StringTableShape : public BaseShape<StringTableKey*> {
 public:
  static inline bool IsMatch(Key key, Object value);

  static inline uint32_t Hash(ReadOnlyRoots roots, Key key);

  static inline uint32_t HashForObject(ReadOnlyRoots roots, Object object);

  static inline Handle<Object> AsHandle(Isolate* isolate, Key key);

  static inline Handle<Map> GetMap(ReadOnlyRoots roots);

  static const int kPrefixSize = 0;
  static const int kEntrySize = 1;
};

class SeqOneByteString;

EXTERN_DECLARE_HASH_TABLE(StringTable, StringTableShape)

// StringTable.
//
// No special elements in the prefix and the element size is 1
// because only the string itself (the key) needs to be stored.
class V8_EXPORT_PRIVATE StringTable
    : public HashTable<StringTable, StringTableShape> {
 public:
  // Find string in the string table. If it is not there yet, it is
  // added. The return value is the string found.
  static Handle<String> LookupString(Isolate* isolate, Handle<String> key);
  template <typename StringTableKey>
  static Handle<String> LookupKey(Isolate* isolate, StringTableKey* key);
  static Handle<String> AddKeyNoResize(Isolate* isolate, StringTableKey* key);

  // Shrink the StringTable if it's very empty (kMaxEmptyFactor) to avoid the
  // performance overhead of re-allocating the StringTable over and over again.
  static Handle<StringTable> CautiousShrink(Isolate* isolate,
                                            Handle<StringTable> table);

  // {raw_string} must be a tagged String pointer.
  // Returns a tagged pointer: either an internalized string, or a Smi
  // sentinel.
  static Address LookupStringIfExists_NoAllocate(Isolate* isolate,
                                                 Address raw_string);

  static void EnsureCapacityForDeserialization(Isolate* isolate, int expected);

  DECL_CAST(StringTable)

  static const int kMaxEmptyFactor = 4;
  static const int kMinCapacity = 2048;
  static const int kMinShrinkCapacity = kMinCapacity;

 private:
  template <typename char_type>
  friend class JsonParser;

  OBJECT_CONSTRUCTORS(StringTable, HashTable<StringTable, StringTableShape>);
};

class StringSetShape : public BaseShape<String> {
 public:
  static inline bool IsMatch(String key, Object value);
  static inline uint32_t Hash(ReadOnlyRoots roots, String key);
  static inline uint32_t HashForObject(ReadOnlyRoots roots, Object object);

  static const int kPrefixSize = 0;
  static const int kEntrySize = 1;
};

EXTERN_DECLARE_HASH_TABLE(StringSet, StringSetShape)

class StringSet : public HashTable<StringSet, StringSetShape> {
 public:
  V8_EXPORT_PRIVATE static Handle<StringSet> New(Isolate* isolate);
  V8_EXPORT_PRIVATE static Handle<StringSet> Add(Isolate* isolate,
                                                 Handle<StringSet> blacklist,
                                                 Handle<String> name);
  V8_EXPORT_PRIVATE bool Has(Isolate* isolate, Handle<String> name);

  DECL_CAST(StringSet)
  OBJECT_CONSTRUCTORS(StringSet, HashTable<StringSet, StringSetShape>);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_STRING_TABLE_H_
