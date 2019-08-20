// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

// crbug.com/933134
(function() {
  var builder = new WasmModuleBuilder();
  builder.addImportedGlobal("mod", "i32", kWasmI32);
  builder.addImportedGlobal("mod", "f32", kWasmF32);
  var module = new WebAssembly.Module(builder.toBuffer());
  return new WebAssembly.Instance(module, {
    mod: {
      i32: 1e12,
      f32: 1e300,
    }
  });
})();
