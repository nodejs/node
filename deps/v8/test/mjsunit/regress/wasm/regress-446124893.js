// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-custom-descriptors

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
builder.startRecGroup();
let $desc1 = builder.nextTypeIndex() + 1;
let $struct0 = builder.addStruct({descriptor: $desc1});
/* $desc1 */ builder.addStruct({describes: $struct0});
let $desc3 = builder.nextTypeIndex() + 1;
let $struct2 = builder.addStruct({descriptor: $desc3, supertype: $struct0});
/* $desc3 */ builder.addStruct({describes: $struct2, supertype: $desc1});
builder.endRecGroup();

let sig_r_v = makeSig([], [kWasmAnyRef]);
builder.addFunction("boom", sig_r_v).exportFunc().addBody([
  kGCPrefix, kExprStructNew, $desc3,
  kGCPrefix, kExprStructNewDesc, $struct2,  // type: exact $struct2
  kGCPrefix, kExprRefGetDesc, $struct0,     // type: inexact (!) $desc1
  kGCPrefix, kExprStructNewDesc, $struct0,  // requires exact $desc1
]);

let error =
    /struct.new_desc\[0\] expected type \(ref null exact 1\), found ref.get_desc of type \(ref 1\)/;
assertThrows(() => builder.instantiate(), WebAssembly.CompileError, error);
