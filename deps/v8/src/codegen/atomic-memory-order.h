// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_ATOMIC_MEMORY_ORDER_H_
#define V8_CODEGEN_ATOMIC_MEMORY_ORDER_H_

#include <ostream>

#include "src/base/logging.h"

namespace v8 {
namespace internal {

// Atomic memory orders supported by the compiler.
enum class AtomicMemoryOrder : uint8_t { kAcqRel, kSeqCst };

inline size_t hash_value(AtomicMemoryOrder order) {
  return static_cast<uint8_t>(order);
}

inline std::ostream& operator<<(std::ostream& os, AtomicMemoryOrder order) {
  switch (order) {
    case AtomicMemoryOrder::kAcqRel:
      return os << "kAcqRel";
    case AtomicMemoryOrder::kSeqCst:
      return os << "kSeqCst";
  }
  UNREACHABLE();
}

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_ATOMIC_MEMORY_ORDER_H_
