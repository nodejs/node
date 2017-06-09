// Flags: --trace-warnings
'use strict';
const common = require('../common');
const p = Promise.reject(new Error('This was rejected'));
setImmediate(() => p.catch(common.noop));
