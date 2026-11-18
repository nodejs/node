// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --jitless --wasm-jitless --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Numeric parameter conversion calls allocation builtins before dispatching to
// the imported callable. Those calls must not leave the context register
// clobbered when a Proxy is dispatched through CallProxy.
const kParamCount = 40;
const builder = new WasmModuleBuilder();
const importSig =
    builder.addType(makeSig(new Array(kParamCount).fill(kWasmI64), []));
const importIndex = builder.addImport('m', 'f', importSig);

const body = [];
for (let i = 0; i < kParamCount; ++i) {
  body.push(...wasmI64Const(BigInt(i) * 0x100000001n + 0x11223344n));
}
body.push(kExprCallFunction, importIndex);
builder.addFunction('run', kSig_v_v).addBody(body).exportFunc();

let calls = 0;
const callable = new Proxy(function(...args) {
  assertEquals(kParamCount, args.length);
  for (let i = 0; i < args.length; ++i) {
    assertEquals(BigInt(i) * 0x100000001n + 0x11223344n, args[i]);
  }
  ++calls;
}, {
  apply(target, receiver, args) {
    return Reflect.apply(target, receiver, args);
  },
});

const instance = builder.instantiate({m: {f: callable}});
%SimulateNewspaceFull();
instance.exports.run();
assertEquals(1, calls);
