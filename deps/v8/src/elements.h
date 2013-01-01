// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_ELEMENTS_H_
#define V8_ELEMENTS_H_

#include "elements-kind.h"
#include "objects.h"
#include "heap.h"
#include "isolate.h"

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
  virtual void Validate(JSObject* obj) = 0;

  // Returns true if a holder contains an element with the specified key
  // without iterating up the prototype chain.  The caller can optionally pass
  // in the backing store to use for the check, which must be compatible with
  // the ElementsKind of the ElementsAccessor. If backing_store is NULL, the
  // holder->elements() is used as the backing store.
  virtual bool HasElement(Object* receiver,
                          JSObject* holder,
                          uint32_t key,
                          FixedArrayBase* backing_store = NULL) = 0;

  // Returns the element with the specified key or undefined if there is no such
  // element. This method doesn't iterate up the prototype chain.  The caller
  // can optionally pass in the backing store to use for the check, which must
  // be compatible with the ElementsKind of the ElementsAccessor. If
  // backing_store is NULL, the holder->elements() is used as the backing store.
  MUST_USE_RESULT virtual MaybeObject* Get(
      Object* receiver,
      JSObject* holder,
      uint32_t key,
      FixedArrayBase* backing_store = NULL) = 0;

  // Returns an element's attributes, or ABSENT if there is no such
  // element. This method doesn't iterate up the prototype chain.  The caller
  // can optionally pass in the backing store to use for the check, which must
  // be compatible with the ElementsKind of the ElementsAccessor. If
  // backing_store is NULL, the holder->elements() is used as the backing store.
  MUST_USE_RESULT virtual PropertyAttributes GetAttributes(
      Object* receiver,
      JSObject* holder,
      uint32_t key,
      FixedArrayBase* backing_store = NULL) = 0;

  // Returns an element's type, or NONEXISTENT if there is no such
  // element. This method doesn't iterate up the prototype chain.  The caller
  // can optionally pass in the backing store to use for the check, which must
  // be compatible with the ElementsKind of the ElementsAccessor. If
  // backing_store is NULL, the holder->elements() is used as the backing store.
  MUST_USE_RESULT virtual PropertyType GetType(
      Object* receiver,
      JSObject* holder,
      uint32_t key,
      FixedArrayBase* backing_store = NULL) = 0;

  // Returns an element's accessors, or NULL if the element does not exist or
  // is plain. This method doesn't iterate up the prototype chain.  The caller
  // can optionally pass in the backing store to use for the check, which must
  // be compatible with the ElementsKind of the ElementsAccessor. If
  // backing_store is NULL, the holder->elements() is used as the backing store.
  MUST_USE_RESULT virtual AccessorPair* GetAccessorPair(
      Object* receiver,
      JSObject* holder,
      uint32_t key,
      FixedArrayBase* backing_store = NULL) = 0;

  // Modifies the length data property as specified for JSArrays and resizes the
  // underlying backing store accordingly. The method honors the semantics of
  // changing array sizes as defined in EcmaScript 5.1 15.4.5.2, i.e. array that
  // have non-deletable elements can only be shrunk to the size of highest
  // element that is non-deletable.
  MUST_USE_RESULT virtual MaybeObject* SetLength(JSArray* holder,
                                                 Object* new_length) = 0;

  // Modifies both the length and capacity of a JSArray, resizing the underlying
  // backing store as necessary. This method does NOT honor the semantics of
  // EcmaScript 5.1 15.4.5.2, arrays can be shrunk beyond non-deletable
  // elements. This method should only be called for array expansion OR by
  // runtime JavaScript code that use InternalArrays and don't care about
  // EcmaScript 5.1 semantics.
  MUST_USE_RESULT virtual MaybeObject* SetCapacityAndLength(JSArray* array,
                                                            int capacity,
                                                            int length) = 0;

  // Deletes an element in an object, returning a new elements backing store.
  MUST_USE_RESULT virtual MaybeObject* Delete(JSObject* holder,
                                              uint32_t key,
                                              JSReceiver::DeleteMode mode) = 0;

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
  MUST_USE_RESULT virtual MaybeObject* CopyElements(
      JSObject* source_holder,
      uint32_t source_start,
      FixedArrayBase* destination,
      ElementsKind destination_kind,
      uint32_t destination_start,
      int copy_size,
      FixedArrayBase* source = NULL) = 0;

  MUST_USE_RESULT MaybeObject* CopyElements(JSObject* from_holder,
                                            FixedArrayBase* to,
                                            ElementsKind to_kind,
                                            FixedArrayBase* from = NULL) {
    return CopyElements(from_holder, 0, to, to_kind, 0,
                        kCopyToEndAndInitializeToHole, from);
  }

  MUST_USE_RESULT virtual MaybeObject* AddElementsToFixedArray(
      Object* receiver,
      JSObject* holder,
      FixedArray* to,
      FixedArrayBase* from = NULL) = 0;

  // Returns a shared ElementsAccessor for the specified ElementsKind.
  static ElementsAccessor* ForKind(ElementsKind elements_kind) {
    ASSERT(elements_kind < kElementsKindCount);
    return elements_accessors_[elements_kind];
  }

  static ElementsAccessor* ForArray(FixedArrayBase* array);

  static void InitializeOncePerProcess();
  static void TearDown();

 protected:
  friend class NonStrictArgumentsElementsAccessor;

  virtual uint32_t GetCapacity(FixedArrayBase* backing_store) = 0;

  // Element handlers distinguish between indexes and keys when they manipulate
  // elements.  Indexes refer to elements in terms of their location in the
  // underlying storage's backing store representation, and are between 0 and
  // GetCapacity.  Keys refer to elements in terms of the value that would be
  // specified in JavaScript to access the element. In most implementations,
  // keys are equivalent to indexes, and GetKeyForIndex returns the same value
  // it is passed. In the NumberDictionary ElementsAccessor, GetKeyForIndex maps
  // the index to a key using the KeyAt method on the NumberDictionary.
  virtual uint32_t GetKeyForIndex(FixedArrayBase* backing_store,
                                  uint32_t index) = 0;

 private:
  static ElementsAccessor** elements_accessors_;
  const char* name_;

  DISALLOW_COPY_AND_ASSIGN(ElementsAccessor);
};

} }  // namespace v8::internal

#endif  // V8_ELEMENTS_H_
