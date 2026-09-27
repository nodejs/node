// Flags: --expose-internals --no-warnings --allow-natives-syntax
'use strict';

const common = require('../common');
const assert = require('assert');
const { websocketMask, websocketUnmask } = require('http');

const key = Buffer.from([1, 2, 3, 4]);
const source = Buffer.from('hello world, this is a masking test');
const expected = Buffer.from(source.map((b, i) => b ^ key[i & 3]));

function testFastMask() {
  const output = Buffer.alloc(source.length);
  websocketMask(source, key, output, 0, source.length);
  assert.deepStrictEqual(output, expected);
}

function testFastUnmask() {
  const buf = Buffer.from(expected);
  websocketUnmask(buf, key);
  assert.deepStrictEqual(buf, source);
}

eval('%PrepareFunctionForOptimization(websocketMask)');
testFastMask();
eval('%OptimizeFunctionOnNextCall(websocketMask)');
testFastMask();

eval('%PrepareFunctionForOptimization(websocketUnmask)');
testFastUnmask();
eval('%OptimizeFunctionOnNextCall(websocketUnmask)');
testFastUnmask();

if (common.isDebug) {
  const { internalBinding } = require('internal/test/binding');
  const { getV8FastApiCallCount } = internalBinding('debug');
  assert.strictEqual(getV8FastApiCallCount('buffer.mask'), 2);
}
