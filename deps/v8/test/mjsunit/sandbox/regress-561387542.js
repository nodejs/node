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
  0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, // wasm magic & version
  0x01, 0x04, 0x01, 0x60, 0x00, 0x00,             // type section: () -> ()
  0x03, 0x02, 0x01, 0x00,                         // func section
  0x07, 0x05, 0x01, 0x01, 0x66, 0x00, 0x00,       // export section: export "f"
  0x0a, 0x04, 0x01, 0x02, 0x00, 0x0b             // code section: empty function
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
