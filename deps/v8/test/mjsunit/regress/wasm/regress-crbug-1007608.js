// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Bug is in the C-to-Wasm entry, used e.g. by the Wasm interpreter.
// Flags: --wasm-interpret-all

load("test/mjsunit/wasm/wasm-module-builder.js");

let argc = 7;
let builder = new WasmModuleBuilder();
let types = new Array(argc).fill(kWasmI32);
let sig = makeSig(types, []);
let body = [];
for (let i = 0; i < argc; ++i) {
  body.push(kExprLocalGet, i);
}
body.push(kExprCallFunction, 0);
builder.addImport('', 'f', sig);
builder.addFunction("main", sig).addBody(body).exportAs('main');
let instance = builder.instantiate({
  '': {
    'f': function() { throw "don't crash"; }
  }
});
assertThrows(instance.exports.main);
