// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --print-wasm-code

// Just test that printing the code of the following wasm modules does not
// crash.

load('test/mjsunit/wasm/wasm-constants.js');
load('test/mjsunit/wasm/wasm-module-builder.js');

(function print_deserialized_code() {
  // https://crbug.com/849656
  print(arguments.callee.name);
  var builder = new WasmModuleBuilder();
  builder.addImport('', 'imp', kSig_i_v);

  builder.addFunction('main', kSig_i_v)
      .addBody([
        kExprCallFunction,
        0,
      ])
      .exportFunc();

  var wire_bytes = builder.toBuffer();
  var module = new WebAssembly.Module(wire_bytes);
  print('serializing');
  var buff = %SerializeWasmModule(module);
  print('deserializing');
  module = %DeserializeWasmModule(buff, wire_bytes);
})();
