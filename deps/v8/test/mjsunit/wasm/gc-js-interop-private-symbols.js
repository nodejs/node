// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --wasm-custom-descriptors --js-nonextensible-applies-to-private

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Wasm objects have a fixed layout and behave like sealed empty JS objects:
// their maps must never transition and no properties may be added to them.

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

// Structs and arrays must retain their type identity after failed assignment:
// sloppy mode fails silently; strict mode throws a TypeError.
// Note: engine-internal private symbols (like %CreatePrivateSymbol) are still
// allowed on sealed JS objects, so this section applies only to Wasm objects.
const wasmStruct = instance.exports.makeStruct();
assertEquals(1, instance.exports.isStruct(wasmStruct));
assertEquals(1, wasmStruct[privateSymbol] = 1);
assertSame(undefined, wasmStruct[privateSymbol]);
assertEquals(1, instance.exports.isStruct(wasmStruct));
assertThrows(() => {
  'use strict';
  wasmStruct[privateSymbol] = 1;
}, TypeError);
assertEquals(1, instance.exports.isStruct(wasmStruct));

const wasmArray = instance.exports.makeArray();
assertEquals(1, instance.exports.isArray(wasmArray));
assertEquals(1, wasmArray[privateSymbol] = 1);
assertSame(undefined, wasmArray[privateSymbol]);
assertEquals(1, instance.exports.isArray(wasmArray));
assertThrows(() => {
  'use strict';
  wasmArray[privateSymbol] = 1;
}, TypeError);
assertEquals(1, instance.exports.isArray(wasmArray));

// Public fields, private fields (#field), and private methods (#method) on
// class return-override must also throw and preserve type identity.
class Base {
  constructor(o) {
    return o;
  }
}
class DerivedWithPublicField extends Base {
  field = 42;
}
class DerivedWithPrivateField extends Base {
  #field = 42;
  static setField(o) {
    o.#field = 1;
  }
}
class DerivedWithPrivateMethod extends Base {
  #method() {}
}

// Also test a sealed empty JS object to verify consistency under
// --js-nonextensible-applies-to-private.
const sealedJsObj = Object.seal({__proto__: null});

for (let i = 0; i < 3; i++) {
  for (const obj of [wasmStruct, wasmArray, sealedJsObj]) {
    assertThrows(() => new DerivedWithPublicField(obj), TypeError);
    assertThrows(() => new DerivedWithPrivateField(obj), TypeError);
    assertThrows(() => DerivedWithPrivateField.setField(obj), TypeError);
    assertThrows(() => new DerivedWithPrivateMethod(obj), TypeError);
  }
  assertEquals(1, instance.exports.isStruct(wasmStruct));
  assertEquals(1, instance.exports.isArray(wasmArray));
}

// Identity hashes used by WeakMap/WeakSet must continue to work.
const wm = new WeakMap();
wm.set(wasmStruct, 'struct-value');
wm.set(wasmArray, 'array-value');
wm.set(sealedJsObj, 'js-value');
assertEquals('struct-value', wm.get(wasmStruct));
assertEquals('array-value', wm.get(wasmArray));
assertEquals('js-value', wm.get(sealedJsObj));
