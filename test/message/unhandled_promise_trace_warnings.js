'use strict';
const common = require('../common');
common.requireFlags('--trace-warnings');
common.disableCrashOnUnhandledRejection();
const p = Promise.reject(new Error('This was rejected'));
setImmediate(() => p.catch(() => {}));
