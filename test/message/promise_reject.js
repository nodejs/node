// Flags: --expose-gc

'use strict';

const common = require('../common');
const assert = require('assert');

Promise.reject(new Error('oops'));

// Manually call GC due to possible memory constraints with attempting to
// trigger it "naturally".
setTimeout(common.mustCall(() => {
  global.gc();
  global.gc();
  global.gc();
}), 2);

process.on('uncaughtException', (err) => {
  assert.fail('Should not trigger uncaught exception');
});

process.on('exit', () => process._rawDebug('exit event emitted'));
