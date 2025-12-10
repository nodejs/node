// Test that with limited virtual memory space, WASM can still allocate memory
// even without --disable-wasm-trap-handler.
'use strict';

require('../common');
new WebAssembly.Memory({ initial: 10, maximum: 100 });
