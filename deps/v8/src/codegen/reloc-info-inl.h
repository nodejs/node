// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_RELOC_INFO_INL_H_
#define V8_CODEGEN_RELOC_INFO_INL_H_

#include "src/codegen/assembler-inl.h"
#include "src/codegen/reloc-info.h"
#include "src/heap/heap-write-barrier-inl.h"

namespace v8 {
namespace internal {

void RelocInfo::set_target_object(Tagged<InstructionStream> host,
                                  Tagged<HeapObject> target,
                                  WriteBarrierMode write_barrier_mode,
                                  ICacheFlushMode icache_flush_mode) {
  set_target_object(target, icache_flush_mode);
  if (!v8_flags.disable_write_barriers) {
    WriteBarrierForCode(host, this, target, write_barrier_mode);
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_RELOC_INFO_INL_H_
