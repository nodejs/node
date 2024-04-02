// Flags: --expose-gc

'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');
const makeDuplexPair = require('../common/duplexpair');
const tick = require('../common/tick');

// This tests that running garbage collection while an Http2Session has
// a write *scheduled*, it will survive that garbage collection.

{
  // This creates a session and schedules a write (for the settings frame).
  let client = http2.connect('http://localhost:80', {
    createConnection: common.mustCall(() => makeDuplexPair().clientSide)
  });

  // First, wait for any nextTicks() and their responses
  // from the `connect()` call to run.
  tick(10, () => {
    // This schedules a write.
    client.settings(http2.getDefaultSettings());
    client = null;
    global.gc();
  });
}
