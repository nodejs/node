'use strict';

const { expectWarning } = require('../common');

const assert = require('assert');
const { runInNewContext } = require('vm');

assert.strictEqual(typeof Atomics.wake, 'function');
assert.strictEqual(typeof Atomics.notify, 'function');

assert.strictEqual(runInNewContext('typeof Atomics.wake'), 'function');
assert.strictEqual(runInNewContext('typeof Atomics.notify'), 'function');

expectWarning(
  'Atomics',
  'Atomics.wake will be removed in a future version, ' +
  'use Atomics.notify instead.');

Atomics.wake(new Int32Array(new SharedArrayBuffer(4)), 0, 0);
