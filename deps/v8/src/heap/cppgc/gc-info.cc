// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/internal/gc-info.h"
#include "include/v8config.h"
#include "src/heap/cppgc/gc-info-table.h"

namespace cppgc {
namespace internal {

GCInfoIndex EnsureGCInfoIndex(std::atomic<GCInfoIndex>& registered_index,
                              FinalizationCallback finalization_callback,
                              TraceCallback trace_callback,
                              NameCallback name_callback, bool has_v_table) {
  return GlobalGCInfoTable::GetMutable().RegisterNewGCInfo(
      registered_index,
      {finalization_callback, trace_callback, name_callback, has_v_table});
}

}  // namespace internal
}  // namespace cppgc
