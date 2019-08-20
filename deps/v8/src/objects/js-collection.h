// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_COLLECTION_H_
#define V8_OBJECTS_JS_COLLECTION_H_

#include "src/objects/js-collection-iterator.h"
#include "src/objects/objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class OrderedHashSet;
class OrderedHashMap;

class JSCollection : public JSObject {
 public:
  DECL_CAST(JSCollection)

  // [table]: the backing hash table
  DECL_ACCESSORS(table, Object)

  // Layout description.
  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize,
                                TORQUE_GENERATED_JSCOLLECTION_FIELDS)

  static const int kAddFunctionDescriptorIndex = 3;

  DECL_VERIFIER(JSCollection)

  OBJECT_CONSTRUCTORS(JSCollection, JSObject);
};

// The JSSet describes EcmaScript Harmony sets
class JSSet : public JSCollection {
 public:
  DECL_CAST(JSSet)

  static void Initialize(Handle<JSSet> set, Isolate* isolate);
  static void Clear(Isolate* isolate, Handle<JSSet> set);

  // Dispatched behavior.
  DECL_PRINTER(JSSet)
  DECL_VERIFIER(JSSet)
  DEFINE_FIELD_OFFSET_CONSTANTS(JSCollection::kHeaderSize,
                                TORQUE_GENERATED_JSWEAK_SET_FIELDS)

  OBJECT_CONSTRUCTORS(JSSet, JSCollection);
};

class JSSetIterator
    : public OrderedHashTableIterator<JSSetIterator, OrderedHashSet> {
 public:
  // Dispatched behavior.
  DECL_PRINTER(JSSetIterator)
  DECL_VERIFIER(JSSetIterator)

  DECL_CAST(JSSetIterator)

  OBJECT_CONSTRUCTORS(JSSetIterator,
                      OrderedHashTableIterator<JSSetIterator, OrderedHashSet>);
};

// The JSMap describes EcmaScript Harmony maps
class JSMap : public JSCollection {
 public:
  DECL_CAST(JSMap)

  static void Initialize(Handle<JSMap> map, Isolate* isolate);
  static void Clear(Isolate* isolate, Handle<JSMap> map);

  // Dispatched behavior.
  DECL_PRINTER(JSMap)
  DECL_VERIFIER(JSMap)
  DEFINE_FIELD_OFFSET_CONSTANTS(JSCollection::kHeaderSize,
                                TORQUE_GENERATED_JSWEAK_MAP_FIELDS)

  OBJECT_CONSTRUCTORS(JSMap, JSCollection);
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
  inline Object CurrentValue();

  OBJECT_CONSTRUCTORS(JSMapIterator,
                      OrderedHashTableIterator<JSMapIterator, OrderedHashMap>);
};

// Base class for both JSWeakMap and JSWeakSet
class JSWeakCollection : public JSObject {
 public:
  DECL_CAST(JSWeakCollection)

  // [table]: the backing hash table mapping keys to values.
  DECL_ACCESSORS(table, Object)

  static void Initialize(Handle<JSWeakCollection> collection, Isolate* isolate);
  V8_EXPORT_PRIVATE static void Set(Handle<JSWeakCollection> collection,
                                    Handle<Object> key, Handle<Object> value,
                                    int32_t hash);
  static bool Delete(Handle<JSWeakCollection> collection, Handle<Object> key,
                     int32_t hash);
  static Handle<JSArray> GetEntries(Handle<JSWeakCollection> holder,
                                    int max_entries);

  DECL_VERIFIER(JSWeakCollection)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize,
                                TORQUE_GENERATED_JSWEAK_COLLECTION_FIELDS)

  static const int kAddFunctionDescriptorIndex = 3;

  // Iterates the function object according to the visiting policy.
  class BodyDescriptorImpl;

  // Visit the whole object.
  using BodyDescriptor = BodyDescriptorImpl;

  static const int kSizeOfAllWeakCollections = kHeaderSize;

  OBJECT_CONSTRUCTORS(JSWeakCollection, JSObject);
};

// The JSWeakMap describes EcmaScript Harmony weak maps
class JSWeakMap : public JSWeakCollection {
 public:
  DECL_CAST(JSWeakMap)

  // Dispatched behavior.
  DECL_PRINTER(JSWeakMap)
  DECL_VERIFIER(JSWeakMap)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSWeakCollection::kHeaderSize,
                                TORQUE_GENERATED_JSWEAK_MAP_FIELDS)
  STATIC_ASSERT(kSize == kSizeOfAllWeakCollections);
  OBJECT_CONSTRUCTORS(JSWeakMap, JSWeakCollection);
};

// The JSWeakSet describes EcmaScript Harmony weak sets
class JSWeakSet : public JSWeakCollection {
 public:
  DECL_CAST(JSWeakSet)

  // Dispatched behavior.
  DECL_PRINTER(JSWeakSet)
  DECL_VERIFIER(JSWeakSet)
  DEFINE_FIELD_OFFSET_CONSTANTS(JSWeakCollection::kHeaderSize,
                                TORQUE_GENERATED_JSWEAK_SET_FIELDS)
  STATIC_ASSERT(kSize == kSizeOfAllWeakCollections);

  OBJECT_CONSTRUCTORS(JSWeakSet, JSWeakCollection);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_COLLECTION_H_
