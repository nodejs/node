// Flags: --unhandled-rejections=warn
'use strict';

const common = require('../common');

common.disableCrashOnUnhandledRejection();

// Verify that ignoring unhandled rejection works fine and that no warning is
// logged.

new Promise(() => {
  throw new Error('One');
});

Promise.reject('test');

// Unhandled rejections trigger two warning per rejection. One is the rejection
// reason and the other is a note where this warning is coming from.
process.on('warning', common.mustCall(4));
process.on('uncaughtException', common.mustNotCall('uncaughtException'));
process.on('rejectionHandled', common.mustCall(2));

process.on('unhandledRejection', (reason, promise) => {
  // Handle promises but still warn!
  promise.catch(() => {});
});

setTimeout(common.mustCall(), 2);
