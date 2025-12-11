// Flags: --allow-natives-syntax --expose-internals --no-warnings
'use strict';

// Tests that process.hrtime.bigint() works.

const common = require('../common');
const assert = require('assert');

const { internalBinding } = require('internal/test/binding');

const start = process.hrtime.bigint();
assert.strictEqual(typeof start, 'bigint');

const end = process.hrtime.bigint();
assert.strictEqual(typeof end, 'bigint');

assert(end - start >= 0n);

eval('%PrepareFunctionForOptimization(process.hrtime.bigint)');
assert(process.hrtime.bigint());
eval('%OptimizeFunctionOnNextCall(process.hrtime.bigint)');
assert(process.hrtime.bigint());

if (common.isDebug) {
  const { getV8FastApiCallCount } = internalBinding('debug');
  assert.strictEqual(getV8FastApiCallCount('process.hrtimeBigInt'), 1);
}
