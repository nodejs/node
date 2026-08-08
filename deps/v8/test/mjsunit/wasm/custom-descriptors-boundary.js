// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-custom-descriptors --experimental-wasm-js-interop

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/wasm/prototype-setup-builder.js');

let builder = new WasmModuleBuilder();
let proto_config = new WasmPrototypeSetupBuilder(builder);

builder.startRecGroup();
let $desc = builder.nextTypeIndex() + 1;
let $struct = builder.addStruct({descriptor: $desc});
builder.addStruct({describes: $struct});

let $struct2 = builder.nextTypeIndex();
let $desc2 = $struct2 + 1;
let $desc3 = $struct2 + 2;

builder.addStruct({descriptor: $desc2});  // struct2
builder.addStruct({descriptor: $desc3, describes: $struct2});  // desc2
builder.addStruct(
    {fields: [makeField(kWasmExternRef, false)], describes: $desc2});  // desc3
builder.endRecGroup();

let identity = null;  // We'll initialize this before calling checkDescriptor.
function checkDescriptor(obj) {
  assertEquals('object', typeof obj);
  // Property access doesn't crash.
  assertEquals(undefined, obj.foo);
  // Any wrapping maintains pointer identity.
  assertSame(identity, obj);
}

let identity2 = null;  // For the descriptor of type $desc2.
function checkDescriptor2(obj) {
  assertEquals('object', typeof obj);
  // Property access doesn't crash.
  assertEquals(undefined, obj.foo);
  // Any wrapping maintains pointer identity.
  assertSame(identity2, obj);
  // Methods can be called.
  assertEquals(42, obj.method42());
}

function js_func_any(desc) {
  checkDescriptor(desc);
}
function js_func_extern(desc) {
  checkDescriptor(desc);
}
function js_func_indexed(desc) {
  checkDescriptor(desc);
}

// When installing imported JS functions as methods, what actually gets
// installed is the Wasm wrapper around the JS function (similar to re-exporting
// an imported function). So even though a later `obj.js_method_any()` call
// looks like it's passing a receiver and no parameter, the Wasm-wrapped
// function has a parameter and no receiver.
// This is different in {pDesc2.pure_js_method}, which we're creating below.
function js_method_any(desc2) {
  assertSame(this, globalThis);
  checkDescriptor2(desc2);
}
function js_method_extern(desc2) {
  assertSame(this, globalThis);
  checkDescriptor2(desc2);
}
function js_method_indexed(desc2) {
  assertSame(this, globalThis);
  checkDescriptor2(desc2);
}

let $imp_any =
    builder.addImport('m', 'js_func_any', makeSig([kWasmAnyRef], []));
let $imp_extern =
    builder.addImport('m', 'js_func_extern', makeSig([kWasmExternRef], []));
let $imp_indexed = builder.addImport(
    'm', 'js_func_indexed', makeSig([wasmRefNullType($desc)], []));

let $imp_method_any =
    builder.addImport('m', 'js_method_any', makeSig([kWasmAnyRef], []));
let $imp_method_extern =
    builder.addImport('m', 'js_method_extern', makeSig([kWasmExternRef], []));
let $imp_method_indexed = builder.addImport(
    'm', 'js_method_indexed', makeSig([wasmRefType($desc2)], []));

let $g_ProtoForDesc2 = builder.addImportedGlobal('p', 'pDesc2', kWasmExternRef);

let $g_desc = builder.addGlobal(
    wasmRefType($desc).exact(), false, false,
    [kGCPrefix, kExprStructNewDefault, $desc]).exportAs('g_desc');
let $g_desc_extern = builder.addGlobal(kWasmExternRef, false, false, [
  kExprGlobalGet, $g_desc.index,
  kGCPrefix, kExprExternConvertAny,
]).exportAs('g_desc_extern');

let $g_desc_any = builder.addGlobal(kWasmAnyRef, false, false, [
  kExprGlobalGet, $g_desc.index,
]).exportAs('g_desc_any');

let $g_desc3_inst =
    builder.addGlobal(wasmRefType($desc3).exact(), false, false, [
      kExprGlobalGet, $g_ProtoForDesc2,
      kGCPrefix, kExprStructNew, $desc3,
    ]);

let $g_desc2_inst =
    builder.addGlobal(wasmRefType($desc2).exact(), false, false, [
      kExprGlobalGet, $g_desc3_inst.index,
      kGCPrefix, kExprStructNewDesc, $desc2,
    ]).exportAs('g_desc2');

builder.addFunction('get_desc_any', makeSig([], [kWasmAnyRef]))
    .exportFunc()
    .addBody([kExprGlobalGet, $g_desc.index]);

builder.addFunction('get_desc_extern', makeSig([], [kWasmExternRef]))
    .exportFunc()
    .addBody([
      kExprGlobalGet, $g_desc.index,
      kGCPrefix, kExprExternConvertAny]);

builder.addFunction('get_desc_indexed', makeSig([], [wasmRefNullType($desc)]))
    .exportFunc()
    .addBody([kExprGlobalGet, $g_desc.index]);

