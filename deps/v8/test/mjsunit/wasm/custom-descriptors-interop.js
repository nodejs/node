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
let $g_OtherProto = builder.addImportedGlobal("p", "pOther", kWasmExternRef);

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
  .addBody([
    kExprLocalGet, 0,
    kExprGlobalGet, $g_SuperDesc.index,
    kGCPrefix, kExprStructNewDesc, $Super,
  ]);

let $makeSub = builder.addFunction(
    "MakeSub", makeSig([kWasmI32, kWasmI32], [kWasmAnyRef]))
  .addBody([
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kExprGlobalGet, $g_SubDesc.index,
    kGCPrefix, kExprStructNewDesc, $Sub,
  ]);

let $makeOther = builder.addFunction("MakeOther", makeSig([], [kWasmAnyRef]))
  .addBody([kExprRefNull, kAnyRefCode]);

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

let $other_config = proto_config.addConfig($g_OtherProto)
  .addConstructor("Other", $makeOther);

proto_config.build();

let constructors = {};
let imports = {
  p: {
    pSuper: {__proto__: {rootProp: "root"}},
    pSub: {},
    pOther: {},
  },
  c: {constructors},
};
let instance = builder.instantiate(imports, { builtins: ['js-prototypes'] });
let Super = constructors.Super;
let Sub = constructors.Sub;

let o = {};
%DebugPrint(o);  // Before bug fix, this crashed.

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

  // Mixed prototype chain.
  let js_sub = {jsProp: "js"};
  Object.setPrototypeOf(js_sub, sub);
  assertEquals("function", typeof js_sub.superMethod);
  let iteration = 0;
  for (let p in js_sub) {
    iteration++;
    if (iteration == 1) assertEquals("jsProp", p);
    // The methods on {sub} and {sup} don't show up here because they're not
    // enumerable, but we do iterate over the entire prototype chain.
    if (iteration == 2) assertEquals("rootProp", p);
  }
  assertEquals(2, iteration);
}
%PrepareFunctionForOptimization(Test);
for (let i = 0; i < 3; i++) Test();
%OptimizeFunctionOnNextCall(Test);
Test();

// Passing {null} constructors when setting up constructors throws.
let bad_imports = {...imports};
bad_imports.c.constructors = null;
assertThrows(
    () => builder.instantiate(bad_imports, {builtins: ['js-prototypes']}),
    WebAssembly.RuntimeError, "illegal cast");

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
    testStackTrace(e, [
      /RuntimeError: illegal cast/,
      /at subMethod \(wasm:\/\/wasm\/[0-9a-f]+:wasm-function\[5\]:0x150/,
      /at Traps \(.*\)/,
    ]);
  }
}

(function NoConstructors() {
  let builder = new WasmModuleBuilder();

  builder.startRecGroup();
  let $desc = builder.nextTypeIndex() + 1;
  let $struct = builder.addStruct(
      {fields: [makeField(kWasmI32, true)], descriptor: $desc});
  /* $desc*/ builder.addStruct(
      {fields: [makeField(kWasmExternRef, false)], describes: $struct});
  builder.endRecGroup();

  let proto_config = new WasmPrototypeSetupBuilder(builder);
  let $g_proto = builder.addImportedGlobal("p", "p", kWasmExternRef);

  let $g_desc = builder.addGlobal(
    wasmRefType($desc).exact(), false, false, [
      kExprGlobalGet, $g_proto,
      kGCPrefix, kExprStructNew, $desc,
    ]);

  builder.addFunction("make", makeSig([kWasmI32], [kWasmAnyRef]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprGlobalGet, $g_desc.index,
      kGCPrefix, kExprStructNewDesc, $struct,
    ]);

  let $method = builder.addFunction("method", kSig_i_r).addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprAnyConvertExtern,
    kGCPrefix, kExprRefCast, $struct,
    kGCPrefix, kExprStructGet, $struct, 0,
  ]);

  proto_config.addConfig($g_proto).addMethod("method", kWasmMethod, $method);
  proto_config.build();

  let proto_segment = 0;
  let func_segment = 1;
  let data_segment = 0;
  let data_segment_length = builder.data_segments[data_segment].data.length;
  builder.addFunction("slowPath", kSig_v_v).exportFunc().addBody([
    kExprI32Const, 0,
    kExprI32Const, 1,  // Number of prototypes.
    kGCPrefix, kExprArrayNewElem, proto_config.$array_externref, proto_segment,

    kExprI32Const, 0,
    kExprI32Const, 1,  // Number of functions.
    kGCPrefix, kExprArrayNewElem, proto_config.$array_funcref, func_segment,

    kExprI32Const, 0,
    kExprI32Const, ...wasmSignedLeb(data_segment_length),
    kGCPrefix, kExprArrayNewData, proto_config.$array_i8, data_segment,
    // {ref.null} instead of {global.get} throws us off the fast path.
    kExprRefNull, kExternRefCode,
    kExprCallFunction, proto_config.$configureAll,
  ]);

  let imports = {
    p: { p: {} },
    c: { constructors: null },
  }

  let instance = builder.instantiate(imports, { builtins: ['js-prototypes'] });
  let obj = instance.exports.make(42);
  assertEquals(42, obj.method());
  instance.exports.slowPath();  // Does not throw.
})();
