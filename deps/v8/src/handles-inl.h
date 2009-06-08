// Copyright 2006-2008 the V8 project authors. All rights reserved.
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
//

#ifndef V8_HANDLES_INL_H_
#define V8_HANDLES_INL_H_

#include "apiutils.h"
#include "handles.h"
#include "api.h"

namespace v8 {
namespace internal {

template<class T>
Handle<T>::Handle(T* obj) {
  ASSERT(!obj->IsFailure());
  location_ = reinterpret_cast<T**>(HandleScope::CreateHandle(obj));
}


template <class T>
inline T* Handle<T>::operator*() const {
  ASSERT(location_ != NULL);
  ASSERT(reinterpret_cast<Address>(*location_) != kHandleZapValue);
  return *location_;
}


#ifdef DEBUG
inline NoHandleAllocation::NoHandleAllocation() {
  v8::ImplementationUtilities::HandleScopeData* current =
      v8::ImplementationUtilities::CurrentHandleScope();
  extensions_ = current->extensions;
  // Shrink the current handle scope to make it impossible to do
  // handle allocations without an explicit handle scope.
  current->limit = current->next;
  current->extensions = -1;
}


inline NoHandleAllocation::~NoHandleAllocation() {
  // Restore state in current handle scope to re-enable handle
  // allocations.
  v8::ImplementationUtilities::CurrentHandleScope()->extensions = extensions_;
}
#endif


} }  // namespace v8::internal

#endif  // V8_HANDLES_INL_H_
