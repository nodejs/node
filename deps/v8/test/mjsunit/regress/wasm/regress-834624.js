// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-interpret-all

load("test/mjsunit/wasm/wasm-module-builder.js");

let instance;
(function DoTest() {
  function call_main() {
    instance.exports.main();
  }
  let module = new WasmModuleBuilder();
  module.addImport('mod', 'func', kSig_v_i);
  module.addFunction('main', kSig_v_i)
    .addBody([kExprLocalGet, 0, kExprCallFunction, 0])
    .exportFunc();
  instance = module.instantiate({
    mod: {
      func: call_main
    }
  });
  try {
    instance.exports.main();
  } catch (e) {
    // ignore
  }
})();
