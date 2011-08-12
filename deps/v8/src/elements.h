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
  virtual MaybeObject* GetWithReceiver(JSObject* obj,
                                       Object* receiver,
                                       uint32_t index) = 0;

  virtual MaybeObject* Delete(JSObject* obj,
                              uint32_t index,
                              JSReceiver::DeleteMode mode) = 0;

  virtual MaybeObject* AddJSArrayKeysToFixedArray(JSArray* other,
                                                  FixedArray* keys) = 0;

  // Returns a shared ElementsAccessor for the specified ElementsKind.
  static ElementsAccessor* ForKind(JSObject::ElementsKind elements_kind) {
    ASSERT(elements_kind < JSObject::kElementsKindCount);
    return elements_accessors_[elements_kind];
  }

  static void InitializeOncePerProcess();

 private:
  static ElementsAccessor** elements_accessors_;

  DISALLOW_COPY_AND_ASSIGN(ElementsAccessor);
};

} }  // namespace v8::internal

#endif  // V8_ELEMENTS_H_
