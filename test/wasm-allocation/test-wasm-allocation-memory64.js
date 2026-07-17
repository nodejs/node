// RLIMIT_AS: 21474836480
// With 20GB virtual memory, there's enough space for the first wasm memory64
// allocation to succeed, but not enough for subsequent ones since each
// wasm memory64 with guard regions reserves 16GB of virtual address space.
'use strict';

const common = require('../common');
const assert = require('assert');
const { isMainThread } = require('worker_threads');

// The first allocation should succeed.
const first = new WebAssembly.Memory({ address: 'i64', initial: 10n, maximum: 100n });
assert(first);

if (!isMainThread) {
  // https://github.com/nodejs/node/issues/62870
  common.skip('Workers terminate instead of throwing');
}

// Subsequent allocations should eventually fail due to running out of
// virtual address space. memory64 reserves 16GB per allocation (vs 8GB for
// memory32), so the limit is reached even faster.
assert.throws(() => {
  const instances = [first];
  for (let i = 1; i < 30; i++) {
    instances.push(new WebAssembly.Memory({ address: 'i64', initial: 10n, maximum: 100n }));
  }
}, /WebAssembly\.Memory/);
