'use strict';
/* eslint-disable no-global-assign */

require('../common');

const perf_hooks = require('perf_hooks');
const assert = require('assert');

const perf = performance;
assert.strictEqual(globalThis.performance, perf_hooks.performance);
performance = undefined;
assert.strictEqual(globalThis.performance, undefined);
assert.strictEqual(typeof perf_hooks.performance.now, 'function');

// Restore the value of performance for the known globals check
performance = perf;
