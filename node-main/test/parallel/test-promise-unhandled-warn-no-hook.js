// Flags: --unhandled-rejections=warn
'use strict';

const common = require('../common');

// Verify that --unhandled-rejections=warn works fine

new Promise(() => {
  throw new Error('One');
});

Promise.reject('test');

// Unhandled rejections trigger two warning per rejection. One is the rejection
// reason and the other is a note where this warning is coming from.
process.on('warning', common.mustCall(4));
process.on('uncaughtException', common.mustNotCall('uncaughtException'));
process.on('rejectionHandled', common.mustNotCall('rejectionHandled'));

setTimeout(common.mustCall(), 2);
