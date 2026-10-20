// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test reuses the Wasm GC inlining tests from the old Turbofan-based
// inlining, but executes it with the new Turboshaft Wasm-in-JS inlining and
// adds tracing to check that we actually inline.

// Flags: --allow-natives-syntax --turbofan
// Flags: --turbolev --wasm-in-js-inlining-wrapper --wasm-in-js-inlining-body
// Disable V8 stress modes and baseline compilers to guarantee deterministic compilation.
// Flags: --no-always-sparkplug --no-stress-maglev --no-stress-concurrent-inlining
// Flags: --trace-turbo-inlining

// =============================================================================
// CHECK-LABEL: TestInliningStructGet
// =============================================================================
// -- Non-nullable signature (createStruct and getField) rejected --
// CHECK: Considering JS-to-Wasm wrapper for Wasm function [2] createStruct of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - not inlining: unsupported types in Wasm signature
// CHECK: Considering JS-to-Wasm wrapper for Wasm function [3] getField of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - not inlining: unsupported types in Wasm signature

// -- Nullable signature (createStructNull and getFieldNull) wrapper inlined --
// CHECK: Considering JS-to-Wasm wrapper for Wasm function [0] createStructNull of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining wrapper
// CHECK: Considering JS-to-Wasm wrapper for Wasm function [1] getFieldNull of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining wrapper

// -- Body inlining: struct.new rejected, struct.get succeeded --
// CHECK: Considering Wasm function [0] createStructNull of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - not inlining: unsupported operation: struct.new
// CHECK: Considering Wasm function [1] getFieldNull of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining Wasm function

// -- Exceptional cases successfully wrapper & body inlined --
// CHECK: Test exceptional cases
// CHECK: - test get null
// CHECK: Considering JS-to-Wasm wrapper for Wasm function [1] getFieldNull of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining wrapper
// CHECK: Considering Wasm function [1] getFieldNull of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining Wasm function

// CHECK: - test undefined
// CHECK: Considering JS-to-Wasm wrapper for Wasm function [1] getFieldNull of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining wrapper
// CHECK: Considering Wasm function [1] getFieldNull of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining Wasm function

// CHECK: - test Smi
// CHECK: Considering JS-to-Wasm wrapper for Wasm function [1] getFieldNull of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining wrapper
// CHECK: Considering Wasm function [1] getFieldNull of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining Wasm function

// CHECK: - test -0
// CHECK: Considering JS-to-Wasm wrapper for Wasm function [1] getFieldNull of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining wrapper
// CHECK: Considering Wasm function [1] getFieldNull of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining Wasm function

// CHECK: - test HeapNumber with fractional digits
// CHECK: Considering JS-to-Wasm wrapper for Wasm function [1] getFieldNull of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining wrapper
// CHECK: Considering Wasm function [1] getFieldNull of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining Wasm function

// CHECK: - test Smi/HeapNumber too large for i31ref
// CHECK: Considering JS-to-Wasm wrapper for Wasm function [1] getFieldNull of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining wrapper
// CHECK: Considering Wasm function [1] getFieldNull of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining Wasm function

// -- JS try-catch block inlining rejected --
// CHECK: - test inlining into try block
// CHECK: Considering JS-to-Wasm wrapper for Wasm function [1] getFieldNull of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining wrapper
// CHECK: Considering Wasm function [1] getFieldNull of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - not inlining: a current catch block is set

// =============================================================================
// CHECK-LABEL: TestInliningStructgetFieldTypes
// =============================================================================
// CHECK: Considering JS-to-Wasm wrapper for Wasm function [1] getI64 of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining wrapper
// CHECK: Considering Wasm function [1] getI64 of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining Wasm function

// CHECK: Considering JS-to-Wasm wrapper for Wasm function [2] getF64 of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining wrapper
// CHECK: Considering Wasm function [2] getF64 of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining Wasm function

