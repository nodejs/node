'use strict';

const assert = require('assert');
const { internalBinding } = require('internal/test/binding');

const usdtEnabled =
  internalBinding('diagnostics_channel').probeSemaphore !== undefined;

// CI declares the expected support. Linux builds without sys/sdt.h are valid.
const expected = process.env.NODE_TEST_EXPECT_USDT;
if (expected !== undefined) {
  assert.ok(expected === '0' || expected === '1',
            'NODE_TEST_EXPECT_USDT must be 0 or 1');
  assert.strictEqual(usdtEnabled, expected === '1',
                     `Expected USDT support: ${expected}`);
}

module.exports = { usdtEnabled };
