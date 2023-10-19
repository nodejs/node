// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --wasm-test-streaming --wasm-lazy-compilation --wasm-lazy-validation

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');


(function f1() {
  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 1);
  builder.addFunction('main', kSig_i_i).addBody([
      kExprLocalGet, 0,
      kExprI32LoadMem, 0, 0
  ]).exportFunc();
  const instance = builder.instantiate();
  instance.exports.main();
})();

(function f2() {
  const builder = new WasmModuleBuilder();
  builder.addFunction('id', kSig_i_i).addBody([]).exportFunc();
  const buffer = builder.toBuffer();
  const instance = builder.instantiate();
  try {
    instance.exports.id();
  } catch {}
})();
