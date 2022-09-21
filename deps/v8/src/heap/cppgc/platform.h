// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_PLATFORM_H_
#define V8_HEAP_CPPGC_PLATFORM_H_

#include <string>

#include "include/cppgc/source-location.h"
#include "src/base/macros.h"

namespace cppgc {
namespace internal {

class HeapBase;

class V8_EXPORT_PRIVATE FatalOutOfMemoryHandler final {
 public:
  using Callback = void(const std::string&, const SourceLocation&, HeapBase*);

  FatalOutOfMemoryHandler() = default;
  explicit FatalOutOfMemoryHandler(HeapBase* heap) : heap_(heap) {}

  [[noreturn]] void operator()(
      const std::string& reason = std::string(),
      const SourceLocation& = SourceLocation::Current()) const;

  void SetCustomHandler(Callback*);

  // Disallow copy/move.
  FatalOutOfMemoryHandler(const FatalOutOfMemoryHandler&) = delete;
  FatalOutOfMemoryHandler& operator=(const FatalOutOfMemoryHandler&) = delete;

 private:
  HeapBase* heap_ = nullptr;
  Callback* custom_handler_ = nullptr;
};

// Gets the global OOM handler that is not bound to any specific Heap instance.
FatalOutOfMemoryHandler& GetGlobalOOMHandler();

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_PLATFORM_H_
