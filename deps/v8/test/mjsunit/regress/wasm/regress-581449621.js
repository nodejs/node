// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-compilation-hints --wasm-test-streaming

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const pad_body = new Uint8Array(200000);
pad_body.fill(kExprNop);
pad_body[pad_body.length - 3] = kExprLocalGet;
pad_body[pad_body.length - 2] = 0;
pad_body[pad_body.length - 1] = kExprEnd;

const promises = [];
for (let i = 0; i < 3; i++) {
  const builder = new WasmModuleBuilder();
  const sig = builder.addType(kSig_i_i);
  builder.addGlobal(kWasmI32, false, false, wasmI32Const(i));

  const caller = builder.addFunction('caller', sig)
    .addBody([
      kExprLocalGet, 0,
      kExprCallFunction, 2,
    ])
    .exportFunc();

  builder.addFunction('pad', sig).addBodyWithEnd(pad_body);

  const callee = builder.addFunction('callee', sig)
    .addBody([kExprLocalGet, 0]);

  builder.setCompilationPriority(caller.index, 0, 0);
  builder.setInstructionFrequencies(caller.index, [{offset: 3, frequency: 64}]);
  builder.setInstructionFrequencies(callee.index, [{offset: 1, frequency: 64}]);

  promises.push(WebAssembly.compile(builder.toBuffer()));
}
assertPromiseResult(Promise.all(promises));
