// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-js-interop

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute("test/mjsunit/wasm/prototype-setup-builder.js");

const builder = new WasmModuleBuilder();

builder.startRecGroup();
let $desc = builder.nextTypeIndex() + 1;
let $struct = builder.addStruct({descriptor: $desc});
/* $desc */ builder.addStruct({describes: $struct,
                               fields: [makeField(kWasmExternRef, false)]});
builder.endRecGroup();

let proto_config = new WasmPrototypeSetupBuilder(builder);

let $g_proto = builder.addImportedGlobal("p", "p", kWasmExternRef);
let $g_desc = builder.addGlobal(wasmRefType($desc).exact(), false, false, [
  kExprGlobalGet, $g_proto,
  kGCPrefix, kExprStructNew, $desc,
]);

let $method = builder.addFunction("method", kSig_i_r).exportFunc().addBody([
  kExprUnreachable,
]);

proto_config.addConfig($g_proto).addMethod("method", kWasmMethod, $method);
proto_config.build();

let my_proto = {};
let imports = {
  p: { p: my_proto },
  c: { constructors: null },
};
let instance = builder.instantiate(imports, { builtins: ['js-prototypes'] });
d8.debugger.enable();
assertThrows(() => my_proto.method(), WebAssembly.RuntimeError, "unreachable");
