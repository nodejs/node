// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Disable liftoff to make sure we generate turbofan traces.
// Flags: --no-liftoff
// Flags: --trace-turbo --trace-turbo-graph
// Flags: --trace-turbo-cfg-file=test/mjsunit/tools/turbo.cfg
// Flags: --trace-turbo-path=test/mjsunit/tools

// Only trace the wasm functions:
// Flags: --trace-turbo-filter=wasm*

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// The idea behind this test is to make sure we do not crash when using the
// --trace-turbo flag given different sort of inputs.

(function testWASM() {
  let builder = new WasmModuleBuilder();

  builder.addFunction("add", kSig_i_ii)
    .addBody([kExprLocalGet, 0,
              kExprLocalGet, 1,
              kExprI32Add])
    .exportFunc();

  let instance = builder.instantiate();
  instance.exports.add(21, 21);
})();
