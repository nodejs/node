// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const MEMORY_PAGES = 32769;
const VULN_OFFSET = 0x7FFFFFFC;

const builder = new WasmModuleBuilder();
builder.addMemory(MEMORY_PAGES);
builder.addFunction("vuln_func", kSig_l_v)
    .addBody([
        kExprI32Const, 0,
        kExprI64LoadMem, 3, ...wasmUnsignedLeb(VULN_OFFSET),
        kExprI64Const, 32,
        kExprI64ShrU
    ])
    .exportFunc();

try {
  const instance = builder.instantiate();
  const vuln_func = instance.exports.vuln_func;
  vuln_func();
} catch (e) {
  // This test needs to allocate more than 2GB of memory, which won't work on
  // 32-bit systems.
  assertEquals(
      'RangeError: WebAssembly.Instance(): Out of memory: Cannot allocate Wasm memory for new instance',
      e.toString());
}
