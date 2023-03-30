// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/well-known-imports.h"

#include "src/wasm/wasm-code-manager.h"

namespace v8::internal::wasm {

const char* WellKnownImportName(WellKnownImport wki) {
  switch (wki) {
    case WellKnownImport::kUninstantiated:
      return "uninstantiated";
    case WellKnownImport::kGeneric:
      return "generic";
    case WellKnownImport::kStringToLowerCaseStringref:
      return "String.toLowerCase";
  }
}

WellKnownImportsList::UpdateResult WellKnownImportsList::Update(
    base::Vector<WellKnownImport> entries) {
  DCHECK_EQ(entries.size(), static_cast<size_t>(size_));
  {
    base::MutexGuard lock(&mutex_);
    for (size_t i = 0; i < entries.size(); i++) {
      WellKnownImport entry = entries[i];
      DCHECK(entry != WellKnownImport::kUninstantiated);
      WellKnownImport old = statuses_[i].load(std::memory_order_relaxed);
      if (old == WellKnownImport::kGeneric) continue;
      if (old == entry) continue;
      if (old == WellKnownImport::kUninstantiated) {
        statuses_[i].store(entry, std::memory_order_relaxed);
      } else {
        // To avoid having to clear Turbofan code multiple times, we give up
        // entirely once the first problem occurs.
        // This is a heuristic; we could also choose to make finer-grained
        // decisions and only set {statuses_[i] = kGeneric}. We expect that
        // this case won't ever happen for production modules, so guarding
        // against pathological cases seems more important than being lenient
        // towards almost-well-behaved modules.
        for (size_t j = 0; j < entries.size(); j++) {
          statuses_[j].store(WellKnownImport::kGeneric,
                             std::memory_order_relaxed);
        }
        return UpdateResult::kFoundIncompatibility;
      }
    }
  }
  return UpdateResult::kOK;
}

void WellKnownImportsList::Initialize(
    base::Vector<const WellKnownImport> entries) {
  DCHECK_EQ(entries.size(), static_cast<size_t>(size_));
  for (size_t i = 0; i < entries.size(); i++) {
    DCHECK_EQ(WellKnownImport::kUninstantiated,
              statuses_[i].load(std::memory_order_relaxed));
    statuses_[i].store(entries[i], std::memory_order_relaxed);
  }
}

}  // namespace v8::internal::wasm
