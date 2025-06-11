// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-custom-descriptors --allow-natives-syntax

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

function StringToArray(str) {
  let result = [str.length];
  for (let c of str) result.push(c.charCodeAt(0));
  return result;
}

var builder = new WasmModuleBuilder();
builder.startRecGroup();
var $struct0 = builder.addStruct(
    {fields: [makeField(kWasmI32, true)], descriptor: 1});
var $desc0 = builder.addStruct({describes: $struct0});
builder.endRecGroup();

var $glob0 = builder.addGlobal(wasmRefType($desc0).exact(), false, false, [
  kGCPrefix, kExprStructNewDefault, $desc0
]);

let $make = builder.addFunction("make", makeSig([], [kWasmExternRef]))
    .exportFunc()
    .addBody([
      kExprGlobalGet, $glob0.index,
      kGCPrefix, kExprStructNewDefault, $struct0,
      kGCPrefix, kExprExternConvertAny,
    ]);

// As of writing this test, we need to use `externref` rather than a more
// specific type like (ref $struct0) if we want wrapper inlining to happen.
let $get = builder.addFunction("get", makeSig([kWasmExternRef], [kWasmI32]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprAnyConvertExtern,
      kGCPrefix, kExprRefCast, $struct0,
      kGCPrefix, kExprStructGet, $struct0, 0,
    ]);

// An extra i32.add instruction currently makes the function non-inlineable,
// but the wrapper will still get inlined.
let $get_add = builder.addFunction("get_add",
                                   makeSig([kWasmExternRef], [kWasmI32]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprAnyConvertExtern,
      kGCPrefix, kExprRefCast, $struct0,
      kGCPrefix, kExprStructGet, $struct0, 0,
      kExprI32Const, 1,
      kExprI32Add,
    ]);

let global_entries = [
  1,  // 1 entry
  $glob0.index,
  0,  // no parent
  2,  // 2 methods
  0, ...StringToArray("get"), $get.index,
  0, ...StringToArray("get_add"), $get_add.index,
  1,  // constructor
  ...StringToArray("Obj"), $make.index,
  0,  // no statics
];
builder.addCustomSection("experimental-descriptors", [
  0,  // version
  0,  // module name
  2,  // GlobalEntries subsection
  ...wasmUnsignedLeb(global_entries.length),
  ...global_entries,
]);

let instance = builder.instantiate();
let Obj = instance.exports.Obj;

let obj = new Obj();

function InlineFunction(o) {
  return o.get();
}
function InlineWrapper(o) {
  return o.get_add();
}
%PrepareFunctionForOptimization(InlineFunction);
%PrepareFunctionForOptimization(InlineWrapper);
for (let i = 0; i < 10; i++) {
  assertEquals(0, InlineFunction(obj));
  assertEquals(1, InlineWrapper(obj));
}

console.log("Now optimizing 'InlineFunction'");
%OptimizeFunctionOnNextCall(InlineFunction);
assertEquals(0, InlineFunction(obj));

console.log("Now optimizing 'InlineWrapper'");
%OptimizeFunctionOnNextCall(InlineWrapper);
assertEquals(1, InlineWrapper(obj));
