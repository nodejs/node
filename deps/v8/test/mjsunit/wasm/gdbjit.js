// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --gdbjit

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// A simple test to ensure that passing the --gdbjit flag doesn't crash.
(function testGdbJitFlag() {
  const builder = new WasmModuleBuilder();
  builder.addFunction('i32_add', kSig_i_ii)
      .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Add])
      .exportFunc();

  const module = new WebAssembly.Module(builder.toBuffer());
  const instance = new WebAssembly.Instance(module);

  assertEquals(instance.exports.i32_add(1, 2), 3);
}());
