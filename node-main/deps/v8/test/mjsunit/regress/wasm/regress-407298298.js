// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff --experimental-wasm-jspi

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let kSig_d_r = makeSig([kWasmExternRef], [kWasmF64]);

let kImports = {
  DataView: {
    byteLengthImport: Function.prototype.call.bind(
        Object.getOwnPropertyDescriptor(DataView.prototype, 'byteLength').get)
  },
};

let builder = new WasmModuleBuilder();
let kDataViewByteLength =
    builder.addImport('DataView', 'byteLengthImport', kSig_d_r);
builder.addFunction('byteLength', kSig_d_r).exportFunc().addBody([
  kExprLocalGet, 0,
  kExprCallFunction, kDataViewByteLength,
]);
let instance = builder.instantiate(kImports);
const kLength = 8;
let buffer = new SharedArrayBuffer(kLength, {maxByteLength: 2 * kLength});
let dataview = new DataView(buffer);
assertPromiseResult(
    WebAssembly.promising(instance.exports.byteLength)(dataview),
    v => assertEquals(v, kLength));
