// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --wasm-lazy-compilation --expose-gc

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const num_functions = 2;

function create_builder() {
  const builder = new WasmModuleBuilder();
  for (let i = 0; i < num_functions; ++i) {
    builder.addFunction('f' + i, kSig_i_v)
        .addBody(wasmI32Const(i))
        .exportFunc();
  }
  return builder;
}

const wire_bytes = create_builder().toBuffer();

function serializeModule() {
  const module = new WebAssembly.Module(wire_bytes);
  const buff = %SerializeWasmModule(module);
  return buff;
};

const serialized_module = serializeModule();
// Do some GCs to make sure the first module got collected and removed from the
// module cache.
gc();
gc();
gc();

(function testSerializedModule() {
  print(arguments.callee.name);
  const module = %DeserializeWasmModule(serialized_module, wire_bytes);

  const instance = new WebAssembly.Instance(module);
  assertEquals(0, instance.exports.f0());
  assertEquals(1, instance.exports.f1());
})();
