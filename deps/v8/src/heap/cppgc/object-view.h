// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_OBJECT_VIEW_H_
#define V8_HEAP_CPPGC_OBJECT_VIEW_H_

#include "include/v8config.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap-page.h"

namespace cppgc {
namespace internal {

// ObjectView allows accessing a header within the bounds of the actual object.
// It is not exposed externally and does not keep the underlying object alive.
template <AccessMode = AccessMode::kNonAtomic>
class ObjectView final {
 public:
  V8_INLINE explicit ObjectView(const HeapObjectHeader& header);

  V8_INLINE Address Start() const;
  V8_INLINE ConstAddress End() const;
  V8_INLINE size_t Size() const;

 private:
  const HeapObjectHeader& header_;
  const BasePage* base_page_;
  const bool is_large_object_;
};

template <AccessMode access_mode>
ObjectView<access_mode>::ObjectView(const HeapObjectHeader& header)
    : header_(header),
      base_page_(
          BasePage::FromPayload(const_cast<HeapObjectHeader*>(&header_))),
      is_large_object_(header_.IsLargeObject<access_mode>()) {
  DCHECK_EQ(Start() + Size(), End());
}

template <AccessMode access_mode>
Address ObjectView<access_mode>::Start() const {
  return header_.ObjectStart();
}

template <AccessMode access_mode>
ConstAddress ObjectView<access_mode>::End() const {
  return is_large_object_ ? LargePage::From(base_page_)->PayloadEnd()
                          : header_.ObjectEnd<access_mode>();
}

template <AccessMode access_mode>
size_t ObjectView<access_mode>::Size() const {
  return is_large_object_ ? LargePage::From(base_page_)->ObjectSize()
                          : header_.ObjectSize<access_mode>();
}

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_OBJECT_VIEW_H_
