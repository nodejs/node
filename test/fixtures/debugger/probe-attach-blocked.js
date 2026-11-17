'use strict';
// Blocked the main thread with a synchronous read until its stdin is closed, so it
// cannot answer inspector requests.
require('fs').writeSync(1, 'PROBE_READY\n');
require('fs').readSync(0, Buffer.alloc(1));
