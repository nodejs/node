'use strict';

const { expectWarning, noWarnCode } = require('../common');

const assert = require('assert');
const { runInNewContext } = require('vm');

assert.strictEqual(typeof Atomics.wake, 'function');
assert.strictEqual(typeof Atomics.notify, 'function');

assert.strictEqual(runInNewContext('typeof Atomics.wake'), 'function');
assert.strictEqual(runInNewContext('typeof Atomics.notify'), 'function');

expectWarning(
  'Atomics',
  'Atomics.wake will be removed in a future version, ' +
  'use Atomics.notify instead.', noWarnCode);

Atomics.wake(new Int32Array(new SharedArrayBuffer(4)), 0, 0);
