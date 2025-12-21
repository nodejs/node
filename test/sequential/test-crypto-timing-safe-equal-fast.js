// Test v8 fast path for crypto.timingSafeEqual works correctly.
// Flags: --expose-internals --allow-natives-syntax
'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

// V8 Fast API
const foo = Buffer.from('foo');
const bar = Buffer.from('bar');
const longer = Buffer.from('longer');
function testFastPath(buf1, buf2) {
  return crypto.timingSafeEqual(buf1, buf2);
}
eval('%PrepareFunctionForOptimization(testFastPath)');
assert.strictEqual(testFastPath(foo, bar), false);
eval('%OptimizeFunctionOnNextCall(testFastPath)');
assert.strictEqual(testFastPath(foo, bar), false);
assert.strictEqual(testFastPath(foo, foo), true);
assert.throws(() => testFastPath(foo, longer), {
  code: 'ERR_CRYPTO_TIMING_SAFE_EQUAL_LENGTH',
});
assert.throws(() => testFastPath(foo, ''), {
  code: 'ERR_INVALID_ARG_TYPE',
});
assert.throws(() => testFastPath('', ''), {
  code: 'ERR_INVALID_ARG_TYPE',
});

if (common.isDebug) {
  const { internalBinding } = require('internal/test/binding');
  const { getV8FastApiCallCount } = internalBinding('debug');
  assert.strictEqual(getV8FastApiCallCount('crypto.timingSafeEqual.ok'), 2);
  assert.strictEqual(getV8FastApiCallCount('crypto.timingSafeEqual.error'), 3);
}
