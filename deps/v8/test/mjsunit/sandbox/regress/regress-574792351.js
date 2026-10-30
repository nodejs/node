// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --wasm-imported-strings-utf8 --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

function workerCode() {
  onmessage = function(addr) {
    let memview = new Sandbox.MemoryView(addr.data, 0x100);
    let view = new Uint8Array(memview);
    view[12] = 0xC2;
    view[13] = 0xA2;
    %BlockAt('NewStringFromBytes', 10000);
    postMessage("ready");
    %WaitUntilBlocked('NewStringFromBytes', 10000);
    view[12] = 0x41;
    view[13] = 0x41;
    %Resume('NewStringFromBytes');
  };
}

let worker = new Worker(workerCode, {type: 'function'});

const builder = new WasmModuleBuilder();
let $array0 = builder.addArray(kWasmI8, {final: true});
let $sig1 = builder.addType(makeSig(
    [wasmRefNullType($array0), kWasmI32, kWasmI32],
    [wasmRefType(kWasmExternRef)]));
let $sig2 = builder.addType(makeSig([], [wasmRefNullType($array0)]));
let $sig3 = builder.addType(
    makeSig([wasmRefNullType($array0)], [wasmRefType(kWasmExternRef)]));
let decodeStringFromUTF8Array =
    builder.addImport('wasm:text-decoder', 'decodeStringFromUTF8Array', $sig1);
let create_array = builder.addFunction("create_array", $sig2).exportFunc()
  .addBody([
    kExprI32Const, 10,
    kGCPrefix, kExprArrayNewDefault, $array0,
  ]);
let decode = builder.addFunction("decode", $sig3).exportFunc().addBody([
    kExprLocalGet, 0,  // $var0
    kExprI32Const, 0,
    kExprI32Const, 2,
    kExprCallFunction, decodeStringFromUTF8Array,
  ]);

let instance = builder.instantiate({}, {builtins: ["text-decoder"]});
let wasm_array = instance.exports.create_array();
let addr = Sandbox.getAddressOf(wasm_array);

worker.postMessage(addr);
worker.getMessage();

instance.exports.decode(wasm_array);
