// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/external-pointer-table.h"

#include "src/base/platform/wrappers.h"

namespace v8 {
namespace internal {

void ExternalPointerTable::GrowTable(ExternalPointerTable* table) {
  // TODO(v8:10391, saelo): overflow check here and in the multiplication below
  uint32_t new_capacity = table->capacity_ + table->capacity_ / 2;
  table->buffer_ = reinterpret_cast<Address*>(
      base::Realloc(table->buffer_, new_capacity * sizeof(Address)));
  CHECK(table->buffer_);
  memset(&table->buffer_[table->capacity_], 0,
         (new_capacity - table->capacity_) * sizeof(Address));
  table->capacity_ = new_capacity;
}

}  // namespace internal
}  // namespace v8
