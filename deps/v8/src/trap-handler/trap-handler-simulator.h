// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TRAP_HANDLER_TRAP_HANDLER_SIMULATOR_H_
#define V8_TRAP_HANDLER_TRAP_HANDLER_SIMULATOR_H_

#include <cstdint>

// This header defines the ProbeMemory function to be used by simulators to
// trigger a signal at a defined location, before doing an actual memory access.

// This implementation is only usable on an x64 host with non-x64 target (i.e. a
// simulator build on x64).
#if (!defined(_M_X64) && !defined(__x86_64__)) || defined(V8_TARGET_ARCH_X64)
#error "Do only include this file on simulator builds on x64."
#endif

namespace v8 {
namespace internal {
namespace trap_handler {

// Probe a memory address by doing a 1-byte read from the given address. If the
// address is not readable, this will cause a trap as usual, but the trap
// handler will recognise the address of the instruction doing the access and
// treat it specially. It will use the given {pc} to look up the respective
// landing pad and return to this function to return that landing pad. If {pc}
// is not registered as a protected instruction, the signal will be propagated
// as usual.
// If the read at {address} succeeds, this function returns {0} instead.
extern "C" uintptr_t ProbeMemory(uintptr_t address, uintptr_t pc);

}  // namespace trap_handler
}  // namespace internal
}  // namespace v8

#endif  // V8_TRAP_HANDLER_TRAP_HANDLER_SIMULATOR_H_
