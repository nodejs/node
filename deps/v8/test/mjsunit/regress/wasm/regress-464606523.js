// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff --no-wasm-lazy-compilation

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction('main', kSig_v_v).addBody([]).exportFunc();
const module = builder.toModule();
const serizalized_bytes = d8.wasm.serializeModule(module);

const fake_wire_bytes = new Uint8Array(new ArrayBuffer(8, {maxByteLength: 16}));
assertThrows(
    () => d8.wasm.deserializeModule(serizalized_bytes, fake_wire_bytes), Error,
    /Trying to deserialize manipulated bytes/);
