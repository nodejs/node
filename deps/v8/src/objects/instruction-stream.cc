// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/instruction-stream.h"

#include "src/codegen/assembler-inl.h"
#include "src/codegen/flush-instruction-cache.h"
#include "src/codegen/reloc-info.h"
#include "src/objects/instruction-stream-inl.h"

namespace v8 {
namespace internal {

void InstructionStream::Relocate(intptr_t delta) {
  Code code;
  if (!TryGetCodeUnchecked(&code, kAcquireLoad)) return;
  // This is called during evacuation and code.instruction_stream() will point
  // to the old object. So pass *this directly to the RelocIterator.
  for (RelocIterator it(code, *this, unchecked_relocation_info(),
                        RelocInfo::kApplyMask);
       !it.done(); it.next()) {
    it.rinfo()->apply(delta);
  }
  FlushInstructionCache(instruction_start(), code.instruction_size());
}

}  // namespace internal
}  // namespace v8
