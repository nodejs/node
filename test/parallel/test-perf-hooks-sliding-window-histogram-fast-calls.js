// Flags: --expose-internals --no-warnings --allow-natives-syntax
'use strict';

const common = require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');
const {
  createSlidingWindowHistogram,
} = require('perf_hooks');

const histogram = createSlidingWindowHistogram({
  chunks: 2,
  recordsPerChunk: 1,
});

function record() {
  histogram.record(1);
}

eval('%PrepareFunctionForOptimization(histogram.record)');
record();
eval('%OptimizeFunctionOnNextCall(histogram.record)');
record();

assert.strictEqual(histogram.snapshot().count, 2);

if (common.isDebug) {
  const { getV8FastApiCallCount } = internalBinding('debug');
  assert.strictEqual(
    getV8FastApiCallCount('histogram.slidingWindow.record'), 1);
}
