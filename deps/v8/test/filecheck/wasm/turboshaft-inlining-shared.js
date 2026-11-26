// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Reuses the shared-everything Wasm-in-JS inlining test cases from
// turbofan-inlining-shared.js with the Turboshaft Wasm-in-JS inlining pipeline.

// Flags: --wasm-shared --turbolev --turbofan --allow-natives-syntax
// Flags: --wasm-in-js-inlining-wrapper --wasm-in-js-inlining-body
// Flags: --no-always-sparkplug --no-stress-maglev
// Flags: --trace-turbo-inlining --turbo-filter=js_*
// Flags: --no-stress-concurrent-inlining

// CHECK: Considering Wasm function [0] main of module {{.*}} for inlining
// CHECK-NEXT: - not inlining: module uses experimental 'shared' feature

// CHECK: Considering Wasm function [0] get_shared_i31 of module {{.*}} for inlining
// CHECK-NEXT: - not inlining: module uses experimental 'shared' feature

// CHECK: Considering Wasm function [1] get_unshared of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function

d8.file.execute('test/filecheck/wasm/turbofan-inlining-shared.js');
