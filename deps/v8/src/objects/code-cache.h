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

// The key in the code cache hash table consists of the property name and the
// code object. The actual match is on the name and the code flags. If a key
// is created using the flags and not a code object it can only be used for
// lookup not to create a new entry.
class CodeCacheHashTableKey final {
 public:
  CodeCacheHashTableKey(Handle<Name> name, Code::Flags flags)
      : name_(name), flags_(flags), code_() {
    DCHECK(name_->IsUniqueName());
  }

  CodeCacheHashTableKey(Handle<Name> name, Handle<Code> code)
      : name_(name), flags_(code->flags()), code_(code) {
    DCHECK(name_->IsUniqueName());
  }

  bool IsMatch(Object* other) {
    DCHECK(other->IsFixedArray());
    FixedArray* pair = FixedArray::cast(other);
    Name* name = Name::cast(pair->get(0));
    Code::Flags flags = Code::cast(pair->get(1))->flags();
    if (flags != flags_) return false;
    DCHECK(name->IsUniqueName());
    return *name_ == name;
  }

  static uint32_t NameFlagsHashHelper(Name* name, Code::Flags flags) {
    return name->Hash() ^ flags;
  }

  uint32_t Hash() { return NameFlagsHashHelper(*name_, flags_); }

  MUST_USE_RESULT Handle<Object> AsHandle(Isolate* isolate) {
    Handle<Code> code = code_.ToHandleChecked();
    Handle<FixedArray> pair = isolate->factory()->NewFixedArray(2);
    pair->set(0, *name_);
    pair->set(1, *code);
    return pair;
  }

 private:
  Handle<Name> name_;
  Code::Flags flags_;
  // TODO(jkummerow): We should be able to get by without this.
  MaybeHandle<Code> code_;
};

class CodeCacheHashTableShape : public BaseShape<CodeCacheHashTableKey*> {
 public:
  static inline bool IsMatch(CodeCacheHashTableKey* key, Object* value) {
    return key->IsMatch(value);
  }

  static inline uint32_t Hash(Isolate* isolate, CodeCacheHashTableKey* key) {
    return key->Hash();
  }

  static inline uint32_t HashForObject(Isolate* isolate, Object* object) {
    FixedArray* pair = FixedArray::cast(object);
    Name* name = Name::cast(pair->get(0));
    Code* code = Code::cast(pair->get(1));
    return CodeCacheHashTableKey::NameFlagsHashHelper(name, code->flags());
  }

  static inline Handle<Object> AsHandle(Isolate* isolate,
                                        CodeCacheHashTableKey* key);

  static const int kPrefixSize = 0;
  // The both the key (name + flags) and value (code object) can be derived from
  // the fixed array that stores both the name and code.
  // TODO(verwaest): Don't allocate a fixed array but inline name and code.
  // Rewrite IsMatch to get table + index as input rather than just the raw key.
  static const int kEntrySize = 1;
};

class CodeCacheHashTable
    : public HashTable<CodeCacheHashTable, CodeCacheHashTableShape> {
 public:
  static Handle<CodeCacheHashTable> Put(Handle<CodeCacheHashTable> table,
                                        Handle<Name> name, Handle<Code> code);

  Code* Lookup(Name* name, Code::Flags flags);

  DECL_CAST(CodeCacheHashTable)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(CodeCacheHashTable);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_CODE_CACHE_H_
