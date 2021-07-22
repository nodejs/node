// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_OBJECT_VIEW_H_
#define V8_HEAP_CPPGC_OBJECT_VIEW_H_

#include "include/v8config.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap-page.h"

namespace cppgc {
namespace internal {

// ObjectView allows accessing a header within the bounds of the actual object.
// It is not exposed externally and does not keep the underlying object alive.
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

ObjectView::ObjectView(const HeapObjectHeader& header)
    : header_(header),
      base_page_(
          BasePage::FromPayload(const_cast<HeapObjectHeader*>(&header_))),
      is_large_object_(header_.IsLargeObject()) {
  DCHECK_EQ(Start() + Size(), End());
}

Address ObjectView::Start() const { return header_.ObjectStart(); }

ConstAddress ObjectView::End() const {
  return is_large_object_ ? LargePage::From(base_page_)->PayloadEnd()
                          : header_.ObjectEnd();
}

size_t ObjectView::Size() const {
  return is_large_object_ ? LargePage::From(base_page_)->ObjectSize()
                          : header_.ObjectSize();
}

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_OBJECT_VIEW_H_
