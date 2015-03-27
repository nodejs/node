// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ELEMENTS_H_
#define V8_ELEMENTS_H_

#include "src/elements-kind.h"
#include "src/heap/heap.h"
#include "src/isolate.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

// Abstract base class for handles that can operate on objects with differing
// ElementsKinds.
class ElementsAccessor {
 public:
  explicit ElementsAccessor(const char* name) : name_(name) { }
  virtual ~ElementsAccessor() { }

  virtual ElementsKind kind() const = 0;
  const char* name() const { return name_; }

  // Checks the elements of an object for consistency, asserting when a problem
  // is found.
  virtual void Validate(Handle<JSObject> obj) = 0;

  // Returns true if a holder contains an element with the specified key
  // without iterating up the prototype chain.  The caller can optionally pass
  // in the backing store to use for the check, which must be compatible with
  // the ElementsKind of the ElementsAccessor. If backing_store is NULL, the
  // holder->elements() is used as the backing store.
  virtual bool HasElement(
      Handle<JSObject> holder,
      uint32_t key,
      Handle<FixedArrayBase> backing_store) = 0;

  inline bool HasElement(
      Handle<JSObject> holder,
      uint32_t key) {
    return HasElement(holder, key, handle(holder->elements()));
  }

  // Returns the element with the specified key or undefined if there is no such
  // element. This method doesn't iterate up the prototype chain.  The caller
  // can optionally pass in the backing store to use for the check, which must
  // be compatible with the ElementsKind of the ElementsAccessor. If
  // backing_store is NULL, the holder->elements() is used as the backing store.
  MUST_USE_RESULT virtual MaybeHandle<Object> Get(
      Handle<Object> receiver,
      Handle<JSObject> holder,
      uint32_t key,
      Handle<FixedArrayBase> backing_store) = 0;

  MUST_USE_RESULT inline MaybeHandle<Object> Get(
      Handle<Object> receiver,
      Handle<JSObject> holder,
      uint32_t key) {
    return Get(receiver, holder, key, handle(holder->elements()));
  }

  // Returns an element's attributes, or ABSENT if there is no such
  // element. This method doesn't iterate up the prototype chain.  The caller
  // can optionally pass in the backing store to use for the check, which must
  // be compatible with the ElementsKind of the ElementsAccessor. If
  // backing_store is NULL, the holder->elements() is used as the backing store.
  MUST_USE_RESULT virtual PropertyAttributes GetAttributes(
      Handle<JSObject> holder,
      uint32_t key,
      Handle<FixedArrayBase> backing_store) = 0;

  MUST_USE_RESULT inline PropertyAttributes GetAttributes(
      Handle<JSObject> holder,
      uint32_t key) {
    return GetAttributes(holder, key, handle(holder->elements()));
  }

  // Returns an element's accessors, or NULL if the element does not exist or
  // is plain. This method doesn't iterate up the prototype chain.  The caller
  // can optionally pass in the backing store to use for the check, which must
  // be compatible with the ElementsKind of the ElementsAccessor. If
  // backing_store is NULL, the holder->elements() is used as the backing store.
  MUST_USE_RESULT virtual MaybeHandle<AccessorPair> GetAccessorPair(
      Handle<JSObject> holder,
      uint32_t key,
      Handle<FixedArrayBase> backing_store) = 0;

  MUST_USE_RESULT inline MaybeHandle<AccessorPair> GetAccessorPair(
      Handle<JSObject> holder,
      uint32_t key) {
    return GetAccessorPair(holder, key, handle(holder->elements()));
  }

  // Modifies the length data property as specified for JSArrays and resizes the
  // underlying backing store accordingly. The method honors the semantics of
  // changing array sizes as defined in EcmaScript 5.1 15.4.5.2, i.e. array that
  // have non-deletable elements can only be shrunk to the size of highest
  // element that is non-deletable.
  MUST_USE_RESULT virtual MaybeHandle<Object> SetLength(
      Handle<JSArray> holder,
      Handle<Object> new_length) = 0;

  // Modifies both the length and capacity of a JSArray, resizing the underlying
  // backing store as necessary. This method does NOT honor the semantics of
  // EcmaScript 5.1 15.4.5.2, arrays can be shrunk beyond non-deletable
  // elements. This method should only be called for array expansion OR by
  // runtime JavaScript code that use InternalArrays and don't care about
  // EcmaScript 5.1 semantics.
  virtual void SetCapacityAndLength(
      Handle<JSArray> array,
      int capacity,
      int length) = 0;

