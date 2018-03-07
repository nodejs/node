// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_COLLECTION_H_
#define V8_OBJECTS_JS_COLLECTION_H_

#include "src/objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class JSCollection : public JSObject {
 public:
  // [table]: the backing hash table
  DECL_ACCESSORS(table, Object)

  static const int kTableOffset = JSObject::kHeaderSize;
  static const int kSize = kTableOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSCollection);
};

// The JSSet describes EcmaScript Harmony sets
class JSSet : public JSCollection {
 public:
  DECL_CAST(JSSet)

  static void Initialize(Handle<JSSet> set, Isolate* isolate);
  static void Clear(Handle<JSSet> set);

  // Dispatched behavior.
  DECL_PRINTER(JSSet)
  DECL_VERIFIER(JSSet)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSSet);
};

class JSSetIterator
    : public OrderedHashTableIterator<JSSetIterator, OrderedHashSet> {
 public:
  // Dispatched behavior.
  DECL_PRINTER(JSSetIterator)
  DECL_VERIFIER(JSSetIterator)

  DECL_CAST(JSSetIterator)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSSetIterator);
};

// The JSMap describes EcmaScript Harmony maps
class JSMap : public JSCollection {
 public:
  DECL_CAST(JSMap)

  static void Initialize(Handle<JSMap> map, Isolate* isolate);
  static void Clear(Handle<JSMap> map);

  // Dispatched behavior.
  DECL_PRINTER(JSMap)
  DECL_VERIFIER(JSMap)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSMap);
};

class JSMapIterator
    : public OrderedHashTableIterator<JSMapIterator, OrderedHashMap> {
 public:
  // Dispatched behavior.
  DECL_PRINTER(JSMapIterator)
  DECL_VERIFIER(JSMapIterator)

  DECL_CAST(JSMapIterator)

  // Returns the current value of the iterator. This should only be called when
  // |HasMore| returns true.
  inline Object* CurrentValue();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSMapIterator);
};

// Base class for both JSWeakMap and JSWeakSet
class JSWeakCollection : public JSObject {
 public:
  DECL_CAST(JSWeakCollection)

  // [table]: the backing hash table mapping keys to values.
  DECL_ACCESSORS(table, Object)

  // [next]: linked list of encountered weak maps during GC.
  DECL_ACCESSORS(next, Object)

  static void Initialize(Handle<JSWeakCollection> collection, Isolate* isolate);
  static void Set(Handle<JSWeakCollection> collection, Handle<Object> key,
                  Handle<Object> value, int32_t hash);
  static bool Delete(Handle<JSWeakCollection> collection, Handle<Object> key,
                     int32_t hash);
  static Handle<JSArray> GetEntries(Handle<JSWeakCollection> holder,
                                    int max_entries);

  static const int kTableOffset = JSObject::kHeaderSize;
  static const int kNextOffset = kTableOffset + kPointerSize;
  static const int kSize = kNextOffset + kPointerSize;

  // Visiting policy defines whether the table and next collection fields
  // should be visited or not.
  enum BodyVisitingPolicy { kIgnoreWeakness, kRespectWeakness };

  // Iterates the function object according to the visiting policy.
  template <BodyVisitingPolicy>
  class BodyDescriptorImpl;

  // Visit the whole object.
  typedef BodyDescriptorImpl<kIgnoreWeakness> BodyDescriptor;

  // Don't visit table and next collection fields.
  typedef BodyDescriptorImpl<kRespectWeakness> BodyDescriptorWeak;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSWeakCollection);
};

// The JSWeakMap describes EcmaScript Harmony weak maps
class JSWeakMap : public JSWeakCollection {
 public:
  DECL_CAST(JSWeakMap)

  // Dispatched behavior.
  DECL_PRINTER(JSWeakMap)
  DECL_VERIFIER(JSWeakMap)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSWeakMap);
};

// The JSWeakSet describes EcmaScript Harmony weak sets
class JSWeakSet : public JSWeakCollection {
 public:
  DECL_CAST(JSWeakSet)

  // Dispatched behavior.
  DECL_PRINTER(JSWeakSet)
  DECL_VERIFIER(JSWeakSet)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSWeakSet);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_COLLECTION_H_
