'use strict';

const common = require('../common');
common.disableCrashOnUnhandledRejection();

process.on('unhandledRejection', (err) => {
  process.exitWithException(err);
});

Promise.reject(new Error('uh oh'));

setImmediate(common.mustNotCall);
