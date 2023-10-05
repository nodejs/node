// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_WRITE_BARRIER_KIND_H_
#define V8_COMPILER_WRITE_BARRIER_KIND_H_

#include <ostream>

#include "src/base/logging.h"

namespace v8 {
namespace internal {
namespace compiler {

// Write barrier kinds supported by compiler.
enum WriteBarrierKind : uint8_t {
  kNoWriteBarrier,
  kAssertNoWriteBarrier,
  kMapWriteBarrier,
  kPointerWriteBarrier,
  kIndirectPointerWriteBarrier,
  kEphemeronKeyWriteBarrier,
  kFullWriteBarrier
};

inline size_t hash_value(WriteBarrierKind kind) {
  return static_cast<uint8_t>(kind);
}

inline std::ostream& operator<<(std::ostream& os, WriteBarrierKind kind) {
  switch (kind) {
    case kNoWriteBarrier:
      return os << "NoWriteBarrier";
    case kAssertNoWriteBarrier:
      return os << "AssertNoWriteBarrier";
    case kMapWriteBarrier:
      return os << "MapWriteBarrier";
    case kPointerWriteBarrier:
      return os << "PointerWriteBarrier";
    case kIndirectPointerWriteBarrier:
      return os << "IndirectPointerWriteBarrier";
    case kEphemeronKeyWriteBarrier:
      return os << "EphemeronKeyWriteBarrier";
    case kFullWriteBarrier:
      return os << "FullWriteBarrier";
  }
  UNREACHABLE();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_WRITE_BARRIER_KIND_H_