builder.addFunction('call_js_any', kSig_v_v).exportFunc().addBody([
  kExprGlobalGet, $g_desc.index,
  kExprCallFunction, $imp_any,
]);
builder.addFunction('call_js_extern', kSig_v_v).exportFunc().addBody([
  kExprGlobalGet, $g_desc.index,
  kGCPrefix, kExprExternConvertAny,
  kExprCallFunction, $imp_extern,
]);
builder.addFunction('call_js_indexed', kSig_v_v).exportFunc().addBody([
  kExprGlobalGet, $g_desc.index,
  kExprCallFunction, $imp_indexed,
]);

let $table_any = builder.addTable(kWasmAnyRef, 1, 1).exportAs('table_any');
let $table_extern = builder.addTable(kWasmExternRef, 1, 1)
    .exportAs('table_extern');
let $table_indexed = builder.addTable(wasmRefNullType($desc), 1, 1)
    .exportAs('table_indexed');

builder.addFunction('fill_tables', kSig_v_v).exportFunc().addBody([
  kExprI32Const,  0,
  kExprGlobalGet, $g_desc.index,
  kExprTableSet,  $table_any.index,
  kExprI32Const,  0,
  kExprGlobalGet, $g_desc.index,
  kGCPrefix,      kExprExternConvertAny,
  kExprTableSet,  $table_extern.index,
  kExprI32Const,  0,
  kExprGlobalGet, $g_desc.index,
  kExprTableSet,  $table_indexed.index,
]);

let $tag_any = builder.addTag(makeSig([kWasmAnyRef], []));
builder.addExportOfKind('tag_any', kExternalTag, $tag_any);

let $tag_extern = builder.addTag(makeSig([kWasmExternRef], []));
builder.addExportOfKind('tag_extern', kExternalTag, $tag_extern);

let $tag_indexed = builder.addTag(makeSig([wasmRefNullType($desc)], []));
builder.addExportOfKind('tag_indexed', kExternalTag, $tag_indexed);

builder.addFunction('throw_any', kSig_v_v).exportFunc().addBody([
  kExprGlobalGet, $g_desc.index,
  kExprThrow, $tag_any
]);
builder.addFunction('throw_extern', kSig_v_v).exportFunc().addBody([
  kExprGlobalGet, $g_desc.index,
  kGCPrefix, kExprExternConvertAny,
  kExprThrow, $tag_extern
]);
builder.addFunction('throw_indexed', kSig_v_v).exportFunc().addBody([
  kExprGlobalGet, $g_desc.index,
  kExprThrow, $tag_indexed,
]);

let $method42 = builder.addFunction('method42', kSig_i_r).addBody([
  kExprI32Const, 42,
]);

proto_config.addConfig($g_ProtoForDesc2)
            .addMethod('method42', kWasmMethod, $method42)
            .addMethod('js_method_any', kWasmMethod, $imp_method_any)
            .addMethod('js_method_extern', kWasmMethod, $imp_method_extern)
            .addMethod('js_method_indexed', kWasmMethod, $imp_method_indexed);

proto_config.build();

let pDesc2 = {};
let imports = {
  m: {
    js_func_any,
    js_func_extern,
    js_func_indexed,
    js_method_any,
    js_method_extern,
    js_method_indexed,
  },
  p: {pDesc2},
  c: {constructors: {}},
};

let instance = builder.instantiate(imports, {builtins: ['js-prototypes']});
let wasm = instance.exports;
pDesc2.pure_js_method = function() {
  checkDescriptor2(this);
}

identity = wasm.get_desc_any();
identity2 = wasm.g_desc2.value;

// First run will use generic wrappers, second run will use compiled wrappers,
// third run is for redundancy.
for (let i = 0; i < 3; i++) {
  // Parameters
  wasm.call_js_any();
  wasm.call_js_extern();
  wasm.call_js_indexed();

  // Returns
  assertSame(identity, wasm.get_desc_any());
  assertSame(identity, wasm.get_desc_extern());
  assertSame(identity, wasm.get_desc_indexed());

  // Globals
  assertSame(identity, wasm.g_desc_any.value);
  assertSame(identity, wasm.g_desc_extern.value);
  assertSame(identity, wasm.g_desc.value);

  // Tables
  wasm.fill_tables();
  assertSame(identity, wasm.table_any.get(0));
  assertSame(identity, wasm.table_extern.get(0));
  assertSame(identity, wasm.table_indexed.get(0));

  // Exceptions
  try {
    wasm.throw_any();
    assertUnreachable();
  } catch (e) {
    assertSame(identity, e.getArg(wasm.tag_any, 0));
  }
  try {
    wasm.throw_extern();
    assertUnreachable();
  } catch (e) {
    assertSame(identity, e.getArg(wasm.tag_extern, 0));
  }
  try {
    wasm.throw_indexed();
    assertUnreachable();
  } catch (e) {
    assertSame(identity, e.getArg(wasm.tag_indexed, 0));
  }

  // Descriptors-with-descriptors have usable prototypes.
  let desc2 = wasm.g_desc2.value;
  checkDescriptor2(desc2);
  desc2.pure_js_method();
  desc2.js_method_any();
  desc2.js_method_extern();
  desc2.js_method_indexed();
}
