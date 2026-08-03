// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-staging

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
let js_import_index = builder.addImport('m', 'f', kSig_v_v);
builder.addFunction('main', kSig_v_v).addBody([
    kExprCallFunction, js_import_index
]).exportFunc();
let callback = () => {};
let wasm_inst = builder.instantiate({m: {f: () => callback()}});
const array = [12.34, 56.78];
array.fork_map = 1337;

function deopt_target() {
  WebAssembly.promising(wasm_inst.exports.main)();
  array[0] = 13.37;
}

(function() {
  function disable_opts() {}
  disable_opts(...Array(100).fill(0));
  for(let i = 0; i < 10000; i++) deopt_target();
})();

callback = () => array[0] = "foo";

deopt_target();
console.log(array[0]);
