// Flags: --trace-warnings
'use strict';
const p = Promise.reject(new Error('This was rejected'));
setImmediate(() => p.catch(() => {}));
