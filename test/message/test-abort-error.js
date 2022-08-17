'use strict';
require('../common');
const { setImmediate } = require('node:timers/promises');

Error.stackTraceLimit = 2;
setImmediate(undefined, { signal: AbortSignal.abort() }).catch(console.error);
