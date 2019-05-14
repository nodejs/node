// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MAYBE_OBJECT_H_
#define V8_OBJECTS_MAYBE_OBJECT_H_

#include "include/v8-internal.h"
#include "include/v8.h"
#include "src/globals.h"
#include "src/objects.h"
#include "src/objects/smi.h"

namespace v8 {
namespace internal {

class HeapObject;
class Isolate;
class StringStream;

// A MaybeObject is either a SMI, a strong reference to a HeapObject, a weak
// reference to a HeapObject, or a cleared weak reference. It's used for
// implementing in-place weak references (see design doc: goo.gl/j6SdcK )
class MaybeObject {
 public:
  MaybeObject() : ptr_(kNullAddress) {}
  explicit MaybeObject(Address ptr) : ptr_(ptr) {}

  bool operator==(const MaybeObject& other) const { return ptr_ == other.ptr_; }
  bool operator!=(const MaybeObject& other) const { return ptr_ != other.ptr_; }

  Address ptr() const { return ptr_; }

  // Enable incremental transition of client code.
  MaybeObject* operator->() { return this; }
  const MaybeObject* operator->() const { return this; }

  bool IsSmi() const { return HAS_SMI_TAG(ptr_); }
  inline bool ToSmi(Smi* value);
  inline Smi ToSmi() const;

  bool IsCleared() const {
    return static_cast<uint32_t>(ptr_) == kClearedWeakHeapObjectLower32;
  }

  inline bool IsStrongOrWeak() const;
  inline bool IsStrong() const;

  // If this MaybeObject is a strong pointer to a HeapObject, returns true and
  // sets *result. Otherwise returns false.
  inline bool GetHeapObjectIfStrong(HeapObject* result) const;

  // DCHECKs that this MaybeObject is a strong pointer to a HeapObject and
  // returns the HeapObject.
  inline HeapObject GetHeapObjectAssumeStrong() const;

  inline bool IsWeak() const;
  inline bool IsWeakOrCleared() const;

  // If this MaybeObject is a weak pointer to a HeapObject, returns true and
  // sets *result. Otherwise returns false.
  inline bool GetHeapObjectIfWeak(HeapObject* result) const;

  // DCHECKs that this MaybeObject is a weak pointer to a HeapObject and
  // returns the HeapObject.
  inline HeapObject GetHeapObjectAssumeWeak() const;

  // If this MaybeObject is a strong or weak pointer to a HeapObject, returns
  // true and sets *result. Otherwise returns false.
  inline bool GetHeapObject(HeapObject* result) const;
  inline bool GetHeapObject(HeapObject* result,
                            HeapObjectReferenceType* reference_type) const;

  // DCHECKs that this MaybeObject is a strong or a weak pointer to a HeapObject
  // and returns the HeapObject.
  inline HeapObject GetHeapObject() const;

  // DCHECKs that this MaybeObject is a strong or a weak pointer to a HeapObject
  // or a SMI and returns the HeapObject or SMI.
  inline Object GetHeapObjectOrSmi() const;

  inline bool IsObject() const;
  template <typename T>
  T cast() const {
    DCHECK(!HasWeakHeapObjectTag(ptr_));
    return T::cast(Object(ptr_));
  }

  static MaybeObject FromSmi(Smi smi) {
    DCHECK(HAS_SMI_TAG(smi->ptr()));
    return MaybeObject(smi->ptr());
  }

  static MaybeObject FromObject(Object object) {
    DCHECK(!HasWeakHeapObjectTag(object.ptr()));
    return MaybeObject(object.ptr());
  }

  static inline MaybeObject MakeWeak(MaybeObject object);

#ifdef VERIFY_HEAP
  static void VerifyMaybeObjectPointer(Isolate* isolate, MaybeObject p);
#endif

  // Prints this object without details.
  void ShortPrint(FILE* out = stdout);

  // Prints this object without details to a message accumulator.
  void ShortPrint(StringStream* accumulator);

  void ShortPrint(std::ostream& os);

#ifdef OBJECT_PRINT
  void Print();
  void Print(std::ostream& os);
#else
  void Print() { ShortPrint(); }
  void Print(std::ostream& os) { ShortPrint(os); }
#endif

 private:
  Address ptr_;
};

// A HeapObjectReference is either a strong reference to a HeapObject, a weak
// reference to a HeapObject, or a cleared weak reference.
class HeapObjectReference : public MaybeObject {
 public:
  explicit HeapObjectReference(Address address) : MaybeObject(address) {}
  explicit HeapObjectReference(Object object) : MaybeObject(object->ptr()) {}

  static HeapObjectReference Strong(Object object) {
    DCHECK(!object->IsSmi());
    DCHECK(!HasWeakHeapObjectTag(object));
    return HeapObjectReference(object);
  }

  static HeapObjectReference Weak(Object object) {
    DCHECK(!object->IsSmi());
    DCHECK(!HasWeakHeapObjectTag(object));
    return HeapObjectReference(object->ptr() | kWeakHeapObjectMask);
  }

  V8_INLINE static HeapObjectReference ClearedValue(Isolate* isolate);

  template <typename THeapObjectSlot>
  V8_INLINE static void Update(THeapObjectSlot slot, HeapObject value);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_MAYBE_OBJECT_H_
