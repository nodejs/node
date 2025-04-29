// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --stress-wasm-stack-switching --jit-fuzzing

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
const builder = new WasmModuleBuilder();

const call = (function() {

    function dummy(input) { return Object.hasOwn(input, 'dummy'); }

    function call(receiver, method) {
      for (let i = 0; i < 10; i++) dummy(receiver);
      try {
        Reflect.apply(receiver[method], receiver, []);
      } catch (e) {
      }
    }
    return call;
})();

call([], "flat");
const v7 = { p() {} };
for (let i = 0; i < 6; i++) {
  call(v7, "p");
}

let $tag0 = builder.addTag(kSig_v_v);
let w0 = builder.addFunction("w0", kSig_v_v).exportFunc().addBody([
    kExprThrow, $tag0,
  ]);

let v15 = builder.instantiate().exports;
call(v15, "w0");
