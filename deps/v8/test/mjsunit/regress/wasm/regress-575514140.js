// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that lazy TurboFan compilation (--no-liftoff) handles well-known import
// assumptions being invalidated concurrently on another thread before the
// compiled code is published.

// Flags: --no-liftoff

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const imp = builder.addImport('m', 'f', kSig_d_d);
const notify = builder.addImport('m', 'notify', kSig_v_v);
const body = [kExprLocalGet, 0];
for (let i = 0; i < 30; i++) {
  body.push(kExprCallFunction, imp);
}
const callee = builder.addFunction(undefined, kSig_d_d).addBody(body);
// Signal the worker from Wasm right before calling the lazily compiled callee
// so the worker races directly against callee's lazy compilation.
builder.addFunction(undefined, kSig_d_d)
    .addBody([
      kExprCallFunction, notify,
      kExprLocalGet, 0,
      kExprCallFunction, callee.index,
    ])
    .exportAs('main');
// Mutate a custom section byte per round to bypass the NativeModule cache.
builder.addCustomSection('s', [0]);
const wire_bytes = new Uint8Array(builder.toBuffer());

const kRounds = 4;
const modules = Array.from({length: kRounds}, (_, i) => {
  wire_bytes[wire_bytes.length - 1] = i;
  return new WebAssembly.Module(wire_bytes);
});
// sync[0]: main-thread round; sync[1]: worker round; sync[2]: worker ready.
const sab = new SharedArrayBuffer(12);
const sync = new Int32Array(sab);

function workerCode() {
  onmessage = ({data: {modules, sab}}) => {
    const sync = new Int32Array(sab);
    Atomics.store(sync, 2, 1);
    for (let r = 0; r < modules.length; r++) {
      while (Atomics.load(sync, 0) <= r) {
      }
      // Instantiating with Math.cos invalidates the Math.sin assumption.
      new WebAssembly.Instance(modules[r], {m: {f: Math.cos, notify: () => {}}});
      Atomics.store(sync, 1, r + 1);
    }
  };
}

const worker = new Worker(workerCode, {type: 'function'});
worker.postMessage({modules, sab});
while (Atomics.load(sync, 2) === 0) {
}

for (let r = 0; r < kRounds; r++) {
  // First instantiation sets the well-known import status to Math.sin.
  const instance = new WebAssembly.Instance(modules[r], {
    m: {f: Math.sin, notify: () => Atomics.store(sync, 0, r + 1)},
  });
  assertEquals(0, instance.exports.main(0));
  while (Atomics.load(sync, 1) <= r) {
  }
}
worker.terminate();
