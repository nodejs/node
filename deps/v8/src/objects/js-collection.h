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

V8_OBJECT class JSCollection : public JSObject {
 public:
  // The backing hash table.
  inline Tagged<Object> table() const;
  inline void set_table(Tagged<Object> value,
                        WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  static const int kAddFunctionDescriptorIndex = 3;
  static const int kHeaderSize;

 public:
  TaggedMember<Object> table_;
} V8_OBJECT_END;

inline constexpr int JSCollection::kHeaderSize = sizeof(JSCollection);

// The JSSet describes ECMAScript Harmony sets
V8_OBJECT class JSSet final : public JSCollection {
 public:
  static void Initialize(DirectHandle<JSSet> set, Isolate* isolate);
  static void Clear(Isolate* isolate, DirectHandle<JSSet> set);
  void Rehash(Isolate* isolate);

  // Dispatched behavior.
  DECL_PRINTER(JSSet)
  DECL_VERIFIER(JSSet)
} V8_OBJECT_END;

V8_OBJECT class JSSetIterator final
    : public OrderedHashTableIterator<JSSetIterator, OrderedHashSet> {
 public:
  // Dispatched behavior.
  DECL_PRINTER(JSSetIterator)
  DECL_VERIFIER(JSSetIterator)
} V8_OBJECT_END;

// The JSMap describes ECMAScript Harmony maps
V8_OBJECT class JSMap final : public JSCollection {
 public:
  static void Initialize(DirectHandle<JSMap> map, Isolate* isolate);
  static void Clear(Isolate* isolate, DirectHandle<JSMap> map);
  void Rehash(Isolate* isolate);

  // Dispatched behavior.
  DECL_PRINTER(JSMap)
  DECL_VERIFIER(JSMap)
} V8_OBJECT_END;

V8_OBJECT class JSMapIterator final
    : public OrderedHashTableIterator<JSMapIterator, OrderedHashMap> {
 public:
  // Dispatched behavior.
  DECL_PRINTER(JSMapIterator)
  DECL_VERIFIER(JSMapIterator)

  // Returns the current value of the iterator. This should only be called when
  // |HasMore| returns true.
  inline Tagged<Object> CurrentValue();
} V8_OBJECT_END;

// Base class for both JSWeakMap and JSWeakSet
V8_OBJECT class JSWeakCollection : public JSObject {
 public:
  // The backing hash table mapping keys to values.
  inline Tagged<Object> table() const;
  inline void set_table(Tagged<Object> value,
                        WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  static void Initialize(DirectHandle<JSWeakCollection> collection,
                         Isolate* isolate);
  V8_EXPORT_PRIVATE static void Set(DirectHandle<JSWeakCollection> collection,
                                    DirectHandle<Object> key,
                                    DirectHandle<Object> value, int32_t hash);
  static bool Delete(DirectHandle<JSWeakCollection> collection,
                     DirectHandle<Object> key, int32_t hash);
  static DirectHandle<JSArray> GetEntries(DirectHandle<JSWeakCollection> holder,
                                          uint32_t max_entries);

  static const int kAddFunctionDescriptorIndex = 3;
  static const int kHeaderSize;

  // Iterates the function object according to the visiting policy.
  class BodyDescriptorImpl;

  // Visit the whole object.
  using BodyDescriptor = BodyDescriptorImpl;

 public:
  TaggedMember<Object> table_;
} V8_OBJECT_END;

inline constexpr int JSWeakCollection::kHeaderSize = sizeof(JSWeakCollection);

// The JSWeakMap describes ECMAScript Harmony weak maps
V8_OBJECT class JSWeakMap final : public JSWeakCollection {
 public:
  // Dispatched behavior.
  DECL_PRINTER(JSWeakMap)
  DECL_VERIFIER(JSWeakMap)
} V8_OBJECT_END;

// The JSWeakSet describes ECMAScript Harmony weak sets
V8_OBJECT class JSWeakSet final : public JSWeakCollection {
 public:
  // Dispatched behavior.
  DECL_PRINTER(JSWeakSet)
  DECL_VERIFIER(JSWeakSet)
} V8_OBJECT_END;

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_COLLECTION_H_
