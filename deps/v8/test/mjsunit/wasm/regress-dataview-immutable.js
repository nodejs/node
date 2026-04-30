// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-immutable-arraybuffer --wasm-tiering-budget=1 --wasm-sync-tier-up

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let $sig0 = builder.addType(makeSig([kWasmExternRef, kWasmI32, kWasmI32], []));
let s = builder.addImport('m', 's', $sig0);
let w = builder.addFunction('w', $sig0).exportFunc().addBody([
  kExprLocalGet, 0,
  kExprLocalGet, 1,
  kExprLocalGet, 2,
  kExprCallFunction, s,
]);

var setInt8 = Function.prototype.call.bind(DataView.prototype.setInt8);
const instance = builder.instantiate({ m: { s: setInt8 } });


var buf = new ArrayBuffer(16);
var immBuf = buf.transferToImmutable();
var immDv = new DataView(immBuf);

var normalDv = new DataView(new ArrayBuffer(16));

// Call twice to trigger tier-up (with low budget flag)
instance.exports.w(normalDv, 0, 42);
instance.exports.w(normalDv, 0, 42);

assertEquals(0, immDv.getInt8(0));

assertThrows(() => instance.exports.w(immDv, 0, 0x77), TypeError);

// Should still be 0
assertEquals(0, immDv.getInt8(0));
