// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --enable-testing-opcode-in-wasm

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(1, 1);

const sig_r_r = makeSig([kWasmExternRef], [kWasmExternRef]);
const sig_l_ir = makeSig([kWasmI32, kWasmExternRef], [kWasmI64]);

const id = builder.addImport('m', 'id', sig_r_r);

const kNumLocals = 15;
const body = [
  // Force Liftoff to bail out so TurboFan compiles this function with
  // `for_debugging` set (cross-arch, including x64 and arm64).
  kExprNopForTestingUnsupportedInLiftoff,
  kExprLocalGet, 0,
  kExprI32Eqz,
  kExprIf, kWasmVoid,
    // Keep an externref spilled across a call to record a tagged spill slot in
    // the safepoint table.
    kExprLocalGet, 1,
    kExprCallFunction, id,
    kExprLocalGet, 1,
    kExprCallFunction, id,
    kExprDrop,
    kExprRefIsNull,
    kExprI64UConvertI32,
    kExprReturn,
  kExprElse,
];

for (let i = 0; i < kNumLocals; i++) {
  body.push(
      kExprLocalGet, 0,
      kExprI64UConvertI32,
      ...wasmI64Const(0x41414143 + 4 * i),
      kExprI64Add,
      kExprLocalSet, 2 + i);
}

body.push(
    // Trap via the trap handler while untagged i64 values occupy the spill
    // slots.
    kExprLocalGet, 0,
    kExprI32LoadMem, 2, 0,
    kExprI64UConvertI32);

for (let i = 0; i < kNumLocals; i++) {
  body.push(kExprLocalGet, 2 + i, kExprI64Add);
}

body.push(
    kExprReturn,
  kExprEnd,
  ...wasmI64Const(0));

builder.addFunction('test', sig_l_ir)
    .addLocals(kWasmI64, kNumLocals)
    .addBody(body)
    .exportFunc();

%WasmEnterDebugging();
const instance = builder.instantiate({m: {id: x => x}});
const test = instance.exports.test;
const ref = {};

Error.stackTraceLimit = 100;
function callWithFrames(n) {
  if (n > 0) return callWithFrames(n - 1);
  return test(0x100000, ref);
}

for (let i = 0; i < 300; i++) {
  try {
    callWithFrames(100);
    assertUnreachable();
  } catch (e) {
    assertInstanceof(e, WebAssembly.RuntimeError);
  }
}
