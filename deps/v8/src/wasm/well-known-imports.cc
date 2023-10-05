// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/well-known-imports.h"

#include "src/wasm/wasm-code-manager.h"

namespace v8::internal::wasm {

const char* WellKnownImportName(WellKnownImport wki) {
  switch (wki) {
    // Generic:
    case WellKnownImport::kUninstantiated:
      return "uninstantiated";
    case WellKnownImport::kGeneric:
      return "generic";

    // Functions:
    case WellKnownImport::kDoubleToString:
      return "DoubleToString";
    case WellKnownImport::kIntToString:
      return "IntToString";
    case WellKnownImport::kParseFloat:
      return "ParseFloat";
    case WellKnownImport::kStringCharCodeAt:
      return "String.charCodeAt";
    case WellKnownImport::kStringCodePointAt:
      return "String.codePointAt";
    case WellKnownImport::kStringCompare:
      return "String.compare";
    case WellKnownImport::kStringConcat:
      return "String.concat";
    case WellKnownImport::kStringEquals:
      return "String.equals";
    case WellKnownImport::kStringFromCharCode:
      return "String.fromCharCode";
    case WellKnownImport::kStringFromCodePoint:
      return "String.fromCodePoint";
    case WellKnownImport::kStringFromWtf16Array:
      return "String.fromWtf16Array";
    case WellKnownImport::kStringFromWtf8Array:
      return "String.fromWtf8Array";
    case WellKnownImport::kStringIndexOf:
      return "String.indexOf";
    case WellKnownImport::kStringLength:
      return "String.length";
    case WellKnownImport::kStringSubstring:
      return "String.substring";
    case WellKnownImport::kStringToLocaleLowerCaseStringref:
      return "String.toLocaleLowerCase";
    case WellKnownImport::kStringToLowerCaseStringref:
      return "String.toLowerCase";
    case WellKnownImport::kStringToWtf16Array:
      return "String.toWtf16Array";
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
