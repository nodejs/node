// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_STORE_BUFFER_H_
#define V8_STORE_BUFFER_H_

#include "src/allocation.h"
#include "src/base/logging.h"
#include "src/base/platform/platform.h"
#include "src/globals.h"
#include "src/heap/slot-set.h"

namespace v8 {
namespace internal {

// Intermediate buffer that accumulates old-to-new stores from the generated
// code. On buffer overflow the slots are moved to the remembered set.
class StoreBuffer {
 public:
  static const int kStoreBufferSize = 1 << (14 + kPointerSizeLog2);
  static const int kStoreBufferMask = kStoreBufferSize - 1;

  static void StoreBufferOverflow(Isolate* isolate);

  explicit StoreBuffer(Heap* heap);
  void SetUp();
  void TearDown();

  // Used to add entries from generated code.
  inline Address* top_address() { return reinterpret_cast<Address*>(&top_); }

  void MoveEntriesToRememberedSet();

 private:
  Heap* heap_;

  Address* top_;

  // The start and the limit of the buffer that contains store slots
  // added from the generated code.
  Address* start_;
  Address* limit_;

  base::VirtualMemory* virtual_memory_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_STORE_BUFFER_H_
