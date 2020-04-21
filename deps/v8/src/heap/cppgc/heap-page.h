// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_HEAP_PAGE_H_
#define V8_HEAP_CPPGC_HEAP_PAGE_H_

#include "src/base/macros.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/heap.h"

namespace cppgc {
namespace internal {

class V8_EXPORT_PRIVATE BasePage {
 public:
  static BasePage* FromPayload(const void*);

 protected:
  explicit BasePage(Heap* heap);

  Heap* heap_;
};

class V8_EXPORT_PRIVATE NormalPage final : public BasePage {
 public:
  static NormalPage* Create(Heap* heap);
  static void Destroy(NormalPage*);

  Address PayloadStart() const { return payload_start_; }
  Address PayloadEnd() const { return payload_end_; }

 private:
  explicit NormalPage(Heap* heap, Address reservation);

  Address reservation_;
  Address payload_start_;
  Address payload_end_;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_HEAP_PAGE_H_
