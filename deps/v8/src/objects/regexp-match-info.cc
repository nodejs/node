// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "src/objects/regexp-match-info-inl.h"

namespace v8::internal {

// static
DirectHandle<RegExpMatchInfo> RegExpMatchInfo::New(Isolate* isolate,
                                                   int capture_count,
                                                   AllocationType allocation) {
  // TODO(375937549): Convert capacity and capture_count to uint32_t.
  int capacity = JSRegExp::RegistersForCaptureCount(capture_count);
  DCHECK_GE(capacity, 0);
  DCHECK_GE(static_cast<uint32_t>(capacity), kMinCapacity);
  std::optional<DisallowGarbageCollection> no_gc;
  DirectHandle<RegExpMatchInfo> result =
      Allocate(isolate, static_cast<uint32_t>(capacity), &no_gc, allocation);

  ReadOnlyRoots roots{isolate};
  MemsetTagged(result->RawFieldOfFirstElement(), Smi::zero(), capacity);
  result->set_number_of_capture_registers(capacity);
  result->set_last_subject(*isolate->factory()->empty_string(),
                           SKIP_WRITE_BARRIER);
  result->set_last_input(roots.undefined_value(), SKIP_WRITE_BARRIER);

  return result;
}

DirectHandle<RegExpMatchInfo> RegExpMatchInfo::ReserveCaptures(
    Isolate* isolate, DirectHandle<RegExpMatchInfo> match_info,
    int capture_count) {
  int required_capacity = JSRegExp::RegistersForCaptureCount(capture_count);
  DCHECK_GE(required_capacity, 0);
  const uint32_t cap = match_info->capacity().value();
  if (static_cast<uint32_t>(required_capacity) > cap) {
    DirectHandle<RegExpMatchInfo> new_info = New(isolate, capture_count);
    RegExpMatchInfo::CopyElements(isolate, *new_info, 0, *match_info, 0, cap);
    match_info = new_info;
  }
  match_info->set_number_of_capture_registers(required_capacity);
  return match_info;
}

}  // namespace v8::internal
