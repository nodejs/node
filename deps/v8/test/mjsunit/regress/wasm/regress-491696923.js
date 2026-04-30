// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --experimental-wasm-js-interop

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();

let $array_externref = builder.addArray(kWasmExternRef, {final: true});
let $array_funcref = builder.addArray(kWasmFuncRef, {final: true});
let $array_i8 = builder.addArray(kWasmI8, {final: true});
let $configureAll = builder.addImport(
    "wasm:js-prototypes",
    "configureAll",
    makeSig([wasmRefNullType($array_externref),
             wasmRefNullType($array_funcref),
             wasmRefNullType($array_i8),
             kWasmExternRef], [])
);
builder.addExport("configureAll", $configureAll);

let $makeExternRefArr = builder.addFunction(
    "makeExternRefArr",
    makeSig([kWasmExternRef], [wasmRefType($array_externref)])
).addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprArrayNewFixed, $array_externref, 1,
]).exportFunc();

let data = [
  1,  // 1 config
  1,  // 1 constructor
  1, 0x41,  // name: "A"
  0,   // no statics
  0,   // no methods
  0x7f // parentidx = -1
]
let data_segment = builder.addPassiveDataSegment(data);

let $makeI8Arr = builder.addFunction(
    "makeI8Arr",
    makeSig([], [wasmRefType($array_i8)])
).addBody([
    kExprI32Const, 0,
    kExprI32Const, ...wasmSignedLeb(data.length),
    kGCPrefix, kExprArrayNewData, $array_i8, data_segment,
]).exportFunc();


let $dummyWasmFunc = builder.addFunction("dummy", kSig_v_v).addBody([]);
builder.addDeclarativeElementSegment([$dummyWasmFunc.index]);

let $makeFuncrefArr = builder.addFunction(
    "makeFuncrefArr",
    makeSig([], [wasmRefType($array_funcref)])
).addBody([
    kExprRefFunc, ...wasmSignedLeb($dummyWasmFunc.index),
    kGCPrefix, kExprArrayNewFixed, $array_funcref, 1,
]).exportFunc();

const instance = builder.instantiate({}, { builtins: ["js-prototypes"] });

let data_array = instance.exports.makeI8Arr();
let prototypes = instance.exports.makeExternRefArr(data_array);
let functions = instance.exports.makeFuncrefArr();
let all_constructors = {};

try {
  instance.exports.configureAll(
      prototypes, functions, data_array, all_constructors);
} catch(e) {
  console.log(`Caught harmless exception: ${e}`);
}

gc();

function func() { }
func.prototype = "hello";
prototypes = instance.exports.makeExternRefArr(func);

try {
  instance.exports.configureAll(
      prototypes, functions, data_array, all_constructors);
} catch(e) {
  console.log(`Caught harmless exception: ${e}`);
}

gc();
