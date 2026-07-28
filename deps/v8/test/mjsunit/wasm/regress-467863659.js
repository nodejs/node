// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --experimental-wasm-stringref

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
let type  = builder.addType(makeSig([kWasmStringRef], [kWasmStringRef]));
let to_lower_case = builder.addImport("m", "toLowerCase", type);

builder.addFunction('call_tolower', type)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprCallFunction, to_lower_case
    ]);


let imports = ({
  m: {
    toLowerCase: Function.prototype.call.bind(String.prototype.toLowerCase)
  }
});

let instance = new WebAssembly.Instance(builder.toModule(), imports);
instance.exports.call_tolower("ABC");
