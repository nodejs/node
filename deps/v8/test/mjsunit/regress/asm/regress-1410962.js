// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const memory = new WebAssembly.Memory({ initial: 1, maximum: 4 });

const heap = memory.buffer;

function builder(stdlib, foreign, heap) {
  "use asm";

  const mem = new stdlib.Float64Array(heap);

  function add(x, y) {
    x = +x;
    y = +y;

    return +mem[0] + +mem[1];
  }

  return {add: add};
}

// Check that the asm.js module can be instantiated in the asm.js pipeline.
builder(globalThis, undefined, new ArrayBuffer(2 ** 16));
assertTrue(%IsAsmWasmCode(builder));

const asm_js_module = builder(globalThis, undefined, heap);

// Should not throw.
memory.grow(0);
