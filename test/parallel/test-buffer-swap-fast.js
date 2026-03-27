// Flags: --expose-internals --no-warnings --allow-natives-syntax
'use strict';

const common = require('../common');
const assert = require('assert');

function testFastSwap16() {
  const buf = Buffer.from([0x01, 0x02, 0x03, 0x04]);
  const expected = Buffer.from([0x02, 0x01, 0x04, 0x03]);
  const padded = Buffer.alloc(256);
  buf.copy(padded);
  padded.swap16();
  assert.deepStrictEqual(padded.subarray(0, 4), expected);
}

function testFastSwap32() {
  const buf = Buffer.from([0x01, 0x02, 0x03, 0x04]);
  const expected = Buffer.from([0x04, 0x03, 0x02, 0x01]);
  const padded = Buffer.alloc(256);
  buf.copy(padded);
  padded.swap32();
  assert.deepStrictEqual(padded.subarray(0, 4), expected);
}

function testFastSwap64() {
  const buf = Buffer.from([0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08]);
  const expected = Buffer.from([0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01]);
  const padded = Buffer.alloc(256);
  buf.copy(padded);
  padded.swap64();
  assert.deepStrictEqual(padded.subarray(0, 8), expected);
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
