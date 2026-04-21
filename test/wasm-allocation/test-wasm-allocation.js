// RLIMIT_AS: 21474836480
// With 20GB virtual memory, there's enough space for the first few wasm memory
// allocation to succeed, but not enough for many subsequent ones since each
// wasm memory32 with guard regions reserves 8GB of virtual address space.
'use strict';

const common = require('../common');
const assert = require('assert');
const { isMainThread } = require('worker_threads');

// The first allocation should succeed.
const first = new WebAssembly.Memory({ initial: 10, maximum: 100 });
assert(first);

if (!isMainThread) {
  // https://github.com/nodejs/node/issues/62870
  common.skip('Workers terminate instead of throwing');
}

// Subsequent allocations should eventually fail due to running out of
// virtual address space.
assert.throws(() => {
  const instances = [first];
  for (let i = 1; i < 30; i++) {
    instances.push(new WebAssembly.Memory({ initial: 10, maximum: 100 }));
  }
}, /WebAssembly\.Memory/);
