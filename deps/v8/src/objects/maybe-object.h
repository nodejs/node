// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MAYBE_OBJECT_H_
#define V8_OBJECTS_MAYBE_OBJECT_H_

#include "include/v8-internal.h"
#include "include/v8.h"
#include "src/globals.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

class HeapObject;
class Smi;
class StringStream;

// A MaybeObject is either a SMI, a strong reference to a HeapObject, a weak
// reference to a HeapObject, or a cleared weak reference. It's used for
// implementing in-place weak references (see design doc: goo.gl/j6SdcK )
class MaybeObject {
 public:
  bool IsSmi() const { return HAS_SMI_TAG(this); }
  inline bool ToSmi(Smi** value);

  bool IsCleared() const {
    return ::v8::internal::IsClearedWeakHeapObject(this);
  }

  inline bool IsStrongOrWeak() const;
  inline bool IsStrong() const;

  // If this MaybeObject is a strong pointer to a HeapObject, returns true and
  // sets *result. Otherwise returns false.
  inline bool GetHeapObjectIfStrong(HeapObject** result);

  // DCHECKs that this MaybeObject is a strong pointer to a HeapObject and
  // returns the HeapObject.
  inline HeapObject* GetHeapObjectAssumeStrong();

  inline bool IsWeak() const;
  inline bool IsWeakOrCleared() const;

  // If this MaybeObject is a weak pointer to a HeapObject, returns true and
  // sets *result. Otherwise returns false.
  inline bool GetHeapObjectIfWeak(HeapObject** result);

  // DCHECKs that this MaybeObject is a weak pointer to a HeapObject and
  // returns the HeapObject.
  inline HeapObject* GetHeapObjectAssumeWeak();

  // If this MaybeObject is a strong or weak pointer to a HeapObject, returns
  // true and sets *result. Otherwise returns false.
  inline bool GetHeapObject(HeapObject** result);
  inline bool GetHeapObject(HeapObject** result,
                            HeapObjectReferenceType* reference_type);

  // DCHECKs that this MaybeObject is a strong or a weak pointer to a HeapObject
  // and returns the HeapObject.
  inline HeapObject* GetHeapObject();

  // DCHECKs that this MaybeObject is a strong or a weak pointer to a HeapObject
  // or a SMI and returns the HeapObject or SMI.
  inline Object* GetHeapObjectOrSmi();

  inline bool IsObject() const;
  template <typename T>
  T* cast() {
    DCHECK(!HasWeakHeapObjectTag(this));
    return T::cast(reinterpret_cast<Object*>(this));
  }

  static MaybeObject* FromSmi(Smi* smi) {
    DCHECK(HAS_SMI_TAG(smi));
    return reinterpret_cast<MaybeObject*>(smi);
  }

  static MaybeObject* FromObject(Object* object) {
    DCHECK(!HasWeakHeapObjectTag(object));
    return reinterpret_cast<MaybeObject*>(object);
  }

  static inline MaybeObject* MakeWeak(MaybeObject* object);

#ifdef VERIFY_HEAP
  static void VerifyMaybeObjectPointer(Isolate* isolate, MaybeObject* p);
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
  DISALLOW_IMPLICIT_CONSTRUCTORS(MaybeObject);
};

// A HeapObjectReference is either a strong reference to a HeapObject, a weak
// reference to a HeapObject, or a cleared weak reference.
class HeapObjectReference : public MaybeObject {
 public:
  static HeapObjectReference* Strong(Object* object) {
    DCHECK(!object->IsSmi());
    DCHECK(!HasWeakHeapObjectTag(object));
    return reinterpret_cast<HeapObjectReference*>(object);
  }

  static HeapObjectReference* Weak(Object* object) {
    DCHECK(!object->IsSmi());
    DCHECK(!HasWeakHeapObjectTag(object));
    return AddWeakHeapObjectMask(object);
  }

  static HeapObjectReference* ClearedValue() {
    return reinterpret_cast<HeapObjectReference*>(kClearedWeakHeapObject);
  }

  static void Update(HeapObjectReference** slot, HeapObject* value) {
    DCHECK(!HAS_SMI_TAG(*slot));
    DCHECK(Internals::HasHeapObjectTag(value));

#ifdef DEBUG
    bool weak_before = HasWeakHeapObjectTag(*slot);
#endif

    *slot = reinterpret_cast<HeapObjectReference*>(
        reinterpret_cast<intptr_t>(value) |
        (reinterpret_cast<intptr_t>(*slot) & kWeakHeapObjectMask));

#ifdef DEBUG
    bool weak_after = HasWeakHeapObjectTag(*slot);
    DCHECK_EQ(weak_before, weak_after);
#endif
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(HeapObjectReference);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_MAYBE_OBJECT_H_
