// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/code-space-access.h"

#include "src/common/code-memory-access-inl.h"

namespace v8::internal::wasm {

CodeSpaceWriteScope::CodeSpaceWriteScope()
    : rwx_write_scope_("For wasm::CodeSpaceWriteScope.") {}

}  // namespace v8::internal::wasm
