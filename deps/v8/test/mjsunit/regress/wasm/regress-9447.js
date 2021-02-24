// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-simd

load('test/mjsunit/wasm/wasm-module-builder.js');

let kSig_s_v = makeSig([], [kWasmS128]);
// Generate a re-exported function that wraps a JavaScript callable, but with a
// function signature that is incompatible (i.e. simd return type) with JS.
var fun1 = (function GenerateFun1() {
  let builder = new WasmModuleBuilder();
  function fun() { return 0 }
  let fun_index = builder.addImport('m', 'fun', kSig_s_v)
  builder.addExport("fun", fun_index);
  let instance = builder.instantiate({ m: { fun: fun }});
  return instance.exports.fun;
})();

// Generate an exported function that calls the above re-export from another
// module, still with a function signature that is incompatible with JS.
var fun2 = (function GenerateFun2() {
  let builder = new WasmModuleBuilder();
  let fun_index = builder.addImport("m", "fun", kSig_s_v)
  builder.addFunction('main', kSig_v_v)
      .addBody([
        kExprCallFunction, fun_index,
        kExprDrop
      ])
      .exportFunc();
  let instance = builder.instantiate({ m: { fun: fun1 }});
  return instance.exports.main;
})();

// Both exported functions should throw, no matter how often they get wrapped.
assertThrows(fun1, TypeError,
             /type incompatibility when transforming from\/to JS/);
assertThrows(fun2, TypeError,
             /type incompatibility when transforming from\/to JS/);
