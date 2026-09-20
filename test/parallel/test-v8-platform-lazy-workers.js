// Flags: --expose-gc

'use strict';

require('../common');

// V8 posts tasks onto the Node.js platform worker pool during GC. Those
// workers (and the delayed-task libuv loop) are started on demand; this
// verifies that path still runs after a process that did not create the
// pool at bootstrap.
globalThis.gc();
