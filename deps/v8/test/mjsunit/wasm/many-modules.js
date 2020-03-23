// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// No reason to stress-opt this; save some time.
// Flags: --no-stress-opt

load('test/mjsunit/wasm/wasm-module-builder.js');

// We generate the module bytes once to make this test more efficient,
// especially on simulator builds. The bytes contain a sentinel which is later
// patched to different constants. This makes the modules distinct and forces
// the engine to create different code for them.

// This is the sentinel placed in the bytes. It's a 5 byte LEB-encoded integer.
const sentinel = wasmSignedLeb(0x12345678);
assertEquals(5, sentinel.length);

const builder = new WasmModuleBuilder();
builder.addFunction('f', kSig_i_i).addBody([kExprI32Const, ...sentinel]);
const module_bytes = builder.toBuffer();

// Checks whether {module_bytes[i .. i+sentinel.length]} matches {sentinel}.
const has_sentinel = (i, k = 0) => module_bytes[i + k] == sentinel[k] &&
    (k == sentinel.length - 1 || has_sentinel(i, k + 1));
// Now find the sentinel.
const find_sentinel = i =>
    module_bytes.slice(i).findIndex((e, i) => has_sentinel(i));
const sentinel_position = find_sentinel(0);
assertTrue(has_sentinel(sentinel_position), 'found sentinel');
assertEquals(-1, find_sentinel(sentinel_position + 1), 'exactly one sentinel');

// Generating {num_modules} modules should not run out of memory, since the code
// space needed per module is quite low.
const num_modules = 10000;
// Keep all generated modules alive.
const modules = [];
// Reset sentinel section to nops so that shorter LEBs will just be followed by
// nops. This resion will be patched in the loop with values of increasing size.
module_bytes.set(Array(sentinel.length).fill(_ => kExprNop), sentinel_position);
for (let i = 0; i < num_modules; ++i) {
  if (i % 50 == 0) print(i);
  module_bytes.set(wasmSignedLeb(i), sentinel_position);
  modules.push(new WebAssembly.Module(module_bytes));
}
