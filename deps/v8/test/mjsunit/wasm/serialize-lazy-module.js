// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The test needs --no-liftoff because we can't serialize and deserialize
// Liftoff code.
// Flags: --wasm-lazy-compilation --allow-natives-syntax --expose-gc
// Flags: --no-liftoff

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function SerializeUncompiledModule() {
  print(arguments.callee.name);
  const [wire_bytes, i1, buff] = (function GenerateInstance() {
    const builder = new WasmModuleBuilder();

    // Add 20 functions.
    for (let i = 0; i < 20; ++i) {
      builder.addFunction('f' + i, kSig_i_i)
          .addBody([kExprI32Const, i])
          .exportFunc();
    }

    const wire_bytes = builder.toBuffer();
    const module = new WebAssembly.Module(wire_bytes);
    // Run one function so that serialization happens.
    let instance = new WebAssembly.Instance(module);
    instance.exports.f3();
    const buff = %SerializeWasmModule(module);
    return [wire_bytes, instance, buff];
  })();

  gc();
  const module = %DeserializeWasmModule(buff, wire_bytes);

  const i2 = new WebAssembly.Instance(module);

  assertEquals(13, i2.exports.f13());
  assertEquals(11, i1.exports.f11());
})();

(function SerializePartlyCompiledModule() {
  print(arguments.callee.name);
  const [wire_bytes, i1, buff] = (function GenerateInstance() {
    const builder = new WasmModuleBuilder();

    // Add 20 functions.
    for (let i = 0; i < 20; ++i) {
      builder.addFunction('f' + i, kSig_i_i)
          .addBody([kExprI32Const, i])
          .exportFunc();
    }

    const wire_bytes = builder.toBuffer();
    const module = new WebAssembly.Module(wire_bytes);
    const i1 = new WebAssembly.Instance(module);
    // Run one function so that serialization happens.
    i1.exports.f3();
    const buff = %SerializeWasmModule(module);

    assertEquals(2, i1.exports.f2());
    assertEquals(11, i1.exports.f11());

    return [wire_bytes, i1, buff];
  })();

  gc();
  const module = %DeserializeWasmModule(buff, wire_bytes);

  const i2 = new WebAssembly.Instance(module);

  assertEquals(13, i2.exports.f13());
  assertEquals(11, i1.exports.f11());
  assertEquals(9, i1.exports.f9());
})();
