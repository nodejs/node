'use strict';
const common = require('../common');

// Flags: --expose-gc

Promise.reject(new Error('oops'));

// Manually call GC due to possible memory contraints with attempting to
// trigger it "naturally".
setTimeout(common.mustCall(() => {
  /* eslint-disable no-undef */
  gc();
  gc();
  gc();
  /* eslint-enable no-undef */
}, 1), 2);

process.on('beforeExit', () =>
    common.fail('beforeExit should not be reached'));

process.on('uncaughtException', (err) => {
    // XXX(Fishrock123): This test is currently broken...
    console.log(err.stack);
    common.fail('Should not trigger uncaught exception');
});

process.on('exit', () => process._rawDebug('exit event emitted'));
