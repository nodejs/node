'use strict';

require('../common');
const assert = require('assert');
const { mock } = require('node:test');

const originalAbortSignalTimeout = AbortSignal.timeout;

mock.timers.enable({ apis: ['AbortSignal.timeout'] });

const signal = AbortSignal.timeout(50);
assert.strictEqual(signal.aborted, false);

mock.timers.tick(49);
assert.strictEqual(signal.aborted, false);

mock.timers.tick(1);
assert.strictEqual(signal.aborted, true);

mock.timers.reset();
assert.strictEqual(AbortSignal.timeout, originalAbortSignalTimeout);
