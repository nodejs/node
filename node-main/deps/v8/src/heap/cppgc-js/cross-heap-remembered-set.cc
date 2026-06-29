// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc-js/cross-heap-remembered-set.h"

#include "src/api/api-inl.h"
#include "src/handles/global-handles-inl.h"
#include "src/heap/cppgc/heap-page.h"

namespace v8::internal {

void CrossHeapRememberedSet::RememberReferenceIfNeeded(
    Isolate& isolate, Tagged<CppHeapPointerWrapperObjectT> host_obj,
    void* cppgc_object) {
  DCHECK_NOT_NULL(cppgc_object);
  // Any in-cage pointer must point to a vaild, not freed cppgc object.
  auto* page =
      cppgc::internal::BasePage::FromInnerAddress(&heap_base_, cppgc_object);
  // TODO(v8:13475): Better filter with on-cage check.
  if (!page) return;
  auto& value_hoh = page->ObjectHeaderFromInnerAddress(cppgc_object);
  if (!value_hoh.IsYoung()) return;
  remembered_v8_to_cppgc_references_.push_back(
      isolate.global_handles()->Create(host_obj));
}

void CrossHeapRememberedSet::Reset(Isolate& isolate) {
  for (auto& h : remembered_v8_to_cppgc_references_) {
    isolate.global_handles()->Destroy(h.location());
  }
  remembered_v8_to_cppgc_references_.clear();
  remembered_v8_to_cppgc_references_.shrink_to_fit();
}

}  // namespace v8::internal
