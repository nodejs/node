// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --shared-strings --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const kArity = 1000;
const kIterations = 200;
const builder = new WasmModuleBuilder();
const sig = makeSig(new Array(kArity).fill(kWasmI32), [kWasmI32]);
builder.addFunction('f', sig).addBody([kExprI32Const, 7]).exportFunc();
builder.addFunction('g', sig).addBody([kExprI32Const, 7]).exportFunc();
const wasmModule = builder.toModule();

const fnB = new WebAssembly.Instance(wasmModule).exports.g;
const exportsList = Array.from(
    {length: kIterations}, () => new WebAssembly.Instance(wasmModule).exports.f);

const mem = new Uint32Array(new Sandbox.MemoryView(0, 0x100000000));
const kDispatchOffset = Sandbox.getFieldOffset(
    Sandbox.getInstanceTypeIdFor('JS_FUNCTION'), 'dispatch_handle');
const handleB = mem[(Sandbox.getAddressOf(fnB) + kDispatchOffset) >>> 2];

const shared = new SharedArrayBuffer(32);
const control = new Int32Array(shared);
control[3] = handleB;
control[7] = 0;

const worker = new Worker(function() {
  onmessage = function(event) {
    const control = new Int32Array(event.data);
    const mem = new Uint32Array(new Sandbox.MemoryView(0, 0x100000000));
    Atomics.store(control, 5, 1);
    Atomics.notify(control, 5);
    while (Atomics.load(control, 0) !== 2) {
      if (Atomics.load(control, 0) !== 1) continue;
      const idx = Atomics.load(control, 1) >>> 0;
      const delay = Atomics.load(control, 6);
      for (let s = 0; s < delay; ++s) Atomics.load(control, 0);
      if (Atomics.load(control, 0) === 1) {
        mem[idx] = Atomics.load(control, 3);
        %SharedGC();
        Atomics.store(control, 0, 0);
      }
    }
  };
}, {type: 'function'});

worker.postMessage(shared);
Atomics.wait(control, 5, 0);

const targets = [fnB];
for (let i = 1; i < 32; ++i) targets.push(Function('return ' + i));

for (let i = 0; i < kIterations; ++i) {
  const fnA = exportsList[i];
  const idx = (Sandbox.getAddressOf(fnA) + kDispatchOffset) >>> 2;
  targets[0] = fnA;
  Atomics.store(control, 1, idx);
  Atomics.store(control, 6, i & 15);
  Atomics.store(control, 0, 1);
  const res = targets[control[7] & 31]();
  while (Atomics.load(control, 0) === 1) {}
  assertEquals(7, res);
}

Atomics.store(control, 0, 2);
worker.terminate();
