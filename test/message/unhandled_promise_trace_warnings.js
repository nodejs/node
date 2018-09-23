// Flags: --trace-warnings
'use strict';
const common = require('../common');
common.disableCrashOnUnhandledRejection();
const p = Promise.reject(new Error('This was rejected'));
setImmediate(() => p.catch(() => {}));
