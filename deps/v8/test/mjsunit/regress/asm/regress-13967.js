// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const initial = 2**16;
const max = 4 * initial;

const memory = new ArrayBuffer(initial, { maxByteLength: max } );

const heap = memory;

function builder(stdlib, foreign, heap) {
  "use asm";

  const mem = new stdlib.Float64Array(heap);

  function add(x, y) {
    x = +x;
    y = +y;

    mem[0] = +x;
    mem[1] = +y;

    return +mem[0] + +mem[1];
  }

  return {add: add};
}
// Check that the asm.js module can be instantiated in the asm.js pipeline.
builder(globalThis, undefined, new ArrayBuffer(initial));
assertTrue(%IsAsmWasmCode(builder));

const asm_js_module = builder(globalThis, undefined, heap);

memory.resize(8);

// Memory access out of bounds should return NaN.
assertEquals(NaN, asm_js_module.add(4, 56));

memory.resize(16);
const foo = new Float64Array(memory);

assertEquals(0, foo[1]);
