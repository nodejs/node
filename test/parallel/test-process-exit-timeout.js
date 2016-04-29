'use strict';

const common = require('../common');
const assert = require('assert');

// Schedule something that'll keep the loop active normally
var tick = 0;
setInterval(() => {
  tick++;
  // Abort just in case this goes on too long
  assert(tick < 10);
}, common.platformTimeout(1000));

process.on('exitingSoon', common.mustCall((ready, timed) => {
  // timed is true because there is a timer
  assert.strictEqual(timed, true);
  // Purposefully do not call the ready callback.
}));

// This will sit for about five seconds then exit normally with exit code
// zero. The test fails if it hangs forever.
assert.strictEqual(process.exitSoon(0, common.platformTimeout(5000)), true);
