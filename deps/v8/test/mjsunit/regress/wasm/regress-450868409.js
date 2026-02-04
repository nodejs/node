// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

var imp = {};
Object.defineProperty(imp, 'mem', {
  get: () => {
    var mem_props = {initial: 1};
    Object.defineProperty(mem_props, 'maximum', {
      get: () => {
        throw 'foo'
      }
    });
    let x = new WebAssembly.Memory(mem_props);
  }
});

const builder = new WasmModuleBuilder();
builder.addImportedMemory('env', 'mem');
const buffer = builder.toBuffer();
assertPromiseResult(
    WebAssembly.instantiate(buffer, {env: imp}), assertUnreachable,
    value => assertEquals('foo', value));
