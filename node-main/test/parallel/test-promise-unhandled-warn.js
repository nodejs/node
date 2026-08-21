// Flags: --unhandled-rejections=warn
'use strict';

const common = require('../common');
const assert = require('assert');

// Verify that ignoring unhandled rejection works fine and that no warning is
// logged.

new Promise(() => {
  throw new Error('One');
});

Promise.reject('test');

function lookForMeInStackTrace() {
  Promise.reject(new class ErrorLike {
    constructor() {
      Error.captureStackTrace(this);
      this.message = 'ErrorLike';
    }
  }());
}
lookForMeInStackTrace();

// Unhandled rejections trigger two warning per rejection. One is the rejection
// reason and the other is a note where this warning is coming from.
process.on('warning', common.mustCall((reason) => {
  if (reason.message.includes('ErrorLike')) {
    assert.match(reason.stack, /lookForMeInStackTrace/);
  }
}, 6));
process.on('uncaughtException', common.mustNotCall('uncaughtException'));
process.on('rejectionHandled', common.mustCall(3));

process.on('unhandledRejection', (reason, promise) => {
  // Handle promises but still warn!
  promise.catch(() => {});
});

setTimeout(common.mustCall(), 2);
