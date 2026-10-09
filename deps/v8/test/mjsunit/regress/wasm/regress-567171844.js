// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-custom-descriptors --wasm-js-interop

const wasm_bytes = new Uint8Array([
  0,97,115,109,1,0,0,0,1,26,6,94,111,1,94,112,1,94,120,1,96,4,99,0,99,1,99,2,111,0,96,0,0,96,0,0,
  2,81,4,18,119,97,115,109,58,106,115,45,112,114,111,116,111,116,121,112,101,115,12,99,111,110,
  102,105,103,117,114,101,65,108,108,0,3,1,99,12,99,111,110,115,116,114,117,99,116,111,114,115,
  3,111,0,3,109,111,100,10,111,117,116,101,114,95,102,117,110,99,0,4,1,99,5,112,114,111,116,111,
  3,111,0,3,3,2,4,5,8,1,3,9,11,2,5,111,1,35,0,11,1,0,1,2,12,1,1,10,37,2,4,0,16,1,11,30,0,65,0,
  65,1,251,10,0,0,65,0,65,1,251,10,1,1,65,0,65,14,251,9,2,0,35,0,16,0,11,11,17,1,1,14,1,0,1,0,
  8,109,121,77,101,116,104,111,100,127,0,25,4,110,97,109,101,1,18,2,2,8,109,121,77,101,116,104,
  111,100,3,5,115,116,97,114,116
]);

const myProto = {};
const constructors = {};
const handler = { apply: function() { return 42; } };
const proxy = new Proxy(function() {}, handler);

const wasm_module = new WebAssembly.Module(wasm_bytes,
  { builtins: ["js-prototypes"] });
const wasm_instance = new WebAssembly.Instance(
  wasm_module,
  {
    mod: {
      outer_func: proxy
    },
    c: { proto: myProto, constructors: constructors }
  }
);

for (let i = 0; i < 1000; i++) {
  constructors.myMethod();
}

handler.apply = 123;

assertThrows(() => constructors.myMethod(), TypeError,
  "number 123 is not a function");
