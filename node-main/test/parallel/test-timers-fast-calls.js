// Flags: --allow-natives-syntax --expose-internals --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');

const { internalBinding } = require('internal/test/binding');
const binding = internalBinding('timers');

function testFastCalls() {
  binding.scheduleTimer(1);
  binding.toggleTimerRef(true);
  binding.toggleTimerRef(false);
  binding.toggleImmediateRef(true);
  binding.toggleImmediateRef(false);
}

eval('%PrepareFunctionForOptimization(testFastCalls)');
testFastCalls();
eval('%OptimizeFunctionOnNextCall(testFastCalls)');
testFastCalls();

if (common.isDebug) {
  const { getV8FastApiCallCount } = internalBinding('debug');
  assert.strictEqual(getV8FastApiCallCount('timers.scheduleTimer'), 1);
  assert.strictEqual(getV8FastApiCallCount('timers.toggleTimerRef'), 2);
  assert.strictEqual(getV8FastApiCallCount('timers.toggleImmediateRef'), 2);
}
