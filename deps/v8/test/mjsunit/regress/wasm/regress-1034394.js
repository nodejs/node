// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

// Construct a big table switch. The code size will overflow 4096 bytes.
const NUM_CASES = 3073;

let body = [];
// Add one block, so we can jump to this block or to the function end.
body.push(kExprBlock);
body.push(kWasmStmt);

// Add the big BrTable.
body.push(kExprLocalGet, 0);
body.push(kExprBrTable, ...wasmSignedLeb(NUM_CASES));
for (let i = 0; i < NUM_CASES + 1; i++) {
  body.push(i % 2);
}

// End the block.
body.push(kExprEnd);

// Create a module for this.
let builder = new WasmModuleBuilder();
builder.addFunction('main', kSig_v_i).addBody(body).exportFunc();
let instance = builder.instantiate();
instance.exports.main(0);
