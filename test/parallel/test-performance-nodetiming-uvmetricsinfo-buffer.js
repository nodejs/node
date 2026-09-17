// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('node:assert');
const { internalBinding } = require('internal/test/binding');

// The event loop metrics reported by libuv are 64-bit counters. The buffer
// used to transfer them to JavaScript must not truncate them to 32 bits.
const { uvMetricsBuffer, uvMetricsInfo } = internalBinding('performance');
assert.ok(uvMetricsBuffer instanceof Float64Array);
assert.strictEqual(uvMetricsBuffer.length, 3);

uvMetricsInfo();
for (const value of uvMetricsBuffer) {
  assert.ok(Number.isSafeInteger(value), `${value} is not a safe integer`);
  assert.ok(value >= 0, `${value} is negative`);
}
