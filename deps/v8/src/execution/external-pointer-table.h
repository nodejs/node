// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_EXTERNAL_POINTER_TABLE_H_
#define V8_EXECUTION_EXTERNAL_POINTER_TABLE_H_

#include "src/base/platform/wrappers.h"
#include "src/common/external-pointer.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

class V8_EXPORT_PRIVATE ExternalPointerTable {
 public:
  static const int kExternalPointerTableInitialCapacity = 1024;

  ExternalPointerTable()
      : buffer_(reinterpret_cast<Address*>(base::Calloc(
            kExternalPointerTableInitialCapacity, sizeof(Address)))),
        length_(1),
        capacity_(kExternalPointerTableInitialCapacity),
        freelist_head_(0) {
    // Explicitly setup the invalid nullptr entry.
    STATIC_ASSERT(kNullExternalPointer == 0);
    buffer_[kNullExternalPointer] = kNullAddress;
  }

  ~ExternalPointerTable() { ::free(buffer_); }

  Address get(uint32_t index) const {
    CHECK_LT(index, length_);
    return buffer_[index];
  }

  void set(uint32_t index, Address value) {
    DCHECK_NE(kNullExternalPointer, index);
    CHECK_LT(index, length_);
    buffer_[index] = value;
  }

  uint32_t allocate() {
    uint32_t index = length_++;
    if (index >= capacity_) {
      GrowTable(this);
    }
    DCHECK_NE(kNullExternalPointer, index);
    return index;
  }

  void free(uint32_t index) {
    // TODO(v8:10391, saelo): implement simple free list here, i.e. set
    // buffer_[index] to freelist_head_ and set freelist_head
    // to index
    DCHECK_NE(kNullExternalPointer, index);
  }

  // Returns true if the entry exists in the table and therefore it can be read.
  bool is_valid_index(uint32_t index) const {
    // TODO(v8:10391, saelo): also check here if entry is free
    return index < length_;
  }

  uint32_t size() const { return length_; }

  static void GrowTable(ExternalPointerTable* table);

 private:
  friend class Isolate;

  Address* buffer_;
  uint32_t length_;
  uint32_t capacity_;
  uint32_t freelist_head_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_EXTERNAL_POINTER_TABLE_H_
