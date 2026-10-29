// Flags: --v8-pool-size=0 --expose-gc

'use strict';

require('../common');

// This verifies that V8 tasks scheduled by GC are handled on worker threads if
// `--v8-pool-size=0` is given. The worker threads are managed by Node.js'
// `v8::Platform` implementation.
globalThis.gc();
