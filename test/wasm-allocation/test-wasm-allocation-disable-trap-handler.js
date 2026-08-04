// Flags: --disable-wasm-trap-handler
// RLIMIT_AS: 34359738368

// 32GB should be enough for at least 2 cages, but not enough for 30.

// Test that with limited virtual memory space, --disable-wasm-trap-handler
// fully disables trap-based bounds checks, and thus allows WASM to run with
// inline bound checks.
'use strict';

require('../common');
const instances = [];
for (let i = 0; i < 30; i++) {
  instances.push(new WebAssembly.Memory({ initial: 10, maximum: 100 }));
}

// Test memory64 works too.
for (let i = 0; i < 30; i++) {
  instances.push(new WebAssembly.Memory({ initial: 10n, maximum: 100n, address: 'i64' }));
}
