// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/handler-table-builder.h"

#include "src/base/numerics/safe_conversions.h"
#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/interpreter/bytecode-register.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {
namespace interpreter {

HandlerTableBuilder::HandlerTableBuilder(Zone* zone) : entries_(zone) {}

template <typename IsolateT>
DirectHandle<TrustedByteArray> HandlerTableBuilder::ToHandlerTable(
    IsolateT* isolate) {
  uint32_t effective_size = 0;
  for (const auto& entry : entries_) {
    if (!entry.dropped) effective_size++;
  }

  DirectHandle<TrustedByteArray> table_byte_array =
      isolate->factory()->NewTrustedByteArray(
          HandlerTable::LengthForRange(effective_size));
  HandlerTable table(*table_byte_array);
  uint32_t table_index = 0;
  for (size_t i = 0; i < entries_.size(); ++i) {
    Entry& entry = entries_[i];
    if (entry.dropped) continue;
    HandlerTable::CatchPrediction pred = entry.catch_prediction_;
    table.SetRangeStart(table_index, static_cast<int>(entry.offset_start));
    table.SetRangeEnd(table_index, static_cast<int>(entry.offset_end));
    table.SetRangeHandler(table_index, static_cast<int>(entry.offset_target),
                          pred);
    table.SetRangeData(table_index, entry.context.index());
    table_index++;
  }
  return table_byte_array;
}

template DirectHandle<TrustedByteArray> HandlerTableBuilder::ToHandlerTable(
    Isolate* isolate);
template DirectHandle<TrustedByteArray> HandlerTableBuilder::ToHandlerTable(
    LocalIsolate* isolate);

int HandlerTableBuilder::NewHandlerEntry() {
  int handler_id = static_cast<int>(entries_.size());
  // Avoid unnecessary spaces
  // clang-format off
  Entry entry = {0, 0, 0, Register::invalid_value(), HandlerTable::UNCAUGHT, false};
  // clang-format on
  entries_.push_back(entry);
  return handler_id;
}


void HandlerTableBuilder::SetTryRegionStart(int handler_id, size_t offset) {
  DCHECK(Smi::IsValid(offset));  // Encoding of handler table requires this.
  entries_[handler_id].offset_start = offset;
}


void HandlerTableBuilder::SetTryRegionEnd(int handler_id, size_t offset) {
  DCHECK(Smi::IsValid(offset));  // Encoding of handler table requires this.
  entries_[handler_id].offset_end = offset;
}


void HandlerTableBuilder::SetHandlerTarget(int handler_id, size_t offset) {
  DCHECK(Smi::IsValid(offset));  // Encoding of handler table requires this.
  entries_[handler_id].offset_target = offset;
}

void HandlerTableBuilder::SetPrediction(
    int handler_id, HandlerTable::CatchPrediction prediction) {
  entries_[handler_id].catch_prediction_ = prediction;
}


void HandlerTableBuilder::SetContextRegister(int handler_id, Register reg) {
  entries_[handler_id].context = reg;
}

void HandlerTableBuilder::DropHandlerEntry(int handler_id) {
  entries_[handler_id].dropped = true;
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
