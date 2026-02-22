// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-custom-descriptors --allow-natives-syntax
// Flags: --experimental-wasm-js-interop

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");
d8.file.execute("test/mjsunit/wasm/prototype-setup-builder.js");

var builder = new WasmModuleBuilder();
builder.startRecGroup();
var $struct0 = builder.addStruct(
    {fields: [makeField(kWasmI32, true)], descriptor: 1});
var $desc0 = builder.addStruct(
    {fields: [makeField(kWasmExternRef, false)], describes: $struct0});
builder.endRecGroup();

let proto_config = new WasmPrototypeSetupBuilder(builder);

var $proto0 = builder.addImportedGlobal("p", "p0", kWasmExternRef);

var $glob0 = builder.addGlobal(wasmRefType($desc0).exact(), false, false, [
  kExprGlobalGet, $proto0,
  kGCPrefix, kExprStructNew, $desc0,
]);

let $make = builder.addFunction("make", makeSig([], [kWasmExternRef]))
    .exportFunc()
    .addBody([
      kExprGlobalGet, $glob0.index,
      kGCPrefix, kExprStructNewDefaultDesc, $struct0,
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

proto_config.addConfig($proto0)
    .addConstructor("Obj", $make)
    .addMethod("get", kWasmMethod, $get)
    .addMethod("get_add", kWasmMethod, $get_add);

proto_config.build();

let constructors = {};
let imports = {
  p: {p0: {}},
  c: {constructors},
};
let instance = builder.instantiate(imports, {builtins: ["js-prototypes"]});
let Obj = constructors.Obj;

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
