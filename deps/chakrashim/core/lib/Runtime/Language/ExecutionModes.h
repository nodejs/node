//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Non-profiling interpreter
//     - For instance, it is used for NoNative mode
//     - Does not transition to other execution modes
EXECUTION_MODE(Interpreter)

// Auto-profiling interpreter
//     - Starts in min-profiling mode
//     - Switches to profiling mode for loops based on iteration count
//     - Switches back to min-profiling mode upon leaving a loop
EXECUTION_MODE(AutoProfilingInterpreter)

// Profiling interpreter (does full profiling)
EXECUTION_MODE(ProfilingInterpreter)

// Simple JIT
//     - Behavior is determined based on the NewSimpleJit flag
//     - Off: Behave as old simple JIT (does full profiling)
//     - On: Behave as new simple JIT (does not profile, includes fast paths)
EXECUTION_MODE(SimpleJit)

// Full JIT (no profiling, self-explanatory)
EXECUTION_MODE(FullJit)

EXECUTION_MODE(Count)
