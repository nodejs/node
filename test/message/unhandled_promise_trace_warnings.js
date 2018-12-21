'use strict';
const common = require('../common');
if (!process.execArgv.includes('--trace-warnings'))
  common.relaunchWithFlags(['--trace-warnings']);
common.disableCrashOnUnhandledRejection();
const p = Promise.reject(new Error('This was rejected'));
setImmediate(() => p.catch(() => {}));
