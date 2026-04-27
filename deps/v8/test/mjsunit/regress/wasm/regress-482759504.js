// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stress-per-context-marking-worklist

let wasm_memory;
try {
    wasm_memory = new WebAssembly.Memory({initial: 1 << 16, maximum: 1 << 16, shared: true});
} catch (e) {
  const is_oom = (e instanceof RangeError) &&
      e.message == 'WebAssembly.Memory(): could not allocate memory';
  if (!is_oom) throw e;
}
function workerCode() {
  this.onmessage = _ => this.postMessage('');
}

const worker = new Worker(workerCode, {type: 'function'});
if (wasm_memory) wasm_memory.toResizableBuffer();
worker.postMessage(wasm_memory);
worker.getMessage();
