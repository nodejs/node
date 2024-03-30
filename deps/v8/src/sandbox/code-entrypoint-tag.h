// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_CODE_ENTRYPOINT_TAG_H_
#define V8_SANDBOX_CODE_ENTRYPOINT_TAG_H_

#include "src/common/globals.h"

namespace v8 {
namespace internal {

// A tag to distinguish code pointers with different calling conventions.
//
// When the sandbox is active, Code objects (and their entrypoints) are
// referenced from inside the sandbox through the code pointer table (CPT). As
// different types of Code objects use different calling conventions, an
// attacker must be prevented from invoking a Code object with the wrong
// calling convention. For example, a JavaScript function call should not end
// up invoking a bytecode handler or a WebAssembly routine. Code entrypoint
// tags are used for that purpose: the entrypoint pointer in the CPT is tagged
// with this tag, and the callee untags it with the expected tag. If there is a
// tag mismatch, the entrypoint pointer will point to an invalid address.
constexpr int kCodeEntrypointTagShift = 48;
enum CodeEntrypointTag : uint64_t {
  kDefaultCodeEntrypointTag = 0,
  kBytecodeHandlerEntrypointTag = uint64_t{1} << kCodeEntrypointTagShift,
  // TODO(saelo): create more of these tags. Likely we'll also want to
  // distinguish between Wasm, RegExp, and JavaScript code.
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SANDBOX_CODE_ENTRYPOINT_TAG_H_
