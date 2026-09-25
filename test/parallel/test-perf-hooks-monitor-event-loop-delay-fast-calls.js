// Flags: --allow-natives-syntax --expose-internals --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');

const { internalBinding } = require('internal/test/binding');
const { createELDHistogram } = internalBinding('performance');

const histogram = createELDHistogram(1, true);

function testFastMethods() {
  histogram.start(true);
  histogram.stop();
}

eval('%PrepareFunctionForOptimization(testFastMethods)');
testFastMethods();
eval('%OptimizeFunctionOnNextCall(testFastMethods)');
testFastMethods();

if (common.isDebug) {
  const { getV8FastApiCallCount } = internalBinding('debug');
  assert.strictEqual(getV8FastApiCallCount('histogram.eventLoopDelay.start'), 1);
  assert.strictEqual(getV8FastApiCallCount('histogram.eventLoopDelay.stop'), 1);
}
