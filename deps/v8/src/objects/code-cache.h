// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_CODE_CACHE_H_
#define V8_OBJECTS_CODE_CACHE_H_

#include "src/objects/hash-table.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class CodeCacheHashTableShape : public BaseShape<HashTableKey*> {
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
  // The both the key (name + flags) and value (code object) can be derived from
  // the fixed array that stores both the name and code.
  // TODO(verwaest): Don't allocate a fixed array but inline name and code.
  // Rewrite IsMatch to get table + index as input rather than just the raw key.
  static const int kEntrySize = 1;
};

class CodeCacheHashTable
    : public HashTable<CodeCacheHashTable, CodeCacheHashTableShape,
                       HashTableKey*> {
 public:
  static Handle<CodeCacheHashTable> Put(Handle<CodeCacheHashTable> table,
                                        Handle<Name> name, Handle<Code> code);

  Code* Lookup(Name* name, Code::Flags flags);

  DECLARE_CAST(CodeCacheHashTable)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(CodeCacheHashTable);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_CODE_CACHE_H_
