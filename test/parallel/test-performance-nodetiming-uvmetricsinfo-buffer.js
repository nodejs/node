// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('node:assert');
const { internalBinding } = require('internal/test/binding');

// The event loop metrics reported by libuv are 64-bit counters. The buffers
// used to transfer them to JavaScript must not truncate them to 32 bits.
const {
  uvMetricsBuffer,
  uvMetricsBigIntBuffer,
  uvMetricsInfo,
} = internalBinding('performance');
assert.ok(uvMetricsBuffer instanceof Float64Array);
assert.strictEqual(uvMetricsBuffer.length, 3);
assert.ok(uvMetricsBigIntBuffer instanceof BigUint64Array);
assert.strictEqual(uvMetricsBigIntBuffer.length, 3);

uvMetricsInfo();
for (let i = 0; i < uvMetricsBuffer.length; i++) {
  const value = uvMetricsBuffer[i];
  assert.ok(Number.isSafeInteger(value), `${value} is not a safe integer`);
  assert.ok(value >= 0, `${value} is negative`);
  assert.strictEqual(BigInt(value), uvMetricsBigIntBuffer[i]);
}
