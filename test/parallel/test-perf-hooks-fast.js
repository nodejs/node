// Flags: --expose-internals --no-warnings --allow-natives-syntax
'use strict';

const common = require('../common');
const assert = require('assert');
const {
  performance
} = require('perf_hooks');

const { internalBinding } = require('internal/test/binding');

function testFastPerf() {
  const obj = performance.eventLoopUtilization();
  assert.strictEqual(typeof obj.idle, 'number');
  assert.strictEqual(typeof obj.active, 'number');
  assert.strictEqual(typeof obj.utilization, 'number');
}

eval('%PrepareFunctionForOptimization(testFastPerf)');
testFastPerf();
eval('%OptimizeFunctionOnNextCall(testFastPerf)');
testFastPerf();

if (common.isDebug) {
  const { getV8FastApiCallCount } = internalBinding('debug');
  assert.strictEqual(getV8FastApiCallCount('performance.loopIdleTime'), 1);
}
