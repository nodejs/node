// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/trace-trait.h"

#include "src/heap/cppgc/gc-info-table.h"
#include "src/heap/cppgc/heap-page.h"

namespace cppgc {
namespace internal {

TraceDescriptor TraceTraitFromInnerAddressImpl::GetTraceDescriptor(
    const void* address) {
  // address is guaranteed to be on a normal page because this is used only for
  // mixins.
  const BasePage* page = BasePage::FromPayload(address);
  page->SynchronizedLoad();
  const HeapObjectHeader& header =
      page->ObjectHeaderFromInnerAddress<AccessMode::kAtomic>(address);
  return {header.ObjectStart(),
          GlobalGCInfoTable::GCInfoFromIndex(
              header.GetGCInfoIndex<AccessMode::kAtomic>())
              .trace};
}

}  // namespace internal
}  // namespace cppgc
