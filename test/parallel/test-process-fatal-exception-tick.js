'use strict';

const common = require('../common');
const assert = require('assert');

// If a process encounters an uncaughtException, it should schedule
// processing of nextTicks on the next Immediates cycle but not
// before all Immediates are handled

let stage = 0;

process.once('uncaughtException', common.expectsError({
  type: Error,
  message: 'caughtException'
}));

setImmediate(() => {
  stage++;
  process.nextTick(() => assert.strictEqual(stage, 2));
});
const now = Date.now();
setTimeout(() => setImmediate(() => stage++), 1);
while (now + 10 >= Date.now());
throw new Error('caughtException');
