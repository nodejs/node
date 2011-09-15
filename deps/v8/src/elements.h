// Copyright 2011 the V8 project authors. All rights reserved.
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

#include "objects.h"

namespace v8 {
namespace internal {

// Abstract base class for handles that can operate on objects with differing
// ElementsKinds.
class ElementsAccessor {
 public:
  ElementsAccessor() { }
  virtual ~ElementsAccessor() { }
  virtual MaybeObject* Get(FixedArrayBase* backing_store,
                           uint32_t key,
                           JSObject* holder,
                           Object* receiver) = 0;

  virtual MaybeObject* Delete(JSObject* holder,
                              uint32_t key,
                              JSReceiver::DeleteMode mode) = 0;

  virtual MaybeObject* AddElementsToFixedArray(FixedArrayBase* from,
                                               FixedArray* to,
                                               JSObject* holder,
                                               Object* receiver) = 0;

  // Returns a shared ElementsAccessor for the specified ElementsKind.
  static ElementsAccessor* ForKind(ElementsKind elements_kind) {
    ASSERT(elements_kind < kElementsKindCount);
    return elements_accessors_[elements_kind];
  }

  static ElementsAccessor* ForArray(FixedArrayBase* array);

  static void InitializeOncePerProcess();

 protected:
  friend class NonStrictArgumentsElementsAccessor;

  virtual uint32_t GetCapacity(FixedArrayBase* backing_store) = 0;

  virtual bool HasElementAtIndex(FixedArrayBase* backing_store,
                                 uint32_t index,
                                 JSObject* holder,
                                 Object* receiver) = 0;

  // Element handlers distinguish between indexes and keys when the manipulate
  // elements.  Indexes refer to elements in terms of their location in the
  // underlying storage's backing store representation, and are between 0
  // GetCapacity.  Keys refer to elements in terms of the value that would be
  // specific in JavaScript to access the element. In most implementations, keys
  // are equivalent to indexes, and GetKeyForIndex returns the same value it is
  // passed. In the NumberDictionary ElementsAccessor, GetKeyForIndex maps the
  // index to a key using the KeyAt method on the NumberDictionary.
  virtual uint32_t GetKeyForIndex(FixedArrayBase* backing_store,
                                  uint32_t index) = 0;

 private:
  static ElementsAccessor** elements_accessors_;

  DISALLOW_COPY_AND_ASSIGN(ElementsAccessor);
};

} }  // namespace v8::internal

#endif  // V8_ELEMENTS_H_
