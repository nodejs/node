// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-test-streaming --no-wasm-lazy-compilation

function buildModule() {
  let bytes = [];
  function emit(...b) {
    bytes.push(...b);
  }
  function emitU32V(val) {
    while (val > 0x7f) {
      bytes.push((val & 0x7f) | 0x80);
      val >>= 7;
    }
    bytes.push(val & 0x7f);
  }
  function emitSection(id, content) {
    emit(id);
    emitU32V(content.length);
    bytes.push(...content);
  }
  // Wasm header
  emit(0x00, 0x61, 0x73, 0x6d);
  emit(0x01, 0x00, 0x00, 0x00);
  // Type section: one function type () -> ()
  emitSection(1, [1, 0x60, 0, 0]);
  // Function section: one function of type 0
  emitSection(3, [1, 0]);
  // Memory section: one memory min=1
  emitSection(5, [1, 0, 1]);
  // Export section: export function as "f"
  emitSection(7, [1, 1, 0x66, 0x00, 0]);
  // DataCount section: declares 1 data segment
  emitSection(12, [1]);
  // Code section: one function body with data.drop 0
  let funcBody = [0, 0xfc, 0x09, 0x00, 0x0b];  // data.drop 0
  emitSection(10, [1, funcBody.length, ...funcBody]);
  // NO Data section - this is the key: DataCount says 1 but no Data section
  // exists
  return new Uint8Array(bytes);
}

let moduleBytes = buildModule();
WebAssembly.compile(moduleBytes).then(
  (mod) => print('Succeeded'),
  (err) => print('Error: ' + err)
);
