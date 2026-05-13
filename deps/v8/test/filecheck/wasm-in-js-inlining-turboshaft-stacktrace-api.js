// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbolev --turbolev-inline-js-wasm-wrappers
// Flags: --turboshaft-wasm-in-js-inlining
// Flags: --allow-natives-syntax
// Flags: --no-always-sparkplug
// Flags: --turbofan --no-stress-maglev
// Flags: --trace-turbo-inlining
// Concurrent inlining leads to additional traces.
// Flags: --no-concurrent-inlining
// Flags: --no-stress-concurrent-inlining

d8.file.execute("test/mjsunit/mjsunit.js");
d8.file.execute("test/mjsunit/wasm/wasm-gc-inlining-stacktrace-api.js");

// The first test should inline the Wasm body.
// CHECK: Considering JS-to-Wasm wrapper for Wasm function [{{[0-9]+}}] arrayLenNull of module {{.*}} for inlining
// CHECK-NEXT: - inlining wrapper
// CHECK-NEXT: Considering Wasm function [{{[0-9]+}}] arrayLenNull of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function

// The second test should not, because the Wasm call is in a try-catch block.
// CHECK: Considering JS-to-Wasm wrapper for Wasm function [{{[0-9]+}}] arrayLenNull of module {{.*}} for inlining
// CHECK-NEXT: - inlining wrapper
// CHECK-NEXT: Considering Wasm function [{{[0-9]+}}] arrayLenNull of module {{.*}} for inlining
// CHECK-NEXT: - not inlining: a current catch block is set
