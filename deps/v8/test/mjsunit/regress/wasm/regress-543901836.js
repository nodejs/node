// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --gc-global --expose-externalize-string
// Flags: --allow-natives-syntax

const SIZE = 32 * 1024 * 1024;

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let $sig0 = builder.addType(makeSig([], [wasmRefType(kWasmExternRef)]));
let $sig1 = builder.addType(makeSig([kWasmExternRef, kWasmI32, kWasmI32],
                                    [wasmRefType(kWasmExternRef)]));
let $getExt = builder.addImport('env', 'getExt', $sig0);
let substring = builder.addImport('wasm:js-string', 'substring', $sig1);
builder.addFunction("run", $sig0).exportFunc().addBody([
    kExprCallFunction, $getExt,
    kExprI32Const, 0,
    kExprI32Const, 8,
    kExprCallFunction, substring,
  ]);

function makeExternal(s) {
  const t = createExternalizableString(s);
  externalizeString(t);
  return t;
}
function getExt() {
  const s = makeExternal('A'.repeat(SIZE));
  %SimulateNewspaceFull();
  return s;
}

let instance = builder.instantiate(
    { env: { getExt } }, { builtins: ['js-string'] });
%WasmTierUpFunction(instance.exports.run);

for (let i = 0; i < 5; i++) {
  instance.exports.run();
}
