'use strict';

// Regression test for BigInt exponentiation with negative exponent.
// Expected message: RangeError: Exponent must be positive
// Current behavior (V8 regression): RangeError: undefined must be positive
// Upstream issue: https://issues.chromium.org/issues/439515479

const common = require('../common');
const assert = require('assert');

// Skip until the upstream V8 bug is fixed and rolls into Node.js.
common.skip('Skipped due to upstream V8 bug: BigInt negative exponent error message is incorrect. See https://issues.chromium.org/issues/439515479');

// The code below documents the expected behavior once the regression is fixed.
{
  try {
    // 2n ** -1n is invalid because the result would be a fraction (1/2),
    // which BigInt cannot represent.
    // eslint-disable-next-line no-unused-expressions
    2n ** -1n;
    assert.fail('Expected RangeError to be thrown');
  } catch (e) {
    assert(e instanceof RangeError);
    assert.match(e.message, /Exponent must be positive/);
  }
}

{
  const left = 2n;
  const right = -1n;
  try {
    // eslint-disable-next-line no-unused-expressions
    left ** right;
    assert.fail('Expected RangeError to be thrown');
  } catch (e) {
    assert(e instanceof RangeError);
    assert.match(e.message, /Exponent must be positive/);
  }
}
