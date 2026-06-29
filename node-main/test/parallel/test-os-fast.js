// Flags: --expose-internals --no-warnings --allow-natives-syntax
'use strict';

const common = require('../common');
const assert = require('assert');
const {
  totalmem,
  freemem,
  availableParallelism,
} = require('os');

const { internalBinding } = require('internal/test/binding');

function testFastOs() {
  assert.strictEqual(typeof totalmem(), 'number');
  assert.strictEqual(typeof freemem(), 'number');
  assert.strictEqual(typeof availableParallelism(), 'number');
}

eval('%PrepareFunctionForOptimization(testFastOs)');
testFastOs();
eval('%OptimizeFunctionOnNextCall(testFastOs)');
testFastOs();

if (common.isDebug) {
  const { getV8FastApiCallCount } = internalBinding('debug');
  assert.strictEqual(getV8FastApiCallCount('os.totalmem'), 1);
  assert.strictEqual(getV8FastApiCallCount('os.freemem'), 1);
  assert.strictEqual(getV8FastApiCallCount('os.availableParallelism'), 1);
}
