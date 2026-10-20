// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

// Test JS-to-Wasm wrapper inlining in Turbolev and ensure eager deopt during
// argument conversion does not rewind execution past non-idempotent calls.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const sig = builder.addType(makeSig([kWasmI32, kWasmI32], []));

// 1. Import JS callback 'cb' as Wasm function index 0.
const importIdx = builder.addImport('env', 'f', sig);

// 2. Export function index 0 (the imported JS callback 'cb') as 'main'.
builder.addExport('main', importIdx);

let count = 0;
let wasmFn;
function cb(n) {
  count++;
  if (n > 0) {
    wasmFn(n - 1, 0);
    // Passing a JSObject to an i32 parameter.
    // Baseline: Out-of-line wrapper converts to i32 (0) without deopting.
    // Turbolev: Inlined wrapper eagerly deoptimizes cb.
    wasmFn(n - 1, {});
  }
}

// 3. 'wasmFn' is the JS-to-Wasm export wrapper wrapping imported JS callback 'cb'.
wasmFn = builder.instantiate({ env: { f: cb } }).exports.main;

%PrepareFunctionForOptimization(cb);
cb(1);
assertEquals(3, count);

%OptimizeFunctionOnNextCall(cb);
cb(1);
assertEquals(6, count);
