// RLIMIT_AS: 3221225472
// When the virtual memory limit is 3GB, there is not enough virtual memory for
// even one wasm cage. In this case Node.js should automatically adapt and
// skip enabling trap-based bounds checks, so that WASM can at least run with
// inline bound checks.
'use strict';

require('../common');
new WebAssembly.Memory({ initial: 10, maximum: 100 });

// Test memory64 works too.
new WebAssembly.Memory({ address: 'i64', initial: 10n, maximum: 100n });
