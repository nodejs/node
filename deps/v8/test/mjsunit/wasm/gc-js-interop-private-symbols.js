// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Wasm objects are opaque and have a fixed layout; their maps must never
// transition and no properties may be added to them.

const builder = new WasmModuleBuilder();
const $struct = builder.addStruct([makeField(kWasmI64, true)]);
const $array = builder.addArray(kWasmI64);

builder.addFunction('makeStruct', makeSig([], [wasmRefType($struct)]))
    .exportFunc()
    .addBody([
      ...wasmI64Const(0),
      kGCPrefix, kExprStructNew, $struct,
    ]);

builder.addFunction('makeArray', makeSig([], [wasmRefType($array)]))
    .exportFunc()
    .addBody([
      ...wasmI64Const(0),
      ...wasmI32Const(1),
      kGCPrefix, kExprArrayNew, $array,
    ]);

builder.addFunction('isStruct', makeSig([kWasmExternRef], [kWasmI32]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprAnyConvertExtern,
      kGCPrefix, kExprRefTest, $struct,
    ]);

builder.addFunction('isArray', makeSig([kWasmExternRef], [kWasmI32]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprAnyConvertExtern,
      kGCPrefix, kExprRefTest, $array,
    ]);

const instance = builder.instantiate();
const privateSymbol = %CreatePrivateSymbol('private');

// Structs and arrays must retain their type identity after failed assignment.
const wasmStruct = instance.exports.makeStruct();
assertEquals(1, instance.exports.isStruct(wasmStruct));
assertThrows(() => wasmStruct[privateSymbol] = 1, TypeError);
assertEquals(1, instance.exports.isStruct(wasmStruct));

const wasmArray = instance.exports.makeArray();
assertEquals(1, instance.exports.isArray(wasmArray));
assertThrows(() => wasmArray[privateSymbol] = 1, TypeError);
assertEquals(1, instance.exports.isArray(wasmArray));

// Standard JS private fields (#field) on class return-override must also throw
// and preserve type identity.
class Base {
  constructor(o) {
    return o;
  }
}
class DerivedWithPrivateField extends Base {
  #field = 42;
}

assertThrows(() => new DerivedWithPrivateField(wasmStruct), TypeError);
assertEquals(1, instance.exports.isStruct(wasmStruct));

assertThrows(() => new DerivedWithPrivateField(wasmArray), TypeError);
assertEquals(1, instance.exports.isArray(wasmArray));

// Identity hashes used by WeakMap/WeakSet must continue to work.
const wm = new WeakMap();
wm.set(wasmStruct, 'struct-value');
wm.set(wasmArray, 'array-value');
assertEquals('struct-value', wm.get(wasmStruct));
assertEquals('array-value', wm.get(wasmArray));
