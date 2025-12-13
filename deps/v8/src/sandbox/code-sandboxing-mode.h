// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_CODE_SANDBOXING_MODE_H_
#define V8_SANDBOX_CODE_SANDBOXING_MODE_H_

#include <cinttypes>
#include <ostream>

#include "src/base/logging.h"

namespace v8 {
namespace internal {

// Sandboxing mode of a unit of code.
//
// Hardware support allows code to run in a sandboxed execution mode. This enum
// is used to specify the sandboxing mode that a given unit of code executes in.
enum class CodeSandboxingMode : uint8_t {
  // When code runs in sandboxed execution mode, it does not write to
  // out-of-sandbox memory but only to memory inside the sandbox.
  //
  // Without hardware sandboxing support, this is only an annotation, but with
  // hardware sandboxing support enabled, this property is actually enforced at
  // runtime, for example through the use of memory protection keys.
  //
  // Note: hardware sandboxing support is currently experimental and still
  // allows a number of other memory regions to be accessed by sandboxed code.
  kSandboxed,

  // In this mode, code can read and write all process' memory.
  kUnsandboxed,
};

inline std::ostream& operator<<(std::ostream& os, CodeSandboxingMode mode) {
  switch (mode) {
    case CodeSandboxingMode::kSandboxed:
      return os << "kSandboxed";
    case CodeSandboxingMode::kUnsandboxed:
      return os << "kUnsandboxed";
  }
  UNREACHABLE();
}

}  // namespace internal
}  // namespace v8

#endif  // V8_SANDBOX_CODE_SANDBOXING_MODE_H_
