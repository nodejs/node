// RLIMIT_AS: 21474836480
// With 20GB virtual memory, there's enough space for the first few wasm memory
// allocation to succeed, but not enough for many subsequent ones since each
// wasm memory32 with guard regions reserves 8GB of virtual address space.
'use strict';

require('../common');
new WebAssembly.Memory({ initial: 10, maximum: 100 });