  // Deletes an element in an object, returning a new elements backing store.
  MUST_USE_RESULT virtual MaybeHandle<Object> Delete(
      Handle<JSObject> holder, uint32_t key, LanguageMode language_mode) = 0;

  // If kCopyToEnd is specified as the copy_size to CopyElements, it copies all
  // of elements from source after source_start to the destination array.
  static const int kCopyToEnd = -1;
  // If kCopyToEndAndInitializeToHole is specified as the copy_size to
  // CopyElements, it copies all of elements from source after source_start to
  // destination array, padding any remaining uninitialized elements in the
  // destination array with the hole.
  static const int kCopyToEndAndInitializeToHole = -2;

  // Copy elements from one backing store to another. Typically, callers specify
  // the source JSObject or JSArray in source_holder. If the holder's backing
  // store is available, it can be passed in source and source_holder is
  // ignored.
  virtual void CopyElements(
      Handle<FixedArrayBase> source,
      uint32_t source_start,
      ElementsKind source_kind,
      Handle<FixedArrayBase> destination,
      uint32_t destination_start,
      int copy_size) = 0;

  // NOTE: this method violates the handlified function signature convention:
  // raw pointer parameter |source_holder| in the function that allocates.
  // This is done intentionally to avoid ArrayConcat() builtin performance
  // degradation.
  virtual void CopyElements(
      JSObject* source_holder,
      uint32_t source_start,
      ElementsKind source_kind,
      Handle<FixedArrayBase> destination,
      uint32_t destination_start,
      int copy_size) = 0;

  inline void CopyElements(
      Handle<JSObject> from_holder,
      Handle<FixedArrayBase> to,
      ElementsKind from_kind) {
    CopyElements(
      *from_holder, 0, from_kind, to, 0, kCopyToEndAndInitializeToHole);
  }

  MUST_USE_RESULT virtual MaybeHandle<FixedArray> AddElementsToFixedArray(
      Handle<Object> receiver, Handle<JSObject> holder, Handle<FixedArray> to,
      Handle<FixedArrayBase> from, FixedArray::KeyFilter filter) = 0;

  MUST_USE_RESULT inline MaybeHandle<FixedArray> AddElementsToFixedArray(
      Handle<Object> receiver, Handle<JSObject> holder, Handle<FixedArray> to,
      FixedArray::KeyFilter filter) {
    return AddElementsToFixedArray(receiver, holder, to,
                                   handle(holder->elements()), filter);
  }

  // Returns a shared ElementsAccessor for the specified ElementsKind.
  static ElementsAccessor* ForKind(ElementsKind elements_kind) {
    DCHECK(static_cast<int>(elements_kind) < kElementsKindCount);
    return elements_accessors_[elements_kind];
  }

  static ElementsAccessor* ForArray(Handle<FixedArrayBase> array);

  static void InitializeOncePerProcess();
  static void TearDown();

 protected:
  friend class SloppyArgumentsElementsAccessor;

  virtual uint32_t GetCapacity(Handle<FixedArrayBase> backing_store) = 0;

  // Element handlers distinguish between indexes and keys when they manipulate
  // elements.  Indexes refer to elements in terms of their location in the
  // underlying storage's backing store representation, and are between 0 and
  // GetCapacity.  Keys refer to elements in terms of the value that would be
  // specified in JavaScript to access the element. In most implementations,
  // keys are equivalent to indexes, and GetKeyForIndex returns the same value
  // it is passed. In the NumberDictionary ElementsAccessor, GetKeyForIndex maps
  // the index to a key using the KeyAt method on the NumberDictionary.
  virtual uint32_t GetKeyForIndex(Handle<FixedArrayBase> backing_store,
                                  uint32_t index) = 0;

 private:
  static ElementsAccessor** elements_accessors_;
  const char* name_;

  DISALLOW_COPY_AND_ASSIGN(ElementsAccessor);
};

void CheckArrayAbuse(Handle<JSObject> obj, const char* op, uint32_t key,
                     bool allow_appending = false);

MUST_USE_RESULT MaybeHandle<Object> ArrayConstructInitializeElements(
    Handle<JSArray> array,
    Arguments* args);

} }  // namespace v8::internal

#endif  // V8_ELEMENTS_H_
