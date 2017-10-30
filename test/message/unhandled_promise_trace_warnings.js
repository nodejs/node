// Flags: --trace-warnings
'use strict';
require('../common');
const p = Promise.reject(new Error('This was rejected'));
setImmediate(() => p.catch(() => {}));
