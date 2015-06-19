// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_IDENTITY_MAP_H_
#define V8_HEAP_IDENTITY_MAP_H_

#include "src/handles.h"

namespace v8 {
namespace internal {

class Heap;

// Base class of identity maps contains shared code for all template
// instantions.
class IdentityMapBase {
 public:
  // Enable or disable concurrent mode for this map. Concurrent mode implies
  // taking the heap's relocation lock during most operations.
  void SetConcurrent(bool concurrent) { concurrent_ = concurrent; }

 protected:
  // Allow Tester to access internals, including changing the address of objects
  // within the {keys_} array in order to simulate a moving GC.
  friend class IdentityMapTester;

  typedef void** RawEntry;

  IdentityMapBase(Heap* heap, Zone* zone)
      : heap_(heap),
        zone_(zone),
        concurrent_(false),
        gc_counter_(-1),
        size_(0),
        mask_(0),
        keys_(nullptr),
        values_(nullptr) {}
  ~IdentityMapBase();

  RawEntry GetEntry(Handle<Object> key);
  RawEntry FindEntry(Handle<Object> key);

 private:
  // Internal implementation should not be called directly by subclasses.
  int LookupIndex(Object* address);
  int InsertIndex(Object* address);
  void Rehash();
  void Resize();
  RawEntry Lookup(Handle<Object> key);
  RawEntry Insert(Handle<Object> key);
  int Hash(Object* address);

  Heap* heap_;
  Zone* zone_;
  bool concurrent_;
  int gc_counter_;
  int size_;
  int mask_;
  Object** keys_;
  void** values_;
};

// Implements an identity map from object addresses to a given value type {V}.
// The map is robust w.r.t. garbage collection by synchronization with the
// supplied {heap}.
//  * Keys are treated as strong roots.
//  * SMIs are valid keys, except SMI #0.
//  * The value type {V} must be reinterpret_cast'able to {void*}
//  * The value type {V} must not be a heap type.
template <typename V>
class IdentityMap : public IdentityMapBase {
 public:
  IdentityMap(Heap* heap, Zone* zone) : IdentityMapBase(heap, zone) {}

  // Searches this map for the given key using the object's address
  // as the identity, returning:
  //    found => a pointer to the storage location for the value
  //    not found => a pointer to a new storage location for the value
  V* Get(Handle<Object> key) { return reinterpret_cast<V*>(GetEntry(key)); }

  // Searches this map for the given key using the object's address
  // as the identity, returning:
  //    found => a pointer to the storage location for the value
  //    not found => {nullptr}
  V* Find(Handle<Object> key) { return reinterpret_cast<V*>(FindEntry(key)); }

  // Set the value for the given key.
  void Set(Handle<Object> key, V value) {
    *(reinterpret_cast<V*>(GetEntry(key))) = value;
  }
};
}
}  // namespace v8::internal

#endif  // V8_HEAP_IDENTITY_MAP_H_
