// Flags: --unhandled-rejections=none
'use strict';

const common = require('../common');

common.disableCrashOnUnhandledRejection();

// Verify that ignoring unhandled rejection works fine and that no warning is
// logged.

new Promise(() => {
  throw new Error('One');
});

Promise.reject('test');

process.on('warning', common.mustNotCall('warning'));
process.on('uncaughtException', common.mustNotCall('uncaughtException'));
process.on('rejectionHandled', common.mustNotCall('rejectionHandled'));

process.on('unhandledRejection', common.mustCall(2));

setTimeout(common.mustCall(), 2);
