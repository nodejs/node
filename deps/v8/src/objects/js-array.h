// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_ARRAY_H_
#define V8_OBJECTS_JS_ARRAY_H_

#include "src/objects/allocation-site.h"
#include "src/objects/fixed-array.h"
#include "src/objects/js-objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// The JSArray describes JavaScript Arrays
//  Such an array can be in one of two modes:
//    - fast, backing storage is a FixedArray and length <= elements.length();
//       Please note: push and pop can be used to grow and shrink the array.
//    - slow, backing storage is a HashTable with numbers as keys.
class JSArray : public JSObject {
 public:
  // [length]: The length property.
  DECL_ACCESSORS(length, Object)

  // Overload the length setter to skip write barrier when the length
  // is set to a smi. This matches the set function on FixedArray.
  inline void set_length(Smi* length);

  static bool HasReadOnlyLength(Handle<JSArray> array);
  static bool WouldChangeReadOnlyLength(Handle<JSArray> array, uint32_t index);

  // Initialize the array with the given capacity. The function may
  // fail due to out-of-memory situations, but only if the requested
  // capacity is non-zero.
  static void Initialize(Handle<JSArray> array, int capacity, int length = 0);

  // If the JSArray has fast elements, and new_length would result in
  // normalization, returns true.
  bool SetLengthWouldNormalize(uint32_t new_length);
  static inline bool SetLengthWouldNormalize(Heap* heap, uint32_t new_length);

  // Initializes the array to a certain length.
  inline bool AllowsSetLength();

  static void SetLength(Handle<JSArray> array, uint32_t length);

  // Set the content of the array to the content of storage.
  static inline void SetContent(Handle<JSArray> array,
                                Handle<FixedArrayBase> storage);

  // ES6 9.4.2.1
  V8_WARN_UNUSED_RESULT static Maybe<bool> DefineOwnProperty(
      Isolate* isolate, Handle<JSArray> o, Handle<Object> name,
      PropertyDescriptor* desc, ShouldThrow should_throw);

  static bool AnythingToArrayLength(Isolate* isolate,
                                    Handle<Object> length_object,
                                    uint32_t* output);
  V8_WARN_UNUSED_RESULT static Maybe<bool> ArraySetLength(
      Isolate* isolate, Handle<JSArray> a, PropertyDescriptor* desc,
      ShouldThrow should_throw);

  // Checks whether the Array has the current realm's Array.prototype as its
  // prototype. This function is best-effort and only gives a conservative
  // approximation, erring on the side of false, in particular with respect
  // to Proxies and objects with a hidden prototype.
  inline bool HasArrayPrototype(Isolate* isolate);

  DECL_CAST(JSArray)

  // Dispatched behavior.
  DECL_PRINTER(JSArray)
  DECL_VERIFIER(JSArray)

  // Number of element slots to pre-allocate for an empty array.
  static const int kPreallocatedArrayElements = 4;

  // Layout description.
  static const int kLengthOffset = JSObject::kHeaderSize;
  static const int kSize = kLengthOffset + kPointerSize;

  static const int kLengthDescriptorIndex = 0;

  // Max. number of elements being copied in Array builtins.
  static const int kMaxCopyElements = 100;

  // This constant is somewhat arbitrary. Any large enough value would work.
  static const uint32_t kMaxFastArrayLength = 32 * 1024 * 1024;

  static const int kInitialMaxFastElementArray =
      (kMaxRegularHeapObjectSize - FixedArray::kHeaderSize - kSize -
       AllocationMemento::kSize) >>
      kDoubleSizeLog2;

  // Valid array indices range from +0 <= i < 2^32 - 1 (kMaxUInt32).
  static const uint32_t kMaxArrayIndex = kMaxUInt32 - 1;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSArray);
};

Handle<Object> CacheInitialJSArrayMaps(Handle<Context> native_context,
                                       Handle<Map> initial_map);

// The JSArrayIterator describes JavaScript Array Iterators Objects, as
// defined in ES section #sec-array-iterator-objects.
class JSArrayIterator : public JSObject {
 public:
  DECL_PRINTER(JSArrayIterator)
  DECL_VERIFIER(JSArrayIterator)

  DECL_CAST(JSArrayIterator)

  // [iterated_object]: the [[IteratedObject]] inobject property.
  DECL_ACCESSORS(iterated_object, Object)

  // [next_index]: The [[ArrayIteratorNextIndex]] inobject property.
  // The next_index is always a positive integer, and it points to
  // the next index that is to be returned by this iterator. It's
  // possible range is fixed depending on the [[iterated_object]]:
  //
  //   1. For JSArray's the next_index is always in Unsigned32
  //      range, and when the iterator reaches the end it's set
  //      to kMaxUInt32 to indicate that this iterator should
  //      never produce values anymore even if the "length"
  //      property of the JSArray changes at some later point.
  //   2. For JSTypedArray's the next_index is always in
  //      UnsignedSmall range, and when the iterator terminates
  //      it's set to Smi::kMaxValue.
  //   3. For all other JSReceiver's it's always between 0 and
  //      kMaxSafeInteger, and the latter value is used to mark
  //      termination.
  //
  // It's important that for 1. and 2. the value fits into the
  // Unsigned32 range (UnsignedSmall is a subset of Unsigned32),
  // since we use this knowledge in the fast-path for the array
  // iterator next calls in TurboFan (in the JSCallReducer) to
  // keep the index in Word32 representation. This invariant is
  // checked in JSArrayIterator::JSArrayIteratorVerify().
  DECL_ACCESSORS(next_index, Object)

  // [kind]: the [[ArrayIterationKind]] inobject property.
  inline IterationKind kind() const;
  inline void set_kind(IterationKind kind);

  static const int kIteratedObjectOffset = JSObject::kHeaderSize;
  static const int kNextIndexOffset = kIteratedObjectOffset + kPointerSize;
  static const int kKindOffset = kNextIndexOffset + kPointerSize;
  static const int kSize = kKindOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSArrayIterator);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_ARRAY_H_
