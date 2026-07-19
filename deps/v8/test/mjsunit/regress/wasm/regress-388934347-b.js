// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Force eager compilation; if Liftoff bails out (because of missing SSSE3) it
// compiles with TurboFan, which eventually triggers caching. This was missing
// support for --single-threaded.
// Flags: --no-enable-ssse3 --no-wasm-lazy-compilation --single-threaded

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
for (let i = 0; i < 5; ++i) {
builder.addFunction(`fct${i}`, kSig_i_i).addBody([
    ...wasmF64Const(0),
    kNumericPrefix, kExprI32UConvertSatF64,
    ]).exportFunc();
}
builder.instantiate();
