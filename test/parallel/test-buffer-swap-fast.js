// Flags: --expose-internals --no-warnings --allow-natives-syntax
'use strict';

const common = require('../common');
const assert = require('assert');

function testFastSwap16() {
  const buf = Buffer.alloc(256);
  buf.swap16();
}

function testFastSwap32() {
  const buf = Buffer.alloc(256);
  buf.swap32();
}

function testFastSwap64() {
  const buf = Buffer.alloc(256);
  buf.swap64();
}

eval('%PrepareFunctionForOptimization(Buffer.prototype.swap16)');
testFastSwap16();
eval('%OptimizeFunctionOnNextCall(Buffer.prototype.swap16)');
testFastSwap16();

eval('%PrepareFunctionForOptimization(Buffer.prototype.swap32)');
testFastSwap32();
eval('%OptimizeFunctionOnNextCall(Buffer.prototype.swap32)');
testFastSwap32();

eval('%PrepareFunctionForOptimization(Buffer.prototype.swap64)');
testFastSwap64();
eval('%OptimizeFunctionOnNextCall(Buffer.prototype.swap64)');
testFastSwap64();

if (common.isDebug) {
  const { internalBinding } = require('internal/test/binding');
  const { getV8FastApiCallCount } = internalBinding('debug');
  assert.strictEqual(getV8FastApiCallCount('buffer.swap16'), 1);
  assert.strictEqual(getV8FastApiCallCount('buffer.swap32'), 1);
  assert.strictEqual(getV8FastApiCallCount('buffer.swap64'), 1);
}
