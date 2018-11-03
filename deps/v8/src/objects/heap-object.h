// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_HEAP_OBJECT_H_
#define V8_OBJECTS_HEAP_OBJECT_H_

#include "src/globals.h"

#include "src/objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// This is the new way to represent the Object class. It is temporarily
// separate to allow an incremental transition.
// For a design overview, see https://goo.gl/Ph4CGz.
class ObjectPtr {
 public:
  ObjectPtr() : ptr_(kNullAddress) {}
  explicit ObjectPtr(Address ptr) : ptr_(ptr) {}

  // Enable incremental transition.
  operator Object*() const { return reinterpret_cast<Object*>(ptr()); }

  bool operator==(const ObjectPtr other) const {
    return this->ptr() == other.ptr();
  }
  bool operator!=(const ObjectPtr other) const {
    return this->ptr() != other.ptr();
  }

  // Returns the tagged "(heap) object pointer" representation of this object.
  Address ptr() const { return ptr_; }

  ObjectPtr* operator->() { return this; }
  const ObjectPtr* operator->() const { return this; }

#define IS_TYPE_FUNCTION_DECL(Type) V8_INLINE bool Is##Type() const;
  OBJECT_TYPE_LIST(IS_TYPE_FUNCTION_DECL)
  HEAP_OBJECT_TYPE_LIST(IS_TYPE_FUNCTION_DECL)
#undef IS_TYPE_FUNCTION_DECL

#ifdef VERIFY_HEAP
  void ObjectVerify(Isolate* isolate) {
    reinterpret_cast<Object*>(ptr())->ObjectVerify(isolate);
  }
#endif

 private:
  Address ptr_;
};

// Replacement for HeapObject; temporarily separate for incremental transition:
class HeapObjectPtr : public ObjectPtr {
 public:
  inline Map* map() const;

  inline ObjectSlot map_slot();

  inline WriteBarrierMode GetWriteBarrierMode(
      const DisallowHeapAllocation& promise);

  // Enable incremental transition.
  operator HeapObject*() { return reinterpret_cast<HeapObject*>(ptr()); }
  operator const HeapObject*() const {
    return reinterpret_cast<const HeapObject*>(ptr());
  }

  bool IsHeapObject() const { return true; }
  bool IsHeapObjectPtr() const { return true; }

#define IS_TYPE_FUNCTION_DECL(Type) V8_INLINE bool Is##Type() const;
  HEAP_OBJECT_TYPE_LIST(IS_TYPE_FUNCTION_DECL)
#undef IS_TYPE_FUNCTION_DECL

  // Untagged aligned address.
  inline Address address() const { return ptr() - kHeapObjectTag; }

  inline ObjectSlot RawField(int byte_offset) const;

#ifdef OBJECT_PRINT
  void PrintHeader(std::ostream& os, const char* id);  // NOLINT
#endif

  static const int kMapOffset = HeapObject::kMapOffset;

  OBJECT_CONSTRUCTORS(HeapObjectPtr)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_HEAP_OBJECT_H_
