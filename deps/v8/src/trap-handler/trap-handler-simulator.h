// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TRAP_HANDLER_TRAP_HANDLER_SIMULATOR_H_
#define V8_TRAP_HANDLER_TRAP_HANDLER_SIMULATOR_H_

#include <cstdint>

#include "include/v8config.h"
#include "src/trap-handler/trap-handler.h"

// This header defines the ProbeMemory function to be used by simulators to
// trigger a signal at a defined location, before doing an actual memory access.

#ifdef V8_TRAP_HANDLER_VIA_SIMULATOR

namespace v8::internal::trap_handler {

// Probe a memory address by doing a 1-byte read from the given address. If the
// address is not readable, this will cause a trap as usual, but the trap
// handler will recognise the address of the instruction doing the access and
// treat it specially. It will use the given {pc} to look up the respective
// landing pad and return to this function to return that landing pad. If {pc}
// is not registered as a protected instruction, the signal will be propagated
// as usual.
// If the read at {address} succeeds, this function returns {0} instead.
uintptr_t ProbeMemory(uintptr_t address, uintptr_t pc)
// Specify an explicit symbol name (defined in
// handler-outside-simulator.cc). Just {extern "C"} would produce
// "ProbeMemory", but we want something more expressive on stack traces.
#if V8_OS_DARWIN
    asm("_v8_internal_simulator_ProbeMemory");
#else
    asm("v8_internal_simulator_ProbeMemory");
#endif

}  // namespace v8::internal::trap_handler

#endif  // V8_TRAP_HANDLER_VIA_SIMULATOR

#endif  // V8_TRAP_HANDLER_TRAP_HANDLER_SIMULATOR_H_
