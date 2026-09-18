// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_D8_MEMORY_ACCESS_INFORMATION_H_
#define V8_D8_MEMORY_ACCESS_INFORMATION_H_

#include "include/v8config.h"

struct user_regs_struct;

#if defined(V8_ENABLE_MEMORY_CORRUPTION_API) && defined(V8_OS_LINUX) && \
    V8_TARGET_ARCH_X64
#define V8_ENABLE_HARDWARE_WATCHPOINT_SUPPORT

namespace v8 {

using reg_value_type = unsigned long long;  // NOLINT(runtime/int)

struct MemoryAccessInformation {
  enum Kind { kRead, kWrite, kCmp, kCmpxchg, kRepMovs };
  enum Extension { kZeroExtend, kSignExtend, kNoExtend };

  Kind kind;

  // For kRead kind, one of the two following fields will be set.

  // Pointer into the `user_regs_struct`.
  reg_value_type* result_reg = nullptr;
  // Index of the XMM register (0-15) if it's an XMM register.
  int xmm_reg_index = -1;
  // Index of the mask register (0-7) if it's an AVX-512 mask register.
  int k_reg_index = -1;

  int access_width = 8;
  // Width of the destination register. Differs from access_width only for
  // sign/zero-extending moves (e.g. movsxbl: byte source, 32-bit destination).
  int dest_width = 8;
  Extension extension = kNoExtend;
};

// Returns a pointer to the field in `regs` which was read before `regs.rip`.
// This is used when a watchpoint is hit to figure out if it was a read or
// write, and where the result of the read was stored.
// On a write, this returns `nullptr`.
MemoryAccessInformation ParseMemoryAccessInformationFromInstruction(
    const char* insn_pos, struct user_regs_struct& regs);

}  // namespace v8

#endif

#endif  // V8_D8_MEMORY_ACCESS_INFORMATION_H_