// CHECK: Considering JS-to-Wasm wrapper for Wasm function [3] getI8 of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining wrapper
// CHECK: Considering Wasm function [3] getI8 of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining Wasm function

// CHECK: Considering JS-to-Wasm wrapper for Wasm function [4] getI16 of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining wrapper
// CHECK: Considering Wasm function [4] getI16 of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining Wasm function

// =============================================================================
// CHECK-LABEL: TestInliningMultiModule
// =============================================================================
// CHECK: Considering JS-to-Wasm wrapper for Wasm function [1] get of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining wrapper
// CHECK: Considering JS-to-Wasm wrapper for Wasm function [1] get of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - not inlining: already inlining from another Wasm instance
// CHECK: Considering Wasm function [1] get of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining Wasm function

// =============================================================================
// CHECK-LABEL: TestInliningExternConvertAny
// =============================================================================
// CHECK: Considering JS-to-Wasm wrapper for Wasm function [1] getRef of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining wrapper
// CHECK: Considering Wasm function [1] getRef of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining Wasm function
// CHECK: Considering JS-to-Wasm wrapper for Wasm function [2] getVal of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining wrapper
// CHECK: Considering Wasm function [2] getVal of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining Wasm function

// =============================================================================
// CHECK-LABEL: TestArrayLen
// =============================================================================
// CHECK: Considering JS-to-Wasm wrapper for Wasm function [1] arrayLen of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining wrapper
// CHECK: Considering Wasm function [1] arrayLen of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining Wasm function

// =============================================================================
// CHECK-LABEL: TestArrayGet
// =============================================================================
// CHECK: Considering JS-to-Wasm wrapper for Wasm function [1] get of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining wrapper
// CHECK: Considering Wasm function [1] get of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining Wasm function

// =============================================================================
// CHECK-LABEL: TestArrayGetPacked
// =============================================================================
// CHECK: - test getS
// CHECK: Considering JS-to-Wasm wrapper for Wasm function [1] getS of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining wrapper
// CHECK: Considering Wasm function [1] getS of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining Wasm function

// CHECK: - test getU
// CHECK: Considering JS-to-Wasm wrapper for Wasm function [2] getU of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining wrapper
// CHECK: Considering Wasm function [2] getU of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining Wasm function

// =============================================================================
// CHECK-LABEL: TestCastArray
// =============================================================================
// CHECK: Considering JS-to-Wasm wrapper for Wasm function [2] castArray of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining wrapper
// CHECK: Considering Wasm function [2] castArray of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining Wasm function

// =============================================================================
// CHECK-LABEL: TestInliningArraySet
// =============================================================================
// CHECK: Considering JS-to-Wasm wrapper for Wasm function [2] set of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining wrapper
// CHECK: Considering Wasm function [2] set of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining Wasm function

// =============================================================================
// CHECK-LABEL: TestInliningStructSet
// =============================================================================
// CHECK: Considering JS-to-Wasm wrapper for Wasm function [2] set of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining wrapper
// CHECK: Considering Wasm function [2] set of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining Wasm function

// =============================================================================
// CHECK-LABEL: TestInliningTrapStackTrace
// =============================================================================
// CHECK: Considering JS-to-Wasm wrapper for Wasm function [1] getField of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining wrapper
// CHECK: Considering Wasm function [1] getField of module {{0x[0-9a-f]+}} for inlining
// CHECK-NEXT: - inlining Wasm function

d8.file.execute("test/mjsunit/mjsunit.js");

// Do not optimize the assertThrows helper, since otherwise JS-into-JS inlining
// of `assertTraps` -> `assertThrows` -> `executeCode` -> anonymous lambda ->
// Wasm function to test might inline the Wasm call into a try-catch block,
// which would cause bailouts of the Wasm-in-JS inlining
// ("not inlining: a current catch block is set").
%NeverOptimizeFunction(assertThrows);

d8.file.execute("test/mjsunit/wasm/wasm-gc-inlining.js");
