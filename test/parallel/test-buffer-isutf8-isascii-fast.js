// Flags: --expose-internals --no-warnings --allow-natives-syntax
'use strict';

const common = require('../common');
const assert = require('assert');
const { Buffer, isAscii, isUtf8 } = require('buffer');

const ascii = Buffer.from('hello');
const utf8 = Buffer.from('hello \xc4\x9f');

function testFastIsAscii() {
  assert.strictEqual(isAscii(ascii), true);
}

function testFastIsUtf8() {
  assert.strictEqual(isUtf8(utf8), true);
}

eval('%PrepareFunctionForOptimization(isAscii)');
testFastIsAscii();
eval('%OptimizeFunctionOnNextCall(isAscii)');
testFastIsAscii();

eval('%PrepareFunctionForOptimization(isUtf8)');
testFastIsUtf8();
eval('%OptimizeFunctionOnNextCall(isUtf8)');
testFastIsUtf8();

if (common.isDebug) {
  const { internalBinding } = require('internal/test/binding');
  const { getV8FastApiCallCount } = internalBinding('debug');
  assert.strictEqual(getV8FastApiCallCount('buffer.isAscii'), 1);
  assert.strictEqual(getV8FastApiCallCount('buffer.isUtf8'), 1);
}
