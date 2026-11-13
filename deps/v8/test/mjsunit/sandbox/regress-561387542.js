// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --cache=after-execute

function outer() {
  function inner() {
    return 42;
  }
  return inner;
}

const innerFunc = outer();

const wasmTable = new WebAssembly.Table({ initial: 2, element: "anyfunc" });

const wasmBytes = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d,  // wasm magic
  0x01, 0x00, 0x00, 0x00,  // wasm version

  0x01,                    // section kind: Type
  0x04,                    // section length 4
  0x01, 0x60,              // types count 1:  kind: func
  0x00,                    // param count 0
  0x00,                    // return count 0

  0x03,                    // section kind: Function
  0x02,                    // section length 2
  0x01, 0x00,              // functions count 1: 0 $f

  0x07,                    // section kind: Export
  0x05,                    // section length 5
  0x01,                    // exports count 1: export # 0
  0x01,                    // field name length:  1
  0x66,                    // field name: f
  0x00, 0x00,              // kind: function index:  0

  0x0a,                    // section kind: Code
  0x04,                    // section length 4
  0x01,                    // functions count 1
                           // function #0 $f
  0x02,                    // body size 2
  0x00,                    // 0 entries in locals list
  0x0b,                    // end
]);
const wasmModule = new WebAssembly.Module(wasmBytes);
const wasmInstance = new WebAssembly.Instance(wasmModule);
const wasmExportedFunc = wasmInstance.exports.f;

const sandboxMemory = new DataView(new Sandbox.MemoryView(0, 0x100000000));

const tableAddress = Sandbox.getAddressOf(wasmTable);
const wasmDispatchTableHandle = sandboxMemory.getUint32(tableAddress + 28, true);

const funcAddress = Sandbox.getAddressOf(innerFunc);
const funcSfiAddress = sandboxMemory.getUint32(funcAddress + 16, true) - 1;
const currentTrustedHandle = sandboxMemory.getUint32(funcSfiAddress + 4, true);

const scriptAddress = sandboxMemory.getUint32(funcSfiAddress + 20, true) - 1;
const isDeserialized = (sandboxMemory.getUint32(scriptAddress + 56, true) & 0x1000) !== 0;

if (!isDeserialized) {
  sandboxMemory.setUint32(funcSfiAddress + 4, wasmDispatchTableHandle, true);
} else {
  sandboxMemory.setUint32(tableAddress + 28, currentTrustedHandle, true);
  wasmTable.set(0, wasmExportedFunc);
}
