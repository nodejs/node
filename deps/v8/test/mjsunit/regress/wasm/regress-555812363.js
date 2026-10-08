// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-js-interop

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/wasm/prototype-setup-builder.js');

let builder = new WasmModuleBuilder();
let proto_config = new WasmPrototypeSetupBuilder(builder);

let $g_proto = builder.addImportedGlobal('p', 'p', kWasmExternRef);
let $import = builder.addImport('q', 'f', kSig_v_v);
let $method = builder.addFunction('m', kSig_v_r).exportFunc().addBody([
  kExprCallFunction, $import,
]);

proto_config.addConfig($g_proto).addMethod('m', kWasmMethod, $method);
proto_config.build();

let proto = {};
function f() {
  assertEquals(null, f.caller);
}

let imports = {
  p: { p: proto },
  q: { f },
  c: { constructors: {} },
};
builder.instantiate(imports, { builtins: ['js-prototypes'] });
proto.m();
