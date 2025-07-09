// Flags: --expose-internals --no-warnings --allow-natives-syntax
'use strict';

const common = require('../common');
const assert = require('assert');

const { internalBinding } = require('internal/test/binding');

const histogram = require('perf_hooks').createHistogram();

function testFastMethods() {
  histogram.record(1);
  histogram.recordDelta();
  histogram.percentile(50);
  histogram.reset();
}

eval('%PrepareFunctionForOptimization(histogram.record)');
eval('%PrepareFunctionForOptimization(histogram.recordDelta)');
eval('%PrepareFunctionForOptimization(histogram.percentile)');
eval('%PrepareFunctionForOptimization(histogram.reset)');
testFastMethods();
eval('%OptimizeFunctionOnNextCall(histogram.record)');
eval('%OptimizeFunctionOnNextCall(histogram.recordDelta)');
eval('%OptimizeFunctionOnNextCall(histogram.percentile)');
eval('%OptimizeFunctionOnNextCall(histogram.reset)');
testFastMethods();

if (common.isDebug) {
  const { getV8FastApiCallCount } = internalBinding('debug');
  assert.strictEqual(getV8FastApiCallCount('histogram.record'), 1);
  assert.strictEqual(getV8FastApiCallCount('histogram.recordDelta'), 1);
  assert.strictEqual(getV8FastApiCallCount('histogram.percentile'), 1);
  assert.strictEqual(getV8FastApiCallCount('histogram.reset'), 1);
}
