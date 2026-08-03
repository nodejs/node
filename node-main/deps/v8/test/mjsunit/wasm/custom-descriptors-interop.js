// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --experimental-wasm-custom-descriptors
// Flags: --experimental-wasm-js-interop

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");
d8.file.execute("test/mjsunit/wasm/prototype-setup-builder.js");

let builder = new WasmModuleBuilder();

builder.startRecGroup();
let super_fields = [makeField(kWasmI32, true)];
let sub_fields = super_fields.concat([makeField(kWasmI32, true)]);
let desc_fields = [makeField(kWasmExternRef, false), makeField(kWasmI32, true)];
let $SuperDesc = builder.nextTypeIndex() + 1;
let $Super = builder.addStruct({fields: super_fields, descriptor: $SuperDesc});
/* $SuperDesc*/ builder.addStruct({fields: desc_fields, describes: $Super});
let $SubDesc = builder.nextTypeIndex() + 1;
let $Sub = builder.addStruct(
    {fields: sub_fields, descriptor: $SubDesc, supertype: $Super});
/* $SubDesc*/ builder.addStruct(
    {fields: desc_fields, describes: $Sub, supertype: $SuperDesc});
builder.endRecGroup();

let proto_config = new WasmPrototypeSetupBuilder(builder);

let $g_SuperProto = builder.addImportedGlobal("p", "pSuper", kWasmExternRef);
let $g_SubProto = builder.addImportedGlobal("p", "pSub", kWasmExternRef);

let $g_SuperDesc = builder.addGlobal(
    wasmRefType($SuperDesc).exact(), false, false, [
      kExprGlobalGet, $g_SuperProto,
      kExprI32Const, 10,
      kGCPrefix, kExprStructNew, $SuperDesc,
    ]);
let $g_SubDesc = builder.addGlobal(
  wasmRefType($SubDesc).exact(), false, false, [
      kExprGlobalGet, $g_SubProto,
      kExprI32Const, 20,
      kGCPrefix, kExprStructNew, $SubDesc,
    ]);
let $g_static = builder.addGlobal(kWasmI32, true, false);

let $makeSuper = builder.addFunction(
    "MakeSuper", makeSig([kWasmI32], [kWasmAnyRef]))
  .exportFunc()
  .addBody([
    kExprLocalGet, 0,
    kExprGlobalGet, $g_SuperDesc.index,
    kGCPrefix, kExprStructNew, $Super,
  ]);

let $makeSub = builder.addFunction(
    "MakeSub", makeSig([kWasmI32, kWasmI32], [kWasmAnyRef]))
  .exportFunc()
  .addBody([
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kExprGlobalGet, $g_SubDesc.index,
    kGCPrefix, kExprStructNew, $Sub,
  ]);

let $super_method = builder.addFunction("superMethod", kSig_i_r).addBody([
  kExprLocalGet, 0,
  kGCPrefix, kExprAnyConvertExtern,
  kGCPrefix, kExprRefCast, $Super,
  kGCPrefix, kExprStructGet, $Super, 0,
]);

let $sub_method = builder.addFunction("subMethod", kSig_i_r).addBody([
  kExprLocalGet, 0,
  kGCPrefix, kExprAnyConvertExtern,
  kGCPrefix, kExprRefCast, $Sub,
  kGCPrefix, kExprStructGet, $Sub, 1,
]);

let $super_static = builder.addFunction("superStatic", kSig_i_r).addBody([
  kExprI32Const, 12,
]);

let $sub_static = builder.addFunction("subStatic", kSig_i_r).addBody([
  kExprI32Const, 23,
]);

let $static_get = builder.addFunction("staticGetter", kSig_i_v).addBody([
  kExprGlobalGet, $g_static.index,
]);
let $static_set = builder.addFunction("staticSetter", kSig_v_i).addBody([
  kExprLocalGet, 0,
  kExprGlobalSet, $g_static.index,
]);

// Configure prototypes.

let $super_config = proto_config.addConfig($g_SuperProto)
  .addConstructor("Super", $makeSuper)
  .addMethod("superMethod", kWasmMethod, $super_method)
  .addMethod("traps", kWasmMethod, $sub_method)
  .addStatic("method", kWasmMethod, $super_static);

let $sub_config = proto_config.addConfig($g_SubProto, $super_config)
  .addConstructor("Sub", $makeSub)
  .addMethod("subMethod", kWasmMethod, $sub_method)
  .addStatic("method", kWasmMethod, $sub_static)
  .addStatic("property", kWasmGetter, $static_get)
  .addStatic("property", kWasmSetter, $static_set);

proto_config.build();

let constructors = {};
let imports = {
  p: {
    pSuper: {},
    pSub: {},
  },
  c: {constructors},
}

let instance = builder.instantiate(imports, { builtins: ['js-prototypes'] });

let Super = constructors.Super;
let Sub = constructors.Sub;

function Test() {
  // Static methods.
  assertEquals(12, Super.method());
  assertEquals(23, Sub.method());

  // Static getter/setter.
  Sub.property = 42;
  assertEquals(42, Sub.property);

  // Instance methods.
  let sup = new Super(1);
  assertEquals(1, sup.superMethod());
  let sub = new Sub(2, 3);
  assertEquals(2, sub.superMethod());
  assertEquals(3, sub.subMethod());
  assertEquals("undefined", typeof sub.method);
  assertEquals("undefined", typeof sub.property);

  assertTrue(sup instanceof Super);
  assertFalse(sup instanceof Sub);
  assertEquals(Super.prototype, Object.getPrototypeOf(sup));
  assertEquals(Super, Super.prototype.constructor);

  assertTrue(sub instanceof Sub);
  assertTrue(sub instanceof Super);
  assertEquals(Sub.prototype, Object.getPrototypeOf(sub));
  assertEquals(Sub, sub.constructor);
  assertEquals(Super.prototype, Object.getPrototypeOf(Sub.prototype));
}
%PrepareFunctionForOptimization(Test);
for (let i = 0; i < 3; i++) Test();
%OptimizeFunctionOnNextCall(Test);
Test();

function Traps() {
  let sup = new Super(1);
  sup.traps();
}
function testStackTrace(error, expected) {
  try {
    let stack = error.stack.split("\n");
    assertTrue(stack.length >= expected.length);
    for (let i = 0; i < expected.length; ++i) {
      assertMatches(expected[i], stack[i]);
    }
  } catch(failure) {
    print("Actual stack trace: ", error.stack);
    throw failure;
  }
}

%PrepareFunctionForOptimization(Traps);
for (let i = 0; i < 3; i++) {
  if (i == 2) { %OptimizeFunctionOnNextCall(Traps); }
  try {
    Traps();
    assertUnreachable();
  } catch (e) {
    console.log("error stack: ", e.stack);
    testStackTrace(e, [
      /RuntimeError: illegal cast/,
      /at subMethod \(wasm:\/\/wasm\/[0-9a-f]+:wasm-function\[4\]:0x14f/,
      /at Traps \(.*\)/,
    ]);
  }
}
