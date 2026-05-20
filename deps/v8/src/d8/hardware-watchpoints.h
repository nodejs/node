// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_D8_HARDWARE_WATCHPOINTS_H_
#define V8_D8_HARDWARE_WATCHPOINTS_H_

#include "include/v8config.h"

#if defined(V8_ENABLE_MEMORY_CORRUPTION_API) && defined(V8_OS_LINUX) && \
    V8_TARGET_ARCH_X64
#define V8_ENABLE_HARDWARE_WATCHPOINT_SUPPORT
#endif

namespace v8 {

template <typename T>
class FunctionCallbackInfo;
class Value;

// Call this early in the process (beginning of the main function) to set up the
// process for hardware watchpoints for in-sandbox memory manipulation.
// This will fork the process and set up one process as tracer ("debugger"),
// while the other process will return from this function and continue execution
// (the "d8 process").
void SetupForHardwareWatchpoints(bool enable_tracing);

// Call this on exit to remove all hardware watchpoints.
void ResetAllHardwareWatchpoints();

// This callback will be called from d8 to set a new watchpoint.
void SetHardwareWatchpointCallback(const v8::FunctionCallbackInfo<v8::Value>&);

}  // namespace v8

#endif  // V8_D8_HARDWARE_WATCHPOINTS_H_
