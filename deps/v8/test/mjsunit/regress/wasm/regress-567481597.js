// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-stack-switching-stack-size=1

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const kNumParams = 1000;
const builder = new WasmModuleBuilder();
const sig = builder.addType(makeSig(new Array(kNumParams).fill(kWasmF64), []));
builder.addFunction('f', sig).exportFunc().addBody([]);
const instance = builder.instantiate();
const promising = WebAssembly.promising(instance.exports.f);

// On platforms with 4 KB page allocation granularity (e.g. Linux), the
// secondary stack is rounded up to 8 KB (4 KB usable), so 1000 f64 parameters
// (~8 KB) trigger the preemptive stack overflow check and reject with a
// RangeError. On platforms with 16 KB or 64 KB allocation granularity (e.g.
// macOS arm64 or Windows), the secondary stack is rounded up to a full page
// and the call succeeds.
assertPromiseResult(
    promising(...new Array(kNumParams)),
    () => {},
    e => assertInstanceof(e, RangeError));
